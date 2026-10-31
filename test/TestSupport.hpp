#pragma once

/* Fakes and helpers shared by the driver tests. */

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "unity.h"

#include "scale/Driver.hpp"

using Bytes = std::vector<uint8_t>;

/** "AA 02 31 31" -> {0xAA, 0x02, 0x31, 0x31} */
inline Bytes hex(std::string_view text)
{
    Bytes out;
    int high = -1;
    for (char c : text) {
        int v;
        if (c >= '0' && c <= '9') {
            v = c - '0';
        } else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f') {
            v = (c | 0x20) - 'a' + 10;
        } else {
            continue;
        }
        if (high < 0) {
            high = v;
        } else {
            out.push_back(static_cast<uint8_t>(high << 4 | v));
            high = -1;
        }
    }
    return out;
}

#define TEST_ASSERT_BYTES(expectedHex, actual)                                                               \
    do {                                                                                                     \
        const Bytes expected_ = hex(expectedHex);                                                            \
        const auto actualRef_ = (actual); /* evaluate once, by value: may refer into a temporary */          \
        const Bytes actual_(std::begin(actualRef_), std::end(actualRef_));                                   \
        TEST_ASSERT_EQUAL_MESSAGE(expected_.size(), actual_.size(), "length");                               \
        TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_.data(), actual_.data(), expected_.size());                    \
    } while (0)

/** Records writes instead of sending them over BLE; serves scripted reads. */
class FakeTransport : public scale::Transport {
public:
    bool write(std::span<const uint8_t> data) override
    {
        writes.emplace_back(data.begin(), data.end());
        return true;
    }

    std::optional<std::vector<uint8_t>> read(const char* characteristicUuid) override
    {
        const auto it = reads.find(characteristicUuid);
        return it == reads.end() ? std::nullopt : std::optional<std::vector<uint8_t>>(it->second);
    }

    std::vector<Bytes> writes;
    std::map<std::string, Bytes> reads;
};

class RecordingListener : public scale::Listener {
public:
    void onScaleConnected(std::string_view) override {}
    void onScaleDisconnected() override {}
    void onReading(const scale::Reading& reading) override { readings.push_back(reading.weightDg); }
    std::vector<int32_t> readings;
};

/** Advertisement with only a name. */
inline scale::Advertisement named(std::string_view name)
{
    return scale::Advertisement{name, {}};
}

/** Decodes through the driver's public interface; empty if nothing was published. */
inline std::optional<int32_t> feed(scale::Driver& driver, const Bytes& frame)
{
    RecordingListener listener;
    driver.setListener(&listener);
    driver.onNotify(frame);
    driver.setListener(nullptr);
    if (listener.readings.empty()) {
        return std::nullopt;
    }
    return listener.readings.back();
}
