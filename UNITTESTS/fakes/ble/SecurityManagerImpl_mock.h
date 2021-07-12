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

#ifndef SECURITYMANAGERMOCK_H
#define SECURITYMANAGERMOCK_H

#include "gmock/gmock.h"
#include "source/generic/SecurityManagerImpl.h"

namespace ble {

class SecurityManagerMock : public ble::impl::SecurityManager {
public:
    SecurityManagerMock() {};
    SecurityManagerMock(const GattServerMock&) = delete;
    SecurityManagerMock& operator=(const GattServerMock&) = delete;
    virtual ~SecurityManagerMock() {};

    MOCK_METHOD(ble_error_t, reset, (), (override));
    MOCK_METHOD(ble_error_t, init, (bool enableBonding, bool requireMITM, SecurityIOCapabilities_t iocaps, const Passkey_t passkey, bool signing, const char *dbFilepath), (override));
    MOCK_METHOD(ble_error_t, setDatabaseFilepath, (const char *dbFilepath), (override));
    MOCK_METHOD(ble_error_t, preserveBondingStateOnReset, (bool enable), (override));
    MOCK_METHOD(ble_error_t, writeBondingStateToPersistentStorage, (), (override));
    MOCK_METHOD(ble_error_t, purgeAllBondingState, (), (override));
    MOCK_METHOD(ble_error_t, generateWhitelistFromBondTable, (::ble::whitelist_t *whitelist), (const, override));
    MOCK_METHOD(ble_error_t, requestPairing, (ble::connection_handle_t connectionHandle), (override));
    MOCK_METHOD(ble_error_t, acceptPairingRequest, (ble::connection_handle_t connectionHandle), (override));
    MOCK_METHOD(ble_error_t, cancelPairingRequest, (ble::connection_handle_t connectionHandle), (override));
    MOCK_METHOD(ble_error_t, setPairingRequestAuthorisation, (bool required), (override));
    MOCK_METHOD(ble_error_t, getPeerIdentity, (ble::connection_handle_t connectionHandle), (override));
    MOCK_METHOD(ble_error_t, allowLegacyPairing, (bool allow), (override));
    MOCK_METHOD(ble_error_t, getSecureConnectionsSupport, (bool *enabled), (override));
    MOCK_METHOD(ble_error_t, setIoCapability, (SecurityIOCapabilities_t iocaps), (override));
    MOCK_METHOD(ble_error_t, setDisplayPasskey, (const Passkey_t passkey), (override));
    MOCK_METHOD(ble_error_t, setLinkSecurity, (ble::connection_handle_t connectionHandle, SecurityMode_t securityMode), (override));
    MOCK_METHOD(ble_error_t, setKeypressNotification, (bool enabled), (override));
    MOCK_METHOD(ble_error_t, enableSigning, (ble::connection_handle_t connectionHandle, bool enabled), (override));
    MOCK_METHOD(ble_error_t, setHintFutureRoleReversal, (bool enable), (override));
    MOCK_METHOD(ble_error_t, getLinkEncryption, (ble::connection_handle_t connectionHandle, ble::link_encryption_t *encryption), (override));
    MOCK_METHOD(ble_error_t, setLinkEncryption, (ble::connection_handle_t connectionHandle, ble::link_encryption_t encryption), (override));
    MOCK_METHOD(ble_error_t, setEncryptionKeyRequirements, (uint8_t minimumByteSize, uint8_t maximumByteSize), (override));
    MOCK_METHOD(ble_error_t, getEncryptionKeySize, (connection_handle_t connectionHandle, uint8_t *size), (override));
    MOCK_METHOD(ble_error_t, requestAuthentication, (ble::connection_handle_t connectionHandle), (override));
    MOCK_METHOD(ble_error_t, generateOOB, (const ble::address_t *address), (override));
    MOCK_METHOD(ble_error_t, setOOBDataUsage, (ble::connection_handle_t connectionHandle, bool useOOB, bool OOBProvidesMITM), (override));
    MOCK_METHOD(ble_error_t, passkeyEntered, (ble::connection_handle_t connectionHandle, Passkey_t passkey), (override));
    MOCK_METHOD(ble_error_t, legacyPairingOobReceived, (const ble::address_t *address, const ble::oob_tk_t *tk), (override));
    MOCK_METHOD(ble_error_t, confirmationEntered, (ble::connection_handle_t connectionHandle, bool confirmation), (override));
    MOCK_METHOD(ble_error_t, sendKeypressNotification, (ble::connection_handle_t connectionHandle, ble::Keypress_t keypress), (override));
    MOCK_METHOD(ble_error_t, oobReceived, (const ble::address_t *address, const ble::oob_lesc_value_t *random, const ble::oob_confirm_t *confirm), (override));
    MOCK_METHOD(ble_error_t, getSigningKey, (ble::connection_handle_t connectionHandle, bool authenticated), (override));
    MOCK_METHOD(ble_error_t, setPrivateAddressTimeout, (uint16_t timeout_in_seconds), (override));
    MOCK_METHOD(void, onShutdown, (const SecurityManagerShutdownCallback_t &callback), (override));
    MOCK_METHOD(SecurityManagerShutdownCallbackChain_t&, onShutdown, (), (override));
    MOCK_METHOD(void, setSecurityManagerEventHandler, (EventHandler *handler), (override));
};

}

#endif //SECURITYMANAGERMOCK_H
