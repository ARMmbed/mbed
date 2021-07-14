/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2017, STMicroelectronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

#if DEVICE_SERIAL

#include "serial_api_hal.h"

// Possible choices of the LPUART_CLOCK_SOURCE configuration set in json file
#define USE_LPUART_CLK_LSE    0x01
#define USE_LPUART_CLK_PCLK1  0x02
#define USE_LPUART_CLK_HSI    0x04

int stdio_uart_inited = 0; // used in platform/mbed_board.c and platform/mbed_retarget.cpp
serial_t stdio_uart;

extern UART_HandleTypeDef uart_handlers[];
extern uint32_t serial_irq_ids[];

// Utility functions
HAL_StatusTypeDef init_uart(serial_t *obj);
int8_t get_uart_index(UARTName uart_name);

#if STATIC_PINMAP_READY
#define SERIAL_INIT_DIRECT serial_init_direct
void serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap)
#else
#define SERIAL_INIT_DIRECT _serial_init_direct
static void _serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap)
#endif
{
    struct serial_s *obj_s = SERIAL_S(obj);

    // Get the peripheral name (UART_1, UART_2, ...) from the pin and assign it to the object
    obj_s->uart = (UARTName)pinmap->peripheral;
    MBED_ASSERT(obj_s->uart != (UARTName)NC);

    // Reset and enable clock
#if defined(USART1_BASE)
    if (obj_s->uart == UART_1) {
        __HAL_RCC_USART1_CLK_ENABLE();
    }
#endif

#if defined (USART2_BASE)
    if (obj_s->uart == UART_2) {
        __HAL_RCC_USART2_CLK_ENABLE();
    }
#endif

#if defined(USART3_BASE)
    if (obj_s->uart == UART_3) {
        __HAL_RCC_USART3_CLK_ENABLE();
    }
#endif

#if defined(UART4_BASE)
    if (obj_s->uart == UART_4) {
        __HAL_RCC_UART4_CLK_ENABLE();
    }
#endif

#if defined(USART4_BASE)
    if (obj_s->uart == UART_4) {
        __HAL_RCC_USART4_CLK_ENABLE();
    }
#endif

#if defined(UART5_BASE)
    if (obj_s->uart == UART_5) {
        __HAL_RCC_UART5_CLK_ENABLE();
    }
#endif

#if defined(USART5_BASE)
    if (obj_s->uart == UART_5) {
        __HAL_RCC_USART5_CLK_ENABLE();
    }
#endif

#if defined(USART6_BASE)
    if (obj_s->uart == UART_6) {
        __HAL_RCC_USART6_CLK_ENABLE();
    }
#endif

#if defined(UART7_BASE)
    if (obj_s->uart == UART_7) {
        __HAL_RCC_UART7_CLK_ENABLE();
    }
#endif

#if defined(USART7_BASE)
    if (obj_s->uart == UART_7) {
        __HAL_RCC_USART7_CLK_ENABLE();
    }
#endif

#if defined(UART8_BASE)
    if (obj_s->uart == UART_8) {
        __HAL_RCC_UART8_CLK_ENABLE();
    }
#endif

#if defined(USART8_BASE)
    if (obj_s->uart == UART_8) {
        __HAL_RCC_USART8_CLK_ENABLE();
    }
#endif

#if defined(UART9_BASE)
    if (obj_s->uart == UART_9) {
        __HAL_RCC_UART9_CLK_ENABLE();
    }
#endif

#if defined(UART10_BASE)
    if (obj_s->uart == UART_10) {
        __HAL_RCC_UART10_CLK_ENABLE();
    }
#endif

#if defined(LPUART1_BASE)
    if (obj_s->uart == LPUART_1) {
        __HAL_RCC_LPUART1_CLK_ENABLE();
    }
#endif

    // Assign serial object index
    obj_s->index = get_uart_index(obj_s->uart);
    MBED_ASSERT(obj_s->index >= 0);

    // Configure UART pins
    pin_function(pinmap->tx_pin, pinmap->tx_function);
    pin_mode(pinmap->tx_pin, PullUp);
    pin_function(pinmap->rx_pin, pinmap->rx_function);
    pin_mode(pinmap->rx_pin, PullUp);

    // Configure UART
    obj_s->baudrate = 9600; // baudrate default value
    if (pinmap->stdio_config) {
#if MBED_CONF_PLATFORM_STDIO_BAUD_RATE
        obj_s->baudrate = MBED_CONF_PLATFORM_STDIO_BAUD_RATE; // baudrate takes value from platform/mbed_lib.json
#endif /* MBED_CONF_PLATFORM_STDIO_BAUD_RATE */
    } else {
#if MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE
        obj_s->baudrate = MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE; // baudrate takes value from platform/mbed_lib.json
#endif /* MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE */
    }
    obj_s->databits = UART_WORDLENGTH_8B;
    obj_s->stopbits = UART_STOPBITS_1;
    obj_s->parity   = UART_PARITY_NONE;

#if DEVICE_SERIAL_FC
    obj_s->hw_flow_ctl = UART_HWCONTROL_NONE;
#endif

    obj_s->pin_tx = pinmap->tx_pin;
    obj_s->pin_rx = pinmap->rx_pin;

    init_uart(obj); /* init_uart will be called again in serial_baud function, so don't worry if init_uart returns HAL_ERROR */

    // For stdio management in platform/mbed_board.c and platform/mbed_retarget.cpp
    if (pinmap->stdio_config) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    uint32_t uart_tx = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t uart_rx = pinmap_peripheral(rx, PinMap_UART_RX);

    int peripheral = (int)pinmap_merge(uart_tx, uart_rx);

    int tx_function = (int)pinmap_find_function(tx, PinMap_UART_TX);
    int rx_function = (int)pinmap_find_function(rx, PinMap_UART_RX);

    uint8_t stdio_config = false;

    if ((tx == CONSOLE_TX) || (rx == CONSOLE_RX)) {
        stdio_config = true;
    } else {
        if (uart_tx == pinmap_peripheral(CONSOLE_TX, PinMap_UART_TX)) {
            error("Error: new serial object is using same UART as STDIO");
        }
    }

    const serial_pinmap_t explicit_uart_pinmap = {peripheral, tx, tx_function, rx, rx_function, stdio_config};

    SERIAL_INIT_DIRECT(obj, &explicit_uart_pinmap);
}

