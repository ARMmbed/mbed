/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Attribute protocol client and server API.
 *
 *  Copyright (c) 2009-2019 Arm Ltd. All Rights Reserved.
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
#ifndef ATT_API_H
#define ATT_API_H

#include "wsf_timer.h"
#include "att_defs.h"
#include "att_uuid.h"
#include "dm_api.h"
#include "cfg_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/
/*! \addtogroup STACK_ATTS_API
 *  \{ */
/** \name ATT Server Attribute Settings
 * Settings that may be set on each attribute.
 */
/**@{*/
#define ATTS_SET_UUID_128                0x01    /*!< \brief Set if the UUID is 128 bits in length */
#define ATTS_SET_WRITE_CBACK             0x02    /*!< \brief Set if the group callback is executed when
                                                             this attribute is written by a client device */
#define ATTS_SET_READ_CBACK              0x04    /*!< \brief Set if the group callback is executed when
                                                             this attribute is read by a client device */
#define ATTS_SET_VARIABLE_LEN            0x08    /*!< \brief Set if the attribute has a variable length */
#define ATTS_SET_ALLOW_OFFSET            0x10    /*!< \brief Set if writes are allowed with an offset */
#define ATTS_SET_CCC                     0x20    /*!< \brief Set if the attribute is a client characteristic
                                                             configuration descriptor */
#define ATTS_SET_ALLOW_SIGNED            0x40    /*!< \brief Set if signed writes are allowed */
#define ATTS_SET_REQ_SIGNED              0x80    /*!< \brief Set if signed writes are required if link
                                                             is not encrypted */
/**@}*/

/** \name ATT Server Attribute Permissions
 * Permissions used to describe a attribute's security setting.
 * These values can be set in any combination.
 */
/**@{*/
#define ATTS_PERMIT_READ                 0x01    /*!< \brief Set if attribute can be read */
#define ATTS_PERMIT_READ_AUTH            0x02    /*!< \brief Set if attribute read requires authentication */
#define ATTS_PERMIT_READ_AUTHORIZ        0x04    /*!< \brief Set if attribute read requires authorization */
#define ATTS_PERMIT_READ_ENC             0x08    /*!< \brief Set if attribute read requires encryption */
#define ATTS_PERMIT_WRITE                0x10    /*!< \brief Set if attribute can be written */
#define ATTS_PERMIT_WRITE_AUTH           0x20    /*!< \brief Set if attribute write requires authentication */
#define ATTS_PERMIT_WRITE_AUTHORIZ       0x40    /*!< \brief Set if attribute write requires authorization */
#define ATTS_PERMIT_WRITE_ENC            0x80    /*!< \brief Set if attribute write requires encryption */
/**@}*/
/*! \} */    /* STACK_ATTS_API */

/*! \addtogroup STACK_ATTC_API
 *  \{ */
/** \name ATT Client Discovery and Configuration Settings
 * Settings used to configurate ATT Discovery procedure for ATT Clients.
 */
/**@{*/
#define ATTC_SET_UUID_128                0x01    /*!< \brief Set if the UUID is 128 bits in length */
#define ATTC_SET_REQUIRED                0x02    /*!< \brief Set if characteristic must be discovered */
#define ATTC_SET_DESCRIPTOR              0x04    /*!< \brief Set if this is a characteristic descriptor */
/**@}*/
/*! \} */    /* STACK_ATTC_API */

/*! \addtogroup STACK_ATT_API
 *  \{ */
/** \name ATT Callback Events
 * Events related to ATT transactions.
 */
/**@{*/
#define ATT_CBACK_START                  0x02    /*!< \brief ATT callback event starting value */

/*! \brief ATT client callback events */
enum                                        /*!< \brief Internal note: event values match method values */
{
  ATTC_FIND_INFO_RSP = ATT_CBACK_START,     /*!< \brief Find information response */
  ATTC_FIND_BY_TYPE_VALUE_RSP,              /*!< \brief Find by type value response */
  ATTC_READ_BY_TYPE_RSP,                    /*!< \brief Read by type value response */
  ATTC_READ_RSP,                            /*!< \brief Read response */
  ATTC_READ_LONG_RSP,                       /*!< \brief Read long response */
  ATTC_READ_MULTIPLE_RSP,                   /*!< \brief Read multiple response */
  ATTC_READ_BY_GROUP_TYPE_RSP,              /*!< \brief Read group type response */
  ATTC_WRITE_RSP,                           /*!< \brief Write response */
  ATTC_WRITE_CMD_RSP,                       /*!< \brief Write command response */
  ATTC_PREPARE_WRITE_RSP,                   /*!< \brief Prepare write response */
  ATTC_EXECUTE_WRITE_RSP,                   /*!< \brief Execute write response */
  ATTC_HANDLE_VALUE_NTF,                    /*!< \brief Handle value notification */
  ATTC_HANDLE_VALUE_IND,                    /*!< \brief Handle value indication */
  ATTC_READ_MULT_VAR_RSP = 16,              /*!< \brief Read multiple variable length response */
  ATTC_MULT_VALUE_NTF,                      /*!< \brief Read multiple value notification */
  /* ATT server callback events */
  ATTS_HANDLE_VALUE_CNF,                    /*!< \brief Handle value confirmation */
  ATTS_MULT_VALUE_CNF,                      /*!< \brief Handle multiple value confirmation */
  ATTS_CCC_STATE_IND,                       /*!< \brief Client chracteristic configuration state change */
  ATTS_DB_HASH_CALC_CMPL_IND,               /*!< \brief Database hash calculation complete */
  /* ATT common callback events */
  ATT_MTU_UPDATE_IND,                       /*!< \brief Negotiated MTU value */
  ATT_EATT_CONN_CMPL_IND,                   /*!< \brief EATT Connect channels complete */
  ATT_EATT_RECONFIG_CMPL_IND                /*!< \brief EATT Reconfigure complete */
};

