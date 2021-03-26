/*
 * Copyright (c) 2019 ARM Limited
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

#include "mbed_interface.h"
#include "mbed_trace.h"
#include "netsocket/nsapi_types.h"
#include "SAMSUNG_S5JS100_L3IP_Driver.h"


#define TRACE_GROUP             "S5JS100_L3IP"
#define S5JS100_BUFF_ALIGNMENT  0


SAMSUNG_S5JS100_L3IP_Driver::SAMSUNG_S5JS100_L3IP_Driver(const char *ifname, int mtu)
    : _io_device(nullptr),
      _mtu(mtu)
{
    tr_info("IF = %s, MTU = %d", ifname, mtu);
    strncpy(_ifname, ifname, sizeof(_ifname));
}

SAMSUNG_S5JS100_L3IP_Driver::~SAMSUNG_S5JS100_L3IP_Driver()
{
    power_down();
}

bool SAMSUNG_S5JS100_L3IP_Driver::link_out(net_stack_mem_buf_t *buf)
{
    bool result = true;

    MBED_ASSERT(_memory_manager);
    MBED_ASSERT(_io_device);

    // Allocate continuous buffer for data
    const int total_len = _memory_manager->get_total_len(buf);
    net_stack_mem_buf_t *cont_buf = _memory_manager->alloc_heap(total_len, S5JS100_BUFF_ALIGNMENT);

    if (cont_buf) {
        uint8_t *cont_buf_ptr = (uint8_t *)_memory_manager->get_ptr(cont_buf);

        /* copy all frames to continuous buffer */
        for (net_stack_mem_buf_t *q = buf; q != nullptr; q = _memory_manager->get_next(q)) {
            const uint8_t *ptr = (const uint8_t *)_memory_manager->get_ptr(q);
            const uint16_t len = _memory_manager->get_len(q);

            memcpy(cont_buf_ptr, ptr, len);
            cont_buf_ptr += len;
        }

        const int sent = _io_device->write((const char *)_memory_manager->get_ptr(cont_buf), total_len);
        tr_debug("link_out (%s): sent %d/%d bytes", _ifname, sent, total_len);
        if (sent != total_len) {
            result = false;
        }

        _memory_manager->free(cont_buf);
    } else {
        tr_error("Unable to allocate buffer for TX data!");
        result = false;
    }

    _memory_manager->free(buf);

    return result;
}

bool SAMSUNG_S5JS100_L3IP_Driver::power_up()
{
    _io_device = getModemIoDeviceByName(_ifname);
    tr_debug("power_up: io_device = %p", _io_device);
    if (_io_device) {
        _io_device->register_ReadCb(read_cb, this);
        _l3ip_link_state_cb(true);
        return true;
    } else {
        tr_error("Unable to get modem IO device!");
        return false;
    }
}

uint32_t SAMSUNG_S5JS100_L3IP_Driver::get_mtu_size() const
{
    tr_debug("get_mtu_size: mtu = %d", _mtu);
    return _mtu;
}

uint32_t SAMSUNG_S5JS100_L3IP_Driver::get_align_preference() const
{
    tr_debug("get_align_preference: align = %d", S5JS100_BUFF_ALIGNMENT);
    return S5JS100_BUFF_ALIGNMENT;
}

void SAMSUNG_S5JS100_L3IP_Driver::get_ifname(char *name, uint8_t size) const
{
    strncpy(name, _ifname, size);
}

void SAMSUNG_S5JS100_L3IP_Driver::set_link_input_cb(l3ip_link_input_cb_t input_cb)
{
    _l3ip_link_input_cb = input_cb;
}

void SAMSUNG_S5JS100_L3IP_Driver::set_link_state_cb(l3ip_link_state_change_cb_t state_cb)
{
    _l3ip_link_state_cb = state_cb;
}

/* This is pure virtual function in base class and not used
*  No-op at this stage
*/
void SAMSUNG_S5JS100_L3IP_Driver::add_ipv4_multicast_group(const SocketAddress &address)
{
}

/* This is pure virtual function in base class and not used
*  No-op at this stage
*/
void SAMSUNG_S5JS100_L3IP_Driver::add_ipv6_multicast_group(const SocketAddress &address)
{
}

/* This is pure virtual function in base class and not used
*  No-op at this stage
*/
void SAMSUNG_S5JS100_L3IP_Driver::remove_ipv4_multicast_group(const SocketAddress &address)
{
}

/* This is pure virtual function in base class and not used
*  No-op at this stage
*/
void SAMSUNG_S5JS100_L3IP_Driver::remove_ipv6_multicast_group(const SocketAddress &address)
{
}

/* This is pure virtual function in base class and not used
*  No-op at this stage
*/
void SAMSUNG_S5JS100_L3IP_Driver::set_all_multicast(bool all)
{
}

void SAMSUNG_S5JS100_L3IP_Driver::power_down()
{
    tr_debug("power_down");
    if (_io_device) {
        _io_device->register_ReadCb(nullptr, nullptr);
        _io_device = nullptr;
    }
}

void SAMSUNG_S5JS100_L3IP_Driver::set_memory_manager(NetStackMemoryManager &mem_mngr)
{
    _memory_manager = &mem_mngr;
}

void SAMSUNG_S5JS100_L3IP_Driver::read_cb(mio_buf *buf, void *param)
{
    MBED_ASSERT(buf);
    MBED_ASSERT(param);

    SAMSUNG_S5JS100_L3IP_Driver *driver = (SAMSUNG_S5JS100_L3IP_Driver *)param;
    driver->packet_rx(buf);
}

void SAMSUNG_S5JS100_L3IP_Driver::packet_rx(mio_buf *buf)
{
    tr_debug("packet_rx (%s): buf = %p, len = %d, ch = %d", _ifname, buf->data, buf->len, buf->ch);

    net_stack_mem_buf_t *read_buf = _memory_manager->alloc_heap(buf->len, S5JS100_BUFF_ALIGNMENT);
    if (read_buf) {
        _memory_manager->copy_to_buf(read_buf, buf->data, buf->len);
        _l3ip_link_input_cb(read_buf);
    } else {
        tr_error("Unable to allocate buffer for RX data!");
    }
}

/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */
