/*!
 * @file      lr1110_gnss.c
 *
 * @brief     GNSS scan driver implementation for LR1110
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

#include "lr1110_gnss.h"
#include "lr1110_regmem.h"
#include "lr1110_hal.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

#ifndef MIN
#define MIN( a, b ) ( ( a > b ) ? b : a )
#endif  // MIN

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define LR1110_GNSS_SET_CONSTALLATION_CMD_LENGTH ( 2 + 1 )
#define LR1110_GNSS_READ_CONSTALLATION_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SET_ALMANAC_UPDATE_CMD_LENGTH ( 2 + 1 )
#define LR1110_GNSS_READ_ALMANAC_UPDATE_CMD_LENGTH ( 2 )
#define LR1110_GNSS_READ_FW_VERSION_CMD_LENGTH ( 2 )
#define LR1110_GNSS_READ_SUPPORTED_CONSTELLATION_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SET_SCAN_MODE_CMD_LENGTH ( 2 + 1 )
#define LR1110_GNSS_SCAN_AUTONOMOUS_CMD_LENGTH ( 2 + 7 )
#define LR1110_GNSS_SCAN_ASSISTED_CMD_LENGTH ( 2 + 7 )
#define LR1110_GNSS_SCAN_CONTINUOUS_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SCAN_GET_RES_SIZE_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SCAN_READ_RES_CMD_LENGTH ( 2 )
#define LR1110_GNSS_ALMANAC_FULL_UPDATE_CMD_LENGTH ( 2 )
#define LR1110_GNSS_ALMANAC_READ_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SET_ASSISTANCE_POSITION_CMD_LENGTH ( 2 + 4 )
#define LR1110_GNSS_READ_ASSISTANCE_POSITION_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SET_XTAL_ERROR_CMD_LENGTH ( 2 + 2 )
#define LR1110_GNSS_READ_XTAL_ERROR_CMD_LENGTH ( 2 )
#define LR1110_GNSS_PUSH_SOLVER_MSG_CMD_LENGTH ( 2 )
#define LR1110_GNSS_PUSH_DM_MSG_CMD_LENGTH ( 2 )
#define LR1110_GNSS_GET_CONTEXT_STATUS_CMD_LENGTH ( 2 )
#define LR1110_GNSS_SCAN_GET_TIMINGS_CMD_LENGTH ( 2 )
#define LR1110_GNSS_GET_NB_SV_SATELLITES_CMD_LENGTH ( 2 )
#define LR1110_GNSS_GET_SV_SATELLITES_CMD_LENGTH ( 2 )

#define LR1110_GNSS_ALMANAC_REAC_RBUFFER_LENGTH ( 6 )
#define LR1110_GNSS_ALMANAC_DATE_LENGTH ( 2 )
#define LR1110_GNSS_FULL_ALMANAC_UPDATE_PKT_LENGTH ( 1000 )
#define LR1110_GNSS_READ_ALMANAC_TEMPBUFFER_SIZE_BYTE ( 47 )
#define LR1110_GNSS_SCAN_GET_TIMINGS_RBUFFER_LENGTH ( 8 )
#define LR1110_GNSS_MAX_DETECTED_SV ( 32 )
#define LR1110_GNSS_DETECTED_SV_SINGLE_LENGTH ( 2 )
#define LR1110_GNSS_MAX_DETECTED_SV_BUFFER_LENGTH \
    ( LR1110_GNSS_MAX_DETECTED_SV * LR1110_GNSS_DETECTED_SV_SINGLE_LENGTH )
#define LR1110_GNSS_READ_FIRMWARE_VERSION_RBUFFER_LENGTH ( 2 )

#define LR1110_GNSS_SCALING_LATITUDE 90
#define LR1110_GNSS_SCALING_LONGITUDE 180
#define LR1110_GNSS_SNR_TO_CNR_OFFSET ( 31 )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

static lr1110_status_t lr1110_gnss_get_almanac_address_size( const void* context, uint32_t* memory, uint16_t* size );

enum
{
    LR1110_GNSS_SET_CONSTALLATION_OC            = 0x0400,  //!< set the constellation to use
    LR1110_GNSS_READ_CONSTALLATION_OC           = 0x0401,  //!< read the used consteallations
    LR1110_GNSS_SET_ALMANAC_UPDATE_OC           = 0x0402,  //!< set almanac update configuration
    LR1110_GNSS_READ_ALMANAC_UPDATE_OC          = 0x0403,  //!< read the almanac update configuration
    LR1110_GNSS_READ_FW_VERSION_OC              = 0x0406,  //!< read the firmware version
    LR1110_GNSS_READ_SUPPORTED_CONSTELLATION_OC = 0x0407,  //!< read the supported constellations
    LR1110_GNSS_SET_SCAN_MODE_OC                = 0x0408,  //!< define single or double capture
    LR1110_GNSS_SCAN_AUTONOMOUS_OC              = 0x0409,  //!<
    LR1110_GNSS_SCAN_ASSISTED_OC                = 0x040A,  //!<
    LR1110_GNSS_SCAN_CONTINUOUS_OC              = 0x040B,  //!< launch the second scan
    LR1110_GNSS_SCAN_GET_RES_SIZE_OC            = 0x040C,  //!< get the size of the output payload
    LR1110_GNSS_SCAN_READ_RES_OC                = 0x040D,  //!< read the byte stream
    LR1110_GNSS_ALMANAC_FULL_UPDATE_OC          = 0x040E,  //!< Almanac update
    LR1110_GNSS_ALMANAC_READ_OC                 = 0x040F,  //!< Read all almanacs
    LR1110_GNSS_SET_ASSISTANCE_POSITION_OC      = 0x0410,  //!< set the assistance position
    LR1110_GNSS_READ_ASSISTANCE_POSITION_OC     = 0x0411,  //!< read the assistance position
    LR1110_GNSS_SET_XTAL_ERROR_OC               = 0x0412,  //!< set xtal accuracy
    LR1110_GNSS_READ_XTAL_ERROR_OC              = 0x0413,  //!< read the xtal accuracy
    LR1110_GNSS_PUSH_SOLVER_MSG_OC              = 0x0414,  //!<
    LR1110_GNSS_PUSH_DM_MSG_OC                  = 0x0415,  //!<
    LR1110_GNSS_GET_CONTEXT_STATUS_OC           = 0x0416,
    LR1110_GNSS_GET_NB_SATELLITES_OC            = 0x0417,
    LR1110_GNSS_GET_SATELLITES_OC               = 0x0418,
    LR1110_GNSS_GET_TIMINGS_OC                  = 0x0419,
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*!
 * @brief Raw Payload update from the GNSS DMC
 */
