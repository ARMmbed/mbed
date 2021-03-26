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

#include "rtos/rtos.h"
#include "s5js100.h"
#include "s5js100_dcxo.h"
#include "s5js100_type.h"
#include "mbed_trace.h"
#define TRACE_GROUP "DCXO"

#ifndef S5JS100_DCXO_DBG_ON
#define S5JS100_DCXO_DBG_ON     0
#endif
#define S5JS100_DCXO_DBG        if (S5JS100_DCXO_DBG_ON) tr_info

#define DCXO_TEMPERATURE_RANGE  126 /* -40 ~ 85 */
#define DCXO_TSU_TABLE_MIN_TEMPERATURE (-40)
#define DCXO_TSU_TABLE_MAX_TEMPERATURE  85
#define DCXO_TSU_TABLE_MIN              10000
#define DCXO_TSU_TABLE_LENGTH           35
#define DCXO_TSU_TABLE_UNIT             5000

Thread *g_dcxo_tid;
static Semaphore dcxo_tbl_sem(1, 1);

static unsigned int tsu_code_to_temperature(unsigned int tsu_code);
static unsigned int tsu_temperature_to_code(unsigned int temp_celcius);

// table for KDS 9pF and SIDK board
// B constant : 3,380, T0 : 25'C
static const unsigned short g_dcxo_tsu_table[] = {
    49690,  //-40'C, 10,000
    48942,
    48194,  //-30'C, 20,000
    47446,
    46698,  //-20'C, 30,000
    45639,
    44316,  //-10'C, 40,000
    42776,
    41063,  //0'C, 50,000
    39214,
    37269,  //10'C, 60,000
    35261,
    33221,  //20'C, 70,000
    31177,
    29156,  //30'C, 80,000
    27180,
    25267,  //40'C, 90,000
    23436,
    21700,  //50'C, 100,000
    20069,
    18553,  //60'C, 110,000
    17155,
    15878,  //70'C, 120,000
    14721,
    13681,  //80'C, 130,000
    12751,
    11922,  //90'C, 140,000
    11181,
    10513,  //100'C, 150,000
    9899,
    9319,   //110'C, 160,000
    8749,
    8162,   //120'C, 170,000
    7527,
    6813,   //130'C, 180,000
};

static volatile int tsu_interrupt_flag;
static int dcxo_init_flag;
static Full_set_table_t *dcxo_tbl;

// return value : (celcius temperature)*1000 + 50,000
static unsigned int tsu_code_to_temperature(unsigned int tsu_code)
{
    unsigned int temp_code;
    int index, base, ratio;

    index = 0;

    while (index < DCXO_TSU_TABLE_LENGTH) {
        if (g_dcxo_tsu_table[index] <= tsu_code) {
            break;
        } else {
            index++;
        }
    }

    if (index <= 0) {
        index = 1;
    } else if (index >= DCXO_TSU_TABLE_LENGTH) {
        index = DCXO_TSU_TABLE_LENGTH - 1;
    }

    base = (index - 1) * DCXO_TSU_TABLE_UNIT + DCXO_TSU_TABLE_MIN;
    ratio = ((int)(g_dcxo_tsu_table[index - 1] - tsu_code) * DCXO_TSU_TABLE_UNIT)
            / (int)(g_dcxo_tsu_table[index - 1] - g_dcxo_tsu_table[index]);
    temp_code = base + ratio;
    return temp_code;
}

// temp_celcius : (celcius temperature)*1000 + 50,000
static unsigned int tsu_temperature_to_code(unsigned int temp_celcius)
{
    int index, ratio;

    index = (temp_celcius - DCXO_TSU_TABLE_MIN) / DCXO_TSU_TABLE_UNIT;

    if (temp_celcius <= DCXO_TSU_TABLE_MIN) {
        index = 0;
        ratio = (int)(DCXO_TSU_TABLE_MIN + DCXO_TSU_TABLE_UNIT * index - temp_celcius) * (int)(g_dcxo_tsu_table[index] - g_dcxo_tsu_table[index + 1]) / DCXO_TSU_TABLE_UNIT;
    } else if (index >= DCXO_TSU_TABLE_LENGTH - 1) {
        index = DCXO_TSU_TABLE_LENGTH - 1;
        ratio = (int)(DCXO_TSU_TABLE_MIN + DCXO_TSU_TABLE_UNIT * index - temp_celcius) * (int)(g_dcxo_tsu_table[index - 1] - g_dcxo_tsu_table[index]) / DCXO_TSU_TABLE_UNIT;
    } else {
        ratio = (int)(DCXO_TSU_TABLE_MIN + DCXO_TSU_TABLE_UNIT * index - temp_celcius) * (int)(g_dcxo_tsu_table[index] - g_dcxo_tsu_table[index + 1]) / DCXO_TSU_TABLE_UNIT;
    }

    //dbg("\nindex : %d , base : %d , ratio : %d\n", index, g_dcxo_tsu_table[index], ratio);

    return g_dcxo_tsu_table[index] + ratio;
}

