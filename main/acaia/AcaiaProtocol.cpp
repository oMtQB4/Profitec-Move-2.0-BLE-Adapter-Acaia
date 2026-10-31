#include "acaia/AcaiaProtocol.hpp"

#include <array>
#include <cstdlib>

namespace acaia {

namespace {

constexpr uint8_t kTimerStart = 0x00;
constexpr uint8_t kTimerReset = 0x01;
constexpr uint8_t kTimerStop = 0x02;

bool isHeaderAt(std::span<const uint8_t> data, std::size_t i)
{
    return i + 1 < data.size() && data[i] == kHeader1 && data[i + 1] == kHeader2;
}

} // namespace

const char* toString(Command command)
{
    switch (command) {
    case Command::Heartbeat:
        return "heartbeat";
    case Command::Ident:
        return "ident";
    case Command::NotificationRequest:
        return "notification-request";
    case Command::GetSettings:
        return "get-settings";
    case Command::Tare:
        return "tare";
    case Command::TimerStart:
        return "timer-start";
    case Command::TimerStop:
        return "timer-stop";
    case Command::TimerReset:
        return "timer-reset";
    case Command::Unknown:
        break;
    }
    return "unknown";
}

Frame encode(MessageType type, std::span<const uint8_t> payload)
{
    Frame frame{kHeader1, kHeader2, static_cast<uint8_t>(type)};
    uint8_t cksumEven = 0;
    uint8_t cksumOdd = 0;
    for (std::size_t i = 0; i < payload.size(); i++) {
        frame.push(payload[i]);
        (i % 2 == 0 ? cksumEven : cksumOdd) += payload[i];
    }
    frame.push(cksumEven);
    frame.push(cksumOdd);
    return frame;
}

/*
 * Same layout as a real Lunar 2021 (aioacaia fixture
 * efdd0c0c05df060000010007000002f30d): the weight record is followed by a
 * timer record, 17 bytes in total.
 */
Frame encodeWeight(int32_t weightDg, uint32_t timeDs)
{
    const auto value = static_cast<uint32_t>(std::abs(weightDg));
    const std::array<uint8_t, 12> payload{
        12,
        static_cast<uint8_t>(EventType::Weight),
        static_cast<uint8_t>(value),
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value >> 16),
        static_cast<uint8_t>(value >> 24),
        1,                                                // decimals: value / 10^1
        static_cast<uint8_t>(weightDg < 0 ? 0x02 : 0x00), // flags: bit 1 = negative
        static_cast<uint8_t>(EventType::Timer),
        static_cast<uint8_t>(timeDs / 600),     // minutes
        static_cast<uint8_t>(timeDs / 10 % 60), // seconds
        static_cast<uint8_t>(timeDs % 10),      // tenths
    };
    return encode(MessageType::Event, payload);
}

Frame encodeTimer(uint32_t timeDs)
{
    const std::array<uint8_t, 5> payload{
        5,
        static_cast<uint8_t>(EventType::Timer),
        static_cast<uint8_t>(timeDs / 600),     // minutes
        static_cast<uint8_t>(timeDs / 10 % 60), // seconds
        static_cast<uint8_t>(timeDs % 10),      // tenths
    };
    return encode(MessageType::Event, payload);
}

/*
 * Field positions follow pyacaia's Settings parser: [1] battery (low 7 bits),
 * [2] unit (2 = grams, 5 = ounces), [4] auto-off / 5 min, [6] beep on. The
 * remaining bytes are unknown and sent as zero.
 */
Frame encodeStatus(uint8_t batteryPct)
{
    const std::array<uint8_t, 8> payload{8, static_cast<uint8_t>(batteryPct & 0x7F), 2, 0, 0, 0, 1, 0};
    return encode(MessageType::Status, payload);
}

std::vector<ParsedFrame> split(std::span<const uint8_t> data)
{
    std::vector<ParsedFrame> frames;
    std::size_t i = 0;
    while (i + 2 < data.size()) {
        if (!isHeaderAt(data, i)) {
            i++;
            continue;
        }
        const std::size_t start = i + 3;
        std::size_t end = start;
        while (end < data.size() && !isHeaderAt(data, end)) {
            end++;
        }
        frames.push_back({data[i + 2], data.subspan(start, end - start)});
        i = end;
    }
    return frames;
}

Command classify(const ParsedFrame& frame)
{
    switch (static_cast<MessageType>(frame.type)) {
    case MessageType::Heartbeat:
        return Command::Heartbeat;
    case MessageType::Ident:
        return Command::Ident;
    case MessageType::Event:
        return Command::NotificationRequest;
    case MessageType::GetSettings:
        return Command::GetSettings;
    case MessageType::Tare:
        return Command::Tare;
    case MessageType::Timer:
        if (frame.payload.size() < 2) {
            return Command::Unknown;
        }
        switch (frame.payload[1]) {
        case kTimerStart:
            return Command::TimerStart;
        case kTimerReset:
            return Command::TimerReset;
        case kTimerStop:
            return Command::TimerStop;
        default:
            return Command::Unknown;
        }
    case MessageType::Status:
        break;
    }
    return Command::Unknown;
}

} // namespace acaia
