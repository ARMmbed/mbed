/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller connection interface file.
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

#ifndef LCTR_INT_CONN_H
#define LCTR_INT_CONN_H

#include "lctr_int.h"
#include "lctr_api_conn.h"
#include "lctr_pdu_conn.h"
#include "lctr_pdu_adv.h"
#include "lmgr_api_conn.h"
#include "ll_defs.h"
#include "sch_api.h"
#include "bb_ble_api.h"
#include "bb_ble_api_op.h"
#include "wsf_cs.h"
#include "wsf_msg.h"
#include "wsf_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

#define LCTR_MAX_CONS_CRC           2   /*!< Maximum number of consecutive CRC failures. */

#define LCTR_DATA_PDU_START_OFFSET  2   /*!< Data PDU start offset in a buffer (match ACL header size). */

#define LCTR_DATA_PDU_FC_OFFSET     0   /*!< Flow control fields data PDU offset. */
#define LCTR_DATA_PDU_LEN_OFFSET    1   /*!< Length field data PDU offset. */

#define LCTR_MAX_INSTANT            32767   /*!< Maximum instant value for connection update. */

#define LCTR_CTRL_DATA_HANDLE       0xFF    /*!< Connection handle used for LL control PDUs. */

#define LCTR_LE_PING_ATTEMPTS       4   /*!< LE Ping attempts. */

#define LCTR_CH_SEL_MAX             2   /*!< Total number of channel selection algorithms. */

/*! \brief      Resolve the connection handle from the context pointer. */
#define LCTR_GET_CONN_HANDLE(pCtx)  (pCtx - pLctrConnTbl)

/*! \brief      Resolve connection context from the handle. */
#define LCTR_GET_CONN_CTX(h)        &(pLctrConnTbl[h])

/*! \brief      Resolve txPower from PHY index. */
#define LCTR_GET_TXPOWER(pCtx, phy, option) \
        (pCtx->phyTxPower[phy - (((phy == LL_PHY_LE_CODED) && (option ==  BB_PHY_OPTIONS_BLE_S2)) ? 0 : 1)])

/*! \brief      Set the txpower of a specified PHY. */
#define LCTR_SET_TXPOWER(pCtx, phy, pow) (pCtx->phyTxPower[phy - 1] = pow)

/*! \brief      Special reset terminate reason. */
#define LCTR_RESET_TERM_REASON  0xFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      Connected states. */
enum
{
  LCTR_CONN_STATE_INITIALIZED,          /*!< Connection initialized. */
  LCTR_CONN_STATE_ESTABLISHED_STARTUP,  /*!< Connection established before ready to start LLCP. */
  LCTR_CONN_STATE_ESTABLISHED_READY,    /*!< Connection established and ready to start LLCP. */
  LCTR_CONN_STATE_TERMINATING           /*!< Connection terminating. */
};

/*! \brief      LLCP states. */
enum
{
  LCTR_LLCP_STATE_IDLE,                 /*!< LLCP idle state. */
  LCTR_LLCP_STATE_BUSY,                 /*!< LLCP busy state. */
  LCTR_LLCP_STATE_TOTAL                 /*!< Total number of LLCP states. */
};

/*! \brief      Common LLCP states. */
enum
{
  LCTR_CMN_STATE_IDLE,                 /*!< LLCP idle state. */
  LCTR_CMN_STATE_BUSY,                 /*!< LLCP busy state. */
  LCTR_CMN_STATE_TOTAL                 /*!< Total number of LLCP states. */
};

/*! \brief      SVT states. */
enum
{
  LCTR_SVT_STATE_IDLE,                 /*!< SVT idle state. */
  LCTR_SVT_STATE_URGENT,               /*!< SVT urgent state. */
  LCTR_SVT_STATE_FATAL,                /*!< SVT fatal state. */
  LCTR_SVT_STATE_TOTAL                 /*!< Total number of SVT state. */
};

/*! \brief      Periodic sync source. */
enum
{
  LCTR_SYNC_SRC_SCAN,               /*!< Periodic sync info from scanner. */
  LCTR_SYNC_SRC_BCST,               /*!< Periodic sync info from broadcaster. */
  LCTR_SYNC_SRC_TOTAL               /*!< Total number of periodic sync source. */
};

/*! \brief      Common LLCP procedure IDs. */
enum
{
  /* Common SM LLCP procedures. */
  LCTR_PROC_CMN_TERM,                   /*!< Termination procedure. */
  LCTR_PROC_CMN_CH_MAP_UPD,             /*!< Channel map update procedure. */
  LCTR_PROC_CMN_FEAT_EXCH,              /*!< Feature exchange procedure. */
  LCTR_PROC_CMN_VER_EXCH,               /*!< Version exchange procedure. */
  LCTR_PROC_CMN_DATA_LEN_UPD,           /*!< Data length update procedure. */
  LCTR_PROC_CMN_SET_MIN_USED_CHAN,      /*!< Set minimum number of used channels procedure. */
  LCTR_PROC_CMN_PER_ADV_SYNC_TRSF,      /*!< Periodic advertising sync transfer procedure. */
  LCTR_PROC_CMN_REQ_PEER_SCA,           /*!< Request peer SCA procedure. */
  LCTR_PROC_CMN_CH_CLASS_REPORTING,     /*!< Channel classification reporting procedure. */
  LCTR_PROC_CMN_CH_STATUS_REPORT,       /*!< Channel status reporting procedure. */
  LCTR_PROC_CMN_TOTAL,                  /*!< Total number of common procedures. */