/****************************************************************************
 * Function:  up_timerisr
 *
 * Description:
 *   The timer ISR will perform a variety of services for various portions
 *   of the systems.
 *
 ****************************************************************************/
int dcxo_tsu_isr(int irq, void *context, void *arg)
{
    NVIC_DisableIRQ((IRQn_Type)irq);

    tsu_interrupt_flag = 1;

    if (irq == S5JS100_IRQ_TSU0) {
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 1, 1);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 0x10, 0x10);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0, 0xFFFF, 0x0);
        //dbg("TSU0 interrupt\n");
    } else if (irq == S5JS100_IRQ_TSU1) {
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 2, 2);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 0x20, 0x20);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0, 0xFFFF0000, 0xFFFF0000);
        //dbg("TSU1 interrupt\n");
    }

    return 0;
}



/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
/****************************************************************************
 * Name: s5js100_dcxo_ctb_loop
 *
 * Description:
 * update DCXO value in every ms by parameter freq
 *
 * Returned Value:
 *   a NULL on failure
 *
 ****************************************************************************/
int s5js100_dcxo_tbl_dump(void)
{
    int i;
    for (i = 0; i < DCXO_TEMPERATURE_RANGE; i++) {
        S5JS100_DCXO_DBG("%d : CTB[%d]", i - 40, (dcxo_tbl + i)->ctb);
    }
    return 0;
}

void dcxo_ctb_sem_wait(void)
{
    dcxo_tbl_sem.try_acquire();
}

void dcxo_ctb_sem_release(void)
{
    dcxo_tbl_sem.release();
}

void s5js100_dcxo_ctb_loop(void)
{
    int tsu, index, ctb, ctb_prev;
    int ctb_gap;
    int loop_started = 0;

    //s5js100_dcxo_tbl_dump();

    while (1) {
        // refer dcxo table
        // read TSU
        tsu = (int) s5js100_tsu_get_temperature();
        index = (tsu - 10000) / 1000;

        dcxo_ctb_sem_wait();

        if (tsu < (50000 + DCXO_TSU_TABLE_MIN_TEMPERATURE * 1000)) {
            index = 0;
            ctb_gap = dcxo_tbl[index + 1].ctb - dcxo_tbl[index].ctb;
            ctb = dcxo_tbl[index].ctb + (ctb_gap * (tsu - (50000 + DCXO_TSU_TABLE_MIN_TEMPERATURE * 1000))) / 1000;
        } else if (tsu > (50000 + DCXO_TSU_TABLE_MAX_TEMPERATURE * 1000)) {
            index = DCXO_TEMPERATURE_RANGE - 1;
            ctb_gap = dcxo_tbl[index].ctb - dcxo_tbl[index - 1].ctb;
            ctb = dcxo_tbl[index].ctb + (ctb_gap * (tsu - (50000 + DCXO_TSU_TABLE_MAX_TEMPERATURE * 1000))) / 1000;
        } else {
            ctb_gap = dcxo_tbl[index + 1].ctb - dcxo_tbl[index].ctb;
            ctb = dcxo_tbl[index].ctb + ctb_gap * (tsu % 1000) / 1000;
        }

        if (ctb < 0) {
            ctb = 0;
        } else if (ctb > 16383) {
            ctb = 16383;
        }

        if (!loop_started) {
            S5JS100_DCXO_DBG("s5js100_dcxo_ctb_loop FIRST update temp(%d) coarse(%d -> %d)", tsu - 50000, s5js100_dcxo_get_coarse_tune_value, ctb);
            s5js100_dcxo_set_tune_value(ctb, 4096);
        } else {
            ctb_prev = s5js100_dcxo_get_coarse_tune_value();

            if (ctb > ctb_prev) {
                ctb_prev++;
            } else if (ctb < ctb_prev) {
                ctb_prev--;
            }
            s5js100_dcxo_set_tune_value(ctb_prev, 4096);
        }

        if (ctb == 0) {
            S5JS100_DCXO_DBG("[ERROR]CTB was 0x0[temp:%d]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", tsu);
        }

        loop_started = 1;
        dcxo_ctb_sem_release();
        osDelay(50);
    }
}

