/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
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

/**
  * This file configures the system clock as follows:
  *-----------------------------------------------------------------------------
  * System clock source | HSE (external 32 MHz clock)
  *-----------------------------------------------------------------------------
  * SYSCLK(MHz)         | 32
  * AHBCLK (MHz)        | 32
  * APB1CLK (MHz)       | 32
  * APB2CLK (MHz)       | 32
  * USB capable         | YES
  *-----------------------------------------------------------------------------
**/

#include "stm32wbxx.h"
#include "mbed_error.h"
#include "stm32wbxx_ll_hsem.h"
#include "otp.h"


static void Config_HSE(void)
{
    OTP_ID0_t *p_otp;

    /**
     * Read HSE_Tuning from OTP
     */
    p_otp = (OTP_ID0_t *) OTP_Read(0);
    if (p_otp) {
        LL_RCC_HSE_SetCapacitorTuning(p_otp->hse_tuning);
    }

    return;
}


/**
  * @brief  Configures the System clock source, PLL Multiplier and Divider factors, AHB/APBx prescalers
  * @note   This function should be called only once the RCC clock configuration
  *         is reset to the default reset state (done in SystemInit() function).
  * @param  None
  * @retval None
  */

void SetSysClock(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    __HAL_RCC_HSEM_CLK_ENABLE();

    /* This prevents the CPU2 (M0+) to configure RCC */
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID));

    Config_HSE();

    /* Initializes the CPU, AHB and APB busses clocks */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        error("HAL_RCC_OscConfig error\n");
    }

    /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4 | RCC_CLOCKTYPE_HCLK2
                                  | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        error("HAL_RCC_ClockConfig error\n");
    }

    /* Initializes the peripherals clocks */
    /* RNG needs to be configured like in M0 core, i.e. with HSI48 */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS | RCC_PERIPHCLK_RFWAKEUP | RCC_PERIPHCLK_RNG ;
    PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_LSE;
    PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_LSE;
    PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSE;
    PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        error("HAL_RCCEx_PeriphCLKConfig error\n");
    }

    LL_PWR_SMPS_SetStartupCurrent(LL_PWR_SMPS_STARTUP_CURRENT_80MA);
    LL_PWR_SMPS_SetOutputVoltageLevel(LL_PWR_SMPS_OUTPUT_VOLTAGE_1V40);
    // LL_PWR_SMPS_Enable();

    /* Select HSI as system clock source after Wake Up from Stop mode */
    LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);

    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);
}