void serial_free(serial_t *obj)
{
    struct serial_s *obj_s = SERIAL_S(obj);

    // Reset UART and disable clock
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */
#if defined(USART1_BASE)
    if (obj_s->uart == UART_1) {
        __HAL_RCC_USART1_FORCE_RESET();
        __HAL_RCC_USART1_RELEASE_RESET();
        __HAL_RCC_USART1_CLK_DISABLE();
    }
#endif

#if defined(USART2_BASE)
    if (obj_s->uart == UART_2) {
        __HAL_RCC_USART2_FORCE_RESET();
        __HAL_RCC_USART2_RELEASE_RESET();
        __HAL_RCC_USART2_CLK_DISABLE();
    }
#endif

#if defined(USART3_BASE)
    if (obj_s->uart == UART_3) {
        __HAL_RCC_USART3_FORCE_RESET();
        __HAL_RCC_USART3_RELEASE_RESET();
        __HAL_RCC_USART3_CLK_DISABLE();
    }
#endif

#if defined(UART4_BASE)
    if (obj_s->uart == UART_4) {
        __HAL_RCC_UART4_FORCE_RESET();
        __HAL_RCC_UART4_RELEASE_RESET();
        __HAL_RCC_UART4_CLK_DISABLE();
    }
#endif

#if defined(USART4_BASE)
    if (obj_s->uart == UART_4) {
        __HAL_RCC_USART4_FORCE_RESET();
        __HAL_RCC_USART4_RELEASE_RESET();
        __HAL_RCC_USART4_CLK_DISABLE();
    }
#endif

#if defined(UART5_BASE)
    if (obj_s->uart == UART_5) {
        __HAL_RCC_UART5_FORCE_RESET();
        __HAL_RCC_UART5_RELEASE_RESET();
        __HAL_RCC_UART5_CLK_DISABLE();
    }
#endif

#if defined(USART5_BASE)
    if (obj_s->uart == UART_5) {
        __HAL_RCC_USART5_FORCE_RESET();
        __HAL_RCC_USART5_RELEASE_RESET();
        __HAL_RCC_USART5_CLK_DISABLE();
    }
#endif

#if defined(USART6_BASE)
    if (obj_s->uart == UART_6) {
        __HAL_RCC_USART6_FORCE_RESET();
        __HAL_RCC_USART6_RELEASE_RESET();
        __HAL_RCC_USART6_CLK_DISABLE();
    }
#endif

#if defined(UART7_BASE)
    if (obj_s->uart == UART_7) {
        __HAL_RCC_UART7_FORCE_RESET();
        __HAL_RCC_UART7_RELEASE_RESET();
        __HAL_RCC_UART7_CLK_DISABLE();
    }
#endif

#if defined(USART7_BASE)
    if (obj_s->uart == UART_7) {
        __HAL_RCC_USART7_FORCE_RESET();
        __HAL_RCC_USART7_RELEASE_RESET();
        __HAL_RCC_USART7_CLK_DISABLE();
    }
#endif

#if defined(UART8_BASE)
    if (obj_s->uart == UART_8) {
        __HAL_RCC_UART8_FORCE_RESET();
        __HAL_RCC_UART8_RELEASE_RESET();
        __HAL_RCC_UART8_CLK_DISABLE();
    }
#endif

#if defined(USART8_BASE)
    if (obj_s->uart == UART_8) {
        __HAL_RCC_USART8_FORCE_RESET();
        __HAL_RCC_USART8_RELEASE_RESET();
        __HAL_RCC_USART8_CLK_DISABLE();
    }
#endif

#if defined(UART9_BASE)
    if (obj_s->uart == UART_9) {
        __HAL_RCC_UART9_FORCE_RESET();
        __HAL_RCC_UART9_RELEASE_RESET();
        __HAL_RCC_UART9_CLK_DISABLE();
    }
#endif

#if defined(UART10_BASE)
    if (obj_s->uart == UART_10) {
        __HAL_RCC_UART10_FORCE_RESET();
        __HAL_RCC_UART10_RELEASE_RESET();
        __HAL_RCC_UART10_CLK_DISABLE();
    }
#endif

#if defined(LPUART1_BASE)
    if (obj_s->uart == LPUART_1) {
        __HAL_RCC_LPUART1_FORCE_RESET();
        __HAL_RCC_LPUART1_RELEASE_RESET();
        __HAL_RCC_LPUART1_CLK_DISABLE();
    }
#endif
#if defined(DUAL_CORE) && (TARGET_STM32H7)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */

    // Configure GPIOs back to reset value
    pin_function(obj_s->pin_tx, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));
    pin_function(obj_s->pin_rx, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));
