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

#ifndef TARGETS_TARGET_SAMSUNG_TARGET_SIDK_S5JS100_S5JS100_I2C_H_
#define TARGETS_TARGET_SAMSUNG_TARGET_SIDK_S5JS100_S5JS100_I2C_H_

#include "objects.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* I2C Register Offsets *****************************************************/
#define HSI2C_MASTER_TX_SLAVE_RX    1
#define HSI2C_SLAVE_TX_MASTER_RX    2
#define HSI2C_MASTER_TX_MASTER_RX   4

#define HSI2C_MASTER_BUFFER_BASE    0x44000000
#define HSI2C_SLAVE_BUFFER_BASE     0x44800000

#define HSI2C_MASTER_ID             0x9 /*  Master ID 00001xxx */
#define HSI2C_SLAVE_ADDRESS         0x50

/* Register Map *************************************************************/
#define CTL               0x0000
#define FIFO_CTL          0x0004
#define TRAILING_CTL      0x0008
#define INT_EN            0x0020
#define INT_STAT          0x0024
#define FIFO_STAT         0x0030
#define TXDATA            0x0034
#define RXDATA            0x0038
#define I2C_CONF          0x0040
#define I2C_AUTO_CONF     0x0044
#define I2C_TIMEOUT       0x0048
#define I2C_MANUAL_CMD    0x004C
#define I2C_TRANS_STATUS  0x0050
#define I2C_TIMING_HS1    0x0054
#define I2C_TIMING_HS2    0x0058
#define I2C_TIMING_HS3    0x005C
#define I2C_TIMING_FS1    0x0060
#define I2C_TIMING_FS2    0x0064
#define I2C_TIMING_FS3    0x0068
#define I2C_TIMING_SLA    0x006C
#define I2C_ADDR          0x0070

#define I2C_START         (1 << 0)
#define I2C_RESTART       (1 << 1)
#define I2C_STOP          (1 << 2)
#define I2C_SEND_DATA     (1 << 3)
#define I2C_READ_DATA     (1 << 4)
#define I2C_RX_ACK        (1 << 5)
#define I2C_TX_MASK       (0xFF << 24)
#define I2C_RX_MASK       (0xFF << 16)

#define I2C_SPEED_400KHZ  400000

#define HSI2C_INT_SLAVE_ADDR_MATCH        (1 << 15)
#define HSI2C_INT_XFER_DONE_MANUAL        (1 << 13)
#define HSI2C_INT_XFER_DONE_NOACK_MANUAL  (1 << 12)
#define HSI2C_INT_NO_DEV_AUTO             (1 << 10)
#define HSI2C_INT_NO_DEV_ACK_AUTO         (1 << 9)
#define HSI2C_INT_TRANS_ABORT_AUTO        (1 << 8)
#define HSI2C_INT_TRANSFER_DONE_AUTO      (1 << 7)
#define HSI2C_INT_RX_OVERRUN              (1 << 5)
#define HSI2C_INT_TX_UNDERRUN             (1 << 2)
#define HSI2C_INT_RX_ALMOST_FULL          (1 << 1)
#define HSI2C_INT_TX_ALMOST_EMPTY         (1 << 0)

#define HSI2C_INT_ALL  0xFFFF

#define SLAVE_TX_MODE  (1 << 19)

#define STOP_COND      (1 << 18)

#define I2C_MASTER      0
#define I2C_SLAVE_MODE  1
#define I2C_POLLING     0
#define I2C_INTERRUPT   1

/* #define I2C_INTERRUPT   1 */

/* I2C address calculation.  Convert 7- and 10-bit address to 8-bit and
 * 16-bit read/write address
 */
#define I2C_READBIT          0x01

/* Convert 7- to 8-bit address */
#define I2C_ADDR8(a)         ((a) << 1)
#define I2C_WRITEADDR8(a)    I2C_ADDR8(a)
#define I2C_READADDR8(a)     (I2C_ADDR8(a) | I2C_READBIT)

/* Convert 10- to 16-bit address */
#define I2C_ADDR10H(a)       (0xf0 | (((a) >> 7) & 0x06))
#define I2C_ADDR10L(a)       ((a) & 0xff)

#define I2C_WRITEADDR10H(a)  I2C_ADDR10H(a)
#define I2C_WRITEADDR10L(a)  I2C_ADDR10L(a)

#define I2C_READADDR10H(a)   (I2C_ADDR10H(a) | I2C_READBIT)
#define I2C_READADDR10L(a)   I2C_ADDR10L(a)

/* Bit definitions for the flags field in struct i2c_msg_s */

#define I2C_M_READ           0x0001 /**< Read data, from slave to master */
#define I2C_M_TEN            0x0002 /**< Ten bit address */
#define I2C_M_NORESTART      0x0080 /**< Message should not begin with
                                     * (re-)start of transfer */
#define I2C_SLAVE            0x0703 /**< Use this slave address */
#define I2C_SLAVE_FORCE      0x0706 /**< Use this slave address, even if it
                                     * is already in use by a driver! */
#define I2C_TENBIT           0x0704 /**< 0 for 7 bit addrs, != 0 for 10 bit */
#define I2C_RDWR             0x0707 /**< Combined R/W transfer (one STOP only) */

#define I2C_FREQUENCY    0X801

#define I2C_M_IGNORE_NAK  0x1000    /**< if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART   0x4000  /**< if I2C_FUNC_NOSTART */


static int hsi2c_start(struct i2c_s *obj);
static int hsi2c_stop(struct i2c_s *obj);
static int s5js100_i2c_uninitialize(struct i2c_s *obj);
static void s5js100_i2c0_interrupt(void);
unsigned int s5js100_i2c_setclock(struct i2c_s *obj, unsigned int frequency);
int s5js100_i2c_setownaddress(struct i2c_s *obj, int addr, int nbits);
int s5js100_i2c_read(struct i2c_s *obj, uint8_t *buffer, int buflen, int stop);
int s5js100_i2c_write(struct i2c_s *obj, const uint8_t *buffer, int buflen, int stop);
int s5js100_i2c_byte_read(struct i2c_s *obj, int stop);
int s5js100_i2c_byte_write(struct i2c_s *obj, const uint8_t buffer);
void up_i2cinitialize(struct i2c_s *obj);

#endif /* TARGETS_TARGET_SAMSUNG_TARGET_SIDK_S5JS100_S5JS100_I2C_H_ */
