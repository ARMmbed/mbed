/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager main module.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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

#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "hci_api.h"
#include "l2c_defs.h"
#include "dm_api.h"
#include "dm_main.h"
#include "dm_conn.h"
#include "dm_scan.h"
#include "dm_adv.h"
#include "dm_dev.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* HCI callback event routing table */
static const uint8_t dmHciToIdTbl[] =
{
  DM_ID_DEV,                                    /* HCI_RESET_SEQ_CMPL_CBACK_EVT */
  DM_ID_CONN,                                   /* HCI_LE_CONN_CMPL_CBACK_EVT */
  DM_ID_CONN,                                   /* HCI_LE_ENHANCED_CONN_CMPL_CBACK_EVT */
  DM_ID_CONN,                                   /* HCI_DISCONNECT_CMPL_CBACK_EVT */
  DM_ID_CONN,                                   /* HCI_LE_CONN_UPDATE_CMPL_CBACK_EVT */
  DM_ID_CONN,                                   /* HCI_LE_CREATE_CONN_CANCEL_CMD_CMPL_CBACK_EVT */
  DM_ID_SCAN,                                   /* HCI_LE_ADV_REPORT_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_READ_RSSI_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_READ_CHAN_MAP_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_READ_TX_PWR_LVL_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_READ_REMOTE_VER_INFO_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_READ_REMOTE_FEAT_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_LE_LTK_REQ_REPL_CMD_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_LE_LTK_REQ_NEG_REPL_CMD_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_ENC_KEY_REFRESH_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_ENC_CHANGE_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_LE_LTK_REQ_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_VENDOR_SPEC_CMD_STATUS_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_VENDOR_SPEC_CMD_CMPL_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_VENDOR_SPEC_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_HW_ERROR_CBACK_EVT */
  DM_ID_PRIV,                                   /* HCI_LE_ADD_DEV_TO_RES_LIST_CMD_CMPL_CBACK_EVT */
  DM_ID_PRIV,                                   /* HCI_LE_REM_DEV_FROM_RES_LIST_CMD_CMPL_CBACK_EVT */
  DM_ID_PRIV,                                   /* HCI_LE_CLEAR_RES_LIST_CMD_CMPL_CBACK_EVT */
  DM_ID_PRIV,                                   /* HCI_LE_READ_PEER_RES_ADDR_CMD_CMPL_CBACK_EVT */
  DM_ID_PRIV,                                   /* HCI_LE_READ_LOCAL_RES_ADDR_CMD_CMPL_CBACK_EVT */
  DM_ID_PRIV,                                   /* HCI_LE_SET_ADDR_RES_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_LE_ENCRYPT_CMD_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_LE_RAND_CMD_CMPL_CBACK_EVT  */
  DM_ID_CONN_2,                                 /* HCI_LE_REM_CONN_PARAM_REP_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_REM_CONN_PARAM_NEG_REP_CMD_CMPL_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_LE_READ_DEF_DATA_LEN_CMD_CMPL_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_LE_WRITE_DEF_DATA_LEN_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_SET_DATA_LEN_CMD_CMPL_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_LE_READ_MAX_DATA_LEN_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_REM_CONN_PARAM_REQ_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_DATA_LEN_CHANGE_CBACK_EVT  */
  DM_ID_SEC,                                    /* HCI_LE_READ_LOCAL_P256_PUB_KEY_CMPL_CBACK_EVT */
  DM_ID_SEC,                                    /* HCI_LE_GENERATE_DHKEY_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_WRITE_AUTH_PAYLOAD_TO_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_AUTH_PAYLOAD_TO_EXPIRED_EVT */
  DM_ID_PHY,                                    /* HCI_LE_READ_PHY_CMD_CMPL_CBACK_EVT */
  DM_ID_PHY,                                    /* HCI_LE_SET_DEF_PHY_CMD_CMPL_CBACK_EVT */
  DM_ID_PHY,                                    /* HCI_LE_PHY_UPDATE_CMPL_CBACK_EVT  */
  DM_ID_SCAN,                                   /* HCI_LE_EXT_ADV_REPORT_CBACK_EVT */
  DM_ID_SCAN,                                   /* HCI_LE_SCAN_TIMEOUT_CBACK_EVT */
  DM_ID_ADV,                                    /* HCI_LE_ADV_SET_TERM_CBACK_EVT */
  DM_ID_ADV,                                    /* HCI_LE_SCAN_REQ_RCVD_CBACK_EVT */
  DM_ID_SYNC,                                   /* HCI_LE_PER_ADV_SYNC_EST_CBACK_EVT */
  DM_ID_SYNC,                                   /* HCI_LE_PER_ADV_REPORT_CBACK_EVT */
  DM_ID_SYNC,                                   /* HCI_LE_PER_ADV_SYNC_LOST_CBACK_EVT */
  DM_ID_DEV,                                    /* HCI_LE_CH_SEL_ALGO_CBACK_EVT */
  DM_ID_SCAN,                                   /* HCI_LE_SCAN_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_ADV,                                    /* HCI_LE_ADV_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_SCAN,                                   /* HCI_LE_EXT_SCAN_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_ADV,                                    /* HCI_LE_EXT_ADV_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_ADV_PER,                                /* HCI_LE_PER_ADV_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_DEV_PRIV,                               /* HCI_LE_SET_RAND_ADDR_CMD_CMPL_CBACK_EVT */
  DM_ID_SYNC,                                   /* HCI_LE_PER_SYNC_TRSF_RCVD_CBACK_EVT */
  DM_ID_PAST,                                   /* HCI_LE_PER_ADV_SYNC_TRSF_CMD_CMPL_CBACK_EVT */
  DM_ID_PAST,                                   /* HCI_LE_PER_ADV_SET_INFO_TRSF_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_CONN_IQ_REPORT_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_CTE_REQ_FAILED_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_SET_CONN_CTE_RX_PARAMS_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_SET_CONN_CTE_TX_PARAMS_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_CONN_CTE_REQ_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_CONN_CTE_RSP_ENABLE_CMD_CMPL_CBACK_EVT */
  DM_ID_CONN_CTE,                               /* HCI_LE_READ_ANTENNA_INFO_CMD_CMPL_CBACK_EVT */
  DM_ID_CIS,                                    /* HCI_LE_CIS_EST_CBACK_EVT */
  DM_ID_CIS,                                    /* HCI_LE_CIS_REQ_CBACK_EVT */
  DM_ID_CIS,                                    /* HCI_CIS_DISCONNECT_CMPL_CBACK_EVT */
  DM_ID_CONN_2,                                 /* HCI_LE_REQ_PEER_SCA_CBACK_EVT */
  DM_ID_CIS_CIG,                                /* HCI_LE_SET_CIG_PARAMS_CMD_CMPL_CBACK_EVT */
  DM_ID_CIS_CIG,                                /* HCI_LE_REMOVE_CIG_CMD_CMPL_CBACK_EVT */
  DM_ID_ISO,                                    /* HCI_LE_SETUP_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT */
  DM_ID_ISO,                                    /* HCI_LE_REMOVE_ISO_DATA_PATH_CMD_CMPL_CBACK_EVT */
  DM_ID_ISO,                                    /* HCI_CONFIG_DATA_PATH_CMD_CMPL_CBACK_EVT */
  DM_ID_ISO,                                    /* HCI_READ_LOCAL_SUP_CODECS_CMD_CMPL_CBACK_EVT */
  DM_ID_ISO,                                    /* HCI_READ_LOCAL_SUP_CODEC_CAP_CMD_CMPL_CBACK_EVT */
  DM_ID_ISO,                                    /* HCI_READ_LOCAL_SUP_CTR_DLY_CMD_CMPL_CBACK_EVT */
  DM_ID_BIS,                                    /* HCI_LE_CREATE_BIG_CMPL_CBACK_EVT */
  DM_ID_BIS,                                    /* HCI_LE_TERM_BIG_CMPL_CBACK_EVT */
  DM_ID_BIS_SYNC,                               /* HCI_LE_BIG_SYNC_EST_CBACK_EVT */
  DM_ID_BIS_SYNC,                               /* HCI_LE_BIG_SYNC_LOST_CBACK_EVT */
  DM_ID_BIS_SYNC,                               /* HCI_LE_BIG_TERM_SYNC_CMPL_CBACK_EVT */
  DM_ID_SYNC                                    /* HCI_LE_BIG_INFO_ADV_REPORT_CBACK_EVT */
#if MBED_CONF_CORDIO_ROUTE_UNHANDLED_COMMAND_COMPLETE_EVENTS
  , DM_ID_DEV                                   /* HCI_UNHANDLED_CMD_COMPL_CBACK_EVT */
#endif
};

