/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Enhanced attribute protocol client and server API.
 *
 *  Copyright (c) 2019-2020 Arm Ltd. All Rights Reserved.
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
#ifndef EATT_API_H
#define EATT_API_H

#include "wsf_timer.h"
#include "att_defs.h"
#include "att_uuid.h"
#include "dm_api.h"
#include "l2c_api.h"
#include "cfg_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \addtogroup STACK_ATT_API
 *  \{ */
/** \name EATT Role
 * EATT role can be initiator or acceptor.
 */
/**@{*/
#define EATT_ROLE_INITIATOR       L2C_COC_ROLE_INITIATOR
#define EATT_ROLE_ACCEPTOR        L2C_COC_ROLE_ACCEPTOR
/**@}*/
/*! \} */    /* STACK_ATT_API */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \addtogroup STACK_ATT_API
 *  \{ */
/**
 * \name EATT Data Types
 *
 */
/**@{*/
/*! \brief EATT multiple notify tuple structure */
typedef struct
{
  uint16_t handle;                /*!< Attribute handle */
  uint16_t len;                   /*!< Length of pValue in bytes. */
  uint8_t *pValue;                /*!< Attribute value */
} eattTuple_t;
/**@}*/
/*! \} */    /* STACK_ATT_API */

/*! \addtogroup STACK_ATTS_API
 *  \{ */
/** \name EATT Server Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Send multiple attribute protocol Handle Value Notification.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattsMultiValueNtf(dmConnId_t connId, uint8_t priority, uint16_t numTuples, eattTuple_t *pTupleList);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattsHandleValueInd(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen,
                         uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattsHandleValueNtf(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen,
                         uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattsHandleValueIndZeroCpy(dmConnId_t connId, uint8_t priority, uint16_t handle,
                                uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattsHandleValueNtfZeroCpy(dmConnId_t connId, uint8_t priority, uint16_t handle,
                                uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Initialize the Enhanced ATT Server.
 *
 *  \return None
 *
 *  \note   EattInit must be called after the ATT task is created and before calling EattsInit 
 *          and/or EattcInit.
 */
 /*************************************************************************************************/
void EattsInit();
/**@}*/
/*! \} */    /* STACK_ATTS_API */

/*! \addtogroup STACK_ATTC_API
 *  \{ */
/** \name EATT Client Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find Information Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcFindInfoReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                      bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find By Type Value Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuid16      16-bit UUID to find.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcFindByTypeValueReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                             uint16_t uuid16, uint16_t valueLen, uint8_t *pValue, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read By Type Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuidLen     Length of UUID (2 or 16).
 *  \param  pUuid       Pointer to UUID data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadByTypeReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                        uint8_t uuidLen, uint8_t *pUuid, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  priority  Operation priority.
 *  \param  handle    Attribute handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadReq(dmConnId_t connId, uint8_t priority, uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  offset      Read attribute data starting at this offset.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadLongReq(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t offset, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Multiple Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  numHandles  Number of handles in attribute handle list.
 *  \param  pHandles    List of attribute handles.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadMultipleReq(dmConnId_t connId, uint8_t priority, uint8_t numHandles, uint16_t *pHandles);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read By Group Type Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuidLen     Length of UUID (2 or 16).
 *  \param  pUuid       Pointer to UUID data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadByGroupTypeReq(dmConnId_t connId, uint8_t priority, uint16_t startHandle, uint16_t endHandle,
                            uint8_t uuidLen, uint8_t *pUuid, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcWriteReq(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen,
                   uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Cancel an attribute protocol request in progress.
 *
 *  \param  connId    DM connection ID.
 *  \param  priority  Operation priority.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcCancelReq(dmConnId_t connId, uint8_t priority);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol indication confirmation.
 *
 *  \param  connId    DM connection ID.
 *  \param  cid       L2Cap channel ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcIndConfirm(dmConnId_t connId, uint16_t cid);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Command.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcWriteCmd(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t valueLen,
                   uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Prepare Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  handle      Attribute handle.
 *  \param  offset      Write attribute data starting at this offset.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  valueByRef  TRUE if pValue data is accessed by reference rather than copied.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcPrepareWriteReq(dmConnId_t connId, uint8_t priority, uint16_t handle, uint16_t offset,
                          uint16_t valueLen, uint8_t *pValue, bool_t valueByRef, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Execute Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  writeAll    TRUE to write all queued writes, FALSE to cancel all queued writes.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcExecuteWriteReq(dmConnId_t connId, uint8_t priority, bool_t writeAll);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Multiple Variable Length Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  priority    Operation priority.
 *  \param  numHandles  The number of handles in pHandles.
 *  \param  pHandles    List of attribute handles to read.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattcReadMultVarLenReq(dmConnId_t connId, uint8_t priority, uint8_t numHandles, uint16_t *pHandles);

/*************************************************************************************************/
/*!
 *  \brief  Initialize the Enhanced ATT Client.
 *
 *  \return None
 *
 *  \note   EattInit must be called after the ATT task is created and before calling EattsInit 
 *          and/or EattcInit.
 */
 /*************************************************************************************************/
void EattcInit();
/**@}*/
/*! \} */    /* STACK_ATTC_API */

/*! \addtogroup STACK_ATT_API
 *  \{ */
/** \name EATT Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Begin requesting EATT L2CAP coc channels.
 *
 *  \note   When pEattCfg->initiateEatt is TRUE, EattEstablishChannels is called automatically
 *          on DM_CONN_OPEN_IND. If pEattCfg->initiateEatt is FALSE, EattEstablishChannels can be
 *          called by the application after DM_CONN_OPEN_IND to begin creating EATT channels.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void EattEstablishChannels(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \brief  Returns the number of open EATT channels on a given connection.
 *
 *  \param  connId  DM connection identifier.
 *
 *  \return Number of open EATT channels.
 */
/*************************************************************************************************/
uint8_t EattGetNumChannelsInUse(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \brief  Initialize the Enhanced ATT subsystem.
 *
 *  \return None
 *
 *  \note   EattInit must be called after the ATT task is created and before calling EattsInit 
 *          and/or EattcInit.
 */
 /*************************************************************************************************/
void EattInit(uint8_t roleBits);
/**@}*/
/*! \} */    /* STACK_ATT_API */

#ifdef __cplusplus
};
#endif

#endif /* EATT_API_H */
