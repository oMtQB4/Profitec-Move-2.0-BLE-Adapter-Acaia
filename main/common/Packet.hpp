#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

/** Fixed-capacity byte buffer, so encoders never touch the heap. */
template <std::size_t Capacity>
class Packet {
public:
    Packet() = default;
    Packet(std::initializer_list<uint8_t> bytes)
    {
        for (uint8_t b : bytes) {
            push(b);
        }
    }

    bool push(uint8_t b)
    {
        if (size_ >= Capacity) {
            overflow_ = true;
            return false;
        }
        data_[size_++] = b;
        return true;
    }

    std::span<const uint8_t> bytes() const { return {data_.data(), size_}; }
    const uint8_t* data() const { return data_.data(); }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    /** True if a push was dropped because the buffer was full. */
    bool overflowed() const { return overflow_; }
    uint8_t operator[](std::size_t i) const { return data_[i]; }

private:
    std::array<uint8_t, Capacity> data_{};
    std::size_t size_ = 0;
    bool overflow_ = false;
};
