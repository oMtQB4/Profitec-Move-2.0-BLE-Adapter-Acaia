#include <cstring>
#include <string>
#include <vector>

#include "unity.h"

#include "ota/AppImage.hpp"
#include "ota/OtaProtocol.hpp"
#include "ota/OtaSession.hpp"
#include "ota/Triggers.hpp"

using namespace ota;

namespace {

using Bytes = std::vector<uint8_t>;

constexpr const char* kProject = "acaia-bridge";

/** Start of an app image as the ESP-IDF build lays it out, padded to `size`. */
Bytes makeImage(size_t size, const char* project = kProject, uint16_t chipId = AppImage::kChipIdEsp32s3,
                const char* version = "1.2.3")
{
    Bytes image(size, 0x5A);
    image[0] = 0xE9;
    image[12] = static_cast<uint8_t>(chipId);
    image[13] = static_cast<uint8_t>(chipId >> 8);
    const uint8_t descMagic[] = {0x32, 0x54, 0xCD, 0xAB};
    std::memcpy(&image[32], descMagic, sizeof(descMagic));
    std::memset(&image[32 + 16], 0, 32);
    std::memcpy(&image[32 + 16], version, std::strlen(version));
    std::memset(&image[32 + 48], 0, 32);
    std::memcpy(&image[32 + 48], project, std::strlen(project));
    return image;
}

Bytes beginCommand(uint32_t size)
{
    return {0x01, static_cast<uint8_t>(size), static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size >> 16),
            static_cast<uint8_t>(size >> 24)};
}

Bytes dataPacket(uint16_t seq, const Bytes& image, size_t offset, size_t length)
{
    Bytes packet{static_cast<uint8_t>(seq), static_cast<uint8_t>(seq >> 8)};
    packet.insert(packet.end(), image.begin() + offset, image.begin() + offset + length);
    return packet;
}

uint32_t u32At(const Bytes& frame, size_t offset)
{
    return frame[offset] | frame[offset + 1] << 8 | frame[offset + 2] << 16 | static_cast<uint32_t>(frame[offset + 3]) << 24;
}

class FakeFlash : public FlashTarget {
public:
    uint32_t capacity() const override { return capacityBytes; }
    Status begin(uint32_t) override
    {
        ++begins;
        active = true;
        content.clear();
        return beginStatus;
    }
    Status write(std::span<const uint8_t> data) override
    {
        content.insert(content.end(), data.begin(), data.end());
        return Status::Ok;
    }
    Status finish() override
    {
        active = false;
        finished = finishStatus == Status::Ok;
        return finishStatus;
    }
    void abort() override
    {
        active = false;
        ++aborts;
    }

    uint32_t capacityBytes = 0x1C0000;
    Status beginStatus = Status::Ok;
    Status finishStatus = Status::Ok;
    Bytes content;
    bool active = false;
    bool finished = false;
    int begins = 0;
    int aborts = 0;
};

class FakeNotifier : public Notifier {
public:
    void notify(std::span<const uint8_t> frame) override { frames.emplace_back(frame.begin(), frame.end()); }
    const Bytes& last() const { return frames.back(); }
    std::vector<Bytes> frames;
};

struct Fixture {
    Fixture()
        : session(flash, notifier, {.projectName = kProject, .window = 4})
    {
        session.setEnabled(true);
    }

    /** Sends the whole image in `chunk`-byte packets. */
    void transfer(const Bytes& image, uint16_t chunk)
    {
        session.onControl(beginCommand(static_cast<uint32_t>(image.size())), chunk, 0);
        uint16_t seq = 0;
        for (size_t offset = 0; offset < image.size(); offset += chunk) {
            session.onData(dataPacket(seq++, image, offset, std::min<size_t>(chunk, image.size() - offset)), 0);
        }
    }

    FakeFlash flash;
    FakeNotifier notifier;
    OtaSession session;
};

// --- protocol ---------------------------------------------------------------

void testChunkSizeFollowsMtu()
{
    TEST_ASSERT_EQUAL_UINT16(18, chunkSizeForMtu(23));  // default MTU: 20 - 2
    TEST_ASSERT_EQUAL_UINT16(180, chunkSizeForMtu(185)); // iOS
    TEST_ASSERT_EQUAL_UINT16(510, chunkSizeForMtu(517)); // capped by the 512-byte attribute limit
    TEST_ASSERT_EQUAL_UINT16(18, chunkSizeForMtu(0));
}

