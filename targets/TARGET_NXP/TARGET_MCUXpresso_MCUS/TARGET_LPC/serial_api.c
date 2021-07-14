/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
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
#include "fsl_usart.h"
#include "PeripheralPins.h"
#include "clock_config.h"
#include "gpio_api.h"

static uint32_t serial_irq_ids[FSL_FEATURE_SOC_USART_COUNT] = {0};
static uart_irq_handler irq_handler;
/* Array of UART peripheral base address. */
static USART_Type *const uart_addrs[] = USART_BASE_PTRS;

int stdio_uart_inited = 0;
serial_t stdio_uart;

#if STATIC_PINMAP_READY
#define SERIAL_INIT_DIRECT serial_init_direct
void serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap)
#else
#define SERIAL_INIT_DIRECT _serial_init_direct
static void _serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap)
#endif
{
    obj->index = (uint32_t)pinmap->peripheral;
    MBED_ASSERT((int)obj->index != NC);

    usart_config_t config;

    switch (obj->index) {
        case 0:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0);
            RESET_PeripheralReset(kFC0_RST_SHIFT_RSTn);
            break;
        case 1:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM1);
            RESET_PeripheralReset(kFC1_RST_SHIFT_RSTn);
            break;
        case 2:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);
            RESET_PeripheralReset(kFC2_RST_SHIFT_RSTn);
            break;
        case 3:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3);
            RESET_PeripheralReset(kFC3_RST_SHIFT_RSTn);
            break;
        case 4:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM4);
            RESET_PeripheralReset(kFC4_RST_SHIFT_RSTn);
            break;
        case 5:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM5);
            RESET_PeripheralReset(kFC5_RST_SHIFT_RSTn);
            break;
        case 6:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM6);
            RESET_PeripheralReset(kFC6_RST_SHIFT_RSTn);
            break;
        case 7:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM7);
            RESET_PeripheralReset(kFC7_RST_SHIFT_RSTn);
            break;
#if (FSL_FEATURE_SOC_USART_COUNT > 8U)
        case 8:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM8);
            RESET_PeripheralReset(kFC8_RST_SHIFT_RSTn);
            break;
#endif
#if (FSL_FEATURE_SOC_USART_COUNT > 9U)
        case 9:
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM9);
            RESET_PeripheralReset(kFC9_RST_SHIFT_RSTn);
            break;
#endif
    }

    USART_GetDefaultConfig(&config);
    config.baudRate_Bps = 9600;
    config.enableTx = true;
    config.enableRx = true;

    USART_Init(uart_addrs[obj->index], &config, 12000000);

    pin_function(pinmap->tx_pin, pinmap->tx_function);
    pin_function(pinmap->rx_pin, pinmap->rx_function);

    if (pinmap->tx_pin != NC) {
        pin_mode(pinmap->tx_pin, PullUp);
    }
    if (pinmap->rx_pin != NC) {
        pin_mode(pinmap->rx_pin, PullUp);
    }

    if (obj->index == STDIO_UART) {
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

    const serial_pinmap_t explicit_uart_pinmap = {peripheral, tx, tx_function, rx, rx_function, false};

    SERIAL_INIT_DIRECT(obj, &explicit_uart_pinmap);
}

void serial_free(serial_t *obj)
{
    USART_Deinit(uart_addrs[obj->index]);
    serial_irq_ids[obj->index] = 0;
}

void serial_baud(serial_t *obj, int baudrate)
{
    USART_SetBaudRate(uart_addrs[obj->index], (uint32_t)baudrate, 12000000);
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    USART_Type *base = uart_addrs[obj->index];
    uint8_t temp;

    /* Set bit count and parity mode. */
    temp = base->CFG & ~(USART_CFG_PARITYSEL_MASK | USART_CFG_DATALEN_MASK | USART_CFG_STOPLEN_MASK);

    if (parity != ParityNone)
    {
        /* Enable Parity */
        if (parity == ParityOdd) {
            temp |= USART_CFG_PARITYSEL(3U);
        } else if (parity == ParityEven) {
            temp |= USART_CFG_PARITYSEL(2U);
        } else {
            // Hardware does not support forced parity
            MBED_ASSERT(0);
        }
    }

    /* Set stop bits */
    if (stop_bits == 2) {
        temp |= USART_CFG_STOPLEN(1U);
    }

    /* Set Data size */
    if (data_bits == 8) {
        temp |= USART_CFG_DATALEN(1U);
    } else {
        temp |= USART_CFG_DATALEN(2U);
    }

    base->CFG = temp;
}

