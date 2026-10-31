#include "scale/drivers/Futula.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"fff0", "fff4", "fff1", WriteMode::WithResponse};

constexpr uint8_t kHeader = 0xFD;
constexpr uint8_t kSetUnit = 0x00;
constexpr uint8_t kUnitGrams = 0x04;
constexpr uint8_t kTare = 0x32;

} // namespace

bool Futula::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "lfsmart scale") ||
           startsWithIgnoreCase(advertisement.name, "LEFU-CK811");
}

std::optional<Reading> Futula::decode(std::span<const uint8_t> data)
{
    if (data.size() < 9) {
        return std::nullopt;
    }
    const int32_t weight = bytes::le16(data, 3);
    return Reading{data[5] > 0 ? -weight : weight};
}

Futula::Frame Futula::frame(uint8_t command, uint8_t argument)
{
    Frame f{kHeader, command, argument};
    f[10] = bytes::xorOf(std::span<const uint8_t>(f).first(10));
    return f;
}

const GattProfile& Futula::profile() const
{
    return kProfile;
}

void Futula::onConnected(Transport& transport)
{
    transport.write(frame(kSetUnit, kUnitGrams));
}

bool Futula::send(Command command, Transport& transport)
{
    return command == Command::Tare && transport.write(frame(kTare, 0x00));
}

} // namespace scale
