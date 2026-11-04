#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "ota/AppImage.hpp"
#include "ota/OtaProtocol.hpp"

namespace ota {

/** Where the image goes: the inactive app slot on the device, a fake in tests. */
class FlashTarget {
public:
    virtual ~FlashTarget() = default;
    /** Largest image the slot takes. */
    virtual uint32_t capacity() const = 0;
    virtual Status begin(uint32_t imageSize) = 0;
    virtual Status write(std::span<const uint8_t> data) = 0;
    /** Verifies the written image and makes it the next boot target. */
    virtual Status finish() = 0;
    virtual void abort() = 0;
};

/** Sends notifications to the client. */
class Notifier {
public:
    virtual ~Notifier() = default;
    virtual void notify(std::span<const uint8_t> frame) = 0;
};

/**
 * One firmware transfer: checks commands, sequence numbers and the image
 * header before anything reaches the flash, and paces the client with
 * progress notifications. Pure logic, driven by OtaService on the app task.
 */
class OtaSession {
public:
    struct Options {
        std::string projectName;
        uint16_t chipId = AppImage::kChipIdEsp32s3;
        uint16_t window = 16;          // data packets the client may have in flight
        uint32_t timeoutMs = 10000;    // abort without data for this long
    };

    enum class State {
        Idle,
        Receiving,
        Done, // verified and set as boot target: restart now
    };

    OtaSession(FlashTarget& flash, Notifier& notifier, Options options);

    /** Update mode allows Begin; outside it the adapter only answers. */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /** `chunk` is the payload size the connection's MTU allows. */
    void onControl(std::span<const uint8_t> data, uint16_t chunk, uint32_t nowMs);
    void onData(std::span<const uint8_t> data, uint32_t nowMs);
    /** The client wrote more packets than the window allows. */
    void onOverflow();
    /** The client is gone: drop a transfer in progress. */
    void onDisconnect();
    void poll(uint32_t nowMs);

    State state() const { return state_; }
    uint32_t bytesWritten() const { return written_; }
    uint32_t imageSize() const { return imageSize_; }
    const std::string& imageVersion() const { return imageVersion_; }

private:
    void begin(std::span<const uint8_t> data, uint16_t chunk, uint32_t nowMs);
    void end();
    Status consume(std::span<const uint8_t> payload);
    Status validateHeader();
    void fail(Status status);
    void reset();

    FlashTarget& flash_;
    Notifier& notifier_;
    Options options_;
    bool enabled_ = false;
    bool dataRejected_ = false;
    State state_ = State::Idle;
    uint32_t imageSize_ = 0;
    uint32_t received_ = 0;
    uint32_t written_ = 0;
    uint16_t nextSeq_ = 0;
    uint16_t packetsSinceAck_ = 0;
    uint32_t lastActivityMs_ = 0;
    std::array<uint8_t, AppImage::kHeaderLength> header_{};
    std::string imageVersion_;
};

} // namespace ota
