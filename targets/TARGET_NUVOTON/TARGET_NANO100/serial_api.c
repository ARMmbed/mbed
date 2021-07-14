/* mbed Microcontroller Library
 * Copyright (c) 2015-2017 Nuvoton
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

#include "cmsis.h"
#include "mbed_error.h"
#include "mbed_assert.h"
#include "PeripheralPins.h"
#include "nu_modutil.h"
#include "nu_bitutil.h"
#include <string.h>
#include <stdbool.h>

#if DEVICE_SERIAL_ASYNCH
#include "dma_api.h"
#include "dma.h"
#endif

struct nu_uart_var {
    uint32_t    ref_cnt;                // Reference count of the H/W module
    serial_t *  obj;
    uint32_t    fifo_size_tx;
    uint32_t    fifo_size_rx;
    void        (*vec)(void);
#if DEVICE_SERIAL_ASYNCH
    uint8_t     pdma_perp_tx;
    uint8_t     pdma_perp_rx;
#endif
};

void UART0_IRQHandler(void);
void UART1_IRQHandler(void);
static void uart_irq(serial_t *obj);

#if DEVICE_SERIAL_ASYNCH
static void uart_irq_async(serial_t *obj);

static void uart_dma_handler_tx(uint32_t id, uint32_t event);
static void uart_dma_handler_rx(uint32_t id, uint32_t event);

static void serial_tx_enable_interrupt(serial_t *obj, uint32_t address, uint8_t enable);
static void serial_rx_enable_interrupt(serial_t *obj, uint32_t address, uint8_t enable);
static void serial_enable_interrupt(serial_t *obj, SerialIrq irq, uint32_t enable);
static void serial_rollback_interrupt(serial_t *obj, SerialIrq irq);
static int serial_write_async(serial_t *obj);
static int serial_read_async(serial_t *obj);

static uint32_t serial_rx_event_check(serial_t *obj);
static uint32_t serial_tx_event_check(serial_t *obj);

static int serial_is_tx_complete(serial_t *obj);
static void serial_tx_enable_event(serial_t *obj, int event, uint8_t enable);

static void serial_tx_buffer_set(serial_t *obj, const void *tx, size_t length, uint8_t width);
static void serial_rx_buffer_set(serial_t *obj, void *rx, size_t length, uint8_t width);
static void serial_rx_set_char_match(serial_t *obj, uint8_t char_match);
static void serial_rx_enable_event(serial_t *obj, int event, uint8_t enable);
static int serial_is_rx_complete(serial_t *obj);

static void serial_check_dma_usage(DMAUsage *dma_usage, int *dma_ch);
#endif

static int serial_is_irq_en(serial_t *obj, SerialIrq irq);

bool serial_can_deep_sleep(void);

static struct nu_uart_var uart0_var = {
    .ref_cnt            =   0,
    .obj                =   NULL,
    .fifo_size_tx       =   16,
    .fifo_size_rx       =   16,
    .vec                =   UART0_IRQHandler,
#if DEVICE_SERIAL_ASYNCH
    .pdma_perp_tx       =   PDMA_UART0_TX,
    .pdma_perp_rx       =   PDMA_UART0_RX
#endif
};
static struct nu_uart_var uart1_var = {
    .ref_cnt            =   0,
    .obj                =   NULL,
    .fifo_size_tx       =   16,
    .fifo_size_rx       =   16,
    .vec                =   UART1_IRQHandler,
#if DEVICE_SERIAL_ASYNCH
    .pdma_perp_tx       =   PDMA_UART1_TX,
    .pdma_perp_rx       =   PDMA_UART1_RX
#endif
};


int stdio_uart_inited = 0;
serial_t stdio_uart;
static uint32_t uart_modinit_mask = 0;

static const struct nu_modinit_s uart_modinit_tab[] = {
    {UART_0, UART0_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_UART_CLK_DIVIDER(1), UART0_RST, UART0_IRQn, &uart0_var},
    {UART_1, UART1_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_UART_CLK_DIVIDER(1), UART1_RST, UART1_IRQn, &uart1_var},
    
    {NC, 0, 0, 0, 0, (IRQn_Type) 0, NULL}
};

extern void mbed_sdk_init(void);

void serial_init(serial_t *obj, PinName tx, PinName rx)
{
    // NOTE: With armcc, serial_init() gets called from _sys_open() timing of which is before main()/mbed_sdk_init().
    mbed_sdk_init();
    
    // Determine which UART_x the pins are used for
    uint32_t uart_tx = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t uart_rx = pinmap_peripheral(rx, PinMap_UART_RX);
    // Get the peripheral name (UART_x) from the pins and assign it to the object
    obj->serial.uart = (UARTName) pinmap_merge(uart_tx, uart_rx);
    MBED_ASSERT((int)obj->serial.uart != NC);

    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    struct nu_uart_var *var = (struct nu_uart_var *) modinit->var;

    obj->serial.pin_tx = tx;
    obj->serial.pin_rx = rx;
    obj->serial.pin_rts = NC;
    obj->serial.pin_cts = NC;

    pinmap_pinout(tx, PinMap_UART_TX);
    pinmap_pinout(rx, PinMap_UART_RX);
        
    if (! var->ref_cnt) {
        // Select IP clock source
        CLK_SetModuleClock(modinit->clkidx, modinit->clksrc, modinit->clkdiv);

        // Enable IP clock
        CLK_EnableModuleClock(modinit->clkidx);

        // Reset this module
        SYS_ResetModule(modinit->rsetidx);

        // Configure baudrate
        int baudrate = 9600;
        if (obj->serial.uart == STDIO_UART) {
#if MBED_CONF_PLATFORM_STDIO_BAUD_RATE
            baudrate = MBED_CONF_PLATFORM_STDIO_BAUD_RATE;
#endif
        } else {
#if MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE
            baudrate = MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE;
#endif            
        }
        serial_baud(obj, baudrate);

        // Configure data bits, parity, and stop bits
        serial_format(obj, 8, ParityNone, 1);
    }
    var->ref_cnt ++;

    obj->serial.vec = var->vec;
    obj->serial.irq_en = 0;

#if DEVICE_SERIAL_ASYNCH
    obj->serial.dma_usage_tx = DMA_USAGE_NEVER;
    obj->serial.dma_usage_rx = DMA_USAGE_NEVER;
    obj->serial.event = 0;
    obj->serial.dma_chn_id_tx = DMA_ERROR_OUT_OF_CHANNELS;
    obj->serial.dma_chn_id_rx = DMA_ERROR_OUT_OF_CHANNELS;
#endif

    /* With support for checking H/W UART initialized or not, we allow serial_init(&stdio_uart)
     * calls in even though H/W UART 'STDIO_UART' has initialized. When serial_init(&stdio_uart)
     * calls in, we only need to set the 'stdio_uart_inited' flag. */
    if (((uintptr_t) obj) == ((uintptr_t) &stdio_uart)) {
        MBED_ASSERT(obj->serial.uart == STDIO_UART);
        stdio_uart_inited = 1;
    }

    if (var->ref_cnt) {
        // Mark this module to be inited.
        int i = modinit - uart_modinit_tab;
        uart_modinit_mask |= 1 << i;
    }
}