#if DEVICE_SERIAL_FC
    if ((obj_s->hw_flow_ctl == UART_HWCONTROL_RTS) || (obj_s->hw_flow_ctl == UART_HWCONTROL_RTS_CTS)) {
        pin_function(obj_s->pin_rts, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));
    }
    if ((obj_s->hw_flow_ctl == UART_HWCONTROL_CTS) || (obj_s->hw_flow_ctl == UART_HWCONTROL_RTS_CTS)) {
        pin_function(obj_s->pin_cts, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));
    }
#endif

    serial_irq_ids[obj_s->index] = 0;
}

void serial_baud(serial_t *obj, int baudrate)
{
    struct serial_s *obj_s = SERIAL_S(obj);

    obj_s->baudrate = baudrate;

#if defined(LPUART1_BASE)
    /* Note that LPUART clock source must be in the range [3 x baud rate, 4096 x baud rate], check Ref Manual */
    if (obj_s->uart == LPUART_1) {
        RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
#if ((MBED_CONF_TARGET_LPUART_CLOCK_SOURCE) & USE_LPUART_CLK_LSE)
        if (baudrate <= 9600) {
            // Enable LSE in case it is not already done
            if (!__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY)) {
                RCC_OscInitTypeDef RCC_OscInitStruct = {0};
                RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
                RCC_OscInitStruct.LSEState       = RCC_LSE_ON;
                RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_OFF;
#if defined(DUAL_CORE) && (TARGET_STM32H7)
                while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
                }
#endif /* DUAL_CORE */
                HAL_RCC_OscConfig(&RCC_OscInitStruct);
#if defined(DUAL_CORE) && (TARGET_STM32H7)
                LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
            }
            // Keep it to verify if HAL_RCC_OscConfig didn't exit with a timeout
            if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY)) {
#if defined(DUAL_CORE) && (TARGET_STM32H7)
                while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
                }
