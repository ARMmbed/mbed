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

#include <stdbool.h>

#include "s5js100.h"
#include "s5js100_error.h"
#include "s5js100_pinconfig.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/
#define DEFAULT_INPUT (GPIO_INPUT | GPIO_PULLDOWN)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
// BIT MASK
#define MASK_1_BIT  (0x00000001)
#define MASK_2_BIT  (0x00000003)
#define MASK_3_BIT  (0x00000007)
#define MASK_4_BIT  (0x0000000F)
#define MASK_5_BIT  (0x0000001F)
#define MASK_6_BIT  (0x0000003F)
#define MASK_7_BIT  (0x0000007F)

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
struct tmux_range_ {
    uint16_t addr;
    uint8_t start_num;
    uint8_t end_num;
};

uint32_t get_tmux_addr(enum gpio_id_ id)
{
    uint32_t i;
    uint32_t offset;
    const struct tmux_range_ tmux_range[] = {
        //   addr  start  end
        {0x03C, 24, 72},
        {0x100, 0, 23},
    };

    offset = 0;

    for (i = 0; i < ARRAY_SIZE(tmux_range); i++) {
        if ((id >= tmux_range[i].start_num) && (id <= tmux_range[i].end_num)) {
            offset = tmux_range[i].addr + ((id - tmux_range[i].start_num) * 4);
            break;
        }
    }

    return (TOP_MUX_BASE + offset);
}

void gpio_set_input_enable(enum gpio_id_ id, enum in_enable_ inen)
{
    uint32_t addr;

    addr = get_tmux_addr(id);
    SetBits(addr, BITPOS_OE, MASK_1_BIT, OUT_DIS);
    SetBits(addr, BITPOS_IE, MASK_1_BIT, inen);
}

void gpio_set_output_enable(enum gpio_id_ id, enum out_enable_ outen)
{
    uint32_t addr;

    addr = get_tmux_addr(id);
    SetBits(addr, BITPOS_IE, MASK_1_BIT, IN_DIS);
    SetBits(addr, BITPOS_OE, MASK_1_BIT, outen);
}

void gpio_set_func(enum gpio_id_ id, enum nmux_func_ func)
{
    uint32_t addr;

    addr = get_tmux_addr(id);
    SetBits(addr, BITPOS_FUNC, MASK_3_BIT, func);
}

void gpio_set_data(enum gpio_id_ id, enum level_ level)
{
    uint32_t addr;
    uint32_t bitpos;

    addr = GPIO_OUT_REG + ((id >> 5) << 2); // able to set only GPIO_OUT
    bitpos = id & 0x1F;

    SetBits(addr, bitpos, MASK_1_BIT, level);
}

enum level_ gpio_get_data(enum gpio_id_ id)
{
    uint32_t addr;
    uint32_t bitpos;
    enum level_ level;
    enum dir_ dir;

    addr = GPIO_CON_REG + ((id >> 5) << 2); // decide GPIO direction

    bitpos = id & 0x1F;
    dir = (enum dir_)GetBits(addr, bitpos, MASK_1_BIT);

    if (dir == IN) {
        addr = GPIO_IN_REG + ((id >> 5) << 2);
    } else { /* OUTPUT */
        addr = GPIO_OUT_REG + ((id >> 5) << 2);
    }

    bitpos = id & 0x1F;

    level = (enum level_)GetBits(addr, bitpos, MASK_1_BIT);
    return level;
}

enum pull_up_down_ gpio_get_pupd(enum gpio_id_ id)
{
    uint32_t addr;
    enum pull_up_down_ pupd;

    addr = get_tmux_addr(id);

    pupd = (enum pull_up_down_)GetBits(addr, BITPOS_PUPD, MASK_2_BIT);
    return pupd;
}

void gpio_set_pupd(enum gpio_id_ id, enum pull_up_down_ pupd)
{
    uint32_t addr;

    addr = get_tmux_addr(id);
    if (pupd == GPIO_PUPD_DIS) {
        SetBits(addr, BITPOS_PUPD + 1, MASK_1_BIT, pupd);
    } else {
        SetBits(addr, BITPOS_PUPD, MASK_2_BIT, pupd);
    }
}

