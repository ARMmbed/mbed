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
// math.h required for floating point operations for baud rate calculation

#if DEVICE_SERIAL
#include "mbed_assert.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "serial_api.h"
#include "cmsis.h"
#include "PeripheralPins.h"
#include "gpio_api.h"
#include "RZ_A2_Init.h"

#include "iodefine.h"
#include "mbed_drv_cfg.h"
#include "mbed_critical.h"

/******************************************************************************
 * INITIALIZATION
 ******************************************************************************/

#define UART_NUM    5
#define IRQ_NUM     3

static void uart0_tx_irq(void);
static void uart0_rx_irq(void);
static void uart0_er_irq(void);
static void uart1_tx_irq(void);
static void uart1_rx_irq(void);
static void uart1_er_irq(void);
static void uart2_tx_irq(void);
static void uart2_rx_irq(void);
static void uart2_er_irq(void);
static void uart3_tx_irq(void);
static void uart3_rx_irq(void);
static void uart3_er_irq(void);
static void uart4_tx_irq(void);
static void uart4_rx_irq(void);
static void uart4_er_irq(void);

static uint8_t serial_available_buffer(serial_t *obj);
static void serial_irq_err_set(serial_t *obj, uint32_t enable);

static volatile struct st_scifa *SCIFA[UART_NUM] = {
    &SCIFA0, &SCIFA1, &SCIFA2, &SCIFA3, &SCIFA4
};

int stdio_uart_inited = 0;
serial_t stdio_uart;
static uart_irq_handler irq_handler;

struct serial_global_data_s {
    uint32_t serial_irq_id;
    gpio_t sw_rts, sw_cts;
    serial_t *tranferring_obj, *receiving_obj;
    uint32_t async_tx_callback, async_rx_callback;
    int event, wanted_rx_events;
};

static struct serial_global_data_s uart_data[UART_NUM];

static const IRQn_Type irq_set_tbl[UART_NUM][IRQ_NUM] = {
    {RXI0_IRQn, TXI0_IRQn, ERI0_IRQn},
    {RXI1_IRQn, TXI1_IRQn, ERI1_IRQn},
    {RXI2_IRQn, TXI2_IRQn, ERI2_IRQn},
    {RXI3_IRQn, TXI3_IRQn, ERI3_IRQn},
    {RXI4_IRQn, TXI4_IRQn, ERI4_IRQn},
};

static const IRQHandler hander_set_tbl[UART_NUM][IRQ_NUM] = {
    {uart0_rx_irq, uart0_tx_irq, uart0_er_irq},
    {uart1_rx_irq, uart1_tx_irq, uart1_er_irq},
    {uart2_rx_irq, uart2_tx_irq, uart2_er_irq},
    {uart3_rx_irq, uart3_tx_irq, uart3_er_irq},
    {uart4_rx_irq, uart4_tx_irq, uart4_er_irq},
};


void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    volatile uint8_t dummy_buf;
    int is_stdio_uart = 0;
    // determine the UART to use
    uint32_t uart_tx = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t uart_rx = pinmap_peripheral(rx, PinMap_UART_RX);
    uint32_t ch_no = pinmap_merge(uart_tx, uart_rx);

#if defined(PRINTF_NOT_USE)
    if ((int)ch_no == NC) {
        obj->serial.ch = ch_no;
        return;
    }
#else
    MBED_ASSERT((int)ch_no != NC);
