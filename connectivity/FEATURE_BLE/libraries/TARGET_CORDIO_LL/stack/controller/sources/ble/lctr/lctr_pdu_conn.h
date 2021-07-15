/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller data channel packet interface file.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#ifndef LCTR_PDU_CONN_SLAVE_H
#define LCTR_PDU_CONN_SLAVE_H

#include "wsf_types.h"
#include "bb_ble_api.h"
#include "ll_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Constants
**************************************************************************************************/

#define LCTR_OFFSET_COUNT         6       /*!< Number of offsets in a connection parameter request. */

/*! \brief      Packet boundary flags. */
enum
{
  LCTR_PB_START_NON_AUTO_FLUSH  = 0,    /*!< Start of a non-automatically flushable L2CAP PDU. */
  LCTR_PB_CONT_FRAG             = 1,    /*!< Continuing fragment of Higher Layer Message. */
  LCTR_PB_START_AUTO_FLUSH      = 2,    /*!< Start of an automatically flushable L2CAP PDU. */
  /* N.B. next two enumerations intentionally use identical values. */
  LCTR_PB_COMPLETE_AUTO_FLUSH   = 3,    /*!< A complete L2CAP PDU automatically flushable (not allowed for LE-U). */
  LCTR_PB_VS_DATA               = 3     /*!< Vendor specific data. */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      ACL header. */
typedef struct
{
  uint16_t          connHandle;         /*!< Connection handle. */
  uint8_t           pktBound;           /*!< Packet boundary flag. */
  uint16_t          len;                /*!< Data length. */
} lctrAclHdr_t;

/*! \brief      Data channel PDU header. */
typedef struct
{
  uint8_t           llid;               /*!< PDU type. */
  uint8_t           nesn;               /*!< Next Expected Sequence Number. */
  uint8_t           sn;                 /*!< Sequence Number. */
  uint8_t           md;                 /*!< More Data. */
  uint8_t           len;                /*!< Payload length. */
} lctrDataPduHdr_t;

/*! \brief      Connection update indication PDU. */
typedef struct
{
  uint8_t           txWinSize;          /*!< transmitWindowSize value. */
  uint16_t          txWinOffset;        /*!< transmitWindowOffset value. */
  uint16_t          interval;           /*!< connInterval value. */
  uint16_t          latency;            /*!< connSlaveLatency value. */
  uint16_t          timeout;            /*!< connSupervisionTimeout value. */
  uint16_t          instant;            /*!< connInstant value. */
} lctrConnUpdInd_t;

/*! \brief      Connection update indication PDU. */
typedef struct
{
  uint64_t          chanMask;           /*!< Channel mask. */
  uint16_t          instant;            /*!< connInstant value. */
} lctrChanMapInd_t;

/*! \brief      Terminate indication control PDU. */
typedef struct
{
  uint8_t           errorCode;          /*!< Error code. */
} lctrTermInd_t;

/*! \brief      Encryption request control PDU. */
typedef struct
{
  uint8_t           rand[LL_RAND_LEN];  /*!< Random number. */
  uint16_t          ediv;               /*!< Encrypted diversifier. */
  uint8_t           skd_m[LL_SKD_LEN/2];/*!< Master's session key diversifier. */
  uint8_t           iv_m[LL_IV_LEN/2];  /*!< Master's initialization vector. */
} lctrEncReq_t;

/*! \brief      Encryption response control PDU. */
typedef struct
{
  uint8_t           skd_s[LL_SKD_LEN/2];/*!< Slave's session key diversifier. */
  uint8_t           iv_s[LL_IV_LEN/2];  /*!< Slave's initialization vector. */
} lctrEncRsp_t;

/*! \brief      Unknown response control PDU. */
typedef struct
{
  uint8_t           unknownType;        /*!< Unknown type. */
} lctrUnknownRsp_t;

/*! \brief      Feature request or response control PDU. */
typedef struct
{
  uint64_t          featSet;            /*!< Feature set. */
} lctrFeat_t;

/*! \brief      Connection parameter request/response values. */
typedef struct
{
  uint16_t          connIntervalMin;    /*!< Minimum connection interval. */
  uint16_t          connIntervalMax;    /*!< Maximum connection interval. */
  uint16_t          connLatency;        /*!< Connection latency. */
  uint16_t          supTimeout;         /*!< Supervision timeout. */
  uint8_t           prefPeriod;         /*!< Preferred periodicity. */
  uint16_t          refConnEvtCnt;      /*!< Reference connection event count. */
  uint16_t          offset[LCTR_OFFSET_COUNT];      /*!< Anchor point offset from \a refConnEvtCnt, first preference. */
} lctrConnParam_t;

/*! \brief      Version indication control PDU. */
typedef struct
{
  uint8_t           versNr;             /*!< Bluetooth controller specification version. */
  uint16_t          compId;             /*!< Company identifier. */
  uint16_t          subVersNr;          /*!< Sub-Version number. */
} lctrVerInd_t;

/*! \brief      Reject indication control PDU. */
typedef struct
{
  uint8_t           opcode;             /*!< Reject opcode. */
  uint8_t           reason;             /*!< Reject reason code. */
} lctrRejInd_t;

/*! \brief      Data length request or response PDU. */
typedef struct
{
  uint16_t          maxRxLen;           /*!< Maximum receive length. */
  uint16_t          maxRxTime;          /*!< Maximum receive time. */
  uint16_t          maxTxLen;           /*!< Maximum transmit length. */
  uint16_t          maxTxTime;          /*!< Maximum transmit time. */
} lctrDataLen_t;

/*! \brief      PHY request or response PDU. */
typedef struct
{
  uint8_t           txPhys;             /*!< Transmitter PHYs. */
  uint8_t           rxPhys;             /*!< Receiver PHYs. */
} lctrPhy_t;

/*! \brief      PHY update indication PDU. */
typedef struct
{
  uint8_t           masterToSlavePhy;   /*!< Master-to-slave PHY. */
  uint8_t           slaveToMasterPhy;   /*!< Slave-to-master PHY. */
  uint16_t          instant;            /*!< Instant. */
} lctrPhyUpdInd_t;

/*! \brief      Minimum number of used channels indication PDU. */
typedef struct
{
  uint8_t           phys;               /*!< Bitmask for the affected PHYs. */
  uint8_t           minUsedChan;        /*!< Minimum number of used channels. */
} lctrMinUsedChanInd_t;

/*! \brief      Periodic sync indication PDU. */
typedef struct
{
  uint16_t          id;                 /*!< identifier provided by the host. */
  uint8_t           syncInfo[LL_SYNC_INFO_LEN];   /*!< Syncinfo field. */
  uint16_t          ceCounter;          /*!< Reference connection event counter. */
  uint16_t          lastPECounter;      /*!< paEventCounter applying to AUX_SYNC_IND PDU. */
  uint8_t           sid;                /*!< Advertising SID subfield. */
  uint8_t           aType;              /*!< Address type of periodic advertising. */
  uint8_t           sca;                /*!< Sleep clock accuracy of the device sending this PDU. */
  uint8_t           phy;                /*!< PHY used by the periodic advertising. */
  uint64_t          advA;               /*!< Advertiser's address in the periodic advertising. */
  uint16_t          syncConnEvtCounter; /*!< Connection event counter when the contents of the PDU is determined. */
} lctrPerSyncInd_t;

/*! \brief      Peer SCA response PDU. */
typedef struct
{
  uint8_t           sca;                /*!< Peer SCA. */
} lctrPeerSca_t;

/*! \brief      CIS request PDU. */
typedef struct
{
  uint8_t           cigId;              /*!< CIG identifier. */
  uint8_t           cisId;              /*!< CIS identifier. */
  uint8_t           phyMToS;            /*!< Master to slave PHY. */
  uint8_t           phySToM;            /*!< Slave to Master PHY. */
  uint8_t           framing;            /*!< PDU framing type. */
  uint16_t          sduSizeMToS;        /*!< Maximum SDU size from the master Host. */
  uint16_t          sduSizeSToM;        /*!< Maximum SDU size from the slave Host. */
  uint32_t          sduIntervalMToS;    /*!< Time interval between the start of consecutive SDUs from the master Host  */
  uint32_t          sduIntervalSToM;    /*!< Time interval between the start of consecutive SDUs from the master Host  */
  uint16_t          plMToS;             /*!< Master to slave payload. */
  uint16_t          plSToM;             /*!< Slave to master payload. */
  uint8_t           nse;                /*!< Number of subevent. */
  uint32_t          subIntervUsec;      /*!< Contain the time between the start of a subevent and the start of the next subevent, 24 significant bits. */
  uint8_t           bnMToS;             /*!< Master to slave burst number, 4 significant bits. */
  uint8_t           bnSToM;             /*!< Slave to master burst number, 4 significant bits. */
  uint8_t           ftMToS;             /*!< Master to slave flush time. */
  uint8_t           ftSToM;             /*!< Slave to master flush time. */
  uint16_t          isoInterval;        /*!< Contain the time between two CIS anchor points in 1.25msec unit. */
  uint32_t          cisOffMinUsec;      /*!< Contain the minimum time between the CE and the first CIS anchor point. */
  uint32_t          cisOffMaxUsec;      /*!< Contain the maximum time between the CE and the first CIS anchor point. */
  uint16_t          ceRef;              /*!< Contain the reference CE where offsets are applied. */
} lctrCisReq_t;

/*! \brief      CIS response PDU. */
typedef struct
{
  uint32_t          cisOffMinUsec;      /*!< Contain the minimum time between the CE and the first CIS anchor point. */
  uint32_t          cisOffMaxUsec;      /*!< Contain the maximum time between the CE and the first CIS anchor point. */
  uint16_t          ceRef;              /*!< Contain the reference CE where offsets are applied. */
} lctrCisRsp_t;

/*! \brief      CIS indication PDU. */
typedef struct
{
  uint32_t          accessAddr;         /*!< Contain the access address of the CIS. */
  uint32_t          cisOffUsec;         /*!< Contain the time from the start of the referenced CE to the first CIS anchor point. */
  uint32_t          cigSyncDelayUsec;   /*!< CIG synchronization delay in usec. */
  uint32_t          cisSyncDelayUsec;   /*!< CIG synchronization delay in usec. */
  uint16_t          ceRef;              /*!< Contain the reference CE where offsets are applied. */
} lctrCisInd_t;

/*! \brief      CIS terminate PDU. */
typedef struct
{
  uint8_t           cigId;              /*!< CIG identifier. */
  uint8_t           cisId;              /*!< CIS identifier. */
  uint8_t           reason;             /*!< Reason for termination. */
} lctrCisTermInd_t;

/*! \brief      Subrate request PDU. */
typedef struct
{
  uint16_t          srMin;              /*!< Subrate minimum value. */
  uint16_t          srMax;              /*!< Subrate maximum value. */
  uint16_t          maxLatency;         /*!< Maximum latency. */
  uint16_t          contNum;            /*!< Continuation number. */
  uint16_t          svt;                /*!< Supervision timeout in 10ms units. */
} lctrSubrateReq_t;

/*! \brief      Subrate indication PDU. */
typedef struct
{
  uint16_t          srFactor;           /*!< Subrate factor. */
  uint16_t          srBaseEvent;        /*!< Subrate base event. */
  uint16_t          latency;            /*!< Latency. */
  uint16_t          contNum;            /*!< Continuation number. */
  uint16_t          svt;                /*!< Supervision timeout in 10ms units. */
} lctrSubrateInd_t;

/*! \brief      Power control request PDU. */
typedef struct
{
  uint8_t           phy;                /*!< PHY. */
  int8_t            delta;              /*!< Requested delta. */
  int8_t            txPower;            /*!< Local transmit power. */
} lctrPwrCtrlReq_t;

/*! \brief      Power control RSP PDU. */
typedef struct
{
  uint8_t           limits;             /*!< Limits field. */
  int8_t            delta;              /*!< Change in power. */
  int8_t            txPower;            /*!< Local txPower. */
  uint8_t           apr;                /*!< Acceptable power reduction. */
} lctrPwrCtrlRsp_t;

/*! \brief      Power change indication PDU. */
typedef struct
{
  uint8_t           phy;                /*!< PHY. */
  uint8_t           limits;             /*!< Limits field. */
  int8_t            delta;              /*!< Device txPower change. */
  int8_t            txPower;            /*!< Local txPower. */
} lctrPwrChngInd_t;

/*! \brief      Channel reporting indication PDU. */
typedef struct
{
  uint8_t           enable;             /*!< Enable. */
  uint8_t           minSpacing;         /*!< Minimum spacing. */
  uint8_t           maxDelay;           /*!< Maximum delay. */
} lctrChanRptInd_t;

/*! \brief      Channel status indication PDU. */
typedef struct
{
  uint8_t           chanStatusMap[LL_CH_STATUS_LEN - 1];
                                        /*!< Map of current peer channel status. */
} lctrChanStatusInd_t;

/*! \brief      Data channel control PDU. */
typedef struct
{
  lctrDataPduHdr_t  hdr;                /*!< Unpacked PDU header. */
  uint8_t opcode;                       /*!< Control PDU opcode. */

  union
  {
    lctrConnUpdInd_t connUpdInd;        /*!< Connection update indication. */
    lctrChanMapInd_t chanMapInd;        /*!< Channel map request. */
    lctrTermInd_t    termInd;           /*!< Terminate indication. */
    lctrEncReq_t     encReq;            /*!< Encryption request. */
    lctrEncRsp_t     encRsp;            /*!< Encryption response. */
    lctrUnknownRsp_t unknownRsp;        /*!< Unknown response. */
    lctrFeat_t       featReqRsp;        /*!< Feature request or response or slave feature request. */
    lctrVerInd_t     verInd;            /*!< Version indication. */
    lctrConnParam_t  connParamReqRsp;   /*!< Connection parameter request or response. */
    lctrRejInd_t     rejInd;            /*!< Reject indication. */
    lctrDataLen_t    lenReq;            /*!< Data length request. */
    lctrDataLen_t    lenRsp;            /*!< Data length response. */
    lctrPhy_t        phyReq;            /*!< PHY request. */
    lctrPhy_t        phyRsp;            /*!< PHY response. */
    lctrPhyUpdInd_t  phyUpdInd;         /*!< PHY update indication. */
    lctrMinUsedChanInd_t minUsedChanInd;/*!< Minimum number of used channels indication. */
    lctrPerSyncInd_t perSyncInd;        /*!< Periodic sync indication. */
    lctrPeerSca_t    peerSca;           /*!< Peer SCA request/response. */
    lctrCisReq_t     cisReq;            /*!< CIS request. */
    lctrCisRsp_t     cisRsp;            /*!< CIS response. */
    lctrCisInd_t     cisInd;            /*!< CIS indication. */
    lctrCisTermInd_t cisTerm;           /*!< CIS terminate indication. */
    lctrPwrCtrlReq_t pwrCtrlReq;        /*!< Power control request. */
    lctrPwrCtrlRsp_t pwrCtrlRsp;        /*!< Power control response. */
    lctrPwrChngInd_t pwrChngInd;        /*!< Power change indication. */
    lctrChanRptInd_t chanRptInd;        /*!< Channel report indication. */
    lctrChanStatusInd_t chanStatusInd;
                                        /*!< Channel status indication. */
    lctrSubrateReq_t subrateReq;        /*!< Subrate request. */
    lctrSubrateInd_t subrateInd;        /*!< Subrate indication. */
  } pld;                                /*!< Unpacked PDU payload. */
} lctrDataPdu_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Pack */
uint8_t lctrPackAclHdr(uint8_t *pBuf, const lctrAclHdr_t *pHdr);
uint8_t lctrPackDataPduHdr(uint8_t *pBuf, const lctrDataPduHdr_t *pHdr);
uint8_t lctrPackConnUpdInd(uint8_t *pBuf, const lctrConnUpdInd_t *pPdu);

/* Unpack */
uint8_t lctrUnpackAclHdr(lctrAclHdr_t *pHdr, const uint8_t *pBuf);
uint8_t lctrUnpackDataPduHdr(lctrDataPduHdr_t *pHdr, const uint8_t *pBuf);
uint8_t lctrUnpackConnUpdateIndPdu(lctrConnUpdInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackChanMapIndPdu(lctrChanMapInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackTerminateIndPdu(lctrTermInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackUnknownRspPdu(lctrUnknownRsp_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackFeaturePdu(lctrFeat_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackRejectIndPdu(uint8_t *pReason, const uint8_t *pBuf);
uint8_t lctrUnpackVersionIndPdu(lctrVerInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackConnParamPdu(lctrConnParam_t *pConnParam, const uint8_t *pBuf);
uint8_t lctrUnpackRejectExtIndPdu(lctrRejInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackDataLengthPdu(lctrDataLen_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackPhyPdu(lctrPhy_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackPhyUpdateIndPdu(lctrPhyUpdInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackMinUsedChanIndPdu(lctrMinUsedChanInd_t *pPdu, const uint8_t *pBuf);
uint8_t lctrUnpackPerSyncIndPdu(lctrPerSyncInd_t *pPdu, const uint8_t *pBuf);

/* Decode */
uint8_t lctrDecodeCtrlPdu(lctrDataPdu_t *pPdu, const uint8_t *pBuf, uint8_t role);
uint8_t lctrDecodeEncPdu(lctrDataPdu_t *pPdu, const uint8_t *pBuf, uint8_t role);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_PDU_CONN_SLAVE_H */
