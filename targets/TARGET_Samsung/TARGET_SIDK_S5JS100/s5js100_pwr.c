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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "s5js100.h"
#include "s5js100_type.h"
#include "s5js100_pwr.h"
#include "s5js100_pmusfr.h"
#include "s5js100_pmip.h"
#include "s5js100_cmu.h"

#include "mbed_trace.h"
#include "platform/mbed_wait_api.h"
#include "platform/mbed_thread.h"
#define TRACE_GROUP "PWR"

#ifndef S5JS100_PWR_DBG_ON
#define S5JS100_PWR_DBG_ON        1
#endif
#define S5JS100_PWR_DBG        if (S5JS100_PWR_DBG_ON) tr_info

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define OFF 0
#define ON 1

/****************************************************************************
 * Private Data
 ****************************************************************************/
enum _PMU_CMD_ {
    PMU_CMD_RESET = 1,
    PMU_CMD_INIT,
    PMU_CMD_SLEEP,
    PMU_CMD_AUTO,
    PMU_CMD_SHORTSLEEP,
};

static PORT_NUM alv_wkup_src;
static FILTER_TYPE wkup_type;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void hw_delay_us(unsigned int Value)
{
    volatile unsigned i, j;

    for (i = 0; i < (Value * 2); i++)
        for (j = 0; j < 100; j++);
}

#ifdef PRINT_NBSLEEP_LOG
static void direct_uart(char *buf, unsigned int buf_len)
{
#if 0
    unsigned int i;
    for (i = 0; i < buf_len; i++) {
        up_putc(buf[i]);
    }
#endif
}

