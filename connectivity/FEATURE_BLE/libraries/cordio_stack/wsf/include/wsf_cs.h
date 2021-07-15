/*************************************************************************************************/
/*!
 *  \file   wsf_cs.h
 *
 *  \brief  Critical section macros.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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
#ifndef WSF_CS_H
#define WSF_CS_H

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup WSF_CS_API
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifndef WSF_CS_STATS
/*! \brief  Use CS statistics hooks. */
#define WSF_CS_STATS    FALSE
#endif

/*************************************************************************************************/
/*!
  *  \def    WSF_CS_INIT
  *
  *  \brief  Initialize critical section.  This macro may define a variable.
  *
  *  \param  cs    Critical section variable to be defined.
  */
/*************************************************************************************************/
#define WSF_CS_INIT(cs)

/*************************************************************************************************/
/*!
  *  \def    WSF_CS_ENTER
  *
  *  \brief  Enter a critical section.
  *
  *  \param  cs    Critical section variable.
  */
/*************************************************************************************************/
#define WSF_CS_ENTER(cs)       WsfCsEnter()

/*************************************************************************************************/
/*!
  *  \def    WSF_CS_EXIT
  *
  *  \brief  Exit a critical section.
  *
  *  \param  cs    Critical section variable.
  */
/*************************************************************************************************/
#define WSF_CS_EXIT(cs)        WsfCsExit()

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Get critical section duration watermark level.
 *
 *  \return Critical section duration watermark level.
 */
/*************************************************************************************************/
uint32_t WsfCsStatsGetCsWaterMark(void);

/*************************************************************************************************/
/*!
 *  \brief  Enter a critical section.
 */
/*************************************************************************************************/
void WsfCsEnter(void);

/*************************************************************************************************/
/*!
 *  \brief  Exit a critical section.
 */
/*************************************************************************************************/
void WsfCsExit(void);

/*! \} */    /* WSF_CS_API */

#ifdef __cplusplus
};
#endif

#endif /* WSF_CS_H */
