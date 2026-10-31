/*
 * Further scales (Beanconqueror and others). "source" frames are quoted from
 * the protocol sources named in each driver header; "constructed" frames were
 * built from the documented layout. Timemore CRCs were cross-checked with an
 * independent CRC-16/MODBUS implementation.
 */

#include "TestSupport.hpp"

#include "scale/drivers/Blackcoffee.hpp"
#include "scale/drivers/BookooThemis.hpp"
#include "scale/drivers/DiyScales.hpp"
#include "scale/drivers/Futula.hpp"
#include "scale/drivers/GoatStoryGina.hpp"
#include "scale/drivers/Smartchef.hpp"
#include "scale/drivers/Timemore.hpp"
#include "scale/drivers/WeighMaster.hpp"
#include "scale/drivers/WeighMyBru.hpp"

using namespace scale;

namespace {

// --- Bookoo Themis ---

void testBookooDecodeAndCommands()
{
    BookooThemis driver;
    // Constructed per the official docs: +200.00 g, battery 85 %.
    const Bytes weight = hex("03 0B 00 30 39 01 2B 00 4E 20 2B 00 64 55 00 64 01 00 00 3A");
    TEST_ASSERT_EQUAL_INT32(2000, *feed(driver, weight));
    Bytes negative = weight;
    negative[6] = '-';
    TEST_ASSERT_EQUAL_INT32(-2000, *feed(driver, negative));
    Bytes ultraGroundCoffee = weight;
    ultraGroundCoffee[1] = 0x0F;
    TEST_ASSERT_FALSE(feed(driver, ultraGroundCoffee).has_value());

    TEST_ASSERT_BYTES("03 0A 01 00 00 08", *BookooThemis::encode(Command::Tare));
    TEST_ASSERT_BYTES("03 0A 04 00 00 0A", *BookooThemis::encode(Command::TimerStart));
    TEST_ASSERT_BYTES("03 0A 05 00 00 0D", *BookooThemis::encode(Command::TimerStop));
    TEST_ASSERT_BYTES("03 0A 06 00 00 0C", *BookooThemis::encode(Command::TimerReset));
    TEST_ASSERT_TRUE(BookooThemis::matches(named("BOOKOO_SC 1234")));
}

// --- Timemore ---

void testTimemoreBlackMirror()
{
    TimemoreBlackMirror driver;
    TEST_ASSERT_EQUAL_INT32(2000, *feed(driver, hex("00 D0 07 00 00 00 00 00")));
    TEST_ASSERT_EQUAL_INT32(-10, *feed(driver, hex("00 F6 FF FF")));
    TEST_ASSERT_EQUAL_HEX8(0x00, *TimemoreBlackMirror::encode(Command::Tare));
    TEST_ASSERT_EQUAL_HEX8(0x08, *TimemoreBlackMirror::encode(Command::TimerStart));
    TEST_ASSERT_EQUAL_HEX8(0x09, *TimemoreBlackMirror::encode(Command::TimerStop));
    TEST_ASSERT_EQUAL_HEX8(0x0A, *TimemoreBlackMirror::encode(Command::TimerReset));
}

void testTimemoreA55ADecode()
{
    const Bytes positive = hex("A5 5A 01 01 00 08 00 00 07 D0 00 00 00 00 64 88");
    const Bytes negative = hex("A5 5A 01 01 00 08 FF FF FF 83 00 00 00 00 DB 59");
    TimemoreBasic basic;
    TimemoreDot dot;
    TEST_ASSERT_EQUAL_INT32(2000, *feed(basic, positive));
    TEST_ASSERT_EQUAL_INT32(-125, *feed(basic, negative));

    Bytes badCrc = positive;
    badCrc.back() ^= 0xFF;
    TEST_ASSERT_FALSE(feed(basic, badCrc).has_value()); // Basic checks the CRC
    TEST_ASSERT_EQUAL_INT32(2000, *feed(dot, badCrc));   // Dot does not
    TEST_ASSERT_FALSE(feed(dot, hex("A5 5A 01 05 00 02 00 50 00 00")).has_value()); // battery
}

void testTimemoreA55ACommandsAndInit()
{
    TEST_ASSERT_BYTES("A5 5A 03 0D 00 00 64 D1", timemore::encode(Command::Tare)->bytes());
    TEST_ASSERT_BYTES("A5 5A 03 02 00 01 01 18 67", timemore::encode(Command::TimerStart)->bytes());
    TEST_ASSERT_BYTES("A5 5A 03 02 00 01 02 19 27", timemore::encode(Command::TimerStop)->bytes());
    TEST_ASSERT_BYTES("A5 5A 03 02 00 01 03 D9 E6", timemore::encode(Command::TimerReset)->bytes());

    FakeTransport transport;
    TimemoreDot driver;
    driver.connected(transport, 0);
    TEST_ASSERT_EQUAL(0, transport.writes.size());
    driver.tick(transport, 500);
    TEST_ASSERT_BYTES("A5 5A 03 06 00 01 00 E8 A7", transport.writes[0]); // unit grams
    driver.tick(transport, 700);
    TEST_ASSERT_BYTES("A5 5A 03 08 00 02 01 00 EB 31", transport.writes[1]); // standard mode
}

void testTimemoreMatchingOrder()
{
    TEST_ASSERT_TRUE(TimemoreBlackMirror::matches(named("Timemore Scale")));
    TEST_ASSERT_TRUE(TimemoreBasic::matches(named("TIMEMORE Basic 3")));
    TEST_ASSERT_TRUE(TimemoreBasic::matches(named("Basic3")));
    TEST_ASSERT_TRUE(TimemoreDot::matches(named("TES017")));
    TEST_ASSERT_TRUE(TimemoreDot::matches(named("Timemore Dot")));
    TEST_ASSERT_FALSE(TimemoreDot::matches(named("Dotti's phone")));
}

// --- Read-only scales ---

void testSmartchefAndBlackcoffee()
{
    Smartchef smartchef;
    TEST_ASSERT_EQUAL_INT32(2000, *feed(smartchef, hex("00 00 00 00 00 07 D0 00 00")));
    TEST_ASSERT_EQUAL_INT32(-2000, *feed(smartchef, hex("00 00 00 0B 00 07 D0 00 00")));
    FakeTransport transport;
    TEST_ASSERT_FALSE(smartchef.send(Command::Tare, transport)); // bridge zeroes in software

    Blackcoffee blackcoffee;
    TEST_ASSERT_EQUAL_INT32(2000, *feed(blackcoffee, hex("00 00 01 00 03 0D 40 00 00 00 00 00 00 00 00")));
    TEST_ASSERT_EQUAL_INT32(-2000, *feed(blackcoffee, hex("00 00 81 00 03 0D 40 00 00 00 00 00 00 00 00")));
    TEST_ASSERT_TRUE(Blackcoffee::matches(named("my_scale")));
}

void testDiyScales()
{
    DiyPythonScale python;
    TEST_ASSERT_EQUAL_INT32(185, *feed(python, hex("07 3A")));
    DiyRustScale rust;
    TEST_ASSERT_EQUAL_INT32(185, *feed(rust, hex("00 00 07 3A")));
    TEST_ASSERT_EQUAL_INT32(-10, *feed(rust, hex("FF FF FF 9C")));
    TEST_ASSERT_TRUE(DiyRustScale::matches(named("coffee-scale")));
    TEST_ASSERT_FALSE(DiyRustScale::matches(named("my coffee-scale 2")));
    TEST_ASSERT_TRUE(DiyPythonScale::matches(named("mpy-coffee")));
}

// --- Futula ---

void testFutula()
{
    Futula driver;
    TEST_ASSERT_EQUAL_INT32(185, *feed(driver, hex("00 00 00 B9 00 00 00 00 04")));
    TEST_ASSERT_EQUAL_INT32(-185, *feed(driver, hex("00 00 00 B9 00 01 00 00 04")));

    FakeTransport transport;
    driver.connected(transport, 0);
    TEST_ASSERT_BYTES("FD 00 04 00 00 00 00 00 00 00 F9", transport.writes[0]); // unit grams
    TEST_ASSERT_TRUE(driver.send(Command::Tare, transport));
    TEST_ASSERT_BYTES("FD 32 00 00 00 00 00 00 00 00 CF", transport.writes[1]);
    TEST_ASSERT_FALSE(driver.send(Command::TimerStart, transport));
    TEST_ASSERT_TRUE(Futula::matches(named("LFSmart Scale")));
    TEST_ASSERT_TRUE(Futula::matches(named("LEFU-CK811")));
    TEST_ASSERT_FALSE(Futula::matches(named("LEFU body fat")));
}

// --- WeighMaster ---

void testWeighMaster()
{
    WeighMaster driver;
    TEST_ASSERT_EQUAL_INT32(1000, *feed(driver, hex("01 02 00 80 00 03 E8")));  // source: +100.0 g stable
    TEST_ASSERT_EQUAL_INT32(-1000, *feed(driver, hex("01 02 00 90 00 03 E8"))); // source: -100.0 g
    TEST_ASSERT_EQUAL_INT32(-1234, *feed(driver, hex("01 02 00 10 00 04 D2"))); // talla test

    FakeTransport transport;
    driver.connected(transport, 0);
    TEST_ASSERT_TRUE(driver.send(Command::Tare, transport));
    TEST_ASSERT_BYTES("02", transport.writes[0]);
    driver.tick(transport, 300);
    TEST_ASSERT_BYTES("05 00", transport.writes[1]); // beep after tare
    TEST_ASSERT_TRUE(driver.send(Command::TimerStart, transport));
    TEST_ASSERT_BYTES("52 0B 01 00 00 00 00", transport.writes[2]);
}

// --- WeighMyBru ---

void testWeighMyBru()
{
    WeighMyBru driver;
    TEST_ASSERT_EQUAL_INT32(185, *feed(driver, hex("00 00 94 41"))); // 18.5f
    TEST_ASSERT_BYTES("03 0A 01 01 00", *WeighMyBru::encode(Command::Tare));
    TEST_ASSERT_BYTES("03 0A 04 01 00", *WeighMyBru::encode(Command::TimerReset));
    TEST_ASSERT_TRUE(WeighMyBru::matches(named("WeighMyBru")));
}

// --- Goat Story Gina ---

void testGinaUsesCalibration()
{
    // talla unit test: calibration 12500 -> factor 1.25; raw 2500 -> 200.0 g.
    FakeTransport transport;
    transport.reads["91341523-bac2-42d9-bbb3-942f7a10976c"] = hex("D4 30 00 00");
    GoatStoryGina driver;
    driver.connected(transport, 0);
    TEST_ASSERT_EQUAL_INT32(2000, *feed(driver, hex("C4 09 00 00")));

    GoatStoryGina uncalibrated;
    FakeTransport empty;
    uncalibrated.connected(empty, 0);
    TEST_ASSERT_FALSE(feed(uncalibrated, hex("C4 09 00 00")).has_value());
    TEST_ASSERT_FALSE(GoatStoryGina::calibrationFactor(hex("00 00 00 00")).has_value());
    TEST_ASSERT_FALSE(GoatStoryGina::matches(named("Gina"))); // service UUID only
}

} // namespace

void runMoreDriverTests()
{
    RUN_TEST(testBookooDecodeAndCommands);
    RUN_TEST(testTimemoreBlackMirror);
    RUN_TEST(testTimemoreA55ADecode);
    RUN_TEST(testTimemoreA55ACommandsAndInit);
    RUN_TEST(testTimemoreMatchingOrder);
    RUN_TEST(testSmartchefAndBlackcoffee);
    RUN_TEST(testDiyScales);
    RUN_TEST(testFutula);
    RUN_TEST(testWeighMaster);
    RUN_TEST(testWeighMyBru);
    RUN_TEST(testGinaUsesCalibration);
}
