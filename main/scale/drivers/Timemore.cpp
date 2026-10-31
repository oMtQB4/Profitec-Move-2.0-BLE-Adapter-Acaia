#include "scale/drivers/Timemore.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kBlackMirrorProfile{"181d", "2a9d", "553f4e49-bf21-4468-9c6c-0e4fb5b17697",
                                          WriteMode::WithResponse};
constexpr GattProfile kA55AProfile{"fff0", "fff1", "fff2", WriteMode::WithoutResponse};

constexpr uint8_t kHeader1 = 0xA5;
constexpr uint8_t kHeader2 = 0x5A;
constexpr uint8_t kOpCommand = 0x03;
constexpr uint8_t kCmdWeight = 0x01;
constexpr uint8_t kCmdTimer = 0x02;
constexpr uint8_t kCmdTare = 0x0D;
constexpr std::size_t kHeaderLength = 6; // A5 5A op cmd lenHi lenLo

} // namespace

// --- Black Mirror ---

bool TimemoreBlackMirror::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "timemore scale");
}

std::optional<Reading> TimemoreBlackMirror::decode(std::span<const uint8_t> data)
{
    if (data.size() < 3) {
        return std::nullopt;
    }
    return Reading{static_cast<int16_t>(bytes::le16(data, 1))};
}

std::optional<uint8_t> TimemoreBlackMirror::encode(Command command)
{
    switch (command) {
    case Command::Tare:
        return 0x00;
    case Command::TimerStart:
        return 0x08;
    case Command::TimerStop:
        return 0x09;
    case Command::TimerReset:
        return 0x0A;
    }
    return std::nullopt;
}

const GattProfile& TimemoreBlackMirror::profile() const
{
    return kBlackMirrorProfile;
}

bool TimemoreBlackMirror::send(Command command, Transport& transport)
{
    const auto byte = encode(command);
    return byte && transport.write(std::span<const uint8_t>(&*byte, 1));
}

// --- A5 5A protocol (Basic 3, Dot) ---

namespace timemore {

uint16_t crc16Modbus(std::span<const uint8_t> data)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t b : data) {
        crc ^= b;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

Frame command(uint8_t cmd, std::span<const uint8_t> data)
{
    Frame frame{kHeader1, kHeader2, kOpCommand, cmd, static_cast<uint8_t>(data.size() >> 8),
                static_cast<uint8_t>(data.size())};
    for (uint8_t b : data) {
        frame.push(b);
    }
    const uint16_t crc = crc16Modbus(frame.bytes());
    frame.push(static_cast<uint8_t>(crc >> 8));
    frame.push(static_cast<uint8_t>(crc));
    return frame;
}

std::optional<Reading> decode(std::span<const uint8_t> data, bool checkCrc)
{
    if (data.size() < kHeaderLength || data[0] != kHeader1 || data[1] != kHeader2) {
        return std::nullopt;
    }
    const std::size_t length = bytes::be16(data, 4);
    if ((data[2] != 0x01 && data[2] != 0x02) || data[3] != kCmdWeight || length < 4 ||
        data.size() < kHeaderLength + length + 2) {
        return std::nullopt;
    }
    if (checkCrc && crc16Modbus(data.first(kHeaderLength + length)) != bytes::be16(data, kHeaderLength + length)) {
        return std::nullopt;
    }
    return Reading{static_cast<int32_t>(bytes::be32(data, kHeaderLength))};
}

std::optional<Frame> encode(Command command)
{
    const auto timer = [](uint8_t action) {
        const std::array<uint8_t, 1> data{action};
        return timemore::command(kCmdTimer, data);
    };
    switch (command) {
    case Command::Tare:
        return timemore::command(kCmdTare, {});
    case Command::TimerStart:
        return timer(0x01);
    case Command::TimerStop:
        return timer(0x02);
    case Command::TimerReset:
        return timer(0x03);
    }
    return std::nullopt;
}

} // namespace timemore

bool TimemoreBasic::matches(const Advertisement& advertisement)
{
    const std::string_view name = advertisement.name;
    return containsIgnoreCase(name, "basic3") || containsIgnoreCase(name, "basic 3") ||
           (containsIgnoreCase(name, "timemore") && containsIgnoreCase(name, "basic"));
}

const GattProfile& TimemoreBasic::profile() const
{
    return kA55AProfile;
}

bool TimemoreDot::matches(const Advertisement& advertisement)
{
    // Beanconqueror also matches a bare "dot", which is too broad for a scanner.
    const std::string_view name = advertisement.name;
    return containsIgnoreCase(name, "tes017") ||
           (containsIgnoreCase(name, "timemore") && containsIgnoreCase(name, "dot"));
}

const GattProfile& TimemoreDot::profile() const
{
    return kA55AProfile;
}

} // namespace scale
