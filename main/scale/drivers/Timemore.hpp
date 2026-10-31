#pragma once

/*
 * Timemore scales. Two unrelated protocols:
 *
 * Black Mirror family ("timemore scale"): Beanconqueror timemoreScale.ts only.
 *   Service 181D, notify 2A9D, write 553f4e49-... (with response).
 *   Weight int16 LE at [1..2], 0.1 g. Single-byte commands.
 *
 * Basic 3 and Dot: Beanconqueror timemoreBasicScale.ts / timemoreDotScale.ts,
 *   weight layout confirmed by Decent de1app (Dot).
 *   Service FFF0, notify FFF1, write FFF2.
 *   Frames: A5 5A <op> <cmd> <len u16 BE> <data> <CRC-16/MODBUS BE over all previous bytes>.
 *   Weight: op 01|02, cmd 01, data[0..3] int32 BE, 0.1 g. Basic checks the CRC, Dot not.
 *   Command bytes follow Beanconqueror's CRC-correct frames; de1app's captured
 *   Dot commands differ, so the Dot's commands are the least certain part.
 */

#include <array>
#include <cstddef>
#include <optional>

#include "common/Packet.hpp"
#include "scale/Driver.hpp"

namespace scale {

class TimemoreBlackMirror : public DecodingDriver<TimemoreBlackMirror> {
public:
    static constexpr const char* kName = "Timemore Black Mirror";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<uint8_t> encode(Command command);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

namespace timemore {

using Frame = Packet<16>;

uint16_t crc16Modbus(std::span<const uint8_t> data);
Frame command(uint8_t cmd, std::span<const uint8_t> data);
std::optional<Reading> decode(std::span<const uint8_t> data, bool checkCrc);
std::optional<Frame> encode(Command command);

} // namespace timemore

/** Shared behaviour of the A5 5A protocol. */
template <class Self, bool CheckCrc>
class TimemoreA55A : public DecodingDriver<Self> {
public:
    static std::optional<Reading> decode(std::span<const uint8_t> data) { return timemore::decode(data, CheckCrc); }

    bool send(Command command, Transport& transport) override
    {
        const auto frame = timemore::encode(command);
        return frame && transport.write(frame->bytes());
    }

protected:
    void onConnected(Transport& transport) override
    {
        const std::array<uint8_t, 1> grams{0x00};
        const std::array<uint8_t, 2> standardMode{0x01, 0x00};
        this->schedule(500, timemore::command(0x06, grams).bytes());
        this->schedule(700, timemore::command(0x08, standardMode).bytes());
        (void)transport;
    }
};

class TimemoreBasic : public TimemoreA55A<TimemoreBasic, true> {
public:
    static constexpr const char* kName = "Timemore Basic";
    static bool matches(const Advertisement& advertisement);
    const GattProfile& profile() const override;
};

class TimemoreDot : public TimemoreA55A<TimemoreDot, false> {
public:
    static constexpr const char* kName = "Timemore Dot";
    static bool matches(const Advertisement& advertisement);
    const GattProfile& profile() const override;
};

} // namespace scale
