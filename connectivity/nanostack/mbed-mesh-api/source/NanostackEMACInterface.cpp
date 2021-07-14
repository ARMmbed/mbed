/*
 * Copyright (c) 2017 ARM Limited. All rights reserved.
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

#include "NanostackEthernetInterface.h"
#include "NanostackEthernetPhy.h"
#include "nsdynmemLIB.h"
#include "arm_hal_phy.h"
#include "EMAC.h"
#include "enet_tasklet.h"
#include "mbed_interface.h"

class EMACPhy : public NanostackEthernetPhy {
public:
    EMACPhy(NanostackMemoryManager &mem, EMAC &m);
    int8_t phy_register() override;
    void get_mac_address(uint8_t *mac) override;
    void set_mac_address(uint8_t *mac) override;

    int8_t address_write(phy_address_type_e, uint8_t *);
    int8_t tx(uint8_t *data_ptr, uint16_t data_len, uint8_t tx_handle, data_protocol_e data_flow);

    void emac_phy_rx(emac_mem_buf_t *mem);

private:
    NanostackMemoryManager &memory_manager;
    EMAC &emac;
    uint8_t mac_addr[6];
    int8_t device_id = -1;
    phy_device_driver_s phy = {};
};

// GAH! no handles on the callback. Force a single interface
static EMACPhy *single_phy;

extern "C"
{
    static int8_t emac_phy_address_write(phy_address_type_e address_type, uint8_t *address_ptr)
    {
        return single_phy->address_write(address_type, address_ptr);
    }

    static int8_t emac_phy_interface_state_control(phy_interface_state_e, uint8_t)
    {
        return -1;
    }

    static int8_t emac_phy_tx(uint8_t *data_ptr, uint16_t data_len, uint8_t tx_handle, data_protocol_e data_flow)
    {
        return single_phy->tx(data_ptr, data_len, tx_handle, data_flow);
    }

    EMACPhy::EMACPhy(NanostackMemoryManager &mem, EMAC &m) : memory_manager(mem), emac(m)
    {
        /* Same default address logic as lwIP glue uses */
#if (MBED_MAC_ADDRESS_SUM != MBED_MAC_ADDR_INTERFACE)
        mac_addr[0] = MBED_MAC_ADDR_0;
        mac_addr[1] = MBED_MAC_ADDR_1;
        mac_addr[2] = MBED_MAC_ADDR_2;
        mac_addr[3] = MBED_MAC_ADDR_3;
        mac_addr[4] = MBED_MAC_ADDR_4;
        mac_addr[5] = MBED_MAC_ADDR_5;
#else
        mbed_mac_address((char *) mac_addr);
#endif
        /* We have a default MAC address, so do don't force them to supply one */
        /* They may or may not update hwaddr with their address */
        emac.get_hwaddr(mac_addr);
    }


    void EMACPhy::emac_phy_rx(emac_mem_buf_t *mem)
    {
        const uint8_t *ptr = NULL;
        uint8_t *tmpbuf = NULL;
        uint32_t total_len;

        if (memory_manager.get_next(mem) == NULL) {
            // Easy contiguous case
            ptr = static_cast<const uint8_t *>(memory_manager.get_ptr(mem));
            total_len = memory_manager.get_len(mem);
        } else {
            // Nanostack can't accept chunked data - make temporary contiguous copy
            total_len = memory_manager.get_total_len(mem);
            ptr = tmpbuf = static_cast<uint8_t *>(ns_dyn_mem_temporary_alloc(total_len));
            if (tmpbuf) {
                memory_manager.copy_from_buf(tmpbuf, total_len, mem);
            }
        }

        if (ptr && phy.phy_rx_cb) {
            phy.phy_rx_cb(ptr, total_len, 0xff, 0, device_id);
        }
        ns_dyn_mem_free(tmpbuf);
        memory_manager.free(mem);
    }

} // extern "C"

