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
#ifndef __ARCH_ARM_SRC_S5JS100_CHIP_S5JS100_GPIO_H__
#define __ARCH_ARM_SRC_S5JS100_CHIP_S5JS100_GPIO_H__

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* Bit-encoded input to s5js100_configgpio() */
/* [02:00] */
#define GPIO_FUNC_SHIFT     (0)
#define GPIO_FUNC_MASK      (0x7 << GPIO_FUNC_SHIFT)
#define GPIO_ALT0           (0x0 << GPIO_FUNC_SHIFT)
#define GPIO_ALT1           (0x1 << GPIO_FUNC_SHIFT)
#define GPIO_ALT2           (0x2 << GPIO_FUNC_SHIFT)
#define GPIO_ALT3           (0x3 << GPIO_FUNC_SHIFT)
#define GPIO_ALT4           (0x4 << GPIO_FUNC_SHIFT)
#define GPIO_ALT5           (0x5 << GPIO_FUNC_SHIFT)
#define GPIO_ALT6           (0x6 << GPIO_FUNC_SHIFT)
/* [03:03] */
#define GPIO_INT_SHIFT          3
#define GPIO_INT_MASK           (0x1 << GPIO_INT_SHIFT)
#define GPIO_EINT           (0x1 << GPIO_INT_SHIFT)
/* [06:04] */
#define GPIO_EINT_SHIFT         4
#define GPIO_EINT_MASK          (0x7 << GPIO_EINT_SHIFT)
#define GPIO_EINT_LOW           (0x0 << GPIO_EINT_SHIFT)
#define GPIO_EINT_HIGH          (0x1 << GPIO_EINT_SHIFT)
#define GPIO_EINT_FALLING_EDGE      (0x2 << GPIO_EINT_SHIFT)
#define GPIO_EINT_RISING_EDGE       (0x3 << GPIO_EINT_SHIFT)
#define GPIO_EINT_BOTH_EDGE     (0x4 << GPIO_EINT_SHIFT)
/* [07:07] */
#define GPIO_VALUE_SHIFT        7
#define GPIO_VALUE_MASK         (0x1 << GPIO_VALUE_SHIFT)
#define GPIO_VALUE_ZERO         (0x0 << GPIO_VALUE_SHIFT)
#define GPIO_VALUE_ONE          (0x1 << GPIO_VALUE_SHIFT)
/* [10:08] */
#define GPIO_DRVSTR_SHIFT       8
#define GPIO_DRVSTR_MASK        (0x7 << GPIO_DRVSTR_SHIFT)
#define GPIO_FAST1X         (0x0 << GPIO_DRVSTR_SHIFT)
#define GPIO_FAST2X         (0x1 << GPIO_DRVSTR_SHIFT)
#define GPIO_FAST3X         (0x2 << GPIO_DRVSTR_SHIFT)
#define GPIO_FAST4X         (0x3 << GPIO_DRVSTR_SHIFT)
#define GPIO_SLOW1X         (0x4 << GPIO_DRVSTR_SHIFT)
#define GPIO_SLOW2X         (0x5 << GPIO_DRVSTR_SHIFT)
#define GPIO_SLOW3X         (0x6 << GPIO_DRVSTR_SHIFT)
#define GPIO_SLOW4X         (0x7 << GPIO_DRVSTR_SHIFT)
/* [12:11] */
#define GPIO_PUPD_SHIFT         11
#define GPIO_PUPD_MASK          (0x3 << GPIO_PUPD_SHIFT)
#define GPIO_FLOAT          (0x0 << GPIO_PUPD_SHIFT)
#define GPIO_RESERVED           (0x1 << GPIO_PUPD_SHIFT)
#define GPIO_PULLDOWN           (0x2 << GPIO_PUPD_SHIFT)
#define GPIO_PULLUP         (0x3 << GPIO_PUPD_SHIFT)
/* [13:13] */
#define GPIO_INPUT_SHIFT        13
#define GPIO_INPUT_MASK         (0x1 << GPIO_INPUT_SHIFT)
#define GPIO_INPUT          (0x1 << GPIO_INPUT_SHIFT)
/* [14:14] */
#define GPIO_OUTPUT_SHIFT       14
#define GPIO_OUTPUT_MASK        (0x1 << GPIO_OUTPUT_SHIFT)
#define GPIO_OUTPUT         (0x1 << GPIO_OUTPUT_SHIFT)
/* [22:16] */
#define GPIO_PIN_SHIFT          16
#define GPIO_PIN_MASK           (0x7F << GPIO_PIN_SHIFT)
/* [28:23] */
#define GPIO_IRQ_SHIFT          23
#define GPIO_IRQ_MASK           (0x3F << GPIO_IRQ_SHIFT)

