/*!
 * @file      lr1110_radio.c
 *
 * @brief     Radio driver implementation for LR1110
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

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include "lr1110_radio.h"
#include "lr1110_hal.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define LR1110_RADIO_RESET_STATS_CMD_LENGTH ( 2 )
#define LR1110_RADIO_GET_STATS_CMD_LENGTH ( 2 )
#define LR1110_RADIO_GET_PKT_TYPE_CMD_LENGTH ( 2 )
#define LR1110_RADIO_GET_RXBUFFER_STATUS_CMD_LENGTH ( 2 )
#define LR1110_RADIO_GET_PKT_STATUS_CMD_LENGTH ( 2 )
#define LR1110_RADIO_GET_RSSI_INST_CMD_LENGTH ( 2 )
#define LR1110_RADIO_SET_GFSK_SYNC_WORD_CMD_LENGTH ( 2 + 8 )
#define LR1110_RADIO_SET_LORA_PUBLIC_NETWORK_CMD_LENGTH ( 2 + 8 )
#define LR1110_RADIO_SET_RX_CMD_LENGTH ( 2 + 3 )
#define LR1110_RADIO_SET_TX_CMD_LENGTH ( 2 + 3 )
#define LR1110_RADIO_SET_RF_FREQUENCY_CMD_LENGTH ( 2 + 4 )
#define LR1110_RADIO_SET_AUTO_TX_RX_CMD_LENGTH ( 2 + 7 )
#define LR1110_RADIO_SET_CAD_PARAMS_CMD_LENGTH ( 2 + 7 )
#define LR1110_RADIO_SET_PKT_TYPE_CMD_LENGTH ( 2 + 1 )
#define LR1110_RADIO_SET_MODULATION_PARAMS_GFSK_CMD_LENGTH ( 2 + 10 )
#define LR1110_RADIO_SET_MODULATION_PARAMS_LORA_CMD_LENGTH ( 2 + 4 )
#define LR1110_RADIO_SET_PKT_PARAM_GFSK_CMD_LENGTH ( 2 + 9 )
#define LR1110_RADIO_SET_PKT_PARAM_LORA_CMD_LENGTH ( 2 + 6 )
#define LR1110_RADIO_SET_TX_PARAMS_CMD_LENGTH ( 2 + 2 )
#define LR1110_RADIO_SET_PKT_ADDRESS_CMD_LENGTH ( 2 + 2 )
#define LR1110_RADIO_SET_RX_TX_FALLBACK_MODE_CMD_LENGTH ( 2 + 1 )
#define LR1110_RADIO_SET_RX_DUTY_CYCLE_MODE_CMD_LENGTH ( 2 + 7 )
#define LR1110_RADIO_SET_PA_CFG_CMD_LENGTH ( 2 + 4 )
#define LR1110_RADIO_STOP_TIMEOUT_ON_PREAMBLE_CMD_LENGTH ( 2 + 1 )
#define LR1110_RADIO_SET_CAD_CMD_LENGTH ( 2 )
#define LR1110_RADIO_SET_TX_CW_CMD_LENGTH ( 2 )
#define LR1110_RADIO_SET_TX_INFINITE_PREAMBLE_CMD_LENGTH ( 2 )
#define LR1110_RADIO_SET_LORA_SYNC_TIMEOUT_CMD_LENGTH ( 2 + 1 )
#define LR1110_RADIO_SET_GFSK_CRC_PARAMS_CMD_LENGTH ( 2 + 8 )
#define LR1110_RADIO_SET_GFSK_WHITENING_CMD_LENGTH ( 2 + 2 )
#define LR1110_RADIO_SET_RX_BOOSTED_LENGTH ( 2 + 1 )
#define LR1110_RADIO_SET_LORA_SYNC_WORD_CMD_LENGTH ( 2 + 1 )

/**
 * @brief Internal RTC frequency
 */
#define LR1110_RTC_FREQ_IN_HZ 32768UL

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

