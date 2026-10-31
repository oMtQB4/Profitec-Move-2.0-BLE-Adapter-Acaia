#include "unity.h"

void runAcaiaProtocolTests();
void runEurekaPrecisaTests();
void runDriverRegistryTests();
void runAdvertisingDataTests();
void runLedPatternTests();
void runDeviceInfoTests();
void runSoftwareTareTests();

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
    return UNITY_END();
}
