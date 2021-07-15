/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  HCI core interfaces.
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
#ifndef HCI_CORE_H
#define HCI_CORE_H

#include "hci_core_ps.h"
#include "hci_core_iso.h"
#include "wsf_queue.h"
#include "wsf_os.h"
#include "hci_api.h"
#include "cfg_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Callback Function Types
**************************************************************************************************/

/*! \brief HCI Reset sequence callback type */
typedef void(*hciResetSeq_t)(uint8_t *pMsg, uint16_t opcode);

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief Per-connection structure for ACL packet accounting */
typedef struct
{
  uint8_t           *pTxAclPkt;                   /*!< \brief Fragmenting TX ACL packet pointer */
  uint8_t           *pNextTxFrag;                 /*!< \brief Next TX ACL packet fragment */
  uint8_t           *pRxAclPkt;                   /*!< \brief RX ACL packet pointer */
  uint8_t           *pNextRxFrag;                 /*!< \brief Next RX ACL packet fragment */
  uint16_t          handle;                       /*!< \brief Connection handle */
  uint16_t          txAclRemLen;                  /*!< \brief Fragmenting TX ACL packet remaining length */
  uint16_t          rxAclRemLen;                  /*!< \brief Fragmented RX ACL packet remaining length */
  bool_t            fragmenting;                  /*!< \brief TRUE if fragmenting a TX ACL packet */
  bool_t            flowDisabled;                 /*!< \brief TRUE if data flow disabled */
  uint8_t           queuedBufs;                   /*!< \brief Queued ACL buffers on this connection */
  uint8_t           outBufs;                      /*!< \brief Outstanding ACL buffers sent to controller */
} hciCoreConn_t;

/*! \brief Main control block for dual-chip implementation */
typedef struct
{
  hciCoreConn_t     conn[DM_CONN_MAX];            /*!< \brief Connection structures */
  hciCoreCig_t      *pCig;                        /*!< \biref CIG structures */
  hciCoreCis_t      *pCis;                        /*!< \brief CIS structures */
  hciCoreBig_t      *pBig;                        /*!< \brief BIG structures */
  hciCoreBis_t      *pBis;                        /*!< \brief BIS structures */
  uint8_t           leStates[HCI_LE_STATES_LEN];  /*!< \brief Controller LE supported states */
  bdAddr_t          bdAddr;                       /*!< \brief Bluetooth device address */
  wsfQueue_t        aclQueue;                     /*!< \brief HCI ACL TX queue */
  hciCoreConn_t     *pConnRx;                     /*!< \brief Connection struct for current transport RX packet */
  uint16_t          maxRxAclLen;                  /*!< \brief Maximum reassembled RX ACL packet length */
  uint16_t          bufSize;                      /*!< \brief Controller ACL data buffer size */
  uint16_t          isoBufSize;                   /*!< \brief ISO buffer size. */
  uint8_t           aclQueueHi;                   /*!< \brief Disable flow when this many ACL buffers queued */
  uint8_t           aclQueueLo;                   /*!< \brief Enable flow when this many ACL buffers queued */
  uint8_t           availBufs;                    /*!< \brief Current avail ACL data buffers */
  uint8_t           availIsoBufs;                 /*!< \brief Current available ISO data buffers. */
  uint8_t           numBufs;                      /*!< \brief Controller number of ACL data buffers */
  uint8_t           isoNumBufs;                   /*!< \brief Number of ISO buffers. */
  uint8_t           whiteListSize;                /*!< \brief Controller white list size */
  uint8_t           numCmdPkts;                   /*!< \brief Controller command packed count */
  uint64_t          leSupFeat;                    /*!< \brief Controller LE supported features */
  int8_t            advTxPwr;                     /*!< \brief Controller advertising TX power */
  uint8_t           resListSize;                  /*!< \brief Controller resolving list size */
  uint16_t          maxAdvDataLen;                /*!< \brief Controller maximum advertisement (or scan response) data length */
  uint8_t           numSupAdvSets;                /*!< \brief Controller maximum number of advertising sets */
  uint8_t           perAdvListSize;               /*!< \brief Controller periodic advertising list size */
  hciLocalVerInfo_t locVerInfo;                   /*!< \brief Controller version information */
  hciResetSeq_t     extResetSeq;                  /*!< \brief HCI extended reset sequence callback */
} hciCoreCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \brief Control block */
extern hciCoreCb_t hciCoreCb;

/*! \brief Event mask */
extern const uint8_t hciEventMask[HCI_EVT_MASK_LEN];

/*! \brief LE event mask */
extern const uint8_t hciLeEventMask[HCI_LE_EVT_MASK_LEN];

/*! \brief Event mask page 2 */
extern const uint8_t hciEventMaskPage2[HCI_EVT_MASK_LEN];

/*! \brief LE supported features configuration mask */
extern uint64_t hciLeSupFeatCfg;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  HCI core initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreInit(void);

/*************************************************************************************************/
/*!
 *  \fn     hciCoreResetStart
 *
 *  \brief  Start the HCI reset sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreResetStart(void);

/*************************************************************************************************/
/*!
 *  \brief  Perform internal processing on HCI connection open.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreConnOpen(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Perform internal processing on HCI connection close.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreConnClose(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Get a connection structure by handle
 *
 *  \param  handle  Connection handle.
 *
 *  \return Pointer to connection structure or NULL if not found.
 */
/*************************************************************************************************/
hciCoreConn_t *hciCoreConnByHandle(uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Send ACL data to transport.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreSendAclData(hciCoreConn_t *pConn, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  Service the TX data path.
 *
 *  \param  bufs    Number of new buffers now available.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxReady(uint8_t bufs);

/*************************************************************************************************/
/*!
 *  \brief  Send ACL packets, start of packet.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  len      ACL packet length.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxAclStart(hciCoreConn_t *pConn, uint16_t len, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  Send ACL packets, continuation of fragmented packets.
 *
 *  \param  pConn    Pointer to connection structure.  If set non-NULL, then a fragment is
 *                   sent from this connection structure.  If NULL the function finds the next
 *                   connection structure with a fragment to be sent.
 *
 *  \return TRUE if packet sent, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t hciCoreTxAclContinue(hciCoreConn_t *pConn);

/*************************************************************************************************/
/*!
 *  \brief  This function is called from the HCI transport layer when transmission of an ACL
 *          packet is complete.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxAclComplete(hciCoreConn_t *pConn, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  Reassemble an ACL packet.
 *
 *  \param  pData   Input ACL packet.
 *
 *  \return pointer to ACL packet to send, or NULL if no packet to send.
 */
/*************************************************************************************************/
uint8_t *hciCoreAclReassembly(uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \brief  Check if a TX ACL packet is being fragmented.
 *
 *  \param  pConn Connection context.
 *
 *  \return TRUE if fragmenting a TX ACL packet, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t hciCoreTxAclDataFragmented(hciCoreConn_t *pConn);

#ifdef __cplusplus
};
#endif

#endif /* HCI_CORE_H */