enum
{
    LR1110_RADIO_RESET_STATS_OC               = 0x0200,
    LR1110_RADIO_GET_STATS_OC                 = 0x0201,
    LR1110_RADIO_GET_PKT_TYPE_OC              = 0x0202,
    LR1110_RADIO_GET_RXBUFFER_STATUS_OC       = 0x0203,
    LR1110_RADIO_GET_PKT_STATUS_OC            = 0x0204,
    LR1110_RADIO_GET_RSSI_INST_OC             = 0x0205,
    LR1110_RADIO_SET_GFSK_SYNC_WORD_OC        = 0x0206,
    LR1110_RADIO_SET_LORA_PUBLIC_NETWORK_OC   = 0x0208,
    LR1110_RADIO_SET_RX_OC                    = 0x0209,
    LR1110_RADIO_SET_TX_OC                    = 0x020A,
    LR1110_RADIO_SET_RF_FREQUENCY_OC          = 0x020B,
    LR1110_RADIO_AUTOTXRX_OC                  = 0x020C,
    LR1110_RADIO_SET_CAD_PARAMS_OC            = 0x020D,
    LR1110_RADIO_SET_PKT_TYPE_OC              = 0x020E,
    LR1110_RADIO_SET_MODULATION_PARAM_OC      = 0x020F,
    LR1110_RADIO_SET_PKT_PARAM_OC             = 0x0210,
    LR1110_RADIO_SET_TX_PARAMS_OC             = 0x0211,
    LR1110_RADIO_SET_PKT_ADRS_OC              = 0x0212,
    LR1110_RADIO_SET_RX_TX_FALLBACK_MODE_OC   = 0x0213,
    LR1110_RADIO_SET_RX_DUTY_CYCLE_OC         = 0x0214,
    LR1110_RADIO_SET_PA_CFG_OC                = 0x0215,
    LR1110_RADIO_STOP_TIMEOUT_ON_PREAMBLE_OC  = 0x0217,
    LR1110_RADIO_SET_CAD_OC                   = 0x0218,
    LR1110_RADIO_SET_TX_CW_OC                 = 0x0219,
    LR1110_RADIO_SET_TX_INFINITE_PREAMBLE_OC  = 0x021A,
    LR1110_RADIO_SET_LORA_SYNC_TIMEOUT_OC     = 0x021B,
    LR1110_RADIO_SET_GFSK_CRC_PARAMS_OC       = 0x0224,
    LR1110_RADIO_SET_GFSK_WHITENING_PARAMS_OC = 0x0225,
    LR1110_RADIO_SET_RX_BOOSTED_OC            = 0x0227,
    LR1110_RADIO_SET_LORA_SYNC_WORD_OC        = 0x022B,
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/**
 * Constructor
 */
SX1272_LoRaRadio::SX1272_LoRaRadio(PinName spi_mosi,
                                   PinName spi_miso
                                   PinName spi_sclk,
                                   PinName nss,
                                   PinName nreset,
                                   PinName dio7,
                                   PinName dio8,
                                   PinName dio9,
                                   PinName busy,
                                   PinName lnactrlon)
    :   _spi(spi_mosi, spi_miso, spi_sclk),
        _chip_select(nss, 1),
        _reset_ctl(nreset),
        _dio0_ctl(dio7), _dio1_ctl(dio8), _dio2_ctl(dio9),

/**
 * Destructor
 */
lr1110_LoRaRadio::~lr1110_LoRaRadio()
{

}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*!
 * @brief Get the CRC length in byte from the corresponding GFSK radio parameter
 *
 * @param [in] crc_type GFSK CRC parameter
 *
 * @returns CRC length in byte
 */
static inline uint32_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_crc_len_in_bytes( lr1110_radio_gfsk_crc_type_t crc_type );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_reset_stats( const void* context )
{
    uint8_t cbuffer[LR1110_RADIO_RESET_STATS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_RESET_STATS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_RESET_STATS_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_RESET_STATS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_stats( const void* context, lr1110_radio_stats_gfsk_t* stats )
{
    uint8_t         cbuffer[LR1110_RADIO_GET_STATS_CMD_LENGTH];
    uint8_t         rbuffer[sizeof( lr1110_radio_stats_gfsk_t )] = { 0x00 };
    lr1110_status_t status                                       = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_STATS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_STATS_OC >> 0 );

    status = ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_STATS_CMD_LENGTH, rbuffer,
                                                  sizeof( lr1110_radio_stats_gfsk_t ) );

    if( status == LR1110_STATUS_OK )
    {
        stats->nb_pkt_received  = ( ( uint16_t ) rbuffer[0] << 8 ) + ( uint16_t ) rbuffer[1];
        stats->nb_pkt_crc_error = ( ( uint16_t ) rbuffer[2] << 8 ) + ( uint16_t ) rbuffer[3];
        stats->nb_pkt_len_error = ( ( uint16_t ) rbuffer[4] << 8 ) + ( uint16_t ) rbuffer[5];
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_lora_stats( const void* context, lr1110_radio_stats_lora_t* stats )
{
    uint8_t         cbuffer[LR1110_RADIO_GET_STATS_CMD_LENGTH];
    uint8_t         rbuffer[sizeof( lr1110_radio_stats_lora_t )] = { 0x00 };
    lr1110_status_t status                                       = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_STATS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_STATS_OC >> 0 );

    status = ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_STATS_CMD_LENGTH, rbuffer,
                                                  sizeof( lr1110_radio_stats_lora_t ) );

    if( status == LR1110_STATUS_OK )
    {
        stats->nb_pkt_received     = ( ( uint16_t ) rbuffer[0] << 8 ) + ( uint16_t ) rbuffer[1];
        stats->nb_pkt_crc_error    = ( ( uint16_t ) rbuffer[2] << 8 ) + ( uint16_t ) rbuffer[3];
        stats->nb_pkt_header_error = ( ( uint16_t ) rbuffer[4] << 8 ) + ( uint16_t ) rbuffer[5];
        stats->nb_pkt_falsesync    = ( ( uint16_t ) rbuffer[6] << 8 ) + ( uint16_t ) rbuffer[7];
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_pkt_type( const void* context, lr1110_radio_pkt_type_t* pkt_type )
{
    uint8_t cbuffer[LR1110_RADIO_GET_PKT_TYPE_CMD_LENGTH];
    uint8_t pkt_type_raw = 0;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_PKT_TYPE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_PKT_TYPE_OC >> 0 );

    lr1110_status_t status =
        ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_PKT_TYPE_CMD_LENGTH, &pkt_type_raw, 1 );

    if( status == LR1110_STATUS_OK )
    {
        *pkt_type = ( lr1110_radio_pkt_type_t ) pkt_type_raw;
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_rx_buffer_status( const void*                      context,
                                                   lr1110_radio_rx_buffer_status_t* rx_buffer_status )
{
    uint8_t         cbuffer[LR1110_RADIO_GET_RXBUFFER_STATUS_CMD_LENGTH];
    uint8_t         rbuffer[sizeof( *rx_buffer_status )] = { 0x00 };
    lr1110_status_t status                               = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_RXBUFFER_STATUS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_RXBUFFER_STATUS_OC >> 0 );

    status = ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_RXBUFFER_STATUS_CMD_LENGTH,
                                                  rbuffer, sizeof( *rx_buffer_status ) );

    if( status == LR1110_STATUS_OK )
    {
        rx_buffer_status->pld_len_in_bytes     = rbuffer[0];
        rx_buffer_status->buffer_start_pointer = rbuffer[1];
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_pkt_status( const void* context, lr1110_radio_pkt_status_gfsk_t* pkt_status )
{
    uint8_t         cbuffer[LR1110_RADIO_GET_PKT_STATUS_CMD_LENGTH];
    uint8_t         rbuffer[4] = { 0x00 };
    lr1110_status_t status     = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_PKT_STATUS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_PKT_STATUS_OC >> 0 );

    status =
        ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_PKT_STATUS_CMD_LENGTH, rbuffer, 4 );

    if( status == LR1110_STATUS_OK )
    {
        pkt_status->rssi_sync_in_dbm = -( int8_t )( rbuffer[0] >> 1 );
        pkt_status->rssi_avg_in_dbm  = -( int8_t )( rbuffer[1] >> 1 );
        pkt_status->rx_len_in_bytes  = rbuffer[2];
        pkt_status->is_addr_err      = ( ( rbuffer[3] & 0x20 ) != 0 ) ? true : false;
        pkt_status->is_crc_err       = ( ( rbuffer[3] & 0x10 ) != 0 ) ? true : false;
        pkt_status->is_len_err       = ( ( rbuffer[3] & 0x08 ) != 0 ) ? true : false;
        pkt_status->is_abort_err     = ( ( rbuffer[3] & 0x04 ) != 0 ) ? true : false;
        pkt_status->is_received      = ( ( rbuffer[3] & 0x02 ) != 0 ) ? true : false;
        pkt_status->is_sent          = ( ( rbuffer[3] & 0x01 ) != 0 ) ? true : false;
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_lora_pkt_status( const void* context, lr1110_radio_pkt_status_lora_t* pkt_status )
{
    uint8_t         cbuffer[LR1110_RADIO_GET_PKT_STATUS_CMD_LENGTH];
    uint8_t         rbuffer[3] = { 0x00 };
    lr1110_status_t status     = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_PKT_STATUS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_PKT_STATUS_OC >> 0 );

    status =
        ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_PKT_STATUS_CMD_LENGTH, rbuffer, 3 );

    if( status == LR1110_STATUS_OK )
    {
        pkt_status->rssi_pkt_in_dbm        = -( int8_t )( rbuffer[0] >> 1 );
        pkt_status->snr_pkt_in_db          = ( ( ( int8_t ) rbuffer[1] ) + 2 ) >> 2;
        pkt_status->signal_rssi_pkt_in_dbm = -( int8_t )( rbuffer[2] >> 1 );
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_rssi_inst( const void* context, int8_t* rssi_in_dbm )
{
    uint8_t         cbuffer[LR1110_RADIO_GET_RSSI_INST_CMD_LENGTH];
    uint8_t         rssi   = 0;
    lr1110_status_t status = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_GET_RSSI_INST_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_GET_RSSI_INST_OC >> 0 );

    status = ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_RADIO_GET_RSSI_INST_CMD_LENGTH, &rssi,
                                                  sizeof( rssi ) );

    if( status == LR1110_STATUS_OK )
    {
        *rssi_in_dbm = -( int8_t )( rssi >> 1 );
    }

    return status;
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_gfsk_sync_word( const void* context, const uint8_t* gfsk_sync_word )
{
    uint8_t cbuffer[LR1110_RADIO_SET_GFSK_SYNC_WORD_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_GFSK_SYNC_WORD_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_GFSK_SYNC_WORD_OC >> 0 );

    for( uint8_t index = 0; index < 8; index++ )
    {
        cbuffer[2 + index] = gfsk_sync_word[index];
    }

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_GFSK_SYNC_WORD_CMD_LENGTH, 0, 0 );
}

#ifndef LR1110_DISABLE_WARNINGS
#warning \
    "The function lr1110_radio_set_lora_sync_word replaces the \
deprecated function lr1110_radio_set_lora_public_network. \
lr1110_radio_set_lora_sync_word, however, is incompatible \
with chip firmware versions prior to 0x303. For those legacy chips \
only, please use lr1110_radio_set_lora_public_network. \
To deactivate this warning, define C preprocessor symbol \
LR1110_DISABLE_WARNINGS."
#endif
lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_lora_sync_word( const void* context, const uint8_t sync_word )
{
    uint8_t cbuffer[LR1110_RADIO_SET_LORA_SYNC_WORD_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_LORA_SYNC_WORD_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_LORA_SYNC_WORD_OC >> 0 );

    cbuffer[2] = sync_word;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_LORA_SYNC_WORD_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_lora_public_network( const void*                            context,
                                                      const lr1110_radio_lora_network_type_t network_type )
{
    uint8_t cbuffer[LR1110_RADIO_SET_LORA_PUBLIC_NETWORK_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_LORA_PUBLIC_NETWORK_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_LORA_PUBLIC_NETWORK_OC >> 0 );

    cbuffer[2] = ( uint8_t ) network_type;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_LORA_SYNC_WORD_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_rx( const void* context, const uint32_t timeout_in_ms )
{
    const uint32_t timeout_in_rtc_step = lr1110_radio_convert_time_in_ms_to_rtc_step( timeout_in_ms );

    return lr1110_radio_set_rx_with_timeout_in_rtc_step( context, timeout_in_rtc_step );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_rx_with_timeout_in_rtc_step( const void* context, const uint32_t timeout )
{
    uint8_t cbuffer[LR1110_RADIO_SET_RX_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_RX_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_RX_OC >> 0 );

    cbuffer[2] = ( uint8_t )( timeout >> 16 );
    cbuffer[3] = ( uint8_t )( timeout >> 8 );
    cbuffer[4] = ( uint8_t )( timeout >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_RX_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_tx( const void* context, const uint32_t timeout_in_ms )
{
    const uint32_t timeout_in_rtc_step = lr1110_radio_convert_time_in_ms_to_rtc_step( timeout_in_ms );

    return lr1110_radio_set_tx_with_timeout_in_rtc_step( context, timeout_in_rtc_step );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_tx_with_timeout_in_rtc_step( const void* context, const uint32_t timeout_in_rtc_step )
{
    uint8_t cbuffer[LR1110_RADIO_SET_TX_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_TX_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_TX_OC >> 0 );

    cbuffer[2] = ( uint8_t )( timeout_in_rtc_step >> 16 );
    cbuffer[3] = ( uint8_t )( timeout_in_rtc_step >> 8 );
    cbuffer[4] = ( uint8_t )( timeout_in_rtc_step >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_TX_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_rf_freq( const void* context, const uint32_t freq_in_hz )
{
    uint8_t cbuffer[LR1110_RADIO_SET_RF_FREQUENCY_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_RF_FREQUENCY_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_RF_FREQUENCY_OC >> 0 );

    cbuffer[2] = ( uint8_t )( freq_in_hz >> 24 );
    cbuffer[3] = ( uint8_t )( freq_in_hz >> 16 );
    cbuffer[4] = ( uint8_t )( freq_in_hz >> 8 );
    cbuffer[5] = ( uint8_t )( freq_in_hz >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_RF_FREQUENCY_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_auto_tx_rx( const void* context, const uint32_t delay,
                                         const lr1110_radio_intermediary_mode_t intermediary_mode,
                                         const uint32_t                         timeout )
{
    uint8_t cbuffer[LR1110_RADIO_SET_AUTO_TX_RX_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_AUTOTXRX_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_AUTOTXRX_OC >> 0 );

    cbuffer[2] = ( uint8_t )( delay >> 16 );
    cbuffer[3] = ( uint8_t )( delay >> 8 );
    cbuffer[4] = ( uint8_t )( delay );

    cbuffer[5] = ( uint8_t ) intermediary_mode;

    cbuffer[6] = ( uint8_t )( timeout >> 16 );
    cbuffer[7] = ( uint8_t )( timeout >> 8 );
    cbuffer[8] = ( uint8_t )( timeout );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_AUTO_TX_RX_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_cad_params( const void* context, const lr1110_radio_cad_params_t* cad_params )
{
    uint8_t cbuffer[LR1110_RADIO_SET_CAD_PARAMS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_CAD_PARAMS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_CAD_PARAMS_OC >> 0 );

    cbuffer[2] = cad_params->cad_symb_nb;
    cbuffer[3] = cad_params->cad_detect_peak;
    cbuffer[4] = cad_params->cad_detect_min;
    cbuffer[5] = cad_params->cad_exit_mode;

    cbuffer[6] = ( uint8_t )( cad_params->cad_timeout >> 16 );
    cbuffer[7] = ( uint8_t )( cad_params->cad_timeout >> 8 );
    cbuffer[8] = ( uint8_t )( cad_params->cad_timeout );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_CAD_PARAMS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_pkt_type( const void* context, const lr1110_radio_pkt_type_t pkt_type )
{
    uint8_t cbuffer[LR1110_RADIO_SET_PKT_TYPE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_PKT_TYPE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_PKT_TYPE_OC >> 0 );

    cbuffer[2] = ( uint8_t ) pkt_type;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_PKT_TYPE_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_gfsk_mod_params( const void*                           context,
                                                  const lr1110_radio_mod_params_gfsk_t* mod_params )
{
    uint8_t cbuffer[LR1110_RADIO_SET_MODULATION_PARAMS_GFSK_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_MODULATION_PARAM_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_MODULATION_PARAM_OC >> 0 );

    cbuffer[2] = ( uint8_t )( mod_params->br_in_bps >> 24 );
    cbuffer[3] = ( uint8_t )( mod_params->br_in_bps >> 16 );
    cbuffer[4] = ( uint8_t )( mod_params->br_in_bps >> 8 );
    cbuffer[5] = ( uint8_t )( mod_params->br_in_bps >> 0 );

    cbuffer[6] = ( uint8_t ) mod_params->pulse_shape;
    cbuffer[7] = ( uint8_t ) mod_params->bw_dsb_param;

    cbuffer[8]  = ( uint8_t )( mod_params->fdev_in_hz >> 24 );
    cbuffer[9]  = ( uint8_t )( mod_params->fdev_in_hz >> 16 );
    cbuffer[10] = ( uint8_t )( mod_params->fdev_in_hz >> 8 );
    cbuffer[11] = ( uint8_t )( mod_params->fdev_in_hz >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_MODULATION_PARAMS_GFSK_CMD_LENGTH,
                                                 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_lora_mod_params( const void*                           context,
                                                  const lr1110_radio_mod_params_lora_t* mod_params )
{
    uint8_t cbuffer[LR1110_RADIO_SET_MODULATION_PARAMS_LORA_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_MODULATION_PARAM_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_MODULATION_PARAM_OC >> 0 );

    cbuffer[2] = ( uint8_t ) mod_params->sf;
    cbuffer[3] = ( uint8_t ) mod_params->bw;
    cbuffer[4] = ( uint8_t ) mod_params->cr;
    cbuffer[5] = ( uint8_t ) mod_params->ldro;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_MODULATION_PARAMS_LORA_CMD_LENGTH,
                                                 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_gfsk_pkt_params( const void*                           context,
                                                  const lr1110_radio_pkt_params_gfsk_t* pkt_params )
{
    uint8_t cbuffer[LR1110_RADIO_SET_PKT_PARAM_GFSK_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_PKT_PARAM_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_PKT_PARAM_OC >> 0 );

    cbuffer[2] = ( uint8_t )( pkt_params->preamble_len_in_bits >> 8 );
    cbuffer[3] = ( uint8_t )( pkt_params->preamble_len_in_bits >> 0 );

    cbuffer[4] = ( uint8_t )( pkt_params->preamble_detector );

    cbuffer[5] = pkt_params->sync_word_len_in_bits;

    cbuffer[6] = ( uint8_t )( pkt_params->address_filtering );

    cbuffer[7] = ( uint8_t )( pkt_params->header_type );

    cbuffer[8] = pkt_params->pld_len_in_bytes;

    cbuffer[9] = ( uint8_t )( pkt_params->crc_type );

    cbuffer[10] = ( uint8_t )( pkt_params->dc_free );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_PKT_PARAM_GFSK_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_lora_pkt_params( const void*                           context,
                                                  const lr1110_radio_pkt_params_lora_t* pkt_params )
{
    uint8_t cbuffer[LR1110_RADIO_SET_PKT_PARAM_LORA_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_PKT_PARAM_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_PKT_PARAM_OC >> 0 );

    cbuffer[2] = ( uint8_t )( pkt_params->preamble_len_in_symb >> 8 );
    cbuffer[3] = ( uint8_t )( pkt_params->preamble_len_in_symb >> 0 );

    cbuffer[4] = ( uint8_t )( pkt_params->header_type );

    cbuffer[5] = pkt_params->pld_len_in_bytes;

    cbuffer[6] = ( uint8_t )( pkt_params->crc );

    cbuffer[7] = ( uint8_t )( pkt_params->iq );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_PKT_PARAM_LORA_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_tx_params( const void* context, const int8_t pwr_in_dbm,
                                            const lr1110_radio_ramp_time_t ramp_time )
{
    uint8_t cbuffer[LR1110_RADIO_SET_TX_PARAMS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_TX_PARAMS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_TX_PARAMS_OC >> 0 );

    cbuffer[2] = ( uint8_t ) pwr_in_dbm;
    cbuffer[3] = ( uint8_t ) ramp_time;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_TX_PARAMS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_pkt_address( const void* context, const uint8_t node_address,
                                              const uint8_t broadcast_address )
{
    uint8_t cbuffer[LR1110_RADIO_SET_PKT_ADDRESS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_PKT_ADRS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_PKT_ADRS_OC >> 0 );

    cbuffer[2] = node_address;
    cbuffer[3] = broadcast_address;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_PKT_ADDRESS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_rx_tx_fallback_mode( const void*                         context,
                                                      const lr1110_radio_fallback_modes_t fallback_mode )
{
    uint8_t cbuffer[LR1110_RADIO_SET_RX_TX_FALLBACK_MODE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_RX_TX_FALLBACK_MODE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_RX_TX_FALLBACK_MODE_OC >> 0 );

    cbuffer[2] = fallback_mode;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_RX_TX_FALLBACK_MODE_CMD_LENGTH, 0,
                                                 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_rx_duty_cycle( const void* context, const uint32_t rx_period_in_ms,
                                                const uint32_t                          sleep_period_in_ms,
                                                const lr1110_radio_rx_duty_cycle_mode_t mode )
{
    const uint32_t rx_period_in_rtc_step    = lr1110_radio_convert_time_in_ms_to_rtc_step( rx_period_in_ms );
    const uint32_t sleep_period_in_rtc_step = lr1110_radio_convert_time_in_ms_to_rtc_step( sleep_period_in_ms );

    return lr1110_radio_set_rx_duty_cycle_with_timings_in_rtc_step( context, rx_period_in_rtc_step,
                                                                    sleep_period_in_rtc_step, mode );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_rx_duty_cycle_with_timings_in_rtc_step( const void*    context,
                                                                         const uint32_t rx_period_in_rtc_step,
                                                                         const uint32_t sleep_period_in_rtc_step,
                                                                         const lr1110_radio_rx_duty_cycle_mode_t mode )
{
    uint8_t cbuffer[LR1110_RADIO_SET_RX_DUTY_CYCLE_MODE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_RX_DUTY_CYCLE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_RX_DUTY_CYCLE_OC >> 0 );

    cbuffer[2] = ( uint8_t )( rx_period_in_rtc_step >> 16 );
    cbuffer[3] = ( uint8_t )( rx_period_in_rtc_step >> 8 );
    cbuffer[4] = ( uint8_t )( rx_period_in_rtc_step >> 0 );

    cbuffer[5] = ( uint8_t )( sleep_period_in_rtc_step >> 16 );
    cbuffer[6] = ( uint8_t )( sleep_period_in_rtc_step >> 8 );
    cbuffer[7] = ( uint8_t )( sleep_period_in_rtc_step >> 0 );

    cbuffer[8] = mode;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_RX_DUTY_CYCLE_MODE_CMD_LENGTH, 0,
                                                 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_pa_cfg( const void* context, const lr1110_radio_pa_cfg_t* pa_cfg )
{
    uint8_t cbuffer[LR1110_RADIO_SET_PA_CFG_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_PA_CFG_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_PA_CFG_OC >> 0 );

    cbuffer[2] = ( uint8_t ) pa_cfg->pa_sel;
    cbuffer[3] = ( uint8_t ) pa_cfg->pa_reg_supply;

    cbuffer[4] = pa_cfg->pa_duty_cycle;
    cbuffer[5] = pa_cfg->pa_hp_sel;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_PA_CFG_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_stop_timeout_on_preamble( const void* context, const bool stop_timeout_on_preamble )
{
    uint8_t cbuffer[LR1110_RADIO_STOP_TIMEOUT_ON_PREAMBLE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_STOP_TIMEOUT_ON_PREAMBLE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_STOP_TIMEOUT_ON_PREAMBLE_OC >> 0 );

    cbuffer[2] = ( uint8_t ) stop_timeout_on_preamble;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_STOP_TIMEOUT_ON_PREAMBLE_CMD_LENGTH, 0,
                                                 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_cad( const void* context )
{
    uint8_t cbuffer[LR1110_RADIO_SET_CAD_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_CAD_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_CAD_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_CAD_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_tx_cw( const void* context )
{
    uint8_t cbuffer[LR1110_RADIO_SET_TX_CW_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_TX_CW_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_TX_CW_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_TX_CW_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_tx_infinite_preamble( const void* context )
{
    uint8_t cbuffer[LR1110_RADIO_SET_TX_INFINITE_PREAMBLE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_TX_INFINITE_PREAMBLE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_TX_INFINITE_PREAMBLE_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_TX_INFINITE_PREAMBLE_CMD_LENGTH, 0,
                                                 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_lora_sync_timeout( const void* context, const uint8_t nb_symbol )
{
    uint8_t cbuffer[LR1110_RADIO_SET_LORA_SYNC_TIMEOUT_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_LORA_SYNC_TIMEOUT_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_LORA_SYNC_TIMEOUT_OC >> 0 );

    cbuffer[2] = nb_symbol;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_LORA_SYNC_TIMEOUT_CMD_LENGTH, 0,
                                                 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_gfsk_crc_params( const void* context, const uint32_t seed, const uint32_t polynomial )
{
    uint8_t cbuffer[LR1110_RADIO_SET_GFSK_CRC_PARAMS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_GFSK_CRC_PARAMS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_GFSK_CRC_PARAMS_OC >> 0 );

    cbuffer[2] = ( uint8_t )( seed >> 24 );
    cbuffer[3] = ( uint8_t )( seed >> 16 );
    cbuffer[4] = ( uint8_t )( seed >> 8 );
    cbuffer[5] = ( uint8_t )( seed >> 0 );

    cbuffer[6] = ( uint8_t )( polynomial >> 24 );
    cbuffer[7] = ( uint8_t )( polynomial >> 16 );
    cbuffer[8] = ( uint8_t )( polynomial >> 8 );
    cbuffer[9] = ( uint8_t )( polynomial >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_GFSK_CRC_PARAMS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_set_gfsk_whitening_seed( const void* context, const uint16_t seed )
{
    uint8_t cbuffer[LR1110_RADIO_SET_GFSK_WHITENING_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_GFSK_WHITENING_PARAMS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_GFSK_WHITENING_PARAMS_OC >> 0 );

    cbuffer[2] = ( uint8_t )( seed >> 8 );
    cbuffer[3] = ( uint8_t )( seed >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_GFSK_WHITENING_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_cfg_rx_boosted( const void* context, const bool enable_boost_mode )
{
    uint8_t cbuffer[LR1110_RADIO_SET_RX_BOOSTED_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_RADIO_SET_RX_BOOSTED_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_RADIO_SET_RX_BOOSTED_OC >> 0 );

    cbuffer[2] = ( enable_boost_mode == true ) ? 0x01 : 0x00;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_RADIO_SET_RX_BOOSTED_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_rx_bandwidth( uint32_t bw_in_hz, lr1110_radio_gfsk_bw_t* bw_parameter )
{
    if( bw_in_hz <= 4800 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_4800;
    }
    else if( bw_in_hz <= 5800 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_5800;
    }
    else if( bw_in_hz <= 7300 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_7300;
    }
    else if( bw_in_hz <= 9700 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_9700;
    }
    else if( bw_in_hz <= 11700 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_11700;
    }
    else if( bw_in_hz <= 14600 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_14600;
    }
    else if( bw_in_hz <= 19500 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_19500;
    }
    else if( bw_in_hz <= 23400 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_23400;
    }
    else if( bw_in_hz <= 29300 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_29300;
    }
    else if( bw_in_hz <= 39000 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_39000;
    }
    else if( bw_in_hz <= 46900 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_46900;
    }
    else if( bw_in_hz <= 58600 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_58600;
    }
    else if( bw_in_hz <= 78200 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_78200;
    }
    else if( bw_in_hz <= 93800 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_93800;
    }
    else if( bw_in_hz <= 117300 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_117300;
    }
    else if( bw_in_hz <= 156200 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_156200;
    }
    else if( bw_in_hz <= 187200 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_187200;
    }
    else if( bw_in_hz <= 234300 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_234300;
    }
    else if( bw_in_hz <= 312000 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_312000;
    }
    else if( bw_in_hz <= 373600 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_373600;
    }
    else if( bw_in_hz <= 467000 )
    {
        *bw_parameter = LR1110_RADIO_GFSK_BW_467000;
    }
    else
    {
        return LR1110_STATUS_ERROR;
    }

    return LR1110_STATUS_OK;
}

uint32_t lr1110_LoRaRadio::lr1110_radio_get_lora_time_on_air_numerator( const lr1110_radio_pkt_params_lora_t* pkt_p,
                                                      const lr1110_radio_mod_params_lora_t* mod_p )
{
    const int32_t pld_len_in_bytes = pkt_p->pld_len_in_bytes;
    const int32_t sf               = mod_p->sf;
    const bool    pld_is_fix       = pkt_p->header_type == LR1110_RADIO_LORA_PKT_IMPLICIT;

    uint32_t fine_synch        = ( sf <= 6 ) ? 1 : 0;
    bool     long_interleaving = ( mod_p->cr > 4 );

    uint32_t total_bytes_nb = pld_len_in_bytes + ( ( pkt_p->crc == LR1110_RADIO_LORA_CRC_ON ) ? 2 : 0 );
    uint32_t tx_bits_symbol = sf - 2 * ( mod_p->ldro != 0 ? 1 : 0 );

    int32_t ceil_numerator;
    int32_t ceil_denominator;

    uint32_t intermed;

    uint32_t symbols_nb_data;
    int32_t  tx_infobits_header;
    int32_t  tx_infobits_payload;

    if( long_interleaving )
    {
        const int32_t fec_rate_numerator   = 4;
        const int32_t fec_rate_denominator = ( mod_p->cr + ( mod_p->cr == 7 ? 1 : 0 ) );

        if( pld_is_fix )
        {
            int32_t tx_bits_symbol_start = sf - 2 + 2 * fine_synch;
            if( 8 * total_bytes_nb * fec_rate_denominator <= 7 * fec_rate_numerator * tx_bits_symbol_start )
            {
                ceil_numerator   = 8 * total_bytes_nb * fec_rate_denominator;
                ceil_denominator = fec_rate_numerator * tx_bits_symbol_start;
            }
            else
            {
                int32_t tx_codedbits_header = tx_bits_symbol_start * 8;
                ceil_numerator = 8 * fec_rate_numerator * tx_bits_symbol + 8 * total_bytes_nb * fec_rate_denominator -
                                 fec_rate_numerator * tx_codedbits_header;
                ceil_denominator = fec_rate_numerator * tx_bits_symbol;
            }
        }
        else
        {
            tx_infobits_header = ( sf * 4 + fine_synch * 8 - 28 ) & ~0x07;
            if( tx_infobits_header < 8 * total_bytes_nb )
            {
                if( tx_infobits_header > 8 * pld_len_in_bytes )
                {
                    tx_infobits_header = 8 * pld_len_in_bytes;
                }
            }
            tx_infobits_payload = 8 * total_bytes_nb - tx_infobits_header;
            if( tx_infobits_payload < 0 )
            {
                tx_infobits_payload = 0;
            }

            ceil_numerator   = tx_infobits_payload * fec_rate_denominator + 8 * fec_rate_numerator * tx_bits_symbol;
            ceil_denominator = fec_rate_numerator * tx_bits_symbol;
        }
    }
    else
    {
        tx_infobits_header = sf * 4 + fine_synch * 8 - 8;

        if( !pld_is_fix )
        {
            tx_infobits_header -= 20;
        }

        tx_infobits_payload = 8 * total_bytes_nb - tx_infobits_header;

        if( tx_infobits_payload < 0 )
            tx_infobits_payload = 0;

        ceil_numerator   = tx_infobits_payload;
        ceil_denominator = 4 * tx_bits_symbol;
    }

    symbols_nb_data = ( ( ceil_numerator + ceil_denominator - 1 ) / ceil_denominator );
    if( !long_interleaving )
    {
        symbols_nb_data = symbols_nb_data * ( mod_p->cr + 4 ) + 8;
    }
    intermed = pkt_p->preamble_len_in_symb + 4 + 2 * fine_synch + symbols_nb_data;

    return ( uint32_t )( ( 4 * intermed + 1 ) * ( 1 << ( sf - 2 ) ) ) - 1;
}

uint32_t lr1110_LoRaRadio::lr1110_radio_get_lora_bw_in_hz( lr1110_radio_lora_bw_t bw )
{
    uint32_t bw_in_hz = 0;

    switch( bw )
    {
    case LR1110_RADIO_LORA_BW_10:
        bw_in_hz = 10417UL;
        break;
    case LR1110_RADIO_LORA_BW_15:
        bw_in_hz = 15625UL;
        break;
    case LR1110_RADIO_LORA_BW_20:
        bw_in_hz = 20833UL;
        break;
    case LR1110_RADIO_LORA_BW_31:
        bw_in_hz = 31250UL;
        break;
    case LR1110_RADIO_LORA_BW_41:
        bw_in_hz = 41667UL;
        break;
    case LR1110_RADIO_LORA_BW_62:
        bw_in_hz = 62500UL;
        break;
    case LR1110_RADIO_LORA_BW_125:
        bw_in_hz = 125000UL;
        break;
    case LR1110_RADIO_LORA_BW_250:
        bw_in_hz = 250000UL;
        break;
    case LR1110_RADIO_LORA_BW_500:
        bw_in_hz = 500000UL;
        break;
    }

    return bw_in_hz;
}

uint32_t lr1110_LoRaRadio::lr1110_radio_get_lora_time_on_air_in_ms( const lr1110_radio_pkt_params_lora_t* pkt_p,
                                                  const lr1110_radio_mod_params_lora_t* mod_p )
{
    uint32_t numerator   = 1000U * lr1110_radio_get_lora_time_on_air_numerator( pkt_p, mod_p );
    uint32_t denominator = lr1110_radio_get_lora_bw_in_hz( mod_p->bw );
    // Perform integral ceil()
    return ( numerator + denominator - 1 ) / denominator;
}

uint32_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_time_on_air_numerator( const lr1110_radio_pkt_params_gfsk_t* pkt_p )
{
    return pkt_p->preamble_len_in_bits + ( pkt_p->header_type == LR1110_RADIO_GFSK_PKT_VAR_LEN ? 8 : 0 ) +
           pkt_p->sync_word_len_in_bits +
           ( ( pkt_p->pld_len_in_bytes +
               ( pkt_p->address_filtering == LR1110_RADIO_GFSK_ADDRESS_FILTERING_DISABLE ? 0 : 1 ) +
               lr1110_radio_get_gfsk_crc_len_in_bytes( pkt_p->crc_type ) )
             << 3 );
}

uint32_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_time_on_air_in_ms( const lr1110_radio_pkt_params_gfsk_t* pkt_p,
                                                  const lr1110_radio_mod_params_gfsk_t* mod_p )
{
    uint32_t numerator   = 1000U * lr1110_radio_get_gfsk_time_on_air_numerator( pkt_p );
    uint32_t denominator = mod_p->br_in_bps;

    // Perform integral ceil()
    return ( numerator + denominator - 1 ) / denominator;
}

uint32_t lr1110_LoRaRadio::lr1110_radio_convert_time_in_ms_to_rtc_step( uint32_t time_in_ms )
{
    return ( uint32_t )( time_in_ms * LR1110_RTC_FREQ_IN_HZ / 1000 );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static inline uint32_t lr1110_LoRaRadio::lr1110_radio_get_gfsk_crc_len_in_bytes( lr1110_radio_gfsk_crc_type_t crc_type )
{
    switch( crc_type )
    {
    case LR1110_RADIO_GFSK_CRC_OFF:
        return 0;
    case LR1110_RADIO_GFSK_CRC_1_BYTE:
        return 1;
    case LR1110_RADIO_GFSK_CRC_2_BYTES:
        return 2;
    case LR1110_RADIO_GFSK_CRC_1_BYTE_INV:
        return 1;
    case LR1110_RADIO_GFSK_CRC_2_BYTES_INV:
        return 2;
    }

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
