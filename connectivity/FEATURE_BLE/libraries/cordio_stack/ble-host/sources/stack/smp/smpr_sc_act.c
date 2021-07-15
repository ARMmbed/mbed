/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP Secure Connections responder state machine action functions.
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
 *  \brief  Store the pin.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActStoreLescPin(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  if (pCcb->pScCcb->authType == SMP_AUTH_TYPE_PASSKEY)
  {
    /* Store the pin */
    Calc128Cpy(pCcb->pScCcb->pScratch->Ra, (uint8_t *)calc128Zeros);
    Calc128Cpy(pCcb->pScCcb->pScratch->Rb, (uint8_t *)calc128Zeros);

    if (pMsg->dm.authRsp.authDataLen <= 3)
    {
      WStrReverseCpy(&pCcb->pScCcb->pScratch->Ra[13], pMsg->dm.authRsp.authData, pMsg->dm.authRsp.authDataLen);
      WStrReverseCpy(&pCcb->pScCcb->pScratch->Rb[13], pMsg->dm.authRsp.authData, pMsg->dm.authRsp.authDataLen);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Responder public key exchange.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActSendPubKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Execute Common Auth Select actions */
  smpScActAuthSelect(pCcb, pMsg);

  /* Verify auth select didn't result in a cancel request */
  if (pCcb->state)
  {
    /* Send our public key */
    smpScSendPubKey(pCcb, pMsg);
  }
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
void smprScActJwncSetup(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Select Random Na (128-bits) */
  SecRand(pCcb->pScCcb->pScratch->Nb_Eb, SMP_RAND_LEN);

  SMP_TRACE_128("Rand Nb", pCcb->pScCcb->pScratch->Nb_Eb);

  /* Set Ra and Rb to zero */
  Calc128Cpy(pCcb->pScCcb->pScratch->Ra, (uint8_t*) calc128Zeros);
  Calc128Cpy(pCcb->pScCcb->pScratch->Rb, (uint8_t*) calc128Zeros);

  /* Next command is a Pair Rand from Initiator */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;

  /* Perform F4 Calculation of Cb */
  smpScActJwncCalcF4(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Send the confirm to the initiator for Just Works/Numeric Comparison pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActJwncSendCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("JWNC Confirm", pMsg->aes.pCiphertext);

  smpScSendPairCnf(pCcb, pMsg, pMsg->aes.pCiphertext);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate Toolkit function G2 for Just Works pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActJwncCalcG2(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pNa = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Na from initiator is in Random Cmd from initiator */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Na_Ea, pNa, SMP_RAND_LEN);

  /* Calculate Vb using G2 */
  smpScActJwncCalcG2(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
*  \brief  Notify the application of the verify value calculated with Just Works pairing
*
*  \param  pCcb  Connection control block.
*  \param  pMsg  State machine message.
*
*  \return None.
*/
/*************************************************************************************************/
void smprScActJwncDisplay(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Next command is a DH Key Check  */
  pCcb->nextCmdCode = SMP_CMD_DHKEY_CHECK;

  /* Send Pair Rand Nb to the Initiator */
  smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Nb_Eb);

  /* Send Numeric Comparison to application, if applicable */
  smpScActJwncDisplay(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Store the confirm value from the initiator
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkStoreCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCa = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Store the Cai from the initiator */
  WStrReverseCpy(pCcb->pScCcb->pScratch->PeerCa_Ea, pCa, SMP_CONFIRM_LEN);
}

/*************************************************************************************************/
/*!
 *  \brief  Store the Confirm and Calculate the Cbi using toolkit function F4 for passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkStoreCnfAndCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smprScActPkStoreCnf(pCcb, pMsg);
  smprScActPkCalcCb(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Store the Pin and Calculate the Cbi using toolkit function F4 for passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkStorePinAndCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smprScActStoreLescPin(pCcb, pMsg);
  smprScActPkCalcCb(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the Cbi using toolkit function F4 for passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkCalcCb(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Get random Nbi */
  SecRand(pCcb->pScCcb->pScratch->Nb_Eb, SMP_RAND_LEN);

  SMP_TRACE_128("Rand Nb", pCcb->pScCcb->pScratch->Nb_Eb);

  /* Next command is the Pair Random */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;

  /* Cb = f4(PKbx, PKax, Nbi, Rbi) where f4(U, V, x, Z) = AES-CMACx (U || V || Z) */
  SmpScCalcF4(pCcb, pMsg,
              pCcb->pScCcb->pLocalPublicKey->pubKeyX,
              pCcb->pScCcb->pPeerPublicKey->pubKeyX,
              smpGetPkBit(pCcb), pCcb->pScCcb->pScratch->Nb_Eb);
}

/*************************************************************************************************/
/*!
 *  \brief  Send the Cbi to the initiator in a Confirm Command Message in passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkSendCnf(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("Cbi", pMsg->aes.pCiphertext);

  /* Send the Cbi to the peer */
  smpScSendPairCnf(pCcb, pMsg, pMsg->aes.pCiphertext);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the Cai to be checked against the confirm value from the initiator
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkCalcCa(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pNa = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Copy the Nai from the initiator */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Na_Ea, pNa, SMP_RAND_LEN);

  /* Cai = f4(PKax, PKbx, Nbi, Rbi) where f4(U, V, x, Z) = AES-CMACx (U || V || Z) */
  SmpScCalcF4(pCcb, pMsg,
              pCcb->pScCcb->pPeerPublicKey->pubKeyX,
              pCcb->pScCcb->pLocalPublicKey->pubKeyX,
              smpGetPkBit(pCcb), pCcb->pScCcb->pScratch->Na_Ea);
}

/*************************************************************************************************/
/*!
 *  \brief  Send the Nai in a Random Command Message in passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActPkSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("Ca", pMsg->aes.pCiphertext);
  SMP_TRACE_128("Ca Peer", pCcb->pScCcb->pScratch->PeerCa_Ea);

  /* Verify the Calculated Cai to previously received Cai */
  if (memcmp(pCcb->pScCcb->pScratch->PeerCa_Ea, pMsg->aes.pCiphertext, SMP_RAND_LEN))
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

      /* Send the Nbi */
      smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Nb_Eb);
    }

    /* Post an event to move to the next passkey confirm or complete the process */
    hdr.param = pCcb->connId;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Setup for OOB pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActOobSetup(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* The next command is the Pair Rand from the initiator */
  pCcb->nextCmdCode = SMP_CMD_PAIR_RAND;
}

/*************************************************************************************************/
/*!
 *  \brief  Check the confirm passed via OOB methods
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActOobCalcCa(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pNa = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Copy the Na from the initiator */
  WStrReverseCpy(pCcb->pScCcb->pScratch->Na_Ea, pNa, SMP_CONFIRM_LEN);

  /* If the peer device's OOB data flag does not indicate remote OOB data has been received,
     clear Rb. */
  if (pCcb->pairReq[SMP_OOB_POS] != SMP_OOB_DATA_PRESENT)
  {
    Calc128Cpy(pCcb->pScCcb->pScratch->Rb, (uint8_t*) calc128Zeros);
  }

  /* If we indicated the presence of remote OOB data has been received, calculate Ca. */
  if (pCcb->pairRsp[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT)
  {
    /* Calculate Ca using Toolkit function F4 */
    SmpScCalcF4(pCcb, pMsg,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                0, pCcb->pScCcb->pScratch->Ra);
  }
  else
  {
    /* Simulate the ca calculation is complete and clear ra */
    secCmacMsg_t msg;

    Calc128Cpy(pCcb->pScCcb->pScratch->Ra, (uint8_t*)calc128Zeros);

    msg.hdr.param = pCcb->connId;
    msg.hdr.event = SMP_MSG_WSF_CMAC_CMPL;
    msg.pPlainText = NULL;

    smpSmExecute(pCcb, (smpMsg_t *) &msg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send the random value to the initiator when using OOB pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActOobSendRand(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Only compare Ca if we indicated that we received OOB data. */
  if (pCcb->pairRsp[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT)
  {
    SMP_TRACE_128("Ca", pMsg->aes.pCiphertext);

    /* Check that the Ca value passed via OOB methods match expectations */
    if (memcmp(pCcb->pScCcb->pScratch->PeerCa_Ea, pMsg->aes.pCiphertext, SMP_CONFIRM_LEN))
    {
      smpScFailWithReattempt(pCcb, SMP_ERR_CONFIRM_VALUE);
      return;
    }
  }

  /* Next command is a DK Key Check from initiator */
  pCcb->nextCmdCode = SMP_CMD_DHKEY_CHECK;

  /* Get a random Nb */
  SecRand(pCcb->pScCcb->pScratch->Nb_Eb, SMP_RAND_LEN);

  SMP_TRACE_128("Rand Nb", pCcb->pScCcb->pScratch->Nb_Eb);

  /* Send the rand Nb to the initiator */
  smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Nb_Eb);
}

/*************************************************************************************************/
/*!
*  \brief  Store DH Key Check from Initiator and wait for user input on Numeric Comparison
*
*  \param  pCcb  Connection control block.
*  \param  pMsg  State machine message.
*
*  \return None.
*/
/*************************************************************************************************/
void smprScActStoreDhCheck(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pEa = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  /* Signal that not further commands are expected until Key Distribution phase (if applicable) */
  pCcb->nextCmdCode = SMP_CMD_MAX;

  /* The Ea from the peer is in the cmd message (copy as MSB First) */
  WStrReverseCpy(pCcb->pScCcb->pScratch->PeerCa_Ea, pEa, SMP_CONFIRM_LEN);
}

/*************************************************************************************************/
/*!
 *  \brief  Prepare to wait for the DH Key Check Cmd from the initiator
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActWaitDhCheck(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Next command is a DH Key Check */
  pCcb->nextCmdCode = SMP_CMD_DHKEY_CHECK;

  if (pCcb->pScCcb->authType == SMP_AUTH_TYPE_PASSKEY)
  {
    /* Send the Pair Rand Nb */
    smpScSendRand(pCcb, pMsg, pCcb->pScCcb->pScratch->Nb_Eb);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the DHKey, for Cryptographic Function 5
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smprScActCalcDHKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  if (pCcb->nextCmdCode == SMP_CMD_DHKEY_CHECK)
  {
    uint8_t *pEa = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

    /* The Ea from the peer is in the cmd message (copy as MSB First) */
    WStrReverseCpy(pCcb->pScCcb->pScratch->PeerCa_Ea, pEa, SMP_CONFIRM_LEN);
  }

  /* Complete the calculation */
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
void smprScActDHKeyCheckSend(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  SMP_TRACE_128("DHKey Eb", pMsg->aes.pCiphertext);

  /* The Eb from the CMAC calculation */
  Calc128Cpy(pCcb->pScCcb->pScratch->Nb_Eb, pMsg->aes.pCiphertext);

  /* Verify the DH Key Check Ea with the value received from the initiator */
  if (memcmp(pCcb->pScCcb->pScratch->PeerCa_Ea, pCcb->pScCcb->pScratch->Na_Ea, SMP_RAND_LEN) == 0)
  {
    /* Adjust key based on max key length */
    uint8_t  encKeyLen = (pCcb->pairReq[SMP_MAXKEY_POS] < pCcb->pairRsp[SMP_MAXKEY_POS]) ?
                          pCcb->pairReq[SMP_MAXKEY_POS] : pCcb->pairRsp[SMP_MAXKEY_POS];

    memset((pCcb->pScCcb->pLtk->ltk_t + encKeyLen), 0, (SMP_KEY_LEN - encKeyLen));
    pCcb->keyReady = TRUE;
    
    SMP_TRACE_128("LTK:", pCcb->pScCcb->pLtk->ltk_t);

    /* Send the DH Key check Eb to the initiator */
    smpScSendDHKeyCheck(pCcb, pMsg, pCcb->pScCcb->pScratch->Nb_Eb);
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

