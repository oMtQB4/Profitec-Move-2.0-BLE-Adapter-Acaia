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

} // namespace

Bridge::Bridge(scale::ScaleClient& scale, acaia::AcaiaPeripheral& acaia, Options options)
    : scale_(scale)
    , acaia_(acaia)
    , options_(options)
    , commands_(xQueueCreate(kCommandQueueLength, sizeof(acaia::Command)))
    , tare_(TareController::Options{.keepTaredZeroOnConnect = options.autoTare})
{
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

    // The Move 2.0 connects when a shot starts and sends no tare; a real Lunar
    // zeroes itself in its espresso modes. Polled every 10 ms, this runs before
    // the machine has subscribed to the first weight notification.
    const bool clientConnected = acaia_.isClientConnected();
    if (clientConnected != clientConnected_) {
        clientConnected_ = clientConnected;
        autoTare_.reset();
        if (clientConnected && options_.tareOnClientConnect) {
            switch (tare_.clientConnected(now)) {
            case TareController::Connect::Zeroed:
                tare("machine connected", now);
                break;
            case TareController::Connect::KeptReconnect:
                ESP_LOGI(TAG, "machine reconnected within the shot: keeping the zero point");
                break;
            case TareController::Connect::KeptTared:
                ESP_LOGI(TAG, "machine connected: scale already tared for this cup (%ld dg)",
                         static_cast<long>(tare_.outputDg()));
                break;
            }
        } else if (!clientConnected) {
            tare_.clientDisconnected(now);
        }
    }

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
    acaia_.startAdvertising();
}

void Bridge::onScaleDisconnected()
{
    ESP_LOGI(TAG, "scale gone");
    if (!options_.advertiseWithoutScale) {
        acaia_.stopAdvertising();
        acaia_.disconnectClient();
    }
}

void Bridge::tare(const char* reason, uint32_t nowMs)
{
    // Zeroed at once in software; the real scale's tare follows a few readings later.
    tareStartMs_ = nowMs;
    ESP_LOGI(TAG, "%s: weight zeroed", reason);
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
    if (options_.autoTare && !clientConnected_ && autoTare_.update(weightDg, now)) {
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
