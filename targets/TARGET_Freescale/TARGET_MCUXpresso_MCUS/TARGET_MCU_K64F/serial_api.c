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
#include "mbed_power_mgmt.h"
#include "pinmap.h"
#include "fsl_uart.h"
#include "peripheral_clock_defines.h"
#include "PeripheralPins.h"
#include "dma_reqs.h"
#include "fsl_clock_config.h"

static uint32_t serial_irq_ids[FSL_FEATURE_SOC_UART_COUNT] = {0};
static uart_irq_handler irq_handler;
/* Array of UART peripheral base address. */
static UART_Type *const uart_addrs[] = UART_BASE_PTRS;
/* Array of UART bus clock frequencies */
static clock_name_t const uart_clocks[] = UART_CLOCK_FREQS;

/* UART transfer states */
#define kUART_TxIdle    0
#define kUART_TxBusy    1
#define kUART_RxIdle    2
#define kUART_RxBusy    3

int stdio_uart_inited = 0;
serial_t stdio_uart;

void serial_init_direct(serial_t *obj, const serial_pinmap_t *pinmap)
{
    obj->serial.index = (uint32_t)pinmap->peripheral;
    MBED_ASSERT((int)obj->serial.index != NC);

    uart_config_t config;

    UART_GetDefaultConfig(&config);
    config.baudRate_Bps = 9600;
    config.enableTx = false;
    config.enableRx = false;

    UART_Init(uart_addrs[obj->serial.index], &config, CLOCK_GetFreq(uart_clocks[obj->serial.index]));


    if (pinmap->tx_pin != NC) {
        pin_function(pinmap->tx_pin, pinmap->tx_function);
        UART_EnableTx(uart_addrs[obj->serial.index], true);
        pin_mode(pinmap->tx_pin, PullUp);
    }
    if (pinmap->rx_pin != NC) {
        pin_function(pinmap->rx_pin, pinmap->rx_function);
        UART_EnableRx(uart_addrs[obj->serial.index], true);
        pin_mode(pinmap->rx_pin, PullUp);
    }

    if (obj->serial.index == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }

    obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;;
    obj->serial.txstate = kUART_TxIdle;
    obj->serial.rxstate = kUART_RxIdle;

    /* Zero the handle. */
    memset(&(obj->serial.uart_transfer_handle), 0, sizeof(obj->serial.uart_transfer_handle));
}

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    uint32_t uart_tx = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t uart_rx = pinmap_peripheral(rx, PinMap_UART_RX);

    int peripheral = (int)pinmap_merge(uart_tx, uart_rx);

    int tx_function = (int)pinmap_find_function(tx, PinMap_UART_TX);
    int rx_function = (int)pinmap_find_function(rx, PinMap_UART_RX);

    const serial_pinmap_t explicit_uart_pinmap = {peripheral, tx, tx_function, rx, rx_function, 0};

    serial_init_direct(obj, &explicit_uart_pinmap);
}

void serial_free(serial_t *obj)
{
    UART_Deinit(uart_addrs[obj->serial.index]);
    serial_irq_ids[obj->serial.index] = 0;
}

void serial_baud(serial_t *obj, int baudrate)
{
    UART_SetBaudRate(uart_addrs[obj->serial.index], (uint32_t)baudrate, CLOCK_GetFreq(uart_clocks[obj->serial.index]));
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    UART_Type *base = uart_addrs[obj->serial.index];
    uint8_t temp;
    /* Set bit count and parity mode. */
    temp = base->C1 & ~(UART_C1_PE_MASK | UART_C1_PT_MASK | UART_C1_M_MASK);
    if (parity != ParityNone) {
        /* Enable Parity */
        temp |= (UART_C1_PE_MASK | UART_C1_M_MASK);
        if (parity == ParityOdd) {
            temp |= UART_C1_PT_MASK;
        } else if (parity == ParityEven) {
            // PT=0 so nothing more to do
        } else {
            // Hardware does not support forced parity
            MBED_ASSERT(0);
        }
    }
    base->C1 = temp;
#if defined(FSL_FEATURE_UART_HAS_STOP_BIT_CONFIG_SUPPORT) && FSL_FEATURE_UART_HAS_STOP_BIT_CONFIG_SUPPORT
    /* Set stop bit per char */
    base->BDH = (base->BDH & ~UART_BDH_SBNS_MASK) | UART_BDH_SBNS((uint8_t)--stop_bits);
#endif
}

