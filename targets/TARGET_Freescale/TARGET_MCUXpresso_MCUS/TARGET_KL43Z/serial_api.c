/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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
#include "serial_api.h"

#if DEVICE_SERIAL

// math.h required for floating point operations for baud rate calculation
#include <math.h>
#include "mbed_assert.h"

#include <string.h>

#include "cmsis.h"
#include "pinmap.h"
#include "fsl_lpuart.h"
#include "peripheral_clock_defines.h"
#include "PeripheralPins.h"
#include "fsl_clock_config.h"

static uint32_t serial_irq_ids[FSL_FEATURE_SOC_LPUART_COUNT] = {0};
static uart_irq_handler irq_handler;
/* Array of UART peripheral base address. */
static LPUART_Type *const uart_addrs[] = LPUART_BASE_PTRS;
/* Array of LPUART bus clock frequencies */
static clock_name_t const uart_clocks[] = LPUART_CLOCK_FREQS;

int stdio_uart_inited = 0;
serial_t stdio_uart;

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    uint32_t uart_tx = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t uart_rx = pinmap_peripheral(rx, PinMap_UART_RX);
    obj->index = pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)obj->index != NC);

    /* Set the LPUART clock source */
    if (obj->index == LPUART_0) {
        CLOCK_SetLpuart0Clock(1U);
    } else {
        CLOCK_SetLpuart1Clock(1U);
    }

    lpuart_config_t config;
    LPUART_GetDefaultConfig(&config);
    config.baudRate_Bps = 9600;
    config.enableTx = false;
    config.enableRx = false;

    LPUART_Init(uart_addrs[obj->index], &config, CLOCK_GetFreq(uart_clocks[obj->index]));

    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);

    if (tx != NC) {
        LPUART_EnableTx(uart_addrs[obj->index], true);
        pin_mode(tx, PullUp);
    }
    if (rx != NC) {
        LPUART_EnableRx(uart_addrs[obj->index], true);
        pin_mode(rx, PullUp);
    }

    if (obj->index == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_free(serial_t *obj)
{
    LPUART_Deinit(uart_addrs[obj->index]);
    serial_irq_ids[obj->index] = 0;
}

void serial_baud(serial_t *obj, int baudrate)
{
    LPUART_SetBaudRate(uart_addrs[obj->index], (uint32_t)baudrate, CLOCK_GetFreq(uart_clocks[obj->index]));
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    LPUART_Type *base = uart_addrs[obj->index];
    uint8_t temp;
    /* Set bit count and parity mode. */
    temp = base->CTRL & ~(LPUART_CTRL_PE_MASK | LPUART_CTRL_PT_MASK | LPUART_CTRL_M_MASK);
    if (parity != ParityNone)
    {
        /* Enable Parity */
        temp |= (LPUART_CTRL_PE_MASK | LPUART_CTRL_M_MASK);
        if (parity == ParityOdd) {
            temp |= LPUART_CTRL_PT_MASK;
        } else if (parity == ParityEven) {
            // PT=0 so nothing more to do
        } else {
            // Hardware does not support forced parity
            MBED_ASSERT(0);
        }
    }
    base->CTRL = temp;

#if defined(FSL_FEATURE_LPUART_HAS_STOP_BIT_CONFIG_SUPPORT) && FSL_FEATURE_LPUART_HAS_STOP_BIT_CONFIG_SUPPORT
    /* set stop bit per char */
    temp = base->BAUD & ~LPUART_BAUD_SBNS_MASK;
    base->BAUD = temp | LPUART_BAUD_SBNS((uint8_t)--stop_bits);
#endif
}

/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/
static inline void uart_irq(uint32_t transmit_empty, uint32_t receive_full, uint32_t index)
{
    LPUART_Type *base = uart_addrs[index];

    /* If RX overrun. */
    if (LPUART_STAT_OR_MASK & base->STAT)
    {
        /* Read base->D, otherwise the RX does not work. */
        (void)base->DATA;
        LPUART_ClearStatusFlags(base, kLPUART_RxOverrunFlag);
    }

    if (serial_irq_ids[index] != 0) {
        if (transmit_empty && (LPUART_GetEnabledInterrupts(uart_addrs[index]) & kLPUART_TxDataRegEmptyInterruptEnable))
            irq_handler(serial_irq_ids[index], TxIrq);

        if (receive_full && (LPUART_GetEnabledInterrupts(uart_addrs[index]) & kLPUART_RxDataRegFullInterruptEnable))
            irq_handler(serial_irq_ids[index], RxIrq);
    }
}

void uart0_irq()
{
    uint32_t status_flags = LPUART0->STAT;
    uart_irq((status_flags & kLPUART_TxDataRegEmptyFlag), (status_flags & kLPUART_RxDataRegFullFlag), 0);
}

void uart1_irq()
{
    uint32_t status_flags = LPUART1->STAT;
    uart_irq((status_flags & kLPUART_TxDataRegEmptyFlag), (status_flags & kLPUART_RxDataRegFullFlag), 1);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    serial_irq_ids[obj->index] = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    IRQn_Type uart_irqs[] = LPUART_RX_TX_IRQS;
    uint32_t vector = 0;

    switch (obj->index) {
        case 0:
            vector = (uint32_t)&uart0_irq;
            break;
        case 1:
            vector = (uint32_t)&uart1_irq;
            break;
        default:
            break;
    }

    if (enable) {
        switch (irq) {
            case RxIrq:
                LPUART_EnableInterrupts(uart_addrs[obj->index], kLPUART_RxDataRegFullInterruptEnable);
                break;
            case TxIrq:
                LPUART_EnableInterrupts(uart_addrs[obj->index], kLPUART_TxDataRegEmptyInterruptEnable);
                break;
            default:
                break;
        }
        NVIC_SetVector(uart_irqs[obj->index], vector);
        NVIC_EnableIRQ(uart_irqs[obj->index]);

    } else { // disable
        int all_disabled = 0;
        SerialIrq other_irq = (irq == RxIrq) ? (TxIrq) : (RxIrq);
        switch (irq) {
            case RxIrq:
                LPUART_DisableInterrupts(uart_addrs[obj->index], kLPUART_RxDataRegFullInterruptEnable);
                break;
            case TxIrq:
                LPUART_DisableInterrupts(uart_addrs[obj->index], kLPUART_TxDataRegEmptyInterruptEnable);
                break;
            default:
                break;
        }
        switch (other_irq) {
            case RxIrq:
                all_disabled = ((LPUART_GetEnabledInterrupts(uart_addrs[obj->index]) & kLPUART_RxDataRegFullInterruptEnable) == 0);
                break;
            case TxIrq:
                all_disabled = ((LPUART_GetEnabledInterrupts(uart_addrs[obj->index]) & kLPUART_TxDataRegEmptyInterruptEnable) == 0);
                break;
            default:
                break;
        }
        if (all_disabled)
            NVIC_DisableIRQ(uart_irqs[obj->index]);
    }
}

int serial_getc(serial_t *obj)
{
    uint8_t data;

    LPUART_ReadBlocking(uart_addrs[obj->index], &data, 1);
    return data;
}

void serial_putc(serial_t *obj, int c)
{
    while (!serial_writable(obj));
    LPUART_WriteByte(uart_addrs[obj->index], (uint8_t)c);
}

int serial_readable(serial_t *obj)
{
    uint32_t status_flags = LPUART_GetStatusFlags(uart_addrs[obj->index]);
    if (status_flags & kLPUART_RxOverrunFlag)
        LPUART_ClearStatusFlags(uart_addrs[obj->index], kLPUART_RxOverrunFlag);
    return (status_flags & kLPUART_RxDataRegFullFlag);
}

int serial_writable(serial_t *obj)
{
    uint32_t status_flags = LPUART_GetStatusFlags(uart_addrs[obj->index]);
    if (status_flags & kLPUART_RxOverrunFlag)
        LPUART_ClearStatusFlags(uart_addrs[obj->index], kLPUART_RxOverrunFlag);
    return (status_flags & kLPUART_TxDataRegEmptyFlag);
}

int serial_tx_empty(serial_t *obj)
{
    uint32_t status_flags = LPUART_GetStatusFlags(uart_addrs[obj->index]);
    return ((status_flags & kLPUART_TxDataRegEmptyFlag) && (status_flags & kLPUART_TransmissionCompleteFlag));
}

void serial_clear(serial_t *obj)
{
}

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj)
{
    uart_addrs[obj->index]->CTRL |= LPUART_CTRL_SBK_MASK;
}

