/*
 * Drivers for scales on the Profitec Move 2.0 app list. "source" frames are
 * quoted from the protocol sources named in each driver header; "constructed"
 * frames were built from the documented layout.
 */

#include "TestSupport.hpp"

#include "scale/drivers/DecentScale.hpp"
#include "scale/drivers/DifluidMicrobalance.hpp"
#include "scale/drivers/Felicita.hpp"
#include "scale/drivers/HiroiaJimmy.hpp"
#include "scale/drivers/Skale.hpp"
#include "scale/drivers/VariaAku.hpp"

using namespace scale;

namespace {

// --- Decent Scale / EspressiScale ---

void testDecentDecode()
{
    DecentScale driver;
    TEST_ASSERT_EQUAL_INT32(300, *feed(driver, hex("03 CE 01 2C 00 00 E0")));       // constructed
    TEST_ASSERT_EQUAL_INT32(-10, *feed(driver, hex("03 CA FF F6 00 00 C4")));       // constructed
    TEST_ASSERT_EQUAL_INT32(185, *feed(driver, hex("03 CE 00 B9 00 00 74")));       // EspressiScale, constructed
    TEST_ASSERT_EQUAL_INT32(185, *feed(driver, hex("03 CE 00 B9 00 05 03 00 00 00"))); // 10-byte frame
    TEST_ASSERT_FALSE(feed(driver, hex("03 AA 01 00 00 00 A8")).has_value());        // button
    TEST_ASSERT_FALSE(feed(driver, hex("03 0A 03 FF FF 00 0A")).has_value());        // heartbeat echo
}

void testDecentCommands()
{
    TEST_ASSERT_BYTES("03 0F FD 00 00 01 F0", *DecentScale::encode(Command::Tare, 253));
    TEST_ASSERT_BYTES("03 0F FE 00 00 01 F3", *DecentScale::encode(Command::Tare, 254));
    TEST_ASSERT_BYTES("03 0B 03 00 00 00 0B", *DecentScale::encode(Command::TimerStart, 0));
    TEST_ASSERT_BYTES("03 0B 00 00 00 00 08", *DecentScale::encode(Command::TimerStop, 0));
    TEST_ASSERT_BYTES("03 0B 02 00 00 00 0A", *DecentScale::encode(Command::TimerReset, 0));
}

void testDecentConnectHeartbeatAndDoubleSend()
{
    FakeTransport transport;
    DecentScale driver;
    driver.connected(transport, 1000);
    TEST_ASSERT_EQUAL(1, transport.writes.size());
    TEST_ASSERT_BYTES("03 0A 01 01 00 00 09", transport.writes[0]); // LED on, grams
    driver.tick(transport, 1050);
    TEST_ASSERT_BYTES("03 0A 03 FF FF 00 0A", transport.writes.back()); // first heartbeat
    driver.tick(transport, 1200);
    TEST_ASSERT_BYTES("03 0A 01 01 00 00 09", transport.writes.back()); // LED on repeated
    const std::size_t before = transport.writes.size();
    driver.tick(transport, 3050);
    TEST_ASSERT_EQUAL(before + 1, transport.writes.size()); // next heartbeat after 2 s

    driver.send(Command::Tare, transport);
    driver.send(Command::Tare, transport);
    TEST_ASSERT_BYTES("03 0F FD 00 00 01 F0", transport.writes[before + 1]);
    TEST_ASSERT_BYTES("03 0F FE 00 00 01 F3", transport.writes[before + 2]);
}

void testDecentMatches()
{
    TEST_ASSERT_TRUE(DecentScale::matches(named("Decent Scale")));
    TEST_ASSERT_TRUE(DecentScale::matches(named("ButtsHaus Scale")));
    TEST_ASSERT_TRUE(DecentScale::matches(named("EspressiScale")));
    TEST_ASSERT_FALSE(DecentScale::matches(named("Decent")));
}

// --- DiFluid Microbalance ---

void testDifluidDecode()
{
    // Official protocol document example: 76.0 g.
    const Bytes example = hex("DF DF 03 00 0D 00 00 02 F8 00 00 00 00 00 0A 27 B0 00 A9");
    TEST_ASSERT_EQUAL_INT32(760, DifluidMicrobalanceBase::decode(example)->weightDg);

    Bytes badChecksum = example;
    badChecksum.back() = 0xA8;
    TEST_ASSERT_FALSE(DifluidMicrobalanceBase::decode(badChecksum).has_value());
    TEST_ASSERT_FALSE(DifluidMicrobalanceBase::decode(hex("DF DF 03 02 01 01 C5")).has_value()); // echo

    // Constructed: 1.000 oz (unit byte 1) -> 28.3 g
    Bytes ounces = hex("DF DF 03 00 0D 00 00 03 E8 00 00 00 00 00 00 00 00 01 00");
    uint8_t sum = 0;
    for (std::size_t i = 0; i + 1 < ounces.size(); i++) {
        sum = static_cast<uint8_t>(sum + ounces[i]);
    }
    ounces.back() = sum;
    TEST_ASSERT_EQUAL_INT32(283, DifluidMicrobalanceBase::decode(ounces)->weightDg);
}

void testDifluidConnectAndCommands()
{
    FakeTransport transport;
    DifluidMicrobalance driver;
    driver.connected(transport, 0);
    TEST_ASSERT_BYTES("DF DF 01 04 01 00 C4", transport.writes[0]); // grams
    driver.tick(transport, 100);
    TEST_ASSERT_BYTES("DF DF 01 00 01 01 C1", transport.writes[1]); // auto-send weight

    TEST_ASSERT_TRUE(driver.send(Command::Tare, transport));
    TEST_ASSERT_BYTES("DF DF 03 02 01 01 C5", transport.writes.back());
    TEST_ASSERT_TRUE(driver.send(Command::TimerStart, transport));
    TEST_ASSERT_BYTES("DF DF 03 02 01 00 C4", transport.writes.back());
    TEST_ASSERT_TRUE(driver.send(Command::TimerStop, transport));
    TEST_ASSERT_BYTES("DF DF 03 01 01 00 C3", transport.writes.back());
    TEST_ASSERT_FALSE(driver.send(Command::TimerReset, transport));
}

void testDifluidMatchesByServiceOrName()
{
    const Bytes ti = hex("03 03 DD 00");
    const Bytes classic = hex("03 03 EE 00");
    TEST_ASSERT_TRUE(DifluidMicrobalanceTi::matches({"", ti}));
    TEST_ASSERT_FALSE(DifluidMicrobalance::matches({"", ti}));
    TEST_ASSERT_TRUE(DifluidMicrobalance::matches({"", classic}));
    TEST_ASSERT_TRUE(DifluidMicrobalanceTi::matches(named("Microbalance Ti")));
    TEST_ASSERT_FALSE(DifluidMicrobalance::matches(named("Microbalance Ti")));
    TEST_ASSERT_TRUE(DifluidMicrobalance::matches(named("Microbalance")));
}

// --- Felicita ---

void testFelicitaDecode()
{
    Felicita driver;
    // Captured frame from the Beanconqueror readme: +0.00 g.
    TEST_ASSERT_EQUAL_INT32(0, *feed(driver, hex("01 02 2B 30 30 30 30 30 30 20 67 53 4F 30 22 89 0D 0A")));
    TEST_ASSERT_EQUAL_INT32(10101, *feed(driver, hex("01 02 2B 31 30 31 30 31 30 20 67"))); // 1010.10 g
    TEST_ASSERT_EQUAL_INT32(-185, *feed(driver, hex("01 02 2D 30 30 31 38 35 30 20 67")));  // -18.50 g
    TEST_ASSERT_FALSE(feed(driver, hex("01 02 2B 30 30 41 30 30 30 20 67")).has_value());   // not a digit
}

void testFelicitaCommands()
{
    TEST_ASSERT_EQUAL_HEX8(0x54, *Felicita::encode(Command::Tare));
    TEST_ASSERT_EQUAL_HEX8(0x52, *Felicita::encode(Command::TimerStart));
    TEST_ASSERT_EQUAL_HEX8(0x53, *Felicita::encode(Command::TimerStop));
    TEST_ASSERT_EQUAL_HEX8(0x43, *Felicita::encode(Command::TimerReset));
}

// --- Hiroia Jimmy ---

void testJimmyDecodeAndTare()
{
    HiroiaJimmy driver;
    TEST_ASSERT_EQUAL_INT32(300, *feed(driver, hex("01 00 00 00 2C 01 00")));
    TEST_ASSERT_EQUAL_INT32(-10, *feed(driver, hex("01 00 00 00 F6 FF FF")));
    TEST_ASSERT_EQUAL_INT32(283, *feed(driver, hex("09 00 00 00 E8 03 00"))); // 1.000 oz

    FakeTransport transport;
    driver.connected(transport, 0);
    TEST_ASSERT_TRUE(driver.send(Command::Tare, transport));
    driver.tick(transport, 200);
    TEST_ASSERT_EQUAL(2, transport.writes.size());
    TEST_ASSERT_BYTES("07 00", transport.writes[1]);
    TEST_ASSERT_FALSE(driver.send(Command::TimerStart, transport));
    TEST_ASSERT_TRUE(HiroiaJimmy::matches(named("HIROIA JIMMY")));
}

// --- Skale ---

void testSkaleDecodeConnectAndCommands()
{
    Skale driver;
    TEST_ASSERT_EQUAL_INT32(300, *feed(driver, hex("00 2C 01")));
    TEST_ASSERT_EQUAL_INT32(-1, *feed(driver, hex("00 FF FF")));
    TEST_ASSERT_EQUAL_INT32(2500, *feed(driver, hex("00 C4 09")));

    FakeTransport transport;
    driver.connected(transport, 0);
    driver.tick(transport, 100);
    driver.tick(transport, 200);
    TEST_ASSERT_EQUAL(3, transport.writes.size());
    TEST_ASSERT_BYTES("ED", transport.writes[0]);
    TEST_ASSERT_BYTES("EC", transport.writes[1]);
    TEST_ASSERT_BYTES("03", transport.writes[2]);

    TEST_ASSERT_EQUAL_HEX8(0x10, *Skale::encode(Command::Tare));
    TEST_ASSERT_EQUAL_HEX8(0xDD, *Skale::encode(Command::TimerStart));
    TEST_ASSERT_EQUAL_HEX8(0xD1, *Skale::encode(Command::TimerStop));
    TEST_ASSERT_EQUAL_HEX8(0xD0, *Skale::encode(Command::TimerReset));
}

// --- Varia AKU ---

void testVariaDecodeAndCommands()
{
    VariaAku driver;
    TEST_ASSERT_EQUAL_INT32(2000, *feed(driver, hex("FA 01 03 00 4E 20 6C"))); // +200.00 g, constructed
    TEST_ASSERT_EQUAL_INT32(-100, *feed(driver, hex("FA 01 03 10 03 E8 F9"))); // -10.00 g, constructed
    TEST_ASSERT_FALSE(feed(driver, hex("FA 85 01 50 D4")).has_value());       // battery

    TEST_ASSERT_BYTES("FA 82 01 01 82", *VariaAku::encode(Command::Tare)); // Beanconqueror + de1app
    TEST_ASSERT_BYTES("FA 88 01 01 88", *VariaAku::encode(Command::TimerStart));
    TEST_ASSERT_BYTES("FA 89 01 01 89", *VariaAku::encode(Command::TimerStop));
    TEST_ASSERT_BYTES("FA 8A 01 01 8A", *VariaAku::encode(Command::TimerReset));
    TEST_ASSERT_TRUE(VariaAku::matches(named("Varia AKU")));
    TEST_ASSERT_TRUE(VariaAku::matches(named("AKU MINI")));
}

} // namespace

void runMoveListDriverTests()
{
    RUN_TEST(testDecentDecode);
    RUN_TEST(testDecentCommands);
    RUN_TEST(testDecentConnectHeartbeatAndDoubleSend);
    RUN_TEST(testDecentMatches);
    RUN_TEST(testDifluidDecode);
    RUN_TEST(testDifluidConnectAndCommands);
    RUN_TEST(testDifluidMatchesByServiceOrName);
    RUN_TEST(testFelicitaDecode);
    RUN_TEST(testFelicitaCommands);
    RUN_TEST(testJimmyDecodeAndTare);
    RUN_TEST(testSkaleDecodeConnectAndCommands);
    RUN_TEST(testVariaDecodeAndCommands);
}