/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/
static inline void uart_irq(uint32_t transmit_empty, uint32_t receive_full, uint32_t index)
{
    UART_Type *base = uart_addrs[index];

    /* If RX overrun. */
    if (UART_S1_OR_MASK & base->S1) {
        /* Read base->D, otherwise the RX does not work. */
        (void)base->D;
    }

    if (serial_irq_ids[index] != 0) {
        if (transmit_empty && (UART_GetEnabledInterrupts(uart_addrs[index]) & kUART_TxDataRegEmptyInterruptEnable)) {
            irq_handler(serial_irq_ids[index], TxIrq);
        }

        if (receive_full && (UART_GetEnabledInterrupts(uart_addrs[index]) & kUART_RxDataRegFullInterruptEnable)) {
            irq_handler(serial_irq_ids[index], RxIrq);
        }
    }
}

void uart0_irq()
{
    uint32_t status_flags = UART0->S1;
    uart_irq((status_flags & kUART_TxDataRegEmptyFlag), (status_flags & kUART_RxDataRegFullFlag), 0);
}

void uart1_irq()
{
    uint32_t status_flags = UART1->S1;
    uart_irq((status_flags & UART_S1_TDRE_MASK), (status_flags & UART_S1_RDRF_MASK), 1);
}

void uart2_irq()
{
    uint32_t status_flags = UART2->S1;
    uart_irq((status_flags & UART_S1_TDRE_MASK), (status_flags & UART_S1_RDRF_MASK), 2);
}

void uart3_irq()
{
    uint32_t status_flags = UART3->S1;
    uart_irq((status_flags & UART_S1_TDRE_MASK), (status_flags & UART_S1_RDRF_MASK), 3);
}

void uart4_irq()
{
    uint32_t status_flags = UART4->S1;
    uart_irq((status_flags & UART_S1_TDRE_MASK), (status_flags & UART_S1_RDRF_MASK), 4);
}

void uart5_irq()
{
    uint32_t status_flags = UART5->S1;
    uart_irq((status_flags & UART_S1_TDRE_MASK), (status_flags & UART_S1_RDRF_MASK), 5);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    serial_irq_ids[obj->serial.index] = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    IRQn_Type uart_irqs[] = UART_RX_TX_IRQS;
    uint32_t vector = 0;

    switch (obj->serial.index) {
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
        default:
            break;
    }

    if (enable) {
        switch (irq) {
            case RxIrq:
                UART_EnableInterrupts(uart_addrs[obj->serial.index], kUART_RxDataRegFullInterruptEnable);
                break;
            case TxIrq:
                UART_EnableInterrupts(uart_addrs[obj->serial.index], kUART_TxDataRegEmptyInterruptEnable);
                break;
            default:
                break;
        }
        NVIC_SetVector(uart_irqs[obj->serial.index], vector);
        NVIC_EnableIRQ(uart_irqs[obj->serial.index]);

    } else { // disable
        int all_disabled = 0;
        SerialIrq other_irq = (irq == RxIrq) ? (TxIrq) : (RxIrq);
        switch (irq) {
            case RxIrq:
                UART_DisableInterrupts(uart_addrs[obj->serial.index], kUART_RxDataRegFullInterruptEnable);
                break;
            case TxIrq:
                UART_DisableInterrupts(uart_addrs[obj->serial.index], kUART_TxDataRegEmptyInterruptEnable);
                break;
            default:
                break;
        }
        switch (other_irq) {
            case RxIrq:
                all_disabled = ((UART_GetEnabledInterrupts(uart_addrs[obj->serial.index]) & kUART_RxDataRegFullInterruptEnable) == 0);
                break;
            case TxIrq:
                all_disabled = ((UART_GetEnabledInterrupts(uart_addrs[obj->serial.index]) & kUART_TxDataRegEmptyInterruptEnable) == 0);
                break;
            default:
                break;
        }
        if (all_disabled) {
            NVIC_DisableIRQ(uart_irqs[obj->serial.index]);
        }
    }
}