void testCommandParsing()
{
    TEST_ASSERT_TRUE(parseBegin(beginCommand(0x00012345))->imageSize == 0x00012345);
    TEST_ASSERT_FALSE(parseBegin(Bytes{0x01, 0x00}).has_value());
    TEST_ASSERT_TRUE(commandOpcode(Bytes{0x02}) == Opcode::End);
    TEST_ASSERT_FALSE(commandOpcode(Bytes{0x02, 0x00}).has_value());
    TEST_ASSERT_FALSE(commandOpcode(Bytes{0x7F}).has_value());
    TEST_ASSERT_FALSE(parseData(Bytes{0x00, 0x00}).has_value()); // header only
    const auto packet = parseData(Bytes{0x34, 0x12, 0xAA});
    TEST_ASSERT_EQUAL_UINT16(0x1234, packet->seq);
    TEST_ASSERT_EQUAL(1, packet->payload.size());
}

void testInfoEncoding()
{
    const Bytes info = encodeInfo({.updateMode = true,
                                   .transferring = false,
                                   .chunk = 510,
                                   .window = 16,
                                   .capacity = 0x1C0000,
                                   .firmwareVersion = "1.2.3"});
    const Bytes expected{1, 0x01, 0xFE, 0x01, 16, 0, 0x00, 0x00, 0x1C, 0x00, '1', '.', '2', '.', '3'};
    TEST_ASSERT_TRUE(info == expected);
}

// --- image header -----------------------------------------------------------

void testAppImageParsesHeader()
{
    const auto image = AppImage::parse(makeImage(AppImage::kHeaderLength));
    TEST_ASSERT_TRUE(image.has_value());
    TEST_ASSERT_EQUAL_UINT16(AppImage::kChipIdEsp32s3, image->chipId);
    TEST_ASSERT_EQUAL_STRING(kProject, image->projectName.c_str());
    TEST_ASSERT_EQUAL_STRING("1.2.3", image->version.c_str());
}

void testAppImageRejectsGarbage()
{
    TEST_ASSERT_FALSE(AppImage::parse(makeImage(AppImage::kHeaderLength - 1)).has_value());
    Bytes noMagic = makeImage(AppImage::kHeaderLength);
    noMagic[0] = 0x00;
    TEST_ASSERT_FALSE(AppImage::parse(noMagic).has_value());
    Bytes noDesc = makeImage(AppImage::kHeaderLength);
    noDesc[32] = 0x00;
    TEST_ASSERT_FALSE(AppImage::parse(noDesc).has_value());
}

// --- session ----------------------------------------------------------------

void testFullTransferVerifiesAndFinishes()
{
    Fixture f;
    const Bytes image = makeImage(2000);
    f.transfer(image, 180);
    TEST_ASSERT_TRUE(f.flash.content == image);
    TEST_ASSERT_EQUAL_UINT32(2000, f.session.bytesWritten());
    TEST_ASSERT_EQUAL_STRING("1.2.3", f.session.imageVersion().c_str());

    // Begin response carries chunk and window.
    TEST_ASSERT_TRUE(f.notifier.frames.front() == (Bytes{0x81, 0, 180, 0, 4, 0}));
    // Last progress acknowledges everything.
    TEST_ASSERT_EQUAL_HEX8(0x90, f.notifier.last()[0]);
    TEST_ASSERT_EQUAL_UINT32(2000, u32At(f.notifier.last(), 2));

    f.session.onControl(Bytes{0x02}, 180, 0);
    TEST_ASSERT_TRUE(f.flash.finished);
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Done);
    TEST_ASSERT_TRUE(f.notifier.last() == (Bytes{0x82, 0}));
}

void testProgressEveryHalfWindow()
{
    Fixture f; // window 4: acknowledge every 2 packets
    const Bytes image = makeImage(1000);
    f.session.onControl(beginCommand(1000), 100, 0);
    f.session.onData(dataPacket(0, image, 0, 100), 0);
    TEST_ASSERT_EQUAL(1, f.notifier.frames.size()); // header still held back
    f.session.onData(dataPacket(1, image, 100, 100), 0);
    TEST_ASSERT_EQUAL(2, f.notifier.frames.size());
    TEST_ASSERT_EQUAL_UINT32(200, u32At(f.notifier.last(), 2)); // acknowledged although the header is held back
    TEST_ASSERT_TRUE(f.flash.content.empty());
    f.session.onData(dataPacket(2, image, 200, 100), 0);
    f.session.onData(dataPacket(3, image, 300, 100), 0);
    TEST_ASSERT_EQUAL(3, f.notifier.frames.size());
    TEST_ASSERT_EQUAL_UINT32(400, u32At(f.notifier.last(), 2));
}

