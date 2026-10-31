#include "bridge/Bridge.hpp"

#include <cstdlib>

#include <esp_log.h>
#include <esp_timer.h>

namespace bridge {

namespace {

constexpr const char* TAG = "bridge";
constexpr UBaseType_t kCommandQueueLength = 8;

} // namespace

Bridge::Bridge(scale::ScaleClient& scale, acaia::AcaiaPeripheral& acaia, Options options)
    : scale_(scale)
    , acaia_(acaia)
    , options_(options)
    , commands_(xQueueCreate(kCommandQueueLength, sizeof(acaia::Command)))
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
    // The Move 2.0 connects when a shot starts and sends no tare; a real Lunar
    // zeroes itself in its espresso modes. Polled every 10 ms, this runs before
    // the machine has subscribed to the first weight notification.
    const bool clientConnected = acaia_.isClientConnected();
    if (clientConnected != clientConnected_) {
        clientConnected_ = clientConnected;
        if (clientConnected && options_.tareOnClientConnect) {
            tare("client connected");
        } else if (!clientConnected) {
            tare_.clear();
        }
    }

    acaia::Command command;
    while (xQueueReceive(commands_, &command, 0) == pdTRUE) {
        if (command == acaia::Command::Tare) {
            tare("tare command");
            continue;
        }
        const auto scaleCommand = toScaleCommand(command);
        if (!scaleCommand || !scale_.send(*scaleCommand)) {
            ESP_LOGW(TAG, "could not forward %s to scale", acaia::toString(command));
        }
    }
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

void Bridge::tare(const char* reason)
{
    // Zero at once in software; the real scale's tare follows a few readings later.
    if (scale_.send(scale::Command::Tare)) {
        tare_.zeroAndAwaitScaleTare();
        tareSentUs_ = esp_timer_get_time();
        ESP_LOGI(TAG, "%s: weight zeroed, scale tare sent", reason);
    } else {
        tare_.zero();
        ESP_LOGI(TAG, "%s: weight zeroed (scale not tared)", reason);
    }
}

void Bridge::onReading(const scale::Reading& reading)
{
    bool handedOver = false;
    const int32_t weightDg = tare_.apply(reading.weightDg, &handedOver);
    if (handedOver) {
        ESP_LOGI(TAG, "scale tare took effect after %lld ms", (esp_timer_get_time() - tareSentUs_) / 1000);
    }
    ESP_LOGD(TAG, "weight %ld.%ld g", static_cast<long>(weightDg / 10), static_cast<long>(std::abs(weightDg % 10)));
    acaia_.sendWeight(weightDg);
}

void Bridge::onAcaiaCommand(acaia::Command command)
{
    if (xQueueSend(commands_, &command, 0) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full, dropping %s", acaia::toString(command));
    }
}

} // namespace bridge
