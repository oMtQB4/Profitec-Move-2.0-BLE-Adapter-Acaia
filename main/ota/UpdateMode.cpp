#include "ota/UpdateMode.hpp"

#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs.h>

namespace ota {

namespace {

constexpr const char* TAG = "update-mode";
constexpr const char* kNvsNamespace = "ota";
constexpr const char* kPowerCyclesKey = "pwr_cycles";
constexpr uint32_t kUpdateRequest = 0x55504454; // "UPDT"

// Survives esp_restart(), random after power-on (hence the magic value).
RTC_NOINIT_ATTR uint32_t s_bootRequest;

uint8_t loadPowerCycles()
{
    nvs_handle_t handle;
    uint8_t count = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, kPowerCyclesKey, &count);
        nvs_close(handle);
    }
    return count;
}

} // namespace

UpdateMode::UpdateMode(Options options)
    : options_(options)
{
    if (options_.buttonGpio >= 0) {
        gpio_config_t config{};
        config.pin_bit_mask = 1ULL << options_.buttonGpio;
        config.mode = GPIO_MODE_INPUT;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&config);
    }
}

bool UpdateMode::decideAtBoot()
{
    const bool requested = s_bootRequest == kUpdateRequest;
    s_bootRequest = 0;

    const uint8_t stored = loadPowerCycles();
    const PowerCycleCount count =
        PowerCycleCount::onBoot(stored, esp_reset_reason() == ESP_RST_POWERON, options_.powerCycles);
    if (count.stored != stored) {
        storePowerCycles(count.stored);
    }
    powerCyclesPending_ = count.stored != 0;
    if (count.stored != 0) {
        ESP_LOGI(TAG, "power-on %u of %u for update mode", count.stored, options_.powerCycles);
    }
    if (count.enterUpdateMode) {
        ESP_LOGI(TAG, "entering update mode after %u power-ons", options_.powerCycles);
    }
    return requested || count.enterUpdateMode;
}

bool UpdateMode::poll(uint32_t nowMs)
{
    if (powerCyclesPending_ && nowMs >= kStableAfterMs) {
        powerCyclesPending_ = false;
        storePowerCycles(0);
    }
    if (options_.buttonGpio < 0) {
        return false;
    }
    const bool pressed = gpio_get_level(static_cast<gpio_num_t>(options_.buttonGpio)) == 0;
    return button_.update(pressed, nowMs);
}

void UpdateMode::restart(bool updateMode)
{
    ESP_LOGI(TAG, "restarting into %s", updateMode ? "update mode" : "bridge mode");
    s_bootRequest = updateMode ? kUpdateRequest : 0;
    esp_restart();
}

void UpdateMode::storePowerCycles(uint8_t count)
{
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    nvs_set_u8(handle, kPowerCyclesKey, count);
    nvs_commit(handle);
    nvs_close(handle);
}

} // namespace ota