static void convert2hex(char *_buffer, unsigned int data)
{
    // we make our string assuming all hex digits are 0 to 9
    // string will be of the form 0xabcd
    // where a,b,c,d are the individual hex digits
    _buffer[0] = '0';
    _buffer[1] = 'x';
    _buffer[2] = ((data >> 28) & 0x0F) + '0';
    _buffer[3] = ((data >> 24) & 0x0F) + '0';
    _buffer[4] = ((data >> 20) & 0x0F) + '0';
    _buffer[5] = ((data >> 16) & 0x0F) + '0';
    _buffer[6] = ((data >> 12) & 0x0F) + '0';
    _buffer[7] = ((data >> 8)  & 0x0F) + '0';
    _buffer[8] = ((data >> 4)  & 0x0F) + '0';
    _buffer[9] = ((data)     & 0x0F) + '0';
    _buffer[10] = '\n';

    // now we correct for the case where a digit
    // is A to F:
    if (_buffer[2] > '9') {
        _buffer[2] += 7;
    }
    if (_buffer[3] > '9') {
        _buffer[3] += 7;
    }
    if (_buffer[4] > '9') {
        _buffer[4] += 7;
    }
    if (_buffer[5] > '9') {
        _buffer[5] += 7;
    }
    if (_buffer[6] > '9') {
        _buffer[6] += 7;
    }
    if (_buffer[7] > '9') {
        _buffer[7] += 7;
    }
    if (_buffer[8] > '9') {
        _buffer[8] += 7;
    }
    if (_buffer[9] > '9') {
        _buffer[9] += 7;
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
extern char *get_env(const char *name);
extern int s5js100_dcxo_initialize(void);
MCPU_MODE mcpu_device_mode = MCPU_NONE;

void pmu_sys_cdc_clk_en(int poweron)
{
    modifyreg32(PMU_SYS_DCGOVRD, 1 << 1, poweron << 1);
}

void pmu_alive_cdc_clk_en(int poweron)
{
    modifyreg32(PMU_ALIVE_DCGOVRD, 1 << 1, poweron << 1);
}

void mcpu_bin_loading(unsigned int tcm_base_addr, unsigned int flash_base_addr, unsigned int size)
{
    memcpy((void *)tcm_base_addr, (void *)flash_base_addr, size);

    /* Select one cache invaildate function from below */
    SCB_CleanInvalidateDCache_by_Addr((void *)tcm_base_addr, size);
    //SCB_CleanInvalidateDCache();
}

unsigned int get_pmu_sys_mcpu_core_status(void)
{
    return getreg32(PMU_SYS_MCPUMON);
}

void pmu_sys_asyncbus_mask(PWR_BLK eblock)
{
    unsigned int val;
    val = getreg32(PMU_SYS_PASMASK);
    putreg32(val & (~eblock), PMU_SYS_PASMASK);
}

void pmu_sys_asyncbus_unmask(PWR_BLK eblock)
{
    modifyreg32(PMU_SYS_PASMASK, 0x3, eblock);
}

void pmu_sys_reset(PWR_BLK eblock, int reset)
{
    if (reset == 1) {
        putreg32(getreg32(PMU_SYS_IPRST_N) & (~eblock), PMU_SYS_IPRST_N);
        putreg32(getreg32(PMU_SYS_CMURST_N) & (~eblock), PMU_SYS_CMURST_N);
    } else {
        modifyreg32(PMU_SYS_CMURST_N, 0x3, eblock);
        hw_delay_us(1);
        modifyreg32(PMU_SYS_IPRST_N, 0x3, eblock);
        hw_delay_us(1);
    }
}

unsigned int get_temp(void)
{
    unsigned int i, CurTemp = 0;

    putreg32(0x201f2, PMU_ALIVE_ROTSU_CFG1);
    putreg32((1u << 29) + (1u << 28), PMU_ALIVE_ROTSU_CFG0);

    for (i = 0; i < 10; i++) {
        putreg32((1u << 29) + (1u << 28) + (1u << 24), PMU_ALIVE_ROTSU_CFG0);
        hw_delay_us(120);
        putreg32((1u << 29) + (1u << 28) + (0u << 24), PMU_ALIVE_ROTSU_CFG0);
        hw_delay_us(10);
        CurTemp += getreg32(PMU_ALIVE_ROTSU_CFG0) & 0x7ff;
        putreg32(0, PMU_ALIVE_ROTSU_CFG0);
    }

    return CurTemp / 10;
}

unsigned int get_rotsu_value(int count)
{
    int i, value = 0;

    putreg32(0x201f2, PMU_ALIVE_ROTSU_CFG1);
    putreg32((1u << 29) + (1u << 28), PMU_ALIVE_ROTSU_CFG0);

    for (i = 0; i < count; i++) {
        putreg32((1u << 29) + (1u << 28) + (1u << 24), PMU_ALIVE_ROTSU_CFG0);
        hw_delay_us(120);
        putreg32((1u << 29) + (1u << 28) + (0u << 24), PMU_ALIVE_ROTSU_CFG0);
        hw_delay_us(10);
        value += getreg32(PMU_ALIVE_ROTSU_CFG0) & 0x7ff;
        putreg32(0, PMU_ALIVE_ROTSU_CFG0);
    }

    return value;
}

int rotsu_get_temperature(void)
{
    int adc, celcius;

    adc = get_rotsu_value(10);
    celcius = ((100 * adc - 683040) * 100) / 680;

    return (celcius + 50000);
}

bool mcpu_is_on(void)
{
    if (getreg32(SYS_CFG_MCPU_HALTN) == 0x0) {
        return false;
    } else {
        return true;
    }
}

void mcpu_reset(void)
{
    mcpu_device_mode = MCPU_NONE;

    /*  if off, do nothing */
    if (getreg32(SYS_CFG_MCPU_HALTN) == 0x0) {
        return;
    }

    putreg32(0x0, SYS_CFG_MCPU_HALTN);

    pmu_sys_cdc_clk_en(ON);

    putreg32(0x0, PMU_SYS_LH_PSTATE);
    putreg32(0xf, PMU_SYS_LH_PREQ);
    while (getreg32(PMU_SYS_LH_PACCEPT) != 0xF);
    putreg32(0x0, PMU_SYS_LH_PREQ);
    while (getreg32(PMU_SYS_LH_PACCEPT) != 0x0);

    pmu_sys_asyncbus_mask(RFIP_BLK + MCPU_BLK);
    pmu_sys_reset(RFIP_BLK + MCPU_BLK, 1);

    pmu_sys_cdc_clk_en(OFF);

}

void s5js100_pmu_init(void);
void mcpu_init(MCPU_MODE device)
{
    mcpu_reset();

    s5js100_pmu_init();

    putreg32(0x1, SYS_CFG_LH_RCG_EN);
    putreg32(0x2, SYS_CFG_PPMU);
    putreg32(0x0, SYS_CFG_PAS);

    putreg32(0x40000, BAAW_SRC_START_ADDR_0);
    putreg32(0x41000, BAAW_SRC_END_ADDR_0);
    putreg32(0x40000, BAAW_REMAPPED_ADDR_0);
    putreg32(0x80000003, BAAW_ACCESS_ENABLE_OF_WINDOW_0);

    putreg32(0x10000, BAAW_SRC_START_ADDR_1);
    putreg32(0x10010, BAAW_SRC_END_ADDR_1);
    putreg32(0x10000, BAAW_REMAPPED_ADDR_1);
    putreg32(0x80000003, BAAW_ACCESS_ENABLE_OF_WINDOW_1);

    putreg32(0x85022, BAAW_SRC_START_ADDR_2);
    putreg32(0x85042, BAAW_SRC_END_ADDR_2);
    putreg32(0x85022, BAAW_REMAPPED_ADDR_2);
    putreg32(0x80000003, BAAW_ACCESS_ENABLE_OF_WINDOW_2);

    putreg32(0x82021, BAAW_SRC_START_ADDR_3);
    putreg32(0x82022, BAAW_SRC_END_ADDR_3);
    putreg32(0x85022, BAAW_REMAPPED_ADDR_3);
    putreg32(0x80000003, BAAW_ACCESS_ENABLE_OF_WINDOW_3);

    mcpu_bin_loading(TCM_BASE, (device == MCPU_CP) ? CP_BIN_BASE : GNSS_BIN_BASE, MCPU_BINARY_SIZE);

    switch (device) {
        case MCPU_CP:
            putreg32(0x01000001, 0x82000A04);   //SCMU_MR_REGISTER_A04
            putreg32(0x0, SYS_CFG_CELLULAR_GNSS_SEL);
            putreg32(0x1, PMU_SYS_ABX_PWRCTRL);
            break;
        case MCPU_GNSS:
            putreg32(0x01020001, 0x82000A04);   //SCMU_MR_REGISTER_A04
            putreg32(0x1, SYS_CFG_CELLULAR_GNSS_SEL);
            putreg32(0xe7, PMU_SYS_ABX_PWRCTRL);
            break;
        default:
#if defined (__ARMCC_VERSION)
            while (1);
#else
            asm("b .");
#endif
            break;
    }


    /* DEVICE DEPENDENT CODE!!! WRONG PALCE!!!
    Move it in Users application or device init section.
        // 4. Pin mux chagen for RF signal
        modifyreg32(0x8202110c, 0x7, 0x2);  //XGPIO3, when 0x2, PA_MODE0 is selected.
        modifyreg32(0x82021110, 0x7, 0x2);  //XGPIO4, when 0x2, PA_MODE1 is selected.
        modifyreg32(0x82021114, 0x7, 0x2);  //XGPIO5, when 0x2, PA_ON0 is selected.
        modifyreg32(0x82021118, 0x7, 0x2);  //XGPIO6, when 0x2, PA_ON1 is selected..
    */
    // 5. Buck boost turn on.
    s5js100_pmip_control_BuckBooster(1);

    // 6. mcpu run
    putreg32(0x1, SYS_CFG_MCPU_HALTN);  // PAS_ICG_DIS=0, PAS_PREQ_QUICK_STARTn=0

    S5JS100_PWR_DBG("\nMCPU BOOT\n");

    if (device == MCPU_GNSS) {
        putreg32(0x7004, 0x8202114C);
        putreg32(0x7004, 0x82021150);
        putreg32(0x7004, 0x82021154);
        putreg32(0x7004, 0x82021158);
        putreg32(0x7004, 0x8202115C);

        S5JS100_PWR_DBG("\nXANT_SW change\n");
        putreg32(0x4107, 0x82021088);
        putreg32(0x4107, 0x8202108C);
        putreg32(0x4107, 0x82021090);

        S5JS100_PWR_DBG("\nGPIO Out enable\n");
        putreg32(0x00001000, 0x85026104);
        putreg32(0xFFFFc7FF, 0x85026004);
        putreg32(0x6303, 0x8202111c);
        putreg32(0x100, 0x820009e0);

    } else {
        S5JS100_PWR_DBG("\nSetting FOR CP\n");

        putreg32(0x0, 0x8504101C);
        putreg32(0x0, 0x8504120C);
        putreg32(0x8, 0x8504120C);
        putreg32(0xf9f4f44, 0x85041208);
        putreg32(0xC, 0x8504120C);

        /* check if eFUSE ABX value available */
        if (getreg32(0x82010500) != 0x0) {
            S5JS100_PWR_DBG("ABX CAL, with EFUSE..(0x%x)\n", getreg32(0x82010500));
            putreg32(getreg32(0x82010500) & 0x3FF, 0x85041204);
            putreg32(0x09, 0x8504120C);
            S5JS100_PWR_DBG("0x8504120C=0x%x\n", getreg32(0x8504120C));
            S5JS100_PWR_DBG("ABX value : 0x%8X\n", getreg32(0x85041204));
            S5JS100_PWR_DBG("0x85041200 value : 0x%8X\n", getreg32(0x85041200));
            S5JS100_PWR_DBG("0x85041204 value : 0x%8X\n", getreg32(0x85041204));
            S5JS100_PWR_DBG("0x85041208 value : 0x%8X\n", getreg32(0x85041208));
            S5JS100_PWR_DBG("0x8504120C value : 0x%8X\n", getreg32(0x8504120C));
        } else {
            S5JS100_PWR_DBG("ABX AUTO CAL\n");
            S5JS100_PWR_DBG("Check 1st register\n");
            S5JS100_PWR_DBG("0x85041200 value : 0x%8X\n", getreg32(0x85041200));
            S5JS100_PWR_DBG("0x85041204 value : 0x%8X\n", getreg32(0x85041204));
            S5JS100_PWR_DBG("0x85041208 value : 0x%8X\n", getreg32(0x85041208));
            S5JS100_PWR_DBG("0x8504120C value : 0x%8X\n", getreg32(0x8504120C));

            putreg32(getreg32(0x85041204) | (1 << 10), 0x85041204);
            putreg32(getreg32(0x8504120C) | (1 << 2), 0x8504120C);
            //while (!(getreg32(0x85041200) & (1 << 10)));
            //wait_ms(10);
            thread_sleep_for(10);
            putreg32(getreg32(0x85041200) & 0x3FF, 0x85041204);
            if ((getreg32(0x85041204) & 0x3FF) == 0x0) {
                putreg32(0x210, 0x85041204);
            }
            S5JS100_PWR_DBG("Check 2nd register\n");
            S5JS100_PWR_DBG("ABX value : 0x%8X\n", getreg32(0x85041204));
            S5JS100_PWR_DBG("0x85041200 value : 0x%8X\n", getreg32(0x85041200));
            S5JS100_PWR_DBG("0x85041204 value : 0x%8X\n", getreg32(0x85041204));
            S5JS100_PWR_DBG("0x85041208 value : 0x%8X\n", getreg32(0x85041208));
            S5JS100_PWR_DBG("0x8504120C value : 0x%8X\n", getreg32(0x8504120C));
        }

        S5JS100_PWR_DBG("ABX CAL, DAC..\n");
        putreg32(0x0D, 0x8504120C);


        S5JS100_PWR_DBG("r_USIM0_DATA pull up enable..\n");
        modifyreg32(0x82021040, (0x3 << 11), (0x3 << 11));
    }

    modifyreg32(PMU_SYS_ABX_PWRCTRL, 0x1, 0x0);

    if (device == MCPU_CP) {
        S5JS100_PWR_DBG("PMU DAC PWR control..\n");
        modifyreg32(PMU_SYS_DAC_PWRCTRL, 0x1, 0x0);
        S5JS100_PWR_DBG("NSLEEP Enable / don't update re-calculated counter value..\n");
        modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (0x7 << 7), (0x7 << 7));
        putreg32((1u << 2) + (1u << 0) + (1u << 1), PMU_ALIVE_NSLEEP_CFG);  //    [0] :wakeup interrupt enable , [1]: sys_valid_int_en
        S5JS100_PWR_DBG("CP boot done..\n");

    }

    mcpu_device_mode = device;
}


void s5js100_sw_powerdomain_off(PWR_BLK eblock)
{
    pmu_sys_cdc_clk_en(ON);

    if (eblock & MCPU_BLK) {
        modifyreg32(PMU_SYS_ABX_PWRCTRL, 0x1, 0x1);
        modifyreg32(PMU_SYS_DAC_PWRCTRL, 0x1, 0x1);

        if (getreg32(PMU_SYS_IPRST_N)) {
            putreg32(0x0, PMU_SYS_LH_PSTATE);
            putreg32(0xf, PMU_SYS_LH_PREQ);
            while (getreg32(PMU_SYS_LH_PACCEPT) != 0xF);
            putreg32(0x0, PMU_SYS_LH_PREQ);
            while (getreg32(PMU_SYS_LH_PACCEPT) != 0x0) ;
        }
    }

    hw_delay_us(1);
    pmu_sys_asyncbus_mask(eblock);
    pmu_sys_reset(eblock, 1);
    pmu_sys_cdc_clk_en(OFF);

}

void s5js100_sw_powerdomain_on(PWR_BLK eblock)
{
    pmu_sys_cdc_clk_en(ON);

    pmu_sys_asyncbus_unmask(eblock);
    pmu_sys_reset(eblock, 0);

    if (eblock & MCPU_BLK) {
        putreg32(0xf, PMU_SYS_LH_PSTATE);
        putreg32(0xf, PMU_SYS_LH_PREQ);
        while (getreg32(PMU_SYS_LH_PACCEPT) != 0xF);
        putreg32(0x0, PMU_SYS_LH_PREQ);
        while (getreg32(PMU_SYS_LH_PACCEPT) != 0x0) ;
    }

    pmu_sys_cdc_clk_en(OFF);
}

unsigned int get_bootflag(void)
{
    return !!(getreg32(PMU_ALIVE_BOOTFLAG) & 1);
}

void alive_gpio_cfg(PORT_NUM gpio, PORT_DIR cfg)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio, (0x3 << 0), 1 << cfg);
}

