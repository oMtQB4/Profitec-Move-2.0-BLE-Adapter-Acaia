#include "unity.h"

void runAcaiaProtocolTests();
void runEurekaPrecisaTests();
void runDriverRegistryTests();
void runAdvertisingDataTests();
void runLedPatternTests();
void runDeviceInfoTests();
void runSoftwareTareTests();
void runDriverFrameworkTests();
void runMoveListDriverTests();
void runMoreDriverTests();

extern "C" void setUp() {}
extern "C" void tearDown() {}

int main()
{
    UNITY_BEGIN();
    runAcaiaProtocolTests();
    runEurekaPrecisaTests();
    runDriverRegistryTests();
    runAdvertisingDataTests();
    runLedPatternTests();
    runDeviceInfoTests();
    runSoftwareTareTests();
    runDriverFrameworkTests();
    runMoveListDriverTests();
    runMoreDriverTests();
    return UNITY_END();
}
