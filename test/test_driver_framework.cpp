#include "TestSupport.hpp"

#include "scale/AdvertisingData.hpp"
#include "scale/NameMatch.hpp"

using namespace scale;

namespace {

/** Exposes the protected scheduling helpers. */
class ProbeDriver : public Driver {
public:
    std::string_view name() const override { return "probe"; }
    const GattProfile& profile() const override { return profile_; }
    void onNotify(std::span<const uint8_t>) override {}
    bool send(Command, Transport& transport) override { return sendTwice(transport, hex("01 02"), 200); }

    bool queue(uint32_t delayMs, const Bytes& bytes) { return schedule(delayMs, bytes); }
    int ticks = 0;

protected:
    void onTick(Transport&, uint32_t) override { ticks++; }

private:
    GattProfile profile_{"fff0", "fff1", "fff2"};
};

void testSendTwiceRepeatsAfterGap()
{
    FakeTransport transport;
    ProbeDriver driver;
    driver.connected(transport, 1000);
    driver.send(Command::Tare, transport);
    TEST_ASSERT_EQUAL(1, transport.writes.size());
    driver.tick(transport, 1150);
    TEST_ASSERT_EQUAL(1, transport.writes.size());
    driver.tick(transport, 1200);
    TEST_ASSERT_EQUAL(2, transport.writes.size());
    TEST_ASSERT_BYTES("01 02", transport.writes[1]);
    driver.tick(transport, 2000);
    TEST_ASSERT_EQUAL(2, transport.writes.size());
    TEST_ASSERT_EQUAL(3, driver.ticks);
}

void testScheduledWritesKeepOrderAndCapacity()
{
    FakeTransport transport;
    ProbeDriver driver;
    driver.connected(transport, 0);
    TEST_ASSERT_TRUE(driver.queue(300, hex("03")));
    TEST_ASSERT_TRUE(driver.queue(100, hex("01")));
    TEST_ASSERT_TRUE(driver.queue(100, hex("02")));
    driver.tick(transport, 300);
    TEST_ASSERT_EQUAL(3, transport.writes.size());
    TEST_ASSERT_BYTES("03", transport.writes[0]); // all due: sent in scheduling order
    TEST_ASSERT_BYTES("01", transport.writes[1]);
    TEST_ASSERT_BYTES("02", transport.writes[2]);

    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(driver.queue(10, hex("00")));
    }
    TEST_ASSERT_FALSE(driver.queue(10, hex("00")));
}

void testUnitConversionRounds()
{
    TEST_ASSERT_EQUAL_INT32(2, fromCentigrams(15));
    TEST_ASSERT_EQUAL_INT32(1, fromCentigrams(14));
    TEST_ASSERT_EQUAL_INT32(-2, fromCentigrams(-15));
    TEST_ASSERT_EQUAL_INT32(185, fromMilligrams(18500));
    TEST_ASSERT_EQUAL_INT32(-185, fromMilligrams(-18451));
    TEST_ASSERT_EQUAL_INT32(185, fromGrams(18.5f));
}

void testNameMatchingIgnoresCase()
{
    TEST_ASSERT_TRUE(startsWithIgnoreCase("BOOKOO_SC 123", "bookoo_sc"));
    TEST_ASSERT_TRUE(containsIgnoreCase("My Felicita Arc", "FELICITA"));
    TEST_ASSERT_FALSE(startsWithIgnoreCase("Sk", "skale"));
    TEST_ASSERT_FALSE(containsIgnoreCase("Decent", "decent scale"));
}

void testServiceUuidMatching()
{
    // flags, then complete 16-bit list {00DD, 180F}
    const Bytes services16 = hex("02 01 06 05 03 DD 00 0F 18");
    TEST_ASSERT_TRUE(advertisesService16(services16, 0x00DD));
    TEST_ASSERT_TRUE(advertisesService16(services16, 0x180F));
    TEST_ASSERT_FALSE(advertisesService16(services16, 0x00EE));

    constexpr Uuid128 gina = uuid128("91341521-BAC2-42D9-BBB3-942F7A10976C");
    TEST_ASSERT_EQUAL_HEX8(0x6C, gina[0]); // little-endian: last byte of the string first
    TEST_ASSERT_EQUAL_HEX8(0x91, gina[15]);
    Bytes services128 = hex("00 00 11 07"); // zero padding, then a 128-bit list
    services128.insert(services128.end(), gina.begin(), gina.end());
    TEST_ASSERT_TRUE(advertisesService128(services128, gina));
    TEST_ASSERT_FALSE(advertisesService128(services16, gina));
}

} // namespace

void runDriverFrameworkTests()
{
    RUN_TEST(testSendTwiceRepeatsAfterGap);
    RUN_TEST(testScheduledWritesKeepOrderAndCapacity);
    RUN_TEST(testUnitConversionRounds);
    RUN_TEST(testNameMatchingIgnoresCase);
    RUN_TEST(testServiceUuidMatching);
}