  /* Custom SM LLCP procedures. */
  LCTR_PROC_CONN_UPD,                   /*!< Connection update procedure. */
  LCTR_PROC_CONN_PARAM,                 /*!< Connection parameter procedure. */
  LCTR_PROC_ENCRYPT,                    /*!< Encryption Start or Restart procedure. */
  LCTR_PROC_LE_PING,                    /*!< LE Ping procedure. */
  LCTR_PROC_PHY_UPD,                    /*!< PHY update procedure. */
  LCTR_PROC_PHY_UPD_PEER,               /*!< Peer-initiated PHY update procedure. */
  LCTR_PROC_CIS_EST,                    /*!< CIS establishment procedure. */
  LCTR_PROC_CIS_EST_PEER,               /*!< Peer-initiated CIS establishment procedure. */
  LCTR_PROC_CIS_TERM,                   /*!< CIS termination procedure. */
  LCTR_PROC_CIS_TERM_PEER,              /*!< Peer-initiated CIS termination procedure. */
  LCTR_PROC_PWR_IND,                    /*!< Power indication prodecure. */
  LCTR_PROC_PWR_CTRL,                   /*!< Power control procedure. */
  LCTR_PROC_SUBRATE,                    /*!< Subrate procedure. */

  LCTR_PROC_INVALID = 0xFF              /*!< Invalid ID. */

  /* Note: additional procedures without instants can be overridden. */
};

/*! \brief        Power control monitor schemes. */
enum
{
  LCTR_PC_MONITOR_AUTO,                /*!< Automatic monitoring scheme. */
  LCTR_PC_MONITOR_PATH_LOSS,           /*!< Path loss monitoring scheme. */

  LCTR_PC_MONITOR_SCHEME_TOTAL         /*!< Total number of monitoring schemes. */
};

/*! \brief        Power control scheme states. */
enum
{
  LCTR_PC_MONITOR_DISABLED,            /*!< Disabled monitoring. */
  LCTR_PC_MONITOR_ENABLED,             /*!< Monitoring enabled. */
  LCTR_PC_MONITOR_READY                /*!< Monitoring ready for enable. */
};

/*! \brief      Check if CIS is enabled by the CIS handle signature. */
typedef bool_t (*lctrCheckCisEstCisFn_t)(uint16_t cisHandle);
/*! \brief      Check for CIS termination signature. */
typedef bool_t (*lctrCheckTermFn_t)(uint16_t aclHandle);
/*! \brief      Check if there is a CIS established by the ACL handle signature. */
typedef bool_t (*lctrCheckCisEstAclFn_t)(uint16_t aclHandle);