uint8_t lr1110_gnss_update_payload_from_dmc_raw_buffer[LR1110_GNSS_MAX_SIZE_ARRAY];  // define size

/*!
 * @brief almanac date
 */
uint16_t almanac_date;  //!< update : when almanac update

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*!
 * @brief Helper function that convert an array of uint8_t into a uint32_t single value
 *
 * @warning It is up to the caller to ensuer that value points to an array of at least sizeof(uint32_t) elements.
 */
static uint32_t lr1110_gnss_uint8_to_uint32( uint8_t* value );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

lr1110_status_t lr1110_gnss_get_result_size( const void* context, uint16_t* result_size )
{
    uint8_t cbuffer[LR1110_GNSS_SCAN_GET_RES_SIZE_CMD_LENGTH] = {
        ( uint8_t )( LR1110_GNSS_SCAN_GET_RES_SIZE_OC >> 8 ), ( uint8_t )( LR1110_GNSS_SCAN_GET_RES_SIZE_OC >> 0 )
    };
    uint8_t rbuffer[sizeof( uint16_t )] = { 0 };

    const lr1110_hal_status_t hal_status =
        lr1110_hal_read( context, cbuffer, LR1110_GNSS_SCAN_GET_RES_SIZE_CMD_LENGTH, rbuffer, sizeof( uint16_t ) );

    if( hal_status == LR1110_HAL_STATUS_OK )
    {
        *result_size = ( ( uint16_t ) rbuffer[0] << 8 ) + ( ( uint16_t ) rbuffer[1] );
    }

    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_read_results( const void* context, uint8_t* result_buffer,
                                          const uint16_t result_buffer_size )
{
    uint8_t cbuffer[LR1110_GNSS_SCAN_READ_RES_CMD_LENGTH] = { ( uint8_t )( LR1110_GNSS_SCAN_READ_RES_OC >> 8 ),
                                                              ( uint8_t )( LR1110_GNSS_SCAN_READ_RES_OC >> 0 ) };
    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_SCAN_READ_RES_CMD_LENGTH, result_buffer,
                                                result_buffer_size );
}

