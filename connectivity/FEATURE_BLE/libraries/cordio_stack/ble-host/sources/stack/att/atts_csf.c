/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT client supported features module.
 *
 *  Copyright (c) 2019 Arm Ltd. All Rights Reserved.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "att_api.h"
#include "att_main.h"
#include "atts_main.h"
#include "util/bstream.h"
#include "svc_core.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
attsCsfCb_t attsCsfCb;

/*************************************************************************************************/
/*!
 *  \brief  Set status of on-going database hash update operation.
 *
 *  \param  isUpdating      \ref TRUE is updating, otherwise \ref FALSE;
 *
 *  \return None.
 */
/*************************************************************************************************/
void attsCsfSetHashUpdateStatus(bool_t isUpdating)
{
  if (attsCsfCb.isHashUpdating == isUpdating)
  {
    /* Already in the current state, nothing to do. */
    return;
  }
  else
  {
    /* Update state. */
    attsCsfCb.isHashUpdating = isUpdating;
  }

  /* Update complete.
   * Check if clients were pending on the hash value and fulfill their requests.
   */
  if (isUpdating == FALSE)
  {
    ATT_TRACE_INFO0("Database hash calculation complete");

    attsCheckPendDbHashReadRsp();

    /* Note: Clients which were pending on a Database Hash read from a Read by Type Request are not
     * transitioned to the change-aware state here.  The application is expected to initiate the
     * state transition of all clients when the new hash is set.  If this is not done, the
     * state of pending Clients will be out of sync, and will be corrected on the next database
     * sync.
     */
  }
  else
  {
    ATT_TRACE_INFO0("Calculating database hash");

    /* If the application, for whatever reason, previously recalculated the database hash over an
     * unchanged database and a client pended on a Read By Type Request of the database hash, then
     * that clients state may be out of step if the application did not initiate a state
     * transition.  That state transition is forced here to keep handle next transition.
     */
    for (uint8_t i = 0; i < DM_CONN_MAX; i++)
    {
      if (attsCsfCb.attsCsfTable[i].changeAwareState == ATTS_CLIENT_CHANGE_AWARE_DB_READ_PENDING)
      {
        attsCsfCb.attsCsfTable[i].changeAwareState = ATTS_CLIENT_CHANGE_PENDING_AWARE;
      }
    }
  }
}

/*************************************************************************************************/
/*!
*  \brief  Check if database hash update is in progress.
*
*  \return \ref TRUE if update in progress, \ref FALSE otherwise.
*/
/*************************************************************************************************/
uint8_t attsCsfGetHashUpdateStatus(void)
{
  return attsCsfCb.isHashUpdating;
}

/*************************************************************************************************/
/*!
 *  \brief  Check client awareness to database hash before sending notification or indication.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      ATT handle.
 *
 *  \return \ref TRUE if client is aware, otherwise \ref FALSE.
 */
