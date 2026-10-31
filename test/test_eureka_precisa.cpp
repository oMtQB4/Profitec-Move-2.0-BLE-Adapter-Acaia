/* Byte vectors come from Beanconqueror and de1app (see EurekaPrecisa.hpp). */

#include "TestSupport.hpp"

#include "scale/drivers/EurekaPrecisa.hpp"

using namespace scale;

namespace {

void testMatchesAdvertisedNames()
{
    TEST_ASSERT_TRUE(EurekaPrecisa::matches(named("CFS-9002")));
    TEST_ASSERT_TRUE(EurekaPrecisa::matches(named("LSJ-001")));
    TEST_ASSERT_FALSE(EurekaPrecisa::matches(named("CFS-90")));
    TEST_ASSERT_FALSE(EurekaPrecisa::matches(named("LUNAR-123456")));
}

void testEncodesCommands()
{
    TEST_ASSERT_BYTES("AA 02 31 31", *EurekaPrecisa::encode(Command::Tare));
    TEST_ASSERT_BYTES("AA 02 33 33", *EurekaPrecisa::encode(Command::TimerStart));
    TEST_ASSERT_BYTES("AA 02 34 34", *EurekaPrecisa::encode(Command::TimerStop));
    TEST_ASSERT_BYTES("AA 02 35 35", *EurekaPrecisa::encode(Command::TimerReset));
}

void testDecodesWeight()
{
    TEST_ASSERT_EQUAL_INT32(365, EurekaPrecisa::decode(hex("AA 09 41 01 0C 00 00 6D 01 00 00"))->weightDg);
    TEST_ASSERT_EQUAL_INT32(-15, EurekaPrecisa::decode(hex("AA 09 41 00 00 00 01 0F 00"))->weightDg);
}

void testRejectsOtherFrames()
{
    TEST_ASSERT_FALSE(EurekaPrecisa::decode(hex("AA 09 42 00 00 00 00 00 00")).has_value());
    TEST_ASSERT_FALSE(EurekaPrecisa::decode(hex("AA 09 41 00 00 00 00 00")).has_value());
}

void testDriverSetsGramsOnConnectAndPublishesReadings()
{
    FakeTransport transport;
    EurekaPrecisa driver;

    driver.connected(transport, 0);
    TEST_ASSERT_EQUAL(1, transport.writes.size());
    TEST_ASSERT_BYTES("AA 03 36 00", transport.writes[0]);

    TEST_ASSERT_EQUAL_INT32(42, *feed(driver, hex("AA 09 41 00 00 00 00 2A 00")));

    TEST_ASSERT_TRUE(driver.send(Command::Tare, transport));
    TEST_ASSERT_EQUAL(2, transport.writes.size());
}

} // namespace

void runEurekaPrecisaTests()
{
    RUN_TEST(testMatchesAdvertisedNames);
    RUN_TEST(testEncodesCommands);
    RUN_TEST(testDecodesWeight);
    RUN_TEST(testRejectsOtherFrames);
    RUN_TEST(testDriverSetsGramsOnConnectAndPublishesReadings);
}
