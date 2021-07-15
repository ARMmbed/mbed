/*************************************************************************************************/
/*!
 *  \brief  LL initialization for controller configuration.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "ll_init_api.h"
#include "lhci_api.h"
#include "chci_api.h"

/**************************************************************************************************
  Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Initialize controller LHCI handler.
 */
/*************************************************************************************************/
void LlInitLhciHandler(void)
{
  wsfHandlerId_t handlerId;

#if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
  handlerId = WsfOsSetNextHandler(LhciIsoHandler);
  LhciIsoHandlerInit(handlerId);
#else
  handlerId = WsfOsSetNextHandler(LhciHandler);
  LhciHandlerInit(handlerId);
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize controller HCI transport.
 *
 *  \param  maxAclLen       Maximum ACL data length.
 *  \param  maxIsoSduLen    Maximum ISO SDU data length.
 */
/*************************************************************************************************/
void LlInitChciTrInit(uint16_t maxAclLen, uint16_t maxIsoSduLen)
{
  wsfHandlerId_t handlerId = WsfOsSetNextHandler(ChciTrHandler);
  ChciTrHandlerInit(handlerId, maxAclLen, maxIsoSduLen);
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize LL HCI.
 */
/*************************************************************************************************/
void LlInitLhciInit(void)
{
  #ifdef INIT_CENTRAL
    LhciScanMasterInit();
    #ifdef INIT_ENCRYPTED
      LhciEncMasterInit();
    #endif
    LhciConnMasterInit();
  #else
    #ifdef INIT_OBSERVER
      LhciScanMasterInit();
    #endif
  #endif

  #ifdef INIT_PERIPHERAL
    LhciAdvSlaveInit();
    #ifdef INIT_ENCRYPTED
      LhciEncSlaveInit();
    #endif
    LhciConnInit();
  #else
    #ifdef INIT_BROADCASTER
      LhciAdvSlaveInit();
    #endif
  #endif

  #ifdef INIT_ENCRYPTED
    LhciPrivAdvInit();
    LhciPrivConnInit();
    LhciScInit();
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_0)
    #ifdef INIT_CENTRAL
      LhciExtScanMasterInit();
      LhciExtConnMasterInit();
    #else
      #ifdef INIT_OBSERVER
        LhciExtScanMasterInit();
      #endif
    #endif

    #ifdef INIT_BROADCASTER
      LhciExtAdvSlaveInit();
    #endif

    #if defined(INIT_PERIPHERAL) || defined(INIT_CENTRAL)
      LhciChannelSelection2Init();
      LhciPhyInit();
      #if defined(INIT_OBSERVER) || defined(INIT_BROADCASTER)
        #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_1)
          LhciPastInit();
        #endif
        #if (BT_VER >= LL_VER_BT_CORE_SPEC_SYDNEY)
          LhciEnhConnUpdateInit();
        #endif
      #endif
    #endif
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
    #ifdef INIT_CENTRAL
      LhciCisMasterInit();
      LhciBisMasterInit();
    #else
      #ifdef INIT_OBSERVER
        LhciBisMasterInit();
      #endif
    #endif

    #ifdef INIT_BROADCASTER
      LhciBisSlaveInit();
    #endif

    #ifdef INIT_PERIPHERAL
      LhciCisSlaveInit();
    #endif

    LhciIsoInit();
    LhciPowerControlInit();
  #endif
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize controller configuration.
 *
 *  \param  pCfg    Runtime configuration.
 *
 *  \return Memory used.
 */
/*************************************************************************************************/
uint32_t LlInitControllerInit(LlInitRtCfg_t *pCfg)
{
  uint32_t totalMemUsed;

  totalMemUsed = LlInit(pCfg);

  LlInitChciTrInit(pCfg->pLlRtCfg->maxAclLen, pCfg->pLlRtCfg->maxIsoSduLen);
  LlInitLhciInit();
  LlInitLhciHandler();
  LhciInitFinalize();

  #if (CHCI_TR_COMP_ENA)
  {
    extern void LhciTrEventComplete(void);
    LhciRegisterSendTrCompleteHandler(LhciTrEventComplete);
  }
  #endif

  return totalMemUsed;
}