void alive_gpio_set_value(PORT_NUM gpio, int value)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio, (0x1 << 5), value << 5);
}

int alive_gpio_get_value(PORT_NUM gpio)
{
    return ((getreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio) >> 4) & 0x1);
}

void gpio_set_pull(PORT_NUM gpio, PUD_CON mode)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio, (0x3 << 2), mode << 2);
}

void gpio_eint_mask(PORT_NUM gpio)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio, (1u << 9), 0x0 << 9);
}

void gpio_eint_unmask(PORT_NUM gpio)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio, (1u << 9), 0x1 << 9);
}

int gpio_eint_is_pending(PORT_NUM gpio)
{
    return ((getreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + gpio * 0x4) >> 10) & 0x1);
}

void gpio_eint_clear_pending(PORT_NUM gpio)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + gpio * 0x4, (1u << 10), 1 << 10);
}

void gpio_eint_set_filter(PORT_NUM gpio, FILTER_TYPE type)
{
    modifyreg32((uint32_t)PMU_ALIVE_GPIOALV0CTRL + 0x4 * gpio, (0x3 << 6), type << 6);
}

void sel_recalculate_clk(void)
{
    if (getreg32(PMU_ALIVE_TCXODCXOSEL) == 0x2) {
        /* keep TCXO clk source */
        modifyreg32(PMU_ALIVE_TCXODCXOSEL, (0x1 << 0), 1);
    } else {
        /* keep DCXO clk source */
    }

}

bool is_TCXO(void)
{
    if (getreg32(PMU_ALIVE_TCXODCXOSEL) & (0x1 << 1)) {
        return 1;
    } else {
        return 0;
    }
}



extern int external_pin;
static int alvgpio_interrupt(void)
{
    if (!strcmp(get_env("ALVTOGGLE"), "ON")) {

        external_pin = !external_pin;

    } else {
        //putreg8( '>' , 0x83014020);
        //putreg8( (external_pin&0xFF) + 0x30, 0x83014020);
        if (!((getreg32(0x810000f0 + alv_wkup_src * 0x4) & (1 << 4)) >> 4 ^ wkup_type)) {
            gpio_eint_set_filter(alv_wkup_src, !wkup_type);
            external_pin = 1;
        } else {
            gpio_eint_set_filter(alv_wkup_src, wkup_type);
            external_pin = 0;
        }
        //putreg8( (external_pin&0xFF) + 0x30, 0x83014020);
        //putreg8( '<' , 0x83014020);
        //putreg8( '\r' , 0x83014020);
        //putreg8( '\n' , 0x83014020);
    }
    gpio_eint_clear_pending(alv_wkup_src);
    putreg32(0x4, 0xE000E280);
    return 0;
}

