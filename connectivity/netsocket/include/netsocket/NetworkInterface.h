/*
 * Copyright (c) 2015 ARM Limited
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

/** @file NetworkInterface.h Network Interface base class */
/** @addtogroup netinterface
 * Network Interface classes
 * @{ */


#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include "netsocket/nsapi_types.h"
#include "netsocket/SocketAddress.h"
#include "Callback.h"
#include "DNS.h"


// Predeclared classes
class NetworkStack;
class EthInterface;
class WiFiInterface;
class MeshInterface;
class CellularInterface;
class EMACInterface;
class PPPInterface;

/** Common interface that is shared between network devices.
 *
 */
class NetworkInterface: public DNS {
public:

    virtual ~NetworkInterface();

    /** Return the default network interface.
     *
     * Returns the default network interface, as determined by JSON option
     * target.network-default-interface-type or other overrides.
     *
     * The type of the interface returned can be tested by calling ethInterface(),
     * wifiInterface(), meshInterface(), cellularInterface(), emacInterface() and checking
     * for NULL pointers.
     *
     * The default behavior is to return the default interface for the
     * interface type specified by target.network-default-interface-type. Targets
     * should set this in their targets.json to guide default selection,
     * and applications may override.
     *
     * The interface returned should be already configured for use such that its
     * connect() method works with no parameters. For connection types needing
     * configuration, settings should normally be obtained from JSON - the
     * settings for the core types are under the "nsapi" JSON config tree.
     *
     * The list of possible settings for default interface type is open-ended,
     * as is the number of possible providers. Core providers are:
     *
     * * ETHERNET: EthernetInterface, using default EMAC and OnboardNetworkStack
     * * MESH: ThreadInterface or LoWPANNDInterface, using default NanostackRfPhy
     * * CELLULAR: OnboardModemInterface
     * * WIFI: None - always provided by a specific class
     *
     * Specific drivers may be activated by other settings of the
     * default-network-interface-type configuration.  This will depend on the
     * target and the driver. For example a board may have its default setting
     * as "AUTO" which causes it to autodetect an Ethernet cable. This should
     * be described in the target's documentation.
     *
     * An application can override all target settings by implementing
     * NetworkInterface::get_default_instance() themselves - the default
     * definition is weak, and calls get_target_default_instance().
     */
    static NetworkInterface *get_default_instance();

    /** Set network interface as default one.
     */
    virtual void set_as_default();

    /** Get the local MAC address.
     *
     *  Provided MAC address is intended for info or debug purposes and
     *  may be not provided if the underlying network interface does not
     *  provide a MAC address.
     *
     *  @return         Null-terminated representation of the local MAC address
     *                  or null if no MAC address is available.
     */
    virtual const char *get_mac_address();

    /** Set the MAC address to the interface.
     *
     *  Set the provided MAC address on the network interface. The address must
     *  be unique globally. The address must be set before calling the interface
     *  connect() method.
     *
     *  Not all interfaces are supporting MAC address set and an error is not returned
     *  for this method call. Verify the changed MAC address by checking packet
     *  captures from the used network interface.
     *
     *  6-byte EUI-48 MAC addresses are used for Ethernet while Mesh interface is
     *  using 8-byte EUI-64 address.
     *
     *  More information about obtaining MAC address can be found from:
     *  https://standards.ieee.org/products-services/regauth/index.html
     *
     *  @param          mac_addr Buffer containing the MAC address in hexadecimal format.
     *  @param          addr_len Length of provided buffer in bytes (6 or 8)
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if address is not valid
     *  @retval         NSAPI_ERROR_BUSY if address can't be set.
     */
    virtual nsapi_error_t set_mac_address(uint8_t *mac_addr, nsapi_size_t addr_len);

    /** Get the local IP address
     *
     *  @param          address SocketAddress representation of the local IP address
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     *  @retval         NSAPI_ERROR_NO_ADDRESS if the address cannot be obtained from stack
     */
    virtual nsapi_error_t get_ip_address(SocketAddress *address);

    /** Get the IPv6 link local address
     *
     *  @param          address SocketAddress representation of the link local IPv6 address
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     */
    virtual nsapi_error_t get_ipv6_link_local_address(SocketAddress *address);

    /** Get the local network mask.
     *
     *  @param          address SocketAddress representation of netmask
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     *  @retval         NSAPI_ERROR_NO_ADDRESS if the address cannot be obtained from stack
     */
    virtual nsapi_error_t get_netmask(SocketAddress *address);