int8_t EMACPhy::address_write(phy_address_type_e address_type, uint8_t *address_ptr)
{
    if (address_type != PHY_MAC_48BIT) {
        return -1;
    }
    memcpy(mac_addr, address_ptr, 6);
    emac.set_hwaddr(address_ptr);
    return 0;
}

int8_t EMACPhy::tx(uint8_t *data_ptr, uint16_t data_len, uint8_t tx_handle, data_protocol_e data_flow)
{
    emac_mem_buf_t *mem = memory_manager.alloc_pool(data_len, 0);
    if (!mem) {
        return -1;
    }
    memory_manager.copy_to_buf(mem, data_ptr, data_len);

    // They take ownership - their responsibility to free
    emac.link_out(mem);

    return 0;
}

int8_t EMACPhy::phy_register()
{
    if (device_id < 0) {

        phy.PHY_MAC = mac_addr;
        phy.address_write = emac_phy_address_write;
        phy.driver_description = const_cast<char *>("ETH");
        phy.link_type = PHY_LINK_ETHERNET_TYPE;
        phy.phy_MTU = 0;
        phy.phy_header_length = 0;
        phy.phy_tail_length = 0;
        phy.state_control = emac_phy_interface_state_control;
        phy.tx = emac_phy_tx;
        phy.phy_rx_cb = NULL;
        phy.phy_tx_done_cb = NULL;

        emac.set_memory_manager(memory_manager);
        emac.set_link_input_cb(mbed::callback(this, &EMACPhy::emac_phy_rx));
        emac.set_link_state_cb(enet_tasklet_link_state_changed);

        if (!emac.power_up()) {
            return -1;
        }

        phy.phy_MTU = emac.get_mtu_size();
        /* Set the address - this could be either board default, what they
         * told us with EMAC::get_mac_address, or something manually specified
         * with EMACPhy::set_mac_address
         */
        emac.set_hwaddr(mac_addr);

        emac.set_all_multicast(true);

        device_id = arm_net_phy_register(&phy);
        // driver_readiness_status_callback = driver_status_cb;

        if (device_id < 0) {
            //tr_error("Ethernet Driver failed to register with Stack. RetCode=%i", eth_driver_enabled);
            //driver_readiness_status_callback(0, eth_interface_id);
            emac.power_down();
            return -1;
        }
    }

    return device_id;
}

void EMACPhy::get_mac_address(uint8_t *mac)
{
    memcpy(mac, mac_addr, sizeof mac_addr);
}

void EMACPhy::set_mac_address(uint8_t *mac)
{
    memcpy(mac_addr, mac, sizeof mac_addr);
}

nsapi_error_t Nanostack::add_ethernet_interface(EMAC &emac, bool default_if, Nanostack::EthernetInterface **interface_out, const uint8_t *mac_addr, NetworkInterface *user_network_interface)
{
    if (single_phy) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    single_phy = new (std::nothrow) EMACPhy(this->memory_manager, emac);
    if (!single_phy) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    if (mac_addr) {
        single_phy->set_mac_address(const_cast<uint8_t *>(mac_addr));
    }

    Nanostack::EthernetInterface *interface;

    interface = new (std::nothrow) Nanostack::EthernetInterface(*single_phy);
    if (!interface) {
        return NSAPI_ERROR_NO_MEMORY;
    }

    nsapi_error_t err = interface->initialize();
    if (err) {
        delete interface;
        return err;
    }

    *interface_out = interface;

    return NSAPI_ERROR_OK;
}

nsapi_error_t Nanostack::add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, const uint8_t *mac_addr, NetworkInterface *user_network_interface)
{
    Nanostack::EthernetInterface *interface;
    nsapi_error_t err = add_ethernet_interface(emac, default_if, &interface, mac_addr);
    *interface_out = interface;
    return err;
}

nsapi_error_t Nanostack::add_ethernet_interface(EMAC &emac, bool default_if, OnboardNetworkStack::Interface **interface_out, NetworkInterface *user_network_interface)
{
    Nanostack::EthernetInterface *interface;
    nsapi_error_t err = add_ethernet_interface(emac, default_if, &interface);
    *interface_out = interface;
    return err;
}