int s5js100_tsu_get_code(void)
{
    return (getreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_STA0) & 0xFFFF);
}

int s5js100_dcxo_force_update_ctb(void)
{
    int tsu, index, ctb, code;
    int ctb_gap;

    // read TSU
    code = s5js100_tsu_get_code();
    tsu = (int) tsu_code_to_temperature(code);

    index = (tsu - 10000) / 1000;

    if (tsu < (50000 + DCXO_TSU_TABLE_MIN_TEMPERATURE * 1000)) {
        index = 0;
        ctb_gap = dcxo_tbl[index + 1].ctb - dcxo_tbl[index].ctb;
        ctb = dcxo_tbl[index].ctb + (ctb_gap * (tsu - (50000 + DCXO_TSU_TABLE_MIN_TEMPERATURE * 1000))) / 1000;
    } else if (tsu > (50000 + DCXO_TSU_TABLE_MAX_TEMPERATURE * 1000)) {
        index = DCXO_TEMPERATURE_RANGE - 1;
        ctb_gap = dcxo_tbl[index].ctb - dcxo_tbl[index - 1].ctb;
        ctb = dcxo_tbl[index].ctb + (ctb_gap * (tsu - (50000 + DCXO_TSU_TABLE_MAX_TEMPERATURE * 1000))) / 1000;
    } else {
        ctb_gap = dcxo_tbl[index + 1].ctb - dcxo_tbl[index].ctb;
        ctb = dcxo_tbl[index].ctb + ctb_gap * (tsu % 1000) / 1000;
    }

    if (ctb < 0) {
        ctb = 0;
    } else if (ctb > 16383) {
        ctb = 16383;
    }

    s5js100_dcxo_set_tune_value(ctb, 4096);

    if (ctb == 0) {
        S5JS100_DCXO_DBG("Force CTB was 0x0[temp:%d, code:%d]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", tsu, code);
        //asm("b .");
    }

    if ((tsu - 50000) > 0) {
        //dcxollvdbg("TSU : %d.%03d 'C, CTB : %d ( %d )\n", (tsu - 50000) / 1000, (tsu) % 1000, ctb, tsu);
    } else if ((tsu - 50000) > -1000) {
        //dcxollvdbg("TSU : -%d.%03d 'C, CTB : %d ( %d )\n", (tsu - 50000) / 1000, (1000 - (tsu) % 1000) % 1000, ctb, tsu);
    } else {
        //dcxollvdbg("TSU : %d.%03d 'C, CTB : %d ( %d )\n", (tsu - 50000) / 1000, (1000 - (tsu) % 1000) % 1000, ctb, tsu);
    }

    return 0;
}




/****************************************************************************
 * Name: s5js100_dcxo_initialize
 *
 * Description:
 *   Initialize the DCXO.
 *
 * Returned Value:
 *   a NULL on failure
 *
 ****************************************************************************/
