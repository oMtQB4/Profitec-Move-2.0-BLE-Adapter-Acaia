#pragma once

#include <atomic>
#include <cstdint>

#include <esp_timer.h>

namespace ota {

/**
 * Rollback guard for a freshly updated app. The bootloader boots a new image
 * once in "pending verify" state; unless the app confirms itself before the
 * next reset, the bootloader goes back to the previous image.
 *
 * The app counts as healthy when its main loop is still running
 * kHealthyAfterMs after boot. If it hasn't confirmed by kDeadlineMs (hung
 * loop), a timer restarts the chip, which triggers the rollback.
 */
class AppValidity {
public:
    static constexpr uint32_t kHealthyAfterMs = 10000;
    static constexpr uint32_t kDeadlineMs = 60000;

    /** Once at boot. */
    void begin();
    /** From the app loop. */
    void poll(uint32_t nowMs);

private:
    static void onDeadline(void* arg);

    bool pending_ = false;
    std::atomic<bool> confirmed_{false};
    esp_timer_handle_t deadline_ = nullptr;
};

} // namespace ota
