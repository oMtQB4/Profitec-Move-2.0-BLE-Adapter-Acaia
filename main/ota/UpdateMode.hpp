#pragma once

#include <cstdint>

#include "ota/Triggers.hpp"

namespace ota {

/**
 * Update mode is a separate boot: the bridge doesn't run, the adapter only
 * waits for firmware. Switching between the modes restarts the chip.
 *
 * Ways in: hold the button (BOOT on DevKitC boards) for 3 s while running,
 * or power the adapter on `powerCycles` times in a row, each time for less
 * than 10 s. Holding the button in update mode leaves it again.
 */
class UpdateMode {
public:
    struct Options {
        int buttonGpio;      // < 0: no button
        uint8_t powerCycles; // 0: off
    };

    explicit UpdateMode(Options options);

    /** Once at boot, after nvs_flash_init(): true to start in update mode. */
    bool decideAtBoot();

    /** Call from the app loop: true when the button asks to switch modes. */
    bool poll(uint32_t nowMs);

    /** Restarts into update mode or back into the bridge. */
    [[noreturn]] static void restart(bool updateMode);

private:
    static constexpr uint32_t kHoldMs = 3000;
    static constexpr uint32_t kStableAfterMs = 10000;

    void storePowerCycles(uint8_t count);

    Options options_;
    LongPress button_{kHoldMs};
    bool powerCyclesPending_ = false;
};

} // namespace ota
