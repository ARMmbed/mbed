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

#include <string.h>
#include "mbed_error.h"
#include "s5js100_i2c.h"
#include "cmsis.h"
#include "s5js100_pinconfig.h"

#define S5JS100_DEFAULT_I2CXFER_CLOCK   (400 * 1000)    /* 100Khz */
#define S5JS100_DEFAULT_I2CSLAVE_ADDR   0x22
#define S5JS100_DEFAULT_I2C_TIMEOUT     20000
#define HSI2C_INT_XFER_DONE (HSI2C_INT_XFER_DONE_NOACK_MANUAL | HSI2C_INT_XFER_DONE_MANUAL)

/* I2C Interface */
static const struct i2c_ops_s s5js100_i2c_ops = {
    .setfrequency = s5js100_i2c_setclock,
    .setaddress = s5js100_i2c_setownaddress,
    .write = s5js100_i2c_write,
    .read = s5js100_i2c_read,
    .bytewrite = s5js100_i2c_byte_write,
    .byteread = s5js100_i2c_byte_read,
#ifdef CONFIG_I2C_TRANSFER
    .transfer = s5js100_i2c_transfer,
#endif
#ifdef DEVICE_I2CSLAVE
    .setownaddress = NULL,
    .registercallback = NULL,
#endif
    .start = hsi2c_start,
    .stop = hsi2c_stop,
    .free = s5js100_i2c_uninitialize,
};

static const struct i2c_config_s s5js100_i2c_config = {
    .base = S5JS100_I2C,
    .scl_pin = GPIO_I2C0_SCL,
    .sda_pin = GPIO_I2C0_SDA,
    .isr = s5js100_i2c0_interrupt,
    .irq = S5JS100_IRQ_I2C,
    .devno = 0,
};

