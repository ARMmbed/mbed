/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Token trace decode.
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

#ifdef WSF_DETOKEN_TRACE

#include <string.h>
#include "wsf_types.h"
#include "wsf_trace.h"
#include "util/bstream.h"
#include "wsf_types.h"
#include "wsf_detoken.h"
#include "wsf_assert.h"
#include "hci_defs.h"
#include "hci_api.h"

#if WSF_DETOKEN_TRACE == WSF_DETOKEN_TRACE_NORDIC
#include "../../controller/ble5-ctr/gcc/bin/ble5-ctr_tokens.h"
#endif /* WSF_DETOKEN_TRACE */

/**************************************************************************************************
 Macros
**************************************************************************************************/

/*! \brief Maximum size of a single trace log message. */
#define WSF_TOKEN_BUFFER_SIZE             128

/**************************************************************************************************
 Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Initialize detoken trace.
 */
/*************************************************************************************************/
void WsfDetokenInit(void)
{
  /* Take no action - place holder should future platform require initialization */
}

/*************************************************************************************************/
/*!
 *  \brief  Enable/disable detoken trace.
 *
 *  \param  enable    TRUE to enable, FALSE to disable.
 */
/*************************************************************************************************/
void WsfDetokenEnable(bool_t enable)
{
  uint8_t pEvtMask[] = {0, 0, 0, 0, 0, 0, 0, 0};

  if (enable)
  {
    pEvtMask[0] = WSF_DETOKEN_ENABLE_BIT;
  }

  HciVendorSpecificCmd(WSF_DETOKEN_VS_SET_EVENT_MASK_OPCODE, sizeof(pEvtMask), pEvtMask);
}

/*************************************************************************************************/
/*!
 *  \brief  Find token by id via binary search
 *
 *  \param  token     Token identifier.
 *
 *  \return Returns a pointer to token.
 */
/*************************************************************************************************/
static const tokenElem_t *wsfDetokenFindToken(uint32_t token)
{
  uint16_t numTokens = sizeof (tokenLookupTbl) / sizeof(tokenElem_t);
  uint16_t top = 0;
  uint16_t bottom = numTokens - 1;
  uint16_t midPoint = (top + bottom) / 2;

  if ((token < tokenLookupTbl[0].token) || (token > tokenLookupTbl[numTokens-1].token))
  {
    return NULL;
  }

  while (top <= bottom)
  {
    if (tokenLookupTbl[midPoint].token < token)
    {
      top = midPoint + 1;
    }
    else if (tokenLookupTbl[midPoint].token == token)
    {
      return &tokenLookupTbl[midPoint];
    }
    else
    {
      bottom = midPoint - 1;
    }

    midPoint = (top + bottom) / 2;
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Find token string by id via binary search
 *
 *  \param  token     Token identifier.
 *
 *  \return Returns a token string.
 */
/*************************************************************************************************/
static const char *wsfDetokenFindTokenString(uint32_t token)
{
  uint16_t numTokens = sizeof (tokenLookupTbl) / sizeof(tokenElem_t);
  uint16_t top = 0;
  uint16_t bottom = numTokens - 1;
  uint16_t midPoint = (top + bottom) / 2;

  while (top <= bottom)
  {
    if (tokenStrTbl[midPoint].strId < token)
    {
      top = midPoint + 1;
    }
    else if (tokenStrTbl[midPoint].strId == token)
    {
      return tokenStrTbl[midPoint].pText;
    }
    else
    {
      bottom = midPoint - 1;
    }

    midPoint = (top + bottom) / 2;
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  Decode and display HCI Token Trace events
 *
 *  \param  pBuffer   Buffer containing HCI event.
 */
/*************************************************************************************************/
static void wsfDetokenDecodeHciTokenTrace(uint8_t *pBuffer)
{
  uint32_t tokenId;
  uint32_t var;
  const tokenElem_t *pToken;
  char  lineBuf[WSF_TOKEN_BUFFER_SIZE + 1];

  BYTES_TO_UINT32(tokenId, (pBuffer + 2));
  BYTES_TO_UINT32(var, (pBuffer + 6));

  tokenId = tokenId & 0x0FFFFFFF;

  pToken = wsfDetokenFindToken(tokenId);

  if (pToken != NULL)
  {
    uint8_t numParams = pToken->paramInfo & 0xf;
    uint8_t paramMask = pToken->paramInfo >> 4;

    strncpy(lineBuf, pToken->pMsg, WSF_TOKEN_BUFFER_SIZE);

    switch (numParams)
    {
    case 0:
      LL_TRACE_INFO0(lineBuf);
      break;

    case 1:
      if (paramMask == WSF_DETOKEN_PARAM_VARIABLE)
      {
        LL_TRACE_INFO1(lineBuf, var);
      }
      else
      {
        LL_TRACE_INFO1(lineBuf, wsfDetokenFindTokenString(var));
      }
      break;

    case 2:
      if (paramMask == WSF_DETOKEN_PARAM_VARIABLE)
      {
        LL_TRACE_INFO2(lineBuf, var & 0xfff, var >> 16);
      }
      else if (paramMask == WSF_DETOKEN_PARAM_STRING)
      {
        LL_TRACE_INFO2(lineBuf, var & 0xfff, wsfDetokenFindTokenString(var >> 16));
      }
      else
      {
        LL_TRACE_INFO2(lineBuf, wsfDetokenFindTokenString(var & 0xfff), var >> 16);
      }
      break;

    case 3:
      LL_TRACE_INFO3(lineBuf, var & 0xff, (var >> 8) & 0xff, (var >> 16) & 0xffff);
      break;
    }
  }
  else
  {
    LL_TRACE_INFO0("UNKNOWN TOKEN TRACE\n");
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Process vendor specific HCI events and decode token trace events from the LL
 *
 *  \param  len       Length of pBuffer in bytes.
 *  \param  pBuffer   Buffer containing HCI event.
 *
 *  \return TRUE if VS HCI message is a token, else FALSE.
 */
/*************************************************************************************************/
bool_t WsfDetokenProcessHciEvent(uint16_t len, uint8_t *pBuffer)
{
  uint16_t vsEvent;

  BYTES_TO_UINT16(vsEvent, pBuffer);

  if (vsEvent == WSF_DETOKEN_VS_EVT_TOKEN)
  {
    wsfDetokenDecodeHciTokenTrace(pBuffer);
    return TRUE;
  }

  return FALSE;
}

#endif /* WSF_DETOKEN_TRACE */
