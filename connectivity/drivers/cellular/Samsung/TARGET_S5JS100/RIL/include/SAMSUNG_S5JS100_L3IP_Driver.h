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

#ifndef SAMSUNG_S5JS100_L3IP_DRIVER_H_
#define SAMSUNG_S5JS100_L3IP_DRIVER_H_

#include "L3IP.h"
#include "modem_io_device.h"

class SAMSUNG_S5JS100_L3IP_Driver : public L3IP {

public:

    SAMSUNG_S5JS100_L3IP_Driver(const char *ifname, int mtu);
    virtual ~SAMSUNG_S5JS100_L3IP_Driver();

    /**
     * Return maximum transmission unit
     *
     * @return     MTU in bytes
     */
    uint32_t get_mtu_size() const override;

    /**
     * Gets memory buffer alignment preference
     *
     * Gets preferred memory buffer alignment of the cellular device.
     * @return         Memory alignment requirement in bytes
     */
    uint32_t get_align_preference() const override;

    /**
     * Return interface name
     *
     * @param name Pointer to where the name should be written
     * @param size Maximum number of characters to copy
     */
    void get_ifname(char *name, uint8_t size) const override;

    /**
     * Sends the packet over the link
     *
     * That cannot be called from an interrupt context.
     *
     * @param buf  Packet to be sent
     * @return     True if the packet was sent, false otherwise
     */
    bool link_out(net_stack_mem_buf_t *buf) override;

    /**
     * Initializes the hardware
     *
     * @return True on success, False in case of an error.
     */
    bool power_up() override;

    /**
     * Deinitializes the hardware
     *
     */
    void power_down() override;

    /**
     * Sets a callback that needs to be called for packets received for that interface
     *
     * @param input_cb Function to be register as a callback
     */
    void set_link_input_cb(l3ip_link_input_cb_t input_cb) override;

    /**
     * Sets a callback that needs to be called on link status changes for given interface
     *
     * @param state_cb Function to be register as a callback
     */
    void set_link_state_cb(l3ip_link_state_change_cb_t state_cb) override;

    /** Add device to an IP4 multicast group
     *
     * @param address  An IP4 multicast group address
     */
    void add_ipv4_multicast_group(const SocketAddress &address) override;

    /** Add device to an IP6 multicast group
     *
     * @param address  An IP6 multicast group address
     */
    void add_ipv6_multicast_group(const SocketAddress &address) override;

    /** Remove device from an IPV4 multicast group
     *
     * @param address  An IPV4 multicast group address
     */
    void remove_ipv4_multicast_group(const SocketAddress &address) override;

    /** Remove device from an IPV6 multicast group
    *
    * @param address  An IPV6 multicast group address
    */
    void remove_ipv6_multicast_group(const SocketAddress &address) override;

    /** Request reception of all multicast packets
     *
     * @param all True to receive all multicasts
     *            False to receive only multicasts addressed to specified groups
     */
    void set_all_multicast(bool all) override;

    /** Sets memory manager that is used to handle memory buffers
     *
     * @param mem_mngr Pointer to memory manager
     */
    void set_memory_manager(NetStackMemoryManager &mem_mngr) override;

private:

    /** Callback from ModemIoDevice indicating incoming data
     *
     *@param buf   Pointer to received buffer
     *@param param Pointer to SAMSUNG_S5JS100_L3IP_Driver instance
     */
    static void read_cb(mio_buf *buf, void *param);

    /** Send buffer to LWIP
     *
     * @param buf Pointer to received buffer
     */
    void packet_rx(mio_buf *buf);

    // Prevent empty and copy construction
    SAMSUNG_S5JS100_L3IP_Driver();
    SAMSUNG_S5JS100_L3IP_Driver(const SAMSUNG_S5JS100_L3IP_Driver &);

private:

    l3ip_link_input_cb_t        _l3ip_link_input_cb;    /**< Callback for incoming data */
    l3ip_link_state_change_cb_t _l3ip_link_state_cb;    /**< Link state change callback */
    NetStackMemoryManager       *_memory_manager;       /**< Memory manager */

    ModemIoDevice              *_io_device;             /**< Handle to modem */
    char                        _ifname[32];            /**< Interface name */
    int                         _mtu;                   /**< MTU */
};

#endif /* SAMSUNG_S5JS100_L3IP_DRIVER_H_ */
