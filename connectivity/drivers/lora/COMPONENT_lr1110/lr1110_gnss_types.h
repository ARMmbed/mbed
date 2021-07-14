/*!
 * @file      lr1110_gnss_types.h
 *
 * @brief     GNSS scan driver types for LR1110
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

#ifndef LR1110_GNSS_TYPES_H
#define LR1110_GNSS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*!
 * @brief Maximal buffer size
 */
#define LR1110_GNSS_MAX_SIZE_ARRAY 2820  //!< (128sv * 22bytes + 4bytes for CRC)

/*!
 * @brief Number of almanacs in full update payload
 */
#define LR1110_GNSS_FULL_UPDATE_N_ALMANACS ( 128 )

/*!
 * @brief Size of the almanac of a single satellite when reading
 */
#define LR1110_GNSS_SINGLE_ALMANAC_READ_SIZE ( 22 )

/*!
 * @brief Size of the almanac of a single satellite when writing
 */
#define LR1110_GNSS_SINGLE_ALMANAC_WRITE_SIZE ( 20 )

/*!
 * @brief Size of the almanac of the GNSS context status buffer
 */
#define LR1110_GNSS_CONTEXT_STATUS_LENGTH ( 7 )

#define LR1110_GNSS_FULL_ALMANAC_WRITE_BUFFER_SIZE \
    ( ( LR1110_GNSS_FULL_UPDATE_N_ALMANACS * LR1110_GNSS_SINGLE_ALMANAC_WRITE_SIZE ) + 20 )

#define LR1110_GNSS_FULL_ALMANAC_READ_BUFFER_SIZE \
    ( ( LR1110_GNSS_FULL_UPDATE_N_ALMANACS * LR1110_GNSS_SINGLE_ALMANAC_READ_SIZE ) + 4 )

#define LR1110_GNSS_DMC_ALMANAC_UPDATE_POS ( 1U )
#define LR1110_GNSS_DMC_ALMANAC_UPDATE_GPS_MASK ( 0x01UL << LR1110_GNSS_DMC_ALMANAC_UPDATE_POS )
#define LR1110_GNSS_DMC_ALMANAC_UPDATE_BEIDOU_MASK ( 0x02UL << LR1110_GNSS_DMC_ALMANAC_UPDATE_POS )

#define LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_MSB_POS ( 0U )
#define LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_MSB_MASK ( 0x01UL << LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_MSB_POS )

#define LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_LSB_POS ( 7U )
#define LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_LSB_MASK ( 0x01UL << LR1110_GNSS_DMC_FREQUENCY_SEARCH_SPACE_LSB_POS )

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*!
 * @brief Satellite ID type
 */
typedef uint8_t lr1110_gnss_satellite_id_t;

/*!
 * @brief bit mask indicating which information is added in the output payload
 */
enum lr1110_gnss_input_parameters_e
{
    LR1110_GNSS_IRQ_PSEUDO_RANGE_MASK = ( 1 << 0 ),
    LR1110_GNSS_DOPPLER_MASK          = ( 1 << 1 ),
    LR1110_GNSS_BIT_CHANGE_MASK       = ( 1 << 2 ),
};

/*!
 * @brief Constellation identifiers
 */
typedef enum
{
    LR1110_GNSS_GPS_MASK    = 0x01,
    LR1110_GNSS_BEIDOU_MASK = 0x02,
} lr1110_gnss_constellation_t;

/*!
 * @brief Bit mask of constellation configurations
 *
 * @see lr1110_gnss_constellation_t
 */
typedef uint8_t lr1110_gnss_constellation_mask_t;

/*!
 * @brief Search mode for GNSS scan
 */
typedef enum
{
    LR1110_GNSS_OPTION_DEFAULT     = 0x00,  //!< Search all requested satellites or fail
    LR1110_GNSS_OPTION_BEST_EFFORT = 0x01,  //!< Add additional search if not all satellites are found
} lr1110_gnss_search_mode_t;

/*!
 * @brief GNSS response type indicates the destination: Host MCU, GNSS solver or
 * GNSS DMC
 */
typedef enum
{
    LR1110_GNSS_DESTINATION_HOST   = 0x00,  //!< Host MCU
    LR1110_GNSS_DESTINATION_SOLVER = 0x01,  //!< GNSS Solver
    LR1110_GNSS_DESTINATION_DMC    = 0x02,  //!< GNSS DMC
} lr1110_gnss_destination_t;

/*!
 * @brief Message to host indicating the status of the message
 */
typedef enum
{
    LR1110_GNSS_HOST_OK                                         = 0x00,
    LR1110_GNSS_HOST_UNEXPECTED_CMD                             = 0x01,
    LR1110_GNSS_HOST_UNIMPLEMENTED_CMD                          = 0x02,
    LR1110_GNSS_HOST_INVALID_PARAMETERS                         = 0x03,
    LR1110_GNSS_HOST_MESSAGE_SANITY_CHECK_ERROR                 = 0x04,
    LR1110_GNSS_HOST_IQ_CAPTURE_FAILS                           = 0x05,
    LR1110_GNSS_HOST_NO_TIME                                    = 0x06,
    LR1110_GNSS_HOST_NO_SATELLITE_DETECTED                      = 0x07,
    LR1110_GNSS_HOST_ALMANAC_IN_FLASH_TOO_OLD                   = 0x08,
    LR1110_GNSS_HOST_ALMANAC_UPDATE_FAILS_CRC_ERROR             = 0x09,
    LR1110_GNSS_HOST_ALMANAC_UPDATE_FAILS_FLASH_INTEGRITY_ERROR = 0x0A,
    LR1110_GNSS_HOST_ALMANAC_UPDATE_FAILS_ALMANAC_DATE_TOO_OLD  = 0x0B,
} lr1110_gnss_message_host_status_t;