/*! \brief      Connection context. */
typedef struct
{
  bool_t            enabled;            /*!< Context enabled. */
  uint8_t           state;              /*!< Current state. */
  uint8_t           role;               /*!< Connection role. */
  uint32_t          opModeFlags;        /*!< Operational mode flags. */

  /* Host/API */
  uint8_t           termReason;         /*!< Termination reason code. */
  uint8_t           replyWaitingMsk;    /*!< Host reply waiting mask. */

  /* BB data */
  BbOpDesc_t        connBod;            /*!< Connection BOD. */
  BbBleData_t       bleData;            /*!< BLE BB operation data. */

  union
  {
    struct
    {
      uint16_t      totalAcc;           /*!< Combined sleep clock inaccuracy. */
      uint16_t      lastActiveEvent;    /*!< Last active event counter. */
      uint32_t      txWinSizeUsec;      /*!< Tx window size. */
      uint32_t      anchorPointUsec;    /*!< Anchor point in microseconds. */
      uint32_t      unsyncedTime;       /*!< Unsynced time in BB tick before connection update. */
      bool_t        initAckRcvd;        /*!< Ack received from master. */
      bool_t        abortSlvLatency;    /*!< If TRUE abort slave latency. */

      uint8_t       consCrcFailed;      /*!< Number of consecutive CRC failures. */
      bool_t        syncWithMaster;     /*!< Flag indicating synchronize packet received from master. */
      bool_t        rxFromMaster;       /*!< At least one successful packet received from master. */
      uint32_t      firstRxStartTsUsec; /*!< Timestamp of the first received frame regardless of CRC error in microseconds. */

      uint32_t      lastStatusSentTs;   /*!< Timestamp of last channel status update. */
      uint32_t      chanStatMinIntUs;   /*!< Minimum interval in us between channel status updates. */
      uint32_t      chanStatMaxDelay;   /*!< Max delay before a change of channel status is allowed to be sent. */
      uint32_t      queuedChanStatusTs; /*!< Timestamp of last channel status that is queued to be sent out. */
    } slv;                              /*!< Slave connection specific data. */

    struct
    {
      bool_t        sendConnUpdInd;     /*!< Send LL_CONNECTION_UPDATE_IND flag. */
      uint8_t       peerChannelStatus[LL_MAX_NUM_CHAN_DATA];
                                        /*!< Current channel status map of peer. */
      uint32_t      recvdChanStatTs;    /*!< Timestamp of last received channel status from peer. */
      uint32_t      chanStatMinIntUs;   /*!< Minimum interval in us between channel status updates. */
    } mst;                              /*!< Master connection specific data. */
  } data;                               /*!< Role specific data. */

  /* Connection event parameters */
  uint16_t          maxLatency;         /*!< Maximum latency. */
  uint16_t          eventCounter;       /*!< Connection event counter. */
  uint16_t          connInterval;       /*!< Connection interval. */

  /* RF parameters */
  int8_t            rssi;               /*!< RSSI. */
  uint8_t           lastRxStatus;       /*!< Status code of last rx. */

  /* Channel parameters */
  uint8_t           lastChanIdx;        /*!< Current channel index. */
  uint8_t           hopInc;             /*!< Hop increment. */
  uint8_t           numUsedChan;        /*!< Number of used channels. */
  uint64_t          chanMask;           /*!< Channel mask. */
  uint8_t           chanRemapTbl[LL_CHAN_DATA_MAX_IDX + 1];
                                        /*!< Channel remapping table. */
  uint8_t           usedChSel;          /*!< Used channel selection. */
  uint16_t          chIdentifier;       /*!< Channel identifier. */
  uint32_t          crcInit;            /*!< CRC initialization value. */
  uint32_t          accessAddr;         /*!< Connection access address. */
  int8_t            phyTxPower[LL_PC_PHY_TOTAL];
                                        /*!< Saved txPower configuration for PHYs. */

  /* Flow control */
  lctrDataPduHdr_t  txHdr;              /*!< Transmit data PDU header. */
  lctrDataPduHdr_t  rxHdr;              /*!< Receive data PDU header. */
  wsfQueue_t        txLeuQ;             /*!< Transmit LE-U queue. */
  wsfQueue_t        txArqQ;             /*!< Transmit ARQ queue. */
  wsfQueue_t        rxDataQ;            /*!< Receive data pending queue. */
  uint8_t           numTxComp;          /*!< Number of completed Tx buffers. */
  uint8_t           numRxPend;          /*!< Number of Rx pending buffers. */
  uint8_t           numTxPendCtrlPdu;   /*!< Number of pending Control PDUs. */
  bool_t            emptyPduPend;       /*!< Empty PDU ACK pending. */
  bool_t            emptyPduFirstAtt;   /*!< Empty PDU first attempt. */
  bool_t            forceStartPdu;      /*!< Next data will be forced to be a start PDU */

  /* Supervision */
  uint16_t          supTimeoutMs;       /*!< Supervision timeout in milliseconds. */
  wsfTimer_t        tmrSupTimeout;      /*!< Supervision timer. */
  bool_t            connEst;            /*!< Connection established. */
  uint8_t           svtState;           /*!< SVT urgency state. */

  /* Encryption */
  bool_t            pauseRxData;        /*!< Pause Rx data PDUs. */
  bool_t            pauseTxData;        /*!< Pause Tx data PDUs. */
  uint8_t           ltk[LL_KEY_LEN];    /*!< Long term key. */
  uint8_t           skd[LL_SKD_LEN];    /*!< Session key diversifier. */
  uint8_t           iv[LL_IV_LEN];      /*!< Initialization vector. */
  uint8_t           rand[LL_RAND_LEN];  /*!< Random number. */
  uint16_t          ediv;               /*!< Encrypted diversifier. */

  uint64_t          txPktCounter;       /*!< Transmit packet counter. */
  uint64_t          rxPktCounter;       /*!< Transmit packet counter. */

  /* Authentication */
  uint32_t          pingPeriodMs;       /*!< Ping period in milliseconds. */
  wsfTimer_t        tmrPingTimeout;     /*!< Ping timer. */
  uint32_t          authTimeoutMs;      /*!< Authentication payload timeout in milliseconds. */
  wsfTimer_t        tmrAuthTimeout;     /*!< Authentication payload timer. */

  LlEncMode_t       newEncMode;         /*!< Pending encryption mode. */
  bool_t            pendEncMode;        /*!< New encryption mode pending. */

  bool_t            keyUpdFlag;         /*!< Flag for key update. */

  /* Peer device info */
  uint8_t           numSentVerInd;      /*!< Number of sent LL_VERSION_IND. */
  bool_t            remoteVerValid;     /*!< Peer version data valid. */
  lctrVerInd_t      remoteVer;          /*!< Peer version data. */
  bool_t            featExchFlag;       /*!< Flag for completed feature exchange. */
  uint64_t          peerFeatures;       /*!< Peer reported features. */
  uint64_t          usedFeatSet;        /*!< Used feature set. */
  uint8_t           peerSca;            /*!< Peer SCA. */

  int8_t            peerTxPower;        /*!< Peer reported txPower. */
  uint8_t           peerPwrLimits;      /*!< Peer power limits field. */
  uint8_t           peerApr[LL_PC_PHY_TOTAL];
                                        /*!< Acceptable reduction of power as calculated by the peer. */

  /* Channel reporting parameters. */
  bool_t            chanStatRptEnable;  /*!< Enable status of channel status reporting. */

  /* Data length */
  lctrDataLen_t     localDataPdu;       /*!< Local Data PDU parameters. */
  lctrDataLen_t     effDataPdu;         /*!< Effective Data PDU parameters. */
  uint16_t          localConnDurUsec;   /*!< Local connection event duration. */
  uint16_t          effConnDurUsec;     /*!< Effective connection event duration. */

  /* PHY */
  uint8_t           allPhys;            /*!< Default all PHYs. */
  uint8_t           txPhys;             /*!< Default transmitter PHYs. */
  uint8_t           rxPhys;             /*!< Default receiver PHYs. */
  uint8_t           txPhysPending;      /*!< Pending transmitter PHYs. */

  /* Peer minimum number of used channels */
  uint8_t           peerMinUsedChan[LL_MAX_PHYS];
                                        /*!< Peer minimum number of used channels for PHYs. */

  /* Periodic sync indication */
  bool_t            sendPerSync;        /*!< Send LL_PERIODIC_SYNC_IND flag. */
  uint8_t           perSyncSrc;         /*!< Periodic sync source. */
  uint16_t          perServiceData;     /*!< ID for periodic sync indication. */
  uint16_t          perSyncHandle;      /*!< Periodic sync handle. */

  /* PAST parameters. */
  uint8_t           syncMode;           /*!< Sync transfer mode. */
  uint16_t          syncSkip;           /*!< Sync skip for periodic adv sync transfer. */
  uint16_t          syncTimeout;        /*!< Sync timeout for periodic adv sync transfer. */

  /* LLCP */
  uint8_t           llcpState;          /*!< Current LLCP state. */
  uint8_t           encState;           /*!< Current encryption state. */
  uint8_t           pingState;          /*!< Current ping state. */
  uint8_t           connUpdState;       /*!< Connection update state. */
  uint8_t           ecuState;           /*!< Enhanced connection update state. */
  uint8_t           phyUpdState;        /*!< PHY update state. */
  uint8_t           cmnState;           /*!< Common LLCP state. */
  bool_t            peerReplyWaiting;   /*!< Peer waiting for reply. */
  bool_t            llcpInstantComp;    /*!< Procedure instant completed. */
  bool_t            termAckReqd;        /*!< Ack required for Rx'ed LL_TERMINATE_IND. */
  bool_t            ackAfterCtrlPdu;    /*!< Ack Tx'ed after last Control PDU Rx'ed. */
  bool_t            llcpIsOverridden;   /*!< Is the current procedure overridden by other procedure. */
  bool_t            isSlvReadySent;     /*!< True if slave ready to initiate startup LLCP procedure is sent. */
  bool_t            isFirstNonCtrlPdu;  /*!< True if first non-control PDU from master and slave. */
  bool_t            isSlvPhyUpdInstant; /*!< True if slave is in PHY update instant state. */
  uint8_t           llcpActiveProc;     /*!< Current procedure. */
  uint32_t          llcpNotifyMask;     /*!< Host notification mask. */
  uint32_t          llcpPendMask;       /*!< Pending LLCP procedures. */
  uint32_t          llcpIncompMask;     /*!< Incomplete LLCP procedures. */
  LlConnSpec_t      connUpdSpec;        /*!< Host connection update specification. */
  lctrConnUpdInd_t  connUpd;            /*!< Connection update parameters. */
  lctrConnParam_t   connParam;          /*!< Stored peer connection parameter request or response. */
  lctrChanMapInd_t  chanMapUpd;         /*!< Channel map parameters. */
  lctrPhy_t         phyReq;             /*!< Stored peer PHY request. */
  lctrPhyUpdInd_t   phyUpd;             /*!< PHY update parameters. */
  wsfTimer_t        tmrProcRsp;         /*!< Procedure response timer. */
  uint8_t           scaUpdAction;       /*!< Sca update action variable. */
  bool_t            readRemoteTxPower;  /*!< Currently reading remote txPower. */
  int8_t            scaMod;             /*!< Local sca override modifier. */
  uint8_t           reqErrCode;         /*!< LLCP error code. */

  /* Power Control */
  wsfTimer_t        tmrPowerCtrl;       /*!< Power control monitor timer. */
  int8_t            delta;              /*!< Power control delta storage. */
  bool_t            peerReqRecvd;       /*!< Peer request received. */
  uint8_t           reqPhy;             /*!< PHY of most recent power control request. */
  bool_t            powerRptLocal;      /*!< Currently reporting local power changes. */
  bool_t            powerRptRemote;     /*!< Currently reporting remote power changes. */
  uint8_t           powerMonitorScheme; /*!< Active power monitoring scheme. */
  uint8_t           monitoringState;    /*!< Current state of active power monitoring scheme. */
  bool_t            controllerInitRead; /*!< A controller initiated read command. */
  union
  {
    struct
    {
      int8_t highThreshold;             /*!< High RSSI threshold. */
      int8_t lowThreshold;              /*!< Low RSSI threshold. */
      uint8_t totalAccumulatedRssi;     /*!< Total accumulated RSSI samples in current sample slot. */
      uint32_t accumulatedRssi;         /*!< Absolute value of accumulated RSSI in current RSSI sample slot (RSSI can never be positive). */
      lctrRssiRunAvg_t rssiRunAvg;      /*!< RSSI running average. */
      uint8_t requestVal;               /*!< Value of increase/decrease in power to request. */
    } autoMonitor;                      /*!< Autonomous RSSI monitoring specific data. */

    struct
    {
      uint8_t highThreshold;            /*!< Path loss high threshold. */
      uint8_t highHysteresis;           /*!< Path loss high hysteresis. */
      uint8_t lowThreshold;             /*!< Path loss low threshold. */
      uint8_t lowHysteresis;            /*!< Path loss low hysteresis. */
      uint8_t minTimeSpent;             /*!< Minimum time spent to trigger an event. */
      uint8_t curTimeSpent;             /*!< Current time spent in a new path loss zone. */
      uint8_t curZone;                  /*!< Current path loss zone. */
      uint8_t newZone;                  /*!< New zone. */
      bool_t initialPathLossRead;       /*!< A power control request is required to start path loss monitoring. */
    } pathLoss;                         /*!< Path loss parameters. */
  } pclMonitorParam;                    /*!< Power control monitoring data. */

  /* CIS */
  uint16_t          llcpCisHandle;      /*!< CIS handle for the LLCP procedure. */
  lctrCheckTermFn_t checkCisTerm;       /*!< Pointer to the check CIS termination function. */
  lctrCheckCisEstAclFn_t  checkCisEstAcl;    /*!< Pointer to the check if CIS is established function. */
  uint8_t           cisRssiExtremeTimeSpent; /*!< CIS's current time spent in an extreme zone. */
  uint8_t           cisTotalAccumulatedRssi; /*!< Total accumulated RSSI samples in current sample slot. */
  uint32_t          cisAccumulatedRssi; /*!< Absolute value of accumulated RSSI in current RSSI sample slot (RSSI can never be positive). */
  lctrRssiRunAvg_t  cisRunAvg;          /*!< Running average of RSSI for CIS connection for power control. */

  /* Enhanced Connection Update */
  struct
  {
    /* Default values. */
    uint16_t        defSrMin;           /*!< Subrate minimum value. */
    uint16_t        defSrMax;           /*!< Subrate maximum value. */
    uint16_t        defMaxLatency;      /*!< Maximum latency. */
    uint16_t        defContNum;         /*!< Continuation number. */
    uint16_t        defSvt;             /*!< Supervision timeout. */

    /* Active values. */
    uint16_t        srFactor;           /*!< Active subrate factor. */
    uint16_t        srBaseEvent;        /*!< Subrate base event. */
    uint16_t        contNum;            /*!< Continuation number. */
    uint16_t        svt;                /*!< Supervision timeout. */
    uint16_t        contNumCtr;         /*!< Number of continuation number in subrate events. */

    struct
    {
      uint16_t      srFactor;           /*!< Active subrate factor. */
      uint16_t      srBaseEvent;        /*!< Subrate base event. */
      uint16_t      contNum;            /*!< Continuation number. */
      uint16_t      contNumCtr;         /*!< Number of continuation number in subrate events. */
      uint16_t      maxLatency;         /*!< Maximum latency. */
      uint16_t      svt;                /*!< Supervision timeout. */
    } pending;

    bool_t          isSubrateTransMode; /*!< True if central in subrate transition mode. */
  } ecu;                                /*!< Enhanced connection update parameters. */
} lctrConnCtx_t;