void serial_free(serial_t *obj)
{
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    struct nu_uart_var *var = (struct nu_uart_var *) modinit->var;
    
    var->ref_cnt --;
    if (! var->ref_cnt) {
#if DEVICE_SERIAL_ASYNCH
        if (obj->serial.dma_chn_id_tx != DMA_ERROR_OUT_OF_CHANNELS) {
            dma_channel_free(obj->serial.dma_chn_id_tx);
            obj->serial.dma_chn_id_tx = DMA_ERROR_OUT_OF_CHANNELS;
        }
        if (obj->serial.dma_chn_id_rx != DMA_ERROR_OUT_OF_CHANNELS) {
            dma_channel_free(obj->serial.dma_chn_id_rx);
            obj->serial.dma_chn_id_rx = DMA_ERROR_OUT_OF_CHANNELS;
        }
#endif

        UART_Close((UART_T *) NU_MODBASE(obj->serial.uart));
    
        UART_DISABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), (UART_IER_RDA_IE_Msk | UART_IER_THRE_IE_Msk | UART_IER_RTO_IE_Msk));
        NVIC_DisableIRQ(modinit->irq_n);
    
        // Disable IP clock
        CLK_DisableModuleClock(modinit->clkidx);
    }
    
    if (var->obj == obj) {
        var->obj = NULL;
    }

    /* Clear the 'stdio_uart_inited' flag when serial_free(&stdio_uart) calls in. */
    if (((uintptr_t) obj) == ((uintptr_t) &stdio_uart)) {
        MBED_ASSERT(obj->serial.uart == STDIO_UART);
        stdio_uart_inited = 0;
    }

    if (! var->ref_cnt) {
        // Mark this module to be deinited.
        int i = modinit - uart_modinit_tab;
        uart_modinit_mask &= ~(1 << i);
    }
    
    // Free up pins
    gpio_set(obj->serial.pin_tx);
    gpio_set(obj->serial.pin_rx);
    gpio_set(obj->serial.pin_rts);
    gpio_set(obj->serial.pin_cts);
    obj->serial.pin_tx = NC;
    obj->serial.pin_rx = NC;
    obj->serial.pin_rts = NC;
    obj->serial.pin_cts = NC;
}

void serial_baud(serial_t *obj, int baudrate) {
    // Flush Tx FIFO. Otherwise, output data may get lost on this change.
    while (! UART_IS_TX_EMPTY(((UART_T *) NU_MODBASE(obj->serial.uart))));
    
    obj->serial.baudrate = baudrate;
    UART_Open((UART_T *) NU_MODBASE(obj->serial.uart), baudrate);
}

void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits) {
    // Flush Tx FIFO. Otherwise, output data may get lost on this change.
    while (! UART_IS_TX_EMPTY(((UART_T *) NU_MODBASE(obj->serial.uart))));
    
    // TODO: Assert for not supported parity and data bits
    obj->serial.databits = data_bits;
    obj->serial.parity = parity;
    obj->serial.stopbits = stop_bits;
    
    uint32_t databits_intern = (data_bits == 5) ? UART_WORD_LEN_5 :
        (data_bits == 6) ? UART_WORD_LEN_6 :
        (data_bits == 7) ? UART_WORD_LEN_7 : 
        UART_WORD_LEN_8;
    uint32_t parity_intern = (parity == ParityOdd || parity == ParityForced1) ? UART_PARITY_ODD :
        (parity == ParityEven || parity == ParityForced0) ? UART_PARITY_EVEN :
        UART_PARITY_NONE;
    uint32_t stopbits_intern = (stop_bits == 2) ? UART_STOP_BIT_2 : UART_STOP_BIT_1;
    UART_SetLine_Config((UART_T *) NU_MODBASE(obj->serial.uart), 
        0,  // Don't change baudrate 
        databits_intern, 
        parity_intern, 
        stopbits_intern);
}

