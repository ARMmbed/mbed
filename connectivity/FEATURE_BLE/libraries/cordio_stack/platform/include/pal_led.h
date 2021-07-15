/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      LED driver definition.
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

#ifndef PAL_LED_H
#define PAL_LED_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_LED
 *  \{ */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Reserved LED IDs. */
enum
{
  /* System signals. */
  PAL_LED_ID_CPU_ACTIVE     = 30,	/*!< CPU active LED ID. */
  PAL_LED_ID_ERROR          = 31  	/*!< Error LED ID. */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalLedInit(void);
void PalLedDeInit(void);

/* Control and Status */
void PalLedOn(uint8_t id);
void PalLedOff(uint8_t id);

/*! \} */    /* PAL_LED */

#ifdef __cplusplus
};
#endif

#endif /* PAL_LED_H */