/* DM callback event length table */
static const uint16_t dmEvtCbackLen[] =
{
  sizeof(wsfMsgHdr_t),                         /* DM_RESET_CMPL_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_ADV_START_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_ADV_STOP_IND */
  sizeof(dmAdvNewAddrIndEvt_t),                /* DM_ADV_NEW_ADDR_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_SCAN_START_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_SCAN_STOP_IND */
  sizeof(hciLeAdvReportEvt_t),                 /* DM_SCAN_REPORT_IND */
  sizeof(hciLeConnCmplEvt_t),                  /* DM_CONN_OPEN_IND */
  sizeof(hciDisconnectCmplEvt_t),              /* DM_CONN_CLOSE_IND */
  sizeof(hciLeConnUpdateCmplEvt_t),            /* DM_CONN_UPDATE_IND */
  sizeof(dmSecPairCmplIndEvt_t),               /* DM_SEC_PAIR_CMPL_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_SEC_PAIR_FAIL_IND */
  sizeof(dmSecEncryptIndEvt_t),                /* DM_SEC_ENCRYPT_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_SEC_ENCRYPT_FAIL_IND */
  sizeof(dmSecAuthReqIndEvt_t),                /* DM_SEC_AUTH_REQ_IND */
  sizeof(dmSecKeyIndEvt_t),                    /* DM_SEC_KEY_IND */
  sizeof(hciLeLtkReqEvt_t),                    /* DM_SEC_LTK_REQ_IND */
  sizeof(dmSecPairIndEvt_t),                   /* DM_SEC_PAIR_IND */
  sizeof(dmSecSlaveIndEvt_t),                  /* DM_SEC_SLAVE_REQ_IND */
  sizeof(dmSecOobCalcIndEvt_t),                /* DM_SEC_CALC_OOB_IND */
  sizeof(secEccMsg_t),                         /* DM_SEC_ECC_KEY_IND */
  sizeof(dmSecCnfIndEvt_t),                    /* DM_SEC_COMPARE_IND */
  sizeof(dmSecKeypressIndEvt_t),               /* DM_SEC_KEYPRESS_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_PRIV_RESOLVED_ADDR_IND */
  sizeof(dmPrivGenAddrIndEvt_t),               /* DM_PRIV_GENERATE_RPA_IND */
  sizeof(hciReadRssiCmdCmplEvt_t),             /* DM_CONN_READ_RSSI_IND */
  sizeof(hciLeAddDevToResListCmdCmplEvt_t),    /* DM_PRIV_ADD_DEV_TO_RES_LIST_IND */
  sizeof(hciLeRemDevFromResListCmdCmplEvt_t),  /* DM_PRIV_REM_DEV_FROM_RES_LIST_IND */
  sizeof(hciLeClearResListCmdCmplEvt_t),       /* DM_PRIV_CLEAR_RES_LIST_IND */
  sizeof(hciLeReadPeerResAddrCmdCmplEvt_t),    /* DM_PRIV_READ_PEER_RES_ADDR_IND */
  sizeof(hciLeReadLocalResAddrCmdCmplEvt_t),   /* DM_PRIV_READ_LOCAL_RES_ADDR_IND */
  sizeof(hciLeSetAddrResEnableCmdCmplEvt_t),   /* DM_PRIV_SET_ADDR_RES_ENABLE_IND */
  sizeof(hciLeRemConnParamReqEvt_t),           /* DM_REM_CONN_PARAM_REQ_IND */
  sizeof(hciLeDataLenChangeEvt_t),             /* DM_CONN_DATA_LEN_CHANGE_IND */
  sizeof(hciWriteAuthPayloadToCmdCmplEvt_t),   /* DM_CONN_WRITE_AUTH_TO_IND */
  sizeof(hciAuthPayloadToExpiredEvt_t),        /* DM_CONN_AUTH_TO_EXPIRED_IND */
  sizeof(hciLeReadPhyCmdCmplEvt_t),            /* DM_PHY_READ_IND */
  sizeof(hciLeSetDefPhyCmdCmplEvt_t),          /* DM_PHY_SET_DEF_IND */
  sizeof(hciLePhyUpdateEvt_t),                 /* DM_PHY_UPDATE_IND */
  sizeof(dmAdvSetStartEvt_t),                  /* DM_ADV_SET_START_IND */
  sizeof(hciLeAdvSetTermEvt_t),                /* DM_ADV_SET_STOP_IND */
  sizeof(hciLeScanReqRcvdEvt_t),               /* DM_SCAN_REQ_RCVD_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_EXT_SCAN_START_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_EXT_SCAN_STOP_IND */
  sizeof(hciLeExtAdvReportEvt_t),              /* DM_EXT_SCAN_REPORT_IND */
  sizeof(dmPerAdvSetStartEvt_t),               /* DM_PER_ADV_SET_START_IND */
  sizeof(dmPerAdvSetStopEvt_t),                /* DM_PER_ADV_SET_STOP_IND */
  sizeof(hciLePerAdvSyncEstEvt_t),             /* DM_PER_ADV_SYNC_EST_IND */
  sizeof(hciLePerAdvSyncEstEvt_t),             /* DM_PER_ADV_SYNC_EST_FAIL_IND */
  sizeof(hciLePerAdvSyncLostEvt_t),            /* DM_PER_ADV_SYNC_LOST_IND */
  sizeof(HciLePerAdvSyncTrsfRcvdEvt_t),        /* DM_PER_ADV_SYNC_TRSF_EST_IND */
  sizeof(HciLePerAdvSyncTrsfRcvdEvt_t),        /* DM_PER_ADV_SYNC_TRSF_EST_FAIL_IND */
  sizeof(hciLePerAdvSyncTrsfCmdCmplEvt_t),     /* DM_PER_ADV_SYNC_TRSF_IND */
  sizeof(hciLePerAdvSetInfoTrsfCmdCmplEvt_t),  /* DM_PER_ADV_SET_INFO_TRSF_IND */
  sizeof(hciLePerAdvReportEvt_t),              /* DM_PER_ADV_REPORT_IND */
  sizeof(hciLeReadRemoteFeatCmplEvt_t),        /* DM_REMOTE_FEATURES_IND */
  sizeof(hciReadRemoteVerInfoCmplEvt_t),       /* DM_READ_REMOTE_VER_INFO_IND */
  sizeof(hciLeConnIQReportEvt_t),              /* DM_CONN_IQ_REPORT_IND */
  sizeof(hciLeCteReqFailedEvt_t),              /* DM_CTE_REQ_FAIL_IND */
  sizeof(hciLeSetConnCteRxParamsCmdCmplEvt_t), /* DM_CONN_CTE_RX_SAMPLE_START_IND */
  sizeof(hciLeSetConnCteRxParamsCmdCmplEvt_t), /* DM_CONN_CTE_RX_SAMPLE_START_IND */
  sizeof(hciLeSetConnCteTxParamsCmdCmplEvt_t), /* DM_CONN_CTE_TX_CFG_IND */
  sizeof(hciLeConnCteReqEnableCmdCmplEvt_t),   /* DM_CONN_CTE_REQ_START_IND */
  sizeof(hciLeConnCteReqEnableCmdCmplEvt_t),   /* DM_CONN_CTE_REQ_STOP_IND */
  sizeof(hciLeConnCteRspEnableCmdCmplEvt_t),   /* DM_CONN_CTE_RSP_START_IND */
  sizeof(hciLeConnCteRspEnableCmdCmplEvt_t),   /* DM_CONN_CTE_RSP_STOP_IND */
  sizeof(hciLeReadAntennaInfoCmdCmplEvt_t),    /* DM_READ_ANTENNA_INFO_IND */
  sizeof(hciLeSetCigParamsCmdCmplEvt_t),       /* DM_CIS_CIG_CONFIG_IND */
  sizeof(hciLeRemoveCigCmdCmplEvt_t),          /* DM_CIS_CIG_REMOVE_IND */
  sizeof(HciLeCisReqEvt_t),                    /* DM_CIS_REQ_IND */
  sizeof(HciLeCisEstEvt_t),                    /* DM_CIS_OPEN_IND */
  sizeof(hciDisconnectCmplEvt_t),              /* DM_CIS_CLOSE_IND */
  sizeof(HciLeReqPeerScaCmplEvt_t_t),          /* DM_REQ_PEER_SCA_IND */
  sizeof(dmSetupIsoDataPathEvt_t),             /* DM_ISO_DATA_PATH_SETUP_IND */
  sizeof(dmRemoveIsoDataPathEvt_t),            /* DM_ISO_DATA_PATH_REMOVE_IND */
  sizeof(hciConfigDataPathCmdCmplEvt_t),       /* DM_DATA_PATH_CONFIG_IND */
  sizeof(hciReadLocalSupCodecsCmdCmplEvt_t),   /* DM_READ_LOCAL_SUP_CODECS_IND */
  sizeof(hciReadLocalSupCodecCapCmdCmplEvt_t), /* DM_READ_LOCAL_SUP_CODEC_CAP_IND */
  sizeof(hciReadLocalSupCtrDlyCmdCmplEvt_t),   /* DM_READ_LOCAL_SUP_CTR_DLY_IND */
  sizeof(HciLeCreateBigCmplEvt_t),             /* DM_BIG_START_IND */
  sizeof(HciLeTerminateBigCmplEvt_t),          /* DM_BIG_STOP_IND */
  sizeof(HciLeBigSyncEstEvt_t),                /* DM_BIG_SYNC_EST_IND */
  sizeof(HciLeBigSyncEstEvt_t),                /* DM_BIG_SYNC_EST_FAIL_IND */
  sizeof(HciLeBigSyncLostEvt_t),               /* DM_BIG_SYNC_LOST_IND */
  sizeof(HciLeBigTermSyncCmplEvt_t),           /* DM_BIG_SYNC_STOP_IND */
  sizeof(HciLeBigInfoAdvRptEvt_t),             /* DM_BIG_INFO_ADV_REPORT_IND */
  sizeof(dmL2cCmdRejEvt_t),                    /* DM_L2C_CMD_REJ_IND */
  sizeof(wsfMsgHdr_t),                         /* DM_ERROR_IND */
  sizeof(hciHwErrorEvt_t),                     /* DM_HW_ERROR_IND */
  sizeof(hciVendorSpecEvt_t)                   /* DM_VENDOR_SPEC_IND */
};

