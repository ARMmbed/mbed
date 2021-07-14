/***************************************************************************//**
 * @file serial_api.c
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#include "device.h"
#include "clocking.h"
#if DEVICE_SERIAL

#include "mbed_assert.h"
#include "mbed_power_mgmt.h"
#include "serial_api.h"
#include "serial_api_HAL.h"
#include <string.h>
#include <stdbool.h>

#include "pinmap.h"
#include "pinmap_function.h"
#include "PeripheralPins.h"
#include "PeripheralNames.h"

#include "em_usart.h"
#include "em_leuart.h"
#include "em_cmu.h"
#include "em_dma.h"
#include "dma_api_HAL.h"
#include "dma_api.h"
#include "sleep_api.h"
#include "buffer.h"

/** Validation of LEUART register block pointer reference
 *  for assert statements. */
#if !defined(LEUART_COUNT)
#define LEUART_REF_VALID(ref)    (0)
#elif (LEUART_COUNT == 1)
#define LEUART_REF_VALID(ref)    ((ref) == LEUART0)
#elif (LEUART_COUNT == 2)
#define LEUART_REF_VALID(ref)    (((ref) == LEUART0) || ((ref) == LEUART1))
#else
#error Undefined number of low energy UARTs (LEUART).
#endif

#ifndef UART_PRESENT
#define UART_COUNT (0)
#endif
#ifndef USART_PRESENT
#define USART_COUNT (0)
#endif
#ifndef LEUART_PRESENT
#define LEUART_COUNT (0)
#endif

#define MODULES_SIZE_SERIAL (UART_COUNT + USART_COUNT + LEUART_COUNT)

/* Store IRQ id for each UART */
static uint32_t serial_irq_ids[MODULES_SIZE_SERIAL] = { 0 };
/* Interrupt handler from mbed common */
static uart_irq_handler irq_handler;
/* Keep track of incoming DMA IRQ's */
static bool serial_dma_irq_fired[DMA_CHAN_COUNT] = { false };

/* Serial interface on CONSOLE_TX/CONSOLE_RX retargets stdio */
int stdio_uart_inited = 0;
serial_t stdio_uart;

static void uart_irq(UARTName, SerialIrq);
static uint8_t serial_get_index(serial_t *obj);
static void serial_enable(serial_t *obj, uint8_t enable);
static void serial_enable_pins(serial_t *obj, uint8_t enable);
static void serial_set_route(serial_t *obj);
static IRQn_Type serial_get_rx_irq_index(serial_t *obj);
static IRQn_Type serial_get_tx_irq_index(serial_t *obj);
static CMU_Clock_TypeDef serial_get_clock(serial_t *obj);
static void serial_dmaSetupChannel(serial_t *obj, bool tx_nrx);
static void serial_rx_abort_asynch_intern(serial_t *obj, int unblock_sleep);
static void serial_tx_abort_asynch_intern(serial_t *obj, int unblock_sleep);
static void serial_block_sleep(serial_t *obj);
static void serial_unblock_sleep(serial_t *obj);
static void serial_leuart_baud(serial_t *obj, int baudrate);

/* ISRs for RX and TX events */
#ifdef UART0
static void uart0_rx_irq() { uart_irq(UART_0, RxIrq); }
static void uart0_tx_irq() { uart_irq(UART_0, TxIrq); USART_IntClear((USART_TypeDef*)UART_0, USART_IFC_TXC);}
#endif
#ifdef UART1
static void uart1_rx_irq() { uart_irq(UART_1, RxIrq); }
static void uart1_tx_irq() { uart_irq(UART_1, TxIrq); USART_IntClear((USART_TypeDef*)UART_1, USART_IFC_TXC);}
#endif
#ifdef USART0
static void usart0_rx_irq() { uart_irq(USART_0, RxIrq); }
static void usart0_tx_irq() { uart_irq(USART_0, TxIrq); USART_IntClear((USART_TypeDef*)USART_0, USART_IFC_TXC);}
#endif
#ifdef USART1
static void usart1_rx_irq() { uart_irq(USART_1, RxIrq); }
static void usart1_tx_irq() { uart_irq(USART_1, TxIrq); USART_IntClear((USART_TypeDef*)USART_1, USART_IFC_TXC);}
#endif
#ifdef USART2
static void usart2_rx_irq() { uart_irq(USART_2, RxIrq); }
static void usart2_tx_irq() { uart_irq(USART_2, TxIrq); USART_IntClear((USART_TypeDef*)USART_2, USART_IFC_TXC);}
#endif
#ifdef USART3
static void usart3_rx_irq() { uart_irq(USART_3, RxIrq); }
static void usart3_tx_irq() { uart_irq(USART_3, TxIrq); USART_IntClear((USART_TypeDef*)USART_3, USART_IFC_TXC);}
#endif
#ifdef USART4
static void usart4_rx_irq() { uart_irq(USART_4, RxIrq); }
static void usart4_tx_irq() { uart_irq(USART_4, TxIrq); USART_IntClear((USART_TypeDef*)USART_4, USART_IFC_TXC);}
#endif
#ifdef USART5
static void usart5_rx_irq() { uart_irq(USART_5, RxIrq); }
static void usart5_tx_irq() { uart_irq(USART_5, TxIrq); USART_IntClear((USART_TypeDef*)USART_5, USART_IFC_TXC);}
#endif
#ifdef LEUART0
static void leuart0_irq()
{
    if(LEUART_IntGetEnabled(LEUART0) & (LEUART_IF_RXDATAV | LEUART_IF_FERR | LEUART_IF_PERR | LEUART_IF_RXOF)) {
        uart_irq(LEUART_0, RxIrq);
    }

    if(LEUART_IntGetEnabled(LEUART0) & (LEUART_IF_TXC | LEUART_IF_TXBL | LEUART_IF_TXOF)) {
        uart_irq(LEUART_0, TxIrq);
        LEUART_IntClear(LEUART0, LEUART_IFC_TXC);
    }
}
#endif
#ifdef LEUART1
static void leuart1_irq()
{
    if(LEUART_IntGetEnabled(LEUART1) & (LEUART_IF_RXDATAV | LEUART_IF_FERR | LEUART_IF_PERR | LEUART_IF_RXOF)) {
        uart_irq(LEUART_1, RxIrq);
    }

    if(LEUART_IntGetEnabled(LEUART1) & (LEUART_IF_TXC | LEUART_IF_TXBL | LEUART_IF_TXOF)) {
        uart_irq(LEUART_1, TxIrq);
        LEUART_IntClear(LEUART1, LEUART_IFC_TXC);
    }
}
#endif

/**
 * Initialize the UART using default settings, overridden by settings from serial object
 *
 * @param obj pointer to serial object
 */
static void uart_init(serial_t *obj, uint32_t baudrate, SerialParity parity, int stop_bits)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        LEUART_Init_TypeDef init = LEUART_INIT_DEFAULT;

        if (stop_bits == 2) {
            init.stopbits = leuartStopbits2;
        } else {
            init.stopbits = leuartStopbits1;
        }

        switch (parity) {
            case ParityOdd:
            case ParityForced0:
                init.parity = leuartOddParity;
                break;
            case ParityEven:
            case ParityForced1:
                init.parity = leuartEvenParity;
                break;
            default: /* ParityNone */
                init.parity = leuartNoParity;
                break;
        }

        init.enable = leuartDisable;
        init.baudrate = 9600;
        init.databits = leuartDatabits8;
#ifdef LEUART_USING_LFXO
        init.refFreq = LEUART_LF_REF_FREQ;
#else
        init.refFreq = LEUART_REF_FREQ;
#endif
        LEUART_Init(obj->serial.periph.leuart, &init);

        if (baudrate != 9600) {
            serial_baud(obj, baudrate);
        }
    } else {
        USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;

        if (stop_bits == 2) {
            init.stopbits = usartStopbits2;
        } else {
            init.stopbits = usartStopbits1;
        }
        switch (parity) {
            case ParityOdd:
            case ParityForced0:
                init.parity = usartOddParity;
                break;
            case ParityEven:
            case ParityForced1:
                init.parity = usartEvenParity;
                break;
            default: /* ParityNone */
                init.parity = usartNoParity;
                break;
        }

        init.enable = usartDisable;
        init.baudrate = baudrate;
        init.oversampling = usartOVS16;
        init.databits = usartDatabits8;
        init.refFreq = REFERENCE_FREQUENCY;

        USART_InitAsync(obj->serial.periph.uart, &init);
    }
}
/**
* Get index of serial object, relating it to the physical peripheral.
*
* @param obj pointer to serial peripheral (= base address of periph)
* @return internal index of U(S)ART peripheral
*/
static inline uint8_t serial_pointer_get_index(uint32_t serial_ptr)
{
    uint8_t index = 0;
#ifdef UART0
    if (serial_ptr == UART_0) return index;
    index++;
#endif
#ifdef UART1
    if (serial_ptr == UART_1) return index;
    index++;
#endif
#ifdef USART0
    if (serial_ptr == USART_0) return index;
    index++;
#endif
#ifdef USART1
    if (serial_ptr == USART_1) return index;
    index++;
#endif
#ifdef USART2
    if (serial_ptr == USART_2) return index;
    index++;
#endif
#ifdef USART3
    if (serial_ptr == USART_3) return index;
    index++;
#endif
#ifdef USART4
    if (serial_ptr == USART_4) return index;
    index++;
#endif
#ifdef USART5
    if (serial_ptr == USART_5) return index;
    index++;
#endif
#ifdef LEUART0
    if (serial_ptr == LEUART_0) return index;
    index++;
#endif
#ifdef LEUART1
    if (serial_ptr == LEUART_1) return index;
    index++;
#endif
    return 0;
}

/**
* Get index of serial object, relating it to the physical peripheral.
*
* @param obj pointer to serial object (mbed object)
* @return internal index of U(S)ART peripheral
*/
static inline uint8_t serial_get_index(serial_t *obj)
{
    return serial_pointer_get_index((uint32_t)obj->serial.periph.uart);
}

/**
* Get index of serial object RX IRQ, relating it to the physical peripheral.
*
* @param obj pointer to serial object
* @return internal NVIC RX IRQ index of U(S)ART peripheral
*/
static inline IRQn_Type serial_get_rx_irq_index(serial_t *obj)
{
    switch ((uint32_t)obj->serial.periph.uart) {
#ifdef UART0
        case UART_0:
            return UART0_RX_IRQn;
#endif
#ifdef UART1
        case UART_1:
            return UART1_RX_IRQn;
#endif
#ifdef USART0
        case USART_0:
            return USART0_RX_IRQn;
#endif
#ifdef USART1
        case USART_1:
            return USART1_RX_IRQn;
#endif
#ifdef USART2
        case USART_2:
            return USART2_RX_IRQn;
#endif
#ifdef USART3
        case USART_3:
            return USART3_RX_IRQn;
#endif
#ifdef USART4
        case USART_4:
            return USART4_RX_IRQn;
#endif
#ifdef USART5
        case USART_5:
            return USART5_RX_IRQn;
#endif
#ifdef LEUART0
        case LEUART_0:
            return LEUART0_IRQn;
#endif
#ifdef LEUART1
        case LEUART_1:
            return LEUART1_IRQn;
#endif
        default:
            MBED_ASSERT(0);
    }
    return (IRQn_Type)0;
}

