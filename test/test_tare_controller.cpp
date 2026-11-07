#include "unity.h"

#include "bridge/TareController.hpp"

using namespace bridge;

namespace {

using C = TareController;

/** Cup of 150 g on the scale, machine connects at t = 1000 ms. */
C connectedWithCup()
{
    C tare;
    tare.apply(1500);
    TEST_ASSERT_TRUE(tare.clientConnected(1000) == C::Connect::Zeroed);
    return tare;
}

void testConnectZeroesAndTaresScaleAtOnce()
{
    C tare = connectedWithCup();
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(1500));
    TEST_ASSERT_TRUE(tare.scaleTareDue(1000));
    tare.scaleTareSent(true, 1000);
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000 + C::kConfirmMs - 1));
}

void testResendsWhenScaleTareDoesNotShowUp()
{
    C tare = connectedWithCup();
    tare.scaleTareSent(true, 1000);
    tare.apply(1500);
    TEST_ASSERT_TRUE(tare.scaleTareDue(1000 + C::kConfirmMs));
    tare.scaleTareSent(true, 1000 + C::kConfirmMs);
    tare.scaleTareSent(true, 1000 + 2 * C::kConfirmMs);
    TEST_ASSERT_EQUAL(C::kMaxSends, tare.sends());
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000 + 10 * C::kConfirmMs)); // gives up, software zero stays
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(1860));
}

void testRetriesFailedWriteQuickly()
{
    C tare = connectedWithCup();
    tare.scaleTareSent(false, 1000);
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000 + C::kFailedRetryMs - 1));
    TEST_ASSERT_TRUE(tare.scaleTareDue(1000 + C::kFailedRetryMs));
}

void testStopsOnceScaleHasTared()
{
    C tare = connectedWithCup();
    tare.scaleTareSent(true, 1000);
    bool handedOver = false;
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(0, &handedOver));
    TEST_ASSERT_TRUE(handedOver);
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000 + 5 * C::kConfirmMs));
}

void testReconnectWithinShotKeepsZero()
{
    C tare = connectedWithCup();
    tare.scaleTareSent(true, 1000);
    TEST_ASSERT_EQUAL_INT32(200, tare.apply(1700)); // 20 g, scale not tared

    tare.clientDisconnected(5000); // weak signal drops the machine's link
    tare.tick(7000);
    TEST_ASSERT_TRUE(tare.clientConnected(7000) == C::Connect::KeptReconnect); // same shot: no new zero
    TEST_ASSERT_EQUAL_INT32(250, tare.apply(1750));
}

void testReconnectAfterGraceIsNewShot()
{
    C tare = connectedWithCup();
    tare.scaleTareSent(true, 1000);
    tare.apply(1860);
    tare.clientDisconnected(30000);
    tare.tick(30000 + C::kReconnectGraceMs);
    TEST_ASSERT_EQUAL_INT32(1860, tare.apply(1860)); // raw again between shots
    TEST_ASSERT_TRUE(tare.clientConnected(60000) == C::Connect::Zeroed);
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(1860));
    TEST_ASSERT_TRUE(tare.scaleTareDue(60000));
}

void testEmptyScaleIsTaredOnce()
{
    C tare;
    tare.apply(0);
    TEST_ASSERT_TRUE(tare.clientConnected(1000) == C::Connect::Zeroed);
    TEST_ASSERT_TRUE(tare.scaleTareDue(1000));
    tare.scaleTareSent(true, 1000);
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000 + 5 * C::kConfirmMs));
}

void testKeepsAutoTaredZeroWhenMachineConnects()
{
    C tare(C::Options{.keepTaredZeroOnConnect = true});
    tare.apply(1500);
    tare.tare(0); // cup placed: auto-tare
    tare.scaleTareSent(true, 0);
    tare.apply(0); // scale tared
    // The Move connects 5 s into the shot; 3 g of espresso already in the cup.
    tare.apply(30);
    TEST_ASSERT_TRUE(tare.clientConnected(5000) == C::Connect::KeptTared);
    TEST_ASSERT_EQUAL_INT32(31, tare.apply(31));
}

void testZeroesOnConnectWhenCupWasNotTared()
{
    C tare(C::Options{.keepTaredZeroOnConnect = true});
    tare.apply(1500); // cup put down too slowly for auto-tare
    TEST_ASSERT_TRUE(tare.clientConnected(5000) == C::Connect::Zeroed);
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(1500));
}

void testWeightDropAfterLateWindowIsReal()
{
    C tare;
    tare.apply(0);
    tare.apply(1500);
    tare.tare(0);
    tare.scaleTareSent(true, 0);
    tare.apply(0); // scale tared, cup on it
    tare.apply(1000); // something else put on: 100 g
    tare.tick(C::kLateTareWindowMs);
    TEST_ASSERT_EQUAL_INT32(1000, tare.apply(1000));
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(0)); // taken off again: shows 0, not frozen at 100 g
}

void testSoftwareOnlyTareNeverTouchesTheScale()
{
    C tare(C::Options{.tareScale = false});
    tare.apply(1500);
    TEST_ASSERT_TRUE(tare.clientConnected(1000) == C::Connect::Zeroed);
    TEST_ASSERT_EQUAL_INT32(0, tare.apply(1500));
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000));
    TEST_ASSERT_FALSE(tare.scaleTareDue(1000 + 10 * C::kConfirmMs));
    TEST_ASSERT_EQUAL(0, tare.sends());
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(1860)); // 36 g of espresso
}

