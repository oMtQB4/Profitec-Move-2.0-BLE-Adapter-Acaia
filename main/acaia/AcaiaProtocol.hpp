#pragma once

/*
 * Acaia scale protocol, scale side (Lunar 2021 / Pyxis GATT layout).
 *
 * Mirrored from client implementations: pyacaia (lucapinello/pyacaia) and
 * AcaiaArduinoBLE (tatemazer/AcaiaArduinoBLE).
 *
 * Frame: EF DD <type> <payload...> <cksum_even> <cksum_odd>
 * The checksums are the low bytes of the sums of the payload bytes at even and
 * odd indices. Scale->client frames carry their length as payload[0], and the
 * whole frame is payload[0] + 5 bytes long. Client->scale commands carry no
 * length byte.
 */

#include <cstdint>
#include <span>
#include <vector>

#include "common/Packet.hpp"

namespace acaia {

inline constexpr const char* kServiceUuid = "49535343-fe7d-4ae5-8fa9-9fafd205e455";
inline constexpr const char* kNotifyUuid = "49535343-1e4d-4bd9-ba61-23c647249616";
inline constexpr const char* kWriteUuid = "49535343-8841-43f4-a8d4-ecbe34729bb3";
/** Third characteristic of a real Lunar 2021; purpose unknown, offered for completeness. */
inline constexpr const char* kAuxUuid = "49535343-4c8a-39b3-2f49-511cff073b7e";

inline constexpr uint8_t kHeader1 = 0xEF;
inline constexpr uint8_t kHeader2 = 0xDD;

enum class MessageType : uint8_t {
    Heartbeat = 0x00,
    Tare = 0x04,
    GetSettings = 0x06,
    Status = 0x08,
    Ident = 0x0B,
    Event = 0x0C,
    Timer = 0x0D,
};

enum class EventType : uint8_t {
    Weight = 0x05,
    Timer = 0x07,
};

/** What a client (the coffee machine) asks the scale to do. */
enum class Command {
    Unknown,
    Heartbeat,
    Ident,
    NotificationRequest,
    GetSettings,
    Tare,
    TimerStart,
    TimerStop,
    TimerReset,
};

const char* toString(Command command);

using Frame = Packet<32>;

/** One frame found in a client write. `payload` points into the input buffer. */
struct ParsedFrame {
    uint8_t type;
    std::span<const uint8_t> payload; // includes trailing checksum bytes, if present
};

Frame encode(MessageType type, std::span<const uint8_t> payload);

/** Weight event, value in 0.1 g, followed by the scale timer in 0.1 s. */
Frame encodeWeight(int32_t weightDg, uint32_t timeDs = 0);

/** Timer event, value in 0.1 s. */
Frame encodeTimer(uint32_t timeDs);

/** Status/settings message: battery %, unit grams, beep on. */
Frame encodeStatus(uint8_t batteryPct);

/**
 * Splits a client write into frames. Without a length byte, a frame runs until
 * the next EF DD header or the end of the buffer.
 */
std::vector<ParsedFrame> split(std::span<const uint8_t> data);

Command classify(const ParsedFrame& frame);

} // namespace acaia
