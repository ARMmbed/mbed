/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  LL initialization for SoC configuration.
 *
 *  Copyright (c) 2013-2019 Arm Ltd. All Rights Reserved.
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

#include "ll_init_api.h"
#include "bb_ble_api.h"
#include "pal_bb_ble.h"
#include "pal_radio.h"
#include "sch_api.h"
#include "bb_ble_sniffer_api.h"

/**************************************************************************************************
  Functions
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Initialize BB.
 */
/*************************************************************************************************/
void LlInitBbInit(void)
{
  BbInit();

  BbBleInit();

  #ifdef INIT_CENTRAL
    BbBleScanMasterInit();
    BbBleConnMasterInit();
  #else
    #ifdef INIT_OBSERVER
      BbBleScanMasterInit();
    #endif
  #endif

  #ifdef INIT_PERIPHERAL
    BbBleAdvSlaveInit();
    BbBleConnSlaveInit();
  #else
    #ifdef INIT_BROADCASTER
      BbBleAdvSlaveInit();
    #endif
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_0)
    #ifdef INIT_OBSERVER
      BbBleAuxScanMasterInit();
      BbBlePerScanMasterInit();
    #endif

    #ifdef INIT_BROADCASTER
      BbBleAuxAdvSlaveInit();
      PalRadioInitPathComp();
    #endif
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
    #ifdef INIT_CENTRAL
      BbBleCisMasterInit();
    #endif
    #ifdef INIT_PERIPHERAL
      BbBleCisSlaveInit();
    #endif
    #ifdef INIT_OBSERVER
    BbBleBisMasterInit();
    #endif
    #ifdef INIT_BROADCASTER
    BbBleBisSlaveInit();
    #endif
  #endif

  BbBleTestInit();

#if (BB_SNIFFER_ENABLED == TRUE)
  BbBleInitSniffer(BB_SNIFFER_OUTPUT_NULL_METHOD, FALSE);
#endif
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize scheduler.
 */
/*************************************************************************************************/
void LlInitSchInit(void)
{
  wsfHandlerId_t handlerId = WsfOsSetNextHandler(SchHandler);
  SchHandlerInit(handlerId);
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize LL.
 */
/*************************************************************************************************/
void LlInitLlInit(void)
{
  #ifdef INIT_CENTRAL
    LlScanMasterInit();
    LlInitMasterInit();
    #ifdef INIT_ENCRYPTED
      LlEncConnMasterInit();
    #else
      LlConnMasterInit();
    #endif
  #else
    #ifdef INIT_OBSERVER
      LlScanMasterInit();
    #endif
  #endif

  #ifdef INIT_PERIPHERAL
    LlAdvSlaveInit();
    #ifdef INIT_ENCRYPTED
      LlEncConnSlaveInit();
    #else
      LlConnSlaveInit();
    #endif
  #else
    #ifdef INIT_BROADCASTER
      LlAdvSlaveInit();
    #endif
  #endif

  #ifdef INIT_ENCRYPTED
    LlPrivInit();
    LlScInit();
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_0)
    #ifdef INIT_CENTRAL
      LlExtScanMasterInit();
      LlExtInitMasterInit();
      LlPhyMasterInit();
      #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
        LlCisMasterInit();
        LlBisMasterInit();
      #endif
    #else
      #ifdef INIT_OBSERVER
        LlExtScanMasterInit();
        #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
          LlBisMasterInit();
        #endif
      #endif
    #endif

    #ifdef INIT_PERIPHERAL
      LlExtAdvSlaveInit();
      LlPhySlaveInit();
      #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
        LlCisSlaveInit();
        LlBisSlaveInit();
      #endif
    #else
      #ifdef INIT_BROADCASTER
        LlExtAdvSlaveInit();
        #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
          LlBisSlaveInit();
        #endif
      #endif
    #endif

    #if defined(INIT_PERIPHERAL) || defined(INIT_CENTRAL)
      LlChannelSelection2Init();
      #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_1)
        LlPastInit();
      #endif
      #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
        LlPowerControlInit();
      #endif
      #if (BT_VER >= LL_VER_BT_CORE_SPEC_SYDNEY)
        LlEnhConnUpdateInit();
      #endif
    #endif
  #endif

  /* Initialize handler after feature bits are set. */
  wsfHandlerId_t handlerId = WsfOsSetNextHandler(LlHandler);
  LlHandlerInit(handlerId);
}

