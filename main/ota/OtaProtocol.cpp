#include "ota/OtaProtocol.hpp"

#include <algorithm>

namespace ota {

namespace {

constexpr size_t kMaxVersionLength = 32;

void putU16(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void putU32(std::vector<uint8_t>& out, uint32_t value)
{
    putU16(out, static_cast<uint16_t>(value));
    putU16(out, static_cast<uint16_t>(value >> 16));
}

uint32_t getU32(std::span<const uint8_t> data)
{
    return static_cast<uint32_t>(data[0]) | static_cast<uint32_t>(data[1]) << 8 |
           static_cast<uint32_t>(data[2]) << 16 | static_cast<uint32_t>(data[3]) << 24;
}

} // namespace

const char* toString(Status status)
{
    switch (status) {
    case Status::Ok: return "ok";
    case Status::NotInUpdateMode: return "not in update mode";
    case Status::InvalidCommand: return "invalid command";
    case Status::InvalidSize: return "invalid size";
    case Status::NotStarted: return "not started";
    case Status::SequenceError: return "sequence error";
    case Status::Overflow: return "overflow";
    case Status::InvalidImage: return "invalid image";
    case Status::TooMuchData: return "too much data";
    case Status::Incomplete: return "incomplete";
    case Status::FlashError: return "flash error";
    case Status::VerifyFailed: return "verify failed";
    case Status::Timeout: return "timeout";
    }
    return "unknown";
}

uint16_t chunkSizeForMtu(uint16_t mtu)
{
    constexpr uint16_t kAttWriteHeader = 3;
    constexpr uint16_t kMinMtu = 23;
    const uint16_t value = std::min<uint16_t>(std::max(mtu, kMinMtu) - kAttWriteHeader, kMaxAttributeLength);
    return value - kDataHeaderLength;
}

std::optional<Opcode> commandOpcode(std::span<const uint8_t> data)
{
    if (data.empty()) {
        return std::nullopt;
    }
    switch (static_cast<Opcode>(data[0])) {
    case Opcode::Begin:
        return data.size() == 5 ? std::optional(Opcode::Begin) : std::nullopt;
    case Opcode::End:
    case Opcode::Abort:
        return data.size() == 1 ? std::optional(static_cast<Opcode>(data[0])) : std::nullopt;
    default:
        return std::nullopt;
    }
}

std::optional<BeginCommand> parseBegin(std::span<const uint8_t> data)
{
    if (commandOpcode(data) != Opcode::Begin) {
        return std::nullopt;
    }
    return BeginCommand{getU32(data.subspan(1))};
}

std::optional<DataPacket> parseData(std::span<const uint8_t> data)
{
    if (data.size() <= kDataHeaderLength) {
        return std::nullopt;
    }
    return DataPacket{static_cast<uint16_t>(data[0] | data[1] << 8), data.subspan(kDataHeaderLength)};
}

std::vector<uint8_t> encodeBeginResponse(Status status, uint16_t chunk, uint16_t window)
{
    std::vector<uint8_t> out{static_cast<uint8_t>(Opcode::BeginResponse), static_cast<uint8_t>(status)};
    putU16(out, chunk);
    putU16(out, window);
    return out;
}

std::vector<uint8_t> encodeResponse(Opcode opcode, Status status)
{
    return {static_cast<uint8_t>(opcode), static_cast<uint8_t>(status)};
}

std::vector<uint8_t> encodeProgress(Status status, uint32_t bytes)
{
    std::vector<uint8_t> out{static_cast<uint8_t>(Opcode::Progress), static_cast<uint8_t>(status)};
    putU32(out, bytes);
    return out;
}

std::vector<uint8_t> encodeInfo(const Info& info)
{
    std::vector<uint8_t> out{kProtocolVersion,
                             static_cast<uint8_t>((info.updateMode ? 0x01 : 0) | (info.transferring ? 0x02 : 0))};
    putU16(out, info.chunk);
    putU16(out, info.window);
    putU32(out, info.capacity);
    const std::string_view version = info.firmwareVersion.substr(0, kMaxVersionLength);
    out.insert(out.end(), version.begin(), version.end());
    return out;
}

} // namespace ota
