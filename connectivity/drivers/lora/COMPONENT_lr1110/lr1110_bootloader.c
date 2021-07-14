/*!
 * @file      lr1110_bootloader.c
 *
 * @brief     Bootloader driver implementation for LR1110
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

#include "lr1110_bootloader.h"
#include "lr1110_hal.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define LR1110_FLASH_DATA_UINT32_MAX ( 64 )
#define LR1110_FLASH_DATA_UINT8_MAX ( LR1110_FLASH_DATA_UINT32_MAX * 4 )

#define LR1110_BL_CMD_NO_PARAM_LENGTH 2
#define LR1110_BL_VERSION_CMD_LENGTH LR1110_BL_CMD_NO_PARAM_LENGTH
#define LR1110_BL_ERASE_FLASH_CMD_LENGTH LR1110_BL_CMD_NO_PARAM_LENGTH
#define LR1110_BL_ERASE_PAGE_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH + 1 )
#define LR1110_BL_WRITE_FLASH_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH + 4 )
#define LR1110_BL_WRITE_FLASH_ENCRYPTED_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH + 4 )
#define LR1110_BL_GET_HASH_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH )
#define LR1110_BL_REBOOT_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH + 1 )
#define LR1110_BL_GET_PIN_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH )
#define LR1110_BL_READ_CHIP_EUI_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH )
#define LR1110_BL_READ_JOIN_EUI_CMD_LENGTH ( LR1110_BL_CMD_NO_PARAM_LENGTH )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

enum
{
    LR1110_BL_GET_STATUS_OC            = 0x0100,
    LR1110_BL_GET_VERSION_OC           = 0x0101,
    LR1110_BL_ERASE_FLASH_OC           = 0x8000,
    LR1110_BL_ERASE_PAGE_OC            = 0x8001,
    LR1110_BL_WRITE_FLASH_OC           = 0x8002,
    LR1110_BL_WRITE_FLASH_ENCRYPTED_OC = 0x8003,
    LR1110_BL_GET_HASH_OC              = 0x8004,
    LR1110_BL_REBOOT_OC                = 0x8005,
    LR1110_BL_GET_PIN_OC               = 0x800B,
    LR1110_BL_READ_CHIP_EUI_OC         = 0x800C,
    LR1110_BL_READ_JOIN_EUI_OC         = 0x800D,
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

uint32_t min( uint32_t a, uint32_t b )
{
    uint32_t min = a;

    if( a > b )
    {
        min = b;
    }

    return min;
}

/*!
 * @brief Helper function to fill cbuffer with opcode and offset
 *
 * Typically used in write flash functions.
 *
 * @warning It is up to the caller to ensure the size of cbuffer is big enough to contain all information!
 */
static void lr1110_bootloader_fill_cbuffer_opcode_offset_flash( uint8_t* cbuffer, uint16_t opcode, uint32_t offset );

/*!
 * @brief Helper function to fill cdata with data
 *
 * Typically used in write flash functions.
 *
 * @warning It is up to the caller to ensure the size of cdata is big enough to contain all data!
 */
static void lr1110_bootloader_fill_cdata_flash( uint8_t* cdata, const uint32_t* data, uint8_t data_length );

/*!
 * @brief Helper function to fill cbuffer and cdata with information to write flash
 *
 * Typically used in write flash functions. Internally calls lr1110_bootloader_fill_cbuffer_opcode_offset_flash and
 * lr1110_bootloader_fill_cdata_flash.
 *
 * @warning It is up to the caller to ensure the sizes of cbuffer and cdata are big enough to contain their respective
 * information!
 */
static void lr1110_bootloader_fill_cbuffer_cdata_flash( uint8_t* cbuffer, uint8_t* cdata, uint16_t opcode,
                                                        uint32_t offset, const uint32_t* data, uint8_t data_length );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

