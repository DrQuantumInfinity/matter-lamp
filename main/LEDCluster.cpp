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

#include "LEDCluster.h"
#include "ColorFormat.h"
#include "ColorUtils.h"
#include "LEDWidget.h"
#include "led_strip.h"

static const char * TAG = "LEDCluster";

void LEDCluster::Init(uint8_t * gpios, float * temps, uint8_t size, uint8_t colorGpio)
{
    for (int ledIndex = 0; ledIndex < size; ledIndex++)
    {
        LEDWidget newLed;
        newLed.InitMono((gpio_num_t) gpios[ledIndex], (ledc_channel_t) ledIndex);
        mLeds.push_back(newLed);
        mLedProp.push_back(0.0f);
        mTemps.push_back(temps[ledIndex]);
        ESP_LOGI(TAG, "Created LED:");
        ESP_LOGI(TAG, "GPIO %d:", gpios[ledIndex]);
        ESP_LOGI(TAG, "Channel %d", ledIndex);
        ESP_LOGI(TAG, "Temp: %1.2f", temps[ledIndex]);
    }
    ESP_LOGI(TAG, "Done creating");
    mColorLed.InitColor((gpio_num_t) colorGpio, (rmt_channel_t) 0);
    mMode = Mode_Mono;
    // for (int ledIndex = 0; ledIndex < mLeds.size(); ledIndex++)
    // {
    //     ESP_LOGI(TAG, "Checking LED:");
    //     ESP_LOGI(TAG, "GPIO %d:", mLeds[ledIndex].mGPIONum);
    //     ESP_LOGI(TAG, "Channel %d", mLeds[ledIndex].mChannel);
    //     ESP_LOGI(TAG, "Temp: %1.2f", mTemps[ledIndex]);
    //     ESP_LOGI(TAG, "Prop: %1.2f", mLedProp[ledIndex]);
    // }
}

void LEDCluster::Set(bool state)
{
    ESP_LOGI(TAG, "Setting state to %d", state ? 1 : 0);
    if (state == mState)
        return;
    mState = state;

    DoSet();
}

void LEDCluster::Toggle()
{
    ESP_LOGI(TAG, "Toggling state to %d", !mState);
    mState = !mState;

    DoSet();
}

void LEDCluster::SetBrightness(uint8_t brightness)
{
    ESP_LOGI(TAG, "Setting brightness to %d", brightness);
    if (brightness == mBrightness)
        return;

    mBrightness = brightness;

    DoSet();
}

uint8_t LEDCluster::GetLevel()
{
    return this->mBrightness;
}

bool LEDCluster::IsTurnedOn()
{
    return this->mState;
}

void LEDCluster::SetColorTemp(uint16_t temp)
{
    ESP_LOGI(TAG, "Got Temp %d", temp);
    // float tempf = remap(500, 167, 2600, 5000, temp);
    float tempf = 1000000.0 / (double) temp;
    ESP_LOGI(TAG, "Calced Temp = %1.3f", tempf);
    mLedProp = multiLerp(mTemps, tempf);

    ESP_LOGI(TAG, "Calced prop1 = %1.3f", mLedProp[0]);
    ESP_LOGI(TAG, "Calced prop2 = %1.3f", mLedProp[1]);
    ESP_LOGI(TAG, "Calced prop3 = %1.3f", mLedProp[2]);
    mMode = Mode_Mono;
    DoSet();
}

void LEDCluster::SetColor(int16_t Hue, int16_t Saturation)
{
    if (Hue == mHue && Saturation == mSaturation)
        return;
    if (Hue < 0)
    {
        Hue = mHue;
    }
    if (Saturation < 0)
    {
        Saturation = mSaturation;
    }
    mHue        = Hue;
    mSaturation = Saturation;
    mMode = Mode_Color;
    DoSet();
}

void LEDCluster::DoSet(void)
{
    for (int ledIndex = 0; ledIndex < mLeds.size(); ledIndex++)
    {
        mLeds[ledIndex].Set(mState && (mMode == Mode_Mono));
        mLeds[ledIndex].SetBrightness((uint8_t) (mLedProp[ledIndex] * mBrightness));
    }
    mColorLed.SetColor(mHue, mSaturation);
    mColorLed.Set(mState && (mMode == Mode_Color));
    mColorLed.SetBrightness(mBrightness);
}