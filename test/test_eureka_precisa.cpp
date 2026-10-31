/* Byte vectors come from Beanconqueror and de1app (see EurekaPrecisa.hpp). */

#include <array>
#include <vector>

#include "unity.h"

#include "scale/drivers/EurekaPrecisa.hpp"

using namespace scale;

namespace {

/** Records writes instead of sending them over BLE. */
class FakeTransport : public Transport {
public:
    bool write(std::span<const uint8_t> data) override
    {
        writes.emplace_back(data.begin(), data.end());
        return true;
    }
    std::vector<std::vector<uint8_t>> writes;
};

class RecordingListener : public Listener {
public:
    void onScaleConnected(std::string_view) override {}
    void onScaleDisconnected() override {}
    void onReading(const Reading& reading) override { readings.push_back(reading); }
    std::vector<Reading> readings;
};

void testMatchesAdvertisedNames()
{
    TEST_ASSERT_TRUE(EurekaPrecisa::matches({"CFS-9002"}));
    TEST_ASSERT_TRUE(EurekaPrecisa::matches({"LSJ-001"}));
    TEST_ASSERT_FALSE(EurekaPrecisa::matches({"CFS-90"}));
    TEST_ASSERT_FALSE(EurekaPrecisa::matches({"LUNAR-123456"}));
}

void testEncodesCommands()
{
    const std::array<uint8_t, 4> tare{0xAA, 0x02, 0x31, 0x31};
    const std::array<uint8_t, 4> start{0xAA, 0x02, 0x33, 0x33};
    const std::array<uint8_t, 4> stop{0xAA, 0x02, 0x34, 0x34};
    const std::array<uint8_t, 4> reset{0xAA, 0x02, 0x35, 0x35};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(tare.data(), EurekaPrecisa::encode(Command::Tare)->data(), 4);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(start.data(), EurekaPrecisa::encode(Command::TimerStart)->data(), 4);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(stop.data(), EurekaPrecisa::encode(Command::TimerStop)->data(), 4);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(reset.data(), EurekaPrecisa::encode(Command::TimerReset)->data(), 4);
}

void testDecodesWeight()
{
    const std::array<uint8_t, 11> positive{0xAA, 0x09, 0x41, 0x01, 0x0C, 0x00, 0x00, 0x6D, 0x01, 0x00, 0x00};
    const std::array<uint8_t, 9> negative{0xAA, 0x09, 0x41, 0x00, 0x00, 0x00, 0x01, 0x0F, 0x00};
    TEST_ASSERT_EQUAL_INT32(365, EurekaPrecisa::decode(positive)->weightDg);
    TEST_ASSERT_EQUAL_INT32(-15, EurekaPrecisa::decode(negative)->weightDg);
}

void testRejectsOtherFrames()
{
    const std::array<uint8_t, 9> wrongHeader{0xAA, 0x09, 0x42, 0, 0, 0, 0, 0, 0};
    const std::array<uint8_t, 8> tooShort{0xAA, 0x09, 0x41, 0, 0, 0, 0, 0};
    TEST_ASSERT_FALSE(EurekaPrecisa::decode(wrongHeader).has_value());
    TEST_ASSERT_FALSE(EurekaPrecisa::decode(tooShort).has_value());
}

void testDriverSetsGramsOnConnectAndPublishesReadings()
{
    FakeTransport transport;
    RecordingListener listener;
    EurekaPrecisa driver;
    driver.setListener(&listener);

    driver.onConnected(transport);
    TEST_ASSERT_EQUAL(1, transport.writes.size());
    const std::vector<uint8_t> grams{0xAA, 0x03, 0x36, 0x00};
    TEST_ASSERT_TRUE(transport.writes[0] == grams);

    const std::array<uint8_t, 9> frame{0xAA, 0x09, 0x41, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00};
    driver.onNotify(frame);
    TEST_ASSERT_EQUAL(1, listener.readings.size());
    TEST_ASSERT_EQUAL_INT32(42, listener.readings[0].weightDg);

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