#endif /* DUAL_CORE */
                PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_LSE;
                HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
#if defined(DUAL_CORE) && (TARGET_STM32H7)
                LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
                if (init_uart(obj) == HAL_OK) {
                    return;
                }
            }
        }
#endif
#if ((MBED_CONF_TARGET_LPUART_CLOCK_SOURCE) & USE_LPUART_CLK_PCLK1)
        PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
        if (init_uart(obj) == HAL_OK) {
            return;
        }
#endif
#if ((MBED_CONF_TARGET_LPUART_CLOCK_SOURCE) & USE_LPUART_CLK_HSI)
        // Enable HSI in case it is not already done
        if (!__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY)) {
            RCC_OscInitTypeDef RCC_OscInitStruct = {0};
            RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
            RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
            RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_OFF;
            RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
#if defined(DUAL_CORE) && (TARGET_STM32H7)
            while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
            }
#endif /* DUAL_CORE */
            HAL_RCC_OscConfig(&RCC_OscInitStruct);
#if defined(DUAL_CORE) && (TARGET_STM32H7)
            LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
        }
        // Keep it to verify if HAL_RCC_OscConfig didn't exit with a timeout
        if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY)) {
            PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_HSI;
#if defined(DUAL_CORE) && (TARGET_STM32H7)
            while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
            }
#endif /* DUAL_CORE */
            HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
#if defined(DUAL_CORE) && (TARGET_STM32H7)
            LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
            if (init_uart(obj) == HAL_OK) {
                return;
            }
        }
#endif
        // Last chance using SYSCLK
        PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_SYSCLK;
#if defined(DUAL_CORE) && (TARGET_STM32H7)
        while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
        }
#endif /* DUAL_CORE */
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
#if defined(DUAL_CORE) && (TARGET_STM32H7)
        LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */
    }
#endif /* LPUART1_BASE */

    if (init_uart(obj) != HAL_OK) {
        debug("Cannot initialize UART with baud rate %u\n", baudrate);
    }
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    struct serial_s *obj_s = SERIAL_S(obj);

    switch (parity) {
        case ParityOdd:
            obj_s->parity = UART_PARITY_ODD;
            break;
        case ParityEven:
            obj_s->parity = UART_PARITY_EVEN;
            break;
        default: // ParityNone
        case ParityForced0: // unsupported!
        case ParityForced1: // unsupported!
            obj_s->parity = UART_PARITY_NONE;
            break;
    }

    switch (data_bits) {
        case 7:
            if (parity != UART_PARITY_NONE) {
                obj_s->databits = UART_WORDLENGTH_8B;
            } else {
#if defined UART_WORDLENGTH_7B
                obj_s->databits = UART_WORDLENGTH_7B;
#else
                error("7-bit data format without parity is not supported");
#endif
            }
            break;
        case 8:
            if (parity != UART_PARITY_NONE) {
                obj_s->databits = UART_WORDLENGTH_9B;
            } else {
                obj_s->databits = UART_WORDLENGTH_8B;
            }
            break;
        case 9:
            if (parity != UART_PARITY_NONE) {
                error("Parity is not supported with 9-bit data format");
            } else {
                obj_s->databits = UART_WORDLENGTH_9B;
            }
            break;
        default:
            error("Only 7, 8 or 9-bit data formats are supported");
            break;
    }

    if (stop_bits == 2) {
        obj_s->stopbits = UART_STOPBITS_2;
    } else {
        obj_s->stopbits = UART_STOPBITS_1;
    }

    init_uart(obj);
}

const PinMap *serial_tx_pinmap()
{
    return PinMap_UART_TX;
}

const PinMap *serial_rx_pinmap()
{
    return PinMap_UART_RX;
}

const PinMap *serial_cts_pinmap()
{
#if !DEVICE_SERIAL_FC
    static const PinMap PinMap_UART_CTS[] = {
        {NC, NC, 0}
    };
#endif

    return PinMap_UART_CTS;
}

const PinMap *serial_rts_pinmap()
{
#if !DEVICE_SERIAL_FC
    static const PinMap PinMap_UART_RTS[] = {
        {NC, NC, 0}
    };
#endif

    return PinMap_UART_RTS;
}

/******************************************************************************
 * READ/WRITE
 ******************************************************************************/

