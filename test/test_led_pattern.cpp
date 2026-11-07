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
    TEST_ASSERT_TRUE(stateFrom(true, true, false) == State::OtherClient);
    TEST_ASSERT_TRUE(stateFrom(false, true, false) == State::SearchingScale);
}

void testOtherClientIsGreen()
{
    TEST_ASSERT_TRUE(colorFor(State::OtherClient, 0, 255) == (Color{0, 255, 0}));
    TEST_ASSERT_TRUE(colorFor(State::OtherClient, 500, 255) == (Color{0, 255, 0}));
}

void testActiveOnlyShowsConnections()
{
    constexpr LedMode mode = LedMode::ActiveOnly;
    TEST_ASSERT_TRUE(colorFor(State::SearchingScale, 0, 255, mode) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::WaitingForMachine, 0, 255, mode) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::Bridging, 0, 255, mode) == (Color{0, 0, 255}));
    TEST_ASSERT_TRUE(colorFor(State::OtherClient, 0, 255, mode) == (Color{0, 255, 0}));
}

void testOffHidesEverythingButUpdates()
{
    constexpr LedMode mode = LedMode::Off;
    TEST_ASSERT_TRUE(colorFor(State::SearchingScale, 0, 255, mode) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::WaitingForMachine, 0, 255, mode) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::Bridging, 0, 255, mode) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::OtherClient, 0, 255, mode) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::UpdateMode, 0, 255, mode) == (Color{160, 0, 255}));
    TEST_ASSERT_TRUE(colorFor(State::Updating, 0, 255, mode) == (Color{160, 0, 255}));
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

void testUpdateStates()
{
    const Color purple = colorFor(State::UpdateMode, 0, 255);
    TEST_ASSERT_TRUE(purple == (Color{160, 0, 255}));
    TEST_ASSERT_TRUE(colorFor(State::UpdateMode, 500, 255) == purple);
    TEST_ASSERT_TRUE(colorFor(State::Updating, 0, 255) == purple);
    TEST_ASSERT_TRUE(colorFor(State::Updating, 100, 255) == kOff);
    TEST_ASSERT_TRUE(colorFor(State::Updating, 200, 255) == purple);
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
    RUN_TEST(testUpdateStates);
    RUN_TEST(testBrightnessScalesChannels);
    RUN_TEST(testOtherClientIsGreen);
    RUN_TEST(testActiveOnlyShowsConnections);
    RUN_TEST(testOffHidesEverythingButUpdates);
}
