#include <array>

#include "unity.h"

#include "scale/AdvertisingData.hpp"

using namespace scale;

namespace {

void testEurekaPaddedAdvertisementWithNameInScanResponse()
{
    // Captured from a real Eureka Precisa: 31-byte advertisement padded with
    // zeros, followed by the scan response carrying "CFS-9002".
    const std::array<uint8_t, 62> payload{
        0x02, 0x01, 0x06, 0x09, 0xff, 0xff, 0xff, 0xe0, 0x1d, 0x23, 0x30, 0xa6, 0xbc, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,
        0x09, 0x43, 0x46, 0x53, 0x2d, 0x39, 0x30, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    TEST_ASSERT_TRUE(findLocalName(payload) == "CFS-9002");
}

void testPrefersCompleteOverShortenedName()
{
    const std::array<uint8_t, 10> payload{0x03, 0x08, 'A', 'B', 0x05, 0x09, 'A', 'B', 'C', 'D'};
    TEST_ASSERT_TRUE(findLocalName(payload) == "ABCD");
}

void testFallsBackToShortenedName()
{
    const std::array<uint8_t, 7> payload{0x02, 0x01, 0x06, 0x03, 0x08, 'A', 'B'};
    TEST_ASSERT_TRUE(findLocalName(payload) == "AB");
}

void testNoNameOrTruncatedStructure()
{
    const std::array<uint8_t, 3> flagsOnly{0x02, 0x01, 0x06};
    const std::array<uint8_t, 4> truncated{0x09, 0x09, 'A', 'B'};
    TEST_ASSERT_TRUE(findLocalName(flagsOnly).empty());
    TEST_ASSERT_TRUE(findLocalName(truncated).empty());
}

} // namespace

void runAdvertisingDataTests()
{
    RUN_TEST(testEurekaPaddedAdvertisementWithNameInScanResponse);
    RUN_TEST(testPrefersCompleteOverShortenedName);
    RUN_TEST(testFallsBackToShortenedName);
    RUN_TEST(testNoNameOrTruncatedStructure);
}
