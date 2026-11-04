#pragma once

#include <optional>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "acaia/AcaiaPeripheral.hpp"
#include "bridge/AutoTare.hpp"
#include "bridge/TareController.hpp"
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
 *   start). The real scale is tared too (see TareController for retries and
 *   reconnects).
 * - With `autoTare`, placing a cup tares the scale before the shot, like a
 *   Lunar in its espresso modes; the machine connects only seconds later.
 */
class Bridge : public scale::Listener, public acaia::CommandSink {
public:
    struct Options {
        bool advertiseWithoutScale = false;
        bool tareOnClientConnect = true;
        bool autoTare = true;
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
    void tare(const char* reason, uint32_t nowMs);
    void sendScaleTareIfDue(uint32_t nowMs);

    scale::ScaleClient& scale_;
    acaia::AcaiaPeripheral& acaia_;
    const Options options_;
    QueueHandle_t commands_;
    TareController tare_;
    AutoTare autoTare_;
    uint32_t tareStartMs_ = 0;
    bool clientConnected_ = false;
};

} // namespace bridge
