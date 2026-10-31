#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "common/Packet.hpp"

namespace scale {

/** One weight sample from a physical scale. */
struct Reading {
    int32_t weightDg; // 0.1 g
};

/** Rounds finer scale resolutions to the 0.1 g the bridge relays. */
constexpr int32_t fromCentigrams(int32_t cg)
{
    return (cg >= 0 ? cg + 5 : cg - 5) / 10;
}

constexpr int32_t fromMilligrams(int32_t mg)
{
    return (mg >= 0 ? mg + 50 : mg - 50) / 100;
}

inline int32_t fromGrams(float grams)
{
    return static_cast<int32_t>(std::lround(grams * 10.0f));
}

/** Scale-independent commands the bridge can forward. */
enum class Command {
    Tare,
    TimerStart,
    TimerStop,
    TimerReset,
};

/** What a scan saw. */
struct Advertisement {
    std::string_view name;
    std::span<const uint8_t> payload; // raw advertising data + scan response
};

enum class WriteMode {
    Auto, // pick from the characteristic's properties, preferring write without response
    WithResponse,
    WithoutResponse,
};

/** GATT layout a driver needs. UUID strings in NimBLEUUID format ("fff0" or 128-bit). */
struct GattProfile {
    const char* service;
    const char* notify;
    const char* write; // nullptr for scales that take no commands
    WriteMode writeMode = WriteMode::Auto;
};

/** Link to the connected scale; implemented by the BLE client. */
class Transport {
public:
    virtual ~Transport() = default;
    virtual bool write(std::span<const uint8_t> data) = 0;
    /** Reads a characteristic of the profile's service. */
    virtual std::optional<std::vector<uint8_t>> read(const char* characteristicUuid) = 0;
};

/** Observer for everything a scale reports. */
class Listener {
public:
    virtual ~Listener() = default;
    virtual void onScaleConnected(std::string_view driverName) = 0;
    virtual void onScaleDisconnected() = 0;
    virtual void onReading(const Reading& reading) = 0;
};

/**
 * Protocol strategy for one scale model. Drivers only translate bytes; the BLE
 * client owns scanning, connecting and subscribing. All methods run on the
 * app task, never concurrently.
 *
 * To add a scale: subclass Driver, give it `static constexpr const char* kName`
 * and `static bool matches(const Advertisement&)`, and register it in
 * scale/drivers/Drivers.cpp.
 */
class Driver {
public:
    virtual ~Driver() = default;

    virtual std::string_view name() const = 0;
    virtual const GattProfile& profile() const = 0;

    /** Called by the client once notifications are enabled. */
    void connected(Transport& transport, uint32_t nowMs)
    {
        nowMs_ = nowMs;
        onConnected(transport);
    }

    /** Called by the client every ~50 ms: sends due scheduled writes, then onTick(). */
    void tick(Transport& transport, uint32_t nowMs);

    /** Called for every notification on the profile's notify characteristic. */
    virtual void onNotify(std::span<const uint8_t> data) = 0;

    /** Returns false if the scale does not support the command. */
    virtual bool send(Command command, Transport& transport) = 0;

    void setListener(Listener* listener) { listener_ = listener; }

protected:
    /** Handshake after subscribing. Use schedule() for delayed steps. */
    virtual void onConnected(Transport& transport) { (void)transport; }

    /** Periodic work such as heartbeats. */
    virtual void onTick(Transport& transport, uint32_t nowMs)
    {
        (void)transport;
        (void)nowMs;
    }

    void publish(const Reading& reading)
    {
        if (listener_ != nullptr) {
            listener_->onReading(reading);
        }
    }

    /** Queues a write `delayMs` from now. Returns false if the queue is full. */
    bool schedule(uint32_t delayMs, std::span<const uint8_t> bytes);

    /** Writes now and once more after `gapMs`; some scales drop single commands. */
    bool sendTwice(Transport& transport, std::span<const uint8_t> bytes, uint32_t gapMs = 200)
    {
        const bool sent = transport.write(bytes);
        schedule(gapMs, bytes);
        return sent;
    }

private:
    struct Pending {
        uint32_t dueMs;
        Packet<24> bytes;
    };

    Listener* listener_ = nullptr;
    uint32_t nowMs_ = 0;
    std::array<Pending, 8> pending_{};
    std::size_t pendingCount_ = 0;
};

/**
 * Base for drivers whose notifications decode without state: `Self` provides
 * `static constexpr const char* kName` and
 * `static std::optional<Reading> decode(std::span<const uint8_t>)`.
 */
template <class Self>
class DecodingDriver : public Driver {
public:
    std::string_view name() const override { return Self::kName; }

    void onNotify(std::span<const uint8_t> data) override
    {
        if (auto reading = Self::decode(data)) {
            publish(*reading);
        }
    }
};

} // namespace scale
