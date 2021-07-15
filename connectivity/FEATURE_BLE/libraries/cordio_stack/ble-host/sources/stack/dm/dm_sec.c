/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  DM security module.
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
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "sec_api.h"
#include "wsf_buf.h"
#include "wsf_trace.h"
#include "util/calc128.h"
#include "smp_api.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_conn.h"
#include "dm_sec.h"

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
dmSecCb_t dmSecCb;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Component function interface */
static const dmFcnIf_t dmSecFcnIf =
{
  dmSecReset,
  dmSecHciHandler,
  (dmMsgHandler_t) dmSecMsgHandler
};

/*************************************************************************************************/
/*!
 *  \brief  Utility function for API encrypt request or ltk response.
 *
 *  \param  connId    DM connection ID.
 *  \param  status    HCI_SUCCESS if LTK available.
 *  \param  *pLTK     Pointer to LTK parameter structure.
 *  \param  event     DM handler event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSecApiLtkMsg(dmConnId_t connId, uint8_t status, dmSecLtk_t *pLtk, uint8_t event)
{
  dmSecApiEncryptReq_t  *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(dmSecApiEncryptReq_t))) != NULL)
  {
    pMsg->hdr.event = event;
    pMsg->hdr.param = connId;
    pMsg->hdr.status = status;
    memcpy(&pMsg->ltk, pLtk, sizeof(dmSecLtk_t));

    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM dev HCI event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSecHciHandler(hciEvt_t *pEvent)
{
  dmConnCcb_t           *pCcb;
  uint8_t               *pKey;
  dmSecEncryptIndEvt_t  encryptInd;
  uint8_t               secLevel;

  if ((pCcb = dmConnCcbByHandle(pEvent->hdr.param)) != NULL)
  {
    if (pEvent->hdr.event == HCI_LE_LTK_REQ_CBACK_EVT)
    {
      /* if ediv and rand are zero then check if STK is available from SMP */
      if ((pEvent->leLtkReq.encDiversifier == 0) &&
          (memcmp(pEvent->leLtkReq.randNum, calc128Zeros, HCI_RAND_LEN) == 0))
      {
        if ((pKey = SmpDmGetStk(pCcb->connId, &secLevel)) != NULL)
        {
          /* store security level */
          pCcb->tmpSecLevel = secLevel;

          /* not using LTK */
          pCcb->usingLtk = FALSE;

          /* provide key to HCI */
          HciLeLtkReqReplCmd(pEvent->hdr.param, pKey);
          return;
        }
      }
      else if (SmpDmLescEnabled(pCcb->connId) == TRUE)
      {
        /* EDIV and Rand must be zero in LE Secure Connections */
        HciLeLtkReqNegReplCmd(pEvent->hdr.param);
        return;
      }

      /* call callback to get key from app */

      /* set connection busy */
      DmConnSetIdle(pCcb->connId, DM_IDLE_DM_ENC, DM_CONN_BUSY);

      /* using LTK */
      pCcb->usingLtk = TRUE;

      /* use the header from the encryptInd struct for efficiency */
      pEvent->hdr.param = pCcb->connId;
      pEvent->hdr.event = DM_SEC_LTK_REQ_IND;
      (*dmCb.cback)((dmEvt_t *) pEvent);
    }
    else if (pEvent->hdr.event == HCI_ENC_KEY_REFRESH_CMPL_CBACK_EVT ||
             pEvent->hdr.event == HCI_ENC_CHANGE_CBACK_EVT)
    {
      /* set connection idle */
      DmConnSetIdle(pCcb->connId, DM_IDLE_DM_ENC, DM_CONN_IDLE);

      encryptInd.hdr.param = pCcb->connId;
      encryptInd.hdr.status = pEvent->hdr.status;
      if (encryptInd.hdr.status == HCI_SUCCESS)
      {
        encryptInd.hdr.event = DM_SEC_ENCRYPT_IND;

        /* update security level of connection */
        pCcb->secLevel = pCcb->tmpSecLevel;

        /* set LTK flag */
        encryptInd.usingLtk = pCcb->usingLtk;
      }
      else
      {
        encryptInd.hdr.event = DM_SEC_ENCRYPT_FAIL_IND;
      }

      /* call callback before passing to SMP */
      DmSmpCbackExec((dmEvt_t *) &encryptInd);

      /* pass to SMP */
      encryptInd.hdr.param = pCcb->connId;
      encryptInd.hdr.status = pEvent->hdr.status;
      SmpDmEncryptInd((wsfMsgHdr_t *) &encryptInd);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM dev event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSecMsgHandler(dmSecMsg_t *pMsg)
{
  dmConnCcb_t *pCcb;

  /* look up ccb */
  if ((pCcb = dmConnCcbById((dmConnId_t) pMsg->hdr.param)) != NULL)
  {
    /* process API encrypt req */
    switch (pMsg->hdr.event)
    {
    case DM_SEC_MSG_API_ENCRYPT_REQ:
      /* set connection busy */
      DmConnSetIdle(pCcb->connId, DM_IDLE_DM_ENC, DM_CONN_BUSY);

      /* store security level */
      pCcb->tmpSecLevel = pMsg->encryptReq.secLevel;

      /* using LTK */
      pCcb->usingLtk = TRUE;

      /* start encryption */
      HciLeStartEncryptionCmd(pCcb->handle, pMsg->encryptReq.ltk.rand,
                              pMsg->encryptReq.ltk.ediv, pMsg->encryptReq.ltk.key);

      break;

    case DM_SEC_MSG_API_LTK_RSP:
      /* if key found */
      if (pMsg->ltkRsp.keyFound)
      {
        /* store security level */
        pCcb->tmpSecLevel = pMsg->ltkRsp.secLevel;

        /* provide key to HCI */
        HciLeLtkReqReplCmd(pCcb->handle, pMsg->ltkRsp.key);
      }
      else
      {
        /* key not found; set connection idle */
        DmConnSetIdle(pCcb->connId, DM_IDLE_DM_ENC, DM_CONN_IDLE);

        HciLeLtkReqNegReplCmd(pCcb->handle);
      }
      break;

    default:
      break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  Execute DM callback from SMP procedures.
 *
 *  \param  pDmEvt    Pointer to callback event data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSmpCbackExec(dmEvt_t *pDmEvt)
{
  /* certain messages need to get to ATT */
  if (pDmEvt->hdr.event == DM_SEC_PAIR_CMPL_IND ||
      pDmEvt->hdr.event == DM_SEC_ENCRYPT_IND)
  {
    if (dmConnCb.connCback[DM_CLIENT_ID_ATT] != NULL)
    {
      (*dmConnCb.connCback[DM_CLIENT_ID_ATT])(pDmEvt);
    }
  }

  /* execute DM client callback */
  (*dmCb.cback)(pDmEvt);
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called to cancel the pairing process.
 *
 *  \param  connId    DM connection ID.
 *  \param  reason    Failure reason.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSecCancelReq(dmConnId_t connId, uint8_t reason)
{
  wsfMsgHdr_t  *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(wsfMsgHdr_t))) != NULL)
  {
    pMsg->event = SMP_MSG_API_CANCEL_REQ;
    pMsg->param = connId;
    pMsg->status = reason;

    /* note we're sending this to SMP */
    SmpDmMsgSend((smpDmMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called in response to a DM_SEC_AUTH_REQ_IND event to provide
 *          PIN or OOB data during pairing.
 *
 *  \param  connId      DM connection ID.
 *  \param  authDataLen Length of PIN or OOB data.
 *  \param  pAuthData   pointer to PIN or OOB data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSecAuthRsp(dmConnId_t connId, uint8_t authDataLen, uint8_t *pAuthData)
{
  smpDmAuthRsp_t   *pMsg;

  WSF_ASSERT(authDataLen <= SMP_OOB_LEN);

  if ((pMsg = WsfMsgAlloc(sizeof(smpDmAuthRsp_t))) != NULL)
  {
    pMsg->hdr.event = SMP_MSG_API_AUTH_RSP;
    pMsg->hdr.param = connId;
    pMsg->authDataLen = authDataLen;

    if (pAuthData != NULL)
    {
      memcpy(pMsg->authData, pAuthData, authDataLen);
    }

    /* note we're sending this to SMP */
    SmpDmMsgSend((smpDmMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM security.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSecInit(void)
{
  dmFcnIfTbl[DM_ID_SEC] = (dmFcnIf_t *) &dmSecFcnIf;

  dmSecCb.pCsrk = dmSecCb.pIrk = (uint8_t *) calc128Zeros;
  dmSecCb.addrType = DM_ADDR_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  This function sets the local CSRK used by the device.
 *
 *  \param  pCsrk     Pointer to CSRK.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSecSetLocalCsrk(uint8_t *pCsrk)
{
  WsfTaskLock();
  dmSecCb.pCsrk = pCsrk;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  This function sets the local IRK used by the device.
 *
 *  \param  pCsrk     Pointer to IRK.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSecSetLocalIrk(uint8_t *pIrk)
{
  WsfTaskLock();
  dmSecCb.pIrk = pIrk;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  This function sets the local identity address used by the device.
 *
 *  \param  identityAddr  Local identity address.
 *  \param  addrType      Local identity address type.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmSecSetLocalIdentityAddr(const bdAddr_t identityAddr, uint8_t addrType)
{
  WsfTaskLock();
  dmSecCb.addrType = addrType;
  BdaCpy(dmSecCb.identityAddr, identityAddr);
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  This function gets the local CSRK used by the device.
 *
 *  \return Pointer to CSRK.
 */
/*************************************************************************************************/
uint8_t *DmSecGetLocalCsrk(void)
{
  return dmSecCb.pCsrk;
}

/*************************************************************************************************/
/*!
 *  \brief  This function gets the local IRK used by the device.
 *
 *  \return Pointer to IRK.
 */
/*************************************************************************************************/
uint8_t *DmSecGetLocalIrk(void)
{
  return dmSecCb.pIrk;
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  This function gets the local identity address used by the device.
 *
 *  \return Pointer to the identity address.
 */
/*************************************************************************************************/
uint8_t *DmSecGetLocalIdentityAddr(void)
{
  /* if the local identity address has been set */
  if (dmSecCb.addrType != DM_ADDR_NONE)
  {
    return dmSecCb.identityAddr;
  }

  return HciGetBdAddr();
}

/*************************************************************************************************/
/*!
 *  \brief  For internal use only.  This function gets the local identity address type used by the
 *          device.
 *
 *  \return The identity address type.
 */
/*************************************************************************************************/
uint8_t DmSecGetLocalIdentityAddrType(void)
{
  /* if the local identity address has been set */
  if (dmSecCb.addrType != DM_ADDR_NONE)
  {
    return dmSecCb.addrType;
  }

  return DM_ADDR_PUBLIC;
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the sec module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmSecReset(void)
{
  /* initialize smp database */
  SmpDbInit();
}
