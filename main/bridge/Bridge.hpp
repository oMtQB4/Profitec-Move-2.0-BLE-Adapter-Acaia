#pragma once

#include <optional>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "acaia/AcaiaPeripheral.hpp"
#include "bridge/SoftwareTare.hpp"
#include "scale/ScaleClient.hpp"

namespace bridge {

/**
 * Mediator between the real scale and the emulated Acaia scale.
 *
 * - Scale readings become Acaia weight notifications.
 * - Machine commands (tare, timer) are queued and forwarded to the scale.
 * - Unless `advertiseWithoutScale`, the emulated scale only advertises while a
 *   real scale is connected, like a real scale that is switched off.
 * - With `tareOnClientConnect`, weight reads 0 when the machine connects (shot
 *   start), like a Lunar in an auto-tare mode. The real scale is tared too.
 */
class Bridge : public scale::Listener, public acaia::CommandSink {
public:
    struct Options {
        bool advertiseWithoutScale = false;
        bool tareOnClientConnect = true;
    };

    Bridge(scale::ScaleClient& scale, acaia::AcaiaPeripheral& acaia, Options options);
    ~Bridge() override;

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    void begin();
    /** Tracks the machine connection and forwards queued commands. Call from the app task. */
    void poll();

    static std::optional<scale::Command> toScaleCommand(acaia::Command command);

    // scale::Listener (app task)
    void onScaleConnected(std::string_view driverName) override;
    void onScaleDisconnected() override;
    void onReading(const scale::Reading& reading) override;

    // acaia::CommandSink (BLE host task)
    void onAcaiaCommand(acaia::Command command) override;

private:
    void tare(const char* reason);

    scale::ScaleClient& scale_;
    acaia::AcaiaPeripheral& acaia_;
    const Options options_;
    QueueHandle_t commands_;
    SoftwareTare tare_;
    int64_t tareSentUs_ = 0;
    bool clientConnected_ = false;
};

} // namespace bridge
