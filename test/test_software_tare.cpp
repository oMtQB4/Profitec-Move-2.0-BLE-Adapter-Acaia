#include "unity.h"

#include "bridge/SoftwareTare.hpp"

using namespace bridge;

namespace {

void testPassesThroughUntilZeroed()
{
    SoftwareTare tare;
    TEST_ASSERT_EQUAL_INT32(1500, tare.apply(1500));
}

void testZeroUsesLastReading()
{
    SoftwareTare tare;
    tare.apply(1500); // cup on the scale
    tare.zero();
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(1500));
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(1860)); // 36 g of espresso
}

void testClearRestoresRawReadings()
{
    SoftwareTare tare;
    tare.apply(1500);
    tare.zero();
    tare.clear();
    TEST_ASSERT_EQUAL_INT32(1500, tare.apply(1500));
}

void testHandsOverWhenScaleTareShowsUp()
{
    SoftwareTare tare;
    tare.apply(1500); // cup
    tare.zeroAndAwaitScaleTare();
    bool handedOver = true;

    // Scale has not tared yet; espresso starts flowing.
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(1500, &handedOver));
    TEST_ASSERT_FALSE(handedOver);
    TEST_ASSERT_EQUAL_INT32(12, tare.apply(1512, &handedOver));
    TEST_ASSERT_FALSE(handedOver);
    TEST_ASSERT_TRUE(tare.awaitingScaleTare());

    // Scale tare took effect: the hand-over reading repeats the last output, so the
    // relayed weight never jumps. The cost is at most one reading interval of flow
    // (here 0.3 g), since the moment the scale zeroed itself is unknown.
    TEST_ASSERT_EQUAL_INT32(12, tare.apply(15, &handedOver));
    TEST_ASSERT_TRUE(handedOver);
    TEST_ASSERT_TRUE(tare.scaleTared());
    TEST_ASSERT_EQUAL_INT32(357, tare.apply(360, &handedOver));
    TEST_ASSERT_FALSE(handedOver);
}

void testLateScaleTareKeepsWeightContinuous()
{
    SoftwareTare tare;
    tare.apply(1500); // cup
    tare.zeroAndAwaitScaleTare();
    TEST_ASSERT_EQUAL_INT32(150, tare.apply(1650)); // 15 g espresso, scale not tared yet

    // The scale tares only now and zeroes cup and espresso: raw drops to ~0.
    bool handedOver = false;
    TEST_ASSERT_EQUAL_INT32(150, tare.apply(0, &handedOver));
    TEST_ASSERT_TRUE(handedOver);
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(210)); // 21 g more -> 36 g in the cup
}

void testKeepsOffsetIfScaleNeverTares()
{
    SoftwareTare tare;
    tare.apply(1500);
    tare.zeroAndAwaitScaleTare();
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL_INT32(0, tare.apply(1500));
    }
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(1860));
    TEST_ASSERT_TRUE(tare.awaitingScaleTare());
    TEST_ASSERT_FALSE(tare.scaleTared());
}

void testRepeatedScaleTareKeepsWeightContinuous()
{
    SoftwareTare tare;
    tare.apply(1500); // cup
    tare.zeroAndAwaitScaleTare();
    tare.apply(0); // first tare command took effect
    TEST_ASSERT_TRUE(tare.scaleTared());
    TEST_ASSERT_EQUAL_INT32(80, tare.apply(80)); // 8 g of espresso

    // A resent tare command arrives late and zeroes the scale again.
    bool handedOver = false;
    TEST_ASSERT_EQUAL_INT32(80, tare.apply(2, &handedOver));
    TEST_ASSERT_TRUE(handedOver);
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(282));
}

void testFlowingEspressoIsNoRepeatedTare()
{
    SoftwareTare tare;
    tare.apply(1500);
    tare.zeroAndAwaitScaleTare();
    tare.apply(0);
    bool handedOver = false;
    for (int32_t raw = 0; raw <= 360; raw += 3) {
        TEST_ASSERT_EQUAL_INT32(raw, tare.apply(raw, &handedOver));
        TEST_ASSERT_FALSE(handedOver);
    }
    // Noise of a few tenths, small drops: still no tare.
    TEST_ASSERT_EQUAL_INT32(355, tare.apply(355, &handedOver));
    TEST_ASSERT_FALSE(handedOver);
}

void testNothingToAwaitWhenAlreadyZero()
{
    SoftwareTare tare;
    tare.apply(0);
    tare.zeroAndAwaitScaleTare();
    TEST_ASSERT_FALSE(tare.awaitingScaleTare());
    TEST_ASSERT_EQUAL_INT32(5, tare.apply(5));
}

} // namespace

void runSoftwareTareTests()
{
    RUN_TEST(testPassesThroughUntilZeroed);
    RUN_TEST(testZeroUsesLastReading);
    RUN_TEST(testClearRestoresRawReadings);
    RUN_TEST(testHandsOverWhenScaleTareShowsUp);
    RUN_TEST(testLateScaleTareKeepsWeightContinuous);
    RUN_TEST(testKeepsOffsetIfScaleNeverTares);
    RUN_TEST(testRepeatedScaleTareKeepsWeightContinuous);
    RUN_TEST(testFlowingEspressoIsNoRepeatedTare);
    RUN_TEST(testNothingToAwaitWhenAlreadyZero);
}