#endif

    // pinout the chosen uart
    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);

    obj->serial.ch = ch_no;
    obj->serial.uart = SCIFA[ch_no];

    /* ==== Module standby clear ==== */
    CPG.STBCR4.BYTE &= ~(0x80 >> ch_no);
    dummy_buf = CPG.STBCR4.BYTE;
    (void)dummy_buf;

    /* ==== SCIF initial setting ==== */
    /* --- Serial control register(SCR) setting --- */
    obj->serial.uart->SCR.WORD = 0x0000;   /* SCIFA transmitting and receiving operations stop */

    /* ---- FIFO control register (FCR) setting ---- */
    if ((int)uart_tx != NC) {      /* Use transmit */
        /* Transmit FIFO reset */
        obj->serial.uart->FCR.BIT.TFRST = 0x01;
    }

    if ((int)uart_rx != NC) {      /* Use receive */
        /* Receive FIFO data register reset */
        obj->serial.uart->FCR.BIT.RFRST = 0x1;
    }

    /* ---- Serial status register (FSR) setting ---- */
    obj->serial.uart->FSR.BIT.DR  = 0x0;  /* ER bit clear  */
    obj->serial.uart->FSR.BIT.BRK = 0x0;  /* BRK bit clear */
    obj->serial.uart->FSR.BIT.ER  = 0x0;  /* ER bit clear  */

    /* ---- Line status register (LSR) setting ---- */
    obj->serial.uart->LSR.BIT.ORER = 0x0; /* ORER bit clear */

    /* ---- Serial control register (SCR) setting ---- */
    obj->serial.uart->SCR.BIT.CKE = 0x0;  /* B'00 : Internal CLK */

    serial_baud(obj, 9600);
    serial_format(obj, 8, ParityNone, 1);

    /* ---- Serial extension mode register (SEMR) setting ----
    b7 BGDM - Baud rate generator double-speed mode  : Normal mode
    b0 ABCS - Base clock select in asynchronous mode : Base clock is 16 times the bit rate */

    obj->serial.uart->SEMR.BIT.ABCS0 = 0x0;    /* Base clock select in asynchronous mode : Base clock is 16 times the bit rate */
    obj->serial.uart->SEMR.BIT.NFEN  = 0x1;    /*  Noise Filter : Enable */
    obj->serial.uart->SEMR.BIT.DIR   = 0x0;    /* LSB first */
    obj->serial.uart->SEMR.BIT.MDDRS = 0x0;    /* Select BRR register */
    obj->serial.uart->SEMR.BIT.BRME  = 0x0;    /* Bit Modulation Enable : Disable */
    obj->serial.uart->SEMR.BIT.BGDM  = 0x0;    /* Baud rate generator double-speed mode  : Normal mode*/

    obj->serial.uart->FCR.BIT.LOOP   = 0x0;    /* Loop-back test                    : Disabled      */
    if ((int)uart_rx != NC) {
        obj->serial.uart->FCR.BIT.RFRST = 0x0; /* Receive FIFO data register reset  : Disabled      */
        obj->serial.uart->SCR.BIT.TE    = 0x1; /* transmitting operations is enabled */
    } else {
        obj->serial.uart->FCR.BIT.RFRST = 0x1; /* Receive FIFO data register reset  : Enable        */
    }
    if ((int)uart_tx != NC) {      /* Use transmit */
        obj->serial.uart->FCR.BIT.TFRST = 0x0; /* Transmit FIFO data register reset : Disabled      */
        obj->serial.uart->SCR.BIT.RE    = 0x1; /* receiving operations is enabled */
    } else {
        obj->serial.uart->FCR.BIT.TFRST = 0x1; /* Transmit FIFO data register reset : Enable        */
    }
    obj->serial.uart->FCR.BIT.MCE   = 0x0;     /* Modem control enable              : Disabled      */
    obj->serial.uart->FCR.BIT.TTRG  = 0x3;     /* Transmit FIFO data trigger        : 0-data        */
    obj->serial.uart->FCR.BIT.RTRG  = 0x0;     /* Receive FIFO data trigger         : 1-data        */
    obj->serial.uart->FCR.BIT.RSTRG = 0x0;     /* RTS output active trigger         : Initial value */

    /* ---- Serial port register (SCR, SPTR) setting ---- */
    /* b1 SPB2IO - Serial port break output : Enabled
       b0 SPB2DT - Serial port break data   : High-level */

    obj->serial.uart->SPTR.WORD |= 0x3;   /* b1 SPB2IO - Serial port break output : Enabled    */
    /* b0 SPB2DT - Serial port break data   : High-level */

    if ((int)uart_rx != NC) {
        obj->serial.uart->SCR.BIT.TIE   = 0x1;
        obj->serial.uart->SCR.BIT.TE    = 0x1; /* transmitting operations is enabled */
    }
    if ((int)uart_tx != NC) {      /* Use transmit */
        obj->serial.uart->SCR.BIT.RIE   = 0x1;
        obj->serial.uart->SCR.BIT.RE    = 0x1; /* receiving operations is enabled */
    }

    is_stdio_uart = (ch_no == STDIO_UART) ? (1) : (0);

    if (is_stdio_uart) {
        stdio_uart_inited = 1;
        memcpy(&stdio_uart, obj, sizeof(serial_t));
    }
}