void alvgpio_interrupt_enable(void)
{
    if (alv_wkup_src != GPIO_ALIVE_NONE) {
        NVIC_SetVector(S5JS100_IRQ_PMU_AlivePad, (unsigned int)alvgpio_interrupt);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
        SCB_InvalidateICache();
#endif
        gpio_eint_unmask(alv_wkup_src);
        NVIC_EnableIRQ(S5JS100_IRQ_PMU_AlivePad);
    }
}

#define GPIO_ALIVE_CTRL(x) (0x810000f0 + (x) * 0x4)
void alvgpio_init(void)
{
    int toggle = 0;
    putreg32(0x0, GPIO_ALIVE_CTRL(0));
    putreg32(0x0, GPIO_ALIVE_CTRL(1));

    // Set GPIOx to be input pull down
    modifyreg32(GPIO_ALIVE_CTRL(0), (0x1 << 0x2), (0x1 << 0x2)); //Pullup/down enable
    modifyreg32(GPIO_ALIVE_CTRL(0), (0x1 << 0x3), (0x0 << 0x3)); //Input pull-down

    modifyreg32(GPIO_ALIVE_CTRL(1), (0x1 << 0x2), (0x1 << 0x2)); //Pullup/down enable
    modifyreg32(GPIO_ALIVE_CTRL(1), (0x1 << 0x3), (0x0 << 0x3)); //Input pull-down



    modifyreg32(0x85023088, 0x1, 0x1);

    // select wakeup source
    alv_wkup_src = GPIO_ALIVE_NONE;

    if (!strcmp(get_env("ALVTOGGLE"), "ON")) {
        toggle = 1;
        wkup_type = RISING;
    } else {
        wkup_type = HIGHLEVEL;
    }

    if (!strncmp(get_env("WAKEUP"), "ALVGPIO0", 8)) {
        alv_wkup_src = GPIO_ALIVE_0;
    }
    if (!strncmp(get_env("WAKEUP"), "ALVGPIO1", 8)) {
        alv_wkup_src = GPIO_ALIVE_1;
    }

    // GpioAlv Output enable
    if (!strcmp(get_env("ALVGPIO0"), "HIGH")) {
        modifyreg32(GPIO_ALIVE_CTRL(0), 0x21, 0x21);
    }
    if (!strcmp(get_env("ALVGPIO0"), "LOW")) {
        modifyreg32(GPIO_ALIVE_CTRL(0), 0xC, 0x4);
    }
    if (!strcmp(get_env("ALVGPIO1"), "HIGH")) {
        modifyreg32(GPIO_ALIVE_CTRL(1), 0x21, 0x21);
    }
    if (!strcmp(get_env("ALVGPIO1"), "LOW")) {
        modifyreg32(GPIO_ALIVE_CTRL(1), 0xC, 0x4);
    }

    if (alv_wkup_src != GPIO_ALIVE_NONE) {

        // wkup src trigger
        if (get_env("WAKEUP")[8] == 'H') {
            wkup_type = toggle ? RISING : HIGHLEVEL;
        }
        if (get_env("WAKEUP")[8] == 'L') {
            wkup_type = toggle ? FALLING : LOWLEVEL;
        }

        // wkup src Input enable
        modifyreg32(GPIO_ALIVE_CTRL(alv_wkup_src), 0x2, 0x2);

        if (toggle) {
            gpio_eint_set_filter(alv_wkup_src, wkup_type);
        } else {
            if (!((getreg32(0x810000f0 + alv_wkup_src * 0x4) & (1 << 4)) >> 4 ^ wkup_type)) {
                gpio_eint_set_filter(alv_wkup_src, !wkup_type);
                external_pin = 1;
            } else {
                gpio_eint_set_filter(alv_wkup_src, wkup_type);
                external_pin = 0;
            }
        }
    }

    // interrupt clear at PMU_ALIVE
    gpio_eint_clear_pending(GPIO_ALIVE_0);
    gpio_eint_clear_pending(GPIO_ALIVE_1);

    // pending clear AlivePadInt
    putreg32(0x4, 0xE000E280);
}

void s5js100_pmu_init(void)
{
    unsigned int pmu_alive_slpclk_cfg = 1;

#if USE_SLOW_CLOCK_TCXO
    pmu_alive_slpclk_cfg = 0;
#else
    if (!strncmp(get_env("BOARD"), "0.2", 3)) {
        pmu_alive_slpclk_cfg = 0;
    }
#endif
    pmu_alive_slpclk_cfg = 0;

    alvgpio_init();
    alvgpio_interrupt_enable();

    if (get_bootflag() == COLD_BOOT) {
        putreg32(pmu_alive_slpclk_cfg, PMU_ALIVE_SLPCLK_CFG);

        putreg32((1u << 0) + (3u << 4) + (3u << 8) + (3u << 12) + (4u << 16) + (5u << 20) + (5u << 24) + (6u << 28), PMU_ALIVE_PMIPCTRL_ONTIMING);
        putreg32((1u << 0) + (1u << 4) + (1u << 8), PMU_ALIVE_PMIPCTRL_OFFTIMING);
        putreg32((4u << 10) + (4u << 6), PMU_ALIVE_DCXOCTRL_CFG0);

        sel_recalculate_clk();

        putreg32(0x001, PMU_ALIVE_RECALC_CFG2);
        putreg32((1u << 31) + (2u << 0), PMU_ALIVE_RECALC_CFG3);
    } else {
        putreg32(0x0, PMU_ALIVE_SYSOFF);

        putreg32(pmu_alive_slpclk_cfg, PMU_ALIVE_SLPCLK_CFG);

        if ((getreg32(PMU_ALIVE_WKUPINTSNAPSHOT) & GPIO_ALV0) > 0) {
            //putreg32((1u << 10), PMU_ALIVE_GPIOALV0CTRL);
            //putreg32((1u << 2) + (1u << 1), PMU_ALIVE_GPIOALV0CTRL);
        }
        if ((getreg32(PMU_ALIVE_WKUPINTSNAPSHOT) & GPIO_ALV1) > 0) {
            //putreg32((1u << 10), PMU_ALIVE_GPIOALV1CTRL);
            //putreg32((1u << 3) + (1u << 2) + (1u << 1), PMU_ALIVE_GPIOALV1CTRL);
        }
        if ((getreg32(PMU_ALIVE_WKUPINTSNAPSHOT) & APSLPCNT) > 0) {
            modifyreg32(PMU_ALIVE_APSLPCNT_CFG0, (1u << 3), 0 << 3);
            putreg32(0, PMU_ALIVE_APSLPCNT_CFG0);
        }
        if ((getreg32(PMU_ALIVE_WKUPINTSNAPSHOT) & NSLEEP) > 0) {
            //do what?
        }
        if ((getreg32(PMU_ALIVE_WKUPINTSNAPSHOT) & TEMPMON) > 0) {
            modifyreg32(PMU_ALIVE_TSUINT, (1u << 3), 1 << 3);
        }
    }

    putreg32(0xf21, PMU_ALIVE_PMIPCTRL_OFFTIMING);
    s5js100_sw_powerdomain_on(MCPU_BLK + RFIP_BLK);
    putreg32(0x0, PMU_SYS_PROFILERCTRL);
    putreg32((1u << 2), PMU_SYS_PROFILERCTRL);

    s5js100_pmip_initialize();

    s5js100_dcxo_initialize();

}


