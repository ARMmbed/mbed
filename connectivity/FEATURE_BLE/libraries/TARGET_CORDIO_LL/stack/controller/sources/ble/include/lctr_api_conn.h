/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller connection interface file.
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

#ifndef LCTR_API_CONN_H
#define LCTR_API_CONN_H

#include "lctr_api.h"
#include "bb_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *  \addtogroup LL_LCTR_API_CONN
 *  \{
 */

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Data channel PDU length (header + payload + MIC). */
#define LCTR_DATA_PDU_LEN(len)      ((len) + LL_DATA_HDR_LEN + BB_DATA_PDU_TAILROOM)

/*! \brief      Maximum data channel PDU length (header + payload + MIC). */
#define LCTR_DATA_PDU_MAX_LEN       LCTR_DATA_PDU_LEN(BB_DATA_PLD_MAX_LEN)

/*! \brief      Minimum data channel PDU length (header + payload + MIC). */
#define LCTR_DATA_PDU_MIN_LEN       LCTR_DATA_PDU_LEN(LL_MAX_DATA_LEN_MIN)

/*! \brief      Maximum value for maximum Data PDU length (spec limit is 251) */
#define LCTR_MAX_DATA_LEN_MAX       BB_DATA_PLD_MAX_LEN

/*! \brief      Maximum handle index (CIS included). */
#define LCTR_MAX_HANDLE_INDEX       (pLctrRtCfg->maxConn + pLctrRtCfg->maxCis)

/*! \brief      Connected task messages for \a LCTR_DISP_CONN dispatcher. */
enum
{
  /* Broadcast events */
  LCTR_CONN_MSG_RESET                   = LCTR_MSG_RESET,
  /* Receive remote PDU events */
  _LCTR_CONN_MSG_RX_EVENTS              = 10,
  LCTR_CONN_MSG_RX_CONNECT_IND,         /*!< Connect indication received. */
  LCTR_CONN_MSG_RX_LLCP,                /*!< LLCP message received. */
  LCTR_CONN_MSG_RX_LLCP_UNKNOWN,        /*!< Unknown LLCP message received. */
  LCTR_CONN_MSG_RX_LLCP_INVALID_PARAM,  /*!< LLCP message with invalid parameter received. */
  /* Host/API events */
  _LCTR_CONN_MSG_API_EVENTS             = 20,
  LCTR_CONN_MSG_API_CONN_UPDATE,        /*!< Connection update API event. */
  LCTR_CONN_MSG_API_CHAN_MAP_UPDATE,    /*!< Channel map update API event. */
  LCTR_CONN_MSG_API_DISCONNECT,         /*!< Disconnect API event. */
  LCTR_CONN_MSG_API_START_ENC,          /*!< Start encryption API event. */
  LCTR_CONN_MSG_API_LTK_REPLY,          /*!< LTK reply. */
  LCTR_CONN_MSG_API_LTK_NEG_REPLY,      /*!< LTK negative reply. */
  LCTR_CONN_MSG_API_REMOTE_FEATURE,     /*!< Read remote feature API event. */
  LCTR_CONN_MSG_API_REMOTE_VERSION,     /*!< Read remote version API event. */
  LCTR_CONN_MSG_API_CONN_PARAM_REPLY,   /*!< Remote connection parameter request reply API event. */
  LCTR_CONN_MSG_API_CONN_PARAM_NEG_REPLY,/*!< Remote connection parameter request negative reply API event. */
  LCTR_CONN_MSG_API_DATA_LEN_CHANGE,    /*!< Data length change API event. */
  LCTR_CONN_MSG_API_PHY_UPDATE,         /*!< PHY update API event. */
  LCTR_CONN_MSG_API_SET_MIN_USED_CHAN,  /*!< Set minimum number of used channels API event. */
  LCTR_CONN_MSG_API_PER_ADV_SYNC_TRSF,  /*!< Periodic advertising sync transfer API event. */
  LCTR_CONN_MSG_API_REQ_PEER_SCA,       /*!< Request peer SCA. */
  LCTR_CONN_MSG_API_CIS_REQ,            /*!< CIS request API event. */
  LCTR_CONN_MSG_API_CIS_REQ_ACCEPT,     /*!< Peer CIS request accept API event. */
  LCTR_CONN_MSG_API_CIS_REQ_REJECT,     /*!< Peer CIS request accept API event. */
  LCTR_CONN_MSG_API_PWR_CTRL_REQ,       /*!< Peer power control request API event. */
  LCTR_CONN_MSG_API_SUBRATE_REQ,        /*!< Subrate request API event. */
  /* Internal events */
  _LCTR_CONN_INT_EVENTS                 = 40,
  LCTR_CONN_DATA_PENDING,               /*!< New data pending. */
  LCTR_CONN_ARQ_Q_FLUSHED,              /*!< ARQ queue transitioned to empty. */
  LCTR_CONN_MST_ESTABLISH,              /*!< Establish connection (master only). */
  LCTR_CONN_SLV_INIT_STARTUP_LLCP,      /*!< Slave initiates startup LLCP procedures(slave only). */
  _LCTR_CONN_LLCP_EVENTS                = 50,
  LCTR_CONN_LLCP_CONN_UPDATE,           /*!< LL initiated connection update procedure. */
  LCTR_CONN_LLCP_VERSION_EXCH,          /*!< LL initiated remote version exchange. */
  LCTR_CONN_LLCP_FEATURE_EXCH,          /*!< LL initiated remote feature exchange. */
  LCTR_CONN_LLCP_LENGTH_EXCH,           /*!< LL initiated data length exchange. */
  LCTR_CONN_LLCP_PWR_CTRL_SERVICE,      /*!< Service power control monitor. */
  LCTR_CONN_LLCP_TERM,                  /*!< LL initiated termination. */
  LCTR_CONN_LLCP_CHANNEL_STATUS,        /*!< LL initiated channel status. */
  LCTR_CONN_LLCP_CHANNEL_REPORTING,     /*!< LL initiated channel reporting. */
  LCTR_CONN_LLCP_PROC_CMPL,             /*!< LLCP procedure completed. */
  LCTR_CONN_LLCP_PROC_ABORTED,          /*!< LLCP procedure aborted. */
  LCTR_CONN_LLCP_START_PENDING,         /*!< Start pending LLCP procedure. */
  LCTR_CONN_LLCP_SKIP_CONN_PARAM,       /*!< Skip connection parameter exchange. */
  LCTR_CONN_LLCP_REJECT_CONN_UPD,       /*!< Reject a connection update. */
  _LCTR_CONN_TERM_EVENTS                = 70,
  LCTR_CONN_TERM_SUP_TIMEOUT,           /*!< Terminate connection due to supervision timeout. */
  LCTR_CONN_TERM_MIC_FAILED,            /*!< Terminate connection due to MIC failure. */
  LCTR_CONN_TERM_INST_PASSED,           /*!< Terminate connection due to instant passed. */
  LCTR_CONN_TERM_CIS_LOCAL_RESOURCE,    /*!< Terminate CIS connection due to local resource limitation. */
  LCTR_CONN_TERMINATED,                 /*!< Connection event terminated. */
  LCTR_CONN_INIT_CANCELED,              /*!< Connection cancelled event. */
  _LCTR_CONN_TMR_EVENTS                 = 80,
  LCTR_CONN_TMR_LLCP_RSP_EXP,           /*!< LLCP response timer expired. */
  LCTR_CONN_TMR_CIS_LLCP_RSP_EXP,       /*!< CIS LLCP response timer expired. */
  LCTR_CONN_TMR_PING_PERIOD_EXP,        /*!< LE Ping period timer expired. */
  LCTR_CONN_TMR_AUTH_PAYLOAD_EXP        /*!< Authentication payload timer expired. */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Connection update message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  LlConnSpec_t      connSpec;           /*!< Updated connection specification. */
} lctrConnUpdate_t;

/*! \brief      Disconnect message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           reason;             /*!< Disconnect reason. */
} lctrDisconnect_t;

