#include "ota/EspFlashTarget.hpp"

#include <esp_log.h>

namespace ota {

namespace {

constexpr const char* TAG = "ota";

} // namespace

EspFlashTarget::EspFlashTarget()
    : slot_(esp_ota_get_next_update_partition(nullptr))
{
    if (slot_ == nullptr) {
        ESP_LOGE(TAG, "no OTA app slot: flash the full image once over USB");
    }
}

uint32_t EspFlashTarget::capacity() const
{
    return slot_ != nullptr ? slot_->size : 0;
}

Status EspFlashTarget::begin(uint32_t imageSize)
{
    if (slot_ == nullptr) {
        return Status::FlashError;
    }
    // Sequential writes erase sector by sector while data arrives, instead
    // of blocking for seconds to erase the whole slot up front.
    const esp_err_t err = esp_ota_begin(slot_, OTA_WITH_SEQUENTIAL_WRITES, &handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        handle_ = 0;
        return Status::FlashError;
    }
    ESP_LOGI(TAG, "receiving %u bytes into %s", static_cast<unsigned>(imageSize), slot_->label);
    return Status::Ok;
}

Status EspFlashTarget::write(std::span<const uint8_t> data)
{
    const esp_err_t err = esp_ota_write(handle_, data.data(), data.size());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
        return err == ESP_ERR_OTA_VALIDATE_FAILED ? Status::InvalidImage : Status::FlashError;
    }
    return Status::Ok;
}

Status EspFlashTarget::finish()
{
    // esp_ota_end releases the handle in every case.
    esp_err_t err = esp_ota_end(handle_);
    handle_ = 0;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return err == ESP_ERR_OTA_VALIDATE_FAILED ? Status::VerifyFailed : Status::FlashError;
    }
    err = esp_ota_set_boot_partition(slot_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return Status::FlashError;
    }
    ESP_LOGI(TAG, "image verified, next boot from %s", slot_->label);
    return Status::Ok;
}

void EspFlashTarget::abort()
{
    if (handle_ != 0) {
        esp_ota_abort(handle_);
        handle_ = 0;
        ESP_LOGW(TAG, "update aborted");
    }
}

} // namespace ota