int serial_getc(serial_t *obj)
{
    while (!serial_readable(obj));
    uint8_t data;
    data = UART_ReadByte(uart_addrs[obj->serial.index]);

    return data;
}

void serial_putc(serial_t *obj, int c)
{
    while (!serial_writable(obj));
    UART_WriteByte(uart_addrs[obj->serial.index], (uint8_t)c);
}

int serial_readable(serial_t *obj)
{
    uint32_t status_flags = UART_GetStatusFlags(uart_addrs[obj->serial.index]);
    if (status_flags & kUART_RxOverrunFlag) {
        UART_ClearStatusFlags(uart_addrs[obj->serial.index], kUART_RxOverrunFlag);
    }
    return (status_flags & kUART_RxDataRegFullFlag);
}

int serial_writable(serial_t *obj)
{
    uint32_t status_flags = UART_GetStatusFlags(uart_addrs[obj->serial.index]);
    if (status_flags & kUART_RxOverrunFlag) {
        UART_ClearStatusFlags(uart_addrs[obj->serial.index], kUART_RxOverrunFlag);
    }
    return (status_flags & kUART_TxDataRegEmptyFlag);
}

int serial_tx_empty(serial_t *obj)
{
    uint32_t status_flags = UART_GetStatusFlags(uart_addrs[obj->serial.index]);
    return ((status_flags & kUART_TxDataRegEmptyFlag) && (status_flags & kUART_TransmissionCompleteFlag));
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
    uart_addrs[obj->serial.index]->C2 |= UART_C2_SBK_MASK;
}

void serial_break_clear(serial_t *obj)
{
    uart_addrs[obj->serial.index]->C2 &= ~UART_C2_SBK_MASK;
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
    return PinMap_UART_CTS;
}

const PinMap *serial_rts_pinmap()
{
    return PinMap_UART_RTS;
}

#if DEVICE_SERIAL_FC

/*
 * Only hardware flow control is implemented in this API.
 */
void serial_set_flow_control_direct(serial_t *obj, FlowControl type, const serial_fc_pinmap_t *pinmap)
{
    switch (type) {
        case FlowControlRTS:
            pin_function(pinmap->rx_flow_pin, pinmap->rx_flow_function);
            pin_mode(pinmap->rx_flow_pin, PullNone);
            uart_addrs[obj->serial.index]->MODEM &= ~UART_MODEM_TXCTSE_MASK;
            uart_addrs[obj->serial.index]->MODEM |= UART_MODEM_RXRTSE_MASK;
            break;

        case FlowControlCTS:
            pin_function(pinmap->tx_flow_pin, pinmap->tx_flow_function);
            pin_mode(pinmap->tx_flow_pin, PullNone);
            uart_addrs[obj->serial.index]->MODEM &= ~UART_MODEM_RXRTSE_MASK;
            uart_addrs[obj->serial.index]->MODEM |= UART_MODEM_TXCTSE_MASK;
            break;

        case FlowControlRTSCTS:
            pin_function(pinmap->rx_flow_pin, pinmap->rx_flow_function);
            pin_mode(pinmap->rx_flow_pin, PullNone);
            pin_function(pinmap->tx_flow_pin, pinmap->tx_flow_function);
            pin_mode(pinmap->tx_flow_pin, PullNone);
            uart_addrs[obj->serial.index]->MODEM |= UART_MODEM_TXCTSE_MASK | UART_MODEM_RXRTSE_MASK;
            break;

        case FlowControlNone:
            uart_addrs[obj->serial.index]->MODEM &= ~(UART_MODEM_TXCTSE_MASK | UART_MODEM_RXRTSE_MASK);
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

    serial_set_flow_control_direct(obj, type, &explicit_uart_fc_pinmap);
}

#endif

static void serial_send_asynch(serial_t *obj)
{
    uart_transfer_t sendXfer;

    /*Setup send transfer*/
    sendXfer.data = obj->tx_buff.buffer;
    sendXfer.dataSize = obj->tx_buff.length;

    if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED ||
            obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
        UART_SendEDMA(uart_addrs[obj->serial.index], &obj->serial.uart_dma_handle, &sendXfer);
    } else {
        UART_TransferSendNonBlocking(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle, &sendXfer);
    }
}

