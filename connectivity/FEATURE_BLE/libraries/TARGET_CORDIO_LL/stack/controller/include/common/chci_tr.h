/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Controller HCI transport interface file.
 *
 *  Copyright (c) 2013-2019 ARM Ltd. All Rights Reserved.
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

#ifndef CHCI_TR_H
#define CHCI_TR_H

#include "wsf_types.h"
#include "wsf_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifndef UART_BAUD
/*! \brief  Default UART baud rate. */
#define UART_BAUD   1000000
#endif

#ifndef UART_HWFC
/*! \brief  Default Hardware Flow Control. */
#define UART_HWFC    1
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \brief  Signal the completion of a message receive.
 *
 *  \param  prot    Protocol.
 *  \param  type    Message type.
 *  \param  pBuf    Message.
 */
/*************************************************************************************************/
void chciTrRecv(uint8_t prot, uint8_t type, uint8_t *pBuf);

/*************************************************************************************************/
/*!
 *  \brief  Signal the completion of a message transmission.
 */
/*************************************************************************************************/
void chciTrSendComplete(void);

#ifdef __cplusplus
};
#endif

#endif /* CHCI_TR_H */
