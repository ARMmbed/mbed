/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Link layer controller PHY features (slave) implementation file.
 *
 *  Copyright (c) 2016-2018 Arm Ltd. All Rights Reserved.
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

#include "lctr_int_conn.h"
#include "lctr_int_conn_slave.h"
#include "lctr_int_slave_phy.h"
#include "lctr_api_phy.h"

/*************************************************************************************************/
/*!
 *  \brief      Initialize link layer controller resources for PHY features (slave).
 */
/*************************************************************************************************/
void LctrSlvPhyConnInit(void)
{
  /* Add LLCP SM handler. */
  lctrSlvLlcpSmTbl[LCTR_LLCP_SM_PHY_UPD] = lctrSlvLlcpExecutePhyUpdateSm;

  lctrSlvCheckEncOverridePhyUpdateFn = lctrSlvCheckEncOverridePhyUpdate;
}
