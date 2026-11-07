#pragma once

#include <cstdint>
#include <optional>

#include "common/PeerAddress.hpp"

namespace bridge {

/** Who is connected to the emulated Lunar. */
enum class ClientKind : uint8_t {
    None = 0,
    Identifying = 1, ///< connected, not recognised yet
    Machine = 2,     ///< the coffee machine
    Other = 3,       ///< an app, the configurator, ...
};

const char* toString(ClientKind kind);

/**
 * Tells the coffee machine apart from other clients of the emulated Lunar.
 *
 * The Move 2.0 only listens: it subscribes to the weight and never writes
 * (no ident, no heartbeat, no tare). Acaia apps send an ident right after
 * subscribing; the configurator never subscribes to the weight. So a client
 * that subscribes and stays silent for kQuietMs is the machine.
 *
 * That takes a few seconds, but the machine connects only once the shot runs
 * and the weight has to be zeroed at once. So the machine's address is
 * learned (if it is a stable one) and recognises the machine the moment it
 * connects from then on.
 */
class ClientIdentifier {
public:
    static constexpr uint32_t kQuietMs = 3000;       ///< subscribed and silent this long: the machine
    static constexpr uint32_t kSubscribeMs = 5000;   ///< not subscribed by then: not the machine

    /** The machine address remembered from earlier (NVS), if any. */
    void setKnownMachine(std::optional<PeerAddress> address) { known_ = address; }
    const std::optional<PeerAddress>& knownMachine() const { return known_; }

    void connected(const PeerAddress& address, uint32_t nowMs)
    {
        address_ = address;
        connectedAtMs_ = nowMs;
        subscribedAtMs_.reset();
        kind_ = known_ && *known_ == address ? ClientKind::Machine : ClientKind::Identifying;
    }

    void disconnected()
    {
        kind_ = ClientKind::None;
        subscribedAtMs_.reset();
    }

    /** The client subscribed to the weight notifications. */
    void subscribed(uint32_t nowMs)
    {
        if (!subscribedAtMs_) {
            subscribedAtMs_ = nowMs;
        }
    }

    /** The client wrote an Acaia command (ident, heartbeat, tare, ...). */
    void wrote()
    {
        if (kind_ == ClientKind::Identifying) {
            kind_ = ClientKind::Other;
        }
    }

    void tick(uint32_t nowMs)
    {
        if (kind_ != ClientKind::Identifying) {
            return;
        }
        if (subscribedAtMs_ && nowMs - *subscribedAtMs_ >= kQuietMs) {
            kind_ = ClientKind::Machine;
            if (address_.isStable() && known_ != address_) {
                known_ = address_;
                learned_ = address_;
            }
        } else if (!subscribedAtMs_ && nowMs - connectedAtMs_ >= kSubscribeMs) {
            kind_ = ClientKind::Other;
        }
    }

    /** Forgets the machine; the next one is learned again. */
    void forgetMachine()
    {
        known_.reset();
        learned_.reset();
    }

    /** A newly learned machine address to persist, once. */
    std::optional<PeerAddress> takeLearned()
    {
        auto learned = learned_;
        learned_.reset();
        return learned;
    }

    ClientKind kind() const { return kind_; }
    const PeerAddress& address() const { return address_; }

private:
    std::optional<PeerAddress> known_;
    std::optional<PeerAddress> learned_;
    PeerAddress address_;
    ClientKind kind_ = ClientKind::None;
    uint32_t connectedAtMs_ = 0;
    std::optional<uint32_t> subscribedAtMs_;
};

inline const char* toString(ClientKind kind)
{
    switch (kind) {
    case ClientKind::None:
        return "none";
    case ClientKind::Identifying:
        return "identifying";
    case ClientKind::Machine:
        return "machine";
    case ClientKind::Other:
        return "other device";
    }
    return "?";
}

} // namespace bridge
