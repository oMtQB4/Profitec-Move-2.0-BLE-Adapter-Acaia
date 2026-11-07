#pragma once

#include <cstdint>
#include <optional>

#include <led_strip.h>

#include "status/LedPattern.hpp"

namespace status {

/** Shows the adapter state on a single addressable RGB LED (WS2812). */
class StatusLed {
public:
    StatusLed(int gpio, uint8_t brightness);
    ~StatusLed();

    StatusLed(const StatusLed&) = delete;
    StatusLed& operator=(const StatusLed&) = delete;

    void configure(LedMode mode, uint8_t brightness)
    {
        mode_ = mode;
        brightness_ = brightness;
    }

    /** Call periodically; only talks to the LED when the colour changes. */
    void update(State state, uint32_t nowMs);

private:
    led_strip_handle_t strip_ = nullptr;
    uint8_t brightness_;
    LedMode mode_ = LedMode::Standard;
    std::optional<Color> shown_;
};

} // namespace status
