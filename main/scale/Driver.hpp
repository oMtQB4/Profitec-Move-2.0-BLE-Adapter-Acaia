#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace scale {

/** One weight sample from a physical scale. */
struct Reading {
    int32_t weightDg; // 0.1 g
};

/** Scale-independent commands the bridge can forward. */
enum class Command {
    Tare,
    TimerStart,
    TimerStop,
    TimerReset,
};

/** What a scan saw. Grows when a driver needs more than the name to match. */
struct Advertisement {
    std::string_view name;
};

/** GATT layout a driver needs. UUID strings in NimBLEUUID format ("fff0" or 128-bit). */
struct GattProfile {
    const char* service;
    const char* notify;
    const char* write;
    bool writeWithResponse;
};

/** Byte sink towards the connected scale; implemented by the BLE client. */
class Transport {
public:
    virtual ~Transport() = default;
    virtual bool write(std::span<const uint8_t> data) = 0;
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

    /** Called once notifications are enabled; send any handshake here. */
    virtual void onConnected(Transport& transport) { (void)transport; }

    /** Called for every notification on the profile's notify characteristic. */
    virtual void onNotify(std::span<const uint8_t> data) = 0;

    /** Returns false if the scale does not support the command. */
    virtual bool send(Command command, Transport& transport) = 0;

    /** Called periodically (~100 ms) while connected, e.g. for heartbeats. */
    virtual void onTick(Transport& transport) { (void)transport; }

    void setListener(Listener* listener) { listener_ = listener; }

protected:
    void publish(const Reading& reading)
    {
        if (listener_ != nullptr) {
            listener_->onReading(reading);
        }
    }

private:
    Listener* listener_ = nullptr;
};

} // namespace scale