void serial_free(serial_t *obj)
{

}

// serial_baud
// set the baud rate, taking in to account the current SystemFrequency
void serial_baud(serial_t *obj, int baudrate)
{
    uint32_t idx;
    uint32_t brr;
    uint32_t mddr;
    uint32_t wk_data = 8;
    float    wk_error;

#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return;
    }
#endif

    if (baudrate <= 0) {
        return;
    }

    for (idx = 0; idx <= 4; idx++) {
        /* Calculate Bit Rate */
        brr = CM0_RENESAS_RZ_A2_P1_CLK / (wk_data * baudrate);
        if (brr > 0) {
            /* The maximum baud rate is (CM0_RENESAS_RZ_A2_P1_CLK / 8) */
            /* If the baud rate is more, brr is set to 0 */
            brr -= 1;
        }

        /* When idx is 4 (CKS=3), it allows up to the limit value which can be adjusted with MDDR */
        if ((brr <= 255) || (idx == 4)) {
            if (brr > 255) {
                brr = 255;
            }

            /* Calculate Modulation Duty */
            wk_error = (float)CM0_RENESAS_RZ_A2_P1_CLK / (float)(wk_data * baudrate * (brr + 1));
            mddr = (uint32_t)((256.0f / wk_error) + 0.5f);
            if (mddr < 128) {
                mddr = 128;    /* MDDR >= 128 (Executed only when idx is 4) */
            }

            /* idx= 0: BGDM=1 ABCS0=1 CKS=0 */
            /*      1: BGDM=0 ABCS0=0 CKS=0 */
            /*      2: BGDM=0 ABCS0=0 CKS=1 */
            /*      3: BGDM=1 ABCS0=1 CKS=3 (Do not use CKS=2) */
            /*      4: BGDM=0 ABCS0=0 CKS=3 */
            if ((idx == 0) || (idx == 3)) {
                obj->serial.uart->SEMR.BIT.BGDM  = 0x1;     /* Baud rate generator double-speed mode */
                obj->serial.uart->SEMR.BIT.ABCS0 = 0x1;     /* 8 times the transfer rate as the base clock */
                obj->serial.uart->SMR.BIT.CKS    = idx;     /* Clock Select  0(1/1), 1(1/4), 2(1/16), 3(1/64) */
            } else {
                obj->serial.uart->SEMR.BIT.BGDM  = 0x0;     /* Baud rate generator normal mode */
                obj->serial.uart->SEMR.BIT.ABCS0 = 0x0;     /* 16 times the transfer rate as the base clock */
                obj->serial.uart->SMR.BIT.CKS    = idx - 1; /* Clock Select  0(1/1), 1(1/4), 2(1/16), 3(1/64) */
            }
            obj->serial.uart->SEMR.BIT.MDDRS = 0x0;                   /* Select BRR register */
            obj->serial.uart->BRR_MDDR.BRR.BYTE = (uint8_t)brr;       /* Bit Rate */
            obj->serial.uart->SEMR.BIT.BRME = 0x0;                    /* Bit rate modulation is disabled */
            if (mddr <= 255) {
                obj->serial.uart->SEMR.BIT.MDDRS = 0x1;               /* Select MDDR register */
                obj->serial.uart->BRR_MDDR.MDDR.BYTE = (uint8_t)mddr; /* Modulation Duty */
                obj->serial.uart->SEMR.BIT.BRME = 0x1;                /* Bit rate modulation is enabled */
            }
            break;
        }
        wk_data = (wk_data << 2);
    }
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits)
{
#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return;
    }
