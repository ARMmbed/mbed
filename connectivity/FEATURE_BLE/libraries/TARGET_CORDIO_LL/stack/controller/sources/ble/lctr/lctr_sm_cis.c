/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller CIS main state machine implementation file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_cis.h"
#include "lctr_api_conn.h"
#include "sch_api.h"
#include "wsf_assert.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Get remapped event.
 *
 *  \param      pCisCtx CIS Context.
 *  \param      event   Subsystem event.
 *
 *  \return     Procedure event ID.
 */
/*************************************************************************************************/
static uint8_t lctrRemapEvent(lctrCisCtx_t *pCisCtx, uint8_t event)
{
  switch (event)
  {
    /*** Peer messages ***/

    /*** Host messages ***/

    case LCTR_CONN_MSG_API_DISCONNECT:
    {
      pCisCtx->hostInitTerm = TRUE;
      lctrCisStoreDisconnectReason(pCisCtx);
      return LCTR_CIS_MSG_CIS_DISC;
    }

    /*** Internal messages ***/
    case LCTR_CONN_TERM_SUP_TIMEOUT:
    {
      lctrCisStoreConnTimeoutTerminateReason(pCisCtx);
      return LCTR_CIS_MSG_CIS_CONN_FAIL;
    }
    case LCTR_CIS_MSG_CIS_TERM_MIC_FAILED:
    {
      lctrCisStoreMicFailedTerminateReason(pCisCtx);
      return LCTR_CIS_MSG_CIS_CONN_FAIL;
    }
    case LCTR_CIS_MSG_CIS_EST:
      return LCTR_CIS_MSG_CIS_EST;
    case LCTR_CIS_MSG_CIS_EST_FAIL:
      return LCTR_CIS_MSG_CIS_EST_FAIL;
    case LCTR_CIS_MSG_CIS_CLOSED:
      return LCTR_CIS_MSG_CIS_CLOSED;

    default:
      break;
  }
  return LCTR_CIS_MSG_INVALID;
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Execute CIS main state machine.
 *
 *  \param      pCisCtx         CIS  context.
 *  \param      event           State machine event.
 *************************************************************************************************/
void lctrCisExecuteSm(lctrCisCtx_t *pCisCtx, uint8_t event)
{
  if ((event = lctrRemapEvent(pCisCtx, event)) == LCTR_CIS_MSG_INVALID)
  {
    return;
  }

  LL_TRACE_INFO3("lctrCisExecuteSm: handle=%d state=%u, event=%u", pCisCtx->cisHandle, pCisCtx->state, event);

  switch (pCisCtx->state)
  {
    case LCTR_CIS_STATE_IDLE:
      switch (event)
      {
        case LCTR_CIS_MSG_CIS_EST:
          lctrCisActCisEst(pCisCtx);
          pCisCtx->state = LCTR_CIS_STATE_EST;
          break;
        case LCTR_CIS_MSG_CIS_EST_FAIL:
          lctrCisActCisEstFail(pCisCtx);
          pCisCtx->state = LCTR_CIS_STATE_IDLE;
          break;
        default:
          LL_TRACE_INFO1("Invalid event in the LCTR_CIS_STATE_IDLE, event=%u", event);
          break;
      }
      break;

    case LCTR_CIS_STATE_EST:
      switch (event)
      {
        case LCTR_CIS_MSG_CIS_DISC:
          lctrCisActDisc(pCisCtx);
          pCisCtx->state = LCTR_CIS_STATE_SHUTDOWN;
          break;
        case LCTR_CIS_MSG_CIS_CONN_FAIL:
          lctrCisActFail(pCisCtx);
          pCisCtx->state = LCTR_CIS_STATE_IDLE;
          break;
        case LCTR_CIS_MSG_CIS_CLOSED:
          lctrCisActClosed(pCisCtx);
          pCisCtx->state = LCTR_CIS_STATE_IDLE;
          break;

        default:
          LL_TRACE_INFO1("Invalid event in the LCTR_CIS_STATE_EST, event=%u", event);
          break;
      }
      break;

      case LCTR_CIS_STATE_SHUTDOWN:
        switch (event)
        {
          case LCTR_CIS_MSG_CIS_CLOSED:
            lctrCisActClosed(pCisCtx);
            pCisCtx->state = LCTR_CIS_STATE_IDLE;
            break;

          default:
            LL_TRACE_INFO1("Invalid event in the LCTR_CIS_STATE_SHUTDOWN, event=%u", event);
            break;
        }
        break;

    default:
      break;
  }
}
