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

#include "mbed_assert.h"
#include "spi_api.h"

#if DEVICE_SPI

#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "PeripheralPins.h"

#if DEVICE_SPI_ASYNCH
#define SPI_S(obj)    (( struct spi_s *)(&(obj->spi)))
#else
#define SPI_S(obj)    (( struct spi_s *)(obj))
#endif

/*
 * Only the frequency is managed in the family specific part
 * the rest of SPI management is common to all STM32 families
 */
int spi_get_clock_freq(spi_t *obj)
{
    struct spi_s *spiobj = SPI_S(obj);
    int spi_hz = 0;

    /* Get source clock depending on SPI instance */
    switch ((int)spiobj->spi) {
        case SPI_1:
            spi_hz = HAL_RCC_GetPCLK2Freq();
            break;
#if defined SPI2_BASE
        case SPI_2:
            spi_hz = HAL_RCC_GetPCLK1Freq();
            break;
#endif
        default:
            error("CLK: SPI instance not set");
            break;
    }

    return spi_hz;
}

#endif
