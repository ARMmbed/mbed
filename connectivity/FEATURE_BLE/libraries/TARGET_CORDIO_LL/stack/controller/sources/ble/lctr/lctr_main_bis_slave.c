/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller slave operation builder implementation file.
 *
 *  Copyright (c) 2019 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_bis_slave.h"
#include "lctr_pdu_adv_ae.h"
#include "lctr_int_iso.h"
#include "sch_api.h"
#include "sch_api_ble.h"
#include "wsf_assert.h"
#include "wsf_math.h"
#include <string.h>

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

extern uint8_t lctrChoosePreferredPhy(uint8_t val);

/**************************************************************************************************
  Local Function
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Compute the maximum PDU transmission time in microseconds.
 *
 *  \param      phy         Transmission PHY.
 *  \param      maxPdu      Maximum PDU size.
 *  \param      encrypt     Encryption enabled.
 *
 *  \return     BIS Space in microseconds.
 *
 *  \note       This value includes preamble, access address, CRC and T_MSS.
 */
/*************************************************************************************************/
static uint32_t lctrBisCalcMaxPduTimeUsec(uint8_t phy, uint8_t maxPdu, uint8_t encrypt)
{
  uint32_t duration;

  switch (phy)
  {
    case BB_PHY_BLE_1M:
    default:
      duration = LL_DATA_LEN_TO_TIME_1M(maxPdu, encrypt);
      break;
    case BB_PHY_BLE_2M:
      duration = LL_DATA_LEN_TO_TIME_2M(maxPdu, encrypt);
      break;
    case BB_PHY_BLE_CODED:
      duration = LL_DATA_LEN_TO_TIME_CODED_S8(maxPdu, encrypt);
      break;
  }

  return duration + LL_BLE_TMSS_US;
}

/*************************************************************************************************/
/*!
 *  \brief      Setup BIG context.
 *
 *  \param      pBigCtx         BIG context.
 *  \param      pCreateBig      Create BIG parameters.
 */
/*************************************************************************************************/
static void lctrSlvSetupBigContext(lctrBigCtx_t *pBigCtx, LlCreateBig_t *pCreateBig)
{
  pBigCtx->role         = LL_ROLE_SLAVE;

  pBigCtx->framing      = pCreateBig->framing;
  pBigCtx->packing      = pCreateBig->packing;

  pBigCtx->phy          = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCreateBig->phys));

  pBigCtx->encrypt      = pCreateBig->encrypt;
  if (pBigCtx->encrypt)
  {
    /* Generate group parameters. */
    PalCryptoGenerateRandomNumber(pBigCtx->giv, LL_GIV_LEN);
    PalCryptoGenerateRandomNumber(pBigCtx->gskd, LL_GSKD_LEN);

    /* Setup encryption parameters. */
    lctrBisCalcGroupSessionKey(pBigCtx->gskd, pCreateBig->bcstCode, pBigCtx->bleData.chan.enc.sk);
  }

  /* Packet */
  pBigCtx->maxPdu = WSF_MIN(BB_DATA_PLD_MAX_LEN, pCreateBig->maxSdu);
  pBigCtx->maxSdu = pCreateBig->maxSdu;

  /* Event */
  pBigCtx->bn  = LlMathDivideUint32RoundUp(pCreateBig->maxSdu, pBigCtx->maxPdu);
  pBigCtx->irc = pCreateBig->rtn + 1;
  pBigCtx->pto = 0;
  pBigCtx->nse = (pBigCtx->bn + pBigCtx->pto) * pBigCtx->irc;

  /* Timing */
  pBigCtx->sduInterUsec = pCreateBig->sduInterUsec;
  pBigCtx->isoInterUsec = pCreateBig->sduInterUsec;
  /* TODO consider MTL */
  /* TODO consider framing in BIS Space computation */

  switch (pBigCtx->packing)
  {
    case LL_PACKING_INTERLEAVED:
    {
      pBigCtx->bisSpaceUsec = lctrBisCalcMaxPduTimeUsec(pBigCtx->phy, pBigCtx->maxPdu, pCreateBig->encrypt);
      pBigCtx->subInterUsec = pBigCtx->bisSpaceUsec * pCreateBig->numBis;
      pBigCtx->syncDelayUsec = (pCreateBig->numBis * pBigCtx->subInterUsec) - LL_BLE_TMSS_US;
      break;
    }

    case LL_PACKING_SEQUENTIAL:
    default:
    {
      pBigCtx->subInterUsec = lctrBisCalcMaxPduTimeUsec(pBigCtx->phy, pBigCtx->maxPdu, pCreateBig->encrypt);
      pBigCtx->bisSpaceUsec = pBigCtx->subInterUsec * pBigCtx->nse;
      pBigCtx->syncDelayUsec = (pCreateBig->numBis * pBigCtx->bisSpaceUsec) - LL_BLE_TMSS_US;
      break;
    }
  }

  /* Ensure successful divide. */
  if (pBigCtx->bn || pBigCtx->sduInterUsec)
  {
    if (pBigCtx->framing == LL_ISO_PDU_TYPE_FRAMED)
    {
      pBigCtx->transLatUsec = pBigCtx->syncDelayUsec +
        pBigCtx->pto * (pBigCtx->nse / pBigCtx->bn - pBigCtx->irc) * pBigCtx->isoInterUsec +
        pBigCtx->isoInterUsec + pBigCtx->sduInterUsec;
    }
    else
    {
      pBigCtx->transLatUsec = pBigCtx->syncDelayUsec +
        pBigCtx->pto * (pBigCtx->nse / pBigCtx->bn - pBigCtx->irc) * pBigCtx->isoInterUsec +
        ((pBigCtx->isoInterUsec / pBigCtx->sduInterUsec) - 1) * pBigCtx->sduInterUsec;
    }
  }
  else
  {
    LL_TRACE_ERR0("Invalid BN or sduInterUsec value");
    pBigCtx->transLatUsec = LL_ISO_TRANSPORT_LAT_MIN;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Set up a new BIG with test command.
 *
 *  \param      pBigCtx         BIG context.
 *  \param      pCreateBigTest  Create BIG test parameters.
 */
/*************************************************************************************************/
static void lctrSlvSetupBigTestContext(lctrBigCtx_t *pBigCtx, LlCreateBigTest_t *pCreateBigTest)
{
  pBigCtx->role         = LL_ROLE_SLAVE;

  pBigCtx->framing      = pCreateBigTest->framing;
  pBigCtx->packing      = pCreateBigTest->packing;

  pBigCtx->phy          = lctrPhysBitToPhy(lctrChoosePreferredPhy(pCreateBigTest->phys));

  pBigCtx->encrypt      = pCreateBigTest->encrypt;
  if (pBigCtx->encrypt)
  {
    /* Generate group parameters. */
    PalCryptoGenerateRandomNumber(pBigCtx->giv, LL_GIV_LEN);
    PalCryptoGenerateRandomNumber(pBigCtx->gskd, LL_GSKD_LEN);

    /* Setup encryption parameters. */
    lctrBisCalcGroupSessionKey(pBigCtx->gskd, pCreateBigTest->bcstCode, pBigCtx->bleData.chan.enc.sk);
  }

  pBigCtx->maxPdu       = pCreateBigTest->maxPdu;
  pBigCtx->maxSdu       = pCreateBigTest->maxSdu;
  pBigCtx->sduInterUsec = pCreateBigTest->sduInterUsec;
  pBigCtx->isoInterUsec = LCTR_ISO_INT_TO_US(pCreateBigTest->isoInter);
  pBigCtx->bn           = pCreateBigTest->bn;
  pBigCtx->irc          = pCreateBigTest->irc;
  pBigCtx->pto          = pCreateBigTest->pto;
  pBigCtx->nse          = pCreateBigTest->nse;

  switch (pBigCtx->packing)
  {
    case LL_PACKING_INTERLEAVED:
    {
      pBigCtx->bisSpaceUsec = lctrBisCalcMaxPduTimeUsec(pBigCtx->phy, pBigCtx->maxPdu, pCreateBigTest->encrypt);
      pBigCtx->subInterUsec = pBigCtx->bisSpaceUsec * pCreateBigTest->numBis;
      pBigCtx->syncDelayUsec = (pCreateBigTest->numBis * pBigCtx->subInterUsec) - LL_BLE_TMSS_US;
      break;
    }

    case LL_PACKING_SEQUENTIAL:
    default:
    {
      pBigCtx->subInterUsec = lctrBisCalcMaxPduTimeUsec(pBigCtx->phy, pBigCtx->maxPdu, pCreateBigTest->encrypt);
      pBigCtx->bisSpaceUsec = pBigCtx->subInterUsec * pBigCtx->nse;
      pBigCtx->syncDelayUsec = (pCreateBigTest->numBis * pBigCtx->bisSpaceUsec) - LL_BLE_TMSS_US;
      break;
    }
  }

  /* Ensure successful divide. */
  if (pBigCtx->bn || pBigCtx->sduInterUsec)
  {
    if (pBigCtx->framing == LL_ISO_PDU_TYPE_FRAMED)
    {
      pBigCtx->transLatUsec = pBigCtx->syncDelayUsec +
        pBigCtx->pto * (pBigCtx->nse / pBigCtx->bn - pBigCtx->irc) * pBigCtx->isoInterUsec +
        pBigCtx->isoInterUsec + pBigCtx->sduInterUsec;
    }
    else
    {
      pBigCtx->transLatUsec = pBigCtx->syncDelayUsec +
        pBigCtx->pto * (pBigCtx->nse / pBigCtx->bn - pBigCtx->irc) * pBigCtx->isoInterUsec +
        ((pBigCtx->isoInterUsec / pBigCtx->sduInterUsec) - 1) * pBigCtx->sduInterUsec;
    }
  }
  else
  {
    LL_TRACE_ERR0("Invalid BN or sduInterUsec value");
    pBigCtx->transLatUsec = LL_ISO_TRANSPORT_LAT_MIN;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Setup BIG channelization parameters.
 *
 *  \param      pBigCtx         BIG context.
 */
/*************************************************************************************************/
static void lctrSlvSetupBigChannel(lctrBigCtx_t *pBigCtx)
{
  pBigCtx->seedAccAddr = lctrComputeSeedAccessAddr();
  pBigCtx->baseCrcInit = lctrComputeCrcInit() >> 8;

  pBigCtx->ctrChan.opType = BB_BLE_OP_SLV_BIS_EVENT;
  pBigCtx->ctrChan.accAddr = lctrComputeBisAccessAddr(pBigCtx->seedAccAddr, 0);

  pBigCtx->ctrChSelInfo.chanMask = lmgrCb.chanClass;
  pBigCtx->ctrChSelInfo.usedChSel = LL_CH_SEL_2;
  pBigCtx->ctrChSelInfo.chIdentifier = (uint16_t)(pBigCtx->ctrChan.accAddr >> 16) ^
                                       (uint16_t)(pBigCtx->ctrChan.accAddr >> 0);
  LmgrBuildRemapTable(&pBigCtx->ctrChSelInfo);

  pBigCtx->ctrChan.crcInit = (pBigCtx->baseCrcInit << 8) | 0;
  pBigCtx->ctrChan.txPower = lmgrCb.advTxPwr;
  pBigCtx->ctrChan.txPhy = pBigCtx->phy;
  pBigCtx->ctrChan.initTxPhyOptions = BB_PHY_OPTIONS_DEFAULT;
  pBigCtx->ctrChan.tifsTxPhyOptions = BB_PHY_OPTIONS_DEFAULT;
  /* pBigCtx->ctrChan.peerTxStableModIdx = FALSE; */

#if (LL_ENABLE_TESTER)
  pBigCtx->ctrChan.accAddrRx = pBigCtx->ctrChan.accAddr ^ llTesterCb.dataAccessAddrRx;
  pBigCtx->ctrChan.accAddrTx = pBigCtx->ctrChan.accAddr ^ llTesterCb.dataAccessAddrTx;
  pBigCtx->ctrChan.crcInitRx = pBigCtx->ctrChan.crcInit ^ llTesterCb.dataCrcInitRx;
  pBigCtx->ctrChan.crcInitTx = pBigCtx->ctrChan.crcInit ^ llTesterCb.dataCrcInitTx;
#endif

  if (pBigCtx->encrypt)
  {
    pBigCtx->ctrChan.enc.enaEncrypt = TRUE;
    pBigCtx->ctrChan.enc.enaAuth = TRUE;
    pBigCtx->ctrChan.enc.nonceMode = PAL_BB_NONCE_MODE_EXT64_CNTR;

    memcpy(pBigCtx->ctrChan.enc.iv, pBigCtx->giv, LL_IV_LEN);
    pBigCtx->ctrChan.enc.iv[0] ^= pBigCtx->ctrChan.accAddr >>  0;
    pBigCtx->ctrChan.enc.iv[1] ^= pBigCtx->ctrChan.accAddr >>  8;
    pBigCtx->ctrChan.enc.iv[2] ^= pBigCtx->ctrChan.accAddr >> 16;
    pBigCtx->ctrChan.enc.iv[3] ^= pBigCtx->ctrChan.accAddr >> 24;

    memcpy(pBigCtx->ctrChan.enc.sk, pBigCtx->bleData.chan.enc.sk, PAL_CRYPTO_LL_KEY_LEN);

    /* The directionBit shall be set to 1 for Broadcast Isochronous PDUs. */
    pBigCtx->ctrChan.enc.dir = 1;
    pBigCtx->ctrChan.enc.type = PAL_BB_TYPE_BIS;

    if (lctrInitCipherBlkHdlr)
    {
      lctrInitCipherBlkHdlr(&pBigCtx->ctrChan.enc, LCTR_BIG_CTRL_ENC_ID(pBigCtx), 1);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      BIS slave reset handler.
 */
/*************************************************************************************************/
static void lctrSlvBisResetHandler(void)
{
  lctrBisDefaults();
}

/*************************************************************************************************/
/*!
 *  \brief      BIG slave message dispatcher.
 *
 *  \param      pMsg    Pointer to message buffer.
 */
/*************************************************************************************************/
static void lctrSlvBigDisp(LctrSlvBigMsg_t *pMsg)
{
  if (pMsg->hdr.dispId != LCTR_DISP_BCST)
  {
    lctrBigCtx_t *pBigCtx;

    if ((pBigCtx = lctrFindBigByHandle(pMsg->hdr.handle)) != NULL)
    {
      lctrSlvBigExecuteSm(pBigCtx, pMsg->hdr.event);
    }
  }
  else
  {
    for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
    {
      if (pLctrBigTbl[i].enabled && (pLctrBigTbl[i].role == LL_ROLE_SLAVE))
      {
        pMsg->hdr.handle = pLctrBigTbl[i].handle;
        lctrSlvBigExecuteSm(&pLctrBigTbl[i], pMsg->hdr.event);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for ACAD BIG created.
 *
 *  \param      advHandle   Advertising handle
 */
/*************************************************************************************************/
static void lctrSlvBisAcadBigCreated(uint8_t advHandle)
{
  LctrAcadBigInfo_t *pBigInfo;
  lctrAdvSet_t *pAdvSet = lctrFindAdvSet(advHandle);
  lctrBigCtx_t *pBigCtx = lctrFindBigByHandle(pLctrAcadSlvMsg->bigCreated.bigHandle);
  WSF_ASSERT(pBigCtx);
  WSF_ASSERT(pAdvSet);

  pBigInfo = &pAdvSet->acadParams[LCTR_ACAD_ID_BIG_INFO].bigInfo;

  /* A new BigInfo cannot replace a currently running one. */
  WSF_ASSERT(pBigInfo->hdr.state != LCTR_ACAD_STATE_ENABLED);

  if (pBigCtx->encrypt)
  {
    pBigInfo->hdr.len = LL_ACAD_BIG_INFO_ENCRPT_LEN;
    memcpy(pBigInfo->giv, pBigCtx->giv, LL_GIV_LEN);
    memcpy(pBigInfo->gskd, pBigCtx->gskd, LL_GSKD_LEN);
  }
  else
  {
    pBigInfo->hdr.len = LL_ACAD_BIG_INFO_UNENCRPT_LEN;
  }

  pBigInfo->hdr.state = LCTR_ACAD_STATE_ENABLED;

  /* pBigInfo->bigOffs      = 0; */  /* deferred until timing values are known */
  /* pBigInfo->bigOffsUnits = 0; */  /* deferred until timing values are known */
  pBigInfo->isoInter        = LL_MATH_DIV_1250(pBigCtx->isoInterUsec);
  pBigInfo->numBis          = pBigCtx->numBis;
  pBigInfo->nse             = pBigCtx->nse;
  pBigInfo->bn              = pBigCtx->bn;
  pBigInfo->subEvtInterUsec = pBigCtx->subInterUsec;
  pBigInfo->pto             = pBigCtx->pto;
  pBigInfo->bisSpaceUsec    = pBigCtx->bisSpaceUsec;
  pBigInfo->irc             = pBigCtx->irc;
  pBigInfo->maxPdu          = pBigCtx->maxPdu;
  pBigInfo->seedAccAddr     = pBigCtx->seedAccAddr;
  pBigInfo->sduInterUsec    = pBigCtx->sduInterUsec;
  pBigInfo->maxSdu          = pBigCtx->maxSdu;
  pBigInfo->baseCrcInit     = pBigCtx->baseCrcInit;
  pBigInfo->chanMap         = pBigCtx->ctrChSelInfo.chanMask;
  pBigInfo->phy             = pBigCtx->phy;
  pBigInfo->bisPldCtr       = pBigCtx->eventCounter * pBigCtx->bn;
  pBigInfo->framing         = pBigCtx->framing;
  pBigInfo->encrypt         = pBigCtx->encrypt;
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for ACAD BIG created.
 *
 *  \param      advHandle Advertising handle
 */
/*************************************************************************************************/
static void lctrSlvBisAcadBigTerminated(uint8_t advHandle)
{
  lctrAdvSet_t *pAdvSet = lctrFindAdvSet(advHandle);
  WSF_ASSERT(pAdvSet)

  lctrSlvAcadDisable(&pAdvSet->acadParams[LCTR_ACAD_ID_BIG_INFO]);
}

/*************************************************************************************************/
/*!
 *  \brief      Action function for ACAD BIG created.
 *
 *  \param      advHandle Advertising handle
 */
/*************************************************************************************************/
static void lctrSlvBisRemoveAdvSet(uint8_t advHandle)
{
  lctrAdvSet_t *pAdvSet = lctrFindAdvSet(advHandle);

  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if ((pBigCtx->role == LL_ROLE_SLAVE) &&
        (pBigCtx->roleData.slv.pAdvSet == pAdvSet))
    {
      pBigCtx->roleData.slv.pAdvSet = NULL;
      break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Get reference time (due time) of the next BIG.
 *
 *  \param      rmHandle      RM handle.
 *  \param      pDurUsec      Pointer to duration of the connection BOD.
 *
 *  \return     Due time in microseconds of the connection handle.
 */
/*************************************************************************************************/
static uint32_t lctrGetBigRefTime(uint8_t rmHandle, uint32_t *pDurUsec)
{
  uint32_t refTime = 0;
  lctrBigCtx_t *pBigCtx = LCTR_RM_HANDLE_TO_BIG(rmHandle);

  if (pBigCtx->enabled)
  {
    WSF_ASSERT(pBigCtx->bleData.chan.opType == BB_BLE_OP_SLV_BIS_EVENT);

    refTime = pBigCtx->bod.dueUsec;

    if (pDurUsec)
    {
      *pDurUsec = pBigCtx->bod.minDurUsec;
    }
  }

  return refTime;
}

/*************************************************************************************************/
/*!
 *  \brief  Host channel class update handler for CIS master.
 *
 *  \param  chanMap     Updated channel map.
 *
 *  \return Status code.
 */
/*************************************************************************************************/
static uint8_t lctrMstBigChClassUpdate(uint64_t chanMap)
{
  for (unsigned int i = 0; i < pLctrRtCfg->maxBig; i++)
  {
    lctrBigCtx_t *pBigCtx = &pLctrBigTbl[i];

    if (pBigCtx->enabled && (pBigCtx->role == LL_ROLE_SLAVE))
    {
      lctrMsgHdr_t *pMsg;

      if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
      {
        pMsg->handle = pBigCtx->handle;
        pMsg->dispId = LCTR_DISP_BIG_BCST;
        pMsg->event = LCTR_SLV_BIG_MSG_CH_MAP_UPD;

        WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
      }
      else
      {
        LL_TRACE_ERR0("lctrMstBigChClassUpdate: out of message buffers");
        return LL_ERROR_CODE_CMD_DISALLOWED;
      }
    }
  }

  return LL_SUCCESS;
}

/**************************************************************************************************
  External Function
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for BIS slave.
 */
/*************************************************************************************************/
void LctrSlvBisInit(void)
{
  /* Add reset handler. */
  lctrResetHdlrTbl[LCTR_DISP_BIG_BCST] = lctrSlvBisResetHandler;

  /* Add BIS slave message dispatcher. */
  lctrMsgDispTbl[LCTR_DISP_BIG_BCST] = (LctrMsgDisp_t)lctrSlvBigDisp;

  lctrRegisterChClassHandler(lctrMstBigChClassUpdate);

  lctrBisDefaults();

  /* Set supported features. */
  if (pLctrRtCfg->btVer >= LL_VER_BT_CORE_SPEC_5_1)
  {
    lmgrPersistCb.featuresDefault |= LL_FEAT_ISO_SYNC;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  BIG Create Sync.
 *
 *  \param  pCreateBig      BIG create sync parameters.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrSlvBisCreateBig(LlCreateBig_t *pCreateBig)
{
  lctrBigCtx_t *pBigCtx;

  if (pCreateBig->numBis > pLctrRtCfg->maxBis)
  {
    LL_TRACE_WARN0("LctrSlvBisCreateBig: exceeds maximum allowed BIS context");
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  if (pCreateBig->numBis > lctrGetNumAvailBisCtx())
  {
    LL_TRACE_WARN0("LctrSlvBisCreateBig: insufficient BIS context");
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  if ((pBigCtx = lctrFindBigByHandle(pCreateBig->bigHandle)) != NULL)
  {
    LL_TRACE_WARN1("LctrSlvBisCreateBig: bigHandle=%u already in use", pCreateBig->bigHandle);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }
  else
  {
    if (lctrBigIsPerAdvUsed(pCreateBig->advHandle) == TRUE)
    {
      LL_TRACE_WARN1("LctrSlvBisCreateBig: advHandle=%u not available", pCreateBig->advHandle);
      return LL_ERROR_CODE_UNKNOWN_ADV_ID;
    }

    if ((pBigCtx = lctrAllocBigCtx(pCreateBig->bigHandle)) == NULL)
    {
      LL_TRACE_WARN0("LctrSlvBisCreateBig: insufficient BIG context");
      return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
    }
  }

  lctrSlvSetupBigContext(pBigCtx, pCreateBig);
  lctrSlvSetupBigChannel(pBigCtx);

  if (LlMathDivideUint32RoundUp(pBigCtx->maxSdu, pBigCtx->maxPdu) > LL_MAX_FRAG)
  {
    LL_TRACE_WARN1("LctrSlvBisCreateBig: exceeds maximum fragments max=%u", LL_MAX_FRAG);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  for (unsigned int i = 0; i < pCreateBig->numBis; i++)
  {
    lctrBisCtx_t *pBisCtx;

    pBisCtx = lctrAllocBisCtx(pBigCtx);

    if (pBisCtx)
    {
      lctrSetupBisContext(pBisCtx, pBigCtx->seedAccAddr, pBigCtx->baseCrcInit, lmgrCb.chanClass,
                          (LlPhy_t)pBigCtx->phy);
    }
    else
    {
      lctrFreeBigCtx(pBigCtx);
      return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
    }
  }

  /* Assign function pointer for ACAD BIG. */
  lctrAdvSet_t *pAdvSet = lctrFindAdvSet(pCreateBig->advHandle);
  if (pAdvSet)
  {
    pBigCtx->roleData.slv.pAdvSet = pAdvSet;
    pAdvSet->bigCreated = &lctrSlvBisAcadBigCreated;
    pAdvSet->bigTerminated = &lctrSlvBisAcadBigTerminated;
    pAdvSet->removeCback = lctrSlvBisRemoveAdvSet;
  }

  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = pCreateBig->bigHandle;
    pMsg->dispId = LCTR_DISP_BIG_BCST;
    pMsg->event = LCTR_SLV_BIG_MSG_CREATE_BIG;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Create BIG test command.
 *
 *  \param  pCreateBigTest  Create BIG test parameters.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrSlvBisCreateBigTest(LlCreateBigTest_t *pCreateBigTest)
{
  lctrBigCtx_t *pBigCtx;

  if (pCreateBigTest->numBis > lctrGetNumAvailBisCtx())
  {
    LL_TRACE_WARN0("LctrSlvBisCreateBigTest: insufficient BIS context");
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  if ((pBigCtx = lctrFindBigByHandle(pCreateBigTest->bigHandle)) != NULL)
  {
    LL_TRACE_WARN1("LctrSlvBisCreateBigTest: bigHandle=%u already in use", pCreateBigTest->bigHandle);
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }
  else
  {
    if (lctrBigIsPerAdvUsed(pCreateBigTest->advHandle) == TRUE)
    {
      LL_TRACE_WARN1("LctrSlvBisCreateBigTest: advHandle=%u not available", pCreateBigTest->advHandle);
      return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
    }

    if ((pBigCtx = lctrAllocBigCtx(pCreateBigTest->bigHandle)) == NULL)
    {
      LL_TRACE_WARN0("LctrSlvBisCreateBigTest: insufficient BIG context");
      return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
    }
  }

  lctrSlvSetupBigTestContext(pBigCtx, pCreateBigTest);
  lctrSlvSetupBigChannel(pBigCtx);

  if (LlMathDivideUint32RoundUp(pBigCtx->maxSdu, pBigCtx->maxPdu) > LL_MAX_FRAG)
  {
    LL_TRACE_WARN1("LctrSlvBisCreateBig: exceeds maximum fragments max=%u", LL_MAX_FRAG);
    return LL_ERROR_CODE_INVALID_HCI_CMD_PARAMS;
  }

  for (unsigned int i = 0; i < pCreateBigTest->numBis; i++)
  {
    lctrBisCtx_t *pBisCtx;

    pBisCtx = lctrAllocBisCtx(pBigCtx);

    if (pBisCtx)
    {
      lctrSetupBisContext(pBisCtx, pBigCtx->seedAccAddr, pBigCtx->baseCrcInit, lmgrCb.chanClass,
                          (LlPhy_t)pBigCtx->phy);
    }
    else
    {
      lctrFreeBigCtx(pBigCtx);
      return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
    }
  }

  /* Enable BIG Info ACAD. */
  lctrAdvSet_t *pAdvSet = lctrFindAdvSet(pCreateBigTest->advHandle);
  if (pAdvSet)
  {
    pBigCtx->roleData.slv.pAdvSet = pAdvSet;
    pAdvSet->bigCreated = lctrSlvBisAcadBigCreated;
    pAdvSet->bigTerminated = lctrSlvBisAcadBigTerminated;
    pAdvSet->removeCback = lctrSlvBisRemoveAdvSet;
  }

  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = pCreateBigTest->bigHandle;
    pMsg->dispId = LCTR_DISP_BIG_BCST;
    pMsg->event = LCTR_SLV_BIG_MSG_CREATE_BIG;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Terminate BIG.
 *
 *  \param  bigHandle   BIG handle.
 *  \param  reason      Termination reason.
 *
 *  \return Status error code.
 */
/*************************************************************************************************/
uint8_t LctrSlvBisTerminateBig(uint8_t bigHandle, uint8_t reason)
{
  lctrBigCtx_t *pBigCtx;

  if ((pBigCtx = lctrFindBigByHandle(bigHandle)) == NULL)
  {
    LL_TRACE_WARN1("LctrSlvBisTerminateBig: invalid BIG handle=%u", bigHandle);
    return LL_ERROR_CODE_UNKNOWN_ADV_ID;
  }

  if (pBigCtx->role != LL_ROLE_SLAVE)
  {
    LL_TRACE_WARN0("LctrSlvBisTerminateBig: invalid role");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  if (pBigCtx->state != LCTR_SLV_BIG_STATE_ENABLED)
  {
    LL_TRACE_WARN0("LctrSlvBisTerminateBig: invalid state");
    return LL_ERROR_CODE_CMD_DISALLOWED;
  }

  /* Store reason code. */
  pBigCtx->bcp.term.reason = reason;

  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->handle = bigHandle;
    pMsg->dispId = LCTR_DISP_BIG_BCST;
    pMsg->event = LCTR_SLV_BIG_MSG_TERMINATE_BIG;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of Create BIG complete event
 *
 *  \param      pBigCtx           BIG context.
 *  \param      status            Status.
 */
/*************************************************************************************************/
void lctrNotifyHostCreateBigComplete(lctrBigCtx_t *pBigCtx, uint8_t status)
{
  LlCreateBigCnf_t evt;

  /* Clear not required; all values are written. */
  /* memset(&evt, 0, sizeof(LlCreateBigCnf_t)); */

  evt.hdr.param = pBigCtx->handle;
  evt.hdr.event = LL_CREATE_BIG_CNF;
  evt.hdr.status = status;

  evt.status = status;
  evt.bigHandle = pBigCtx->handle;

  if (evt.status == LL_SUCCESS)
  {
    evt.syncDelayUsec = pBigCtx->syncDelayUsec;
    evt.transLatUsec = pBigCtx->transLatUsec;
    evt.phy = pBigCtx->phy;
    evt.nse = pBigCtx->nse;
    evt.bn = pBigCtx->bn;
    evt.pto = pBigCtx->pto;
    evt.irc = pBigCtx->irc;
    evt.maxPdu = pBigCtx->maxPdu;
    evt.isoInterval = LL_MATH_DIV_1250(pBigCtx->isoInterUsec);
    evt.numBis = pBigCtx->numBis;

    for (unsigned int i = 0; i < evt.numBis; i++)
    {
      evt.bisHandle[i] = pBigCtx->pBisCtx[i]->handle;
    }
  }

  LL_TRACE_INFO2("### LlEvent ###  LL_CREATE_BIG_CNF, bigHandle=%u, status=%u", pBigCtx->handle, status);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief      Notify host of Create BIG complete event
 *
 *  \param      pBigCtx           BIG context.
 */
/*************************************************************************************************/
void lctrNotifyHostTerminateBigComplete(lctrBigCtx_t *pBigCtx)
{
  LlTerminateBigInd_t evt;

  /* Clear not required; all values are written. */
  /* memset(&evt, 0, sizeof(LlTerminateBigInd_t)); */

  evt.hdr.param = pBigCtx->handle;
  evt.hdr.event = LL_TERM_BIG_IND;
  evt.hdr.status = LL_SUCCESS;

  evt.bigHandle = pBigCtx->handle;
  evt.reason = pBigCtx->bcp.term.reason;

  LL_TRACE_INFO2("### LlEvent ###  LL_TERM_BIG_IND, bigHandle=%u, reason=%u", pBigCtx->handle, pBigCtx->bcp.term.reason);

  LmgrSendEvent((LlEvt_t *)&evt);
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal slave BIG subsystem message.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  event       Slave BIS event.
 */
/*************************************************************************************************/
void lctrSlvBigSendMsg(lctrBigCtx_t *pBigCtx, uint8_t event)
{
  lctrMsgHdr_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(lctrMsgHdr_t))) != NULL)
  {
    pMsg->handle = pBigCtx->handle;
    pMsg->dispId = LCTR_DISP_BIG_BCST;
    pMsg->event = event;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send internal slave BIS ACAD subsystem message.
 *
 *  \param  pBigCtx     BIG context.
 *  \param  event       Slave BIS event.
 */
/*************************************************************************************************/
void lctrSlvBigSendAcadMsg(lctrBigCtx_t *pBigCtx, uint8_t event)
{
  if (!pBigCtx->roleData.slv.pAdvSet)
  {
    return;
  }

  lctrBigCreated_t *pMsg;

  if ((pMsg = (lctrBigCreated_t *)WsfMsgAlloc(sizeof(*pMsg))) != NULL)
  {
    pMsg->hdr.handle = pBigCtx->roleData.slv.pAdvSet->handle;
    pMsg->hdr.dispId = LCTR_DISP_ACAD;
    pMsg->hdr.event  = event;
    pMsg->bigHandle = pBigCtx->handle;

    WsfMsgSend(lmgrPersistCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Build BIG operation.
 *
 *  \param  pBigCtx     BIG context.
 *
 *  \return Error status code.
 */
/*************************************************************************************************/
uint8_t lctrSlvBigBuildOp(lctrBigCtx_t *pBigCtx)
{
  lctrBisCtx_t * const pBisCtx = pBigCtx->pBisCtx[0];

  BbOpDesc_t * const pOp = &pBigCtx->bod;
  BbBleData_t * const pBle = &pBigCtx->bleData;
  BbBleSlvBisEvent_t * const pBis = &pBle->op.slvBis;

  memset(pOp, 0, sizeof(BbOpDesc_t));
  memset(pBle, 0, sizeof(BbBleData_t));
  memset(pBis, 0, sizeof(BbBleSlvBisEvent_t));

  /*** General Setup ***/

  pOp->reschPolicy = BB_RESCH_FIXED_PREFERRED;
  pOp->protId = BB_PROT_BLE;
  pOp->prot.pBle = pBle;
  pOp->endCback = lctrSlvBigEndOp;
  pOp->abortCback = lctrSlvBigAbortOp;
  pOp->pCtx = pBigCtx;

  /*** BLE General Setup ***/

  pBle->chan = pBisCtx->chan;

  /*** General setup ***/

  pOp->minDurUsec = pBigCtx->syncDelayUsec;
  pOp->maxDurUsec = pBigCtx->syncDelayUsec;

  /*** BIS setup ***/

  pBis->execCback = lctrSlvBigBeginOp;

  switch (pBigCtx->packing)
  {
    case LL_PACKING_INTERLEAVED:
      pBis->txDataCback = lctrSlvBisTxCompletionInterleaved;
      break;

    case LL_PACKING_SEQUENTIAL:
    default:
      pBis->txDataCback = lctrSlvBisTxCompletionSequential;
      break;
  }

  /*** Commit operation ***/

  if (!SchRmAdd(LCTR_BIG_TO_RM_HANDLE(pBigCtx), SCH_RM_PREF_PERFORMANCE,
                pBigCtx->isoInterUsec, pBigCtx->isoInterUsec, pOp->minDurUsec,
                NULL, lctrGetBigRefTime))
  {
    LL_TRACE_WARN1("Could not schedule bigHandle=%u", pBigCtx->handle);
    return LL_ERROR_CODE_CONN_REJ_LIMITED_RESOURCES;
  }

  const uint32_t refTime = pBigCtx->roleData.slv.pAdvSet->advBod.dueUsec;
  uint32_t offsetUsec = SchRmGetOffsetUsec(pBigCtx->isoInterUsec,
                                           LCTR_BIG_TO_RM_HANDLE(pBigCtx), refTime);
  pOp->dueUsec = refTime + offsetUsec;

  while (TRUE)
  {
    lctrSelectBigChannels(pBigCtx);

    if (SchInsertAtDueTime(pOp, NULL))
    {
      break;
    }

    LL_TRACE_WARN1("!!! BIG schedule conflict handle=%u", pBigCtx->handle);

    /* Advance to next interval. */
    pBigCtx->eventCounter += 1;
    pOp->dueUsec += pBigCtx->isoInterUsec;
  }

  LL_TRACE_INFO2("    >>> BIG started, handle=%u, numBis=%u <<<", pBigCtx->handle, pBigCtx->numBis);
  LL_TRACE_INFO3("                     bn=%u, nse=%u, irc=%u", pBigCtx->bn, pBigCtx->nse, pBigCtx->irc);
  LL_TRACE_INFO3("                     pto=%u, framing=%u, packing=%u", pBigCtx->pto, pBigCtx->framing, pBigCtx->packing);
  LL_TRACE_INFO1("                     isoInterUsec=%u", pBigCtx->isoInterUsec);
  LL_TRACE_INFO1("                     bisSpaceUsec=%u", pBigCtx->bisSpaceUsec);
  LL_TRACE_INFO1("                     subInterUsec=%u", pBigCtx->subInterUsec);
  LL_TRACE_INFO1("                     seedAccAddr=0x%08x", pBigCtx->seedAccAddr);
  LL_TRACE_INFO1("                     pBod=0x%08x", pOp);

  return LL_SUCCESS;
}