void gpio_set_ds(enum gpio_id_ id, enum drive_strength_ ds)
{
    uint32_t addr;

    addr = get_tmux_addr(id);
    SetBits(addr, BITPOS_DS, MASK_2_BIT, ds);
}

void gpio_set_sr(enum gpio_id_ id, enum slew_rate_ sr)
{
    uint32_t addr;

    addr = get_tmux_addr(id);
    SetBits(addr, BITPOS_SR, MASK_1_BIT, sr);
}

/****************************************************************************
 * Name: s5js100_setdrv
 *
 * Description:
 *   Set drive strength of a GPIO pin.
 *
 ****************************************************************************/
void s5js100_setdrv(uint32_t cfgset)
{
    uint32_t ds;
    uint32_t pin;
    /* CAVEAT: GPIO_FAST|SLOWXXX is compatible with GPIO_DRV_XXX */
    ds = (cfgset & GPIO_DRVSTR_MASK) >> GPIO_DRVSTR_SHIFT;
    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;
    gpio_set_ds(pin, ds);
}

/****************************************************************************
 * Name: s5js100_pullup
 *
 * Description:
 *   Set pull-up/down on a GPIO pin.
 *
 ****************************************************************************/
void s5js100_pullup(uint32_t cfgset)
{
    uint32_t pin;
    uint32_t pupd;

    /* CAVEAT: GPIO_PUPD_XXX is compatible with GPIO_PUD_XXX */
    pupd = (cfgset & GPIO_PUPD_MASK) >> GPIO_PUPD_SHIFT;
    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;
    gpio_set_pupd(pin, pupd);
}

/****************************************************************************
 * Name: s5js100_setintedge
 *
 * Description:
 *
 ****************************************************************************/
void s5js100_setintedge(unsigned int pin, int edge)
{
    uint32_t group;
    uint32_t addr;
    uint32_t bitpos;

    group = pin >> 5;           //group = 0, 1,
    addr = group << 2;          // addr = 0x0, 0x4,
    bitpos = pin & 0x1F;

    switch (edge) {
        case LEVEL_LOW:
            putreg32(getreg32(GPIO_INT_METHOD_REG + addr) | (1 << bitpos), GPIO_INT_METHOD_REG + addr);
            putreg32(getreg32(GPIO_INT_POL_REG + addr) & ~(1 << bitpos), GPIO_INT_POL_REG + addr);
            break;
        case LEVEL_HIGH:
            putreg32(getreg32(GPIO_INT_METHOD_REG + addr) | (1 << bitpos), GPIO_INT_METHOD_REG + addr);
            putreg32(getreg32(GPIO_INT_POL_REG + addr) | (1 << bitpos), GPIO_INT_POL_REG + addr);
            break;
        case NEG_EDGE:
            putreg32(getreg32(GPIO_INT_METHOD_REG + addr) & ~(1 << bitpos), GPIO_INT_METHOD_REG + addr);
            putreg32(getreg32(GPIO_INT_BOTH_REG + addr) & ~(1 << bitpos), GPIO_INT_BOTH_REG + addr);
            putreg32(getreg32(GPIO_INT_POL_REG + addr) & ~(1 << bitpos), GPIO_INT_POL_REG + addr);
            gpio_set_pupd(pin, GPIO_PULL_UP);
            break;
        case POS_EDGE:
            putreg32(getreg32(GPIO_INT_METHOD_REG + addr) & ~(1 << bitpos), GPIO_INT_METHOD_REG + addr);
            putreg32(getreg32(GPIO_INT_BOTH_REG + addr) & ~(1 << bitpos), GPIO_INT_BOTH_REG + addr);
            putreg32(getreg32(GPIO_INT_POL_REG + addr) | (1 << bitpos), GPIO_INT_POL_REG + addr);
            gpio_set_pupd(pin, GPIO_PULL_DOWN);
            break;
        case BOTH_EDGE:
            putreg32(getreg32(GPIO_INT_METHOD_REG + addr) & ~(1 << bitpos), GPIO_INT_METHOD_REG + addr);
            putreg32(getreg32(GPIO_INT_BOTH_REG + addr) | (1 << bitpos), GPIO_INT_BOTH_REG + addr);
            break;
        default:
            break;
    }
}

