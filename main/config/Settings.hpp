#pragma once

#include <cstdint>

#include "status/LedPattern.hpp"

namespace config {

/** When the adapter zeroes the weight by itself (the Move never sends a tare). */
enum class AutoTareMode : uint8_t {
    /**
     * When a cup is put down (or taken off) and the weight has settled, while
     * the machine is not connected. If the cup wasn't tared, the weight is
     * still zeroed when the machine connects.
     */
    CupPlaced = 0,
    /** Only when the coffee machine connects, i.e. a few seconds into the shot. */
    MachineConnects = 1,
};

/** Settings the user changes at runtime (configurator), stored in NVS. */
struct Settings {
    bool autoTare = true;
    AutoTareMode autoTareMode = AutoTareMode::CupPlaced;
    /** Also tare the real scale; otherwise the weight is zeroed in software only and the scale display keeps its own zero. */
    bool tareScale = true;
    status::LedMode ledMode = status::LedMode::Standard;
    uint8_t ledBrightness = 24; ///< 1-255

    bool operator==(const Settings&) const = default;

    /** Zero the weight when the machine connects. */
    bool tareOnMachineConnect() const { return autoTare; }
    /** Watch the weight for a cup being put down. */
    bool tareOnCupPlaced() const { return autoTare && autoTareMode == AutoTareMode::CupPlaced; }
};

} // namespace config