#if DEVICE_SERIAL_FC

void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow)
{
    UART_T *uart_base = (UART_T *) NU_MODBASE(obj->serial.uart);

    // Free up old rts/cts pins when they are different from new ones
    if (obj->serial.pin_rts != rxflow) {
        gpio_set(obj->serial.pin_rts);
        obj->serial.pin_rts = rxflow;
    }
    if (obj->serial.pin_cts != txflow) {
        gpio_set(obj->serial.pin_cts);
        obj->serial.pin_cts = txflow;
    }

    if (rxflow != NC) {
        // Check if RTS pin matches.
        uint32_t uart_rts = pinmap_peripheral(rxflow, PinMap_UART_RTS);
        MBED_ASSERT(uart_rts == obj->serial.uart);
        // Enable the pin for RTS function
        pinmap_pinout(rxflow, PinMap_UART_RTS);
        // nRTS pin output is low level active
        uart_base->MCSR |= UART_MCSR_LEV_RTS_Msk;
        // Configure RTS trigger level to 8 bytes
        uart_base->TLCTL = (uart_base->TLCTL & ~UART_TLCTL_RTS_TRI_LEV_Msk) | UART_TLCTL_RTS_TRI_LEV_8BYTES;
        // Configure RX Trigger Level to 8 bytes
        uart_base->TLCTL = (uart_base->TLCTL & ~UART_TLCTL_RFITL_Msk) | UART_TLCTL_RFITL_8BYTES;
        
        if (type == FlowControlRTS || type == FlowControlRTSCTS) {
            // Enable RTS
            uart_base->CTL |= UART_CTL_AUTO_RTS_EN_Msk;
        } else {
            // Disable RTS
            uart_base->CTL &= ~UART_CTL_AUTO_RTS_EN_Msk;
            /* Drive nRTS pin output to low-active. Allow the peer to be able to send data
             * even though its CTS is still enabled. */
            /* NOTE: NOT SUPPORT on NANO130 */
        }
    }

    /* If CTS is disabled, we don't need to configure CTS. But to be consistent with
     * RTS code above, we still configure CTS. */
    if (txflow != NC) {    
        // Check if CTS pin matches.
        uint32_t uart_cts = pinmap_peripheral(txflow, PinMap_UART_CTS);
        MBED_ASSERT(uart_cts == obj->serial.uart);
        // Enable the pin for CTS function
        pinmap_pinout(txflow, PinMap_UART_CTS);
        // nCTS pin input is low level active
        uart_base->MCSR |= UART_MCSR_LEV_CTS_Msk;

        if (type == FlowControlCTS || type == FlowControlRTSCTS)  {        
            // Enable CTS
            uart_base->CTL |= UART_CTL_AUTO_CTS_EN_Msk;
        } else {
            // Disable CTS
            uart_base->CTL &= ~UART_CTL_AUTO_CTS_EN_Msk;
        }
    }
}

#endif  //DEVICE_SERIAL_FC

void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id)
{
    // Flush Tx FIFO. Otherwise, output data may get lost on this change.
    while (! UART_IS_TX_EMPTY(((UART_T *) NU_MODBASE(obj->serial.uart))));
    
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    obj->serial.irq_handler = (uint32_t) handler;
    obj->serial.irq_id = id;
    
    // Restore sync-mode vector
    obj->serial.async_en = 0;
}

void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    obj->serial.irq_en = enable;
    serial_enable_interrupt(obj, irq, enable);
}

int serial_getc(serial_t *obj)
{
    // TODO: Fix every byte access requires accompaniment of one interrupt. This has side effect of performance degradation.
    while (! serial_readable(obj));
    int c = UART_READ(((UART_T *) NU_MODBASE(obj->serial.uart)));
    
    // NOTE: On Nuvoton targets, no H/W IRQ to match TxIrq/RxIrq.
    //       Simulation of TxIrq/RxIrq requires the call to Serial::putc()/Serial::getc() respectively. 
    if (obj->serial.ier_msk & (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk)) {
        UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
    }
    
    return c;
}

void serial_putc(serial_t *obj, int c)
{
    // TODO: Fix every byte access requires accompaniment of one interrupt. This has side effect of performance degradation.
    while (! serial_writable(obj));
    UART_WRITE(((UART_T *) NU_MODBASE(obj->serial.uart)), c);
    
    // NOTE: On Nuvoton targets, no H/W IRQ to match TxIrq/RxIrq.
    //       Simulation of TxIrq/RxIrq requires the call to Serial::putc()/Serial::getc() respectively. 
    if (obj->serial.ier_msk & UART_IER_THRE_IE_Msk) {
        UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_THRE_IE_Msk);
    }
}

int serial_readable(serial_t *obj)
{
    return ! UART_GET_RX_EMPTY(((UART_T *) NU_MODBASE(obj->serial.uart)));
}

int serial_writable(serial_t *obj)
{
    return ! UART_IS_TX_FULL(((UART_T *) NU_MODBASE(obj->serial.uart)));
}

