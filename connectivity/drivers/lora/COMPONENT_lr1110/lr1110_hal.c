/**
 * @file      lr1110_hal.c
 *
 * @brief     HAL implementation for LR1110 radio chip
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2020. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "lr1110_hal.h"
#include "configuration.h"
#include "system.h"

lr1110_hal_status_t lr1110_hal_reset( const void* radio )
{
    radio_t* radio_local = ( radio_t* ) radio;

    system_gpio_set_pin_state( radio_local->reset, SYSTEM_GPIO_PIN_STATE_LOW );
    system_time_wait_ms( 1 );
    system_gpio_set_pin_state( radio_local->reset, SYSTEM_GPIO_PIN_STATE_HIGH );

    return LR1110_HAL_STATUS_OK;
}

lr1110_hal_status_t lr1110_hal_wakeup( const void* radio )
{
    radio_t* radio_local = ( radio_t* ) radio;

    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_LOW );
    system_time_wait_ms( 1 );
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_HIGH );

    return LR1110_HAL_STATUS_OK;
}

lr1110_hal_status_t lr1110_hal_read( const void* radio, const uint8_t* cbuffer, const uint16_t cbuffer_length,
                                     uint8_t* rbuffer, const uint16_t rbuffer_length )
{
    radio_t* radio_local = ( radio_t* ) radio;
    uint8_t  dummy_byte  = 0x00;

    system_gpio_wait_for_state( radio_local->busy, SYSTEM_GPIO_PIN_STATE_LOW );

    /* 1st SPI transaction */
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_LOW );
    system_spi_write( radio_local->spi, cbuffer, cbuffer_length );
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_HIGH );

    system_gpio_wait_for_state( radio_local->busy, SYSTEM_GPIO_PIN_STATE_LOW );

    /* 2nd SPI transaction */
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_LOW );
    system_spi_write( radio_local->spi, &dummy_byte, 1 );
    system_spi_read( radio_local->spi, rbuffer, rbuffer_length );
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_HIGH );

    return LR1110_HAL_STATUS_OK;
}

lr1110_hal_status_t lr1110_hal_write( const void* radio, const uint8_t* cbuffer, const uint16_t cbuffer_length,
                                      const uint8_t* cdata, const uint16_t cdata_length )
{
    radio_t* radio_local = ( radio_t* ) radio;

    system_gpio_wait_for_state( radio_local->busy, SYSTEM_GPIO_PIN_STATE_LOW );

    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_LOW );
    system_spi_write( radio_local->spi, cbuffer, cbuffer_length );
    system_spi_write( radio_local->spi, cdata, cdata_length );
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_HIGH );

    return LR1110_HAL_STATUS_OK;
}

lr1110_hal_status_t lr1110_hal_write_read( const void* radio, const uint8_t* cbuffer, uint8_t* rbuffer,
                                           const uint16_t length )
{
    radio_t* radio_local = ( radio_t* ) radio;

    system_gpio_wait_for_state( radio_local->busy, SYSTEM_GPIO_PIN_STATE_LOW );

    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_LOW );
    system_spi_write_read( radio_local->spi, cbuffer, rbuffer, length );
    system_gpio_set_pin_state( radio_local->nss, SYSTEM_GPIO_PIN_STATE_HIGH );

    return LR1110_HAL_STATUS_OK;
}