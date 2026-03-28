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
#include "ColorUtils.h"

static const char * TAG = "ColorUtils";

float lerp(float from, float to, float t)
{
    return from + (to - from) * t;
}

float invLerp(float from, float to, float value, bool clamp)
{
    float prop = (value - from) / (to - from);
    if (clamp)
    {
        prop = prop < 1.0f ? prop : 1.0f;
        prop = prop > 0.0f ? prop : 0.0f;
    }
    return prop;
}

float remap(float origFrom, float origTo, float targetFrom, float targetTo, float value)
{
    float rel = invLerp(origFrom, origTo, value);
    return lerp(targetFrom, targetTo, rel);
}

std::vector<float> multiLerp(std::vector<float> boundaries, float target)
{
    // if (target < boundaries[0])
    // {
    //     ESP_LOGI(TAG, "Under boundary 0: %1.3f", boundaries[0]);
    //     result[0] = 1.0f;
    // }
    // else if (target < boundaries[1])
    // {
    //     ESP_LOGI(TAG, "Under boundary 1: %1.3f", boundaries[1]);
    //     float prop = invLerp(boundaries[0], boundaries[1], target);
    //     result[0]  = prop;
    //     result[1]  = 1.0f - prop;
    // }
    // else if (target < boundaries[2])
    // {
    //     ESP_LOGI(TAG, "Under boundary 1: %1.3f", boundaries[0]);
    //     result[0] = 1.0f;
    // }
    // else
    // {}

    ESP_LOGI(TAG, "target = %1.3f", target);
    bool didBreak = false;
    std::vector<float> result(boundaries.size(), 0);
    if (target < boundaries[0])
    {
        ESP_LOGI(TAG, "Under boundary 0: %1.3f", boundaries[0]);
        result[0] = 1.0f;
    }
    for (int i = 1; i < boundaries.size(); i++)
    {
        if (target < boundaries[i])
        {
            ESP_LOGI(TAG, "found boundary %d, %1.3f", i, boundaries[i]);
            didBreak      = true;
            float prop    = invLerp(boundaries[i - 1], boundaries[i], target);
            result[i - 1] = 1.0f - prop;
            result[i]     = prop;
            break;
        }
        else
        {
            ESP_LOGI(TAG, "not over boundary %d, %1.3f", i, boundaries[i]);
            continue;
        }
    }
    if (!didBreak)
    {
        // int i = boundaries.size() - 1;
        // if (target < boundaries[i])
        // {
        //     ESP_LOGI(TAG, "found boundary %d, %1.3f", i, boundaries[i - 1]);
        //     didBreak      = true;
        //     float prop    = invLerp(boundaries[i - 1], boundaries[i], target);
        //     result[i - 1] = prop;
        //     result[i]     = 1.0f - prop;
        // }
        // else
        // {
        ESP_LOGI(TAG, "Overflow");
        result[boundaries.size() - 1] = 1.0f;
        // }
    }
    return result;
}