    /** Get the local gateway.
     *
     *  @param          address SocketAddress representation of gateway address
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         NSAPI_ERROR_UNSUPPORTED if this feature is not supported
     *  @retval         NSAPI_ERROR_PARAMETER if the provided pointer is invalid
     *  @retval         NSAPI_ERROR_NO_ADDRESS if the address cannot be obtained from stack
     */
    virtual nsapi_error_t get_gateway(SocketAddress *address);

    /** Get the network interface name
     *
     *  @return         Null-terminated representation of the network interface name
     *                  or null if  interface not exists
     */
    virtual char *get_interface_name(char *interface_name);

    /** Configure this network interface to use a static IP address.
     *  Implicitly disables DHCP, which can be enabled in set_dhcp.
     *  Requires that the network is disconnected.
     *
     *  @param ip_address SocketAddress object containing the local IP address
     *  @param netmask    SocketAddress object containing the local network mask
     *  @param gateway    SocketAddress object containing the local gateway
     *  @retval           NSAPI_ERROR_OK on success
     *  @retval           NSAPI_ERROR_UNSUPPORTED if this function is unsupported
     */
    virtual nsapi_error_t set_network(const SocketAddress &ip_address, const SocketAddress &netmask, const SocketAddress &gateway);

    /** Enable or disable DHCP on connecting the network.
     *
     *  Enabled by default unless a static IP address has been assigned. Requires
     *  that the network is disconnected.
     *
     *  @param dhcp     True to enable DHCP.
     *  @retval         NSAPI_ERROR_OK on success.
     *  @retval         NSAPI_ERROR_UNSUPPORTED if operation is not supported.
     */
    virtual nsapi_error_t set_dhcp(bool dhcp);

    /** Connect to a network.
     *
     * This blocks until connection is established, but asynchronous operation can be enabled
     * by calling NetworkInterface::set_blocking(false).
     *
     * In asynchronous mode this starts the connection sequence and returns immediately.
     * Status of the connection can then checked from NetworkInterface::get_connection_status()
     * or from status callbacks.
     *
     * NetworkInterface internally handles reconnections until disconnect() is called.
     *
     * @return NSAPI_ERROR_OK if connection established in blocking mode.
     * @return NSAPI_ERROR_OK if asynchronous operation started.
     * @return NSAPI_ERROR_BUSY if asynchronous operation cannot be started.
                                Implementation guarantees event generation, which can be used as an
                                trigger to reissue the rejected request.
     * @return NSAPI_ERROR_IS_CONNECTED if already connected.
     * @return negative error code on failure.
     */
    virtual nsapi_error_t connect() = 0;

    /** Disconnect from the network
     *
     *  This blocks until interface is disconnected, unless interface is set to
     *  asynchronous (non-blocking) mode by calling NetworkInterface::set_blocking(false).
     *
     * @return NSAPI_ERROR_OK on successfully disconnected in blocking mode.
     * @return NSAPI_ERROR_OK if asynchronous operation started.
     * @return NSAPI_ERROR_BUSY if asynchronous operation cannot be started.
                                Implementation guarantees event generation, which can be used as an
                                trigger to reissue the rejected request.
     * @return NSAPI_ERROR_NO_CONNECTION if already disconnected.
     * @return negative error code on failure.
     */
    virtual nsapi_error_t disconnect() = 0;

    /** Translate a hostname to an IP address with specific version using network interface name.
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param host     Hostname to resolve.
     *  @param address  Pointer to a SocketAddress to store the result.
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC).
     *  @param interface_name  Network interface name
     *  @retval         NSAPI_ERROR_OK on success
     *  @retval         int Negative error code on failure.
     *                  See @ref NetworkStack::gethostbyname
     */
    virtual nsapi_error_t gethostbyname(const char *host,
                                        SocketAddress *address, nsapi_version_t version = NSAPI_UNSPEC, const char *interface_name = NULL);

    /** Translate a hostname to the multiple IP addresses with specific version using network interface name.
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param hostname     Hostname to resolve.
     *  @param hints  Pointer to a SocketAddress with  query parameters.
     *  @param res    Pointer to a SocketAddress array  to store the result..
     *  @param interface_name  Network interface name
     *  @return         number of results on success, negative error code on failure.
     */
    virtual nsapi_value_or_error_t getaddrinfo(const char *hostname, SocketAddress *hints, SocketAddress **res, const char *interface_name = NULL);

