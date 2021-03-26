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
#include "mbed_error.h"
#include "mbed_debug.h"
#include "cmsis.h"
#include "qspi_api.h"
#include "s5js100_hal_qspi.h"
#include "sflash_api.h"

HAL_StatusTypeDef HAL_QSPI_Init(void)
{
    HAL_StatusTypeDef status = HAL_OK;
#if 0

#endif
    return status;
}

HAL_StatusTypeDef HAL_QSPI_DeInit(void)
{
    HAL_StatusTypeDef status = HAL_OK;
#if 0

#endif
    return status;
}

HAL_StatusTypeDef HAL_QSPI_CommandReceive(const qspi_command_t *command, uint8_t *pData, size_t *length, unsigned int Timeout)
{
    HAL_StatusTypeDef status = HAL_OK;
    if (command->instruction.disabled) {

    } else {

    }
    switch (command->instruction.value) {
        case FLASH_CMD_RDSR:
            (*pData) = (getreg32(S5JS100_SFLASH_BASE + 0xDC) & 0xff);// getreg8(S5JS100_SFLASH_RDSR);
            break;

        case FLASH_CMD_SE:
            up_progmem_erasepage((S5JS100_FLASH_QSPI_TEST_PADDR - S5JS100_FLASH_PADDR + command->address.value) / 256);
            break;

        case FLASH_CMD_PP: //Page Program
            up_progmem_write((S5JS100_FLASH_QSPI_TEST_PADDR + command->address.value), pData, *length);
            break;

        case FLASH_CMD_READ: //read data
            memcpy(pData, (uint8_t *)(S5JS100_FLASH_QSPI_TEST_PADDR + command->address.value), *length);
            break;

        case FLASH_CMD_WRSR:
            putreg8(1, S5JS100_SFLASH_WREN);
            putreg8(pData[0], S5JS100_SFLASH_WRSR);
            break;

        default:
            break;
    }
    return status;
}

#endif

