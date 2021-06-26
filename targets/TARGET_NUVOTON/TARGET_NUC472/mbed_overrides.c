/* mbed Microcontroller Library
 * Copyright (c) 2015-2016 Nuvoton
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
#include "analogin_api.h"

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

#if MBED_CONF_TARGET_HXT_PRESENT
    /* Enable External XTAL (4~24 MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
#else
    /* Disable External XTAL (4~24 MHz) */
    CLK_DisableXtalRC(CLK_PWRCTL_HXTEN_Msk);
#endif
    /* Enable LIRC */
    CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);
#if MBED_CONF_TARGET_LXT_PRESENT
    /* Enable LXT */
    CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);
#else
    /* Disable LXT */
    CLK_DisableXtalRC(CLK_PWRCTL_LXTEN_Msk);
#endif

#if MBED_CONF_TARGET_HXT_PRESENT
    /* Waiting for External XTAL (4~24 MHz) ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);
#endif
    /* Waiting for LIRC ready */
    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);
#if MBED_CONF_TARGET_LXT_PRESENT
    /* Waiting for LXT ready */
    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);
#endif

#if MBED_CONF_TARGET_HXT_PRESENT
    /* Switch HCLK clock source to HXT */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HXT,CLK_CLKDIV0_HCLK(1));
#else
    /* Switch HCLK clock source to HIRC */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC,CLK_CLKDIV0_HCLK(1));
#endif

    /* Set PLL to power down mode and PLLSTB bit in CLKSTATUS register will be cleared by hardware.*/
    CLK->PLLCTL|= CLK_PLLCTL_PD_Msk;

    /* Set PLL frequency */
#if MBED_CONF_TARGET_HXT_PRESENT
    CLK->PLLCTL = CLK_PLLCTL_84MHz_HXT;
#else
    CLK->PLLCTL = CLK_PLLCTL_50MHz_HIRC;
#endif

    /* Waiting for clock ready */
    CLK_WaitClockReady(CLK_STATUS_PLLSTB_Msk);

    /* Switch HCLK clock source to PLL */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_PLL,CLK_CLKDIV0_HCLK(1));

#if DEVICE_ANALOGIN
    /* Vref connect to AVDD */
    SYS->VREFCTL = (SYS->VREFCTL & ~SYS_VREFCTL_VREFCTL_Msk) | SYS_VREFCTL_VREF_AVDD;
    /* Switch ADC0 to EADC mode */
    SYS->VREFCTL = (SYS->VREFCTL & ~SYS_VREFCTL_ADCMODESEL_Msk) | SYS_VREFCTL_ADCMODESEL_EADC;
#endif
    
    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}
