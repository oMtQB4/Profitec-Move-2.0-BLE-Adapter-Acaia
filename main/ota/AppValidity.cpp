#include "ota/AppValidity.hpp"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace ota {

namespace {

constexpr const char* TAG = "ota";

} // namespace

void AppValidity::begin()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }
    pending_ = true;
    ESP_LOGW(TAG, "new firmware on %s: confirming after %u s", running->label,
             static_cast<unsigned>(kHealthyAfterMs / 1000));

    const esp_timer_create_args_t args{
        .callback = &AppValidity::onDeadline,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "app-validity",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &deadline_) == ESP_OK) {
        esp_timer_start_once(deadline_, static_cast<uint64_t>(kDeadlineMs) * 1000);
    }
}

void AppValidity::poll(uint32_t nowMs)
{
    if (!pending_ || confirmed_.load() || nowMs < kHealthyAfterMs) {
        return;
    }
    confirmed_.store(true);
    if (deadline_ != nullptr) {
        esp_timer_stop(deadline_);
    }
    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "new firmware confirmed");
    } else {
        ESP_LOGE(TAG, "confirming firmware failed: %s", esp_err_to_name(err));
    }
}

void AppValidity::onDeadline(void* arg)
{
    auto* self = static_cast<AppValidity*>(arg);
    if (!self->confirmed_.load()) {
        ESP_LOGE(TAG, "new firmware not confirmed in time: restarting to roll back");
        esp_restart();
    }
}

} // namespace ota