lr1110_status_t lr1110_gnss_get_timings( const void* context, lr1110_gnss_timings_t* timings )
{
    uint8_t cbuffer[LR1110_GNSS_SCAN_GET_TIMINGS_CMD_LENGTH]     = { ( uint8_t )( LR1110_GNSS_GET_TIMINGS_OC >> 8 ),
                                                                 ( uint8_t )( LR1110_GNSS_GET_TIMINGS_OC >> 0 ) };
    uint8_t rbuffer[LR1110_GNSS_SCAN_GET_TIMINGS_RBUFFER_LENGTH] = { 0 };
    const lr1110_hal_status_t hal_status = lr1110_hal_read( context, cbuffer, LR1110_GNSS_SCAN_GET_TIMINGS_CMD_LENGTH,
                                                            rbuffer, LR1110_GNSS_SCAN_GET_TIMINGS_RBUFFER_LENGTH );

    timings->computation_ms = lr1110_gnss_uint8_to_uint32( &rbuffer[0] ) / 1000;
    timings->radio_ms       = lr1110_gnss_uint8_to_uint32( &rbuffer[4] ) / 1000;
    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_one_satellite_almanac_update(
    const void* context, const lr1110_gnss_almanac_single_satellite_update_bytestram_t bytestream )
{
    uint8_t cbuffer[LR1110_GNSS_ALMANAC_FULL_UPDATE_CMD_LENGTH] = {
        ( uint8_t )( LR1110_GNSS_ALMANAC_FULL_UPDATE_OC >> 8 ), ( uint8_t )( LR1110_GNSS_ALMANAC_FULL_UPDATE_OC >> 0 )
    };

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_ALMANAC_FULL_UPDATE_CMD_LENGTH,
                                                 bytestream, LR1110_GNSS_SINGLE_ALMANAC_WRITE_SIZE );
}

