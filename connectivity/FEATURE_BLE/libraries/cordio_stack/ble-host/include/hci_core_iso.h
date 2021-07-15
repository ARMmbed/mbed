/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI core ISO interfaces.
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
#ifndef HCI_CORE_ISO_H
#define HCI_CORE_ISO_H

#include "hci_core_ps.h"
#include "wsf_queue.h"
#include "wsf_os.h"
#include "hci_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief  ISO Function table indicies. */
enum
{
  HCI_CORE_CIS,                     /*!< \brief  HCI core for CIS. */
  HCI_CORE_BIS,                     /*!< \brief  HCI core for BIS. */

  HCI_CORE_TOTAL_ISO_TYPES          /*!< \brief  Total number of HCI core types. */
};

/*! \brief  Get ISO header length. */
#define HCI_GET_ISO_DATA_HDR_LEN(pIsoHdr) (HCI_ISO_HDR_LEN + (((pIsoHdr)->tsFlag) ? HCI_ISO_DL_MAX_LEN : HCI_ISO_DL_MIN_LEN)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief  Data structure for unpacked ISO data packet. */
typedef struct
{
  uint8_t             *pData;       /*!< Beginning of data buffer. */

  /* ISO Header. */
  uint16_t            handle;       /*!< ISO Handle. */
  uint8_t             pb;           /*!< Packet boundary. */
  bool_t              tsFlag;       /*!< Timestamp flag. */
  uint16_t            isoLen;       /*!< ISO data load length. */

  /* Data load. */
  uint32_t            ts;           /*!< Timestamp (If exists). */
  uint16_t            sn;           /*!< Packet sequence number. */
  uint16_t            sduLen;       /*!< SDU Length. */
  uint8_t             ps;           /*!< Packet status. */
  uint8_t             *pIsoSdu;     /*!< ISO SDU buffer. */
} hciIsoDataPkt_t;

/*! \brief  Data structure for transmit control. */
typedef struct
{
  uint8_t             outBufs;      /*!< \brief Currently outgoing buffers. */
  bool_t              fragmenting;  /*!< \brief Currently fragmenting. */
  hciIsoDataPkt_t     *pCurPkt;     /*!< \brief Currently transmitting ISO packet if fragmenting. */

  wsfQueue_t          sduQueue;     /*!< \brief Queue of SDUs to be transmitted. */
} hciIsoTxCb_t;

/*! \brief  CIG structure to account for CIS. */
typedef struct
{
  uint8_t            cigId;         /*!< \brief CIG ID. */
  uint8_t            cisCount;      /*!< \brief Number of CIS handles in the CIG. */
  uint8_t            activeCis;     /*!< \brief Current active CISes on this CIG. */
  uint8_t            role;          /*!< \brief Role of CIG. */
  uint16_t           cisHandles[HCI_MAX_CIS_COUNT]; /*!< \brief Associated CIS handles. */
} hciCoreCig_t;

/*! \brief  CIS control block. */
typedef struct
{
  uint16_t            handle;       /*!< \brief Connection handle */
  hciIsoTxCb_t        txCb;         /*!< \brief Transmit control block. */
  hciCoreCig_t        *pCig;        /*!< \brief Associated CIG. */
} hciCoreCis_t;

/*! \brief  BIG structure to account for BIS. */
typedef struct
{
  uint8_t             bigId;        /*!< \brief BIG identifier. */
  uint8_t             bigCount;     /*!< \brief Number of BIG handles in the BIG. */
  uint8_t             activeBis;    /*!< \brief Number of active BISes on this BIG. */
  uint8_t             role;         /*!< \brief Role of BIG. */
  uint16_t            bigHandles[HCI_MAX_BIS_COUNT]; /*!< \brief Associated BIS handles. */
} hciCoreBig_t;

/*! \brief  BIS structure for packet handling. */
typedef struct
{
  uint16_t            handle;       /*!< \brief Connection handle. */
  hciCoreBig_t        *pBig;        /*!< \brief Associated BIG. */

  /*! \brief role specific data. */
  union
  {
    struct
    {
      hciIsoTxCb_t    txCb;         /*!< \brief Transmit control block. */
    } slv;                          /*!< \brief Slave specific role data. */
    /* struct mst; */               /*!< \brief Master specific role data. */
  } roleData;
} hciCoreBis_t;

/*! \brief  ISO Rx reassembly function signature. */
typedef uint8_t *(*hciIsoReassembleCb_t)(uint8_t *pData);

/*! \brief  ISO Tx complete call back function signature. */
typedef void (*hciIsoTxReadyCb_t)(uint8_t bufs);

/*! \brief  ISO open group function signature. */
typedef void (*hciIsoGroupOpenCb_t)(uint8_t id, uint8_t count, uint16_t *pHandles, uint8_t role);

/*! \brief  ISO close group function signature. */
typedef void (*hciIsoGroupCloseCb_t)(uint8_t id);

/*! \brief  ISO open function signature. */
typedef void (*hciCoreIsoConnOpenClose_t)(uint16_t handle);

/*! \brief  ISO get control block function signature. */
typedef void *(*hciCoreIsoFindByHandle_t)(uint16_t handle);

/* ISO connection open and close, and get a control block function interface */
typedef struct
{
  hciCoreIsoConnOpenClose_t connClose;
  hciCoreIsoConnOpenClose_t connOpen;
  hciCoreIsoFindByHandle_t  findByHandle;
  hciIsoGroupOpenCb_t       groupOpen;
  hciIsoGroupCloseCb_t      groupClose;
} isoFcnIf_t;