/**
* Get index of serial object TX IRQ, relating it to the physical peripheral.
*
* @param obj pointer to serial object
* @return internal NVIC TX IRQ index of U(S)ART peripheral
*/
static inline IRQn_Type serial_get_tx_irq_index(serial_t *obj)
{
    switch ((uint32_t)obj->serial.periph.uart) {
#ifdef UART0
        case UART_0:
            return UART0_TX_IRQn;
#endif
#ifdef UART1
        case UART_1:
            return UART1_TX_IRQn;
#endif
#ifdef USART0
        case USART_0:
            return USART0_TX_IRQn;
#endif
#ifdef USART1
        case USART_1:
            return USART1_TX_IRQn;
#endif
#ifdef USART2
        case USART_2:
            return USART2_TX_IRQn;
#endif
#ifdef USART3
        case USART_3:
            return USART3_TX_IRQn;
#endif
#ifdef USART4
        case USART_4:
            return USART4_TX_IRQn;
#endif
#ifdef USART5
        case USART_5:
            return USART5_TX_IRQn;
#endif
#ifdef LEUART0
        case LEUART_0:
            return LEUART0_IRQn;
#endif
#ifdef LEUART1
        case LEUART_1:
            return LEUART1_IRQn;
#endif
        default:
            MBED_ASSERT(0);
    }
    return (IRQn_Type)0;
}

/**
* Get clock tree for serial peripheral pointed to by obj.
*
* @param obj pointer to serial object
* @return CMU_Clock_TypeDef for U(S)ART
*/
inline CMU_Clock_TypeDef serial_get_clock(serial_t *obj)
{
    switch ((uint32_t)obj->serial.periph.uart) {
#ifdef UART0
        case UART_0:
            return cmuClock_UART0;
#endif
#ifdef UART1
        case UART_1:
            return cmuClock_UART1;
#endif
#ifdef USART0
        case USART_0:
            return cmuClock_USART0;
#endif
#ifdef USART1
        case USART_1:
            return cmuClock_USART1;
#endif
#ifdef USART2
        case USART_2:
            return cmuClock_USART2;
#endif
#ifdef USART3
        case USART_3:
            return cmuClock_USART3;
#endif
#ifdef USART4
        case USART_4:
            return cmuClock_USART4;
#endif
#ifdef USART5
        case USART_5:
            return cmuClock_USART5;
#endif
#ifdef LEUART0
        case LEUART_0:
            return cmuClock_LEUART0;
#endif
#ifdef LEUART1
        case LEUART_1:
            return cmuClock_LEUART1;
#endif
        default:
            return cmuClock_HFPER;
    }
}

void serial_preinit(serial_t *obj, PinName tx, PinName rx)
{
    /* Get UART object connected to the given pins */
    UARTName uart_tx = (UARTName) pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName) pinmap_peripheral(rx, PinMap_UART_RX);
    /* Check that pins are connected to same UART */
    UARTName uart = (UARTName) pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((unsigned int) uart != NC);

    obj->serial.periph.uart = (USART_TypeDef *) uart;

    /* Get location */
    uint32_t uart_tx_loc = pin_location(tx, PinMap_UART_TX);
    uint32_t uart_rx_loc = pin_location(rx, PinMap_UART_RX);

#if defined(_SILICON_LABS_32B_PLATFORM_1)
    /* Check that pins are used by same location for the given UART */
    obj->serial.location = pinmap_merge(uart_tx_loc, uart_rx_loc);
    MBED_ASSERT(obj->serial.location != NC);
#else
    obj->serial.location_tx = uart_tx_loc;
    obj->serial.location_rx = uart_rx_loc;
#endif

    /* Store pins in object for easy disabling in serial_free() */
    //TODO: replace all usages with AF_USARTx_TX_PORT(location) macro to save 8 bytes from struct
    obj->serial.rx_pin = rx;
    obj->serial.tx_pin = tx;

    /* Select interrupt */
    switch ((uint32_t)obj->serial.periph.uart) {
#ifdef UART0
        case UART_0:
            NVIC_SetVector(UART0_RX_IRQn, (uint32_t) &uart0_rx_irq);
            NVIC_SetVector(UART0_TX_IRQn, (uint32_t) &uart0_tx_irq);
            NVIC_SetPriority(UART0_TX_IRQn, 1);
            break;
#endif
#ifdef UART1
        case UART_1:
            NVIC_SetVector(UART1_RX_IRQn, (uint32_t) &uart1_rx_irq);
            NVIC_SetVector(UART1_TX_IRQn, (uint32_t) &uart1_tx_irq);
            NVIC_SetPriority(UART1_TX_IRQn, 1);
            break;
#endif
#ifdef USART0
        case USART_0:
            NVIC_SetVector(USART0_RX_IRQn, (uint32_t) &usart0_rx_irq);
            NVIC_SetVector(USART0_TX_IRQn, (uint32_t) &usart0_tx_irq);
            NVIC_SetPriority(USART0_TX_IRQn, 1);
            break;
#endif
#ifdef USART1
        case USART_1:
            NVIC_SetVector(USART1_RX_IRQn, (uint32_t) &usart1_rx_irq);
            NVIC_SetVector(USART1_TX_IRQn, (uint32_t) &usart1_tx_irq);
            NVIC_SetPriority(USART1_TX_IRQn, 1);
            break;
#endif
#ifdef USART2
        case USART_2:
            NVIC_SetVector(USART2_RX_IRQn, (uint32_t) &usart2_rx_irq);
            NVIC_SetVector(USART2_TX_IRQn, (uint32_t) &usart2_tx_irq);
            NVIC_SetPriority(USART2_TX_IRQn, 1);
            break;
#endif
#ifdef USART3
        case USART_3:
            NVIC_SetVector(USART3_RX_IRQn, (uint32_t) &usart3_rx_irq);
            NVIC_SetVector(USART3_TX_IRQn, (uint32_t) &usart3_tx_irq);
            NVIC_SetPriority(USART3_TX_IRQn, 1);
            break;
#endif
#ifdef USART4
        case USART_4:
            NVIC_SetVector(USART4_RX_IRQn, (uint32_t) &usart4_rx_irq);
            NVIC_SetVector(USART4_TX_IRQn, (uint32_t) &usart4_tx_irq);
            NVIC_SetPriority(USART4_TX_IRQn, 1);
            break;
#endif
#ifdef USART5
        case USART_5:
            NVIC_SetVector(USART5_RX_IRQn, (uint32_t) &usart5_rx_irq);
            NVIC_SetVector(USART5_TX_IRQn, (uint32_t) &usart5_tx_irq);
            NVIC_SetPriority(USART5_TX_IRQn, 1);
            break;
#endif
#ifdef LEUART0
        case LEUART_0:
            NVIC_SetVector(LEUART0_IRQn, (uint32_t) &leuart0_irq);
            break;
#endif
#ifdef LEUART1
        case LEUART_1:
            NVIC_SetVector(LEUART1_IRQn, (uint32_t) &leuart1_irq);
            break;
#endif
    }
}

static void serial_enable_pins(serial_t *obj, uint8_t enable)
{
    if (enable) {
        /* Configure GPIO pins*/
        if(obj->serial.rx_pin != NC) {
            pin_mode(obj->serial.rx_pin, Input);
        }
        /* Set DOUT first to prevent glitches */
        if(obj->serial.tx_pin != NC) {
            GPIO_PinOutSet((GPIO_Port_TypeDef)(obj->serial.tx_pin >> 4 & 0xF), obj->serial.tx_pin & 0xF);
            pin_mode(obj->serial.tx_pin, PushPull);
        }
    } else {
        if(obj->serial.rx_pin != NC) {
            pin_mode(obj->serial.rx_pin, Disabled);
        }
        if(obj->serial.tx_pin != NC) {
            pin_mode(obj->serial.tx_pin, Disabled);
        }
    }
}

static void serial_set_route(serial_t *obj)
{
    /* Enable pins for UART at correct location */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
#ifdef _LEUART_ROUTE_LOCATION_SHIFT
        obj->serial.periph.leuart->ROUTE = (obj->serial.location << _LEUART_ROUTE_LOCATION_SHIFT);
        if(obj->serial.tx_pin != NC) {
            obj->serial.periph.leuart->ROUTE |= LEUART_ROUTE_TXPEN;
        } else {
            obj->serial.periph.leuart->ROUTE &= ~LEUART_ROUTE_TXPEN;
        }
        if(obj->serial.rx_pin != NC) {
            obj->serial.periph.leuart->ROUTE |= LEUART_ROUTE_RXPEN;
        } else {
            obj->serial.periph.leuart->CMD    = LEUART_CMD_RXBLOCKEN;
            obj->serial.periph.leuart->ROUTE &= ~LEUART_ROUTE_RXPEN;
        }
#else
        if(obj->serial.location_tx != NC) {
            obj->serial.periph.leuart->ROUTELOC0 = (obj->serial.periph.leuart->ROUTELOC0 & (~_LEUART_ROUTELOC0_TXLOC_MASK)) | (obj->serial.location_tx << _LEUART_ROUTELOC0_TXLOC_SHIFT);
            obj->serial.periph.leuart->ROUTEPEN  = (obj->serial.periph.leuart->ROUTEPEN & (~_LEUART_ROUTEPEN_TXPEN_MASK)) | LEUART_ROUTEPEN_TXPEN;
        } else {
            obj->serial.periph.leuart->ROUTEPEN  = (obj->serial.periph.leuart->ROUTEPEN & (~_LEUART_ROUTEPEN_TXPEN_MASK));
        }
        if(obj->serial.location_rx != NC) {
            obj->serial.periph.leuart->ROUTELOC0 = (obj->serial.periph.leuart->ROUTELOC0 & (~_LEUART_ROUTELOC0_RXLOC_MASK)) | (obj->serial.location_rx << _LEUART_ROUTELOC0_RXLOC_SHIFT);
            obj->serial.periph.leuart->ROUTEPEN  = (obj->serial.periph.leuart->ROUTEPEN & (~_LEUART_ROUTEPEN_RXPEN_MASK)) | LEUART_ROUTEPEN_RXPEN;
        } else {
            obj->serial.periph.leuart->CMD       = LEUART_CMD_RXBLOCKEN;
            obj->serial.periph.leuart->ROUTEPEN  = (obj->serial.periph.leuart->ROUTEPEN & (~_LEUART_ROUTEPEN_RXPEN_MASK));
        }
#endif
    } else {
#ifdef _USART_ROUTE_LOCATION_SHIFT
        obj->serial.periph.uart->ROUTE = (obj->serial.location << _LEUART_ROUTE_LOCATION_SHIFT);
        if(obj->serial.tx_pin != NC) {
            obj->serial.periph.uart->ROUTE |= USART_ROUTE_TXPEN;
        } else {
            obj->serial.periph.uart->ROUTE &= ~USART_ROUTE_TXPEN;
        }
        if(obj->serial.rx_pin != NC) {
            obj->serial.periph.uart->ROUTE |= USART_ROUTE_RXPEN;
        } else {
            obj->serial.periph.uart->CMD    = USART_CMD_RXBLOCKEN;
            obj->serial.periph.uart->ROUTE &= ~USART_ROUTE_RXPEN;
        }
#else
        if(obj->serial.location_tx != NC) {
            obj->serial.periph.uart->ROUTELOC0 = (obj->serial.periph.uart->ROUTELOC0 & (~_USART_ROUTELOC0_TXLOC_MASK)) | (obj->serial.location_tx << _USART_ROUTELOC0_TXLOC_SHIFT);
            obj->serial.periph.uart->ROUTEPEN  = (obj->serial.periph.uart->ROUTEPEN & (~_USART_ROUTEPEN_TXPEN_MASK)) | USART_ROUTEPEN_TXPEN;
        } else {
            obj->serial.periph.uart->ROUTEPEN  = (obj->serial.periph.uart->ROUTEPEN & (~_USART_ROUTEPEN_TXPEN_MASK));
        }
        if(obj->serial.location_rx != NC) {
            obj->serial.periph.uart->ROUTELOC0 = (obj->serial.periph.uart->ROUTELOC0 & (~_USART_ROUTELOC0_RXLOC_MASK)) | (obj->serial.location_rx << _USART_ROUTELOC0_RXLOC_SHIFT);
            obj->serial.periph.uart->ROUTEPEN  = (obj->serial.periph.uart->ROUTEPEN & (~_USART_ROUTEPEN_RXPEN_MASK)) | USART_ROUTEPEN_RXPEN;
        } else {
            obj->serial.periph.uart->CMD       = USART_CMD_RXBLOCKEN;
            obj->serial.periph.uart->ROUTEPEN  = (obj->serial.periph.uart->ROUTEPEN & (~_USART_ROUTEPEN_RXPEN_MASK));
        }
#endif
    }
}

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    serial_preinit(obj, tx, rx);

    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        // Set up LEUART clock tree