int serial_tx_empty(serial_t *obj)
{
    return UART_IS_TX_EMPTY(((UART_T *) NU_MODBASE(obj->serial.uart)));
}

void serial_pinout_tx(PinName tx)
{
    pinmap_pinout(tx, PinMap_UART_TX);
}

void serial_break_set(serial_t *obj)
{
    ((UART_T *) NU_MODBASE(obj->serial.uart))->TLCTL |= UART_TLCTL_BCB_Msk;
}

void serial_break_clear(serial_t *obj)
{
    ((UART_T *) NU_MODBASE(obj->serial.uart))->TLCTL &= ~UART_TLCTL_BCB_Msk;
}

void UART0_IRQHandler(void)
{
#if DEVICE_SERIAL_ASYNCH
    if (uart0_var.obj->serial.async_en)
        uart_irq_async(uart0_var.obj);
    else
#endif
        uart_irq(uart0_var.obj);
}

void UART1_IRQHandler(void)
{
#if DEVICE_SERIAL_ASYNCH
    if (uart1_var.obj->serial.async_en)
        uart_irq_async(uart1_var.obj);
    else
#endif
        uart_irq(uart1_var.obj);
}

static void uart_irq(serial_t *obj)
{
    UART_T *uart_base = (UART_T *) NU_MODBASE(obj->serial.uart);

    if (uart_base->ISR & (UART_ISR_RDA_IS_Msk | UART_ISR_RTO_IS_Msk)) {
        // Simulate clear of the interrupt flag. Temporarily disable the interrupt here and to be recovered on next read.
        UART_DISABLE_INT(uart_base, (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
        if (obj->serial.irq_handler && serial_is_irq_en(obj, RxIrq)) {
            // Call irq_handler() only when RxIrq is enabled
            ((uart_irq_handler) obj->serial.irq_handler)(obj->serial.irq_id, RxIrq);
        }
    }
    
    if (uart_base->ISR & UART_ISR_THRE_IS_Msk) {
        // Simulate clear of the interrupt flag. Temporarily disable the interrupt here and to be recovered on next write.
        UART_DISABLE_INT(uart_base, UART_IER_THRE_IE_Msk);
        if (obj->serial.irq_handler && serial_is_irq_en(obj, TxIrq)) {
            // Call irq_handler() only when TxIrq is enabled
            ((uart_irq_handler) obj->serial.irq_handler)(obj->serial.irq_id, TxIrq);
        }
    }
    
    // NOTE: Ignore all other interrupt flags. Clear them. Otherwise, program will get stuck in interrupt.
    uart_base->ISR = uart_base->ISR;
    uart_base->FSR = uart_base->FSR;
}


#if DEVICE_SERIAL_ASYNCH
int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint)
{
    MBED_ASSERT(tx_width == 8 || tx_width == 16 || tx_width == 32);

    obj->serial.dma_usage_tx = hint;
    serial_check_dma_usage(&obj->serial.dma_usage_tx, &obj->serial.dma_chn_id_tx);
    
    // UART IRQ is necessary for both interrupt way and DMA way
    serial_tx_enable_event(obj, event, 1);
    serial_tx_buffer_set(obj, tx, tx_length, tx_width);
            
    int n_word = 0;
    if (obj->serial.dma_usage_tx == DMA_USAGE_NEVER) {
        // Interrupt way
        n_word = serial_write_async(obj);
        serial_tx_enable_interrupt(obj, handler, 1);
    } else {
        // DMA way
        const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
        MBED_ASSERT(modinit != NULL);
        MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
        dma_enable(obj->serial.dma_chn_id_tx, 1);                   // Enable this DMA channel
        PDMA_SetTransferMode(obj->serial.dma_chn_id_tx,
            ((struct nu_uart_var *) modinit->var)->pdma_perp_tx,    // Peripheral connected to this PDMA
            0,  // Scatter-gather disabled
            0); // Scatter-gather descriptor address
        PDMA_SetTransferCnt(obj->serial.dma_chn_id_tx, 
            (tx_width == 8) ? PDMA_WIDTH_8 : (tx_width == 16) ? PDMA_WIDTH_16 : PDMA_WIDTH_32, 
            tx_length);
        PDMA_SetTransferAddr(obj->serial.dma_chn_id_tx,
            (uint32_t) tx,  // NOTE:
                            // NUC472: End of source address
                            // M451: Start of source address
            PDMA_SAR_INC,   // Source address incremental
            (uint32_t) NU_MODBASE(obj->serial.uart),    // Destination address
            PDMA_DAR_FIX);  // Destination address fixed
        PDMA_EnableInt(obj->serial.dma_chn_id_tx,
            PDMA_IER_TD_IE_Msk); // Interrupt type
        // Register DMA event handler
        dma_set_handler(obj->serial.dma_chn_id_tx, (uint32_t) uart_dma_handler_tx, (uint32_t) obj, DMA_EVENT_ALL);
        serial_tx_enable_interrupt(obj, handler, 1);
        /* We needn't actually enable UART INT to go UART ISR -> handler.
         * Instead, as PDMA INT is triggered, we will go PDMA ISR -> UART ISR -> handler
         * with serial_tx/rx_enable_interrupt having set up this call path. */
        UART_DISABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_THRE_IE_Msk);
        PDMA_Trigger(obj->serial.dma_chn_id_tx);
        ((UART_T *) NU_MODBASE(obj->serial.uart))->CTL |= UART_CTL_DMA_TX_EN_Msk;   // Start DMA transfer
    }
    
    return n_word;
}

