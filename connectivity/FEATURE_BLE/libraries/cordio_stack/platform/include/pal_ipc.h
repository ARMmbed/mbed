/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      IPC driver definition.
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

#ifndef PAL_IPC_H
#define PAL_IPC_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_IPC
 *  \{ */

/**************************************************************************************************
  Type Definitions
**************************************************************************************************/

/*! \brief      Operational states. */
typedef enum
{
  PAL_IPC_STATE_UNINIT = 0,         /*!< Uninitialized state. */
  PAL_IPC_STATE_ERROR  = 0,         /*!< Error state. */
  PAL_IPC_STATE_IDLE,               /*!< Initialized state. */
  PAL_IPC_STATE_BUSY                /*!< Busy state. */
} PalIpcState_t;

typedef void (*PalIpcMsgHandler_t)(uint8_t *pBuf, uint16_t len);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalIpcInit(PalIpcMsgHandler_t recvCback);
void PalIpcDeInit(void);

/* Control and Status */
PalIpcState_t PalIpcGetState(void);

/* Data Transfer */
void PalIpcWriteData(const uint8_t *pData, uint16_t len);

/*! \} */    /* PAL_IPC */

#ifdef __cplusplus
};
#endif

#endif /* PAL_IPC_H */