/*! \brief      Call signature of a cipher block handler. */
typedef void (*lctrCipherBlkHdlr_t)(PalCryptoEnc_t *pEnc, uint8_t id, uint8_t dir);

/*! \brief      Call signature of a packet encryption handler. */
typedef bool_t (*lctrPktEncHdlr_t)(PalCryptoEnc_t *pEnc, uint8_t *pHdr, uint8_t *pBuf, uint8_t *pMic);

/*! \brief      Call signature of a packet decryption handler. */
typedef bool_t (*lctrPktDecHdlr_t)(PalCryptoEnc_t *pEnc, uint8_t *pBuf);

/*! \brief      Call signature of a set packet count handler. */
typedef void (*lctrPktCntHdlr_t)(uint64_t pktCnt);

/*! \brief      Call signature of a LLCP state machine handler. */
typedef bool_t (*LctrLlcpHdlr_t)(lctrConnCtx_t *pCtx, uint8_t event);

/*! \brief      Call signature of a control PDU handler. */
typedef void (*lctrCtrlPduHdlr_t)(lctrConnCtx_t *pCtx, uint8_t *pBuf);

/*! \brief      Call signature of a Channel state machine handler. */
typedef uint8_t (*LctrChSelHdlr_t)(lctrConnCtx_t *pCtx, uint16_t numSkip);

