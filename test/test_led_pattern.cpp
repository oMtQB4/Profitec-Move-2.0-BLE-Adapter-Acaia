#include "unity.h"

#include "status/LedPattern.hpp"

using namespace status;

namespace {

constexpr Color kOff{0, 0, 0};

void testStateFromConnections()
{
    TEST_ASSERT_TRUE(stateFrom(false, false) == State::SearchingScale);
    TEST_ASSERT_TRUE(stateFrom(false, true) == State::SearchingScale); // machine alone gets no weight
    TEST_ASSERT_TRUE(stateFrom(true, false) == State::WaitingForMachine);
    TEST_ASSERT_TRUE(stateFrom(true, true) == State::Bridging);
}

void testSearchingBlinksRed()
{
    const Color on = colorFor(State::SearchingScale, 0, 255);
    TEST_ASSERT_TRUE(on == (Color{255, 0, 0}));
    TEST_ASSERT_TRUE(colorFor(State::SearchingScale, 499, 255) == on);
    TEST_ASSERT_TRUE(colorFor(State::SearchingScale, 500, 255) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::SearchingScale, 1000, 255) == on);
}

void testConnectedStatesAreSteady()
{
    TEST_ASSERT_TRUE(colorFor(State::WaitingForMachine, 0, 255) == colorFor(State::WaitingForMachine, 500, 255));
    TEST_ASSERT_TRUE(colorFor(State::Bridging, 0, 255) == (Color{0, 0, 255}));
    TEST_ASSERT_TRUE(colorFor(State::Bridging, 500, 255) == (Color{0, 0, 255}));
}

void testBrightnessScalesChannels()
{
    TEST_ASSERT_TRUE(colorFor(State::Bridging, 0, 24) == (Color{0, 0, 24}));
    TEST_ASSERT_TRUE(colorFor(State::WaitingForMachine, 0, 0) == kOff);
}

} // namespace

void runLedPatternTests()
{
    RUN_TEST(testStateFromConnections);
    RUN_TEST(testSearchingBlinksRed);
    RUN_TEST(testConnectedStatesAreSteady);
    RUN_TEST(testBrightnessScalesChannels);
}