#endif

    MBED_ASSERT((stop_bits == 1) || (stop_bits == 2));
    MBED_ASSERT((data_bits == 8) || (data_bits == 7));
    MBED_ASSERT((parity == ParityNone) || (parity == ParityOdd) || (parity == ParityEven) ||
                (parity == ParityForced1) || (parity == ParityForced0));

    if (stop_bits == 1) {
        obj->serial.uart->SMR.BIT.STOP = 0x0;    /* Stop bit length    : 1 stop bit  */
    } else {
        obj->serial.uart->SMR.BIT.STOP = 0x1;    /* Stop bit length    : 2 stop bits */
    }

    switch (parity) {
        case ParityNone:
            obj->serial.uart->SMR.BIT.PE = 0x0;  /* Parity enable : Add and check are disabled */
            obj->serial.uart->SMR.BIT.PM = 0x0;  /* Parity mode : even */
            break;
        case ParityOdd:
            obj->serial.uart->SMR.BIT.PE = 0x1;  /* Parity enable : Add and check are enabled  */
            obj->serial.uart->SMR.BIT.PM = 0x1;  /* Parity mode : odd */
            break;
        case ParityEven:
            obj->serial.uart->SMR.BIT.PE = 0x1;  /* Parity enable : Add and check are enabled  */
            obj->serial.uart->SMR.BIT.PM = 0x0;  /* Parity mode : even */
            break;
        case ParityForced1:
        case ParityForced0:
        default:
            obj->serial.uart->SMR.BIT.PE = 0x0;  /* Parity enable : Add and check are disabled */
            obj->serial.uart->SMR.BIT.PM = 0x0;  /* Parity mode : even */
            break;
    }

    if (data_bits == 8) {
        obj->serial.uart->SMR.BIT.CHR = 0x0;    /* Character length   : 8-bit data  */
    } else {
        obj->serial.uart->SMR.BIT.CHR = 0x1;    /* Character length   : 7-bit data  */
    }
    obj->serial.uart->SMR.BIT.CM = 0x0;         /* Communication mode : Asynchronous mode  */
}

/******************************************************************************
 * INTERRUPTS HANDLING
 ******************************************************************************/

static void uart_tx_irq(uint32_t ch)
{
    serial_t *obj = uart_data[ch].tranferring_obj;
    if (obj) {
        int i = obj->tx_buff.length - obj->tx_buff.pos;
        if (0 < i) {
            if (serial_available_buffer(obj) < i) {
                i = serial_available_buffer(obj);
            }
            do {
                uint8_t c = *(uint8_t *)obj->tx_buff.buffer;
                obj->tx_buff.buffer = (uint8_t *)obj->tx_buff.buffer + 1;
                ++obj->tx_buff.pos;
                obj->serial.uart->FTDR.BYTE = c;
                obj->serial.uart->FSR.WORD &= ~0x0060u;
            } while (--i);
        } else {
            uart_data[ch].tranferring_obj = NULL;
            uart_data[ch].event = SERIAL_EVENT_TX_COMPLETE;
            ((void (*)())uart_data[ch].async_tx_callback)();
        }
    }

    irq_handler(uart_data[ch].serial_irq_id, TxIrq);
}