#ifdef LEUART_USING_LFXO
        //set to use LFXO
        CMU_ClockEnable(cmuClock_CORELE, true);
        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);
#else
        //set to use high-speed clock
#ifdef _SILICON_LABS_32B_PLATFORM_2
        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_HFCLKLE);
#else
        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_CORELEDIV2);
#endif
#endif
    }

    CMU_ClockEnable(serial_get_clock(obj), true);

    /* Configure UART for async operation */
    uart_init(obj, MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE, ParityNone, 1);

    /* Enable pins for UART at correct location */
    serial_set_route(obj);

    /* Reset interrupts */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        obj->serial.periph.leuart->IFC = LEUART_IFC_TXC;
        obj->serial.periph.leuart->CTRL |= LEUART_CTRL_RXDMAWU | LEUART_CTRL_TXDMAWU;
    } else {
        obj->serial.periph.uart->IFC = USART_IFC_TXC;
    }

    /* If this is the UART to be used for stdio, copy it to the stdio_uart struct */
    if(obj == &stdio_uart) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }

    serial_enable_pins(obj, true);
    serial_enable(obj, true);

    obj->serial.dmaOptionsTX.dmaChannel = -1;
    obj->serial.dmaOptionsTX.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;

    obj->serial.dmaOptionsRX.dmaChannel = -1;
    obj->serial.dmaOptionsRX.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;

}

void serial_free(serial_t *obj)
{
    if( LEUART_REF_VALID(obj->serial.periph.leuart) ) {
        LEUART_Enable(obj->serial.periph.leuart, leuartDisable);
    } else {
        USART_Enable(obj->serial.periph.uart, usartDisable);
    }
    serial_enable_pins(obj, false);
}

static void serial_enable(serial_t *obj, uint8_t enable)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        if (enable) {
            LEUART_Enable(obj->serial.periph.leuart, leuartEnable);
        } else {
            LEUART_Enable(obj->serial.periph.leuart, leuartDisable);
        }
    } else {
        if (enable) {
            USART_Enable(obj->serial.periph.uart, usartEnable);
        } else {
            USART_Enable(obj->serial.periph.uart, usartDisable);
        }
    }
    serial_irq_ids[serial_get_index(obj)] = 0;
}

/**
 * Set UART baud rate
 */
void serial_baud(serial_t *obj, int baudrate)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        serial_leuart_baud(obj, baudrate);
    } else {
        USART_BaudrateAsyncSet(obj->serial.periph.uart, REFERENCE_FREQUENCY, (uint32_t)baudrate, usartOVS16);
    }
}

/**
 * Set LEUART baud rate
 * Calculate whether LF or HF clock should be used.
 */
static void serial_leuart_baud(serial_t *obj, int baudrate)
{
#ifdef LEUART_USING_LFXO
    /* check if baudrate is within allowed range */
#if defined(_SILICON_LABS_32B_PLATFORM_2)
    // P2 has 9 bits + 5 fractional bits in LEUART CLKDIV register
    MBED_ASSERT(baudrate >= (LEUART_LF_REF_FREQ >> 9));
#else
    // P1 has 7 bits + 5 fractional bits in LEUART CLKDIV register
    MBED_ASSERT(baudrate >= (LEUART_LF_REF_FREQ >> 7));
#endif

    if(baudrate > (LEUART_LF_REF_FREQ >> 1)){
        // Baudrate is bigger than LFCLK/2 - we need to use the HF clock
        uint8_t divisor = 1;

#if defined(_SILICON_LABS_32B_PLATFORM_2)
        /* Check if baudrate is within allowed range: (HFCLK/4096, HFCLK/2] */
        MBED_ASSERT((baudrate <= (LEUART_HF_REF_FREQ >> 1)) && (baudrate > (LEUART_HF_REF_FREQ >> 12)));

        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_HFCLKLE);

        if(baudrate > (LEUART_HF_REF_FREQ >> 9)){
            divisor = 1;
        }else if(baudrate > (LEUART_HF_REF_FREQ >> 10)){
            divisor = 2;
        }else if(baudrate > (LEUART_HF_REF_FREQ >> 11)){
            divisor = 4;
        }else{
            divisor = 8;
        }
#else // P1
        /* Check if baudrate is within allowed range */
        MBED_ASSERT((baudrate <= (LEUART_HF_REF_FREQ >> 1)) && (baudrate > (LEUART_HF_REF_FREQ >> 10)));

        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_CORELEDIV2);

        if(baudrate > (LEUART_HF_REF_FREQ >> 7)){
            divisor = 1;
        }else if(baudrate > (LEUART_HF_REF_FREQ >> 8)){
            divisor = 2;
        }else if(baudrate > (LEUART_HF_REF_FREQ >> 9)){
            divisor = 4;
        }else{
            divisor = 8;
        }
#endif
        CMU_ClockDivSet(serial_get_clock(obj), divisor);
        LEUART_BaudrateSet(obj->serial.periph.leuart, LEUART_HF_REF_FREQ/divisor, (uint32_t)baudrate);
    }else{
        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);
        CMU_ClockDivSet(serial_get_clock(obj), 1);
        LEUART_BaudrateSet(obj->serial.periph.leuart, LEUART_LF_REF_FREQ, (uint32_t)baudrate);
    }
#else
    /* check if baudrate is within allowed range */
    MBED_ASSERT((baudrate > (LEUART_REF_FREQ >> 10)) && (baudrate <= (LEUART_REF_FREQ >> 1)));
    uint8_t divisor = 1;
    if(baudrate > (LEUART_REF_FREQ >> 7)){
        divisor = 1;
    }else if(baudrate > (LEUART_REF_FREQ >> 8)){
        divisor = 2;
    }else if(baudrate > (LEUART_REF_FREQ >> 9)){
        divisor = 4;
    }else{
        divisor = 8;
    }
    CMU_ClockDivSet(serial_get_clock(obj), divisor);
    LEUART_BaudrateSet(obj->serial.periph.leuart, LEUART_REF_FREQ/divisor, (uint32_t)baudrate);
#endif
}

/**
 * Set UART format by re-initializing the peripheral.
 */
void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        /* Save the serial state */
        uint8_t     was_enabled = LEUART_StatusGet(obj->serial.periph.leuart) & (LEUART_STATUS_TXENS | LEUART_STATUS_RXENS);
        uint32_t    enabled_interrupts = obj->serial.periph.leuart->IEN;

        LEUART_Init_TypeDef init = LEUART_INIT_DEFAULT;

        /* We support 8 data bits ONLY on LEUART*/
        MBED_ASSERT(data_bits == 8);

        /* Re-init the UART */
        init.enable = (was_enabled == 0 ? leuartDisable : leuartEnable);
        init.baudrate = LEUART_BaudrateGet(obj->serial.periph.leuart);
        if (stop_bits == 2) {
            init.stopbits = leuartStopbits2;
        } else {
            init.stopbits = leuartStopbits1;
        }
        switch (parity) {
            case ParityOdd:
            case ParityForced0:
                init.parity = leuartOddParity;
                break;
            case ParityEven:
            case ParityForced1:
                init.parity = leuartEvenParity;
                break;
            default: /* ParityNone */
                init.parity = leuartNoParity;
                break;
        }

        LEUART_Init(obj->serial.periph.leuart, &init);

        /* Re-enable pins for UART at correct location */
        serial_set_route(obj);

        /* Re-enable interrupts */
        if(was_enabled != 0) {
            obj->serial.periph.leuart->IFC = LEUART_IFC_TXC;
            obj->serial.periph.leuart->IEN = enabled_interrupts;
        }
    } else {
        /* Save the serial state */
        uint8_t     was_enabled = USART_StatusGet(obj->serial.periph.uart) & (USART_STATUS_TXENS | USART_STATUS_RXENS);
        uint32_t    enabled_interrupts = obj->serial.periph.uart->IEN;


        USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;

        /* We support 4 to 8 data bits */
        MBED_ASSERT(data_bits >= 4 && data_bits <= 8);

        /* Re-init the UART */
        init.enable = (was_enabled == 0 ? usartDisable : usartEnable);
        init.baudrate = USART_BaudrateGet(obj->serial.periph.uart);
        init.oversampling = usartOVS16;
        init.databits = (USART_Databits_TypeDef)((data_bits - 3) << _USART_FRAME_DATABITS_SHIFT);
        if (stop_bits == 2) {
            init.stopbits = usartStopbits2;
        } else {
            init.stopbits = usartStopbits1;
        }
        switch (parity) {
            case ParityOdd:
            case ParityForced0:
                init.parity = usartOddParity;
                break;
            case ParityEven:
            case ParityForced1:
                init.parity = usartEvenParity;
                break;
            default: /* ParityNone */
                init.parity = usartNoParity;
                break;
        }

        USART_InitAsync(obj->serial.periph.uart, &init);

        /* Re-enable pins for UART at correct location */
        serial_set_route(obj);

        /* Re-enable interrupts */
        if(was_enabled != 0) {
            obj->serial.periph.uart->IFC = USART_IFC_TXC;
            obj->serial.periph.uart->IEN = enabled_interrupts;
        }
    }
}

