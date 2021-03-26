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
#ifndef MBED_ADC_DEF_H
#define MBED_ADC_DEF_H

#define S5JS100_ADC_MAX_CHANNELS    4

/* Register Address *********************************************************/
#define S5JS100_ADC_CON1        0x85021000
#define S5JS100_ADC_CON2        0x85021004
#define S5JS100_ADC_DAT         0x85021008
#define S5JS100_ADC_SUMDAT      0x8502100C
#define S5JS100_ADC_INT         0x85021010
#define S5JS100_ADC_INT_STATUS  0x85021014
#define S5JS100_ADC_DEBUG_DATA  0x8502101C
#define S5JS100_ADC_VERSION     0x85021028
/* Register Bitfield Definitions ********************************************/

/* ADC control 1 register */
#define ADC_CON1_SOFTRESET_SHIFT    1
#define ADC_CON1_SOFTRESET_MASK     (0x3 << ADC_CON1_SOFTRESET_SHIFT)
#define ADC_CON1_SOFTRESET_RESET    (0x2 << ADC_CON1_SOFTRESET_SHIFT)
#define ADC_CON1_SOFTRESET_NONRESET (0x1 << ADC_CON1_SOFTRESET_SHIFT)

#define ADC_CON1_STCEN_SHIFT        0
#define ADC_CON1_STCEN_MASK         (0x1 << ADC_CON1_STCEN_SHIFT)
#define ADC_CON1_STCEN_DISABLE      (0x0 << ADC_CON1_STCEN_SHIFT)
#define ADC_CON1_STCEN_ENABLE       (0x1 << ADC_CON1_STCEN_SHIFT)

/* ADC control 2 register */
#define ADC_CON2_CTIME_SHIFT        4
#define ADC_CON2_CTIME_MASK         (0x7 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_1            (0x0 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_2            (0x1 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_4            (0x2 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_8            (0x3 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_16           (0x4 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_32           (0x5 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_64           (0x6 << ADC_CON2_CTIME_SHIFT)
#define ADC_CON2_CTIME_RESERVED     (0x7 << ADC_CON2_CTIME_SHIFT)

#define ADC_CON2_ACHSEL_SHIFT       0
#define ADC_CON2_ACHSEL_MASK        (0xf << ADC_CON2_ACHSEL_SHIFT)
#define ADC_CON2_ACHSEL_CHANNEL0    (0x0 << ADC_CON2_ACHSEL_SHIFT)
#define ADC_CON2_ACHSEL_CHANNEL1    (0x1 << ADC_CON2_ACHSEL_SHIFT)
#define ADC_CON2_ACHSEL_CHANNEL2    (0x2 << ADC_CON2_ACHSEL_SHIFT)
#define ADC_CON2_ACHSEL_CHANNEL3    (0x3 << ADC_CON2_ACHSEL_SHIFT)

/* ADC average data and flag register */
#define ADC_DAT_FLAG_SHIFT          31
#define ADC_DAT_FLAG_MASK           (0x1 << ADC_DAT_FLAG_SHIFT)
#define ADC_DAT_FLAG_INPROGRESS     (0x0 << ADC_DAT_FLAG_SHIFT)
#define ADC_DAT_FLAG_END            (0x1 << ADC_DAT_FLAG_SHIFT)

#define ADC_DAT_ADCDAT_SHIFT        0
#define ADC_DAT_ADCDAT_MASK         (0xfff << ADC_DAT_ADCDAT_SHIFT)

/* ADC sum data and flag register */
#define ADC_SUMDAT_FLAG_SHIFT       31
#define ADC_SUMDAT_FLAG_MASK        (0x1 << ADC_SUM_DAT_FLAG_SHIFT)
#define ADC_SUMDAT_FLAG_INPROGRESS  (0x0 << ADC_SUM_DAT_FLAG_SHIFT)
#define ADC_SUMDAT_FLAG_END         (0x1 << ADC_SUM_DAT_FLAG_SHIFT)

#define ADC_SUMDAT_ADCDAT_SHIFT     0
#define ADC_SUMDAT_ADCDAT_MASK      (0xfff << ADC_SUM_DAT_ADCDAT_SHIFT)

/* ADC interrupt enable/disable register */
#define ADC_INT_DISABLE             0
#define ADC_INT_ENABLE              1

/* ADC interrupt status register */
#define ADC_INT_STATUS_PENDING      1


typedef enum {
    adc_conversion_1time    = ADC_CON2_CTIME_1,
    adc_conversion_2time    = ADC_CON2_CTIME_2,
    adc_conversion_4time    = ADC_CON2_CTIME_4,
    adc_conversion_8time    = ADC_CON2_CTIME_8,
    adc_conversion_16time   = ADC_CON2_CTIME_16,
    adc_conversion_32time   = ADC_CON2_CTIME_32,
    adc_conversion_64time   = ADC_CON2_CTIME_64,
} adc_conversion_mode_t;

typedef enum {
    adc_channel_0       = ADC_CON2_ACHSEL_CHANNEL0,
    adc_channel_1       = ADC_CON2_ACHSEL_CHANNEL1,
    adc_channel_2       = ADC_CON2_ACHSEL_CHANNEL2,
    adc_channel_3       = ADC_CON2_ACHSEL_CHANNEL3,
} adc_channel_t;

#endif
