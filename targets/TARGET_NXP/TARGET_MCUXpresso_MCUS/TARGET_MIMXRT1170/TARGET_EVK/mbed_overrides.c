/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
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
#include "pinmap.h"
#include "fsl_clock_config.h"
#include "fsl_clock.h"
#include "fsl_iomuxc.h"
#include "fsl_gpio.h"


void BOARD_ConfigMPU(void)
{
   
    /* Disable I cache and D cache */
    if (SCB_CCR_IC_Msk == (SCB_CCR_IC_Msk & SCB->CCR))
    {
        SCB_DisableICache();
    }

    if (SCB_CCR_DC_Msk == (SCB_CCR_DC_Msk & SCB->CCR))
    {
        SCB_DisableDCache();
    }

ARM_MPU_Disable();

    /* Region 0 setting: No data access permission. */
    MPU->RBAR = ARM_MPU_RBAR(0, 0x00000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_NONE, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_4GB);

    /* Region 1 setting: Memory with Device type, not shareable, non-cacheable. */
    MPU->RBAR = ARM_MPU_RBAR(1, 0x80000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_512MB);

    /* Region 2 setting: Memory with Device type, not shareable,  non-cacheable. */
    MPU->RBAR = ARM_MPU_RBAR(2, 0x60000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_512MB);

    /* Region 3 setting: Memory with Device type, not shareable, non-cacheable. */
    MPU->RBAR = ARM_MPU_RBAR(3, 0x00000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1GB);

    /* Region 4 setting: Memory with Normal type, not shareable, outer/inner write back */
    MPU->RBAR = ARM_MPU_RBAR(4, 0x00000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_256KB);

    /* Region 5 setting: Memory with Normal type, not shareable, outer/inner write back */
    MPU->RBAR = ARM_MPU_RBAR(5, 0x20000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_256KB);

    /* Region 6 setting: Memory with Normal type, not shareable, outer/inner write back */
    MPU->RBAR = ARM_MPU_RBAR(6, 0x20200000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_1MB);

    /* Region 7 setting: Memory with Normal type, not shareable, outer/inner write back */
    MPU->RBAR = ARM_MPU_RBAR(7, 0x20300000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_512KB);

    /* Region 8 setting: Memory with Normal type, not shareable, outer/inner write back. */
    MPU->RBAR = ARM_MPU_RBAR(8, 0x30000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_RO, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_16MB);


    /* Region 9 setting: Memory with Normal type, not shareable, outer/inner write back */
    MPU->RBAR = ARM_MPU_RBAR(9, 0x80000000U);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_64MB);
   
    /* Region 11 setting: Memory with Device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(11, 0x40000000);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_16MB);

    /* Region 12 setting: Memory with Device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(12, 0x41000000);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_2MB);

    /* Region 13 setting: Memory with Device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(13, 0x41400000);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1MB);

    /* Region 14 setting: Memory with Device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(14, 0x41800000);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1MB);

    /* Region 15 setting: Memory with Device type, not shareable, non-cacheable */
    MPU->RBAR = ARM_MPU_RBAR(15, 0x42000000);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_1MB);

    /* Enable MPU */
    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);

    /* Enable I cache and D cache */
    SCB_EnableDCache();
    SCB_EnableICache();

}


// called before main
void mbed_sdk_init()
{
    BOARD_ConfigMPU();
    BOARD_BootClockRUN();

}

void spi_setup_clock()
{
   
}

uint32_t spi_get_clock(void)
{
    return 1;
}

void us_ticker_setup_clock()
{
clock_root_config_t rootCfg = {0};
   /* Configure gpt timer using Osc48MDiv2 */
    rootCfg.mux = kCLOCK_GPT1_ClockRoot_MuxOscRc48MDiv2;
    rootCfg.div = 1;
    CLOCK_SetRootClock(kCLOCK_Root_Gpt1, &rootCfg);
}

uint32_t us_ticker_get_clock()
{
return CLOCK_GetRootClockFreq(kCLOCK_Root_Gpt1 );
}

void serial_setup_clock(void)
{
  clock_root_config_t rootCfg = {0};
   /* Configure Lpuart1 using SysPll2*/
    rootCfg.mux = kCLOCK_LPUART1_ClockRoot_MuxSysPll2Out;
    rootCfg.div = 22;
    CLOCK_SetRootClock(kCLOCK_Root_Lpuart1, &rootCfg);
}

uint32_t serial_get_clock(void)
{
    return CLOCK_GetRootClockFreq(kCLOCK_Root_Lpuart1);;
}

void i2c_setup_clock()
{
  
}

uint32_t i2c_get_clock()
{
    return 1;
}






