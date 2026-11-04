#pragma once

#include <cstdint>
#include <cstdlib>
#include <optional>

namespace bridge {

/**
 * Detects a cup being placed on (or taken off) the scale, like the auto-tare of
 * a Lunar in its espresso modes. The Move 2.0 connects only seconds after the
 * shot has started and never tares, so the scale has to be zero by then.
 *
 * A change counts when the weight moves by at least kMinLoadDg within
 * kMaxChangeMs and then settles for kSettleMs. Pouring (water, espresso) is
 * slower than putting a cup down and does not trigger it.
 */
class AutoTare {
public:
    static constexpr int32_t kMinLoadDg = 200;      ///< 20 g
    static constexpr int32_t kStableDg = 5;         ///< ±0.5 g counts as steady
    static constexpr uint32_t kSettleMs = 800;
    static constexpr uint32_t kMaxChangeMs = 2000;

    /** Feeds a relayed reading. Returns true once when a placed or removed load has settled. */
    bool update(int32_t weightDg, uint32_t nowMs)
    {
        if (settled_) {
            const bool away = std::abs(weightDg - *settled_) > kStableDg;
            if (away && !changeStartMs_) {
                changeStartMs_ = nowMs;
            } else if (!away) {
                changeStartMs_.reset();
            }
        }
        if (!candidate_ || std::abs(weightDg - *candidate_) > kStableDg) {
            candidate_ = weightDg;
            candidateSinceMs_ = nowMs;
            reported_ = false;
            return false;
        }
        if (reported_ || nowMs - candidateSinceMs_ < kSettleMs) {
            return false;
        }
        reported_ = true;
        const bool changed = settled_ && changeStartMs_ && std::abs(*candidate_ - *settled_) >= kMinLoadDg
            && candidateSinceMs_ - *changeStartMs_ <= kMaxChangeMs;
        settled_ = changed ? 0 : *candidate_; // after the tare the scale reads 0
        changeStartMs_.reset();
        return changed;
    }

    /** Forgets the levels seen so far; the next steady weight becomes the reference. */
    void reset()
    {
        candidate_.reset();
        settled_.reset();
        changeStartMs_.reset();
        reported_ = false;
    }

private:
    std::optional<int32_t> candidate_;
    std::optional<int32_t> settled_;
    std::optional<uint32_t> changeStartMs_;
    uint32_t candidateSinceMs_ = 0;
    bool reported_ = false;
};

} // namespace bridge