/*! \brief      Start encryption message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           rand[LL_RAND_LEN];  /*!< Random number. */
  uint16_t          diversifier;        /*!< Diversifier. */
  uint8_t           key[LL_KEY_LEN];    /*!< Encryption key. */
} lctrStartEnc_t;

/*! \brief      Remote connection parameter reply message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           key[LL_KEY_LEN];    /*!< Encryption key. */
} lctrLtkReply_t;

/*! \brief      Remote connection parameter reply message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  LlConnSpec_t      connSpec;           /*!< Updated connection specification. */
} lctrConnParamReply_t;

/*! \brief      Remote connection parameter negative reply message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           reason;             /*!< Reason code. */
} lctrConnParamNegReply_t;

/*! \brief      Data length change message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint16_t          maxTxLen;           /*!< Maximum transmit length. */
  uint16_t          maxTxTime;          /*!< Maximum transmit time. */
} lctrDataLengthChange_t;

/*! \brief      PHY update message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           allPhys;            /*!< All PHYs preferences. */
  uint8_t           txPhys;             /*!< Preferred transmitter PHYs. */
  uint8_t           rxPhys;             /*!< Preferred receiver PHYs. */
  uint16_t          phyOptions;         /*!< PHY options. */
} lctrPhyUpdate_t;