    /** Hostname translation callback (for use with gethostbyname_async()).
     *
     *  Callback will be called after DNS resolution completes or a failure occurs.
     *
     *  @note Callback should not take more than 10ms to execute, otherwise it might
     *  prevent underlying thread processing. A portable user of the callback
     *  should not make calls to network operations due to stack size limitations.
     *  The callback should not perform expensive operations such as socket recv/send
     *  calls or blocking operations.
     *
     *  @param result  Negative error code on failure or
     *                 value that represents the number of DNS records
     *  @param address On success, destination for the host SocketAddress.
     */
    typedef mbed::Callback<void (nsapi_value_or_error_t result, SocketAddress *address)> hostbyname_cb_t;

    /** Translate a hostname to an IP address (asynchronous) using network interface name.
     *
     *  The hostname may be either a domain name or a dotted IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  Call is non-blocking. Result of the DNS operation is returned by the callback.
     *  If this function returns failure, callback will not be called. In case result
     *  is success (IP address was found from DNS cache), callback will be called
     *  before function returns.
     *
     *  @param host     Hostname to resolve.
     *  @param callback Callback that is called for result.
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC).
     *  @param interface_name  Network interface name
     *  @return         0 on immediate success,
     *                  negative error code on immediate failure or
     *                  a positive unique id that represents the hostname translation operation
     *                  and can be passed to cancel.
     */
    virtual nsapi_value_or_error_t gethostbyname_async(const char *host, hostbyname_cb_t callback, nsapi_version_t version = NSAPI_UNSPEC,
                                                       const char *interface_name = NULL);

    /** Translate a hostname to the multiple IP addresses (asynchronous) using network interface name.
     *
     *  The hostname may be either a domain name or a dotted IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  Call is non-blocking. Result of the DNS operation is returned by the callback.
     *  If this function returns failure, callback will not be called. In case result
     *  is success (IP address was found from DNS cache), callback will be called
     *  before function returns.
     *
     *  @param hostname     Hostname to resolve.
     *  @param hints  Pointer to a SocketAddress with  query parameters.
     *  @param callback Callback that is called for result.
     *  @param interface_name  Network interface name
     *  @return         0 on immediate success,
     *                  negative error code on immediate failure or
     *                  a positive unique id that represents the hostname translation operation
     *                  and can be passed to cancel.
     */
    virtual nsapi_value_or_error_t getaddrinfo_async(const char *hostname, SocketAddress *hints, hostbyname_cb_t callback, const char *interface_name = NULL);

    /** Cancel asynchronous hostname translation.
     *
     *  When translation is cancelled, callback will not be called.
     *
     *  @param id       Unique id of the hostname translation operation (returned
     *                  by gethostbyname_async)
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure.
     */
    virtual nsapi_error_t gethostbyname_async_cancel(int id);

    /** Add a domain name server to list of servers to query
     *
     *  @param address  Address for the dns host.
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure.
     */
    virtual nsapi_error_t add_dns_server(const SocketAddress &address, const char *interface_name);

    /** Get a domain name server from a list of servers to query
     *
     *  Returns a DNS server address for a index. If returns error no more
     *  DNS servers to read.
     *
     *  @param index    Index of the DNS server, starts from zero
     *  @param address  Destination for the host address
     *  @param interface_name  Network interface name
     *  @return         NSAPI_ERROR_OK on success, negative error code on failure
     */
    virtual nsapi_error_t get_dns_server(int index, SocketAddress *address, const char *interface_name = NULL);

    /** Register callback for status reporting.
     *
     *  The specified status callback function will be called on status changes
     *  on the network. The parameters on the callback are the event type and
     *  event-type dependent reason parameter. Only one callback can be registered at a time.
     *
     *  To unregister a callback call with status_cb parameter as a zero.
     *
     *  *NOTE:* Any callbacks registered with this function will be overwritten if
     *          add_event_listener() API is used.
     *
     *  @param status_cb The callback for status changes.
     */
    virtual void attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb);

#if MBED_CONF_NSAPI_ADD_EVENT_LISTENER_RETURN_CHANGE
    /** Add event listener for interface.
     *
     * This API allows multiple callback to be registered for a single interface.
     * of both leads to undefined behavior.
     *
     *  @param status_cb The callback for status changes.
     *  @return NSAPI_ERROR_OK on success
     *  @return NSAPI_ERROR_NO_MEMORY if the function fails to create a new entry.
     */
    nsapi_error_t add_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb);
#else
    /** Add event listener for interface.
     *
     * This API allows multiple callback to be registered for a single interface.
     * When first called, internal list of event handlers are created and registered to
     * interface through attach() API.
     *
     * Application may only use attach() or add_event_listener() interface. Mixing usage
     * of both leads to undefined behavior.
     *
     * @warning This version of the function does not use the `std::nothrow` feature. Subsequently,
     * the function may fail to allocate memory and cause a system error. To use the new
     * version with the changes, set "nsapi.add-event-listener-return-change": 1 in the
     * target overrides section in your mbed_app.json file.
     *
     *  @param status_cb The callback for status changes.
     */
    MBED_DEPRECATED_SINCE("mbed-os-6.12", "This function return value will change to nsapi_error_t in the next major release. See documentation for details.")
    void add_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb);
