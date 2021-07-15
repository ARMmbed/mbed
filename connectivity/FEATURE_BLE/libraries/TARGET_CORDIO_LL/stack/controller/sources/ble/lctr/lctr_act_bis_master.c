/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller master BIG action routines.
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

#include "lctr_int_bis_master.h"
#include "sch_api.h"

/*************************************************************************************************/
/*!
 *  \brief      Notify host with BIG Create Sync Complete event
 *
 *  \param      pBigCtx         BIG context.
 *  \param      status          Status.
 */
/*************************************************************************************************/
static void lctrNotifyHostBigCreateSyncComplete(lctrBigCtx_t *pBigCtx, LlStatus_t status)
{
  LlBigSyncEstInd_t evt = { 0 };

  evt.hdr.param = pBigCtx->handle;
  evt.hdr.event = LL_BIG_SYNC_EST_IND;
  evt.hdr.status = status;

  evt.status = status;
  evt.bigHandle = pBigCtx->handle;

  if (evt.status == LL_SUCCESS)
  {
    evt.transLatUsec = pBigCtx->transLatUsec;
    evt.nse = pBigCtx->nse;
    evt.bn = pBigCtx->bn;
    evt.pto = pBigCtx->pto;
    evt.irc = pBigCtx->irc;
    evt.maxPdu = pBigCtx->maxPdu;
    evt.isoInterval = LL_MATH_DIV_1250(pBigCtx->isoInterUsec);

    for (unsigned int i = 0; i < LL_MAX_BIS; i++)
    {
      if (pBigCtx->pBisCtx[i])
      {
        evt.bisHandle[evt.numBis++] = pBigCtx->pBisCtx[i]->handle;
      }
    }
  }

  LL_TRACE_INFO2("### LlEvent ###  LL_BIG_SYNC_EST_IND, bigHandle=%u, status=%u", pBigCtx->handle, status);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host with BIG Terminate Sync Complete event
 *
 *  \param      bigHandle       BIG handle.
 *  \param      status          Status.
 */
/*************************************************************************************************/
void lctrNotifyHostBigTerminateComplete(LlStatus_t status, uint8_t bigHandle)
{
  LlBigTermSyncCnf_t evt;

  /* Clear not required; all values are written. */
  /* memset(&evt, 0, sizeof(LlBigTermSyncCnf_t)); */

  evt.hdr.param = bigHandle;
  evt.hdr.event = LL_BIG_TERM_SYNC_CNF;
  evt.hdr.status = status;

  evt.status = status;
  evt.bigHandle = bigHandle;

  LL_TRACE_INFO2("### LlEvent ###  LL_BIG_TERM_SYNC_CNF, status=%u, bigHandle=%u", status, bigHandle);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host with BIG Terminate Sync Complete event
 *
 *  \param      bigHandle       BIG handle.
 *  \param      reason          Status.
 */
/*************************************************************************************************/
void lctrNotifyHostSyncLost(uint8_t bigHandle, LlStatus_t reason)
{
  LlBigSyncLostInd_t evt;

  /* Clear not required; all values are written. */
  /* memset(&evt, 0, sizeof(LlBigTermSyncCnf_t)); */

  evt.hdr.param = bigHandle;
  evt.hdr.event = LL_BIG_SYNC_LOST_IND;
  evt.hdr.status = LL_SUCCESS;

  evt.bigHandle = bigHandle;
  evt.reason = reason;

  LL_TRACE_INFO2("### LlEvent ###  LL_BIG_SYNC_LOST_IND, bigHandle=%u, reason=%u", bigHandle, reason);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Start BIG Synchronization.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActStart(lctrBigCtx_t *pBigCtx)
{
  /* Defer until BIG Info received. */
  /* lctrMstBigBuildOp(pBigCtx); */

  BbStart(BB_PROT_BLE);

  LmgrIncResetRefCount();
}

/*************************************************************************************************/
/*!
 *  \brief      Synchronize to BIG.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActBigSync(lctrBigCtx_t *pBigCtx)
{
  LctrAcadBigInfo_t *pBigInfo = &pLctrMstBigMsg->bigInfo.data;

  lctrMstSetupBigContext(pBigCtx, pBigInfo);
  lctrMstSetupBigChannel(pBigCtx, pBigInfo);

  for (unsigned int i = 0; i < pBigInfo->numBis; i++)
  {
    lctrBisCtx_t *pBisCtx;
    unsigned int j;

    for (j = 0; j < pBigCtx->roleData.mst.numBisIdx; j++)
    {
      /* Check host BIS filter. */
      if (pBigCtx->roleData.mst.bisIdx[j] == (i + 1))
      {
        /* Availability is verified on BIG Create Sync command. */
        pBisCtx = lctrAllocBisCtx(pBigCtx);
        WSF_ASSERT(pBisCtx);

        lctrSetupBisContext(pBisCtx, pBigInfo->seedAccAddr, pBigInfo->baseCrcInit, pBigInfo->chanMap, pBigInfo->phy);

        if (pBigCtx->roleData.mst.pFirstBisCtx == NULL)
        {
          /* Optimize ISR by reducing lookup on every event; store first BIS. */
          pBigCtx->roleData.mst.pFirstBisCtx = pBisCtx;
          pBigCtx->roleData.mst.firstBisOffsUsec = pBigCtx->bisSpaceUsec * i;
          pBigCtx->roleData.mst.firstBisEvtIdx = i;
        }
        else if (pBigCtx->roleData.mst.pSecondBisCtx == NULL)
        {
          /* Optimize ISR by reducing lookup on every event; store second BIS; NULL if none. */
          pBigCtx->roleData.mst.pSecondBisCtx = pBisCtx;
        }

        break;
      }

    }

    if (j == pBigCtx->roleData.mst.numBisIdx)
    {
      /* Disable BIS subevent. */
      pBigCtx->pBisCtx[pBigCtx->numBis++] = NULL;
    }
  }

  lctrMstBigBuildOp(pBigCtx, &pLctrMstBigMsg->bigInfo.data);

  WsfTimerStartMs(&pBigCtx->roleData.mst.bigSyncTmr, pBigCtx->roleData.mst.bigSyncTimeoutMs);

  lctrNotifyHostBigCreateSyncComplete(pBigCtx, LL_SUCCESS);
}

