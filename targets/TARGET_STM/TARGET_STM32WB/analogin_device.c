/* mbed Microcontroller Library
 * Copyright (c) 2019 STMicroelectronics
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if DEVICE_ANALOGIN

#include "analogin_api.h"
#include "mbed_assert.h"
#include "mbed_error.h"
#include "mbed_debug.h"
#include "mbed_wait_api.h"
#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"

void analogin_init(analogin_t *obj, PinName pin)
{
    uint32_t function = (uint32_t)NC;

    // ADC Internal Channels "pins"  (Temperature, Vref, Vbat, ...)
    //   are described in PinNames.h and PeripheralPins.c
    //   Pin value must be between 0xF0 and 0xFF
    if ((pin < 0xF0) || (pin >= 0x100)) {
        // Normal channels
        // Get the peripheral name from the pin and assign it to the object
        obj->handle.Instance = (ADC_TypeDef *)pinmap_peripheral(pin, PinMap_ADC);
        // Get the functions (adc channel) from the pin and assign it to the object
        function = pinmap_function(pin, PinMap_ADC);
        // Configure GPIO
        pinmap_pinout(pin, PinMap_ADC);
    } else {
        // Internal channels
        obj->handle.Instance = (ADC_TypeDef *)pinmap_peripheral(pin, PinMap_ADC_Internal);
        function = pinmap_function(pin, PinMap_ADC_Internal);
        // No GPIO configuration for internal channels
    }
    MBED_ASSERT(obj->handle.Instance != (ADC_TypeDef *)NC);
    MBED_ASSERT(function != (uint32_t)NC);

    obj->channel = STM_PIN_CHANNEL(function);

    // Save pin number for the read function
    obj->pin = pin;

    // Configure ADC object structures
    obj->handle.State = HAL_ADC_STATE_RESET;
    obj->handle.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV2;          // Asynchronous clock mode, input ADC clock
    obj->handle.Init.Resolution            = ADC_RESOLUTION_12B;
    obj->handle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    obj->handle.Init.ScanConvMode          = ADC_SCAN_DISABLE;              // Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1)
    obj->handle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;           // On STM32L1xx ADC, overrun detection is enabled only if EOC selection is set to each conversion (or transfer by DMA enabled, this is not the case in this example).
    obj->handle.Init.LowPowerAutoWait      = DISABLE;
    obj->handle.Init.ContinuousConvMode    = DISABLE;                       // Continuous mode disabled to have only 1 conversion at each conversion trig
    obj->handle.Init.NbrOfConversion       = 1;                             // Parameter discarded because sequencer is disabled
    obj->handle.Init.DiscontinuousConvMode = DISABLE;                       // Parameter discarded because sequencer is disabled
    obj->handle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;            // Software start to trig the 1st conversion manually, without external event
    obj->handle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    obj->handle.Init.DMAContinuousRequests = DISABLE;
    obj->handle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;      // DR register is overwritten with the last conversion result in case of overrun
#if defined (ADC_SUPPORT_2_5_MSPS)
    obj->handle.Init.LowPowerAutoPowerOff  = DISABLE;
    obj->handle.Init.SamplingTimeCommon1   = ADC_SAMPLETIME_79CYCLES_5;
    obj->handle.Init.SamplingTimeCommon2   = ADC_SAMPLETIME_160CYCLES_5;
    obj->handle.Init.TriggerFrequencyMode  = ADC_TRIGGER_FREQ_HIGH;
#else
    obj->handle.Init.NbrOfDiscConversion   = 1;                             // Parameter discarded because sequencer is disabled
    obj->handle.Init.OversamplingMode      = DISABLE;                       // No oversampling
#endif

    // Enable ADC core clock
    __HAL_RCC_ADC_CLK_ENABLE();

    // Enable ADC conversion clock.
    // Only necessary with asynchronous clock source
    __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_SYSCLK);

    if (HAL_ADC_Init(&obj->handle) != HAL_OK) {
        error("ADC initialization failed\r\n");
    }

    // ADC calibration is done only once
    if (!HAL_ADCEx_Calibration_GetValue(&obj->handle, ADC_SINGLE_ENDED)) {
        HAL_ADCEx_Calibration_Start(&obj->handle, ADC_SINGLE_ENDED);
    }
}

uint16_t adc_read(analogin_t *obj)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    // Configure ADC channel
    sConfig.Rank         = ADC_REGULAR_RANK_1;
#if !defined (ADC_SUPPORT_2_5_MSPS)
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
#else
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
#endif

    switch (obj->channel) {
        case 0:
            sConfig.Channel = ADC_CHANNEL_VREFINT;
#if !defined (ADC_SUPPORT_2_5_MSPS)
            sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5; // Minimum ADC sampling time when reading the internal reference voltage is 4us
#else
            sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
#endif
            break;
        case 1:
            sConfig.Channel = ADC_CHANNEL_1;
            break;
        case 2:
            sConfig.Channel = ADC_CHANNEL_2;
            break;
        case 3:
            sConfig.Channel = ADC_CHANNEL_3;
            break;
        case 4:
            sConfig.Channel = ADC_CHANNEL_4;
            break;
        case 5:
            sConfig.Channel = ADC_CHANNEL_5;
            break;
        case 6:
            sConfig.Channel = ADC_CHANNEL_6;
            break;
        case 7:
            sConfig.Channel = ADC_CHANNEL_7;
            break;
        case 8:
            sConfig.Channel = ADC_CHANNEL_8;
            break;
        case 9:
            sConfig.Channel = ADC_CHANNEL_9;
            break;
        case 10:
            sConfig.Channel = ADC_CHANNEL_10;
            break;
        case 11:
            sConfig.Channel = ADC_CHANNEL_11;
            break;
        case 12:
            sConfig.Channel = ADC_CHANNEL_12;
            break;
        case 13:
            sConfig.Channel = ADC_CHANNEL_13;
            break;
        case 14:
            sConfig.Channel = ADC_CHANNEL_14;
            break;
        case 15:
            sConfig.Channel = ADC_CHANNEL_15;
            break;
        case 16:
            sConfig.Channel = ADC_CHANNEL_16;
            break;
        case 17:
            sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
#if !defined (ADC_SUPPORT_2_5_MSPS)
            sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5; // Minimum ADC sampling time when reading the temperature is 5us
#else
            sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
#endif
            break;
        case 18:
            sConfig.Channel = ADC_CHANNEL_VBAT;
#if !defined (ADC_SUPPORT_2_5_MSPS)
            sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5; // Minimum ADC sampling time when reading the VBAT is 12us
#else
            sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
#endif
            break;
        default:
            return 0;
    }

    if (HAL_ADC_ConfigChannel(&obj->handle, &sConfig) != HAL_OK) {
        debug("ADC channel configuration failed\r\n");
    }

    // Start conversion
    if (HAL_ADC_Start(&obj->handle) != HAL_OK) {
        debug("ADC start of conversion failed\r\n");
    }

    // Wait end of conversion and get value
    uint16_t adcValue = 0;
    if (HAL_ADC_PollForConversion(&obj->handle, 10) == HAL_OK) {
        adcValue = (uint16_t)HAL_ADC_GetValue(&obj->handle);
    }

    if (HAL_ADC_Stop(&obj->handle) != HAL_OK) {
        debug("HAL_ADC_Stop failed\r\n");
    }

    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE((&obj->handle)->Instance), LL_ADC_PATH_INTERNAL_NONE);
    return adcValue;
}

const PinMap *analogin_pinmap()
{
    return PinMap_ADC;
}

#endif