int s5js100_dcxo_initialize(void)
{
    if (dcxo_init_flag == 0) {
        // initialize DOTSU, clk:100kHz
        //putreg32(0x3E5, S5JS100_DCXO_CFG + DCXO_CFG_ADC_CFG);
        //putreg32(0x1BE7, S5JS100_DCXO_CFG + DCXO_CFG_ADC_CFG); //ATE setting value
        putreg32(0x13FE5, S5JS100_DCXO_CFG + DCXO_CFG_ADC_CFG); //Final setting value
        putreg32(0x3A, S5JS100_DCXO_CFG + DCXO_CFG_DIV_CFG);
        putreg32(0x30, S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1);
        putreg32(0xFFFF0000, S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0);

        // tsu sensing time by average unit
        // 0:10us, 1:40us, 2:160us, 3:640us, 4:2.56ms, 5:10.24ms, 6:81.92ms, 7:327.68 ms
        putreg32(0x3, S5JS100_DCXO_CFG + DCXO_CFG_AVR_CFG);
        //putreg32(0x7, S5JS100_DCXO_CFG + DCXO_CFG_AVR_CFG);

        // wait till update temperature
        while (!(getreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_STA1) & 0xFFFF0000)) {
        }

        // initialize DCXO
        // IDAC_SEL = 1
        modifyreg32(S5JS100_PMU_ALIVE + 0x520, 1 << 3, 1 << 3);

#if 0
        // IDAC_EX = 0xa0
        modifyreg32(S5JS100_PMU_ALIVE + 0x524, 0xff << 5, 0xa0 << 5);
#else
        // change tune parameter
        // IDAC_EX = 100
        modifyreg32(S5JS100_PMU_ALIVE + 0x524, 0xff << 5, 100 << 5);

        // BUF_SEL = 8
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0xf << 3, 8 << 3);
#endif

#if defined(DCXO_FINE_PMU_COARSE_DCXO)
        // Fine=FTUNE2, Coarse=DcxoFTune
        // FuneMuxSel = 1
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 1, 1);

        // FTSEL = 1
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 1 << 16, 1 << 16);

        // FTSWP = 1
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 1 << 15, 1 << 15);

#elif defined(DCXO_FINE_PMU_COARSE_PMU)
        // Fine=FTUNE2, Coarse=FTUNE
        // FuneMuxSel = 0
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 1, 0);

        // FTSEL = 1
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 1 << 16, 1 << 16);

        // FTSWP = 1
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 1 << 15, 1 << 15);
#elif defined(DCXO_FINE_DCXO_COARSE_PMU)
        // Fine=DcxoFTune, Coarse=FTUNE2
        // FuneMuxSel = 1
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 1, 1);

        // FTSEL = 1
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 1 << 16, 1 << 16);

        // FTSWP = 0
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 1 << 15, 0);
#endif
        dcxo_init_flag = 1;
    }

    return 1;
}

int s5js100_dcxo_start_ctb_loop(Full_set_table_t tbl[])
{
    dcxo_tbl = tbl;

    g_dcxo_tid = new rtos::Thread(osPriorityNormal, 2048, NULL, "ctbloop");
    g_dcxo_tid->start(s5js100_dcxo_ctb_loop);

    if (!g_dcxo_tid) {
        return -ESRCH;
    }

    return 0;
}

/****************************************************************************
 * Name: s5js100_tsu_get_temperature
 *
 * Description:
 *   Read temperature of TSX.
 *
 * Returned Value:
 *   celcius temperature
 *
 ****************************************************************************/
int s5js100_tsu_calculate_temperature(unsigned int code)
{
    return tsu_code_to_temperature(code);
}

int s5js100_tsu_convert_code(unsigned int temperature)
{
    return tsu_temperature_to_code(temperature);
}

int s5js100_tsu_get_temperature(void)
{
    return tsu_code_to_temperature(s5js100_tsu_get_code());
}

int s5js100_tsu_set_interrupt(int high_interrupt, int temp)
{
    unsigned int tsu_code, irq_no;

    if (temp < DCXO_TSU_TABLE_MIN_TEMPERATURE) {
        temp = DCXO_TSU_TABLE_MIN_TEMPERATURE;
    } else if (temp > DCXO_TSU_TABLE_MIN_TEMPERATURE + DCXO_TSU_TABLE_LENGTH - 1) {
        temp = DCXO_TSU_TABLE_MIN_TEMPERATURE + DCXO_TSU_TABLE_LENGTH - 1;
    }

    if (high_interrupt) {
        tsu_code = tsu_temperature_to_code(temp - 1);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 1, 1);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 1, 0);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0, 0xFFFF, tsu_code);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 4), 0);
        irq_no = S5JS100_IRQ_TSU0;
    } else {
        tsu_code = tsu_temperature_to_code(temp);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 2, 2);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 2, 0);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0, 0xFFFF0000, tsu_code << 16);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 5), 0);
        irq_no = S5JS100_IRQ_TSU1;
    }

    //dbg("\ns5js100_tsu_set_interrupt(%d), tsu_code : 0x%x", high_interrupt, tsu_code);

    NVIC_SetVector((IRQn_Type)irq_no, (uint32_t)dcxo_tsu_isr);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif
    NVIC_EnableIRQ((IRQn_Type)irq_no);

    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 2), (1 << 2));
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 2), 0);

    return 1;
}

