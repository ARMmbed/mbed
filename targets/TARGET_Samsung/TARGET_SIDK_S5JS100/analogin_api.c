/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
#if DEVICE_ANALOGIN

#include "analogin_api.h"
#include "analogin_def.h"
#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "PeripheralPins.h"

const PinMap *analogin_pinmap(void)
{
    return PinMap_ADC;
}

static void adc_startconv(analogin_t *obj, int enable)
{
    if (enable) {
        modifyreg32(S5JS100_ADC_CON1, 1, ADC_CON1_STCEN_ENABLE);
        while (!(getreg32(S5JS100_ADC_DAT) & ((uint32_t)(1 << 31))));
    } else {
        modifyreg32(S5JS100_ADC_CON1, ADC_CON1_STCEN_ENABLE, 0);
    }
}

static void adc_conversion(analogin_t *obj)
{
    uint16_t sample;
    /* Read the ADC sample and pass it to the upper-half */
    sample = (uint16_t)(getreg32(S5JS100_ADC_DAT) & ADC_DAT_ADCDAT_MASK);

    /* Set the next channel to be sampled */
    //obj->current++;

    if (obj->current >= obj->cchannels) {
        obj->current = 0;
    }

    /* Change to the next channel */
    modifyreg32(S5JS100_ADC_CON2, ADC_CON2_ACHSEL_MASK,
                obj->chanlist[obj->current]);

    /* Exit, start a new conversion */
    adc_startconv(obj, 1);
}

static int adc_interrupt(analogin_t *obj)
{
    if (getreg32(S5JS100_ADC_INT_STATUS) & ADC_INT_STATUS_PENDING) {
        adc_conversion(obj);
        /* Clear interrupt pending */
        putreg32(ADC_INT_STATUS_PENDING, S5JS100_ADC_INT_STATUS);
    }
}

static int adc_set_ch(analogin_t *obj, uint8_t ch)
{
    int i;

    if (ch == 0) {
        /*
         *       * 0 is reserved for special purpose which samples
         *               * all channels
         *                       */
        obj->current = 0;
    } else {
        /* REVISIT: changing channel is not supported for now */

        for (i = 0; i < obj->cchannels &&
                obj->chanlist[i] != ch; i++);

        if (i >= obj->cchannels) {
            return -1;
        }

        obj->current   = i;
    }

    modifyreg32(S5JS100_ADC_CON2, ADC_CON2_ACHSEL_MASK, obj->chanlist[obj->current]);

    return 0;
}

static void adc_reset(analogin_t *obj)
{
    /* Reset ADC */
    putreg32(ADC_CON1_SOFTRESET_RESET, S5JS100_ADC_CON1);


    /* Release ADC from reset state */
    putreg32(ADC_CON1_SOFTRESET_NONRESET, S5JS100_ADC_CON1);

    /* Configuration of the channel conversions */
    adc_set_ch(obj, obj->current);
    // putreg32(ADC_INT_ENABLE, S5JS100_ADC_INT);
}


static void adc_setup(analogin_t *obj)
{
    NVIC_SetVector((IRQn_Type)(S5JS100_IRQ_GPADC), (uint32_t)(adc_interrupt));
    adc_reset(obj);
    NVIC_EnableIRQ((IRQn_Type)(S5JS100_IRQ_GPADC));
}

float analogin_read(analogin_t *obj)
{
    uint32_t data;
    adc_conversion(obj);
    data = getreg32(S5JS100_ADC_DAT) & 0xFFF;
    return (float)data / 0xFFF;
}

uint16_t analogin_read_u16(analogin_t *obj)
{
    uint16_t data;
    adc_conversion(obj);
    data = getreg32(S5JS100_ADC_DAT) & 0xFFF;
    data = (data / 0xfff) * 0xffff;
    return data;
}

#if STATIC_PINMAP_READY
#define ANALOGIN_INIT_DIRECT analogin_init_direct
void analogin_init_direct(analogin_t *obj, const PinMap *pinmap)
#else
#define ANALOGIN_INIT_DIRECT _analogin_init_direct
static void _analogin_init_direct(analogin_t *obj, const PinMap *pinmap)
#endif
{
    int i;
    uint8_t chanlist[] = {
        adc_channel_0,
        adc_channel_1,
        adc_channel_2,
        adc_channel_3,
    };

    obj->adc = (void *)S5JS100_GPADCIF0;
    obj->current = (int)pinmap->peripheral;
    obj->cchannels = 4;

    for (i = 0; i < 4; i++) {
        obj->chanlist[i] = chanlist[i];
    }

    adc_setup(obj);
}

void analogin_init(analogin_t *obj, PinName pin)
{
    int peripheral = (int)pinmap_peripheral(pin, PinMap_ADC);
    int function = (int)pinmap_find_function(pin, PinMap_ADC);

    const PinMap static_pinmap = {pin, peripheral, function};

    ANALOGIN_INIT_DIRECT(obj, &static_pinmap);
}

void analogin_free(analogin_t *obj)
{

}

#endif
