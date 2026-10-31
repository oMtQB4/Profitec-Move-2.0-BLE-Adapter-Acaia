#include <cstdio>
#include <string>

#include <NimBLEDevice.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "acaia/AcaiaPeripheral.hpp"
#include "bridge/Bridge.hpp"
#include "scale/DriverRegistry.hpp"
#include "scale/ScaleClient.hpp"
#include "scale/drivers/Drivers.hpp"
#include "status/StatusLed.hpp"

namespace {

constexpr const char* TAG = "main";

#if CONFIG_BRIDGE_ADVERTISE_WITHOUT_SCALE
constexpr bool kAdvertiseWithoutScale = true;
#else
constexpr bool kAdvertiseWithoutScale = false;
#endif

#if CONFIG_BRIDGE_TARE_ON_CLIENT_CONNECT
constexpr bool kTareOnClientConnect = true;
#else
constexpr bool kTareOnClientConnect = false;
#endif

/** "LUNAR-" plus the last three bytes of the BT MAC, like a real Lunar. */
std::string advertisedName()
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    char suffix[7];
    std::snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return std::string(CONFIG_BRIDGE_NAME_PREFIX) + suffix;
}

void initNvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

} // namespace

extern "C" void app_main()
{
    initNvs();

    const std::string name = advertisedName();
    NimBLEDevice::init(name);

    static scale::DriverRegistry registry;
    scale::registerBuiltinDrivers(registry);
    for (const auto& entry : registry.entries()) {
        ESP_LOGI(TAG, "driver: %.*s", static_cast<int>(entry.name.size()), entry.name.data());
    }

    static scale::ScaleClient scaleClient(registry);
    static acaia::AcaiaPeripheral acaiaPeripheral(name);
    static bridge::Bridge bridge(scaleClient, acaiaPeripheral,
                                 {.advertiseWithoutScale = kAdvertiseWithoutScale,
                                  .tareOnClientConnect = kTareOnClientConnect});
    scaleClient.setListener(&bridge);
    acaiaPeripheral.setCommandSink(&bridge);

    acaiaPeripheral.begin();
    scaleClient.begin();
    bridge.begin();

#if CONFIG_BRIDGE_STATUS_LED
    static status::StatusLed statusLed(CONFIG_BRIDGE_STATUS_LED_GPIO, CONFIG_BRIDGE_STATUS_LED_BRIGHTNESS);
#endif

    for (;;) {
        scaleClient.poll();
        bridge.poll();
#if CONFIG_BRIDGE_STATUS_LED
        statusLed.update(status::stateFrom(scaleClient.isConnected(), acaiaPeripheral.isClientConnected()),
                         static_cast<uint32_t>(esp_timer_get_time() / 1000));
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
