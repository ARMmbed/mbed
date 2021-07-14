/**
 * @file      system_spi.c
 *
 * @brief     MCU SPI-related functions
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

#include "system_spi.h"

void system_spi_init( void )
{
    LL_SPI_InitTypeDef  SPI_InitStruct  = { 0 };
    LL_GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock( LL_APB2_GRP1_PERIPH_SPI1 );
    LL_AHB2_GRP1_EnableClock( LL_AHB2_GRP1_PERIPH_GPIOA );

    /** SPI1 GPIO Configuration
    PA5   ------> SPI1_SCK
    PA6   ------> SPI1_MISO
    PA7   ------> SPI1_MOSI
    */
    GPIO_InitStruct.Pin        = LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate  = LL_GPIO_AF_5;
    LL_GPIO_Init( GPIOA, &GPIO_InitStruct );

    SPI_InitStruct.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV4;
    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode              = LL_SPI_MODE_MASTER;
    SPI_InitStruct.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    SPI_InitStruct.ClockPolarity     = LL_SPI_POLARITY_LOW;
    SPI_InitStruct.ClockPhase        = LL_SPI_PHASE_1EDGE;
    SPI_InitStruct.NSS               = LL_SPI_NSS_SOFT;
    SPI_InitStruct.BitOrder          = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly           = 7;
    LL_SPI_Init( SPI1, &SPI_InitStruct );

    LL_SPI_SetStandard( SPI1, LL_SPI_PROTOCOL_MOTOROLA );
    LL_SPI_DisableNSSPulseMgt( SPI1 );

    LL_SPI_Enable( SPI1 );
    while( LL_SPI_IsEnabled( SPI1 ) == 0 )
    {
    };

    LL_SPI_SetRxFIFOThreshold( SPI1, LL_SPI_RX_FIFO_TH_QUARTER );
}

void system_spi_write( SPI_TypeDef* spi, const uint8_t* buffer, uint16_t length )
{
    for( uint16_t i = 0; i < length; i++ )
    {
        while( LL_SPI_IsActiveFlag_TXE( spi ) == 0 )
        {
        };

        LL_SPI_TransmitData8( spi, buffer[i] );

        while( LL_SPI_IsActiveFlag_RXNE( spi ) == 0 )
        {
        };

        LL_SPI_ReceiveData8( spi );
    }
}

void system_spi_read( SPI_TypeDef* spi, uint8_t* buffer, uint16_t length )
{
    system_spi_write_read( spi, buffer, buffer, length );
}

void system_spi_write_read( SPI_TypeDef* spi, const uint8_t* cbuffer, uint8_t* rbuffer, uint16_t length )
{
    for( uint16_t i = 0; i < length; i++ )
    {
        while( LL_SPI_IsActiveFlag_TXE( spi ) == 0 )
        {
        };

        LL_SPI_TransmitData8( spi, cbuffer[i] );

        while( LL_SPI_IsActiveFlag_RXNE( spi ) == 0 )
        {
        };

        rbuffer[i] = LL_SPI_ReceiveData8( spi );
    }
}