/*************************************************************************************************/
/*!
 *  \brief  Set BB runtime configuration.
 *
 *  \param  pBbRtCfg        BB runtime configuration (must be static).
 *  \param  wlSizeCfg       Whitelist size.
 *  \param  rlSizeCfg       Resolving list size.
 *  \param  plSizeCfg       Periodic list size.
 *  \param  pFreeMem        Free memory.
 *  \param  freeMemAvail    Amount of free memory in bytes.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint32_t LlInitSetBbRtCfg(const BbRtCfg_t *pBbRtCfg, const uint8_t wlSizeCfg, const uint8_t rlSizeCfg,
                          const uint8_t plSizeCfg, uint8_t *pFreeMem, uint32_t freeMemAvail)
{
  uint32_t memUsed;
  uint32_t totalMemUsed = 0;

  BbInitRunTimeCfg(pBbRtCfg);

  memUsed = BbBleInitWhiteList(wlSizeCfg, pFreeMem, freeMemAvail);
  pFreeMem += memUsed;
  freeMemAvail -= memUsed;
  totalMemUsed += memUsed;

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_0)
    memUsed = BbBleInitPeriodicList(plSizeCfg, pFreeMem, freeMemAvail);
    pFreeMem += memUsed;
    freeMemAvail -= memUsed;
    totalMemUsed += memUsed;
  #endif

  #ifdef INIT_ENCRYPTED
    memUsed = BbBleInitResolvingList(rlSizeCfg, pFreeMem, freeMemAvail);
    /* pFreeMem += memUsed; */
    /* freeMemAvail -= memUsed; */
    totalMemUsed += memUsed;
  #else
    BbBleInitResolvingList(0, NULL, 0);
  #endif

  return totalMemUsed;
}

/*************************************************************************************************/
/*!
 *  \brief  Set LL runtime configuration.
 *
 *  \param  pLlRtCfg        LL runtime configuration (must be static).
 *  \param  pFreeMem        Free memory.
 *  \param  freeMemAvail    Amount of free memory in bytes.
 *
 *  \return Amount of free memory consumed.
 */
/*************************************************************************************************/
uint32_t LlInitSetLlRtCfg(const LlRtCfg_t *pLlRtCfg, uint8_t *pFreeMem, uint32_t freeMemAvail)
{
  uint32_t totalMemUsed = 0;

  #if defined (INIT_PERIPHERAL) || defined (INIT_CENTRAL) || (BT_VER >= LL_VER_BT_CORE_SPEC_5_0)
  uint32_t memUsed;
  #endif

  LlInitRunTimeCfg(pLlRtCfg);

  #if defined (INIT_PERIPHERAL) || defined (INIT_CENTRAL)
    if (pLlRtCfg->maxConn)
    {
      memUsed = LlInitConnMem(pFreeMem, freeMemAvail);
      pFreeMem += memUsed;
      freeMemAvail -= memUsed;
      totalMemUsed += memUsed;
    }
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_0)
    memUsed = LlInitExtScanMem(pFreeMem, freeMemAvail);
    pFreeMem += memUsed;
    freeMemAvail -= memUsed;
    totalMemUsed += memUsed;

    memUsed = LlInitExtAdvMem(pFreeMem, freeMemAvail);
    pFreeMem += memUsed;
    freeMemAvail -= memUsed;
    totalMemUsed += memUsed;
  #endif

  #if (BT_VER >= LL_VER_BT_CORE_SPEC_5_2)
    memUsed = LlInitCisMem(pFreeMem, freeMemAvail);
    pFreeMem += memUsed;
    freeMemAvail -= memUsed;
    totalMemUsed += memUsed;

    memUsed = LlInitBisMem(pFreeMem, freeMemAvail);
    pFreeMem += memUsed;
    freeMemAvail -= memUsed;
    totalMemUsed += memUsed;

    memUsed = LlInitIsoMem(pFreeMem, freeMemAvail);
    /* pFreeMem += memUsed; */
    /* freeMemAvail -= memUsed; */
    totalMemUsed += memUsed;
  #endif

  return totalMemUsed;
}

/*************************************************************************************************/
/*!
 *  \brief  Initialize configuration.
 *
 *  \param  pCfg    Runtime configuration.
 *
 *  \return Memory used.
 */
/*************************************************************************************************/
uint32_t LlInit(LlInitRtCfg_t *pCfg)
{
  uint32_t memUsed;
  uint32_t totalMemUsed = 0;

  memUsed = LlInitSetBbRtCfg(pCfg->pBbRtCfg, pCfg->wlSizeCfg, pCfg->rlSizeCfg, pCfg->plSizeCfg,
                             pCfg->pFreeMem, pCfg->freeMemAvail);
  pCfg->pFreeMem += memUsed;
  pCfg->freeMemAvail -= memUsed;
  totalMemUsed += memUsed;

  memUsed = LlInitSetLlRtCfg(pCfg->pLlRtCfg, pCfg->pFreeMem, pCfg->freeMemAvail);
  pCfg->pFreeMem += memUsed;
  pCfg->freeMemAvail -= memUsed;
  totalMemUsed += memUsed;

  LlInitBbInit();
  LlInitSchInit();
  LlInitLlInit();

  return totalMemUsed;
}
