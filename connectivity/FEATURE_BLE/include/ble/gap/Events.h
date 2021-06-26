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

#ifndef BLE_GAP_EVENTS_H
#define BLE_GAP_EVENTS_H

#include "ble/common/blecommon.h"
#include "ble/common/BLETypes.h"

namespace ble {

/**
 * @addtogroup ble
 * @{
 * @addtogroup gap
 * @{
 */

/**
 * Event generated when an advertising packet is seen during passive scanning
 * or a scan response is received during active scanning.
 *
 * @see ble::Gap::EventHandler::onPeriodicAdvertisingReport()
 */
struct AdvertisingReportEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create an advertising report event.
     *
     * @param type Type of advertising used.
     * @param peerAddressType Peer address type of advertiser.
     * @param peerAddress Peer address of advertiser.
     * @param primaryPhy PHY used on the primary channels.
     * @param secondaryPhy PHY used on secondary channels.
     * @param SID Set identification number.
     * @param txPower Transmission power reported by the packet.
     * @param rssi Measured signal strength.
     * @param periodicInterval Interval of periodic advertising.
     * @param directAddressType Directed advertising target address type.
     * @param directAddress Directed advertising target address.
     * @param advertisingData Advertising payload.
     */
    AdvertisingReportEvent(
        const advertising_event_t &type,
        const peer_address_type_t &peerAddressType,
        const address_t &peerAddress,
        const phy_t &primaryPhy,
        const phy_t &secondaryPhy,
        advertising_sid_t SID,
        advertising_power_t txPower,
        rssi_t rssi,
        uint16_t periodicInterval,
        const peer_address_type_t &directAddressType,
        const address_t &directAddress,
        const mbed::Span<const uint8_t> &advertisingData
    ) :
        peerAddress(peerAddress),
        type(type),
        peerAddressType(peerAddressType),
        primaryPhy(primaryPhy),
        secondaryPhy(secondaryPhy),
        SID(SID),
        txPower(txPower),
        periodicInterval(periodicInterval),
        rssi(rssi),
        directAddressType(directAddressType),
        directAddress(directAddress),
        advertisingData(advertisingData)
    {
    }

#endif
    /** Get event type. */
    const advertising_event_t &getType() const
    {
        return type;
    }

    /** Get peer address type. */
    const peer_address_type_t &getPeerAddressType() const
    {
        return peerAddressType;
    }

    /** Get peer address. */
    const address_t &getPeerAddress() const
    {
        return peerAddress;
    }

    /** Get primary PHY. */
    const phy_t &getPrimaryPhy() const
    {
        return primaryPhy;
    }

    /** Get secondary PHY. */
    const phy_t &getSecondaryPhy() const
    {
        return secondaryPhy;
    }

    /** Get advertising set identifier. */
    advertising_sid_t getSID() const
    {
        return SID;
    }

    /** Get TX power. */
    advertising_power_t getTxPower() const
    {
        return txPower;
    }

    /** Get received signal strength. */
    rssi_t getRssi() const
    {
        return rssi;
    }

    /** Indicate if periodic interval is valid */
    bool isPeriodicIntervalPresent() const {
        return periodicInterval != 0;
    }

    /** Get interval. */
    periodic_interval_t getPeriodicInterval() const
    {
        return periodic_interval_t(periodicInterval);
    }

    /** Get target address type in directed advertising. */
    const peer_address_type_t &getDirectAddressType() const
    {
        return directAddressType;
    }

    /** Get target address in directed advertising. */
    const address_t &getDirectAddress() const
    {
        return directAddress;
    }

    /** Get payload. */
    const mbed::Span<const uint8_t> &getPayload() const
    {
        return advertisingData;
    }

    /** Set peer address. */
    void setPeerAddress(
        const address_t &newPeerAddress
    )
    {
        peerAddress = newPeerAddress;
    }


    /** Set peer address type. */
    void setPeerAddressType(
        const peer_address_type_t &newPeerAddressType
    )
    {
        peerAddressType = newPeerAddressType;
    }