void testSoftwareOnlyZeroOutlastsTheShot()
{
    C tare(C::Options{.tareScale = false});
    tare.apply(1500);
    tare.clientConnected(1000);
    tare.apply(1860);
    tare.clientDisconnected(30000);
    tare.tick(30000 + C::kReconnectGraceMs);
    // The scale still reads the cup: dropping the offset would make 186 g appear.
    TEST_ASSERT_EQUAL_INT32(360, tare.apply(1860));
    TEST_ASSERT_EQUAL_INT32(-1500, tare.apply(0)); // cup taken off; auto-tare zeroes this
}

void testSoftwareOnlyCupRemovalIsNotMistakenForScaleTare()
{
    C tare(C::Options{.tareScale = false});
    tare.apply(1500);
    tare.tare(0);
    TEST_ASSERT_EQUAL_INT32(-1500, tare.apply(0)); // no hand-over to a scale tare that never comes
}

void testOptionsChangeAtRuntime()
{
    C tare;
    tare.setOptions(C::Options{.tareScale = false});
    tare.apply(1500);
    tare.tare(0);
    TEST_ASSERT_FALSE(tare.scaleTareDue(0));
    tare.setOptions(C::Options{.tareScale = true});
    tare.tare(100);
    TEST_ASSERT_TRUE(tare.scaleTareDue(100));
}

// --- AutoTare ---

/** Feeds `weightDg` every 100 ms from `fromMs` up to `toMs`; returns how often it fired. */
int feed(AutoTare& autoTare, int32_t weightDg, uint32_t fromMs, uint32_t toMs)
{
    int fired = 0;
    for (uint32_t t = fromMs; t <= toMs; t += 100) {
        fired += autoTare.update(weightDg, t) ? 1 : 0;
    }
    return fired;
}

void testCupPlacedTaresOnceSettled()
{
    AutoTare autoTare;
    TEST_ASSERT_EQUAL(0, feed(autoTare, 0, 0, 2000)); // empty scale: reference
    autoTare.update(800, 2100);                        // cup going down
    TEST_ASSERT_EQUAL(0, feed(autoTare, 1500, 2200, 2900));
    TEST_ASSERT_EQUAL(1, feed(autoTare, 1500, 3000, 5000)); // steady for 0.8 s: once
    TEST_ASSERT_EQUAL(0, feed(autoTare, 0, 5100, 8000));    // scale reads 0 after the tare
}

void testCupRemovedTaresToo()
{
    AutoTare autoTare;
    feed(autoTare, 0, 0, 2000);
    TEST_ASSERT_EQUAL(1, feed(autoTare, -1500, 2100, 4000));
}

void testSmallChangesAndNoiseDoNotTare()
{
    AutoTare autoTare;
    feed(autoTare, 0, 0, 2000);
    TEST_ASSERT_EQUAL(0, feed(autoTare, 150, 2100, 5000)); // 15 g: below a cup
    for (uint32_t t = 5100; t < 9000; t += 100) {
        TEST_ASSERT_FALSE(autoTare.update(150 + static_cast<int32_t>(t / 100 % 2) * 4, t)); // ±0.4 g
    }
}

void testPouringDoesNotTare()
{
    AutoTare autoTare;
    feed(autoTare, 0, 0, 2000);
    // Bloom: 50 g of water over 5 s, then a pause.
    for (uint32_t t = 2100, w = 0; t <= 7000; t += 100, w += 10) {
        TEST_ASSERT_FALSE(autoTare.update(static_cast<int32_t>(w), t));
    }
    TEST_ASSERT_EQUAL(0, feed(autoTare, 500, 7100, 12000));
}

void testShotStartWithoutMachineDoesNotTare()
{
    AutoTare autoTare;
    feed(autoTare, 0, 0, 2000); // cup already tared
    // First espresso drips before the Move connects.
    for (uint32_t t = 2100, w = 0; t <= 7000; t += 100, w += 2) {
        TEST_ASSERT_FALSE(autoTare.update(static_cast<int32_t>(w), t));
    }
}

} // namespace

void runTareControllerTests()
{
    RUN_TEST(testConnectZeroesAndTaresScaleAtOnce);
    RUN_TEST(testResendsWhenScaleTareDoesNotShowUp);
    RUN_TEST(testRetriesFailedWriteQuickly);
    RUN_TEST(testStopsOnceScaleHasTared);
    RUN_TEST(testReconnectWithinShotKeepsZero);
    RUN_TEST(testReconnectAfterGraceIsNewShot);
    RUN_TEST(testEmptyScaleIsTaredOnce);
    RUN_TEST(testKeepsAutoTaredZeroWhenMachineConnects);
    RUN_TEST(testZeroesOnConnectWhenCupWasNotTared);
    RUN_TEST(testWeightDropAfterLateWindowIsReal);
    RUN_TEST(testSoftwareOnlyTareNeverTouchesTheScale);
    RUN_TEST(testSoftwareOnlyZeroOutlastsTheShot);
    RUN_TEST(testSoftwareOnlyCupRemovalIsNotMistakenForScaleTare);
    RUN_TEST(testOptionsChangeAtRuntime);
    RUN_TEST(testCupPlacedTaresOnceSettled);
    RUN_TEST(testCupRemovedTaresToo);
    RUN_TEST(testSmallChangesAndNoiseDoNotTare);
    RUN_TEST(testPouringDoesNotTare);
    RUN_TEST(testShotStartWithoutMachineDoesNotTare);
}
