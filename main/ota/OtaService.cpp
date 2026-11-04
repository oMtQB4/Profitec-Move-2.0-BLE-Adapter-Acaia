#include "ota/OtaService.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <host/ble_gap.h>
#include <sdkconfig.h>

namespace ota {

namespace {

constexpr const char* TAG = "ota";
constexpr size_t kMaxPendingNotifications = 8;
constexpr UBaseType_t kQueueLength = 20; // window plus room for control writes
constexpr int64_t kDataGapLogUs = 500000;
constexpr uint32_t kRateLogPackets = 128;

OtaSession::Options sessionOptions(uint16_t window)
{
    OtaSession::Options options;
    options.projectName = esp_app_get_description()->project_name; // only images of this project
    options.window = window;
    return options;
}

/** Logs what the link actually negotiated: it decides the update speed. */
int logLinkEvent(ble_gap_event* event, void*)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONN_UPDATE: {
        ble_gap_conn_desc desc;
        if (event->conn_update.status == 0 && ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
            ESP_LOGI(TAG, "link: interval %.2f ms, latency %u, timeout %u ms", desc.conn_itvl * 1.25,
                     desc.conn_latency, desc.supervision_timeout * 10);
        }
        break;
    }
    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        ESP_LOGI(TAG, "link: PHY tx %u rx %u (2 = 2M)", event->phy_updated.tx_phy, event->phy_updated.rx_phy);
        break;
    case BLE_GAP_EVENT_DATA_LEN_CHG:
        ESP_LOGI(TAG, "link: data length tx %u rx %u bytes", event->data_len_chg.max_tx_octets,
                 event->data_len_chg.max_rx_octets);
        break;
    default:
        break;
    }
    return 0;
}

ble_gap_event_listener s_linkListener;

} // namespace

OtaService::OtaService(FlashTarget& flash, std::string firmwareVersion)
    : firmwareVersion_(std::move(firmwareVersion))
    , capacity_(flash.capacity())
    , session_(flash, *this, sessionOptions(kWindow))
    , queue_(xQueueCreate(kQueueLength, sizeof(Message)))
{
    static_assert(kWindow < kQueueLength, "the queue must hold a full window");
}

OtaService::~OtaService()
{
    vQueueDelete(queue_);
}

void OtaService::registerService(NimBLEServer& server)
{
    NimBLEService* service = server.createService(kServiceUuid);
    NimBLECharacteristic* info = service->createCharacteristic(kInfoUuid, NIMBLE_PROPERTY::READ, 64);
    control_ = service->createCharacteristic(kControlUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY, 16);
    NimBLECharacteristic* data = service->createCharacteristic(
        kDataUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, kMaxAttributeLength);
    info->setCallbacks(this);
    control_->setCallbacks(this);
    data->setCallbacks(this);
    ble_gap_event_listener_register(&s_linkListener, logLinkEvent, nullptr);
}

void OtaService::enterUpdateMode(const std::string& name)
{
    updateMode_.store(true);
    session_.setEnabled(true);
    NimBLEDevice::setDeviceName(name);

    // Name in the advertisement, update service in the scan response: the
    // browser updater finds only adapters that are in update mode.
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(name);
    NimBLEAdvertisementData scanData;
    scanData.addServiceUUID(NimBLEUUID(kServiceUuid));

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->stop();
    advertising->setAdvertisementData(advData);
    advertising->setScanResponseData(scanData);
    advertising->enableScanResponse(true);
    ESP_LOGI(TAG, "update mode: advertising as %s", name.c_str());
    advertising->start();
}

void OtaService::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
{
    const NimBLEAttValue value = characteristic->getValue();
    // Only the BLE host task writes here, so one scratch message is enough.
    Message& message = scratch_;
    message.kind = characteristic == control_ ? Message::Kind::Control : Message::Kind::Data;
    message.connHandle = connInfo.getConnHandle();
    message.chunk = chunkSizeForMtu(connInfo.getMTU());
    message.length = static_cast<uint16_t>(std::min<size_t>(value.size(), sizeof(message.data)));
    message.receivedUs = esp_timer_get_time();
    std::memcpy(message.data, value.data(), message.length);
    if (xQueueSend(queue_, &message, 0) != pdTRUE) {
        overflow_.store(true);
    }
}

void OtaService::onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
{
    characteristic->setValue(encodeInfo({
        .updateMode = updateMode_.load(),
        .transferring = transferring_.load(),
        .chunk = chunkSizeForMtu(connInfo.getMTU()),
        .window = kWindow,
        .capacity = capacity_,
        .firmwareVersion = firmwareVersion_,
    }));
}

void OtaService::poll(uint32_t nowMs, uint32_t waitMs)
{
    if (overflow_.exchange(false)) {
        ESP_LOGW(TAG, "client ignored the window, packets dropped");
        session_.onOverflow();
    }
    TickType_t wait = pdMS_TO_TICKS(waitMs);
    while (xQueueReceive(queue_, &received_, wait) == pdTRUE) {
        wait = 0;
        handle(received_, nowMs);
    }

    if (sessionConn_ != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_find(sessionConn_, nullptr) != 0) {
        ESP_LOGI(TAG, "update client disconnected");
        sessionConn_ = BLE_HS_CONN_HANDLE_NONE;
        session_.onDisconnect();
    }
    session_.poll(nowMs);
    transferring_.store(isTransferring());
    flushNotifications();
    keepAdvertising();
}