static void uart_rx_irq(uint32_t ch)
{
    serial_t *obj = uart_data[ch].receiving_obj;
    if (obj) {
        if (obj->serial.uart->LSR.BIT.ORER == 1) {
            obj->serial.uart->LSR.BIT.ORER = 0;
            if (uart_data[ch].wanted_rx_events & SERIAL_EVENT_RX_OVERRUN_ERROR) {
                serial_rx_abort_asynch(obj);
                uart_data[ch].event = SERIAL_EVENT_RX_OVERRUN_ERROR;
                ((void (*)())uart_data[ch].async_rx_callback)();
            }
            return;
        }
        int c = serial_getc(obj);
        if (c != -1) {
            ((uint8_t *)obj->rx_buff.buffer)[obj->rx_buff.pos] = c;
            ++obj->rx_buff.pos;
            if (c == obj->char_match && ! obj->char_found) {
                obj->char_found = 1;
                if (obj->rx_buff.pos == obj->rx_buff.length) {
                    if (uart_data[ch].wanted_rx_events & SERIAL_EVENT_RX_COMPLETE) {
                        uart_data[ch].event = SERIAL_EVENT_RX_COMPLETE;
                    }
                }
                if (uart_data[ch].wanted_rx_events & SERIAL_EVENT_RX_CHARACTER_MATCH) {
                    uart_data[ch].event |= SERIAL_EVENT_RX_CHARACTER_MATCH;
                }
                if (uart_data[ch].event) {
                    uart_data[ch].receiving_obj = NULL;
                    ((void (*)())uart_data[ch].async_rx_callback)();
                }
            } else if (obj->rx_buff.pos == obj->rx_buff.length) {
                uart_data[ch].receiving_obj = NULL;
                if (uart_data[ch].wanted_rx_events & SERIAL_EVENT_RX_COMPLETE) {
                    uart_data[ch].event = SERIAL_EVENT_RX_COMPLETE;
                    ((void (*)())uart_data[ch].async_rx_callback)();
                }
            }
        } else {
            serial_rx_abort_asynch(obj);
            if (uart_data[ch].wanted_rx_events & (SERIAL_EVENT_RX_PARITY_ERROR | SERIAL_EVENT_RX_FRAMING_ERROR)) {
                uart_data[ch].event = SERIAL_EVENT_RX_PARITY_ERROR | SERIAL_EVENT_RX_FRAMING_ERROR;
                if (obj->serial.uart->FSR.BIT.PER == 1) {
                    uart_data[ch].event = SERIAL_EVENT_RX_PARITY_ERROR;
                } else if (obj->serial.uart->FSR.BIT.FER == 1) {
                    uart_data[ch].event = SERIAL_EVENT_RX_FRAMING_ERROR;
                }
                ((void (*)())uart_data[ch].async_rx_callback)();
            }
            return;
        }
    }

    irq_handler(uart_data[ch].serial_irq_id, RxIrq);
}

static void uart_err_irq(uint32_t ch)
{
    serial_t *obj = uart_data[ch].receiving_obj;
    if (obj) {
        serial_irq_err_set(obj, 0);
        if (uart_data[ch].wanted_rx_events & (SERIAL_EVENT_RX_PARITY_ERROR | SERIAL_EVENT_RX_FRAMING_ERROR)) {
            uart_data[ch].event = SERIAL_EVENT_RX_PARITY_ERROR | SERIAL_EVENT_RX_FRAMING_ERROR;
            if (obj->serial.uart->FSR.BIT.PER == 1) {
                uart_data[ch].event = SERIAL_EVENT_RX_PARITY_ERROR;
            } else if (obj->serial.uart->FSR.BIT.FER == 1) {
                uart_data[ch].event = SERIAL_EVENT_RX_FRAMING_ERROR;
            }
            ((void (*)())uart_data[ch].async_rx_callback)();
        }
        serial_rx_abort_asynch(obj);

        core_util_critical_section_enter();
        if (obj->serial.uart->FSR.WORD & 0x009Cu) {
            obj->serial.uart->FSR.WORD &= ~0x009Cu;
        }
        if (obj->serial.uart->LSR.BIT.ORER == 1) {
            obj->serial.uart->LSR.BIT.ORER = 0;
        }
        core_util_critical_section_exit();
    }
}

static void uart0_tx_irq(void)
{
    uart_tx_irq(0);
}
static void uart0_rx_irq(void)
{
    uart_rx_irq(0);
}
static void uart0_er_irq(void)
{
    uart_err_irq(0);
}

static void uart1_tx_irq(void)
{
    uart_tx_irq(1);
}
static void uart1_rx_irq(void)
{
    uart_rx_irq(1);
}
static void uart1_er_irq(void)
{
    uart_err_irq(1);
}

static void uart2_tx_irq(void)
{
    uart_tx_irq(2);
}
static void uart2_rx_irq(void)
{
    uart_rx_irq(2);
}
static void uart2_er_irq(void)
{
    uart_err_irq(2);
}

static void uart3_tx_irq(void)
{
    uart_tx_irq(3);
}
static void uart3_rx_irq(void)
{
    uart_rx_irq(3);
}
static void uart3_er_irq(void)
{
    uart_err_irq(3);
}