/*! \brief ATT callback events */
#define ATT_CBACK_END                    ATT_EATT_RECONFIG_CMPL_IND  /*!< \brief ATT callback event ending value */
/**@}*/

/** \name ATT Client Awareness of Database Change
 * Status of a client's awareness of a database change.
 */
/**@{*/
/*! \brief client's awareness to database change. */
enum attClientAwareStates
{
  ATTS_CLIENT_CHANGE_AWARE = 0,             /*!< Client Aware. */
  ATTS_CLIENT_CHANGE_PENDING_AWARE,         /*!< Client Aware pending ATT Request.  For internal stack use only. */
  ATTS_CLIENT_CHANGE_AWARE_DB_READ_PENDING, /*!< Client Aware, Database Hash read pending hash update completion. For internal stack use only. */
  ATTS_CLIENT_CHANGE_UNAWARE,               /*!< Client Unaware. */
};
/**@}*/

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief ATT run-time configurable parameters */
typedef struct
{
  wsfTimerTicks_t   discIdleTimeout;  /*!< \brief ATT server service discovery connection idle timeout in seconds */
  uint16_t          mtu;              /*!< \brief desired ATT MTU */
  uint8_t           transTimeout;     /*!< \brief transcation timeout in seconds */
  uint8_t           numPrepWrites;    /*!< \brief number of queued prepare writes supported by server */
} attCfg_t;

/*! \brief EATT run-time configurable parameters */
typedef struct
{
  uint16_t          mtu;              /*!< \brief MTU */
  uint16_t          mps;              /*!< \brief MPS */
  bool_t            initiateEatt;     /*!< \brief Open EATT channels automatically on connect */
  uint8_t           authoriz;         /*!< \brief Authorization required */
  uint8_t           secLevel;         /*!< \brief Security level required */
  uint8_t           numChans;         /*!< \brief Number of enhanced l2cap channels per connection */
  const uint8_t     *pPriorityTbl;    /*!< \brief Min channel priority table. Set NULL to map priority to L2CAP channel index. */
} eattCfg_t;

/*!
 * \brief ATT callback event
 *
 * \param hdr.event     Callback event
 * \param hdr.param     DM connection ID
 * \param hdr.status    Event status:  ATT_SUCCESS or error status
 * \param pValue        Pointer to value data, valid if valueLen > 0
 * \param valueLen      Length of value data
 * \param handle        Attribute handle
 * \param continuing    TRUE if more response packets expected
 * \param mtu           Negotiated MTU value
 */
typedef struct
{
  wsfMsgHdr_t           hdr;          /*!< \brief Header structure */
  uint8_t               *pValue;      /*!< \brief Value */
  uint16_t              valueLen;     /*!< \brief Value length */
  uint16_t              handle;       /*!< \brief Attribute handle */
  bool_t                continuing;   /*!< \brief TRUE if more response packets expected */
  uint16_t              mtu;          /*!< \brief Negotiated MTU value */
} attEvt_t;

/*! \brief ATT event callback type.
 *
 *  This callback function sends ATT events to the client application.  A
 *  single callback function is used for both ATTS and ATTC.
 *
 *  \param pEvt Pointer to ATT event structure.
 *
 *  \return None.
 */
typedef void (*attCback_t)(attEvt_t *pEvt);
/*! \} */    /* STACK_ATT_API */

/*! \addtogroup STACK_ATTS_API
 *  \{ */
/**
 * \name Attribute Server Data Types
 *
 */
/**@{*/
/*! \brief Attribute structure */
typedef struct
{
  uint8_t const     *pUuid;           /*!< \brief Pointer to the attribute's UUID */
  uint8_t           *pValue;          /*!< \brief Pointer to the attribute's value */
  uint16_t          *pLen;            /*!< \brief Pointer to the length of the attribute's value */
  uint16_t          maxLen;           /*!< \brief Maximum length of attribute's value */
  uint8_t           settings;         /*!< \brief Attribute settings */
  uint8_t           permissions;      /*!< \brief Attribute permissions */
} attsAttr_t;

