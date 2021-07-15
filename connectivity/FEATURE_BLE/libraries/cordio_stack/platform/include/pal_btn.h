/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Button driver definition.
 *
 *  Copyright (c) 2018-2019 ARM Ltd. All Rights Reserved.
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

#ifndef PAL_BTN_H
#define PAL_BTN_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_BUTTON
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Operational states. */
typedef enum
{
  PAL_BTN_STATE_UNINIT = 0,     /*!< Uninitialized state. */
  PAL_BTN_STATE_ERROR  = 0,     /*!< Error state. */
  PAL_BTN_STATE_READY           /*!< Ready state. */
} PalBtnState_t;

/*! \brief      Button position. */
typedef enum
{
  PAL_BTN_POS_INVALID,          /*!< Button position is invalid. */
  PAL_BTN_POS_DOWN,             /*!< Button position is depressed. */
  PAL_BTN_POS_UP                /*!< Button position is released. */
} PalBtnPos_t;

/*! \brief      Action callback signature. */
typedef void (*PalBtnActionCback_t)(uint8_t btnId, PalBtnPos_t state);

/*! \brief      Audio button assignments (only mapped in audio applications). */
enum
{
  PAL_BTN_AUDIO_PLAY = 0x80,    /*!< Play button. */
  PAL_BTN_AUDIO_FWD,            /*!< Fast Forward button. */
  PAL_BTN_AUDIO_RWD,            /*!< Fast Rewind button. */
  PAL_BTN_AUDIO_VOL_UP,         /*!< Volume Up button. */
  PAL_BTN_AUDIO_VOL_DN,         /*!< Volume Down button. */
  PAL_BTN_AUDIO_MUTE,           /*!< Mute button. */
  PAL_BTN_PAUSE                 /*!< Pause button. */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalBtnInit(PalBtnActionCback_t actCback);
void PalBtnDeInit(void);

/* Control and Status */
PalBtnState_t PalBtnGetState(void);
PalBtnPos_t PalBtnGetPosition(uint8_t id);

/*! \} */    /* PAL_BUTTON */

#ifdef __cplusplus
};
#endif

#endif /* PAL_BTN_H */
