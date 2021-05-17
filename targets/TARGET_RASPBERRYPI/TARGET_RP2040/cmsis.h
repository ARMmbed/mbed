#ifndef __RP2040_H__
#define __RP2040_H__

typedef enum IRQn
{
    /* ToDo: use this Cortex interrupt numbers if your device is a CORTEX-M0 device                   */
    NonMaskableInt_IRQn           = -14,      /*!<  2 Cortex-M0 Non Maskable Interrupt              */
    HardFault_IRQn                = -13,      /*!<  3 Cortex-M0 Hard Fault Interrupt                */
    SVCall_IRQn                   = -5,       /*!< 11 Cortex-M0 SV Call Interrupt                   */
    PendSV_IRQn                   = -2,       /*!< 14 Cortex-M0 Pend SV Interrupt                   */
    SysTick_IRQn                  = -1,       /*!< 15 Cortex-M0 System Tick Interrupt               */

    /* ----------------------  RP2040 Specific Interrupt Numbers  ------------------ */
    TIMER_IRQ_0n                  = 0,       /*!<     */
    TIMER_IRQ_1n                  = 1,       /*!<     */
    TIMER_IRQ_2n                  = 2,       /*!<     */
    TIMER_IRQ_3n                  = 3,       /*!<     */
    PWM_IRQ_WRAPn                 = 4,       /*!<     */
    USBCTRL_IRQn                  = 5,       /*!<     */
    XIP_IRQn                      = 6,       /*!<     */
    PIO0_IRQ_0n                   = 7,       /*!<     */
    PIO0_IRQ_1n                   = 8,       /*!<     */
    PIO1_IRQ_0n                   = 9,       /*!<     */
    PIO1_IRQ_1n                   = 10,      /*!<     */
    DMA_IRQ_0n                    = 11,      /*!<     */
    DMA_IRQ_1n                    = 12,      /*!<     */
    IO_IRQ_BANK0n                 = 13,      /*!<     */
    IO_IRQ_QSPIn                  = 14,      /*!<     */
    SIO_IRQ_PROC0n                = 15,      /*!<     */
    SIO_IRQ_PROC1n                = 16,      /*!<     */
    CLOCKS_IRQn                   = 17,      /*!<     */
    SPI0_IRQn                     = 18,      /*!<     */
    SPI1_IRQn                     = 19,      /*!<     */
    UART0_IRQn                    = 20,      /*!<     */
    UART1_IRQn                    = 21,      /*!<     */
    ADC_IRQ_FIFOn                 = 22,      /*!<     */
    I2C0_IRQn                     = 23,      /*!<     */
    I2C1_IRQn                     = 24,      /*!<     */
    RTC_IRQn                      = 25,      /*!<     */
} IRQn_Type;

/*
 * ==========================================================================
 * ----------- Processor and Core Peripheral Section ------------------------
 * ==========================================================================
 */

/* Configuration of the Cortex-M0+ Processor and Core Peripherals */
#define __CM0PLUS_REV             0x0000    /* Core revision r0p0                              */
#define __MPU_PRESENT             0         /* MPU present or not                              */
#define __VTOR_PRESENT            1         /* VTOR present or not                             */
#define __NVIC_PRIO_BITS          2         /* Number of Bits used for Priority Levels         */
#define __Vendor_SysTickConfig    0         /* Set to 1 if different SysTick Config is used    */

#include "core_cm0plus.h"
#include "cmsis_nvic.h"

extern uint32_t SystemCoreClock;     // System Clock Frequency (Core Clock)

#endif