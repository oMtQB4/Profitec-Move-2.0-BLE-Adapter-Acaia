#pragma once

#include <cstdint>
#include <cstdlib>

namespace bridge {

/**
 * Zeroes readings instantly in software, optionally handing over to the real
 * scale's own tare once that has taken effect.
 *
 * A tare command to the scale takes effect only after a few readings; until
 * then the machine would see the full cup weight, which can end a shot at
 * once. The offset covers that gap and is dropped when the raw readings show
 * the scale has zeroed itself, so the relayed weight stays continuous.
 */
class SoftwareTare {
public:
    /**
     * Remembers the raw reading and returns it relative to the zero point.
     * Returns true in `handedOver` when this reading completed a hand-over.
     */
    int32_t apply(int32_t rawDg, bool* handedOver = nullptr)
    {
        const bool done = awaitingScaleTare_ && std::abs(rawDg) < std::abs(rawDg - offsetDg_);
        if (done) {
            // The scale reads closer to its own zero than to the old one: it has tared.
            // It may have done so late, with espresso already in the cup, which it then
            // zeroed as well. Keep the output continuous instead of dropping the offset.
            const int32_t lastOutputDg = lastRawDg_ - offsetDg_;
            offsetDg_ = rawDg - lastOutputDg;
            awaitingScaleTare_ = false;
        }
        lastRawDg_ = rawDg;
        if (handedOver != nullptr) {
            *handedOver = done;
        }
        return rawDg - offsetDg_;
    }

    /** Makes the last raw reading the new zero. */
    void zero()
    {
        offsetDg_ = lastRawDg_;
        awaitingScaleTare_ = false;
    }

    /** Like zero(), then drops the offset once the real scale's tare shows up. */
    void zeroAndAwaitScaleTare()
    {
        zero();
        awaitingScaleTare_ = offsetDg_ != 0;
    }

    /** Passes raw readings through again. */
    void clear()
    {
        offsetDg_ = 0;
        awaitingScaleTare_ = false;
    }

    bool awaitingScaleTare() const { return awaitingScaleTare_; }

private:
    int32_t lastRawDg_ = 0;
    int32_t offsetDg_ = 0;
    bool awaitingScaleTare_ = false;
};

} // namespace bridge