    /** Set new advertising payload. */
    void setAdvertisingData(
        const mbed::Span<const uint8_t> &newAdvertisingData
    )
    {
        advertisingData = newAdvertisingData;
    }

private:
    address_t peerAddress;
    advertising_event_t type;
    peer_address_type_t peerAddressType;
    phy_t primaryPhy;
    phy_t secondaryPhy;
    advertising_sid_t SID;
    advertising_power_t txPower;
    uint16_t periodicInterval;
    rssi_t rssi;
    peer_address_type_t directAddressType;
    address_t directAddress;
    ble_error_t status;
    mbed::Span<const uint8_t> advertisingData;
};

/**
 * Event generated when a connection initiation ends (successfully or not).
 *
 * @see ble::Gap::EventHandler::onConnectionComplete().
 */
struct ConnectionCompleteEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create a connection complete event.
     *
     * @param success BLE_ERROR_NONE if connection succeeded.
     * @param connectionHandle Connection handle if successful.
     * @param ownRole Role of the local device.
     * @param peerAddressType Peer address type.
     * @param peerAddress Peer address.
     * @param localResolvablePrivateAddress Local address type if privacy enabled.
     * @param peerResolvablePrivateAddress Peer address type if privacy enabled.
     * @param connectionInterval Connection interval.
     * @param connectionLatency Connection latency in events.
     * @param supervisionTimeout Supervision timeout.
     * @param masterClockAccuracy Peer clock accuracy in parts per million.
     */
    ConnectionCompleteEvent(
        ble_error_t status,
        connection_handle_t connectionHandle,
        connection_role_t ownRole,
        const peer_address_type_t &peerAddressType,
        const address_t &peerAddress,
        const address_t &localResolvablePrivateAddress,
        const address_t &peerResolvablePrivateAddress,
        conn_interval_t connectionInterval,
        slave_latency_t connectionLatency,
        supervision_timeout_t supervisionTimeout,
        uint16_t masterClockAccuracy
    ) :
        status(status),
        connectionHandle(connectionHandle),
        peerAddress(peerAddress),
        ownRole(ownRole),
        peerAddressType(peerAddressType),
        localResolvablePrivateAddress(localResolvablePrivateAddress),
        connectionInterval(connectionInterval),
        peerResolvablePrivateAddress(peerResolvablePrivateAddress),
        connectionLatency(connectionLatency),
        supervisionTimeout(supervisionTimeout),
        masterClockAccuracy(masterClockAccuracy)
    {
    }

#endif

    /** Get connection complete event status. */
    ble_error_t getStatus() const
    {
        return status;
    }

    /** Get connection handle (valid only when successful). */
    connection_handle_t getConnectionHandle() const
    {
        return connectionHandle;
    }

    /** Get own role. */
    connection_role_t getOwnRole() const
    {
        return ownRole;
    }

    /** Get peer address type. */
    const peer_address_type_t &getPeerAddressType() const
    {
        return peerAddressType;
    }

    /** Get peer address. */
    const address_t &getPeerAddress() const
    {
        return peerAddress;
    }

    /** Get get local resolvable random address if privacy is used. */
    const address_t &getLocalResolvablePrivateAddress() const
    {
        return localResolvablePrivateAddress;
    }

    /** Get peer resolvable private address if privacy is used. */
    const address_t &getPeerResolvablePrivateAddress() const
    {
        return peerResolvablePrivateAddress;
    }

    /** Get connection interval. */
    conn_interval_t getConnectionInterval() const
    {
        return connectionInterval;
    }

    /** Get connection latency. */
    slave_latency_t getConnectionLatency() const
    {
        return connectionLatency;
    }

    /** Get supervision timeout. */
    supervision_timeout_t getSupervisionTimeout() const
    {
        return supervisionTimeout;
    }

    /** Get clock accuracy in parts per million. */
    uint16_t getMasterClockAccuracy() const
    {
        return masterClockAccuracy;
    }


    /** Set connection complete event status. */
    void setStatus(ble_error_t new_status)
    {
        status = new_status;
    }

    /** Set peer address type. */
    void setPeerAddressType(const peer_address_type_t& address_type)
    {
        peerAddressType = address_type;
    }

    /** Set peer address. */
    void setPeerAddress(const address_t &address)
    {
        peerAddress = address;
    }

    /** Set get local resolvable random address if privacy is used. */
    void setLocalResolvablePrivateAddress(const address_t &address)
    {
        localResolvablePrivateAddress = address;
    }

    /** Set peer resolvable private address if privacy is used. */
    void setPeerResolvablePrivateAddress(const address_t &address)
    {
        peerResolvablePrivateAddress = address;
    }

