/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief      Link layer (LL) slave parameter interface implementation file.
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

#include "ll_api.h"
#include "wsf_cs.h"
#include "wsf_math.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include <string.h>
#include "lctr_api_iso.h"
#include "lmgr_api_iso.h"

/*************************************************************************************************/
/*!
 *  \brief      Get the maximum ISO buffers size.
 *
 *  \return     Maximum buffers size in bytes.
 */
/*************************************************************************************************/
uint16_t LlGetIsoMaxBufSize(void)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetIsoMaxBufSize, maxBufSize=%u", pLctrRtCfg->maxIsoSduLen);

  return pLctrRtCfg->maxIsoSduLen;
}

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ISO transmit queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetIsoTxBufs(void)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetIsoTxBufs, numBufs=%u", pLctrRtCfg->numIsoTxBuf);

  return pLctrRtCfg->numIsoTxBuf;
}

/*************************************************************************************************/
/*!
 *  \brief      Get the number of buffers in the LL ISO receive queue.
 *
 *  \return     Number of buffers.
 */
/*************************************************************************************************/
uint8_t LlGetIsoRxBufs(void)
{
  LL_TRACE_INFO1("### LlApi ###  LlGetIsoRxBufs, numBufs=%u", pLctrRtCfg->numIsoRxBuf);

  return pLctrRtCfg->numIsoRxBuf;
}

/*************************************************************************************************/
/*!
 *  \brief      Read ISO link Quality stats.
 *
 *  \param      handle  connection handle.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlReadIsoLinkQual(uint16_t handle, LlIsoLinkQual_t * pStats)
{
  LL_TRACE_INFO1("### LlApi ###  LlReadIsoLinkQual, handle=%u", handle);

  return LctrReadIsoLinkQual(handle, pStats);
}

/*************************************************************************************************/
/*!
 *  \brief      Initialize LL for use with an audio codec.
 */
/*************************************************************************************************/
void LlInitCodec(void)
{
  LctrInitCodec();
}

/*************************************************************************************************/
/*!
 *  \brief      Send an ISO data packet.
 *
 *  \param      pData   Data buffer
 *
 *  Send an ISO data packet. pData points to an ACL buffer formatted according to [1]; the host
 *  must set the connection handle, flags, and length fields in the buffer.
 */
/*************************************************************************************************/
void LlSendIsoData(uint8_t *pData)
{
  if (lmgrIsoCb.availTxBuf == 0)
  {
    LL_TRACE_WARN0("!!! ISO flow control detected; dropping Tx data PDU");

    /* Drop packet. */
    WsfMsgFree(pData);
    return;
  }

  LctrTxIso(pData);
}

/*************************************************************************************************/
/*!
 *  \brief      Receive an ISO data packet
 *
 *  \return     Data buffer.
 *
 *  Receive an ISO data packet.  This function returns a pointer to an ISO buffer formatted
 *  according to [1].  The host must parse the header to determine the connection handle, flags,
 *  and length fields.  If no ISO buffers are available this function returns NULL.
 *
 *  The host must deallocate the buffer by calling WsfMsgFree() and then calling
 *  LlRecvIsoDataComplete().
 */
/*************************************************************************************************/
uint8_t *LlRecvIsoData(void)
{
  return LctrRxIso();
}

/*************************************************************************************************/
/*!
 *  \brief      Indicate that received ISO data buffer has been deallocated
 *
 *  \param      numBufs     Number of completed packets.
 *
 *  Indicate that received ISO data buffer has been deallocated.
 */
/*************************************************************************************************/
void LlRecvIsoDataComplete(uint8_t numBufs)
{
  LctrRxIsoComplete(numBufs);
}

