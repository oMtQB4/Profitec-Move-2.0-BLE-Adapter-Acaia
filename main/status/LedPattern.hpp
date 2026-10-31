#pragma once

#include <cstdint>

namespace status {

/** What the adapter is doing, as shown on the status LED. */
enum class State {
    SearchingScale,    // no real scale; fake Lunar hidden          -> red, blinking
    WaitingForMachine, // scale connected; fake Lunar advertising   -> yellow, steady
    Bridging,          // scale and coffee machine connected        -> blue, steady
};

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool operator==(const Color&) const = default;
};

/** A missing scale wins: the machine alone gets no weight. */
State stateFrom(bool scaleConnected, bool machineConnected);

/** LED colour for `state` at time `nowMs`, scaled to `brightness` (0-255). */
Color colorFor(State state, uint32_t nowMs, uint8_t brightness);

} // namespace status
