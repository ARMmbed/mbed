/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
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
 *
 * Ported to NXP LPC43XX by Micromint USA <support@micromint.com>
 */
#include "mbed_assert.h"
#include <math.h>

#include "spi_api.h"
#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"

// SCU mode for SPI pins
#define SCU_PINIO_SPI       SCU_PINIO_FAST

static const PinMap PinMap_SPI_SCLK[] = {
    {P1_19, SPI_1, (SCU_PINIO_SPI | 1)},
    {P3_0,  SPI_0, (SCU_PINIO_SPI | 4)},
    {P3_3,  SPI_0, (SCU_PINIO_SPI | 2)},
    {PF_0,  SPI_0, (SCU_PINIO_SPI | 0)},
    {PF_4,  SPI_1, (SCU_PINIO_SPI | 0)},
    {NC,    NC,    0}
};

static const PinMap PinMap_SPI_MOSI[] = {
    {P0_1,  SPI_1, (SCU_PINIO_SPI | 1)},
    {P1_2,  SPI_0, (SCU_PINIO_SPI | 5)},
    {P1_4,  SPI_1, (SCU_PINIO_SPI | 5)},
    {P3_7,  SPI_0, (SCU_PINIO_SPI | 5)},
    {P3_8,  SPI_0, (SCU_PINIO_SPI | 2)},
    {P9_2,  SPI_0, (SCU_PINIO_SPI | 7)},
    {PF_3,  SPI_0, (SCU_PINIO_SPI | 2)},
    {PF_7,  SPI_1, (SCU_PINIO_SPI | 2)},
    {NC,    NC,    0}
};

static const PinMap PinMap_SPI_MISO[] = {
    {P0_0,  SPI_1, (SCU_PINIO_SPI | 1)},
    {P1_1,  SPI_0, (SCU_PINIO_SPI | 5)},
    {P1_3,  SPI_1, (SCU_PINIO_SPI | 5)},
    {P3_6,  SPI_0, (SCU_PINIO_SPI | 5)},
    {P3_7,  SPI_0, (SCU_PINIO_SPI | 2)},
    {P9_1,  SPI_0, (SCU_PINIO_SPI | 7)},
    {PF_2,  SPI_0, (SCU_PINIO_SPI | 2)},
    {PF_6,  SPI_1, (SCU_PINIO_SPI | 2)},
    {NC,    NC,    0}
};

static const PinMap PinMap_SPI_SSEL[] = {
    {P1_0,  SPI_0, (SCU_PINIO_SPI | 5)},
    {P1_5,  SPI_1, (SCU_PINIO_SPI | 5)},
    {P1_20, SPI_1, (SCU_PINIO_SPI | 2)},
    {P3_6,  SPI_0, (SCU_PINIO_SPI | 2)},
    {P3_8,  SPI_0, (SCU_PINIO_SPI | 5)},
    {P9_0,  SPI_0, (SCU_PINIO_SPI | 7)},
    {PF_1,  SPI_0, (SCU_PINIO_SPI | 2)},
    {PF_5,  SPI_1, (SCU_PINIO_SPI | 2)},
    {NC,    NC,    0}
};

static inline int ssp_disable(spi_t *obj);
static inline int ssp_enable(spi_t *obj);

void spi_init_direct(spi_t *obj, explicit_pinmap_t *explicit_pinmap)
{
    obj->spi = (LPC_SSP_T*)explicit_pinmap->peripheral;
    MBED_ASSERT((int)obj->spi != NC);

    // enable clocking
    switch ((int)obj->spi) {
        case SPI_0: LPC_CGU->BASE_CLK[CLK_BASE_SSP0] = (1 << 11) | (CLKIN_MAINPLL << 24); break;
        case SPI_1: LPC_CGU->BASE_CLK[CLK_BASE_SSP1] = (1 << 11) | (CLKIN_MAINPLL << 24); break;
    }

    // pin out the spi pins
    pin_function(explicit_pinmap->pin[0], explicit_pinmap->function[0]);
    pin_mode(explicit_pinmap->pin[0], PullNone);
    pin_function(explicit_pinmap->pin[1], explicit_pinmap->function[1]);
    pin_mode(explicit_pinmap->pin[1], PullNone);
    pin_function(explicit_pinmap->pin[2], explicit_pinmap->function[2]);
    pin_mode(explicit_pinmap->pin[2], PullNone);
    if (explicit_pinmap->pin[3] != NC) {
        pin_function(explicit_pinmap->pin[3], explicit_pinmap->function[3]);
        pin_mode(explicit_pinmap->pin[3], PullNone);
    }
}

void spi_init(spi_t *obj, PinName mosi, PinName miso, PinName sclk, PinName ssel)
{
    // determine the SPI to use
    uint32_t spi_mosi = pinmap_peripheral(mosi, PinMap_SPI_MOSI);
    uint32_t spi_miso = pinmap_peripheral(miso, PinMap_SPI_MISO);
    uint32_t spi_sclk = pinmap_peripheral(sclk, PinMap_SPI_SCLK);
    uint32_t spi_ssel = pinmap_peripheral(ssel, PinMap_SPI_SSEL);
    uint32_t spi_data = pinmap_merge(spi_mosi, spi_miso);
    uint32_t spi_cntl = pinmap_merge(spi_sclk, spi_ssel);

    int peripheral = (int)pinmap_merge(spi_data, spi_cntl);

    // pin out the spi pins
    int mosi_function = (int)pinmap_find_function(mosi, PinMap_SPI_MOSI);
    int miso_function = (int)pinmap_find_function(miso, PinMap_SPI_MISO);
    int sclk_function = (int)pinmap_find_function(sclk, PinMap_SPI_SCLK);
    int ssel_function = (int)pinmap_find_function(ssel, PinMap_SPI_SSEL);

    int pins_function[] = {mosi_function, miso_function, sclk_function, ssel_function};
    PinName pins[] = {mosi, miso, sclk, ssel};
    explicit_pinmap_t explicit_spi_pinmap = {peripheral, pins, pins_function};

    spi_init_direct(obj, &explicit_spi_pinmap);
}

