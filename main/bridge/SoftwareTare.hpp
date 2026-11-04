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
 * once. The offset covers that gap and is adjusted whenever the raw readings
 * show the scale has zeroed itself, so the relayed weight stays continuous.
 * A tare command may be sent more than once; each one that takes effect is
 * absorbed the same way.
 */
class SoftwareTare {
public:
    /** A repeated scale tare must drop the reading by at least this much (0.1 g). */
    static constexpr int32_t kMinRepeatStepDg = 30;

    /**
     * Remembers the raw reading and returns it relative to the zero point.
     * Returns true in `handedOver` when this reading showed a scale tare.
     */
    int32_t apply(int32_t rawDg, bool* handedOver = nullptr)
    {
        const bool done = awaitingScaleTare_ && (scaleTared_ ? isRepeatedTare(rawDg) : isFirstTare(rawDg));
        if (done) {
            // The scale has zeroed itself, possibly late with espresso already in the
            // cup, which it then zeroed as well. Keep the output continuous.
            const int32_t lastOutputDg = lastRawDg_ - offsetDg_;
            offsetDg_ = rawDg - lastOutputDg;
            scaleTared_ = true;
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
        scaleTared_ = false;
    }

    /** Like zero(), then follows the real scale's tare(s) until clear() or the next zero. */
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
        scaleTared_ = false;
    }

    /** Stops following scale tares; the current offset stays. */
    void stopAwaiting() { awaitingScaleTare_ = false; }

    /** The last reading relative to the zero point. */
    int32_t outputDg() const { return lastRawDg_ - offsetDg_; }

    /** True while scale tares are followed (from zeroAndAwaitScaleTare() until stopAwaiting(), clear() or zero()). */
    bool awaitingScaleTare() const { return awaitingScaleTare_; }
    /** True once the real scale's tare has shown up in the readings. */
    bool scaleTared() const { return scaleTared_; }

private:
    // The scale reads closer to its own new zero than to the old one. Measured
    // against the zero point, so a scale that settles over a few readings counts too.
    bool isFirstTare(int32_t rawDg) const { return std::abs(rawDg) < std::abs(rawDg - offsetDg_); }

    // After the first tare a repeated command zeroes the scale again: the reading
    // falls in one step to less than half. In a shot the weight only grows.
    bool isRepeatedTare(int32_t rawDg) const
    {
        return lastRawDg_ - rawDg >= kMinRepeatStepDg && 2 * std::abs(rawDg) < lastRawDg_;
    }

    int32_t lastRawDg_ = 0;
    int32_t offsetDg_ = 0;
    bool awaitingScaleTare_ = false;
    bool scaleTared_ = false;
};

} // namespace bridge
