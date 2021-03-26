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

#if DEVICE_QSPI

#include "qspi_api.h"
#include "mbed_error.h"
#include "mbed_debug.h"
#include "cmsis.h"
#include "pinmap.h"

#define QSPI_FLASH_SIZE_DEFAULT 0x80000000
#define HAL_QSPI_TIMEOUT_DEFAULT_VALUE       ((uint32_t)5000U)

static uint32_t get_alt_bytes_size(const uint32_t num_bytes)
{
    return 0xFFFFFFFF;
}

qspi_status_t qspi_init(qspi_t *obj, PinName io0, PinName io1, PinName io2, PinName io3, PinName sclk, PinName ssel, uint32_t hz, uint8_t mode)
{
    return QSPI_STATUS_OK;
}

qspi_status_t qspi_free(qspi_t *obj)
{
    return QSPI_STATUS_OK;
}

qspi_status_t qspi_frequency(qspi_t *obj, int hz)
{
    qspi_status_t status = QSPI_STATUS_OK;
    return status;
}

qspi_status_t qspi_write(qspi_t *obj, const qspi_command_t *command, const void *data, size_t *length)
{
    if (HAL_QSPI_CommandReceive(command, data, length, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        status = QSPI_STATUS_ERROR;
    }

    return status;
}

qspi_status_t qspi_read(qspi_t *obj, const qspi_command_t *command, void *data, size_t *length)
{
    qspi_status_t status = QSPI_STATUS_OK;

    if (HAL_QSPI_CommandReceive(command, data, length, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        status = QSPI_STATUS_ERROR;
    }
    return status;
}

qspi_status_t qspi_command_transfer(qspi_t *obj, const qspi_command_t *command, const void *tx_data, size_t tx_size, void *rx_data, size_t rx_size)
{

    qspi_status_t status = QSPI_STATUS_OK;

    if ((tx_data == NULL || tx_size == 0) && (rx_data == NULL || rx_size == 0)) {
        if (HAL_QSPI_CommandReceive(command, 0, 0, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
            status = QSPI_STATUS_ERROR;
            return status;
        }
    } else {
        // often just read a register, check if we need to transmit anything prior reading
        if (tx_data != NULL && tx_size) {
            size_t tx_length = tx_size;
            status = qspi_write(obj, command, tx_data, &tx_length);
        }

        if (rx_data != NULL && rx_size) {
            size_t rx_length = rx_size;
            status = qspi_read(obj, command, rx_data, &rx_length);
        }
    }
    return status;
}

const PinMap *qspi_master_sclk_pinmap()
{
    return 0;//PinMap_QSPI_SCLK;
}

const PinMap *qspi_master_ssel_pinmap()
{
    return 0;//PinMap_QSPI_SSEL;
}

const PinMap *qspi_master_data0_pinmap()
{
    return 0;//PinMap_QSPI_DATA0;
}

const PinMap *qspi_master_data1_pinmap()
{
    return 0;//PinMap_QSPI_DATA1;
}

const PinMap *qspi_master_data2_pinmap()
{
    return 0;//PinMap_QSPI_DATA2;
}

const PinMap *qspi_master_data3_pinmap()
{
    return 0;//PinMap_QSPI_DATA3;
}

#endif

/** @}*/
