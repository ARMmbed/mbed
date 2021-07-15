/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller data channel packet interface file.
 *
 *  Copyright (c) 2013-2018 Arm Ltd. All Rights Reserved.
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

#ifndef LCTR_PDU_ISO_H
#define LCTR_PDU_ISO_H

#include "wsf_types.h"
#include "wsf_queue.h"
#include "bb_ble_api.h"
#include "ll_defs.h"
#include "ll_api.h"

#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief      Get length of ISO Header. */
#define LCTR_GET_ISO_DATA_HDR_LEN(pIsoHdr)      (HCI_ISO_HDR_LEN + (((pIsoHdr)->tsFlag) ? HCI_ISO_DL_MAX_LEN : HCI_ISO_DL_MIN_LEN))

/**************************************************************************************************
  Constants
**************************************************************************************************/

/*! \brief      Packet boundary flags. */
typedef enum
{
  LCTR_PB_FIRST     = 0,                /*!< Data is the first fragment of a fragmented SDU. */
  LCTR_PB_CONT      = 1,                /*!< Data is a continuation fragment of a fragmented SDU. */
  LCTR_PB_COMP      = 2,                /*!< Data is a complete SDU. */
  LCTR_PB_LAST      = 3                 /*!< Data is the last fragment of a fragmented SDU. */
} lctrPktBoundary_t;

/*! \brief      Packet status flags. */
typedef enum
{
  LCTR_PS_VALID     = 0,                /*!< Data is received correctly. */
  LCTR_PS_INVALID   = 1,                /*!< Data with possible errors. */
  LCTR_PS_LOST      = 2,                /*!< Data with lost data. */
} lctrPktStatus_t;

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief      ISO header. */
typedef struct
{
  /* ISO header */
  uint16_t          handle;             /*!< CIS or BIS handle. */
  lctrPktBoundary_t pb:8;               /*!< Packet boundary flag. */
  bool_t            tsFlag;             /*!< Timestamp flag. */
  uint16_t          len;                /*!< Data length. */

  /* ISO Data Load */
  uint32_t          ts;                 /*!< Timestamp. */
  uint16_t          pktSn;              /*!< Packet sequence number. */
  uint16_t          sduLen;             /*!< SDU length. */
  lctrPktStatus_t   ps:8;               /*!< Packet status. */
  uint8_t           *pSdu;              /*!< First byte of ISO SDU. */
} lctrIsoHdr_t;

/*! \brief      ISO SDU descriptor. */
typedef struct
{
  lctrIsoHdr_t      hdr;                /*!< ISO header. */
  uint32_t          ts;                 /*!< Timestamp. */
  uint8_t           data;               /*!< Data. */
} lctrSduDesc_t;

/*! \brief      ISO CIS PDU header. */
typedef struct
{
  uint8_t           llid;               /*!< PDU type. */
  uint8_t           nesn;               /*!< Next Expected Sequence Number. */
  uint8_t           sn;                 /*!< Sequence Number. */
  uint8_t           cie;                /*!< TRUE if stop next subevent. */
  uint8_t           np;                 /*!< PDU type, whether a NULL PDU. */
  uint8_t           len;                /*!< Payload length. */
} lctrCisDataPduHdr_t;

/*! \brief      ISO BIS PDU header. */
typedef struct
{
  LlIsoLlid_t       llid:8;             /*!< PDU type. */
  uint8_t           cssn;               /*!< Control Subevent Sequence Number. */
  bool_t            cstf;               /*!< Control Subevent Submission Flag. */
  uint8_t           len;                /*!< Payload length. */
} lctrBisDataPduHdr_t;

/*! \brief      ISOAL Segmentation Header. */
typedef struct
{
  bool_t            sc;                 /*!< Segment continuation flag. */
  bool_t            cmplt;              /*!< Segment complete flag. */
  uint8_t           len;                /*!< Payload length. */
  uint32_t          toffs;              /*!< Time offset. */
} lctrIsoSegHdr_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* Pack */
uint8_t lctrIsoPackHdr(uint8_t *pBuf, const lctrIsoHdr_t *pHdr);
uint8_t lctrIsoPackSegHdr(uint8_t *pBuf, const lctrIsoSegHdr_t *pSegHdr);
uint8_t lctrIsoUnpackSegHdr(lctrIsoSegHdr_t *pHdr, uint8_t *pBuf);
uint8_t lctrCisPackDataPduHdr(uint8_t *pBuf, const lctrCisDataPduHdr_t *pHdr);
uint8_t lctrBisPackDataPduHdr(uint8_t *pBuf, const lctrBisDataPduHdr_t *pHdr);
uint8_t lctrBisPackBigChannelMapInd(uint8_t *pBuf, uint64_t chanMap, uint16_t instance);
uint8_t lctrBisPackBigTerminateInd(uint8_t *pBuf, uint8_t reason, uint16_t instance);

/* Unpack */
uint8_t lctrIsoUnpackHdr(lctrIsoHdr_t *pHdr, uint8_t *pBuf);
uint8_t lctrCisUnpackDataPduHdr(lctrCisDataPduHdr_t *pHdr, const uint8_t *pBuf);
uint8_t lctrBisUnpackDataPduHdr(lctrBisDataPduHdr_t *pHdr, const uint8_t *pBuf);
uint8_t lctrBisUnpackBigChannelMapInd(uint64_t *pChanMap, uint16_t *pInstance, const uint8_t *pBuf);
uint8_t lctrBisUnpackBigTerminateInd(uint8_t *pReason, uint16_t *pInstance, const uint8_t *pBuf);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_PDU_ISO_H */
