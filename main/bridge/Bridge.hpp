#pragma once

#include <optional>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "acaia/AcaiaPeripheral.hpp"
#include "bridge/AutoTare.hpp"
#include "bridge/ClientIdentifier.hpp"
#include "bridge/TareController.hpp"
#include "config/Settings.hpp"
#include "scale/ScaleClient.hpp"

namespace bridge {

/**
 * Mediator between the real scale and the emulated Acaia scale.
 *
 * - Scale readings become Acaia weight notifications.
 * - Machine commands (tare, timer) are queued and forwarded to the scale.
 * - Unless `advertiseWithoutScale`, the emulated scale only advertises while a
 *   real scale is connected, like a real scale that is switched off.
 * - Tells the coffee machine apart from other clients (ClientIdentifier).
 * - Auto-tare (config::Settings): the weight reads 0 when the machine connects
 *   (shot start), and in the CupPlaced mode already when a cup is put down,
 *   like a Lunar in its espresso modes; the machine connects only seconds into
 *   the shot. The real scale is tared too unless `tareScale` is off (see
 *   TareController for retries and reconnects).
 */
class Bridge : public scale::Listener, public acaia::CommandSink {
public:
    struct Options {
        bool advertiseWithoutScale = false;
    };

    Bridge(scale::ScaleClient& scale, acaia::AcaiaPeripheral& acaia, Options options,
           const config::Settings& settings, std::optional<PeerAddress> knownMachine);
    ~Bridge() override;

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    void begin();
    /** Tracks the machine connection and forwards queued commands. Call from the app task. */
    void poll();

    void applySettings(const config::Settings& settings);
    void forgetMachine();
    /** A newly learned machine address to persist, once. */
    std::optional<PeerAddress> takeLearnedMachine() { return identifier_.takeLearned(); }

    ClientKind clientKind() const { return identifier_.kind(); }
    const std::optional<PeerAddress>& knownMachine() const { return identifier_.knownMachine(); }
    /** Driver name of the connected scale; empty without one. */
    const std::string& scaleName() const { return scaleName_; }

    static std::optional<scale::Command> toScaleCommand(acaia::Command command);

    // scale::Listener (app task)
    void onScaleConnected(std::string_view driverName) override;
    void onScaleDisconnected() override;
    void onReading(const scale::Reading& reading) override;

    // acaia::CommandSink (BLE host task)
    void onAcaiaCommand(acaia::Command command) override;

private:
    void trackClient(uint32_t nowMs);
    void clientArrived(uint32_t nowMs);
    void clientLeft(uint32_t nowMs);
    /** Cup detection runs between shots: without a client, or with one that isn't the machine. */
    bool watchForCup() const;
    void tare(const char* reason, uint32_t nowMs);
    void sendScaleTareIfDue(uint32_t nowMs);

    scale::ScaleClient& scale_;
    acaia::AcaiaPeripheral& acaia_;
    const Options options_;
    config::Settings settings_;
    QueueHandle_t commands_;
    TareController tare_;
    AutoTare autoTare_;
    ClientIdentifier identifier_;
    std::string scaleName_;
    uint32_t tareStartMs_ = 0;
    uint32_t connection_ = 0;      // AcaiaPeripheral::connectionCount() of the tracked client
    bool clientConnected_ = false;
    bool tareClient_ = false;      // the tracked client got the connect tare (machine or maybe machine)
    ClientKind loggedKind_ = ClientKind::None;
};

} // namespace bridge
