/* mbed Microcontroller Library
 * Copyright (c) 2020 ARM Limited
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

#ifndef SECURITYMANAGERSTUB_H
#define SECURITYMANAGERSTUB_H

namespace ble {
namespace impl {

class SecurityManager {
public:
    SecurityManager() {};
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;
    virtual ~SecurityManager() {};

    using SecurityIOCapabilities_t = ble::SecurityManager::SecurityIOCapabilities_t;
    using SecurityMode_t = ble::SecurityManager::SecurityMode_t;
    using SecurityManagerShutdownCallback_t = ble::SecurityManager::SecurityManagerShutdownCallback_t;
    using SecurityManagerShutdownCallbackChain_t = ble::SecurityManager::SecurityManagerShutdownCallbackChain_t;
    using EventHandler = ble::SecurityManager::EventHandler;
    using Passkey_t  = ble::SecurityManager::Passkey_t ;

    static auto constexpr IO_CAPS_NONE = ble::SecurityManager::IO_CAPS_NONE;

    virtual ble_error_t init(
        bool enableBonding = true,
        bool requireMITM = true,
        SecurityIOCapabilities_t iocaps = IO_CAPS_NONE,
        const Passkey_t passkey = nullptr,
        bool signing = true,
        const char *dbFilepath = nullptr
    ) { return BLE_ERROR_NONE; };

    virtual ble_error_t setDatabaseFilepath(const char *dbFilepath = nullptr) { return BLE_ERROR_NONE; };

    virtual ble_error_t reset() { return BLE_ERROR_NONE; };

    virtual ble_error_t preserveBondingStateOnReset(bool enable) { return BLE_ERROR_NONE; };

    virtual ble_error_t writeBondingStateToPersistentStorage() { return BLE_ERROR_NONE; };

    ////////////////////////////////////////////////////////////////////////////
    // List management
    //

    virtual ble_error_t purgeAllBondingState() { return BLE_ERROR_NONE; };

    virtual ble_error_t generateWhitelistFromBondTable(::ble::whitelist_t *whitelist) const { return BLE_ERROR_NONE; };

    ////////////////////////////////////////////////////////////////////////////
    // Pairing
    //

#if BLE_ROLE_CENTRAL
    virtual ble_error_t requestPairing(ble::connection_handle_t connectionHandle) { return BLE_ERROR_NONE; };
#endif // BLE_ROLE_CENTRAL

#if BLE_ROLE_PERIPHERAL
    virtual ble_error_t acceptPairingRequest(ble::connection_handle_t connectionHandle) { return BLE_ERROR_NONE; };
#endif // BLE_ROLE_PERIPHERAL

    virtual ble_error_t cancelPairingRequest(ble::connection_handle_t connectionHandle) { return BLE_ERROR_NONE; };

    virtual ble_error_t setPairingRequestAuthorisation(bool required = true) { return BLE_ERROR_NONE; };

    virtual ble_error_t getPeerIdentity(ble::connection_handle_t connectionHandle) { return BLE_ERROR_NONE; };

    ////////////////////////////////////////////////////////////////////////////
    // Feature support
    //
#if BLE_FEATURE_SECURE_CONNECTIONS
    virtual ble_error_t allowLegacyPairing(bool allow = true) { return BLE_ERROR_NONE; };

    virtual ble_error_t getSecureConnectionsSupport(bool *enabled) { return BLE_ERROR_NONE; };
#endif // BLE_FEATURE_SECURE_CONNECTIONS

    ////////////////////////////////////////////////////////////////////////////
    // Security settings
    //

    virtual ble_error_t setIoCapability(SecurityIOCapabilities_t iocaps) { return BLE_ERROR_NONE; };

    virtual ble_error_t setDisplayPasskey(const Passkey_t passkey) { return BLE_ERROR_NONE; };

    virtual ble_error_t setLinkSecurity(ble::connection_handle_t connectionHandle, SecurityMode_t securityMode) { return BLE_ERROR_NONE; };

    virtual ble_error_t setKeypressNotification(bool enabled = true) { return BLE_ERROR_NONE; };

#if BLE_FEATURE_SIGNING

    virtual ble_error_t enableSigning(ble::connection_handle_t connectionHandle, bool enabled = true) { return BLE_ERROR_NONE; };

#endif // BLE_FEATURE_SIGNING

    virtual ble_error_t setHintFutureRoleReversal(bool enable = true) { return BLE_ERROR_NONE; };

    ////////////////////////////////////////////////////////////////////////////
    // Encryption
    //

    virtual ble_error_t getLinkEncryption(ble::connection_handle_t connectionHandle, ble::link_encryption_t *encryption) { return BLE_ERROR_NONE; };

    virtual ble_error_t setLinkEncryption(ble::connection_handle_t connectionHandle, ble::link_encryption_t encryption) { return BLE_ERROR_NONE; };

    virtual ble_error_t setEncryptionKeyRequirements(uint8_t minimumByteSize, uint8_t maximumByteSize) { return BLE_ERROR_NONE; };

    virtual ble_error_t getEncryptionKeySize(
        connection_handle_t connectionHandle,
        uint8_t *size
    ) { return BLE_ERROR_NONE; };

    ////////////////////////////////////////////////////////////////////////////
    // Authentication
    //

    virtual ble_error_t requestAuthentication(ble::connection_handle_t connectionHandle) { return BLE_ERROR_NONE; };

    ////////////////////////////////////////////////////////////////////////////
    // MITM
    //

    virtual ble_error_t generateOOB(const ble::address_t *address) { return BLE_ERROR_NONE; };

    virtual ble_error_t setOOBDataUsage(ble::connection_handle_t connectionHandle, bool useOOB, bool OOBProvidesMITM = true) { return BLE_ERROR_NONE; };

    virtual ble_error_t passkeyEntered(ble::connection_handle_t connectionHandle, Passkey_t passkey) { return BLE_ERROR_NONE; };

    virtual ble_error_t legacyPairingOobReceived(const ble::address_t *address, const ble::oob_tk_t *tk) { return BLE_ERROR_NONE; };
#if BLE_FEATURE_SECURE_CONNECTIONS
    virtual ble_error_t confirmationEntered(ble::connection_handle_t connectionHandle, bool confirmation) { return BLE_ERROR_NONE; };

    virtual ble_error_t sendKeypressNotification(ble::connection_handle_t connectionHandle, ble::Keypress_t keypress) { return BLE_ERROR_NONE; };

    virtual ble_error_t oobReceived(
        const ble::address_t *address,
        const ble::oob_lesc_value_t *random,
        const ble::oob_confirm_t *confirm
    ) { return BLE_ERROR_NONE; };
#endif // BLE_FEATURE_SECURE_CONNECTIONS

    ////////////////////////////////////////////////////////////////////////////
    // Keys
    //
#if BLE_FEATURE_SIGNING
    virtual ble_error_t getSigningKey(ble::connection_handle_t connectionHandle, bool authenticated) { return BLE_ERROR_NONE; };
#endif // BLE_FEATURE_SIGNING
    ////////////////////////////////////////////////////////////////////////////
    // Privacy
    //

#if BLE_FEATURE_PRIVACY
    virtual ble_error_t setPrivateAddressTimeout(
        uint16_t timeout_in_seconds
    ) { return BLE_ERROR_NONE; };
#endif // BLE_FEATURE_PRIVACY

    /* Event callback handlers. */
public:

    virtual void onShutdown(const SecurityManagerShutdownCallback_t &callback) { };

    template<typename T>
    void onShutdown(T *objPtr, void (T::*memberPtr)(const SecurityManager *)) { };

    virtual SecurityManagerShutdownCallbackChain_t &onShutdown() { static SecurityManagerShutdownCallbackChain_t chain; return chain; };

    virtual void setSecurityManagerEventHandler(EventHandler *handler) { };
};

}
}

#endif //SECURITYMANAGERSTUB_H