void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint)
{
    MBED_ASSERT(rx_width == 8 || rx_width == 16 || rx_width == 32);

    obj->serial.dma_usage_rx = hint;
    serial_check_dma_usage(&obj->serial.dma_usage_rx, &obj->serial.dma_chn_id_rx);
    // DMA doesn't support char match, so fall back to IRQ if it is requested.
    if (obj->serial.dma_usage_rx != DMA_USAGE_NEVER && 
        (event & SERIAL_EVENT_RX_CHARACTER_MATCH) && 
        char_match != SERIAL_RESERVED_CHAR_MATCH) {
        obj->serial.dma_usage_rx = DMA_USAGE_NEVER;
        dma_channel_free(obj->serial.dma_chn_id_rx);
        obj->serial.dma_chn_id_rx = DMA_ERROR_OUT_OF_CHANNELS;
    }
    
    // UART IRQ is necessary for both interrupt way and DMA way
    serial_rx_enable_event(obj, event, 1);
    serial_rx_buffer_set(obj, rx, rx_length, rx_width);
    serial_rx_set_char_match(obj, char_match);
        
    if (obj->serial.dma_usage_rx == DMA_USAGE_NEVER) {
        // Interrupt way
        serial_rx_enable_interrupt(obj, handler, 1);
    } else {
        // DMA way
        const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
        MBED_ASSERT(modinit != NULL);
        MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
        dma_enable(obj->serial.dma_chn_id_rx, 1);                   // Enable this DMA channel
        PDMA_SetTransferMode(obj->serial.dma_chn_id_rx,
            ((struct nu_uart_var *) modinit->var)->pdma_perp_rx,    // Peripheral connected to this PDMA
            0,  // Scatter-gather disabled
            0); // Scatter-gather descriptor address
        PDMA_SetTransferCnt(obj->serial.dma_chn_id_rx, 
            (rx_width == 8) ? PDMA_WIDTH_8 : (rx_width == 16) ? PDMA_WIDTH_16 : PDMA_WIDTH_32, 
            rx_length);
        PDMA_SetTransferAddr(obj->serial.dma_chn_id_rx,
            (uint32_t) NU_MODBASE(obj->serial.uart),    // Source address
            PDMA_SAR_FIX,   // Source address fixed
            (uint32_t) rx,  // NOTE: 
                            // NUC472: End of destination address
                            // M451: Start of destination address
            PDMA_DAR_INC);  // Destination address incremental
        PDMA_EnableInt(obj->serial.dma_chn_id_rx,
            PDMA_IER_TD_IE_Msk); // Interrupt type
        // Register DMA event handler
        dma_set_handler(obj->serial.dma_chn_id_rx, (uint32_t) uart_dma_handler_rx, (uint32_t) obj, DMA_EVENT_ALL);
        serial_rx_enable_interrupt(obj, handler, 1);
        /* We needn't actually enable UART INT to go UART ISR -> handler.
         * Instead, as PDMA INT is triggered, we will go PDMA ISR -> UART ISR -> handler
         * with serial_tx/rx_enable_interrupt having set up this call path. */
        UART_DISABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
        PDMA_Trigger(obj->serial.dma_chn_id_rx);
        ((UART_T *) NU_MODBASE(obj->serial.uart))->CTL |= UART_CTL_DMA_RX_EN_Msk;   // Start DMA transfer
    }
}

void serial_tx_abort_asynch(serial_t *obj)
{
    // Flush Tx FIFO. Otherwise, output data may get lost on this change.
    while (! UART_IS_TX_EMPTY(((UART_T *) NU_MODBASE(obj->serial.uart))));
    
    if (obj->serial.dma_usage_tx != DMA_USAGE_NEVER) {
        if (obj->serial.dma_chn_id_tx != DMA_ERROR_OUT_OF_CHANNELS) {
            PDMA_DisableInt(obj->serial.dma_chn_id_tx, PDMA_IER_TD_IE_Msk);
            // NOTE: On NUC472, next PDMA transfer will fail with PDMA_STOP() called.
            dma_enable(obj->serial.dma_chn_id_tx, 0);
        }
        ((UART_T *) NU_MODBASE(obj->serial.uart))->CTL &= ~UART_CTL_DMA_TX_EN_Msk;
    }
    
    // Necessary for both interrupt way and DMA way
    serial_enable_interrupt(obj, TxIrq, 0);
    serial_rollback_interrupt(obj, TxIrq);
}

void serial_rx_abort_asynch(serial_t *obj)
{
    if (obj->serial.dma_usage_rx != DMA_USAGE_NEVER) {
        if (obj->serial.dma_chn_id_rx != DMA_ERROR_OUT_OF_CHANNELS) {
            PDMA_DisableInt(obj->serial.dma_chn_id_rx, PDMA_IER_TD_IE_Msk);
            // NOTE: On NUC472, next PDMA transfer will fail with PDMA_STOP() called.
            dma_enable(obj->serial.dma_chn_id_rx, 0);
        }
        ((UART_T *) NU_MODBASE(obj->serial.uart))->CTL &= ~UART_CTL_DMA_RX_EN_Msk;
    }
    
    // Necessary for both interrupt way and DMA way
    serial_enable_interrupt(obj, RxIrq, 0);
    serial_rollback_interrupt(obj, RxIrq);
}