void serial_break_clear(serial_t *obj)
{
    uart_addrs[obj->index]->CTRL &= ~LPUART_CTRL_SBK_MASK;
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

static int serial_is_enabled(uint32_t uart_index)
{
    int clock_enabled = 0;
    switch (uart_index) {
        case 0:
            clock_enabled = (SIM->SCGC5 & SIM_SCGC5_LPUART0_MASK) >> SIM_SCGC5_LPUART0_SHIFT;
            break;
        case 1:
            clock_enabled = (SIM->SCGC5 & SIM_SCGC5_LPUART1_MASK) >> SIM_SCGC5_LPUART1_SHIFT;
            break;
        default:
            break;
    }

    return clock_enabled;
}

bool serial_check_tx_ongoing()
{
    LPUART_Type *base;
    int i;
    bool uart_tx_ongoing = false;

    for (i = 0; i < FSL_FEATURE_SOC_LPUART_COUNT; i++) {
        /* First check if UART is enabled */
        if (!serial_is_enabled(i)) {
            /* UART is not enabled, check the next instance */
            continue;
        }

        base = uart_addrs[i];

        /* Check if data is waiting to be written out of transmit buffer */
        if (!(kLPUART_TransmissionCompleteFlag & LPUART_GetStatusFlags((LPUART_Type *)base))) {
            uart_tx_ongoing = true;
            break;
        }
    }

    return uart_tx_ongoing;
}

#endif