static struct i2c_s s5js100_i2c_priv = {
    .xfer_speed = S5JS100_DEFAULT_I2CXFER_CLOCK,
    .master = I2C_MASTER,
    .mode = I2C_POLLING,
    .slave_addr = S5JS100_DEFAULT_I2CSLAVE_ADDR,
    .addrlen = 7,
//      .freqid = d1_acpu_l3,
    .timeout = S5JS100_DEFAULT_I2C_TIMEOUT,
//      .name = "s5js100_i2c0",
    .initialized = 0,
    .retries = 3,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void hw_delay_us(unsigned int Value)
{
    volatile unsigned i, j;

    for (i = 0; i < (Value * 2); i++)
        for (j = 0; j < 100; j++)
            ;
}

static void hsi2c_set_hs_timing(unsigned int base, unsigned int nClkDiv,
                                unsigned int tSTART_SU, unsigned int tSTART_HD, unsigned int tSTOP_SU,
                                unsigned int tSDA_SU, unsigned int tDATA_SU, unsigned int tDATA_HD,
                                unsigned int tSCL_L, unsigned int tSCL_H, unsigned int tSR_RELEASE)
{
    tSTART_SU &= 0xFF;
    tSTART_HD &= 0xFF;
    tSTOP_SU &= 0xFF;
    putreg32(((tSTART_SU << 24) | (tSTART_HD << 16) | (tSTOP_SU << 8)),
             base + I2C_TIMING_HS1);

    tDATA_SU &= 0xFF;
    tSCL_L &= 0xFF;
    tSCL_H &= 0xFF;
    putreg32(((tDATA_SU << 24) | (tSCL_L << 8) | (tSCL_H << 0)),
             base + I2C_TIMING_HS2);

    nClkDiv &= 0xFF;
    tSR_RELEASE &= 0xFF;
    putreg32(((nClkDiv << 16) | (tSR_RELEASE << 0)), base + I2C_TIMING_HS3);

    tDATA_HD &= 0xFFFF;
    putreg32(tDATA_HD, base + I2C_TIMING_SLA);

}

static void hsi2c_set_fs_timing(unsigned int base, unsigned int nClkDiv,
                                unsigned int tSTART_SU, unsigned int tSTART_HD, unsigned int tSTOP_SU,
                                unsigned int tDATA_SU, unsigned int tDATA_HD, unsigned int tSCL_L,
                                unsigned int tSCL_H, unsigned int tSR_RELEASE)
{
    tSTART_SU &= 0xFF;
    tSTART_HD &= 0xFF;
    tSTOP_SU &= 0xFF;
    putreg32(((tSTART_SU << 24) | (tSTART_HD << 16) | (tSTOP_SU << 8)),
             base + I2C_TIMING_FS1);

    tDATA_SU &= 0xFF;
    tSCL_L &= 0xFF;
    tSCL_H &= 0xFF;
    putreg32(((tDATA_SU << 24) | (tSCL_L << 8) | (tSCL_H << 0)),
             base + I2C_TIMING_FS2);

    nClkDiv &= 0xFF;
    tSR_RELEASE &= 0xFF;
    putreg32(((nClkDiv << 16) | (tSR_RELEASE << 0)), base + I2C_TIMING_FS3);

    tDATA_HD &= 0xFFFF;
    putreg32(tDATA_HD, base + I2C_TIMING_SLA);
}

static void hsi2c_calculate_timing(unsigned int base, unsigned int nPclk,
                                   unsigned int nOpClk)
{
    unsigned int reg;
    unsigned int nClkDiv;
    unsigned int tFTL_CYCLE_SCL;
    int i;
    int uTemp0;
    int uTemp1;
    int uTemp2 = 0;

    reg = getreg32(base + I2C_CONF);
    reg &= ~(0x7 << 13);
    putreg32(reg, base + I2C_CONF);

    reg = getreg32(base + I2C_CONF);
    reg &= ~(0x7 << 16);
    putreg32(reg, base + I2C_CONF);

    tFTL_CYCLE_SCL = (getreg32(base + I2C_CONF) >> 16) & 0x7;

    uTemp0 = (float)(nPclk / nOpClk) - (tFTL_CYCLE_SCL + 3) * 2;

    for (i = 0; i < 256; i++) {
        uTemp1 = (uTemp0 + ((tFTL_CYCLE_SCL + 3) % (i + 1)) * 2) / (i + 1);
        if (uTemp1 < 512) { /*  TSCL_L/H max is 512 / 2 */
            uTemp2 = uTemp1 - 2;
            break;
        }
    }

    unsigned int tSCL_H;
    nClkDiv = i;
    if (nOpClk > I2C_SPEED_400KHZ) {
        tSCL_H = ((uTemp2 + 10) / 3) - 5;
    } else {
        tSCL_H = uTemp2 / 2;
    }

    unsigned int tSCL_L = uTemp2 - tSCL_H;
    unsigned int tSTART_SU = tSCL_L;
    unsigned int tSTART_HD = tSCL_L;
    unsigned int tSTOP_SU = tSCL_L;
    unsigned int tSDA_SU = tSCL_L;
    unsigned int tDATA_SU = tSCL_L;
    unsigned int tDATA_HD = tSCL_L / 2;
    unsigned int tSR_RELEASE = uTemp2;

    if (nOpClk > I2C_SPEED_400KHZ) {
        /* 400Khz setting for Extended ID */
        hsi2c_set_fs_timing(base, 1, 38, 38, 38, 19, 19, 38, 38, 76);
        hsi2c_set_hs_timing(base, nClkDiv, tSTART_SU, tSTART_HD, tSTOP_SU,
                            tSDA_SU, tDATA_SU, tDATA_HD, tSCL_L, tSCL_H, tSR_RELEASE);
    } else {
        hsi2c_set_fs_timing(base, nClkDiv, tSTART_SU, tSTART_HD, tSTOP_SU,
                            tDATA_SU, tDATA_HD, tSCL_L, tSCL_H, tSR_RELEASE);
    }
}

static void hsi2c_conf(unsigned int base, unsigned int nOpClk)
{
    unsigned int val;

    val = getreg32(base + I2C_CONF);
    val &= ~(3 << 30);
    if (nOpClk > I2C_SPEED_400KHZ) {
        val |= (1 << 29);
    } else {
        val &= ~(1 << 29);
    }
    putreg32(val, base + I2C_CONF);
}

static void hsi2c_enable_int(unsigned int base, unsigned int bit)
{
    unsigned int val;
    val = getreg32(base + INT_EN);
    val |= bit;
    putreg32(val, base + INT_EN);
}

static void hsi2c_disable_int(unsigned int base, unsigned int bit)
{
    unsigned int val;

    val = getreg32(base + INT_EN);
    val &= ~bit;
    putreg32(val, base + INT_EN);
}

static void hsi2c_clear_int(unsigned int base, unsigned int bit)
{
    putreg32(bit, base + INT_STAT);
}

static unsigned int hsi2c_read_int_status(unsigned int base)
{
    return getreg32(base + INT_STAT) & getreg32(base + INT_EN);
}

static void hsi2c_set_slave_addr(unsigned int base, unsigned short addr,
                                 unsigned int is_master)
{
    unsigned int val;

    addr &= 0x3FF;

    val = getreg32(base + I2C_ADDR);

    if (is_master == 0) {
        val &= ~0x3ff;
        val |= addr;
    } else {
        val &= ~(0x3FF << 10);
        val |= (addr << 10);
    }
    putreg32(val, base + I2C_ADDR);
}

static int hsi2c_manual_fast_init(struct i2c_s *obj)
{
    unsigned int val;
    unsigned int base = obj->config->base;

    hsi2c_conf(base, obj->xfer_speed);
    hsi2c_calculate_timing(base, obj->clock, obj->xfer_speed);
    hsi2c_enable_int(base, HSI2C_INT_XFER_DONE_MANUAL | HSI2C_INT_XFER_DONE_NOACK_MANUAL);

    obj->initialized = 1;

    val = getreg32(base + I2C_CONF);
    val &= ~((1 << 31) | (1 << 29));
    putreg32(val, base + I2C_CONF);

    hsi2c_enable_int(base, HSI2C_INT_XFER_DONE);

    return 0;
}

static int hsi2c_wait_xfer_done(struct i2c_s *obj)
{
    int val;
    int timeout = obj->timeout;
    unsigned int base = obj->config->base;

    while (timeout-- > 0) {
        val = getreg32(base + INT_STAT) & HSI2C_INT_XFER_DONE_MANUAL;
        if (val) {
            putreg32(val, base + INT_STAT);
            return (val == HSI2C_INT_XFER_DONE_MANUAL);
        }
    }

    return -ETIMEDOUT;
}

static int hsi2c_wait_xfer_noack(struct i2c_s *obj)
{
    int val;
    int timeout = obj->timeout;
    unsigned int base = obj->config->base;

    while (timeout-- > 0) {
        val = getreg32(base + INT_STAT) & HSI2C_INT_XFER_DONE_NOACK_MANUAL;
        if (val) {
            putreg32(val, base + INT_STAT);
            return (val == HSI2C_INT_XFER_DONE_NOACK_MANUAL);
        }
    }

    return -ETIMEDOUT;
}

static int hsi2c_start(struct i2c_s *obj)
{
    putreg32(0x88, obj->config->base + CTL);

    putreg32(I2C_START, obj->config->base + I2C_MANUAL_CMD);

    return hsi2c_wait_xfer_done(obj);
}

static int hsi2c_stop(struct i2c_s *obj)
{
    putreg32(I2C_STOP, obj->config->base + I2C_MANUAL_CMD);

    return hsi2c_wait_xfer_done(obj);
}

static void hsi2c_repstart(struct i2c_s *obj)
{
    putreg32(I2C_RESTART, obj->config->base + I2C_MANUAL_CMD);
}

static int hsi2c_outb(struct i2c_s *obj, unsigned char data)
{
    unsigned int val;
    int ret;

    val = ((unsigned int) data) << 24 | I2C_SEND_DATA;
    putreg32(val, obj->config->base + I2C_MANUAL_CMD);

    ret = hsi2c_wait_xfer_done(obj);

    return ret;
}

static int hsi2c_inb(struct i2c_s *obj, bool is_ack)
{
    unsigned int val = I2C_READ_DATA;
    unsigned char data;
    int ret;
    unsigned int base = obj->config->base;

    /* Looks awkward, but if I2C_RX_ACK is set, ACK is NOT generated */
    if (!is_ack) {
        val |= I2C_RX_ACK;
    }
    putreg32(val, base + I2C_MANUAL_CMD);
    ret = hsi2c_wait_xfer_done(obj);
    if (ret < 0) {
        return ret; /* timeout */
    }

    data = (getreg32(base + I2C_MANUAL_CMD) >> 16) & 0xff;

    return data;
}

static int sendbytes(struct i2c_s *obj, struct i2c_msg_s *msg)
{
    uint8_t *p = msg->buffer;
    int count = msg->length;
    int nak_ok = msg->flags & I2C_M_IGNORE_NAK;
    int wrcount = 0, ret;

    while (count > 0) {
        ret = hsi2c_outb(obj, *p);
        if ((ret == 1) || ((ret == 0) && nak_ok)) {
            count--;
            p++;
            wrcount++;
        } else if (ret == 0) {
            /* NAK from the slave */
            return -EIO;
        } else {
            /* Timeout */
            return ret;
        }
    }

    return wrcount;
}

static int readbytes(struct i2c_s *obj, struct i2c_msg_s *msg)
{
    int val;
    int rdcount = 0;
    unsigned char *p = msg->buffer;
    int count = msg->length;

    while (count > 0) {
        val = hsi2c_inb(obj, (count > 1));
        if (val < 0) {
            break;
        }

        *p++ = val;
        rdcount++;
        count--;
    }

    return rdcount;
}

static int try_address(struct i2c_s *obj, unsigned char addr, int retries)
{
    int i, ret = 0;

    for (i = 0; i <= retries; i++) {
        ret = hsi2c_outb(obj, addr);
        if (ret == 1 || i == retries) {
            break;
        }
        hsi2c_stop(obj);
//      up_udelay(obj->timeout / 2);
        hw_delay_us(obj->timeout / 2);
        hsi2c_start(obj);
    }

    return ret;
}

static int do_address(struct i2c_s *obj, struct i2c_msg_s *msg)
{
    unsigned short flags = msg->flags;
    unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
    unsigned char addr;
    int ret;
    int retries;

    retries = nak_ok ? 0 : obj->retries;

    if (flags & I2C_M_TEN) {
        /* a 10-bit address in manual mode */
        addr = 0xf0 | ((msg->addr >> 7) & 0x06);

        ret = try_address(obj, addr, retries);
        if ((ret != 1) && !nak_ok) {
            return -ENXIO;
        }

        /* the remaining 8 bit address */
        ret = hsi2c_outb(obj, msg->addr & 0xff);
        if ((ret != 1) && !nak_ok) {
            return -ENXIO;
        }

        if (flags & I2C_M_READ) {
            hsi2c_repstart(obj);
            hsi2c_wait_xfer_done(obj);
            addr |= 0x1;
            ret = try_address(obj, addr, retries);
            if ((ret != 1) && !nak_ok) {
                return -EIO;
            }
        }
    } else {
        /* 7-bit address*/
        addr = msg->addr;
        if (flags & I2C_M_READ) {
            addr |= 0x1;
        }
        ret = try_address(obj, addr, retries);
        if ((ret != 1) && !nak_ok) {
            return -ENXIO;
        }
    }

    return 0;
}

static void hsi2c_run_auto_mode(unsigned int base, int on)
{
    unsigned int val = getreg32(base + I2C_AUTO_CONF);

    if (on) {
        val |= (1 << 31);
    } else {
        val &= ~(1 << 31);
    }

    putreg32(val, base + I2C_AUTO_CONF);
}

static void hsi2c_tx_fifo_reset(unsigned int base, int resetb)
{
    unsigned int val = getreg32(base + FIFO_CTL);

    if (resetb) {
        val |= (1 << 3);
    } else {
        val &= ~(1 << 3);
    }

    putreg32(val, base + FIFO_CTL);
}

static void hsi2c_set_auto_config(unsigned int base, unsigned int stop,
                                  unsigned int tx, unsigned int len)
{
    unsigned int val = getreg32(base + I2C_AUTO_CONF);

    if (stop) {
        val |= (1 << 17);
    } else {
        val &= ~(1 << 17);
    }
    if (tx) {
        val &= ~(1 << 16);
    } else {
        val |= (1 << 16);
    }

    val &= ~0xFFFF;
    val |= len;
    putreg32(val, base + I2C_AUTO_CONF);
}

static void hsi2c_set_trans_mode(unsigned int base, unsigned int master,
                                 unsigned int tx)
{
    unsigned int val = getreg32(base + CTL);

    val |= (1 << 0); /* ctrl 0 bit write 1 */

    if (master) {
        val |= (1 << 3);
    } else {
        val &= ~(1 << 3);
    }

    val &= ~(3 << 6);
    if (tx) {
        val |= (1 << 7);
    } else {
        val |= (1 << 6);
    }

    putreg32(val, base + CTL);
}

static void hsi2c_set_hwacg_mode(unsigned int base, unsigned int slave)
{
    unsigned int val = getreg32(base + CTL);

    val &= ~(0x3 << 24);

    if (slave) {
        val |= (0x1 << 24);
    } else {
        val &= ~(0x1 << 24);
    }

    putreg32(val, base + CTL);
}

static int hsi2c_master_handler(void *args)
{
    struct i2c_s *priv = args;
    unsigned int int_status;
    int off = priv->master_test_data->cur_msg, buf_off =
                  priv->master_test_data->buf_count;
    int trans_count = priv->master_test_data->num;
    struct i2c_msg_s *msg = &priv->master_test_data->msg[off];
    unsigned int base = priv->config->base;

    int_status = hsi2c_read_int_status(base);
    if (int_status & HSI2C_INT_TX_ALMOST_EMPTY) {
        if (buf_off < msg->length) {
            putreg32(msg->buffer[buf_off], base + TXDATA);
            priv->master_test_data->buf_count++;
        } else {
            hsi2c_disable_int(base, HSI2C_INT_TX_ALMOST_EMPTY);
        }
        hsi2c_clear_int(base, HSI2C_INT_TX_ALMOST_EMPTY);
    }
    if (int_status & HSI2C_INT_RX_ALMOST_FULL) {
        msg->buffer[buf_off] = (unsigned char) getreg32(base + RXDATA);
        priv->master_test_data->buf_count++;
        hsi2c_clear_int(base, HSI2C_INT_RX_ALMOST_FULL);
    }
    if (int_status & HSI2C_INT_TRANSFER_DONE_AUTO) {
        if ((off + 1) == trans_count) {
            /* complete(&priv->master_test_data->done); ?? */
        } else {
            off = ++priv->master_test_data->cur_msg;
            priv->master_test_data->buf_count = 0;
            msg++;

            if ((off + 1) == trans_count) {
                if (msg->flags & I2C_M_READ) {
                    hsi2c_set_auto_config(base, 1, 0, msg->length);
                } else {
                    hsi2c_set_auto_config(base, 1, 1, msg->length);
                }
            } else {
                if (msg->flags & I2C_M_READ) {
                    hsi2c_set_auto_config(base, 0, 0, msg->length);
                } else {
                    hsi2c_set_auto_config(base, 0, 1, msg->length);
                }
            }

            if (msg->flags & I2C_M_READ) {
                hsi2c_set_trans_mode(base, 1, 0);
            } else {
                hsi2c_set_trans_mode(base, 1, 1);
            }

            hsi2c_run_auto_mode(base, 1);

        }
        hsi2c_clear_int(base, HSI2C_INT_TRANSFER_DONE_AUTO);
    }

    return 0;
}

static int hsi2c_slave_handler(void *args)
{
    struct i2c_s *priv = args;
    unsigned int int_status;
    unsigned int val;
    unsigned int base = priv->config->base;

    int_status = hsi2c_read_int_status(base);
    if (int_status & HSI2C_INT_SLAVE_ADDR_MATCH) {
        val = getreg32(base + I2C_TRANS_STATUS);
        if (val & SLAVE_TX_MODE) {
            priv->slave_test_data->status = SLAVE_SET_DATA;
            hsi2c_enable_int(base, HSI2C_INT_TX_ALMOST_EMPTY);
        } else {
            priv->slave_test_data->status = SLAVE_GET_REG;
        }

        hsi2c_clear_int(base, HSI2C_INT_SLAVE_ADDR_MATCH);
    } else if (int_status & HSI2C_INT_RX_ALMOST_FULL) {
        if (priv->slave_test_data->status == SLAVE_GET_REG) {
            priv->slave_test_data->current_reg = getreg32(base + RXDATA);
            priv->slave_test_data->status = SLAVE_GET_DATA;
        } else if (priv->slave_test_data->status == SLAVE_GET_DATA) {
            priv->slave_test_data->data[priv->slave_test_data->current_reg] =
                getreg32(base + RXDATA);
            priv->slave_test_data->current_reg++;
        }
        hsi2c_clear_int(base, HSI2C_INT_RX_ALMOST_FULL);
    } else if (int_status & HSI2C_INT_TX_ALMOST_EMPTY) {
        if (getreg32(base + I2C_TRANS_STATUS) & STOP_COND) {
            hsi2c_disable_int(base, HSI2C_INT_TX_ALMOST_EMPTY);
        } else {
            if (priv->slave_test_data->status == SLAVE_SET_DATA) {
                putreg32(
                    priv->slave_test_data->data[priv->slave_test_data->current_reg],
                    base + TXDATA);
                priv->slave_test_data->current_reg++;
            }
        }
        hsi2c_clear_int(base, HSI2C_INT_TX_ALMOST_EMPTY);
    }
    if (int_status & HSI2C_INT_RX_OVERRUN) {
        /* for Rx_overrun test */
        hsi2c_clear_int(base, HSI2C_INT_RX_OVERRUN);
    }
    if (int_status & HSI2C_INT_TX_UNDERRUN) {
        /* for Tx_underrun test */
        hsi2c_clear_int(base, HSI2C_INT_RX_OVERRUN);
    }

    return 0;
}

static void hsi2c_set_auto_mode(unsigned int base)
{
    unsigned int val;

    val = getreg32(base + I2C_CONF);
    val |= (1 << 31);
    putreg32(val, base + I2C_CONF);
}

static void hsi2c_set_fifo_level(unsigned int base)
{
    putreg32(0x10013, base + FIFO_CTL);
}

static void hsi2c_master_setup(struct i2c_s *obj, unsigned int mode,
                               unsigned int speed, unsigned int slave_addr)
{
    if (obj->mode == I2C_POLLING) {
        obj->xfer_speed = speed;
        hsi2c_calculate_timing(obj->config->base, obj->clock, speed);
    }
#ifdef CONFIG_S5JS100_I2C_INTERRUPT_MODE
    else if (priv->mode == I2C_INTERRUPT) {
        priv->master_test_data = (struct master_data *)malloc(sizeof(struct master_data));
        /* complete_init(&priv->master_test_data->done); */
        hsi2c_set_trans_mode(obj->config->base, 1, 1); /* set master mode */
        hsi2c_conf(obj->config->base, speed);
        hsi2c_calculate_timing(obj->config->base, obj->clock, speed);
        hsi2c_set_slave_addr(obj->config->base, slave_addr, 1);
        hsi2c_set_auto_mode(obj->config->base);
        hsi2c_set_fifo_level(obj->config->base);
    }
#endif
}

static void hsi2c_slave_setup(struct i2c_s *obj, unsigned int mode, unsigned int speed,
                              unsigned int slave_addr)
{
//  priv->slave_test_data = (struct slave_data *)malloc(sizeof(struct slave_data));

    /* slave mode is only support slave mode */
    hsi2c_set_trans_mode(obj->config->base, 0, 0); /* set slave mode */

    /*set hwacg for slave mode */
    hsi2c_set_hwacg_mode(obj->config->base, 1);

    hsi2c_conf(obj->config->base, speed);
    hsi2c_calculate_timing(obj->config->base, obj->clock, speed);
    hsi2c_set_slave_addr(obj->config->base, slave_addr, 0);

    hsi2c_disable_int(obj->config->base, HSI2C_INT_ALL);
    hsi2c_enable_int(obj->config->base,
                     HSI2C_INT_SLAVE_ADDR_MATCH | HSI2C_INT_RX_ALMOST_FULL);

    hsi2c_set_fifo_level(obj->config->base);
    hsi2c_set_auto_config(obj->config->base, 0, 0, 0);

    hsi2c_set_trans_mode(obj->config->base, 0, 0);

#ifdef CONFIG_S5JS100_I2C_INTERRUPT_MODE
    if ((priv->master == I2C_SLAVE_MODE) || (priv->mode == I2C_INTERRUPT)) {
        irq_attach(obj->config->irq, obj->config->isr, NULL);
    }
#endif
}

static void hsi2c_master_cleanup(struct i2c_s *obj)
{
#ifdef CONFIG_S5JS100_I2C_INTERRUPT_MODE
    if (obj->mode == I2C_INTERRUPT) {
        free(obj->master_test_data);
        obj->master_test_data = NULL;
        irq_detach(obj->config->irq);
    }
#endif
}

static void hsi2c_slave_cleanup(struct i2c_s *obj)
{
    /*  FreeHwMemP(priv->slave_test_data); */
//  free(obj->slave_test_data);
//  obj->slave_test_data = NULL;
#ifdef CONFIG_S5JS100_I2C_INTERRUPT_MODE
    if ((obj->master == I2C_SLAVE_MODE) || (obj->mode = I2C_INTERRUPT)) {
        irq_detach(obj->config->irq);
    }
#endif
}

static int hsi2c_setup(struct i2c_s *obj, unsigned int master, unsigned int mode,
                       unsigned int speed, unsigned int slave_addr)
{
    obj->master = master;
    obj->mode = mode;
    obj->xfer_speed = speed;
    obj->slave_addr = slave_addr;

    hsi2c_manual_fast_init(obj);

    if (master == I2C_MASTER) {
        hsi2c_master_setup(obj, mode, speed, slave_addr);
    } else if (master == I2C_SLAVE_MODE) {
        hsi2c_slave_setup(obj, mode, speed, slave_addr);
    }

    return 0;
}

static int hsi2c_cleanup(struct i2c_s *obj)
{
    if (obj->master == I2C_MASTER) {
        hsi2c_master_cleanup(obj);
    } else if (obj->master == I2C_SLAVE_MODE) {
        hsi2c_slave_cleanup(obj);
    }

    obj->master = I2C_MASTER;
    obj->mode = I2C_POLLING;
    obj->xfer_speed = I2C_SPEED_400KHZ;

    putreg32(0x80000000, obj->config->base + CTL);
//  up_udelay(10);
    hw_delay_us(10);

    putreg32(0x0, obj->config->base + CTL);
//  up_udelay(100);
    hw_delay_us(100);

    return 0;
}

static void s5js100_i2c0_interrupt(void)
{
    struct i2c_s *priv;

    /* Read the masked interrupt status */
    priv = &s5js100_i2c_priv;

    /* Let the common interrupt handler do the rest of the work */
    hsi2c_master_handler(priv);
}

static int s5js100_i2c_initialize(struct i2c_s *obj, unsigned int frequency)
{

    const struct i2c_config_s *config = obj->config;

    s5js100_configgpio(config->scl_pin);
    s5js100_configgpio(config->sda_pin);

    /* Enable the I2C master block */
    /* Configure the the initial I2C clock frequency. */
    obj->xfer_speed = frequency;
    hsi2c_setup(obj, obj->master, obj->mode, obj->xfer_speed, obj->slave_addr);

    /*
     * Attach interrupt handlers and enable interrupts at the NVIC (still
     * disabled at the source).
     */
#ifdef CONFIG_S5JS100_I2C_INTERRUPT_MODE
    if ((obj->master == I2C_SLAVE_MODE) || (obj->mode == I2C_INTERRUPT)) {
        irq_attach(config->irq, config->isr, NULL);
        up_enable_irq(config->irq);
    }
#endif

    return 0;
}

/*static*/ int s5js100_i2c_uninitialize(struct i2c_s *obj)
{
    /* Disable I2C */
    hsi2c_cleanup(obj);

    /* Unconfigure GPIO pins */
    s5js100_unconfiggpio(obj->config->scl_pin);
    s5js100_unconfiggpio(obj->config->sda_pin);

    /* Disable and detach interrupts */
#ifdef CONFIG_S5JS100_I2C_INTERRUPT_MODE
    if ((obj->master == I2C_SLAVE_MODE) || (obj->mode == I2C_INTERRUPT)) {
        up_disable_irq(obj->config->irq);
        irq_detach(obj->config->irq);
    }
#endif

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
/**
 * @brief    i2c clock setting
 * @param    struct i2c_s *obj : pointer to i2c_dev_s
 * @param    unsigned int frequency : ex ) 100000 == 100khz
 * @return   void
 * @note
 */
unsigned int s5js100_i2c_setclock(struct i2c_s *obj, unsigned int frequency)
{
    /* Has the I2C bus frequency changed? */
    if (frequency != obj->xfer_speed) {
        /*
         * Calculate the clock divider that results in the highest frequency
         * that is than or equal to the desired speed.
         */
        if (obj->mode != I2C_POLLING) {
            hsi2c_conf(obj->config->base, frequency);
        }

        hsi2c_calculate_timing(obj->config->base, obj->clock, frequency);
    }

    /* Save the new I2C frequency */
    obj->xfer_speed = frequency;
    return 0;
}

/**
 * @brief    Setting Slave Address for slave mode
 * @param    struct i2c_s *obj : pointer to i2c_dev_s
 * @param    int addr : Slave address
 * @param    int nbits : 0==7bit
 *          1==10bit
 * @return    = 0
 * @note
 */
int s5js100_i2c_setownaddress(struct i2c_s *obj, int addr, int nbits)
{
    obj->slave_addr = addr;
//  if (nbits == 1) {
//      obj->msgv->flags |= I2C_M_TEN;
//  }
    hsi2c_set_slave_addr(obj->config->base, addr, 0);
    return 0;
}
/**
 * @brief    Generic I2C transfer function
 * @param    struct i2c_master_s *dev : structure visible to the I2C client
 * @param    struct i2c_msg_s *msgv : I2C transaction segment
 * @param    int msgc :  length
 * @return   int : ==0 :0
 * @note
 */
int s5js100_i2c_transfer(struct i2c_s *obj, struct i2c_msg_s *msgv, int msgc, int stop)
{
    struct i2c_msg_s *pmsg;
    int ret = 0;
    int i;
    int nak_ok;
    int start = 1;
//  int stop = 1;
    unsigned int base = obj->config->base;

    /* TODO: initialization */
    if (start) {
        hsi2c_start(obj);
    }

    if (obj->xfer_speed > I2C_SPEED_400KHZ) {
        hsi2c_conf(base, I2C_SPEED_400KHZ);
        putreg32((0xF << 24 | I2C_SEND_DATA), base + I2C_MANUAL_CMD);
        hsi2c_wait_xfer_noack(obj);

        hsi2c_conf(base, obj->xfer_speed);
        hsi2c_repstart(obj);
        hsi2c_wait_xfer_noack(obj);
    }

    for (i = 0; i < msgc; i++) {
        pmsg = &msgv[i];
        nak_ok = pmsg->flags & I2C_M_IGNORE_NAK;
        if (!(pmsg->flags & I2C_M_NOSTART)) {
            if ((i > 0) || (start == 0)) {
                hsi2c_repstart(obj);
                hsi2c_wait_xfer_done(obj);
            }
            ret = do_address(obj, pmsg);
            if ((ret != 0) && !nak_ok) {
                goto fail;
            }
        }
        if (pmsg->flags & I2C_M_READ) {
            /* read bytes into buffer */
            ret = readbytes(obj, pmsg);
            if (ret < pmsg->length) {
                if (ret >= 0) {
                    return -EIO;
                }
                goto fail;
            }
        } else {
            /* write bytes from buffer */
            ret = sendbytes(obj, pmsg);
            if (ret < pmsg->length) {
                if (ret >= 0) {
                    ret = -EIO;
                }
                goto fail;
            }
        }
    }
    //ret = i;

fail:
//  obj->mcnt = 0;
//  obj->mptr = NULL;
    if (stop) {
        hsi2c_stop(obj);
    }

    /* Ensure that address or flags don't change meanwhile */
//  s5js100_i2c_sem_post(obj);
    return ret;
}

int s5js100_i2c_read(struct i2c_s *obj, uint8_t *buffer, int buflen, int stop)
{

    struct i2c_msg_s msg;
    unsigned int flags;

    if ((buffer == NULL) || (buflen < 0)) {
        return -EINVAL;
    }

    /* 7- or 10-bit? */
    flags = (obj->addrlen == 10) ? I2C_M_TEN : 0;

    /* Setup for the transfer */
    msg.addr = obj->slave_addr;
    msg.flags = (flags | I2C_M_READ);
    msg.buffer = buffer;
    msg.length = buflen;

    return s5js100_i2c_transfer(obj, &msg, 1, stop);
}

int s5js100_i2c_write(struct i2c_s *obj, const uint8_t *buffer, int buflen, int stop)
{
    struct i2c_msg_s msg;

    msg.addr = obj->slave_addr;
    msg.flags = (obj->addrlen == 10) ? I2C_M_TEN : 0;
    msg.buffer = (unsigned char *)buffer;
    msg.length = buflen;

    return s5js100_i2c_transfer(obj, &msg, 1, stop);
}

int s5js100_i2c_byte_read(struct i2c_s *obj, int stop)
{
    unsigned int val = I2C_READ_DATA;
    unsigned char data;
    int ret;
    unsigned int base = obj->config->base;

    /* Looks awkward, but if I2C_RX_ACK is set, ACK is NOT generated */
    if (stop) {
        val |= I2C_RX_ACK;
    }
    putreg32(val, base + I2C_MANUAL_CMD);
    if (!stop) {
        hsi2c_repstart(obj);
    }
    // hsi2c_repstart(obj);
    ret = hsi2c_wait_xfer_done(obj);
    if (ret < 0) {
        return ret; /* timeout */
    }

    data = (getreg32(base + I2C_MANUAL_CMD) >> 16) & 0xff;

    return data;
}

int s5js100_i2c_byte_write(struct i2c_s *obj, const uint8_t buffer)
{
    return hsi2c_outb(obj, buffer);
}

/**
 * @brief   Initialize one I2C bus
 * @param   int port :
 * @return  struct i2c_master_s : device structure
 * @note
 */
void up_i2cinitialize(struct i2c_s *obj)
{
    struct i2c_s *priv = NULL;

    priv = &s5js100_i2c_priv;
    cal_clk_enable(d1_acpu_l3);
    priv->clock = cal_clk_getrate(d1_acpu_l3);
    priv->config = &s5js100_i2c_config;
    /* Initialize private device structure */
    priv->ops = &s5js100_i2c_ops;

    /* Initialize the I2C hardware */
    s5js100_i2c_initialize(priv, priv->xfer_speed);

    memcpy(obj, priv, sizeof(struct i2c_s));
}
