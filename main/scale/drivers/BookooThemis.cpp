#include "scale/drivers/BookooThemis.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"0ffe", "ff11", "ff12", WriteMode::Auto};

constexpr uint8_t kProduct = 0x03;
constexpr uint8_t kWeightFrame = 0x0B;
constexpr std::size_t kWeightFrameLength = 20;

} // namespace

bool BookooThemis::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "bookoo_sc");
}

std::optional<Reading> BookooThemis::decode(std::span<const uint8_t> data)
{
    if (data.size() < kWeightFrameLength || data[0] != kProduct || data[1] != kWeightFrame) {
        return std::nullopt;
    }
    const auto centigrams = static_cast<int32_t>(bytes::be24(data, 7));
    return Reading{fromCentigrams(data[6] == '-' ? -centigrams : centigrams)};
}

std::optional<BookooThemis::Frame> BookooThemis::encode(Command command)
{
    switch (command) {
    case Command::Tare:
        return Frame{0x03, 0x0A, 0x01, 0x00, 0x00, 0x08};
    case Command::TimerStart:
        return Frame{0x03, 0x0A, 0x04, 0x00, 0x00, 0x0A};
    case Command::TimerStop:
        return Frame{0x03, 0x0A, 0x05, 0x00, 0x00, 0x0D};
    case Command::TimerReset:
        return Frame{0x03, 0x0A, 0x06, 0x00, 0x00, 0x0C};
    }
    return std::nullopt;
}

const GattProfile& BookooThemis::profile() const
{
    return kProfile;
}

bool BookooThemis::send(Command command, Transport& transport)
{
    const auto bytes = encode(command);
    return bytes && transport.write(*bytes);
}

} // namespace scale
