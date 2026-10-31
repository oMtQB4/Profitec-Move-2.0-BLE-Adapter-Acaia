#include "scale/drivers/VariaAku.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"fff0", "fff1", "fff2", WriteMode::WithoutResponse};

constexpr uint8_t kHeader = 0xFA;
constexpr uint8_t kWeight = 0x01;
constexpr uint8_t kNegative = 0x10;

VariaAku::Frame frame(uint8_t command)
{
    VariaAku::Frame f{kHeader, command, 0x01, 0x01, 0};
    f[4] = bytes::xorOf(std::span<const uint8_t>(f).subspan(1, 3));
    return f;
}

} // namespace

bool VariaAku::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "varia aku") || containsIgnoreCase(advertisement.name, "aku mini");
}

std::optional<Reading> VariaAku::decode(std::span<const uint8_t> data)
{
    if (data.size() < 6 || data[1] != kWeight || data[2] != 0x03) {
        return std::nullopt;
    }
    const auto centigrams = static_cast<int32_t>((data[3] & 0x0F) << 16 | bytes::be16(data, 4));
    return Reading{fromCentigrams((data[3] & kNegative) ? -centigrams : centigrams)};
}

std::optional<VariaAku::Frame> VariaAku::encode(Command command)
{
    switch (command) {
    case Command::Tare:
        return frame(0x82);
    case Command::TimerStart:
        return frame(0x88);
    case Command::TimerStop:
        return frame(0x89);
    case Command::TimerReset:
        return frame(0x8A); // Beanconqueror sends a mismatching XOR here; this one is consistent
    }
    return std::nullopt;
}

const GattProfile& VariaAku::profile() const
{
    return kProfile;
}

bool VariaAku::send(Command command, Transport& transport)
{
    const auto bytes = encode(command);
    return bytes && transport.write(*bytes);
}

} // namespace scale