int s5js100_tsu_set_interrupt_code(int high_interrupt, int tsu_code, int check)
{
    unsigned int irq_no;
    int ret = 1;

    //dbg("s5js100_tsu_set_interrupt(%d), tsu_code : 0x%x\n", high_interrupt, tsu_code);

    if (check) {
        tsu_interrupt_flag = 0;
    }

    if (!high_interrupt) {
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 1, 1);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 1, 0);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0, 0xFFFF, tsu_code);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 4), 0);
        irq_no = S5JS100_IRQ_TSU0;
    } else {
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 2, 2);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, 2, 0);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG0, 0xFFFF0000, tsu_code << 16);
        modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 5), 0);
        irq_no = S5JS100_IRQ_TSU1;
    }

    NVIC_SetVector((IRQn_Type)irq_no, (uint32_t)dcxo_tsu_isr);
#if defined (__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif
    NVIC_EnableIRQ((IRQn_Type)irq_no);

    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 2), (1 << 2));
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_IRQ_CFG1, (1 << 2), 0);

    if (check) {
        //sleep(1);

        if (!tsu_interrupt_flag) {
            ret = 0;
        }
    }

    return ret;
}

int s5js100_dcxo_get_coarse_tune_value(void)
{
    int coarse_tune;

#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    // Fine=DcxoFTune, Coarse=FTUNE2
    coarse_tune = (getreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0) >> 1) & 0xFFFFF;
    coarse_tune >>= 6;
#elif defined(DCXO_FINE_PMU_COARSE_PMU)
    // Fine=FTUNE2, Coarse=FTUNE

    // Coarse=FTUNE 14bit
    // EVT0 : 0x81000524[26:13], EVT1 : 0x8100000C[13:0]
    if (is_evt0()) {
        coarse_tune = (getreg32(S5JS100_PMU_ALIVE + 0x524) >> 13) & 0x3FFF;
    } else {
        coarse_tune = (getreg32(S5JS100_PMU_ALIVE + 0xC) >> 0) & 0x3FFF;
    }
#elif defined(DCXO_FINE_DCXO_COARSE_PMU)
    // Fine=DcxoFTune, Coarse=FTUNE2

    // Coarse=FTUNE2 14bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]
    if (is_evt0()) {
        coarse_tune = (getreg32(S5JS100_PMU_ALIVE + 0x528) >> 17) & 0x3FFF;
    } else {
        coarse_tune = (getreg32(S5JS100_PMU_ALIVE + 0xC) >> 14) & 0x3FFF;
    }
#endif

    return coarse_tune;
}


int s5js100_dcxo_set_tune_value(unsigned int coarse_value, unsigned int fine_value)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    // Fine=DcxoFTune, Coarse=FTUNE2
    coarse_value <<= 6;

    // Coarse = DcxoFTune 20bit
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 0xFFFFF << 1, coarse_value << 1);

    // Fine=FTUNE2 14bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x3FFF << 17, fine_value << 17);
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 14, fine_value << 14);
    }
#elif defined(DCXO_FINE_PMU_COARSE_PMU)
    // Fine=FTUNE2, Coarse=FTUNE

    // Fine = FTUNE2 13bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x3FFF << 17, fine_value << 17);
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 14, fine_value << 14);
    }

    // Coarse=FTUNE 14bit
    // EVT0 : 0x81000524[26:13], EVT1 : 0x8100000C[13:0]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x524, (0x3FFF << 13), (coarse_value << 13));
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 0, coarse_value << 0);
    }
#elif defined(DCXO_FINE_DCXO_COARSE_PMU)
    // Fine=DcxoFTune, Coarse=FTUNE2
    fine_value <<= 6;

    // Fine = DcxoFTune 20bit
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 0xFFFFF << 1, fine_value << 1);

    // Coarse=FTUNE2 14bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x3FFF << 17, coarse_value << 17);
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 14, coarse_value << 14);
    }
#endif

    return 1;
}