/*! \brief Client supported features record structure. */
typedef struct
{
  uint8_t csf;              /*!< Client supported features characteristic value */
  uint8_t changeAwareState; /*!< Client awareness of GATT database changes */
} attsCsfRec_t;
/**@}*/

/** \name ATT Server Callbacks
 *
 */
/**@{*/
/*! \brief Attribute group read callback
 *
 * This is the attribute server read callback.  It is executed on an attribute
 * read operation if bitmask ATTS_SET_READ_CBACK is set in the settings field
 * of the attribute structure.
 * For a read operation, if the operation is successful the function must set
 * pAttr->pValue to the data to be read.  In addition, if the attribute is
 * variable length then pAttr->pLen must be set as well.
 *
 *  \param connId    DM connection ID.
 *  \param handle    Attribute handle.
 *  \param operation Operation type.
 *  \param offset    Read data offset.
 *  \param pAttr     Pointer to attribute structure.
 *
 *  \return status of the operation. \ref ATT_SUCCESS if successful.
 */
typedef uint8_t (*attsReadCback_t)(dmConnId_t connId, uint16_t handle, uint8_t operation,
                                   uint16_t offset, attsAttr_t *pAttr);

/*! \brief Attribute group write callback
 *
 * This is the attribute server write callback.  It is executed on an
 * attribute write operation if bitmask ATTS_SET_WRITE_CBACK is set
 * in the settings field of the attribute structure.
 *
 *  \param connId    DM connection ID.
 *  \param handle    Attribute handle.
 *  \param operation Operation type.
 *  \param offset    Write data offset.
 *  \param len       Length of data to write.
 *  \param pValue    Pointer to data to write.
 *  \param pAttr     Pointer to attribute structure.
 *
 *  \return status of operation.  \ref ATT_SUCCESS if successful.
 */
typedef uint8_t (*attsWriteCback_t)(dmConnId_t connId, uint16_t handle, uint8_t operation,
                                    uint16_t offset, uint16_t len, uint8_t *pValue,
                                    attsAttr_t *pAttr);

/*! \brief ATTS authorization callback type
 *
 *  This callback function is executed when a read or write operation occurs
 *  and the security field of an attribute structure is set to
 *  \ref ATTS_PERMIT_READ_AUTHORIZ or \ref ATTS_PERMIT_WRITE_AUTHORIZ respectively.
 *
 *  \param connId DM Connection ID.
 *  \param permit Set to \ref ATTS_PERMIT_WRITE for a writre operation or
 *         \ref ATTS_PERMIT_READ for a read operation.
 *  \param handle Attribute handle.
 *
 *  \return status of the operation. \ref ATT_SUCCESS if successful, else if failure
 *          the \ref ATT_ERR_AUTH is typically returned.
 */
typedef uint8_t (*attsAuthorCback_t)(dmConnId_t connId, uint8_t permit, uint16_t handle);

/*! \brief ATTS client supported features write callback type
 *
 *  This callback function is executed when the client supported features record for
 *  this client has been updated.
 *
 *  \param connId            DM Connection ID.
 *  \param changeAwareState  The state of awareness to a change, see ::attClientAwareStates.
 *  \param pCsf              Pointer to the client supported features value.
 *
 *  \return None.
 */
typedef void (*attsCsfWriteCback_t)(dmConnId_t connId, uint8_t changeAwareState, uint8_t *pCsf);
/**@}*/

/**
 * \name Attribute Server Data Types
 *
 */
/**@{*/
/*! \brief Attribute group */
typedef struct attsGroup_tag
{
  struct attsGroup_tag  *pNext;       /*!< \brief For internal use only */
  attsAttr_t            *pAttr;       /*!< \brief Pointer to attribute list for this group */
  attsReadCback_t       readCback;    /*!< \brief Read callback function */
  attsWriteCback_t      writeCback;   /*!< \brief Write callback function */
  uint16_t              startHandle;  /*!< \brief The handle of the first attribute in this group */
  uint16_t              endHandle;    /*!< \brief The handle of the last attribute in this group */
} attsGroup_t;

/*! \brief Client characteristc configuration settings */
typedef struct
{
  uint16_t          handle;           /*!< \brief Client characteristc configuration descriptor handle */
  uint16_t          valueRange;       /*!< \brief Acceptable value range of the descriptor value */
  uint8_t           secLevel;         /*!< \brief Security level of characteristic value */
} attsCccSet_t;

/*! \brief ATTS client characteristic configuration callback structure */
typedef struct
{
  wsfMsgHdr_t           hdr;          /*!< \brief Header structure */
  uint16_t              handle;       /*!< \brief CCCD handle */
  uint16_t              value;        /*!< \brief CCCD value */
  uint8_t               idx;          /*!< \brief CCCD settings index */
} attsCccEvt_t;
/**@}*/
/*! \} */    /* STACK_ATTS_API */

/*! \addtogroup STACK_ATTC_API
 *  \{ */
/**
 * \name ATT Client Data Types
 */
/**@{*/
/*! \brief ATT client structure for characteristic and descriptor discovery */
typedef struct attcDiscChar_tag
{
  uint8_t const           *pUuid;       /*!< \brief Pointer to UUID */
  uint8_t                 settings;     /*!< \brief Characteristic discovery settings */
} attcDiscChar_t;

