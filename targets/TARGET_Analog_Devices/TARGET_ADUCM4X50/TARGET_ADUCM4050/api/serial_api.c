/*******************************************************************************
 * Copyright (c) 2010-2017 Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   - Modified versions of the software must be conspicuously marked as such.
 *   - This software is licensed solely and exclusively for use with processors
 *     manufactured by or for Analog Devices, Inc.
 *   - This software may not be combined or merged with other code in any manner
 *     that would cause the software to become subject to terms and conditions
 *     which differ from those listed here.
 *   - Neither the name of Analog Devices, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *   - The use of this software may or may not infringe the patent rights of one
 *     or more patent holders.  This license does not release you from the
 *     requirement that you obtain separate licenses from these patent holders
 *     to use this software.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-
 * INFRINGEMENT, TITLE, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ANALOG DEVICES, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, PUNITIVE OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, DAMAGES ARISING OUT OF
 * CLAIMS OF INTELLECTUAL PROPERTY RIGHTS INFRINGEMENT; PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include "serial_api.h"

#if DEVICE_SERIAL

// math.h required for floating point operations for baud rate calculation
#include <math.h>
#include "mbed_assert.h"

#include <string.h>

#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "drivers/uart/adi_uart.h"
#define ADI_UART_MEMORY_SIZE   (ADI_UART_BIDIR_MEMORY_SIZE)
#define ADI_UART_NUM_DEVICES   2

static ADI_UART_HANDLE hDevice[ADI_UART_NUM_DEVICES];
static uint32_t UartDeviceMem[ADI_UART_NUM_DEVICES][(ADI_UART_MEMORY_SIZE + 3)/4];
static uint32_t serial_irq_ids[ADI_UART_NUM_DEVICES] = {0};
static uart_irq_handler irq_handler = NULL;
int stdio_uart_inited = 0;
serial_t stdio_uart;
static int rxbuffer[2];
static int txbuffer[2];

static void uart_callback(void *pCBParam, uint32_t Event, void *pArg)
{
    MBED_ASSERT(irq_handler);
    serial_t *obj = pCBParam;
    if (Event == ADI_UART_EVENT_TX_BUFFER_PROCESSED)
        irq_handler(serial_irq_ids[obj->index], TxIrq);
    else if (Event == ADI_UART_EVENT_RX_BUFFER_PROCESSED)
        irq_handler(serial_irq_ids[obj->index], RxIrq);
}


void serial_free(serial_t *obj)
{
    adi_uart_Close(hDevice[obj->index]);
}

void serial_baud(serial_t *obj, int baudrate)
{
    uint32_t uartdivc,uartdivm,uartdivn,uartosr;

    // figures based on PCLK of 26MHz
    switch (baudrate) {
        case    9600:
            uartdivc= 28;
            uartdivm= 3;
            uartdivn= 46;
            uartosr= 3;
            break;
        case   19200:
            uartdivc= 14;
            uartdivm= 3;
            uartdivn= 46;
            uartosr= 3;
            break;
        case   38400:
            uartdivc= 07;
            uartdivm= 3;
            uartdivn= 46;
            uartosr= 3;
            break;
        case   57600:
            uartdivc= 14;
            uartdivm= 1;
            uartdivn= 15;
            uartosr= 3;
            break;
        case  115200:
            uartdivc= 03;
            uartdivm= 2;
            uartdivn= 719;
            uartosr= 3;
            break;
        case  230400:
            uartdivc= 03;
            uartdivm= 1;
            uartdivn= 359;
            uartosr= 3;
            break;
        default:    // default of 9600kbps
            uartdivc= 28;
            uartdivm= 3;
            uartdivn= 46;
            uartosr= 3;
            break;
    }

    adi_uart_ConfigBaudRate(hDevice[obj->index],uartdivc,uartdivm,uartdivn,uartosr);
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    ADI_UART_PARITY convertedparity   = ADI_UART_NO_PARITY;
    ADI_UART_STOPBITS convertedstopbits = ADI_UART_ONE_STOPBIT;

    if (stop_bits) {
        convertedstopbits = ADI_UART_ONE_AND_HALF_TWO_STOPBITS;
    }

    if (parity == ParityOdd) {
        convertedparity = ADI_UART_ODD_PARITY;
    } else if (parity == ParityEven) {
        convertedparity = ADI_UART_EVEN_PARITY;
    } else if (parity == ParityForced1) {
        convertedparity = ADI_UART_ODD_PARITY_STICKY;
    } else if (parity == ParityForced0) {
        convertedparity = ADI_UART_EVEN_PARITY_STICKY;
    }

    adi_uart_SetConfiguration(hDevice[obj->index], convertedparity, convertedstopbits,
                              (ADI_UART_WORDLEN)(data_bits - 5));
}

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    uint32_t uart_tx = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t uart_rx = pinmap_peripheral(rx, PinMap_UART_RX);
    obj->index = pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)obj->index != NC);
    adi_uart_Open(obj->index,ADI_UART_DIR_BIDIRECTION,UartDeviceMem[obj->index],ADI_UART_MEMORY_SIZE,&hDevice[obj->index]);
    serial_baud(obj, 9600);
    serial_format(obj, 8, ParityNone, 1);
    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);
    if (tx != NC) {
        pin_mode(tx, PullUp);
    }
    if (rx != NC) {
        pin_mode(rx, PullUp);
    }
    if (obj->index == STDIO_UART) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

int serial_readable(serial_t *obj)
{
    bool bAvailable = false;
    adi_uart_IsRxBufferAvailable(hDevice[obj->index], &bAvailable);
    return bAvailable;
}

int serial_getc(serial_t *obj)
{
    int c;
    void *pBuff;
    uint32_t pHwError;
    adi_uart_SubmitRxBuffer(hDevice[obj->index], &rxbuffer[obj->index], 1, 1);
    adi_uart_GetRxBuffer(hDevice[obj->index], &pBuff, &pHwError);
    c = (unsigned) rxbuffer[obj->index];
    return (c);
}

int serial_writable(serial_t *obj)
{
    bool bAvailable = false;
    adi_uart_IsTxBufferAvailable(hDevice[obj->index], &bAvailable);
    return bAvailable;
}

int serial_tx_empty(serial_t *obj)
{
    bool bAvailable = false;
    adi_uart_IsTxComplete(hDevice[obj->index], &bAvailable);
    return bAvailable;
}

void serial_putc(serial_t *obj, int c)
{
    void *pBuff;
    uint32_t pHwError;
    txbuffer[obj->index]= (char) c;
    adi_uart_SubmitTxBuffer(hDevice[obj->index],&txbuffer[obj->index], 1, 1);
    adi_uart_GetTxBuffer(hDevice[obj->index], &pBuff, &pHwError);
    return;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    MBED_ASSERT(obj);

    adi_uart_RegisterCallback(hDevice[obj->index], &uart_callback, obj);
    if (enable) {
    } else {
    }
}

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj)
{
    adi_uart_ForceTxBreak(hDevice[obj->index], true);
}

void serial_break_clear(serial_t *obj)
{
    adi_uart_ForceTxBreak(hDevice[obj->index], false);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{

    MBED_ASSERT(obj);

    irq_handler = handler;
    serial_irq_ids[obj->index] = id;
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

#endif
