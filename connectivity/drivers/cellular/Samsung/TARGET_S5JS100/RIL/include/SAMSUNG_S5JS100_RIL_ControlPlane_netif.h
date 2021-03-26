/*
 * Copyright (c) 2020, Arm Limited and affiliates.
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
#ifndef SAMSUNG_S5JS100_RIL_CONTROLPLANE_NETIF_H_
#define SAMSUNG_S5JS100_RIL_CONTROLPLANE_NETIF_H_

#include "ControlPlane_netif.h"
#include "modem_io_device.h"
#include <inttypes.h>

namespace mbed {

class SAMSUNG_S5JS100_RIL_ControlPlane_netif: public ControlPlane_netif {
public:
    SAMSUNG_S5JS100_RIL_ControlPlane_netif(char *ifname);
    virtual ~SAMSUNG_S5JS100_RIL_ControlPlane_netif();

protected:
    // ControlPlane_netif
    nsapi_size_or_error_t send(const void *cpdata, nsapi_size_t cpdata_length) override;
    nsapi_size_or_error_t recv(void *cpdata, nsapi_size_t cpdata_length) override;
    void data_received() override;
    void attach(void (*callback)(void *), void *data) override;

private:
    static void read_cb(mio_buf *buf, void *param);
    void packet_rx(mio_buf *buf);

private:
    void (*_cb)(void *);
    void *_data;
    char _recv_buffer[MBED_CONF_CELLULAR_MAX_CP_DATA_RECV_LEN];
    uint32_t _recv_len;

    ModemIoDevice *_io_device;
};

} /* namespace mbed */

#endif /* SAMSUNG_S5JS100_RIL_CONTROLPLANE_NETIF_H_ */