/*************************************************************************************************/
/*!
 *  \brief      Sync Lost due to BIG Terminated PDU received.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActTerm(lctrBigCtx_t *pBigCtx)
{
  /* Shutdown completes with events generated in BOD end callback. */

  pBigCtx->roleData.mst.syncLostReason = pBigCtx->bcp.term.reason;
}

/*************************************************************************************************/
/*!
 *  \brief      Shutdown active BIS operation.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActShutdown(lctrBigCtx_t *pBigCtx)
{
  /* Shutdown completes with events generated in BOD end callback. */

  if (pBigCtx->state == LCTR_MST_BIG_STATE_SYNCING)
  {
    lctrNotifyHostBigCreateSyncComplete(pBigCtx, LL_ERROR_CODE_CONN_TERM_BY_LOCAL_HOST);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Synchronization with broadcaster lost.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActSyncLost(lctrBigCtx_t *pBigCtx)
{
  /* Shutdown completes with events generated in BOD end callback. */

  pBigCtx->roleData.mst.syncLostReason = LL_ERROR_CODE_CONN_TIMEOUT;
}

/*************************************************************************************************/
/*!
 *  \brief      Drop synchronization due to MIC failure.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActMicFailed(lctrBigCtx_t *pBigCtx)
{
  /* Shutdown completes with events generated in BOD end callback. */

  pBigCtx->roleData.mst.syncLostReason = LL_ERROR_CODE_CONN_TERM_MIC_FAILURE;
}

/*************************************************************************************************/
/*!
 *  \brief      Terminated advertising after host periodic advertising disable.
 *
 *  \param      pBigCtx     BIG context.
 */
/*************************************************************************************************/
void lctrMstBigActCleanup(lctrBigCtx_t *pBigCtx)
{
  BbStop(BB_PROT_BLE);

  lctrFreeBigCtx(pBigCtx);

  LmgrDecResetRefCount();

  if (pBigCtx->roleData.mst.syncLostReason != LL_SUCCESS)
  {
    lctrNotifyHostSyncLost(pBigCtx->handle, pBigCtx->roleData.mst.syncLostReason);
  }
  else
  {
    lctrNotifyHostBigTerminateComplete(LL_SUCCESS, pBigCtx->handle);
  }
}