/*! \brief ATT client structure for characteristic and descriptor configuration */
typedef struct
{
  uint8_t const         *pValue;      /*!< \brief Pointer to default value or NULL */
  uint8_t               valueLen;     /*!< \brief Default value length */
  uint8_t               hdlIdx;       /*!< \brief Index of its handle in handle list */
} attcDiscCfg_t;

/*! \brief ATT client discovery control block */
typedef struct
{
  attcDiscChar_t        **pCharList;  /*!< \brief Characterisic list for discovery */
  uint16_t              *pHdlList;    /*!< \brief Characteristic handle list */
  attcDiscCfg_t         *pCfgList;    /*!< \brief Characterisic list for configuration */
  uint8_t               charListLen;  /*!< \brief Characteristic and handle list length */
  uint8_t               cfgListLen;   /*!< \brief Configuration list length */

  uint16_t              svcStartHdl;  /*!< \brief Internal use only */
  uint16_t              svcEndHdl;    /*!< \brief Internal use only */
  uint8_t               charListIdx;  /*!< \brief Internal use only */
  uint8_t               endHdlIdx;    /*!< \brief Internal use only */
} attcDiscCb_t;
/**@}*/
/*! \} */    /* STACK_ATTC_API */

/*! \addtogroup STACK_ATTS_API
 *  \{ */
/** \name ATT Server Callbacks
 *
 */
/**@{*/
/*! \brief ATTS client characteristic configuration callback
 *
 *  Client characteristic configuration callback.  This function is executed
 *  when a CCCD value changes.  This happens when a peer device writes a new
 *  value to the CCCD or when a CCCD table is initialized by calling \ref AttsCccInitTable.
 *
 *  \param pEvt Pointer to callback structure.
 *
 *  \return None.
 */