#define GPIO_PIN0           (0 << GPIO_PIN_SHIFT)
#define GPIO_PIN1           (1 << GPIO_PIN_SHIFT)
#define GPIO_PIN2           (2 << GPIO_PIN_SHIFT)
#define GPIO_PIN3           (3 << GPIO_PIN_SHIFT)
#define GPIO_PIN4           (4 << GPIO_PIN_SHIFT)
#define GPIO_PIN5           (5 << GPIO_PIN_SHIFT)
#define GPIO_PIN6           (6 << GPIO_PIN_SHIFT)
#define GPIO_PIN7           (7 << GPIO_PIN_SHIFT)
#define GPIO_PIN8           (8 << GPIO_PIN_SHIFT)
#define GPIO_PIN9           (9 << GPIO_PIN_SHIFT)
#define GPIO_PIN10          (10 << GPIO_PIN_SHIFT)
#define GPIO_PIN11          (11 << GPIO_PIN_SHIFT)
#define GPIO_PIN12          (12 << GPIO_PIN_SHIFT)
#define GPIO_PIN13          (13 << GPIO_PIN_SHIFT)
#define GPIO_PIN14          (14 << GPIO_PIN_SHIFT)
#define GPIO_PIN15          (15 << GPIO_PIN_SHIFT)
#define GPIO_PIN16          (16 << GPIO_PIN_SHIFT)
#define GPIO_PIN17          (17 << GPIO_PIN_SHIFT)
#define GPIO_PIN18          (18 << GPIO_PIN_SHIFT)
#define GPIO_PIN19          (19 << GPIO_PIN_SHIFT)
#define GPIO_PIN20          (20 << GPIO_PIN_SHIFT)
#define GPIO_PIN21          (21 << GPIO_PIN_SHIFT)
#define GPIO_PIN22          (22 << GPIO_PIN_SHIFT)
#define GPIO_PIN23          (23 << GPIO_PIN_SHIFT)
#define GPIO_PIN24          (24 << GPIO_PIN_SHIFT)
#define GPIO_PIN25          (25 << GPIO_PIN_SHIFT)
#define GPIO_PIN26          (26 << GPIO_PIN_SHIFT)
#define GPIO_PIN27          (27 << GPIO_PIN_SHIFT)
#define GPIO_PIN28          (28 << GPIO_PIN_SHIFT)
#define GPIO_PIN29          (29 << GPIO_PIN_SHIFT)
#define GPIO_PIN30          (30 << GPIO_PIN_SHIFT)
#define GPIO_PIN31          (31 << GPIO_PIN_SHIFT)
#define GPIO_PIN32          (32 << GPIO_PIN_SHIFT)
#define GPIO_PIN33          (33 << GPIO_PIN_SHIFT)
#define GPIO_PIN34          (34 << GPIO_PIN_SHIFT)
#define GPIO_PIN35          (35 << GPIO_PIN_SHIFT)
#define GPIO_PIN36          (36 << GPIO_PIN_SHIFT)
#define GPIO_PIN37          (37 << GPIO_PIN_SHIFT)
#define GPIO_PIN38          (38 << GPIO_PIN_SHIFT)
#define GPIO_PIN39          (39 << GPIO_PIN_SHIFT)
#define GPIO_PIN40          (40 << GPIO_PIN_SHIFT)
#define GPIO_PIN41          (41 << GPIO_PIN_SHIFT)
#define GPIO_PIN42          (42 << GPIO_PIN_SHIFT)
#define GPIO_PIN43          (43 << GPIO_PIN_SHIFT)
#define GPIO_PIN44          (44 << GPIO_PIN_SHIFT)
#define GPIO_PIN45          (45 << GPIO_PIN_SHIFT)
#define GPIO_PIN46          (46 << GPIO_PIN_SHIFT)
#define GPIO_PIN47          (47 << GPIO_PIN_SHIFT)
#define GPIO_PIN48          (48 << GPIO_PIN_SHIFT)
#define GPIO_PIN49          (49 << GPIO_PIN_SHIFT)
#define GPIO_PIN50          (50 << GPIO_PIN_SHIFT)
#define GPIO_PIN51          (51 << GPIO_PIN_SHIFT)
#define GPIO_PIN52          (52 << GPIO_PIN_SHIFT)
#define GPIO_PIN53          (53 << GPIO_PIN_SHIFT)
#define GPIO_PIN54          (54 << GPIO_PIN_SHIFT)
#define GPIO_PIN55          (55 << GPIO_PIN_SHIFT)
#define GPIO_PIN56          (56 << GPIO_PIN_SHIFT)
#define GPIO_PIN57          (57 << GPIO_PIN_SHIFT)
#define GPIO_PIN58          (58 << GPIO_PIN_SHIFT)
#define GPIO_PIN59          (59 << GPIO_PIN_SHIFT)
#define GPIO_PIN60          (60 << GPIO_PIN_SHIFT)
#define GPIO_PIN61          (61 << GPIO_PIN_SHIFT)
#define GPIO_PIN62          (62 << GPIO_PIN_SHIFT)
#define GPIO_PIN63          (63 << GPIO_PIN_SHIFT)
#define GPIO_PIN64          (64 << GPIO_PIN_SHIFT)
#define GPIO_PIN65          (65 << GPIO_PIN_SHIFT)
#define GPIO_PIN66          (66 << GPIO_PIN_SHIFT)
#define GPIO_PIN67          (67 << GPIO_PIN_SHIFT)
#define GPIO_PIN68          (68 << GPIO_PIN_SHIFT)
#define GPIO_PIN69          (69 << GPIO_PIN_SHIFT)
#define GPIO_PIN70          (70 << GPIO_PIN_SHIFT)
#define GPIO_PIN71          (71 << GPIO_PIN_SHIFT)
#define GPIO_PIN72          (72 << GPIO_PIN_SHIFT)