/*************************************************************************************************/
uint8_t attsCsfIsClientChangeAware(dmConnId_t connId, uint16_t handle)
{
  if ((attsCsfCb.attsCsfTable[connId - 1].csf & ATTS_CSF_ROBUST_CACHING) &&
      (attsCsfCb.attsCsfTable[connId - 1].changeAwareState == ATTS_CLIENT_CHANGE_UNAWARE) &&
      (handle != GATT_SC_HDL))
  {
    return FALSE;
  }

  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \brief  Update client change-aware state based on protocol event.
 *
 *  \param  connId      Connection ID.
 *  \param  opcode      ATT PDU type.
 *  \param  pPacket     Data packet from L2CAP.
 *
 *  \return \ref ATT_SUCCESS if client is change-aware, else \ref ATT_ERR_DATABASE_OUT_OF_SYNC.
 */
/*************************************************************************************************/
uint8_t attsCsfActClientState(dmConnId_t connId, uint8_t opcode, uint8_t *pPacket)
{
  uint8_t err = ATT_SUCCESS;
  attsCsfRec_t *pRec;

  /* PDU which do not operate on att handles are handled agnostically of the client's state. */
  if (opcode == ATT_PDU_MTU_REQ || opcode == ATT_PDU_VALUE_CNF)
  {
    return err;
  }

  pRec = &attsCsfCb.attsCsfTable[connId - 1];

  /* If the client is change-unaware */
  if (pRec->changeAwareState == ATTS_CLIENT_CHANGE_UNAWARE)
  {
    /* If not a command */
    if ((opcode & ATT_PDU_MASK_COMMAND) == 0)
    {
      /* Note: there is no need to call back to the application here.  The application only
       * needs to know when a transition to or from the change-aware state occurs.
       */

      /* Move client change-aware state to pending */
      pRec->changeAwareState = ATTS_CLIENT_CHANGE_PENDING_AWARE;

      ATT_TRACE_INFO2("ConnId %d change aware state is %d", connId,
                      ATTS_CLIENT_CHANGE_PENDING_AWARE);
    }

    /* If this is a command or the Client has indicated Robust Caching, set an error so that
     * this command or request is not processed.
     */
    if ((opcode & ATT_PDU_MASK_COMMAND) ||
        (pRec->csf & ATTS_CSF_ROBUST_CACHING))
    {
      /* return a database out of sync error */
      err = ATT_ERR_DATABASE_OUT_OF_SYNC;
    }
  }
  else if (pRec->changeAwareState == ATTS_CLIENT_CHANGE_PENDING_AWARE)
  {
    /* If not a command */
    if ((opcode & ATT_PDU_MASK_COMMAND) == 0)
    {
      /* Move client change-aware state to aware */
      pRec->changeAwareState = ATTS_CLIENT_CHANGE_AWARE;

      ATT_TRACE_INFO2("ConnId %d change aware state is %d", connId, ATTS_CLIENT_CHANGE_AWARE);

      /* Callback to application to store updated awareness, if bonded. */
      if (attsCsfCb.writeCback != NULL)
      {
        attsCsfCb.writeCback(connId, pRec->changeAwareState, &pRec->csf);
      }
    }
    else
    {
      /* Return an error so that command is not processed. */
      err = ATT_ERR_DATABASE_OUT_OF_SYNC;
    }
  }

  /* If this is Read by Type request */
  if (opcode == ATT_PDU_READ_TYPE_REQ)
  {
    uint16_t    uuid;

    /* Extract UUID: Skip L2C, ATT Header and 4 byte handle range */
    BYTES_TO_UINT16(uuid, (pPacket + L2C_PAYLOAD_START + ATT_HDR_LEN + 4));

    /* If this is a Read By Type Request of the Database Hash characteristic value */
    if (uuid == ATT_UUID_DATABASE_HASH)
    {
      err = ATT_SUCCESS;

      /* Reading the hash during a hash update causes the new hash to be returned and counts
       * towards the peer's progression towards a change-aware state.
       */
      if (attsCsfCb.isHashUpdating)
      {
        /* This read will not be processed until after the hash update completes, so this read
        * request shall be counted as a move from change-unaware to chang-aware pending.
        */
        pRec->changeAwareState = ATTS_CLIENT_CHANGE_AWARE_DB_READ_PENDING;

        ATT_TRACE_INFO2("ConnId %d change aware state is %d", connId,
                        ATTS_CLIENT_CHANGE_AWARE_DB_READ_PENDING);
      }
    }
  }

  if (err == ATT_ERR_DATABASE_OUT_OF_SYNC)
  {
    ATT_TRACE_INFO2("ConnId %d out of sync, PDU with opcode 0x%02x ignored!", connId, opcode);
  }

  return err;
}

/*************************************************************************************************/
/*!
 *  \brief  Update a client's state of awareness to a change in the database.
 *
 *  \param  connId    DM connection ID.  if \ref DM_CONN_ID_NONE, sets the state for all connected
 *                    clients.
 *  \param  state     The state of awareness to a change, see ::attClientAwareStates.
 *
 *  \return None.
 *
 *  \note A callback to application is not needed as it is expected the caller (i.e. the
 *        application) will have updated all persistent records prior to calling this function.
 */
/*************************************************************************************************/
void AttsCsfSetClientChangeAwareState(dmConnId_t connId, uint8_t state)
{
  if (connId == DM_CONN_ID_NONE)
  {
    for (uint8_t i = 0; i < DM_CONN_MAX; i++)
    {
      if (attsCsfCb.attsCsfTable[i].changeAwareState == ATTS_CLIENT_CHANGE_AWARE_DB_READ_PENDING)
      {
        attsCsfCb.attsCsfTable[i].changeAwareState = ATTS_CLIENT_CHANGE_PENDING_AWARE;
      }
      else
      {
        attsCsfCb.attsCsfTable[i].changeAwareState = state;
      }
    }
  }
  else
  {
    attsCsfCb.attsCsfTable[connId - 1].changeAwareState = state;

    ATT_TRACE_INFO2("ConnId %d change aware state is %d", connId, state);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize the client supported features for a connection.
 *
 *  \param  connId            DM connection ID.
 *  \param  changeAwareState  The state of awareness to a change in the database.
 *  \param  pCsf              Pointer to the client supported features value to cache. \ref NULL or
 *                            buffer of length \ref ATT_CSF_LEN.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCsfConnOpen(dmConnId_t connId, uint8_t changeAwareState, uint8_t *pCsf)
{
  if (pCsf != NULL)
  {
    attsCsfCb.attsCsfTable[connId - 1].changeAwareState = changeAwareState;
    memcpy(&attsCsfCb.attsCsfTable[connId - 1].csf, pCsf, ATT_CSF_LEN);
  }
  else
  {
    /* Note: this set client to the change-aware state. */
    memset(&attsCsfCb.attsCsfTable[connId - 1], 0, sizeof(attsCsfRec_t));
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Register callback.
 *
 *  \param  writeCback  Application callback for when features or status change.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCsfRegister(attsCsfWriteCback_t writeCback)
{
  attsCsfCb.writeCback = writeCback;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATTS client supported features module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCsfInit(void)
{
  attsCsfCb.isHashUpdating = FALSE;
  attsCsfCb.writeCback = NULL;
}

/*************************************************************************************************/
/*!
 *  \brief  GATT write of client supported feature characteristic value.
 *
 *  \param  connId      DM connection ID.
 *  \param  offset      offset into csf characteristic.
 *  \param  valueLen    length of write in bytes.
 *  \param  pValue      Pointer to client's supported features characteristic value.
 *
 *  \return \ref ATT_SUCCESS is successful, \ref ATT_ERR_VALUE_NOT_ALLOWED if any supported
 *          features are flipped from 1 to 0.
 */
/*************************************************************************************************/
uint8_t AttsCsfWriteFeatures(dmConnId_t connId, uint16_t offset, uint16_t valueLen, uint8_t *pValue)
{
  attsCsfRec_t *pCsfRec = &attsCsfCb.attsCsfTable[connId - 1];
  uint8_t      newCsf;

  /* future parameter in case the client supported features characteristic becomes a multi-octet
   * structure.
   */
  (void)offset;

  /* sanity check on length */
  if (valueLen < ATT_CSF_LEN)
  {
    return ATT_ERR_LENGTH;
  }
  
  newCsf = *pValue & ATTS_CSF_ALL_FEATURES;

  /* A client cannot clear any bits it has set. */
  if ((pCsfRec->csf > 0) && (newCsf == 0))
  {
    return ATT_ERR_VALUE_NOT_ALLOWED;
  }

  pCsfRec->csf |= newCsf;

  ATT_TRACE_INFO2("connId %d updated csf to 0x%02x", connId, pCsfRec->csf);

  /* Callback to application to store updated features, if bonded. */
  if (attsCsfCb.writeCback != NULL)
  {
    attsCsfCb.writeCback(connId, pCsfRec->changeAwareState, &pCsfRec->csf);
  }

  return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  Get client supported feature record.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCsfOut     Output parameter for client supported features buffer.
 *  \param  pCsfOutLen  Length of output parameter buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCsfGetFeatures(dmConnId_t connId, uint8_t *pCsfOut, uint8_t pCsfOutLen)
{
  if (pCsfOutLen <= ATT_CSF_LEN)
  {
    memcpy(pCsfOut, &attsCsfCb.attsCsfTable[connId - 1].csf, pCsfOutLen);
  }
}

/*************************************************************************************************/
/*!
 *  \brief  Get client state of awareness to a change in the database.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return Client's change-aware state. See ::attClientAwareStates.
 */
/*************************************************************************************************/
uint8_t AttsCsfGetClientChangeAwareState(dmConnId_t connId)
{
  return attsCsfCb.attsCsfTable[connId - 1].changeAwareState;
}
