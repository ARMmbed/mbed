/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
 *
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

#if BLE_FEATURE_SECURITY

#include "ble/BLE.h"
#include "ble/common/BLERoles.h"

#include "source/pal/PalSecurityManager.h"

#include "source/generic/SecurityManagerImpl.h"
#include "source/generic/MemorySecurityDb.h"
#include "source/generic/FileSecurityDb.h"
#include "source/generic/KVStoreSecurityDb.h"

#include "mbed-trace/mbed_trace.h"
#include "common/ble_trace_helpers.h"

#define TRACE_GROUP "BLSM"


using ble::advertising_peer_address_type_t;
using ble::AuthenticationMask;
using ble::KeyDistribution;
using ble::connection_peer_address_type_t;

namespace ble {
namespace impl {

namespace {
constexpr auto SECURITY_MODE_ENCRYPTION_OPEN_LINK =
    ble::SecurityManager::SECURITY_MODE_ENCRYPTION_OPEN_LINK;

constexpr auto SECURITY_MODE_ENCRYPTION_NO_MITM =
    ble::SecurityManager::SECURITY_MODE_ENCRYPTION_NO_MITM;

constexpr auto SECURITY_MODE_ENCRYPTION_WITH_MITM =
    ble::SecurityManager::SECURITY_MODE_ENCRYPTION_WITH_MITM;

constexpr auto SECURITY_MODE_SIGNED_NO_MITM =
    ble::SecurityManager::SECURITY_MODE_SIGNED_NO_MITM;

constexpr auto SECURITY_MODE_SIGNED_WITH_MITM =
    ble::SecurityManager::SECURITY_MODE_SIGNED_WITH_MITM;

using SecurityCompletionStatus_t = ble::SecurityManager::SecurityCompletionStatus_t;

constexpr auto SEC_STATUS_TIMEOUT =
    ble::SecurityManager::SEC_STATUS_TIMEOUT;

constexpr auto SEC_STATUS_SUCCESS =
    ble::SecurityManager::SEC_STATUS_SUCCESS;

}

/* Implements PalSecurityManager */

////////////////////////////////////////////////////////////////////////////
// SM lifecycle management
//

ble_error_t SecurityManager::init(
    bool bondable,
    bool mitm,
    SecurityIOCapabilities_t iocaps,
    const uint8_t* passkey,
    bool signing,
    const char* db_path
)
{
    tr_info("SM init: bondable=%s, mitm=%s, iocaps=%s, passkey=%s, signing=%s, db_path=%p",
        to_string(bondable),
        to_string(mitm),
        to_string(iocaps),
        passkey_str(passkey),
        to_string(signing),
        db_path
    );
#if !(BLE_FEATURE_SIGNING)
    if (signing) {
        tr_error("Failed to init SM with signing option if signing support disabled");
        return BLE_ERROR_INVALID_PARAM;
    }
#endif // !(BLE_FEATURE_SIGNING)
    ble_error_t result = _pal.initialize();

    if (result != BLE_ERROR_NONE) {
        return result;
    }

    result = init_database(db_path);

    if (result != BLE_ERROR_NONE) {
        return result;
    }

    _pal.set_io_capability((io_capability_t::type) iocaps);

    if (passkey) {
        _pal.set_display_passkey(PasskeyAscii::to_num(passkey));
    } else {
        _pal.set_display_passkey(0);
    }

    _legacy_pairing_allowed = true;

    bool secure_connections;
    _pal.get_secure_connections_support(secure_connections);
    tr_debug("Secure connection support: %s", to_string(secure_connections));

    _default_authentication.set_bondable(bondable);
    _default_authentication.set_mitm(mitm);
    _default_authentication.set_secure_connections(secure_connections);
    _default_authentication.set_keypress_notification(true);

    // FIXME: depends on BR/EDR support
    _default_key_distribution.set_link(false);
#if BLE_FEATURE_SIGNING
    _default_key_distribution.set_signing(signing);
    if (signing) {
        init_signing();
    }
#else
    _default_key_distribution.set_signing(false);
#endif // BLE_FEATURE_SIGNING
#if BLE_FEATURE_CONNECTABLE
    _connection_monitor.set_connection_event_handler(this);
#endif
#if BLE_FEATURE_SIGNING
    _signing_monitor.set_signing_event_handler(this);
#endif
    _pal.set_event_handler(this);

#if BLE_FEATURE_PRIVACY
    result = init_resolving_list();
#endif

#if BLE_FEATURE_PRIVACY
    // set the local identity address and irk
    if (result == BLE_ERROR_NONE) {
    	result = init_identity();
    }
#endif // BLE_FEATURE_PRIVACY

    if (result != BLE_ERROR_NONE) {
        delete _db;
        _db = nullptr;
    }

    return result;
}


ble_error_t SecurityManager::setDatabaseFilepath(
    const char *db_path
)
{
    tr_info("Set DB path: %s", db_path);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    /* operation only allowed with no connections active */
    for (auto & _control_block : _control_blocks) {
        if (_control_block.connected) {
            return BLE_ERROR_OPERATION_NOT_PERMITTED;
        }
    }

    ble_error_t result = init_database(db_path);
    if (result != BLE_ERROR_NONE) {
        return result;
    }

#if BLE_FEATURE_PRIVACY
    init_resolving_list();
#endif

    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::reset()
{
    delete _db;
    _db = nullptr;
    _pal.reset();

    /* Notify that the instance is about to shutdown */
    shutdownCallChain.call(&ble::BLE::Instance().securityManager());
    shutdownCallChain.clear();
    eventHandler = &defaultEventHandler;

    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::preserveBondingStateOnReset(bool enabled)
{
    tr_info("Preserve bonding state: %s", to_string(enabled));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    _db->set_restore(enabled);
    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::writeBondingStateToPersistentStorage()
{
    tr_info("Writing bonding to storage");
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    _db->sync();
    return BLE_ERROR_NONE;
}

////////////////////////////////////////////////////////////////////////////
// List management
//


ble_error_t SecurityManager::purgeAllBondingState(void)
{
    tr_info("Purging all bonds");
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    _db->clear_entries();

    ble_error_t ret = BLE_ERROR_NONE;

#if BLE_FEATURE_SIGNING
    // generate new csrk and irk
    ret = init_signing();
    if (ret) {
        return ret;
    }
#endif // BLE_FEATURE_SIGNING
#if BLE_FEATURE_PRIVACY
    ret = init_identity();
#endif // BLE_FEATURE_PRIVACY

    return ret;
}


ble_error_t SecurityManager::generateWhitelistFromBondTable(::ble::whitelist_t *whitelist) const {
    tr_info("Generate whitelist from bonds");
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    if (eventHandler) {
        if (!whitelist) {
            tr_error("Failure, whitelist not provided");
            return BLE_ERROR_INVALID_PARAM;
        }
        _db->generate_whitelist_from_bond_table(
            mbed::callback(eventHandler, &EventHandler::whitelistFromBondTable),
            whitelist
        );
        return BLE_ERROR_NONE;
    } else {
        tr_error("No event handler registered to receive whitelist generated");
        return BLE_ERROR_INVALID_STATE;

    }
}

////////////////////////////////////////////////////////////////////////////
// Pairing
//

#if BLE_ROLE_CENTRAL
ble_error_t SecurityManager::requestPairing(connection_handle_t connection)
{
    tr_info("Connection %d - Request pairing", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (!_legacy_pairing_allowed && !_default_authentication.get_secure_connections()) {
        tr_error("Failure, legacy pairing not allowed while controller doesn't support secure connection");
        return BLE_ERROR_INVALID_STATE;
    }

    set_mitm_performed(connection, false);
    update_oob_presence(connection);

    AuthenticationMask link_authentication(_default_authentication);

    if (cb->mitm_requested) {
        link_authentication.set_mitm(true);
    }

    /* by default the initiator doesn't send any keys other then identity */
    KeyDistribution initiator_distribution(
        KeyDistribution::KEY_DISTRIBUTION_IDENTITY |
        _default_key_distribution.get_link()
    );

    initiator_distribution.set_signing(
        cb->signing_override_default ?
            cb->signing_requested :
            _default_key_distribution.get_signing()
    );

    /* if requested the initiator may send all the default keys for later
     * use when roles are changed */
    if (_master_sends_keys) {
        initiator_distribution = _default_key_distribution;
    }

    KeyDistribution responder_distribution(_default_key_distribution);

    if (cb->signing_override_default) {
        responder_distribution.set_signing(cb->signing_requested);
    }

    return _pal.send_pairing_request(
        connection,
        cb->oob_present,
        link_authentication,
        initiator_distribution,
        responder_distribution
    );
}
#endif // BLE_ROLE_CENTRAL

#if BLE_ROLE_PERIPHERAL
ble_error_t SecurityManager::acceptPairingRequest(connection_handle_t connection)
{
    tr_info("Connection %d - Accept pairing request", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    update_oob_presence(connection);

    AuthenticationMask link_authentication(_default_authentication);
    if (cb->mitm_requested) {
        link_authentication.set_mitm(true);
    }

    KeyDistribution initiator_distribution = cb->get_initiator_key_distribution();

    bool master_signing = initiator_distribution.get_signing();

    if (_master_sends_keys) {
        initiator_distribution &= _default_key_distribution;
    } else {
        initiator_distribution &= KeyDistribution(
            KeyDistribution::KEY_DISTRIBUTION_IDENTITY |
            KeyDistribution::KEY_DISTRIBUTION_LINK
        );
    }

    /* signing has to be offered and enabled on the link */
    if (master_signing) {
        initiator_distribution.set_signing(
            cb->signing_override_default ?
                cb->signing_requested :
                _default_key_distribution.get_signing()
        );
    }

    KeyDistribution responder_distribution(cb->get_responder_key_distribution());

    responder_distribution &= _default_key_distribution;

    /* signing has to be requested and enabled on the link */
    if (responder_distribution.get_signing()) {
        responder_distribution.set_signing(
            cb->signing_override_default ?
                cb->signing_requested :
                _default_key_distribution.get_signing()
        );
    }

    return _pal.send_pairing_response(
        connection,
        cb->oob_present,
        link_authentication,
        initiator_distribution,
        responder_distribution
    );
}
#endif


ble_error_t SecurityManager::cancelPairingRequest(connection_handle_t connection)
{
    tr_info("Connection %d - Cancel pairing request", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.cancel_pairing(connection, pairing_failure_t::UNSPECIFIED_REASON);
}


ble_error_t SecurityManager::setPairingRequestAuthorisation(bool required)
{
    tr_info("Set manual handling of pairing request: %s", to_string(required));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    _pairing_authorisation_required = required;
    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::getPeerIdentity(connection_handle_t connection)
{
    tr_info("Connection %d - Request peer identity", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    if (!eventHandler) {
        tr_error("Event handler must be registered to request peer identity");
        return BLE_ERROR_INVALID_STATE;
    }

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Control block not found", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    _db->get_entry_identity(
        [connection,this](SecurityDb::entry_handle_t handle, const SecurityEntryIdentity_t* identity) {
            if (identity) {
                tr_info("Connection %d - Peer identity address retrieved: %s, public: %s",
                    connection,
                    to_string(identity->identity_address),
                    to_string(identity->identity_address_is_public)
                );
            } else {
                tr_info("Connection %d - Failed to retrieve peer identity address", connection);
            }
            if (eventHandler) {
                eventHandler->peerIdentity(
                    connection,
                    identity ? &identity->identity_address : nullptr,
                    identity ? identity->identity_address_is_public : false
                );
            } else {
                tr_warning("Event handler removed while retrieving peer ID address");
            }
        },
        cb->db_entry
    );
    return BLE_ERROR_NONE;
}

////////////////////////////////////////////////////////////////////////////
// Feature support
//

#if BLE_FEATURE_SECURE_CONNECTIONS
ble_error_t SecurityManager::allowLegacyPairing(bool allow)
{
    tr_info("Allow legacy pairing: %s", to_string(allow));
    _legacy_pairing_allowed = allow;
    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::getSecureConnectionsSupport(bool *enabled)
{
    return _pal.get_secure_connections_support(*enabled);
}
#endif // BLE_FEATURE_SECURE_CONNECTIONS

////////////////////////////////////////////////////////////////////////////
// Security settings
//


ble_error_t SecurityManager::setIoCapability(SecurityIOCapabilities_t iocaps)
{
    tr_info("Set I/O capabilities: %s", to_string(iocaps));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.set_io_capability((io_capability_t::type) iocaps);
}


ble_error_t SecurityManager::setDisplayPasskey(const uint8_t* passkey)
{
    tr_info("Set display passkey: %s", passkey_str(passkey));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.set_display_passkey(PasskeyAscii::to_num(passkey));
}


ble_error_t SecurityManager::setAuthenticationTimeout(
    connection_handle_t connection,
    uint32_t timeout_in_ms
)
{
    tr_info("Connection %d - Set connection authentication timeout: %" PRIu32 " ms", connection, timeout_in_ms);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.set_authentication_timeout(connection, timeout_in_ms / 10);
}


ble_error_t SecurityManager::getAuthenticationTimeout(
    connection_handle_t connection,
    uint32_t *timeout_in_ms
)
{
    if (!_db) return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    uint16_t timeout_in_10ms;
    ble_error_t status = _pal.get_authentication_timeout(connection, timeout_in_10ms);
    *timeout_in_ms = 10 * timeout_in_10ms;
    return status;
}


ble_error_t SecurityManager::setLinkSecurity(
    connection_handle_t connection,
    SecurityMode_t securityMode
)
{
    tr_info("Connection %d - Set link security: %s", connection, to_string(securityMode));

    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not found", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (cb->encryption_requested) {
        tr_error("Failure, encryption already requested");
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }

    switch (securityMode) {
        case SECURITY_MODE_ENCRYPTION_OPEN_LINK:
            return setLinkEncryption(connection, link_encryption_t::NOT_ENCRYPTED);

        case SECURITY_MODE_ENCRYPTION_NO_MITM:
            return setLinkEncryption(connection, link_encryption_t::ENCRYPTED);

        case SECURITY_MODE_ENCRYPTION_WITH_MITM:
            return setLinkEncryption(connection, link_encryption_t::ENCRYPTED_WITH_MITM);

#if BLE_FEATURE_SIGNING
        case SECURITY_MODE_SIGNED_NO_MITM:
            return getSigningKey(connection, false);

        case SECURITY_MODE_SIGNED_WITH_MITM:
            return getSigningKey(connection, true);
#endif // BLE_FEATURE_SIGNING

        default:
            tr_error("Failure, Invalid security mode");
            return BLE_ERROR_INVALID_PARAM;
    }
}


ble_error_t SecurityManager::setKeypressNotification(bool enabled)
{
    tr_info("Enable user keypress notification: %s", to_string(enabled));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    _default_authentication.set_keypress_notification(enabled);
    return BLE_ERROR_NONE;
}

#if BLE_FEATURE_SIGNING
ble_error_t SecurityManager::enableSigning(
    connection_handle_t connection,
    bool enabled
)
{
    tr_info("Connection %d - Enable signing: %s", connection, to_string(enabled));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return BLE_ERROR_INVALID_PARAM;
    }

    cb->signing_override_default = true;

    if (enabled && !cb->signing_requested && !_default_key_distribution.get_signing()) {
        cb->signing_requested = true;
        if (flags->csrk_stored) {
            /* used the stored ones when available */
            _db->get_entry_peer_csrk(
                mbed::callback(this, &SecurityManager::set_peer_csrk_cb),
                cb->db_entry
            );
        } else {
            tr_info("Connection %d - CSRK not available, pair to acquire it", connection);
            /* create keys if needed and exchange them */
            init_signing();
            if (cb->is_master) {
#if BLE_ROLE_CENTRAL
                return requestPairing(connection);
#else
                tr_error("Device configured as master while central role disabled");
                return BLE_ERROR_NOT_IMPLEMENTED;
#endif
            } else {
#if BLE_ROLE_PERIPHERAL
                return slave_security_request(connection);
#else
                tr_error("Device configured as slave while peripheral role disabled");
                return BLE_ERROR_NOT_IMPLEMENTED;
#endif
            }
        }
    } else {
        cb->signing_requested = enabled;
    }

    return BLE_ERROR_NONE;
}
#endif

ble_error_t SecurityManager::setHintFutureRoleReversal(bool enable)
{
    tr_info("Set role reversal hint: %s", to_string(enable));
    _master_sends_keys = enable;
    return BLE_ERROR_NONE;
}

////////////////////////////////////////////////////////////////////////////
// Encryption
//


ble_error_t SecurityManager::getLinkEncryption(
    connection_handle_t connection,
    link_encryption_t *encryption
)
{
    if (!_db) {
        tr_error("Failed to retrieve link encryption, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failed to retrieve link encryption: no CB available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failed to retrieve link encryption, empty distribution flags", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (cb->encrypted) {
        if (flags->ltk_mitm_protected  || cb->mitm_performed) {
            if (flags->secure_connections_paired) {
                *encryption = link_encryption_t::ENCRYPTED_WITH_SC_AND_MITM;
            } else {
                *encryption = link_encryption_t::ENCRYPTED_WITH_MITM;
            }
        } else {
            *encryption = link_encryption_t::ENCRYPTED;
        }
    } else if (cb->encryption_requested) {
        *encryption = link_encryption_t::ENCRYPTION_IN_PROGRESS;
    } else {
        *encryption = link_encryption_t::NOT_ENCRYPTED;
    }

    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::setLinkEncryption(
    connection_handle_t connection,
    link_encryption_t encryption
)
{
    tr_info("Connection %d - Set encryption: %s", connection, to_string(encryption));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return BLE_ERROR_INVALID_PARAM;
    }

    link_encryption_t current_encryption(link_encryption_t::NOT_ENCRYPTED);
    getLinkEncryption(connection, &current_encryption);

    if (current_encryption == link_encryption_t::ENCRYPTION_IN_PROGRESS) {
        tr_error("Connection %d - Failure, encryption in progress", connection);
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    }

    if (current_encryption == encryption) {
        tr_info("Connection %d - Link already encrypted to the right level", connection);
        /* ignore if the link is already at required state*/

    } else if (encryption == link_encryption_t::NOT_ENCRYPTED) {
        // Fail as it is not permitted to turn down encryption
        tr_error("Connection %d - Failure, forbidden to turn down encryption", connection);
        return BLE_ERROR_OPERATION_NOT_PERMITTED;
    } else if (encryption == link_encryption_t::ENCRYPTED) {

        /* only change if we're not already encrypted with mitm */
        if (current_encryption != link_encryption_t::ENCRYPTED_WITH_MITM ||
            current_encryption != link_encryption_t::ENCRYPTED_WITH_SC_AND_MITM
        ) {
            cb->encryption_requested = true;
            return enable_encryption(connection);
        } else {
            tr_info("Connection %d - Encryption level %s sufficient", connection, to_string(current_encryption));
        }

    } else if (encryption == link_encryption_t::ENCRYPTED_WITH_MITM) {

        if (flags->ltk_mitm_protected && !cb->encrypted) {
            cb->encryption_requested = true;
            return enable_encryption(connection);
        } else {
            cb->encryption_requested = true;
            return requestAuthentication(connection);
        }

    } else if (encryption == link_encryption_t::ENCRYPTED_WITH_SC_AND_MITM) {

        if (flags->ltk_mitm_protected &&
            flags->secure_connections_paired &&
            !cb->encrypted
        ) {
            cb->encryption_requested = true;
            return enable_encryption(connection);
        } else {
            cb->encryption_requested = true;
            return requestAuthentication(connection);
        }

    } else {
        tr_error("Connection %d - Failure invalid encryption level", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (eventHandler) {
        eventHandler->linkEncryptionResult(connection, current_encryption);
    }

    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::getEncryptionKeySize(
    connection_handle_t connectionHandle,
    uint8_t *size
)
{
    if (!_db) {
        tr_error("Connection %d - Failed to retrieve encryption key size: DB not initialized", connectionHandle);
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connectionHandle);
    if (!cb) {
        tr_error("Connection %d - Failed to retrieve encryption key size: no CB", connectionHandle);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failed to retrieve encryption key size: no flags retrieved", connectionHandle);
        return BLE_ERROR_INVALID_PARAM;
    }

    *size = flags->encryption_key_size;
    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::setEncryptionKeyRequirements(
    uint8_t minimumByteSize,
    uint8_t maximumByteSize
)
{
    tr_info("Set encryption key requirements: [%d:%d]", minimumByteSize, maximumByteSize);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.set_encryption_key_requirements(minimumByteSize, maximumByteSize);
}

////////////////////////////////////////////////////////////////////////////
// Keys
//

#if BLE_FEATURE_SIGNING
ble_error_t SecurityManager::getSigningKey(connection_handle_t connection, bool authenticated)
{
    tr_info("Connection %d - retrieve signing key: authenticated = %s", connection, to_string(authenticated));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failure, no distribution flags set", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    if (flags->csrk_stored && (flags->csrk_mitm_protected || !authenticated)) {
        /* we have a key that is either authenticated or we don't care if it is
         * so retrieve it from the DB now */
        _db->get_entry_peer_csrk(
            mbed::callback(this, &SecurityManager::return_csrk_cb),
            cb->db_entry
        );
        return BLE_ERROR_NONE;

    } else {
        /* we don't have the right key so we need to get it first
         * keys exchange will create the signingKey event */
        if (authenticated) {
            tr_info("Connection %d - Signing key not available, request authentication", connection);
            return requestAuthentication(connection);
        } else if (cb->is_master) {
#if BLE_ROLE_CENTRAL
            tr_info("Connection %d - Signing key not available, request pairing", connection);
            return requestPairing(connection);
#else
            tr_error("Failure, can't act as master if central role disabled");
            return BLE_ERROR_NOT_IMPLEMENTED;
#endif
        } else {
#if BLE_ROLE_PERIPHERAL
            tr_info("Connection %d - Signing key not available, issue slave security request to pair", connection);
            return slave_security_request(connection);
#else
            tr_error("Failure, can't act as slave if peripheral role disabled");
            return BLE_ERROR_NOT_IMPLEMENTED;
#endif
        }
    }
}
#endif // BLE_FEATURE_SIGNING

////////////////////////////////////////////////////////////////////////////
// Privacy
//

#if BLE_FEATURE_PRIVACY
ble_error_t SecurityManager::setPrivateAddressTimeout(uint16_t timeout_in_seconds)
{
    tr_info("Set private address timeout: %d s", timeout_in_seconds);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    _private_address_controller.set_timeout(
        resolvable_address_timeout_t(timeout_in_seconds)
    );
    return BLE_ERROR_NONE;
}
#endif // BLE_FEATURE_PRIVACY

////////////////////////////////////////////////////////////////////////////
// Authentication
//


ble_error_t SecurityManager::requestAuthentication(connection_handle_t connection)
{
    tr_info("Connection %d - Request authentication", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (flags->ltk_mitm_protected) {
        if (cb->authenticated) {
            tr_info("Connection %d - Nothing to do, link already authenticated", connection);
            return BLE_ERROR_NONE;
        } else {
            tr_info("Connection %d - ltk mitm protected, requesting encryption", connection);
            cb->encryption_requested = true;
            return enable_encryption(connection);
        }
    } else {
        cb->mitm_requested = true;
        if (cb->is_master) {
            tr_info("Connection %d - ltk not mitm protected, request new pairing", connection);
#if BLE_ROLE_CENTRAL
            return requestPairing(connection);
#else
            tr_error("Invalid state cannot request pairing if central role disabled");
            return BLE_ERROR_NOT_IMPLEMENTED;
#endif
        } else {
            tr_info("Connection %d - ltk not mitm protected, issue slave security request", connection);
#if BLE_ROLE_PERIPHERAL
            return slave_security_request(connection);
#else
            tr_error("Invalid state, cannot send slave security request if peripheral role disabled");
            return BLE_ERROR_NOT_IMPLEMENTED;
#endif
        }
    }
}

////////////////////////////////////////////////////////////////////////////
// MITM
//


ble_error_t SecurityManager::generateOOB(
    const address_t *address
)
{
    MBED_ASSERT(address != nullptr);

    tr_info("Generate OOB for %s", to_string(*address));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    /* legacy pairing */
    ble_error_t status = get_random_data(_oob_temporary_key.data(), 16);

    if (status == BLE_ERROR_NONE) {
        tr_info("OOB temporary key generated: %s", to_string(_oob_temporary_key));
        _oob_temporary_key_creator_address = *address;

        if (eventHandler) {
            eventHandler->legacyPairingOobGenerated(
                &_oob_temporary_key_creator_address,
                &_oob_temporary_key
            );
        }
    } else {
        tr_error("Failed to generate random data");
        return status;
    }

#if BLE_FEATURE_SECURE_CONNECTIONS
    /* Secure connections. Avoid generating if we're already waiting for it.
     * If a local random is set to 0 it means we're already calculating. */
    if (!is_all_zeros(_oob_local_random)) {
        /* save the current values in case the call to
         * generate_secure_connections_oob fails */
        address_t orig_local_address = _oob_local_address;
        oob_lesc_value_t orig_local_random = _oob_local_random;

        _oob_local_address = *address;
        /* this will be updated when calculation completes,
         * a value of all zeros is an invalid random value */
        set_all_zeros(_oob_local_random);

        status = _pal.generate_secure_connections_oob();
        if (status != BLE_ERROR_NONE && status != BLE_ERROR_NOT_IMPLEMENTED) {
            _oob_local_address = orig_local_address;
            _oob_local_random = orig_local_random;
            return status;
        }
    } else {
        tr_error("Impossible to generate SC OOB data: computation already running");
        return BLE_STACK_BUSY;
    }
#endif // BLE_FEATURE_SECURE_CONNECTIONS

    return BLE_ERROR_NONE;
}


ble_error_t SecurityManager::setOOBDataUsage(
    connection_handle_t connection,
    bool useOOB,
    bool OOBProvidesMITM
)
{
    tr_info("Connection %d - Set OOB data usage: use OOB = %s, Provides MITM = %s", connection, to_string(useOOB), to_string(OOBProvidesMITM));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    cb->attempt_oob = useOOB;
    cb->oob_mitm_protection = OOBProvidesMITM;

    if (useOOB) {
        return generateOOB(&cb->local_address);
    } else {
        return BLE_ERROR_NONE;
    }
}

#if BLE_FEATURE_SECURE_CONNECTIONS
ble_error_t SecurityManager::confirmationEntered(
    connection_handle_t connection,
    bool confirmation
)
{
    tr_info("Connection %d - confirmation entered: %s", connection, to_string(confirmation));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.confirmation_entered(connection, confirmation);
}
#endif // BLE_FEATURE_SECURE_CONNECTIONS

ble_error_t SecurityManager::passkeyEntered(
    connection_handle_t connection,
    Passkey_t passkey
)
{
    tr_info("Connection %d - Passkey entered: %s", connection, passkey_str(passkey));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.passkey_request_reply(
        connection,
        PasskeyAscii::to_num(passkey)
    );
}

#if BLE_FEATURE_SECURE_CONNECTIONS
ble_error_t SecurityManager::sendKeypressNotification(
    connection_handle_t connection,
    ble::Keypress_t keypress
)
{
    tr_info("Connection %d - Send Keypress notification: %s", connection, to_string(keypress));
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    return _pal.send_keypress_notification(connection, keypress);
}
#endif // BLE_FEATURE_SECURE_CONNECTIONS


ble_error_t SecurityManager::legacyPairingOobReceived(
    const address_t *address,
    const oob_tk_t *tk
)
{
    if (!_db) {
        tr_error("Failed to set legacy pairing OOB, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    if (address && tk) {
        tr_info("Legacy pairing OOB received: address = %s, tk = %s", to_string(address), to_string(tk));
        ControlBlock_t *cb = get_control_block(*address);
        if (!cb) {
            tr_error("Failure, control block not available");
            return BLE_ERROR_INVALID_PARAM;
        }

        SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
        if (!flags) {
            tr_error("Failure, no distribution flags set");
            return BLE_ERROR_INVALID_PARAM;
        }

        _oob_temporary_key = *tk;
        _oob_temporary_key_creator_address = *address;

        if (flags->peer_address == _oob_temporary_key_creator_address) {
            cb->attempt_oob = true;
        }

        if (cb->legacy_pairing_oob_request_pending) {
            on_legacy_pairing_oob_request(cb->connection);
            /* legacy_pairing_oob_request_pending stops us from
             * going into a loop of asking the user for oob
             * so this reset needs to happen after the call above */
            cb->legacy_pairing_oob_request_pending = false;
        }
    } else {
        tr_warning("Trying to set invalid legacy OOB pairing data: address = %p, temporary key = %p", address, tk);
    }
    return BLE_ERROR_NONE;
}

#if BLE_FEATURE_SECURE_CONNECTIONS
ble_error_t SecurityManager::oobReceived(
    const address_t *address,
    const oob_lesc_value_t *random,
    const oob_confirm_t *confirm
)
{
    if (!_db) {
        tr_error("Failed to set OOB pairing data, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    if (address && random && confirm) {
        tr_info("Set OOB pairing data: address = %s, random = %s, confirm = %s", to_string(*address), to_string(*random), to_string(*confirm));
        _oob_peer_address = *address;
        _oob_peer_random = *random;
        _oob_peer_confirm = *confirm;
        return BLE_ERROR_NONE;
    } else {
        tr_warning("Trying to set invalid OOB pairing data: address = %p, random = %p, confirm = %p", address, random, confirm);
    }

    return BLE_ERROR_INVALID_PARAM;
}
#endif // BLE_FEATURE_SECURE_CONNECTIONS

////////////////////////////////////////////////////////////////////////////
// Helper functions
//


ble_error_t SecurityManager::init_database(
    const char *db_path
)
{
    tr_info("Initialize database.");
    delete _db;

#if BLE_SECURITY_DATABASE_FILESYSTEM
    FILE* db_file = nullptr;
    if (db_path) {
        tr_info("Using filesystem path = %s", db_path);
        db_file = FileSecurityDb::open_db_file(db_path);
    }

    if (db_file) {
        _db = new (std::nothrow) FileSecurityDb(db_file);
    } else
#else
    if (db_path) {
        tr_error("Passed in a path for BLE security DB init but BLE_SECURITY_DATABASE_FILESYSTEM is disabled");
    }
#endif
#if BLE_SECURITY_DATABASE_KVSTORE
    if (KVStoreSecurityDb::open_db()) {
        _db = new (std::nothrow) KVStoreSecurityDb();
    } else
#endif
    {
        _db = new (std::nothrow) MemorySecurityDb();
    }

    if (!_db) {
        tr_error("Not enough memory to create the database");
        return BLE_ERROR_NO_MEM;
    }

    _db->restore();

    return BLE_ERROR_NONE;
}

#if BLE_FEATURE_PRIVACY
ble_error_t SecurityManager::init_resolving_list()
{
    tr_info("Initialize resolving list");
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    /* match the resolving list to the currently stored set of IRKs */
    uint8_t resolving_list_capacity = _private_address_controller.read_resolving_list_capacity();
    auto* identity_list_p =
        new (std::nothrow) SecurityEntryIdentity_t[resolving_list_capacity];

    if (identity_list_p) {
        Span<SecurityEntryIdentity_t> identity_list(
            identity_list_p,
            resolving_list_capacity
        );

        _db->get_identity_list(
            mbed::callback(this, &SecurityManager::on_identity_list_retrieved),
            identity_list
        );
    } else {
        tr_error("Not enough memory to create resolving list");
        return BLE_ERROR_NO_MEM;
    }

    return BLE_ERROR_NONE;
}
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_SIGNING
ble_error_t SecurityManager::init_signing()
{
    tr_info("Init signing");
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    const csrk_t *pcsrk = _db->get_local_csrk();
    sign_count_t local_sign_counter = _db->get_local_sign_counter();

    csrk_t csrk;
    if (!pcsrk || *pcsrk == csrk_t{}) {
        ble_error_t ret = get_random_data(csrk.data(), csrk.size());
        if (ret != BLE_ERROR_NONE) {
            tr_error("Failed to create local csrk");
            return ret;
        }

        pcsrk = &csrk;
        _db->set_local_csrk(csrk);
        _db->set_local_sign_counter(local_sign_counter);
    }

    return _pal.set_csrk(*pcsrk, local_sign_counter);
}
#endif // BLE_FEATURE_SIGNING

#if BLE_FEATURE_PRIVACY
ble_error_t SecurityManager::init_identity()
{
    tr_info("Init identity");
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }
    const irk_t *pirk = nullptr;

    ble::Gap& gap = BLE::Instance().gap();

    irk_t irk = _db->get_local_irk();
    address_t identity_address;
    bool public_identity_address = false;
    if (irk != irk_t()) {
        pirk = &irk;
        public_identity_address = _db->is_local_identity_address_public();
        identity_address = _db->get_local_identity_address();

        if (!_db->is_local_identity_address_public()) {
            // Some controllers doesn't store their random static address and
            // instead generates them at each reboot.
            // The code should replace the random static address with the identity
            // address if this is the case.
            if (_db->get_local_identity_address() != gap.getRandomStaticAddress()) {
                ble_error_t err = gap.setRandomStaticAddress(_db->get_local_identity_address());
                if (err) {
                    return err;
                }
            }
        }
    } else {
        ble_error_t ret = get_random_data(irk.data(), irk.size());
        if (ret != BLE_ERROR_NONE) {
            tr_error("Failed to create local irk");
            return ret;
        }

        pirk = &irk;
        public_identity_address = false;
        identity_address = gap.getRandomStaticAddress();
        _db->set_local_identity(irk, identity_address, public_identity_address);
    }

    auto err = _pal.set_irk(*pirk);
    if (!err) {
        _private_address_controller.set_local_irk(*pirk);
        _pal.set_identity_address(identity_address, public_identity_address);
    }
    return err;
}
#endif // BLE_FEATURE_PRIVACY

ble_error_t SecurityManager::get_random_data(uint8_t *buffer, size_t size)
{
    size_t remaining_size = size;
    uint8_t* it = buffer;
    byte_array_t<8> random_data;

    while (remaining_size) {
        /* fill out the buffer by reading the random data in chunks
         * and copying it until reaching the set size */
        size_t copy_size = std::min(remaining_size, random_data.size());
        ble_error_t ret = _pal.get_random_data(random_data);
        if (ret != BLE_ERROR_NONE) {
            tr_error("Pal failed at generating remaining %d bytes of random data, abort get_random_data.", size);
            return ret;
        }
        memcpy(it, random_data.data(), copy_size);
        remaining_size -= copy_size;
        it += copy_size;
    }

    tr_info("Random data generated: %s", tr_array(buffer, size));

    return BLE_ERROR_NONE;
}


#if BLE_ROLE_PERIPHERAL
ble_error_t SecurityManager::slave_security_request(connection_handle_t connection)
{
    tr_info("Connection %d - Initiate security request", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }
    AuthenticationMask link_authentication(_default_authentication);
    link_authentication.set_mitm(cb->mitm_requested);
    return _pal.slave_security_request(connection, link_authentication);
}
#endif // BLE_ROLE_PERIPHERAL


ble_error_t SecurityManager::enable_encryption(connection_handle_t connection)
{
    tr_info("Connection %d - Enable encryption", connection);
    if (!_db) {
        tr_error("Failure, DB not initialized");
        return BLE_ERROR_INITIALIZATION_INCOMPLETE;
    }

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return BLE_ERROR_INVALID_PARAM;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return BLE_ERROR_INVALID_PARAM;
    }

    if (cb->is_master) {
#if BLE_ROLE_CENTRAL
        if (flags->ltk_stored) {
            _db->get_entry_peer_keys(
                mbed::callback(this, &SecurityManager::enable_encryption_cb),
                cb->db_entry
            );
            return BLE_ERROR_NONE;
        } else {
            tr_info("Connection %d - Ltk not available, initiate pairing", connection);
            return requestPairing(connection);
        }
#else
        tr_error("Invalid state, cannot act as a master if central role disabled");
        return BLE_ERROR_NOT_IMPLEMENTED;
#endif // BLE_ROLE_CENTRAL
    } else {
#if BLE_ROLE_PERIPHERAL
        tr_info("Connection %d - Ltk not available, send slave security request", connection);
        return slave_security_request(connection);
#else
        tr_error("Invalid state, cannot act as a master if peripheral role disabled");
        return BLE_ERROR_NOT_IMPLEMENTED;
#endif // BLE_ROLE_PERIPHERAL
    }
}

#if BLE_ROLE_CENTRAL
void SecurityManager::enable_encryption_cb(
    SecurityDb::entry_handle_t db_entry,
    const SecurityEntryKeys_t* entryKeys
)
{
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(db_entry);
    if (!cb) {
        tr_error("Failure, control block %p not available", db_entry);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Cannot enable encryption, no distribution flags found for entry %p", cb->connection, cb->db_entry);
        return;
    }

    if (entryKeys) {
#if BLE_FEATURE_SECURE_CONNECTIONS
        if (flags->secure_connections_paired) {
            _pal.enable_encryption(cb->connection, entryKeys->ltk, flags->ltk_mitm_protected);
        } else
#endif
        {
            _pal.enable_encryption(cb->connection, entryKeys->ltk, entryKeys->rand, entryKeys->ediv, flags->ltk_mitm_protected);
        }
    }
}
#endif

void SecurityManager::set_ltk_cb(
    SecurityDb::entry_handle_t db_entry,
    const SecurityEntryKeys_t* entryKeys
)
{
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(db_entry);
    if (!cb) {
        tr_error("Failure, control block %p not available", db_entry);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Invalid state, cannot set remote ltk, no distribution flags found for entry %p",
            cb->connection, cb->db_entry);
        return;
    }

    if (entryKeys) {
        _pal.set_ltk(
            cb->connection,
            entryKeys->ltk,
            flags->ltk_mitm_protected,
            flags->secure_connections_paired
        );
    } else {
        _pal.set_ltk_not_found(cb->connection);
    }
}

#if BLE_FEATURE_SIGNING
void SecurityManager::set_peer_csrk_cb(
    SecurityDb::entry_handle_t db_entry,
    const SecurityEntrySigning_t* signing
)
{
    ControlBlock_t *cb = get_control_block(db_entry);
    if (!cb) {
        tr_error("Failure, control block not available for DB entry %p", db_entry);
        return;
    }

    if (!signing) {
        tr_error("Connection %d - No peer csrk found for DB entry %p", cb->connection, db_entry);
        return;
    }


    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    _pal.set_peer_csrk(
        cb->connection,
        signing->csrk,
        flags->csrk_mitm_protected,
        signing->counter
    );
}


void SecurityManager::return_csrk_cb(
    SecurityDb::entry_handle_t db_entry,
    const SecurityEntrySigning_t *signing
)
{
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(db_entry);
    if (!cb) {
        tr_error("Cannot retrieve signing key for db_entry %p", db_entry);
        return;
    }

    if (!signing) {
        tr_error("Connection %d - No signing key retrieve for DB entry %p", cb->connection, db_entry);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Signing key invalid for DB entry %p, flags not available", cb->connection, db_entry);
        return;
    }

    tr_info("Connection %d - Signing key %s, authenticated: %s retrieved",
        cb->connection, to_string(signing->csrk), to_string(flags->csrk_mitm_protected));
    if (eventHandler) {
        eventHandler->signingKey(
            cb->connection,
            &signing->csrk,
            flags->csrk_mitm_protected
        );
    }
}
#endif // BLE_FEATURE_SIGNING

void SecurityManager::update_oob_presence(connection_handle_t connection)
{
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure while updating OOB presence, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failure while updating OOB presence, no distribution flags set", connection);
        return;
    }

    /* if we support secure connection we only care about secure connections oob data */
    if (_default_authentication.get_secure_connections()) {
        cb->oob_present = (flags->peer_address == _oob_peer_address);
    } else {
        /* otherwise for legacy pairing we first set the oob based on set preference */
        cb->oob_present = cb->attempt_oob;

        /* and also turn it on if we have oob data for legacy pairing */
        if (flags->peer_address == _oob_temporary_key_creator_address
            || cb->local_address == _oob_temporary_key_creator_address) {
            cb->oob_present = true;
        }
    }
    tr_info("Connection %d - Update OOB presence: %s", connection, to_string(cb->oob_present));
}


void SecurityManager::set_mitm_performed(connection_handle_t connection, bool enable)
{
    ControlBlock_t *cb = get_control_block(connection);
    if (cb) {
        cb->mitm_performed = enable;
        /* whenever we reset mitm performed we also reset pending requests
         * as this happens whenever a new pairing attempt happens */
        if (!enable) {
            cb->legacy_pairing_oob_request_pending = false;
        }
        tr_info("Connection %d - MITM performed: %s", connection, to_string(enable));
    } else {
        tr_error("Connection %d - Failed to apply mitm performed, CB not available", connection);
    }
}


void SecurityManager::on_connected(
    connection_handle_t connection,
    connection_role_t role,
    peer_address_type_t peer_address_type,
    address_t peer_address,
    own_address_type_t local_address_type,
    address_t local_address
)
{
#if BLE_FEATURE_SECURITY
    MBED_ASSERT(_db);
    ControlBlock_t *cb = acquire_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failed to acquire a control block for the new connection", connection);
        return;
    }

    // setup the control block
    cb->local_address = local_address;
    cb->is_master = (role == connection_role_t::CENTRAL);

    // get the associated DB handle and the distribution flags if any
    cb->db_entry = _db->open_entry(peer_address_type, peer_address);

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);

    flags->peer_address = peer_address;
    flags->peer_address_is_public =
        (peer_address_type == peer_address_type_t::PUBLIC) ||
        (peer_address_type == peer_address_type_t::PUBLIC_IDENTITY);

    tr_info("Connection %d - Open: ltk=%s, csrk=%s, irk=%s",
        connection, to_string(flags->ltk_stored), to_string(flags->csrk_stored), to_string(flags->irk_stored));

#if BLE_FEATURE_SIGNING
    const bool signing = cb->signing_override_default ?
	                         cb->signing_requested :
	                         _default_key_distribution.get_signing();

    if (signing && flags->csrk_stored) {
        _db->get_entry_peer_csrk(
            mbed::callback(this, &SecurityManager::set_peer_csrk_cb),
            cb->db_entry
        );
    }
#endif // BLE_FEATURE_SIGNING
#endif // BLE_FEATURE_SECURITY
}


void SecurityManager::on_disconnected(
    connection_handle_t connection,
    disconnection_reason_t reason
)
{
    tr_info("Connection %d - Process disconnection", connection);
#if BLE_FEATURE_SECURITY
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Error, control block not available", connection);
        return;
    }
#if BLE_FEATURE_SIGNING
    _pal.remove_peer_csrk(connection);
#endif

    _db->close_entry(cb->db_entry);
    release_control_block(cb);
#endif // BLE_FEATURE_SECURITY
}


void SecurityManager::on_security_entry_retrieved(
    SecurityDb::entry_handle_t entry,
    const SecurityEntryIdentity_t* identity
)
{
    if (!identity) {
        tr_info("No identity retrieve for entry %p", entry);
        return;
    }

    tr_info("Security identity retrieved for entry %p: address: %s, public: %s, irk: %s",
        entry, to_string(identity->identity_address),
        to_string(identity->identity_address_is_public), to_string(identity->irk));

    typedef advertising_peer_address_type_t address_type_t;
#if BLE_FEATURE_PRIVACY
    _private_address_controller.add_device_to_resolving_list(
        identity->identity_address_is_public ?
            address_type_t::PUBLIC :
            address_type_t::RANDOM,
        identity->identity_address,
        identity->irk
    );
#endif // BLE_FEATURE_PRIVACY
}


void SecurityManager::on_identity_list_retrieved(
    Span<SecurityEntryIdentity_t>& identity_list,
    size_t count
)
{
    tr_info("Identity list retrieved: %d entries to add to the resolving list", count);
    typedef advertising_peer_address_type_t address_type_t;

#if BLE_FEATURE_PRIVACY
    _private_address_controller.clear_resolving_list();
    for (size_t i = 0; i < count; ++i) {
        _private_address_controller.add_device_to_resolving_list(
            identity_list[i].identity_address_is_public ?
                address_type_t::PUBLIC :
                address_type_t::RANDOM,
            identity_list[i].identity_address,
            identity_list[i].irk
        );
    }
#endif // BLE_FEATURE_PRIVACY

    delete [] identity_list.data();
}

/* Implements ble::PalSecurityManagerEventHandler */

////////////////////////////////////////////////////////////////////////////
// Pairing
//

#if BLE_ROLE_PERIPHERAL
void SecurityManager::on_pairing_request(
    connection_handle_t connection,
    bool use_oob,
    AuthenticationMask authentication,
    KeyDistribution initiator_dist,
    KeyDistribution responder_dist
)
{
    tr_info("Connection %d - pairing request received: use_oob = %s, authentication = %02X, initiator_dist = %02X, responder_dist = %02X",
        connection, to_string(use_oob), authentication.value(), initiator_dist.value(), responder_dist.value()
    );

    /* cancel pairing if secure connection paring is not possible */
    if (!_legacy_pairing_allowed && !authentication.get_secure_connections()) {
        tr_info("Connection %d - Cancel pairing request, peer does not meet SC requirements", connection);
        cancelPairingRequest(connection);
        return;
    }

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    cb->set_initiator_key_distribution(initiator_dist);
    cb->set_responder_key_distribution(responder_dist);

    cb->mitm_performed = false;

    if (_pairing_authorisation_required) {
        if (eventHandler) {
            tr_info("Connection %d - Forwarding pairing request to user handler", connection);
            eventHandler->pairingRequest(connection);
        } else {
            tr_error("Connection %d - Cannot process pairing request: Require user validation but no handler has been registered.", connection);
        }
    } else {
        acceptPairingRequest(connection);
    }
}
#endif // BLE_ROLE_PERIPHERAL

void SecurityManager::on_pairing_error(
    connection_handle_t connection,
    pairing_failure_t error
)
{
    tr_info("Connection %d - Pairing error %s received", connection, to_string(error.value()));
    set_mitm_performed(connection, false);

    if (eventHandler) {
        eventHandler->pairingResult(
            connection,
            (SecurityCompletionStatus_t)(error.value() | 0x80)
        );
    }

    /* if this pairing was triggered by a failed encryption attempt
     * inform the application of the encryption failure */
    ControlBlock_t *cb = get_control_block(connection);
    if (cb && cb->encryption_requested && cb->encryption_failed && eventHandler) {
        tr_info("Connection %d - Report encryption failure to app handler", connection);
        eventHandler->linkEncryptionResult(
            connection,
            link_encryption_t::NOT_ENCRYPTED
        );
    }
}


void SecurityManager::on_pairing_timed_out(connection_handle_t connection)
{
    tr_info("Connection %d - Pairing timed out", connection);
    set_mitm_performed(connection, false);

    eventHandler->pairingResult(
        connection,
        SEC_STATUS_TIMEOUT
    );
}


void SecurityManager::on_pairing_completed(connection_handle_t connection)
{
    tr_info("Connection %d - Pairing successfully completed", connection);
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (cb) {
        tr_info("Connection %d - Retrieve identity address to register it into resolving list", connection);
        _db->get_entry_identity(
            mbed::callback(this, &SecurityManager::on_security_entry_retrieved),
            cb->db_entry
        );
    }

    eventHandler->pairingResult(
        connection,
        SEC_STATUS_SUCCESS
    );
}

////////////////////////////////////////////////////////////////////////////
// Security
//


void SecurityManager::on_valid_mic_timeout(connection_handle_t connection)
{
    tr_info("Connection %d - Received authentication timeout", connection);
    (void)connection;
}

#if BLE_FEATURE_SIGNING
void SecurityManager::on_signed_write_received(
    connection_handle_t connection,
    sign_count_t sign_counter
)
{
    MBED_ASSERT(_db);
    tr_info("Connection %d - Signed write received: write counter = %" PRIu32, connection, sign_counter);

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        return;
    }
    _db->set_entry_peer_sign_counter(cb->db_entry, sign_counter);
}


void SecurityManager::on_signed_write_verification_failure(
    connection_handle_t connection
)
{
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    tr_info("Connection %d - Received signed write verification failure", connection);

    const bool signing = cb->signing_override_default ?
	                         cb->signing_requested :
	                         _default_key_distribution.get_signing();

    if (signing) {
        cb->csrk_failures++;
        if (cb->csrk_failures == 3) {
            cb->csrk_failures = 0;
            if (cb->is_master) {
#if BLE_ROLE_CENTRAL
                tr_info("Connection %d - Signed write verification failure threshold reached, request pairing", connection);
                requestPairing(connection);
#endif
            } else {
#if BLE_ROLE_PERIPHERAL
                tr_info("Connection %d - Signed write verification failure threshold reached, send slave security request", connection);
                slave_security_request(connection);
#endif
            }
        }
    }
}


void SecurityManager::on_signed_write()
{
    MBED_ASSERT(_db);
    _db->set_local_sign_counter(_db->get_local_sign_counter() + 1);
    tr_info("Signed write sent, local sign counter: %" PRIu32, _db->get_local_sign_counter());
}
#endif // BLE_FEATURE_SIGNING

#if BLE_ROLE_CENTRAL
void SecurityManager::on_slave_security_request(
    connection_handle_t connection,
    AuthenticationMask authentication
)
{
    tr_info("Connection %d - Slave security request received: authentication = %02X",
        connection, authentication.value());
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    bool pairing_required = false;

    if (authentication.get_secure_connections() && !flags->secure_connections_paired
        && _default_authentication.get_secure_connections()) {
        pairing_required = true;
    }

    if (authentication.get_mitm() && !flags->ltk_mitm_protected) {
        pairing_required = true;
        cb->mitm_requested = true;
    }

    if (pairing_required) {
        tr_info("Connection %d - Initiate pairing to satisfy slave security request, mitm: %s", connection, to_string(cb->mitm_requested));
        requestPairing(connection);
    } else if (!cb->encryption_requested) {
        /* this will refresh keys if encryption is already present */
        tr_info("Connection %d - Enable encryption to satisfy slave security request", connection);
        enable_encryption(connection);
    } else {
        tr_info("Connection %d - Encryption pending, nothing to do to satisfy slave security request", connection);
    }
}
#endif // BLE_ROLE_CENTRAL

////////////////////////////////////////////////////////////////////////////
// Encryption
//


void SecurityManager::on_link_encryption_result(
    connection_handle_t connection,
    link_encryption_t result
)
{
    tr_info("Connection %d - Link encryption: updated = %s", connection, to_string(result));

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    if (result == link_encryption_t::ENCRYPTED) {

        cb->encryption_requested = false;
        cb->encryption_failed = false;
        cb->encrypted = true;

    } else if (
        result == link_encryption_t::ENCRYPTED_WITH_MITM ||
        result == link_encryption_t::ENCRYPTED_WITH_SC_AND_MITM
    ) {

        cb->encryption_requested = false;
        cb->encryption_failed = false;
        cb->authenticated = true;
        cb->encrypted = true;

    }
#if BLE_ROLE_CENTRAL
    else if (result == link_encryption_t::NOT_ENCRYPTED
               && cb->encryption_requested
               && !cb->encryption_failed) {

        /* if we failed encryption for the first time
         * retry repairing in case slave lost LTK */
        tr_info("Connection %d - Encryption failed, attempt to pair again, the slave may have lost the shared LTK", connection);
        requestPairing(cb->connection);
        cb->encryption_failed = true;
        /* don't return an event yet since we are retrying */
        return;
    }
#endif // BLE_ROLE_CENTRAL

    eventHandler->linkEncryptionResult(connection, result);
}


void SecurityManager::on_link_encryption_request_timed_out(
    connection_handle_t connection
)
{
    tr_info("Connection %d - Link encryption timed out", connection);
    eventHandler->linkEncryptionResult(
        connection,
        link_encryption_t::NOT_ENCRYPTED
    );
}

////////////////////////////////////////////////////////////////////////////
// MITM
//


void SecurityManager::on_passkey_display(
    connection_handle_t connection,
    passkey_num_t passkey
)
{
    set_mitm_performed(connection);
    PasskeyAscii ascii_passkey(passkey);
    tr_info("Connection %d - Received passkey display %.*s",
        connection, ascii_passkey.PASSKEY_LEN, ascii_passkey.value());
    if (eventHandler) {
        eventHandler->passkeyDisplay(connection, ascii_passkey.value());
    } else {
        tr_warning("Connection %d - No app handler to display pass key", connection);
    }

}

void SecurityManager::on_passkey_request(connection_handle_t connection)
{
    tr_info("Connection %d - Received passkey request event", connection);
    set_mitm_performed(connection);
    if (eventHandler) {
        eventHandler->passkeyRequest(connection);
    } else {
        tr_warning("Connection %d - No app handler to forward pass key request", connection);
    }

}

void SecurityManager::on_legacy_pairing_oob_request(connection_handle_t connection)
{
    tr_info("Connection %d - Received OOB request for legacy pairing", connection);
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    if (flags->peer_address == _oob_temporary_key_creator_address
        || cb->local_address == _oob_temporary_key_creator_address) {
        tr_info("Connection %d - Use OOB temporary key stored locally", connection);
        set_mitm_performed(connection);
        _pal.legacy_pairing_oob_request_reply(connection, _oob_temporary_key);

        /* do not re-use peer OOB */
        if (flags->peer_address == _oob_temporary_key_creator_address) {
            set_all_zeros(_oob_temporary_key_creator_address);
        }

    } else if (!cb->legacy_pairing_oob_request_pending) {

        cb->legacy_pairing_oob_request_pending = true;
        if (eventHandler) {
            tr_info("Connection %d - Send OOB request to application", connection);
            eventHandler->legacyPairingOobRequest(connection);
        } else {
            tr_error("Connection %d - OOB requested but no application event handler available", connection);
        }
    }
}

#if BLE_FEATURE_SECURE_CONNECTIONS
void SecurityManager::on_keypress_notification(
    connection_handle_t connection,
    ble::Keypress_t keypress
)
{
    tr_info("Connection %d - Keypress notification %s received", connection, to_string(keypress));
    set_mitm_performed(connection);
    if (eventHandler) {
        eventHandler->keypressNotification(connection, keypress);
    } else {
        tr_error("Impossible to forward Keypress notification to application, no event handler registered");
    }
}

void SecurityManager::on_confirmation_request(connection_handle_t connection)
{
    tr_info("Connection %d - Confirmation request received", connection);
    set_mitm_performed(connection);
    if (eventHandler) {
        eventHandler->confirmationRequest(connection);
    } else {
        tr_error("Impossible to forward confirmation request to application, no event handler registered");
    }
}

void SecurityManager::on_secure_connections_oob_request(connection_handle_t connection)
{
    tr_info("Connection %d - Received SC OOB request", connection);
    set_mitm_performed(connection);

    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    if (flags->peer_address == _oob_peer_address) {
        _pal.secure_connections_oob_request_reply(connection, _oob_local_random, _oob_peer_random, _oob_peer_confirm);
        /* do not re-use peer OOB */
        set_all_zeros(_oob_peer_address);
    } else {
        _pal.cancel_pairing(connection, pairing_failure_t::OOB_NOT_AVAILABLE);
    }
}

void SecurityManager::on_secure_connections_oob_generated(
    const oob_lesc_value_t &random,
    const oob_confirm_t &confirm
)
{
    tr_info("SC OOB generated, random = %s, confirm = %s", to_string(random), to_string(confirm));
    eventHandler->oobGenerated(&_oob_local_address, &random, &confirm);
    _oob_local_random = random;
}
#endif // BLE_FEATURE_SECURE_CONNECTIONS

////////////////////////////////////////////////////////////////////////////
// Keys
//

#if BLE_FEATURE_SECURE_CONNECTIONS
void SecurityManager::on_secure_connections_ltk_generated(
    connection_handle_t connection,
    const ltk_t &ltk
)
{
    tr_info("Connection %d - SC LTK generated: %s", connection, to_string(ltk));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    flags->ltk_mitm_protected = cb->mitm_performed;
    flags->secure_connections_paired = true;

    _db->set_entry_peer_ltk(cb->db_entry, ltk);
    _db->set_entry_local_ltk(cb->db_entry, ltk);
}
#endif // BLE_FEATURE_SECURE_CONNECTIONS


void SecurityManager::on_keys_distributed_ltk(
    connection_handle_t connection,
    const ltk_t &ltk
)
{
    tr_info("Connection %d - LTK distributed: %s", connection, to_string(ltk));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    flags->ltk_mitm_protected = cb->mitm_performed;

    _db->set_entry_peer_ltk(cb->db_entry, ltk);
}


void SecurityManager::on_keys_distributed_ediv_rand(
    connection_handle_t connection,
    const ediv_t &ediv,
    const rand_t &rand
)
{
    tr_info("Connection %d - EDIV and RAND distributed: ediv = %s, rand = %s", connection, to_string(ediv), to_string(rand));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    _db->set_entry_peer_ediv_rand(cb->db_entry, ediv, rand);
}


void SecurityManager::on_keys_distributed_local_ltk(
    connection_handle_t connection,
    const ltk_t &ltk
)
{
    tr_info("Connection %d - Local LTK distributed: %s", connection, to_string(ltk));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Failure, no distribution flags set");
        return;
    }

    _db->set_entry_local_ltk(cb->db_entry, ltk);
}


void SecurityManager::on_keys_distributed_local_ediv_rand(
    connection_handle_t connection,
    const ediv_t &ediv,
    const rand_t &rand
)
{
    tr_info("Connection %d - Local EDIV/RAND distributed: EDIV = %s, RAND = %s", connection, to_string(ediv), to_string(rand));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    _db->set_entry_local_ediv_rand(cb->db_entry, ediv, rand);
}


void SecurityManager::on_keys_distributed_irk(
    connection_handle_t connection,
    const irk_t &irk
)
{
    tr_info("Connection %d - IRK distributed: %s", connection, to_string(irk));
    
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failure, no distribution flags set", connection);
        return;
    }

    _db->set_entry_peer_irk(cb->db_entry, irk);
}


void SecurityManager::on_keys_distributed_bdaddr(
    connection_handle_t connection,
    advertising_peer_address_type_t peer_address_type,
    const address_t &peer_identity_address
)
{
    tr_info("Connection %d - Peer identity distributed: address = %s, public = %s",
        connection, to_string(peer_identity_address), to_string(peer_address_type == advertising_peer_address_type_t::PUBLIC));

    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    _db->set_entry_peer_bdaddr(
        cb->db_entry,
        (peer_address_type == advertising_peer_address_type_t::PUBLIC),
        peer_identity_address
    );
}

#if BLE_FEATURE_SIGNING
void SecurityManager::on_keys_distributed_csrk(
    connection_handle_t connection,
    const csrk_t &csrk
)
{
    tr_info("Connection %d - CSRK distributed: %s", connection, to_string(csrk));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failure, no distribution flags set", connection);
        return;
    }

    flags->csrk_mitm_protected = cb->mitm_performed;
    _db->set_entry_peer_csrk(cb->db_entry, csrk);

    eventHandler->signingKey(
        connection,
        &csrk,
        flags->csrk_mitm_protected
    );
}
#endif // BLE_FEATURE_SIGNING


void SecurityManager::on_ltk_request(
    connection_handle_t connection,
    const ediv_t &ediv,
    const rand_t &rand
)
{
    tr_info("Connection %d - LTK requested: EDIV = %s, RAND = %s", connection, to_string(ediv), to_string(rand));
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
    if (!flags) {
        tr_error("Connection %d - Failure, no distribution flags set", connection);
        return;
    }

    _db->get_entry_local_keys(
        mbed::callback(this, &SecurityManager::set_ltk_cb),
        &cb->db_entry,
        ediv,
        rand
    );
}

/* control blocks list management */


SecurityManager::ControlBlock_t::ControlBlock_t() :
    connection(0),
    db_entry(nullptr),
    local_address(),
    connected(false),
    authenticated(false),
    is_master(false),
    encryption_requested(false),
    encryption_failed(false),
    encrypted(false),
    signing_requested(false),
    signing_override_default(false),
    mitm_requested(false),
    mitm_performed(false),
    attempt_oob(false),
    oob_mitm_protection(false),
    oob_present(false),
    legacy_pairing_oob_request_pending(false),
    csrk_failures(0) { }


void SecurityManager::on_ltk_request(connection_handle_t connection)
{
    tr_info("Connection %d - SC LTK requested", connection);
    MBED_ASSERT(_db);
    ControlBlock_t *cb = get_control_block(connection);
    if (!cb) {
        tr_error("Connection %d - Failure, control block not available", connection);
        return;
    }

    _db->get_entry_local_keys(
        mbed::callback(this, &SecurityManager::set_ltk_cb),
        cb->db_entry
    );
}


typename SecurityManager::ControlBlock_t*
SecurityManager::acquire_control_block(connection_handle_t connection)
{
    /* grab the first disconnected slot*/
    for (auto & control_block : _control_blocks) {
        if (!control_block.connected) {
            ControlBlock_t* cb = &control_block;
            cb->connected = true;
            cb->connection = connection;
            return cb;
        }
    }

    return nullptr;
}


typename SecurityManager::ControlBlock_t*
SecurityManager::get_control_block(
    connection_handle_t connection
)
{
    for (auto & cb : _control_blocks) {
        if (!cb.connected) {
            continue;
        } else if (connection == cb.connection) {
            return &cb;
        }
    }
    return nullptr;
}


typename SecurityManager::ControlBlock_t*
SecurityManager::get_control_block(
    const address_t &peer_address
)
{
    MBED_ASSERT(_db);
    for (auto & control_block : _control_blocks) {
        ControlBlock_t *cb = &control_block;
        if (cb->connected) {
            SecurityDistributionFlags_t* flags = _db->get_distribution_flags(cb->db_entry);
            if (flags && (flags->peer_address == peer_address)) {
                return cb;
            }
        }
    }
    return nullptr;
}


typename SecurityManager::ControlBlock_t*
SecurityManager::get_control_block(
    SecurityDb::entry_handle_t db_entry
)
{
    for (auto & cb : _control_blocks) {
        if (!cb.connected) {
            continue;
        } else if (db_entry == cb.db_entry) {
            return &cb;
        }
    }
    return nullptr;
}


void SecurityManager::release_control_block(ControlBlock_t* cb)
{
    *cb = ControlBlock_t();
}

void SecurityManager::onShutdown(const SecurityManagerShutdownCallback_t& callback)
{
    shutdownCallChain.add(callback);
}

template <typename T>
void SecurityManager::onShutdown(T *objPtr, void (T::*memberPtr)(const SecurityManager *))
{
    shutdownCallChain.add(objPtr, memberPtr);
}

SecurityManager::SecurityManagerShutdownCallbackChain_t& SecurityManager::onShutdown()
{
    return shutdownCallChain;
}

void SecurityManager::setSecurityManagerEventHandler(EventHandler* handler)
{
    if (handler) {
        eventHandler = handler;
    } else {
        eventHandler = &defaultEventHandler;
    }
}

} /* namespace impl */
} /* namespace ble */

#endif // BLE_FEATURE_SECURITY