private:
    ble_error_t status;
    connection_handle_t connectionHandle;
    address_t peerAddress;
    connection_role_t ownRole;
    peer_address_type_t peerAddressType;
    address_t localResolvablePrivateAddress;
    conn_interval_t connectionInterval;
    address_t peerResolvablePrivateAddress;
    slave_latency_t connectionLatency;
    supervision_timeout_t supervisionTimeout;
    uint16_t masterClockAccuracy;
};

/**
 * Event generated when you first receive a periodic advertisement.
 *
 * @see ble::Gap::EventHandler::onPeriodicAdvertisingSyncEstablished().
 */
struct PeriodicAdvertisingSyncEstablishedEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create advertising sync event.
     *
     * @param success BLE_ERROR_NONE if synchronisation was achieved.
     * @param syncHandle Advertising sync handle.
     * @param sid Advertising set identifier.
     * @param peerAddressType Peer address type.
     * @param peerAddress Peer address.
     * @param peerPhy PHY used for advertisements.
     * @param advertisingInterval Periodic advertising interval.
     * @param masterClockAccuracy Peer clock accuracy in parts per million.
     */
    PeriodicAdvertisingSyncEstablishedEvent(
        ble_error_t status,
        periodic_sync_handle_t syncHandle,
        advertising_sid_t sid,
        const peer_address_type_t &peerAddressType,
        const address_t &peerAddress,
        const phy_t &peerPhy,
        uint16_t advertisingInterval,
        const clock_accuracy_t &peerClockAccuracy
    ) :
        status(status),
        syncHandle(syncHandle),
        sid(sid),
        peerAddressType(peerAddressType),
        peerAddress(peerAddress),
        peerPhy(peerPhy),
        advertisingInterval(advertisingInterval),
        peerClockAccuracy(peerClockAccuracy)
    {
    }

#endif

    /** Get sync establishment status. */
    ble_error_t getStatus() const
    {
        return status;
    }

    /** Get periodic advertising sync handle. */
    periodic_sync_handle_t getSyncHandle() const
    {
        return syncHandle;
    }

    /** Get advertising set identifier. */
    advertising_sid_t getSid() const
    {
        return sid;
    }

    /** Get peer address type. */
    const peer_address_type_t &getPeerAddressType() const
    {
        return peerAddressType;
    }

    /** Get peer address. */
    const address_t &getPeerAddress() const
    {
        return peerAddress;
    }

    /** Get PHY used. */
    const phy_t &getPeerPhy() const
    {
        return peerPhy;
    }

    /** Get interval. */
    uint16_t getAdvertisingInterval() const
    {
        return advertisingInterval;
    }

    /** Get clock accuracy in parts per million. */
    const clock_accuracy_t &getPeerClockAccuracy() const
    {
        return peerClockAccuracy;
    }

private:
    ble_error_t status;
    periodic_sync_handle_t syncHandle;
    advertising_sid_t sid;
    peer_address_type_t peerAddressType;
    const address_t &peerAddress;
    phy_t peerPhy;
    uint16_t advertisingInterval;
    clock_accuracy_t peerClockAccuracy;
};