static void uart4_tx_irq(void)
{
    uart_tx_irq(4);
}
static void uart4_rx_irq(void)
{
    uart_rx_irq(4);
}
static void uart4_er_irq(void)
{
    uart_err_irq(4);
}

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    irq_handler = handler;
    uart_data[obj->serial.ch].serial_irq_id = id;
}

static void serial_irq_set_irq(IRQn_Type IRQn, IRQHandler handler, uint32_t enable)
{
    if (enable) {
        InterruptHandlerRegister(IRQn, (void (*)(uint32_t))handler);
        GIC_SetPriority(IRQn, 5);
        GIC_EnableIRQ(IRQn);
    } else {
        GIC_DisableIRQ(IRQn);
    }
}

static void serial_irq_err_set(serial_t *obj, uint32_t enable)
{
    serial_irq_set_irq(irq_set_tbl[obj->serial.ch][2], hander_set_tbl[obj->serial.ch][2], enable);
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    IRQn_Type IRQn;
    IRQHandler handler;

    IRQn = irq_set_tbl[obj->serial.ch][irq];
    handler = hander_set_tbl[obj->serial.ch][irq];

    if (obj->serial.ch < UART_NUM) {
        serial_irq_set_irq(IRQn, handler, enable);
    }
}

/******************************************************************************
 * READ/WRITE
 ******************************************************************************/
int serial_getc(serial_t *obj)
{
    int data;

#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return 0;
    }
#endif
    /* Confirming receive error(ER,BRK,FER,PER,ORER) */
    if (((obj->serial.uart->FSR.WORD & 0x009Cu) != 0)  ||
            (obj->serial.uart->LSR.BIT.ORER == 1)) {
        /* ---- Detect receive error ---- */
        /* Disable reception             */
        /* Reset receiving FIFO          */
        /* Clearing FIFO reception reset */
        /* Error bit clear               */
        /* Enable reception              */
        obj->serial.uart->SCR.BIT.RE = 0;
        obj->serial.uart->FCR.BIT.RFRST = 1;
        obj->serial.uart->FCR.BIT.RFRST = 0;
        obj->serial.uart->FSR.WORD &= ~0x009Cu;
        obj->serial.uart->LSR.BIT.ORER = 0;
        obj->serial.uart->SCR.BIT.RE = 1;
        return -1;
    }

    /* Is there receive FIFO data? */
    while (obj->serial.uart->FSR.BIT.RDF == 0) {
        /* Wait */
    }

    /* Read receive data */
    data = (int)((obj->serial.uart->FRDR.BYTE) & 0xFFu);
    /* Clear DR,RDF */
    obj->serial.uart->FSR.BIT.RDF = 0;

    return data;
}

void serial_putc(serial_t *obj, int c)
{
#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return;
    }
#endif

    /* Check if it is possible to transmit (TDFE flag) */
    while (obj->serial.uart->FSR.BIT.TDFE == 0) {
        /* Wait */
    }

    /* Write the receiving data in TDR */
    obj->serial.uart->FTDR.BYTE = (uint8_t)c;

    /* Clear TDFE and TEND flag */
    obj->serial.uart->FSR.WORD &= ~0x0060u;
}

int serial_readable(serial_t *obj)
{
    if (obj->serial.uart->FSR.BIT.RDF == 0) {
        return 0;
    } else {
        return 1;
    }
}

int serial_writable(serial_t *obj)
{
#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return 0;
    }
#endif
    if (obj->serial.uart->FSR.BIT.TDFE == 0) {
        return 0;
    } else {
        return 1;
    }
}

int serial_tx_empty(serial_t *obj)
{
#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return 0;
    }
#endif
    if((obj->serial.uart->FSR.BIT.TDFE != 0) &&  // TDFE = 1 && TEND = 1
       (obj->serial.uart->FSR.BIT.TEND != 0)) {
        return 1;
    } else {
        return 0;
    }
}

void serial_clear(serial_t *obj)
{
#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return;
    }
