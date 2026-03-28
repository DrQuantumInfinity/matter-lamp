/*
 *
 *    Copyright (c) 2022-2023 Project CHIP Authors
 *    All rights reserved.
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

#include <stdbool.h>
#include <stdint.h>

#include "AttributeChangeEvent.h"
#include "Button.h"
#include "LEDCluster.h"
#include "freertos/FreeRTOS.h"
#include <platform/CHIPDeviceLayer.h>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>

// Application-defined error codes in the CHIP_ERROR space.
#define APP_ERROR_EVENT_QUEUE_FAILED CHIP_APPLICATION_ERROR(0x01)
#define APP_ERROR_CREATE_TASK_FAILED CHIP_APPLICATION_ERROR(0x02)
#define APP_ERROR_UNHANDLED_EVENT CHIP_APPLICATION_ERROR(0x03)
#define APP_ERROR_CREATE_TIMER_FAILED CHIP_APPLICATION_ERROR(0x04)
#define APP_ERROR_START_TIMER_FAILED CHIP_APPLICATION_ERROR(0x05)
#define APP_ERROR_STOP_TIMER_FAILED CHIP_APPLICATION_ERROR(0x06)

extern LEDCluster AppLEDC;
extern Button AppButton;

enum AppMode
{
    AppMode_Normal,
    AppMode_Rainbow
};
using namespace chip;
using namespace chip::Inet;
using namespace chip::System;
using namespace chip::app::Clusters;

class AppTask
{

public:
    CHIP_ERROR StartAppTask();
    static void AppTaskMain(void * pvParameter);
    void PostEvent(const AttributeChangeEvent * event);

    void ButtonEventHandler(const uint8_t buttonHandle, uint8_t btnAction);

    void UpdateClusterState();
    void OnAttributeChangeCallback(chip::EndpointId endpointId, chip::ClusterId clusterId, chip::AttributeId attributeId,
                                   uint16_t size, uint8_t * value);
    // AppTask GetAppTask(void);

private:
    friend AppTask & GetAppTask(void);
    CHIP_ERROR Init();
    void DispatchEvent(AttributeChangeEvent * event);
    void StartRainbow(void);
    void StopRainbow(void);
    static void SwitchActionEventHandler(AttributeChangeEvent * aEvent);
    static void LightingActionEventHandler(AttributeChangeEvent * aEvent);
    void AttributeChangeHandler(chip::EndpointId endpointId, chip::AttributeId attributeId, chip::ClusterId clusterId,
                                uint8_t * value);
    void OnOffPostAttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value);

    void LevelControlAttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value);

    void ColorControlAttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value);
    TickType_t GetTimeoutTicks(void);
    void HandleTimeout(void);
    uint64_t mNextRainbowUpdateMics;
    AppMode mMode;
    uint8_t mHue;
    static AppTask sAppTask;
};

inline AppTask & GetAppTask(void)
{
    return AppTask::sAppTask;
}