void testRejectsBeginOutsideUpdateMode()
{
    Fixture f;
    f.session.setEnabled(false);
    f.session.onControl(beginCommand(1000), 180, 0);
    TEST_ASSERT_EQUAL_HEX8(0x81, f.notifier.last()[0]);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::NotInUpdateMode), f.notifier.last()[1]);
    TEST_ASSERT_EQUAL(0, f.flash.begins);
}

void testRejectsInvalidSizes()
{
    Fixture f;
    f.flash.capacityBytes = 4096;
    f.session.onControl(beginCommand(4097), 180, 0);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::InvalidSize), f.notifier.last()[1]);
    f.session.onControl(beginCommand(AppImage::kHeaderLength - 1), 180, 0);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::InvalidSize), f.notifier.last()[1]);
    TEST_ASSERT_EQUAL(0, f.flash.begins);
}

void testForeignImageNeverReachesFlash()
{
    const Bytes foreign[] = {makeImage(1000, "other-project"), makeImage(1000, kProject, 5 /* ESP32-C3 */)};
    for (const Bytes& image : foreign) {
        Fixture f;
        f.transfer(image, 180);
        TEST_ASSERT_TRUE(f.flash.content.empty());
        TEST_ASSERT_EQUAL(1, f.flash.aborts);
        TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Idle);
        // Reported once, not once per packet still in flight.
        int failures = 0;
        for (const Bytes& frame : f.notifier.frames) {
            failures += frame[0] == 0x90 && frame[1] != 0 ? 1 : 0;
        }
        TEST_ASSERT_EQUAL(1, failures);
        TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::InvalidImage), f.notifier.frames[1][1]);
    }
}

void testMissingPacketAborts()
{
    Fixture f;
    const Bytes image = makeImage(1000);
    f.session.onControl(beginCommand(1000), 300, 0);
    f.session.onData(dataPacket(0, image, 0, 300), 0);
    f.session.onData(dataPacket(2, image, 600, 300), 0);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::SequenceError), f.notifier.last()[1]);
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Idle);
    TEST_ASSERT_EQUAL(1, f.flash.aborts);
}

void testTooMuchDataAborts()
{
    Fixture f;
    const Bytes image = makeImage(600);
    f.session.onControl(beginCommand(500), 300, 0);
    f.session.onData(dataPacket(0, image, 0, 300), 0);
    f.session.onData(dataPacket(1, image, 300, 300), 0);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::TooMuchData), f.notifier.last()[1]);
}

void testEndBeforeAllData()
{
    Fixture f;
    const Bytes image = makeImage(1000);
    f.session.onControl(beginCommand(1000), 300, 0);
    f.session.onData(dataPacket(0, image, 0, 300), 0);
    f.session.onControl(Bytes{0x02}, 300, 0);
    TEST_ASSERT_TRUE(f.notifier.last() == (Bytes{0x82, static_cast<uint8_t>(Status::Incomplete)}));
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Receiving); // the client may still send the rest
    Fixture idle;
    idle.session.onControl(Bytes{0x02}, 300, 0);
    TEST_ASSERT_TRUE(idle.notifier.last() == (Bytes{0x82, static_cast<uint8_t>(Status::NotStarted)}));
}

void testVerifyFailureLeavesSessionIdle()
{
    Fixture f;
    f.flash.finishStatus = Status::VerifyFailed;
    f.transfer(makeImage(800), 200);
    f.session.onControl(Bytes{0x02}, 200, 0);
    TEST_ASSERT_TRUE(f.notifier.last() == (Bytes{0x82, static_cast<uint8_t>(Status::VerifyFailed)}));
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Idle);
}

void testBeginAgainRestarts()
{
    Fixture f;
    const Bytes image = makeImage(1000);
    f.session.onControl(beginCommand(1000), 300, 0);
    f.session.onData(dataPacket(0, image, 0, 300), 0);
    f.session.onControl(beginCommand(1000), 300, 0); // client reconnected
    TEST_ASSERT_EQUAL(1, f.flash.aborts);
    TEST_ASSERT_EQUAL(2, f.flash.begins);
    f.session.onData(dataPacket(0, image, 0, 300), 0); // sequence starts over
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Receiving);
}

void testTimeoutAndDisconnectAbort()
{
    Fixture f;
    f.session.onControl(beginCommand(1000), 300, 1000);
    f.session.poll(10999);
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Receiving);
    f.session.poll(11000);
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Idle);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::Timeout), f.notifier.last()[1]);

    f.session.onControl(beginCommand(1000), 300, 0);
    f.session.onDisconnect();
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Idle);
    TEST_ASSERT_EQUAL(2, f.flash.aborts);
}

