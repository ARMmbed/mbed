/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  PAL flash driver.
 * 
 *  Copyright (c) 2018-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

#ifndef PAL_FLASH_H
#define PAL_FLASH_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_FLASH
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Operational states. */
typedef enum
{
  PAL_FLASH_STATE_UNINIT = 0,   /*!< Uninitialized state. */
  PAL_FLASH_STATE_ERROR  = 0,   /*!< Error state. */
  PAL_FLASH_STATE_READY,        /*!< Ready state. */
  PAL_FLASH_STATE_BUSY          /*!< Busy state. */
} PalFlashState_t;

/*! \brief      Partition. */
typedef enum
{
  PAL_FLASH_PART_NVM = 0,       /*!< Non-Volatile Memory partition. */
  PAL_FLASH_PART_FOTA_FW,       /*!< Firmware image partition. */
  PAL_FLASH_PART_FOTA_AUTH,     /*!< Firmware image authentication partition. */
  PAL_FLASH_PART_MAX            /*!< Maximum number of partitions. */
} PalFlashPartition_t;

/*! \brief      Operation completion callback (may be called in interrupt context).. */
typedef void (*PalFlashCback_t)(void);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalFlashInit(void);
void PalFlashDeInit(void);

/* Control and Status */
PalFlashState_t PalFlashGetState(PalFlashPartition_t part);
uint32_t PalFlashGetTotalSize(PalFlashPartition_t part);
uint32_t PalFlashGetSectorSize(PalFlashPartition_t part);
bool_t PalFlashService(void);

/* Data Transfer */
void PalFlashRead(PalFlashPartition_t part, void *pBuf, uint32_t size, uint32_t offset, 
                  PalFlashCback_t compCback);
void PalFlashWrite(PalFlashPartition_t part, void *pBuf, uint32_t size, 
                   uint32_t offset, PalFlashCback_t compCback);
void PalFlashEraseSector(PalFlashPartition_t part, uint32_t size, uint32_t offset, 
                         PalFlashCback_t compCback);

/*! \} */    /* PAL_FLASH */

#ifdef __cplusplus
};
#endif

#endif  /* PAL_FLASH_H */
