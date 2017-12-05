
/** \addtogroup hal */
/** @{*/
/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
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
 */
#ifndef MBED_QSPI_API_H
#define MBED_QSPI_API_H

#include "device.h"

#if DEVICE_QSPI

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup hal_qspi QSPI HAL
 * @{
 */

/** QSPI HAL object declaration
 */
typedef struct qspi_s qspi_t;

/** QSPI Bus width
 * 
 * Some parts of commands provide variable bus width 
 */
typedef enum qspi_bus_width {
    QSPI_CFG_BUS_SINGLE,
    QSPI_CFG_BUS_DUAL,
    QSPI_CFG_BUS_QUAD,
} qspi_bus_width_t;

/** Address size
 */
typedef enum qspi_address_size {
    QSPI_CFG_ADDR_SIZE_NONE,
    QSPI_CFG_ADDR_SIZE_8,
    QSPI_CFG_ADDR_SIZE_16,
    QSPI_CFG_ADDR_SIZE_24,
    QSPI_CFG_ADDR_SIZE_32,
} qspi_address_size_t;

/** Alternative size
 */
typedef enum qspi_alt_size {
    QSPI_CFG_ALT_SIZE_NONE,
    QSPI_CFG_ALT_SIZE_8,
    QSPI_CFG_ALT_SIZE_16,
    QSPI_CFG_ALT_SIZE_24,
    QSPI_CFG_ALT_SIZE_32,
} qspi_alt_size_t;

/** QSPI command declaration
 *
 * Defines a frame format
 */
typedef struct qspi_command {
    struct {
        qspi_bus_width_t bus_width; /**< Bus width for the instruction >*/
        uint8_t value;  /**< Instruction value >*/
    } instruction;
    struct {
        qspi_bus_width_t bus_width; /**< Bus width for the address >*/
        qspi_address_size_t size; /**< Address size >*/
        uint32_t value; /**< Address value >*/
    }  address;
    struct {
        qspi_bus_width_t bus_width; /**< Bus width for alternative  >*/
        qspi_alt_size_t size; /**< Alternative size >*/
        uint32_t value; /**< Alternative, 0 - disabled, non-zero defined value used >*/
    } alt;
    uint8_t dummy_count; /**< Dummy cycles count >*/
    struct {
        qspi_bus_width_t bus_width; /**< Bus width for data >*/
    } data;
} qspi_command_t;

/** QSPI return status
 */
typedef enum qspi_status {
    QSPI_STATUS_ERROR = -1, /**< Generic error >*/
    QSPI_STATUS_INVALID_PARAMETER = -2, /**< The parameter is invalid >*/
    QSPI_STATUS_OK    =  0, /**< Function executed sucessfully  >*/
} qspi_status_t;

/** Initialize QSPI peripheral. 
 *
 * It should initialize QSPI pins (io0-io3, sclk and ssel), set frequency, clock polarity and phase mode. The clock for the peripheral should be enabled
 * 
 * @param obj QSPI object
 * @param io0 Data pin 0
 * @param io1 Data pin 1
 * @param io2 Data pin 2
 * @param io3 Data pin 3
 * @param sclk The clock pin
 * @param ssel The chip select pin
 * @param hz The bus frequency
 * @param mode Clock polarity and phase mode (0 - 3)
 * @return QSPI_STATUS_OK if initialisation successfully executed
           QSPI_STATUS_INVALID_PARAMETER if invalid parameter found
           QSPI_STATUS_ERROR otherwise
 */
qspi_status_t qspi_init(qspi_t *obj, PinName io0, PinName io1, PinName io2, PinName io3, PinName sclk, PinName ssel, uint32_t hz, uint8_t mode);

/** Deinitilize QSPI peripheral
 *
 * It should release pins that are associated with the QSPI object, and disable clocks for QSPI peripheral module that was associated with the object
 *
 * @param obj QSPI object
 * @return QSPI_STATUS_OK if deinitialisation successfully executed
           QSPI_STATUS_INVALID_PARAMETER if invalid parameter found
           QSPI_STATUS_ERROR otherwise
 */
qspi_status_t qspi_free(qspi_t *obj);

/** Set the QSPI baud rate
 *
 * Actual frequency may differ from the desired frequency due to available dividers and the bus clock
 * Configures the QSPI peripheral's baud rate
 * @param obj The SPI object to configure
 * @param hz  The baud rate in Hz
 * @return QSPI_STATUS_OK if frequency was set
           QSPI_STATUS_INVALID_PARAMETER if invalid parameter found
           QSPI_STATUS_ERROR otherwise
 */
qspi_status_t qspi_frequency(qspi_t *obj, int hz);

/** Send a command and block of data
 *
 * @param obj QSPI object
 * @param command QSPI command
 * @param data TX buffer
 * @param[in,out] in - length TX buffer length in bytes, out - number of bytes written
 * @return QSPI_STATUS_OK if the data has been succesfully sent
           QSPI_STATUS_INVALID_PARAMETER if invalid parameter found
           QSPI_STATUS_ERROR otherwise
 */
qspi_status_t qspi_write(qspi_t *obj, const qspi_command_t *command, const void *data, size_t *length);

/** Send a command (and optionally data) and get the response. Can be used to send/receive device specific commands.
 *
 * @param obj QSPI object
 * @param command QSPI command
 * @param tx_data TX buffer
 * @param tx_length pointer to variable holding TX buffer length
 * @param rx_data TX buffer
 * @param rx_length pointer to variable holding TX buffer length
 * @return QSPI_STATUS_OK if the data has been succesfully sent
           QSPI_STATUS_INVALID_PARAMETER if invalid parameter found
           QSPI_STATUS_ERROR otherwise
 */

qspi_status_t qspi_command_transfer(qspi_t *obj, const qspi_command_t *command, const void *tx_data, size_t tx_size, void *rx_data, size_t rx_size); 


/** Receive a command and block of data
 *
 * @param obj QSPI object
 * @param command QSPI command
 * @param data RX buffer
 * @param[in,out] in - length RX buffer length in bytes, out - number of bytes read
 * @return QSPI_STATUS_OK if data has been succesfully received
           QSPI_STATUS_INVALID_PARAMETER if invalid parameter found
           QSPI_STATUS_ERROR otherwise
 */
qspi_status_t qspi_read(qspi_t *obj, const qspi_command_t *command, void *data, size_t *length);

/** Enable memory-mapped mode
 *
 * Enables the external memory being seen as internal. Each target defines 
 * the address space for QSPI. The memory bus access to the defined address space
 * results in QSPI read access
 * 
 * @note If memory-mapped mode is enabled, only read is allowed
 *
 *  @param obj QSPI object
 *  @return QSPI_STATUS_OK if the mode was enabled
            QSPI_STATUS_ERROR otherwise
 */
qspi_status_t qspi_enable_memmap_mode(qspi_t *obj);

/** Disable memory-mapped mode
 * 
 *  @param obj QSPI object
 *  @return QSPI_STATUS_OK if the mode was disabled
            QSPI_STATUS_ERROR otherwise
 */
 qspi_status_t qspi_disable_memmap_mode(qspi_t *obj);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif

#endif

/** @}*/
