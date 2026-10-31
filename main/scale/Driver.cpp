#include "scale/Driver.hpp"

namespace scale {

void Driver::tick(Transport& transport, uint32_t nowMs)
{
    nowMs_ = nowMs;
    // Send due writes in the order they were scheduled.
    std::size_t kept = 0;
    for (std::size_t i = 0; i < pendingCount_; i++) {
        if (static_cast<int32_t>(nowMs - pending_[i].dueMs) >= 0) {
            transport.write(pending_[i].bytes.bytes());
        } else {
            pending_[kept++] = pending_[i];
        }
    }
    pendingCount_ = kept;
    onTick(transport, nowMs);
}

bool Driver::schedule(uint32_t delayMs, std::span<const uint8_t> bytes)
{
    if (pendingCount_ >= pending_.size()) {
        return false;
    }
    Pending& pending = pending_[pendingCount_];
    pending.dueMs = nowMs_ + delayMs;
    pending.bytes = Packet<24>{};
    for (uint8_t b : bytes) {
        pending.bytes.push(b);
    }
    pendingCount_++;
    return true;
}

} // namespace scale
