/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "LEDWidget.h"
#include "ColorFormat.h"
#include "led_strip.h"
#include "esp_timer.h"

static const char * TAG = "LEDWidget";

void LEDWidget::InitColor(gpio_num_t pin)
{
    Init(true, pin, (ledc_channel_t)0);
}
void LEDWidget::InitMono(gpio_num_t pin, ledc_channel_t ledcChannel)
{
    Init(false, pin, ledcChannel);
}

void LEDWidget::Init(bool color, gpio_num_t pin, ledc_channel_t ledcChannel)
{
    mState      = false;
    mBrightness = UINT8_MAX;
    mColor = color;
    mGPIONum = pin;
    mLedcChannel = ledcChannel;
    mStrip = nullptr;

    if (color)
    {
        led_strip_config_t strip_config = {};
        strip_config.strip_gpio_num = mGPIONum;
        strip_config.max_leds       = 1;

        led_strip_rmt_config_t rmt_config = {};
        rmt_config.resolution_hz = 10 * 1000 * 1000; // 10 MHz

        ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &mStrip));

        mHue        = 90;
        mSaturation = 255;
        mBrightness = 10;
    }
    else
    {
        ledc_timer_config_t ledc_timer = {
            .speed_mode      = LEDC_LOW_SPEED_MODE, // timer mode
            .duty_resolution = LEDC_TIMER_8_BIT,    // resolution of PWM duty
            .timer_num       = LEDC_TIMER_1,        // timer index
            .freq_hz         = 5000,                // frequency of PWM signal
            .clk_cfg         = LEDC_AUTO_CLK,       // Auto select the source clock
        };
        ledc_timer_config(&ledc_timer);
        ledc_channel_config_t ledc_channel = {
            .gpio_num   = mGPIONum,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = mLedcChannel,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LEDC_TIMER_1,
            .duty       = 0,
            .hpoint     = 0,
        };
        ledc_channel_config(&ledc_channel);
    }
}

void LEDWidget::Set(bool state)
{
    ESP_LOGI(TAG, "Setting state to %d", state ? 1 : 0);
    if (state == mState)
    {
        return;
    }
    else if (state == true)
    {
        mTurnOnTimeMs = esp_timer_get_time();
        ESP_LOGI(TAG, "Turn on at %llu", mTurnOnTimeMs);
    }

    mState = state;

    DoSet();
}

void LEDWidget::Toggle()
{
    ESP_LOGI(TAG, "Toggling state to %d", !mState);
    mState = !mState;

    DoSet();
}

void LEDWidget::SetBrightness(uint8_t brightness)
{
    ESP_LOGI(TAG, "Setting brightness to %d", brightness);
    if (brightness == mBrightness)
        return;

    if (brightness != 1 ||
        esp_timer_get_time() - mTurnOnTimeMs > 500*1000)
    {
        mBrightness = brightness;
    }
    else
    {
        ESP_LOGI(TAG, "Ignoring brightness to %d. timeDelta = %llu", brightness, esp_timer_get_time() - mTurnOnTimeMs);
        return;
    }

    DoSet();
}

uint8_t LEDWidget::GetLevel()
{
    return this->mBrightness;
}

bool LEDWidget::IsTurnedOn()
{
    return this->mState;
}

void LEDWidget::SetColor(int16_t Hue, int16_t Saturation)
{
    if (!mColor)
        return;
    if (Hue < 0)
    {
        Hue = mHue;
    }
    if (Saturation < 0)
    {
        Saturation = mSaturation;
    }
    if (Hue == mHue && Saturation == mSaturation)
        return;

    mHue        = Hue;
    mSaturation = Saturation;

    DoSet();
}

void LEDWidget::DoSet(void)
{
    uint8_t brightness = mState ? mBrightness : 0;

    if (mColor)
    {
        if (mStrip)
        {
            HsvColor_t hsv = { static_cast<uint8_t>((uint16_t)mHue*360/255), (uint8_t)(mSaturation*100/255), brightness };
            RgbColor_t rgb = HsvToRgb(hsv);
            ESP_LOGI(TAG, "DoSet to GPIO number %d, %d:%d:%d", mGPIONum, rgb.r, rgb.g, rgb.b);
            led_strip_set_pixel(mStrip, 0, rgb.r, rgb.g, rgb.b);
            led_strip_refresh(mStrip);
        }
    }
    else
    {
        ESP_LOGI(TAG, "DoSet to GPIO number %d", mGPIONum);
        if (mGPIONum < GPIO_NUM_MAX)
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, mLedcChannel, brightness);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, mLedcChannel);
        }
    }
}