#define S5JS100_GPIO_NPORTS     73
#define GPIO_BASE           (0x85026000)
#define TOP_MUX_BASE        (0x82021000)
#define GPIO_MSG            1
#define GPIO_INT            0

/* Register addresses *******************************************************/

#define GPIO_CPU_OFFSET         (0x0020)
#define GPIO_CON_REG            (GPIO_BASE + 0x0000)
#define GPIO_OUT_REG            (GPIO_BASE + 0x0100)
#define GPIO_IN_REG             (GPIO_BASE + 0x0200)
#define GPIO_INT_MASK_REG       (GPIO_BASE + 0x0300)
#define GPIO_INT_PEND_CLR_REG   (GPIO_BASE + 0x0400)
#define GPIO_INT_POL_REG        (GPIO_BASE + 0x0500)
#define GPIO_INT_METHOD_REG     (GPIO_BASE + 0x0600)
#define GPIO_INT_BOTH_REG       (GPIO_BASE + 0x0700)
#define GPIO_ICG_REG            (GPIO_BASE + 0x0800)
#define GPIO_CPU_SEL_REG        (GPIO_BASE + 0x0900)

enum level_ {
    LOW = 0,
    HIGH = 1,
};

enum dir_ {
    OUT = 0,
    IN = 1,
};