//#define SLPCLK      0.031941f // MHz ..26/DCXO(814)
//#define SLPCLK_DIV      814   // MHz ..26/DCXO(814)

#define SLPCLK      0.032786f   // MHz ..26/DCXO(793)
#define SLPCLK_DIV      793 // MHz ..26/DCXO(793)

void set_sleep_counter(unsigned int time_ms)
{
    unsigned int count;
    count = (unsigned int)(time_ms * SLPCLK * 1000);

    putreg32(count >> 20, PMU_ALIVE_APSLPCNT_CFG1);
    putreg32(count & 0xfffff, PMU_ALIVE_APSLPCNT_CFG2);
}

void conver_nbsleep_to_aspsleep_counter(unsigned int nbsleep_cnt)
{
    putreg32(nbsleep_cnt >> 20, PMU_ALIVE_APSLPCNT_CFG1);
    putreg32(nbsleep_cnt & 0xfffff, PMU_ALIVE_APSLPCNT_CFG2);
}


void enable_counter(void)
{
    unsigned int retry = 1000;
    if (getreg32(PMU_ALIVE_APSLPCNT_CFG0) & (0x1 << 1)) {
        //lldbg("APSLP cnt already enabled\n");
    } else {
        putreg32(0, PMU_ALIVE_APSLPCNT_CFG1);
        putreg32(1, PMU_ALIVE_APSLPCNT_CFG2);
        putreg32((15u << 6) + (1u << 3) + (0u << 2) + (1u << 1) + (1u << 0), PMU_ALIVE_APSLPCNT_CFG0);
        while (!(getreg32(PMU_ALIVE_APSLPCNT_CFG3) & (0x1 << 31) >> 31)) {
            retry--;
            if (!retry) {
                break;
            }
        }
        while (getreg32(PMU_ALIVE_APSLPCNT_CFG3) == 0x123);
        while (getreg32(PMU_ALIVE_APSLPCNT_CFG4) == 0x45678);
    }
}

void disable_counter(void)
{
    putreg32(0x0, PMU_ALIVE_APSLPCNT_CFG0);
    while (getreg32(PMU_ALIVE_APSLPCNT_CFG3) != 0x123);
    while (getreg32(PMU_ALIVE_APSLPCNT_CFG4) != 0x45678);
}

long long get_counter(void)
{
    long long current_count = 0;
    current_count = getreg32(PMU_ALIVE_APSLPCNT_CFG3) << 20;
    current_count |= getreg32(PMU_ALIVE_APSLPCNT_CFG4);
    S5JS100_PWR_DBG("H_COUNT : 0x%x ,L_COUNT : 0x%x ,\n", (unsigned int)(0xffff & (current_count >> 20)), (unsigned int)(0xfffff & current_count));
    return current_count;
}

