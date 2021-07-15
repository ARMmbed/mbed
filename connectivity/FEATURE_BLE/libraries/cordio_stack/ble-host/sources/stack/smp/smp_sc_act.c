/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP Secure Connections common action functions.
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
#include "wsf_assert.h"
#include "util/bstream.h"
#include "util/calc128.h"
#include "util/wstr.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_conn.h"
#include "smp_api.h"
#include "smp_main.h"
#include "smp_sc_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifndef SMP_DHKEY_QUAL_INCLUDED
#define SMP_DHKEY_QUAL_INCLUDED FALSE
#endif

/* Min number of differences required in the bits of the local and peer public keys */
#define SMP_PUBLIC_KEY_DELTA_REQUIRED           2

/**************************************************************************************************
  Global Variables
**************************************************************************************************/
static const uint8_t smpScF5Key[] = {0x62, 0x74, 0x6c, 0x65};

#if SMP_DHKEY_QUAL_INCLUDED
static bool_t SMP_DhKeyZero = FALSE;

/*************************************************************************************************/
/*!
 *  \brief  Called to force the DhKey to zero for qualification test purposes.
 *
 *  \param  enable  TRUE - Force DhKey to zero.  FALSE - Use calculated key
 *
 *  \return None.
 */
/*************************************************************************************************/
void SmpScEnableZeroDhKey(bool_t enable)
{
  SMP_DhKeyZero = enable;
}
#endif /* SMP_DHKEY_QUAL_INCLUDED */

/*************************************************************************************************/
/*!
 *  \brief  Write initator BD Address to a buffer for use with toolkit functions F5 and F6.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pBuf  Buffer to write BD addresses to.
 *
 *  \return None.
 */
/*************************************************************************************************/
static uint8_t *smpScCatInitiatorBdAddr(smpCcb_t *pCcb, uint8_t *pBuf)
{
  dmConnCcb_t *pDmCcb = dmConnCcbById(pCcb->connId);

  if (pDmCcb)
  {
    if (pCcb->initiator)
    {
      /* if local device's using RPA */
      if (!BdaIsZeros(pDmCcb->localRpa))
      {
        *pBuf++ = DM_ADDR_RANDOM;
        WStrReverseCpy(pBuf, pDmCcb->localRpa, BDA_ADDR_LEN);
      }
      else
      {
        *pBuf++ = pDmCcb->localAddrType;
        WStrReverseCpy(pBuf, pDmCcb->localAddr, BDA_ADDR_LEN);
      }
    }
    else
    {
      /* if peer device's using RPA */
      if (!BdaIsZeros(pDmCcb->peerRpa))
      {
        *pBuf++ = DM_ADDR_RANDOM;
        WStrReverseCpy(pBuf, pDmCcb->peerRpa, BDA_ADDR_LEN);
      }
      else
      {
        *pBuf++ = pDmCcb->peerAddrType;
        WStrReverseCpy(pBuf, pDmCcb->peerAddr, BDA_ADDR_LEN);
      }
    }

    pBuf += BDA_ADDR_LEN;
  }

  return pBuf;
}

/*************************************************************************************************/
/*!
 *  \brief  Write responder BD Address to a buffer for use with toolkit functions F5 and F6.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pBuf  Buffer to write BD addresses to.
 *
 *  \return None.
 */
/*************************************************************************************************/
static uint8_t *smpScCatResponderBdAddr(smpCcb_t *pCcb, uint8_t *pBuf)
{
  dmConnCcb_t *pDmCcb = dmConnCcbById(pCcb->connId);

  if (pDmCcb)
  {
    if (pCcb->initiator)
    {
      /* if peer device's using RPA */
      if (!BdaIsZeros(pDmCcb->peerRpa))
      {
        *pBuf++ = DM_ADDR_RANDOM;
        WStrReverseCpy(pBuf, pDmCcb->peerRpa, BDA_ADDR_LEN);
      }
      else
      {
        *pBuf++ = pDmCcb->peerAddrType;
        WStrReverseCpy(pBuf, pDmCcb->peerAddr, BDA_ADDR_LEN);
      }
    }
    else
    {
      /* if local device's using RPA */
      if (!BdaIsZeros(pDmCcb->localRpa))
      {
        *pBuf++ = DM_ADDR_RANDOM;
        WStrReverseCpy(pBuf, pDmCcb->localRpa, BDA_ADDR_LEN);
      }
      else
      {
        *pBuf++ = pDmCcb->localAddrType;
        WStrReverseCpy(pBuf, pDmCcb->localAddr, BDA_ADDR_LEN);
      }
    }

    pBuf += BDA_ADDR_LEN;
  }

  return pBuf;
}

