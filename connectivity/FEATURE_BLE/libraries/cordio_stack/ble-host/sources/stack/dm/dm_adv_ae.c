/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Device manager extended advertising module.
 *
 *  Copyright (c) 2016-2019 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "dm_api.h"
#include "dm_adv.h"
#include "dm_dev.h"
#include "dm_main.h"
#include "dm_conn.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Connectable directed advertising */
#define DM_ADV_CONN_DIRECTED(advType)       (((advType) == DM_ADV_CONN_DIRECT)        || \
                                             ((advType) == DM_ADV_CONN_DIRECT_LO_DUTY))

/*! Connectable advertising */
#define DM_ADV_CONNECTABLE(advType)         (((advType) == DM_ADV_CONN_UNDIRECT)      || \
                                             ((advType) == DM_EXT_ADV_CONN_UNDIRECT)  || \
                                             DM_ADV_CONN_DIRECTED((advType)))

/*! Scannable advertising */
#define DM_ADV_SCANNABLE(advType)           (((advType) == DM_ADV_CONN_UNDIRECT)      || \
                                             ((advType) == DM_ADV_SCAN_UNDIRECT)      || \
                                             ((advType) == DM_EXT_ADV_SCAN_DIRECT))

/*! Non-connectable and non-scannable advertising */
#define DM_NON_CONN_NON_SCAN(advType)       (((advType) == DM_ADV_NONCONN_UNDIRECT)   || \
                                             ((advType) == DM_EXT_ADV_NONCONN_DIRECT))

/*! Directed advertising */
#define DM_ADV_DIRECTED(advType)            (((advType) == DM_EXT_ADV_NONCONN_DIRECT) || \
                                             ((advType) == DM_EXT_ADV_SCAN_DIRECT)    || \
                                             DM_ADV_CONN_DIRECTED((advType)))

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Control block for extended advertising module */
typedef struct
{
  uint8_t             advType;         /*!< Advertising type. */
  bool_t              useLegacyPdu;    /*!< Use legacy advertising PDUs. */
  bool_t              omitAdvAddr;     /*!< Omit advertiser's address from all PDUs. */
  bool_t              incTxPwr;        /*!< Include TxPower in extended header of advertising PDU. */
  int8_t              advTxPwr;        /*!< Advertising Tx Power. */
  uint8_t             priAdvPhy;       /*!< Primary Advertising PHY. */
  uint8_t             secAdvMaxSkip;   /*!< Secondary Advertising Maximum Skip. */
  uint8_t             secAdvPhy;       /*!< Secondary Advertising PHY. */
  bool_t              scanReqNotifEna; /*!< Scan request notification enable. */
  uint8_t             fragPref;        /*!< Fragment preference for advertising data. */
  uint8_t             advSid;          /*!< Advertising Sid. */
  bool_t              advDataSet;      /*!< TRUE if extended adv data has been set. */
  bool_t              scanDataSet;     /*!< TRUE if extended scan data has been set. */
  dmConnId_t          connId;          /*!< Connection identifier (used by directed advertising). */
} dmExtAdvCb_t;

/* Control block for periodic advertising module */
typedef struct
{
  uint16_t            intervalMin;     /*!< Minimum advertising interval. */
  uint16_t            intervalMax;     /*!< Maximum advertising interval. */
  bool_t              incTxPwr;        /*!< Include TxPower in extended header of advertising PDU. */
  uint8_t             advState;        /*!< Periodic advertising state. */
} dmPerAdvCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* extended advertising action function table */
static const dmAdvAct_t dmAdvAct[] =
{
  dmExtAdvActConfig,
  dmExtAdvActSetData,
  dmExtAdvActStart,
  dmExtAdvActStop,
  dmExtAdvActRemoveSet,
  dmExtAdvActClearSets,
  dmExtAdvActSetRandAddr,
  dmExtAdvActTimeout
};

/* extended advertising component function interface */
static const dmFcnIf_t dmAdvFcnIf =
{
  dmExtAdvReset,
  dmExtAdvHciHandler,
  dmExtAdvMsgHandler
};

/* periodic advertising action function table */
static const dmAdvAct_t dmPerAdvAct[] =
{
  dmPerAdvActConfig,
  dmPerAdvActSetData,
  dmPerAdvActStart,
  dmPerAdvActStop
};

/* periodic advertising component function interface */
static const dmFcnIf_t dmPerAdvFcnIf =
{
  dmPerAdvReset,
  dmPerAdvHciHandler,
  dmPerAdvMsgHandler
};

/* extended advertising control block */
static dmExtAdvCb_t dmExtAdvCb[DM_NUM_ADV_SETS];

/* periodic advertising control block */
static dmPerAdvCb_t dmPerAdvCb[DM_NUM_ADV_SETS];

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static bool_t dmAdvEnabled(void);
static void dmExtAdvCbInit(uint8_t advHandle);
static void dmAdvConfig(uint8_t advHandle, uint8_t advType, uint8_t peerAddrType,
                        uint8_t *pPeerAddr, bool_t useLegacyPdu);
static void dmAdvStart(uint8_t numSets, uint8_t *pAdvHandles, uint16_t *pDuration,
                       uint8_t *pMaxEaEvents, uint8_t state);
static void dmAdvStop(uint8_t numSets, uint8_t *pAdvHandles, uint8_t state);

