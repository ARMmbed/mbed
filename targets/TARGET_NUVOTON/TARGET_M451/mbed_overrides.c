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

    /* Enable HIRC clock (Internal RC 22.1184MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
#if MBED_CONF_TARGET_HXT_PRESENT
    /* Enable HXT clock (external XTAL 12MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
#else
    /* Disable HXT clock (external XTAL 12MHz) */
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

    /* Wait for HIRC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);
#if MBED_CONF_TARGET_HXT_PRESENT
    /* Wait for HXT clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);
#endif
    /* Wait for LIRC clock ready */
    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);
#if MBED_CONF_TARGET_LXT_PRESENT
    /* Wait for LXT clock ready */
    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);
#endif

    /* Select HCLK clock source as HIRC and HCLK clock divider as 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));
    
    /* Set core clock as 72000000 from PLL */
    CLK_SetCoreClock(72000000);

#if DEVICE_ANALOGIN
    /* Vref connect to internal */
    SYS->VREFCTL = (SYS->VREFCTL & ~SYS_VREFCTL_VREFCTL_Msk) | SYS_VREFCTL_VREF_3_072V;
#endif
    
    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}
