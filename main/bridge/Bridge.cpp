#include "bridge/Bridge.hpp"

#include <cstdlib>

#include <esp_log.h>
#include <esp_timer.h>

namespace bridge {

namespace {

constexpr const char* TAG = "bridge";
constexpr UBaseType_t kCommandQueueLength = 8;

uint32_t uptimeMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

TareController::Options tareOptions(const config::Settings& settings)
{
    return {.keepTaredZeroOnConnect = settings.tareOnCupPlaced(), .tareScale = settings.tareScale};
}

} // namespace

Bridge::Bridge(scale::ScaleClient& scale, acaia::AcaiaPeripheral& acaia, Options options,
               const config::Settings& settings, std::optional<PeerAddress> knownMachine)
    : scale_(scale)
    , acaia_(acaia)
    , options_(options)
    , settings_(settings)
    , commands_(xQueueCreate(kCommandQueueLength, sizeof(acaia::Command)))
    , tare_(tareOptions(settings))
{
    identifier_.setKnownMachine(knownMachine);
    if (knownMachine) {
        ESP_LOGI(TAG, "known machine: %s", knownMachine->toString().c_str());
    }
}

Bridge::~Bridge()
{
    vQueueDelete(commands_);
}

void Bridge::begin()
{
    if (options_.advertiseWithoutScale) {
        acaia_.startAdvertising();
    }
}

void Bridge::poll()
{
    const uint32_t now = uptimeMs();
    trackClient(now);

    acaia::Command command;
    while (xQueueReceive(commands_, &command, 0) == pdTRUE) {
        if (command == acaia::Command::Tare) {
            tare_.tare(now);
            tare("tare command", now);
            continue;
        }
        const auto scaleCommand = toScaleCommand(command);
        if (!scaleCommand || !scale_.send(*scaleCommand)) {
            ESP_LOGW(TAG, "could not forward %s to scale", acaia::toString(command));
        }
    }

    sendScaleTareIfDue(now);
    tare_.tick(now);
}

void Bridge::applySettings(const config::Settings& settings)
{
    if (settings.tareOnCupPlaced() != settings_.tareOnCupPlaced()) {
        autoTare_.reset();
    }
    settings_ = settings;
    tare_.setOptions(tareOptions(settings));
}

void Bridge::forgetMachine()
{
    identifier_.forgetMachine();
}

/*
 * Polled every 10 ms, so a connect is handled before the client has
 * subscribed to the first weight notification.
 */
void Bridge::trackClient(uint32_t nowMs)
{
    const uint32_t connection = acaia_.connectionCount();
    const bool connected = acaia_.isClientConnected();
    if (clientConnected_ && (!connected || connection != connection_)) {
        clientLeft(nowMs);
    }
    if (connected && !clientConnected_) {
        connection_ = connection;
        clientArrived(nowMs);
    }
    if (!clientConnected_) {
        return;
    }

    if (acaia_.isSubscribed()) {
        identifier_.subscribed(nowMs);
    }
    if (acaia_.clientWrote()) {
        identifier_.wrote();
    }
    identifier_.tick(nowMs);
    const ClientKind kind = identifier_.kind();
    if (kind != loggedKind_) {
        loggedKind_ = kind;
        ESP_LOGI(TAG, "client %s is %s", identifier_.address().toString().c_str(), toString(kind));
        if (kind == ClientKind::Machine && !tareClient_ && settings_.tareOnMachineConnect()) {
            ESP_LOGW(TAG, "an unknown machine: weight not zeroed at connect (forget the old machine in the configurator)");
        }
        if (kind == ClientKind::Other) {
            autoTare_.reset(); // cup detection resumes
        }
    }
}

/*
 * The Move 2.0 connects when a shot starts and sends no tare; a real Lunar
 * zeroes itself in its espresso modes. Until the machine's address is known,
 * every client that might be the machine gets the connect tare.
 */
void Bridge::clientArrived(uint32_t nowMs)
{
    clientConnected_ = true;
    identifier_.connected(acaia_.clientAddress(), nowMs);
    loggedKind_ = identifier_.kind();
    autoTare_.reset();
    const bool maybeMachine = identifier_.kind() == ClientKind::Machine ||
                              (identifier_.kind() == ClientKind::Identifying && !identifier_.knownMachine());
    ESP_LOGI(TAG, "client %s connected: %s", identifier_.address().toString().c_str(),
             identifier_.kind() == ClientKind::Machine ? "the machine" : "identifying");

    tareClient_ = maybeMachine && settings_.tareOnMachineConnect();
    if (!tareClient_) {
        return;
    }
    switch (tare_.clientConnected(nowMs)) {
    case TareController::Connect::Zeroed:
        tare("machine connected", nowMs);
        break;
    case TareController::Connect::KeptReconnect:
        ESP_LOGI(TAG, "machine reconnected within the shot: keeping the zero point");
        break;
    case TareController::Connect::KeptTared:
        ESP_LOGI(TAG, "machine connected: scale already tared for this cup (%ld dg)",
                 static_cast<long>(tare_.outputDg()));
        break;
    }
}

void Bridge::clientLeft(uint32_t nowMs)
{
    clientConnected_ = false;
    identifier_.disconnected();
    loggedKind_ = ClientKind::None;
    autoTare_.reset();
    if (tareClient_) {
        tare_.clientDisconnected(nowMs);
        tareClient_ = false;
    }
}

bool Bridge::watchForCup() const
{
    return settings_.tareOnCupPlaced() && (!clientConnected_ || identifier_.kind() == ClientKind::Other);
}

std::optional<scale::Command> Bridge::toScaleCommand(acaia::Command command)
{
    switch (command) {
    case acaia::Command::Tare:
        return scale::Command::Tare;
    case acaia::Command::TimerStart:
        return scale::Command::TimerStart;
    case acaia::Command::TimerStop:
        return scale::Command::TimerStop;
    case acaia::Command::TimerReset:
        return scale::Command::TimerReset;
    default:
        return std::nullopt;
    }
}

void Bridge::onScaleConnected(std::string_view driverName)
{
    ESP_LOGI(TAG, "scale connected: %.*s", static_cast<int>(driverName.size()), driverName.data());
    scaleName_.assign(driverName.data(), driverName.size());
    acaia_.startAdvertising();
}

void Bridge::onScaleDisconnected()
{
    ESP_LOGI(TAG, "scale gone");
    scaleName_.clear();
    if (!options_.advertiseWithoutScale) {
        acaia_.stopAdvertising();
        acaia_.disconnectClient();
    }
}

void Bridge::tare(const char* reason, uint32_t nowMs)
{
    // Zeroed at once in software; the real scale's tare follows a few readings later.
    tareStartMs_ = nowMs;
    ESP_LOGI(TAG, "%s: weight zeroed%s", reason, settings_.tareScale ? "" : " (software only)");
    sendScaleTareIfDue(nowMs);
}

void Bridge::sendScaleTareIfDue(uint32_t nowMs)
{
    if (!tare_.scaleTareDue(nowMs)) {
        return;
    }
    const bool ok = scale_.send(scale::Command::Tare);
    tare_.scaleTareSent(ok, nowMs);
    if (ok) {
        ESP_LOGI(TAG, "scale tare sent (attempt %d, +%lu ms)", tare_.sends(), static_cast<unsigned long>(nowMs - tareStartMs_));
    } else {
        ESP_LOGW(TAG, "scale tare not sent (attempt %d)", tare_.sends());
    }
}

void Bridge::onReading(const scale::Reading& reading)
{
    bool handedOver = false;
    const int32_t weightDg = tare_.apply(reading.weightDg, &handedOver);
    if (handedOver) {
        ESP_LOGI(TAG, "scale tare took effect after %lu ms", static_cast<unsigned long>(uptimeMs() - tareStartMs_));
    }
    ESP_LOGD(TAG, "weight %ld.%ld g", static_cast<long>(weightDg / 10), static_cast<long>(std::abs(weightDg % 10)));
    acaia_.sendWeight(weightDg);

    // Only between shots: during one the machine is connected.
    const uint32_t now = uptimeMs();
    if (watchForCup() && autoTare_.update(weightDg, now)) {
        tare_.tare(now);
        tare("cup placed or removed", now);
    }
}

void Bridge::onAcaiaCommand(acaia::Command command)
{
    if (xQueueSend(commands_, &command, 0) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full, dropping %s", acaia::toString(command));
    }
}

} // namespace bridge