int serial_readable(serial_t *obj)
{
    struct serial_s *obj_s = SERIAL_S(obj);
    UART_HandleTypeDef *huart = &uart_handlers[obj_s->index];
    /*  To avoid a target blocking case, let's check for
     *  possible OVERRUN error and discard it
     */
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }
    // Check if data is received
    return (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET) ? 1 : 0;
}

int serial_writable(serial_t *obj)
{
    struct serial_s *obj_s = SERIAL_S(obj);
    UART_HandleTypeDef *huart = &uart_handlers[obj_s->index];

    // Check if data is transmitted
    return (__HAL_UART_GET_FLAG(huart, UART_FLAG_TXE) != RESET) ? 1 : 0;
}

int serial_tx_empty(serial_t *obj)
{
    struct serial_s *obj_s = SERIAL_S(obj);
    UART_HandleTypeDef *huart = &uart_handlers[obj_s->index];

    if((__HAL_UART_GET_FLAG(huart, UART_FLAG_TXE) != RESET) && 
       (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) != RESET) ) {
       return 1;
    }
    return 0;
}

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_clear(serial_t *obj)
{
    (void)obj;
}

/******************************************************************************
 * UTILITY FUNCTIONS
 ******************************************************************************/

HAL_StatusTypeDef init_uart(serial_t *obj)
{
    struct serial_s *obj_s = SERIAL_S(obj);
    UART_HandleTypeDef *huart = &uart_handlers[obj_s->index];
    huart->Instance = (USART_TypeDef *)(obj_s->uart);

    huart->Init.BaudRate     = obj_s->baudrate;
    huart->Init.WordLength   = obj_s->databits;
    huart->Init.StopBits     = obj_s->stopbits;
    huart->Init.Parity       = obj_s->parity;
#if DEVICE_SERIAL_FC
    huart->Init.HwFlowCtl    = obj_s->hw_flow_ctl;
#else
    huart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
#endif
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    huart->TxXferCount       = 0;
    huart->TxXferSize        = 0;
    huart->RxXferCount       = 0;
    huart->RxXferSize        = 0;
#if defined(UART_ONE_BIT_SAMPLE_DISABLE) // F0/F3/F7/G0/H7/L0/L4/L5/WB
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
#endif
#if defined(UART_PRESCALER_DIV1) // G0/H7/L4/L5/WB
    huart->Init.ClockPrescaler = UART_PRESCALER_DIV1;
#endif
#if defined(UART_ADVFEATURE_NO_INIT) // F0/F3/F7/G0/H7/L0/L4//5/WB
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
#endif
#if defined(UART_FIFOMODE_DISABLE) // G0/H7/L4/L5/WB
    huart->FifoMode = UART_FIFOMODE_DISABLE;
#endif

    if (obj_s->pin_rx == NC) {
        huart->Init.Mode = UART_MODE_TX;
    } else if (obj_s->pin_tx == NC) {
        huart->Init.Mode = UART_MODE_RX;
    } else {
        huart->Init.Mode = UART_MODE_TX_RX;
    }

#if defined(LPUART1_BASE)
    if (huart->Instance == LPUART1) {
        if (obj_s->baudrate <= 9600) {
#if ((MBED_CONF_TARGET_LPUART_CLOCK_SOURCE) & USE_LPUART_CLK_LSE) && defined(USART_CR3_UCESM)
            HAL_UARTEx_EnableClockStopMode(huart);
#endif
            HAL_UARTEx_EnableStopMode(huart);
        } else {
#if defined(USART_CR3_UCESM)
            HAL_UARTEx_DisableClockStopMode(huart);
#endif
            HAL_UARTEx_DisableStopMode(huart);
        }
    }
#endif

    return HAL_UART_Init(huart);
}

