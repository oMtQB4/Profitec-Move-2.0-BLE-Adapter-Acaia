#include "unity.h"

#include "scale/DriverRegistry.hpp"
#include "scale/drivers/EurekaPrecisa.hpp"

using namespace scale;

namespace {

void testFindsRegisteredDriverAndCreatesIt()
{
    DriverRegistry registry;
    registry.add<EurekaPrecisa>();

    const auto* entry = registry.find({"CFS-9002"});
    TEST_ASSERT_NOT_NULL(entry);
    const auto driver = entry->create();
    TEST_ASSERT_TRUE(driver->name() == EurekaPrecisa::kName);
}

void testUnknownScaleHasNoDriver()
{
    DriverRegistry registry;
    registry.add<EurekaPrecisa>();
    TEST_ASSERT_NULL(registry.find({"SomeHeadphones"}));
}

} // namespace

void runDriverRegistryTests()
{
    RUN_TEST(testFindsRegisteredDriverAndCreatesIt);
    RUN_TEST(testUnknownScaleHasNoDriver);
}