lr1110_status_t lr1110_gnss_almanac_full_update( const void*                                        context,
                                                 const lr1110_gnss_almanac_full_update_bytestream_t almanac_bytestream )
{
    uint8_t cbuffer[LR1110_GNSS_ALMANAC_FULL_UPDATE_CMD_LENGTH] = {
        ( uint8_t )( LR1110_GNSS_ALMANAC_FULL_UPDATE_OC >> 8 ), ( uint8_t )( LR1110_GNSS_ALMANAC_FULL_UPDATE_OC >> 0 )
    };

    lr1110_hal_status_t hal_status = LR1110_HAL_STATUS_OK;

    uint16_t remaining_almanac_to_write = LR1110_GNSS_FULL_ALMANAC_WRITE_BUFFER_SIZE;
    while( remaining_almanac_to_write > 0 )
    {
        const uint16_t almanac_size_to_write =
            MIN( remaining_almanac_to_write, LR1110_GNSS_FULL_ALMANAC_UPDATE_PKT_LENGTH );

        const uint8_t* almanac_to_write =
            almanac_bytestream + ( LR1110_GNSS_FULL_ALMANAC_WRITE_BUFFER_SIZE - remaining_almanac_to_write );

        const lr1110_hal_status_t local_hal_status = lr1110_hal_write(
            context, cbuffer, LR1110_GNSS_ALMANAC_FULL_UPDATE_CMD_LENGTH, almanac_to_write, almanac_size_to_write );
        if( local_hal_status != LR1110_HAL_STATUS_OK )
        {
            return ( lr1110_status_t ) local_hal_status;
        }

        remaining_almanac_to_write -= almanac_size_to_write;
    }
    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_read_almanac( const void*                                context,
                                          lr1110_gnss_almanac_full_read_bytestream_t almanac_bytestream )
{
    uint32_t        almanac_address = 0;
    uint16_t        almanac_size    = 0;
    lr1110_status_t status          = lr1110_gnss_get_almanac_address_size( context, &almanac_address, &almanac_size );
    if( status != LR1110_STATUS_OK )
    {
        return status;
    }

    const uint8_t N_READ_ALMANAC_REGMEM32 = 15;

    for( uint8_t index_regmem32 = 0; index_regmem32 < N_READ_ALMANAC_REGMEM32; index_regmem32++ )
    {
        const uint16_t local_bytestream_index_burst =
            index_regmem32 * LR1110_GNSS_READ_ALMANAC_TEMPBUFFER_SIZE_BYTE * 4;
        uint32_t temporary_buffer[LR1110_GNSS_READ_ALMANAC_TEMPBUFFER_SIZE_BYTE] = { 0 };

        const lr1110_status_t local_status = lr1110_regmem_read_regmem32(
            context, almanac_address, temporary_buffer, LR1110_GNSS_READ_ALMANAC_TEMPBUFFER_SIZE_BYTE );
        if( local_status != LR1110_STATUS_OK )
        {
            return local_status;
        }

        almanac_address += ( LR1110_GNSS_READ_ALMANAC_TEMPBUFFER_SIZE_BYTE * 4 );

        for( uint8_t index_local_temp = 0; index_local_temp < LR1110_GNSS_READ_ALMANAC_TEMPBUFFER_SIZE_BYTE;
             index_local_temp++ )
        {
            const uint16_t local_bytestream_index          = local_bytestream_index_burst + ( index_local_temp * 4 );
            almanac_bytestream[local_bytestream_index + 0] = ( uint8_t )( temporary_buffer[index_local_temp] >> 0 );
            almanac_bytestream[local_bytestream_index + 1] = ( uint8_t )( temporary_buffer[index_local_temp] >> 8 );
            almanac_bytestream[local_bytestream_index + 2] = ( uint8_t )( temporary_buffer[index_local_temp] >> 16 );
            almanac_bytestream[local_bytestream_index + 3] = ( uint8_t )( temporary_buffer[index_local_temp] >> 24 );
        }
    }
    return status;
}

lr1110_status_t lr1110_gnss_get_almanac_age_for_satellite( const void* context, const lr1110_gnss_satellite_id_t sv_id,
                                                           uint16_t* almanac_age )
{
    uint32_t              almanac_base_address = 0;
    uint16_t              almanac_size         = 0;
    const lr1110_status_t status_get_almanac_address_size =
        lr1110_gnss_get_almanac_address_size( context, &almanac_base_address, &almanac_size );

    if( status_get_almanac_address_size != LR1110_STATUS_OK )
    {
        return status_get_almanac_address_size;
    }

    const uint16_t offset_almanac_date                               = sv_id * LR1110_GNSS_SINGLE_ALMANAC_READ_SIZE + 1;
    uint8_t        raw_almanac_date[LR1110_GNSS_ALMANAC_DATE_LENGTH] = { 0 };

    const lr1110_status_t status_read_mem = lr1110_regmem_read_mem8(
        context, almanac_base_address + offset_almanac_date, raw_almanac_date, LR1110_GNSS_ALMANAC_DATE_LENGTH );
    if( status_read_mem == LR1110_STATUS_OK )
    {
        // Note: the memory on LR1110 is LSB first. As the 2-byte wide almanac age is obtained by calling the _mem8, the
        // conversion to uint16_t here is done LSB first
        ( *almanac_age ) =
            ( ( ( uint16_t ) raw_almanac_date[1] ) << 8 ) + ( ( ( uint16_t ) raw_almanac_date[0] ) << 0 );
    }
    return status_read_mem;
}

lr1110_status_t lr1110_gnss_get_almanac_crc( const void* context, uint32_t* almanac_crc )
{
    uint32_t              almanac_base_address = 0;
    uint16_t              almanac_size         = 0;
    const lr1110_status_t status_get_almanac_address_size =
        lr1110_gnss_get_almanac_address_size( context, &almanac_base_address, &almanac_size );

    if( status_get_almanac_address_size != LR1110_STATUS_OK )
    {
        return status_get_almanac_address_size;
    }

    const uint16_t offset_almanac = 128 * LR1110_GNSS_SINGLE_ALMANAC_READ_SIZE;

    const lr1110_status_t status_read_mem =
        lr1110_regmem_read_regmem32( context, almanac_base_address + offset_almanac, almanac_crc, 1 );
    return status_read_mem;
}

lr1110_status_t lr1110_gnss_get_almanac_address_size( const void* context, uint32_t* memory, uint16_t* size )
{
    uint8_t cbuffer[LR1110_GNSS_ALMANAC_READ_CMD_LENGTH] = { ( uint8_t )( LR1110_GNSS_ALMANAC_READ_OC >> 8 ),
                                                             ( uint8_t )( LR1110_GNSS_ALMANAC_READ_OC >> 0 ) };

    uint8_t rbuffer[LR1110_GNSS_ALMANAC_REAC_RBUFFER_LENGTH] = { 0 };

    const lr1110_hal_status_t hal_status = lr1110_hal_read( context, cbuffer, LR1110_GNSS_ALMANAC_READ_CMD_LENGTH,
                                                            rbuffer, LR1110_GNSS_ALMANAC_REAC_RBUFFER_LENGTH );

    if( hal_status == LR1110_HAL_STATUS_OK )
    {
        *memory = lr1110_gnss_uint8_to_uint32( &rbuffer[0] );
        *size   = ( ( ( uint16_t ) rbuffer[4] ) << 8 ) | ( ( uint16_t ) rbuffer[5] );
    }

    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_push_solver_msg( const void* context, const uint8_t* payload, const uint16_t payload_size )
{
    uint8_t cbuffer[LR1110_GNSS_PUSH_SOLVER_MSG_CMD_LENGTH] = {
        ( uint8_t )( LR1110_GNSS_PUSH_SOLVER_MSG_OC >> 8 ), ( uint8_t )( LR1110_GNSS_PUSH_SOLVER_MSG_OC & 0x00FF )
    };
    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_PUSH_SOLVER_MSG_CMD_LENGTH, payload,
                                                 payload_size );
}

lr1110_status_t lr1110_gnss_set_constellations_to_use( const void*                            context,
                                                       const lr1110_gnss_constellation_mask_t constellation_to_use )
{
    uint8_t cbuffer[LR1110_GNSS_SET_CONSTALLATION_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SET_CONSTALLATION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SET_CONSTALLATION_OC >> 0 );

    cbuffer[2] = constellation_to_use;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SET_CONSTALLATION_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_gnss_read_used_constellations( const void*                       context,
                                                      lr1110_gnss_constellation_mask_t* constellations_used )
{
    uint8_t cbuffer[LR1110_GNSS_READ_CONSTALLATION_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_READ_CONSTALLATION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_READ_CONSTALLATION_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_READ_CONSTALLATION_CMD_LENGTH,
                                                constellations_used, sizeof( *constellations_used ) );
}

lr1110_status_t lr1110_gnss_set_almanac_update( const void*                            context,
                                                const lr1110_gnss_constellation_mask_t constellations_to_update )
{
    uint8_t cbuffer[LR1110_GNSS_SET_ALMANAC_UPDATE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SET_ALMANAC_UPDATE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SET_ALMANAC_UPDATE_OC >> 0 );

    cbuffer[2] = constellations_to_update;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SET_ALMANAC_UPDATE_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_gnss_read_almanac_update( const void*                       context,
                                                 lr1110_gnss_constellation_mask_t* constellations_to_update )
{
    uint8_t cbuffer[LR1110_GNSS_READ_ALMANAC_UPDATE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_READ_ALMANAC_UPDATE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_READ_ALMANAC_UPDATE_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_READ_ALMANAC_UPDATE_CMD_LENGTH,
                                                constellations_to_update, sizeof( *constellations_to_update ) );
}

lr1110_status_t lr1110_gnss_read_firmware_version( const void* context, lr1110_gnss_version_t* version )
{
    uint8_t cbuffer[LR1110_GNSS_READ_FW_VERSION_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_READ_FW_VERSION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_READ_FW_VERSION_OC >> 0 );

    uint8_t rbuffer[LR1110_GNSS_READ_FIRMWARE_VERSION_RBUFFER_LENGTH] = { 0 };

    const lr1110_hal_status_t hal_status = lr1110_hal_read( context, cbuffer, LR1110_GNSS_READ_FW_VERSION_CMD_LENGTH,
                                                            rbuffer, LR1110_GNSS_READ_FIRMWARE_VERSION_RBUFFER_LENGTH );

    version->gnss_firmware = rbuffer[0];
    version->gnss_almanac  = rbuffer[1];
    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_read_supported_constellations( const void*                       context,
                                                           lr1110_gnss_constellation_mask_t* supported_constellations )
{
    uint8_t cbuffer[LR1110_GNSS_READ_SUPPORTED_CONSTELLATION_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_READ_SUPPORTED_CONSTELLATION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_READ_SUPPORTED_CONSTELLATION_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_READ_SUPPORTED_CONSTELLATION_CMD_LENGTH,
                                                supported_constellations, sizeof( *supported_constellations ) );
}

lr1110_status_t lr1110_gnss_set_scan_mode( const void* context, const lr1110_gnss_scan_mode_t scan_mode,
                                           uint8_t* inter_capture_delay_second )
{
    uint8_t cbuffer[LR1110_GNSS_SET_SCAN_MODE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SET_SCAN_MODE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SET_SCAN_MODE_OC >> 0 );

    cbuffer[2] = ( uint8_t ) scan_mode;

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_SET_SCAN_MODE_CMD_LENGTH,
                                                inter_capture_delay_second, sizeof( *inter_capture_delay_second ) );
}

lr1110_status_t lr1110_gnss_scan_autonomous( const void* context, const lr1110_gnss_date_t date,
                                             const lr1110_gnss_search_mode_t effort_mode,
                                             const uint8_t gnss_input_parameters, const uint8_t nb_sat )
{
    uint8_t cbuffer[LR1110_GNSS_SCAN_AUTONOMOUS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SCAN_AUTONOMOUS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SCAN_AUTONOMOUS_OC >> 0 );

    cbuffer[2] = ( uint8_t )( date >> 24 );
    cbuffer[3] = ( uint8_t )( date >> 16 );
    cbuffer[4] = ( uint8_t )( date >> 8 );
    cbuffer[5] = ( uint8_t )( date >> 0 );
    cbuffer[6] = ( uint8_t ) effort_mode;
    cbuffer[7] = gnss_input_parameters;
    cbuffer[8] = nb_sat;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SCAN_AUTONOMOUS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_gnss_scan_assisted( const void* context, const lr1110_gnss_date_t date,
                                           const lr1110_gnss_search_mode_t effort_mode,
                                           const uint8_t gnss_input_parameters, const uint8_t nb_sat )
{
    uint8_t cbuffer[LR1110_GNSS_SCAN_ASSISTED_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SCAN_ASSISTED_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SCAN_ASSISTED_OC >> 0 );

    cbuffer[2] = ( uint8_t )( date >> 24 );
    cbuffer[3] = ( uint8_t )( date >> 16 );
    cbuffer[4] = ( uint8_t )( date >> 8 );
    cbuffer[5] = ( uint8_t )( date >> 0 );
    cbuffer[6] = ( uint8_t ) effort_mode;
    cbuffer[7] = gnss_input_parameters;
    cbuffer[8] = nb_sat;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SCAN_ASSISTED_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_gnss_scan_continuous( const void* context )
{
    uint8_t cbuffer[LR1110_GNSS_SCAN_CONTINUOUS_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SCAN_CONTINUOUS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SCAN_CONTINUOUS_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SCAN_CONTINUOUS_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_gnss_set_assistance_position(
    const void* context, const lr1110_gnss_solver_assistance_position_t* assistance_position )
{
    int16_t latitude, longitude;

    uint8_t cbuffer[LR1110_GNSS_SET_ASSISTANCE_POSITION_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SET_ASSISTANCE_POSITION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SET_ASSISTANCE_POSITION_OC >> 0 );

    latitude   = ( ( assistance_position->latitude * 2048 ) / LR1110_GNSS_SCALING_LATITUDE );
    cbuffer[2] = ( uint8_t )( latitude >> 8 );
    cbuffer[3] = ( uint8_t )( latitude );

    longitude  = ( ( assistance_position->longitude * 2048 ) / LR1110_GNSS_SCALING_LONGITUDE );
    cbuffer[4] = ( uint8_t )( longitude >> 8 );
    cbuffer[5] = ( uint8_t )( longitude );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SET_ASSISTANCE_POSITION_CMD_LENGTH, 0,
                                                 0 );
}

lr1110_status_t lr1110_gnss_read_assistance_position( const void*                               context,
                                                      lr1110_gnss_solver_assistance_position_t* assistance_position )
{
    uint8_t position_buffer[4] = { 0x00 };
    int16_t position_tmp;

    uint8_t cbuffer[LR1110_GNSS_READ_ASSISTANCE_POSITION_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_READ_ASSISTANCE_POSITION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_READ_ASSISTANCE_POSITION_OC >> 0 );

    const lr1110_hal_status_t hal_status = lr1110_hal_read(
        context, cbuffer, LR1110_GNSS_READ_ASSISTANCE_POSITION_CMD_LENGTH, position_buffer, sizeof( position_buffer ) );

    position_tmp                  = ( ( ( uint16_t ) position_buffer[0] << 8 ) + position_buffer[1] );
    assistance_position->latitude = ( ( float ) ( position_tmp ) *LR1110_GNSS_SCALING_LATITUDE ) / 2048;

    position_tmp                   = ( ( ( uint16_t ) position_buffer[2] << 8 ) + position_buffer[3] );
    assistance_position->longitude = ( ( float ) ( position_tmp ) *LR1110_GNSS_SCALING_LONGITUDE ) / 2048;
    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_set_xtal_error( const void* context, const float xtal_error_in_ppm )
{
    int16_t error;

    uint8_t cbuffer[LR1110_GNSS_SET_XTAL_ERROR_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_SET_XTAL_ERROR_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_SET_XTAL_ERROR_OC >> 0 );

    error      = ( ( xtal_error_in_ppm * 32768 ) / 40 );
    cbuffer[2] = ( uint8_t )( error >> 8 );
    cbuffer[3] = ( uint8_t )( error );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_SET_XTAL_ERROR_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_gnss_read_xtal_error( const void* context, float* xtal_error_in_ppm )
{
    uint8_t xtal_error_buffer[2] = { 0x00 };
    int16_t xtal_error_temp;

    uint8_t cbuffer[LR1110_GNSS_READ_XTAL_ERROR_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_READ_XTAL_ERROR_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_READ_XTAL_ERROR_OC >> 0 );

    const lr1110_hal_status_t hal_status = lr1110_hal_read( context, cbuffer, LR1110_GNSS_READ_XTAL_ERROR_CMD_LENGTH,
                                                            xtal_error_buffer, sizeof( xtal_error_buffer ) );

    xtal_error_temp    = ( ( ( uint16_t ) xtal_error_buffer[0] << 8 ) + xtal_error_buffer[1] );
    *xtal_error_in_ppm = ( ( float ) ( xtal_error_temp ) *40 ) / 32768;
    return ( lr1110_status_t ) hal_status;
}

lr1110_status_t lr1110_gnss_push_dmc_msg( const void* context, uint8_t* dmc_msg, uint16_t dmc_msg_len )
{
    uint8_t cbuffer[LR1110_GNSS_PUSH_DM_MSG_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_PUSH_DM_MSG_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_PUSH_DM_MSG_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_GNSS_PUSH_DM_MSG_CMD_LENGTH, dmc_msg,
                                                 dmc_msg_len );
}

lr1110_status_t lr1110_gnss_get_context_status( const void*                             context,
                                                lr1110_gnss_context_status_bytestream_t context_status )
{
    uint8_t         cbuffer[LR1110_GNSS_GET_CONTEXT_STATUS_CMD_LENGTH];
    lr1110_status_t status = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_GET_CONTEXT_STATUS_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_GET_CONTEXT_STATUS_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_GET_CONTEXT_STATUS_CMD_LENGTH,
                                                context_status, LR1110_GNSS_CONTEXT_STATUS_LENGTH );
}

lr1110_status_t lr1110_gnss_get_nb_detected_satellites( const void* context, uint8_t* nb_detected_satellites )
{
    uint8_t cbuffer[LR1110_GNSS_GET_NB_SV_SATELLITES_CMD_LENGTH] = { 0 };

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_GET_NB_SATELLITES_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_GET_NB_SATELLITES_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_GNSS_GET_NB_SV_SATELLITES_CMD_LENGTH,
                                                nb_detected_satellites, 1 );
}

lr1110_status_t lr1110_gnss_get_detected_satellites( const void* context, const uint8_t nb_detected_satellites,
                                                     lr1110_gnss_detected_satellite_t* detected_satellite_id_snr )
{
    const uint8_t max_satellites_to_fetch =
        ( LR1110_GNSS_MAX_DETECTED_SV > nb_detected_satellites ) ? nb_detected_satellites : LR1110_GNSS_MAX_DETECTED_SV;
    const uint16_t read_size = max_satellites_to_fetch * LR1110_GNSS_DETECTED_SV_SINGLE_LENGTH;
    uint8_t        result_buffer[LR1110_GNSS_MAX_DETECTED_SV_BUFFER_LENGTH] = { 0 };

    uint8_t cbuffer[LR1110_GNSS_GET_SV_SATELLITES_CMD_LENGTH] = { 0 };

    cbuffer[0] = ( uint8_t )( LR1110_GNSS_GET_SATELLITES_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_GNSS_GET_SATELLITES_OC >> 0 );

    const lr1110_hal_status_t hal_status =
        lr1110_hal_read( context, cbuffer, LR1110_GNSS_GET_SV_SATELLITES_CMD_LENGTH, result_buffer, read_size );
    if( hal_status == LR1110_HAL_STATUS_OK )
    {
        for( uint8_t index_satellite = 0; index_satellite < max_satellites_to_fetch; index_satellite++ )
        {
            const uint16_t                    local_result_buffer_index = index_satellite * 2;
            lr1110_gnss_detected_satellite_t* local_satellite_result    = &detected_satellite_id_snr[index_satellite];

            local_satellite_result->satellite_id = result_buffer[local_result_buffer_index];
            local_satellite_result->cnr = result_buffer[local_result_buffer_index + 1] + LR1110_GNSS_SNR_TO_CNR_OFFSET;
        }
    }
    return ( lr1110_status_t ) hal_status;
}

void lr1110_gnss_parse_context_status_buffer( const lr1110_gnss_context_status_bytestream_t constext_status_bytestream,
                                              lr1110_gnss_context_status_t*                 context_status )
{
    context_status->firmware_version = constext_status_bytestream[0];

    context_status->global_almanac_crc =
        ( ( uint32_t ) constext_status_bytestream[1] << 24 ) + ( ( uint32_t ) constext_status_bytestream[2] << 16 ) +
        ( ( uint32_t ) constext_status_bytestream[3] << 8 ) + ( ( uint32_t ) constext_status_bytestream[4] << 0 );

    context_status->error_code = ( lr1110_gnss_error_code_t )( constext_status_bytestream[5] >> 4 );

    context_status->almanac_update_gps =
        ( ( constext_status_bytestream[5] & LR1110_GNSS_DMC_ALMANAC_UPDATE_GPS_MASK ) != 0 ) ? true : false;

    context_status->almanac_update_beidou =
        ( ( constext_status_bytestream[5] & LR1110_GNSS_DMC_ALMANAC_UPDATE_BEIDOU_MASK ) != 0 ) ? true : false;

    context_status->freq_search_space = ( lr1110_gnss_freq_search_space_t )(
        ( ( ( constext_status_bytestream[5] & LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_MSB_MASK ) >>
            LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_MSB_POS )
          << 1 ) +
        ( ( constext_status_bytestream[6] & LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_LSB_MASK ) >>
          LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_LSB_POS ) );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

uint32_t lr1110_gnss_uint8_to_uint32( uint8_t* value )
{
    return ( ( ( uint32_t ) value[0] ) << 24 ) + ( ( ( uint32_t ) value[1] ) << 16 ) +
           ( ( ( uint32_t ) value[2] ) << 8 ) + ( ( ( uint32_t ) value[3] ) << 0 );
}
/* --- EOF ------------------------------------------------------------------ */