void spi_free(spi_t *obj) {}

void spi_format(spi_t *obj, int bits, int mode, int slave) {
    MBED_ASSERT(((bits >= 4) && (bits <= 16)) || ((mode >= 0) && (mode <= 3)));
    ssp_disable(obj);

    int polarity = (mode & 0x2) ? 1 : 0;
    int phase = (mode & 0x1) ? 1 : 0;

    // set it up
    int DSS = bits - 1;            // DSS (data select size)
    int SPO = (polarity) ? 1 : 0;  // SPO - clock out polarity
    int SPH = (phase) ? 1 : 0;     // SPH - clock out phase

    int FRF = 0;                   // FRF (frame format) = SPI
    uint32_t tmp = obj->spi->CR0;
    tmp &= ~(0x00FF);              // Clear DSS, FRF, CPOL and CPHA [7:0]
    tmp |= DSS << 0
        | FRF << 4
        | SPO << 6
        | SPH << 7;
    obj->spi->CR0 = tmp;

    tmp = obj->spi->CR1;
    tmp &= ~(0xD);
    tmp |= 0 << 0                   // LBM - loop back mode - off
        | ((slave) ? 1 : 0) << 2   // MS - master slave mode, 1 = slave
        | 0 << 3;                  // SOD - slave output disable - na
    obj->spi->CR1 = tmp;
    ssp_enable(obj);
}

void spi_frequency(spi_t *obj, int hz) {
    ssp_disable(obj);

    uint32_t PCLK = SystemCoreClock;

    int prescaler;

    for (prescaler = 2; prescaler <= 254; prescaler += 2) {
        int prescale_hz = PCLK / prescaler;

        // calculate the divider
        int divider = floor(((float)prescale_hz / (float)hz) + 0.5f);

        // check we can support the divider
        if (divider < 256) {
            // prescaler
            obj->spi->CPSR = prescaler;

            // divider
            obj->spi->CR0 &= ~(0xFF00);  // Clear SCR: Serial clock rate [15:8]
            obj->spi->CR0 |= (divider - 1) << 8;
            ssp_enable(obj);
            return;
        }
    }
    error("Couldn't setup requested SPI frequency");
}

static inline int ssp_disable(spi_t *obj) {
    return obj->spi->CR1 &= ~(1 << 1);
}

static inline int ssp_enable(spi_t *obj) {
    return obj->spi->CR1 |= (1 << 1);
}

static inline int ssp_readable(spi_t *obj) {
    return obj->spi->SR & (1 << 2);
}

static inline int ssp_writeable(spi_t *obj) {
    return obj->spi->SR & (1 << 1);
}

static inline void ssp_write(spi_t *obj, int value) {
    while (!ssp_writeable(obj));
    obj->spi->DR = value;
}

static inline int ssp_read(spi_t *obj) {
    while (!ssp_readable(obj));
    return obj->spi->DR;
}

static inline int ssp_busy(spi_t *obj) {
    return (obj->spi->SR & (1 << 4)) ? (1) : (0);
}

int spi_master_write(spi_t *obj, int value) {
    ssp_write(obj, value);
    return ssp_read(obj);
}

int spi_master_block_write(spi_t *obj, const char *tx_buffer, int tx_length,
                           char *rx_buffer, int rx_length, char write_fill) {
    int total = (tx_length > rx_length) ? tx_length : rx_length;

    for (int i = 0; i < total; i++) {
        char out = (i < tx_length) ? tx_buffer[i] : write_fill;
        char in = spi_master_write(obj, out);
        if (i < rx_length) {
            rx_buffer[i] = in;
        }
    }

    return total;
}

int spi_slave_receive(spi_t *obj) {
    return (ssp_readable(obj) && !ssp_busy(obj)) ? (1) : (0);
}

int spi_slave_read(spi_t *obj) {
    return obj->spi->DR;
}

void spi_slave_write(spi_t *obj, int value) {
    while (ssp_writeable(obj) == 0) ;
    obj->spi->DR = value;
}

int spi_busy(spi_t *obj) {
    return ssp_busy(obj);
}

const PinMap *spi_master_mosi_pinmap()
{
    return PinMap_SPI_MOSI;
}

const PinMap *spi_master_miso_pinmap()
{
    return PinMap_SPI_MISO;
}

const PinMap *spi_master_clk_pinmap()
{
    return PinMap_SPI_SCLK;
}

const PinMap *spi_master_cs_pinmap()
{
    return PinMap_SPI_SSEL;
}

const PinMap *spi_slave_mosi_pinmap()
{
    return PinMap_SPI_MOSI;
}

const PinMap *spi_slave_miso_pinmap()
{
    return PinMap_SPI_MISO;
}

const PinMap *spi_slave_clk_pinmap()
{
    return PinMap_SPI_SCLK;
}

const PinMap *spi_slave_cs_pinmap()
{
    return PinMap_SPI_SSEL;
}