/*! \brief      Call signature of an action handler. */
typedef void (*lctrLlcpEh_t)(lctrConnCtx_t *pCtx);

/*! \brief      Call signature of a power monitor function. */
typedef void (*lctrPcMonAct_t)(lctrConnCtx_t *pCtx);

/*! \brief      Call signature of power change indication handler. */
typedef void (*lctrPcPowInd_t)(lctrConnCtx_t *pCtx, uint8_t phy, int8_t delta, int8_t txPower, bool_t phyChange);

/*! \brief      Call signature of power report notification handler. */
typedef void (*lctrPcNotifyPwr_t)(lctrConnCtx_t *pCtx, uint8_t status, uint8_t reason, uint8_t phy, int8_t txPower, uint8_t limits, int8_t delta);

/*! \brief      Call signature of CIS pend disconnect function. */
typedef bool_t (*lctrPendCisDisc_t)(lctrConnCtx_t *pCtx);

/*! \brief      Call signature of CIS power monitoring function. */
typedef void   (*lctrCisServicePowerMonitor_t)(lctrConnCtx_t *pCtx);

/*! \brief      Call signature of calculate number of subrated connection events. */
typedef uint16_t (*lctrCalcSubrateConnEvents_t)(lctrConnCtx_t *pCtx, uint16_t numDataPdu);

/*! \brief      Call signature of check LLCP override. */
typedef bool_t (*lctrCheckLlcpOverride_t)(lctrConnCtx_t *pCtx);

