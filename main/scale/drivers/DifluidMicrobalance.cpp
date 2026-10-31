#include "scale/drivers/DifluidMicrobalance.hpp"

#include <array>
#include <cmath>

#include "scale/AdvertisingData.hpp"
#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"00ee", "aa01", "aa01", WriteMode::Auto};
constexpr GattProfile kProfileTi{"00dd", "aa01", "aa01", WriteMode::Auto};

constexpr uint16_t kService = 0x00EE;
constexpr uint16_t kServiceTi = 0x00DD;

constexpr std::size_t kHeaderLength = 5; // DF DF func cmd len
constexpr uint8_t kUnitOunces = 1;
constexpr double kGramsPerOunce = 28.349523125;

constexpr std::array<uint8_t, 7> kSetGrams{0xDF, 0xDF, 0x01, 0x04, 0x01, 0x00, 0xC4};
constexpr std::array<uint8_t, 7> kAutoSend{0xDF, 0xDF, 0x01, 0x00, 0x01, 0x01, 0xC1};
constexpr std::array<uint8_t, 7> kTare{0xDF, 0xDF, 0x03, 0x02, 0x01, 0x01, 0xC5};
constexpr std::array<uint8_t, 7> kTimerStart{0xDF, 0xDF, 0x03, 0x02, 0x01, 0x00, 0xC4};
constexpr std::array<uint8_t, 7> kTimerStop{0xDF, 0xDF, 0x03, 0x01, 0x01, 0x00, 0xC3};

bool isTiName(std::string_view name)
{
    return containsIgnoreCase(name, "microbalance ti") || containsIgnoreCase(name, "mb ti");
}

} // namespace

std::optional<Reading> DifluidMicrobalanceBase::decode(std::span<const uint8_t> data)
{
    if (data.size() < kHeaderLength || data[0] != 0xDF || data[1] != 0xDF || data[2] != 0x03 || data[3] != 0x00) {
        return std::nullopt;
    }
    const std::size_t length = data[4];
    if (length < 13 || data.size() < kHeaderLength + length + 1) {
        return std::nullopt;
    }
    if (bytes::sumOf(data.first(kHeaderLength + length)) != data[kHeaderLength + length]) {
        return std::nullopt;
    }
    const auto raw = static_cast<int32_t>(bytes::be32(data, kHeaderLength));
    if (data[kHeaderLength + 12] == kUnitOunces) {
        return Reading{static_cast<int32_t>(std::lround(raw / 1000.0 * kGramsPerOunce * 10.0))};
    }
    return Reading{raw};
}

void DifluidMicrobalanceBase::onConnected(Transport& transport)
{
    transport.write(kSetGrams);
    schedule(100, kAutoSend);
}

void DifluidMicrobalanceBase::onNotify(std::span<const uint8_t> data)
{
    if (auto reading = decode(data)) {
        publish(*reading);
    }
}

bool DifluidMicrobalanceBase::send(Command command, Transport& transport)
{
    switch (command) {
    case Command::Tare:
        return transport.write(kTare);
    case Command::TimerStart:
        return transport.write(kTimerStart);
    case Command::TimerStop:
        return transport.write(kTimerStop);
    case Command::TimerReset:
        return false; // no reset command in the protocol
    }
    return false;
}

bool DifluidMicrobalance::matches(const Advertisement& advertisement)
{
    return advertisesService16(advertisement.payload, kService) ||
           (containsIgnoreCase(advertisement.name, "microbalance") && !isTiName(advertisement.name));
}

const GattProfile& DifluidMicrobalance::profile() const
{
    return kProfile;
}

bool DifluidMicrobalanceTi::matches(const Advertisement& advertisement)
{
    return advertisesService16(advertisement.payload, kServiceTi) || isTiName(advertisement.name);
}

const GattProfile& DifluidMicrobalanceTi::profile() const
{
    return kProfileTi;
}

} // namespace scale
