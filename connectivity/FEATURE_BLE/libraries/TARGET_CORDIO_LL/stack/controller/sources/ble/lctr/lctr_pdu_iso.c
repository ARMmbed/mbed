/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller isochronous stream packet implementation file.
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

#include "lctr_pdu_iso.h"
#include "util/bstream.h"
#include "hci_defs.h"
#include <string.h>

/*************************************************************************************************/
/*!
 *  \brief  Unpack an ISO header.
 *
 *  \param  pHdr        Unpacked packet header.
 *  \param  pBuf        Packed packet buffer.
 *
 *  \return Header length.
 */
/*************************************************************************************************/
uint8_t lctrIsoUnpackHdr(lctrIsoHdr_t *pHdr, uint8_t *pBuf)
{
  uint8_t len = HCI_ISO_HDR_LEN;

  uint16_t field16;

  BSTREAM_TO_UINT16(field16, pBuf);
  pHdr->handle = (field16 >>  0) & 0x0FFF;
  pHdr->pb     = (field16 >> 12) & 0x03;
  pHdr->tsFlag = (field16 >> 14) & 0x01;

  BSTREAM_TO_UINT16(pHdr->len, pBuf);

  switch (pHdr->pb)
  {
    case LCTR_PB_COMP:
    case LCTR_PB_FIRST:
      len += HCI_ISO_DL_MIN_LEN;
      if (pHdr->tsFlag)
      {
        BSTREAM_TO_UINT32(pHdr->ts, pBuf);
        len += HCI_ISO_TS_LEN;
      }
      else
      {
        pHdr->ts = 0;
      }

      BSTREAM_TO_UINT16(pHdr->pktSn, pBuf);
      BSTREAM_TO_UINT16(field16, pBuf);

      pHdr->sduLen = (field16 >>  0) & 0x0FFF;
      pHdr->ps     = (field16 >> 14) & 0x03;
      break;

    default:
      pHdr->ts     = 0;
      pHdr->sduLen = 0;
      pHdr->ps     = 0;
      break;
  }

  pHdr->pSdu = pBuf;

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack an ISO header.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pHdr        Unpacked packet header.
 *
 *  \return Header length.
 */
/*************************************************************************************************/
uint8_t lctrIsoPackHdr(uint8_t *pBuf, const lctrIsoHdr_t *pHdr)
{
  const uint8_t len = HCI_ISO_HDR_LEN;
  uint8_t dlLen = 0;

  uint16_t field16;

  field16  = (pHdr->handle & 0x0FFF) <<  0;
  field16 |= (pHdr->pb     & 0x03)   << 12;
  field16 |= (pHdr->tsFlag & 0x01)   << 14;
  UINT16_TO_BSTREAM(pBuf, field16);

  uint8_t *pLen = pBuf;
  pBuf += sizeof(uint16_t);

  switch (pHdr->pb)
  {
    case LCTR_PB_COMP:
    case LCTR_PB_FIRST:
      dlLen = HCI_ISO_DL_MIN_LEN;
      if (pHdr->tsFlag)
      {
        UINT32_TO_BSTREAM(pBuf, pHdr->ts);
        dlLen += HCI_ISO_TS_LEN;
      }

      UINT16_TO_BSTREAM(pBuf, pHdr->pktSn);

      field16  = (pHdr->sduLen & 0x0FFF) <<  0;
      field16 |= (pHdr->ps     & 0x03)   << 14;
      UINT16_TO_BSTREAM(pBuf, field16);
      break;

    default:
      break;
  }

  /* Now that length is known, pack the length. */
  UINT16_TO_BSTREAM(pLen, pHdr->len + dlLen);

  return len + dlLen;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a ISOAL Segmentation header.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pSegHdr     Unpacked PDU header.
 *
 *  \return Segmentation header length.
 */
/*************************************************************************************************/
uint8_t lctrIsoPackSegHdr(uint8_t *pBuf, const lctrIsoSegHdr_t *pSegHdr)
{
  uint8_t len = LL_DATA_HDR_LEN;

  uint8_t flags = 0;
  flags |= (pSegHdr->sc   ) << 0;
  flags |= (pSegHdr->cmplt) << 1;
  UINT8_TO_BSTREAM(pBuf, flags);

  if (pSegHdr->sc == 0)
  {
    UINT8_TO_BSTREAM(pBuf, pSegHdr->len + LL_ISO_SEG_TO_LEN);
    len += LL_ISO_SEG_TO_LEN;
    UINT24_TO_BSTREAM(pBuf, pSegHdr->toffs);
  }
  else
  {
    UINT8_TO_BSTREAM(pBuf, pSegHdr->len);
  }

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack a ISOAL Segmentation header.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pHdr        Unpacked PDU header.
 *
 *  \return Segmentation header length.
 */
/*************************************************************************************************/
uint8_t lctrIsoUnpackSegHdr(lctrIsoSegHdr_t *pHdr, uint8_t *pBuf)
{
  uint8_t len = LL_DATA_HDR_LEN;

  uint8_t flags = 0;
  BSTREAM_TO_UINT8(flags, pBuf);
  pHdr->sc = (flags & LL_ISOAL_SEG_HDR_MASK_SC) >> 0;
  pHdr->cmplt = (flags & LL_ISOAL_SEG_HDR_MASK_CMPLT) >> 1;
  BSTREAM_TO_UINT8(pHdr->len, pBuf);

  if (pHdr->sc == 0)
  {
    len += LL_ISO_SEG_TO_LEN;
    BSTREAM_TO_UINT24(pHdr->toffs, pBuf);
  }

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Pack an CIS data channel PDU header.
 *
 *  \param  pBuf        Packed packet buffer.
 *  \param  pHdr        Unpacked PDU header.
 *
 *  \return Header length.
 */
/*************************************************************************************************/
uint8_t lctrCisPackDataPduHdr(uint8_t *pBuf, const lctrCisDataPduHdr_t *pHdr)
{
  const uint8_t len = LL_DATA_HDR_LEN;

  uint16_t hdr = 0;

  hdr |= (pHdr->llid    ) << 0;
  hdr |= (pHdr->nesn & 1) << 2;
  hdr |= (pHdr->sn   & 1) << 3;
  hdr |= (pHdr->cie  & 1) << 4;
  hdr |= (pHdr->np   & 1) << 6;
  hdr |= (pHdr->len     ) << 8;
  UINT16_TO_BSTREAM(pBuf, hdr);

  return len;
}

/*************************************************************************************************/
/*!
 *  \brief  Unpack a CIS data channel PDU header.
 *
 *  \param  pHdr        Unpacked header.
 *  \param  pBuf        Packed packet buffer.
 *
 *  \return Header length.
 */
/*************************************************************************************************/
uint8_t lctrCisUnpackDataPduHdr(lctrCisDataPduHdr_t *pHdr, const uint8_t *pBuf)
{
  const uint8_t len = LL_DATA_HDR_LEN;

  uint16_t hdr;

  BSTREAM_TO_UINT16(hdr, pBuf);

  pHdr->llid = (hdr >> 0) & LL_DATA_HDR_LLID_MSK;
  pHdr->nesn = (hdr >> 2) & 0x0001;
  pHdr->sn   = (hdr >> 3) & 0x0001;
  pHdr->cie  = (hdr >> 4) & 0x0001;
  pHdr->np   = (hdr >> 6) & 0x0001;
  pHdr->len  = (hdr >> 8) & LL_DATA_HDR_LEN_MSK;

  return len;
}