/**
 * Event generated when periodic advertising packet is received.
 *
 * @see ble::Gap::EventHandler::onPeriodicAdvertisingReport().
 */
struct PeriodicAdvertisingReportEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create periodic advertising report event.
     *
     * @param syncHandle Periodic advertising sync handle
     * @param txPower TX power.
     * @param rssi Received signal strength.
     * @param dataStatus Status to indicate the completeness of the payload.
     * @param payload Periodic advertisement payload.
     */
    PeriodicAdvertisingReportEvent(
        periodic_sync_handle_t syncHandle,
        advertising_power_t txPower,
        rssi_t rssi,
        advertising_data_status_t dataStatus,
        const mbed::Span<const uint8_t> &payload
    ) :
        syncHandle(syncHandle),
        txPower(txPower),
        rssi(rssi),
        dataStatus(dataStatus),
        payload(payload)
    {
    }

#endif

    /** Get periodic advertising sync handle. */
    periodic_sync_handle_t getSyncHandle() const
    {
        return syncHandle;
    }

    /** Get TX power as reported by the advertising packet. */
    advertising_power_t getTxPower() const
    {
        return txPower;
    }

    /** Get received signal strength. */
    rssi_t getRssi() const
    {
        return rssi;
    }

    /** Get data completeness status. */
    const advertising_data_status_t &getDataStatus() const
    {
        return dataStatus;
    }

    /** Get payload. */
    const mbed::Span<const uint8_t> &getPayload() const
    {
        return payload;
    }

private:
    periodic_sync_handle_t syncHandle;
    advertising_power_t txPower;
    rssi_t rssi;
    advertising_data_status_t dataStatus;
    mbed::Span<const uint8_t> payload;
};

/**
 * Event generated when periodic advertising sync is lost.
 *
 * @see ble::Gap::EventHandler::onPeriodicAdvertisingSyncLoss().
 */
struct PeriodicAdvertisingSyncLoss {
#if !defined(DOXYGEN_ONLY)

    /** Create periodic advertising sync loss event.
     *
     * @param syncHandle Periodic advertising sync handle.
     */
    PeriodicAdvertisingSyncLoss(
        periodic_sync_handle_t syncHandle
    ) :
        syncHandle(syncHandle)
    {
    }

#endif

    /** Get periodic sync handle. */
    periodic_sync_handle_t getSyncHandle() const
    {
        return syncHandle;
    }

private:
    periodic_sync_handle_t syncHandle;
};

/**
 * Event generated when scan times out.
 *
 * @see ble::Gap::EventHandler::onScanTimeout().
 */
struct ScanTimeoutEvent { };

/**
 * Event produced when advertising start.
 *
 * @see ble::Gap::EventHandler::onAdvertisingStart().
 */
struct AdvertisingStartEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create an advertising start event.
     *
     * @param advHandle Advertising set handle.
     * @param status Advertising set start command status.
     */
    AdvertisingStartEvent(advertising_handle_t advHandle, ble_error_t status = BLE_ERROR_NONE) :
        advHandle(advHandle), status(status)
    {
    }

#endif

    /** Get advertising handle. */
    advertising_handle_t getAdvHandle() const
    {
        return advHandle;
    }

    /** Get status of operation. */
    ble_error_t getStatus() const
    {
        return status;
    }

private:
    advertising_handle_t advHandle;
    ble_error_t status;
};

/**
 * Event produced when advertising ends.
 *
 * @see ble::Gap::EventHandler::onAdvertisingEnd().
 *
 * @note The connection handle, connected flag and completed_event fields are
 * valid if the flag legacy is not set to true. If status is different from BLE_ERROR_NONE
 * the completed_events field is not valid and the set may still be active.
 */
