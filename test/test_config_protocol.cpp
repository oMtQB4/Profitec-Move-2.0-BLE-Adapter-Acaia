#include "TestSupport.hpp"

#include "config/ConfigProtocol.hpp"

using namespace config;

namespace {

void testEncodesAllSettings()
{
    Settings settings;
    settings.autoTareMode = AutoTareMode::MachineConnects;
    settings.tareScale = false;
    settings.ledMode = status::LedMode::ActiveOnly;
    settings.ledBrightness = 0x80;
    TEST_ASSERT_BYTES("00 01 01  01 01 01  02 01 01  03 01 00  04 01 01  05 01 80", encodeSettings(settings));
}

void testRoundTrip()
{
    Settings settings;
    settings.autoTare = false;
    settings.ledMode = status::LedMode::Off;
    settings.ledBrightness = 7;
    const auto data = encodeSettings(settings);
    Settings decoded;
    TEST_ASSERT_TRUE(decodeSettings(data, decoded));
    TEST_ASSERT_TRUE(decoded == settings);
}

void testPartialWriteKeepsTheRest()
{
    Settings settings;
    TEST_ASSERT_TRUE(decodeSettings(hex("03 01 00"), settings));
    TEST_ASSERT_FALSE(settings.tareScale);
    TEST_ASSERT_TRUE(settings.autoTare);
    TEST_ASSERT_EQUAL_UINT8(24, settings.ledBrightness);
}

void testUnknownKeysAreSkipped()
{
    Settings settings;
    TEST_ASSERT_TRUE(decodeSettings(hex("7F 03 AA BB CC  04 01 02  00 01 09"), settings));
    TEST_ASSERT_TRUE(settings.ledMode == status::LedMode::Off);
}

void testInvalidWriteChangesNothing()
{
    const Settings defaults;
    Settings settings;
    TEST_ASSERT_FALSE(decodeSettings(hex("03 01 00  05 01 00"), settings)); // brightness 0
    TEST_ASSERT_TRUE(settings == defaults);
    TEST_ASSERT_FALSE(decodeSettings(hex("01 01 02"), settings)); // not a bool
    TEST_ASSERT_FALSE(decodeSettings(hex("02 01 02"), settings)); // no such mode
    TEST_ASSERT_FALSE(decodeSettings(hex("04 01 03"), settings)); // no such LED mode
    TEST_ASSERT_FALSE(decodeSettings(hex("01 02 01 00"), settings)); // wrong length
    TEST_ASSERT_FALSE(decodeSettings(hex("01 01"), settings));       // truncated
    TEST_ASSERT_FALSE(decodeSettings(hex("01"), settings));
    TEST_ASSERT_TRUE(settings == defaults);
}

void testEncodesStatus()
{
    Status status;
    status.firmware = "1.2";
    status.scaleConnected = true;
    status.scaleName = "Eureka";
    status.client = bridge::ClientKind::Machine;
    status.machine = "AA";
    TEST_ASSERT_BYTES("00 01 01  01 01 00  02 03 31 2E 32  03 01 01  04 06 45 75 72 65 6B 61  05 01 02  06 02 41 41",
                      encodeStatus(status));
}

void testStatusLeavesOutWhatIsMissing()
{
    Status status;
    status.mode = AdapterMode::Update;
    status.scaleName = "stale";
    TEST_ASSERT_BYTES("00 01 01  01 01 01  02 00  03 01 00  05 01 00", encodeStatus(status));
}

} // namespace

void runConfigProtocolTests()
{
    RUN_TEST(testEncodesAllSettings);
    RUN_TEST(testRoundTrip);
    RUN_TEST(testPartialWriteKeepsTheRest);
    RUN_TEST(testUnknownKeysAreSkipped);
    RUN_TEST(testInvalidWriteChangesNothing);
    RUN_TEST(testEncodesStatus);
    RUN_TEST(testStatusLeavesOutWhatIsMissing);
}
