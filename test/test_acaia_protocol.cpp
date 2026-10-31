/* Byte vectors come from AcaiaArduinoBLE and pyacaia (see AcaiaProtocol.hpp). */

#include <array>
#include <vector>

#include "unity.h"

#include "acaia/AcaiaProtocol.hpp"

using namespace acaia;

namespace {

std::vector<uint8_t> toVector(const Frame& frame)
{
    return {frame.bytes().begin(), frame.bytes().end()};
}

/** Decodes a weight frame the way AcaiaArduinoBLE does. */
float referenceDecodeWeight(const Frame& in)
{
    float value = static_cast<float>((in[6] << 8) + in[5]);
    for (int i = 0; i < in[9]; i++) {
        value /= 10.0f;
    }
    return (in[10] & 0x02) ? -value : value;
}

Command classifySingle(std::span<const uint8_t> data)
{
    const auto frames = split(data);
    TEST_ASSERT_EQUAL(1, frames.size());
    return classify(frames[0]);
}

void testEncodeMatchesReferenceIdent()
{
    const std::vector<uint8_t> expected{0xEF, 0xDD, 0x0B, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                                        0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9A, 0x6D};
    const std::array<uint8_t, 15> payload{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4'};
    const auto actual = toVector(encode(MessageType::Ident, payload));
    TEST_ASSERT_EQUAL(expected.size(), actual.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected.data(), actual.data(), expected.size());
}

void testEncodeMatchesReferenceNotificationRequest()
{
    const std::vector<uint8_t> expected{0xEF, 0xDD, 0x0C, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06};
    const std::array<uint8_t, 9> payload{0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04};
    const auto actual = toVector(encode(MessageType::Event, payload));
    TEST_ASSERT_EQUAL(expected.size(), actual.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected.data(), actual.data(), expected.size());
}

void testWeightFrameMatchesRealLunar2021()
{
    // aioacaia fixture captured from a real scale: 175.9 g, timer 0.2 s.
    const std::vector<uint8_t> expected{0xEF, 0xDD, 0x0C, 0x0C, 0x05, 0xDF, 0x06, 0x00, 0x00,
                                        0x01, 0x00, 0x07, 0x00, 0x00, 0x02, 0xF3, 0x0D};
    const auto actual = toVector(encodeWeight(1759, 2));
    TEST_ASSERT_EQUAL(expected.size(), actual.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected.data(), actual.data(), expected.size());
}

void testWeightFrame()
{
    const Frame frame = encodeWeight(365);
    TEST_ASSERT_EQUAL(17, frame.size());
    TEST_ASSERT_EQUAL_HEX8(0x0C, frame[2]);
    TEST_ASSERT_EQUAL_HEX8(0x05, frame[4]);
    TEST_ASSERT_EQUAL(frame.size(), frame[3] + 5u); // pyacaia: length = payload[0] + 5
    TEST_ASSERT_EQUAL_FLOAT(36.5f, referenceDecodeWeight(frame));
    TEST_ASSERT_EQUAL_FLOAT(-1.2f, referenceDecodeWeight(encodeWeight(-12)));
}

void testTimerFrame()
{
    const Frame frame = encodeTimer(834); // 1:23.4
    TEST_ASSERT_EQUAL(frame.size(), frame[3] + 5u);
    TEST_ASSERT_EQUAL_HEX8(0x07, frame[4]);
    TEST_ASSERT_EQUAL(1, frame[5]);
    TEST_ASSERT_EQUAL(23, frame[6]);
    TEST_ASSERT_EQUAL(4, frame[7]);
}

void testStatusFrame()
{
    const Frame frame = encodeStatus(87);
    TEST_ASSERT_EQUAL(frame.size(), frame[3] + 5u);
    TEST_ASSERT_EQUAL_HEX8(0x08, frame[2]);
    TEST_ASSERT_EQUAL(87, frame[4] & 0x7F);
    TEST_ASSERT_EQUAL(2, frame[5]); // grams
}

void testClassifyClientCommands()
{
    const std::array<uint8_t, 7> heartbeat{0xEF, 0xDD, 0x00, 0x02, 0x00, 0x02, 0x00};
    const std::array<uint8_t, 6> tareShort{0xEF, 0xDD, 0x04, 0x00, 0x00, 0x00};
    const std::array<uint8_t, 20> tareLong{0xEF, 0xDD, 0x04};
    const std::array<uint8_t, 7> start{0xEF, 0xDD, 0x0D, 0x00, 0x00, 0x00, 0x00};
    const std::array<uint8_t, 7> stop{0xEF, 0xDD, 0x0D, 0x00, 0x02, 0x00, 0x02};
    const std::array<uint8_t, 7> reset{0xEF, 0xDD, 0x0D, 0x00, 0x01, 0x00, 0x01};
    const std::array<uint8_t, 14> notification{0xEF, 0xDD, 0x0C, 0x09, 0x00, 0x01, 0x01,
                                               0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06};

    TEST_ASSERT_TRUE(classifySingle(heartbeat) == Command::Heartbeat);
    TEST_ASSERT_TRUE(classifySingle(tareShort) == Command::Tare);
    TEST_ASSERT_TRUE(classifySingle(tareLong) == Command::Tare);
    TEST_ASSERT_TRUE(classifySingle(start) == Command::TimerStart);
    TEST_ASSERT_TRUE(classifySingle(stop) == Command::TimerStop);
    TEST_ASSERT_TRUE(classifySingle(reset) == Command::TimerReset);
    TEST_ASSERT_TRUE(classifySingle(notification) == Command::NotificationRequest);
}

void testSplitConcatenatedFrames()
{
    // garbage byte, ident, notification request in one write
    const std::array<uint8_t, 16> data{0x00, 0xEF, 0xDD, 0x0B, 0x30, 0x31, 0x9A, 0x6D,
                                       0xEF, 0xDD, 0x0C, 0x09, 0x00, 0x01, 0x15, 0x06};
    const auto frames = split(data);
    TEST_ASSERT_EQUAL(2, frames.size());
    TEST_ASSERT_TRUE(classify(frames[0]) == Command::Ident);
    TEST_ASSERT_EQUAL(4, frames[0].payload.size());
    TEST_ASSERT_TRUE(classify(frames[1]) == Command::NotificationRequest);
    TEST_ASSERT_EQUAL(5, frames[1].payload.size());
}

void testSplitIgnoresNoise()
{
    const std::array<uint8_t, 3> data{0x01, 0x02, 0xEF};
    TEST_ASSERT_TRUE(split(data).empty());
}

} // namespace

void runAcaiaProtocolTests()
{
    RUN_TEST(testEncodeMatchesReferenceIdent);
    RUN_TEST(testEncodeMatchesReferenceNotificationRequest);
    RUN_TEST(testWeightFrameMatchesRealLunar2021);
    RUN_TEST(testWeightFrame);
    RUN_TEST(testTimerFrame);
    RUN_TEST(testStatusFrame);
    RUN_TEST(testClassifyClientCommands);
    RUN_TEST(testSplitConcatenatedFrames);
    RUN_TEST(testSplitIgnoresNoise);
}
