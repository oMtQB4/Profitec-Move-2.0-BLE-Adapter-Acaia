#include "scale/drivers/Felicita.hpp"

#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"ffe0", "ffe1", "ffe1", WriteMode::Auto};

constexpr std::size_t kDigits = 6;

} // namespace

bool Felicita::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "felicita");
}

std::optional<Reading> Felicita::decode(std::span<const uint8_t> data)
{
    if (data.size() < 3 + kDigits || data[0] != 0x01 || data[1] != 0x02 || (data[2] != '+' && data[2] != '-')) {
        return std::nullopt;
    }
    int32_t centigrams = 0;
    for (std::size_t i = 0; i < kDigits; i++) {
        const uint8_t c = data[3 + i];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        centigrams = centigrams * 10 + (c - '0');
    }
    return Reading{fromCentigrams(data[2] == '-' ? -centigrams : centigrams)};
}

std::optional<uint8_t> Felicita::encode(Command command)
{
    switch (command) {
    case Command::Tare:
        return 0x54;
    case Command::TimerStart:
        return 0x52;
    case Command::TimerStop:
        return 0x53;
    case Command::TimerReset:
        return 0x43;
    }
    return std::nullopt;
}

const GattProfile& Felicita::profile() const
{
    return kProfile;
}

bool Felicita::send(Command command, Transport& transport)
{
    const auto byte = encode(command);
    return byte && transport.write(std::span<const uint8_t>(&*byte, 1));
}

} // namespace scale