/******************************************************************************
 *                               INTERRUPTS                                   *
 ******************************************************************************/
uint8_t serial_tx_ready(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (obj->serial.periph.leuart->STATUS & LEUART_STATUS_TXBL) ? true : false;
    } else {
        return (obj->serial.periph.uart->STATUS & USART_STATUS_TXBL) ? true : false;
    }
}

uint8_t serial_rx_ready(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (obj->serial.periph.leuart->STATUS & LEUART_STATUS_RXDATAV) ? true : false;
    } else {
        return (obj->serial.periph.uart->STATUS & USART_STATUS_RXDATAV) ? true : false;
    }
}

void serial_write_asynch(serial_t *obj, int data)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        obj->serial.periph.leuart->TXDATA = (uint32_t)data;
    } else {
        obj->serial.periph.uart->TXDATA = (uint32_t)data;
    }
}

int serial_read_asynch(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (int)obj->serial.periph.leuart->RXDATA;
    } else {
        return (int)obj->serial.periph.uart->RXDATA;
    }
}

uint8_t serial_tx_int_flag(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (obj->serial.periph.leuart->IF & LEUART_IF_TXBL) ? true : false;
    } else {
        return (obj->serial.periph.uart->IF & USART_IF_TXBL) ? true : false;
    }
}

uint8_t serial_rx_int_flag(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (obj->serial.periph.leuart->IF & LEUART_IF_RXDATAV) ? true : false;
    } else {
        return (obj->serial.periph.uart->IF & USART_IF_RXDATAV) ? true : false;
    }
}

void serial_read_asynch_complete(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        obj->serial.periph.leuart->IFC |= LEUART_IFC_RXOF; // in case it got full
    } else {
        obj->serial.periph.uart->IFC |= USART_IFC_RXFULL; // in case it got full
    }
}

void serial_write_asynch_complete(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        obj->serial.periph.leuart->IFC |= LEUART_IFC_TXC;
    } else {
        obj->serial.periph.uart->IFC |= USART_IFC_TXC;
    }
}

/** Enable and set the interrupt handler for write (TX)
 *
 * @param obj     The serial object
 * @param address The address of TX handler
 * @param enable  Set to non-zero to enable or zero to disable
 */
void serial_write_enable_interrupt(serial_t *obj, uint32_t address, uint8_t enable)
{
    NVIC_SetVector(serial_get_tx_irq_index(obj), address);
    serial_irq_set(obj, (SerialIrq)1, enable);
}

/** Enable and set the interrupt handler for read (RX)
 *
 * @param obj     The serial object
 * @param address The address of RX handler
 * @param enable  Set to non-zero to enable or zero to disable
 */
void serial_read_enable_interrupt(serial_t *obj, uint32_t address, uint8_t enable)
{
    NVIC_SetVector(serial_get_rx_irq_index(obj), address);
    serial_irq_set(obj, (SerialIrq)0, enable);
}

uint8_t serial_interrupt_enabled(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (obj->serial.periph.leuart->IEN & (LEUART_IEN_RXDATAV | LEUART_IEN_TXBL)) ? true : false;
    } else {
        return (obj->serial.periph.uart->IEN & (USART_IEN_RXDATAV | USART_IEN_TXBL)) ? true : false;
    }
}

/**
 * Set handler for all serial interrupts (is probably SerialBase::_handler())
 * and store IRQ ID to be returned to the handler upon interrupt. ID is
 * probably a pointer to the calling Serial object.
 */
void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    serial_irq_ids[serial_get_index(obj)] = id;
}

/**
 * Generic ISR for all UARTs, both TX and RX
 */
static void uart_irq(UARTName name, SerialIrq irq)
{
    uint8_t index = serial_pointer_get_index((uint32_t)name);
    if (serial_irq_ids[index] != 0) {
        /* Pass interrupt on to mbed common handler */
        irq_handler(serial_irq_ids[index], irq);
        /* Clearing interrupt not necessary */
    }
}

/**
 * Set ISR for a given UART and interrupt event (TX or RX)
 */
void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        /* Enable or disable interrupt */
        if (enable) {
            if (irq == RxIrq) { /* RX */
                obj->serial.periph.leuart->IEN |= LEUART_IEN_RXDATAV;
                NVIC_ClearPendingIRQ(serial_get_rx_irq_index(obj));
                NVIC_EnableIRQ(serial_get_rx_irq_index(obj));
            } else { /* TX */
                obj->serial.periph.leuart->IEN |= LEUART_IEN_TXC;
                NVIC_ClearPendingIRQ(serial_get_tx_irq_index(obj));
                NVIC_SetPriority(serial_get_tx_irq_index(obj), 1);
                NVIC_EnableIRQ(serial_get_tx_irq_index(obj));
            }
        } else {
            if (irq == RxIrq) { /* RX */
                obj->serial.periph.leuart->IEN &= ~LEUART_IEN_RXDATAV;
                NVIC_DisableIRQ(serial_get_rx_irq_index(obj));
            } else { /* TX */
                obj->serial.periph.leuart->IEN &= ~LEUART_IEN_TXC;
                NVIC_DisableIRQ(serial_get_tx_irq_index(obj));
            }
        }
    } else {
        /* Enable or disable interrupt */
        if (enable) {
            if (irq == RxIrq) { /* RX */
                obj->serial.periph.uart->IEN |= USART_IEN_RXDATAV;
                NVIC_ClearPendingIRQ(serial_get_rx_irq_index(obj));
                NVIC_EnableIRQ(serial_get_rx_irq_index(obj));
            } else { /* TX */
                obj->serial.periph.uart->IEN |= USART_IEN_TXC;
                NVIC_ClearPendingIRQ(serial_get_tx_irq_index(obj));
                NVIC_SetPriority(serial_get_tx_irq_index(obj), 1);
                NVIC_EnableIRQ(serial_get_tx_irq_index(obj));
            }
        } else {
            if (irq == RxIrq) { /* RX */
                obj->serial.periph.uart->IEN &= ~USART_IEN_RXDATAV;
                NVIC_DisableIRQ(serial_get_rx_irq_index(obj));
            } else { /* TX */
                obj->serial.periph.uart->IEN &= ~USART_IEN_TXC;
                NVIC_DisableIRQ(serial_get_tx_irq_index(obj));
            }
        }
    }
}

/******************************************************************************
 *                               READ/WRITE                                   *
 ******************************************************************************/

/**
 *  Get one char from serial link
 */
int serial_getc(serial_t *obj)
{
    /* Emlib USART_Rx blocks until data is available, so we don't need to use
     * serial_readable(). Use USART_RxDataGet() to read register directly. */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return LEUART_Rx(obj->serial.periph.leuart);
    } else {
        return USART_Rx(obj->serial.periph.uart);
    }
}

/*
 * Send one char over serial link
 */
void serial_putc(serial_t *obj, int c)
{
    /* Emlib USART_Tx blocks until buffer is writable (non-full), so we don't
     * need to use serial_writable(). */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        LEUART_Tx(obj->serial.periph.leuart, (uint8_t)(c));
        while (!(obj->serial.periph.leuart->STATUS & LEUART_STATUS_TXC));
    } else {
        USART_Tx(obj->serial.periph.uart, (uint8_t)(c));
        while (!(obj->serial.periph.uart->STATUS & USART_STATUS_TXC));
    }
}

/**
 * Check if data is available in RX data vector
 */
int serial_readable(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return obj->serial.periph.leuart->STATUS & LEUART_STATUS_RXDATAV;
    } else {
        return obj->serial.periph.uart->STATUS & USART_STATUS_RXDATAV;
    }
}

/**
 * Check if TX buffer is empty
 */
int serial_writable(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return obj->serial.periph.leuart->STATUS & LEUART_STATUS_TXBL;
    } else {
        return obj->serial.periph.uart->STATUS & USART_STATUS_TXBL;
    }
}

/**
 * Check if TX buffer is empty
 */
int serial_tx_empty(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return ((obj->serial.periph.leuart->STATUS & LEUART_STATUS_TXBL) && (obj->serial.periph.leuart->STATUS & LEUART_STATUS_TXIDLE));
    } else {
        return ((obj->serial.periph.uart->STATUS & USART_STATUS_TXBL) && (obj->serial.periph.uart->STATUS & USART_STATUS_TXIDLE));
    }
}

/**
 * Clear UART interrupts
 */
void serial_clear(serial_t *obj)
{
    /* Interrupts automatically clear when condition is not met anymore */
}

void serial_break_set(serial_t *obj)
{
    /* Send transmission break */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        obj->serial.periph.leuart->TXDATAX = LEUART_TXDATAX_TXBREAK;
    } else {
        obj->serial.periph.uart->TXDATAX = USART_TXDATAX_TXBREAK;
    }
}

void serial_break_clear(serial_t *obj)
{
    /* No need to clear break, it is automatically cleared after one frame.
     * From the reference manual:
     *
     * By setting TXBREAK, the output will be held low during the stop-bit
     * period to generate a framing error. A receiver that supports break
     * detection detects this state, allowing it to be used e.g. for framing
     * of larger data packets. The line is driven high before the next frame
     * is transmitted so the next start condition can be identified correctly
     * by the recipient. Continuous breaks lasting longer than a USART frame
     * are thus not supported by the USART. GPIO can be used for this.
     */
}

