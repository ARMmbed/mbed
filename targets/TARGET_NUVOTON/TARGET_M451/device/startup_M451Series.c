/******************************************************************************
 * @file     startup_M451Series.c
 * @version  V0.10
 * $Revision: 11 $
 * $Date: 15/09/02 10:02a $
 * @brief    CMSIS Cortex-M4 Core Peripheral Access Layer Source File for M451 Series MCU
 *
 * @note
 * Copyright (C) 2013~2015 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

#include "M451Series.h"

/* Suppress warning messages */
#if defined(__CC_ARM)
// Suppress warning message: extended constant initialiser used
#pragma diag_suppress 1296
#elif defined(__ICCARM__)
#elif defined(__GNUC__)
#endif

/* Macro Definitions */
#if defined(__CC_ARM)
#define WEAK            __attribute__ ((weak))
#define ALIAS(f)        __attribute__ ((weak, alias(#f)))

#define WEAK_ALIAS_FUNC(FUN, FUN_ALIAS) \
void FUN(void) __attribute__ ((weak, alias(#FUN_ALIAS)));

#elif defined(__ICCARM__)
//#define STRINGIFY(x) #x
//#define _STRINGIFY(x) STRINGIFY(x)
#define WEAK_ALIAS_FUNC(FUN, FUN_ALIAS) \
void FUN(void);                         \
_Pragma(_STRINGIFY(_WEAK_ALIAS_FUNC(FUN, FUN_ALIAS)))
#define _WEAK_ALIAS_FUNC(FUN, FUN_ALIAS) weak __WEAK_ALIAS_FUNC(FUN, FUN_ALIAS)
#define __WEAK_ALIAS_FUNC(FUN, FUN_ALIAS) FUN##=##FUN_ALIAS

#elif defined(__GNUC__)
#define WEAK            __attribute__ ((weak))
#define ALIAS(f)        __attribute__ ((weak, alias(#f)))

#define WEAK_ALIAS_FUNC(FUN, FUN_ALIAS) \
void FUN(void) __attribute__ ((weak, alias(#FUN_ALIAS)));

#endif


/* Initialize segments */
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
extern uint32_t Image$$ARM_LIB_STACK$$ZI$$Limit;
extern void __main(void);
#elif defined(__ICCARM__)
void __iar_program_start(void);
#elif defined(__GNUC__)
extern uint32_t __StackTop;
extern uint32_t __etext;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;

#if defined(TOOLCHAIN_GCC_ARM)
extern void _start(void);
#else
#error("For GCC toolchain, only support GNU ARM Embedded")
#endif
#endif

/* Default empty handler */
void Default_Handler(void);

/* Reset handler */
void Reset_Handler(void);

/* Cortex-M4 core handlers */
WEAK_ALIAS_FUNC(NMI_Handler, Default_Handler)
WEAK_ALIAS_FUNC(HardFault_Handler, Default_Handler)
WEAK_ALIAS_FUNC(MemManage_Handler, Default_Handler)
WEAK_ALIAS_FUNC(BusFault_Handler , Default_Handler)
WEAK_ALIAS_FUNC(UsageFault_Handler, Default_Handler)
WEAK_ALIAS_FUNC(SVC_Handler, Default_Handler)
WEAK_ALIAS_FUNC(DebugMon_Handler, Default_Handler)
WEAK_ALIAS_FUNC(PendSV_Handler, Default_Handler)
WEAK_ALIAS_FUNC(SysTick_Handler, Default_Handler)

/* Peripherals handlers */
WEAK_ALIAS_FUNC(BOD_IRQHandler, Default_Handler)        // 0: Brown Out detection
WEAK_ALIAS_FUNC(IRC_IRQHandler, Default_Handler)        // 1: Internal RC
WEAK_ALIAS_FUNC(PWRWU_IRQHandler, Default_Handler)      // 2: Power down wake up 
WEAK_ALIAS_FUNC(RAMPE_IRQHandler, Default_Handler)      // 3: RAM parity error
WEAK_ALIAS_FUNC(CLKFAIL_IRQHandler, Default_Handler)    // 4: Clock detection fail
                                                        // 5: Reserved
WEAK_ALIAS_FUNC(RTC_IRQHandler, Default_Handler)        // 6: Real Time Clock 
WEAK_ALIAS_FUNC(TAMPER_IRQHandler, Default_Handler)     // 7: Tamper detection
WEAK_ALIAS_FUNC(WDT_IRQHandler, Default_Handler)        // 8: Watchdog timer
WEAK_ALIAS_FUNC(WWDT_IRQHandler, Default_Handler)       // 9: Window watchdog timer
WEAK_ALIAS_FUNC(EINT0_IRQHandler, Default_Handler)      // 10: External Input 0
WEAK_ALIAS_FUNC(EINT1_IRQHandler, Default_Handler)      // 11: External Input 1
WEAK_ALIAS_FUNC(EINT2_IRQHandler, Default_Handler)      // 12: External Input 2
WEAK_ALIAS_FUNC(EINT3_IRQHandler, Default_Handler)      // 13: External Input 3
WEAK_ALIAS_FUNC(EINT4_IRQHandler, Default_Handler)      // 14: External Input 4
WEAK_ALIAS_FUNC(EINT5_IRQHandler, Default_Handler)      // 15: External Input 5
WEAK_ALIAS_FUNC(GPA_IRQHandler, Default_Handler)        // 16: GPIO Port A
WEAK_ALIAS_FUNC(GPB_IRQHandler, Default_Handler)        // 17: GPIO Port B
WEAK_ALIAS_FUNC(GPC_IRQHandler, Default_Handler)        // 18: GPIO Port C
WEAK_ALIAS_FUNC(GPD_IRQHandler, Default_Handler)        // 19: GPIO Port D
WEAK_ALIAS_FUNC(GPE_IRQHandler, Default_Handler)        // 20: GPIO Port E
WEAK_ALIAS_FUNC(GPF_IRQHandler, Default_Handler)        // 21: GPIO Port F
WEAK_ALIAS_FUNC(SPI0_IRQHandler, Default_Handler)       // 22: SPI0
WEAK_ALIAS_FUNC(SPI1_IRQHandler, Default_Handler)       // 23: SPI1
WEAK_ALIAS_FUNC(BRAKE0_IRQHandler, Default_Handler)     // 24: 
WEAK_ALIAS_FUNC(PWM0P0_IRQHandler, Default_Handler)     // 25: 
WEAK_ALIAS_FUNC(PWM0P1_IRQHandler, Default_Handler)     // 26: 
WEAK_ALIAS_FUNC(PWM0P2_IRQHandler, Default_Handler)     // 27: 
WEAK_ALIAS_FUNC(BRAKE1_IRQHandler, Default_Handler)     // 28: 
WEAK_ALIAS_FUNC(PWM1P0_IRQHandler, Default_Handler)     // 29: 
WEAK_ALIAS_FUNC(PWM1P1_IRQHandler, Default_Handler)     // 30: 
WEAK_ALIAS_FUNC(PWM1P2_IRQHandler, Default_Handler)     // 31: 
WEAK_ALIAS_FUNC(TMR0_IRQHandler, Default_Handler)       // 32: Timer 0
WEAK_ALIAS_FUNC(TMR1_IRQHandler, Default_Handler)       // 33: Timer 1
WEAK_ALIAS_FUNC(TMR2_IRQHandler, Default_Handler)       // 34: Timer 2
WEAK_ALIAS_FUNC(TMR3_IRQHandler, Default_Handler)       // 35: Timer 3
WEAK_ALIAS_FUNC(UART0_IRQHandler, Default_Handler)      // 36: UART0
WEAK_ALIAS_FUNC(UART1_IRQHandler, Default_Handler)      // 37: UART1
WEAK_ALIAS_FUNC(I2C0_IRQHandler, Default_Handler)       // 38: I2C0
WEAK_ALIAS_FUNC(I2C1_IRQHandler, Default_Handler)       // 39: I2C1
WEAK_ALIAS_FUNC(PDMA_IRQHandler, Default_Handler)       // 40: Peripheral DMA
WEAK_ALIAS_FUNC(DAC_IRQHandler, Default_Handler)        // 41: DAC
WEAK_ALIAS_FUNC(ADC00_IRQHandler, Default_Handler)      // 42: ADC0 interrupt source 0
WEAK_ALIAS_FUNC(ADC01_IRQHandler, Default_Handler)      // 43: ADC0 interrupt source 1
WEAK_ALIAS_FUNC(ACMP01_IRQHandler, Default_Handler)     // 44: ACMP0 and ACMP1
                                                        // 45: Reserved
WEAK_ALIAS_FUNC(ADC02_IRQHandler, Default_Handler)      // 46: ADC0 interrupt source 2
WEAK_ALIAS_FUNC(ADC03_IRQHandler, Default_Handler)      // 47: ADC0 interrupt source 3
WEAK_ALIAS_FUNC(UART2_IRQHandler, Default_Handler)      // 48: UART2
WEAK_ALIAS_FUNC(UART3_IRQHandler, Default_Handler)      // 49: UART3
                                                        // 50: Reserved
WEAK_ALIAS_FUNC(SPI2_IRQHandler, Default_Handler)       // 51: SPI2
                                                        // 52: Reserved
WEAK_ALIAS_FUNC(USBD_IRQHandler, Default_Handler)       // 53: USB device
WEAK_ALIAS_FUNC(USBH_IRQHandler, Default_Handler)       // 54: USB host
WEAK_ALIAS_FUNC(USBOTG_IRQHandler, Default_Handler)     // 55: USB OTG
WEAK_ALIAS_FUNC(CAN0_IRQHandler, Default_Handler)       // 56: CAN0
                                                        // 57: Reserved
WEAK_ALIAS_FUNC(SC0_IRQHandler, Default_Handler)        // 58: 
                                                        // 59: Reserved.
                                                        // 60: 
                                                        // 61: 
                                                        // 62:
WEAK_ALIAS_FUNC(TK_IRQHandler, Default_Handler)         // 63:

/* Vector table */
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
__attribute__ ((section("RESET"), used))
const uint32_t __vector_handlers[] = {
#elif defined(__ICCARM__)
extern uint32_t CSTACK$$Limit;
const uint32_t __vector_table[] @ ".intvec" = {
#elif defined(__GNUC__)
__attribute__ ((section(".vector_table")))
const uint32_t __vector_handlers[] = {
#endif

    /* Configure Initial Stack Pointer, using linker-generated symbols */
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
    (uint32_t) &Image$$ARM_LIB_STACK$$ZI$$Limit,
#elif defined(__ICCARM__)
    //(uint32_t) __sfe("CSTACK"),
    (uint32_t) &CSTACK$$Limit,
#elif defined(__GNUC__)
    (uint32_t) &__StackTop,
#endif

    (uint32_t) Reset_Handler,           // Reset Handler
    (uint32_t) NMI_Handler,             // NMI Handler
    (uint32_t) HardFault_Handler,       // Hard Fault Handler
    (uint32_t) MemManage_Handler,       // MPU Fault Handler
    (uint32_t) BusFault_Handler,        // Bus Fault Handler
    (uint32_t) UsageFault_Handler,      // Usage Fault Handler
    0,                                  // Reserved
    0,                                  // Reserved
    0,                                  // Reserved
    0,                                  // Reserved
    (uint32_t) SVC_Handler,             // SVCall Handler
    (uint32_t) DebugMon_Handler,        // Debug Monitor Handler
    0,                                  // Reserved
    (uint32_t) PendSV_Handler,          // PendSV Handler
    (uint32_t) SysTick_Handler,         // SysTick Handler

    /* External Interrupts */
    (uint32_t) BOD_IRQHandler,          // 0: Brown Out detection
    (uint32_t) IRC_IRQHandler,          // 1: Internal RC
    (uint32_t) PWRWU_IRQHandler,        // 2: Power down wake up 
    (uint32_t) RAMPE_IRQHandler,        // 3: RAM parity error
    (uint32_t) CLKFAIL_IRQHandler,      // 4: Clock detection fail
    (uint32_t) Default_Handler,         // 5: Reserved
    (uint32_t) RTC_IRQHandler,          // 6: Real Time Clock 
    (uint32_t) TAMPER_IRQHandler,       // 7: Tamper detection
    (uint32_t) WDT_IRQHandler,          // 8: Watchdog timer
    (uint32_t) WWDT_IRQHandler,         // 9: Window watchdog timer
    (uint32_t) EINT0_IRQHandler,        // 10: External Input 0
    (uint32_t) EINT1_IRQHandler,        // 11: External Input 1
    (uint32_t) EINT2_IRQHandler,        // 12: External Input 2
    (uint32_t) EINT3_IRQHandler,        // 13: External Input 3
    (uint32_t) EINT4_IRQHandler,        // 14: External Input 4
    (uint32_t) EINT5_IRQHandler,        // 15: External Input 5
    (uint32_t) GPA_IRQHandler,          // 16: GPIO Port A
    (uint32_t) GPB_IRQHandler,          // 17: GPIO Port B
    (uint32_t) GPC_IRQHandler,          // 18: GPIO Port C
    (uint32_t) GPD_IRQHandler,          // 19: GPIO Port D
    (uint32_t) GPE_IRQHandler,          // 20: GPIO Port E
    (uint32_t) GPF_IRQHandler,          // 21: GPIO Port F
    (uint32_t) SPI0_IRQHandler,         // 22: SPI0
    (uint32_t) SPI1_IRQHandler,         // 23: SPI1
    (uint32_t) BRAKE0_IRQHandler,       // 24: 
    (uint32_t) PWM0P0_IRQHandler,       // 25: 
    (uint32_t) PWM0P1_IRQHandler,       // 26: 
    (uint32_t) PWM0P2_IRQHandler,       // 27: 
    (uint32_t) BRAKE1_IRQHandler,       // 28: 
    (uint32_t) PWM1P0_IRQHandler,       // 29: 
    (uint32_t) PWM1P1_IRQHandler,       // 30: 
    (uint32_t) PWM1P2_IRQHandler,       // 31: 
    (uint32_t) TMR0_IRQHandler,         // 32: Timer 0
    (uint32_t) TMR1_IRQHandler,         // 33: Timer 1
    (uint32_t) TMR2_IRQHandler,         // 34: Timer 2
    (uint32_t) TMR3_IRQHandler,         // 35: Timer 3
    (uint32_t) UART0_IRQHandler,        // 36: UART0
    (uint32_t) UART1_IRQHandler,        // 37: UART1
    (uint32_t) I2C0_IRQHandler,         // 38: I2C0
    (uint32_t) I2C1_IRQHandler,         // 39: I2C1
    (uint32_t) PDMA_IRQHandler,         // 40: Peripheral DMA
    (uint32_t) DAC_IRQHandler,          // 41: DAC
    (uint32_t) ADC00_IRQHandler,        // 42: ADC0 interrupt source 0
    (uint32_t) ADC01_IRQHandler,        // 43: ADC0 interrupt source 1
    (uint32_t) ACMP01_IRQHandler,       // 44: ACMP0 and ACMP1
    (uint32_t) Default_Handler,         // 45: Reserved
    (uint32_t) ADC02_IRQHandler,        // 46: ADC0 interrupt source 2
    (uint32_t) ADC03_IRQHandler,        // 47: ADC0 interrupt source 3
    (uint32_t) UART2_IRQHandler,        // 48: UART2
    (uint32_t) UART3_IRQHandler,        // 49: UART3
    (uint32_t) Default_Handler,         // 50: Reserved
    (uint32_t) SPI2_IRQHandler,         // 51: SPI2
    (uint32_t) Default_Handler,         // 52: Reserved
    (uint32_t) USBD_IRQHandler,         // 53: USB device
    (uint32_t) USBH_IRQHandler,         // 54: USB host
    (uint32_t) USBOTG_IRQHandler,       // 55: USB OTG
    (uint32_t) CAN0_IRQHandler,         // 56: CAN0
    (uint32_t) Default_Handler,         // 57: Reserved
    (uint32_t) SC0_IRQHandler,          // 58: 
    (uint32_t) Default_Handler,         // 59: Reserved.
    (uint32_t) Default_Handler,         // 60: 
    (uint32_t) Default_Handler,         // 61: 
    (uint32_t) Default_Handler,         // 62:
    (uint32_t) TK_IRQHandler,           // 63:
};

/**
 * \brief This is the code that gets called on processor reset.
 */
void Reset_Handler(void)
{
    /* Disable register write-protection function */
    SYS_UnlockReg();
    
    /* Disable Power-on Reset function */
    SYS_DISABLE_POR();

#if MBED_CONF_TARGET_HXT_PRESENT
    /* HXT Crystal Type Select: INV */
    CLK->PWRCTL &= ~CLK_PWRCTL_HXTSELTYP_Msk;
#endif

    /**
     * NOTE 1: Unlock is required for perhaps some register access in SystemInit().
     * NOTE 2: Because EBI (external SRAM) init is done in SystemInit(), SystemInit() must be called at the very start.
     */
    SystemInit();
    
    /* Enable register write-protection function */
    SYS_LockReg();

#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
    __main();
    
#elif defined(__ICCARM__)
    __iar_program_start();

#elif defined(__GNUC__)
    uint32_t *src_ind = (uint32_t *) &__etext;
    uint32_t *dst_ind = (uint32_t *) &__data_start__;
    uint32_t *dst_end = (uint32_t *) &__data_end__;

    /* Move .data section from ROM to RAM */
    if (src_ind != dst_ind) {
        for (; dst_ind < dst_end;) {
            *dst_ind ++ = *src_ind ++;
        }
    }
   
    /* Initialize .bss section to zero */
    dst_ind = (uint32_t *) &__bss_start__;
    dst_end = (uint32_t *) &__bss_end__;
    if (dst_ind != dst_end) {
        for (; dst_ind < dst_end;) {
            *dst_ind ++ = 0;
        }
    }
    
    _start();
    
#endif

    /* Infinite loop */
    while (1);
}

/**
 * \brief Default interrupt handler for unused IRQs.
 */
void Default_Handler(void)
{
    while (1);
}
