/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) slave control interface implementation file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_api_conn.h"
#include "lmgr_api.h"
#include "wsf_msg.h"
#include "wsf_trace.h"

/*************************************************************************************************/
/*!
 *  \brief      Read remote features.
 *
 *  \param      handle          Connection handle.
 *
 *  \return     Status error code.
 *
 *  Read the link layer features of the remote device.
 */
/*************************************************************************************************/
uint8_t LlReadRemoteFeat(uint16_t handle)
{
  lctrMsgHdr_t *pMsg;

  LL_TRACE_INFO1("### LlApi ###  LlReadRemoteFeat, handle=%u", handle);

  if (LctrIsProcActPended(handle, LCTR_CONN_MSG_API_REMOTE_FEATURE) == TRUE)
  {

    /* The process can already be pended when the connection starts up if
     * host sends this command right when connection establishes if
     * LL_OP_MODE_FLAG_ENA_FEAT_LLCP_STARTUP is enabled.
     */
    if (LctrIsConnHandleEnabled(handle) &&
        !LctrIsFeatExchHostInit(handle))
    {
      LctrSetHostNotifyFeatExch(handle);
      return LL_SUCCESS;
    }

    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if ((LL_API_PARAM_CHECK == 1) &&
       ((handle >= pLctrRtCfg->maxConn) ||
       !LctrIsConnHandleEnabled(handle)))
  {
    return LL_ERROR_CODE_UNKNOWN_CONN_ID;
  }

  if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = handle;
    pMsg->dispId = LCTR_DISP_CONN;
    pMsg->event  = LCTR_CONN_MSG_API_REMOTE_FEATURE;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}
