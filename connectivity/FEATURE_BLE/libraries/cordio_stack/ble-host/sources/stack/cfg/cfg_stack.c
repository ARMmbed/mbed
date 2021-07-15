/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Stack configuration.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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

#include "wsf_types.h"
#include "wsf_assert.h"
#include "cfg_stack.h"
#include "hci_api.h"
#include "dm_api.h"
#include "l2c_api.h"
#include "att_api.h"
#include "smp_api.h"

/**************************************************************************************************
  HCI
**************************************************************************************************/

/**************************************************************************************************
  DM
**************************************************************************************************/

/* Configuration structure */
const dmCfg_t dmCfg =
{
  0
};

/* Configuration pointer */
dmCfg_t *pDmCfg = (dmCfg_t *) &dmCfg;

/**************************************************************************************************
  L2C
**************************************************************************************************/

/* Configuration structure */
const l2cCfg_t l2cCfg =
{
  30                                /* Request timeout in seconds */
};

/* Configuration pointer */
l2cCfg_t *pL2cCfg = (l2cCfg_t *) &l2cCfg;

/**************************************************************************************************
  ATT
**************************************************************************************************/
#if MBED_CONF_CORDIO_DESIRED_ATT_MTU < ATT_DEFAULT_MTU || MBED_CONF_CORDIO_DESIRED_ATT_MTU > ATT_MAX_MTU
#error "CORDIO_CFG_DESIRED_ATT_MTU value is outside valid range"
#endif

/* Configuration structure */
attCfg_t attCfg =
{
  15,                                  /* ATT server service discovery connection idle timeout in seconds */
  MBED_CONF_CORDIO_DESIRED_ATT_MTU,    /* desired ATT MTU */
  ATT_MAX_TRANS_TIMEOUT,               /* transaction timeout in seconds */
  MBED_CONF_CORDIO_MAX_PREPARED_WRITES /* number of queued prepare writes supported by server */
};

/* Configuration pointer */
attCfg_t *pAttCfg = (attCfg_t *) &attCfg;

/**************************************************************************************************
  EATT
**************************************************************************************************/

/* Configuration structure */
const eattCfg_t eattCfg =
{
  64,                               /* MTU */
  64,                               /* MPS */
  FALSE,                            /* Open EATT channels automatically on connect */
  FALSE,                            /* Authorization required */
  DM_SEC_LEVEL_NONE,                /* Security level required */
  0,                                /* Number of enhanced l2cap channels per connection */
  NULL                              /* Min channel priority table. Set NULL to map priority to L2CAP channel index. */
};

/* Configuration pointer */
eattCfg_t *pEattCfg = (eattCfg_t *) &eattCfg;

/* EATT_CONN_CHAN_MAX cannot be greater than L2C_COC_CHAN_MAX */
WSF_CT_ASSERT(EATT_CONN_CHAN_MAX <= L2C_COC_CHAN_MAX);

/**************************************************************************************************
  SMP
**************************************************************************************************/

/* Configuration structure */
smpCfg_t smpCfg =
{
  500,                              /* 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,              /* I/O Capability */
  7,                                /* Minimum encryption key length */
  16,                               /* Maximum encryption key length */
  1,                                /* Attempts to trigger 'repeated attempts' timeout */
  0,                                /* Device authentication requirements */
  64000,                            /* Maximum repeated attempts timeout in msec */
  64000,                            /* Time msec before attemptExp decreases */
  2                                 /* Repeated attempts multiplier exponent */
};

/* Configuration pointer */
smpCfg_t *pSmpCfg = (smpCfg_t *) &smpCfg;

/*************************************************************************************************/
/*!
*  \brief  Get Stack version number.
*
*  \param  pVersion  output parameter for version number.
*
*  \return None.
*/
/*************************************************************************************************/
void StackGetVersionNumber(const char **pVersion)
{
  *pVersion = STACK_VERSION;
}
