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

#include "AppTask.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "DeviceWithDisplay.h"

#include "esp_timer.h"
#include <app-common/zap-generated/attributes/Accessors.h>

#define APP_TASK_NAME "APP"
#define APP_EVENT_QUEUE_SIZE 10
#define APP_TASK_STACK_SIZE (3072)
#define BUTTON_PRESSED 1
#define APP_LIGHT_SWITCH 1

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

static const char * TAG = "app-task";

LEDCluster AppLEDC;

namespace {
constexpr EndpointId kLightEndpointId = 1;
QueueHandle_t sAppEventQueue;
TaskHandle_t sAppTaskHandle;
} // namespace
using namespace chip;
using namespace chip::Inet;
using namespace chip::System;
using namespace chip::app::Clusters;

#define MIN(a, b) (((a)<(b)) ? (a) : (b))

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::StartAppTask()
{
    sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AttributeChangeEvent));
    if (sAppEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate app event queue");
        return APP_ERROR_EVENT_QUEUE_FAILED;
    }

    // Start App task.
    BaseType_t xReturned;
    xReturned = xTaskCreate(AppTaskMain, APP_TASK_NAME, APP_TASK_STACK_SIZE, NULL, 1, &sAppTaskHandle);
    return (xReturned == pdPASS) ? CHIP_NO_ERROR : APP_ERROR_CREATE_TASK_FAILED;
}

void AppTask::OnAttributeChangeCallback(EndpointId endpointId, ClusterId clusterId, AttributeId attributeId, uint16_t size,
                                        uint8_t * value)
{
    AttributeChangeEvent event;
    event.endpointId  = endpointId;
    event.attributeId = attributeId;
    event.clusterId   = clusterId;
    memcpy(event.value, value, MIN(sizeof(event.value), size));
    sAppTask.PostEvent(&event);
}

void AppTask::AttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, ClusterId clusterId, uint8_t * value)
{
    switch (clusterId)
    {
    case OnOff::Id:
        OnOffPostAttributeChangeHandler(endpointId, attributeId, value);
        break;

    case LevelControl::Id:
        LevelControlAttributeChangeHandler(endpointId, attributeId, value);
        break;

    case ColorControl::Id:
        ColorControlAttributeChangeHandler(endpointId, attributeId, value);
        break;

    default:
        ESP_LOGI(TAG, "Unhandled cluster ID: %" PRIu32, clusterId);
        break;
    }
}

void AppTask::OnOffPostAttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value)
{
    VerifyOrExit(attributeId == OnOff::Attributes::OnOff::Id,
                 ESP_LOGI(TAG, "Unhandled Attribute ID: '0x%" PRIx32 "'", attributeId));
    VerifyOrExit(endpointId == 1, ESP_LOGE(TAG, "Unexpected EndPoint ID: `0x%02x'", endpointId));

    AppLEDC.Set(*value);

exit:
    return;
}

void AppTask::LevelControlAttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value)
{
    VerifyOrExit(attributeId == LevelControl::Attributes::CurrentLevel::Id,
                 ESP_LOGI(TAG, "Unhandled Attribute ID: '0x%" PRIx32 "'", attributeId));
    VerifyOrExit(endpointId == 1, ESP_LOGE(TAG, "Unexpected EndPoint ID: `0x%02x'", endpointId));

    AppLEDC.SetBrightness(*value);

exit:
    return;
}

void AppTask::ColorControlAttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value)
{
    using namespace ColorControl::Attributes;
    uint8_t hue, saturation;

    VerifyOrExit(attributeId == CurrentHue::Id || attributeId == CurrentSaturation::Id || attributeId == ColorTemperatureMireds::Id,
                 ESP_LOGI(TAG, "Unhandled AttributeId ID: '0x%" PRIx32 "'", attributeId));
    VerifyOrExit(endpointId == 1, ESP_LOGE(TAG, "Unexpected EndPoint ID: `0x%02x'", endpointId));

    if (attributeId == ColorTemperatureMireds::Id)
    {
        StopRainbow();
        uint16_t temp;
        memcpy(&temp, value, sizeof(temp));
        AppLEDC.SetColorTemp(temp);
    }
    else if (attributeId == CurrentHue::Id)
    {
        hue = *value;
        saturation = -1;
        if (hue == 21)
        {
            StartRainbow();
            saturation = 255;
        }
        else
        {
            StopRainbow();
        }
        //CurrentSaturation::Get(endpointId, &saturation);
        AppLEDC.SetColor(hue, saturation);
    }
    else
    {
        saturation = *value;
        //CurrentHue::Get(endpointId, &hue);
        AppLEDC.SetColor(-1, saturation);
    }

exit:
    return;
}