/*************************************************************************************************/
/*!
 *  \brief  Initialize the extended advertising CB for a given handle.
 *
 *  \param  advHandle        Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmExtAdvCbInit(uint8_t advHandle)
{
  /* initialize advertising set */
  dmExtAdvCb[advHandle].advType = DM_ADV_NONE;
  dmExtAdvCb[advHandle].useLegacyPdu = TRUE;
  dmExtAdvCb[advHandle].omitAdvAddr = FALSE;
  dmExtAdvCb[advHandle].incTxPwr = FALSE;
  dmExtAdvCb[advHandle].advTxPwr = HCI_TX_PWR_NO_PREFERENCE;
  dmExtAdvCb[advHandle].priAdvPhy = HCI_ADV_PHY_LE_1M;
  dmExtAdvCb[advHandle].secAdvMaxSkip = 0;
  dmExtAdvCb[advHandle].secAdvPhy = HCI_ADV_PHY_LE_1M;
  dmExtAdvCb[advHandle].scanReqNotifEna = FALSE;
  dmExtAdvCb[advHandle].fragPref = HCI_ADV_DATA_FRAG_PREF_FRAG;
  dmExtAdvCb[advHandle].advSid = 0;
  dmExtAdvCb[advHandle].connId = DM_CONN_ID_NONE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the periodic advertising CB for a given handle.
 *
 *  \param  advHandle        Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmPerAdvCbInit(uint8_t advHandle)
{
  /* initialize periodic advertising set */
  dmPerAdvCb[advHandle].intervalMin = DM_GAP_ADV_SLOW_INT_MIN;
  dmPerAdvCb[advHandle].intervalMax = DM_GAP_ADV_SLOW_INT_MAX;
  dmPerAdvCb[advHandle].incTxPwr = FALSE;
  dmPerAdvCb[advHandle].advState = DM_ADV_PER_STATE_IDLE;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the extended advertising module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvInit(void)
{
  uint8_t i;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    /* initialize extended control block */
    dmExtAdvCbInit(i);
  }

  /* reset legacy adv module */
  dmAdvInit();
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the periodic advertising module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvInit(void)
{
  uint8_t i;

  /* initialize extended control block */
  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    dmPerAdvCbInit(i);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Check if advertising is enabled for any advertising set.
 *
 *  \return TRUE if advertising enabled. FALSE, otherwise.
 */
/*************************************************************************************************/
static bool_t dmAdvEnabled(void)
{
  uint8_t i;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    /* if doing advertising */
    if (dmAdvCb.advType[i] != DM_ADV_NONE)
    {
      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if connectable directed advertising is enabled for the given advertising set.
 *
 *  \param  numSets       Number of advertising sets.
 *  \param  pAdvHandles   Advertising handles array.
 *
 *  \return TRUE if connectable directed advertising enabled. FALSE, otherwise.
 */
/*************************************************************************************************/
static bool_t dmAdvDirectedEnabled(uint8_t numSets, uint8_t *pAdvHandles)
{
  uint8_t i;

  for (i = 0; i < numSets; i++)
  {
    /* if doing connectable directed advertising */
    if (DM_ADV_CONN_DIRECTED(dmAdvCb.advType[pAdvHandles[i]]))
    {
      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get conectable directed advertising handle for the given connection id.
 *
 *  \param  connId        Connection identifier.
 *
 *  \return Advertising handle. DM_NUM_ADV_SETS, if not found.
 */
/*************************************************************************************************/
static uint8_t dmAdvDirectedByConnId(dmConnId_t connId)
{
  uint8_t i;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    if (DM_ADV_CONN_DIRECTED(dmAdvCb.advType[i]) && (dmExtAdvCb[i].connId == connId))
    {
      return i;
    }
  }

  DM_TRACE_INFO0("dmAdvDirectedByConnId not found");

  return DM_NUM_ADV_SETS;
}

/*************************************************************************************************/
/*!
 *  \brief  Disable connectable directed advertising.
 *
 *  \param  connId        Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmAdvDirectedDisable(dmConnId_t connId)
{
  uint8_t advHandle;

  advHandle = dmAdvDirectedByConnId(connId);
  if (advHandle < DM_NUM_ADV_SETS)
  {
    /* disable advertising set */
    dmAdvCb.advType[advHandle] = DM_ADV_NONE;
    dmAdvCb.advState[advHandle] = DM_ADV_STATE_IDLE;
    dmExtAdvCb[advHandle].connId = DM_CONN_ID_NONE;
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the extended advertising parameters using the given advertising type, and peer
 *          address.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  advType       Advertising type.
 *  \param  peerAddrType  Peer address type.
 *  \param  pPeerAddr     Peer address.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmAdvConfig(uint8_t advHandle, uint8_t advType, uint8_t peerAddrType,
                        uint8_t *pPeerAddr, bool_t useLegacyPdu)
{
  bool_t advDataAllowed = FALSE;
  bool_t scanDataAllowed = FALSE;
  uint16_t advEventProp = 0;
  hciExtAdvParam_t extAdvParam;

  memset(&extAdvParam, 0, sizeof(hciExtAdvParam_t));

  /* set advertising event properties */
  if (useLegacyPdu)
  {
    /* use legacy advertising PDUs (bit 4 = 1) */
    switch (advType)
    {
      case DM_ADV_CONN_UNDIRECT:
        /* connectable and scannable undirected advertising */
        advEventProp = HCI_ADV_PROP_LEG_CONN_UNDIRECT;
        break;

      case DM_ADV_CONN_DIRECT_LO_DUTY:
        /* connectable directed (low duty cycle) advertising */
        advEventProp = HCI_ADV_PROP_LEG_CONN_DIRECT_LO_DUTY;
        break;

      case DM_ADV_CONN_DIRECT:
        /* connectable directed (high duty cycle) advertising */
        advEventProp = HCI_ADV_PROP_LEG_CONN_DIRECT;
        break;

      case DM_ADV_SCAN_UNDIRECT:
        /* scannable undirected advertising */
        advEventProp = HCI_ADV_PROP_LEG_SCAN_UNDIRECT;
        break;

      case DM_ADV_NONCONN_UNDIRECT:
        /* non-connectable undirected advertising */
        advEventProp = HCI_ADV_PROP_LEG_NONCONN_UNDIRECT;
        break;

      default:
        /* unkown advertising type */
        break;
    }

    /* make sure bit 4 (use legacy advertising PDUs) is set */
    advEventProp |= HCI_ADV_PROP_USE_LEG_PDU_BIT;

    /* set primary advertising PHY to LE 1M for legacy advertising */
    extAdvParam.priAdvPhy = HCI_ADV_PHY_LE_1M;

    /* advertising data is not allowed with connectable directed advertising type */
    if (!DM_ADV_CONN_DIRECTED(advType))
    {
      advDataAllowed = TRUE;
    }

    /* scan data is only allowed with connectable and scannable undirected advertising
       and scannable undirected advertising types */
    if ((advType == DM_ADV_CONN_UNDIRECT) || (advType == DM_ADV_SCAN_UNDIRECT))
    {
      scanDataAllowed = TRUE;
    }
  }
  else
  {
    /* use extended advertising PDUs (bit 4 = 0) */

    /* extended advertisement cannot be both connectable and scannable */
    if (advType == DM_ADV_CONN_UNDIRECT)
    {
      /* overwrite it with connectable undirected advertising */
      advType = DM_EXT_ADV_CONN_UNDIRECT;
    }

    if (DM_ADV_CONNECTABLE(advType))
    {
      /* set bit 0 */
      advEventProp |= HCI_ADV_PROP_CONN_ADV_BIT;
    }

    /* if scannable advertising types */
    if (DM_ADV_SCANNABLE(advType))
    {
      /* set bit 1 */
      advEventProp |= HCI_ADV_PROP_SCAN_ADV_BIT;

      /* scan data is allowed only with scannable advertising types */
      scanDataAllowed = TRUE;
    }
    /* advertising data is only allowed with non-scannable advertising types */
    else
    {
      advDataAllowed = TRUE;
    }

    /* if directed advertising */
    if (DM_ADV_DIRECTED(advType))
    {
      /* set bit 2 */
      advEventProp |= HCI_ADV_PROP_DIRECT_ADV_BIT;
    }

    /* high duty cycle connectable directed advertising cannot be used (bit 3 = 0) */

    /* omit advertiser's address from all PDUs */
    if (dmExtAdvCb[advHandle].omitAdvAddr)
    {
      /* set bit 5 */
      advEventProp |= HCI_ADV_PROP_OMIT_ADV_ADDR_BIT;
    }

    /* include TxPower in extended header of advertising PDU */
    if (dmExtAdvCb[advHandle].incTxPwr)
    {
      /* set bit 6 */
      advEventProp |= HCI_ADV_PROP_INC_TX_PWR_BIT;
    }

    /* set primary advertising PHY */
    extAdvParam.priAdvPhy = dmExtAdvCb[advHandle].priAdvPhy;

    /* secondary parameters are only valid for extended advertising */
    extAdvParam.secAdvMaxSkip = dmExtAdvCb[advHandle].secAdvMaxSkip;
    extAdvParam.secAdvPhy = dmExtAdvCb[advHandle].secAdvPhy;
  }

  extAdvParam.advEventProp = advEventProp;

  /* min and max intervals are not used for high duty cycle connectable directed advertising */
  if ((advEventProp & HCI_ADV_PROP_CONN_DIRECT_ADV_BIT) == 0)
  {
    extAdvParam.priAdvInterMin = dmAdvCb.intervalMin[advHandle];
    extAdvParam.priAdvInterMax = dmAdvCb.intervalMax[advHandle];
  }

  extAdvParam.priAdvChanMap = dmAdvCb.channelMap[advHandle];
  extAdvParam.ownAddrType = DmLlAddrType(dmCb.advAddrType);
  extAdvParam.peerAddrType = peerAddrType;
  extAdvParam.pPeerAddr = pPeerAddr;
  extAdvParam.advFiltPolicy = dmCb.advFiltPolicy[advHandle];
  extAdvParam.advTxPwr = dmExtAdvCb[advHandle].advTxPwr;
  extAdvParam.advSID = dmExtAdvCb[advHandle].advSid;
  extAdvParam.scanReqNotifEna = dmExtAdvCb[advHandle].scanReqNotifEna;

  /* if event type doesn't support advertising data */
  if (!advDataAllowed                  &&
      dmExtAdvCb[advHandle].advDataSet &&
      (dmExtAdvCb[advHandle].advType != DM_ADV_NONE))
  {
    /* delete any existing advertising data */
    HciLeSetExtAdvDataCmd(advHandle, HCI_ADV_DATA_OP_COMP_FRAG, HCI_ADV_DATA_FRAG_PREF_FRAG,
                          0, NULL);
    dmExtAdvCb[advHandle].advDataSet = FALSE;
  }

  /* if event type doesn't support scan data */
  if (!scanDataAllowed                  &&
      dmExtAdvCb[advHandle].scanDataSet &&
      (dmExtAdvCb[advHandle].advType != DM_ADV_NONE))
  {
    /* delete any existing scan data */
    HciLeSetExtScanRespDataCmd(advHandle, HCI_ADV_DATA_OP_COMP_FRAG, HCI_ADV_DATA_FRAG_PREF_FRAG,
                               0, NULL);
    dmExtAdvCb[advHandle].scanDataSet = FALSE;
  }

  /* set extended advertising parameters */
  HciLeSetExtAdvParamCmd(advHandle, &extAdvParam);

  /* store advertising type here till advertising's enabled */
  dmExtAdvCb[advHandle].advType = advType;

  /* if local device's using an static address */
  if (DM_RAND_ADDR_SA(dmCb.localAddr, dmCb.advAddrType))
  {
    /* set static random address for advertising set */
    HciLeSetAdvSetRandAddrCmd(advHandle, dmCb.localAddr);
  }

  /* pass advertising set creation to dev priv */
  dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_CTRL, DM_DEV_PRIV_MSG_ADV_SET_ADD, advHandle,
                        DM_ADV_CONNECTABLE(advType));
}

/*************************************************************************************************/
/*!
 *  \brief  Check if a command complete is pending.
 *
 *  \param  None.
 *
 *  \return TRUE if command complete is pending else FALSE.
 */
/*************************************************************************************************/
static bool_t dmExtAdvCmdCmplPending(void)
{
  uint8_t i;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    if ((dmAdvCb.advState[i] != DM_ADV_STATE_IDLE) && (dmAdvCb.advState[i] != DM_ADV_STATE_ADVERTISING))
    {
      return TRUE;
    }
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Check if set's states matche the given state.
 *
 *  \param  numSets       Number of advertising sets to enable.
 *  \param  pAdvHandles   Advertising handles array.
 *
 *  \return None.
 */
/*************************************************************************************************/
static bool_t dmExtAdvCheckState(uint8_t numSets, uint8_t *pAdvHandles, uint8_t state)
{
  uint8_t i;

  /* 0 sets on stop is equivalent to all sets */
  if ((state == DM_ADV_STATE_ADVERTISING) && (numSets == 0))
  {
    for (i = 0; i < DM_NUM_ADV_SETS; i++)
    {
      if (dmAdvCb.advState[i] != DM_ADV_STATE_ADVERTISING)
      {
        return FALSE;
      }
    }
  }
  else
  {
    for (i = 0; i < numSets; i++)
    {
      if (dmAdvCb.advState[pAdvHandles[i]] != state)
      {
        return FALSE;
      }
    }
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the maximum advertising data length supported by Controller or allowed with HCI
 *          Set Extended Advertising/Scan Response Data command.
 *
 *  \param  advType      Advertising type.
 *  \param  useLegacyPdu Whether to use legacy advertising PDUs with extended advertising.
 *  \param  perSetCmd    Get maximum advertising data length allowed with HCI Set Extended
 *                       Advertising/Scan Response Data command.
 *
 *  \return Maximum advertising data length.
 */
/*************************************************************************************************/
static uint16_t dmExtMaxAdvDataLen(uint8_t advType, bool_t useLegacyPdu, bool_t perSetCmd)
{
  uint16_t maxLen;

  /* since high-duty cycle connectable directed advertising cannot be used with AE, legacy
     high-duty cycle connectable directed advertising PDUs will be used with AE instead */
  if ((advType == DM_ADV_CONN_DIRECT) || useLegacyPdu)
  {
    /* maximum advertising data length supported by Controller for legacy PDUs (i.e. 31 bytes) */
    maxLen = HCI_ADV_DATA_LEN;
  }
  else if (DM_ADV_CONNECTABLE(advType))
  {
    /* maximum advertising data length supported by Controller for connectable advertising (i.e.
       191 bytes) */
    maxLen = HCI_EXT_ADV_CONN_DATA_LEN;
  }
  else if (perSetCmd)
  {
    /* maximum number of octets in the Extended Advertising/Scan Response Data parameter */
    maxLen = HCI_EXT_ADV_DATA_LEN;
  }
  else
  {
    /* maximum advertisement data length supported by the Controller */
    maxLen = HciGetMaxAdvDataLen();
  }

  return maxLen;
}

/*************************************************************************************************/
/*!
 *  \brief  Start extended advertising.
 *
 *  \param  numSets       Number of advertising sets to enable.
 *  \param  pAdvHandles   Advertising handles array.
 *  \param  pDuration     Advertising duration (in milliseconds) array.
 *  \param  maxEaEvents   Maximum number of extended advertising events array.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmAdvStart(uint8_t numSets, uint8_t *pAdvHandles, uint16_t *pDuration,
                       uint8_t *pMaxEaEvents, uint8_t state)
{
  uint8_t i;
  hciExtAdvEnableParam_t enableParam[DM_NUM_ADV_SETS];

  for (i = 0; i < numSets; i++)
  {
    /* set the advertising enable parameter */
    enableParam[i].advHandle = pAdvHandles[i];
    enableParam[i].maxEaEvents = pMaxEaEvents[i];

    if (pDuration[i] == 0)
    {
      /* advertising to continue until it's disabled by Host */
      enableParam[i].duration = 0;
    }
    else
    {
      /* convert duration to 10 ms units but take the ceiling of duration */
      enableParam[i].duration = ((pDuration[i] - 1) / 10) + 1;
    }

    dmAdvCb.advState[pAdvHandles[i]] = state;
  }

  /* enable advertising */
  HciLeSetExtAdvEnableCmd(TRUE, numSets, enableParam);
}

/*************************************************************************************************/
/*!
 *  \brief  Stop extended advertising.  If the number of sets is set to 0 then all advertising sets
 *          are disabled.
 *
 *  \param  numSets       Number of advertising sets to disable.
 *  \param  pAdvHandles   Advertising handles array.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmAdvStop(uint8_t numSets, uint8_t *pAdvHandles, uint8_t state)
{
  uint8_t i;
  hciExtAdvEnableParam_t enableParam[DM_NUM_ADV_SETS];

  /* if number of advertising sets is 0 */
  if (numSets == 0)
  {
    /* disable all advertising sets */
    for (i = 0; i < DM_NUM_ADV_SETS; i++)
    {
      /* disable advertising set */
      dmAdvCb.advState[i] = state;
    }
  }
  else
  {
    /* disable specified advertising sets */
    for (i = 0; i < numSets; i++)
    {
      enableParam[i].advHandle = pAdvHandles[i];
      enableParam[i].duration = 0;
      enableParam[i].maxEaEvents = 0;

      /* disable advertising set */
      dmAdvCb.advState[pAdvHandles[i]] = state;
    }
  }

  /* disable advertising */
  HciLeSetExtAdvEnableCmd(FALSE, numSets, enableParam);
}

/*************************************************************************************************/
/*!
 *  \brief  Configure extended advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActConfig(dmAdvMsg_t *pMsg)
{
  uint8_t advHandle = pMsg->apiConfig.advHandle;

  /* if doing directed advertising ignore the request */
  if (dmAdvDirectedEnabled(1, &advHandle))
  {
    DM_TRACE_WARN0("DmAdvConfig during directed advertising!");
    return;
  }

  /* set extended advertising parameters (use legacy PDUs for high duty cycle directed adv) */
  dmAdvConfig(advHandle, pMsg->apiConfig.advType, pMsg->apiConfig.peerAddrType,
              pMsg->apiConfig.peerAddr, ((pMsg->apiConfig.advType == DM_ADV_CONN_DIRECT) ? \
              TRUE : dmExtAdvCb[advHandle].useLegacyPdu));
}

/*************************************************************************************************/
/*!
 *  \brief  Set the extended advertising or extended scan response data to the given data.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActSetData(dmAdvMsg_t *pMsg)
{
  uint8_t advType;
  uint8_t advHandle;
  bool_t deleteData;
  bool_t dataAllowed = FALSE;

  advHandle = pMsg->apiSetData.advHandle;
  advType = dmExtAdvCb[advHandle].advType;

  WSF_ASSERT(pMsg->apiSetData.len <= dmExtMaxAdvDataLen(advType, dmExtAdvCb[advHandle].useLegacyPdu, TRUE));

  /* if data to be deleted */
  if ((pMsg->apiSetData.op == HCI_ADV_DATA_OP_COMP_FRAG) && (pMsg->apiSetData.len == 0))
  {
    deleteData = TRUE;
  }
  else
  {
    deleteData = FALSE;
  }

  /* set new data in HCI */
  if (pMsg->apiSetData.location == DM_DATA_LOC_ADV)
  {
    /* if not deleting, find out if setting adv data is allowed with specified advertising type */
    if (!deleteData)
    {
      /* if using legacy PDUs with extended advertising */
      if (dmExtAdvCb[advHandle].useLegacyPdu)
      {
        /* advertising data is not allowed with connectable directed advertising type */
        if (!DM_ADV_CONN_DIRECTED(advType))
        {
          dataAllowed = TRUE;
        }
      }
      /* extended advertising */
      else
      {
        /* advertising data is only allowed with non-scannable advertising types */
        if (!DM_ADV_SCANNABLE(advType))
        {
          dataAllowed = TRUE;
        }
      }
    }

    /* if deleting advertising data or advertising data is allowed */
    if (deleteData || dataAllowed)
    {
      HciLeSetExtAdvDataCmd(advHandle, pMsg->apiSetData.op, dmExtAdvCb[advHandle].fragPref,
                            pMsg->apiSetData.len, pMsg->apiSetData.pData);
      dmExtAdvCb[advHandle].advDataSet = TRUE;
    }
  }
  else
  {
    /* if not deleting, find out if setting scan data is allowed with specified advertising type */
    if (!deleteData)
    {
      /* if using legacy PDUs with extended advertising */
      if (dmExtAdvCb[advHandle].useLegacyPdu)
      {
        /* scan data is only allowed with connectable and scannable undirected advertising and
           scannable undirected advertising types */
        if ((advType == DM_ADV_CONN_UNDIRECT) || (advType == DM_ADV_SCAN_UNDIRECT))
        {
          dataAllowed = TRUE;
        }
      }
      /* extended advertising */
      else
      {
        /* scan data is only allowed with scannable advertising types */
        if (DM_ADV_SCANNABLE(advType))
        {
          dataAllowed = TRUE;
        }
      }
    }

    /* if deleting scan data or scan data is allowed with specified advertising type */
    if (deleteData || dataAllowed)
    {
      HciLeSetExtScanRespDataCmd(advHandle, pMsg->apiSetData.op, dmExtAdvCb[advHandle].fragPref,
                                 pMsg->apiSetData.len, pMsg->apiSetData.pData);
      dmExtAdvCb[advHandle].scanDataSet = TRUE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start extended advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActStart(dmAdvMsg_t *pMsg)
{
  uint8_t numSets = pMsg->apiStart.numSets;
  uint8_t *pHandles = pMsg->apiStart.advHandle;

  if (dmExtAdvCmdCmplPending() || dmExtAdvCheckState(numSets, pHandles, DM_ADV_STATE_IDLE) == FALSE)
  {
    DM_TRACE_WARN0("DmAdvStart ignored due to advState or pending command complete");
    return;
  }

  /* if doing directed advertising ignore the request */
  if (dmAdvDirectedEnabled(numSets, pHandles))
  {
    DM_TRACE_WARN0("DmAdvStart during directed advertising!");
    return;
  }

  /* store current advertising state */
  dmAdvCb.advEnabled = dmAdvEnabled();

  /* start advertising */
  dmAdvStart(numSets, pHandles, pMsg->apiStart.duration, pMsg->apiStart.maxEaEvents,
             DM_ADV_STATE_STARTING);
}

/*************************************************************************************************/
/*!
 *  \brief  Stop extended advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActStop(dmAdvMsg_t *pMsg)
{
  uint8_t i;
  uint8_t numSets = 0;
  uint8_t advHandle[DM_NUM_ADV_SETS] = {0};

  /* find out which of advertising set's currently enabled */
  for (i = 0; i < pMsg->apiStop.numSets; i++)
  {
    /* if doing advertising */
    if (dmAdvCb.advType[pMsg->apiStop.advHandle[i]] != DM_ADV_NONE)
    {
      advHandle[numSets++] = pMsg->apiStop.advHandle[i];
    }
  }

  if (dmExtAdvCmdCmplPending() || dmExtAdvCheckState(numSets, advHandle, DM_ADV_STATE_ADVERTISING) == FALSE)
  {
    DM_TRACE_WARN0("DmAdvStop ignored due to advState or pending command complete");
    return;
  }

  /* if doing directed advertising ignore the request */
  if (dmAdvDirectedEnabled(numSets, advHandle))
  {
    DM_TRACE_WARN0("DmAdvStop during directed advertising!");
    return;
  }

  /* if advertising's enabled for any set */
  if ((pMsg->apiStop.numSets == 0) || (numSets > 0))
  {
    /* stop advertising */
    dmAdvStop(numSets, advHandle, DM_ADV_STATE_STOPPING);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Remove an advertising set action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActRemoveSet(dmAdvMsg_t *pMsg)
{
  uint8_t advHandle = pMsg->apiRemove.advHandle;

  /* if already advertising */
  if (dmExtAdvCheckState(1, &advHandle, DM_ADV_STATE_ADVERTISING))
  {
    if (dmExtAdvCmdCmplPending() == FALSE)
    {
      /* stop advertising */
      dmAdvStop(1, &advHandle, DM_ADV_STATE_REMOVING_SET);
    }
  }
  else
  {
    /* remove advertising set */
    HciLeRemoveAdvSet(advHandle);

    /* clear advertising set record */
    dmAdvCbInit(advHandle);
    dmExtAdvCbInit(advHandle);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Clear advertising sets action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActClearSets(dmAdvMsg_t *pMsg)
{
  if (dmExtAdvCheckState(HCI_ADV_NUM_SETS_ALL_DISABLE, NULL, DM_ADV_STATE_ADVERTISING))
  {
    if (dmExtAdvCmdCmplPending() == FALSE)
    {
      /* Stop advertising and set DM_ADV_STATE_CLEARING_SETS state  */
      dmAdvStop(HCI_ADV_NUM_SETS_ALL_DISABLE, NULL, DM_ADV_STATE_CLEARING_SETS);
    }
  }
  else
  {
    uint8_t i;

    /* clear all advertising set records */
    for (i = 0; i < DM_NUM_ADV_SETS; i++)
    {
      dmAdvCbInit(i);
      dmExtAdvCbInit(i);
    }

    /* clear all advertising sets */
    HciLeClearAdvSets();
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the random device address for a given advertising set.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActSetRandAddr(dmAdvMsg_t *pMsg)
{
  /* set new address in HCI */
  HciLeSetAdvSetRandAddrCmd(pMsg->apiSetRandAddr.advHandle, pMsg->apiSetRandAddr.addr);
}

/*************************************************************************************************/
/*!
 *  \brief  Start periodic advertising for the specified advertising handle.
 *
 *  \param  advHandle    Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void dmPerAdvStart(uint8_t advHandle)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS)

  /* start periodic advertising */
  dmPerAdvCb[advHandle].advState = DM_ADV_PER_STATE_STARTING;
  HciLeSetPerAdvEnableCmd(TRUE, advHandle);
}

/*************************************************************************************************/
/*!
*  \brief  Stop periodic advertising for the specified advertising handle.
*
*  \param  advHandle    Advertising handle.
*
*  \return None.
*/
/*************************************************************************************************/
static void dmPerAdvStop(uint8_t advHandle)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS)

  /* stop periodic advertising */
  dmPerAdvCb[advHandle].advState = DM_ADV_PER_STATE_STOPPING;
  HciLeSetPerAdvEnableCmd(FALSE, advHandle);
}

/*************************************************************************************************/
/*!
 *  \brief  Get the handle of a pending periodic advertising action.
 *
 *  \param  None.
 *
 *  \return Handle or DM_NUM_ADV_SETS if none are pending.
 */
/*************************************************************************************************/
static uint8_t dmPerAdvCmdCmplPending(void)
{
  uint8_t i;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    if ((dmPerAdvCb[i].advState != DM_ADV_PER_STATE_IDLE) &&
        (dmPerAdvCb[i].advState != DM_ADV_PER_STATE_ADVERTISING))
    {
      return i;
    }
  }

  return DM_NUM_ADV_SETS;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the state for the specified periodic advertising handle.
 *
 *  \param  advHandle    Advertising handle.
 *
 *  \return Advertising state.
 */
/*************************************************************************************************/
uint8_t dmPerAdvState(uint8_t advHandle)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS)

  /* return state */
  return dmPerAdvCb[advHandle].advState;
}

/*************************************************************************************************/
/*!
 *  \brief  Configure periodic advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvActConfig(dmAdvMsg_t *pMsg)
{
  uint8_t advHandle;
  uint16_t advProps;

  /* initialize local variables */
  advHandle = pMsg->apiPerConfig.advHandle;
  advProps = 0;

  /* include TxPower in extended header of advertising PDU */
  if (dmPerAdvCb[advHandle].incTxPwr)
  {
    /* set bit 6 */
    advProps |= HCI_ADV_PROP_INC_TX_PWR_BIT;
  }

  /* set periodic advertising parameters */
  HciLeSetPerAdvParamCmd(advHandle, dmPerAdvCb[advHandle].intervalMin,
                         dmPerAdvCb[advHandle].intervalMax, advProps);
}

/*************************************************************************************************/
/*!
 *  \brief  Set the periodic advertising data to the given data.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvActSetData(dmAdvMsg_t *pMsg)
{
  /* set new data in HCI */
  HciLeSetPerAdvDataCmd(pMsg->apiPerSetData.advHandle, pMsg->apiPerSetData.op,
                        pMsg->apiPerSetData.len, pMsg->apiPerSetData.pData);
}

/*************************************************************************************************/
/*!
 *  \brief  Start periodic advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvActStart(dmAdvMsg_t *pMsg)
{
  /* if any pending periodic advertising command complete */
  if (dmPerAdvCmdCmplPending() < DM_NUM_ADV_SETS)
  {
    DM_TRACE_WARN0("DmPerAdvStart ignored due to advState or pending command complete");
    return;
  }

  /* start periodic advertising */
  dmPerAdvStart(pMsg->apiPerStart.advHandle);
}

/*************************************************************************************************/
/*!
 *  \brief  Stop periodic advertising action function.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvActStop(dmAdvMsg_t *pMsg)
{
  /* if any pending periodic advertising command complete */
  if (dmPerAdvCmdCmplPending() < DM_NUM_ADV_SETS)
  {
    DM_TRACE_WARN0("DmPerAdvStop ignored due to advState or pending command complete");
    return;
  }

  /* stop periodic advertising */
  dmPerAdvStop(pMsg->apiPerStop.advHandle);
}

/*************************************************************************************************/
/*!
 *  \brief  Handle an advertising timeout.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActTimeout(dmAdvMsg_t *pMsg)
{
  /* empty */
}

/*************************************************************************************************/
/*!
 *  \brief  DM extended advertising HCI adv enbale command complete event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvActHciEnableCmpl(hciEvt_t *pEvent)
{
  uint8_t i;
  uint8_t advType;
  uint8_t cbackEvent = 0;
  bool_t clearSets = FALSE;
  dmEvt_t dmMsg;

  memcpy(&dmMsg, &pEvent->hdr, sizeof(wsfMsgHdr_t));

  /* we have to handle stopping and starting separately as it uses the same message memory */

  for ( i= 0; i < DM_NUM_ADV_SETS; i++)
  {
    switch(dmAdvCb.advState[i])
    {
    case DM_ADV_STATE_STOPPING:
    case DM_ADV_STATE_STOPPING_DIRECTED:
      /* prepare the message for callback */
      dmMsg.advSetStop.handle = DM_ADV_HCI_HANDLE_NONE;
      dmMsg.advSetStop.status = dmMsg.hdr.status;
      dmMsg.advSetStop.advHandle = i;

      advType = dmAdvCb.advType[i];

      if (dmMsg.hdr.status == HCI_SUCCESS)
      {
        dmAdvCb.advType[i] = DM_ADV_NONE;
        dmAdvCb.advState[i] = DM_ADV_STATE_IDLE;
        dmExtAdvCb[i].connId = DM_CONN_ID_NONE;

        /* pass advertising stop event to dev priv */
        dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_STOP, DM_ADV_SET_STOP_IND, i, 0);
      }
      else
      {
        dmAdvCb.advState[i] = DM_ADV_STATE_ADVERTISING;
      }

      /* if not connectable directed advertising */
      if ((advType != DM_ADV_NONE) && !DM_ADV_CONN_DIRECTED(advType))
      {
        /* we have to dispatch callbacks one by one as msg only has space for one set of parameters */
        dmMsg.hdr.event = DM_ADV_SET_STOP_IND;
        (*dmCb.cback)((dmEvt_t *) &dmMsg);
      }
      break;

    default:
      break;
    }
  }

  /* safe to write as message is only used by starting, removing and clearing does not send a message */
  dmMsg.advSetStart.numSets = 0;

  for ( i= 0; i < DM_NUM_ADV_SETS; i++)
  {
    switch(dmAdvCb.advState[i])
    {
    case DM_ADV_STATE_STARTING:
    case DM_ADV_STATE_STARTING_DIRECTED:
      dmMsg.advSetStart.advHandle[dmMsg.advSetStart.numSets++] = i;

      if (dmMsg.hdr.status == HCI_SUCCESS)
      {
        /* pass advertising start event to dev priv */
        dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_START, DM_ADV_SET_START_IND, i, 0);

        /* store advertising type */
        dmAdvCb.advType[i] = dmExtAdvCb[i].advType;
        dmAdvCb.advState[i] = DM_ADV_STATE_ADVERTISING;
      }
      else
      {
        dmAdvCb.advState[i] = DM_ADV_STATE_IDLE;
      }

      /* if not connectable directed advertising */
      if (!DM_ADV_CONN_DIRECTED(dmExtAdvCb[i].advType))
      {
        cbackEvent = DM_ADV_SET_START_IND;
      }
      break;

    case DM_ADV_STATE_REMOVING_SET:
    case DM_ADV_STATE_CLEARING_SETS:
      if (dmMsg.hdr.status == HCI_SUCCESS)
      {
        dmAdvCbInit(i);
        dmExtAdvCbInit(i);

        if (dmAdvCb.advState[i] == DM_ADV_STATE_REMOVING_SET)
        {
          HciLeRemoveAdvSet(i);

          /* pass advertising set removed to dev priv */
          dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_CTRL, DM_DEV_PRIV_MSG_ADV_SET_REMOVE, i, 0);
        }
        else
        {
          clearSets = TRUE;
        }

        dmAdvCb.advType[i] = DM_ADV_NONE;
        dmAdvCb.advState[i] = DM_ADV_STATE_IDLE;
        dmExtAdvCb[i].connId = DM_CONN_ID_NONE;
      }
      else
      {
        dmAdvCb.advState[i] = DM_ADV_STATE_ADVERTISING;
      }
      break;

    default:
      break;
    }
  }

  /* clear all advertising sets */
  if (clearSets)
  {
    HciLeClearAdvSets();

    /* pass advertising sets cleared to dev priv */
    dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_CTRL, DM_DEV_PRIV_MSG_ADV_SETS_CLEAR, 0, 0);
  }

  /* call app callback */
  if (cbackEvent)
  {
    dmMsg.hdr.event = cbackEvent;
    (*dmCb.cback)((dmEvt_t *) &dmMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM extended advertising HCI event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvHciHandler(hciEvt_t *pEvent)
{
  if (pEvent->hdr.event == HCI_LE_ADV_SET_TERM_CBACK_EVT)
  {
    uint8_t advHandle = pEvent->leAdvSetTerm.advHandle;
    uint8_t advType = dmAdvCb.advType[advHandle];

    DM_TRACE_INFO2("dmExtAdvHciHandler: event: %d state: %d", pEvent->hdr.event, dmAdvCb.advState[advHandle]);

    /* clear advertising info */
    dmAdvCb.advType[advHandle] = DM_ADV_NONE;
    dmAdvCb.advState[advHandle] = DM_ADV_STATE_IDLE;
    dmExtAdvCb[advHandle].connId = DM_CONN_ID_NONE;

    /* pass advertising stop event to dev priv */
    dmDevPassEvtToDevPriv(DM_DEV_PRIV_MSG_RPA_STOP, DM_ADV_SET_STOP_IND, advHandle, 0);

    if (advType != DM_ADV_NONE)
    {
      /* if not connectable directed advertising */
      if (!DM_ADV_CONN_DIRECTED(advType))
      {
        pEvent->hdr.event = DM_ADV_SET_STOP_IND;
        if (pEvent->leAdvSetTerm.status == HCI_SUCCESS) {
            /* translate the handle to conn id */
            dmConnCcb_t* ccb = dmConnCcbByHandle(pEvent->leAdvSetTerm.handle);
            if (ccb) {
                pEvent->hdr.param = ccb->connId;
            }
        }
        (*dmCb.cback)((dmEvt_t *) pEvent);
      }
      /* else if low duty cycle directed advertising failed to create connection */
      else if (pEvent->leAdvSetTerm.status != HCI_SUCCESS)
      {
        /* generate connection compelete event */
        dmAdvGenConnCmpl(advHandle, pEvent->leAdvSetTerm.status);
      }
    }
  }
  else if (pEvent->hdr.event == HCI_LE_SCAN_REQ_RCVD_CBACK_EVT)
  {
    DM_TRACE_INFO2("dmExtAdvHciHandler: event: %d state: %d", pEvent->hdr.event, dmAdvCb.advState[pEvent->leScanReqRcvd.advHandle]);

    pEvent->hdr.event = DM_SCAN_REQ_RCVD_IND;
    (*dmCb.cback)((dmEvt_t *) pEvent);
  }
  else if (pEvent->hdr.event == HCI_LE_EXT_ADV_ENABLE_CMD_CMPL_CBACK_EVT)
  {
    DM_TRACE_INFO1("dmExtAdvHciHandler: event: %d", pEvent->hdr.event);

    dmExtAdvActHciEnableCmpl(pEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM extended adv event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvMsgHandler(wsfMsgHdr_t *pMsg)
{
  /* execute action function */
  (*dmAdvAct[DM_MSG_MASK(pMsg->event)])((dmAdvMsg_t *)pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the extended advertising module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvReset(void)
{
  hciLeAdvSetTermEvt_t  advSetStop;
  uint8_t               i;

  /* generate advertising set stopped event */
  advSetStop.hdr.event = DM_ADV_SET_STOP_IND;
  advSetStop.hdr.status = advSetStop.status = HCI_SUCCESS;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    /* if stopping advertisement or advertising */
    if ((dmAdvCb.advState[i] == DM_ADV_STATE_STOPPING)          ||
        (dmAdvCb.advState[i] == DM_ADV_STATE_STOPPING_DIRECTED) ||
        (dmAdvCb.advState[i] == DM_ADV_STATE_ADVERTISING))
    {
      /* set advertising handle */
      advSetStop.hdr.param = advSetStop.advHandle = i;

      /* call callback */
      (*dmCb.cback)((dmEvt_t *) &advSetStop);
    }

    /* initialize extended control block */
    dmExtAdvCbInit(i);
  }

  /* reset legacy adv module */
  dmAdvInit();
}

/*************************************************************************************************/
/*!
 *  \brief  DM periodic advertising HCI event handler.
 *
 *  \param  pEvent  Pointer to HCI callback event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvHciHandler(hciEvt_t *pEvent)
{
  if (pEvent->hdr.event == HCI_LE_PER_ADV_ENABLE_CMD_CMPL_CBACK_EVT)
  {
    uint8_t advHandle = dmPerAdvCmdCmplPending();

    /* if pending periodic advertising enable command complete */
    if (advHandle < DM_NUM_ADV_SETS)
    {
      dmEvt_t dmMsg;

      /* copy over event header */
      memcpy(&dmMsg, &pEvent->hdr, sizeof(wsfMsgHdr_t));

      switch (dmPerAdvCb[advHandle].advState)
      {
      case DM_ADV_PER_STATE_STARTING:
        dmMsg.perAdvSetStart.advHandle = advHandle;
        dmMsg.hdr.event = DM_PER_ADV_SET_START_IND;
        dmPerAdvCb[advHandle].advState = (dmMsg.hdr.status == HCI_SUCCESS) ? \
                                         DM_ADV_PER_STATE_ADVERTISING : DM_ADV_PER_STATE_IDLE;
        /* call client callback */
        (*dmCb.cback)(&dmMsg);
        break;

      case DM_ADV_PER_STATE_STOPPING:
        dmMsg.perAdvSetStop.advHandle = advHandle;
        dmMsg.hdr.event = DM_PER_ADV_SET_STOP_IND;
        dmPerAdvCb[advHandle].advState = (dmMsg.hdr.status == HCI_SUCCESS) ? \
                                         DM_ADV_PER_STATE_IDLE : DM_ADV_PER_STATE_ADVERTISING;
        /* call client callback */
        (*dmCb.cback)(&dmMsg);
        break;

      default:
        /* Should not happen */
        WSF_ASSERT(0);
        break;
      }
    }
    else
    {
      DM_TRACE_WARN0("dmPerAdvHciHandler ignored due to no pending enable command complete");
    }
  }
}

/*************************************************************************************************/
/*!
 *  \brief  DM periodic adv event handler.
 *
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvMsgHandler(wsfMsgHdr_t *pMsg)
{
  /* execute action function */
  (*dmPerAdvAct[DM_MSG_MASK(pMsg->event)])((dmAdvMsg_t *)pMsg);
}

/*************************************************************************************************/
/*!
 *  \brief  Reset the periodic advertising module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmPerAdvReset(void)
{
  dmPerAdvSetStopEvt_t  perAdvSetStop;
  uint8_t               i;

  /* generate periodic advertising set stopped event */
  perAdvSetStop.hdr.event = DM_PER_ADV_SET_STOP_IND;
  perAdvSetStop.hdr.status = HCI_SUCCESS;

  for (i = 0; i < DM_NUM_ADV_SETS; i++)
  {
    /* if periodic advertising enabled */
    if ((dmPerAdvCb[i].advState == DM_ADV_PER_STATE_STOPPING) ||
        (dmPerAdvCb[i].advState == DM_ADV_PER_STATE_ADVERTISING))
    {
      /* set advertising handle */
      perAdvSetStop.hdr.param = perAdvSetStop.advHandle = i;

      /* call callback */
      (*dmCb.cback)((dmEvt_t *) &perAdvSetStop);
    }

    /* initialize periodic control block */
    dmPerAdvCbInit(i);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start directed extended advertising.
 *
 *  \param  connId        Connection identifer.
 *  \param  advHandle     Advertising handle.
 *  \param  advType       Advertising type.
 *  \param  duration      Advertising duration (in milliseconds).
 *  \param  maxEaEvents   Maximum number of extended advertising events.
 *  \param  addrType      Address type.
 *  \param  pAddr         Peer device address.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvStartDirected(dmConnId_t connId, uint8_t advHandle, uint8_t advType, uint16_t duration,
                           uint8_t maxEaEvents, uint8_t addrType, uint8_t *pAddr)
{
  if (dmExtAdvCmdCmplPending() || dmExtAdvCheckState(1, &advHandle, DM_ADV_STATE_IDLE) == FALSE)
  {
    return;
  }

  /* start directed advertising */
  dmAdvStart(1, &advHandle, &duration, &maxEaEvents, ((advType == DM_ADV_CONN_DIRECT) ? \
             DM_ADV_STATE_STARTING_DIRECTED : DM_ADV_STATE_STARTING));

  /* store advertising info */
  BdaCpy(dmAdvCb.peerAddr[advHandle], pAddr);
  dmAdvCb.peerAddrType[DM_ADV_HANDLE_DEFAULT] = addrType;
  dmExtAdvCb[advHandle].connId = connId;
}

/*************************************************************************************************/
/*!
 *  \brief  Stop directed extended advertising.
 *
 *  \param  connId        Connection identifer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvStopDirected(dmConnId_t connId)
{
  uint8_t advHandle;

  if (dmExtAdvCmdCmplPending())
  {
    return;
  }

  /* see if high duty cycle directed advertising's enabled */
  advHandle = dmAdvDirectedByConnId(connId);
  if (advHandle < DM_NUM_ADV_SETS)
  {
    /* stop directed advertising */
    dmAdvStop(1, &advHandle, ((dmAdvCb.advType[advHandle] == DM_ADV_CONN_DIRECT) ? \
              DM_ADV_STATE_STOPPING_DIRECTED : DM_ADV_STATE_STOPPING));
  }
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called when a connection is established from directed advertising.
 *
 *  \param  connId        Connection identifer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvConnected(dmConnId_t connId)
{
  dmAdvDirectedDisable(connId);
}

/*************************************************************************************************/
/*!
 *  \brief  This function is called when a directed advertising connection fails.
 *
 *  \param  connId        Connection identifer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void dmExtAdvConnectFailed(dmConnId_t connId)
{
  dmAdvDirectedDisable(connId);
}

/*************************************************************************************************/
/*!
 *  \brief  Set the advertising parameters for periodic advertising.
 *
 *  \param  advHandle     Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPerAdvConfig(uint8_t advHandle)
{
  dmAdvPerApiConfig_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvPerApiConfig_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_PER_MSG_API_CONFIG;
    pMsg->advHandle = advHandle;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set the advertising data to the given data for periodic advertising.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  op            Data operation.
 *  \param  len           Length of the data.  Maximum length is 236 bytes.
 *  \param  pData         Pointer to the data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPerAdvSetData(uint8_t advHandle, uint8_t op, uint8_t len, uint8_t *pData)
{
  dmAdvPerApiSetData_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);
  WSF_ASSERT(len <= HCI_PER_ADV_DATA_LEN);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvPerApiSetData_t) + len)) != NULL)
  {
    pMsg->hdr.event = DM_ADV_PER_MSG_API_SET_DATA;
    pMsg->advHandle = advHandle;
    pMsg->op = op;
    pMsg->len = len;
    memcpy(pMsg->pData, pData, len);
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Start periodic advertising for the advertising set specified by the advertising handle.
 *
 *  \param  advHandle     Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPerAdvStart(uint8_t advHandle)
{
  dmAdvPerApiStart_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvPerApiStart_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_PER_MSG_API_START;
    pMsg->advHandle = advHandle;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Stop periodic advertising for the advertising set specified by the advertising handle.
 *
 *  \param  advHandle     Advertising handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPerAdvStop(uint8_t advHandle)
{
  dmAdvPerApiStop_t *pMsg;

  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  if ((pMsg = WsfMsgAlloc(sizeof(dmAdvPerApiStop_t))) != NULL)
  {
    pMsg->hdr.event = DM_ADV_PER_MSG_API_STOP;
    pMsg->advHandle = advHandle;
    WsfMsgSend(dmCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Set whether or not to use legacy advertising PDUs with extended advertising.
 *
 *  \param  advHandle    Advertising handle.
 *  \param  useLegacyPdu Whether to use legacy advertising PDUs (default value is TRUE).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvUseLegacyPdu(uint8_t advHandle, bool_t useLegacyPdu)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].useLegacyPdu = useLegacyPdu;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set whether or not to omit advertiser's address from all PDUs (anonymous advertising).
 *
 *  \param  advHandle    Advertising handle.
 *  \param  omitAdvAddr  Whether to omit advertiser's address from all PDUs (default value is FALSE).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvOmitAdvAddr(uint8_t advHandle, bool_t omitAdvAddr)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].omitAdvAddr = omitAdvAddr;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set whether or not to include TxPower in extended header of advertising PDU.
 *
 *  \param  advHandle    Advertising handle.
 *  \param  incTxPwr     Whether to include TxPower in extended header of advertising PDU (default
 *                       value is FALSE).
 *  \param  advTxPwr     Advertising tx power (127 = no preference).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvIncTxPwr(uint8_t advHandle, bool_t incTxPwr, int8_t advTxPwr)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].incTxPwr = incTxPwr;
  dmExtAdvCb[advHandle].advTxPwr = advTxPwr;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set extended advertising PHY parameters.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  priAdvPhy     Primary advertising Phy.
 *  \param  secAdvMaxSkip Maximum advertising events Controller can skip before sending AUX_ADV_IND
 *                        on secondary advertising channel (0 = AUX_ADV_IND will be sent prior to
 *                        next advertising event).
 *  \param  secAdvPhy     Secondary advertising Phy.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetPhyParam(uint8_t advHandle, uint8_t priAdvPhy, uint8_t secAdvMaxSkip, uint8_t secAdvPhy)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].priAdvPhy = priAdvPhy;
  dmExtAdvCb[advHandle].secAdvMaxSkip = secAdvMaxSkip;
  dmExtAdvCb[advHandle].secAdvPhy = secAdvPhy;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set scan request notification enable.
 *
 *  \param  advHandle       Advertising handle.
 *  \param  scanReqNotifEna Scan request notification enable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvScanReqNotifEnable(uint8_t advHandle, bool_t scanReqNotifEna)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].scanReqNotifEna = scanReqNotifEna;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set fragment preference for advertising data.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  fragPref      Fragment preference.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetFragPref(uint8_t advHandle, uint8_t fragPref)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].fragPref = fragPref;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set advertising SID for the given advertising handle.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  advSid        Advertsing SID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmAdvSetSid(uint8_t advHandle, uint8_t advSid)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmExtAdvCb[advHandle].advSid = advSid;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set the minimum and maximum advertising intervals for periodic advertising.
 *
 *  \param  advHandle     Advertising handle.
 *  \param  intervalMin   Minimum advertising interval.
 *  \param  intervalMax   Maximum advertising interval.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPerAdvSetInterval(uint8_t advHandle, uint16_t intervalMin, uint16_t intervalMax)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmPerAdvCb[advHandle].intervalMin = intervalMin;
  dmPerAdvCb[advHandle].intervalMax = intervalMax;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Set whether or not to include TxPower in extended header of advertising PDU for
 *          periodic advertising.
 *
 *  \param  advHandle    Advertising handle.
 *  \param  incTxPwr     Whether to include TxPower in extended header of advertising PDU (default
 *                       value is FALSE).
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmPerAdvIncTxPwr(uint8_t advHandle, bool_t incTxPwr)
{
  WSF_ASSERT(advHandle < DM_NUM_ADV_SETS);

  WsfTaskLock();
  dmPerAdvCb[advHandle].incTxPwr = incTxPwr;
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Get status of periodic advertising handle.
 *
 *  \param  advHandle    Advertising handle.
 *
 *  \return TRUE if periodic advertising is running on that handle. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t DmPerAdvEnabled(uint8_t advHandle)
{
  if (advHandle < DM_NUM_ADV_SETS)
  {
    return (dmPerAdvCb[advHandle].advState == DM_ADV_PER_STATE_ADVERTISING);
  }

  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \brief  Get the maximum advertising data length supported by Controller for a given advertising
 *          type.
 *
 *  \param  advType      Advertising type.
 *  \param  useLegacyPdu Whether to use legacy advertising PDUs with extended advertising.
 *
 *  \return Maximum advertising data length.
 */
/*************************************************************************************************/
uint16_t DmExtMaxAdvDataLen(uint8_t advType, bool_t useLegacyPdu)
{
  return dmExtMaxAdvDataLen(advType, useLegacyPdu, FALSE);
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize DM extended advertising.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DmExtAdvInit(void)
{
  WsfTaskLock();

  /* set function interface table */
  dmFcnIfTbl[DM_ID_ADV] = (dmFcnIf_t *) &dmAdvFcnIf;
  dmFcnIfTbl[DM_ID_ADV_PER] = (dmFcnIf_t *) &dmPerAdvFcnIf;

  /* initialize extended/periodic adv modules */
  dmExtAdvInit();
  dmPerAdvInit();

  /* register set advertising set random address callback with DM Dev Privacy */
  dmDevCb.advSetRandAddrCback = HciLeSetAdvSetRandAddrCmd;

  /* initialize HCI VS AE module */
  HciVsAeInit(0);

  HciSetLeSupFeat((HCI_LE_SUP_FEAT_LE_EXT_ADV | HCI_LE_SUP_FEAT_LE_PER_ADV), TRUE);

  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \brief  Whether DM advertising is in extended mode.
 *
 *  \return TRUE if DM advertising is in extended mode. FALSE, otherwise.
 */
/*************************************************************************************************/
bool_t DmAdvModeExt(void)
{
  return (dmFcnIfTbl[DM_ID_ADV] == (dmFcnIf_t *) &dmAdvFcnIf) ? TRUE : FALSE;
}
