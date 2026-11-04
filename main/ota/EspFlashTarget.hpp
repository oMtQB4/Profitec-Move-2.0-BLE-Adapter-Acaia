#pragma once

#include <esp_ota_ops.h>

#include "ota/OtaSession.hpp"

namespace ota {

/** Writes the image into the inactive app slot with esp_ota_*. */
class EspFlashTarget : public FlashTarget {
public:
    EspFlashTarget();

    uint32_t capacity() const override;
    Status begin(uint32_t imageSize) override;
    Status write(std::span<const uint8_t> data) override;
    Status finish() override;
    void abort() override;

private:
    const esp_partition_t* slot_;
    esp_ota_handle_t handle_ = 0;
};

} // namespace ota
