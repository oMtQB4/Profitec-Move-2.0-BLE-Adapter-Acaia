#pragma once

#include <cstdint>
#include <cstdlib>
#include <optional>

#include "bridge/AutoTare.hpp"
#include "bridge/SoftwareTare.hpp"

namespace bridge {

/**
 * When the relayed weight is zeroed and when the real scale gets a tare command.
 *
 * Pure logic: the caller passes the time and performs the scale writes. It makes
 * the shot-start tare robust against what a BLE link does in practice:
 *
 * - The weight is zeroed in software the moment the machine connects, unless
 *   the scale was already tared for this cup (auto-tare): the Move connects
 *   seconds into the shot, and zeroing then would drop the first espresso.
 * - The tare command to the scale is retried when the write fails, and resent
 *   when the readings don't show the tare in time (lost write, busy scale).
 * - A machine that drops the connection and reconnects within the grace period
 *   is still in the same shot: the zero point is kept, not set again mid-shot.
 * - Without `tareScale` the real scale is left alone: the zero is a software
 *   offset only, and it stays until the next tare (the scale display keeps
 *   showing the full weight).
 */
class TareController {
public:
    static constexpr uint32_t kConfirmMs = 1500;     ///< resend if the scale hasn't tared by then
    static constexpr uint32_t kFailedRetryMs = 100;  ///< retry after a failed write
    static constexpr int kMaxSends = 3;
    static constexpr uint32_t kReconnectGraceMs = 10000;
    static constexpr uint32_t kLateTareWindowMs = 5000; ///< after the last send, a tare may still show up

    struct Options {
        /** Keep the zero on connect when less than a cup's weight is on the scale (auto-tare on). */
        bool keepTaredZeroOnConnect = false;
        /** Send tare commands to the real scale; otherwise zero in software only. */
        bool tareScale = true;
    };

    enum class Connect {
        Zeroed,        ///< weight zeroed, scale tare started
        KeptReconnect, ///< same shot as before the dropout
        KeptTared,     ///< scale already tared for this cup
    };

    TareController() = default;
    explicit TareController(Options options) : options_(options) {}

    /** Takes effect with the next connect or tare. */
    void setOptions(Options options) { options_ = options; }

    Connect clientConnected(uint32_t nowMs)
    {
        if (disconnectedAtMs_ && nowMs - *disconnectedAtMs_ < kReconnectGraceMs && zeroActive_) {
            disconnectedAtMs_.reset();
            return Connect::KeptReconnect;
        }
        disconnectedAtMs_.reset();
        if (options_.keepTaredZeroOnConnect && std::abs(software_.outputDg()) < AutoTare::kMinLoadDg) {
            return Connect::KeptTared;
        }
        tare(nowMs);
        return Connect::Zeroed;
    }

    void clientDisconnected(uint32_t nowMs) { disconnectedAtMs_ = nowMs; }

    /** Zeroes the weight now and starts taring the real scale. */
    void tare(uint32_t nowMs)
    {
        zeroActive_ = true;
        sends_ = 0;
        nextSendMs_ = nowMs;
        if (options_.tareScale) {
            software_.zeroAndAwaitScaleTare();
        } else {
            software_.zero(); // scaleTareDue() stays false
        }
    }

    /** True when a tare command should be sent to the real scale now. */
    bool scaleTareDue(uint32_t nowMs) const
    {
        if (!zeroActive_ || !options_.tareScale || sends_ >= kMaxSends || static_cast<int32_t>(nowMs - nextSendMs_) < 0) {
            return false;
        }
        // Always send once; resend only while a tare is still expected.
        return sends_ == 0 || (software_.awaitingScaleTare() && !software_.scaleTared());
    }

    /** Reports the result of the write scaleTareDue() asked for. */
    void scaleTareSent(bool ok, uint32_t nowMs)
    {
        sends_++;
        lastSendMs_ = nowMs;
        nextSendMs_ = nowMs + (ok ? kConfirmMs : kFailedRetryMs);
    }

    /** Ends tare tracking when no more scale tares can show up, and the shot after the grace period. */
    void tick(uint32_t nowMs)
    {
        const bool sendsDone = software_.scaleTared() || sends_ >= kMaxSends;
        if (software_.awaitingScaleTare() && sends_ > 0 && sendsDone && nowMs - lastSendMs_ >= kLateTareWindowMs) {
            software_.stopAwaiting(); // a weight drop from now on is real, e.g. the cup taken off
        }
        if (disconnectedAtMs_ && nowMs - *disconnectedAtMs_ >= kReconnectGraceMs) {
            disconnectedAtMs_.reset();
            if (options_.tareScale) {
                software_.clear(); // the scale has its own zero by now
            }
            zeroActive_ = false;
        }
    }

    int32_t apply(int32_t rawDg, bool* handedOver = nullptr) { return software_.apply(rawDg, handedOver); }

    int32_t outputDg() const { return software_.outputDg(); }
    int sends() const { return sends_; }
    bool scaleTared() const { return software_.scaleTared(); }

private:
    Options options_;
    SoftwareTare software_;
    std::optional<uint32_t> disconnectedAtMs_;
    uint32_t nextSendMs_ = 0;
    uint32_t lastSendMs_ = 0;
    int sends_ = 0;
    bool zeroActive_ = false;
};

} // namespace bridge