/*! \brief      LLCP state machine handlers. */
enum
{
  LCTR_LLCP_SM_ENCRYPT,                 /*!< Encryption LLCP state machine. */
  LCTR_LLCP_SM_PING,                    /*!< Ping state machine. */
  LCTR_LLCP_SM_ENH_CONN_UPD,            /*!< Enhanced connection update state machine. */
  LCTR_LLCP_SM_CONN_UPD,                /*!< Connection update state machine. */
  LCTR_LLCP_SM_PHY_UPD,                 /*!< PHY update state machine. */
  LCTR_LLCP_SM_CIS_EST,                 /*!< CIS establishment state machine. */
  LCTR_LLCP_SM_CIS_TERM,                /*!< CIS termination state machine. */
  LCTR_LLCP_SM_PC,                      /*!< Power control state machine. */
  LCTR_LLCP_SM_CMN,                     /*!< Common LLCP state machine. */
  LCTR_LLCP_SM_TOTAL                    /*!< Total number of LLCP state machine. */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

extern lctrCipherBlkHdlr_t lctrInitCipherBlkHdlr;
extern lctrPktEncHdlr_t lctrPktEncryptHdlr;
extern lctrPktDecHdlr_t lctrPktDecryptHdlr;
extern lctrPktCntHdlr_t lctrSetEncryptPktCountHdlr;
extern lctrPktCntHdlr_t lctrSetDecryptPktCountHdlr;
extern lctrConnCtx_t *pLctrConnTbl;
extern lctrDataPdu_t lctrDataPdu;
extern lctrConnMsg_t *pLctrConnMsg;
extern const LctrVsHandlers_t *pLctrVsHdlrs;
extern lctrCtrlPduHdlr_t lctrCtrlPduHdlr;
extern LctrChSelHdlr_t lctrChSelHdlr[LCTR_CH_SEL_MAX];
extern lctrCheckCisEstCisFn_t  lctrCheckCisEstCisFn;
extern lctrLlcpEh_t lctrSendPerSyncFromScanFn;
extern lctrLlcpEh_t lctrSendPerSyncFromBcstFn;
extern lctrLlcpEh_t lctrStorePeriodicSyncTrsfFn;
extern lctrLlcpEh_t lctrSendPeriodicSyncIndFn;
extern lctrLlcpEh_t lctrReceivePeriodicSyncIndFn;
extern lctrPcMonAct_t lctrPathLossMonitorActFn;
extern lctrPcPowInd_t lctrSendPowerChangeIndCback;
extern lctrPcNotifyPwr_t lctrNotifyPowerReportIndCback;
extern lctrCisServicePowerMonitor_t lctrCisServicePowerMonitorFn;
extern lctrCalcSubrateConnEvents_t lctrCalcSubrateConnEventsFn;
extern lctrCheckLlcpOverride_t lctrSlvCheckEncOverridePhyUpdateFn;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Initialization */
void lctrConnDefaults(void);

/* Builder */
lctrConnCtx_t *lctrAllocConnCtx(void);
void lctrFreeConnCtx(lctrConnCtx_t *pCtx);
uint8_t lctrSelectNextDataChannel(lctrConnCtx_t *pCtx, uint16_t numSkip);
uint8_t lctrSelectNextChannel2(lctrConnCtx_t *pCtx, uint16_t numSkip);
void lctrBuildRemapTable(lctrConnCtx_t *pCtx);

/* Task event handler */
void lctrConnTxCompletedHandler(void);
void lctrConnRxPendingHandler(void);

/* Connection update actions */
void lctrStoreConnUpdateSpec(lctrConnCtx_t *pCtx);
void lctrSendConnUpdateInd(lctrConnCtx_t *pCtx);
void lctrStoreConnUpdate(lctrConnCtx_t *pCtx);
void lctrNotifyHostConnUpdateInd(lctrConnCtx_t *pCtx, uint8_t status);
void lctrActNotifyHostConnUpdSuccess(lctrConnCtx_t *pCtx);
void lctrActHostNegReply(lctrConnCtx_t *pCtx);
void lctrActPeerRejectConnParam(lctrConnCtx_t *pCtx);

/* Channel map actions */
void lctrStoreChanMapUpdate(lctrConnCtx_t *pCtx);
void lctrSendChanMapUpdateInd(lctrConnCtx_t *pCtx);
void lctrStoreChanMap(lctrConnCtx_t *pCtx);

/* Feature exchange actions */
void lctrSendFeatureReq(lctrConnCtx_t *pCtx);
void lctrSendFeatureRsp(lctrConnCtx_t *pCtx);
void lctrStoreUsedFeatures(lctrConnCtx_t *pCtx);

/* Version exchange actions */
void lctrSendVersionInd(lctrConnCtx_t *pCtx);
void lctrStoreRemoteVer(lctrConnCtx_t *pCtx);
void lctrNotifyHostReadRemoteVerCnf(lctrConnCtx_t *pCtx);

/* Terminate actions */
void lctrSendTerminateInd(lctrConnCtx_t *pCtx);
void lctrNotifyHostDisconnectInd(lctrConnCtx_t *pCtx);
void lctrStoreTerminateReason(lctrConnCtx_t *pCtx);
void lctrStoreDisconnectReason(lctrConnCtx_t *pCtx);
void lctrStoreConnFailEstablishTerminateReason(lctrConnCtx_t *pCtx);
void lctrStoreLlcpTimeoutTerminateReason(lctrConnCtx_t *pCtx);
void lctrStoreResetTerminateReason(lctrConnCtx_t *pCtx);
void lctrStoreInstantPassedTerminateReason(lctrConnCtx_t *pCtx);
void lctrStoreMicFailedTerminateReason(lctrConnCtx_t *pCtx);

/* Connection parameter actions */
void lctrStoreConnParamReq(lctrConnCtx_t *pCtx);
void lctrStoreConnParamRsp(lctrConnCtx_t *pCtx);
void lctrStoreConnParamSpec(lctrConnCtx_t *pCtx);
void lctrSendConnParamReq(lctrConnCtx_t *pCtx);
void lctrSendConnParamRsp(lctrConnCtx_t *pCtx);
void lctrNotifyHostConnParamInd(lctrConnCtx_t *pCtx);

/* Data length exchange actions */
void lctrStoreLocalDataLength(lctrConnCtx_t *pCtx);
void lctrSendDataLengthReq(lctrConnCtx_t *pCtx);
void lctrSendDataLengthRsp(lctrConnCtx_t *pCtx);
void lctrStoreRemoteDataLength(lctrConnCtx_t *pCtx);
void lctrNotifyHostDataLengthInd(lctrConnCtx_t *pCtx, uint8_t status);

/* Set minimum number of used channels actions */
void lctrSendSetMinUsedChanInd(lctrConnCtx_t *pCtx);
void lctrStoreSetMinUsedChan(lctrConnCtx_t *pCtx);

/* Send periodic sync indication actions */
void lctrActStorePeriodicSyncTrsf(lctrConnCtx_t *pCtx);
void lctrActSendPeriodicSyncInd(lctrConnCtx_t *pCtx);
void lctrActReceivePeriodicSyncInd(lctrConnCtx_t *pCtx);
void lctrStorePeriodicSyncTrsf(lctrConnCtx_t *pCtx);
void lctrSendPeriodicSyncInd(lctrConnCtx_t *pCtx);
void lctrReceivePeriodicSyncInd(lctrConnCtx_t *pCtx);
void lctrSendPerSyncFromScan(lctrConnCtx_t *pCtx);
void lctrSendPerSyncFromBcst(lctrConnCtx_t *pCtx);

/* Request peer SCA actions. */
void lctrStoreScaAction(lctrConnCtx_t *pCtx);
void lctrSendPeerScaReq(lctrConnCtx_t *pCtx);
void lctrSendPeerScaRsp(lctrConnCtx_t *pCtx);
void lctrStorePeerSca(lctrConnCtx_t *pCtx);
void lctrNotifyHostPeerScaCnf(lctrConnCtx_t *pCtx);

/* Channel status reporting actions. */
void lctrSendChannelStatusInd(lctrConnCtx_t *pCtx);
void lctrStoreChannelStatusInd(lctrConnCtx_t *pCtx);
void lctrSendChannelReportingInd(lctrConnCtx_t *pCtx);
void lctrStoreChannelReportingInd(lctrConnCtx_t *pCtx);

/* Unknown/Unsupported */
void lctrSendUnknownRsp(lctrConnCtx_t *pCtx);
void lctrSendRejectInd(lctrConnCtx_t *pCtx, uint8_t reason, bool_t forceRejectExtInd);

/* Tx data path */
uint16_t lctrTxFragLen(lctrConnCtx_t *pTx);
uint16_t lctrTxInitMem(uint8_t *pFreeMem, uint32_t freeMemSize);
uint8_t *lctrTxCtrlPduAlloc(uint8_t pduLen);
void lctrTxDataPduQueue(lctrConnCtx_t *pCtx, uint16_t fragLen, lctrAclHdr_t *pAclHdr, uint8_t *pAclBuf);
void lctrTxCtrlPduQueue(lctrConnCtx_t *pCtx, uint8_t *pBuf);
uint8_t lctrTxQueuePeek(lctrConnCtx_t *pCtx, PalBbBleTxBufDesc_t *bbDescs, bool_t *pMd);
bool_t lctrTxQueuePop(lctrConnCtx_t *pCtx);
void lctrTxQueuePopCleanup(lctrConnCtx_t *pCtx);
uint8_t lctrTxQueueClear(lctrConnCtx_t *pCtx);

/* Rx data path */
uint8_t *lctrRxPduAlloc(uint16_t maxRxLen);
void lctrRxPduFree(uint8_t *pBuf);
void lctrRxEnq(uint8_t *pBuf, uint16_t eventCounter, uint16_t connHandle);
uint8_t *lctrRxDeq(uint16_t *pConnHandle);
void lctrRxConnEnq(lctrConnCtx_t *pCtx, uint8_t *pBuf);
uint8_t *lctrRxConnDeqAcl(lctrConnCtx_t *pCtx);
uint8_t lctrRxConnClear(lctrConnCtx_t *pCtx);

/* LLCP */
void lctrStartLlcpTimer(lctrConnCtx_t *pCtx);
void lctrStopLlcpTimer(lctrConnCtx_t *pCtx);
void lctrStartPendingLlcp(lctrConnCtx_t *pCtx);

/* Data path pause */
void lctrPauseTxData(lctrConnCtx_t *pCtx);
void lctrUnpauseTxData(lctrConnCtx_t *pCtx);
void lctrCheckPauseComplete(lctrConnCtx_t *pCtx);
void lctrPauseRxData(lctrConnCtx_t *pCtx);
void lctrUnpauseRxData(lctrConnCtx_t *pCtx);

/* Packet times */
void lctrSetPacketTimeRestriction(lctrConnCtx_t *pCtx, uint8_t txPhys);
void lctrRemovePacketTimeRestriction(lctrConnCtx_t *pCtx);
uint16_t lctrCalcConnDurationUsec(lctrConnCtx_t *pCtx, const lctrDataLen_t *pDataLen);

/* Scheduler */
BbOpDesc_t *lctrConnResolveConflict(BbOpDesc_t *pNewOp, BbOpDesc_t *pExistOp);

/* ISR */
uint8_t *lctrProcessRxAck(lctrConnCtx_t *pCtx);
void lctrTxPduAck(lctrConnCtx_t *pCtx);
bool_t lctrProcessTxAck(lctrConnCtx_t *pCtx);
void lctrProcessTxAckCleanup(lctrConnCtx_t *pCtx);
uint16_t lctrSetupForTx(lctrConnCtx_t *pCtx, uint8_t rxStatus, bool_t reqTx);
void lctrRxPostProcessing(lctrConnCtx_t *pCtx, uint8_t *pRxBuf, uint8_t *pNextRxBuf, bool_t loadRxBuf);

/* Helper */
void lctrSendConnMsg(lctrConnCtx_t *pCtx, uint8_t event);
bool_t lctrExceededMaxDur(lctrConnCtx_t *pCtx, uint32_t ceStart, uint32_t pendDurUsec);
uint32_t lctrCalcPingPeriodMs(lctrConnCtx_t *pCtx, uint32_t authTimeoutMs);
uint8_t lctrComputeSca(void);
uint32_t lctrConnGetAnchorPoint(lctrConnCtx_t *pCtx, uint16_t ceCounter);
void lctrInitPhyTxPower(lctrConnCtx_t *pCtx);
uint8_t lctrGetPowerLimits(int8_t txPower);

/* Reservation */
uint32_t lctrGetConnRefTime(uint8_t connHandle, uint32_t *pDurUsec);

/* Channel map updating. */
uint8_t lctrConnChClassUpdate(uint64_t chanMap);

/*************************************************************************************************/
/*!
 *  \brief  Set flags for link termination.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrFlagLinkTerm(lctrConnCtx_t *pCtx)
{
  bool_t removeBod = (pCtx->state == LCTR_CONN_STATE_ESTABLISHED_READY);

  pCtx->state = LCTR_CONN_STATE_TERMINATING;    /* signals ISR to terminate link */
  pCtx->llcpState = LCTR_LLCP_STATE_IDLE;       /* signals ISR do not wait to send TERMINATE_IND */