/* \brief   ISO component function interface table. */
extern isoFcnIf_t *isoFcnIfTbl[HCI_CORE_TOTAL_ISO_TYPES];

/*! \brief CIG control blocks. */
extern hciCoreCig_t hciCoreCig[HCI_MAX_CIG_COUNT];

/*! \brief  CIS control blocks. */
extern hciCoreCis_t hciCoreCis[HCI_MAX_CIS_COUNT];

/*! \brief  BIG control blocks. */
extern hciCoreBig_t hciCoreBig[HCI_MAX_BIG_COUNT];

/*! \brief  BIS control blocks. */
extern hciCoreBis_t hciCoreBis[HCI_MAX_BIS_COUNT];

/*! \brief  ISO reassemble Rx packet function. */
extern hciIsoReassembleCb_t hciIsoReassembleCb;

/*! \brief  ISO Tx complete datapath handler function. */
extern hciIsoTxReadyCb_t hciIsoTxReadyCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/**************************************************************************************************/
/*!
 *  \brief  Initialize HCI ISO resources.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciCoreIsoInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Allocate a Big connection structure.
 *
 *  \param  BigId        Big identifier.
 *  \param  bisCount     Count of BIS associated with this Big.
 *  \param  pBisHandles  List of BIS handles asoociated with this Big.
 *  \param  role         Role of Big.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreBigOpen(uint8_t bigId, uint8_t bigCount, uint16_t *pBigHandles, uint8_t role);

/*************************************************************************************************/
/*!
 *  \brief  Free a Big connection structure.
 *
 *  \param  BigId        Big identifier.
 */
/*************************************************************************************************/
void hciCoreBigClose(uint8_t bigId);

/*************************************************************************************************/
/*!
 *  \brief  Perform internal processing on HCI BIS open.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreBisOpen(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Perform internal processing on HCI BIS close.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreBisClose(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get a BIS structure by handle
 *
 *  \param  handle  Connection handle.
 *
 *  \return Pointer to BIS connection structure or NULL if not found.
 */
/*************************************************************************************************/
void* hciCoreBisByHandle(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Open CIG.
 *
 *  \param  cigId        CIG identifier.
 *  \param  numCis       Number of CIS in this CIG.
 *  \param  pCisHandles  Handle list of CIS in this CIG.
 *  \param  role         Role of CIG.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreCigOpen(uint8_t cigId, uint8_t numCis, uint16_t *pCisHandles, uint8_t role);

/*************************************************************************************************/
/*!
 *  \brief  Close CIG context.
 *
 *  \param  cigId     CIG identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreCigClose(uint8_t cigId);

/*************************************************************************************************/
/*!
 *  \brief  Open CIS.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreCisOpen(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Close CIS.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreCisClose(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get a CIS connection structure by handle
 *
 *  \param  handle  Connection handle.
 *
 *  \return Pointer to CIS connection structure or NULL if not found.
 */
/*************************************************************************************************/
void* hciCoreCisByHandle(uint16_t handle);

/**************************************************************************************************/
/*!
 *  \brief  Initialize HCI ISO common resources.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreIsoCommonInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Send ISO data.
 *
 *  \param  handle ISO Handle to send data to.
 *  \param  len    Length of data.
 *  \param  pData  Data buffer.
 *
 *  \return TRUE if data was succefully sent. FALSE if not.
 */
/*************************************************************************************************/
bool_t hciSendIsoData(uint16_t handle, uint16_t len, uint8_t *pData, bool_t useTs, uint32_t ts);

/*************************************************************************************************/
/*!
 *  \brief  Unpack an ISO header.
 *
 *  \param  pData       Packet buffer.
 *  \param  pIsoPkt     Unpacked packet header.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreUnpackIsoDataPkt(uint8_t *pData, hciIsoDataPkt_t *pIsoPkt);

/*************************************************************************************************/
/*!
 *  \brief  Pack a ISO data packet into data buffer for Tx.
 *
 *  \param  pData   Data buffer.
 *  \param  pIsoPkt ISO Packet.
 *
 *  \return total bytes used by ISO Header.
 */
/*************************************************************************************************/
uint8_t hciCorePackIsoDataPkt(uint8_t *pBuf, hciIsoDataPkt_t *pIsoPkt);

/*************************************************************************************************/
/*!
 *  \brief  Reassembled Rx ISO packet.
 *
 *  \param  pData   Data buffer.
 *
 *  \return Reassembled packet if finished reassembling. NULL if packet is still pending reassembly.
 */
/*************************************************************************************************/
uint8_t *hciCoreIsoReassembly(uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  Service TX Datapath.
 *
 *  \param  bufs  Freed buffers.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreIsoTxReady(uint8_t bufs);

/*************************************************************************************************/
/*!
 *  \brief  Close Tx control block.
 *
 *  \param  pTxCb  Tx control block.
 *  \param  handle ISO handle.
 */
/*************************************************************************************************/
void hciCoreClearIsoTxCb(hciIsoTxCb_t *pTxCb, uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get ISO buffer size.
 *
 *  \param  len     Length of ISO data.
 *
 *  \return ISO buffer size.
 */
/*************************************************************************************************/
uint16_t HciGetIsoBufSize(uint16_t len);

#ifdef __cplusplus
};
#endif

#endif /* HCI_CORE_ISO_H */
