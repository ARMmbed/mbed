/* mbed Microcontroller Library
 * Copyright (c) 2021 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MBED_WSF_TRACE_H_
#define MBED_WSF_TRACE_H_

#if MBED_CONF_MBED_TRACE_ENABLE
void wsf_mbed_trace_init();

# if MBED_CONF_CORDIO_TRACE_HCI_PACKETS
void wsf_mbed_trace_hci(const uint8_t *pBuf, uint32_t len, const uint8_t *prefix);

#  define HCI_PDUMP_CMD(len, pBuf)    wsf_mbed_trace_hci(len, pBuf, "CMD")
#  define HCI_PDUMP_EVT(len, pBuf)    wsf_mbed_trace_hci(len, pBuf, "EVT")
#  define HCI_PDUMP_TX_ACL(len, pBuf) wsf_mbed_trace_hci(len, pBuf, "ACL_TX")
#  define HCI_PDUMP_RX_ACL(len, pBuf) wsf_mbed_trace_hci(len, pBuf, "ACL_RX")
#  define HCI_PDUMP_TX_ISO(len, pBuf) wsf_mbed_trace_hci(len, pBuf, "ISO_TX")
#  define HCI_PDUMP_RX_ISO(len, pBuf) wsf_mbed_trace_hci(len, pBuf, "ISO_RX")
# else
#  define HCI_PDUMP_CMD(len, pBuf)
#  define HCI_PDUMP_EVT(len, pBuf)
#  define HCI_PDUMP_TX_ACL(len, pBuf)
#  define HCI_PDUMP_RX_ACL(len, pBuf)
#  define HCI_PDUMP_TX_ISO(len, pBuf)
#  define HCI_PDUMP_RX_ISO(len, pBuf)
# endif // MBED_CONF_CORDIO_TRACE_HCI_PACKETS

#else
# define HCI_PDUMP_CMD(len, pBuf)
# define HCI_PDUMP_EVT(len, pBuf)
# define HCI_PDUMP_TX_ACL(len, pBuf)
# define HCI_PDUMP_RX_ACL(len, pBuf)
# define HCI_PDUMP_TX_ISO(len, pBuf)
# define HCI_PDUMP_RX_ISO(len, pBuf)
#endif // MBED_CONF_MBED_TRACE_ENABLE

#endif // MBED_WSF_TRACE_H_