void serial_pinout_tx(PinName tx)
{
    /* 0x10 sets DOUT high. Prevents false start. */
    pin_mode(tx, PushPull | 0x10);
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

/************************************************************************************
 *          DMA helper functions                                                    *
 ************************************************************************************/
/******************************************
* static void serial_dmaTransferComplete(uint channel, bool primary, void* user)
*
* Callback function which gets called upon DMA transfer completion
* the user-defined pointer is pointing to the CPP-land thunk
******************************************/
static void serial_dmaTransferComplete(unsigned int channel, bool primary, void *user)
{
    /* Store information about which channel triggered because CPP doesn't take arguments */
    serial_dma_irq_fired[channel] = true;

    /* User pointer should be a thunk to CPP land */
    if (user != NULL) {
        ((DMACallback)user)();
    }
}

#ifndef LDMA_PRESENT

/******************************************
* static void serial_setupDmaChannel(serial_t *obj, bool tx_nrx)
*
* Sets up the DMA configuration block for the assigned channel
* tx_nrx: true if configuring TX, false if configuring RX.
******************************************/
static void serial_dmaSetupChannel(serial_t *obj, bool tx_nrx)
{
    DMA_CfgChannel_TypeDef  channelConfig;

    if(tx_nrx) {
        //setup TX channel
        channelConfig.highPri = false;
        channelConfig.enableInt = true;
        channelConfig.cb = &(obj->serial.dmaOptionsTX.dmaCallback);

        switch((uint32_t)(obj->serial.periph.uart)) {
#ifdef UART0
            case UART_0:
                channelConfig.select = DMAREQ_UART0_TXBL;
                break;
#endif
#ifdef UART1
            case UART_1:
                channelConfig.select = DMAREQ_UART1_TXBL;
                break;
#endif
#ifdef USART0
            case USART_0:
                channelConfig.select = DMAREQ_USART0_TXBL;
                break;
#endif
#ifdef USART1
            case USART_1:
                channelConfig.select = DMAREQ_USART1_TXBL;
                break;
#endif
#ifdef USART2
            case USART_2:
                channelConfig.select = DMAREQ_USART2_TXBL;
                break;
#endif
#ifdef USART3
            case USART_3:
                channelConfig.select = DMAREQ_USART3_TXBL;
                break;
#endif
#ifdef USART4
            case USART_4:
                channelConfig.select = DMAREQ_USART4_TXBL;
                break;
#endif
#ifdef USART5
            case USART_5:
                channelConfig.select = DMAREQ_USART5_TXBL;
                break;
#endif
#ifdef LEUART0
            case LEUART_0:
                channelConfig.select = DMAREQ_LEUART0_TXBL;
                break;
#endif
#ifdef LEUART1
            case LEUART_1:
                channelConfig.select = DMAREQ_LEUART1_TXBL;
                break;
#endif
        }

        DMA_CfgChannel(obj->serial.dmaOptionsTX.dmaChannel, &channelConfig);
    } else {
        //setup RX channel
        channelConfig.highPri = true;
        channelConfig.enableInt = true;
        channelConfig.cb = &(obj->serial.dmaOptionsRX.dmaCallback);

        switch((uint32_t)(obj->serial.periph.uart)) {
#ifdef UART0
            case UART_0:
                channelConfig.select = DMAREQ_UART0_RXDATAV;
                break;
#endif
#ifdef UART1
            case UART_1:
                channelConfig.select = DMAREQ_UART1_RXDATAV;
                break;
#endif
#ifdef USART0
            case USART_0:
                channelConfig.select = DMAREQ_USART0_RXDATAV;
                break;
#endif
#ifdef USART1
            case USART_1:
                channelConfig.select = DMAREQ_USART1_RXDATAV;
                break;
#endif
#ifdef USART2
            case USART_2:
                channelConfig.select = DMAREQ_USART2_RXDATAV;
                break;
#endif
#ifdef USART3
            case USART_3:
                channelConfig.select = DMAREQ_USART3_RXDATAV;
                break;
#endif
#ifdef USART4
            case USART_4:
                channelConfig.select = DMAREQ_USART4_RXDATAV;
                break;
#endif
#ifdef USART5
            case USART_5:
                channelConfig.select = DMAREQ_USART5_RXDATAV;
                break;
#endif
#ifdef LEUART0
            case LEUART_0:
                channelConfig.select = DMAREQ_LEUART0_RXDATAV;
                break;
#endif
#ifdef LEUART1
            case LEUART_1:
                channelConfig.select = DMAREQ_LEUART1_RXDATAV;
                break;
#endif
        }

        DMA_CfgChannel(obj->serial.dmaOptionsRX.dmaChannel, &channelConfig);
    }
}

#endif /* LDMA_PRESENT */

/******************************************
* static void serial_dmaTrySetState(DMA_OPTIONS_t *obj, DMAUsage requestedState)
*
* Tries to set the passed DMA state to the requested state.
*
* requested state possibilities:
*   * NEVER:
*       if the previous state was always, will deallocate the channel
*   * OPPORTUNISTIC:
*       If the previous state was always, will reuse that channel but free upon next completion.
*       If not, will try to acquire a channel.
*       When allocated, state changes to DMA_USAGE_TEMPORARY_ALLOCATED.
*   * ALWAYS:
*       Will try to allocate a channel and keep it.
*       If succesfully allocated, state changes to DMA_USAGE_ALLOCATED.
******************************************/
static void serial_dmaTrySetState(DMA_OPTIONS_t *obj, DMAUsage requestedState, serial_t *serialPtr, bool tx_nrx)
{
    DMAUsage currentState = obj->dmaUsageState;
    int tempDMAChannel = -1;

    if ((requestedState == DMA_USAGE_ALWAYS) && (currentState != DMA_USAGE_ALLOCATED)) {
        /* Try to allocate channel */
        tempDMAChannel = dma_channel_allocate(DMA_CAP_NONE);
        if(tempDMAChannel >= 0) {
            obj->dmaChannel = tempDMAChannel;
            obj->dmaUsageState = DMA_USAGE_ALLOCATED;
            dma_init();
            serial_dmaSetupChannel(serialPtr, tx_nrx);
        }
    } else if (requestedState == DMA_USAGE_OPPORTUNISTIC) {
        if (currentState == DMA_USAGE_ALLOCATED) {
            /* Channels have already been allocated previously by an ALWAYS state, so after this transfer, we will release them */
            obj->dmaUsageState = DMA_USAGE_TEMPORARY_ALLOCATED;
        } else {
            /* Try to allocate channel */
            tempDMAChannel = dma_channel_allocate(DMA_CAP_NONE);
            if(tempDMAChannel >= 0) {
                obj->dmaChannel = tempDMAChannel;
                obj->dmaUsageState = DMA_USAGE_TEMPORARY_ALLOCATED;
                dma_init();
                serial_dmaSetupChannel(serialPtr, tx_nrx);
            }
        }
    } else if (requestedState == DMA_USAGE_NEVER) {
        /* If channel is allocated, get rid of it */
        dma_channel_free(obj->dmaChannel);
        obj->dmaChannel = -1;
        obj->dmaUsageState = DMA_USAGE_NEVER;
    }
}

#ifndef LDMA_PRESENT

static void serial_dmaActivate(serial_t *obj, void* cb, void* buffer, int length, bool tx_nrx)
{
    DMA_CfgDescr_TypeDef channelConfig;

    if(tx_nrx) {
        // Set DMA callback
        obj->serial.dmaOptionsTX.dmaCallback.cbFunc = serial_dmaTransferComplete;
        obj->serial.dmaOptionsTX.dmaCallback.userPtr = NULL;

        // Set up configuration structure
        channelConfig.dstInc = dmaDataIncNone;
        channelConfig.srcInc = dmaDataInc1;
        channelConfig.size = dmaDataSize1;
        channelConfig.arbRate = dmaArbitrate1;
        channelConfig.hprot = 0;

        // Clear TXC
        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_TXC);
        } else {
            USART_IntClear(obj->serial.periph.uart, USART_IFC_TXC);
        }

        // Set callback and enable TXC. This will fire once the
        // serial transfer finishes
        NVIC_SetVector(serial_get_tx_irq_index(obj), (uint32_t)cb);
        serial_irq_set(obj, TxIrq, true);

        DMA_CfgDescr(obj->serial.dmaOptionsTX.dmaChannel, true, &channelConfig);
        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            // Activate TX and clear TX buffer (note that clear must be done
            // separately and before TXEN or DMA will die on some platforms)
            obj->serial.periph.leuart->CMD = LEUART_CMD_CLEARTX;
            obj->serial.periph.leuart->CMD = LEUART_CMD_TXEN;
            while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);

            // Kick off TX DMA
            DMA_ActivateBasic(obj->serial.dmaOptionsTX.dmaChannel, true, false, (void*) &(obj->serial.periph.leuart->TXDATA), buffer, length - 1);
        } else {
            // Activate TX amd clear TX buffer
            obj->serial.periph.uart->CMD = USART_CMD_TXEN | USART_CMD_CLEARTX;

            // Kick off TX DMA
            DMA_ActivateBasic(obj->serial.dmaOptionsTX.dmaChannel, true, false, (void*) &(obj->serial.periph.uart->TXDATA), buffer, length - 1);
        }


    } else {
        // Set DMA callback
        obj->serial.dmaOptionsRX.dmaCallback.cbFunc = serial_dmaTransferComplete;
        obj->serial.dmaOptionsRX.dmaCallback.userPtr = cb;

        // Set up configuration structure
        channelConfig.dstInc = dmaDataInc1;
        channelConfig.srcInc = dmaDataIncNone;
        channelConfig.size = dmaDataSize1;
        channelConfig.arbRate = dmaArbitrate1;
        channelConfig.hprot = 0;

        DMA_CfgDescr(obj->serial.dmaOptionsRX.dmaChannel, true, &channelConfig);

        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            // Activate RX and clear RX buffer
            obj->serial.periph.leuart->CMD = LEUART_CMD_CLEARRX;
            obj->serial.periph.leuart->CMD = LEUART_CMD_RXEN;
            while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);

            // Kick off RX DMA
            DMA_ActivateBasic(obj->serial.dmaOptionsRX.dmaChannel, true, false, buffer, (void*) &(obj->serial.periph.leuart->RXDATA), length - 1);
        } else {
            // Activate RX and clear RX buffer
            obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;

            // Kick off RX DMA
            DMA_ActivateBasic(obj->serial.dmaOptionsRX.dmaChannel, true, false, buffer, (void*) &(obj->serial.periph.uart->RXDATA), length - 1);
        }
    }
}

#endif


#ifdef LDMA_PRESENT

static void serial_dmaSetupChannel(serial_t *obj, bool tx_nrx)
{
}

static void serial_dmaActivate(serial_t *obj, void* cb, void* buffer, int length, bool tx_nrx)
{
    LDMA_PeripheralSignal_t dma_periph;

    obj->serial.dmaOptionsRX.dmaCallback.userPtr = cb;

    if( tx_nrx ) {
        volatile void *target_addr;

        // Clear TXC
        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_TXC);
        } else {
            USART_IntClear(obj->serial.periph.uart, USART_IFC_TXC);
        }

        switch((uint32_t)(obj->serial.periph.uart)) {
#ifdef USART0
            case USART_0:
                dma_periph = ldmaPeripheralSignal_USART0_TXBL;
                target_addr = &USART0->TXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_TXEN | USART_CMD_CLEARTX;
                break;
#endif
#ifdef USART1
            case USART_1:
                dma_periph = ldmaPeripheralSignal_USART1_TXBL;
                target_addr = &USART1->TXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_TXEN | USART_CMD_CLEARTX;
                break;
#endif
#ifdef LEUART0
            case LEUART_0:
                dma_periph = ldmaPeripheralSignal_LEUART0_TXBL;
                target_addr = &LEUART0->TXDATA;
                obj->serial.periph.leuart->CMD = LEUART_CMD_CLEARTX;
                obj->serial.periph.leuart->CMD = LEUART_CMD_TXEN;
                while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);
                break;