/*! \brief      Set minimum number of used channels message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           phys;               /*!< Bitmask for the PHYs. */
  uint8_t           minUsedChan;        /*!< Minimum number of used channels. */
} lctrSetMinUsedChan_t;

/*! \brief      Periodic advertising sync transfer message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint16_t          syncSource;         /*!< Periodic sync source. */
  uint16_t          syncHandle;         /*!< Periodic sync handle. */
  uint16_t          serviceData;        /*!< Service data provided by the host. */
} lctrPerAdvSyncTrsf_t;

/*! \brief      Set minimum number of used channels message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           action;             /*!< Action. */
} lctrScaReq_t;

/*! \brief      CIS set CIG test CIS parameters. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint16_t          cisHandle;          /*!< CIS handle. */
} lctrCreateCis_t;

/*! \brief      Internal reject CIS request message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                    /*!< Message header. */
  uint8_t           reason;                 /*!< Reject reason. */
} lctrRejCisReq_t;

/*! \brief      Disconnect message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message header. */
  uint8_t           reason;             /*!< Disconnect reason. */
  uint16_t          cisHandle;          /*!< CIS handle. */
} lctrCisDisc_t;

/*! \brief      Internal power control request message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message Header. */
  int8_t            delta;              /*!< Delta requested. */
  uint8_t           phy;                /*!< PHY requested. */
} lctrMsgPwrCtrlReq_t;

/*! \brief      Internal subrate request message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message Header. */
  uint16_t          srMin;              /*!< Subrate minimum value. */
  uint16_t          srMax;              /*!< Subrate maximum value. */
  uint16_t          maxLatency;         /*!< Maximum latency. */
  uint16_t          contNum;            /*!< Continuation number. */
  uint16_t          svt;                /*!< Supervision timeout in 10ms units. */
} lctrMsgSubrateReq_t;

/*! \brief      Channel reporting indication message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message Header. */
  uint8_t           enable;             /*!< Enable. */
  uint8_t           minSpacing;         /*!< Minimum status report spacing. */
  uint8_t           maxDelay;           /*!< Maximum status report delay. */
} lctrMsgChRptInd_t;

/*! \brief      Channel status indication message. */
typedef struct
{
  lctrMsgHdr_t      hdr;                /*!< Message Header. */
  uint8_t           chanStatus[LL_MAX_NUM_CHAN_DATA];
                                        /*!< Status of used channels. */
} lctrMsgChStatusInd_t;

/*! \brief      Link layer controller message data. */
typedef union
{
  lctrMsgHdr_t            hdr;              /*!< Message header. */
  lctrConnEstablish_t     connEstablish;    /*!< Connection establish message data. */
  lctrConnUpdate_t        connUpd;          /*!< Connection update message data. */
  lctrChanMapUpdate_t     chanMapUpd;       /*!< Channel map update message data. */
  lctrDisconnect_t        disc;             /*!< Disconnect message data. */
  lctrStartEnc_t          startEnc;         /*!< Start encryption message data. */
  lctrLtkReply_t          ltkReply;         /*!< LTK reply message data. */
  lctrConnParamReply_t    connParamReply;   /*!< Remote connection parameter reply message data. */
  lctrConnParamNegReply_t connParamNegReply;/*!< Remote connection parameter negative reply message data. */
  lctrDataLengthChange_t  dataLenChange;    /*!< Data length change message data. */
  lctrPhyUpdate_t         phyUpd;           /*!< PHY update message data. */
  lctrSetMinUsedChan_t    setMinUsedChan;   /*!< Set minimum number of used channels message data. */
  lctrPerAdvSyncTrsf_t    perAdvSyncTrsf;   /*!< Periodic advertising sync transfer data. */
  lctrScaReq_t            scaReq;           /*!< Sleep clock accuracy request. */
  lctrMsgPwrCtrlReq_t     pwrCtrlReq;       /*!< Power control request. */
  lctrMsgSubrateReq_t     subrateReq;       /*!< Subrate request. */
  lctrMsgChRptInd_t       chanRptInd;       /*!< Channel report indication. */
  lctrMsgChStatusInd_t    chanStatusInd;    /*!< Channel status indication. */

  /* CIS */
  lctrCreateCis_t         createCis;        /*!< Create CIS message data. */
  lctrRejCisReq_t         rejCisReq;        /*!< Reject CIS request message data. */
  lctrCisDisc_t           cisDisc;          /*!< CIS disconnect message data. */
} lctrConnMsg_t;

/*! \brief      Initialize connection context. */
typedef void (*LctrInitConnHdlr_t)(uint16_t connHandle);

