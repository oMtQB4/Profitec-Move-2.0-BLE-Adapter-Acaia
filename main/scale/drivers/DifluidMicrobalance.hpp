#pragma once

/*
 * DiFluid Microbalance (service 00EE) and Microbalance Ti (service 00DD).
 *
 * Sources: DiFluid's official protocol document (difluid-sdk-demo,
 * protocolMicrobalance.md), Beanconqueror difluidMicrobalance(Ti).ts, Decent
 * de1app. Confidence: three sources (Ti: Beanconqueror + official doc).
 *
 * One characteristic AA01 for notify and write. Frames:
 * DF DF <func> <cmd> <len> <data...> <sum of all previous bytes>.
 * Weight: func 03 cmd 00, data[0..3] int32 BE (grams x10, ounces x1000),
 * data[12] unit (0 g, 1 oz). Weight frames only arrive after auto-send is
 * enabled.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class DifluidMicrobalanceBase : public Driver {
public:
    static std::optional<Reading> decode(std::span<const uint8_t> data);

    void onNotify(std::span<const uint8_t> data) override;
    bool send(Command command, Transport& transport) override;

protected:
    void onConnected(Transport& transport) override;
};

class DifluidMicrobalance : public DifluidMicrobalanceBase {
public:
    static constexpr const char* kName = "DiFluid Microbalance";
    static bool matches(const Advertisement& advertisement);

    std::string_view name() const override { return kName; }
    const GattProfile& profile() const override;
};

class DifluidMicrobalanceTi : public DifluidMicrobalanceBase {
public:
    static constexpr const char* kName = "DiFluid Microbalance Ti";
    static bool matches(const Advertisement& advertisement);

    std::string_view name() const override { return kName; }
    const GattProfile& profile() const override;
};

} // namespace scale