/*!
 * @brief GNSS single or double scan mode
 */
typedef enum
{
    LR1110_GNSS_SINGLE_SCAN_MODE = 0x00,
    LR1110_GNSS_DOUBLE_SCAN_MODE = 0x01,
} lr1110_gnss_scan_mode_t;

/*!
 * @brief GNSS error codes
 */
typedef enum lr1110_gnss_error_code_e
{
    LR1110_GNSS_NO_ERROR                                  = 0,
    LR1110_GNSS_ERROR_ALMANAC_TOO_OLD                     = 1,
    LR1110_GNSS_ERROR_UPDATE_CRC_MISMATCH                 = 2,
    LR1110_GNSS_ERROR_UPDATE_FLASH_MEMORY_INTEGRITY       = 3,
    LR1110_GNSS_ERROR_UPDATE_TIME_DIFFERENCE_OVER_1_MONTH = 4,
} lr1110_gnss_error_code_t;

/*!
 * @brief GNSS frequency search space
 */
typedef enum lr1110_gnss_freq_search_space_e
{
    LR1110_GNSS_FREQUENCY_SEARCH_SPACE_250_HZ = 0,
    LR1110_GNSS_FREQUENCY_SEARCH_SPACE_500_HZ = 1,
    LR1110_GNSS_FREQUENCY_SEARCH_SPACE_1_KHZ  = 2,
    LR1110_GNSS_FREQUENCY_SEARCH_SPACE_2_KHZ  = 3,
} lr1110_gnss_freq_search_space_t;

/*!
 * @brief Representation of absolute time for GNSS operations
 *
 * The GNSS absolute time is represented as a 32 bits word that is the number of
 * seconds elapsed since January 6th 1980, 00:00:00
 *
 * The GNSS absolute time must take into account the Leap Seconds between UTC
 * time and GPS time.
 */
typedef uint32_t lr1110_gnss_date_t;

/*!
 * @brief Buffer that holds data for one satellite almanac update
 */
typedef uint8_t lr1110_gnss_almanac_single_satellite_update_bytestram_t[LR1110_GNSS_SINGLE_ALMANAC_WRITE_SIZE];

/*!
 * @brief Buffer that holds data for all almanacs full update - when writing
 */
typedef uint8_t lr1110_gnss_almanac_full_update_bytestream_t[LR1110_GNSS_FULL_ALMANAC_WRITE_BUFFER_SIZE];

/*!
 * @brief Buffer that holds data for all almanacs full update - when reading
 */
typedef uint8_t lr1110_gnss_almanac_full_read_bytestream_t[LR1110_GNSS_FULL_ALMANAC_READ_BUFFER_SIZE];

/*!
 * @brief Buffer that holds data for context status
 */
typedef uint8_t lr1110_gnss_context_status_bytestream_t[LR1110_GNSS_CONTEXT_STATUS_LENGTH];

/*!
 * @brief Assistance position.
 */
typedef struct lr1110_gnss_solver_assistance_position_s
{
    float latitude;   //!< Latitude 12 bits (latitude in degree * 2048/90) with
                      //!< resolution 0.044°
    float longitude;  //!< Longitude 12 bits (longitude in degree * 2048/180)
                      //!< with resolution 0.088°
} lr1110_gnss_solver_assistance_position_t;

/*!
 * @brief Detected satellite structure
 */
typedef struct lr1110_gnss_detected_satellite_s
{
    lr1110_gnss_satellite_id_t satellite_id;
    int8_t                     cnr;  //!< Carrier-to-noise ration (C/N) in dB
} lr1110_gnss_detected_satellite_t;

/*!
 * @brief GNSS timings of the LR1110
 */
typedef struct lr1110_gnss_timings_s
{
    uint32_t radio_ms;
    uint32_t computation_ms;
} lr1110_gnss_timings_t;

/*!
 * @brief Version structure of the LR1110 GNSS firmware
 */
typedef struct lr1110_gnss_version_s
{
    uint8_t gnss_firmware;  //!< Version of the firmware
    uint8_t gnss_almanac;   //!< Version of the almanac format
} lr1110_gnss_version_t;

/*!
 * @brief Structure for GNSS context status
 */
typedef struct lr1110_gnss_context_status_s
{
    uint8_t                         firmware_version;
    uint32_t                        global_almanac_crc;
    lr1110_gnss_error_code_t        error_code;
    bool                            almanac_update_gps;
    bool                            almanac_update_beidou;
    lr1110_gnss_freq_search_space_t freq_search_space;
} lr1110_gnss_context_status_t;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

#ifdef __cplusplus
}
#endif

#endif  // LR1110_GNSS_TYPES_H

/* --- EOF ------------------------------------------------------------------ */