static void serial_receive_asynch(serial_t *obj)
{
    uart_transfer_t receiveXfer;

    /*Setup send transfer*/
    receiveXfer.data = obj->rx_buff.buffer;
    receiveXfer.dataSize = obj->rx_buff.length;

    if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED ||
            obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
        UART_ReceiveEDMA(uart_addrs[obj->serial.index], &obj->serial.uart_dma_handle, &receiveXfer);
    } else {
        UART_TransferReceiveNonBlocking(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle, &receiveXfer, NULL);
    }
}

static bool serial_allocate_dma(serial_t *obj, uint32_t handler)
{
    dma_request_source_t dma_rx_requests[] = UART_DMA_RX_REQUEST_NUMBERS;
    dma_request_source_t dma_tx_requests[] = UART_DMA_TX_REQUEST_NUMBERS;
    edma_config_t userConfig;

    /* Allocate the UART RX DMA channel */
    obj->serial.uartDmaRx.dmaChannel = dma_channel_allocate(dma_rx_requests[obj->serial.index]);
    if (obj->serial.uartDmaRx.dmaChannel == DMA_ERROR_OUT_OF_CHANNELS) {
        return false;
    }

    /* Allocate the UART TX DMA channel */
    obj->serial.uartDmaTx.dmaChannel = dma_channel_allocate(dma_tx_requests[obj->serial.index]);
    if (obj->serial.uartDmaTx.dmaChannel == DMA_ERROR_OUT_OF_CHANNELS) {
        dma_channel_free(obj->serial.uartDmaRx.dmaChannel);
        return false;
    }

    /* EDMA init*/
    /*
     * userConfig.enableRoundRobinArbitration = false;
     * userConfig.enableHaltOnError = true;
     * userConfig.enableContinuousLinkMode = false;
     * userConfig.enableDebugMode = false;
     */
    EDMA_GetDefaultConfig(&userConfig);

    EDMA_Init(DMA0, &userConfig);

    memset(&(obj->serial.uartDmaTx.handle), 0, sizeof(obj->serial.uartDmaTx.handle));
    memset(&(obj->serial.uartDmaRx.handle), 0, sizeof(obj->serial.uartDmaRx.handle));

    EDMA_CreateHandle(&(obj->serial.uartDmaRx.handle), DMA0, obj->serial.uartDmaRx.dmaChannel);
    EDMA_CreateHandle(&(obj->serial.uartDmaTx.handle), DMA0, obj->serial.uartDmaTx.dmaChannel);

    UART_TransferCreateHandleEDMA(uart_addrs[obj->serial.index], &obj->serial.uart_dma_handle, (uart_edma_transfer_callback_t)handler,
                                  NULL, &obj->serial.uartDmaTx.handle, &obj->serial.uartDmaRx.handle);

    return true;
}

void serial_enable_dma(serial_t *obj, uint32_t handler, DMAUsage state)
{
    dma_init();

    if (state == DMA_USAGE_ALWAYS && obj->serial.uartDmaRx.dmaUsageState != DMA_USAGE_ALLOCATED) {
        /* Try to allocate channels */
        if (serial_allocate_dma(obj, handler)) {
            obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_ALLOCATED;
        } else {
            obj->serial.uartDmaRx.dmaUsageState = state;
        }
    } else if (state == DMA_USAGE_OPPORTUNISTIC) {
        if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED) {
            /* Channels have already been allocated previously by an ALWAYS state, so after this transfer, we will release them */
            obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_TEMPORARY_ALLOCATED;
        } else {
            /* Try to allocate channels */
            if (serial_allocate_dma(obj, handler)) {
                obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_TEMPORARY_ALLOCATED;
            } else {
                obj->serial.uartDmaRx.dmaUsageState = state;
            }
        }
    } else if (state == DMA_USAGE_NEVER) {
        /* If channels are allocated, get rid of them */
        if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED) {
            dma_channel_free(obj->serial.uartDmaRx.dmaChannel);
            dma_channel_free(obj->serial.uartDmaRx.dmaChannel);
        }
        obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_NEVER;
    }
}

void serial_enable_event(serial_t *obj, int event, uint8_t enable)
{
    // Keep track of the requested events.
    if (enable) {
        obj->serial.events |= event;
    } else {
        obj->serial.events &= ~event;
    }
}

