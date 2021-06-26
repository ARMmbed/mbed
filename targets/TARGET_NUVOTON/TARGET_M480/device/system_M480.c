/**************************************************************************//**
 * @file     system_M480.c
 * @version  V1.000
 * @brief    CMSIS Cortex-M4 Core Peripheral Access Layer Source File for M480
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

#include "NuMicro.h"


/*----------------------------------------------------------------------------
  DEFINES
 *----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
  Clock Variable definitions
 *----------------------------------------------------------------------------*/
uint32_t SystemCoreClock  = __SYSTEM_CLOCK;    /*!< System Clock Frequency (Core Clock)*/
uint32_t CyclesPerUs      = (__HSI / 1000000UL); /* Cycles per micro second */
uint32_t PllClock         = __HSI;             /*!< PLL Output Clock Frequency         */
uint32_t gau32ClkSrcTbl[] = {__HXT, __LXT, 0UL, __LIRC, 0UL, 0UL, 0UL, __HIRC};

/*----------------------------------------------------------------------------
  Clock functions
 *----------------------------------------------------------------------------*/
void SystemCoreClockUpdate (void)            /* Get Core Clock Frequency      */
{
    uint32_t u32Freq, u32ClkSrc;
    uint32_t u32HclkDiv;

    /* Update PLL Clock */
    PllClock = CLK_GetPLLClockFreq();

    u32ClkSrc = CLK->CLKSEL0 & CLK_CLKSEL0_HCLKSEL_Msk;

    if(u32ClkSrc == CLK_CLKSEL0_HCLKSEL_PLL)
    {
        /* Use PLL clock */
        u32Freq = PllClock;
    }
    else
    {
        /* Use the clock sources directly */
        u32Freq = gau32ClkSrcTbl[u32ClkSrc];
    }

    u32HclkDiv = (CLK->CLKDIV0 & CLK_CLKDIV0_HCLKDIV_Msk) + 1UL;

    /* Update System Core Clock */
    SystemCoreClock = u32Freq / u32HclkDiv;


    //if(SystemCoreClock == 0)
    //    __BKPT(0);

    CyclesPerUs = (SystemCoreClock + 500000UL) / 1000000UL;
}

#if MBED_CONF_TARGET_HXT_PRESENT
/**
  * @brief      Set PF.2 and PF.3 to input mode
  * @param      None
  * @return     None
  * @details    GPIO default state could be configured as input or quasi through user config.
  *             To use HXT, PF.2 and PF.3 must not set as quasi mode. This function changes
  *             PF.2 and PF.3 to input mode no matter which mode they are working at.
  */
static __INLINE void HXTInit(void)
{
    PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);

}
#endif

/**
 * @brief  Initialize the System
 *
 * @param  none
 * @return none
 */
void SystemInit (void)
{
    /* Add your system initialize code here.
       Do not use global variables because this function is called before
       reaching pre-main. RW section maybe overwritten afterwards.          */


    /* FPU settings ------------------------------------------------------------*/
#if (__FPU_PRESENT == 1U) && (__FPU_USED == 1U)
    SCB->CPACR |= ((3UL << 10*2) |                 /* set CP10 Full Access */
                   (3UL << 11*2)  );               /* set CP11 Full Access */
#endif

    /* Disable Flash Access Cycle Auto-tuning, set access cycle for CPU @ 192MHz */
    /* The FMC_CYCCTL_FADIS_Msk bit is removed from BSP and gets unnecessary because it has fixed to 1 by ROMMAP on real chips (Both LAG018 and LAG033). */
    FMC->CYCCTL = (FMC->CYCCTL & ~FMC_CYCCTL_CYCLE_Msk) | (8 << FMC_CYCCTL_CYCLE_Pos);
    /* Configure power down bias, must set 1 before entering power down mode.
       So set it at the very beginning */
    CLK->LDOCTL |= CLK_LDOCTL_PDBIASEN_Msk;
    /* Hand over the control of PF.4~11 I/O function from RTC module to GPIO module */
    CLK->APBCLK0 |= CLK_APBCLK0_RTCCKEN_Msk;
    RTC->GPIOCTL0 &= ~(RTC_GPIOCTL0_CTLSEL0_Msk | RTC_GPIOCTL0_CTLSEL1_Msk |
                       RTC_GPIOCTL0_CTLSEL2_Msk | RTC_GPIOCTL0_CTLSEL3_Msk);
    RTC->GPIOCTL1 &= ~(RTC_GPIOCTL1_CTLSEL4_Msk | RTC_GPIOCTL1_CTLSEL5_Msk |
                       RTC_GPIOCTL1_CTLSEL6_Msk | RTC_GPIOCTL1_CTLSEL7_Msk);
    CLK->APBCLK0 &= ~CLK_APBCLK0_RTCCKEN_Msk;
#if MBED_CONF_TARGET_HXT_PRESENT
    HXTInit();
#endif

#if MBED_CONF_TARGET_SPIM_CCM_ENABLE
    // Divert SRAM bank2 (32 KB) to CCM from SPIM cache
    // NOTE: C-runtime not initialized yet. Ensure no static memory (global variable) are accessed in function below.
    // NOTE: SPIM must keep enabled to run CCM mode.
    CLK_EnableModuleClock(SPIM_MODULE);
    SYS_ResetModule(SPIM_RST);
    SPIM_DISABLE_CACHE();
    SPIM_ENABLE_CCM();
    while (! SPIM_IS_CCM_EN());
#endif
}
/*** (C) COPYRIGHT 2016 Nuvoton Technology Corp. ***/
