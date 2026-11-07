#pragma once

#include <cstdint>

namespace status {

/** What the adapter is doing, as shown on the status LED. */
enum class State {
    SearchingScale,    // no real scale; fake Lunar hidden             -> red, blinking
    WaitingForMachine, // scale connected; fake Lunar advertising      -> yellow, steady
    Bridging,          // scale and coffee machine connected           -> blue, steady
    OtherClient,       // scale and another device (app, configurator) -> green, steady
    UpdateMode,        // waiting for firmware over Bluetooth          -> purple, steady
    Updating,          // receiving firmware                           -> purple, fast blinking
};

/** Which states the LED shows. Update mode is always shown: it is entered on purpose and needs feedback. */
enum class LedMode : uint8_t {
    Standard = 0,   ///< every state
    ActiveOnly = 1, ///< only while a client is connected to the emulated Lunar
    Off = 2,        ///< nothing outside update mode
};

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool operator==(const Color&) const = default;
};

/** A missing scale wins: the machine alone gets no weight. */
State stateFrom(bool scaleConnected, bool clientConnected, bool clientIsMachine = true);

/** LED colour for `state` at time `nowMs`, scaled to `brightness` (0-255); off when `mode` hides the state. */
Color colorFor(State state, uint32_t nowMs, uint8_t brightness, LedMode mode = LedMode::Standard);

} // namespace status
