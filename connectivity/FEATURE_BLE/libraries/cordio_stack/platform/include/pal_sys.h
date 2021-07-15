/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  System hooks.
 *
 *  Copyright (c) 2016-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
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

#ifndef PAL_SYS_H
#define PAL_SYS_H

#include "pal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup PAL_SYS
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifdef DEBUG

/*! \brief      Error check routines; for use with PAL implementation only. */
#define PAL_SYS_ASSERT(expr)    { if (!(expr)) { PalSysAssertTrap(); } }

#else

/*! \brief      Error check routines (disabled). */
#define PAL_SYS_ASSERT(expr)

#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void PalSysInit(void);

/* Diagnostics */
void PalSysAssertTrap(void);
void PalSysSetTrap(bool_t enable);
uint32_t PalSysGetAssertCount(void);
uint32_t PalSysGetStackUsage(void);
bool_t PalSysDiagTraceWrite(const uint8_t *pBuf, uint32_t len);

/* Power Management */
void PalSysSleep(uint32_t nextWakeMs);
void PalSysWakeFromDeepSleep(void);

/* Critical Section */
void PalSysCsEnter(void);
void PalSysCsExit(void);

/*! \} */    /* PAL_SYS */

#ifdef __cplusplus
};
#endif

#endif  /* PAL_SYS_H */
