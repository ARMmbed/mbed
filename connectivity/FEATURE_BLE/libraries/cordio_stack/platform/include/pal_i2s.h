/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      I2S driver definition.
 *
 *  Copyright (c) 2019 ARM Ltd. All Rights Reserved.
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

#ifndef PAL_I2S_H
#define PAL_I2S_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_I2S
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Operational states. */
typedef enum
{
  PAL_I2S_STATE_UNINIT = 0,         /*!< Uninitialized state. */
  PAL_I2S_STATE_ERROR  = 0,         /*!< Error state. */
  PAL_I2S_STATE_IDLE,               /*!< Initialized state. */
  PAL_I2S_STATE_READY,              /*!< Configured state. */
  PAL_I2S_STATE_BUSY                /*!< Busy state. */
} PalI2sState_t;

/*! \brief      Call signature for next buffer pointer. */
typedef void (*PalI2sCompCback_t)(void *);

/*! \brief      I2S modes. */
typedef enum
{
  PAL_I2S_MODE_MASTER,              /*!< Master mode. */
  PAL_I2S_MODE_SLAVE                /*!< Slave mode. */
} PalI2sMode_t;

/*! \brief      Channels. */
typedef enum
{
  PAL_I2S_CH_LEFT_BIT   = (1 << 0), /*!< Left channel bit. */
  PAL_I2S_CH_RIGHT_BIT  = (1 << 1)  /*!< Right channel bit. */
} PalI2sChanMask_t;

/*! \brief      I2S configuration. */
typedef struct
{
  PalI2sMode_t      mode:8;         /*!< Mode of operation. */
  uint32_t          rate;           /*!< Sample rate. */
  uint8_t           bitDepth;       /*!< Sample bit depth. */
  uint8_t           chan;           /*!< Enabled channels. */
  PalI2sCompCback_t frmCback;       /*!< Get next read buffer callback. */
  void              *pCtx;          /*!< Opaque operation context. */
} PalI2sConfig_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalI2sInit(void);
void PalI2sDeInit(void);

/* Control and Status */
PalI2sState_t PalI2sGetState(void);
void PalI2sConfig(PalI2sConfig_t *pCfg);
void PalI2sDeConfig(void);

/* Data Transfer */
void PalI2sReadData(uint8_t *pData, uint16_t len);
void PalI2sWriteData(const uint8_t *pData, uint16_t len);
void PalI2sReadWriteData(uint8_t *pRdData, const uint8_t *pWrData, uint16_t len);

/*! \} */    /* PAL_I2S */

#ifdef __cplusplus
};
#endif

#endif /* PAL_I2S_H */
