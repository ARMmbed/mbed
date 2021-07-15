/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) Host Controller Interface (HCI) initialization implementation file.
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

#include "lhci_int.h"
#include "chci_api.h"
#include "hci_defs.h"
#include "ll_api.h"
#include "bb_api.h"
#include "pal_frc.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "util/bstream.h"
#include <string.h>

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief      Dispatcher table. */
lhciCmdHandler_t lhciCmdTbl[LHCI_MSG_TOTAL];

/*! \brief      Dispatcher table. */
lhciEvtHandler_t lhciEvtTbl[LHCI_MSG_TOTAL];

/*! \brief      Receive pending handler. */
lhciServiceAcl_t lhciServiceAcl;

/*! \brief      Receive pending handler. */
lhciServiceIso_t lhciServiceIso;

/*! \brief      Persistent control block */
lhciPersistCb_t lhciPersistCb;

/*! \brief      LL HCI subsystem control block. */
lhciCb_t lhciCb;

/*! \brief      Handler duration watermark in microseconds. */
uint16_t lhciHandlerWatermarkUsec = 0;

/*************************************************************************************************/
/*!
 *  \brief  Unpack an advertising channel PDU header.
 *
 *  \param  pHdr        Unpacked header.
 *  \param  pBuf        Packed packet buffer.
 *
 *  \return Header length.
 */
