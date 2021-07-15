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
#ifndef CFG_STACK_H
#define CFG_STACK_H


#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup STACK_INIT
 *  \{ */

/**************************************************************************************************
  STACK VERSION
**************************************************************************************************/

/*! \brief Stack release version label */
#define STACK_VERSION         ((const char *)"Packetcraft Host v5.2\n")
/*! \brief Stack release version number */
#define STACK_VER_NUM         21015

/**************************************************************************************************
  HCI
**************************************************************************************************/

/** \name HCI Vendor Specific targets
 *
 */
/**@{*/
#define HCI_VS_GENERIC        0
#define HCI_VS_EMM            1

/*! \brief Vendor specific target configuration */
#ifndef HCI_VS_TARGET
#define HCI_VS_TARGET HCI_VS_GENERIC
#endif
/**@}*/

/** \name HCI Tx Data Tailroom
 * Extra byte allocation required for LL operations (i.e. MIC) in single-chip implementation
 */
/**@{*/
#ifndef HCI_TX_DATA_TAILROOM
/*! \brief Tx data tailroom. */
#define HCI_TX_DATA_TAILROOM  0
#endif
/**@}*/

/**************************************************************************************************
  DM
**************************************************************************************************/

/** \name DM Configuration
 *  DM build-time configuration parameters
 */
/**@{*/
/*! \brief Maximum number of connections */
#ifndef DM_CONN_MAX
#define DM_CONN_MAX              3
#endif

/*! \brief Maximum number of periodic advertising synchronizations */
#ifndef DM_SYNC_MAX
#define DM_SYNC_MAX              3
#endif

/*! \brief Number of supported advertising sets: must be set to 1 for legacy advertising */
#ifndef DM_NUM_ADV_SETS
#define DM_NUM_ADV_SETS          1
#endif

/*! \brief Number of scanner and initiator PHYs (LE 1M, LE 2M and LE Coded): must be set to 1 for
    legacy scanner and initiator */
#ifndef DM_NUM_PHYS
#define DM_NUM_PHYS              1
#endif

/*! \brief Maximum number of connected isochronous groups */
#ifndef DM_CIG_MAX
#define DM_CIG_MAX              2
#endif

/*! \brief Maximum number of connected isochronous streams, it is shared by CIGs */
#ifndef DM_CIS_MAX
#define DM_CIS_MAX              6
#endif

/*! \brief Maximum number of broadcast isochronous groups */
#ifndef DM_BIG_MAX
#define DM_BIG_MAX              2
#endif

/*! \brief Maximum number of broadcast isochronous streams, it is shared by BIGs */
#ifndef DM_BIS_MAX
#define DM_BIS_MAX              6
#endif

/*! \brief Maximum number of isochronous data paths, it is shared by CISes and BISes */
#define DM_ISO_DATA_PATH_MAX    (DM_CIS_MAX + DM_BIS_MAX)

/**@}*/

/**************************************************************************************************
  L2C
**************************************************************************************************/

/** \name L2CAP Configuration
 * L2CAP build-time configuration parameters
 */
/**@{*/
/*! \brief Maximum number of connection oriented channels */
#ifndef L2C_COC_CHAN_MAX
#define L2C_COC_CHAN_MAX         8
#endif

/*! \brief Maximum number of connection oriented channel registered clients */
#ifndef L2C_COC_REG_MAX
#define L2C_COC_REG_MAX          4
#endif
/**@}*/

/**************************************************************************************************
  ATT
**************************************************************************************************/

/** \name ATT Configuration
 * ATT build-time configuration parameters
 */
/**@{*/
/*! \brief Maximum number of simultaneous ATT write commands */
#ifndef ATT_NUM_SIMUL_WRITE_CMD
#define ATT_NUM_SIMUL_WRITE_CMD  1
#endif

/*! \brief Maximum number of simultaneous ATT notifications */
#ifndef ATT_NUM_SIMUL_NTF
#define ATT_NUM_SIMUL_NTF        1
#endif

/* Maximum number of EATT channels per DM connection.  Set to zero if EATT is not required. */
#ifndef EATT_CONN_CHAN_MAX
#define EATT_CONN_CHAN_MAX       0
#endif

/**@}*/

/**************************************************************************************************
  SMP
**************************************************************************************************/

/** \name SMP Configuration
 * SMP build-time configuration parameters
 */
/**@{*/
/*! Max number of devices in the database */
#ifndef SMP_DB_MAX_DEVICES
#define SMP_DB_MAX_DEVICES       3
#endif
/**@}*/

/*************************************************************************************************/
/*!
*  \brief  Get Stack version number.
*
*  \param  pVersion  output parameter for version number.
*
*  \return None.
*/
/*************************************************************************************************/
void StackGetVersionNumber(const char **pVersion);

/*! \} */    /* STACK_INIT */

#ifdef __cplusplus
};
#endif

#endif /* CFG_STACK_H */
