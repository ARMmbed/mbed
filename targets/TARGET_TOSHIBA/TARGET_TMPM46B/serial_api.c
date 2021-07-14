/* mbed Microcontroller Library
 * (C)Copyright TOSHIBA ELECTRONIC DEVICES & STORAGE CORPORATION 2017 All rights reserved
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
#include <string.h>
#include "serial_api.h"
#include "PeripheralNames.h"
#include "pinmap.h"
#include "tmpm46b_uart.h"
#include "tmpm46b_fuart.h"

#define UART_NUM                      6
#define FUART_INT_BITS                0x07FF

static const PinMap PinMap_UART_TX[] = {
    {PE2, SERIAL_0, PIN_DATA(1, 1)},
    {PE5, SERIAL_1, PIN_DATA(1, 1)},
    {PL2, SERIAL_2, PIN_DATA(5, 1)},
    {PB0, SERIAL_3, PIN_DATA(3, 1)},
    {PF1, SERIAL_4, PIN_DATA(3, 1)},
    {PA6, SERIAL_5, PIN_DATA(2, 1)},
    {NC,  NC,    0}
};

static const PinMap PinMap_UART_RX[] = {
    {PE1, SERIAL_0, PIN_DATA(1, 0)},
    {PE6, SERIAL_1, PIN_DATA(1, 0)},
    {PL1, SERIAL_2, PIN_DATA(5, 0)},
    {PB1, SERIAL_3, PIN_DATA(3, 0)},
    {PF2, SERIAL_4, PIN_DATA(3, 0)},
    {PA5, SERIAL_5, PIN_DATA(2, 0)},
    {NC, NC,    0}
};

static const PinMap PinMap_UART_CTS[] = {
    {PE3, SERIAL_0, PIN_DATA(4, 0)},
    {PE4, SERIAL_1, PIN_DATA(4, 0)},
    {PL3, SERIAL_2, PIN_DATA(6, 0)},
    {PA7, SERIAL_3, PIN_DATA(4, 0)},
    {PF0, SERIAL_4, PIN_DATA(3, 0)},
    {PA7, SERIAL_5, PIN_DATA(2, 0)},
    {NC,  NC,    0}
};

static const PinMap PinMap_UART_RTS[] = {
    {PF3, SERIAL_4, PIN_DATA(3, 1)},
    {PA4, SERIAL_5, PIN_DATA(2, 1)},
    {NC,  NC,    0}
};


static uint32_t serial_irq_ids[UART_NUM] = {0};
static uart_irq_handler irq_handler;
void serial_init_UART_configure(int uartname, serial_t *obj, PinName tx, PinName rx);

int stdio_uart_inited = 0;
serial_t stdio_uart;

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    int is_stdio_uart = 0;

    UARTName uart_tx = (UARTName)pinmap_peripheral(tx, PinMap_UART_TX);
    UARTName uart_rx = (UARTName)pinmap_peripheral(rx, PinMap_UART_RX);
    UARTName uart_name = (UARTName)pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)uart_name != NC);

    obj->index = uart_name;
    // Initialize UART instance
    switch (uart_name) {
        case SERIAL_0:
            obj->UARTx = UART0;
            serial_init_UART_configure(SERIAL_0, obj, tx, rx);
            break;
        case SERIAL_1:
            obj->UARTx = UART1;
            serial_init_UART_configure(SERIAL_1, obj, tx, rx);
            break;
        case SERIAL_2:
            obj->UARTx = UART2;
            serial_init_UART_configure(SERIAL_2, obj, tx, rx);
            break;
        case SERIAL_3:
            obj->UARTx = UART3;
            serial_init_UART_configure(SERIAL_3, obj, tx, rx);
            break;
        case SERIAL_4:
            obj->FUART = FUART0;
            serial_init_UART_configure(SERIAL_4, obj, tx, rx);
            break;
        case SERIAL_5:
            obj->FUART = FUART1;
            serial_init_UART_configure(SERIAL_5, obj, tx, rx);
            break;
        default:
            break;
    }

    is_stdio_uart = (uart_name == STDIO_UART) ? (1) : (0);

    if (is_stdio_uart) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_init_UART_configure(int uartname, serial_t *obj, PinName tx, PinName rx)
{
    if (uartname <= SERIAL_3)    {
        obj->uart_config.BaudRate = 9600U;
        obj->uart_config.DataBits = UART_DATA_BITS_8;
        obj->uart_config.StopBits = UART_STOP_BITS_1;
        obj->uart_config.Parity   = UART_NO_PARITY;
        obj->uart_config.FlowCtrl = UART_NONE_FLOW_CTRL;

        if (tx != NC && rx != NC) {
            obj->uart_config.Mode = UART_ENABLE_RX | UART_ENABLE_TX;
        } else if (tx != NC) {
            obj->uart_config.Mode = UART_ENABLE_TX;
        } else if (rx != NC) {
            obj->uart_config.Mode = UART_ENABLE_RX;
        }

        // Pinout the chosen uart
        pinmap_pinout(tx, PinMap_UART_TX);
        pinmap_pinout(rx, PinMap_UART_RX);

        UART_Enable(obj->UARTx);
        UART_SetIdleMode(obj->UARTx, ENABLE);
        UART_Init(obj->UARTx, &obj->uart_config);
    } else {
        obj->fuart_config.BaudRate = 9600U;
        obj->fuart_config.DataBits = FUART_DATA_BITS_8;
        obj->fuart_config.StopBits = FUART_STOP_BITS_1;
        obj->fuart_config.Parity = FUART_NO_PARITY;
        obj->fuart_config.FlowCtrl = FUART_NONE_FLOW_CTRL;

        if (tx != NC && rx != NC) {
            obj->fuart_config.Mode = FUART_ENABLE_TX | FUART_ENABLE_RX;
        } else if (tx != NC) {
            obj->fuart_config.Mode = FUART_ENABLE_TX;
        } else if (rx != NC) {
            obj->fuart_config.Mode = FUART_ENABLE_RX;
        }

        // pin-out the chosen UART
        pinmap_pinout(tx, PinMap_UART_TX);
        pinmap_pinout(rx, PinMap_UART_RX);

        FUART_Init(obj->FUART, &obj->fuart_config);
        FUART_Enable(obj->FUART);
    }
}

void serial_free(serial_t *obj)
{
    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            // Disable UART
            UART_Disable(obj->UARTx);
            UART_SWReset(obj->UARTx);
            // set information of object to invalid
            obj->uart_config.BaudRate = 0;
            obj->uart_config.DataBits = 0;
            obj->uart_config.StopBits = 0;
            obj->uart_config.Parity = 0;
            obj->uart_config.Mode = 0;
            obj->uart_config.FlowCtrl = 0;
            break;
        case SERIAL_4:
        case SERIAL_5:
            // Disable UART
            FUART_Disable(obj->FUART);
            // set information of object to invalid
            obj->fuart_config.BaudRate = 0;
            obj->fuart_config.DataBits = 0;
            obj->fuart_config.StopBits = 0;
            obj->fuart_config.Parity = 0;
            obj->fuart_config.Mode = 0;
            obj->fuart_config.FlowCtrl = 0;
            break;
        default:
            break;
    }
}

// serial_baud
void serial_baud(serial_t *obj, int baudrate)
{
    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            obj->uart_config.BaudRate = baudrate;
            UART_Init(obj->UARTx,&obj->uart_config);
            break;
        case SERIAL_4:
        case SERIAL_5:
            FUART_Disable(obj->FUART);
            obj->fuart_config.BaudRate = baudrate;
            FUART_Init(obj->FUART,&obj->fuart_config);
            FUART_Enable(obj->FUART);
            break;
        default:
            break;
    }
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
    // 0: 1 stop bits, 1: 2 stop bits
    MBED_ASSERT((stop_bits == 1) || (stop_bits == 2));
    MBED_ASSERT((parity == ParityNone) || (parity == ParityOdd) || (parity == ParityEven) ||
                (parity == ParityForced1) || (parity == ParityForced0));
    // 0: 7 data bits ... 2: 9 data bits
    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            MBED_ASSERT((data_bits > 6) && (data_bits < 10)); // 0: 7 data bits ... 2: 9 data bits
            obj->uart_config.DataBits = ((data_bits == 7) ? UART_DATA_BITS_7:
                                         ((data_bits == 8) ? UART_DATA_BITS_8 : UART_DATA_BITS_9));
            obj->uart_config.StopBits = ((stop_bits == 1) ? UART_STOP_BITS_1 : UART_STOP_BITS_2);
            obj->uart_config.Parity = ((parity == ParityOdd) ? UART_ODD_PARITY :
                                       ((parity == ParityEven) ? UART_EVEN_PARITY : UART_NO_PARITY));
            UART_Init(obj->UARTx,&obj->uart_config);
            break;
        case SERIAL_4:
        case SERIAL_5:
            FUART_Disable(obj->FUART);
            MBED_ASSERT((data_bits > 6) && (data_bits < 9)); // 0: 5 data bits ... 2: 8 data bits
            obj->fuart_config.DataBits = ((data_bits == 7) ? FUART_DATA_BITS_7 : FUART_DATA_BITS_8);
            obj->fuart_config.StopBits = ((stop_bits == 1) ? FUART_STOP_BITS_1 : FUART_STOP_BITS_2);
            obj->fuart_config.Parity = ((parity == ParityOdd) ? FUART_ODD_PARITY :
                                        ((parity == ParityEven) ? FUART_EVEN_PARITY :
                                         ((parity == ParityForced1) ? FUART_1_PARITY :
                                          ((parity == ParityForced0) ? FUART_0_PARITY : FUART_NO_PARITY))));
            FUART_Init(obj->FUART,&obj->fuart_config);
            FUART_Enable(obj->FUART);
            break;
        default:
            break;
    }
}

void INTTX0_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_0], TxIrq);
}

void INTRX0_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_0], RxIrq);
}

void INTTX1_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_1], TxIrq);
}

void INTRX1_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_1], RxIrq);
}

void INTTX2_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_2], TxIrq);
}

void INTRX2_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_2], RxIrq);
}

void INTTX3_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_3], TxIrq);
}

void INTRX3_IRQHandler(void)
{
    irq_handler(serial_irq_ids[SERIAL_3], RxIrq);
}

void INTUART0_IRQHandler(void)
{
    FUART_INTStatus fuart_int;
    fuart_int = FUART_GetMaskedINTStatus(FUART0);
    if (fuart_int.Bit.TxFIFO == 1) {
        irq_handler(serial_irq_ids[SERIAL_4], TxIrq);
    }
    if (fuart_int.Bit.RxFIFO == 1) {
        irq_handler(serial_irq_ids[SERIAL_4], RxIrq);
    }
}

void INTUART1_IRQHandler(void)
{
    FUART_INTStatus fuart_int;
    fuart_int = FUART_GetMaskedINTStatus(FUART1);
    if (fuart_int.Bit.TxFIFO == 1) {
        irq_handler(serial_irq_ids[SERIAL_5], TxIrq);
    }
    if (fuart_int.Bit.RxFIFO == 1) {
        irq_handler(serial_irq_ids[SERIAL_5], RxIrq);
    }
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    serial_irq_ids[obj->index] = id;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    IRQn_Type irq_n = (IRQn_Type)0;
    uint32_t int_mask = 0;

    switch (obj->index) {
        case SERIAL_0:
            if (irq == RxIrq) {
                irq_n = INTRX0_IRQn;
            } else {
                irq_n = INTTX0_IRQn;
            }
            break;
        case SERIAL_1:
            if (irq == RxIrq) {
                irq_n = INTRX1_IRQn;
            } else {
                irq_n = INTTX1_IRQn;
            }
            break;
        case SERIAL_2:
            if (irq == RxIrq) {
                irq_n = INTRX2_IRQn;
            } else {
                irq_n = INTTX2_IRQn;
            }
            break;
        case SERIAL_3:
            if (irq == RxIrq) {
                irq_n = INTRX3_IRQn;
            } else {
                irq_n = INTTX3_IRQn;
            }
            break;
        case SERIAL_4:
            irq_n = INTUART0_IRQn;
            break;
        case SERIAL_5:
            irq_n = INTUART1_IRQn;
            break;
        default:
            break;
    }

    if ((obj->index == SERIAL_4) || (obj->index == SERIAL_5)) {
        // Get interrupt mask
        int_mask = obj->FUART->IMSC & FUART_INT_BITS;

        // Set interrupt mask
        if (irq == RxIrq) {
            int_mask |= FUART_RX_FIFO_INT_MASK;
        } else {
            int_mask |= FUART_TX_FIFO_INT_MASK;
        }
        FUART_SetINTMask(obj->FUART, int_mask);
    }

    if (enable) {
        NVIC_EnableIRQ(irq_n);
    } else {
        NVIC_DisableIRQ(irq_n);
    }
}

int serial_getc(serial_t *obj)
{
    int data = 0;

    // Wait until Rx buffer is full
    while (!serial_readable(obj)) {
        // Do nothing
    }
    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            data = (int) UART_GetRxData(obj->UARTx);
            break;
        case SERIAL_4:
        case SERIAL_5:
            data = (int) FUART_GetRxData(obj->FUART);
            break;
        default:
            break;
    }
    return data;
}

void serial_putc(serial_t *obj, int c)
{
    // Wait until Tx buffer is empty
    while (!serial_writable(obj)) {
        // Do nothing
    }

    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            UART_SetTxData(obj->UARTx,(uint32_t)c);
            break;
        case SERIAL_4:
        case SERIAL_5:
            FUART_SetTxData(obj->FUART,(uint32_t)c);
            break;
        default:
            break;
    }
}

int serial_readable(serial_t *obj)
{
    int ret = 0;

    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            if (UART_GetBufState(obj->UARTx, UART_RX) == DONE) {
                ret = 1;
            }
            break;
        case SERIAL_4:
        case SERIAL_5:
            if (FUART_GetStorageStatus(obj->FUART, FUART_RX) == FUART_STORAGE_FULL) {
                ret = 1;
            }
            break;
        default:
            break;
    }
    return ret;
}

int serial_writable(serial_t *obj)
{
    int ret = 0;

    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            if (UART_GetBufState(obj->UARTx, UART_TX) == DONE) {
                ret = 1;
            }
            break;
        case SERIAL_4:
        case SERIAL_5:
            if (FUART_GetStorageStatus(obj->FUART, FUART_TX) == FUART_STORAGE_EMPTY) {
                ret = 1;
            }
            break;
        default:
            break;
    }
    return ret;
}

int serial_tx_empty(serial_t *obj)
{
    int ret = 0;

    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            if (UART_GetBufState(obj->UARTx, UART_TX) == DONE) {
                ret = 1;
            }
            break;
        case SERIAL_4:
        case SERIAL_5:
            if (FUART_GetStorageStatus(obj->FUART, FUART_TX) == FUART_STORAGE_EMPTY) {
                ret = 1;
            }
            break;
        default:
            break;
    }
    return ret;
}

void serial_clear(serial_t *obj)
{
    switch (obj->index) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            UART_GetRxData(obj->UARTx);
            break;
        case SERIAL_4:
        case SERIAL_5:
            FUART_GetRxData(obj->FUART);
            break;
        default:
            break;
    }
}

void serial_pinout_tx(PinName tx)
{
    // pin out the chosen UART
    pinmap_pinout(tx, PinMap_UART_TX);
}

// Set flow control, Just support CTS
void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    UARTName uart_rts = (UARTName)pinmap_peripheral(rxflow, PinMap_UART_RTS);
    UARTName uart_cts;

    // SERIAL_5 & SERIAL_3 have same CTS pin (PA7), only function register is different (4 & 2).
    // pinmap_peripheral() will always return first match from the map.
    // But, if SERIAL_5 is used, then pinmap_peripheral() should return SERIAL_5 (function register 2 to be set).
    if (obj->index == SERIAL_5) {
        uart_cts = (UARTName)pinmap_peripheral(txflow, &PinMap_UART_CTS[5]);
    } else {
        uart_cts = (UARTName)pinmap_peripheral(txflow, PinMap_UART_CTS);
    }
    UARTName uart_name = (UARTName)pinmap_merge(uart_cts, uart_rts);

    switch (uart_name) {
        case SERIAL_0:
        case SERIAL_1:
        case SERIAL_2:
        case SERIAL_3:
            if (FlowControlCTS == type) {
                MBED_ASSERT(uart_cts != (UARTName) NC);

                // Enable the pin for CTS function
                pinmap_pinout(txflow, PinMap_UART_CTS);

                // Support CTS hardware control flow only
                obj->UARTx->MOD0 |= 1 << 6;
            } else {
                // Disable hardware flow control
                obj->UARTx->MOD0 &= !(1 << 6);
            }
            break;
        case SERIAL_4:
        case SERIAL_5:
            FUART_Disable(obj->FUART);
            if (type == FlowControlCTS) {
                MBED_ASSERT(uart_cts != (UARTName) NC);

                // Enable CTS hardware flow control
                obj->FUART->CR |= FUART_CTS_FLOW_CTRL;

                // Enable the pin for CTS and RTS function
                if (uart_name == SERIAL_5) {
                    pinmap_pinout(txflow, &PinMap_UART_CTS[5]);
                } else {
                    pinmap_pinout(txflow, PinMap_UART_CTS);
                }
            } else if (type == FlowControlRTS) {
                MBED_ASSERT(uart_rts != (UARTName) NC);

                // Enable RTS hardware flow control
                obj->FUART->CR |= FUART_RTS_FLOW_CTRL;

                // Enable the pin for RTS function
                pinmap_pinout(rxflow, PinMap_UART_RTS);
            } else if (type == FlowControlRTSCTS) {
                MBED_ASSERT(uart_name != (UARTName) NC);

                // Enable CTS and RTS hardware flow control
                obj->FUART->CR |= FUART_CTS_FLOW_CTRL | FUART_RTS_FLOW_CTRL;

                // Enable the pin for CTS and RTS function
                if (uart_name == SERIAL_5) {
                    pinmap_pinout(txflow, &PinMap_UART_CTS[5]);
                } else {
                    pinmap_pinout(txflow, PinMap_UART_CTS);
                }
                pinmap_pinout(rxflow, PinMap_UART_RTS);
            } else {
                // Disable CTS and RTS hardware flow control
                obj->FUART->CR &= (uint32_t) 0xFFFF0FFF;
            }
            FUART_Enable(obj->FUART);
            break;
        default:
            break;
    }
}

// Pause transmission
void serial_break_set(serial_t *obj)
{
    if (obj->index == SERIAL_4 || obj->index == SERIAL_5) {
        FUART_SetSendBreak(obj->FUART, ENABLE);
    }
}

// Switch to normal transmission
void serial_break_clear(serial_t *obj)
{
    if (obj->index == SERIAL_4 || obj->index == SERIAL_5) {
        FUART_SetSendBreak(obj->FUART, DISABLE);
    }
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