#endif
            default:
                MBED_ASSERT(0);
                while(1);
                break;
        }

        // Set callback and enable TXC. This will fire once the
        // serial transfer finishes
        NVIC_SetVector(serial_get_tx_irq_index(obj), (uint32_t)cb);
        serial_irq_set(obj, TxIrq, true);

        // Start DMA transfer
        LDMA_TransferCfg_t xferConf = LDMA_TRANSFER_CFG_PERIPHERAL(dma_periph);
        LDMA_Descriptor_t desc = LDMA_DESCRIPTOR_SINGLE_M2P_BYTE(buffer, target_addr, length);
        LDMAx_StartTransfer(obj->serial.dmaOptionsTX.dmaChannel, &xferConf, &desc, serial_dmaTransferComplete, NULL);

    } else {
        volatile const void *source_addr;

        switch((uint32_t)(obj->serial.periph.uart)) {
#ifdef USART0
            case USART_0:
                dma_periph = ldmaPeripheralSignal_USART0_RXDATAV;
                source_addr = &USART0->RXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;
                break;
#endif
#ifdef USART1
            case USART_1:
                dma_periph = ldmaPeripheralSignal_USART1_RXDATAV;
                source_addr = &USART1->RXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;
                break;
#endif
#ifdef USART2
            case USART_2:
                dma_periph = ldmaPeripheralSignal_USART2_RXDATAV;
                source_addr = &USART2->RXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;
                break;
#endif
#ifdef USART3
            case USART_3:
                dma_periph = ldmaPeripheralSignal_USART3_RXDATAV;
                source_addr = &USART3->RXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;
                break;
#endif
#ifdef USART4
            case USART_4:
                dma_periph = ldmaPeripheralSignal_USART4_RXDATAV;
                source_addr = &USART4->RXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;
                break;
#endif
#ifdef USART5
            case USART_5:
                dma_periph = ldmaPeripheralSignal_USART5_RXDATAV;
                source_addr = &USART5->RXDATA;
                obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;
                break;
#endif
#ifdef LEUART0
            case LEUART_0:
                dma_periph = ldmaPeripheralSignal_LEUART0_RXDATAV;
                source_addr = &LEUART0->RXDATA;
                obj->serial.periph.leuart->CMD = LEUART_CMD_CLEARRX;
                obj->serial.periph.leuart->CMD = LEUART_CMD_RXEN;
                while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);
                break;
#endif
            default:
                MBED_ASSERT(0);
                while(1);
                break;
        }

        LDMA_TransferCfg_t xferConf = LDMA_TRANSFER_CFG_PERIPHERAL(dma_periph);
        LDMA_Descriptor_t desc = LDMA_DESCRIPTOR_SINGLE_P2M_BYTE(source_addr, buffer, length);
        LDMAx_StartTransfer(obj->serial.dmaOptionsRX.dmaChannel, &xferConf, &desc, serial_dmaTransferComplete, cb);
    }
}

#endif /* LDMA_PRESENT */

/************************************************************************************
 *          ASYNCHRONOUS HAL                                                        *
 ************************************************************************************/

#if DEVICE_SERIAL_ASYNCH

/************************************
 * HELPER FUNCTIONS                 *
 ***********************************/

/** Configure TX events
 *
 * @param obj    The serial object
 * @param event  The logical OR of the TX events to configure
 * @param enable Set to non-zero to enable events, or zero to disable them
 */
void serial_tx_enable_event(serial_t *obj, int event, uint8_t enable)
{
    // Shouldn't have to enable TX interrupt here, just need to keep track of the requested events.
    if(enable) obj->serial.events |= event;
    else obj->serial.events &= ~event;
}

/**
 * @param obj    The serial object.
 * @param event  The logical OR of the RX events to configure
 * @param enable Set to non-zero to enable events, or zero to disable them
 */
void serial_rx_enable_event(serial_t *obj, int event, uint8_t enable)
{
    if(enable) {
        obj->serial.events |= event;
    } else {
        obj->serial.events &= ~event;
    }
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        if(event & SERIAL_EVENT_RX_FRAMING_ERROR) {
            //FERR interrupt source
            if(enable) obj->serial.periph.leuart->IEN |= LEUART_IEN_FERR;
            else obj->serial.periph.leuart->IEN &= ~LEUART_IEN_FERR;
        }
        if(event & SERIAL_EVENT_RX_PARITY_ERROR) {
            //PERR interrupt source
            if(enable) obj->serial.periph.leuart->IEN |= LEUART_IEN_PERR;
            else obj->serial.periph.leuart->IEN &= ~LEUART_IEN_PERR;
        }
        if(event & SERIAL_EVENT_RX_OVERFLOW) {
            //RXOF interrupt source
            if(enable) obj->serial.periph.leuart->IEN |= LEUART_IEN_RXOF;
            else obj->serial.periph.leuart->IEN &= ~LEUART_IEN_RXOF;
        }
    } else {
        if(event & SERIAL_EVENT_RX_FRAMING_ERROR) {
            //FERR interrupt source
            if(enable) obj->serial.periph.uart->IEN |= USART_IEN_FERR;
            else obj->serial.periph.uart->IEN &= ~USART_IEN_FERR;
        }
        if(event & SERIAL_EVENT_RX_PARITY_ERROR) {
            //PERR interrupt source
            if(enable) obj->serial.periph.uart->IEN |= USART_IEN_PERR;
            else obj->serial.periph.uart->IEN &= ~USART_IEN_PERR;
        }
        if(event & SERIAL_EVENT_RX_OVERFLOW) {
            //RXOF interrupt source
            if(enable) obj->serial.periph.uart->IEN |= USART_IEN_RXOF;
            else obj->serial.periph.uart->IEN &= ~USART_IEN_RXOF;
        }
    }
}

/** Configure the TX buffer for an asynchronous write serial transaction
 *
 * @param obj       The serial object.
 * @param tx        The buffer for sending.
 * @param tx_length The number of words to transmit.
 */
void serial_tx_buffer_set(serial_t *obj, void *tx, int tx_length, uint8_t width)
{
    // We only support byte buffers for now
    MBED_ASSERT(width == 8);

    if(serial_tx_active(obj)) return;

    obj->tx_buff.buffer = tx;
    obj->tx_buff.length = tx_length;
    obj->tx_buff.pos = 0;

    return;
}

/** Configure the TX buffer for an asynchronous read serial transaction
 *
 * @param obj       The serial object.
 * @param rx        The buffer for receiving.
 * @param rx_length The number of words to read.
 */
void serial_rx_buffer_set(serial_t *obj, void *rx, int rx_length, uint8_t width)
{
    // We only support byte buffers for now
    MBED_ASSERT(width == 8);

    if(serial_rx_active(obj)) return;

    obj->rx_buff.buffer = rx;
    obj->rx_buff.length = rx_length;
    obj->rx_buff.pos = 0;

    return;
}

/************************************
 * TRANSFER FUNCTIONS               *
 ***********************************/

/** Begin asynchronous TX transfer. The used buffer is specified in the serial object,
 *  tx_buff
 *
 * @param obj  The serial object
 * @param cb   The function to call when an event occurs
 * @param hint A suggestion for how to use DMA with this transfer
 * @return Returns number of data transfered, or 0 otherwise
 */
int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint)
{
   // Check that a buffer has indeed been set up
    MBED_ASSERT(tx != (void*)0);
    if(tx_length == 0) return 0;

    // Set up buffer
    serial_tx_buffer_set(obj, (void *)tx, tx_length, tx_width);

    // Set up events
    serial_tx_enable_event(obj, SERIAL_EVENT_TX_ALL, false);
    serial_tx_enable_event(obj, event, true);

    // Set up sleepmode
    serial_block_sleep(obj);

    // Determine DMA strategy
    serial_dmaTrySetState(&(obj->serial.dmaOptionsTX), hint, obj, true);

    // If DMA, kick off DMA transfer
    if(obj->serial.dmaOptionsTX.dmaChannel >= 0) {
        serial_dmaActivate(obj, (void*)handler, obj->tx_buff.buffer, obj->tx_buff.length, true);
    }
    // Else, activate interrupt. TXBL will take care of buffer filling through ISR.
    else {
        // Store callback
        NVIC_ClearPendingIRQ(serial_get_tx_irq_index(obj));
        NVIC_DisableIRQ(serial_get_tx_irq_index(obj));
        NVIC_SetPriority(serial_get_tx_irq_index(obj), 1);
        NVIC_SetVector(serial_get_tx_irq_index(obj), (uint32_t)handler);
        NVIC_EnableIRQ(serial_get_tx_irq_index(obj));

        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            // Activate TX and clear TX buffer
            obj->serial.periph.leuart->CMD = LEUART_CMD_CLEARTX;
            obj->serial.periph.leuart->CMD = LEUART_CMD_TXEN;
            while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);

            // Enable interrupt
            LEUART_IntEnable(obj->serial.periph.leuart, LEUART_IEN_TXBL);
        } else {
            // Activate TX and clear TX buffer
            obj->serial.periph.uart->CMD = USART_CMD_TXEN | USART_CMD_CLEARTX;

            // Enable interrupt
            USART_IntEnable(obj->serial.periph.uart, USART_IEN_TXBL);
        }
    }

    return 0;
}

/** Begin asynchronous RX transfer (enable interrupt for data collecting)
 *  The used buffer is specified in the serial object - rx_buff
 *
 * @param obj  The serial object
 * @param cb   The function to call when an event occurs
 * @param hint A suggestion for how to use DMA with this transfer
 */
void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint)
{
    // Check that a buffer has indeed been set up
    MBED_ASSERT(rx != (void*)0);
    if(rx_length == 0) return;

    // Set up buffer
    serial_rx_buffer_set(obj,(void*) rx, rx_length, rx_width);

    //disable character match if no character is specified
    if(char_match == SERIAL_RESERVED_CHAR_MATCH){
        event &= ~SERIAL_EVENT_RX_CHARACTER_MATCH;
    }

    /*clear all set interrupts*/
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_PERR | LEUART_IFC_FERR | LEUART_IFC_RXOF);
    }else{
        USART_IntClear(obj->serial.periph.uart,  USART_IFC_PERR | USART_IFC_FERR | USART_IFC_RXOF);
    }

    // Set up events
    serial_rx_enable_event(obj, SERIAL_EVENT_RX_ALL, false);
    serial_rx_enable_event(obj, event, true);
    obj->char_match = char_match;

    // Set up sleepmode
    serial_block_sleep(obj);

    // Determine DMA strategy
    // If character match is enabled, we can't use DMA, sadly. We could when using LEUART though, but that support is not in here yet.
    // TODO: add DMA support for character matching with leuart
    if(!(event & SERIAL_EVENT_RX_CHARACTER_MATCH)) {
        serial_dmaTrySetState(&(obj->serial.dmaOptionsRX), hint, obj, false);
    }else{
        serial_dmaTrySetState(&(obj->serial.dmaOptionsRX), DMA_USAGE_NEVER, obj, false);
    }

    // If DMA, kick off DMA
    if(obj->serial.dmaOptionsRX.dmaChannel >= 0) {
        serial_dmaActivate(obj, (void*)handler, obj->rx_buff.buffer, obj->rx_buff.length, false);
    }
    // Else, activate interrupt. RXDATAV is responsible for incoming data notification.
    else {
        // Store callback
        NVIC_ClearPendingIRQ(serial_get_rx_irq_index(obj));
        NVIC_SetVector(serial_get_rx_irq_index(obj), (uint32_t)handler);
        NVIC_EnableIRQ(serial_get_rx_irq_index(obj));

        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            // Activate RX and clear RX buffer
            obj->serial.periph.leuart->CMD = LEUART_CMD_CLEARRX;
            obj->serial.periph.leuart->CMD = LEUART_CMD_RXEN;
            while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);

            // Enable interrupt
            LEUART_IntEnable(obj->serial.periph.leuart, LEUART_IEN_RXDATAV);
        } else {
            // Activate RX and clear RX buffer
            obj->serial.periph.uart->CMD = USART_CMD_RXEN | USART_CMD_CLEARRX;

            // Clear RXFULL
            USART_IntClear(obj->serial.periph.uart, USART_IFC_RXFULL);

            // Enable interrupt
            USART_IntEnable(obj->serial.periph.uart, USART_IEN_RXDATAV);
        }
    }

    return;
}