void OtaService::handle(const Message& message, uint32_t nowMs)
{
    const std::span<const uint8_t> data{message.data, message.length};
    lastActivityMs_ = nowMs;
    if (message.kind == Message::Kind::Data) {
        traceData(message);
        const int64_t start = esp_timer_get_time();
        session_.onData(data, nowMs);
        busyUs_ += esp_timer_get_time() - start;
        return;
    }
    if (updateMode_.load() && commandOpcode(data) == Opcode::Begin) {
        speedUp(message.connHandle);
    }
    if (commandOpcode(data) == Opcode::End && isTransferring()) {
        const int64_t elapsedUs = esp_timer_get_time() - startedUs_;
        ESP_LOGI(TAG, "transfer: %u bytes in %.1f s (%.1f KB/s), flash busy %.1f s",
                 static_cast<unsigned>(session_.imageSize()), elapsedUs / 1e6,
                 session_.imageSize() / 1024.0 / (elapsedUs / 1e6), busyUs_ / 1e6);
    }
    session_.onControl(data, message.chunk, nowMs);
    if (isTransferring()) {
        sessionConn_ = message.connHandle;
        if (commandOpcode(data) == Opcode::Begin) {
            startedUs_ = esp_timer_get_time();
            busyUs_ = 0;
            dataPackets_ = 0;
            ESP_LOGI(TAG, "update started: %u bytes, %u per packet", static_cast<unsigned>(session_.imageSize()),
                     static_cast<unsigned>(message.chunk));
        }
    } else if (isDone()) {
        ESP_LOGI(TAG, "update to %s complete", session_.imageVersion().c_str());
    }
}

/** Timing of the data stream: when it starts, where it stalls, how fast it runs. */
void OtaService::traceData(const Message& message)
{
    if (!isTransferring()) {
        return;
    }
    if (dataPackets_ == 0) {
        ESP_LOGI(TAG, "first data packet %lld ms after Begin, %u bytes", (message.receivedUs - startedUs_) / 1000,
                 message.length);
    } else if (message.receivedUs - lastDataUs_ > kDataGapLogUs) {
        ESP_LOGW(TAG, "no data for %lld ms before packet %lu", (message.receivedUs - lastDataUs_) / 1000,
                 static_cast<unsigned long>(dataPackets_));
    }
    ++dataPackets_;
    lastDataUs_ = message.receivedUs;
    if (dataPackets_ % kRateLogPackets == 0) {
        const double seconds = (message.receivedUs - startedUs_) / 1e6;
        ESP_LOGI(TAG, "%lu packets, %lu bytes, %.1f KB/s", static_cast<unsigned long>(dataPackets_),
                 static_cast<unsigned long>(session_.bytesWritten()), session_.bytesWritten() / 1024.0 / seconds);
    }
}

/** Short connection interval, long packets and 2M PHY; a client may refuse any of it. */
void OtaService::speedUp(uint16_t connHandle)
{
    if (connHandle == tunedConn_) {
        return;
    }
    tunedConn_ = connHandle;
    ble_gap_conn_desc desc;
    if (ble_gap_conn_find(connHandle, &desc) == 0) {
        ESP_LOGI(TAG, "link at Begin: interval %.2f ms, latency %u, MTU %u", desc.conn_itvl * 1.25, desc.conn_latency,
                 ble_att_mtu(connHandle));
    }
    NimBLEServer* server = NimBLEDevice::getServer();
    server->setDataLen(connHandle, 251);
    server->updatePhy(connHandle, BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK, 0);
    server->updateConnParams(connHandle, 12, 24, 0, 400); // 15-30 ms, 4 s supervision timeout
}

void OtaService::notify(std::span<const uint8_t> frame)
{
    if (frame.size() >= 2 && frame[1] != static_cast<uint8_t>(Status::Ok)) {
        ESP_LOGW(TAG, "response 0x%02x: %s", frame[0], toString(static_cast<Status>(frame[1])));
    }
    if (pending_.size() >= kMaxPendingNotifications) {
        pending_.pop_front();
    }
    pending_.emplace_back(frame.begin(), frame.end());
    flushNotifications();
}

void OtaService::flushNotifications()
{
    while (!pending_.empty() && control_ != nullptr) {
        const std::vector<uint8_t>& frame = pending_.front();
        if (!control_->notify(frame.data(), frame.size())) {
            return; // no buffer: retry on the next poll
        }
        pending_.pop_front();
    }
}

void OtaService::keepAdvertising()
{
    if (!updateMode_.load()) {
        return;
    }
    // Advertising stops on every connection. Keep a free slot visible, in
    // case the coffee machine connected to the adapter's address first.
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (NimBLEDevice::getServer()->getConnectedCount() < CONFIG_BT_NIMBLE_MAX_CONNECTIONS &&
        !advertising->isAdvertising()) {
        advertising->start();
    }
}

} // namespace ota
