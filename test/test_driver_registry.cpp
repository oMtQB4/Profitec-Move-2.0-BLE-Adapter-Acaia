#include "TestSupport.hpp"

#include "scale/DriverRegistry.hpp"
#include "scale/drivers/Drivers.hpp"

using namespace scale;

namespace {

struct Sample {
    const char* advertisedName;
    const char* expectedDriver;
};

// One or more advertised names per driver, as documented by the protocol sources.
constexpr Sample kSamples[] = {
    {"CFS-9002", "Eureka Precisa"},
    {"LSJ-001", "Eureka Precisa"},
    {"Decent Scale", "Decent Scale"},
    {"EspressiScale", "Decent Scale"},
    {"Microbalance Ti", "DiFluid Microbalance Ti"},
    {"Microbalance", "DiFluid Microbalance"},
    {"FELICITA", "Felicita"},
    {"HIROIA JIMMY", "Hiroia Jimmy"},
    {"Skale", "Skale"},
    {"Varia AKU", "Varia AKU"},
    {"AKU MINI", "Varia AKU"},
    {"BOOKOO_SC", "Bookoo Themis"},
    {"Timemore Scale", "Timemore Black Mirror"},
    {"TIMEMORE Basic 3", "Timemore Basic"},
    {"TES017", "Timemore Dot"},
    {"smartchef", "Smartchef"},
    {"blackcoffee", "Blackcoffee"},
    {"LFSmart Scale", "Futula"},
    {"WeighMaster", "WeighMaster"},
    {"WeighMyBru", "WeighMyBru"},
    {"mpy-coffee", "DIY mpy-coffee"},
    {"coffee-scale", "DIY coffee-scale"},
};

void testEverySampleNameResolvesToItsDriver()
{
    DriverRegistry registry;
    registerBuiltinDrivers(registry);
    for (const Sample& sample : kSamples) {
        const auto* entry = registry.find(named(sample.advertisedName));
        TEST_ASSERT_NOT_NULL_MESSAGE(entry, sample.advertisedName);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(sample.expectedDriver, std::string(entry->name).c_str(), sample.advertisedName);
        TEST_ASSERT_TRUE(entry->create()->name() == entry->name);
    }
}

void testUnrelatedDevicesHaveNoDriver()
{
    DriverRegistry registry;
    registerBuiltinDrivers(registry);
    for (const char* name : {"SomeHeadphones", "LUNAR-80E382", "MOVE2-48224", "Hue bulb", "iPhone", ""}) {
        TEST_ASSERT_NULL_MESSAGE(registry.find(named(name)), name);
    }
}

} // namespace

void runDriverRegistryTests()
{
    RUN_TEST(testEverySampleNameResolvesToItsDriver);
    RUN_TEST(testUnrelatedDevicesHaveNoDriver);
}
