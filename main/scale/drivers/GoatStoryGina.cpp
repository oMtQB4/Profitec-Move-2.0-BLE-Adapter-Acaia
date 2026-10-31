#include "scale/drivers/GoatStoryGina.hpp"

#include <cmath>

#include "scale/AdvertisingData.hpp"
#include "scale/Bytes.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"91341521-bac2-42d9-bbb3-942f7a10976c", "91341522-bac2-42d9-bbb3-942f7a10976c",
                               nullptr};
constexpr const char* kCalibrationUuid = "91341523-bac2-42d9-bbb3-942f7a10976c";
constexpr Uuid128 kService = uuid128("91341521-BAC2-42D9-BBB3-942F7A10976C");

} // namespace

bool GoatStoryGina::matches(const Advertisement& advertisement)
{
    return advertisesService128(advertisement.payload, kService);
}

std::optional<double> GoatStoryGina::calibrationFactor(std::span<const uint8_t> calibration)
{
    if (calibration.size() < 4) {
        return std::nullopt;
    }
    const double factor = bytes::le32(calibration, 0) / 10000.0;
    if (!(factor > 0.0) || !std::isfinite(factor)) {
        return std::nullopt;
    }
    return factor;
}

std::optional<Reading> GoatStoryGina::decode(std::span<const uint8_t> data, double factor)
{
    if (data.size() < 4) {
        return std::nullopt;
    }
    // raw / 10 / factor grams -> raw / factor tenths of a gram
    return Reading{static_cast<int32_t>(std::lround(bytes::le32(data, 0) / factor))};
}

const GattProfile& GoatStoryGina::profile() const
{
    return kProfile;
}

void GoatStoryGina::onConnected(Transport& transport)
{
    // Without a valid calibration no weight can be computed, so nothing is published.
    if (const auto calibration = transport.read(kCalibrationUuid)) {
        factor_ = calibrationFactor(*calibration);
    }
}

void GoatStoryGina::onNotify(std::span<const uint8_t> data)
{
    if (!factor_) {
        return;
    }
    if (auto reading = decode(data, *factor_)) {
        publish(*reading);
    }
}

} // namespace scale