/*************************************************************************************************/
static inline uint8_t lhciUnpackHdr(LhciHdr_t *pHdr, const uint8_t *pBuf)
{
  const uint8_t len = sizeof(uint16_t) + sizeof(uint8_t);

  BSTREAM_TO_UINT16(pHdr->opCode, pBuf);
  pHdr->len = *pBuf;

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief      LL HCI message dispatch handler.
 *
 *  \param      event       WSF event.
 *  \param      pMsg        WSF message.
 */
/*************************************************************************************************/
void LhciHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  bool_t startTimeValid;
  uint32_t startTime;
  uint32_t endTime;

  startTimeValid = PalFrcGetCurrentTime();
  if (startTimeValid)
  {
    startTime = PalBbGetCurrentTime();
  }

  if (event & LHCI_EVT_ACL_RCVD)
  {
    uint8_t *pAclBuf;
    wsfHandlerId_t handlerId;

    while ((pAclBuf = WsfMsgDeq(&lhciPersistCb.aclQ, &handlerId)) != NULL)
    {
      LlSendAclData(pAclBuf);
    }
  }

  if (event & LHCI_EVT_CMD_RCVD)
  {
    uint8_t *pCmdBuf;
    wsfHandlerId_t handlerId;
    LhciHdr_t hdr;

    while ((pCmdBuf = WsfMsgDeq(&lhciPersistCb.cmdQ, &handlerId)) != NULL)
    {
      uint8_t *pPldBuf = pCmdBuf + lhciUnpackHdr(&hdr, pCmdBuf);

      if (!lhciCommonDecodeCmdPkt(&hdr, pPldBuf))
      {
        unsigned int msg = 0;
        do
        {
          if (lhciCmdTbl[msg] && lhciCmdTbl[msg](&hdr, pPldBuf))
          {
            break;
          }
        } while (++msg < LHCI_MSG_TOTAL);

        /* Unhandled command. */
        if (msg == LHCI_MSG_TOTAL)
        {
          uint8_t *pEvtBuf;

          /* Send command status with unknown command status. */
          if ((pEvtBuf = lhciAllocEvt(HCI_CMD_STATUS_EVT, HCI_LEN_CMD_STATUS)) != NULL)
          {
            lhciPackCmdStatusEvt(pEvtBuf, HCI_ERR_UNKNOWN_CMD, hdr.opCode);
            lhciSendEvt(pEvtBuf);
          }
        }
      }

      WsfMsgFree(pCmdBuf);
    }
  }

  if (event & LHCI_EVT_SEND_CMPL)
  {
    lhciPersistCb.evtTrPending = FALSE;
    ChciTrNeedsService(CHCI_TR_PROT_BLE);

    if (lhciPersistCb.evtCompCback)
    {
      lhciPersistCb.evtCompCback();
    }
  }

  if (event & LHCI_EVT_HW_ERR)
  {
    uint8_t *pEvtBuf;

    if ((pEvtBuf = lhciAllocEvt(HCI_HW_ERROR_EVT, HCI_LEN_HW_ERR)) != NULL)
    {
      uint8_t *pBuf = (uint8_t *)pEvtBuf;
      UINT8_TO_BSTREAM(pBuf, lhciCb.hwErrorCode);

      lhciSendEvt(pEvtBuf);
    }
  }

  if (startTimeValid &&
      PalFrcGetCurrentTime())
  {
    endTime = PalBbGetCurrentTime();
    uint32_t durUsec = PalFrcDeltaUs(endTime, startTime);
    if (lhciHandlerWatermarkUsec < durUsec)
    {
      lhciHandlerWatermarkUsec = durUsec;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Queue a received buffer for processing by the message handler.
 *
 *  \param  type        Type of message.
 *  \param  pBuf        Pointer to received message.
 */
/*************************************************************************************************/
void lhciRecv(uint8_t type, uint8_t *pBuf)
{
  switch (type)
  {
    case CHCI_TR_TYPE_ISO:
      WsfMsgEnq(&lhciPersistCb.isoQ, 0, pBuf);
      WsfSetEvent(lhciPersistCb.handlerId, LHCI_EVT_ISO_RCVD);
      break;

    case CHCI_TR_TYPE_ACL:
      WsfMsgEnq(&lhciPersistCb.aclQ, 0, pBuf);
      WsfSetEvent(lhciPersistCb.handlerId, LHCI_EVT_ACL_RCVD);
      break;

    case CHCI_TR_TYPE_CMD:
      WsfMsgEnq(&lhciPersistCb.cmdQ, 0, pBuf);
      WsfSetEvent(lhciPersistCb.handlerId, LHCI_EVT_CMD_RCVD);
      break;

    default:
      WsfMsgFree(pBuf);
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Signal transport send completion.
 *
 *  \param  type    Type of message.
 *  \param  pBuf    Pointer to transmitted message.
 */
/*************************************************************************************************/
void lhciSendComplete(uint8_t type, uint8_t *pBuf)
{
  switch (type)
  {
    case CHCI_TR_TYPE_EVT:
      if (pBuf[0] == HCI_LE_META_EVT)
      {
        switch (pBuf[2])
        {
          case HCI_LE_ADV_REPORT_EVT:
          case HCI_LE_DIRECT_ADV_REPORT_EVT:
          case HCI_LE_EXT_ADV_REPORT_EVT:
          case HCI_LE_PER_ADV_REPORT_EVT:
          case HCI_LE_BIG_INFO_ADV_REPORT_EVT:
            lhciCb.numAdvReport--;
            break;
          case HCI_LE_SCAN_REQ_RCVD_EVT:
            lhciCb.numScanReqRcvd--;
            break;
          default:
            break;
        }
      }
      WsfMsgFree(pBuf);
      break;

    case CHCI_TR_TYPE_ACL:
      WsfMsgFree(pBuf);
      LlRecvAclDataComplete(1);
      break;

#if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
    /* TODO: Use function pointer to allow run time configuration of supported versions. */
    case CHCI_TR_TYPE_ISO:
      WsfMsgFree(pBuf);
      LlRecvIsoDataComplete(1);
      break;
#endif

    default:
      break;
  }

  WsfSetEvent(lhciPersistCb.handlerId, LHCI_EVT_SEND_CMPL);
}

/*************************************************************************************************/
/*!
 *  \brief  Service LHCI for transport.
 *
 *  \param  pType   Storage for type of message.
 *  \param  pLen    Storage for length of message.
 *  \param  pBuf    Storage for pointer to transmitted message.
 *
 *  \return TRUE if message ready.
 */
/*************************************************************************************************/
bool_t lhciService(uint8_t *pType, uint16_t *pLen, uint8_t **pBuf)
{
  uint8_t  *pBufTemp;
  uint16_t len;

  if (!lhciPersistCb.evtTrPending)
  {
    wsfHandlerId_t handlerId;

    if ((pBufTemp = WsfMsgDeq(&lhciPersistCb.evtQ, &handlerId)) != NULL)
    {
      len = pBufTemp[1] + HCI_EVT_HDR_LEN;

      lhciPersistCb.evtTrPending = TRUE;
      *pType = CHCI_TR_TYPE_EVT;
      *pLen  = len;
      *pBuf  = pBufTemp;
      return TRUE;
    }
    else
    {
      if (lhciServiceIso != NULL)
      {
        /* Additionally check if ISO data needs servicing. */
        if ((pBufTemp = lhciServiceIso()) != NULL)
        {
          BYTES_TO_UINT16(len, &pBufTemp[2]);
          len += HCI_ISO_HDR_LEN;

          lhciPersistCb.evtTrPending = TRUE;
          *pType = CHCI_TR_TYPE_ISO;
          *pLen  = len;
          *pBuf  = pBufTemp;
          return TRUE;
        }
      }

      if (lhciServiceAcl != NULL)
      {
        /* Additionally check if ACL data needs servicing. */
        if ((pBufTemp = lhciServiceAcl()) != NULL)
        {
          BYTES_TO_UINT16(len, &pBufTemp[2]);
          len += HCI_ACL_HDR_LEN;

          lhciPersistCb.evtTrPending = TRUE;
          *pType = CHCI_TR_TYPE_ACL;
          *pLen  = len;
          *pBuf  = pBufTemp;
          return TRUE;
        }
      }
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Signal transport hardware error.
 *
 *  \param  code        Error code.
 */
/*************************************************************************************************/
void lhciSendHwError(uint8_t code)
{
  /* Only send once. Host is expected to reset system after first error code. */
  if (lhciCb.hwErrorCode == 0)
  {
    lhciCb.hwErrorCode = code;
    WsfSetEvent(lhciPersistCb.handlerId, LHCI_EVT_HW_ERR);
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Reset HCI state.
 *
 *  This function is called at the end of a HCI reset.
 */
/*************************************************************************************************/
void lhciReset(void)
{
  memset(&lhciCb, 0, sizeof(lhciCb));

  lhciCb.evtMsk = LHCI_DEF_EVT_MASK;
  lhciCb.evtMskPg2 = LHCI_DEF_EVT_PG2_MASK;
  lhciCb.leEvtMsk = LHCI_DEF_LE_EVT_MASK;
}

/*************************************************************************************************/
/*!
 *  \brief      Set default Hci supported cmds.
 *
 *  \param      pBuf   Buffer to set supported commands.
 *
 *  This function is called at reset by lmgr.
 */
/*************************************************************************************************/
static void LhciSetDefaultHciSupCmd(uint8_t *pBuf)
{
  pBuf[5]  = HCI_SUP_SET_EVENT_MASK |                           /* mandatory */
             HCI_SUP_RESET;                                     /* mandatory */
  pBuf[14] = HCI_SUP_READ_LOCAL_VER_INFO |                      /* mandatory */
             HCI_SUP_READ_LOCAL_SUP_FEAT;                       /* mandatory */
  pBuf[15] = HCI_SUP_READ_BD_ADDR;                              /* mandatory */
  pBuf[22] = HCI_SUP_SET_EVENT_MASK_PAGE2;                      /* optional or LE ping */
  pBuf[25] = HCI_SUP_LE_SET_EVENT_MASK |                        /* mandatory */
             HCI_SUP_LE_READ_BUF_SIZE |                         /* mandatory */
             HCI_SUP_LE_READ_LOCAL_SUP_FEAT |                   /* mandatory */
             HCI_SUP_LE_SET_RAND_ADDR;                          /* Tx device */
  pBuf[26] = HCI_SUP_LE_READ_WHITE_LIST_SIZE |                  /* mandatory */
             HCI_SUP_LE_CLEAR_WHITE_LIST;                       /* mandatory */
  pBuf[27] = HCI_SUP_LE_ADD_DEV_WHITE_LIST |                    /* mandatory */
             HCI_SUP_LE_REMOVE_DEV_WHITE_LIST |                 /* mandatory */
             HCI_SUP_LE_RAND;                                   /* LL encryption or optional */
  pBuf[28] = HCI_SUP_LE_READ_SUP_STATES |                       /* mandatory (4.1+) */
             HCI_SUP_LE_RECEIVER_TEST |                         /* Rx device */
             HCI_SUP_LE_TRANSMITTER_TEST |                      /* Tx device */
             HCI_SUP_LE_TEST_END;                               /* mandatory */
  pBuf[41] = HCI_SUP_LE_MODIFY_SLEEP_CLK_ACCURACY;
  pBuf[44] = HCI_SUP_LE_SET_HOST_FEATURE;                       /* v5.2 */
  pBuf[45] = 0;                                                 /* v5.2 */
  pBuf[38] = HCI_SUP_LE_READ_TX_POWER;                          /* mandatory (5.0) */

  if (lhciCmdTbl[LHCI_MSG_CONN])
  {
    pBuf[0]  |= HCI_SUP_DISCONNECT;                             /* Master or slave */
    pBuf[2]  |= HCI_SUP_READ_REMOTE_VER_INFO;                   /* Master or slave */
    pBuf[10] |= HCI_SUP_READ_TX_PWR_LVL;                        /* Master or slave */
    pBuf[15] |= HCI_SUP_READ_RSSI;                              /* Master or slave */

    pBuf[27] |= HCI_SUP_LE_CONN_UPDATE |                        /* Master role or initiating conn param req */
                HCI_SUP_LE_READ_CHAN_MAP |                      /* Master or slave */
                HCI_SUP_LE_READ_REMOTE_FEAT;                    /* Master or optional */

    pBuf[33] |= HCI_SUP_LE_SET_DATA_LEN |                       /* Data length extensions */
                HCI_SUP_LE_READ_DEF_DATA_LEN |                  /* Data length extensions */
                HCI_SUP_LE_REM_CONN_PARAM_REQ_REPL |            /* Accepting conn param req */
                HCI_SUP_LE_REM_CONN_PARAM_REQ_NEG_REPL;         /* Accepting conn param req */
    pBuf[34] |= HCI_SUP_LE_WRITE_DEF_DATA_LEN;                  /* Data length extensions */
    pBuf[35] |= HCI_SUP_LE_READ_MAX_DATA_LEN;                   /* Data length extensions */
  }

  if (lhciCmdTbl[LHCI_MSG_SCAN]) /* i.e. master */
  {
    pBuf[26] |= HCI_SUP_LE_SET_SCAN_PARAM |                     /* Rx device */
                HCI_SUP_LE_SET_SCAN_ENABLE;                     /* Rx device */

    if (lhciCmdTbl[LHCI_MSG_CONN])
    {
      pBuf[26] |= HCI_SUP_LE_CREATE_CONN |                      /* Master role */
                  HCI_SUP_LE_CREATE_CONN_CANCEL;                /* Master role */
      pBuf[27] |= HCI_SUP_LE_SET_HOST_CHAN_CLASS;               /* Master role */
    }
  }

  if (lhciCmdTbl[LHCI_MSG_ADV]) /* i.e. slave */
  {
    pBuf[25] |= HCI_SUP_LE_SET_ADV_PARAM |                      /* Tx device */
                HCI_SUP_LE_READ_ADV_TX_POWER |                  /* Tx device */
                HCI_SUP_LE_SET_ADV_DATA;                        /* Tx device */
    pBuf[26] |= HCI_SUP_LE_SET_SCAN_RESP_DATA |                 /* Tx + Rx device */
                HCI_SUP_LE_SET_ADV_ENABLE;                      /* Tx device */
  }

  if (lhciCmdTbl[LHCI_MSG_ENC] &&
      lhciCmdTbl[LHCI_MSG_CONN])
  {
    pBuf[27] |= HCI_SUP_LE_ENCRYPT;                             /* LL encryption */
    pBuf[28] |= HCI_SUP_LE_LTK_REQ_REPL |                       /* LL encryption + slave */
                HCI_SUP_LE_LTK_REQ_NEG_REPL |                   /* LL encryption + slave */
                HCI_SUP_LE_START_ENCRYPTION;                    /* LL encryption + master */
    pBuf[32] |= HCI_SUP_READ_AUTH_PAYLOAD_TO |                  /* LE ping */
                HCI_SUP_WRITE_AUTH_PAYLOAD_TO;                  /* LE ping */
  }

  if (lhciCmdTbl[LHCI_MSG_PRIV])
  {
    pBuf[34] |= HCI_SUP_LE_ADD_DEV_RES_LIST_EVT |               /* LE privacy */
                HCI_SUP_LE_REMOVE_DEV_RES_LIST |                /* LE privacy */
                HCI_SUP_LE_CLEAR_RES_LIST |                     /* LE privacy */
                HCI_SUP_LE_READ_RES_LIST_SIZE;                  /* LE privacy */
    pBuf[35] |= HCI_SUP_LE_SET_ADDR_RES_ENABLE |                /* LE privacy */
                HCI_SUP_LE_SET_RES_PRIV_ADDR_TO;                /* LE privacy */
    pBuf[39] |= HCI_SUP_LE_SET_PRIVACY_MODE;                    /* LE privacy */

    if (lhciCmdTbl[LHCI_MSG_CONN])
    {
      pBuf[34] |= HCI_SUP_LE_READ_PEER_RES_ADDR;                /* LE privacy + master or slave */
      pBuf[35] |= HCI_SUP_LE_READ_LOCAL_RES_ADDR;               /* LE privacy + master or slave */
    }
  }

  if (lhciCmdTbl[LHCI_MSG_SC])
  {
    pBuf[34] |= HCI_SUP_LE_READ_LOCAL_P256_PUB_KEY |            /* Secure connections */
                HCI_SUP_LE_GENERATE_DHKEY;                      /* Secure connections */

    pBuf[41] |= HCI_SUP_LE_GENERATE_DHKEY_V2;                   /* Secure connections */
  }

  if (lhciCmdTbl[LHCI_MSG_PHY])
  {
    pBuf[35] |= HCI_SUP_LE_READ_PHY |                           /* LE PHY features */
                HCI_SUP_LE_SET_DEF_PHY |                        /* LE PHY features */
                HCI_SUP_LE_SET_PHY |                            /* LE PHY features */
                HCI_SUP_LE_ENHANCED_RECEIVER_TEST;              /* LE PHY features */
    pBuf[36] |= HCI_SUP_LE_ENHANCED_TRANSMITTER_TEST;           /* LE PHY features */
  }

  if (lhciCmdTbl[LHCI_MSG_EXT_ADV])
  {
    pBuf[36] |= HCI_SUP_LE_SET_ADV_SET_RAND_ADDR |              /* Extended advertising */
                HCI_SUP_LE_SET_EXT_ADV_PARAM |                  /* Extended advertising */
                HCI_SUP_LE_SET_EXT_ADV_DATA |                   /* Extended advertising */
                HCI_SUP_LE_SET_EXT_SCAN_RESP_DATA |             /* Extended advertising */
                HCI_SUP_LE_SET_EXT_ADV_ENABLE |                 /* Extended advertising */
                HCI_SUP_LE_READ_MAX_ADV_DATA_LEN |              /* Extended advertising */
                HCI_SUP_LE_READ_NUM_OF_SUP_ADV_SETS;            /* Extended advertising */
    pBuf[37] |= HCI_SUP_LE_REMOVE_ADV_SET |                     /* Extended advertising */
                HCI_SUP_LE_CLEAR_ADV_SETS;                      /* Extended advertising */
    pBuf[39] |= HCI_SUP_LE_READ_RF_PATH_COMP |                  /* Extended advertising */
                HCI_SUP_LE_WRITE_RF_PATH_COMP;                  /* Extended advertising */
  }

  if (lhciCmdTbl[LHCI_MSG_EXT_SCAN])
  {
    pBuf[37] |= HCI_SUP_LE_SET_EXT_SCAN_PARAM |                 /* Extended scanning */
                HCI_SUP_LE_SET_EXT_SCAN_ENABLE;                 /* Extended scanning */

    if (lhciCmdTbl[LHCI_MSG_CONN])
    {
      pBuf[37] |= HCI_SUP_LE_EXT_CREATE_CONN;                   /* Extended initiate. */
    }
  }

  if (lhciCmdTbl[LHCI_MSG_PER_ADV])
  {
    pBuf[37] |= HCI_SUP_LE_SET_PER_ADV_PARAM |                  /* Periodic advertising */
                HCI_SUP_LE_SET_PER_ADV_DATA  |                  /* Periodic advertising */
                HCI_SUP_LE_SET_PER_ADV_ENABLE;
  }

  if (lhciCmdTbl[LHCI_MSG_PER_SCAN])
  {
    pBuf[38] |= HCI_SUP_LE_PER_ADV_CREATE_SYNC |                /* Periodic scanning */
                HCI_SUP_LE_PER_ADV_CREATE_SYNC_CANCEL  |        /* Periodic scanning */
                HCI_SUP_LE_PER_ADV_TERMINATE_SYNC  |            /* Periodic scanning */
                HCI_SUP_LE_ADD_DEV_PER_ADV_LIST  |              /* Periodic scanning */
                HCI_SUP_LE_REMOVE_DEV_PER_ADV_LIST  |           /* Periodic scanning */
                HCI_SUP_LE_CLEAR_PER_ADV_LIST  |                /* Periodic scanning */
                HCI_SUP_LE_READ_PER_ADV_LIST_SIZE;              /* Periodic scanning */
  }

  if (lhciCmdTbl[LHCI_MSG_PAST])
  {
    pBuf[40] |= HCI_SUP_LE_SET_PER_ADV_RCV_ENABLE |             /* Periodic advertising sync transfer */
                HCI_SUP_LE_PER_ADV_SYNC_TRANSFER  |             /* Periodic advertising sync transfer */
                HCI_SUP_LE_PER_ADV_SET_INFO_TRANSFER;           /* Periodic advertising sync transfer */
    pBuf[41] |= HCI_SUP_LE_SET_PAST_PARAM |                     /* Periodic advertising sync transfer */
                HCI_SUP_LE_SET_DEFAULT_PAST_PARAM;              /* Periodic advertising sync transfer */
  }

  if (lhciCmdTbl[LHCI_MSG_CIS_MST])
  {
    pBuf[41] |= HCI_SUP_LE_SET_CIG_PARAM;                       /* ISO CIS master */
    pBuf[42] |= HCI_SUP_LE_SET_CIG_PARAM_TEST |                 /* ISO CIS master */
                HCI_SUP_LE_CREATE_CIS |                         /* ISO CIS master */
                HCI_SUP_LE_REMOVE_CIG;                          /* ISO CIS master */
  }

  if (lhciCmdTbl[LHCI_MSG_CIS_SLV])
  {
    pBuf[42] |= HCI_SUP_LE_ACCEPT_CIS_REQ |                     /* ISO CIS slave */
                HCI_SUP_LE_REJECT_CIS_REQ;                      /* ISO CIS slave */
  }

  if (lhciCmdTbl[LHCI_MSG_BIS_MST])
  {
    pBuf[43] |= HCI_SUP_LE_BIG_CREATE_SYNC |                    /* ISO BIS master */
                HCI_SUP_LE_BIG_TERMINATE_SYNC;                  /* ISO BIS master */
  }

  if (lhciCmdTbl[LHCI_MSG_BIS_SLV])
  {
    pBuf[42] |= HCI_SUP_LE_CREATE_BIG |                         /* ISO BIS slave */
                HCI_SUP_LE_CREATE_BIG_TEST |                    /* ISO BIS slave */
                HCI_SUP_LE_TERMINATE_BIG;                       /* ISO BIS slave */
  }

  if (lhciCmdTbl[LHCI_MSG_ISO])
  {
    pBuf[41] |= HCI_SUP_LE_READ_BUF_SIZE_V2 |                   /* ISO */
                HCI_SUP_LE_READ_ISO_TX_SYNC;                    /* ISO */
    pBuf[43] |= HCI_SUP_LE_SETUP_ISO_DATA_PATH |                /* ISO */
                HCI_SUP_LE_REMOVE_ISO_DATA_PATH  |              /* ISO */
                HCI_SUP_LE_REQ_PEER_SCA;                        /* ISO */

    pBuf[43] |= HCI_SUP_LE_ISO_TRANSMIT_TEST |                  /* ISO */
                HCI_SUP_LE_ISO_RECEIVE_TEST |                   /* ISO */
                HCI_SUP_LE_ISO_READ_TEST_COUNTERS;              /* ISO */
    pBuf[44] |= HCI_SUP_LE_ISO_TEST_END |                       /* ISO */
                HCI_SUP_LE_READ_ISO_LINK_QUALITY;               /* ISO */

    pBuf[45] |= HCI_SUP_READ_LOCAL_SUP_CODECS_V2 |              /* ISO */
                HCI_SUP_READ_LOCAL_SUP_CODEC_CAP |              /* ISO */
                HCI_SUP_READ_LOCAL_SUP_CTR_DLY |                /* ISO */
                HCI_SUP_CONFIG_DATA_PATH;                       /* ISO */
  }

  if (lhciCmdTbl[LHCI_MSG_PC])
  {
    pBuf[44] |= HCI_SUP_LE_ENH_READ_TX_POWER_LEVEL |            /* LEPC */
                HCI_SUP_LE_READ_REMOTE_TX_POWER_LEVEL |         /* LEPC */
                HCI_SUP_LE_SET_PATH_LOSS_REPORT_PARAM |         /* LEPC */
                HCI_SUP_LE_SET_PATH_LOSS_REPORT_PARAM |         /* LEPC */
                HCI_SUP_LE_SET_PATH_LOSS_REPORT_ENABLE |        /* LEPC */
                HCI_SUP_LE_SET_TX_POWER_REPORT_ENABLE;          /* LEPC */
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Finalize HCI initialization.
 *
 *  Called after all specific initializers.
 */
/*************************************************************************************************/
void LhciInitFinalize(void)
{
  LhciSetDefaultHciSupCmd(lhciPersistCb.supCmds);
}

/*************************************************************************************************/
/*!
 *  \brief  Register an event complete callback.
 *
 *  \param  compCback   Event completion callback.
 */
/*************************************************************************************************/
void LhciRegisterSendTrCompleteHandler(lhciCompHandler_t compCback)
{
  lhciPersistCb.evtCompCback = compCback;
}
