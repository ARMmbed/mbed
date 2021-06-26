/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2015, STMicroelectronics
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
#ifndef MBED_OBJECTS_H
#define MBED_OBJECTS_H

#include "cmsis.h"
#include "PortNames.h"
#include "PeripheralNames.h"
#include "PinNames.h"
#include "gpio_object.h"

#include "stm32f3xx_ll_usart.h"
#include "stm32f3xx_ll_tim.h"
#include "stm32f3xx_ll_pwr.h"
#include "stm32f3xx_ll_adc.h"
#include "stm32f3xx_ll_rtc.h"
#include "stm32f3xx_ll_rcc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pwmout_s {
    PWMName pwm;
    PinName pin;
    uint32_t prescaler;
    uint32_t period;
    uint32_t pulse;
    uint8_t channel;
    uint8_t inverted;
};

struct spi_s {
    SPI_HandleTypeDef handle;
    IRQn_Type spiIRQ;
    SPIName spi;
    PinName pin_miso;
    PinName pin_mosi;
    PinName pin_sclk;
    PinName pin_ssel;
#if DEVICE_SPI_ASYNCH
    uint32_t event;
    uint8_t transfer_type;
#endif
};

struct serial_s {
    UARTName uart;
    int index; // Used by irq
    uint32_t baudrate;
    uint32_t databits;
    uint32_t stopbits;
    uint32_t parity;
    PinName pin_tx;
    PinName pin_rx;
#if DEVICE_SERIAL_ASYNCH
    uint32_t events;
#endif
#if DEVICE_SERIAL_FC
    uint32_t hw_flow_ctl;
    PinName pin_rts;
    PinName pin_cts;
#endif
};

struct i2c_s {
    /*  The 1st 2 members I2CName i2c
     *  and I2C_HandleTypeDef handle should
     *  be kept as the first members of this struct
     *  to ensure i2c_get_obj to work as expected
     */
    I2CName  i2c;
    I2C_HandleTypeDef handle;
    uint8_t index;
    int hz;
    PinName sda;
    PinName scl;
    int sda_func;
    int scl_func;
    IRQn_Type event_i2cIRQ;
    IRQn_Type error_i2cIRQ;
    uint32_t XferOperation;
    volatile uint8_t event;
    volatile int pending_start;
#if DEVICE_I2CSLAVE
    uint8_t slave;
    volatile uint8_t pending_slave_tx_master_rx;
    volatile uint8_t pending_slave_rx_maxter_tx;
    uint8_t *slave_rx_buffer;
    volatile uint8_t slave_rx_buffer_size;
    volatile uint8_t slave_rx_count;
#endif
#if DEVICE_I2C_ASYNCH
    uint32_t address;
    uint8_t stop;
    uint8_t available_events;
#endif
};

struct dac_s {
    DACName dac;
    PinName pin;
    uint32_t channel;
    DAC_HandleTypeDef handle;
};

struct analogin_s {
    ADC_HandleTypeDef handle;
    PinName pin;
    uint8_t channel;
};

#if DEVICE_CAN
struct can_s {
    CAN_HandleTypeDef CanHandle;
    int index;
    int hz;
};
#endif

#if DEVICE_FLASH
struct flash_s {
    /*  nothing to be stored for now */
    uint32_t dummy;
};
#endif

#define HAL_CRC_IS_SUPPORTED(polynomial, width) ((width) == 7 || (width) == 8 || (width) == 16 || (width) == 32)

struct gpio_irq_s {
    IRQn_Type irq_n;
    uint32_t irq_index;
    uint32_t event;
    PinName pin;
};

struct port_s {
    PortName port;
    uint32_t mask;
    PinDirection direction;
    __IO uint32_t *reg_in;
    __IO uint32_t *reg_out;
};

#ifdef __cplusplus
}
#endif

#endif