static void serial_tx_buffer_set(serial_t *obj, void *tx, int tx_length, uint8_t width)
{
    (void)width;

    // Exit if a transmit is already on-going
    if (serial_tx_active(obj)) {
        return;
    }

    obj->tx_buff.buffer = tx;
    obj->tx_buff.length = tx_length;
    obj->tx_buff.pos = 0;
}

int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint)
{
    // Check that a buffer has indeed been set up
    MBED_ASSERT(tx != (void *)0);

    if (tx_length == 0) {
        return 0;
    }

    if (serial_tx_active(obj)) {
        return 0;
    }

    // Set up buffer
    serial_tx_buffer_set(obj, (void *)tx, tx_length, tx_width);

    // Set up events
    serial_enable_event(obj, SERIAL_EVENT_TX_ALL, false);
    serial_enable_event(obj, event, true);

    /* If using DMA, allocate  channels only if they have not already been allocated */
    if (hint != DMA_USAGE_NEVER) {
        /* User requested to transfer using DMA */
        serial_enable_dma(obj, handler, hint);

        /* Check if DMA setup was successful */
        if (obj->serial.uartDmaRx.dmaUsageState != DMA_USAGE_ALLOCATED && obj->serial.uartDmaRx.dmaUsageState != DMA_USAGE_TEMPORARY_ALLOCATED) {
            /* Set up an interrupt transfer as DMA is unavailable */
            if (obj->serial.uart_transfer_handle.callback == 0) {
                UART_TransferCreateHandle(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle, (uart_transfer_callback_t)handler, NULL);
            }
        }
    } else {
        /* User requested to transfer using interrupts */
        /* Disable the DMA */
        serial_enable_dma(obj, handler, hint);

        /* Set up the interrupt transfer */
        if (obj->serial.uart_transfer_handle.callback == 0) {
            UART_TransferCreateHandle(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle, (uart_transfer_callback_t)handler, NULL);
        }
    }

    obj->serial.txstate = kUART_TxBusy;

    /* Start the transfer */
    serial_send_asynch(obj);

    /* Can't enter deep sleep as long as UART transmit is active */
    sleep_manager_lock_deep_sleep();

    return 0;
}

void serial_rx_buffer_set(serial_t *obj, void *rx, int rx_length, uint8_t width)
{
    // We only support byte buffers for now
    MBED_ASSERT(width == 8);

    if (serial_rx_active(obj)) {
        return;
    }

    obj->rx_buff.buffer = rx;
    obj->rx_buff.length = rx_length;
    obj->rx_buff.pos = 0;

    return;
}

/* Character match is currently not supported */
void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint)
{
    // Check that a buffer has indeed been set up
    MBED_ASSERT(rx != (void *)0);
    if (rx_length == 0) {
        return;
    }

    if (serial_rx_active(obj)) {
        return;
    }

    // Set up buffer
    serial_rx_buffer_set(obj, (void *) rx, rx_length, rx_width);

    // Set up events
    serial_enable_event(obj, SERIAL_EVENT_RX_ALL, false);
    serial_enable_event(obj, event, true);

    //obj->char_match = char_match;

    /* If using DMA, allocate  channels only if they have not already been allocated */
    if (hint != DMA_USAGE_NEVER) {
        /* User requested to transfer using DMA */
        serial_enable_dma(obj, handler, hint);

        /* Check if DMA setup was successful */
        if (obj->serial.uartDmaRx.dmaUsageState != DMA_USAGE_ALLOCATED && obj->serial.uartDmaRx.dmaUsageState != DMA_USAGE_TEMPORARY_ALLOCATED) {
            /* Set up an interrupt transfer as DMA is unavailable */
            if (obj->serial.uart_transfer_handle.callback == 0) {
                UART_TransferCreateHandle(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle, (uart_transfer_callback_t)handler, NULL);
            }
        }

    } else {
        /* User requested to transfer using interrupts */
        /* Disable the DMA */
        serial_enable_dma(obj, handler, hint);

        /* Set up the interrupt transfer */
        if (obj->serial.uart_transfer_handle.callback == 0) {
            UART_TransferCreateHandle(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle, (uart_transfer_callback_t)handler, NULL);
        }
    }

    obj->serial.rxstate = kUART_RxBusy;

    /* Start the transfer */
    serial_receive_asynch(obj);

    /* Can't enter deep sleep as long as UART transfer is active */
    sleep_manager_lock_deep_sleep();
}