  if (removeBod)
  {
    SchRemove(&pCtx->connBod);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Service the Control PDU ACK state after a successful reception.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrCheckControlPduAck(lctrConnCtx_t *pCtx)
{
  if (pCtx->rxHdr.llid == LL_LLID_CTRL_PDU)
  {
    /* Control PDU received; reset Tx ACK pending state. */
    pCtx->ackAfterCtrlPdu = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Service the Control PDU ACK state after a successful transmission.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrSetControlPduAck(lctrConnCtx_t *pCtx)
{
  /* Unconditionally set state upon Tx completion. */
  pCtx->ackAfterCtrlPdu = TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Set flags for link termination.
 *
 *  \param  pCtx    Connection context.
 *
 *  \return TRUE if connection is terminated, FALSE otherwise.
 */
/*************************************************************************************************/
static inline bool_t lctrCheckForLinkTerm(lctrConnCtx_t *pCtx)
{
  if (pCtx->state == LCTR_CONN_STATE_TERMINATING)
  {
    /* Peer device is LL_TERMINATE_IND initiator. */
    if (pCtx->termAckReqd)                  /* Tx Ack required after Rx of LL_TERMINATE_IND */
    {
      if (pCtx->ackAfterCtrlPdu)            /*     guarantee Ack Tx'ed */
      {
        return TRUE;
      }
    }
    /* Local device is LL_TERMINATE_IND initiator. */
    else if ((pCtx->llcpState == LCTR_LLCP_STATE_IDLE) || /* LL_TERMINATE_IND not pending */
             (pCtx->txArqQ.pHead == NULL))                /* guarantee LL_TERMINATE_IND is Ack'ed */
                                                          /*     i.e. "WsfQueueEmpty(&pCtx->txArqQ)" (optimized for ISR) */
    {
      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Increment the Tx/encrypt packet counter.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrIncPacketCounterTx(lctrConnCtx_t *pCtx)
{
  if (lctrSetEncryptPktCountHdlr)
  {
    PalCryptoEnc_t * const pEnc = &pCtx->bleData.chan.enc;

    if ((pEnc->enaEncrypt) &&
        (pEnc->nonceMode == PAL_BB_NONCE_MODE_PKT_CNTR))
    {
      pCtx->txPktCounter++;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Increment the Rx/decrypt packet counter.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrIncPacketCounterRx(lctrConnCtx_t *pCtx)
{
  if (lctrSetDecryptPktCountHdlr)
  {
    PalCryptoEnc_t * const pEnc = &pCtx->bleData.chan.enc;

    if ((pEnc->enaDecrypt) &&
        (pEnc->nonceMode == PAL_BB_NONCE_MODE_PKT_CNTR))
    {
      pCtx->rxPktCounter++;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the transmit packet counter value in the BB.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrSetBbPacketCounterTx(lctrConnCtx_t *pCtx)
{
  if (lctrSetEncryptPktCountHdlr)
  {
    PalCryptoEnc_t * const pEnc = &pCtx->bleData.chan.enc;

    if (!pEnc->enaEncrypt)
    {
      return;
    }

    switch (pEnc->nonceMode)
    {
      case PAL_BB_NONCE_MODE_PKT_CNTR:
        lctrSetEncryptPktCountHdlr(pCtx->txPktCounter);
        break;
      case PAL_BB_NONCE_MODE_EXT16_CNTR:
        lctrSetEncryptPktCountHdlr(pCtx->eventCounter);
        break;
      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the receive packet counter value in the BB.
 *
 *  \param  pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrSetBbPacketCounterRx(lctrConnCtx_t *pCtx)
{
  if (lctrSetDecryptPktCountHdlr)
  {
    PalCryptoEnc_t * const pEnc = &pCtx->bleData.chan.enc;

    if (!pEnc->enaDecrypt)
    {
      return;
    }

    switch (pEnc->nonceMode)
    {
      case PAL_BB_NONCE_MODE_PKT_CNTR:
        lctrSetDecryptPktCountHdlr(pCtx->rxPktCounter);
        break;
      case PAL_BB_NONCE_MODE_EXT16_CNTR:
        lctrSetDecryptPktCountHdlr(pCtx->eventCounter);
        break;
      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check if procedure is active, if not pend the procedure.
 *
 *  \param  pCtx        Connection context.
 *  \param  proc        Procedure ID to check/pend.
 *
 *  \return TRUE if active procedure, FALSE if pended.
 */
/*************************************************************************************************/
static inline bool_t lctrCheckActiveOrPend(lctrConnCtx_t *pCtx, uint8_t proc)
{
  if (pCtx->llcpActiveProc == proc)
  {
    return TRUE;
  }
  else
  {
    pCtx->llcpPendMask |= 1 << proc;
    return FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief      Store connection timeout termination reason.
 *
 *  \param      pCtx    Connection context.
 */
/*************************************************************************************************/
static inline void lctrStoreConnTimeoutTerminateReason(lctrConnCtx_t *pCtx)
{
  if (pCtx->termReason == LL_ERROR_CODE_CONN_FAILED_TO_ESTABLISH)
  {
    pCtx->termReason = LL_ERROR_CODE_CONN_TIMEOUT;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Increment available Tx data buffers.
 */
/*************************************************************************************************/
static inline void lctrDataTxIncAvailBuf(void)
{
  WSF_CS_INIT(cs);

  WSF_CS_ENTER(cs);
  lmgrConnCb.availTxBuf++;
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Decrement available Tx data buffers.
 */
/*************************************************************************************************/
static inline void lctrDataTxDecAvailBuf(void)
{
  WSF_CS_INIT(cs);

  WSF_CS_ENTER(cs);
  lmgrConnCb.availTxBuf--;
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief  Increment available Rx data buffers.
*
 *  \param  numBufs     Number of completed packets.
  */
/*************************************************************************************************/
static inline void lctrDataRxIncAvailBuf(uint8_t numBufs)
{
  WSF_CS_INIT(cs);

  WSF_CS_ENTER(cs);
  lmgrConnCb.availRxBuf += numBufs;
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \brief      Get connection operational mode flags.
 *
 *  \param      pCtx    Connection context to get the flag.
 *  \param      flag    Flags.
 *
 *  \return     Status error code.
 *
 *  Set mode flags governing LL operations of a given connection.
 */
/*************************************************************************************************/
static inline bool_t lctrGetConnOpFlag(lctrConnCtx_t *pCtx, uint32_t flag)
{
  return (pCtx->opModeFlags & flag) ? TRUE : FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief      Get LLCP procedure ID.
 *
 *  \param      event   Event.
 *
 *  \return     LLCP procedure event ID.
 */
/*************************************************************************************************/
static inline uint8_t lctrGetProcId(uint8_t event)
{
  switch (event)
  {
    case LCTR_CONN_MSG_API_CHAN_MAP_UPDATE:
      return LCTR_PROC_CMN_CH_MAP_UPD;

    case LCTR_CONN_MSG_API_REMOTE_FEATURE:
      return LCTR_PROC_CMN_FEAT_EXCH;

    case LCTR_CONN_MSG_API_SET_MIN_USED_CHAN:
      return LCTR_PROC_CMN_SET_MIN_USED_CHAN;

    case LCTR_CONN_MSG_API_REMOTE_VERSION:
      return LCTR_PROC_CMN_VER_EXCH;

    case LCTR_CONN_MSG_API_DISCONNECT:
      return LCTR_PROC_CMN_TERM;

    case LCTR_CONN_MSG_API_DATA_LEN_CHANGE:
      return LCTR_PROC_CMN_DATA_LEN_UPD;

    case LCTR_CONN_MSG_API_PER_ADV_SYNC_TRSF:
      return LCTR_PROC_CMN_PER_ADV_SYNC_TRSF;

    case LCTR_CONN_MSG_API_REQ_PEER_SCA:
      return LCTR_PROC_CMN_REQ_PEER_SCA;

    case LCTR_CONN_MSG_API_CONN_UPDATE:
      return LCTR_PROC_CONN_UPD;

    case LCTR_CONN_MSG_API_PHY_UPDATE:
      return LCTR_PROC_PHY_UPD;

    case LCTR_CONN_MSG_API_START_ENC:
      return LCTR_PROC_ENCRYPT;

    default:
      return LCTR_PROC_INVALID;
  }
}

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_CONN_H */