struct AdvertisingEndEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create an extended advertising end event.
     *
     * @param advHandle Advertising set handle.
     * @param connection Connection handle - only valid if connected is True.
     * @param completed_events Number of events created during before advertising end - only valid
     *        if advertising end has been caused by BLE_ERROR_LIMIT_REACHED, not the local user.
     *        Check getStatus().
     * @param connected True if connection has been established.
     * @param status Error code showing the reason for event. BLE_ERROR_LIMIT_REACHED if set number
     *        of events have been reached. BLE_ERROR_TIMEOUT if set time has elapsed.
     *        BLE_ERROR_SUCCESS if connection occurred or user ended the set. Check isConnected()
     *        to determine which.
     */
    AdvertisingEndEvent(
        advertising_handle_t advHandle,
        connection_handle_t connection,
        uint8_t completed_events,
        bool connected,
        ble_error_t status = BLE_ERROR_NONE
    ) :
        advHandle(advHandle),
        connection(connection),
        completed_events(completed_events),
        connected(connected),
        legacy(false),
        status(status)
    {
    }

    /** Create a legacy advertising end event.
     */
    AdvertisingEndEvent() :
        advHandle(LEGACY_ADVERTISING_HANDLE),
        connection(),
        completed_events(0),
        connected(false),
        legacy(true),
        status(BLE_ERROR_NONE)
    {
    }

#endif

    /** Get advertising handle. */
    advertising_handle_t getAdvHandle() const
    {
        return advHandle;
    }

    /** Get connection handle (valid only if connected successfully). */
    connection_handle_t getConnection() const
    {
        return connection;
    }

    /** Get how many events advertising created. */
    uint8_t getCompleted_events() const
    {
        return completed_events;
    }

    /** Has the advertising ended with a connection. */
    bool isConnected() const
    {
        return connected;
    }

    /** Is the end of legacy advertising.
     *
     * If it is the return of getConnection() getCompleted_events() and isConnected()
     * must be discarded
     */
    bool isLegacy() const
    {
        return legacy;
    }

    /** Get the result of the stop advertising event. If the status is not BLE_ERROR_NONE the set
     * may still be active. */
    ble_error_t getStatus() const
    {
        return status;
    }

private:
    advertising_handle_t advHandle;
    connection_handle_t connection;
    uint8_t completed_events;
    bool connected;
    bool legacy;
    ble_error_t status;
};

/**
 * Event produced when a peer requests a scan response from the advertiser.
 *
 * @see ble::Gap::EventHandler::onScanRequestReceived().
 */
struct ScanRequestEvent {
#if !defined(DOXYGEN_ONLY)

    /** Create scan request event.
     *
     * @param advHandle Advertising handle.
     * @param peerAddressType Peer address type.
     * @param peerAddress Peer address.
     */
    ScanRequestEvent(
        advertising_handle_t advHandle,
        const peer_address_type_t &peerAddressType,
        const address_t &peerAddress
    ) :
        advHandle(advHandle),
        peerAddressType(peerAddressType),
        peerAddress(peerAddress)
    {
    }

#endif

    /** Get advertising handle. */
    advertising_handle_t getAdvHandle() const
    {
        return advHandle;
    }

    /** Get peer address type. */
    const peer_address_type_t &getPeerAddressType() const
    {
        return peerAddressType;
    }

    /** Get peer address. */
    const address_t &getPeerAddress() const
    {
        return peerAddress;
    }

private:
    advertising_handle_t advHandle;
    peer_address_type_t peerAddressType;
    const address_t &peerAddress;
};

/**
 * Event produced when a disconnection is complete.
 *
 * @see ble::Gap::EventHandler::onDisconnectionComplete().
 */
struct DisconnectionCompleteEvent {
#if !defined(DOXYGEN_ONLY)

    DisconnectionCompleteEvent(
        connection_handle_t connectionHandle,
        const disconnection_reason_t &reason
    ) :
        connectionHandle(connectionHandle), reason(reason)
    {
    }

#endif

    /**
     * Get the handle of the connection that has expired.
     */
    connection_handle_t getConnectionHandle() const
    {
        return connectionHandle;
    }

