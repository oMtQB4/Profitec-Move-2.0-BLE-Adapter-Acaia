#include "unity.h"

#include "bridge/ClientIdentifier.hpp"

using namespace bridge;

namespace {

using I = ClientIdentifier;

/** Public address, as an embedded machine would use. */
PeerAddress machineAddress()
{
    return PeerAddress{{0x82, 0xE3, 0x80, 0x2F, 0x37, 0x28}, PeerAddress::kPublic};
}

/** Resolvable private address (top bits 01), as phones and computers use. */
PeerAddress phoneAddress()
{
    return PeerAddress{{0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, PeerAddress::kRandom};
}

void testAddressStability()
{
    TEST_ASSERT_TRUE(machineAddress().isStable());
    TEST_ASSERT_FALSE(phoneAddress().isStable());
    TEST_ASSERT_TRUE((PeerAddress{{0, 0, 0, 0, 0, 0xC1}, PeerAddress::kRandom}.isStable())); // static random
    TEST_ASSERT_FALSE((PeerAddress{{0, 0, 0, 0, 0, 0x01}, PeerAddress::kRandom}.isStable())); // non-resolvable
    TEST_ASSERT_EQUAL_STRING("28:37:2F:80:E3:82", machineAddress().toString().c_str());
}

void testSilentSubscriberIsLearnedAsMachine()
{
    I id;
    id.connected(machineAddress(), 0);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Identifying);
    id.subscribed(200);
    id.tick(200 + I::kQuietMs - 1);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Identifying);
    id.tick(200 + I::kQuietMs);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Machine);

    const auto learned = id.takeLearned();
    TEST_ASSERT_TRUE(learned.has_value());
    TEST_ASSERT_TRUE(*learned == machineAddress());
    TEST_ASSERT_FALSE(id.takeLearned().has_value()); // reported once
}

void testKnownMachineIsRecognisedAtOnce()
{
    I id;
    id.setKnownMachine(machineAddress());
    id.connected(machineAddress(), 0);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Machine);
    id.wrote(); // even a machine that starts sending commands stays the machine
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Machine);
    TEST_ASSERT_FALSE(id.takeLearned().has_value());
}

void testAppSendingIdentIsOther()
{
    I id;
    id.connected(machineAddress(), 0);
    id.subscribed(100);
    id.wrote();
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Other);
    id.tick(10000);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Other);
    TEST_ASSERT_FALSE(id.takeLearned().has_value());
}

void testClientWithoutSubscriptionIsOther()
{
    I id;
    id.connected(machineAddress(), 0); // e.g. the configurator on a computer with a public address
    id.tick(I::kSubscribeMs - 1);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Identifying);
    id.tick(I::kSubscribeMs);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Other);
    TEST_ASSERT_FALSE(id.takeLearned().has_value());
}

void testUnknownAddressWhileMachineKnownIsIdentified()
{
    I id;
    id.setKnownMachine(machineAddress());
    id.connected(phoneAddress(), 0);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Identifying);
    id.subscribed(0);
    id.wrote();
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Other);
}

void testPrivateAddressIsNotLearned()
{
    I id;
    id.connected(phoneAddress(), 0);
    id.subscribed(0);
    id.tick(I::kQuietMs);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Machine); // behaves like one
    TEST_ASSERT_FALSE(id.takeLearned().has_value());   // but can't be recognised again
    TEST_ASSERT_FALSE(id.knownMachine().has_value());
}

void testForgetAndDisconnect()
{
    I id;
    id.setKnownMachine(machineAddress());
    id.connected(machineAddress(), 0);
    id.disconnected();
    TEST_ASSERT_TRUE(id.kind() == ClientKind::None);
    id.forgetMachine();
    TEST_ASSERT_FALSE(id.knownMachine().has_value());
    id.connected(machineAddress(), 1000);
    TEST_ASSERT_TRUE(id.kind() == ClientKind::Identifying);
}

} // namespace

void runClientIdentifierTests()
{
    RUN_TEST(testAddressStability);
    RUN_TEST(testSilentSubscriberIsLearnedAsMachine);
    RUN_TEST(testKnownMachineIsRecognisedAtOnce);
    RUN_TEST(testAppSendingIdentIsOther);
    RUN_TEST(testClientWithoutSubscriptionIsOther);
    RUN_TEST(testUnknownAddressWhileMachineKnownIsIdentified);
    RUN_TEST(testPrivateAddressIsNotLearned);
    RUN_TEST(testForgetAndDisconnect);
}
