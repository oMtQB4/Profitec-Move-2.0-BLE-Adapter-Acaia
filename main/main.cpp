#include <cstdio>
#include <string>

#include <NimBLEDevice.h>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "acaia/AcaiaPeripheral.hpp"
#include "bridge/Bridge.hpp"
#include "ota/AppValidity.hpp"
#include "ota/EspFlashTarget.hpp"
#include "ota/OtaService.hpp"
#include "ota/UpdateMode.hpp"
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

#if CONFIG_BRIDGE_AUTO_TARE
constexpr bool kAutoTare = true;
#else
constexpr bool kAutoTare = false;
#endif

#if CONFIG_BRIDGE_UPDATE_BUTTON
constexpr int kUpdateButtonGpio = CONFIG_BRIDGE_UPDATE_BUTTON_GPIO;
#else
constexpr int kUpdateButtonGpio = -1;
#endif

/** Update mode ends without a client for this long. */
constexpr uint32_t kUpdateModeIdleMs = CONFIG_BRIDGE_UPDATE_MODE_TIMEOUT_MIN * 60 * 1000;
/** Time for the client to receive the final response before the restart. */
constexpr uint32_t kRestartDelayMs = 1500;

uint32_t nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

/** Last three bytes of the BT MAC, like a real Lunar ("LUNAR-" + suffix). */
std::string macSuffix()
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    char suffix[7];
    std::snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return suffix;
}

void initNvs()
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        // Full, newer format, or (after the switch to the OTA flash layout)
        // left-over bytes of the old app: start with an empty NVS.
        ESP_LOGW(TAG, "nvs_flash_init: %s, erasing NVS", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

/** Starts the GATT server once every service is added. */
void startGattServer(const std::string& name)
{
    NimBLEDevice::getServer()->start();
    // start() re-initializes the GAP service, which resets the device name to
    // CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME ("nimble").
    NimBLEDevice::setDeviceName(name);
}

class Led {
public:
    void show(status::State state)
    {
#if CONFIG_BRIDGE_STATUS_LED
        led_.update(state, nowMs());
#else
        (void)state;
#endif
    }

private:
#if CONFIG_BRIDGE_STATUS_LED
    status::StatusLed led_{CONFIG_BRIDGE_STATUS_LED_GPIO, CONFIG_BRIDGE_STATUS_LED_BRIGHTNESS};
#endif
};

[[noreturn]] void runBridge(acaia::AcaiaPeripheral& acaiaPeripheral, ota::OtaService& otaService,
                            ota::UpdateMode& updateMode, ota::AppValidity& validity)
{
    static scale::DriverRegistry registry;
    scale::registerBuiltinDrivers(registry);
    for (const auto& entry : registry.entries()) {
        ESP_LOGI(TAG, "driver: %.*s", static_cast<int>(entry.name.size()), entry.name.data());
    }

    static scale::ScaleClient scaleClient(registry);
    static bridge::Bridge bridge(scaleClient, acaiaPeripheral,
                                 {.advertiseWithoutScale = kAdvertiseWithoutScale,
                                  .tareOnClientConnect = kTareOnClientConnect,
                                  .autoTare = kAutoTare});
    scaleClient.setListener(&bridge);
    acaiaPeripheral.setCommandSink(&bridge);

    scaleClient.begin();
    bridge.begin();

    static Led led;
    for (;;) {
        const uint32_t now = nowMs();
        scaleClient.poll();
        bridge.poll();
        otaService.poll(now); // answers "not in update mode"
        validity.poll(now);
        if (updateMode.poll(now)) {
            ota::UpdateMode::restart(true);
        }
        led.show(status::stateFrom(scaleClient.isConnected(), acaiaPeripheral.isClientConnected()));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

[[noreturn]] void runUpdateMode(ota::OtaService& otaService, ota::UpdateMode& updateMode,
                                ota::AppValidity& validity)
{
    otaService.enterUpdateMode("AcaiaBridge-" + macSuffix());

    static Led led;
    uint32_t doneAtMs = 0;
    for (;;) {
        // Blocks on the write queue: low latency for the next packet.
        otaService.poll(nowMs(), 10);
        const uint32_t now = nowMs();
        validity.poll(now);

        if (otaService.isDone()) {
            if (doneAtMs == 0) {
                doneAtMs = now;
            } else if (now - doneAtMs >= kRestartDelayMs) {
                ota::UpdateMode::restart(false);
            }
        }
        if (updateMode.poll(now)) {
            ota::UpdateMode::restart(false);
        }
        const bool idle = !otaService.isTransferring() && !otaService.isDone();
        if (idle && now - otaService.lastActivityMs() >= kUpdateModeIdleMs) {
            ESP_LOGI(TAG, "no update within %u min", static_cast<unsigned>(CONFIG_BRIDGE_UPDATE_MODE_TIMEOUT_MIN));
            ota::UpdateMode::restart(false);
        }
        led.show(otaService.isTransferring() || otaService.isDone() ? status::State::Updating
                                                                    : status::State::UpdateMode);
    }
}

} // namespace

extern "C" void app_main()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "acaia-bridge %s (%s)", esp_app_get_description()->version, running->label);
    initNvs();

    static ota::AppValidity validity;
    validity.begin();

    static ota::UpdateMode updateMode({.buttonGpio = kUpdateButtonGpio,
                                       .powerCycles = CONFIG_BRIDGE_UPDATE_POWER_CYCLES});
    const bool startInUpdateMode = updateMode.decideAtBoot();

    const std::string name = std::string(CONFIG_BRIDGE_NAME_PREFIX) + macSuffix();
    NimBLEDevice::init(name);

    // Same GATT table in both modes; the OTA service comes last so the Acaia
    // and Device Information handles the machine knows stay the same.
    static acaia::AcaiaPeripheral acaiaPeripheral(name);
    static ota::EspFlashTarget flashTarget;
    static ota::OtaService otaService(flashTarget, esp_app_get_description()->version);
    acaiaPeripheral.begin();
    otaService.registerService(*NimBLEDevice::getServer());
    startGattServer(name);

    if (startInUpdateMode) {
        runUpdateMode(otaService, updateMode, validity);
    }
    runBridge(acaiaPeripheral, otaService, updateMode, validity);
}