/** Attempts to determine if the serial peripheral is already in use for TX
 *
 * @param obj The serial object
 * @return Non-zero if the TX transaction is ongoing, 0 otherwise
 */
uint8_t serial_tx_active(serial_t *obj)
{
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        return (obj->serial.periph.leuart->IEN & (LEUART_IEN_TXBL|LEUART_IEN_TXC)) ? true : false;
    } else {
        return (obj->serial.periph.uart->IEN & (USART_IEN_TXBL|USART_IEN_TXC)) ? true : false;
    }
}

/** Attempts to determine if the serial peripheral is already in use for RX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_rx_active(serial_t *obj)
{
    switch(obj->serial.dmaOptionsRX.dmaUsageState) {
        case DMA_USAGE_TEMPORARY_ALLOCATED:
            /* Temporary allocation always means its active, as this state gets cleared afterwards */
            return 1;
        case DMA_USAGE_ALLOCATED:
            /* Check whether the allocated DMA channel is active by checking the DMA transfer */
#ifndef LDMA_PRESENT
            return DMA_ChannelEnabled(obj->serial.dmaOptionsRX.dmaChannel);
#else
            // LDMA_TransferDone does not work since the CHDONE bits get cleared,
            // so just check if the channel is enabled
            return LDMA->CHEN & (1 << obj->serial.dmaOptionsRX.dmaChannel);
#endif
        default:
            /* Check whether interrupt for serial TX is enabled */
            if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
                return (obj->serial.periph.leuart->IEN & (LEUART_IEN_RXDATAV)) ? true : false;
            } else {
                return (obj->serial.periph.uart->IEN & (USART_IEN_RXDATAV)) ? true : false;
            }
    }
}

/** The asynchronous TX handler. Writes to the TX FIFO and checks for events.
 *  If any TX event has occured, the TX abort function is called.
 *
 * @param obj The serial object
 * @return Returns event flags if a TX transfer termination condition was met or 0 otherwise
 */
int serial_tx_irq_handler_asynch(serial_t *obj)
{
    /* This interrupt handler is called from USART irq */
    uint8_t *buf = obj->tx_buff.buffer;

    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        if(obj->serial.periph.leuart->IEN & LEUART_IEN_TXBL){
            /* There is still data to send */
            while((LEUART_StatusGet(obj->serial.periph.leuart) & LEUART_STATUS_TXBL) && (obj->tx_buff.pos <= (obj->tx_buff.length - 1))) {
                while (obj->serial.periph.leuart->SYNCBUSY);
                LEUART_Tx(obj->serial.periph.leuart, buf[obj->tx_buff.pos]);
                obj->tx_buff.pos++;
            }
            if(obj->tx_buff.pos >= obj->tx_buff.length){
                /* Last byte has been put in TX, set up TXC interrupt */
                LEUART_IntDisable(obj->serial.periph.leuart, LEUART_IEN_TXBL);
                LEUART_IntEnable(obj->serial.periph.leuart, LEUART_IEN_TXC);
                while (obj->serial.periph.leuart->SYNCBUSY);
            }
        }else if (obj->serial.periph.leuart->IF & LEUART_IF_TXC){
            /* Last byte has been successfully transmitted. Stop the procedure */
            serial_tx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_TX_COMPLETE & obj->serial.events;
        }
    } else {
        if(obj->serial.periph.uart->IEN & USART_IEN_TXBL){
            /* There is still data to send */
            while((USART_StatusGet(obj->serial.periph.uart) & USART_STATUS_TXBL) && (obj->tx_buff.pos <= (obj->tx_buff.length - 1))) {
                USART_Tx(obj->serial.periph.uart, buf[obj->tx_buff.pos]);
                obj->tx_buff.pos++;
            }
            if(obj->tx_buff.pos >= obj->tx_buff.length){
                /* Last byte has been put in TX, set up TXC interrupt */
                USART_IntDisable(obj->serial.periph.uart, USART_IEN_TXBL);
                USART_IntEnable(obj->serial.periph.uart, USART_IEN_TXC);
            }
        } else if (obj->serial.periph.uart->IF & USART_IF_TXC) {
            /* Last byte has been successfully transmitted. Stop the procedure */
            serial_tx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_TX_COMPLETE & obj->serial.events;
        }
    }
    return 0;
}

/** The asynchronous RX handler. Reads from the RX FIFOF and checks for events.
 *  If any RX event has occured, the RX abort function is called.
 *
 * @param obj The serial object
 * @return Returns event flags if a RX transfer termination condition was met or 0 otherwise
 */
int serial_rx_irq_handler_asynch(serial_t *obj)
{
    int event = 0;

    /* This interrupt handler is called from USART irq */
    uint8_t *buf = (uint8_t*)obj->rx_buff.buffer;

    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        /* Determine the source of the interrupt */
        if(LEUART_IntGetEnabled(obj->serial.periph.leuart) & LEUART_IF_PERR) {
            /* Parity error has occurred, and we are notifying. */
            LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_PERR);
            serial_rx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_RX_PARITY_ERROR;
        }

        if(LEUART_IntGetEnabled(obj->serial.periph.leuart) & LEUART_IF_FERR) {
            /* Framing error has occurred, and we are notifying */
            LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_FERR);
            serial_rx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_RX_FRAMING_ERROR;
        }

        if(LEUART_IntGetEnabled(obj->serial.periph.leuart) & LEUART_IF_RXOF) {
            /* RX buffer overflow has occurred, and we are notifying */
            LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_RXOF);
            serial_rx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_RX_OVERFLOW;
        }

        if((LEUART_IntGetEnabled(obj->serial.periph.leuart) & LEUART_IF_RXDATAV) || (LEUART_StatusGet(obj->serial.periph.leuart) & LEUART_STATUS_RXDATAV)) {
            /* Valid data in buffer. Determine course of action: continue receiving or interrupt */
            if(obj->rx_buff.pos >= (obj->rx_buff.length - 1)) {
                /* Last char, transfer complete. Switch off interrupt and return event. */
                buf[obj->rx_buff.pos] = LEUART_RxDataGet(obj->serial.periph.leuart);

                event |= SERIAL_EVENT_RX_COMPLETE;

                if((buf[obj->rx_buff.pos] == obj->char_match) && (obj->serial.events & SERIAL_EVENT_RX_CHARACTER_MATCH)) event |= SERIAL_EVENT_RX_CHARACTER_MATCH;

                serial_rx_abort_asynch_intern(obj, 1);
                return event & obj->serial.events;
            } else {
                /* There's still space in the receive buffer */
                while((LEUART_StatusGet(obj->serial.periph.leuart) & LEUART_STATUS_RXDATAV) && (obj->rx_buff.pos <= (obj->rx_buff.length - 1))) {
                    bool aborting = false;
                    buf[obj->rx_buff.pos] = LEUART_RxDataGet(obj->serial.periph.leuart);
                    obj->rx_buff.pos++;

                    /* Check for character match event */
                    if((buf[obj->rx_buff.pos - 1] == obj->char_match) && (obj->serial.events & SERIAL_EVENT_RX_CHARACTER_MATCH)) {
                        aborting = true;
                        event |= SERIAL_EVENT_RX_CHARACTER_MATCH;
                    }

                    /* Check for final char event */
                    if(obj->rx_buff.pos >= (obj->rx_buff.length)) {
                        aborting = true;
                        event |= SERIAL_EVENT_RX_COMPLETE & obj->serial.events;
                    }

                    if(aborting) {
                        serial_rx_abort_asynch_intern(obj, 1);
                        return event & obj->serial.events;
                    }
                }
            }
        }
    } else {
        /* Determine the source of the interrupt */
        if(USART_IntGetEnabled(obj->serial.periph.uart) & USART_IF_PERR) {
            /* Parity error has occurred, and we are notifying. */
            USART_IntClear(obj->serial.periph.uart, USART_IFC_PERR);
            serial_rx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_RX_PARITY_ERROR;
        }

        if(USART_IntGetEnabled(obj->serial.periph.uart) & USART_IF_FERR) {
            /* Framing error has occurred, and we are notifying */
            USART_IntClear(obj->serial.periph.uart, USART_IFC_FERR);
            serial_rx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_RX_FRAMING_ERROR;
        }

        if(USART_IntGetEnabled(obj->serial.periph.uart) & USART_IF_RXOF) {
            /* RX buffer overflow has occurred, and we are notifying */
            USART_IntClear(obj->serial.periph.uart, USART_IFC_RXOF);
            serial_rx_abort_asynch_intern(obj, 1);
            return SERIAL_EVENT_RX_OVERFLOW;
        }

        if((USART_IntGetEnabled(obj->serial.periph.uart) & USART_IF_RXDATAV) || (USART_StatusGet(obj->serial.periph.uart) & USART_STATUS_RXFULL)) {
            /* Valid data in buffer. Determine course of action: continue receiving or interrupt */
            if(obj->rx_buff.pos >= (obj->rx_buff.length - 1)) {
                /* Last char, transfer complete. Switch off interrupt and return event. */
                buf[obj->rx_buff.pos] = USART_RxDataGet(obj->serial.periph.uart);

                event |= SERIAL_EVENT_RX_COMPLETE;

                if((buf[obj->rx_buff.pos] == obj->char_match) && (obj->serial.events & SERIAL_EVENT_RX_CHARACTER_MATCH)) event |= SERIAL_EVENT_RX_CHARACTER_MATCH;

                serial_rx_abort_asynch_intern(obj, 1);
                return event & obj->serial.events;
            } else {
                /* There's still space in the receive buffer */
                while(((USART_StatusGet(obj->serial.periph.uart) & USART_STATUS_RXDATAV) || (USART_StatusGet(obj->serial.periph.uart) & USART_IF_RXFULL)) && (obj->rx_buff.pos <= (obj->rx_buff.length - 1))) {
                    bool aborting = false;
                    buf[obj->rx_buff.pos] = USART_RxDataGet(obj->serial.periph.uart);
                    obj->rx_buff.pos++;

                    /* Check for character match event */
                    if((buf[obj->rx_buff.pos - 1] == obj->char_match) && (obj->serial.events & SERIAL_EVENT_RX_CHARACTER_MATCH)) {
                        aborting = true;
                        event |= SERIAL_EVENT_RX_CHARACTER_MATCH;
                    }

                    /* Check for final char event */
                    if(obj->rx_buff.pos >= (obj->rx_buff.length)) {
                        aborting = true;
                        event |= SERIAL_EVENT_RX_COMPLETE & obj->serial.events;
                    }

                    if(aborting) {
                        serial_rx_abort_asynch_intern(obj, 1);
                        return event & obj->serial.events;
                    }
                }
            }
        }
    }

    /* All events should have generated a return, if no return has happened, no event has been caught */
    return 0;
}