/****************************************************************************
 * Name: s5js100_setintmask
 *
 * Description:
 *   Mask or unmask interrupts from GPIO pins.
 *
 ****************************************************************************/
void s5js100_setintmask(unsigned int pin, int mask)
{
    uint32_t group;
    uint32_t addr;
    uint32_t bitpos;
    group = pin >> 5;           //group = 0, 1,
    addr = group << 2;          // addr = 0x0, 0x4,
    bitpos = pin & 0x1F;

    if (mask) {
        putreg32(getreg32(GPIO_INT_MASK_REG + addr) | (0 << bitpos), GPIO_INT_MASK_REG + addr); // int disable
    } else {
        putreg32(getreg32(GPIO_INT_MASK_REG + addr) | (1 << bitpos), GPIO_INT_MASK_REG + addr); // int enable
    }
    /* workaround, eint first is set pended if counter part has GPIO output */
    putreg32(getreg32(GPIO_INT_PEND_CLR_REG + addr) | (1 << bitpos), GPIO_INT_PEND_CLR_REG + addr); // pending clear
}

/****************************************************************************
 * Name: s5js100_configinput
 *
 * Description:
 *   COnfigure a GPIO input pin based on bit-encoded description of the pin.
 *
 ****************************************************************************/
int s5js100_configinput(uint32_t cfgset)
{
    uint32_t pin;
    uint32_t addr, bitpos;

    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;

    gpio_set_func(pin, GPIO_GPIO);

    /* Set as input */
    addr = GPIO_BASE + ((pin >> 5) << 2);
    bitpos = pin & 0x1F;
    putreg32(getreg32(addr) | (1 << bitpos), addr);
    gpio_set_input_enable(pin, IN_EN);

    /* Set pull-up/down */
    s5js100_pullup(cfgset);

    return 0;
}

/****************************************************************************
 * Name: s5js100_configalt
 *
 * Description:
 *   Configure a GPIO alternate function pin based on bit-encoded
 *   description of the pin.
 *
 ****************************************************************************/
int s5js100_configalt(uint32_t cfgset)
{
    uint32_t pin;
    uint32_t func;
    uint32_t addr;

    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;
    func = (cfgset & GPIO_FUNC_MASK) >> GPIO_FUNC_SHIFT;
    gpio_set_func(pin, func);

    /* Set pull-up mode */
    s5js100_pullup(cfgset);

    /* Set drive strength */
    s5js100_setdrv(cfgset);

    if (cfgset & (GPIO_OUTPUT | GPIO_INPUT)) {
        addr = get_tmux_addr(pin);
        SetBits(addr, BITPOS_IE, MASK_1_BIT, 1);
        SetBits(addr, BITPOS_OE, MASK_1_BIT, 1);
    } else if (cfgset & GPIO_OUTPUT) {
        s5js100_configoutput(cfgset);
    } else if (cfgset & GPIO_INPUT) {
        s5js100_configinput(cfgset);
    } else {
    }

    return 0;
}

/****************************************************************************
 * Name: s5js100_configoutput
 *
 * Description:
 *   Configure a GPIO output pin based on bit-encoded description of the pin.
 *
 ****************************************************************************/
int s5js100_configoutput(uint32_t cfgset)
{
    uint32_t pin;
    uint32_t addr, bitpos;

    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;

    gpio_set_func(pin, GPIO_GPIO);

    /* Set as output */
    addr = GPIO_BASE + ((pin >> 5) << 2);
    bitpos = pin & 0x1F;
    putreg32(getreg32(addr) & ~(1 << bitpos), addr);
    gpio_set_output_enable(pin, OUT_EN);

    /* Set the initial value of the output */
    //s5js100_gpiowrite(cfgset, (cfgset & GPIO_VALUE_MASK) ^ GPIO_VALUE_ZERO);

    /* Set drive strength */
    s5js100_setdrv(cfgset);

    return 0;
}

/****************************************************************************
 * Name: s5js100_configinterrupt
 *
 * Description:
 *   Configure a GPIO interrupt pin based on bit-encoded description
 *   of the pin
 *
 ****************************************************************************/