uint8_t serial_tx_active(serial_t *obj)
{
    if (obj->serial.txstate == kUART_TxIdle) {
        return 0;
    }

    return 1;
}

uint8_t serial_rx_active(serial_t *obj)
{
    if (obj->serial.rxstate == kUART_RxIdle) {
        return 0;
    }

    return 1;
}

int serial_irq_handler_asynch(serial_t *obj)
{
    int status = 0;
    //uint8_t *buf = (uint8_t*)obj->rx_buff.buffer;
    uint32_t status_flags = UART_GetStatusFlags(uart_addrs[obj->serial.index]);

    /* Determine whether the current scenario is DMA or IRQ, and act accordingly */
    if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED || obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
        /* DMA implementation */
        if ((obj->serial.txstate != kUART_TxIdle)  && (obj->serial.uart_dma_handle.txState == kUART_TxIdle)) {
            obj->serial.txstate = kUART_TxIdle;
            status |= SERIAL_EVENT_TX_COMPLETE;
            /* Transmit is complete, re-enable entry to deep sleep mode */
            sleep_manager_unlock_deep_sleep();
        }

        if ((obj->serial.rxstate != kUART_RxIdle) && (obj->serial.uart_dma_handle.rxState == kUART_RxIdle)) {
            obj->serial.rxstate = kUART_RxIdle;
            status |= SERIAL_EVENT_RX_COMPLETE;
            /* Receive is complete, re-enable entry to deep sleep mode */
            sleep_manager_unlock_deep_sleep();
        }

        /* Release the dma channels if they were opportunistically allocated */
        if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
            /* Ensure both TX and RX channels are idle before freeing them */
            if ((obj->serial.uart_dma_handle.txState == kUART_TxIdle) && (obj->serial.uart_dma_handle.rxState == kUART_RxIdle)) {
                dma_channel_free(obj->serial.uartDmaRx.dmaChannel);
                dma_channel_free(obj->serial.uartDmaTx.dmaChannel);
                obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;
            }
        }
    } else {
        /* Interrupt implementation */
        if ((obj->serial.txstate != kUART_TxIdle) && (obj->serial.uart_transfer_handle.txState == kUART_TxIdle)) {
            obj->serial.txstate = kUART_TxIdle;
            status |= SERIAL_EVENT_TX_COMPLETE;
            /* Transmit is complete, re-enable entry to deep sleep mode */
            sleep_manager_unlock_deep_sleep();
        }

        if ((obj->serial.rxstate != kUART_RxIdle) && (obj->serial.uart_transfer_handle.rxState == kUART_RxIdle)) {
            obj->serial.rxstate = kUART_RxIdle;
            status |= SERIAL_EVENT_RX_COMPLETE;
            /* Receive is complete, re-enable entry to deep sleep mode */
            sleep_manager_unlock_deep_sleep();
        }
    }
#if 0
    if (obj->char_match != SERIAL_RESERVED_CHAR_MATCH) {
        /* Check for character match event */
        if (buf[obj->rx_buff.length - 1] == obj->char_match) {
            status |= SERIAL_EVENT_RX_CHARACTER_MATCH;
        }
    }
#endif

    if (status_flags & kUART_RxOverrunFlag) {
        UART_ClearStatusFlags(uart_addrs[obj->serial.index], kUART_RxOverrunFlag);
        status |= SERIAL_EVENT_RX_OVERRUN_ERROR;
    }

    if (status_flags & kUART_FramingErrorFlag) {
        UART_ClearStatusFlags(uart_addrs[obj->serial.index], kUART_FramingErrorFlag);
        status |= SERIAL_EVENT_RX_FRAMING_ERROR;
    }

    if (status_flags & kUART_ParityErrorFlag) {
        UART_ClearStatusFlags(uart_addrs[obj->serial.index], kUART_ParityErrorFlag);
        status |= SERIAL_EVENT_RX_PARITY_ERROR;
    }

    return status & obj->serial.events;
}

