/* mbed OS IP stack API
 * Copyright (c) 2015-2017 ARM Limited
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

#ifndef MBED_IPSTACK_H
#define MBED_IPSTACK_H

#include "nsapi.h"

#include "NetworkStack.h"
#include "EMAC.h"
#include "L3IP.h"
#include "PPP.h"

/**
 * mbed OS API for onboard IP stack abstraction
 *
 * This interface should be used by targets to initialize IP stack, create, bring up and bring down network interfaces.
 *
 * An onboard network stack has the potential ability to register interfaces
 * such as through EMAC, and has its own interface identifiers.
 */
class OnboardNetworkStack : public NetworkStack {
public:
    /** Return the default on-board network stack
     *
     * Returns the default on-board network stack, as configured by
     * JSON option nsapi.default-stack.
     */
    static OnboardNetworkStack &get_default_instance();

    /** Representation of a stack's view of an interface.
     *
     * Provides facilities required by a driver to implement the application
     * NetworkInterface API.
     */
    class Interface {
    protected:
        ~Interface() = default;

    public:
        /** Set IP address
         *
         * bringup() can only take one IP address and in dual stack case
         * another IP address can be set using this function.
         *
         * Must be called before bringup().
         *
         * @param    ip         IP address to be used for the interface as "W:X:Y:Z" or NULL
         * @param    netmask    Net mask to be used for the interface as "W:X:Y:Z" or NULL
         * @param    gw         Gateway address to be used for the interface as "W:X:Y:Z" or NULL
         * @param    ipv6_flag  Provide this flag for IPv6 state flag override. For example, you can set IP6_ADDR_PREFERRED.
         *                      For IPv4, this value will be ignored.
         * @return              NSAPI_ERROR_OK on success, or error code
         */
        virtual nsapi_error_t set_ip_address(const char *ip,
                                             const char *netmask,
                                             const char *gw,
                                             uint8_t ipv6_flag)
        {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        /** Connect the interface to the network
         *
         * Sets up a connection on specified network interface, using DHCP or provided network details. If the @a dhcp is set to
         * true all the remaining parameters are ignored.
         *
         * @param    dhcp       true if the network details should be acquired using DHCP
         * @param    ip         IP address to be used for the interface as "W:X:Y:Z" or NULL
         * @param    netmask    Net mask to be used for the interface as "W:X:Y:Z" or NULL
         * @param    gw         Gateway address to be used for the interface as "W:X:Y:Z" or NULL
         * @param    stack      Allow manual selection of IPv4 and/or IPv6.
         * @param    blocking   Specify whether bringup blocks for connection completion.
         * @return              NSAPI_ERROR_OK on success, or error code
         */
        virtual nsapi_error_t bringup(bool dhcp, const char *ip,
                                      const char *netmask, const char *gw,
                                      nsapi_ip_stack_t stack = DEFAULT_STACK,
                                      bool blocking = true) = 0;

        /** Disconnect interface from the network
         *
         * After this call the network interface is inactive, to use it again user needs to call @n bringup again.
         *
         * @return    NSAPI_ERROR_OK on success, or error code
         */
        virtual nsapi_error_t bringdown() = 0;

        /** Register callback for status reporting
         *
         *  The specified status callback function will be called on status changes
         *  on the network. The parameters on the callback are the event type and
         *  event-type dependent reason parameter.
         *
         *  @param status_cb The callback for status changes
         */
        virtual void attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb) = 0;

        /** Get the connection status
         *
         *  @return         The connection status according to ConnectionStatusType
         */

        virtual nsapi_connection_status_t get_connection_status() const = 0;

        /** Returns interface name
        *
        * @return  string containing name of network interface for example "en0"
        */

        virtual char *get_interface_name(char *buf)
        {
            return NULL;
        };
        /** Return MAC address of the network interface
         *
         * @return              MAC address as "V:W:X:Y:Z"
         */

        virtual char *get_mac_address(char *buf, nsapi_size_t buflen) = 0;

        /** @copydoc NetworkStack::get_ip_address */
        virtual nsapi_error_t get_ip_address(SocketAddress *address) = 0;

        /** @copydoc NetworkStack::get_ipv6_link_local_address */
        virtual nsapi_error_t get_ipv6_link_local_address(SocketAddress *address)
        {
            return NSAPI_ERROR_UNSUPPORTED;
        }

        /** @copydoc NetworkStack::get_netmask */
        virtual nsapi_error_t get_netmask(SocketAddress *address) = 0;

        /** @copydoc NetworkStack::get_gateway */
        virtual nsapi_error_t get_gateway(SocketAddress *address) = 0;
    };

    /** Register a network interface with the IP stack
     *
     * Connects EMAC layer with the IP stack and initializes all the required infrastructure.
     * This function should be called only once for each available interface. EMAC memory
     * manager is available to EMAC after this function call.
     *
     * @param      emac             EMAC HAL implementation for this network interface
     * @param      default_if       true if the interface should be treated as the default one
     * @param[out] interface_out    pointer to stack interface object controlling the EMAC
     * @return                      NSAPI_ERROR_OK on success, or error code
     */
    virtual nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, Interface **interface_out, NetworkInterface *user_network_interface = NULL) = 0;

    virtual nsapi_error_t add_ethernet_interface(EMAC &emac, bool default_if, Interface **interface_out, const uint8_t *mac_addr, NetworkInterface *user_network_interface = NULL)

    {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    virtual nsapi_error_t add_l3ip_interface(L3IP &l3ip, bool default_if, Interface **interface_out)
    {
        return NSAPI_ERROR_OK;
    };

    virtual nsapi_error_t add_ppp_interface(PPP &ppp, bool default_if, Interface **interface_out)
    {
        return NSAPI_ERROR_UNSUPPORTED;
    };

    virtual nsapi_error_t remove_ethernet_interface(Interface **interface_out)
    {
        return NSAPI_ERROR_OK;
    };

    virtual nsapi_error_t remove_l3ip_interface(Interface **interface_out)
    {
        return NSAPI_ERROR_OK;
    };

    virtual nsapi_error_t remove_ppp_interface(Interface **interface_out)
    {
        return NSAPI_ERROR_UNSUPPORTED;
    };

    virtual void set_default_interface(OnboardNetworkStack::Interface *interface)
    {
    }

    OnboardNetworkStack *onboardNetworkStack() final
    {
        return this;
    }
};

#endif /* MBED_IPSTACK_H */