int s5js100_configinterrupt(uint32_t cfgset, uint32_t nirq)
{
    uint32_t pin;

    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;

    if (cfgset & GPIO_EINT) {
        /*
         * First, configure the port as a generic input so that we have
         * a known starting point and consistent behavior during the
         * re-configuration.
         */
        s5js100_configinput(cfgset);

        /* Then, just remember the rising/falling edge interrupt enabled */
        s5js100_setintedge(pin, (cfgset & GPIO_EINT_MASK) >> GPIO_EINT_SHIFT);
        s5js100_setintmask(pin, 0);

        /* workaround gpio eint clear int in NVIC side */
//      SetBits(0xE000E280, nirq - S5JS100_IRQ_FIRST, MASK_1_BIT, 0x1);
        SetBits(0xE000E280, nirq, MASK_1_BIT, 0x1);
    } else {
        s5js100_setintmask(pin, 1);
    }

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
uint8_t s5js100_gpio_irqvector(uint32_t pincfg)
{
    return (uint8_t)((pincfg & GPIO_IRQ_MASK) >> GPIO_IRQ_SHIFT);
}

void s5js100_gpio_clear_pending(uint32_t pincfg)
{
    uint32_t pin;
    uint32_t group;
    uint32_t addr;
    uint32_t bitpos;

    pin = (pincfg & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;
    group = pin >> 5;           //group = 0, 1,
    addr = group << 2;          // addr = 0x0, 0x4,
    bitpos = pin & 0x1F;

    putreg32(getreg32(GPIO_INT_PEND_CLR_REG + addr) | (1 << bitpos), GPIO_INT_PEND_CLR_REG + addr); // pending clear

}

/****************************************************************************
 * Name: s5js100_configgpio
 *
 * Description:
 *   Configure a GPIO pin based on bit-encoded description of the pin.
 *   Once it is configured as alternative (GPIO_ALT1-5) function, it must be
 *   unconfigured with s5js100_unconfiggpio() with the same cfgset first before
 *   it can be set to non-alternative function.
 *
 * Returns:
 *   OK on success
 *   A negated errno value on invalid port, or when pin is locked as ALT
 *   function.
 *
 ****************************************************************************/
int s5js100_configgpio(uint32_t cfgset)
{
    int ret = -EINVAL;
    unsigned int pin;

    /* Get the pin number */
    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;

    ret = s5js100_configalt(cfgset);

    s5js100_configinterrupt(cfgset, (cfgset & GPIO_IRQ_MASK) >> GPIO_IRQ_SHIFT);

    return ret;
}

/****************************************************************************
 * Name: s5js100_unconfiggpio
 *
 * Description:
 *   Unconfigure a GPIO pin based on bit-encoded description of the pin, set
 *   it into default state.
 *
 * Returns:
 *   OK on success
 *   A negated errno value on invalid port
 *
 ****************************************************************************/
int s5js100_unconfiggpio(uint32_t cfgset)
{
    cfgset &= GPIO_PIN_MASK;

    return s5js100_configgpio(cfgset);
}

/****************************************************************************
 * Name: s5js100_gpiowrite
 *
 * Description:
 *   Write one or zero to the selected GPIO pin
 *
 ****************************************************************************/
void s5js100_gpiowrite(uint32_t cfgset, bool value)
{
    unsigned int pin;

    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;
    gpio_set_data(pin, value);
}

/****************************************************************************
 * Name: s5js100_gpioread
 *
 * Description:
 *   Write one or zero to the selected GPIO pin
 *
 ****************************************************************************/
bool s5js100_gpioread(uint32_t cfgset)
{
    unsigned int pin;

    pin = (cfgset & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT;
    return gpio_get_data(pin);
}

uint32_t s5js100_get_intpin(unsigned int ch)
{
    uint32_t i;
    uint32_t pend = getreg32(GPIO_INT_PEND_CLR_REG + (ch * 0x4));
    for (i = 0; i < 32; i++) {
        if (pend & (0x1 << i)) {
            return i + ch * 32;
        }
    }
    return -1;
}