#endif
    core_util_critical_section_enter();
    obj->serial.uart->FCR.WORD |=  0x0006u;   // TFRST = 1, RFRST = 1
    obj->serial.uart->FCR.WORD &= ~0x0006u;   // TFRST = 0, RFRST = 0
    obj->serial.uart->FSR.WORD &= ~0x0093u;   // ER, BRK, RDF, DR = 0
    core_util_critical_section_exit();
}

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj)
{
    core_util_critical_section_enter();
    obj->serial.uart->SCR.BIT.TE = 0;
    core_util_critical_section_exit();
}

void serial_break_clear(serial_t *obj)
{
#if defined(PRINTF_NOT_USE)
    if ((int)obj->serial.ch == NC) {
        return;
    }
#endif
    core_util_critical_section_enter();
    obj->serial.uart->SCR.BIT.TE = 1;
    core_util_critical_section_exit();
}

#if DEVICE_SERIAL_FC
void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    // determine the UART to use

    if (type == FlowControlRTSCTS) {
        core_util_critical_section_enter();
        obj->serial.uart->FCR.BIT.MCE  = 0x1;     // CTS/RTS enable
        core_util_critical_section_exit();
        pinmap_pinout(rxflow, PinMap_UART_RTS);
        pinmap_pinout(txflow, PinMap_UART_CTS);
    } else {
        core_util_critical_section_enter();
        obj->serial.uart->FCR.BIT.MCE  = 0x0;     // CTS/RTS diable
        core_util_critical_section_exit();
    }
}
#endif

static uint8_t serial_available_buffer(serial_t *obj)
{
    return 1;
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

#if DEVICE_SERIAL_ASYNCH

/******************************************************************************
 * ASYNCHRONOUS HAL
 ******************************************************************************/
int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint)
{
    int i;
    buffer_t *buf = &obj->tx_buff;
    struct serial_global_data_s *data = uart_data + obj->serial.ch;

    if (tx_length == 0) {
        return 0;
    }

    buf->buffer = (void *)tx;
    buf->length = tx_length * tx_width / 8;
    buf->pos = 0;
    buf->width = tx_width;
    data->tranferring_obj = obj;
    data->async_tx_callback = handler;
    serial_irq_set(obj, TxIrq, 1);

    while (!serial_writable(obj));
    i = buf->length;
    if (serial_available_buffer(obj) < i) {
        i = serial_available_buffer(obj);
    }
    do {
        uint8_t c = *(uint8_t *)buf->buffer;
        obj->tx_buff.buffer = (uint8_t *)obj->tx_buff.buffer + 1;
        ++buf->pos;
        obj->serial.uart->FTDR.BYTE = c;
        obj->serial.uart->FSR.WORD &= ~0x0060u;
    } while (--i);

    return buf->length;
}

void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width,
                      uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint)
{
    buffer_t *buf = &obj->rx_buff;
    struct serial_global_data_s *data = uart_data + obj->serial.ch;

    if (rx_length == 0) {
        return;
    }

    buf->buffer = rx;
    buf->length = rx_length * rx_width / 8;
    buf->pos = 0;
    buf->width = rx_width;
    obj->char_match = char_match;
    obj->char_found = 0;
    data->receiving_obj = obj;
    data->async_rx_callback = handler;
    data->event = 0;
    data->wanted_rx_events = event;

    serial_irq_set(obj, RxIrq, 1);
    serial_irq_err_set(obj, 1);
}

uint8_t serial_tx_active(serial_t *obj)
{
    return uart_data[obj->serial.ch].tranferring_obj != NULL;
}

uint8_t serial_rx_active(serial_t *obj)
{
    return uart_data[obj->serial.ch].receiving_obj != NULL;
}

int serial_irq_handler_asynch(serial_t *obj)
{
    return uart_data[obj->serial.ch].event;
}

void serial_tx_abort_asynch(serial_t *obj)
{
    uart_data[obj->serial.ch].tranferring_obj = NULL;
    obj->serial.uart->FCR.BIT.TFRST = 1;
    obj->serial.uart->FCR.BIT.TFRST = 0;
}

void serial_rx_abort_asynch(serial_t *obj)
{
    uart_data[obj->serial.ch].receiving_obj = NULL;
    obj->serial.uart->FCR.BIT.RFRST = 1;
    obj->serial.uart->FCR.BIT.RFRST = 0;
}

#endif
#endif