typedef void (*attsCccCback_t)(attsCccEvt_t *pEvt);
/**@}*/
/*! \} */    /* STACK_ATTS_API */

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! \addtogroup STACK_INIT
 *  \{ */
/** \name ATT Configuration Structure
 * Pointer to structure containing initialization details of the ATT Subsystem.  To be configured
 * by Application.
 */
/**@{*/
/*! \brief Configuration pointer */
extern attCfg_t *pAttCfg;

/*! \brief Enhanced configuration pointer */
extern eattCfg_t *pEattCfg;
/**@}*/
/*! \} */    /* STACK_INIT */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! \addtogroup STACK_ATT_API
 *  \{ */
/** \name ATT Setup Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Register a callback with ATT.  This callback will be used for messages from both ATTC
 *          and ATTS.
 *
 *  \param  cback  Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttRegister(attCback_t cback);

/*************************************************************************************************/
/*!
 *  \brief  Register a connection callback with ATT.  The callback is typically used to
 *          manage the attribute server database.
 *
 *  \param  cback  Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttConnRegister(dmCback_t cback);
/**@}*/

/** \name ATT Parameter Functions
 * Functions specific to a connection between 2 devices.  Functions may be called by either
 * Client or server.
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Get the attribute protocol MTU of a connection.
 *
 *  \param  connId    DM connection ID.
 *
 *  \return MTU of the connection.
 */
/*************************************************************************************************/
uint16_t AttGetMtu(dmConnId_t connId);
/**@}*/

/** \name ATT Message Passing Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Allocate an ATT message buffer to be sent with the ATT attribute protocol
 *          zero-copy APIs.
 *
 *  \param  len         Message length in bytes.
 *  \param  opcode      Opcode for ATT message.
 *
 *  \return Pointer to message buffer or NULL if allocation failed.
 */
/*************************************************************************************************/
void *AttMsgAlloc(uint16_t len, uint8_t opcode);

/*************************************************************************************************/
/*!
 *  \brief  Free an ATT message buffer allocated with AttMsgAlloc().
 *
 *  \param  pMsg        Pointer to message buffer.
 *  \param  opcode      Opcode for ATT message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttMsgFree(void *pMsg, uint8_t opcode);
/**@}*/
/*! \} */    /* STACK_ATT_API */

/*! \addtogroup STACK_ATTS_API
 *  \{ */
/** \name ATT Server Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT server for indications/notifications.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsIndInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT server for data signing.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsSignInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Register an authorization callback with the attribute server.
 *
 *  \param  cback  Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsAuthorRegister(attsAuthorCback_t cback);

/*************************************************************************************************/
/*!
 *  \brief  Add an attribute group to the attribute server.
 *
 *  \param  pGroup    Pointer to an attribute group structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsAddGroup(attsGroup_t *pGroup);

/*************************************************************************************************/
/*!
 *  \brief  Remove an attribute group from the attribute server.
 *
 *  \param  startHandle  Start handle of attribute group to be removed.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsRemoveGroup(uint16_t startHandle);

/*************************************************************************************************/
/*!
*  \brief  Calculate database hash from the GATT database.
*
*  \return None.
*/
/*************************************************************************************************/
void AttsCalculateDbHash(void);

/*************************************************************************************************/
/*!
 *  \brief  Create hash from the database string.
 *
 *  \param  pKey    Key for hashing.
 *  \param  pMsg    Plaintext to hash.
 *  \param  msgLen  Length of Plaintext data.
 *
 *  \return \ref TRUE if successful, \ref FALSE if not.
 */
/*************************************************************************************************/
bool_t AttsHashDatabaseString(uint8_t *pKey, uint8_t *pMsg, uint16_t msgLen);

/*************************************************************************************************/
/*!
 *  \brief  Set an attribute value in the attribute server.
 *
 *  \param  handle    Attribute handle.
 *  \param  valueLen  Attribute length.
 *  \param  pValue    Attribute value.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 */
/*************************************************************************************************/
uint8_t AttsSetAttr(uint16_t handle, uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Get an attribute value in the attribute server.
 *
 *  \param  handle    Attribute handle.
 *  \param  pLen      Returned attribute length pointer.
 *  \param  pValue    Returned attribute value pointer.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 *  \return This function returns the attribute length in pLen and a pointer to the attribute
 *          value in pValue.
 */
/*************************************************************************************************/
uint8_t AttsGetAttr(uint16_t handle, uint16_t *pLen, uint8_t **pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueInd(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueNtf(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Indication without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueIndZeroCpy(dmConnId_t connId, uint16_t handle, uint16_t valueLen,
                               uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol Handle Value Notification without copying the attribute
 *          value data.
 *
 *          Note: attribute value buffer 'pValue' must be allocated with AttMsgAlloc().
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsHandleValueNtfZeroCpy(dmConnId_t connId, uint16_t handle, uint16_t valueLen,
                               uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Register the utility service for managing client characteristic
 *          configuration descriptors.  This function is typically called once on
 *          system initialization.
 *
 *  \param  setLen  Length of settings array.
 *  \param  pSet    Array of CCC descriptor settings.
 *  \param  cback   Client callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccRegister(uint8_t setLen, attsCccSet_t *pSet, attsCccCback_t cback);

/*************************************************************************************************/
/*!
 *  \brief  Initialize the client characteristic configuration descriptor value table for a
 *          connection.  The table is initialized with the values from pCccTbl.  If pCccTbl
 *          is NULL the table will be initialized to zero.
 *
 *          This function must be called when a connection is established or when a
 *          device is bonded.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCccTbl     Pointer to the descriptor value array.  The length of the array
 *                      must equal the value of setLen passed to AttsCccRegister().
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccInitTable(dmConnId_t connId, uint16_t *pCccTbl);

/*************************************************************************************************/
/*!
 *  \brief  Clear and deallocate the client characteristic configuration descriptor value
 *          table for a connection.  This function must be called when a connection is closed.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccClearTable(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \brief  Get the value of a client characteristic configuration descriptor by its index.
 *          If not found, return zero.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *
 *  \return Value of the descriptor.
 */
/*************************************************************************************************/
uint16_t AttsCccGet(dmConnId_t connId, uint8_t idx);

/*************************************************************************************************/
/*!
 *  \brief  Set the value of a client characteristic configuration descriptor by its index.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *  \param  value       Value of the descriptor.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCccSet(dmConnId_t connId, uint8_t idx, uint16_t value);

/*************************************************************************************************/
/*!
 *  \brief  Check if a client characteristic configuration descriptor is enabled and if
 *          the characteristic's security level has been met.
 *
 *  \param  connId      DM connection ID.
 *  \param  idx         Index of descriptor in CCC descriptor handle table.
 *
 *  \return Value of the descriptor if security level is met, otherwise zero.
 */
/*************************************************************************************************/
uint16_t AttsCccEnabled(dmConnId_t connId, uint8_t idx);

/*************************************************************************************************/
/*!
 *  \brief  Check if a client characteristic configuration descriptor is enabled and if
 *          the characteristic's security level has been met.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle of CCC descriptor.
 *
 *  \return Value of the descriptor if security level is met, otherwise zero.
 */
/*************************************************************************************************/
uint16_t AttsCccEnabledByHandle(dmConnId_t connId, uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief   Get number of CCC entries in table.
 *
 *  \return  Number of CCC entries in table.
 */
/*************************************************************************************************/
uint8_t AttsGetCccTableLen(void);

/*************************************************************************************************/
/*!
 *  \brief  Send a response to a pending write request.  For use with ATT_RSP_PENDING.
 *
 *  \param  connId      Connection ID.
 *  \param  handle      Attribute handle.
 *  \param  status      Status of the write request.
 *
 *  \return None.
 *
 *  \note   When a higher layer returns ATT_RSP_PENDING to an ATT write callback indicating the
 *          response status is pending, the higher layer must subsequently call this function
 *          with the status of the write request.
 */
/*************************************************************************************************/
void AttsContinueWriteReq(dmConnId_t connId, uint16_t handle, uint8_t status);

/*************************************************************************************************/
/*!
 *  \brief  Set the peer's data signing key on this connection.  This function
 *          is typically called from the ATT connection callback when the connection is
 *          established.  The caller is responsible for maintaining the memory that
 *          contains the key.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCsrk       Pointer to data signing key (CSRK).
 *  \param  authen      Whether CSRK was received on authenticated connection.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsSetCsrk(dmConnId_t connId, uint8_t *pCsrk, bool_t authen);

/*************************************************************************************************/
/*!
 *  \brief  Set the peer's sign counter on this connection.  This function
 *          is typically called from the ATT connection callback when the connection is
 *          established.  ATT maintains the value of the sign counter internally and
 *          sets the value when a signed packet is successfully received.
 *
 *  \param  connId      DM connection ID.
 *  \param  signCounter Sign counter.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsSetSignCounter(dmConnId_t connId, uint32_t signCounter);

/*************************************************************************************************/
/*!
 *  \brief  Get the current value peer's sign counter on this connection.  This function
 *          is typically called from the ATT connection callback when the connection is
 *          closed so the application can store the sign counter for use on future
 *          connections.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return Sign counter.
 */
/*************************************************************************************************/
uint32_t AttsGetSignCounter(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATTS client supported features module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCsfInit(void);

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
void AttsCsfConnOpen(dmConnId_t connId, uint8_t changeAwareState, uint8_t *pCsf);

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
uint8_t AttsCsfWriteFeatures(dmConnId_t connId, uint16_t offset, uint16_t valueLen,
                             uint8_t *pValue);

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
void AttsCsfGetFeatures(dmConnId_t connId, uint8_t *pCsfOut, uint8_t pCsfOutLen);

/*************************************************************************************************/
/*!
 *  \brief  Get client state of awareness to a change in the database.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return Client's change-aware state.  See ::attClientAwareStates.
 */
/*************************************************************************************************/
uint8_t AttsCsfGetClientChangeAwareState(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \brief  Update a client's state of awareness to a change in the database.
 *
 *  \param  connId    DM connection ID.  if \ref DM_CONN_ID_NONE, sets the state for all connected
 *                    clients.
 *  \param  state     The state of awareness to a change, see \ref attClientAwareStates.
 *
 *  \return None.
 *
 *  \note A callback to application is not needed as it is expected the caller (i.e. the
 *        application) will have updated all persistent records prior to calling this function.
 */
/*************************************************************************************************/
void AttsCsfSetClientChangeAwareState(dmConnId_t connId, uint8_t state);

/*************************************************************************************************/
/*!
 *  \brief  Register callback.
 *
 *  \param  writeCback  Application callback for when client supported features record is updated.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsCsfRegister(attsCsfWriteCback_t writeCback);
/**@}*/

/** \name ATT Server Dynamic Service Subsystem Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Initialize the Dynamic ATT Service subsystem.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsDynInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Dynamically create an ATT Service at runtime.
 *
 *  \param  startHandle       Starting attribute handle in the service
 *  \param  endHandle         Last attribute handle in the service
 *
 *  \return Service Handle.
 */
/*************************************************************************************************/
void *AttsDynCreateGroup(uint16_t startHandle, uint16_t endHandle);

/*************************************************************************************************/
/*!
 *  \brief  Dynamically delete an ATT Service at runtime.
 *
 *  \param  pSvcHandle      Service handle returned by \ref AttsDynCreateGroup
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsDynDeleteGroup(void *pSvcHandle);

/*************************************************************************************************/
/*!
 *  \brief  Register callback functions for a dynamic ATT Service at runtime.
 *
 *  \param  pSvcHandle    Service handle returned by \ref AttsDynCreateGroup
 *  \param  readCback     Read callback function
 *  \param  writeCback    Write callback function
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsDynRegister(void *pSvcHandle, attsReadCback_t readCback, attsWriteCback_t writeCback);

/*************************************************************************************************/
/*!
 *  \brief  Dynamically add an attribute to a dynamic ATT Services at runtime.
 *
 *  \param  pSvcHandle    Service handle returned by \ref AttsDynCreateGroup
 *  \param  pUuid         Constant UUID
 *  \param  pValue        Initial value of attribute (copied into attribute memory)
 *  \param  len           Length of pValue in bytes
 *  \param  maxLen        Maximum length of the attribute in bytes
 *  \param  settings      Attribute settings
 *  \param  permissions   Attribute permissions
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsDynAddAttr(void *pSvcHandle, const uint8_t *pUuid, const uint8_t *pValue, uint16_t len,
                    const uint16_t maxLen, uint8_t settings, uint8_t permissions);

/*************************************************************************************************/
/*!
 *  \brief  Dynamically add an attribute with a constant value to a dynamic ATT Services at runtime.
 *
 *  \param  pSvcHandle    Service handle returned by \ref AttsDynCreateGroup
 *  \param  pUuid         Constant UUID
 *  \param  pValue        Pointer to constant attribute memory
 *  \param  len           Length of pValue in bytes
 *  \param  settings      Attribute settings
 *  \param  permissions   Attribute permissions
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsDynAddAttrConst(void *pSvcHandle, const uint8_t *pUuid, const uint8_t *pValue,
                         const uint16_t len, uint8_t settings, uint8_t permissions);
/**@}*/

/** \name ATT Server Testing
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  For testing purposes only.
 *
 *  \param  status    ATT status
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttsErrorTest(uint8_t status);
/**@}*/
/*! \} */    /* STACK_ATTS_API */

/*! \addtogroup STACK_ATTC_API
 *  \{ */
/** \name ATT Client Functions
 *
 */
/**@{*/
/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT client.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Initialize ATT client for data signing.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcSignInit(void);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find Information Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcFindInfoReq(dmConnId_t connId, uint16_t startHandle, uint16_t endHandle, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Find By Type Value Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuid16      16-bit UUID to find.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcFindByTypeValueReq(dmConnId_t connId, uint16_t startHandle, uint16_t endHandle,
                            uint16_t uuid16, uint16_t valueLen, uint8_t *pValue, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read By Type Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuidLen     Length of UUID (2 or 16).
 *  \param  pUuid       Pointer to UUID data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcReadByTypeReq(dmConnId_t connId, uint16_t startHandle, uint16_t endHandle,
                       uint8_t uuidLen, uint8_t *pUuid, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  handle    Attribute handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcReadReq(dmConnId_t connId, uint16_t handle);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Long Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  offset      Read attribute data starting at this offset.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcReadLongReq(dmConnId_t connId, uint16_t handle, uint16_t offset, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read Multiple Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  numHandles  Number of handles in attribute handle list.
 *  \param  pHandles    List of attribute handles.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcReadMultipleReq(dmConnId_t connId, uint8_t numHandles, uint16_t *pHandles);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Read By Group Type Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  startHandle Attribute start handle.
 *  \param  endHandle   Attribute end handle.
 *  \param  uuidLen     Length of UUID (2 or 16).
 *  \param  pUuid       Pointer to UUID data.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcReadByGroupTypeReq(dmConnId_t connId, uint16_t startHandle, uint16_t endHandle,
                            uint8_t uuidLen, uint8_t *pUuid, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcWriteReq(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Write Command.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcWriteCmd(dmConnId_t connId, uint16_t handle, uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol signed Write Command.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  signCounter Value of the sign counter.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcSignedWriteCmd(dmConnId_t connId, uint16_t handle, uint32_t signCounter,
                        uint16_t valueLen, uint8_t *pValue);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Prepare Write Request.
 *
 *  \param  connId      DM connection ID.
 *  \param  handle      Attribute handle.
 *  \param  offset      Write attribute data starting at this offset.
 *  \param  valueLen    Length of value data.
 *  \param  pValue      Pointer to value data.
 *  \param  valueByRef  TRUE if pValue data is accessed by reference rather than copied.
 *  \param  continuing  TRUE if ATTC continues sending requests until complete.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcPrepareWriteReq(dmConnId_t connId, uint16_t handle, uint16_t offset, uint16_t valueLen,
                         uint8_t *pValue, bool_t valueByRef, bool_t continuing);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Execute Write Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  writeAll  TRUE to write all queued writes, FALSE to cancel all queued writes.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcExecuteWriteReq(dmConnId_t connId, bool_t writeAll);

/*************************************************************************************************/
/*!
 *  \brief  Cancel an attribute protocol request in progress.
 *
 *  \param  connId    DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcCancelReq(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \brief  This utility function discovers the given service on a peer device.  Function
 *          AttcFindByTypeValueReq() is called to initiate the discovery procedure.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCb         Pointer to discovery control block.
 *  \param  uuidLen     Length of service UUID (2 or 16).
 *  \param  pUuid       Pointer to service UUID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcDiscService(dmConnId_t connId, attcDiscCb_t *pCb, uint8_t uuidLen, uint8_t *pUuid);

/*************************************************************************************************/
/*!
 *  \brief  This utility function processes a service discovery result.  It should be called
 *          when an \ref ATTC_FIND_BY_TYPE_VALUE_RSP callback event is received after service
 *          discovery is initiated by calling AttcDiscService().
 *
 *  \param  pCb         Pointer to discovery control block.
 *  \param  pMsg        ATT callback event message.
 *
 *  \return ATT_SUCCESS if successful otherwise error.
 */
/*************************************************************************************************/
uint8_t AttcDiscServiceCmpl(attcDiscCb_t *pCb, attEvt_t *pMsg);

/*************************************************************************************************/
/*!
 *  \brief  This utility function starts characteristic and characteristic descriptor
 *          discovery for a service on a peer device.  The service must have been previously
 *          discovered by calling AttcDiscService() and AttcDiscServiceCmpl().
 *
 *  \param  connId      DM connection ID.
 *  \param  pCb         Pointer to discovery control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcDiscCharStart(dmConnId_t connId, attcDiscCb_t *pCb);

/*************************************************************************************************/
/*!
 *  \brief  This utility function processes a characteristic discovery result.  It should be
 *          called when an \ref ATTC_READ_BY_TYPE_RSP or \ref ATTC_FIND_INFO_RSP callback event is
 *          received after characteristic discovery is initiated by calling AttcDiscCharStart().
 *
 *  \param  pCb         Pointer to discovery control block.
 *  \param  pMsg        ATT callback event message.
 *
 *  \return ATT_CONTINUING if successful and the discovery procedure is continuing.
 *          ATT_SUCCESS if the discovery procedure completed successfully.
 *          Otherwise the discovery procedure failed.
 */
/*************************************************************************************************/
uint8_t AttcDiscCharCmpl(attcDiscCb_t *pCb, attEvt_t *pMsg);

/*************************************************************************************************/
/*!
 *  \brief  This utility function starts service include discovery for a service on a peer device.
 *          The service must have been previously discovered by calling AttcDiscService() and
*           AttcDiscServiceCmpl().
 *
 *  \param  connId      DM connection ID.
 *  \param  pCb         Pointer to service discovery control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcDiscIncSvcStart(dmConnId_t connId, attcDiscCb_t *pCb);

/*************************************************************************************************/
/*!
 *  \brief  This utility function processes a service include discovery result.  It should be
 *          called when an ATTC_READ_BY_TYPE_RSP allback event is received after service include
 *          discovery is initiated by calling AttcDiscIncSvcStart().
 *
 *  \param  pCb         Pointer to service discovery control block.
 *  \param  pMsg        ATT callback event message.
 *
 *  \return ATT_CONTINUING if successful and discovery procedure is continuing.
 *          ATT_SUCCESS if discovery procedure completed successfully.
 *          Otherwise the discovery procedure failed.
 */
/*************************************************************************************************/
uint8_t AttcDiscIncSvcCmpl(attcDiscCb_t *pCb, attEvt_t *pMsg);

/*************************************************************************************************/
/*!
 *  \brief  This utility function starts characteristic configuration for characteristics on a
 *          peer device.  The characteristics must have been previously discovered by calling
 *          AttcDiscCharStart() and AttcDiscCharCmpl().
 *
 *  \param  connId      DM connection ID.
 *  \param  pCb         Pointer to discovery control block.
 *
 *  \return ATT_CONTINUING if successful and configuration procedure is continuing.
 *          ATT_SUCCESS if nothing to configure.
 */
/*************************************************************************************************/
uint8_t AttcDiscConfigStart(dmConnId_t connId, attcDiscCb_t *pCb);

/*************************************************************************************************/
/*!
 *  \brief  This utility function initiates the next characteristic configuration procedure.
 *          It should be called when an \ref ATTC_READ_RSP or \ref ATTC_WRITE_RSP callback event
 *          is received after characteristic configuration is initiated by calling
 *          AttcDiscConfigStart().
 *
 *  \param  connId      DM connection ID.
 *  \param  pCb         Pointer to discovery control block.
 *
 *  \return ATT_CONTINUING if successful and configuration procedure is continuing.
 *          ATT_SUCCESS if configuration procedure completed successfully.
 */
/*************************************************************************************************/
uint8_t AttcDiscConfigCmpl(dmConnId_t connId, attcDiscCb_t *pCb);

/*************************************************************************************************/
/*!
 *  \brief  This utility function resumes the characteristic configuration procedure.  It can
 *          be called when an \ref ATTC_READ_RSP or \ref ATTC_WRITE_RSP callback event is received
 *          with failure status to attempt the read or write procedure again.
 *
 *  \param  connId      DM connection ID.
 *  \param  pCb         Pointer to discovery control block.
 *
 *  \return ATT_CONTINUING if successful and configuration procedure is continuing.
 *          ATT_SUCCESS if configuration procedure completed successfully.
 */
/*************************************************************************************************/
uint8_t AttcDiscConfigResume(dmConnId_t connId, attcDiscCb_t *pCb);

/*************************************************************************************************/
/*!
 *  \brief  Initiate an attribute protocol Exchange MTU Request.
 *
 *  \param  connId    DM connection ID.
 *  \param  mtu       Attribute protocol MTU.
 *
 *  \return None.
 *
 *  \note   The Exchange MTU Request will be initiated automatically on a master connection.
 *
 *  \note   This API can be used by the application to initiate an Exchange MTU Request on slave
 *          connections.
 */
/*************************************************************************************************/
void AttcMtuReq(dmConnId_t connId, uint16_t mtu);

/*************************************************************************************************/
/*!
 *  \brief  Set automatic Indication Confirmations sent from this ATT Client.
 *
 *  \param  enable  \ref TRUE to enable automatic confirmations (default), \ref FALSE to disable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcSetAutoConfirm(bool_t enable);

/*************************************************************************************************/
/*!
 *  \brief  Send an attribute protocol indication confirmation.
 *
 *  \param  connId    DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AttcIndConfirm(dmConnId_t connId);
/**@}*/
/*! \} */    /* STACK_ATTC_API */


#ifdef __cplusplus
};
#endif

#endif /* ATT_API_H */