enum gpio_int_type_ {
    LEVEL_LOW,
    LEVEL_HIGH,
    NEG_EDGE,
    POS_EDGE,
    BOTH_EDGE,
};

enum pull_up_down_ {
    GPIO_PUPD_DIS = 0,
    GPIO_PULL_DOWN = 2,         // 0b10 (enable + down)
    GPIO_PULL_UP = 3,           // 0b11 (enable + up)
};

enum drive_strength_ {
    DS_2mA = 0,
    DS_4mA,
    DS_8mA,
    DS_12mA,
};

enum slew_rate_ {
    SR_FAST = 0,
    SR_SLOW,
};

enum in_enable_ {
    IN_DIS = 0,
    IN_EN,
};

enum out_enable_ {
    OUT_DIS = 0,
    OUT_EN,
};

// bit postion in top_mux register
enum bitpos_topmux_ {
    BITPOS_FUNC = 0,
    BITPOS_DS = 8,              //drive strengh
    BITPOS_SR = 10,             //slew rate
    BITPOS_PUPD = 11,
    BITPOS_IE = 13,             //input enable/disable
    BITPOS_OE = 14,             //output enable/disable
};

enum nmux_func_ {
    NMUX_SEL_0 = 0,
    NMUX_SEL_1 = 1,
    NMUX_SEL_2 = 2,
    NMUX_SEL_3 = 3,
    NMUX_SEL_4 = 4,
    NMUX_SEL_5 = 5,
    NMUX_SEL_6 = 6,
    GPIO_GPIO = 7,
};

enum gpio_id_ {
    GPIO0 = 0,
    GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10,
    GPIO11, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO19, GPIO20,
    GPIO21, GPIO22, GPIO23, GPIO24, GPIO25, GPIO26, GPIO27, GPIO28, GPIO29, GPIO30,
    GPIO31, GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO37, GPIO38, GPIO39, GPIO40,
    GPIO41, GPIO42, GPIO43, GPIO44, GPIO45, GPIO46, GPIO47, GPIO48, GPIO49, GPIO50,
    GPIO51, GPIO52, GPIO53, GPIO54, GPIO55, GPIO56, GPIO57, GPIO58, GPIO59, GPIO60,
    GPIO61, GPIO62, GPIO63, GPIO64, GPIO65, GPIO66, GPIO67, GPIO68, GPIO69, GPIO70,
    GPIO71, GPIO72, GPIO_MAX,
};

#define SetBits(uAddr, uBaseBit, uMaskValue, uSetValue) \
    putreg32((getreg32(uAddr) & ~((uMaskValue)<<(uBaseBit))) | (((uMaskValue)&(uSetValue))<<(uBaseBit)), uAddr)
#define GetBits(uAddr, uBaseBit, uMaskValue) \
    ((getreg32(uAddr)>>(uBaseBit))&(uMaskValue))

#ifdef __cplusplus
extern "C" {
#endif
int s5js100_configgpio(uint32_t cfgset);

/****************************************************************************
 * Name: s5js100_gpiowrite
 *
 * Description:
 *   Write one or zero to the selected GPIO pin
 *
 ****************************************************************************/
void s5js100_gpiowrite(uint32_t cfgset, bool value);

/****************************************************************************
 * Name: s5js100_gpioread
 *
 * Description:
 *   Read one or zero to the selected GPIO pin
 *
 ****************************************************************************/
bool s5js100_gpioread(uint32_t cfgset);

void gpio_set_func(enum gpio_id_ id, enum nmux_func_ func);
enum pull_up_down_ gpio_get_pupd(enum gpio_id_ id);

void s5js100_pullup(uint32_t cfgset);
int s5js100_configinput(uint32_t cfgset);
int s5js100_configoutput(uint32_t cfgset);
int s5js100_unconfiggpio(uint32_t cfgset);

#ifdef __cplusplus
}
#endif


#endif
