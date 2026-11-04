#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/*
 * Firmware update over BLE, protocol version 1. Pure encoding, no ESP-IDF, so
 * it is tested on the host. The browser updater (web/updater/protocol.js)
 * implements the client side; keep both in sync.
 *
 * GATT service kServiceUuid:
 *   Info    (read)            InfoV1 below
 *   Control (write, notify)   commands from the client, responses from the adapter
 *   Data    (write, write without response)
 *                             [seq u16][firmware bytes], seq counts from 0 per update
 *
 * All integers little endian. Every notification on Control is
 * [opcode][status][payload]:
 *
 *   client -> adapter            adapter -> client
 *   Begin [0x01][size u32]       [0x81][status][chunk u16][window u16]
 *   End   [0x02]                 [0x82][status]   (on Ok the adapter restarts)
 *   Abort [0x03]                 [0x83][status]
 *                                [0x90][status][bytes u32]  progress / data error
 *
 * Flow control: the client keeps at most `window` data packets
 * unacknowledged. The adapter sends a progress notification every window/2
 * packets and after the last one; `bytes` acknowledges all image bytes
 * received so far.
 */
namespace ota {

inline constexpr const char* kServiceUuid = "7e1a0001-8f3c-4b6e-9a51-3c7d2e4f6a10";
inline constexpr const char* kControlUuid = "7e1a0002-8f3c-4b6e-9a51-3c7d2e4f6a10";
inline constexpr const char* kDataUuid = "7e1a0003-8f3c-4b6e-9a51-3c7d2e4f6a10";
inline constexpr const char* kInfoUuid = "7e1a0004-8f3c-4b6e-9a51-3c7d2e4f6a10";

inline constexpr uint8_t kProtocolVersion = 1;
/** ATT caps an attribute value at 512 bytes; the data packet header takes 2. */
inline constexpr uint16_t kMaxAttributeLength = 512;
inline constexpr uint16_t kDataHeaderLength = 2;
inline constexpr uint16_t kMaxChunk = kMaxAttributeLength - kDataHeaderLength;

enum class Opcode : uint8_t {
    Begin = 0x01,
    End = 0x02,
    Abort = 0x03,
    BeginResponse = 0x81,
    EndResponse = 0x82,
    AbortResponse = 0x83,
    Progress = 0x90,
};

enum class Status : uint8_t {
    Ok = 0,
    NotInUpdateMode = 1, // the adapter only accepts firmware in update mode
    InvalidCommand = 2,
    InvalidSize = 3,     // 0 or larger than the app slot
    NotStarted = 4,      // data or End without Begin
    SequenceError = 5,   // a data packet went missing
    Overflow = 6,        // the client ignored the window
    InvalidImage = 7,    // not an acaia-bridge image for this chip
    TooMuchData = 8,     // more bytes than announced in Begin
    Incomplete = 9,      // End before all bytes arrived
    FlashError = 10,
    VerifyFailed = 11,   // checksum / image verification failed after writing
    Timeout = 12,        // no data for too long
};

const char* toString(Status status);

/** Payload bytes per data packet for an ATT MTU (the write value is MTU - 3). */
uint16_t chunkSizeForMtu(uint16_t mtu);

struct BeginCommand {
    uint32_t imageSize;
};

struct DataPacket {
    uint16_t seq;
    std::span<const uint8_t> payload;
};

/** Parses a Control write; nullopt for anything malformed. */
std::optional<Opcode> commandOpcode(std::span<const uint8_t> data);
std::optional<BeginCommand> parseBegin(std::span<const uint8_t> data);
std::optional<DataPacket> parseData(std::span<const uint8_t> data);

std::vector<uint8_t> encodeBeginResponse(Status status, uint16_t chunk, uint16_t window);
std::vector<uint8_t> encodeResponse(Opcode opcode, Status status);
std::vector<uint8_t> encodeProgress(Status status, uint32_t bytes);

struct Info {
    bool updateMode;
    bool transferring;
    uint16_t chunk;      // for the reading connection's MTU
    uint16_t window;
    uint32_t capacity;   // largest image the app slot takes
    std::string_view firmwareVersion;
};

/** [version][flags][chunk u16][window u16][capacity u32][firmware version, UTF-8, up to 32 bytes] */
std::vector<uint8_t> encodeInfo(const Info& info);

} // namespace ota
