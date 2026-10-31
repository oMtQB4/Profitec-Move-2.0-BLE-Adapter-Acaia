#include "unity.h"

#include "acaia/DeviceInfo.hpp"

using namespace acaia;

namespace {

void testSystemIdFromAddress()
{
    const BdAddress address{0x28, 0x37, 0x2F, 0x80, 0xE3, 0x82};
    const SystemId expected{0x82, 0xE3, 0x80, 0xFE, 0xFF, 0x2F, 0x37, 0x28};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected.data(), systemIdFromAddress(address).data(), expected.size());
}

} // namespace

void runDeviceInfoTests()
{
    RUN_TEST(testSystemIdFromAddress);
}