void s5js100_pmu_sleep(PMU_SLEEP_INFO *info)
{
    PMU_SLEEP_INFO *pmu_info = (PMU_SLEEP_INFO *)((uintptr_t) info);
    unsigned int cur_temp;
    unsigned int _wkupsrc = pmu_info->wakeup_src;

    putreg32(0x0, 0xe000e010); //disable tick , not to interrupt in wfi state
    putreg32(0x02000000, 0xe000ed04); //Pend SysTick clear
    putreg32(0xFFFFFFFF, 0xe000e180); //IRQ0_31 disable
    putreg32(0xFFFFFFFF, 0xe000e184); //IRQ32_64 disable
    putreg32(0xFFFFFFFF, 0xe000e280); //IRQ0_31_PEN clear
    putreg32(0xFFFFFFFF, 0xe000e284); //IRQ32_64_PEN clear

    putreg32(0, PMU_ALIVE_SLPCLK_CFG);  //SwMuxSel=0 1: SwSel selects sleep clock mux

    //disable SMC clockgating
    putreg32(0x200000, MIFCMU_CLK_CON_GAT_GOUT_MIF_UID_SMC_IPCLKPORT_CLK);
    putreg32(0x200000, MIFCMU_CLK_CON_GAT_GOUT_MIF_UID_SMC_IPCLKPORT_L2CLK);

    /*
     * (0) sleep or not
     *
     * Before sleep, SW must check sleep counters
     * if sleep counter reaches 0 soon, :if expand("%") == ""|browse confirm w|else|confirm w|endif
     * SW should skip sleep and just waits for wakeup interrupt
     *
     * (1) SW-controlled Power Domain Off
     *  SW domain power off done
     */
#if defined(__ICCARM__)
#warning "no support s5js100_sw_powerdomain_off"
#else
    s5js100_sw_powerdomain_off(RFIP_BLK + MCPU_BLK);
#endif
    S5JS100_PWR_DBG("1)SW domain power off\n");
    /*
    * PMU, CMU initial setting
    */
    putreg32(0x3, PMU_ALIVE_PROFILERCTRL);  // BlackboxClr before sleep, BlackboxEn=1
    putreg32(0x3, PMU_SYS_PROFILERCTRL);    // BlackboxClr before sleep, BlackboxEn=1
    putreg32((1u << 30) + (2u << 26) + (6u << 22), PMU_ALIVE_ALVSYSCTRLCFG);    //CntLmtPWR_OFF_MIN=1,CntLmtPWR_STABLE=2(fast sim), CntLmtCKCHG=6
    //   PMU_ALIVE_AlvSysCtrlCfg = (1u<<30)+(2u<<26)+(6u<<22)+(1u<<8);//CntLmtPWR_OFF_MIN=1,CntLmtPWR_STABLE=2(fast sim), CntLmtCKCHG=6, ReCalcUpDis=1
    S5JS100_PWR_DBG("2)PMU, CMU initial setting\n");

    putreg32((1u << 8) + (2u << 4), PMU_ALIVE_TEMPMON_CFG0);    //CntLmt_INTERVAL=1, CntLmt_TSU_CLK=2
    cur_temp = get_temp();
    S5JS100_PWR_DBG("2.1)current temperature : 0x%x\n", cur_temp);
    putreg32(cur_temp, PMU_ALIVE_TEMPMON_CFG1);
    putreg32(7, PMU_ALIVE_TEMPMON_CFG2);    //Delta=30
    // PMU_ALIVE_TsuInt = (1u<<0) + (1u<<1); //UpIntEn=1, LowIntEn=1 : tested in TempMon wakeup
    putreg32(30, PMU_ALIVE_RECALC_CFG1);    //RcoRefCnt=3 ==> (30*4 + 1)ea RCO counted in RCO_TIMER_COMPENSATION
    putreg32((SLPCLK_DIV * ((30 * 4) + 1)), PMU_ALIVE_RECALC_CFG0); //InitialDcxoRefCnt = (DCXO/814)*121

    /*
     * Wakeup source selection
     */
    S5JS100_PWR_DBG("3)Wakeup source selection\n");

    putreg32(_wkupsrc, PMU_ALIVE_WKUPSRCEN);    // wakeup source enable
    // it is just for simulation.
    // in real SW, they are all enabled.

    if ((_wkupsrc & GPIO_ALV0) > 0) {   //active high
        gpio_eint_clear_pending(GPIO_ALIVE_0);
        putreg32(0x4, 0xE000E280);
    }

    if ((_wkupsrc & GPIO_ALV1) > 0) {   //active low, pulse type
        gpio_eint_clear_pending(GPIO_ALIVE_1);
        putreg32(0x4, 0xE000E280);
    }

    if ((_wkupsrc & APSLPCNT) > 0) {    // AP sleep counter setting

        putreg32((1u << 5), PMU_ALIVE_APSLPCNT_CFG0);   //5 : interrupt pending clear  , 0: SW reset.
        set_sleep_counter(pmu_info->time_msec);
        S5JS100_PWR_DBG("PMU_ALIVE_APSLPCNT_CFG1 : 0x%x\n", *(unsigned int *)PMU_ALIVE_APSLPCNT_CFG1);
        S5JS100_PWR_DBG("PMU_ALIVE_APSLPCNT_CFG2 : 0x%x\n", *(unsigned int *)PMU_ALIVE_APSLPCNT_CFG2);
        putreg32((15u << 6) + (1u << 3) + (0u << 2) + (1u << 1) + (1u << 0), PMU_ALIVE_APSLPCNT_CFG0);
    }

    if ((_wkupsrc & NSLEEP) > 0) {
        modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (0x7 << 7), (0x7 << 7));
        putreg32((1u << 2) + (1u << 0) + (1u << 1), PMU_ALIVE_NSLEEP_CFG);

    }

    if ((_wkupsrc & TEMPMON) > 0) {
        putreg32(((cur_temp + pmu_info->upper_temp) << 16) + ((cur_temp - pmu_info->lower_temp) << 0), PMU_ALIVE_TEMPMON_CFG3);
        putreg32((1u << 0) + (1u << 1), PMU_ALIVE_TSUINT);

    }

    S5JS100_PWR_DBG("PD CDC clock on \n");

    // SMC LPI lock
    putreg32((1u << 1), PMU_SYS_DCGOVRD);
    putreg32(0, PMU_SYS_SMC_LPI);
    while ((getreg32(PMU_SYS_SMC_LPI) & 0x6) != 0x0) ;
    putreg32(0x0, PMU_SYS_DCGOVRD);
    S5JS100_PWR_DBG("PD CDC clock off \n");
    putreg32(0x1, PMU_ALIVE_SYSOFF);
    putreg32(0x1, PMU_SYS_SYSCTRL_SYSOFF);

    while ((getreg32(0x85024018) >> 3) & 0x1) ;

    modifyreg32(PMU_ALIVE_PMIP_TCXO_LDO, 0x80000000, 0x0);

    // PMU STAUTS to be WARM BOOT
    putreg32(WAKEUP_BOOT, PMU_ALIVE_BOOTFLAG);
#if defined( __GNUC__ )
    asm("dmb");
    asm("wfi");
#else
    __DMB();
    __WFI();
#endif

    while (1) ;
}

void pmu_nb_sleep(void)
{
    PMU_SLEEP_INFO pmu_info;

    pmu_info.wakeup_src = NSLEEP;
    if (alv_wkup_src == GPIO_ALIVE_0) {
        pmu_info.wakeup_src |= GPIO_ALV0;
    }
    if (alv_wkup_src == GPIO_ALIVE_1) {
        pmu_info.wakeup_src |= GPIO_ALV1;
    }

    pmu_info.alv0_gpio_int_type = wkup_type;
    s5js100_pmu_sleep((PMU_SLEEP_INFO *)&pmu_info);
}

void __attribute__((section(".ramfunc"))) s5js100_sflash_deepsleep(int on)
{
    volatile unsigned int count = 2000;

    if (on) {
        modifyreg32(0x85020024, 0xFF000000, 0xB9000000);
        putreg8(1, 0x8502005A);
    } else {
        modifyreg32(0x85020024, 0xFF000000, 0xAB000000);
        putreg8(1, 0x8502005A);
    }

    while (count--) {
#if defined( __GNUC__ )
        asm("nop");
#else
        __NOP();
#endif
    }
}
#if !defined(__ICCARM__)
void __attribute__((section(".ramfunc"))) s5js100_enter_exit_idle_drx(void)
{
#if defined( __GNUC__ )
    asm("isb");
    asm("dsb");
    s5js100_sflash_deepsleep(1);
    asm("dmb");
    asm("wfi");
    s5js100_sflash_deepsleep(0);
    asm("isb");
    asm("dsb");
#else
    __ISB()
    __DSB();
    s5js100_sflash_deepsleep(1);
    __DMB();
    __WFI();
    s5js100_sflash_deepsleep(0);
    __ISB()
    __DSB();
#endif
}
#endif

#if !defined(__ICCARM__)
void __attribute__((section(".ramfunc"))) pmu_sleep_isr(void)
{
#if defined(__ARMCC_VERSION)
    while (1);
#else
    asm("b	.");
#endif
}
#endif


char nbsleep_msg[] = "NBSLEEP CNT LSB=>";
char ldo6_on[] = "LDO6 ON";
char ldo6_off[] = "LDO6 OFF";


/****************** Option Table ***********************/
//#define PRINT_NBSLEEP_LOG
#define LDO_PWR_ON          0

// possible option : 1/1/1, 1/0/1, 1/1/0, 1/0/0, 0/0/0
#define DCXO_IP_ON          0
#define BUS_CLK_26MHZ       0
#define SLEEP_CLK_32KHZ     0

#define PLL_OFF_OSC     //always on
//#define LDO6_PWR_ALWAYS_ON

#define LDO1_ON     0
#define LDO2_ON     0
#define LDO4_ON     0
/*******************************************************/
extern int cal_init(void);
extern int s5js100_dcxo_force_initialize(void);
extern void s5js100_sflash_reinit(void);

