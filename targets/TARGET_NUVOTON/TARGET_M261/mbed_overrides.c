/* 
 * Copyright (c) 2019-2020 Nuvoton Technology Corporation
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

#include "cmsis.h"
#include "mbed_error.h"

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

    /* Enable HIRC clock (Internal RC 12MHz) */
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
    /* Enable HIRC48 clock (Internal RC 48MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRC48EN_Msk);

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
    /* Wait for HIRC48 clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRC48STB_Msk);

    /* Set core clock as 64M from PLL */
    CLK_SetCoreClock(FREQ_64MHZ);
    
    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
	
}
