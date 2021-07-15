/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave extended advertising action routines.
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

#include "lctr_int_adv_slave_ae.h"
#include "lctr_int_adv_slave.h"
#include "lmgr_api_adv_slave_ae.h"
#include "bb_ble_api_reslist.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_msg.h"
#include "util/bstream.h"

#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Common advertise resource cleanup.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
static void lctrExtAdvCleanup(lctrAdvSet_t *pAdvSet)
{
  lctrMsgHdr_t *pMsg;

  BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pAdvSet->auxBleData.op.slvAuxAdv;

  if ((pMsg = (lctrMsgHdr_t *)pAuxAdv->pRxAuxReqBuf) != NULL)
  {
    /* Recover header. */
    WsfMsgFree(pMsg - 1);
    pAuxAdv->pRxAuxReqBuf = NULL;
  }

  BbBleSlvAdvEvent_t * const pAdv = &pAdvSet->bleData.op.slvAdv;
  if ((pMsg = (lctrMsgHdr_t *)pAdv->pRxReqBuf) != NULL)
  {
    /* Recover header. */
    WsfMsgFree(pMsg - 1);
    pAdv->pRxReqBuf = NULL;
  }

  if (pAdvSet->param.advFiltPolicy)
  {
    LmgrDecWhitelistRefCount();
  }

  WSF_ASSERT(lmgrCb.numExtAdvEnabled > 0);
  lmgrCb.numExtAdvEnabled--;
}