uint8_t serial_tx_active(serial_t *obj)
{
    // NOTE: Judge by serial_is_irq_en(obj, TxIrq) doesn't work with sync/async modes interleaved. Change with TX FIFO empty flag.
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    return (obj->serial.async_en);
}

uint8_t serial_rx_active(serial_t *obj)
{
    // NOTE: Judge by serial_is_irq_en(obj, RxIrq) doesn't work with sync/async modes interleaved. Change with RX FIFO empty flag.
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    return (obj->serial.async_en);
}

int serial_irq_handler_asynch(serial_t *obj)
{
    int event_rx = 0;
    int event_tx = 0;
    
    // Necessary for both interrupt way and DMA way
    if (serial_is_irq_en(obj, RxIrq)) {
        event_rx = serial_rx_event_check(obj);
        if (event_rx) {
            serial_rx_abort_asynch(obj);
        }
    }
        
    if (serial_is_irq_en(obj, TxIrq)) {
        event_tx = serial_tx_event_check(obj);
        if (event_tx) {
            serial_tx_abort_asynch(obj);
        }
    }
        
    return (obj->serial.event & (event_rx | event_tx));
}

static void uart_irq_async(serial_t *obj)
{
    if (serial_is_irq_en(obj, RxIrq)) {
        (*obj->serial.irq_handler_rx_async)();
    }
    if (serial_is_irq_en(obj, TxIrq)) {
        (*obj->serial.irq_handler_tx_async)();
    }
}

static void serial_rx_set_char_match(serial_t *obj, uint8_t char_match)
{
    obj->char_match = char_match;
    obj->char_found = 0;
}

static void serial_tx_enable_event(serial_t *obj, int event, uint8_t enable)
{
    obj->serial.event &= ~SERIAL_EVENT_TX_MASK;
    obj->serial.event |= (event & SERIAL_EVENT_TX_MASK);
    
}

static void serial_rx_enable_event(serial_t *obj, int event, uint8_t enable)
{
    obj->serial.event &= ~SERIAL_EVENT_RX_MASK;
    obj->serial.event |= (event & SERIAL_EVENT_RX_MASK);
    
    if (event & SERIAL_EVENT_RX_FRAMING_ERROR) {
        UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_RLS_IE_Msk);
    }
    if (event & SERIAL_EVENT_RX_PARITY_ERROR) {
        UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_RLS_IE_Msk);
    }
    if (event & SERIAL_EVENT_RX_OVERFLOW) {
        UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_BUF_ERR_IE_Msk);
    }
}

static int serial_is_tx_complete(serial_t *obj)
{
    // NOTE: Exclude tx fifo empty check due to no such interrupt on DMA way
    return (obj->tx_buff.pos == obj->tx_buff.length);
}

static int serial_is_rx_complete(serial_t *obj)
{
    return (obj->rx_buff.pos == obj->rx_buff.length);
}

static uint32_t serial_tx_event_check(serial_t *obj)
{
    UART_T *uart_base = (UART_T *) NU_MODBASE(obj->serial.uart);
    
    if (uart_base->ISR & UART_ISR_THRE_IS_Msk) {
        // Simulate clear of the interrupt flag. Temporarily disable the interrupt here and to be recovered on next write.
        UART_DISABLE_INT(uart_base, UART_IER_THRE_IE_Msk);
    }
    
    uint32_t event = 0;
    
    if (obj->serial.dma_usage_tx == DMA_USAGE_NEVER) {
        serial_write_async(obj);
    }
    
    if (serial_is_tx_complete(obj)) {
        event |= SERIAL_EVENT_TX_COMPLETE;
    }
    
    return event;
}

