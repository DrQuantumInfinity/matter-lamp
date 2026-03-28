/*
 *
 *    Copyright (c) 2021-2023 Project CHIP Authors
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

#if CONFIG_LED_TYPE_RMT
#include "driver/rmt.h"
#include "led_strip.h"
#else
#include "driver/ledc.h"
#include "hal/ledc_types.h"
#endif
#include <vector>
#include "LEDWidget.h"

enum Mode { Mode_Mono, Mode_Color };

class LEDCluster
{
public:
    void Init(uint8_t * gpios, float * temps, uint8_t size, uint8_t colorGpio);
    void Set(bool state);
    void Toggle(void);

    void SetBrightness(uint8_t brightness);
    void UpdateState();
    void SetColor(int16_t Hue, int16_t Saturation);
    void SetColorTemp(uint16_t temp);
    uint8_t GetLevel(void);
    bool IsTurnedOn(void);


private:
    bool mState;
    Mode mMode;
    uint8_t mBrightness;

    uint8_t mHue;
    uint8_t mSaturation;
    // led_strip_t * mStrip;
    std::vector<LEDWidget> mLeds;
    LEDWidget mColorLed;
    std::vector<float> mLedProp;
    std::vector<float> mTemps;

    void DoSet(void);
};