/*************************************************************************************************/
/*!
 *  \brief      Common periodic advertising cleanup.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
static void lctrPeriodicAdvACleanup(lctrAdvSet_t *pAdvSet)
{
  BbStop(BB_PROT_BLE);

  SchRmRemove(LCTR_GET_PER_RM_HANDLE(pAdvSet));
  LmgrDecResetRefCount();
}

/*************************************************************************************************/
/*!
 *  \brief      Start extended advertising.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActStart(lctrAdvSet_t *pAdvSet)
{
  BbStart(BB_PROT_BLE);

  /* Reset state. */
  pAdvSet->maxEvents = pLctrSlvExtAdvMsg->enable.maxEvents;
  pAdvSet->numEvents = 0;
  pAdvSet->termReason = LL_SUCCESS;
  pAdvSet->pExtAdvAuxPtr = NULL;
  pAdvSet->connIndRcvd = FALSE;
  pAdvSet->shutdown = FALSE;

  uint8_t status;
  if ((status = lctrSlvExtAdvBuildOp(pAdvSet, pLctrSlvExtAdvMsg->enable.durMs)) != LL_SUCCESS)
  {
    /* TODO suppress terminate event on failed start */
    LmgrSendExtAdvEnableCnf(pAdvSet->handle, status);
    lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);
    return;
  }

  lmgrCb.numExtAdvEnabled++;

  if (pAdvSet->param.advFiltPolicy)
  {
    LmgrIncWhitelistRefCount();
  }

  if (pLctrSlvExtAdvMsg->enable.durMs)
  {
    WsfTimerStartMs(&pAdvSet->tmrAdvDur, pLctrSlvExtAdvMsg->enable.durMs);
  }

  LmgrSendExtAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Start extended advertising internally.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActSelfStart(lctrAdvSet_t *pAdvSet)
{
  BbStart(BB_PROT_BLE);

  /* Reset state. */
  pAdvSet->maxEvents = pLctrSlvExtAdvMsg->enable.maxEvents;
  pAdvSet->numEvents = 0;
  pAdvSet->termReason = LL_SUCCESS;
  pAdvSet->pExtAdvAuxPtr = NULL;
  pAdvSet->connIndRcvd = FALSE;
  pAdvSet->shutdown = FALSE;

  uint8_t status;
  if ((status = lctrSlvExtAdvBuildOp(pAdvSet, pLctrSlvExtAdvMsg->enable.durMs)) != LL_SUCCESS)
  {
    /* TODO suppress terminate event on failed start */
    LmgrSendExtAdvEnableCnf(pAdvSet->handle, status);
    lctrSendAdvSetMsg(pAdvSet, LCTR_EXT_ADV_MSG_TERMINATE);
    return;
  }

  if (pLctrSlvExtAdvMsg->enable.durMs)
  {
    WsfTimerStartMs(&pAdvSet->tmrAdvDur, pLctrSlvExtAdvMsg->enable.durMs);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Restart extended advertising.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActRestart(lctrAdvSet_t *pAdvSet)
{
  WsfTimerStop(&pAdvSet->tmrAdvDur);

  if (pLctrSlvExtAdvMsg->enable.durMs)
  {
    WsfTimerStartMs(&pAdvSet->tmrAdvDur, pLctrSlvExtAdvMsg->enable.durMs);
  }

  pAdvSet->maxEvents = pLctrSlvExtAdvMsg->enable.maxEvents;
  pAdvSet->numEvents = 0;

  LmgrSendExtAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Shutdown active advertising operation.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActShutdown(lctrAdvSet_t *pAdvSet)
{
  WsfTimerStop(&pAdvSet->tmrAdvDur);

  pAdvSet->termReason = LL_SUCCESS;
  pAdvSet->shutdown = TRUE;

  /* TODO: Attempt to remove BOD immediately to reduce shutdown time. */

  /* Shutdown completes with events generated in BOD end callback. */
}

/*************************************************************************************************/
/*!
 *  \brief      Shutdown active advertising operation due to host reset.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActResetShutdown(lctrAdvSet_t *pAdvSet)
{
  /* LCTR_MSG_RESET is broadcasted by hciReset and the processing order between ext ADV SM and
   * periodic ADV SM is not guaranteed. If ext ADV SM runs first, it will purge all info and periodic
   * ADV SM may not run as intended. So, reset cleanup of periodic advertising has to be done from
   * extended ADV SM. */
  if (pAdvSet->perParam.perState == LCTR_PER_ADV_STATE_ENABLED)
  {
    lctrPeriodicAdvActShutdown(pAdvSet);
    lctrPeriodicAdvACleanup(pAdvSet);
  }

  lctrExtAdvActShutdown(pAdvSet);
}

/*************************************************************************************************/
/*!
 *  \brief      Send advertising operation confirm.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActAdvCnf(lctrAdvSet_t *pAdvSet)
{
  LmgrSendExtAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Send advertising operation command disallowed.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActDisallowAdvCnf(lctrAdvSet_t *pAdvSet)
{
  LmgrSendExtAdvEnableCnf(pAdvSet->handle, LL_ERROR_CODE_CMD_DISALLOWED);
}

/*************************************************************************************************/
/*!
 *  \brief      Operation self terminated (e.g. on connection request).
 *
 *  \param      pAdvSet     Advertising set.
 *
 *  Slave self-termination is a result of the expiration of the duration timer, numEvents reached,
 *  or reception of an AUX_CONN_REQ.
 */
/*************************************************************************************************/
void lctrExtAdvActSelfTerm(lctrAdvSet_t *pAdvSet)
{
  if (!pAdvSet->connIndRcvd)
  {
    WsfTimerStop(&pAdvSet->tmrAdvDur);

    lctrExtAdvCleanup(pAdvSet);
    BbStop(BB_PROT_BLE);

    LmgrSendAdvSetTermInd(pAdvSet->handle, pAdvSet->termReason, 0, pAdvSet->numEvents);

    const uint8_t LEGACY_HIGH_DUTY = (LL_ADV_EVT_PROP_LEGACY_ADV_BIT | LL_ADV_EVT_PROP_HIGH_DUTY_ADV_BIT |
                                      LL_ADV_EVT_PROP_DIRECT_ADV_BIT | LL_ADV_EVT_PROP_CONN_ADV_BIT);
    if ((pAdvSet->param.advEventProp & LEGACY_HIGH_DUTY) == LEGACY_HIGH_DUTY )
    {
      /* Legacy connection complete event doesn't include peerRpa and localRpa. */
      lctrNotifyHostConnectInd(pAdvSet->handle, LL_ROLE_SLAVE, NULL,
                               pAdvSet->param.peerAddrType, pAdvSet->param.peerAddr, 0, 0,
                               LL_ERROR_CODE_ADV_TIMEOUT, 0);
    }
  }
  else /* CONNECT_IND or CONNECT_REQ received */
  {
    BbBleData_t * const pBle = &pAdvSet->bleData;
    BbBleSlvAdvEvent_t * const pAdv = &pBle->op.slvAdv;
    BbBleSlvAuxAdvEvent_t * const pAuxAdv = &pAdvSet->auxBleData.op.slvAuxAdv;

    uint8_t *pBuf;
    bool_t restartAdv = FALSE;

    if ((pBuf = pAdv->pRxReqBuf) != NULL)
    {
      /* Legacy advertising PDU. */

      /* If peer address was not resolved, attempt to resolve it now. */
      if (!BbBlePduFiltCheck(pBuf, &pBle->pduFilt, TRUE, &pAdv->filtResults))
      {
        restartAdv = TRUE;
      }
    }
    else if ((pBuf = pAuxAdv->pRxAuxReqBuf) != NULL)
    {
      /* Extended advertising PDU. */
      lctrAdvbPduHdr_t hdr;
      uint8_t *pTempBuf = pBuf;
      lctrConnInd_t connInd;

      pTempBuf += lctrUnpackAdvbPduHdr(&hdr, pTempBuf);
      lctrUnpackConnIndPdu(&connInd, pTempBuf);

      /* If peer address was not resolved, attempt to resolve it now. */
      bbBlePduExtFiltParams_t params;

      memset(&params, 0, sizeof(bbBlePduExtFiltParams_t));
      params.pduType = hdr.pduType;
      params.extHdrFlags |= (LL_EXT_HDR_ADV_ADDR_BIT | LL_EXT_HDR_TGT_ADDR_BIT);    /* ScanA and AdvA are mandatory. */
      params.peerAddr = connInd.initAddr;
      params.peerAddrRand = hdr.txAddrRnd;
      params.localAddr = connInd.advAddr;
      params.localAddrRand = hdr.rxAddrRnd;

      if (!BbBleExtPduFiltCheck(&params, &pBle->pduFilt, TRUE, &pAuxAdv->filtResults))
      {
        restartAdv = TRUE;
      }
    }

    WSF_ASSERT(pBuf);          /* pBuf must not be NULL since conn_ind/aux_conn_req is received. */

    if (restartAdv == FALSE)
    {
      lctrConnEstablish_t *pMsg;

      if ((pMsg = (lctrConnEstablish_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
      {
        lctrAdvbPduHdr_t hdr;

        pMsg->hdr.handle = pAdvSet->handle;
        pMsg->hdr.dispId = LCTR_DISP_CONN_IND;
        /* pMsg->hdr.event = 0; */

        /* coverity[var_deref_model] */
        pBuf += lctrUnpackAdvbPduHdr(&hdr, pBuf);
        lctrUnpackConnIndPdu(&pMsg->connInd, pBuf);

        if (pAdv->pRxReqBuf)
        {
          /* Legacy advertising. */
          BbBlePduFiltResultsGetPeerIdAddr(&pAdv->filtResults, &pMsg->peerIdAddr, &pMsg->peerIdAddrType);
          BbBlePduFiltResultsGetPeerRpa(&pAdv->filtResults, &pMsg->peerRpa);
          pMsg->phy = LL_PHY_LE_1M;
        }
        else
        {
          BbBlePduFiltResultsGetPeerIdAddr(&pAuxAdv->filtResults, &pMsg->peerIdAddr, &pMsg->peerIdAddrType);
          BbBlePduFiltResultsGetPeerRpa(&pAuxAdv->filtResults, &pMsg->peerRpa);
          pMsg->phy = pAdvSet->auxBleData.chan.rxPhy;   /* Same PHY as received CONN_IND. */
        }

        pMsg->connIndEndTsUsec   = pAdvSet->connIndEndTsUsec;
        pMsg->localRpa       = lmgrSlvAdvCb.localRpa;

        pMsg->usedChSel = pAdvSet->usedChSel;

        pMsg->sendAdvSetTerm = TRUE;
        pMsg->numExtAdvEvents = pAdvSet->numEvents;
        pMsg->isAuxConnReq = pAdvSet->isAuxConnReq;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);

        lctrExtAdvCleanup(pAdvSet);
        /* BbStop(BB_PROT_BLE); */    /* Remains enabled; connection starts immediately afterwards. */
      }
      else
      {
        /* Do not cleanup. Restarting will reuse resources. */
        restartAdv = TRUE;
      }
    }

    if (restartAdv == TRUE)
    {
      /* Restart advertising. */
      /* Reuse message. */
      /* coverity[alias_transfer] */
      lctrMsgHdr_t *pMsg = (lctrMsgHdr_t *)pBuf - 1;

      pMsg->handle = pAdvSet->handle;
      pMsg->dispId = LCTR_DISP_EXT_ADV;
      pMsg->event = LCTR_EXT_ADV_MSG_INT_START;
      WsfMsgSend(lmgrPersistCb.handlerId, pMsg);

      BbStop(BB_PROT_BLE);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Terminated advertising after host advertising disable.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActAdvTerm(lctrAdvSet_t *pAdvSet)
{
  lctrExtAdvCleanup(pAdvSet);
  BbStop(BB_PROT_BLE);

  LmgrSendExtAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Cleanup Advertising Set after host reset.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActReset(lctrAdvSet_t *pAdvSet)
{
  /* LCTR_MSG_RESET is broadcasted by hciReset and the processing order between ext ADV SM and
   * periodic ADV SM is not guaranteed. If ext ADV SM runs first, it will purge all info and periodic
   * ADV SM may not run as intended. So, reset cleanup of periodic advertising has to be done from
   * extended ADV SM. */
  if (pAdvSet->perParam.perState == LCTR_PER_ADV_STATE_ENABLED)
  {
    lctrPeriodicAdvActShutdown(pAdvSet);
    lctrPeriodicAdvACleanup(pAdvSet);
  }

  /* Although the AdvSet is freed here, some benign modifications to the context may occurs
   * to complete the SM call path. */
  lctrFreeAdvSet(pAdvSet);
}

/*************************************************************************************************/
/*!
 *  \brief      Terminated advertising and cleanup Advertising Set after host reset.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActResetTerm(lctrAdvSet_t *pAdvSet)
{
  lctrExtAdvCleanup(pAdvSet);
  BbStop(BB_PROT_BLE);

  /* Although the AdvSet is freed here, some benign modifications to the context may occurs
   * to complete the SM call path. */
  lctrFreeAdvSet(pAdvSet);
}

/*************************************************************************************************/
/*!
 *  \brief      Advertising set duration timer expired.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrExtAdvActDurationExpired(lctrAdvSet_t *pAdvSet)
{
  pAdvSet->termReason = LL_ERROR_CODE_ADV_TIMEOUT;

  /* Signal shutdown, event completion occurs in lctrExtAdvActSelfTerm(). */
  pAdvSet->shutdown = TRUE;

  SchRemove(&pAdvSet->advBod);
  SchRemove(&pAdvSet->auxAdvBod);
}

/*************************************************************************************************/
/*!
 *  \brief  Build channel remapping table.
 *
 *  \param  pChanParam  Channel parameters.
 */
/*************************************************************************************************/
void lctrPeriodicBuildRemapTable(lmgrChanParam_t *pChanParam)
{
  unsigned int chanIdx;
  unsigned int numUsedChan = 0;

  for (chanIdx = 0; chanIdx < sizeof(pChanParam->chanRemapTbl); chanIdx++)
  {
    if (pChanParam->chanMask & (UINT64_C(1) << chanIdx))
    {
      pChanParam->chanRemapTbl[numUsedChan++] = chanIdx;
    }
  }

  WSF_ASSERT(numUsedChan);          /* must have at least one channel */

  pChanParam->numUsedChan = numUsedChan;
}

/*************************************************************************************************/
/*!
 *  \brief      Start periodic advertising.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActStart(lctrAdvSet_t *pAdvSet)
{
  uint8_t status;

  BbStart(BB_PROT_BLE);

  pAdvSet->perParam.shutdown = FALSE;
  pAdvSet->perParam.perAccessAddr = lctrComputeAccessAddr();
  pAdvSet->perParam.perEventCounter = 0;
  pAdvSet->perParam.perChanParam.chanMask = lmgrCb.chanClass;
  LmgrBuildRemapTable(&pAdvSet->perParam.perChanParam);
  pAdvSet->perParam.perChanParam.usedChSel = LL_CH_SEL_2;
  pAdvSet->perParam.perChanParam.chIdentifier = (pAdvSet->perParam.perAccessAddr >> 16) ^
                                                (pAdvSet->perParam.perAccessAddr >> 0);
  pAdvSet->perParam.perChIdx = lctrPeriodicSelectNextChannel(&pAdvSet->perParam.perChanParam,
                                                              pAdvSet->perParam.perEventCounter);

  if ((status = lctrSlvPeriodicAdvBuildOp(pAdvSet)) != LL_SUCCESS)
  {
    lctrSendPeriodicAdvSetMsg(pAdvSet, LCTR_PER_ADV_MSG_TERMINATE);
    LmgrSendPeriodicAdvEnableCnf(pAdvSet->handle, status);
    return;
  }

  pAdvSet->perParam.perAdvEnabled = TRUE;

  /* Need to set the flag to add aux BOD when extened adv is already started. */
  if (pAdvSet->state == LCTR_EXT_ADV_STATE_ENABLED && pAdvSet->auxBodUsed == FALSE)
  {
    pAdvSet->perParam.perAuxStart = TRUE;
  }

  if (pAdvSet->state == LCTR_EXT_ADV_STATE_ENABLED)
  {
    /* The Advertising DID is required to change when a SyncInfo field is added to or removed. */
    pAdvSet->advData.alt.ext.did = lctrCalcDID(pAdvSet->advData.pBuf, pAdvSet->advData.len);
    pAdvSet->didPerUpdate = TRUE;
  }

  if (pAdvSet->perParam.enableAdi)
  {
    pAdvSet->perParam.advDID = lctrCalcDID(pAdvSet->advData.pBuf, pAdvSet->advData.len);
  }

  LmgrSendPeriodicAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
  LmgrIncResetRefCount();
}

/*************************************************************************************************/
/*!
 *  \brief      Restart periodic advertising.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActUpdate(lctrAdvSet_t *pAdvSet)
{
  LmgrSendPeriodicAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
  /* TODO cause random address to change */
}

/*************************************************************************************************/
/*!
 *  \brief      Send periodic advertising operation confirm.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActAdvCnf(lctrAdvSet_t *pAdvSet)
{
  LmgrSendPeriodicAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Send periodic advertising operation command disallowed.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActDisallowAdvCnf(lctrAdvSet_t *pAdvSet)
{
  LmgrSendPeriodicAdvEnableCnf(pAdvSet->handle, LL_ERROR_CODE_CMD_DISALLOWED);
}

/*************************************************************************************************/
/*!
 *  \brief      Shutdown active periodic advertising operation.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActShutdown(lctrAdvSet_t *pAdvSet)
{
  if ((pAdvSet->state == LCTR_EXT_ADV_STATE_ENABLED) && (pAdvSet->perParam.perAdvEnabled == TRUE))
  {
    /* The Advertising DID is required to change when a SyncInfo field is added to or removed. */
    pAdvSet->advData.alt.ext.did = lctrCalcDID(pAdvSet->advData.pBuf, pAdvSet->advData.len);
    pAdvSet->didPerUpdate = TRUE;
  }

  pAdvSet->perParam.shutdown = TRUE;
  pAdvSet->perParam.perAdvEnabled = FALSE;
  pAdvSet->perParam.perAuxStart = FALSE;

  /* By removing BOD from scheduler, BOD end callback will be called. */
  /* Shutdown completes with events generated in BOD end callback.    */
  SchRemove(&pAdvSet->perParam.perAdvBod);
}

/*************************************************************************************************/
/*!
 *  \brief      Terminated advertising after host periodic advertising disable.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActAdvTerm(lctrAdvSet_t *pAdvSet)
{
  lctrPeriodicAdvACleanup(pAdvSet);

  LmgrSendPeriodicAdvEnableCnf(pAdvSet->handle, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Terminated periodc advertising after host reset.
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrPeriodicAdvActResetTerm(lctrAdvSet_t *pAdvSet)
{
  lctrPeriodicAdvACleanup(pAdvSet);
}

/*************************************************************************************************/
/*!
 *  \brief      ACAD channel map start handler
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAcadActChanMapUpdateStart(lctrAdvSet_t *pAdvSet)
{
  LctrAcadChanMapUpd_t *pAcadParam = &pAdvSet->acadParams[LCTR_ACAD_ID_CHAN_MAP_UPDATE].chanMapUpdate;

  /* A new channel map update cannot replace a currently running one. */
  if (pAcadParam->hdr.state == LCTR_ACAD_STATE_ENABLED)
  {
    return;
  }

  pAdvSet->perParam.updChanMask = pLctrAcadSlvMsg->chanMapUpd.chanMap;

  pAcadParam->chanMask = pAdvSet->perParam.updChanMask;
  pAcadParam->instant = pAdvSet->perParam.perEventCounter + LL_MIN_INSTANT;
  pAcadParam->hdr.len = LL_ACAD_CHAN_MAP_UPD_LEN;
  pAcadParam->hdr.state = LCTR_ACAD_STATE_ENABLED;
}

/*************************************************************************************************/
/*!
 *  \brief      ACAD channel map finish handler
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAcadActChanMapUpdateFinish(lctrAdvSet_t *pAdvSet)
{
  lctrSlvAcadDisable(&pAdvSet->acadParams[LCTR_ACAD_ID_CHAN_MAP_UPDATE]);
}

/*************************************************************************************************/
/*!
 *  \brief      ACAD BIG created handler
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAcadActBigCreated(lctrAdvSet_t *pAdvSet)
{
  if (pAdvSet->bigCreated)
  {
    pAdvSet->bigCreated(pAdvSet->handle);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      ACAD BIG terminated handler
 *
 *  \param      pAdvSet     Advertising set.
 */
/*************************************************************************************************/
void lctrSlvAcadActBigTerminated(lctrAdvSet_t *pAdvSet)
{
  if (pAdvSet->bigTerminated)
  {
    pAdvSet->bigTerminated(pAdvSet->handle);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Disable and clean a generic ACAD parameter
 *
 *  \param      pAcadParam  Generic ACAD parameter type
 */
/*************************************************************************************************/
void lctrSlvAcadDisable(lctrAcadParam_t *pAcadParam)
{
  memset(pAcadParam, 0, sizeof(*pAcadParam));
  pAcadParam->hdr.state = LCTR_ACAD_STATE_DISABLED;
}
