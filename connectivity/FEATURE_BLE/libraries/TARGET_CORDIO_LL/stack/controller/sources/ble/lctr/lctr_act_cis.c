/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master connected isochronous stream state machine action routines.
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
#include "lctr_int_iso.h"
#include "lmgr_api_cis_master.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Send next CIS disconnect LLCP to be processed, if needed.
 *
 *  \param      ACL Handle that can continue CIS termination procedures.
 */
/*************************************************************************************************/
static void lctrCheckPendingCisTermination(uint16_t aclHandle)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxCis; i++)
  {
    lctrCisCtx_t *pCisCtx = &pLctrCisTbl[i];
    lctrCisDisc_t *pMsg;

    if ((pCisCtx->aclHandle == aclHandle) &&
        !pCisCtx->isClosing && pCisCtx->termPend)
    {
      if ((pMsg = (lctrCisDisc_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        pMsg->hdr.handle = pCisCtx->aclHandle;
        pMsg->hdr.dispId = LCTR_DISP_CONN;
        pMsg->hdr.event = LCTR_CONN_MSG_API_DISCONNECT;

        pMsg->cisHandle =  pCisCtx->cisHandle;
        pMsg->reason = pCisCtx->reason;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);

        pCisCtx->termPend = FALSE;
      }
      else
      {
        LL_TRACE_WARN1("lctrCisActDisc: CIS OOM while terminating handle %d", pCisCtx->cisHandle);
      }

      return;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send CIS termination indication PDU.
 *
 *  \param      pCtx        Connection context.
 *  \param      opcode      Pdu Opcode.
 *  \param      pCisCtx     CIS context.
 *  \param      reason      Termination reason.
 */
/*************************************************************************************************/
static void lctrSendCisTermIndPdu(lctrConnCtx_t *pCtx, uint8_t opcode, lctrCisCtx_t *pCisCtx, uint8_t reason)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_CIS_TERM_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/
    UINT8_TO_BSTREAM (pBuf, opcode);
    UINT8_TO_BSTREAM (pBuf, pCisCtx->cigId);
    UINT8_TO_BSTREAM (pBuf, pCisCtx->cisId);
    UINT8_TO_BSTREAM (pBuf, reason);

    /*** Queue for transmit. ***/
    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send CIS termination indication.
 *
 *  \param      pCtx        Connection context.
 *  \param      pCisCtx     CIS context.
 *  \param      reason      Termination reason.
 */
/*************************************************************************************************/
static void lctrSendCisTermInd(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx, uint8_t reason)
{
  lctrSendCisTermIndPdu(pCtx, LL_PDU_CIS_TERM_IND, pCisCtx, reason);
}

/**************************************************************************************************
  External Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Notify host of CIS disconnected event
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrNotifyHostCisTerm(lctrCisCtx_t *pCisCtx)
{
  LlDisconnectInd_t evt =
  {
    .hdr =
    {
      .param  = pCisCtx->cisHandle,
      .event  = LL_DISCONNECT_IND,
      .status = LL_SUCCESS
    },

    .status   = LL_SUCCESS,
    .handle   = pCisCtx->cisHandle,
    .reason   = pCisCtx->reason
  };

  LL_TRACE_INFO2("### LlEvent ###  LL_DISCONNECT_IND, handle=%u, status=LL_SUCCESS, reason=%u", pCisCtx->cisHandle, pCisCtx->reason);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of CIS established event
 *
 *  \param      pCisCtx           CIS context.
 *  \param      status            Status.
 *  \param      cigSyncDelayUsec  CIG synchronization delayn in usec.
 */
/*************************************************************************************************/
void lctrNotifyHostCisEst(lctrCisCtx_t *pCisCtx, uint8_t status, uint32_t cigSyncDelayUsec)
{
  LlCisEstInd_t evt =
  {
    .hdr =
    {
      .param  = pCisCtx->cisHandle,
      .event  = LL_CIS_EST_IND,
      .status = status
    }
  };

  evt.cisHandle = pCisCtx->cisHandle;
  evt.status  = status;
  evt.cigSyncDelayUsec = cigSyncDelayUsec;
  evt.cisSyncDelayUsec = pCisCtx->cisSyncDelayUsec;
  evt.transLatUsecMToS = pCisCtx->transLatUsec; /* For now it is 0, update it when ISOAL is supported. */
  evt.transLatUsecSToM = pCisCtx->transLatUsec; /* For now it is 0, update it when ISOAL is supported. */
  evt.phyMToS = pCisCtx->phyMToS;
  evt.phySToM = pCisCtx->phySToM;
  evt.nse = pCisCtx->nse;
  evt.bnMToS = pCisCtx->bnMToS;
  evt.bnSToM = pCisCtx->bnSToM;
  evt.ftMToS = pCisCtx->ftMToS;
  evt.ftSToM = pCisCtx->ftSToM;
  evt.isoInterval = pCisCtx->isoInterval;
  evt.maxPduMToS = (pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->localDataPdu.maxTxLen : pCisCtx->localDataPdu.maxRxLen;
  evt.maxPduSToM = (pCisCtx->role == LL_ROLE_SLAVE) ? pCisCtx->localDataPdu.maxTxLen : pCisCtx->localDataPdu.maxRxLen;


  LL_TRACE_INFO2("### LlEvent ###  LL_CIS_EST_IND, cisHandle=%u, status=%u", pCisCtx->cisHandle, status);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal CIS subsystem message.
 *
 *  \param  pCisCtx     CIS context.
 *  \param  event       CIS event.
 */
/*************************************************************************************************/
void lctrSendCisMsg(lctrCisCtx_t *pCisCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = pCisCtx->cisHandle;
    pMsg->dispId = LCTR_DISP_CIS;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal CIS LLCP subsystem message.
 *
 *  \param  pCisCtx     CIS context.
 *  \param  event       Connection event.
 */
/*************************************************************************************************/
void lctrSendCisLlcpMsg(lctrCisCtx_t *pCisCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = (lctrMsgHdr_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = pCisCtx->aclHandle;
    pMsg->dispId = LCTR_DISP_CONN;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Store LLCP termination reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = lctrDataPdu.pld.cisTerm.reason;
}

/*************************************************************************************************/
/*!
 *  \brief      Store host initiated disconnect termination reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreDisconnectReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = pLctrCisMsg->disc.reason;
}

/*************************************************************************************************/
/*!
 *  \brief      Store connection failed to establish termination reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreConnFailEstablishTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = LL_ERROR_CODE_CONN_FAILED_TO_ESTABLISH;
}

/*************************************************************************************************/
/*!
 *  \brief      Store connection timeout termination reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreConnTimeoutTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = LL_ERROR_CODE_CONN_TIMEOUT;
}

/*************************************************************************************************/
/*!
 *  \brief      Store LLCP timeout termination reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreLlcpTimeoutTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = LL_ERROR_CODE_LMP_LL_RESP_TIMEOUT;
}

/*************************************************************************************************/
/*!
 *  \brief      Store local resource limitation reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreLocalLowResourceTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
}

/*************************************************************************************************/
/*!
 *  \brief      Store LLCP peer reject reason.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisStoreLlcpPeerRejTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = lctrDataPdu.pld.rejInd.reason;
}

/*************************************************************************************************/
/*!
 *  \brief      Store invalid request termination reason.
 *
 *  \param      pCisCtx    CIS connection context.
 */
/*************************************************************************************************/
void lctrCisStoreMicFailedTerminateReason(lctrCisCtx_t *pCisCtx)
{
  pCisCtx->reason = LL_ERROR_CODE_CONN_TERM_MIC_FAILURE;
}

/**************************************************************************************************
  CIS main state machine action functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Action function for CIS established.
 *
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrCisActCisEst(lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  lctrConnCtx_t *pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  BbBleData_t *pBle = &pConnCtx->bleData;
  pCigCtx->numCisEsted++;

  /* Initialize txPower. */
  uint8_t txPhy = (pCisCtx->role == LL_ROLE_MASTER) ? pCisCtx->phyMToS : pCisCtx->phySToM;
  uint8_t option = pBle->chan.initTxPhyOptions;
  int8_t txPwr = LCTR_GET_TXPOWER(pConnCtx, txPhy, option);

  if ((txPwr == LL_PWR_CTRL_TXPOWER_UNMANAGED) && (pConnCtx->peerReqRecvd))
  {
    LCTR_SET_TXPOWER(pConnCtx, txPhy, pLctrRtCfg->defTxPwrLvl);

    if (txPhy == LL_PHY_LE_CODED)
    {
      LCTR_SET_TXPOWER(pConnCtx, LL_PC_PHY_CODED_S2, pLctrRtCfg->defTxPwrLvl);
    }

    /* pCisCtx->bleData.chan.txPower = LCTR_GET_TXPOWER(pConnCtx, txPhy, option); */  /* Handled during initialization. */
    if (pConnCtx->usedFeatSet & LL_FEAT_POWER_CHANGE_IND)
    {
      pCisCtx->powerIndReq = TRUE;
    }
  }
  else
  {
    pCisCtx->bleData.chan.txPower = txPwr;
  }

  LL_TRACE_INFO2("lctrCisActCisEst, numCisEsted=%u txPower=%u", pCigCtx->numCisEsted, txPwr);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for CIS fail to establish.
 *
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrCisActCisEstFail(lctrCisCtx_t *pCisCtx)
{
  /* LLCP timeout for the LL_CIS_REQ indicates peer doesn't support CIS feature. */
  if (pCisCtx->isCisReqPend &&
      pCisCtx->reason == LL_ERROR_CODE_LMP_LL_RESP_TIMEOUT)
  {
    pCisCtx->isCisReqPend = FALSE;
    lctrNotifyHostCisEst(pCisCtx, LL_ERROR_CODE_UNSUPPORTED_FEATURE_PARAM_VALUE, 0 /* cigSyncDelayUsec */);
    return;
  }

  lctrNotifyHostCisEst(pCisCtx, pCisCtx->reason, 0 /* cigSyncDelayUsec */);

  lctrCleanupCtx(pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received host disconnect CIS.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisActDisc(lctrCisCtx_t *pCisCtx)
{
  lctrCisDisc_t *pMsg;

  if (!LctrCisTerminationInProgress(pCisCtx->aclHandle))
  {
    if ((pMsg = (lctrCisDisc_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
    {
      pMsg->hdr.handle = pCisCtx->aclHandle;
      pMsg->hdr.dispId = LCTR_DISP_CONN;
      pMsg->hdr.event = LCTR_CONN_MSG_API_DISCONNECT;

      pMsg->cisHandle =  pCisCtx->cisHandle;
      pMsg->reason = pCisCtx->reason;

      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);

      pCisCtx->termPend = FALSE;
    }
    else
    {
      LL_TRACE_WARN1("lctrCisActDisc: CIS OOM while terminating handle %d", pCisCtx->cisHandle);
    }
  }
  else
  {
    LL_TRACE_INFO1("lctrCisActDisc: CIS Termination pending, CIS Termination queued for handle %d", pCisCtx->cisHandle);
    pCisCtx->termPend = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received internal CIS closed.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisActClosed(lctrCisCtx_t *pCisCtx)
{
  uint16_t aclHandle = pCisCtx->aclHandle;

  /* Must do one at a time in case one datapath is not used. */
  LctrRemoveIsoDataPath(pCisCtx->cisHandle, LL_ISO_DATA_PATH_INPUT_BIT);
  LctrRemoveIsoDataPath(pCisCtx->cisHandle, LL_ISO_DATA_PATH_OUTPUT_BIT);

  LL_TRACE_INFO1("lctrCisActClosed, pCisCtx->cisHandle=%u", pCisCtx->cisHandle);

  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  lctrConnCtx_t *pConnCtx = LCTR_GET_CONN_CTX(pCisCtx->aclHandle);

  WSF_ASSERT(pCigCtx);
  WSF_ASSERT(pConnCtx);

  WsfTimerStop(&pCisCtx->tmrSupTimeout);

  pCisCtx->isClosing = TRUE;    /* Close the context in the CIG BOD end callback. */

  /* Fast supervision timeout case. */
  if (pCisCtx->isClosing == TRUE &&
      pCisCtx->reason == LL_ERROR_CODE_CONN_FAILED_TO_ESTABLISH)
  {
    pCisCtx->isClosing = FALSE;
    lctrCleanupCtx(pCisCtx);
  }

  lctrCheckPendingCisTermination(aclHandle);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for received internal CIS connection fail to maintain event.
 *
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisActFail(lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  LL_TRACE_INFO2("lctrCisActFail, pCisCtx->cisHandle=%u pCisCtx->state=%u", pCisCtx->cisHandle, pCisCtx->state);

  /* Supervision timeout or MIC timeout case */
  if (pCisCtx->state == LCTR_CIS_STATE_EST)
  {
    pCisCtx->isClosing = TRUE;    /* Close the context in the CIG BOD end callback. */
    SchRemove(&pCigCtx->cigBod);
  }
}

/**************************************************************************************************
  CIS LLCP state machine action functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Action function for LLCP received host disconnect.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisLlcpActHostDisc(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  lctrSendCisTermInd(pCtx, pCisCtx, pCisCtx->reason);
  lctrCisStartLlcpTimer(pCtx, pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for LLCP received CIS peer disconnect.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrCisLlcpActPeerDisc(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrCigCtx_t *pCigCtx = lctrFindCigById(pCisCtx->cigId);
  WSF_ASSERT(pCigCtx);

  lctrCisStoreTerminateReason(pCisCtx);

  pCtx->termAckReqd = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for LLCP received CIS terminated event.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrCisLlcpActCisTerm(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrSendCisMsg(pCisCtx, LCTR_CIS_MSG_CIS_CLOSED);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for LLCP received internal host disconnect.
 *
 *  \param      pCtx      Connection context.
 *  \param      pCisCtx   CIS context.
 */
/*************************************************************************************************/
void lctrCisLlcpActIntHostDisc(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  lctrSendCisTermInd(pCtx, pCisCtx, pCisCtx->reason);
  lctrCisStartLlcpTimer(pCtx, pCisCtx);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for LLCP received internal CIS peer disconnect.
 *
 *  \param      pCtx    Connection context.
 *  \param      pCisCtx CIS context.
 */
/*************************************************************************************************/
void lctrCisLlcpActIntPeerDisc(lctrConnCtx_t *pCtx, lctrCisCtx_t *pCisCtx)
{
  pCtx->termAckReqd = TRUE;
}