static uint32_t serial_rx_event_check(serial_t *obj)
{
    UART_T *uart_base = (UART_T *) NU_MODBASE(obj->serial.uart);
    
    if (uart_base->ISR & (UART_ISR_RDA_IS_Msk | UART_ISR_RTO_IS_Msk)) {
        // Simulate clear of the interrupt flag. Temporarily disable the interrupt here and to be recovered on next read.
        UART_DISABLE_INT(uart_base, (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
    }
    
    uint32_t event = 0;
    
    if (uart_base->FSR & UART_FSR_BI_F_Msk) {
        uart_base->FSR = UART_FSR_BI_F_Msk;
    }
    if (uart_base->FSR & UART_FSR_FE_F_Msk) {
        uart_base->FSR = UART_FSR_FE_F_Msk;
        event |= SERIAL_EVENT_RX_FRAMING_ERROR;
    }
    if (uart_base->FSR & UART_FSR_PE_F_Msk) {
        uart_base->FSR = UART_FSR_PE_F_Msk;
        event |= SERIAL_EVENT_RX_PARITY_ERROR;
    }
    
    if (uart_base->FSR & UART_FSR_RX_OVER_F_Msk) {
        uart_base->FSR = UART_FSR_RX_OVER_F_Msk;
        event |= SERIAL_EVENT_RX_OVERFLOW;
    }

    if (obj->serial.dma_usage_rx == DMA_USAGE_NEVER) {
        serial_read_async(obj);
    }
    
    if (serial_is_rx_complete(obj)) {
        event |= SERIAL_EVENT_RX_COMPLETE;
    }
    if ((obj->char_match != SERIAL_RESERVED_CHAR_MATCH) && obj->char_found) {
        event |= SERIAL_EVENT_RX_CHARACTER_MATCH;
        // NOTE: Timing to reset char_found?
        // by obj->char_found = 0;
    }
    
    return event;
}

static void uart_dma_handler_tx(uint32_t id, uint32_t event_dma)
{
    serial_t *obj = (serial_t *) id;
    
    // TODO: Pass this error to caller
    if (event_dma & DMA_EVENT_ABORT) {
    }
    // Expect UART IRQ will catch this transfer done event
    if (event_dma & DMA_EVENT_TRANSFER_DONE) {
        obj->tx_buff.pos = obj->tx_buff.length;
    }
    // TODO: Pass this error to caller
    if (event_dma & DMA_EVENT_TIMEOUT) {
    }
    
    uart_irq_async(obj);
}

static void uart_dma_handler_rx(uint32_t id, uint32_t event_dma)
{
    serial_t *obj = (serial_t *) id;
    
    // TODO: Pass this error to caller
    if (event_dma & DMA_EVENT_ABORT) {
    }
    // Expect UART IRQ will catch this transfer done event
    if (event_dma & DMA_EVENT_TRANSFER_DONE) {
        obj->rx_buff.pos = obj->rx_buff.length;
    }
    // TODO: Pass this error to caller
    if (event_dma & DMA_EVENT_TIMEOUT) {
    }
    
    uart_irq_async(obj);
}

static int serial_write_async(serial_t *obj)
{   
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    UART_T *uart_base = (UART_T *) NU_MODBASE(obj->serial.uart);
    
    uint32_t tx_fifo_max = ((struct nu_uart_var *) modinit->var)->fifo_size_tx;
    uint32_t tx_fifo_busy = (uart_base->FSR & UART_FSR_TX_POINTER_F_Msk) >> UART_FSR_TX_POINTER_F_Pos;
    if (uart_base->FSR & UART_FSR_TX_FULL_F_Msk) {
        tx_fifo_busy = tx_fifo_max;
    }
    uint32_t tx_fifo_free = tx_fifo_max - tx_fifo_busy;
    if (tx_fifo_free == 0) {
        // Simulate clear of the interrupt flag
        if (obj->serial.ier_msk & UART_IER_THRE_IE_Msk) {
            UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_THRE_IE_Msk);
        }
        return 0;
    }
    
    uint32_t bytes_per_word = obj->tx_buff.width / 8;
    
    uint8_t *tx = (uint8_t *)(obj->tx_buff.buffer) + bytes_per_word * obj->tx_buff.pos;
    int n_words = 0;
    while (obj->tx_buff.pos < obj->tx_buff.length && tx_fifo_free >= bytes_per_word) {
        switch (bytes_per_word) {
            case 4:
                UART_WRITE(((UART_T *) NU_MODBASE(obj->serial.uart)), *tx ++);
                UART_WRITE(((UART_T *) NU_MODBASE(obj->serial.uart)), *tx ++);
            case 2:
                UART_WRITE(((UART_T *) NU_MODBASE(obj->serial.uart)), *tx ++);
            case 1:
                UART_WRITE(((UART_T *) NU_MODBASE(obj->serial.uart)), *tx ++);
        }
        
        n_words ++;
        tx_fifo_free -= bytes_per_word;
        obj->tx_buff.pos ++;
    }
    
    if (n_words) {
        // Simulate clear of the interrupt flag
        if (obj->serial.ier_msk & UART_IER_THRE_IE_Msk) {
            UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_THRE_IE_Msk);
        }
    }
    
    return n_words;
}

static int serial_read_async(serial_t *obj)
{
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    uint32_t rx_fifo_busy = (((UART_T *) NU_MODBASE(obj->serial.uart))->FSR & UART_FSR_RX_POINTER_F_Msk) >> UART_FSR_RX_POINTER_F_Pos;
    
    uint32_t bytes_per_word = obj->rx_buff.width / 8;
    
    uint8_t *rx = (uint8_t *)(obj->rx_buff.buffer) + bytes_per_word * obj->rx_buff.pos;
    int n_words = 0;
    while (obj->rx_buff.pos < obj->rx_buff.length && rx_fifo_busy >= bytes_per_word) {
        switch (bytes_per_word) {
            case 4:
                *rx ++ = UART_READ(((UART_T *) NU_MODBASE(obj->serial.uart)));
                *rx ++ = UART_READ(((UART_T *) NU_MODBASE(obj->serial.uart)));
            case 2:
                *rx ++ = UART_READ(((UART_T *) NU_MODBASE(obj->serial.uart)));
            case 1:
                *rx ++ = UART_READ(((UART_T *) NU_MODBASE(obj->serial.uart)));
        }
        
        n_words ++;
        rx_fifo_busy -= bytes_per_word;
        obj->rx_buff.pos ++;
        
        if ((obj->serial.event & SERIAL_EVENT_RX_CHARACTER_MATCH) &&
            obj->char_match != SERIAL_RESERVED_CHAR_MATCH) {
            uint8_t *rx_cmp = rx;
            switch (bytes_per_word) {
                case 4:
                    rx_cmp -= 2;
                case 2:
                    rx_cmp --;
                case 1:
                    rx_cmp --;
            }
            if (*rx_cmp == obj->char_match) {
                obj->char_found = 1;
                break;
            }
        }
    }
    
    if (n_words) {
        // Simulate clear of the interrupt flag
        if (obj->serial.ier_msk & (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk)) {
            UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
        }
    }
    
    return n_words;
}