/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/
static inline void uart_irq(uint32_t transmit_empty, uint32_t receive_not_empty, uint32_t index)
{
    if (serial_irq_ids[index] != 0) {
        if (transmit_empty && (uart_addrs[index]->FIFOINTENSET & kUSART_TxLevelInterruptEnable))
            irq_handler(serial_irq_ids[index], TxIrq);

        if (receive_not_empty && (uart_addrs[index]->FIFOINTENSET & kUSART_RxLevelInterruptEnable))
            irq_handler(serial_irq_ids[index], RxIrq);
    }
}

void uart0_irq()
{
    uint32_t status_flags = USART0->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 0);
}

void uart1_irq()
{
    uint32_t status_flags = USART1->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 1);
}

void uart2_irq()
{
    uint32_t status_flags = USART2->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 2);
}

void uart3_irq()
{
    uint32_t status_flags = USART3->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 3);
}

void uart4_irq()
{
    uint32_t status_flags = USART4->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 4);
}

void uart5_irq()
{
    uint32_t status_flags = USART5->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 5);
}

void uart6_irq()
{
    uint32_t status_flags = USART6->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 6);
}

void uart7_irq()
{
    uint32_t status_flags = USART7->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 7);
}

#if (FSL_FEATURE_SOC_USART_COUNT > 8U)
void uart8_irq()
{
    uint32_t status_flags = USART8->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 8);
}
#endif

