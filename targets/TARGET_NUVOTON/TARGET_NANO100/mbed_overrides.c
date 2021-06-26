/* mbed Microcontroller Library
 * Copyright (c) 2015-2017 Nuvoton
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

#include "cmsis.h"

void mbed_sdk_init(void)
{
    // NOTE: Support singleton semantics to be called from other init functions
    static int inited = 0;
    if (inited) {
        return;
    }
    inited = 1;
    
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable HIRC clock (internal OSC 12MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRC_EN_Msk);
#if MBED_CONF_TARGET_HXT_PRESENT
    /* Enable HXT clock (external XTAL 12MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HXT_EN_Msk);
#else
    /* Disable HXT clock (external XTAL 12MHz) */
    CLK_DisableXtalRC(CLK_PWRCTL_HXT_EN_Msk);
#endif
    /* Enable LIRC clock (OSC 10KHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_LIRC_EN_Msk);
#if MBED_CONF_TARGET_LXT_PRESENT
    /* Enable LXT clock (XTAL 32KHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_LXT_EN_Msk);
#else
    /* Disable LXT clock (XTAL 32KHz) */
    CLK_DisableXtalRC(CLK_PWRCTL_LXT_EN_Msk);
#endif

    /* Wait for HIRC clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_HIRC_STB_Msk);
#if MBED_CONF_TARGET_HXT_PRESENT
    /* Wait for HXT clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_HXT_STB_Msk);
#endif
    /* Wait for LIRC clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_LIRC_STB_Msk);
#if MBED_CONF_TARGET_LXT_PRESENT
    /* Wait for LXT clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_LXT_STB_Msk);
#endif

    /* Set HCLK source form HXT/HIRC and HCLK source divide 1  */
#if MBED_CONF_TARGET_HXT_PRESENT
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_HXT, CLK_HCLK_CLK_DIVIDER(1));
#else
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_HIRC, CLK_HCLK_CLK_DIVIDER(1));
#endif

    /* Select HXT/HIRC to clock PLL
     *
     * Comparison between HXT/HIRC-clocked PLL:
     * 1. Spare HXT on board if only HIRC is used.
     * 2. HIRC has shorter stable time.
     * 3. HXT has better accuracy. USBD requires HXT-clocked PLL.
     * 4. HIRC has shorter wake-up time from power-down mode. 
     *    Per test, wake-up time from power-down mode would take:
     *    T1. 1~13 ms (proportional to deep sleep time) with HXT-clocked PLL as HCLK clock source
     *    T2. <1 ms with HIRC-clocked PLL as HCLK clock source
     *    T1 will fail Greentea test which requires max 10 ms wake-up time.
     *
     *    If we just call CLK_SetCoreClock(FREQ_48MHZ) to configure HCLK to 48 MHz,
     *    it will go T1 with HXT already enabled in front. So we manually configure
     *    it to choose HXT/HIRC-clocked PLL.
     */
#define NU_HXT_PLL      1
#define NU_HIRC_PLL     2

#ifndef NU_CLOCK_PLL
#define NU_CLOCK_PLL    NU_HIRC_PLL
#endif

#if (NU_CLOCK_PLL == NU_HXT_PLL) && (MBED_CONF_TARGET_HXT_PRESENT == 0)
#error "HXT is not present to clock PLL"
#endif

#if (NU_CLOCK_PLL == NU_HXT_PLL)
    CLK_EnablePLL(CLK_PLLCTL_PLL_SRC_HXT, FREQ_48MHZ*2);
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_PLL, CLK_HCLK_CLK_DIVIDER(2));
#elif (NU_CLOCK_PLL == NU_HIRC_PLL)
    CLK_EnablePLL(CLK_PLLCTL_PLL_SRC_HIRC, FREQ_48MHZ*2);
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_PLL, CLK_HCLK_CLK_DIVIDER(2));
#endif

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}