static void serial_tx_buffer_set(serial_t *obj, const void *tx, size_t length, uint8_t width)
{
    obj->tx_buff.buffer = (void *) tx;
    obj->tx_buff.length = length;
    obj->tx_buff.pos = 0;
    obj->tx_buff.width = width;
}

static void serial_rx_buffer_set(serial_t *obj, void *rx, size_t length, uint8_t width)
{
    obj->rx_buff.buffer = rx;
    obj->rx_buff.length = length;
    obj->rx_buff.pos = 0;
    obj->rx_buff.width = width;
}

static void serial_tx_enable_interrupt(serial_t *obj, uint32_t handler, uint8_t enable)
{
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    // Necessary for both interrupt way and DMA way
    // A flag to indicate async mode, and tx/rx handlers can be different.
    obj->serial.async_en = 1;
    obj->serial.irq_handler_tx_async = (void (*)(void)) handler;
    serial_enable_interrupt(obj, TxIrq, enable);
}

static void serial_rx_enable_interrupt(serial_t *obj, uint32_t handler, uint8_t enable)
{
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    // Necessary for both interrupt way and DMA way
    // A flag to indicate async mode, and tx/rx handlers can be different.
    obj->serial.async_en = 1;
    obj->serial.irq_handler_rx_async = (void (*) (void)) handler;
    serial_enable_interrupt(obj, RxIrq, enable);
}

static void serial_enable_interrupt(serial_t *obj, SerialIrq irq, uint32_t enable)
{
    if (enable) {
        const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
        MBED_ASSERT(modinit != NULL);
        MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
        NVIC_SetVector(modinit->irq_n, (uint32_t) obj->serial.vec);
        NVIC_EnableIRQ(modinit->irq_n);
        
        struct nu_uart_var *var = (struct nu_uart_var *) modinit->var;
        // Multiple serial S/W objects for single UART H/W module possibly.
        // Bind serial S/W object to UART H/W module as interrupt is enabled.
        var->obj = obj;
        
        switch (irq) {
            // NOTE: Setting ier_msk first to avoid race condition
            case RxIrq:
                obj->serial.ier_msk = obj->serial.ier_msk | (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk);
                UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
                break;
            case TxIrq:
                obj->serial.ier_msk = obj->serial.ier_msk | UART_IER_THRE_IE_Msk;
                UART_ENABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_THRE_IE_Msk);
                break;
        }
    }
    else { // disable
        switch (irq) {
            case RxIrq:
                UART_DISABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk));
                obj->serial.ier_msk = obj->serial.ier_msk & ~(UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk);
                break;
            case TxIrq:
                UART_DISABLE_INT(((UART_T *) NU_MODBASE(obj->serial.uart)), UART_IER_THRE_IE_Msk);
                obj->serial.ier_msk = obj->serial.ier_msk & ~UART_IER_THRE_IE_Msk;
                break;
        }
    }
}

static void serial_rollback_interrupt(serial_t *obj, SerialIrq irq)
{
    const struct nu_modinit_s *modinit = get_modinit(obj->serial.uart, uart_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->serial.uart);
    
    obj->serial.async_en = 0;
    serial_enable_interrupt(obj, irq, obj->serial.irq_en);
}

static void serial_check_dma_usage(DMAUsage *dma_usage, int *dma_ch)
{
    if (*dma_usage != DMA_USAGE_NEVER) {
        if (*dma_ch == DMA_ERROR_OUT_OF_CHANNELS) {
            *dma_ch = dma_channel_allocate(DMA_CAP_NONE);
        }
        if (*dma_ch == DMA_ERROR_OUT_OF_CHANNELS) {
            *dma_usage = DMA_USAGE_NEVER;
        }
    }
    else {
        dma_channel_free(*dma_ch);
        *dma_ch = DMA_ERROR_OUT_OF_CHANNELS;
    }
}

#endif  // #if DEVICE_SERIAL_ASYNCH

static int serial_is_irq_en(serial_t *obj, SerialIrq irq)
{
    int ier_msk = 0;
    
    switch (irq) {
        case RxIrq:
            ier_msk = obj->serial.ier_msk & (UART_IER_RDA_IE_Msk | UART_IER_RTO_IE_Msk);
            break;
        case TxIrq:
            ier_msk = obj->serial.ier_msk & UART_IER_THRE_IE_Msk;
            break;
    }
    
    return !! ier_msk;
}

bool serial_can_deep_sleep(void)
{
    bool sleep_allowed = 1;
    const struct nu_modinit_s *modinit = uart_modinit_tab;
    while (modinit->var != NULL) {
        struct nu_uart_var *uart_var = (struct nu_uart_var *) modinit->var;
        UART_T *uart_base = (UART_T *) NU_MODBASE(modinit->modname);
        if (uart_var->ref_cnt > 0) {
            if (!UART_IS_TX_EMPTY(uart_base)) {
                sleep_allowed = 0;
                break;
            }
        }
        modinit++;
    }
    return sleep_allowed;
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

#endif  // #if DEVICE_SERIAL
