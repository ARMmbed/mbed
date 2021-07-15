/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller PHY features action routines.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_slave_phy.h"
#include "lmgr_api.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief      Store PHY update request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStoreHostPhyUpdate(lctrConnCtx_t *pCtx)
{
  pCtx->allPhys = pLctrConnMsg->phyUpd.allPhys;
  pCtx->txPhys  = pLctrConnMsg->phyUpd.txPhys;
  pCtx->rxPhys  = pLctrConnMsg->phyUpd.rxPhys;

  /* Update PHY options immediately. */
  pCtx->bleData.chan.tifsTxPhyOptions = pLctrConnMsg->phyUpd.phyOptions;
}

/*************************************************************************************************/
/*!
 *  \brief      Store PHY update request.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStorePeerPhyReq(lctrConnCtx_t *pCtx)
{
  pCtx->phyReq = lctrDataPdu.pld.phyReq;
}

/*************************************************************************************************/
/*!
 *  \brief      Store PHY update indication.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
void lctrStorePeerPhyUpdateInd(lctrConnCtx_t *pCtx)
{
  if ((lctrDataPdu.pld.phyUpdInd.masterToSlavePhy == 0) &&
      (lctrDataPdu.pld.phyUpdInd.slaveToMasterPhy == 0))
  {
    /* No change. */
    lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
    return;
  }

  /* Check for valid instant. */
  if ((uint16_t)(lctrDataPdu.pld.phyUpdInd.instant - pCtx->eventCounter) >= LCTR_MAX_INSTANT)
  {
    /* Consider connection lost. */
    lctrSendConnMsg(pCtx, LCTR_CONN_TERM_INST_PASSED);
    return;
  }

  /* TODO check for valid PHYs. */

  pCtx->phyUpd = lctrDataPdu.pld.phyUpdInd;
}

/*************************************************************************************************/
/*!
 *  \brief      Send PHY PDU to peer.
 *
 *  \param      pCtx    Connection context.
 *  \param      opcode  Opcode to send in PDU.
 *  \param      txPhys  Transmitter PHYs.
 *  \param      rxPhys  Receiver PHYs.
 */
/*************************************************************************************************/
static void lctrSendPhyPdu(lctrConnCtx_t *pCtx, uint8_t opcode, uint8_t txPhys, uint8_t rxPhys)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PHY_PDU_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM(pBuf, opcode);

    UINT8_TO_BSTREAM(pBuf, txPhys);
    UINT8_TO_BSTREAM(pBuf, rxPhys);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Send PHY request PDU to peer.
 *
 *  \param      pCtx    Connection context.
 *  \param      txPhys  Transmitter PHYs.
 *  \param      rxPhys  Receiver PHYs.
 */
/*************************************************************************************************/
void lctrSendPhyReqPdu(lctrConnCtx_t *pCtx, uint8_t txPhys, uint8_t rxPhys)
{
  /*** Store PHY request ***/

  pCtx->phyReq.txPhys = txPhys;
  pCtx->phyReq.rxPhys = rxPhys;

  /*** Send PHY request ***/

  lctrSendPhyPdu(pCtx, LL_PDU_PHY_REQ, txPhys, rxPhys);
}

/*************************************************************************************************/
/*!
 *  \brief      Send PHY response PDU to peer.
 *
 *  \param      pCtx    Connection context.
 *  \param      txPhys  Transmitter PHYs.
 *  \param      rxPhys  Receiver PHYs.
 */
/*************************************************************************************************/
void lctrSendPhyRspPdu(lctrConnCtx_t *pCtx, uint8_t txPhys, uint8_t rxPhys)
{
  lctrSendPhyPdu(pCtx, LL_PDU_PHY_RSP, txPhys, rxPhys);
}

/*************************************************************************************************/
/*!
 *  \brief      Send PHY update indication PDU to peer.
 *
 *  \param      pCtx    Connection context.
 *  \param      txPhys  Transmitter PHYS.
 *  \param      rxPhys  Receiver PHYS.
 */
/*************************************************************************************************/
void lctrSendPhyUpdateIndPdu(lctrConnCtx_t *pCtx, uint8_t txPhys, uint8_t rxPhys)
{
  uint8_t *pPdu;

  if ((pPdu = lctrTxCtrlPduAlloc(LL_PHY_UPD_IND_PDU_LEN)) != NULL)
  {
    uint8_t *pBuf = pPdu;

    /*** Store PHY update request ***/

    pCtx->phyUpd.masterToSlavePhy = txPhys;
    pCtx->phyUpd.slaveToMasterPhy = rxPhys;

    uint16_t ceOffset;
#if (LL_ENABLE_TESTER == TRUE)
    if (llTesterCb.eventCounterOverride == TRUE)
    {
      ceOffset = llTesterCb.eventCounterOffset + 1;         /* +1 for next CE */
    }
    else
#endif
    {
      ceOffset = ((LL_MIN_INSTANT + 1 +     /* +1 for next CE */
                   pCtx->maxLatency) *      /* ensure slave will listen to this packet */
                  pCtx->ecu.srFactor);      /* include subrating factor */
    }

    if (txPhys || rxPhys)
    {
      pCtx->phyUpd.instant = pCtx->eventCounter + ceOffset;
    }
    else
    {
      /* No change. */
      pCtx->phyUpd.instant = 0;
      lctrSendConnMsg(pCtx, LCTR_CONN_LLCP_PROC_CMPL);
    }

    /*** Assemble control PDU. ***/

    UINT8_TO_BSTREAM (pBuf, LL_PDU_PHY_UPDATE_IND);
    UINT8_TO_BSTREAM (pBuf, txPhys);
    UINT8_TO_BSTREAM (pBuf, rxPhys);
    UINT16_TO_BSTREAM(pBuf, pCtx->phyUpd.instant);

    /*** Queue for transmit. ***/

    lctrTxCtrlPduQueue(pCtx, pPdu);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of PHY update indication.
 *
 *  \param      pCtx    Connection context.
 *  \param      status  Status.
 */
/*************************************************************************************************/
void lctrNotifyHostPhyUpdateInd(lctrConnCtx_t *pCtx, uint8_t status)
{
  const uint16_t handle = LCTR_GET_CONN_HANDLE(pCtx);

  LlPhyUpdateInd_t evt =
  {
    .hdr =
    {
      .param        = handle,
      .event        = LL_PHY_UPDATE_IND,
      .status       = status
    },

    .status         = status,
    .handle         = handle,
  };

  evt.txPhy = pCtx->bleData.chan.txPhy;
  evt.rxPhy = pCtx->bleData.chan.rxPhy;

  LL_TRACE_INFO2("### LlEvent ###  LL_PHY_UPDATE_IND, handle=%u, status=%u", handle, status);

  LmgrSendEvent((LlEvt_t *)&evt);
}