/*! \brief      Set transmit flow control. */
typedef void (*LctrTxFcHdlr_t)(uint16_t connHandle, uint8_t *pBuf);

/*! \brief      Transmit PDU complete. */
typedef void (*LctrTxPduCompHdlr_t)(uint16_t connHandle);

/*! \brief      Transmit empty packet acknowledgment, returns TRUE if required. */
typedef bool_t (*LctrTxAckHdlr_t)(uint16_t connHandle);

/*! \brief      Receive process flow control, returns TRUE if retransmission. */
typedef bool_t (*LctrRxAckHdlr_t)(uint16_t connHandle);

/*! \brief      VS receive data handler. */
typedef void (*LctrVsRecvHdlr_t)(uint16_t connHandle, uint8_t *pBuf);

/*! \brief      Connection event complete handler. */
typedef void (*LctrCeCompHdlr_t)(uint16_t connHandle);

/*! \brief      Vendor specific PDU handlers. */
typedef struct
{
  LctrInitConnHdlr_t  connSetup;        /*!< Setup connection context. */
  LctrInitConnHdlr_t  connCleanup;      /*!< Cleanup connection context. */
  LctrTxFcHdlr_t      txPduFc;          /*!< Set transmit PDU flow control. */
  LctrTxAckHdlr_t     txPduAck;         /*!< Tx PDU acknowledge required? */
  LctrRxAckHdlr_t     rxPduAck;         /*!< Process a receive PDU acknowledgment. */
  LctrVsRecvHdlr_t    dataRecv;         /*!< Receive data buffer handler. */
  LctrTxPduCompHdlr_t ceSetup;          /*!< Setup connection event. */
  LctrCeCompHdlr_t    ceCleanup;        /*!< Cleanup connection event. */
} LctrVsHandlers_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void LctrSlvConnInit(void);
void LctrSlvConnEncInit(void);
void LctrMstConnInit(void);
void LctrMstConnEncInit(void);
void LctrVsConnInit(const LctrVsHandlers_t *pHdlrs);

/* Helpers */
uint8_t LctrValidateConnSpec(const LlConnSpec_t *pConnSpec);
uint8_t LctrValidateModifyScaParam(uint8_t action);
bool_t LctrIsProcActPended(uint16_t handle, uint8_t event);
bool_t LctrIsFeatExchHostInit(uint16_t handle);
void LctrSetHostNotifyFeatExch(uint16_t handle);

/* Status */
bool_t LctrIsConnHandleEnabled(uint16_t handle);
bool_t LctrIsCisConnHandleEnabled(uint16_t handle);
uint8_t LctrGetRole(uint16_t handle);
int8_t LctrGetRssi(uint16_t handle);
uint8_t lctrSetTxPowerReporting(uint16_t handle, uint8_t enableLocal, uint8_t enableRemote);
int8_t LctrGetTxPowerLevel(uint16_t handle);
uint64_t LctrGetChannelMap(uint16_t handle);
uint64_t LctrGetUsedFeatures(uint16_t handle);
uint8_t LctrGetTxPhy(uint16_t handle);
uint8_t LctrGetRxPhy(uint16_t handle);
bool_t LctrCisTerminationInProgress(uint16_t handle);
void LctrGetPeerMinUsedChan(uint16_t handle, uint8_t *pPeerMinUsedChan);
bool_t LctrIsWaitingForReply(uint16_t handle, uint8_t reply);
bool_t LctrIsCisEnabled(uint16_t handle);

/* Control */
void LctrSetTxPowerLevel(uint16_t handle, int8_t level);
void LctrSetPhyTxPowerLevel(uint16_t handle, int8_t level, uint8_t phy);
int8_t LctrGetPhyTxPowerLevel(uint16_t handle, uint8_t phy);
uint32_t LctrGetAuthPayloadTimeout(uint16_t handle);
bool_t LctrSetAuthPayloadTimeout(uint16_t handle, uint32_t timeoutMs);
void LctrGetEncMode(uint16_t handle, LlEncMode_t *pMode);
bool_t LctrSetEncMode(uint16_t handle, const LlEncMode_t *pMode);
void LctrSetConnOpFlags(uint16_t handle, uint32_t flags, bool_t enable);
uint8_t lctrSetPowerMonitorEnable(uint16_t handle, bool_t enable);

/* Data path */
void LctrTxAcl(uint8_t *pAclBuf);
uint8_t *LctrRxAcl(void);
void LctrRxAclComplete(uint8_t numBufs);

/*! \} */    /* LL_LCTR_API_CONN */

#ifdef __cplusplus
};
#endif

#endif /* LCTR_API_CONN_H */
