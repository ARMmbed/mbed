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
#include "SAMSUNG_S5JS100_RIL_ControlPlane_netif.h"
#include "mbed_trace.h"

#define TRACE_GROUP "SCPNIF"

namespace mbed {

SAMSUNG_S5JS100_RIL_ControlPlane_netif::SAMSUNG_S5JS100_RIL_ControlPlane_netif(char *ifname)
    : _cb(nullptr),
      _data(nullptr),
      _recv_len(0)
{
    _io_device = getModemIoDeviceByName(ifname);
    tr_debug("io_device = %p", _io_device);
    if (_io_device) {
        _io_device->register_ReadCb(read_cb, this);
    } else {
        tr_error("Unable to get modem IO device!");
    }
}

SAMSUNG_S5JS100_RIL_ControlPlane_netif::~SAMSUNG_S5JS100_RIL_ControlPlane_netif()
{
    if (_io_device) {
        _io_device->register_ReadCb(nullptr, nullptr);
        _io_device = nullptr;
    }
}

nsapi_size_or_error_t SAMSUNG_S5JS100_RIL_ControlPlane_netif::send(const void *cpdata, nsapi_size_t cpdata_length)
{
    MBED_ASSERT(_io_device);
    const nsapi_size_t sent = _io_device->write((const char *)cpdata, cpdata_length);
    tr_debug("send: sent %d/%d bytes", sent, cpdata_length);
    return sent;
}

nsapi_size_or_error_t SAMSUNG_S5JS100_RIL_ControlPlane_netif::recv(void *cpdata, nsapi_size_t cpdata_length)
{
    // If no data received through modem callback
    if (!_recv_len) {
        return NSAPI_ERROR_WOULD_BLOCK;
    }

    // If too small buffer for data
    if (_recv_len > cpdata_length) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    memcpy(cpdata, _recv_buffer, _recv_len);
    const uint32_t ret_len = _recv_len;
    _recv_len = 0;

    tr_info("recv: len = %" PRIu32, ret_len);

    return ret_len;
}

void SAMSUNG_S5JS100_RIL_ControlPlane_netif::data_received()
{
    // call socket event
    if (!_cb) {
        return;
    }
    _cb(_data);
}

void SAMSUNG_S5JS100_RIL_ControlPlane_netif::attach(void (*callback)(void *), void *data)
{
    _cb = callback;
    _data = data;
}

void SAMSUNG_S5JS100_RIL_ControlPlane_netif::read_cb(mio_buf *buf, void *param)
{
    MBED_ASSERT(buf);
    MBED_ASSERT(param);

    SAMSUNG_S5JS100_RIL_ControlPlane_netif *driver = (SAMSUNG_S5JS100_RIL_ControlPlane_netif *)param;
    driver->packet_rx(buf);
}

void SAMSUNG_S5JS100_RIL_ControlPlane_netif::packet_rx(mio_buf *buf)
{
    tr_debug("packet_rx: buf = %p, len = %d, ch = %d", buf->data, buf->len, buf->ch);

    const uint32_t cpdata_length = buf->len;
    if (cpdata_length > 0 && cpdata_length <= sizeof(_recv_buffer)) {
        memcpy(_recv_buffer, buf->data, cpdata_length);
        _recv_len = cpdata_length;
        data_received();
    } else {
        tr_error("Invalid data size (%" PRIu32 ")", cpdata_length);
    }
}

} /* namespace mbed */