/* BASE : condition 3               */
/*      default - DCXO_IP_ON        */
/*              - BUS Clock 26MHz   */
/*              - Sleep Clock 32Khz */
//void __attribute__((section(".ramfunc"))) pmu_short_sleep(void)
void pmu_short_sleep(void)
{
    unsigned int i;
    PMU_SLEEP_INFO pmu_info;
    unsigned int nvic[8];
#ifdef PRINT_NBSLEEP_LOG
    char __buffer[12];
#endif

#ifdef PRINT_NBSLEEP_LOG
    convert2hex(__buffer, getreg32(0x81000000 + 0xE8));
    direct_uart(nbsleep_msg, sizeof(nbsleep_msg));
    direct_uart(__buffer, sizeof(__buffer));
#endif

    pmu_info.wakeup_src = NSLEEP;
    if (alv_wkup_src == GPIO_ALIVE_0) {
        pmu_info.wakeup_src |= GPIO_ALV0;
    }
    if (alv_wkup_src == GPIO_ALIVE_1) {
        pmu_info.wakeup_src |= GPIO_ALV1;
    }

    //BUCK1, LDO0 on
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 0) + (1u << 1) + (1u << 7) + (1u << 8), (1u << 0) + (1u << 1) + (0 << 7) + (0 << 8));

    //BUCK0, LDO3 on
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 4) + (1u << 6) + (1u << 11) + (1u << 13), (1u << 4) + (1u << 6) + (0 << 11) + (0 << 13));

    /* LDO 1/2/4 ON/OFF */
    // LDO1 ON/OFF
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 2), LDO1_ON << 2);
    // LDO2 ON/OFF
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 3), LDO2_ON << 3);
    // LDO4 ON/OFF
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 5), LDO4_ON << 5);

    pmu_sys_cdc_clk_en(ON);        //CDC ON

    putreg32((1u << 30) + (2u << 26) + (6u << 22), PMU_ALIVE_ALVSYSCTRLCFG);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, 0x3ff, 0x3ff);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, 1u << 6, 0);    // do not enable clamp cells

    // Do PMIP off
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, 1u << 3, LDO_PWR_ON << 3);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (1u << 2), (DCXO_IP_ON << 2)); //DCXO_IP
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (1u << 1), (BUS_CLK_26MHZ << 1)); //BUS Clock 26MHZ
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (1u << 0), (SLEEP_CLK_32KHZ << 0)); //sleep_clock 32KHz

    putreg32((1u << 1) + (1u << 0), PMU_ALIVE_PROFILERCTRL); //BlackboxClr=1, BlackboxEn=1
    putreg32((1u << 1) + (1u << 0), PMU_SYS_PROFILERCTRL); //BlackboxClr=1, BlackboxEn=1

    putreg32((1u << 1), PMU_SYS_ACPUCTRL_CFG0); //EfuseSenseReqDis=1

    putreg32(pmu_info.wakeup_src, PMU_ALIVE_WKUPSRCEN);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (0x7 << 7), (0x7 << 7));//    [0] :wakeup interrupt enable , [1]: sys_valid_int_en
    putreg32((1u << 2) + (1u << 0) + (1u << 1), PMU_ALIVE_NSLEEP_CFG);//    [0] :wakeup interrupt enable , [1]: sys_valid_int_en

    if ((pmu_info.wakeup_src & APSLPCNT) > 0) { // AP sleep counter setting
        putreg32((1u << 5), PMU_ALIVE_APSLPCNT_CFG0);//5 : interrupt pending clear  , 0: SW reset.
        set_sleep_counter(pmu_info.time_msec);
        putreg32((15u << 6) + (1u << 3) + (0u << 2) + (1u << 1) + (1u << 0), PMU_ALIVE_APSLPCNT_CFG0);  //IgnoreReCalcLmt=15,IntMask_n=1,ReCalcUpdateMask=0,En=1,SwRst_n=1
    }

    for (i = 0; i < 8; i++) {
        nvic[i] = getreg32(0xE000E100 + (0x4 * i));
        putreg32(nvic[i], 0xE000E180 + (0x4 * i));//disable all interrupt
    }

    NVIC_EnableIRQ(S5JS100_IRQ_SLEEP); //NBSLEEP
    alvgpio_interrupt_enable();

    while (getreg32(PMU_SYS_MCPUMON) != 1); // waits for MCPU WFI

    putreg32(0x4, 0xE000E280);
    while (getreg32(0xE000E200 + 0x4) & (0x80)) {
        putreg32(0x80, 0xE000E284);//clear SLEEP pending, should be clear by CP first
    }
#ifdef PRINT_NBSLEEP_LOG
    convert2hex(__buffer, getreg32(PMU_ALIVE_ALVSYSCTRLCFG));
    direct_uart(__buffer, sizeof(__buffer));
#endif

    // PMU could control on/off of LDO6 if LDO6 SwMuxSel was zero.
    // PMU power off LDO6
    modifyreg32(PMU_ALIVE_PMIP_TCXO_LDO, 0x80000000, 0x0);

    *(volatile unsigned *)(0x82000100) = 0x018c0d01; //system mux change PLL -> DCXO, PLL off

    putreg32(0x1, PMU_ALIVE_SYSOFF);
    putreg32(0x1, PMU_SYS_SYSCTRL_SYSOFF);
    putreg32(1, PMU_ALIVE_BOOTFLAG);
#if defined(__ICCARM__)
#warning "ICCARM no support idle drx"
#else
    s5js100_enter_exit_idle_drx(); //cpu wfi
#endif
    while (getreg32(PMU_SYS_ACPUCTRL_FSM) != 0x14151600 && getreg32(PMU_SYS_ACPUCTRL_FSM) != 0);

    /* DCXO ON */
    if (!(DCXO_IP_ON)) {
        s5js100_dcxo_force_initialize();    /* DCXO off -> on after short_sleep */
    }

    cal_init();
    s5js100_sflash_reinit();

    putreg32(0x0, PMU_ALIVE_SYSOFF);
    putreg32(0x0, PMU_SYS_SYSCTRL_SYSOFF);

    /* AP2CP mailbox interrupt generation */
    /* MCPU back to work                  */
    putreg32(0x3F80, PMU_ALIVE_PMIPCTRL_OVRD);

    //wait MCPU wfi break
    //   while(PMU_SYS_McpuMon == 0x1);
    putreg32(0x2, 0x85023020);
    putreg32(0x2, 0x8502301C);

    for (i = 0; i < 8; i++) {
        putreg32(nvic[i], 0xE000E100 + (0x4 * i));  //enable all interrupt
    }

    NVIC_DisableIRQ(S5JS100_IRQ_SLEEP);

    pmu_sys_cdc_clk_en(OFF);        //CDC OFF
    gpio_eint_mask(alv_wkup_src);   //IntEn=1
    alvgpio_interrupt_enable();
}

