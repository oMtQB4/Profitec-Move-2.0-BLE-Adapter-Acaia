#include "status/StatusLed.hpp"

#include <esp_log.h>

namespace status {

namespace {

constexpr const char* TAG = "led";

} // namespace

StatusLed::StatusLed(int gpio, uint8_t brightness)
    : brightness_(brightness)
{
    led_strip_config_t config{}; // zero colour format = GRB, as WS2812 expects
    config.strip_gpio_num = gpio;
    config.max_leds = 1;
    config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmtConfig{};
    rmtConfig.clk_src = RMT_CLK_SRC_DEFAULT;

    const esp_err_t err = led_strip_new_rmt_device(&config, &rmtConfig, &strip_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no status LED on GPIO %d: %s", gpio, esp_err_to_name(err));
        strip_ = nullptr;
    }
}

StatusLed::~StatusLed()
{
    if (strip_ != nullptr) {
        led_strip_del(strip_);
    }
}

void StatusLed::update(State state, uint32_t nowMs)
{
    if (strip_ == nullptr) {
        return;
    }
    const Color color = colorFor(state, nowMs, brightness_);
    if (shown_ == color) {
        return;
    }
    led_strip_set_pixel(strip_, 0, color.r, color.g, color.b);
    led_strip_refresh(strip_);
    shown_ = color;
}

} // namespace status
