/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      I/O expander driver definition.
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

#ifndef PAL_IO_EXP_H
#define PAL_IO_EXP_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_IO_EXP
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Invalid device ID. */
#define PAL_IO_EXP_INVALID_ID 0xFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Read register completion callback. */
typedef void (*PalIoExpReadCompCback_t)(uint32_t value);

/*! \brief      Write register completion callback. */
typedef void (*PalIoExpWriteCompCback_t)(void);

/*! \brief      Device configuration. */
typedef struct
{
  PalIoExpReadCompCback_t rdCback;  /*!< Read data completion callback. */
  PalIoExpWriteCompCback_t wrCback; /*!< Write data completion callback. */
  uint32_t inputMask;               /*!< Bitmask for input pins (1 for input, 0 for output). */
  uint8_t addr;                     /*!< Device address defined by the voltage on the board. */
} PalIoExpConfig_t;

/*! \brief      Operational states. */
typedef enum
{
  PAL_IO_EXP_STATE_UNINIT = 0,  /*!< Uninitialized state. */
  PAL_IO_EXP_STATE_ERROR  = 0,  /*!< Error state. */
  PAL_IO_EXP_STATE_READY  = 1,  /*!< Ready state. */
  PAL_IO_EXP_STATE_BUSY   = 2,  /*!< Busy state. */
} PalIoExpState_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
uint8_t PalIoExpInit(PalIoExpConfig_t *pCfg);
void PalIoExpDeInit(void);

/* Control and Status */
PalIoExpState_t PalIoExpGetState(uint8_t id);

/* Data transfer */
void PalIoExpRead(uint8_t id);
void PalIoExpWrite(uint8_t id, uint32_t value);

/*! \} */    /* PAL_IO_EXP */

#ifdef __cplusplus
};
#endif

#endif /* PAL_IO_EXP_H */