lr1110_status_t lr1110_bootloader_get_version( const void* context, lr1110_bootloader_version_t* version )
{
    uint8_t         cbuffer[LR1110_BL_VERSION_CMD_LENGTH];
    uint8_t         rbuffer[LR1110_BL_VERSION_LENGTH] = { 0x00 };
    lr1110_status_t status                            = LR1110_STATUS_ERROR;

    cbuffer[0] = ( uint8_t )( LR1110_BL_GET_VERSION_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_GET_VERSION_OC >> 0 );

    status = ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_BL_VERSION_CMD_LENGTH, rbuffer,
                                                  LR1110_BL_VERSION_LENGTH );

    if( status == LR1110_STATUS_OK )
    {
        version->hw   = rbuffer[0];
        version->type = rbuffer[1];
        version->fw   = ( ( uint16_t ) rbuffer[2] << 8 ) + ( uint16_t ) rbuffer[3];
    }

    return status;
}

lr1110_status_t lr1110_bootloader_erase_flash( const void* context )
{
    uint8_t cbuffer[LR1110_BL_ERASE_FLASH_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_ERASE_FLASH_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_ERASE_FLASH_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_BL_ERASE_FLASH_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_bootloader_erase_page( const void* context, const uint8_t page_number )
{
    uint8_t cbuffer[LR1110_BL_ERASE_PAGE_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_ERASE_PAGE_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_ERASE_PAGE_OC >> 0 );

    cbuffer[2] = page_number;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_BL_ERASE_PAGE_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_bootloader_write_flash( const void* context, const uint32_t offset, const uint32_t* data,
                                               uint8_t length )
{
    uint8_t cbuffer[LR1110_BL_WRITE_FLASH_CMD_LENGTH];
    uint8_t cdata[256];

    lr1110_bootloader_fill_cbuffer_cdata_flash( cbuffer, cdata, LR1110_BL_WRITE_FLASH_OC, offset, data, length );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_BL_WRITE_FLASH_CMD_LENGTH, cdata,
                                                 length * sizeof( uint32_t ) );
}

lr1110_status_t lr1110_bootloader_write_flash_full( const void* context, const uint32_t offset, const uint32_t* buffer,
                                                    const uint32_t length )
{
    lr1110_status_t status           = LR1110_STATUS_OK;
    uint32_t        remaining_length = length;
    uint32_t        local_offset     = offset;
    uint32_t        loop             = 0;

    while( ( remaining_length != 0 ) && ( status == LR1110_STATUS_OK ) )
    {
        status = ( lr1110_status_t ) lr1110_bootloader_write_flash( context, local_offset, buffer + loop * 64,
                                                                    min( remaining_length, 64 ) );

        local_offset += LR1110_FLASH_DATA_UINT8_MAX;
        remaining_length = ( remaining_length < 64 ) ? 0 : ( remaining_length - 64 );

        loop++;
    }

    return status;
}

lr1110_status_t lr1110_bootloader_write_flash_encrypted( const void* context, const uint32_t offset,
                                                         const uint32_t* data, uint8_t length )
{
    uint8_t cbuffer[LR1110_BL_WRITE_FLASH_ENCRYPTED_CMD_LENGTH];
    uint8_t cdata[256];

    lr1110_bootloader_fill_cbuffer_cdata_flash( cbuffer, cdata, LR1110_BL_WRITE_FLASH_ENCRYPTED_OC, offset, data,
                                                length );

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_BL_WRITE_FLASH_ENCRYPTED_CMD_LENGTH, cdata,
                                                 length * sizeof( uint32_t ) );
}

lr1110_status_t lr1110_bootloader_write_flash_encrypted_full( const void* context, const uint32_t offset,
                                                              const uint32_t* buffer, const uint32_t length )
{
    lr1110_status_t status           = LR1110_STATUS_OK;
    uint32_t        remaining_length = length;
    uint32_t        local_offset     = offset;
    uint32_t        loop             = 0;

    while( ( remaining_length != 0 ) && ( status == LR1110_STATUS_OK ) )
    {
        status = ( lr1110_status_t ) lr1110_bootloader_write_flash_encrypted( context, local_offset, buffer + loop * 64,
                                                                              min( remaining_length, 64 ) );

        local_offset += LR1110_FLASH_DATA_UINT8_MAX;
        remaining_length = ( remaining_length < 64 ) ? 0 : ( remaining_length - 64 );

        loop++;
    }

    return status;
}

lr1110_status_t lr1110_bootloader_get_hash( const void* context, lr1110_bootloader_hash_t hash )
{
    uint8_t cbuffer[LR1110_BL_GET_HASH_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_GET_HASH_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_GET_HASH_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_BL_GET_HASH_CMD_LENGTH, hash,
                                                LR1110_BL_HASH_LENGTH );
}

lr1110_status_t lr1110_bootloader_reboot( const void* context, const bool stay_in_bootloader )
{
    uint8_t cbuffer[LR1110_BL_REBOOT_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_REBOOT_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_REBOOT_OC >> 0 );

    cbuffer[2] = ( stay_in_bootloader == true ) ? 0x03 : 0x00;

    return ( lr1110_status_t ) lr1110_hal_write( context, cbuffer, LR1110_BL_REBOOT_CMD_LENGTH, 0, 0 );
}

lr1110_status_t lr1110_bootloader_read_pin( const void* context, lr1110_bootloader_pin_t pin )
{
    uint8_t cbuffer[LR1110_BL_GET_PIN_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_GET_PIN_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_GET_PIN_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_BL_GET_PIN_CMD_LENGTH, pin,
                                                LR1110_BL_PIN_LENGTH );
}

lr1110_status_t lr1110_bootloader_read_chip_eui( const void* context, lr1110_bootloader_chip_eui_t chip_eui )
{
    uint8_t cbuffer[LR1110_BL_READ_CHIP_EUI_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_READ_CHIP_EUI_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_READ_CHIP_EUI_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_BL_READ_CHIP_EUI_CMD_LENGTH, chip_eui,
                                                LR1110_BL_CHIP_EUI_LENGTH );
}

lr1110_status_t lr1110_bootloader_read_join_eui( const void* context, lr1110_bootloader_join_eui_t join_eui )
{
    uint8_t cbuffer[LR1110_BL_READ_JOIN_EUI_CMD_LENGTH];

    cbuffer[0] = ( uint8_t )( LR1110_BL_READ_JOIN_EUI_OC >> 8 );
    cbuffer[1] = ( uint8_t )( LR1110_BL_READ_JOIN_EUI_OC >> 0 );

    return ( lr1110_status_t ) lr1110_hal_read( context, cbuffer, LR1110_BL_READ_JOIN_EUI_CMD_LENGTH, join_eui,
                                                LR1110_BL_JOIN_EUI_LENGTH );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

void lr1110_bootloader_fill_cbuffer_opcode_offset_flash( uint8_t* cbuffer, uint16_t opcode, uint32_t offset )
{
    cbuffer[0] = ( uint8_t )( opcode >> 8 );
    cbuffer[1] = ( uint8_t )( opcode >> 0 );

    cbuffer[2] = ( uint8_t )( offset >> 24 );
    cbuffer[3] = ( uint8_t )( offset >> 16 );
    cbuffer[4] = ( uint8_t )( offset >> 8 );
    cbuffer[5] = ( uint8_t )( offset >> 0 );
}

void lr1110_bootloader_fill_cdata_flash( uint8_t* cdata, const uint32_t* data, uint8_t data_length )
{
    for( uint8_t index = 0; index < data_length; index++ )
    {
        uint8_t* cdata_local = &cdata[index * sizeof( uint32_t )];

        cdata_local[0] = ( uint8_t )( data[index] >> 24 );
        cdata_local[1] = ( uint8_t )( data[index] >> 16 );
        cdata_local[2] = ( uint8_t )( data[index] >> 8 );
        cdata_local[3] = ( uint8_t )( data[index] >> 0 );
    }
}

void lr1110_bootloader_fill_cbuffer_cdata_flash( uint8_t* cbuffer, uint8_t* cdata, uint16_t opcode, uint32_t offset,
                                                 const uint32_t* data, uint8_t data_length )
{
    lr1110_bootloader_fill_cbuffer_opcode_offset_flash( cbuffer, opcode, offset );
    lr1110_bootloader_fill_cdata_flash( cdata, data, data_length );
}

/* --- EOF ------------------------------------------------------------------ */
