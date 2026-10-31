#include "scale/drivers/DecentScale.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"fff0", "fff4", "36f5", WriteMode::Auto};

constexpr uint8_t kModel = 0x03;
constexpr uint8_t kWeightStable = 0xCE;
constexpr uint8_t kWeightChanging = 0xCA;
constexpr uint8_t kLed = 0x0A;
constexpr uint8_t kTimer = 0x0B;
constexpr uint8_t kTare = 0x0F;

constexpr uint32_t kHeartbeatPeriodMs = 2000;

} // namespace

bool DecentScale::matches(const Advertisement& advertisement)
{
    for (std::string_view prefix : {"Decent Scale", "ButtsHaus Scale", "EspressiScale"}) {
        if (startsWithIgnoreCase(advertisement.name, prefix)) {
            return true;
        }
    }
    return false;
}

std::optional<Reading> DecentScale::decode(std::span<const uint8_t> data)
{
    if (data.size() < 7 || data[0] != kModel || (data[1] != kWeightStable && data[1] != kWeightChanging)) {
        return std::nullopt;
    }
    return Reading{static_cast<int16_t>(bytes::be16(data, 2))};
}

DecentScale::Frame DecentScale::frame(uint8_t command, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4)
{
    Frame f{kModel, command, d1, d2, d3, d4, 0};
    f[6] = bytes::xorOf(std::span<const uint8_t>(f).first(6));
    return f;
}

std::optional<DecentScale::Frame> DecentScale::encode(Command command, uint8_t tareCounter)
{
    switch (command) {
    case Command::Tare:
        return frame(kTare, tareCounter, 0x00, 0x00, 0x01);
    case Command::TimerStart:
        return frame(kTimer, 0x03, 0x00, 0x00, 0x00);
    case Command::TimerStop:
        return frame(kTimer, 0x00, 0x00, 0x00, 0x00);
    case Command::TimerReset:
        return frame(kTimer, 0x02, 0x00, 0x00, 0x00);
    }
    return std::nullopt;
}

const GattProfile& DecentScale::profile() const
{
    return kProfile;
}

void DecentScale::onConnected(Transport& transport)
{
    // LED on, grams, heartbeat not required (Beanconqueror); heartbeats follow anyway.
    sendTwice(transport, frame(kLed, 0x01, 0x01, 0x00, 0x00));
}

void DecentScale::onTick(Transport& transport, uint32_t nowMs)
{
    if (!lastHeartbeatMs_ || nowMs - *lastHeartbeatMs_ >= kHeartbeatPeriodMs) {
        lastHeartbeatMs_ = nowMs;
        transport.write(frame(kLed, 0x03, 0xFF, 0xFF, 0x00));
    }
}

bool DecentScale::send(Command command, Transport& transport)
{
    const auto bytes = encode(command, tareCounter_);
    if (!bytes) {
        return false;
    }
    if (command == Command::Tare) {
        tareCounter_++;
    }
    return sendTwice(transport, *bytes);
}

} // namespace scale