#endif

#if MBED_CONF_PLATFORM_CALLBACK_COMPARABLE
    /** Remove event listener from interface.
     *
     * Remove previously added callback from the handler list.
     *
     *  @param status_cb The callback to unregister.
     */
    void remove_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb);
#endif

    /** Get the connection status.
     *
     *  @return The connection status (@see nsapi_types.h).
     */
    virtual nsapi_connection_status_t get_connection_status() const;

    /** Set asynchronous operation of connect() and disconnect() calls.
     *
     * By default, interfaces are in synchronous mode which means that
     * connect() or disconnect() blocks until it reach the target state or requested operation fails.
     *
     * @param blocking Use false to set NetworkInterface in asynchronous mode.
     * @return NSAPI_ERROR_OK on success
     * @return NSAPI_ERROR_UNSUPPORTED if driver does not support asynchronous mode.
     * @return negative error code on failure.
     */
    virtual nsapi_error_t set_blocking(bool blocking);

    /** Return pointer to an EthInterface.
     * @return Pointer to requested interface type or NULL if this class doesn't implement the interface.
     */
    virtual EthInterface *ethInterface()
    {
        return nullptr;
    }

    /** Return pointer to a WiFiInterface.
     * @return Pointer to requested interface type or NULL if this class doesn't implement the interface.
     */
    virtual WiFiInterface *wifiInterface()
    {
        return nullptr;
    }

    /** Return pointer to a MeshInterface.
     * @return Pointer to requested interface type or NULL if this class doesn't implement the interface.
     */
    virtual MeshInterface *meshInterface()
    {
        return nullptr;
    }

    /** Return pointer to an EMACInterface.
     * @return Pointer to requested interface type or NULL if this class doesn't implement the interface.
     */
    virtual EMACInterface *emacInterface()
    {
        return nullptr;
    }

    /** Return pointer to a CellularInterface.
     * @return Pointer to requested interface type or NULL if this class doesn't implement the interface.
     */
    virtual CellularInterface *cellularInterface()
    {
        return nullptr;
    }

#if !defined(DOXYGEN_ONLY)

protected:
    friend NetworkStack *_nsapi_create_stack(NetworkInterface *iface, std::false_type);

    /** Provide access to the NetworkStack object
     *
     *  @return The underlying NetworkStack object
     */
    virtual NetworkStack *get_stack() = 0;

    /** Get the target's default network instance.
     *
     * This method can be overridden by the target. Default implementations
     * are provided weakly by various subsystems as described in
     * NetworkInterface::get_default_instance(), so targets should not
     * need to override in simple cases.
     *
     * If a target has more elaborate interface selection, it can completely
     * override this behavior by implementing
     * NetworkInterface::get_target_default_instance() themselves, either
     * unconditionally, or for a specific network-default-interface-type setting
     *
     * For example, a device with both Ethernet and Wi-fi could be set up its
     * target so that:
     *    * DEVICE_EMAC is set, and it provides EMAC::get_default_instance(),
     *      which means EthernetInterface provides EthInterface::get_target_instance()
     *      based on that EMAC.
     *    * It provides WifiInterface::get_target_default_instance().
     *    * The core will route NetworkInterface::get_default_instance() to
     *      either of those if network-default-interface-type is set to
     *      ETHERNET or WIFI.
     *    * The board overrides NetworkInterface::get_target_default_instance()
     *      if network-default-interface-type is set to AUTO. This returns
     *      either EthInterface::get_default_instance() or WiFIInterface::get_default_instance()
     *      depending on a cable detection.
     *
     *
     * performs the search described by get_default_instance.
     */
    static NetworkInterface *get_target_default_instance();
#endif //!defined(DOXYGEN_ONLY)

public:
    /** Set default parameters on an interface.
     *
     * A network interface instantiated directly or using calls such as
     * WiFiInterface::get_default_instance() is initially unconfigured.
     * This call can be used to set the default parameters that would
     * have been set if the interface had been requested using
     * NetworkInterface::get_default_instance() (see nsapi JSON
     * configuration).
     */
    virtual void set_default_parameters();

private:
    // Unified implementation for different versions of add_event_listener.
    nsapi_error_t internal_add_event_listener(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb);
};

#endif

/** @}*/