void testOverflowAndAbortCommand()
{
    Fixture f;
    f.session.onControl(beginCommand(1000), 300, 0);
    f.session.onOverflow();
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::Overflow), f.notifier.last()[1]);

    f.session.onControl(beginCommand(1000), 300, 0);
    f.session.onControl(Bytes{0x03}, 300, 0);
    TEST_ASSERT_TRUE(f.notifier.last() == (Bytes{0x83, 0}));
    TEST_ASSERT_TRUE(f.session.state() == OtaSession::State::Idle);
}

void testDataWithoutBegin()
{
    Fixture f;
    f.session.onData(Bytes{0, 0, 1, 2, 3}, 0);
    f.session.onData(Bytes{1, 0, 1, 2, 3}, 0);
    TEST_ASSERT_EQUAL(1, f.notifier.frames.size());
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(Status::NotStarted), f.notifier.last()[1]);
}

void testSequenceWrapsAround()
{
    Fixture f;
    const size_t size = 70000;
    const Bytes image = makeImage(size);
    f.transfer(image, 1); // 70000 packets: seq wraps past 0xFFFF
    TEST_ASSERT_EQUAL_UINT32(size, f.session.bytesWritten());
    TEST_ASSERT_TRUE(f.flash.content == image);
}

// --- triggers ---------------------------------------------------------------

void testLongPressFiresOnceAfterHold()
{
    LongPress press(3000);
    TEST_ASSERT_FALSE(press.update(false, 0));
    TEST_ASSERT_FALSE(press.update(true, 100));
    TEST_ASSERT_FALSE(press.update(true, 3099));
    TEST_ASSERT_TRUE(press.update(true, 3100));
    TEST_ASSERT_FALSE(press.update(true, 9000)); // still held: no repeat
    TEST_ASSERT_FALSE(press.update(false, 9010));
    TEST_ASSERT_FALSE(press.update(true, 9020));
    TEST_ASSERT_FALSE(press.update(false, 9500)); // short press
    TEST_ASSERT_FALSE(press.update(true, 10000));
    TEST_ASSERT_TRUE(press.update(true, 13000));
}

void testPowerCyclesEnterUpdateMode()
{
    auto boot = [](uint8_t stored, bool powerOn) { return PowerCycleCount::onBoot(stored, powerOn, 3); };
    TEST_ASSERT_EQUAL_UINT8(1, boot(0, true).stored);
    TEST_ASSERT_FALSE(boot(0, true).enterUpdateMode);
    TEST_ASSERT_EQUAL_UINT8(2, boot(1, true).stored);
    TEST_ASSERT_TRUE(boot(2, true).enterUpdateMode);
    TEST_ASSERT_EQUAL_UINT8(0, boot(2, true).stored); // the next power-on starts counting again
    // A crash or software restart neither counts nor resets.
    TEST_ASSERT_EQUAL_UINT8(2, boot(2, false).stored);
    TEST_ASSERT_FALSE(boot(2, false).enterUpdateMode);
    // Disabled.
    TEST_ASSERT_FALSE(PowerCycleCount::onBoot(5, true, 0).enterUpdateMode);
    TEST_ASSERT_EQUAL_UINT8(0, PowerCycleCount::onBoot(5, true, 0).stored);
    // A stale count above the setting still triggers.
    TEST_ASSERT_TRUE(PowerCycleCount::onBoot(7, true, 3).enterUpdateMode);
}

} // namespace

void runOtaTests()
{
    RUN_TEST(testChunkSizeFollowsMtu);
    RUN_TEST(testCommandParsing);
    RUN_TEST(testInfoEncoding);
    RUN_TEST(testAppImageParsesHeader);
    RUN_TEST(testAppImageRejectsGarbage);
    RUN_TEST(testFullTransferVerifiesAndFinishes);
    RUN_TEST(testProgressEveryHalfWindow);
    RUN_TEST(testRejectsBeginOutsideUpdateMode);
    RUN_TEST(testRejectsInvalidSizes);
    RUN_TEST(testForeignImageNeverReachesFlash);
    RUN_TEST(testMissingPacketAborts);
    RUN_TEST(testTooMuchDataAborts);
    RUN_TEST(testEndBeforeAllData);
    RUN_TEST(testVerifyFailureLeavesSessionIdle);
    RUN_TEST(testBeginAgainRestarts);
    RUN_TEST(testTimeoutAndDisconnectAbort);
    RUN_TEST(testOverflowAndAbortCommand);
    RUN_TEST(testDataWithoutBegin);
    RUN_TEST(testSequenceWrapsAround);
    RUN_TEST(testLongPressFiresOnceAfterHold);
    RUN_TEST(testPowerCyclesEnterUpdateMode);
}