/*************************************************************************************************/
/*!
 *  \brief      Used to identify and enable the isochronous data path between the host and the
 *              controller for each connected isochronous or broadcast isochronous stream.
 *
 *  \param      handle      BIS or CIS handle.
 *  \param      pPktSn      Packet sequence number.
 *  \param      pTs         Timestamp.
 *  \param      pTimeOffs   Time offset.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlReadIsoTxSync(uint16_t handle, uint16_t *pPktSn, uint32_t *pTs, uint32_t *pTimeOffs)
{
  LL_TRACE_INFO1("### LlApi ###  LlReadIsoTxSync, handle=%u", handle);

  return LctrReadIsoTxSync(handle, pPktSn, pTs, pTimeOffs);
}

/*************************************************************************************************/
/*!
 * \brief       Used to request the Controller to configure the data transport path in a given
 *              direction between the Controller and the Host.
 *
 * \param       pConfigDataPath  Parameters for configure data path.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlConfigureDataPath(LlIsoConfigDataPath_t *pConfigDataPath)
{
  LL_TRACE_INFO2("### LlApi ###  LlConfigureDataPath, dpDir=%u, dpId=%u", pConfigDataPath->dpDir, pConfigDataPath->dpId);

  return LctrConfigureDataPath(pConfigDataPath);
}

/*************************************************************************************************/
/*!
 *  \brief      Used to identify and enable the isochronous data path between the host and the
 *              controller for each connected isochronous stream or broadcast isochronous stream.
 *
 *  \param      pSetupDataPath   Parameters for setup ISO data path.
 */
/*************************************************************************************************/
uint8_t LlSetupIsoDataPath(LlIsoSetupDataPath_t *pSetupDataPath)
{
  LL_TRACE_INFO3("### LlApi ###  LlSetupIsoDataPath, handle=%u, dpDir=%u, dpId=%u", pSetupDataPath->handle, pSetupDataPath->dpDir, pSetupDataPath->dpId);

  return LctrSetupIsoDataPath(pSetupDataPath);
}

/*************************************************************************************************/
/*!
 *  \brief      Used to remove the isochronous data path associated with the
 *              connected isochronous stream or broadcast isochronous stream.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      dpDir       Direction of data path to remove.
 */
/*************************************************************************************************/
uint8_t LlRemoveIsoDataPath(uint16_t handle, uint8_t dpDir)
{
  LL_TRACE_INFO2("### LlApi ###  LlRemoveIsoDataPath, handle=%u, dpDir=%u", handle, dpDir);

  return LctrRemoveIsoDataPath(handle, dpDir);
}

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Tx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoTxTest(uint16_t handle, uint8_t pldType)
{
  LL_TRACE_INFO2("### LlApi ###  LlIsoTxTest handle=%d pldType=%d", handle, pldType);

  return LctrIsoTxTest(handle, pldType);
}

/*************************************************************************************************/
/*!
 *  \brief      Enable ISO Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pldType     Payload type.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoRxTest(uint16_t handle, uint8_t pldType)
{
  LL_TRACE_INFO2("### LlApi ###  LlIsoRxTest handle=%d pldType=%d", handle, pldType);

  return LctrIsoRxTest(handle, pldType);
}

/*************************************************************************************************/
/*!
 *  \brief      ISO read test counter.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pCtr        Test Counter.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoReadTestCounter(uint16_t handle, LlIsoTestCtrs_t *pCtr)
{
  LL_TRACE_INFO1("### LlApi ###  LlIsoReadTestCounter handle=%d", handle);

  LctrIsoReadTestCounter(handle, pCtr);

  return LL_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief      Terminate ISO Tx or Rx test.
 *
 *  \param      handle      CIS or BIS handle.
 *  \param      pCtr        Test counters.
 *
 *  \return     Status error code.
 */
/*************************************************************************************************/
uint8_t LlIsoTestEnd(uint16_t handle, LlIsoTestCtrs_t *pCtr)
{
  LL_TRACE_INFO1("### LlApi ###  LlIsoTestEnd handle=%d", handle);

  return LctrIsoTestEnd(handle, pCtr);
}