void serial_tx_abort_asynch(serial_t *obj)
{
    // If we're not currently transferring, then there's nothing to do here
    if (serial_tx_active(obj) == 0) {
        return;
    }

    if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED || obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
        UART_TransferAbortSendEDMA(uart_addrs[obj->serial.index], &obj->serial.uart_dma_handle);
        /* Release the dma channels if they were opportunistically allocated */
        if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
            /* Ensure both TX and RX channels are idle before freeing them */
            if ((obj->serial.uart_dma_handle.txState == kUART_TxIdle) && (obj->serial.uart_dma_handle.rxState == kUART_RxIdle)) {
                dma_channel_free(obj->serial.uartDmaRx.dmaChannel);
                dma_channel_free(obj->serial.uartDmaTx.dmaChannel);
                obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;
            }
        }
    } else {
        UART_TransferAbortSend(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle);
    }

    obj->serial.txstate = kUART_TxIdle;

    /* Re-enable entry to deep sleep mode */
    sleep_manager_unlock_deep_sleep();
}

void serial_rx_abort_asynch(serial_t *obj)
{
    // If we're not currently transferring, then there's nothing to do here
    if (serial_rx_active(obj) == 0) {
        return;
    }

    if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_ALLOCATED || obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
        UART_TransferAbortReceiveEDMA(uart_addrs[obj->serial.index], &obj->serial.uart_dma_handle);
        /* Release the dma channels if they were opportunistically allocated */
        if (obj->serial.uartDmaRx.dmaUsageState == DMA_USAGE_TEMPORARY_ALLOCATED) {
            /* Ensure both TX and RX channels are idle before freeing them */
            if ((obj->serial.uart_dma_handle.txState == kUART_TxIdle) && (obj->serial.uart_dma_handle.rxState == kUART_RxIdle)) {
                dma_channel_free(obj->serial.uartDmaRx.dmaChannel);
                dma_channel_free(obj->serial.uartDmaTx.dmaChannel);
                obj->serial.uartDmaRx.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;
            }
        }
    } else {
        UART_TransferAbortReceive(uart_addrs[obj->serial.index], &obj->serial.uart_transfer_handle);
    }

    obj->serial.rxstate = kUART_RxIdle;

    /* Re-enable entry to deep sleep mode */
    sleep_manager_unlock_deep_sleep();
}

static int serial_is_enabled(uint32_t uart_index)
{
    int clock_enabled = 0;
    switch (uart_index) {
        case 0:
            clock_enabled = (SIM->SCGC4 & SIM_SCGC4_UART0_MASK) >> SIM_SCGC4_UART0_SHIFT;
            break;
        case 1:
            clock_enabled = (SIM->SCGC4 & SIM_SCGC4_UART1_MASK) >> SIM_SCGC4_UART1_SHIFT;
            break;
        case 2:
            clock_enabled = (SIM->SCGC4 & SIM_SCGC4_UART2_MASK) >> SIM_SCGC4_UART2_SHIFT;
            break;
        case 3:
            clock_enabled = (SIM->SCGC4 & SIM_SCGC4_UART3_MASK) >> SIM_SCGC4_UART3_SHIFT;
            break;
        case 4:
            clock_enabled = (SIM->SCGC1 & SIM_SCGC1_UART4_MASK) >> SIM_SCGC1_UART4_SHIFT;
            break;
        case 5:
            clock_enabled = (SIM->SCGC1 & SIM_SCGC1_UART5_MASK) >> SIM_SCGC1_UART5_SHIFT;
            break;
        default:
            break;
    }

    return clock_enabled;
}

bool serial_check_tx_ongoing()
{
    UART_Type *base;
    int i;
    bool uart_tx_ongoing = false;

    for (i = 0; i < FSL_FEATURE_SOC_UART_COUNT; i++) {
        /* First check if UART is enabled */
        if (!serial_is_enabled(i)) {
            /* UART is not enabled, check the next instance */
            continue;
        }

        base = uart_addrs[i];

        /* Check if data is waiting to be written out of transmit buffer */
        if (!(kUART_TransmissionCompleteFlag & UART_GetStatusFlags((UART_Type *)base))) {
            uart_tx_ongoing = true;
            break;
        }
    }

    return uart_tx_ongoing;
}

#endif