void pmu_ap_short_sleep(int sleep_time_msec)
{
    unsigned int i;
    PMU_SLEEP_INFO pmu_info;
    unsigned int nvic[8];
#ifdef PRINT_NBSLEEP_LOG
    char __buffer[12];
#endif

#ifdef PRINT_NBSLEEP_LOG
    convert2hex(__buffer, getreg32(0x81000000 + 0xE8));
    direct_uart(nbsleep_msg, sizeof(nbsleep_msg));
    direct_uart(__buffer, sizeof(__buffer));
#endif

    pmu_info.wakeup_src = APSLPCNT;
    pmu_info.time_msec = sleep_time_msec;

    //BUCK1, LDO0 on
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 0) + (1u << 1) + (1u << 7) + (1u << 8), (1u << 0) + (1u << 1) + (0 << 7) + (0 << 8));

    //BUCK0, LDO3 on
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 4) + (1u << 6) + (1u << 11) + (1u << 13), (1u << 4) + (1u << 6) + (0 << 11) + (0 << 13));

    /* LDO 1/2/4 ON/OFF */
    // LDO1 ON/OFF
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 2), LDO1_ON << 2);
    // LDO2 ON/OFF
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 3), LDO2_ON << 3);
    // LDO4 ON/OFF
    modifyreg32(PMU_ALIVE_PMIPCTRL_OVRD, (1u << 5), LDO4_ON << 5);

    pmu_sys_cdc_clk_en(ON);        //CDC ON

    putreg32((1u << 30) + (2u << 26) + (6u << 22), PMU_ALIVE_ALVSYSCTRLCFG);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, 0x3ff, 0x3ff);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, 1u << 6, 0);    // do not enable clamp cells

    // Do PMIP off
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, 1u << 3, LDO_PWR_ON << 3);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (1u << 2), (DCXO_IP_ON << 2)); //DCXO_IP
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (1u << 1), (BUS_CLK_26MHZ << 1)); //BUS Clock 26MHZ
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (1u << 0), (SLEEP_CLK_32KHZ << 0)); //sleep_clock 32KHz

    putreg32((1u << 1) + (1u << 0), PMU_ALIVE_PROFILERCTRL); //BlackboxClr=1, BlackboxEn=1
    putreg32((1u << 1) + (1u << 0), PMU_SYS_PROFILERCTRL); //BlackboxClr=1, BlackboxEn=1

    putreg32((1u << 1), PMU_SYS_ACPUCTRL_CFG0); //EfuseSenseReqDis=1

    putreg32(pmu_info.wakeup_src, PMU_ALIVE_WKUPSRCEN);
    modifyreg32(PMU_ALIVE_ALVSYSCTRLCFG, (0x7 << 7), (0x7 << 7));//    [0] :wakeup interrupt enable , [1]: sys_valid_int_en
    putreg32((1u << 2) + (1u << 0) + (1u << 1), PMU_ALIVE_NSLEEP_CFG);//    [0] :wakeup interrupt enable , [1]: sys_valid_int_en

    if ((pmu_info.wakeup_src & APSLPCNT) > 0) { // AP sleep counter setting
        putreg32((1u << 5), PMU_ALIVE_APSLPCNT_CFG0);//5 : interrupt pending clear  , 0: SW reset.
        set_sleep_counter(pmu_info.time_msec);
        putreg32((15u << 6) + (1u << 3) + (0u << 2) + (1u << 1) + (1u << 0), PMU_ALIVE_APSLPCNT_CFG0);  //IgnoreReCalcLmt=15,IntMask_n=1,ReCalcUpdateMask=0,En=1,SwRst_n=1
    }

    for (i = 0; i < 8; i++) {
        nvic[i] = getreg32(0xE000E100 + (0x4 * i));
        putreg32(nvic[i], 0xE000E180 + (0x4 * i));//disable all interrupt
    }
#if 0
    NVIC_EnableIRQ(S5JS100_IRQ_SLEEP); //NBSLEEP
    alvgpio_interrupt_enable();

    while (getreg32(PMU_SYS_MCPUMON) != 1); // waits for MCPU WFI
#endif

    putreg32(0x4, 0xE000E280);
    while (getreg32(0xE000E200 + 0x4) & (0x80)) {
        putreg32(0x80, 0xE000E284);//clear SLEEP pending, should be clear by CP first
    }
#ifdef PRINT_NBSLEEP_LOG
    convert2hex(__buffer, getreg32(PMU_ALIVE_ALVSYSCTRLCFG));
    direct_uart(__buffer, sizeof(__buffer));
#endif

    // PMU could control on/off of LDO6 if LDO6 SwMuxSel was zero.
    // PMU power off LDO6
    modifyreg32(PMU_ALIVE_PMIP_TCXO_LDO, 0x80000000, 0x0);

    *(volatile unsigned *)(0x82000100) = 0x018c0d01; //system mux change PLL -> DCXO, PLL off

    putreg32(0x1, PMU_ALIVE_SYSOFF);
    putreg32(0x1, PMU_SYS_SYSCTRL_SYSOFF);
    putreg32(1, PMU_ALIVE_BOOTFLAG);
#if defined(__ICCARM__)
#warning "ICCARM no support idle drx"
#else
    s5js100_enter_exit_idle_drx(); //cpu wfi
#endif
    while (getreg32(PMU_SYS_ACPUCTRL_FSM) != 0x14151600 && getreg32(PMU_SYS_ACPUCTRL_FSM) != 0);

    /* DCXO ON */
    if (!(DCXO_IP_ON)) {
        s5js100_dcxo_force_initialize();    /* DCXO off -> on after short_sleep */
    }

    cal_init();
    s5js100_sflash_reinit();

    putreg32(0x0, PMU_ALIVE_SYSOFF);
    putreg32(0x0, PMU_SYS_SYSCTRL_SYSOFF);

    /* AP2CP mailbox interrupt generation */
    /* MCPU back to work                  */
    putreg32(0x3F80, PMU_ALIVE_PMIPCTRL_OVRD);

    //wait MCPU wfi break
    //   while(PMU_SYS_McpuMon == 0x1);
    putreg32(0x2, 0x85023020);
    putreg32(0x2, 0x8502301C);

    for (i = 0; i < 8; i++) {
        putreg32(nvic[i], 0xE000E100 + (0x4 * i));  //enable all interrupt
    }

    NVIC_DisableIRQ(S5JS100_IRQ_SLEEP);

    pmu_sys_cdc_clk_en(OFF);        //CDC OFF
    gpio_eint_mask(alv_wkup_src);   //IntEn=1
    alvgpio_interrupt_enable();
}



void pmu_test_auto(int sleep_time_msec)
{
    PMU_SLEEP_INFO pmu_info;

    S5JS100_PWR_DBG("%s...\n", __func__);
    printf("\r\n%s...%d\n", __func__, sleep_time_msec);

    pmu_info.wakeup_src = APSLPCNT;
    pmu_info.time_msec = sleep_time_msec;
    s5js100_pmu_sleep((PMU_SLEEP_INFO *)&pmu_info);
}

unsigned int get_wakeup_src_status(void)
{
    return getreg32(PMU_ALIVE_WKUPINTSNAPSHOT);
}

void mcpu_reset_dump(void)
{
    mcpu_reset();
    s5js100_pmu_init();
    *((unsigned int *)(TCM_BASE + 0x1800 - 0x40)) = 0x55500000;
    SCB_CleanDCache();
    putreg32(0x1, SYS_CFG_MCPU_HALTN);
}