/** Unified IRQ handler. Determines the appropriate handler to execute and returns the flags.
 *
 * WARNING: this code should be stateless, as re-entrancy is very possible in interrupt-based mode.
 */
int serial_irq_handler_asynch(serial_t *obj)
{
    uint32_t txc_int;

    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        txc_int = LEUART_IntGetEnabled(obj->serial.periph.leuart) & LEUART_IF_TXC;
    } else {
        txc_int = USART_IntGetEnabled(obj->serial.periph.uart) & USART_IF_TXC;
    }

    /* First, check if we're running in DMA mode */
    if( (obj->serial.dmaOptionsRX.dmaChannel != -1) &&
        serial_dma_irq_fired[obj->serial.dmaOptionsRX.dmaChannel]) {
        /* Clean up */
        serial_dma_irq_fired[obj->serial.dmaOptionsRX.dmaChannel] = false;
        serial_rx_abort_asynch_intern(obj, 1);

        /* Notify CPP land of RX completion */
        return SERIAL_EVENT_RX_COMPLETE & obj->serial.events;
    } else if (txc_int && (obj->serial.dmaOptionsTX.dmaChannel != -1) &&
               serial_dma_irq_fired[obj->serial.dmaOptionsTX.dmaChannel]) {
        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            /* Clean up */
            serial_dma_irq_fired[obj->serial.dmaOptionsTX.dmaChannel] = false;
            serial_tx_abort_asynch_intern(obj, 1);
            /* Notify CPP land of completion */
            return SERIAL_EVENT_TX_COMPLETE & obj->serial.events;
        }else{
            /* Clean up */
            serial_dma_irq_fired[obj->serial.dmaOptionsTX.dmaChannel] = false;
            serial_tx_abort_asynch_intern(obj, 1);
            /* Notify CPP land of completion */
            return SERIAL_EVENT_TX_COMPLETE & obj->serial.events;
        }
    } else {
        /* Check the NVIC to see which interrupt we're running from
         * Also make sure to prioritize RX */
        if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
            //Different method of checking tx vs rx for LEUART
            if(LEUART_IntGetEnabled(obj->serial.periph.leuart) & (LEUART_IF_RXDATAV | LEUART_IF_FERR | LEUART_IF_PERR | LEUART_IF_RXOF)) {
                return serial_rx_irq_handler_asynch(obj);
            } else if(LEUART_StatusGet(obj->serial.periph.leuart) & (LEUART_STATUS_TXBL | LEUART_STATUS_TXC)) {
                return serial_tx_irq_handler_asynch(obj);
            }
        } else {
            if(USART_IntGetEnabled(obj->serial.periph.uart) & (USART_IF_RXDATAV | USART_IF_RXOF | USART_IF_PERR | USART_IF_FERR)) {
                return serial_rx_irq_handler_asynch(obj);
            } else if(USART_StatusGet(obj->serial.periph.uart) & (USART_STATUS_TXBL | USART_STATUS_TXC)){
                return serial_tx_irq_handler_asynch(obj);
            }
        }
    }

    // All should be done now
    return 0;
}

/** Abort the ongoing TX transaction. It disables the enabled interupt for TX and
 *  flush TX hardware buffer if TX FIFO is used
 *
 * @param obj The serial object
 */
void serial_tx_abort_asynch(serial_t *obj)
{
    serial_tx_abort_asynch_intern(obj, 0);
}

static void serial_tx_abort_asynch_intern(serial_t *obj, int unblock_sleep)
{
    // Transmitter should be disabled here but there are multiple issues
    // making that quite difficult.
    //
    // - Disabling the transmitter when using DMA on platforms prior to
    //   Pearl can cause the UART to leave the line low, generating a break
    //   condition until the next transmission begins.
    //
    // - On (at least) Pearl, once TXC interrupt has fired it will take some time
    //   (some tens of microsec) for TXC to be set in STATUS. If we turn off
    //   the transmitter before this, bad things will happen.
    //
    // - On (at least) Pearl, when using TX DMA it is possible for the USART
    //   status to be: TXENS TXBL TXIDLE = 1, TXBUFCNT = 0, but TXC = 0.
    //
    // All in all, the logic was so fragile it's best to leave it out.

    /* Clean up */
    switch(obj->serial.dmaOptionsTX.dmaUsageState) {
        case DMA_USAGE_ALLOCATED:
            /* stop DMA transfer */
#ifndef LDMA_PRESENT
            DMA_ChannelEnable(obj->serial.dmaOptionsTX.dmaChannel, false);
#else
            LDMA_StopTransfer(obj->serial.dmaOptionsTX.dmaChannel);
#endif
            break;
        case DMA_USAGE_TEMPORARY_ALLOCATED:
            /* stop DMA transfer and release channel */
#ifndef LDMA_PRESENT
            DMA_ChannelEnable(obj->serial.dmaOptionsTX.dmaChannel, false);
#else
            LDMA_StopTransfer(obj->serial.dmaOptionsTX.dmaChannel);
#endif
            dma_channel_free(obj->serial.dmaOptionsTX.dmaChannel);
            obj->serial.dmaOptionsTX.dmaChannel = -1;
            obj->serial.dmaOptionsTX.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;
            break;
        default:
            break;
    }

    /* stop interrupting */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        LEUART_IntDisable(obj->serial.periph.leuart, LEUART_IEN_TXBL);
        LEUART_IntDisable(obj->serial.periph.leuart, LEUART_IEN_TXC);
        LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_TXC);
    } else {
        USART_IntDisable(obj->serial.periph.uart, USART_IEN_TXBL);
        USART_IntDisable(obj->serial.periph.uart, USART_IEN_TXC);
        USART_IntClear(obj->serial.periph.uart, USART_IFC_TXC);
    }

    /* Say that we can stop using this emode */
    if(unblock_sleep)
        serial_unblock_sleep(obj);
}


static void serial_unblock_sleep(serial_t *obj)
{
    if( obj->serial.sleep_blocked > 0 ) {
#ifdef LEUART_USING_LFXO
        if(!LEUART_REF_VALID(obj->serial.periph.leuart) || (LEUART_BaudrateGet(obj->serial.periph.leuart) > (LEUART_LF_REF_FREQ/2))){
            sleep_manager_unlock_deep_sleep();
        }
#else
        sleep_manager_unlock_deep_sleep();
#endif
        obj->serial.sleep_blocked--;
    }
}

static void serial_block_sleep(serial_t *obj)
{
    obj->serial.sleep_blocked++;
#ifdef LEUART_USING_LFXO
    if(!LEUART_REF_VALID(obj->serial.periph.leuart) || (LEUART_BaudrateGet(obj->serial.periph.leuart) > (LEUART_LF_REF_FREQ/2))){
        /* LEUART configured to a baudrate triggering the use of HFCLK, so prevent HFCLK from getting turned off */
        sleep_manager_lock_deep_sleep();
    }
#else
    /* HFCLK unavailable in deepsleep */
    sleep_manager_lock_deep_sleep();
#endif
}

/** Abort the ongoing RX transaction It disables the enabled interrupt for RX and
 *  flush RX hardware buffer if RX FIFO is used
 *
 * @param obj The serial object
 */
void serial_rx_abort_asynch(serial_t *obj)
{
    serial_rx_abort_asynch_intern(obj, 0);
}

static void serial_rx_abort_asynch_intern(serial_t *obj, int unblock_sleep)
{
    /* Stop receiver */
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        obj->serial.periph.leuart->CMD = LEUART_CMD_RXDIS;
        while(obj->serial.periph.leuart->SYNCBUSY & LEUART_SYNCBUSY_CMD);
    } else {
        obj->serial.periph.uart->CMD = USART_CMD_RXDIS;
    }

    /* Clean up */
    switch(obj->serial.dmaOptionsRX.dmaUsageState) {
        case DMA_USAGE_ALLOCATED:
            /* stop DMA transfer */
#ifndef LDMA_PRESENT
            DMA_ChannelEnable(obj->serial.dmaOptionsRX.dmaChannel, false);
#else
            LDMA_StopTransfer(obj->serial.dmaOptionsRX.dmaChannel);
#endif
            break;
        case DMA_USAGE_TEMPORARY_ALLOCATED:
            /* stop DMA transfer and release channel */
#ifndef LDMA_PRESENT
            DMA_ChannelEnable(obj->serial.dmaOptionsRX.dmaChannel, false);
#else
            LDMA_StopTransfer(obj->serial.dmaOptionsRX.dmaChannel);
#endif
            dma_channel_free(obj->serial.dmaOptionsRX.dmaChannel);
            obj->serial.dmaOptionsRX.dmaChannel = -1;
            obj->serial.dmaOptionsRX.dmaUsageState = DMA_USAGE_OPPORTUNISTIC;
            break;
        default:
            /* stop interrupting */
            if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
                LEUART_IntDisable(obj->serial.periph.leuart, LEUART_IEN_RXDATAV | LEUART_IEN_PERR | LEUART_IEN_FERR | LEUART_IEN_RXOF);
            } else {
                USART_IntDisable(obj->serial.periph.uart, USART_IEN_RXDATAV | USART_IEN_PERR | USART_IEN_FERR | USART_IEN_RXOF);
            }
            break;
    }

    /*clear all set interrupts*/
    if(LEUART_REF_VALID(obj->serial.periph.leuart)) {
        LEUART_IntClear(obj->serial.periph.leuart, LEUART_IFC_PERR | LEUART_IFC_FERR | LEUART_IFC_RXOF);
    }else{
        USART_IntClear(obj->serial.periph.uart,  USART_IFC_PERR | USART_IFC_FERR | USART_IFC_RXOF);
    }

    /* Say that we can stop using this emode */
    if( unblock_sleep )
        serial_unblock_sleep(obj);
}

#endif //DEVICE_SERIAL_ASYNCH
#endif //DEVICE_SERIAL