CHIP_ERROR AppTask::Init()
{
    CHIP_ERROR err   = CHIP_NO_ERROR;
    uint8_t gpios[3] = { 17, 18, 19 };
    float temps[3]   = { 2600.0f, 3000.0f, 5000.0f };
    AppLEDC.Init(gpios, temps, 3, 5);
    mMode = AppMode_Normal;
    return err;
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AttributeChangeEvent event;
    CHIP_ERROR err = sAppTask.Init();
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGI(TAG, "AppTask.Init() failed due to %" CHIP_ERROR_FORMAT, err.Format());
        return;
    }

    ESP_LOGI(TAG, "App Task started");

    while (true)
    {
        BaseType_t eventReceived = xQueueReceive(sAppEventQueue, &event, sAppTask.GetTimeoutTicks());
        if (eventReceived == pdTRUE)
        {
            sAppTask.DispatchEvent(&event);
        }
        else
        {
            sAppTask.HandleTimeout();
        }
    }
}

TickType_t AppTask::GetTimeoutTicks(void)
{
    if (mNextRainbowUpdateMics != 0)
    {
        int64_t micsRemaining = mNextRainbowUpdateMics - esp_timer_get_time();
        if (micsRemaining < 0)
        {
            micsRemaining = 0;
        }
        return pdMS_TO_TICKS(micsRemaining / 1000);
    }
    else
    {
        return pdMS_TO_TICKS(1000);
    }
}

void AppTask::HandleTimeout(void)
{
    ESP_LOGI(TAG, "Handle Timeout");
    if (mMode == AppMode_Rainbow)
    {
        if ((int64_t)(mNextRainbowUpdateMics - esp_timer_get_time()) <= 0)
        {
            mNextRainbowUpdateMics = esp_timer_get_time() + 2000 * 1000;
            ESP_LOGI(TAG, "Next Rainbow at %llu", mNextRainbowUpdateMics);
            mHue++ && 0xFF;
            AppLEDC.SetColor(mHue, 255);
        }
    }
    else
    {
        mNextRainbowUpdateMics = 0;
    }
}

void AppTask::StartRainbow(void)
{
    mMode                  = AppMode_Rainbow;
    mNextRainbowUpdateMics = esp_timer_get_time();
    ESP_LOGI(TAG, "Start Rainbow at %llu", mNextRainbowUpdateMics);
    mHue                   = 107;
    HandleTimeout();
}

void AppTask::StopRainbow(void)
{
    ESP_LOGI(TAG, "Stop Rainbow");
    mMode = AppMode_Normal;
    mNextRainbowUpdateMics = 0;
}

void AppTask::PostEvent(const AttributeChangeEvent * aEvent)
{
    if (sAppEventQueue != NULL)
    {
        BaseType_t status;
        if (xPortInIsrContext())
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            status                         = xQueueSendFromISR(sAppEventQueue, aEvent, &higherPrioTaskWoken);
        }
        else
        {
            status = xQueueSend(sAppEventQueue, aEvent, 1);
        }
        if (!status)
            ESP_LOGE(TAG, "Failed to post event to app task event queue");
    }
    else
    {
        ESP_LOGE(TAG, "Event Queue is NULL should never happen");
    }
}

void AppTask::DispatchEvent(AttributeChangeEvent * aEvent)
{
    AttributeChangeHandler(aEvent->endpointId, aEvent->attributeId, aEvent->clusterId, aEvent->value);
}

void AppTask::UpdateClusterState()
{
    ESP_LOGI(TAG, "Writing to OnOff cluster");
    // write the new on/off value
    EmberAfStatus status = Clusters::OnOff::Attributes::OnOff::Set(kLightEndpointId, AppLEDC.IsTurnedOn());

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        ESP_LOGE(TAG, "Updating on/off cluster failed: %x", status);
    }

    ESP_LOGI(TAG, "Writing to Current Level cluster");
    status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kLightEndpointId, AppLEDC.GetLevel());

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        ESP_LOGE(TAG, "Updating level cluster failed: %x", status);
    }
}
