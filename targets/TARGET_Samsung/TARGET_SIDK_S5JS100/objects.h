/****************************************************************************
 *
 * Copyright 2020 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef MBED_OBJECTS_H
#define MBED_OBJECTS_H

#include "cmsis.h"
#include "PortNames.h"
#include "PeripheralNames.h"
#include "PinNames.h"
#include "spi_def.h"
#include "serial_api.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gpio_irq_s {
    PinName pin;
    uint32_t pincfg;
};

struct port_s {
    __IO uint32_t *reg_dir;
    __IO uint32_t *reg_dirclr;
    __IO uint32_t *reg_out;
    __IO  uint32_t *reg_in;
    PortName port;
    uint32_t mask;
};

typedef enum {
    USI0_PORT_ID = 0,
    USI1_PORT_ID,
    USI_MAX_PORTS
} t_usi_ports_enum;

typedef enum {
    PL011_UART0_ID = 0,
    PL011_UART1_ID,
    PL011_UART_MAX
} t_pl011_ports_enum;

typedef enum {
    DUMMY_UART0_ID = 0,
    DUMMY_UART1_ID,
    DUMMY_UART_MAX
} t_dummy_ports_enum;


struct uart_ops_s {
    void (*serial_baud)(void *obj, int baudrate);
    void (*serial_format)(void *obj, int data_bits, SerialParity parity, int stop_bits);
    void (*serial_irq_handler)(void *obj, uart_irq_handler handler, uint32_t id);
    void (*serial_irq_set)(void *obj, SerialIrq irq, uint32_t enable);
    void (*serial_putc)(void *obj, int c);
    int (*serial_writable)(void *obj);
    int (*serial_getc)(void *obj);
    int (*serial_readable)(void *obj);
#if DEVICE_SERIAL_FC
    void (*serial_set_flow_control)(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow);
#endif
};

struct i2c_s;

struct serial_s {
    void *uart;
    int index;
    int rx_fifo_depth;
    int tx_fifo_depth;
    struct uart_ops_s ops;
};

enum slave_status {
    SLAVE_IDLE,
    SLAVE_GET_REG,
    SLAVE_GET_DATA,
    SLAVE_SET_DATA,
};

struct i2c_msg_s {
    unsigned short addr;                /**< Slave address */
    unsigned short flags;               /**< I2C flags; See I2C_M_* definitions */
#ifdef CONFIG_I2C_USERIO
    unsigned short length;          /**< The length of buffer */
    unsigned char *buffer;          /**< The Buffer for transferring message */
#else
    unsigned char *buffer;          /**< The Buffer for transferring message */
    int length;                 /**< The length of buffer */
#endif
};

struct slave_data {
    enum slave_status status;
    unsigned int current_reg;
    unsigned char data[20];
};

struct master_data {
    struct i2c_msg_s *msg;
    int num;
    int cur_msg;
    int buf_count;
    /*  struct completion done; */
};

struct i2c_ops_s {
    unsigned int (*setfrequency)(struct i2c_s *obj, unsigned int frequency);    /**< The operation for setting I2C frequency */
    int (*setaddress)(struct i2c_s *obj, int addr, int nbits);          /**< The operation for setting I2C address */
    int (*write)(struct i2c_s *obj, const unsigned char *buffer, int buflen, int stop); /**< The operation for reading data */
    int (*read)(struct i2c_s *obj, unsigned char *buffer, int buflen, int stop);        /**< The operation for wrting data */
    int (*bytewrite)(struct i2c_s *obj, unsigned char buffer);  /**< The operation for reading data */
    int (*byteread)(struct i2c_s *obj, int stop);       /**< The operation for wrting data */
#ifdef DEVICE_I2CSLAVE
    int (*setownaddress)(struct i2c_s *obj, int addr, int nbits);       /**< The operation for setting own I2C address */
    int (*registercallback)(struct i2c_s *obj, int (*callback)(void));  /**< The operation for registering callback function */
#endif
    int (*start)(struct i2c_s *obj);
    int (*stop)(struct i2c_s *obj);
    int (*free)(struct i2c_s *obj);
};

/* I2C Device hardware configuration */
struct i2c_config_s {
    unsigned long int base;                     /* I2C base address */
    unsigned int scl_pin;               /* GPIO configuration for SCL as SCL */
    unsigned int sda_pin;               /* GPIO configuration for SDA as SDA */
    void (*isr)(void);  /* Interrupt handler */
    unsigned int irq;                   /* IRQ number */
    unsigned char devno;                        /* I2Cn where n = devno */
};

/* I2C Device Private Data */
struct i2c_s {
    const struct i2c_ops_s *ops;    /* Standard I2C operations */
    const struct i2c_config_s *config;  /* Port configuration */
    int state;
    int clock;
    unsigned int xfer_speed;
    unsigned int master;
    unsigned int mode;
    unsigned int slave_addr;
    unsigned int addrlen;
    unsigned int timeout;
    unsigned int initialized;
    unsigned int retries;
    /* interrupt */
    struct slave_data *slave_test_data;
    struct master_data *master_test_data;
};

struct spi_s {
    SPI_TypeDef *spi;
    unsigned int freqid;
    unsigned int nbits;
    unsigned int base;
    unsigned int mode;
    unsigned int frequency;
    unsigned int actual;
};

struct analogin_s {
    void *adc;
    unsigned char cchannels;
    unsigned char current;
    unsigned char chanlist[4];

};

struct flash_s {
    uint32_t dummy;
};

struct trng_s {
    uint32_t dummy;
};

#if DEVICE_QSPI
struct qspi_s {
    //QSPI_HandleTypeDef handle;
    QSPIName qspi;
    PinName io0;
    PinName io1;
    PinName io2;
    PinName io3;
    PinName sclk;
    PinName ssel;
};
#endif
#if DEVICE_PWMOUT
struct pwmout_s {
//    cyhal_pwm_t hal_pwm;
    unsigned int id;
    PWMName pwm_name;
    uint32_t period_us;
    uint32_t width_us;
};
#endif

#include "gpio_object.h"

#ifdef __cplusplus
}
#endif

#endif
