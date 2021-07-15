/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP Secure Connections initiator state machine action functions.
 *
 *  Copyright (c) 2010-2018 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "util/wstr.h"
#include "smp_api.h"
#include "smp_main.h"
#include "smpi_main.h"
#include "smp_sc_main.h"
#include "dm_api.h"

/*************************************************************************************************/
/*!
 *  \brief  Process a public key and perform common auth select actions
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActAuthSelect(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Execute Common Auth Select actions */
  smpScActAuthSelect(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Send public key to peer.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActSendPubKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Next command is the Public Key from the responder */
  pCcb->nextCmdCode = SMP_CMD_PUBLIC_KEY;

  /* Send the public key */
  smpScSendPubKey(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Prepare for the Just Works/Numeric Comparison Use Case
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActJwncSetup(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Select Random Na (128-bits) */
  SecRand(pCcb->pScCcb->pScratch->Na_Ea, SMP_RAND_LEN);

  SMP_TRACE_128("Rand Na", pCcb->pScCcb->pScratch->Na_Ea);

  /* Set Ra and Rb to sero */
  Calc128Cpy(pCcb->pScCcb->pScratch->Ra, (uint8_t*) calc128Zeros);
  Calc128Cpy(pCcb->pScCcb->pScratch->Rb, (uint8_t*) calc128Zeros);

  /* Next command is a Pair Confirm from Responder */
  pCcb->nextCmdCode = SMP_CMD_PAIR_CNF;
}

/*************************************************************************************************/
/*!
 *  \brief  Send the rand value to the responder for Just Works use case
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActJwncSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCb = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Cb from responder is in Confirm from rsponder */
  WStrReverseCpy(pCcb->pScCcb->pScratch->PeerCb, pCb, SMP_CONFIRM_LEN);

  SMP_TRACE_128("Peer Cb", pCcb->pScCcb->pScratch->PeerCb);

  /* Next command is a Pair Random from Responder */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;

  /* Send the Pair Rand */
  smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Na_Ea);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the confirm value for just works use case
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActJwncCalcF4(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pNb = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Nb from responder is in pPacket */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Nb_Eb, pNb, SMP_RAND_LEN);

  smpScActJwncCalcF4(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the verify value for the just works use case
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActJwncCalcG2(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  secCmacMsg_t *pCmac = (secCmacMsg_t *) pMsg;

  SMP_TRACE_128("Local Cb", pCmac->pCiphertext);

  /* Check the result of the F4 confirm calculation */
  if (memcmp(pCcb->pScCcb->pScratch->PeerCb, pCmac->pCiphertext, SMP_CONFIRM_LEN))
  {
    smpScFailWithReattempt(pCcb, SMP_ERR_CONFIRM_VALUE);
  }
  else
  {
    smpScActJwncCalcG2(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the Cai for the passkey use case using toolkit function F4
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActPkCalcCa(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Record the passkey on the first confirm */
  if (pCcb->pScCcb->pkPos == 0)
  {
    Calc128Cpy(pCcb->pScCcb->pScratch->Ra, (uint8_t *)calc128Zeros);
    Calc128Cpy(pCcb->pScCcb->pScratch->Rb, (uint8_t *)calc128Zeros);

    if (pMsg->dm.authRsp.authDataLen <= 3)
    {
      WStrReverseCpy(&pCcb->pScCcb->pScratch->Ra[13], pMsg->dm.authRsp.authData, pMsg->dm.authRsp.authDataLen);
      WStrReverseCpy(&pCcb->pScCcb->pScratch->Rb[13], pMsg->dm.authRsp.authData, pMsg->dm.authRsp.authDataLen);
    }
  }

  /* Get random Nai */
  SecRand(pCcb->pScCcb->pScratch->Na_Ea, SMP_RAND_LEN);

  SMP_TRACE_128("Rand Na", pCcb->pScCcb->pScratch->Na_Ea);

  /* Ca = f4(PKax, PKbx, Nai, Rai) where f4(U, V, x, Z) = AES-CMACx (U || V || Z) */
  SmpScCalcF4(pCcb, pMsg,
              pCcb->pScCcb->pLocalPublicKey->pubKeyX,
              pCcb->pScCcb->pPeerPublicKey->pubKeyX,
              smpGetPkBit(pCcb), pCcb->pScCcb->pScratch->Na_Ea);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the Cbi for the passkey use case using toolkit function F4
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActPkCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pNb = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Record the Nbi */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Nb_Eb, pNb, SMP_RAND_LEN);

  /* Verify the random value was not reflected */
  if (!memcmp(pCcb->pScCcb->pScratch->Na_Ea, pCcb->pScCcb->pScratch->Nb_Eb, SMP_RAND_LEN))
  {
    smpScFailWithReattempt(pCcb, SMP_ERR_UNSPECIFIED);
  }
  else
  {
    /* Cb = f4(PKbx, PKax, Nbi, Rai) where f4(U, V, x, Z) = AES-CMACx (U || V || Z) */
    SmpScCalcF4(pCcb, pMsg,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                pCcb->pScCcb->pLocalPublicKey->pubKeyX,
                smpGetPkBit(pCcb), pCcb->pScCcb->pScratch->Nb_Eb);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send the Cai Confirm for the passkey use case command for passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActPkSendCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("Cai", pMsg->aes.pCiphertext);
  
  /* Keep a copy of the confirm value we send to the peer */
  memcpy(pCcb->pScCcb->pScratch->LocalCb, pMsg->aes.pCiphertext, SMP_RAND_LEN);

  /* Send the Cai to the peer */
  smpScSendPairCnf(pCcb, pMsg, pMsg->aes.pCiphertext);
}

/*************************************************************************************************/
/*!
 *  \brief  Send the Nai Random command for passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActPkSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCb = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Record the Cbi from the responder */
  WStrReverseCpy(pCcb->pScCcb->pScratch->PeerCb, pCb, SMP_CONFIRM_LEN);
  
  /* Verify confirm value is not reflected */
  if (!memcmp(pCcb->pScCcb->pScratch->PeerCb, pCcb->pScCcb->pScratch->LocalCb, SMP_RAND_LEN))
  {
    smpScFailWithReattempt(pCcb, SMP_ERR_UNSPECIFIED);
  }
  else
  {
    /* Next command is the Pair Random */
    pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;

    /* Send the Nai */
    smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Na_Ea);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check the Cbi from the responder against the calculated Cbi for the passkey use case
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActPkCheck(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("Cbi", pMsg->aes.pCiphertext);

  /* Verify the Calculated Cbi to previously received Cbi */
  if (memcmp(pCcb->pScCcb->pScratch->PeerCb, pMsg->aes.pCiphertext, SMP_RAND_LEN))
  {
    smpScFailWithReattempt(pCcb, SMP_ERR_CONFIRM_VALUE);
  }
  else
  {
     wsfMsgHdr_t hdr;

     /* Increment the bit position */
    if (++pCcb->pScCcb->pkPos >= SMP_PK_BIT_COUNT)
    {
      hdr.event = SMP_MSG_INT_PK_CMPL;
    }
    else
    {
      /* Next command is the Pair Confirm */
      pCcb->nextCmdCode = SMP_CMD_PAIR_CNF;

      hdr.event = SMP_MSG_INT_PK_NEXT;
    }

    /* Post an event to move to the next passkey confirm or complete the process */
    hdr.param = pCcb->connId;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the Cb to compare against the Cb sent via OOB methods
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActOobCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* If the peer device's OOB data flag does not indicate remote OOB data has been received,
     clear Ra. */
  if (pCcb->pairRsp[SMP_OOB_POS] != SMP_OOB_DATA_PRESENT)
  {
    Calc128Cpy(pCcb->pScCcb->pScratch->Ra, (uint8_t*) calc128Zeros);
  }

  /* If we indicated the presence of remote OOB data has been received, calculate Cb. */
  if (pCcb->pairReq[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT)
  {
    /* Calculate Cb using Toolkit function F4 */
    SmpScCalcF4(pCcb, pMsg,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                0, pCcb->pScCcb->pScratch->Rb);
  }
  else
  {
    /* Simulate the cb calculation is complete and clear rb */
    secCmacMsg_t msg;

    Calc128Cpy(pCcb->pScCcb->pScratch->Rb, (uint8_t*)calc128Zeros);

    msg.hdr.param = pCcb->connId;
    msg.hdr.event = SMP_MSG_WSF_CMAC_CMPL;
    msg.pPlainText = NULL;

    smpSmExecute(pCcb, (smpMsg_t *) &msg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send random Na to responder in OOB pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActOobSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Only compare Cb if we indicated that we received OOB data. */
  if (pCcb->pairReq[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT)
  {
    SMP_TRACE_128("Initiator Cb", pMsg->aes.pCiphertext);

    /* Verify the Cb matches the value passed from the responder via OOB methods */
    if (memcmp(pCcb->pScCcb->pScratch->PeerCb, pMsg->aes.pCiphertext, SMP_CONFIRM_LEN))
    {
      smpScFailWithReattempt(pCcb, SMP_ERR_CONFIRM_VALUE);
      return;
    }
  }

  /* Next command is a Pair Rand from Responder */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;

  /* Calculate a the Na */
  SecRand(pCcb->pScCcb->pScratch->Na_Ea, SMP_RAND_LEN);

  SMP_TRACE_128("Rand Na", pCcb->pScCcb->pScratch->Na_Ea);

  /* Send the Na to the responder */
  smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Na_Ea);
}

/*************************************************************************************************/
/*!
 *  \brief  Process random Nb from responder in OOB pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActOobProcRand(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pNb = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Copy the Nb from the responder */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Nb_Eb, pNb, SMP_RAND_LEN);

  /* Initiate the DH Check */
  smpScActCalcSharedSecret(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Send the DH Key check.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActDHKeyCheckSend(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("DHKey Eb", pMsg->aes.pCiphertext);

  /* Copy Eb from the smpScActDHKeyCalcF6Eb in LSB first format (as it will be received from peer) */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Nb_Eb, pMsg->aes.pCiphertext, SMP_RAND_LEN);

  /* Next cmd message is the DH Key Check from the responder */
  pCcb->nextCmdCode = SMP_CMD_DHKEY_CHECK;

  /* Send the DH Key check with Ea to the responder */
  smpScSendDHKeyCheck(pCcb, pMsg, pCcb->pScCcb->pScratch->Na_Ea);
}

/*************************************************************************************************/
/*!
 *  \brief  Verify the DH Key Check.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpiScActDHKeyCheckVerify(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pEbPeer = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;;

  /* Verify the DH Key Check Eb with the value received from the responder */
  if (memcmp(pEbPeer, pCcb->pScCcb->pScratch->Nb_Eb, SMP_RAND_LEN) == 0)
  {
    uint8_t buf[SMP_KEY_LEN];
    uint8_t encKeyLen;

    encKeyLen = (pCcb->pairReq[SMP_MAXKEY_POS] < pCcb->pairRsp[SMP_MAXKEY_POS]) ?
                 pCcb->pairReq[SMP_MAXKEY_POS] : pCcb->pairRsp[SMP_MAXKEY_POS];

    /* Adjust key based on max key length */
    memcpy(buf, pCcb->pScCcb->pLtk->ltk_t, encKeyLen);
    memset((buf + encKeyLen), 0, (SMP_KEY_LEN - encKeyLen));
    pCcb->keyReady = TRUE;
    
    SMP_TRACE_128("LTK:", pCcb->pScCcb->pLtk->ltk_t);

    /* Initiate encryption */
    DmSmpEncryptReq(pCcb->connId, smpGetScSecLevel(pCcb), buf);
  }
  else
  {
    /* DH Key check failed */
    wsfMsgHdr_t hdr;

    hdr.param = pCcb->connId;
    hdr.status = SMP_ERR_DH_KEY_CHECK;

    /* update repeated attempts count */
    pCcb->attempts++;
    SmpDbPairingFailed(pCcb->connId);

    if (pCcb->attempts == pSmpCfg->maxAttempts)
    {
      /* max attempts reached */
      hdr.event = SMP_MSG_INT_MAX_ATTEMPTS;
    }
    else
    {
      /* else just fail */
      hdr.event = SMP_MSG_DH_CHECK_FAILURE;
    }

    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
  }
}