int8_t get_uart_index(UARTName uart_name)
{
    uint8_t index = 0;

#if defined(USART1_BASE)
    if (uart_name == UART_1) {
        return index;
    }
    index++;
#endif

#if defined(USART2_BASE)
    if (uart_name == UART_2) {
        return index;
    }
    index++;
#endif

#if defined(USART3_BASE)
    if (uart_name == UART_3) {
        return index;
    }
    index++;
#endif

#if defined(UART4_BASE)
    if (uart_name == UART_4) {
        return index;
    }
    index++;
#endif

#if defined(USART4_BASE)
    if (uart_name == UART_4) {
        return index;
    }
    index++;
#endif

#if defined(UART5_BASE)
    if (uart_name == UART_5) {
        return index;
    }
    index++;
#endif

#if defined(USART5_BASE)
    if (uart_name == UART_5) {
        return index;
    }
    index++;
#endif

#if defined(USART6_BASE)
    if (uart_name == UART_6) {
        return index;
    }
    index++;
#endif

#if defined(UART7_BASE)
    if (uart_name == UART_7) {
        return index;
    }
    index++;
#endif

#if defined(USART7_BASE)
    if (uart_name == UART_7) {
        return index;
    }
    index++;
#endif

#if defined(UART8_BASE)
    if (uart_name == UART_8) {
        return index;
    }
    index++;
#endif

#if defined(USART8_BASE)
    if (uart_name == UART_8) {
        return index;
    }
    index++;
#endif

#if defined(UART9_BASE)
    if (uart_name == UART_9) {
        return index;
    }
    index++;
#endif

#if defined(UART10_BASE)
    if (uart_name == UART_10) {
        return index;
    }
    index++;
#endif

#if defined(LPUART1_BASE)
    if (uart_name == LPUART_1) {
        return index;
    }
    index++;
#endif

    return -1;
}

/* Function used to protect deep sleep while a serial transmission is on-going.
.* Returns 1 if there is at least 1 serial instance with an on-going transfer
 * and 0 otherwise.
*/
int serial_is_tx_ongoing(void)
{
    int TxOngoing = 0;

#if defined(USART1_BASE)
    if (LL_USART_IsEnabled(USART1) && !LL_USART_IsActiveFlag_TC(USART1)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART2_BASE)
    if (LL_USART_IsEnabled(USART2) && !LL_USART_IsActiveFlag_TC(USART2)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART3_BASE)
    if (LL_USART_IsEnabled(USART3) && !LL_USART_IsActiveFlag_TC(USART3)) {
        TxOngoing |= 1;
    }
#endif

#if defined(UART4_BASE)
    if (LL_USART_IsEnabled(UART4) && !LL_USART_IsActiveFlag_TC(UART4)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART4_BASE)
    if (LL_USART_IsEnabled(USART4) && !LL_USART_IsActiveFlag_TC(USART4)) {
        TxOngoing |= 1;
    }
#endif

#if defined(UART5_BASE)
    if (LL_USART_IsEnabled(UART5) && !LL_USART_IsActiveFlag_TC(UART5)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART5_BASE)
    if (LL_USART_IsEnabled(USART5) && !LL_USART_IsActiveFlag_TC(USART5)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART6_BASE)
    if (LL_USART_IsEnabled(USART6) && !LL_USART_IsActiveFlag_TC(USART6)) {
        TxOngoing |= 1;
    }
#endif

#if defined(UART7_BASE)
    if (LL_USART_IsEnabled(UART7) && !LL_USART_IsActiveFlag_TC(UART7)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART7_BASE)
    if (LL_USART_IsEnabled(USART7) && !LL_USART_IsActiveFlag_TC(USART7)) {
        TxOngoing |= 1;
    }
#endif

#if defined(UART8_BASE)
    if (LL_USART_IsEnabled(UART8) && !LL_USART_IsActiveFlag_TC(UART8)) {
        TxOngoing |= 1;
    }
#endif

#if defined(USART8_BASE)
    if (LL_USART_IsEnabled(USART8) && !LL_USART_IsActiveFlag_TC(USART8)) {
        TxOngoing |= 1;
    }
#endif

#if defined(UART9_BASE)
    if (LL_USART_IsEnabled(UART9) && !LL_USART_IsActiveFlag_TC(UART9)) {
        TxOngoing |= 1;
    }
#endif

#if defined(UART10_BASE)
    if (LL_USART_IsEnabled(UART10) && !LL_USART_IsActiveFlag_TC(UART10)) {
        TxOngoing |= 1;
    }
#endif

#if defined(LPUART1_BASE)
    if (LL_USART_IsEnabled(LPUART1) && !LL_USART_IsActiveFlag_TC(LPUART1)) {
        TxOngoing |= 1;
    }
#endif

    return TxOngoing;
}

#else

int serial_is_tx_ongoing(void)
{
    return 0;
}

#endif /* DEVICE_SERIAL */