#if (FSL_FEATURE_SOC_USART_COUNT > 9U)
void uart9_irq()
{
    uint32_t status_flags = USART9->FIFOSTAT;
    uart_irq((status_flags & kUSART_TxFifoEmptyFlag), (status_flags & kUSART_RxFifoNotEmptyFlag), 9);
}
#endif

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    serial_irq_ids[obj->index] = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    IRQn_Type uart_irqs[] = USART_IRQS;
    uint32_t vector = 0;

    switch (obj->index) {
        case 0:
            vector = (uint32_t)&uart0_irq;
            break;
        case 1:
            vector = (uint32_t)&uart1_irq;
            break;
        case 2:
            vector = (uint32_t)&uart2_irq;
            break;
        case 3:
            vector = (uint32_t)&uart3_irq;
            break;
        case 4:
            vector = (uint32_t)&uart4_irq;
            break;
        case 5:
            vector = (uint32_t)&uart5_irq;
            break;
        case 6:
            vector = (uint32_t)&uart6_irq;
            break;
        case 7:
            vector = (uint32_t)&uart7_irq;
            break;
#if (FSL_FEATURE_SOC_USART_COUNT > 8U)
        case 8:
            vector = (uint32_t)&uart8_irq;
            break;
#endif
#if (FSL_FEATURE_SOC_USART_COUNT > 9U)
        case 9:
            vector = (uint32_t)&uart9_irq;
            break;
#endif
        default:
            break;
    }

    if (enable) {
        switch (irq) {
            case RxIrq:
                USART_EnableInterrupts(uart_addrs[obj->index], kUSART_RxLevelInterruptEnable);
                break;
            case TxIrq:
                USART_EnableInterrupts(uart_addrs[obj->index], kUSART_TxLevelInterruptEnable);
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
                USART_DisableInterrupts(uart_addrs[obj->index], kUSART_RxLevelInterruptEnable);
                break;
            case TxIrq:
                USART_DisableInterrupts(uart_addrs[obj->index], kUSART_TxLevelInterruptEnable);
                break;
            default:
                break;
        }
        switch (other_irq) {
            case RxIrq:
                all_disabled = (((uart_addrs[obj->index]->FIFOINTENSET) & kUSART_RxLevelInterruptEnable) == 0);
                break;
            case TxIrq:
                all_disabled = (((uart_addrs[obj->index]->FIFOINTENSET) & kUSART_TxLevelInterruptEnable)== 0);
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
    while (!serial_readable(obj));
    uint8_t data;
    data = USART_ReadByte(uart_addrs[obj->index]);

    return data;
}

void serial_putc(serial_t *obj, int c)
{
    while (!serial_writable(obj));
    USART_WriteByte(uart_addrs[obj->index], (uint8_t)c);
}

int serial_readable(serial_t *obj)
{
    uint32_t status_flags = USART_GetStatusFlags(uart_addrs[obj->index]);

    return (status_flags & kUSART_RxFifoNotEmptyFlag);
}

int serial_writable(serial_t *obj)
{
    uint32_t status_flags = USART_GetStatusFlags(uart_addrs[obj->index]);

    return (status_flags & kUSART_TxFifoNotFullFlag);
}

int serial_tx_empty(serial_t *obj)
{
    uint32_t status_flags = USART_GetStatusFlags(uart_addrs[obj->index]);

    return (status_flags & kUSART_TxFifoEmptyFlag);
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
    uart_addrs[obj->index]->CTL |= USART_CTL_TXBRKEN_MASK;
}

void serial_break_clear(serial_t *obj)
{
    uart_addrs[obj->index]->CTL &= ~USART_CTL_TXBRKEN_MASK;
}

#if DEVICE_SERIAL_FC
/*
 * Only hardware flow control is implemented in this API.
 */
#if STATIC_PINMAP_READY
#define SERIAL_SET_FC_DIRECT serial_set_flow_control_direct
void serial_set_flow_control_direct(serial_t *obj, FlowControl type, const serial_fc_pinmap_t *pinmap)
#else
#define SERIAL_SET_FC_DIRECT _serial_set_flow_control_direct
static void _serial_set_flow_control_direct(serial_t *obj, FlowControl type, const serial_fc_pinmap_t *pinmap)
#endif
{
    gpio_t gpio;

    switch(type) {
        case FlowControlRTS:
            pin_function(pinmap->rx_flow_pin, pinmap->rx_flow_function);
            pin_mode(pinmap->rx_flow_pin, PullNone);
            uart_addrs[obj->index]->CFG &= ~USART_CFG_CTSEN_MASK;
            break;

        case FlowControlCTS:
            /* Do not use RTS, configure pin to GPIO input */
            if (pinmap->rx_flow_pin != NC) {
                gpio_init(&gpio, pinmap->rx_flow_pin);
                gpio_dir(&gpio, PIN_INPUT);
            }

            pin_function(pinmap->tx_flow_pin, pinmap->tx_flow_function);
            pin_mode(pinmap->tx_flow_pin, PullNone);
            uart_addrs[obj->index]->CFG |= USART_CFG_CTSEN_MASK;
            break;

        case FlowControlRTSCTS:
            pin_function(pinmap->rx_flow_pin, pinmap->rx_flow_function);
            pin_mode(pinmap->rx_flow_pin, PullNone);
            pin_function(pinmap->tx_flow_pin, pinmap->tx_flow_function);
            pin_mode(pinmap->tx_flow_pin, PullNone);
            uart_addrs[obj->index]->CFG |= USART_CFG_CTSEN_MASK;
            break;

        case FlowControlNone:
            /* Do not use RTS, configure pin to GPIO input */
            if (pinmap->rx_flow_pin != NC) {
                gpio_init(&gpio, pinmap->rx_flow_pin);
                gpio_dir(&gpio, PIN_INPUT);
            }

            uart_addrs[obj->index]->CFG &= ~USART_CFG_CTSEN_MASK;
            break;

        default:
            break;
    }
}

void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    int tx_flow_function = (int)pinmap_find_function(txflow, PinMap_UART_CTS);
    int rx_flow_function = (int)pinmap_find_function(rxflow, PinMap_UART_RTS);

    const serial_fc_pinmap_t explicit_uart_fc_pinmap = {0, txflow, tx_flow_function, rxflow, rx_flow_function};

    SERIAL_SET_FC_DIRECT(obj, type, &explicit_uart_fc_pinmap);
}

#endif
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
    return PinMap_UART_CTS;
}

const PinMap *serial_rts_pinmap()
{
    return PinMap_UART_RTS;
}

#endif