/* Default component function inteface */
static const dmFcnIf_t dmFcnDefault =
{
  dmEmptyReset,
  (dmHciHandler_t) dmEmptyHandler,
  (dmMsgHandler_t) dmEmptyHandler
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Component function interface table indexed DM component ID */
dmFcnIf_t *dmFcnIfTbl[DM_NUM_IDS] =
{
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_ADV */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_DEV_PRIV */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_SCAN */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_CONN */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_CONN_2 */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_SEC */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_PRIV */
  (dmFcnIf_t *) &dmDevFcnIf,                  /* DM_ID_DEV */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_LESC */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_PHY */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_ADV_PER */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_SYNC */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_PAST */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_CONN_CTE */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_CONN_UPD */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_PRIV_AES */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_CIS */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_CIS_CIG */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_BIS */
  (dmFcnIf_t *) &dmFcnDefault,                /* DM_ID_BIS_SYNC */
  (dmFcnIf_t *) &dmFcnDefault                 /* DM_ID_ISO */
};

/* Control block */
dmCb_t dmCb;

/*************************************************************************************************/
/*!
 *  \brief  HCI event callback function.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmHciEvtCback(hciEvt_t *pEvent)
{
  WSF_ASSERT(pEvent->hdr.event < sizeof(dmHciToIdTbl));

  /* if DM not resetting or resetting but incoming event is HCI reset sequence complete event */
  if (!dmCb.resetting || (pEvent->hdr.event == HCI_RESET_SEQ_CMPL_CBACK_EVT))
  {
    /* route event to DM component handling function */
    (*(dmFcnIfTbl[dmHciToIdTbl[pEvent->hdr.event]]->hciHandler))(pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM empty reset handler.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmEmptyReset(void)
{
  /* empty */
}

/*************************************************************************************************/
/*!
 *  \brief  DM empty event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmEmptyHandler(wsfMsgHdr_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \brief  Pass an HCI event to the DM connection management module.
 *
 *  \param  pEvent  HCI event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmDevPassHciEvtToConn(hciEvt_t *pEvent)
{
  /* pass event to DM connection management module */
  (*(dmFcnIfTbl[DM_ID_CONN]->hciHandler))(pEvent);
}

/*************************************************************************************************/
/*!
 *  \brief  Register a callback with DM for scan and advertising events.
 *
 *  \param  cback  Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmRegister(dmCback_t cback)
{
  dmCb.cback = cback;

  /* if LESC is enabled */
  if (dmFcnIfTbl[DM_ID_LESC] != &dmFcnDefault)
  {
    /* if largest LESC key length is larger than maximum RX PDU length */
    if (SMP_PUB_KEY_MSG_LEN > (HciGetMaxRxAclLen() - L2C_HDR_LEN))
    {
      dmEvt_t evt;

      evt.hdr.param = 0;
      evt.hdr.event = DM_ERROR_IND;
      evt.hdr.status = DM_ERR_SMP_RX_PDU_LEN_EXCEEDED;

      (*dmCb.cback)(&evt);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Find an advertising data element in the given advertising or scan response data.
 *
 *  \param  adType  Advertising data element type to find.
 *  \param  dataLen Data length.
 *  \param  pData   Pointer to advertising or scan response data.
 *
 *  \return Pointer to advertising data element byte array or NULL if not found.
 */
/*************************************************************************************************/
uint8_t *DmFindAdType(uint8_t adType, uint16_t dataLen, uint8_t *pData)
{
  /* while not at end of data and
   * data element length is not zero and
   * data element length is not erroneously more than the data length
   */
  while ((dataLen != 0) && (pData[DM_AD_LEN_IDX] != 0) && (pData[DM_AD_LEN_IDX] < dataLen))
  {
    /* if found */
    if (pData[DM_AD_TYPE_IDX] == adType)
    {
      return pData;
    }

    /* else go to next element */
    dataLen = dataLen - pData[DM_AD_LEN_IDX] - 1;
    pData = pData + pData[DM_AD_LEN_IDX] + 1;
  }

  /* not found */
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  DM handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID for DM.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmHandlerInit(wsfHandlerId_t handlerId)
{
  /* store handler ID */
  dmCb.handlerId = handlerId;

  dmCb.llPrivEnabled = FALSE;
  dmCb.resetting = FALSE;

  /* register with the HCI event interface */
  HciEvtRegister(dmHciEvtCback);
}

/*************************************************************************************************/
/*!
 *  \brief  WSF event handler for DM.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  /* Handle message */
  if (pMsg != NULL)
  {
    WSF_ASSERT(DM_ID_FROM_MSG(pMsg->event) < DM_NUM_IDS);

    /* if DM not resetting */
    if (!dmCb.resetting)
    {
      /* route message to DM component handling function */
      (*(dmFcnIfTbl[DM_ID_FROM_MSG(pMsg->event)]->msgHandler))(pMsg);
    }
  }
  /* Handle events */
  else if (event)
  {

  }
}

/*************************************************************************************************/
/*!
 *  \brief  Whether LL Privacy is enabled.
 *
 *  \return TRUE if LL Privacy is enabled. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t DmLlPrivEnabled(void)
{
  return dmCb.llPrivEnabled;
}

/*************************************************************************************************/
/*!
 *  \brief  Map an address type to a type used by LL.
 *
 *  \param  addrType   Address type used by Host.
 *
 *  \return Address type used by LL.
 */
/*************************************************************************************************/
uint8_t DmLlAddrType(uint8_t addrType)
{
  uint8_t llAddrType = addrType;

  /* if LL Privacy is enabled */
  if (dmCb.llPrivEnabled)
  {
    if (addrType == DM_ADDR_PUBLIC)
    {
      llAddrType = DM_ADDR_PUBLIC_IDENTITY;
    }
    else if (addrType == DM_ADDR_RANDOM)
    {
      llAddrType = DM_ADDR_RANDOM_IDENTITY;
    }
  }

  return llAddrType;
}

/*************************************************************************************************/
/*!
 *  \brief  Map an address type to a type used by Host.
 *
 *  \param  addrType   Address type used by LL.
 *
 *  \return Address type used by Host.
 */
/*************************************************************************************************/
uint8_t DmHostAddrType(uint8_t addrType)
{
  uint8_t hostAddrType = addrType;

  /* if LL Privacy is enabled */
  if (dmCb.llPrivEnabled)
  {
    if (addrType == DM_ADDR_PUBLIC_IDENTITY)
    {
      hostAddrType = DM_ADDR_PUBLIC;
    }
    else if (addrType == DM_ADDR_RANDOM_IDENTITY)
    {
      hostAddrType = DM_ADDR_RANDOM;
    }
  }

  return hostAddrType;
}

/*************************************************************************************************/
/*!
 *  \brief  Return size of a DM callback event.
 *
 *  \param  pDmEvt  DM callback event.
 *
 *  \return Size of DM callback event.
 */
/*************************************************************************************************/
uint16_t DmSizeOfEvt(dmEvt_t *pDmEvt)
{
  uint16_t len;

  /* if a valid DM event ID */
  if ((pDmEvt->hdr.event >= DM_CBACK_START) && (pDmEvt->hdr.event <= DM_CBACK_END))
  {
    len = dmEvtCbackLen[pDmEvt->hdr.event - DM_CBACK_START];
  }
  else
  {
    len = sizeof(wsfMsgHdr_t);
  }

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the PHY index for the given scanner PHY.
 *
 *  \param  numPhys  Number of scanner PHYs.
 *  \param  scanPhy  Scanner PHY.
 *
 *  \return PHY index.
 */
/*************************************************************************************************/
static uint8_t dmScanPhyToIdx(uint8_t numPhys, uint8_t scanPhy)
{
  /* if number of supported PHYs is 1 */
  if (numPhys == 1)
  {
    return 0;
  }

  /* if number of supported PHYs is 2 */
  if (numPhys == 2)
  {
    return (scanPhy == HCI_SCAN_PHY_LE_1M_BIT) ? 0 : 1;
  }

  /* all three PHYs are supported */
  return (scanPhy == HCI_SCAN_PHY_LE_1M_BIT) ? 0 : (scanPhy == HCI_SCAN_PHY_LE_2M_BIT) ? 1 : 2;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the PHY index for the given scanner PHY.
 *
 *  \param  scanPhy  Scanner PHY.
 *
 *  \return PHY index.
 */
/*************************************************************************************************/
uint8_t DmScanPhyToIdx(uint8_t scanPhy)
{
  return dmScanPhyToIdx(DM_NUM_PHYS, scanPhy);
}

/*************************************************************************************************/
/*!
 *  \brief  Return the PHY index for the given initiator PHY.
 *
 *  \param  numPhys  Number of initiator PHYs.
 *  \param  initPhy  Initiator PHY.
 *
 *  \return PHY index.
 */
/*************************************************************************************************/
uint8_t dmInitPhyToIdx(uint8_t numPhys, uint8_t initPhy)
{
  /* if number of supported PHYs is 1 */
  if (numPhys == 1)
  {
    return 0;
  }

  /* if number of supported PHYs is 2 */
  if (numPhys == 2)
  {
    return (initPhy == HCI_INIT_PHY_LE_1M_BIT) ? 0 : 1;
  }

  /* all three PHYs are supported */
  return (initPhy == HCI_INIT_PHY_LE_1M_BIT) ? 0 : (initPhy == HCI_INIT_PHY_LE_2M_BIT) ? 1 : 2;
}

/*************************************************************************************************/
/*!
 *  \brief  Return the PHY index for the given initiator PHY.
 *
 *  \param  initPhy  Initiator PHY.
 *
 *  \return PHY index.
 */
/*************************************************************************************************/
uint8_t DmInitPhyToIdx(uint8_t initPhy)
{
  return dmInitPhyToIdx(DM_NUM_PHYS, initPhy);
}