int s5js100_dcxo_set_tune_value_20bit(unsigned int coarse_value, unsigned int fine_value)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    // Coarse=DcxoFTune, Fine=FTUNE2

    // Coarse = DcxoFTune 20bit
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 0xFFFFF << 1, coarse_value << 1);

    // Fine=FTUNE2 14bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x3FFF << 17, fine_value << 17);
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 14, fine_value << 14);
    }

    // toggle oSwRstDSM
    //modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1<<3), 0);
    //modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1<<3), (1<<3));
#elif defined(DCXO_FINE_PMU_COARSE_PMU)
    // Fine=FTUNE2, Coarse=FTUNE

    coarse_value >>= 6;

    // Fine = FTUNE2 13bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]

    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x3FFF << 17, fine_value << 17);
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 14, fine_value << 14);
    }

    // Coarse=FTUNE 14bit
    // EVT0 : 0x81000524[26:13], EVT1 : 0x8100000C[13:0]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x524, (0x3FFF << 13), (coarse_value << 13));
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 0, coarse_value << 0);
    }
#elif defined(DCXO_FINE_DCXO_COARSE_PMU)
    // Fine=DcxoFTune, Coarse=FTUNE2

    coarse_value >>= 6;

    // Fine = DcxoFTune 20bit
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG0, 0xFFFFF << 1, fine_value << 1);

    // Coarse=FTUNE2 14bit
    // EVT0 : 0x81000528[31:17], EVT1 : 0x8100000C[28:14]
    if (is_evt0()) {
        modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x3FFF << 17, coarse_value << 17);
    } else {
        modifyreg32(S5JS100_PMU_ALIVE + 0xC, 0x3FFF << 14, coarse_value << 14);
    }
#endif

    return 1;
}

int s5js100_dcxo_set_idac_ex(int value)
{
    modifyreg32(S5JS100_PMU_ALIVE + 0x524, 0xff << 5, value << 5);
    return 1;
}

int s5js100_dcxo_set_fb_res(int value)
{
    modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0x7 << 0, value << 0);
    return 1;
}

int s5js100_dcxo_set_buf_sel(int value)
{
    modifyreg32(S5JS100_PMU_ALIVE + 0x528, 0xf << 3, value << 3);
    return 1;
}

int s5js100_dcxo_set_dsmdither(unsigned int dither_bit_sel)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    // set dither_bit_sel, iDsmType=1, iEnDsmDither=0, iPrbsSel=0, iEnDsm=1
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG1, 0x7F, (1 << 6) + (dither_bit_sel << 2) + 1);

    // toggle oSwRstDSM
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), 0);
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), (1 << 3));
#else
#endif
    return 1;
}

int s5js100_dcxo_set_dither_bit_sel(unsigned int dither_bit_sel)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG1, (7 << 2), (dither_bit_sel << 2));
    // toggle oSwRstDSM
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), 0);
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), (1 << 3));
#else
#endif
    return 1;
}

int s5js100_dcxo_set_dsm_type(unsigned int dsm_type)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG1, (1 << 6), (dsm_type << 6));
    // toggle oSwRstDSM
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), 0);
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), (1 << 3));
#else
#endif
    return 1;
}

int s5js100_dcxo_set_en_dsm_dither(unsigned int en_dsm_dither)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG1, (1 << 5), (en_dsm_dither << 5));
    // toggle oSwRstDSM
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), 0);
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), (1 << 3));
#else
#endif
    return 1;
}

int s5js100_dcxo_set_prbs_sel(unsigned int prbs_sel)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG1, (1 << 1), (prbs_sel << 1));
    // toggle oSwRstDSM
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), 0);
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), (1 << 3));
#else
#endif
    return 1;
}

int s5js100_dcxo_en_dsm(unsigned int en_dsm)
{
#if defined(DCXO_FINE_PMU_COARSE_DCXO)
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_DSM_CFG1, (1 << 0), (en_dsm << 0));
    // toggle oSwRstDSM
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), 0);
    modifyreg32(S5JS100_DCXO_CFG + DCXO_CFG_SW_RESETN, (1 << 3), (1 << 3));
#else
#endif
    return 1;
}

extern "C" {
    int s5js100_dcxo_force_initialize(void)
    {
        dcxo_init_flag = 0;
        s5js100_dcxo_initialize();
        s5js100_dcxo_force_update_ctb();

        return 0;
    }
}
