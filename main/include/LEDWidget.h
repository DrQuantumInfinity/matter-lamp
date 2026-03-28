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

#pragma once

#include "driver/gpio.h"
#include "esp_log.h"

#include "driver/rmt.h"
#include "led_strip.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"

class LEDWidget
{
public:
    void InitColor(gpio_num_t pin, rmt_channel_t rmtChannel);
    void InitMono(gpio_num_t pin, ledc_channel_t ledcChannel);
    void Set(bool state);
    void Toggle(void);

    void SetBrightness(uint8_t brightness);
    void UpdateState();
    void SetColor(int16_t Hue, int16_t Saturation);
    uint8_t GetLevel(void);
    bool IsTurnedOn(void);

private:
    void Init(bool color, gpio_num_t pin, rmt_channel_t rmtChannel, ledc_channel_t ledcChannel);
    bool mState;
    uint8_t mBrightness;
    uint64_t mTurnOnTimeMs;
    bool mColor;


    uint8_t mHue;
    uint8_t mSaturation;
// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
//     led_strip_handle_t mStrip;
// #else
    led_strip_t * mStrip;
// #endif
    gpio_num_t mGPIONum;
    ledc_channel_t mLedcChannel;

    void DoSet(void);
};