    /**
     * Get the reason of the disconnection.
     */
    const disconnection_reason_t &getReason() const
    {
        return reason;
    }

private:
    ble::connection_handle_t connectionHandle;
    ble::disconnection_reason_t reason;
};

/**
 * Event received when a peer wants to change the connection parameters.
 *
 * @see ble::Gap::EventHandler::onUpdateConnectionParametersRequest().
 */
struct UpdateConnectionParametersRequestEvent {
#if !defined(DOXYGEN_ONLY)

    UpdateConnectionParametersRequestEvent(
        connection_handle_t connectionHandle,
        const conn_interval_t &minConnectionInterval,
        const conn_interval_t &maxConnectionInterval,
        const slave_latency_t &slaveLatency,
        const supervision_timeout_t &supervision_timeout
    ) :
        connectionHandle(connectionHandle),
        minConnectionInterval(minConnectionInterval),
        maxConnectionInterval(maxConnectionInterval),
        slaveLatency(slaveLatency),
        supervisionTimeout(supervision_timeout)
    {
    }

#endif

    /**
     * Get the connection handle.
     */
    connection_handle_t getConnectionHandle() const
    {
        return connectionHandle;
    }

    /**
     * Get the minimum connection interval requested.
     */
    const conn_interval_t &getMinConnectionInterval() const
    {
        return minConnectionInterval;
    }

    /**
     * Get the maximum connection interval requested.
     */
    const conn_interval_t &getMaxConnectionInterval() const
    {
        return maxConnectionInterval;
    }

    /**
     * Get the slave latency requested.
     */
    const slave_latency_t &getSlaveLatency() const
    {
        return slaveLatency;
    }

    /**
     * Get the supervision timeout requested.
     */
    const supervision_timeout_t &getSupervisionTimeout() const
    {
        return supervisionTimeout;
    }

private:
    ble::connection_handle_t connectionHandle;
    ble::conn_interval_t minConnectionInterval;
    ble::conn_interval_t maxConnectionInterval;
    ble::slave_latency_t slaveLatency;
    ble::supervision_timeout_t supervisionTimeout;
};

/**
 * Event received when connection parameters have been updated.
 *
 * @see ble::Gap::EventHandler::onConnectionParametersUpdateComplete().
 */
struct ConnectionParametersUpdateCompleteEvent {
#if !defined(DOXYGEN_ONLY)

    ConnectionParametersUpdateCompleteEvent(
        ble_error_t status,
        connection_handle_t connectionHandle,
        const conn_interval_t &connectionInterval,
        const slave_latency_t &slaveLatency,
        const supervision_timeout_t &supervisionTimeout
    ) :
        status(status),
        connectionHandle(connectionHandle),
        connectionInterval(connectionInterval),
        slaveLatency(slaveLatency),
        supervisionTimeout(supervisionTimeout)
    {
    }

#endif

    /**
     * Get the status of the operation. It is equal to BLE_ERROR_NONE in case of
     * success.
     */
    ble_error_t getStatus() const
    {
        return status;
    }

    /**
     * Get the handle of the connection that has been updated.
     */
    connection_handle_t getConnectionHandle() const
    {
        return connectionHandle;
    }

    /**
     * Get the new connection interval.
     */
    const conn_interval_t &getConnectionInterval() const
    {
        return connectionInterval;
    }

    /**
     * Get the new slave latency.
     */
    const slave_latency_t &getSlaveLatency() const
    {
        return slaveLatency;
    }

    /**
     * Get the new supervision timeout.
     */
    const supervision_timeout_t &getSupervisionTimeout() const
    {
        return supervisionTimeout;
    }

private:
    ble_error_t status;
    ble::connection_handle_t connectionHandle;
    ble::conn_interval_t connectionInterval;
    ble::slave_latency_t slaveLatency;
    ble::supervision_timeout_t supervisionTimeout;

};

/**
 * @}
 * @}
 */

} // namespace ble

#endif //BLE_GAP_EVENTS_H