/*************************************************************************************************/
/*!
 *  \brief  Process a pairing request and response data.
 *
 *  \param  pCcb      Connection control block.
 *  \param  pOob      Return parameter, TRUE if out-of-band data requested.
 *  \param  pDisplay  Return parameter, TRUE if pin is to be displayed.
 *
 *  \return TRUE on success, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t smpScProcPairing(smpCcb_t *pCcb, uint8_t *pOob, uint8_t *pDisplay)
{
  bool_t          justWorks = TRUE;
  uint8_t         localAuth;
  wsfMsgHdr_t     hdr;

  *pDisplay = FALSE;
  *pOob = SMP_OOB_DATA_NONE;

  /* check for LE Secure Connections */
  if ((pCcb->pairReq[SMP_AUTHREQ_POS] & SMP_AUTH_SC_FLAG) &&
      (pCcb->pairRsp[SMP_AUTHREQ_POS] & SMP_AUTH_SC_FLAG))
  {
    /* if one or both devices have OOB authentication data */
    if ((pCcb->pairReq[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT) ||
        (pCcb->pairRsp[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT))
    {
      *pOob = SMP_OOB_DATA_PRESENT;
      justWorks = FALSE;
    }
  }
  /* legacy pairing */
  else
  {
    /* if both devices have OOB authentication data */
    if ((pCcb->pairReq[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT) &&
        (pCcb->pairRsp[SMP_OOB_POS] == SMP_OOB_DATA_PRESENT))
    {
      *pOob = SMP_OOB_DATA_PRESENT;
      justWorks = FALSE;
    }
  }

  /* if OOB authentication data not present and either device has set MITM option */
  if ((*pOob == SMP_OOB_DATA_NONE)                           &&
      ((pCcb->pairReq[SMP_AUTHREQ_POS] & SMP_AUTH_MITM_FLAG) ||
       (pCcb->pairRsp[SMP_AUTHREQ_POS] & SMP_AUTH_MITM_FLAG)))
  {
    /* check for compatible I/O settings */
    if ((pCcb->pairReq[SMP_IO_POS] != SMP_IO_NO_IN_NO_OUT) &&     /* initiator has i/o and */
        (pCcb->pairRsp[SMP_IO_POS] != SMP_IO_NO_IN_NO_OUT) &&     /* responder has i/o and */
        !(((pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_ONLY) ||     /* both don't have display only */
           (pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_YES_NO)) &&
          ((pCcb->pairRsp[SMP_IO_POS] == SMP_IO_DISP_ONLY) ||
           (pCcb->pairRsp[SMP_IO_POS] == SMP_IO_DISP_YES_NO))))
    {
      /* use pin */
      justWorks = FALSE;

      /* check if pin should be displayed (as initiator) */
      *pDisplay =
        ((pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_ONLY) ||     /* initiator is display only or */
         (pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_YES_NO) ||   /* initiator is display y/n  or */
         ((pCcb->pairReq[SMP_IO_POS] == SMP_IO_KEY_DISP) &&     /* initiator is key/display and */
          (pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_ONLY ||      /* responder is key only or key/display */
           pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_DISP)));

      /* invert display setting if we are not initiator and both are not key only */
      if (!(pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_ONLY &&
            pCcb->pairReq[SMP_IO_POS] == SMP_IO_KEY_ONLY))
      {
        *pDisplay ^= !pCcb->initiator;
      }
    }
  }

  /* Check for Secure Connections */
  if ((pCcb->pairReq[SMP_AUTHREQ_POS] & SMP_AUTH_SC_FLAG) &&
      (pCcb->pairRsp[SMP_AUTHREQ_POS] & SMP_AUTH_SC_FLAG))
  {
    if (smpCb.lescSupported == FALSE)
    {
      /* cancel pairing if LESC is not initialized by application */
      hdr.param = pCcb->connId;
      hdr.status = SMP_ERR_AUTH_REQ;
      hdr.event = SMP_MSG_API_CANCEL_REQ;
      smpSmExecute(pCcb, (smpMsg_t *) &hdr);
      return FALSE;
    }
    else
    {
      pCcb->pScCcb->lescEnabled = TRUE;
      pCcb->pScCcb->authType = SMP_AUTH_TYPE_JUST_WORKS;
      pCcb->pScCcb->display = *pDisplay;

      if (*pOob)
      {
        pCcb->pScCcb->authType = SMP_AUTH_TYPE_OOB;
      }
      else if (justWorks == FALSE)
      {
        pCcb->pScCcb->authType = SMP_AUTH_TYPE_PASSKEY;

        if (((pCcb->pairRsp[SMP_IO_POS] == SMP_IO_KEY_DISP) &&
             ((pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_YES_NO) ||
              (pCcb->pairReq[SMP_IO_POS] == SMP_IO_KEY_DISP))) ||

            ((pCcb->pairRsp[SMP_IO_POS] == SMP_IO_DISP_YES_NO) &&
             (pCcb->pairReq[SMP_IO_POS] == SMP_IO_KEY_DISP)))
        {
          pCcb->pScCcb->authType = SMP_AUTH_TYPE_NUM_COMP;
        }
        else if ((pCcb->pairReq[SMP_AUTHREQ_POS] & SMP_AUTH_KP_FLAG) &&
                 (pCcb->pairRsp[SMP_AUTHREQ_POS] & SMP_AUTH_KP_FLAG))
        {
          pCcb->pScCcb->kpNotify = TRUE;
        }
      }
      else
      {
        /* Check for numeric comparison */
        if ((pCcb->pairRsp[SMP_IO_POS] == SMP_IO_DISP_YES_NO) &&
            (pCcb->pairReq[SMP_IO_POS] == SMP_IO_DISP_YES_NO))
        {
          pCcb->pScCcb->authType = SMP_AUTH_TYPE_NUM_COMP;

          /* set false to add in MITM below */
          justWorks = FALSE;
        }
      }
    }

    hdr.param = pCcb->connId;

    /* Allocate a scratch pad for Peer Key and CMAC calculations */
    if (SmpScAllocScratchBuffers(pCcb))
    {
      /* Store the current ECC key */
      memcpy(pCcb->pScCcb->pLocalPublicKey->pubKeyX, DmSecGetEccKey()->pubKey_x, SMP_PUB_KEY_LEN);
      memcpy(pCcb->pScCcb->pLocalPublicKey->pubKeyY, DmSecGetEccKey()->pubKey_y, SMP_PUB_KEY_LEN);
      memcpy(pCcb->pScCcb->pPrivateKey, DmSecGetEccKey()->privKey, SMP_PRIVATE_KEY_LEN);

      /* Send internal message indicating LESC was requested */
      hdr.event = SMP_MSG_INT_LESC;
    }
    else
    {
      hdr.status = SMP_ERR_UNSPECIFIED;
      hdr.event = SMP_MSG_API_CANCEL_REQ;
    }

    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
  }
  else if ((pSmpCfg->auth & SMP_AUTH_SC_FLAG) != 0)
  {
    /* cancel pairing if configuration requires LESC and we are falling back to legacy */
    hdr.param = pCcb->connId;
    hdr.status = SMP_ERR_AUTH_REQ;
    hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
    return FALSE;
  }
  else
  {
    /* in case we're falling back to legacy after being paired with SC */
    pCcb->pScCcb->lescEnabled = FALSE;

    /* Send internal message indicating legacy security was requested */
    hdr.param = pCcb->connId;
    hdr.event = SMP_MSG_INT_LEGACY;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
  }

  if (!justWorks)
  {
    /* set auth flags with mitm bit set */
    pCcb->auth = (pCcb->pairReq[SMP_AUTHREQ_POS] & pCcb->pairRsp[SMP_AUTHREQ_POS]) | SMP_AUTH_MITM_FLAG;
  }
  else
  {
    /* set auth flags with mitm bit cleared */
    pCcb->auth = pCcb->pairReq[SMP_AUTHREQ_POS] & pCcb->pairRsp[SMP_AUTHREQ_POS] & ~SMP_AUTH_MITM_FLAG;
  }

  /* if we ended up with 'just works' but the device configuration requires authentication */
  localAuth = (pCcb->initiator) ? pCcb->pairReq[SMP_AUTHREQ_POS] : pCcb->pairRsp[SMP_AUTHREQ_POS];
  if (justWorks && (pSmpCfg->auth & localAuth & SMP_AUTH_MITM_FLAG))
  {
    /* cancel pairing */
    hdr.param = pCcb->connId;
    hdr.status = SMP_ERR_AUTH_REQ;
    hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
    return FALSE;
  }

  /* if max encryption key is less than our minimum */
  if (pCcb->pairReq[SMP_MAXKEY_POS] < pSmpCfg->minKeyLen ||
      pCcb->pairRsp[SMP_MAXKEY_POS] < pSmpCfg->minKeyLen)
  {
    /* cancel pairing */
    hdr.param = pCcb->connId;
    hdr.status = SMP_ERR_ENC_KEY_SIZE;
    hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Request authentication data or send ourselves an authentication response.
 *
 *  \param  pCcb      Connection control block.
 *  \param  oob       Out-of-band data requested.
 *  \param  display   TRUE if pin is to be displayed.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScAuthReq(smpCcb_t *pCcb, uint8_t oob, uint8_t display)
{
  /* use a union to save a bit of memory on the stack */
  union
  {
    smpDmAuthRsp_t        authRsp;
    dmSecAuthReqIndEvt_t  authReq;
  } buf;

  /* if authenticated pairing */
  if (((!pCcb->pScCcb->lescEnabled) && (pCcb->auth & SMP_AUTH_MITM_FLAG )) ||
      ((pCcb->pScCcb->lescEnabled) && (pCcb->pScCcb->authType == SMP_AUTH_TYPE_OOB)))
  {
    /* request pin or oob from user */
    buf.authReq.hdr.param = pCcb->connId;
    buf.authReq.hdr.event = DM_SEC_AUTH_REQ_IND;
    buf.authReq.oob = oob;
    buf.authReq.display = display;
    DmSmpCbackExec((dmEvt_t *) &buf.authReq);
  }
  else
  {
    /* else use just works; send ourselves a auth rsp with all zero pin */
    buf.authRsp.hdr.param = pCcb->connId;
    buf.authRsp.hdr.event = SMP_MSG_API_AUTH_RSP;
    buf.authRsp.authData[0] = 0;
    buf.authRsp.authData[1] = 0;
    buf.authRsp.authData[2] = 0;
    buf.authRsp.authDataLen = SMP_PIN_LEN;
    smpSmExecute(pCcb, (smpMsg_t *) &buf.authRsp);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Cleanup after LESC secure pairing ends.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActCleanup(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  smpActCleanup(pCcb, pMsg);

  /* Free scratch buffers */
  SmpScFreeScratchBuffers(pCcb);
}

/*************************************************************************************************/
/*!
 *  \brief  Pairing Failed Action.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActPairingFailed(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Perform LESC Cleanup */
  smpScActCleanup(pCcb, pMsg);

  /* Process failure */
  smpActPairingFailed(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Pairing Cancel Action.
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActPairingCancel(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* send pairing failed packet */
  smpSendPairingFailed(pCcb, pMsg->hdr.status);

  smpScActPairingFailed(pCcb, pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Common actions upon selection of type of LESC pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActAuthSelect(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  wsfMsgHdr_t hdr;
  uint8_t *pPacket = pMsg->data.pPacket + L2C_PAYLOAD_START;
  uint16_t deltaCnt = 0;

  /* Process public key from peer (store in MSB first format) */
  WStrReverseCpy(pCcb->pScCcb->pPeerPublicKey->pubKeyX, &pPacket[SMP_PUB_KEY_X_POS], SMP_PUB_KEY_LEN);
  WStrReverseCpy(pCcb->pScCcb->pPeerPublicKey->pubKeyY, &pPacket[SMP_PUB_KEY_Y_POS], SMP_PUB_KEY_LEN);

  /* Verify the peer and local keys do not match and are also not the same except for one bit */
  for (uint8_t i = 0; i < SMP_PUB_KEY_LEN; i++)
  {
    uint8_t peerByteX = pCcb->pScCcb->pPeerPublicKey->pubKeyX[i];
    uint8_t peerByteY = pCcb->pScCcb->pPeerPublicKey->pubKeyY[i];
    uint8_t localByteX = pCcb->pScCcb->pLocalPublicKey->pubKeyX[i];
    uint8_t localByteY = pCcb->pScCcb->pLocalPublicKey->pubKeyY[i];

    for (uint8_t j = 0; j < 8; j++)
    {
      if ((peerByteX & (1 << j)) != (localByteX & (1 << j)))
      {
        deltaCnt++;
      }

      if ((peerByteY & (1 << j)) != (localByteY & (1 << j)))
      {
        deltaCnt++;
      }
    }

    if (deltaCnt >= SMP_PUBLIC_KEY_DELTA_REQUIRED)
    {
      break;
    }
  }
  
  /* Setup message */
  hdr.param = pCcb->connId;

  if (deltaCnt < SMP_PUBLIC_KEY_DELTA_REQUIRED)
  {
    /* Cancel pairing */
    hdr.status = SMP_ERR_UNSPECIFIED;
    hdr.event = SMP_MSG_API_CANCEL_REQ;
  }
  else
  {
    /* Select authorization mode (JW, OOB, or passkey) */
    switch(pCcb->pScCcb->authType)
    {
    case SMP_AUTH_TYPE_NUM_COMP:
    case SMP_AUTH_TYPE_JUST_WORKS:
      hdr.event = SMP_MSG_INT_JW_NC;
      break;
    case SMP_AUTH_TYPE_OOB:
      hdr.event = SMP_MSG_INT_OOB;
      break;
    case SMP_AUTH_TYPE_PASSKEY:
      hdr.event = SMP_MSG_INT_PASSKEY;
      break;
    default:
      hdr.status = SMP_ERR_UNSPECIFIED;
      hdr.event = SMP_MSG_API_CANCEL_REQ;
      break;
    }
  }

  /* Message state machine */
  smpSmExecute(pCcb, (smpMsg_t *) &hdr);
}

/*************************************************************************************************/
/*!
 *  \brief  Setup for passkey pairing
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActPkSetup(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* use a union to save a bit of memory on the stack */
  union
  {
    smpDmAuthRsp_t        authRsp;
    dmSecAuthReqIndEvt_t  authReq;
  } buf;

  pCcb->pScCcb->pkPos = 0;

  /* Next command is the Pair Confirm */
  pCcb->nextCmdCode = SMP_CMD_PAIR_CNF;

  /* request pin or oob from user */
  buf.authReq.hdr.param = pCcb->connId;
  buf.authReq.hdr.event = DM_SEC_AUTH_REQ_IND;
  buf.authReq.oob = FALSE;
  buf.authReq.display = pCcb->pScCcb->display;
  DmSmpCbackExec((dmEvt_t *)&buf.authReq);
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the confirm value for Just Works pairing using Toolkit function F4
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActJwncCalcF4(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Cb = f4(PKbx, PKax, Nb, 0x00) where f4(U, V, x, Z) = AES-CMACx (U || V || Z) */
  if (pCcb->initiator)
  {
    SMP_TRACE_256("F4 PKb", pCcb->pScCcb->pPeerPublicKey->pubKeyX);
    SMP_TRACE_256("F4 PKa", pCcb->pScCcb->pLocalPublicKey->pubKeyX);
    SMP_TRACE_128("F4 Nb", pCcb->pScCcb->pScratch->Nb_Eb);

    SmpScCalcF4(pCcb, pMsg,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                pCcb->pScCcb->pLocalPublicKey->pubKeyX,
                0, pCcb->pScCcb->pScratch->Nb_Eb);
  }
  else
  {
    SMP_TRACE_256("F4 PKb",  pCcb->pScCcb->pLocalPublicKey->pubKeyX);
    SMP_TRACE_256("F4 PKa", pCcb->pScCcb->pPeerPublicKey->pubKeyX);
    SMP_TRACE_128("F4 Nb", pCcb->pScCcb->pScratch->Nb_Eb);

    SmpScCalcF4(pCcb, pMsg,
                pCcb->pScCcb->pLocalPublicKey->pubKeyX,
                pCcb->pScCcb->pPeerPublicKey->pubKeyX,
                0, pCcb->pScCcb->pScratch->Nb_Eb);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate Just Works verify value using Toolkit function G2
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActJwncCalcG2(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCmacText;

  /* g2(PKax, PKbx, Na, Nb) where g2(U, V, X, Z) = AES-CMACx (U || V || Z) mod 2^32 */
  if ((pCmacText = SmpScAlloc(SMP_G2_TEXT_LEN, pCcb, pMsg)) !=  NULL)
  {
    uint8_t *pCatBuf = pCmacText;

    /* Concatinate PKax, PKbx, Nb */
    if (pCcb->initiator)
    {
      pCatBuf = SmpScCat(pCatBuf, pCcb->pScCcb->pLocalPublicKey->pubKeyX, SMP_PUB_KEY_LEN);
      pCatBuf = SmpScCat(pCatBuf, pCcb->pScCcb->pPeerPublicKey->pubKeyX, SMP_PUB_KEY_LEN);
    }
    else
    {
      pCatBuf = SmpScCat(pCatBuf, pCcb->pScCcb->pPeerPublicKey->pubKeyX, SMP_PUB_KEY_LEN);
      pCatBuf = SmpScCat(pCatBuf, pCcb->pScCcb->pLocalPublicKey->pubKeyX, SMP_PUB_KEY_LEN);
    }

    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Nb_Eb);

    /* Execute CMAC with Na as the key */
    SmpScCmac(pCcb->pScCcb->pScratch->Na_Ea, pCmacText, SMP_G2_TEXT_LEN, pCcb, pMsg);
  }
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
void smpScActJwncDisplay(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  if (pCcb->pScCcb->authType == SMP_AUTH_TYPE_NUM_COMP)
  {
    dmSecCnfIndEvt_t cnfEvt;

    SMP_TRACE_128("JWNC Display Cnf", pMsg->aes.pCiphertext);

    /* Send the confirm value to the application for display to the user */
    Calc128Cpy(cnfEvt.confirm, pMsg->aes.pCiphertext);
    cnfEvt.hdr.param = pCcb->connId;
    cnfEvt.hdr.event = DM_SEC_COMPARE_IND;
    cnfEvt.hdr.status = HCI_SUCCESS;

    DmSmpCbackExec((dmEvt_t *) &cnfEvt);
  }
  else
  {
    /* Just Works always reports success */
    wsfMsgHdr_t hdr;

    hdr.param = pCcb->connId;
    hdr.event = SMP_MSG_API_USER_CONFIRM;
    hdr.status = HCI_SUCCESS;
    smpSmExecute(pCcb, (smpMsg_t *) &hdr);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Notify the application of a received keypress message
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActPkKeypress(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCmd = pMsg->data.pPacket + L2C_PAYLOAD_START;
  uint8_t *pPacket = pMsg->data.pPacket + L2C_PAYLOAD_START + SMP_HDR_LEN;

  if (*pCmd == SMP_CMD_KEYPRESS)
  {
    dmSecKeypressIndEvt_t keypressEvt;

    /* Notify the application a keypress message was received from the responder */
    keypressEvt.notificationType = *pPacket;
    keypressEvt.hdr.param = pCcb->connId;
    keypressEvt.hdr.event = DM_SEC_KEYPRESS_IND;
    keypressEvt.hdr.status = HCI_SUCCESS;

    DmSmpCbackExec((dmEvt_t *) &keypressEvt);
  }
  else if (*pCmd == SMP_CMD_PAIR_CNF)
  {
    /* Handle the case where a confirm is received before the user enters the passkey */
    pMsg->hdr.event = SMP_MSG_EARLY_CNF;
    pMsg->hdr.status = HCI_SUCCESS;
    smpSmExecute(pCcb, pMsg);
  }
  else
  {
    /* Unexpected command */
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Send a keypress command to the responder
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActPkSendKeypress(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  /* Send keypress notification to the peer */
  smpDmKeypress_t *pKeypress = (smpDmKeypress_t *) pMsg;
  uint8_t         *pPkt;
  uint8_t         *p;

  if (pCcb->pScCcb->kpNotify)
  {
    /* start smp response timer */
    smpStartRspTimer(pCcb);

    /* allocate packet buffer */
    if ((pPkt = smpMsgAlloc(SMP_KEYPRESS_MSG_LEN + L2C_PAYLOAD_START)) != NULL)
    {
      /* build packet */
      p = pPkt + L2C_PAYLOAD_START;
      UINT8_TO_BSTREAM(p, SMP_CMD_KEYPRESS);

      /* Store keypress notification type */
      *p = pKeypress->keypress;

      /* send packet */
      smpSendPkt(pCcb, pPkt);
    }
    else
    {
      pMsg->hdr.status = SMP_ERR_UNSPECIFIED;
      pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
      smpSmExecute(pCcb, pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the DHKey, alsop called shared secret, for Cryptographic Function 5
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActCalcSharedSecret(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  secEccKey_t key;

#if SMP_DHKEY_QUAL_INCLUDED
  if (SMP_DhKeyZero)
  {
    secEccMsg_t *pEccMsg = WsfMsgAlloc(sizeof(secEccMsg_t));

    if (pEccMsg)
    {
      /* Send all zero shared secret to handler */
      memset(pEccMsg->data.sharedSecret.secret, 0, SEC_ECC_KEY_LEN);

      pEccMsg->hdr.event = SMP_MSG_WSF_ECC_CMPL;
      pEccMsg->hdr.param = pCcb->connId;
      pEccMsg->hdr.status = HCI_SUCCESS;

      WsfMsgSend(smpCb.handlerId, pEccMsg);

      return;
    }
  }
#endif

  /* Use local private key and peer public key to generate the shared secret */
  memcpy(key.privKey,  pCcb->pScCcb->pPrivateKey, SMP_PRIVATE_KEY_LEN);
  memcpy(key.pubKey_x, pCcb->pScCcb->pPeerPublicKey->pubKeyX, SMP_PUB_KEY_LEN);
  memcpy(key.pubKey_y, pCcb->pScCcb->pPeerPublicKey->pubKeyY, SMP_PUB_KEY_LEN);

  /* Generate the shared secret */
  if (SecEccGenSharedSecret(&key, smpCb.handlerId, pCcb->connId, SMP_MSG_WSF_ECC_CMPL) == FALSE)
  {
    pMsg->hdr.status = SMP_ERR_MEMORY;
    pMsg->hdr.event = SMP_MSG_API_CANCEL_REQ;
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the Key, T, for Cryptographic Function F5
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActCalcF5TKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  secEccMsg_t *pSecretMsg = (secEccMsg_t*) pMsg;
  uint8_t *pCmacText;

  if (pMsg->hdr.status == HCI_SUCCESS)
  {
    /* The shared secret is the DH Key */
    SMP_TRACE_256("DHKey (Shared Secret)", pSecretMsg->data.sharedSecret.secret);

    if ((pCmacText = SmpScAlloc(SMP_F5_TKEY_TEXT_LEN, pCcb, pMsg)) !=  NULL)
    {
      const uint8_t salt[] = {0x6C, 0x88, 0x83, 0x91, 0xAA, 0xF5, 0xA5, 0x38,
                              0x60, 0x37, 0x0B, 0xDB, 0x5A, 0x60, 0x83, 0xBE};

      /* T = AES-CMACsalt (DHKey) */
      memcpy(pCmacText, pSecretMsg->data.sharedSecret.secret, SMP_DHKEY_LEN);

      /* Execute CMAC with salt as the key */
      SmpScCmac(salt, pCmacText, SMP_F5_TKEY_TEXT_LEN, pCcb, pMsg);
    }
  }
  else
  {
    SmpScGetCancelMsgWithReattempt(pCcb->connId, &pMsg->hdr, SMP_ERR_DH_KEY_CHECK);
    smpSmExecute(pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the MacKey from Cryptographic Function F5
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActCalcF5MacKey(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  secCmacMsg_t *pCmac = (secCmacMsg_t *) pMsg;
  uint8_t *pCmacText;

  /* Copy T from the previous step in the F5 Calculation */
  Calc128Cpy(pCcb->pScCcb->pLtk->ltk_t, pCmac->pCiphertext);

  SMP_TRACE_128("T", pCcb->pScCcb->pLtk->ltk_t);

  if ((pCmacText = SmpScAlloc(SMP_F5_TEXT_LEN, pCcb, pMsg)) !=  NULL)
  {
    uint8_t *pCatBuf = pCmacText;

    /* mackey || ltk = f5(W, N1, N2, A1, A2) */
    /* mackey = AES-CMACT (Counter = 0 || keyID ||N1 || N2|| A1|| A2|| Length = 256) */
    *pCatBuf++ = 0;
    pCatBuf = SmpScCat(pCatBuf, smpScF5Key, 4);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Na_Ea);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Nb_Eb);
    pCatBuf = smpScCatInitiatorBdAddr(pCcb, pCatBuf);
    pCatBuf = smpScCatResponderBdAddr(pCcb, pCatBuf);
    *pCatBuf++ = 1;
    *pCatBuf++ = 0;

    /* Execute CMAC with T as the key */
    SmpScCmac(pCcb->pScCcb->pLtk->ltk_t, pCmacText, SMP_F5_TEXT_LEN, pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the LTK from Cryptographic Function F5
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActCalcF5Ltk(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  secCmacMsg_t *pCmac = (secCmacMsg_t *) pMsg;
  uint8_t *pCmacText;

  SMP_TRACE_128("MAC", pMsg->aes.pCiphertext);

  /* Copy Mac Key from the previous step in the F5 Calculation */
  Calc128Cpy(pCcb->pScCcb->pLtk->mac, pCmac->pCiphertext);

  if ((pCmacText = SmpScAlloc(SMP_F5_TEXT_LEN, pCcb, pMsg)) !=  NULL)
  {
    uint8_t *pCatBuf = pCmacText;

    /* mackey || ltk = f5(W, N1, N2, A1, A2) */
    /* ltk = AES-CMACT (Counter = 1 || keyID || N1 || N2|| A1|| A2 ||Length = 256) */
    *pCatBuf++ = 1;
    pCatBuf = SmpScCat(pCatBuf, smpScF5Key, 4);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Na_Ea);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Nb_Eb);
    pCatBuf = smpScCatInitiatorBdAddr(pCcb, pCatBuf);
    pCatBuf = smpScCatResponderBdAddr(pCcb, pCatBuf);
    *pCatBuf++ = 1;
    *pCatBuf++ = 0;

    /* Execute CMAC with T as the key */
    SmpScCmac(pCcb->pScCcb->pLtk->ltk_t, pCmacText, SMP_F5_TEXT_LEN, pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the DH Key Check Ea with toolkit function F6
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActDHKeyCalcF6Ea(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCmacText;

  SMP_TRACE_128("LTK", pMsg->aes.pCiphertext);

  /* Copy LTK from the previous state (smpScActCalcF5Ltk) */
  WStrReverseCpy(pCcb->pScCcb->pLtk->ltk_t, pMsg->aes.pCiphertext, 16);

  if ((pCmacText = SmpScAlloc(SMP_F6_TEXT_LEN, pCcb, pMsg)) !=  NULL)
  {
    uint8_t *pCatBuf = pCmacText;

    /* f6(W, N1, N2, R, IOcap, A1, A2) = AES-CMACW (N1 || N2 || R || IOcap || A1 || A2) */
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Na_Ea);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Nb_Eb);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Rb);

    *pCatBuf++ = pCcb->pairReq[SMP_AUTHREQ_POS]; /* AuthReq */
    *pCatBuf++ = pCcb->pairReq[SMP_OOB_POS]; /* OOB Data Flag */
    *pCatBuf++ = pCcb->pairReq[SMP_IO_POS]; /* IO Capability Parameter */

    pCatBuf = smpScCatInitiatorBdAddr(pCcb, pCatBuf);
    pCatBuf = smpScCatResponderBdAddr(pCcb, pCatBuf);

    /* Execute CMAC with MACKey as the key */
    SmpScCmac(pCcb->pScCcb->pLtk->mac, pCmacText, SMP_F6_TEXT_LEN, pCcb, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Calculate the DH Key Check Eb with toolkit function F6
 *
 *  \param  pCcb  Connection control block.
 *  \param  pMsg  State machine message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void smpScActDHKeyCalcF6Eb(smpCcb_t *pCcb, smpMsg_t *pMsg)
{
  uint8_t *pCmacText;

  SMP_TRACE_128("DHKey Ea", pMsg->aes.pCiphertext);

  if ((pCmacText = SmpScAlloc(SMP_F6_TEXT_LEN, pCcb, pMsg)) !=  NULL)
  {
    uint8_t *pCatBuf = pCmacText;

    /* f6(W, N1, N2, R, IOcap, A1, A2) = AES-CMACW (N1 || N2 || R || IOcap || A1 || A2) */
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Nb_Eb);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Na_Ea);
    pCatBuf = SmpScCat128(pCatBuf, pCcb->pScCcb->pScratch->Ra);

    *pCatBuf++ = pCcb->pairRsp[SMP_AUTHREQ_POS]; /* AuthReq */
    *pCatBuf++ = pCcb->pairRsp[SMP_OOB_POS]; /* OOB Data Flag */
    *pCatBuf++ = pCcb->pairRsp[SMP_IO_POS]; /* IO Capability Parameter */

    pCatBuf = smpScCatResponderBdAddr(pCcb, pCatBuf);
    pCatBuf = smpScCatInitiatorBdAddr(pCcb, pCatBuf);

    /* Execute CMAC with MACKey as the key */
    SmpScCmac(pCcb->pScCcb->pLtk->mac, pCmacText, SMP_F6_TEXT_LEN, pCcb, pMsg);
  }

  /* Copy Ea from the previous state (smpScActDHKeyCalcF6Ea) */
  Calc128Cpy(pCcb->pScCcb->pScratch->Na_Ea, pMsg->aes.pCiphertext);
}

