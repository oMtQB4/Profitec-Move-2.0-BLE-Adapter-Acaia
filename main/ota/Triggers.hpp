#pragma once

#include <cstdint>

/*
 * Ways to enter update mode by hand. Both need someone at the adapter: the
 * update service accepts firmware only in update mode. Pure logic, host-tested.
 */
namespace ota {

/** Fires once when a button has been held for `holdMs`; it has to be released before the next one. */
class LongPress {
public:
    explicit LongPress(uint32_t holdMs)
        : holdMs_(holdMs)
    {
    }

    /** True on the call where the hold time is reached. */
    bool update(bool pressed, uint32_t nowMs)
    {
        if (!pressed) {
            pressedSinceMs_ = kReleased;
            fired_ = false;
            return false;
        }
        if (pressedSinceMs_ == kReleased) {
            pressedSinceMs_ = nowMs;
        }
        if (!fired_ && nowMs - pressedSinceMs_ >= holdMs_) {
            fired_ = true;
            return true;
        }
        return false;
    }

private:
    static constexpr uint32_t kReleased = UINT32_MAX;

    uint32_t holdMs_;
    uint32_t pressedSinceMs_ = kReleased;
    bool fired_ = false;
};

/**
 * Counts power-ons in a row that were each cut off soon after boot (the
 * caller clears the stored count once the adapter has run for a while).
 * Reaching `required` enters update mode. Resets other than power-on (crash,
 * software restart, USB) leave the count alone.
 */
struct PowerCycleCount {
    uint8_t stored;        // value to persist
    bool enterUpdateMode;

    static PowerCycleCount onBoot(uint8_t stored, bool powerOnReset, uint8_t required)
    {
        if (required == 0) {
            return {0, false};
        }
        if (!powerOnReset) {
            return {stored, false};
        }
        const uint8_t count = stored < required ? static_cast<uint8_t>(stored + 1) : required;
        if (count >= required) {
            return {0, true};
        }
        return {count, false};
    }
};

} // namespace ota
