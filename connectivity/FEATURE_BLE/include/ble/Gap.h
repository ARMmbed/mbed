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

#ifndef BLE_GAP_GAP_H
#define BLE_GAP_GAP_H

#include "ble/common/CallChainOfFunctionPointersWithContext.h"

#include "ble/common/BLERoles.h"
#include "ble/common/BLETypes.h"
#include "ble/gap/AdvertisingDataBuilder.h"
#include "ble/gap/AdvertisingDataParser.h"
#include "ble/gap/AdvertisingDataSimpleBuilder.h"
#include "ble/gap/AdvertisingDataTypes.h"
#include "ble/gap/AdvertisingParameters.h"
#include "ble/gap/ConnectionParameters.h"
#include "ble/gap/ScanParameters.h"
#include "ble/gap/Events.h"
#include "ble/gap/Types.h"

namespace ble {

#if !defined(DOXYGEN_ONLY)
namespace impl {
class Gap;
}
#endif // !defined(DOXYGEN_ONLY)

/**
 * @addtogroup ble
 * @{
 * @addtogroup gap
 * @{
 */

/**
 * Define device discovery, connection and link management procedures.
 *
 * - Device discovery: A device can advertise to nearby peers its existence,
 * identity and capabilities. Similarly, a device can scan its environment to
 * find advertising peers. The information acquired during the scan helps to
 * identify peers and understand their use. A scanner may acquire more information
 * about an advertising peer by sending a scan request. If the peer accepts scan
 * requests, it may reply with additional information about its state.
 *
 * - Connection: A bluetooth device can establish a connection to a connectable
 * advertising peer. Once the connection is established, both devices can
 * communicate using the GATT protocol. The GATT protocol allows connected
 * devices to expose a set of states that the other peer can discover, read and write.
 *
 * - Link Management: Connected devices may drop the connection and may adjust
 * connection parameters according to the power envelop needed for their
 * application.
 *
 * @par Accessing gap
 *
 * Instance of a Gap class for a given BLE device should be accessed using
 * BLE::gap(). The reference returned remains valid until the BLE instance
 * shut down (see BLE::shutdown()).
 *
 * @code
 * // assuming ble_device has been initialized
 * BLE& ble_device;
 *
 * ble::Gap& gap = ble_device.gap();
 * @endcode
 *
 * @par Advertising
 *
 * Advertising consists of broadcasting at a regular interval a small amount of
 * data containing valuable information about the device. These packets may be
 * scanned by peer devices listening on BLE advertising channels.
 *
 * Scanners may also request additional information from a device advertising by
 * sending a scan request. If the broadcaster accepts scan requests, it can reply
 * with a scan response packet containing additional information.
 *
 * Advertising parameters are updated using setAdvertisingParams(). The main
 * advertising payload is updated using setAdvertisingPayload(), and the scan response
 * is updated using setAdvertisingScanResponse(). If the advertising is already
 * updated, the data will take effect from the next advertising event.
 *
 * To create a valid advertising payload and scan response, you may use
 * AdvertisingDataBuilder. You must first allocate memory and create an mbed::Span and
 * pass that into the AdvertisingDataBuilder, which will only be able to add as much
 * data as fits in the provided buffer. The builder accepts any size of the buffer,
 * but for the created data to be usable, it must be smaller than the maximum data
 * length returned from getMaxAdvertisingDataLength().
 *
 * Another option is to use AdvertisingDataSimpleBuilder, which allocates memory
 * on the stack and offers a fluent interface at the expense of a reduced set of
 * APIs and error management options.
 *
 * @note Prior to Bluetooth 5, advertising and scanning payload size were limited
 * to LEGACY_ADVERTISING_MAX_SIZE. It changed with Bluetooth 5, and now the maximum
 * size of data that can be advertised depends on the controller. If  you wish
 * to be compatible with older devices, you may wish to advertise with the
 * LEGACY_ADVERTISING_HANDLE. This uses payloads no larger than LEGACY_ADVERTISING_MAX_SIZE
 * with that advertising set.
 *
 * @par Extended advertising
 *
 * Extended advertising allows for a wider choice of options than legacy advertising.
 * You can send bigger payloads and use different PHYs. This allows for bigger throughput
 * or longer range.
 *
 * Extended advertising may be split across many packets and takes place on both the
 * regular advertising channels and the rest of the 37 channels normally used by
 * connected devices.
 *
 * The 3 channels used in legacy advertising are called primary advertisement channels.
 * The remaining 37 channels are used for secondary advertising. Unlike sending data
 * during a connection, this allows the device to broadcast data to multiple devices.
 *
 * The advertising starts on the primary channels (which you may select) and continues
 * on the secondary channels as indicated in the packet sent on the primary channel.
 * This way, the advertising can send large payloads without saturating the advertising
 * channels. Primary channels are limited to 1M and coded PHYs, but secondary channels
 * may use the increased throughput 2M PHY.
 *
 * @par Periodic advertising
 *
 * Similarly, you can use periodic advertising to transfer regular data to multiple
 * devices.
 *
 * The advertiser uses primary channels to advertise the information needed to
 * listen to the periodic advertisements on secondary channels. This sync information
 * will be used by the scanner who can now optimize for power consumption and only
 * listen for the periodic advertisements at specified times.
 *
 * Like extended advertising, periodic advertising offers extra PHY options of 2M
 * and coded. The payload may be updated at any time and will be updated on the next
 * advertisement event when the periodic advertising is active.
 *
 * @par Advertising sets
 *
 * Advertisers may advertise multiple payloads at the same time. The configuration
 * and identification of these is done through advertising sets. Use a handle
 * obtained from createAvertisingSet() for advertising operations. After ending
 * all advertising operations, remove the handle from the system using
 * destroyAdvertisingHandle().
 *
 * Extended advertising and periodic advertising is an optional feature, and not all
 * devices support it. Some will only be able to see the now-called legacy advertising.
 *
 * Legacy advertising is available through a special handle, LEGACY_ADVERTISING_HANDLE.
 * This handle is always available, doesn't need to be created and can't be
 * destroyed.
 *
 * There is a limited number of advertising sets available because they require support
 * from the controller. Their availability is dynamic and may be queried at any time
 * using getMaxAdvertisingSetNumber(). Advertising sets take up resources even if
 * they are not actively advertising right now, so it's important to destroy the set
 * when you're done with it (or reuse it in the next advertisement).
 *
 * Periodic advertising and extended advertising share the same set but not the same
 * data. Extended advertising carries out periodic advertising synchronization
 * information. Therefore, to let other devices be aware that your device
 * exposes periodic advertising, you should start extended advertising of the set.
 * Subsequently, you may disable extended advertising, and the periodic advertising
 * will continue. If you start periodic advertising while extended advertising is
 * inactive, periodic advertising won't start until you start extended advertising
 * at a later time.
 *
 * @par Privacy
 *
 * Privacy is a feature that allows a device to avoid being tracked by other
 * (untrusted) devices. The device achieves it by periodically generating a
 * new random address. The random address may be a resolvable random address,
 * enabling trusted devices to recognize it as belonging to the same
 * device. These trusted devices receive an Identity Resolution Key (IRK)
 * during pairing. This is handled by the SecurityManager and relies on the
 * other device accepting and storing the IRK.
 *
 * You need to enable privacy by calling enablePrivacy() after having
 * initialized the SecurityManager because privacy requires SecurityManager
 * to handle IRKs. The behavior of privacy enabled devices is set by
 * using setCentralPrivacyConfiguration(), which specifies what the device
 * should be with devices using random addresses. Random addresses
 * generated by privacy enabled devices can be of two types: resolvable
 * (by devices who have the IRK) and unresolvable. Unresolvable addresses
 * can't be used for connecting and connectable advertising. Therefore, a
 * resolvable one will be used for these regardless of the privacy
 * configuration.
 *
 * @par Scanning
 *
 * Scanning consists of listening for peer advertising packets. From a scan, a
 * device can identify devices available in its environment.
 *
 * If the device scans actively, then it will send scan request to scannable
 * advertisers and collect their scan responses.
 *
 * Scanning is done by creating ScanParameters and applying them with
 * setScanParameters(). One configured, you may call startScan().
 *
 * When a scanning device receives an advertising packet, it will call
 * onAdvertisingReport() in the registered event handler. A whitelist may be used
 * to limit the advertising reports by setting the correct policy in the scan
 * parameters.
 *
 * @par Connection event handling
 *
 * A peer may connect device advertising connectable packets. The advertising
 * procedure ends as soon as the device is connected. If an advertising timeout
 * has been set in the advertising parameters then onAdvertisingEnd will be called
 * in the registered eventHandler when it runs out.
 *
 * A device accepting a connection request from a peer is named a peripheral,
 * and the device initiating the connection is named a central.
 *
 * Connection is initiated by central devices. A call to connect() will result in
 * the device scanning on any PHYs set in ConectionParamters passed in.
 *
 * Peripheral and central receive a connection event when the connection is
 * effective. If successful will result in a call to onConnectionComplete in the
 * EventHandler registered with the Gap.
 *
 * It the connection attempt fails it will result in onConnectionComplete called
 * on the central device with the event carrying the error flag.
 *
 * @par Changing the PHYsical transport of a connection
 *
 * Once a connection has been established, it is possible to change the physical
 * transport used between the local and the distant device. Changing the transport
 * can either increase the bandwidth or increase the communication range.
 * An increased bandwidth equals a better power consumption but also a loss in
 * sensibility and therefore a degraded range.
 *
 * Symmetrically an increased range means a lowered bandwidth and a degraded power
 * consumption.
 *
 * Applications can change the PHY used by calling the function setPhy. Once the
 * update has been made the result is forwarded to the application by calling the
 * function onPhyUpdateComplete of the event handler registered.
 *
 * @par disconnection
 *
 * The application code initiates a disconnection when it calls the
 * disconnect(Handle_t, DisconnectionReason_t) function.
 *
 * Disconnection may also be initiated by the remote peer or the local
 * controller/stack. To catch all disconnection events, application code may
 * set up an handler taking care of disconnection events by calling
 * onDisconnection().
 *
 * @par Modulation Schemes
 *
 * When supported by the host and controller you can select different modulation
 * schemes (@see BLUETOOTH SPECIFICATION Version 5.0 | Vol 1, Part A - 1.2):
 * - LE 1M PHY
 * - LE 2M PHY
 * - LE coded PHY
 *
 * You may set preferred PHYs (separately for RX and TX) using setPreferredPhys().
 * You may also set the currently used PHYs on a selected connection using setPhy().
 * Both of these settings are only advisory and the controller is allowed to make
 * its own decision on the best PHY to use based on your request, the peer's
 * supported features and the connection's physical conditions.
 *
 * You may query the currently used PHY using readPhy() which will return the
 * result through a call to the registered event handler. You may register the
 * handler with setEventHandler(). The events inform about the currently used
 * PHY and of any changes to PHYs which may be triggered autonomously by the
 * controller or by the peer.
 */
class Gap {
public:
    /**
     * Gap shutdown event handler.
     *
     * @see Gap::onShutdown().
     */
    typedef FunctionPointerWithContext<const Gap *> GapShutdownCallback_t;

    /**
     * Callchain of gap shutdown event handler.
     *
     * @see Gap::onShutdown().
     */
    typedef CallChainOfFunctionPointersWithContext<const Gap *>
        GapShutdownCallbackChain_t;

public:
    /**
     * Definition of the general handler of Gap related events.
     */
    struct EventHandler {
        /**
         * Called when an advertising device receive a scan response.
         *
         * @param event Scan request event.
         *
         * @version: 5+.
         *
         * @see AdvertisingParameters::setScanRequestNotification().
         */
        virtual void onScanRequestReceived(const ScanRequestEvent &event)
        {
        }

        /**
         * Called when advertising starts.
         *
         * @param event Advertising start event.
         *
         * @see startAdvertising()
         */
        virtual void onAdvertisingStart(const AdvertisingStartEvent &event)
        {
        }

        /**
         * Called when advertising ends.
         *
         * Advertising ends when the process timeout or if it is stopped by the
         * application or if the local device accepts a connection request.
         *
         * @param event Advertising end event.
         *
         * @see startAdvertising()
         * @see stopAdvertising()
         * @see onConnectionComplete()
         */
        virtual void onAdvertisingEnd(const AdvertisingEndEvent &event)
        {
        }

        /**
         * Called when a scanner receives an advertising or a scan response packet.
         *
         * @param event Advertising report.
         *
         * @see startScan()
         */
        virtual void onAdvertisingReport(const AdvertisingReportEvent &event)
        {
        }

        /**
         * Called when scan times out.
         *
         * @param event Associated event.
         *
         * @see startScan()
         */
        virtual void onScanTimeout(const ScanTimeoutEvent &event)
        {
        }

        /**
         * Called when first advertising packet in periodic advertising is received.
         *
         * @param event Periodic advertising sync event.
         *
         * @version: 5+.
         *
         * @see createSync()
         */
        virtual void onPeriodicAdvertisingSyncEstablished(
            const PeriodicAdvertisingSyncEstablishedEvent &event
        )
        {
        }

        /**
         * Called when a periodic advertising packet is received.
         *
         * @param event Periodic advertisement event.
         *
         * @version: 5+.
         *
         * @see createSync()
         */
        virtual void onPeriodicAdvertisingReport(
            const PeriodicAdvertisingReportEvent &event
        )
        {
        }

        /**
         * Called when a periodic advertising sync has been lost.
         *
         * @param event Details of the event.
         *
         * @version: 5+.
         *
         * @see createSync()
         */
        virtual void onPeriodicAdvertisingSyncLoss(
            const PeriodicAdvertisingSyncLoss &event
        )
        {
        }

        /**
         * Called when connection attempt ends or an advertising device has been
         * connected.
         *
         * @see startAdvertising()
         * @see connect()
         *
         * @param event Connection event.
         */
        virtual void onConnectionComplete(const ConnectionCompleteEvent &event)
        {
        }

        /**
         * Called when the peer request connection parameters updates.
         *
         * Application must accept the update with acceptConnectionParametersUpdate()
         * or reject it with rejectConnectionParametersUpdate().
         *
         * @param event The connection parameters requested by the peer.
         *
         * @version 4.1+.
         *
         * @note This event is not generated if connection parameters update
         * is managed by the middleware.
         *
         * @see manageConnectionParametersUpdateRequest()
         * @see acceptConnectionParametersUpdate()
         * @see rejectConnectionParametersUpdate()
         */
        virtual void onUpdateConnectionParametersRequest(
            const UpdateConnectionParametersRequestEvent &event
        )
        {
        }

        /**
         * Called when connection parameters have been updated.
         *
         * @param event The new connection parameters.
         *
         * @see updateConnectionParameters()
         * @see acceptConnectionParametersUpdate()
         */
        virtual void onConnectionParametersUpdateComplete(
            const ConnectionParametersUpdateCompleteEvent &event
        )
        {
        }

        /**
         * Called when a connection has been disconnected.
         *
         * @param event Details of the event.
         *
         * @see disconnect()
         */
        virtual void onDisconnectionComplete(const DisconnectionCompleteEvent &event)
        {
        }

        /**
         * Function invoked when the current transmitter and receiver PHY have
         * been read for a given connection.
         *
         * @param status Status of the operation: BLE_ERROR_NONE in case of
         * success or an appropriate error code.
         *
         * @param connectionHandle: The handle of the connection for which the
         * PHYs have been read.
         *
         * @param txPhy PHY used by the transmitter.
         *
         * @param rxPhy PHY used by the receiver.
         *
         * @see readPhy().
         *
         * @version: 5+.
         */
        virtual void onReadPhy(
            ble_error_t status,
            connection_handle_t connectionHandle,
            phy_t txPhy,
            phy_t rxPhy
        )
        {
        }

        /**
         * Function invoked when the update process of the PHY has been completed.
         *
         * The process can be initiated by a call to the function setPhy, the
         * local bluetooth subsystem or the peer.
         *
         * @param status Status of the operation: BLE_ERROR_NONE in case of
         * success or an appropriate error code.
         *
         * @param connectionHandle: The handle of the connection on which the
         * operation was made.
         *
         * @param txPhy PHY used by the transmitter.
         *
         * @param rxPhy PHY used by the receiver.
         *
         * @note Success doesn't mean the PHY has been updated it means both
         * ends have negotiated the best PHY according to their configuration and
         * capabilities. The PHY currently used are present in the txPhy and
         * rxPhy parameters.
         *
         * @see setPhy()
         *
         * @version: 5+.
         */
        virtual void onPhyUpdateComplete(
            ble_error_t status,
            connection_handle_t connectionHandle,
            phy_t txPhy,
            phy_t rxPhy
        )
        {
        }

        /**
         * Function invoked when the connections changes the maximum number of octets
         * that can be sent or received by the controller in a single packet. A single
         * L2CAP packet can be fragmented across many such packets.
         *
         * @note This only triggers if controller supports data length extension and
         * negotiated data length is longer than the default 23.
         *
         * @param connectionHandle The handle of the connection that changed the size.
         * @param txSize Number of octets we can send on this connection in a single packet.
         * @param rxSize Number of octets we can receive on this connection in a single packet.
         */
        virtual void onDataLengthChange(
            connection_handle_t connectionHandle,
            uint16_t txSize,
            uint16_t rxSize
        )
        {
        }

        /**
         * Function invoked when the privacy subsystem has been enabled and is
         * ready to be used.
         */
        virtual void onPrivacyEnabled()
        {
        }
    protected:
        /**
         * Prevent polymorphic deletion and avoid unnecessary virtual destructor
         * as the Gap class will never delete the instance it contains.
         */
        ~EventHandler() = default;
    };

    /**
     * Preferred connection parameter display in Generic Access Service.
     */
    typedef struct {
        /**
         * Minimum interval between two connection events allowed for a
         * connection.
         *
         * It shall be less than or equal to maxConnectionInterval. This value,
         * in units of 1.25ms, is included in the range [0x0006 : 0x0C80].
         */
        uint16_t minConnectionInterval;

        /**
         * Maximum interval between two connection events allowed for a
         * connection.
         *
         * It shall be greater than or equal to minConnectionInterval. This
         * value is in unit of 1.25ms and is in the range [0x0006 : 0x0C80].
         */
        uint16_t maxConnectionInterval;

        /**
         * Number of connection events the slave can drop if it has nothing to
         * communicate to the master.
         *
         * This value shall be in the range [0x0000 : 0x01F3].
         */
        uint16_t slaveLatency;

        /**
         * Link supervision timeout for the connection.
         *
         * Time after which the connection is considered lost if the device
         * didn't receive a packet from its peer.
         *
         * It is larger than:
         *        (1 + slaveLatency) * maxConnectionInterval * 2
         *
         * This value is in the range [0x000A : 0x0C80] and is in unit of
         * 10 ms.
         *
         * @note maxConnectionInterval is in ms in the formulae above.
         */
        uint16_t connectionSupervisionTimeout;
    } PreferredConnectionParams_t;

    /**
     * Assign the event handler implementation that will be used by the gap
     * module to signal events back to the application.
     *
     * @param handler Application implementation of an EventHandler.
     *
     * @note Multiple discrete EventHandler instances may be used by adding them
     * to a ChainableGapEventHandler and then setting the chain as the primary
     * Gap EventHandler using this function.
     *
     * @see ChainableGapEventHandler
     */
    void setEventHandler(EventHandler *handler);

    /** Check controller support for a specific feature.
     *
     * @param feature Feature to check.
     * @return True if feature is supported.
     */
    bool isFeatureSupported(controller_supported_features_t feature);

    /*                                     advertising                                           */
#if BLE_ROLE_BROADCASTER
    /** Return currently available number of supported advertising sets.
     *  This may change at runtime.
     *
     * @note Devices that do not support Bluetooth 5 still offers one advertising
     * set that has the handle LEGACY_ADVERTISING_HANDLE.
     *
     * @return Number of advertising sets that may be created at the same time.
     */
    uint8_t getMaxAdvertisingSetNumber();

    /** Return maximum advertising data length supported.
     *
     * @return Maximum advertising data length supported.
     */
    uint16_t getMaxAdvertisingDataLength();

    /** Return maximum advertising data length supported for connectable advertising.
     *
     * @return Maximum advertising data length supported for connectable advertising.
     */
    uint16_t getMaxConnectableAdvertisingDataLength();

    /** Return maximum advertising data length you may set if advertising set is active.
     *
     * @return Maximum advertising data length you may set if advertising set is active.
     */
    uint16_t getMaxActiveSetAdvertisingDataLength();

#if BLE_FEATURE_EXTENDED_ADVERTISING
    /** Create an advertising set and apply the passed in parameters. The handle returned
     *  by this function must be used for all other calls that accept an advertising handle.
     *  When done with advertising, remove from the system using destroyAdvertisingSet().
     *
     * @note The exception is the LEGACY_ADVERTISING_HANDLE which may be used at any time.
     *
     * @param[out] handle Advertising handle returned, valid only if function returned success.
     * @param parameters Advertising parameters for the newly created set.
     * @return BLE_ERROR_NONE on success.
     *
     * @version 5+
     */
    ble_error_t createAdvertisingSet(
        advertising_handle_t *handle,
        const AdvertisingParameters &parameters
    );

    /** Remove the advertising set (resets its set parameters). The advertising set must not
     *  be active.
     *
     * @note LEGACY_ADVERTISING_HANDLE may not be destroyed.
     *
     * @param handle Advertising set handle.
     * @return BLE_ERROR_NONE on success.
     *
     * @version 5+
     */
    ble_error_t destroyAdvertisingSet(advertising_handle_t handle);
#endif // BLE_FEATURE_EXTENDED_ADVERTISING

    /** Set advertising parameters of an existing set.
     *
     * @param handle Advertising set handle.
     * @param params New advertising parameters.
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t setAdvertisingParameters(
        advertising_handle_t handle,
        const AdvertisingParameters &params
    );

    /** Set new advertising payload for a given advertising set.
     *
     * @param handle Advertising set handle.
     * @param payload Advertising payload.
     *
     * @note If advertising set is active you may only set payload of length equal or less
     * than getMaxActiveSetAdvertisingDataLength(). If you require a longer payload you must
     * stop the advertising set, set the payload and restart the set.
     *
     * @return BLE_ERROR_NONE on success.
     *
     * @see ble::AdvertisingDataBuilder to build a payload.
     */
    ble_error_t setAdvertisingPayload(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> payload
    );

    /** Set new advertising scan response for a given advertising set. This will be sent to
     *  device who perform active scanning.
     *
     * @param handle Advertising set handle.
     * @param response Advertising scan response.
     *
     * @note If advertising set is active you may only set payload of length equal or less
     * than getMaxActiveSetAdvertisingDataLength(). If you require a longer payload you must
     * stop the advertising set, set the payload and restart the set.
     *
     * @return BLE_ERROR_NONE on success.
     *
     * @see ble::AdvertisingDataBuilder to build a payload.
     */
    ble_error_t setAdvertisingScanResponse(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> response
    );

    /** Start advertising using the given advertising set.
     *
     * @param handle Advertising set handle.
     * @param maxDuration Max duration for advertising (in units of 10ms) - 0 means no limit.
     * @param maxEvents Max number of events produced during advertising - 0 means no limit.
     * @return BLE_ERROR_NONE on success. This does not guarantee the set has started if
     * extended advertising is enabled. Register an event handler and wait for onAdvertisingStart
     * event. An (unlikely) failed start the status of the event will contain an error.
     *
     * @see EventHandler::onAdvertisingStart when the advertising starts.
     * @see EventHandler::onScanRequestReceived when a scan request is received.
     * @see EventHandler::onAdvertisingEnd when the advertising ends.
     * @see EventHandler::onConnectionComplete when the device gets connected
     * by a peer.
     */
    ble_error_t startAdvertising(
        advertising_handle_t handle,
        adv_duration_t maxDuration = adv_duration_t::forever(),
        uint8_t maxEvents = 0
    );

    /** Stop advertising given advertising set. This is separate from periodic advertising
     *  which will not be affected.
     *
     * @param handle Advertising set handle.
     * @return BLE_ERROR_NONE on success. For extented advertising this does not guarantee
     * the set is stopped if. Register an event handler and wait for onAdvertisingEnd event.
     * An (unlikely) failed stop the event status will contain the error code.
     */
    ble_error_t stopAdvertising(advertising_handle_t handle);

    /** Check if advertising is active for a given advertising set.
     *
     * @param handle Advertising set handle.
     * @return True if advertising is active on this set.
     */
    bool isAdvertisingActive(advertising_handle_t handle);
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_BROADCASTER
#if BLE_FEATURE_PERIODIC_ADVERTISING
    /** Set periodic advertising parameters for a given advertising set.
     *
     * @param handle Advertising set handle.
     * @param periodicAdvertisingIntervalMin Minimum interval for periodic advertising.
     * @param periodicAdvertisingIntervalMax Maximum interval for periodic advertising.
     * @param advertiseTxPower Include transmission power in the advertisements.
     * @return BLE_ERROR_NONE on success.
     *
     * @version 5+
     */
    ble_error_t setPeriodicAdvertisingParameters(
        advertising_handle_t handle,
        periodic_interval_t periodicAdvertisingIntervalMin,
        periodic_interval_t periodicAdvertisingIntervalMax,
        bool advertiseTxPower = true
    );

    /** Set new periodic advertising payload for a given advertising set.
     *
     * @param handle Advertising set handle.
     * @param payload Advertising payload.
     * @return BLE_ERROR_NONE on success.
     *
     * @note If advertising set is active you may only set payload of length equal or less
     * than getMaxActiveSetAdvertisingDataLength(). If you require a longer payload you must
     * stop the advertising set, set the payload and restart the set. Stopping the set will
     * cause peers to lose sync on the periodic set.
     *
     * @see ble::AdvertisingDataBuilder to build a payload.
     *
     * @version 5+
     */
    ble_error_t setPeriodicAdvertisingPayload(
        advertising_handle_t handle,
        mbed::Span<const uint8_t> payload
    );

    /** Start periodic advertising for a given set. Periodic advertising will not start until
     *  normal advertising is running but will continue to run after normal advertising has stopped.
     *
     * @param handle Advertising set handle.
     * @return BLE_ERROR_NONE on success.
     *
     * @version 5+
     */
    ble_error_t startPeriodicAdvertising(advertising_handle_t handle);

    /** Stop periodic advertising for a given set.
     *
     * @param handle Advertising set handle.
     * @return BLE_ERROR_NONE on success.
     *
     * @version 5+
     */
    ble_error_t stopPeriodicAdvertising(advertising_handle_t handle);

    /** Check if periodic advertising is active for a given advertising set.
     *
     * @param handle Advertising set handle.
     * @return True if periodic advertising is active on this set.
     *
     * @version 5+
     */
    bool isPeriodicAdvertisingActive(advertising_handle_t handle);
#endif // BLE_ROLE_BROADCASTER
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

    /*                                     scanning                                              */
#if BLE_ROLE_OBSERVER
    /** Set new scan parameters.
     *
     * @param params Scan parameters, @see GapScanParameters for details.
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t setScanParameters(const ScanParameters &params);

    /** Start scanning.
     *
     * @param duration How long to scan for. Special value 0 means scan forever.
     * @param filtering Filtering policy.
     * @param period How long to scan for in single period. If the period is 0 and duration
     *               is nonzero the scan will last for single duration.
     *
     * @note When the duration and period parameters are non-zero scanning will last for
     * the duration within the period. After the scan period has expired a new scan period
     * will begin and scanning. This will repeat until stopScan() is called.
     *
     * @return BLE_ERROR_NONE on success.
     *
     * @see EventHandler::onAdvertisingReport to collect advertising reports.
     * @see EventHandler::onScanTimeout when scanning timeout.
     */
    ble_error_t startScan(
        scan_duration_t duration = scan_duration_t::forever(),
        duplicates_filter_t filtering = duplicates_filter_t::DISABLE,
        scan_period_t period = scan_period_t(0)
    );

    /**
     * Stop the ongoing scanning procedure.
     *
     * The current scanning parameters remain in effect.
     *
     * @retval BLE_ERROR_NONE if successfully stopped scanning procedure.
     */
    ble_error_t stopScan();
#endif // BLE_ROLE_OBSERVER

#if BLE_ROLE_OBSERVER
#if BLE_FEATURE_PERIODIC_ADVERTISING
    /** Synchronize with periodic advertising from an advertiser and begin receiving periodic
     *  advertising packets.
     *
     * @param peerAddressType Peer address type.
     * @param peerAddress Peer address.
     * @param sid Advertiser set identifier.
     * @param maxPacketSkip Number of consecutive periodic advertising packets that the receiver
     *                      may skip after successfully receiving a periodic advertising packet.
     * @param timeout Maximum permitted time between successful receptions. If this time is
     *                exceeded, synchronisation is lost.
     * @return BLE_ERROR_NONE on success.
     *
     * @see EventHandler::onPeriodicAdvertisingSyncEstablished when the sync is
     * effective.
     * @see EventHandler::onPeriodicAdvertisingReport when data are issued by the
     * peer.
     * @see EventHandler::onPeriodicAdvertisingSyncLoss when the sync has been
     * loss.
     *
     * @version 5+
     */
    ble_error_t createSync(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        uint8_t sid,
        slave_latency_t maxPacketSkip,
        sync_timeout_t timeout
    );

    /** Synchronize with periodic advertising from an advertiser and begin receiving periodic
     *  advertising packets. Use periodic advertising sync list to determine who to sync with.
     *
     * @param maxPacketSkip Number of consecutive periodic advertising packets that the receiver
     *                      may skip after successfully receiving a periodic advertising packet.
     * @param timeout Maximum permitted time between successful receives.
     *                If this time is exceeded, synchronisation is lost.
     * @return BLE_ERROR_NONE on success.
     *
     * @see EventHandler::onPeriodicAdvertisingSyncEstablished when the sync is
     * effective.
     * @see EventHandler::onPeriodicAdvertisingReport when data are issued by the
     * peer.
     * @see EventHandler::onPeriodicAdvertisingSyncLoss when the sync has been
     * loss.
     *
     * @version 5+
     */
    ble_error_t createSync(
        slave_latency_t maxPacketSkip,
        sync_timeout_t timeout
    );

    /** Cancel sync attempt.
     *
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t cancelCreateSync();

    /** Stop reception of the periodic advertising identified by the handle.
     *
     * @param handle Periodic advertising synchronisation handle.
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t terminateSync(periodic_sync_handle_t handle);

    /** Add device to the periodic advertiser list. Cannot be called when sync is ongoing.
     *
     * @param peerAddressType Peer address type.
     * @param peerAddress Peer address.
     * @param sid Advertiser set identifier.
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t addDeviceToPeriodicAdvertiserList(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        advertising_sid_t sid
    );

    /** Remove device from the periodic advertiser list. Cannot be called when sync is ongoing.
     *
     * @param peerAddressType Peer address type.
     * @param peerAddress Peer address.
     * @param sid Advertiser set identifier.
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t removeDeviceFromPeriodicAdvertiserList(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        advertising_sid_t sid
    );

    /** Remove all devices from periodic advertiser list.
     *
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t clearPeriodicAdvertiserList();

    /** Get number of devices that can be added to the periodic advertiser list.
     * @return Number of devices that can be added to the periodic advertiser list.
     */
    uint8_t getMaxPeriodicAdvertiserListSize();
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

#if BLE_ROLE_CENTRAL
    /**
     * Initiate a connection to a peer.
     *
     * Once the connection is established an onConnectionComplete in the event handler
     * will be called.
     *
     * @param peerAddressType
     * @param peerAddress
     * @param connectionParams
     *
     * @return BLE_ERROR_NONE if connection establishment procedure is started
     * successfully. The connectionCallChain (if set) is invoked upon
     * a connection event.
     *
     * @see EventHandler::onConnectionComplete will be called whether the
     * connection process succeed or fail.
     * @see EventHandler::onDisconnectionComplete is called when the connection
     * ends.
     */
    ble_error_t connect(
        peer_address_type_t peerAddressType,
        const address_t &peerAddress,
        const ConnectionParameters &connectionParams
    );

    /** Cancel the connection attempt. This is not guaranteed to succeed. As a result
     *  onConnectionComplete in the event handler will be called. Check the success parameter
     *  to see if the connection was created.
     *
     * @return BLE_ERROR_NONE if the connection attempt has been requested to be cancelled.
     * Returns BLE_ERROR_OPERATION_NOT_PERMITTED if no ongoing connection for last used address found.
     */
    ble_error_t cancelConnect();
#endif // BLE_ROLE_CENTRAL

#if BLE_FEATURE_CONNECTABLE
    /**
     * Update connection parameters of an existing connection.
     *
     * In the central role, this initiates a Link Layer connection parameter
     * update procedure. In the peripheral role, this sends the corresponding
     * L2CAP request and waits for the central to accept or reject the requested
     * connection parameters.
     *
     * @param connectionHandle The handle of the connection to update.
     * @param minConnectionInterval The minimum connection interval requested.
     * @param maxConnectionInterval The maximum connection interval requested.
     * @param slaveLatency The slave latency requested.
     * @param supervision_timeout The supervision timeout requested.
     * @param minConnectionEventLength The minimum connection event length requested.
     * @param maxConnectionEventLength The maximum connection event length requested.
     *
     * @return BLE_ERROR_NONE if the request has been sent and false otherwise.
     *
     * @see EventHandler::onUpdateConnectionParametersRequest when a central
     * receives a request to update the connection parameters.
     * @see EventHandler::onConnectionParametersUpdateComplete when connection
     * parameters have been updated.
     *
     * @version 4.0+ for central
     * @version 4.1+ for peripheral
     */
    ble_error_t updateConnectionParameters(
        connection_handle_t connectionHandle,
        conn_interval_t minConnectionInterval,
        conn_interval_t maxConnectionInterval,
        slave_latency_t slaveLatency,
        supervision_timeout_t supervision_timeout,
        conn_event_length_t minConnectionEventLength = conn_event_length_t(0),
        conn_event_length_t maxConnectionEventLength = conn_event_length_t(0)
    );

    /**
     * Allows the application to accept or reject a connection parameters update
     * request.
     *
     * If this process is managed by the middleware; new connection parameters
     * from a slave are always accepted.
     *
     * @param userManageConnectionUpdateRequest true to let the application
     * manage the process and false to let the middleware manage it.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     *
     * @version 4.1+
     *
     * @see EventHandler::onUpdateConnectionParametersRequest when a central
     * receives a request to update the connection parameters.
     *
     * @see acceptConnectionParametersUpdate to accept the request.
     * @see rejectConnectionParametersUpdate to reject the request.
     */
    ble_error_t manageConnectionParametersUpdateRequest(
        bool userManageConnectionUpdateRequest
    );

    /**
     * Accept update of the connection parameters.
     *
     * The central can adjust the new connection parameters.
     *
     * @param connectionHandle The handle of the connection that has initiated
     * the request.
     * @param minConnectionInterval The minimum connection interval to be applied.
     * @param maxConnectionInterval The maximum connection interval to be applied.
     * @param slaveLatency The slave latency to be applied.
     * @param supervision_timeout The supervision timeout to be applied.
     * @param minConnectionEventLength The minimum connection event length to be
     * applied.
     * @param maxConnectionEventLength The maximum connection event length to be
     * applied.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     *
     * @version 4.1+
     *
     * @see manageConnectionParametersUpdateRequest To let the application
     * manage the process.
     *
     * @see EventHandler::onUpdateConnectionParametersRequest Called when a
     * request to update the connection parameters is received.
     *
     * @see EventHandler::onConnectionParametersUpdateComplete Called when the
     * new connection parameters are effective.
     */
    ble_error_t acceptConnectionParametersUpdate(
        connection_handle_t connectionHandle,
        conn_interval_t minConnectionInterval,
        conn_interval_t maxConnectionInterval,
        slave_latency_t slaveLatency,
        supervision_timeout_t supervision_timeout,
        conn_event_length_t minConnectionEventLength = conn_event_length_t(0),
        conn_event_length_t maxConnectionEventLength = conn_event_length_t(0)
    );

    /**
     * Reject a request to change the connection parameters.
     *
     * @param connectionHandle The handle of the connection that has initiated
     * the request.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     *
     * @version 4.1+
     *
     * @see manageConnectionParametersUpdateRequest To let the application
     * manage the process.
     *
     * @see EventHandler::onUpdateConnectionParametersRequest Called when a
     * request to update the connection parameters is received.
     */
    ble_error_t rejectConnectionParametersUpdate(
        connection_handle_t connectionHandle
    );

    /**
     * Initiate a disconnection procedure.
     *
     * Once the disconnection procedure has completed a
     * DisconnectionCallbackParams_t, the event is emitted to handlers that
     * have been registered with onDisconnection().
     *
     * @param[in] reason Reason of the disconnection transmitted to the peer.
     * @param[in] connectionHandle Handle of the connection to end.
     *
     * @return  BLE_ERROR_NONE if the disconnection procedure successfully
     * started.
     *
     * @see EventHandler::onDisconnectionComplete when the disconnection is
     * effective.
     */
    ble_error_t disconnect(
        connection_handle_t connectionHandle,
        local_disconnection_reason_t reason
    );
#endif // BLE_FEATURE_CONNECTABLE
#if BLE_FEATURE_PHY_MANAGEMENT
    /**
     * Read the PHY used by the transmitter and the receiver on a connection.
     *
     * Once the PHY has been read, it is reported back via the function onPhyRead
     * of the event handler registered by the application.
     *
     * @param connection Handle of the connection for which the PHY being used is
     * queried.
     *
     * @return BLE_ERROR_NONE if the read PHY procedure has been started or an
     * appropriate error code.
     *
     * @version 5+
     *
     * @see EventHandler::onReadPhy is called when the phy has been read.
     */
    ble_error_t readPhy(connection_handle_t connection);

    /**
     * Set the preferred PHYs to use in a connection.
     *
     * @param txPhys: Set of PHYs preferred for tx operations. If NULL then no
     * preferred PHYs are set and the default value of the subsystem is used.
     *
     * @param rxPhys: Set of PHYs preferred for rx operations. If NULL then no
     * preferred PHYs are set and the default value of the subsystem is used.
     *
     * @return BLE_ERROR_NONE if the preferences have been set or an appropriate
     * error code.
     *
     * @version 5+
     */
    ble_error_t setPreferredPhys(
        const phy_set_t *txPhys,
        const phy_set_t *rxPhys
    );

    /**
     * Update the PHY used by a connection.
     *
     * Once the update process has been completed, it is reported back to the
     * application via the function onPhyUpdateComplete of the event handler
     * registered by the application.
     *
     * @param connection Handle of the connection to update.
     *
     * @param txPhys Set of PHYs preferred for tx operations. If NULL then the
     * choice is up to the Bluetooth subsystem.
     *
     * @param rxPhys Set of PHYs preferred for rx operations. If NULL then the
     * choice is up to the Bluetooth subsystem.
     *
     * @param codedSymbol Number of symbols used to code a bit when le coded is
     * used. If the value is UNDEFINED then the choice is up to the Bluetooth
     * subsystem.
     *
     * @return BLE_ERROR_NONE if the update PHY procedure has been successfully
     * started or an error code.
     *
     * @see EventHandler::onPhyUpdateComplete is called when the phy used by the
     * connection has been updated.
     */
    ble_error_t setPhy(
        connection_handle_t connection,
        const phy_set_t *txPhys,
        const phy_set_t *rxPhys,
        coded_symbol_per_bit_t codedSymbol
    );
#endif // BLE_FEATURE_PHY_MANAGEMENT

#if BLE_FEATURE_PRIVACY
    /**
     * Enable or disable privacy mode of the local device.
     *
     * When privacy is enabled, the system use private addresses while it scans,
     * advertises or initiate a connection. The device private address is
     * renewed every 15 minutes.
     *
     * @par Configuration
     *
     * The privacy feature can be configured with the help of the functions
     * setPeripheralPrivacyConfiguration and setCentralPrivacyConfiguration
     * which respectively set the privacy configuration of the peripheral and
     * central role.
     *
     * @par Default configuration of peripheral role
     *
     * By default private resolvable addresses are used for all procedures;
     * including advertisement of nonconnectable packets. Connection request
     * from an unknown initiator with a private resolvable address triggers the
     * pairing procedure.
     *
     * @par Default configuration of central role
     *
     * By default private resolvable addresses are used for all procedures;
     * including active scanning. Addresses present in advertisement packet are
     * resolved and advertisement packets are forwarded to the application
     * even if the advertiser private address is unknown.
     *
     * @par Initialization of the privacy subsystem
     *
     * When privacy is enabled, the system generates new resolvable and non
     * resolvable private addresses. Scan, Advertising and Connecting to a peer
     * won't be available until the generation process completes. When addresses
     * have been generated, the application is notified that privacy
     * initialisation as completed with a call to EventHandler::onPrivacyEnabled .
     *
     * @param[in] enable Should be set to true to enable the privacy mode and
     * false to disable it.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t enablePrivacy(bool enable);

#if BLE_ROLE_BROADCASTER
    /**
     * Set the privacy configuration used by the peripheral role.
     *
     * @param[in] configuration The configuration to set.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t setPeripheralPrivacyConfiguration(
        const peripheral_privacy_configuration_t *configuration
    );

    /**
     * Get the privacy configuration used by the peripheral role.
     *
     * @param[out] configuration The variable filled with the current
     * configuration.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t getPeripheralPrivacyConfiguration(
        peripheral_privacy_configuration_t *configuration
    );
#endif // BLE_ROLE_BROADCASTER

#if BLE_ROLE_OBSERVER
    /**
     * Set the privacy configuration used by the central role.
     *
     * @param[in] configuration The configuration to set.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t setCentralPrivacyConfiguration(
        const central_privacy_configuration_t *configuration
    );

    /**
     * Get the privacy configuration used by the central role.
     *
     * @param[out] configuration The variable filled with the current
     * configuration.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t getCentralPrivacyConfiguration(
        central_privacy_configuration_t *configuration
    );
#endif // BLE_ROLE_OBSERVER
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_WHITELIST
    /**
     * Get the maximum size of the whitelist.
     *
     * @return Maximum size of the whitelist.
     */
    uint8_t getMaxWhitelistSize() const;

    /**
     * Get the Link Layer to use the internal whitelist when scanning,
     * advertising or initiating a connection depending on the filter policies.
     *
     * @param[in,out] whitelist Define the whitelist instance which is used
     * to store the whitelist requested. In input, the caller provisions memory.
     *
     * @return BLE_ERROR_NONE if the implementation's whitelist was successfully
     * copied into the supplied reference.
     */
    ble_error_t getWhitelist(whitelist_t &whitelist) const;

    /**
     * Set the value of the whitelist to be used during GAP procedures.
     *
     * @param[in] whitelist A reference to a whitelist containing the addresses
     * to be copied to the internal whitelist.
     *
     * @return BLE_ERROR_NONE if the implementation's whitelist was successfully
     * populated with the addresses in the given whitelist.
     *
     * @note The whitelist must not contain non-resolvable addresses. This
     * results in a @ref BLE_ERROR_INVALID_PARAM because the remote peer might
     * change its private address at any time, and it is not possible to resolve
     * it.
     *
     * @note If the input whitelist is larger than @ref getMaxWhitelistSize(),
     * then @ref BLE_ERROR_PARAM_OUT_OF_RANGE is returned.
     */
    ble_error_t setWhitelist(const whitelist_t &whitelist);

#endif // BLE_FEATURE_WHITELIST

    /**
     * Fetch the current address and its type.
     *
     * @param[out] typeP Type of the current address set.
     * @param[out] address Value of the current address.
     *
     * @note If privacy is enabled the device address may be unavailable to
     * application code.
     *
     * @return BLE_ERROR_NONE on success.
     */
    ble_error_t getAddress(
        own_address_type_t &typeP,
        address_t &address
    );

    /**
     * Return the type of a random address.
     *
     * @param[in] address The random address to retrieve the type from. The
     * address must be ordered in little endian.
     *
     * @param[out] addressType Type of the address to fill.
     *
     * @return BLE_ERROR_NONE in case of success or BLE_ERROR_INVALID_PARAM if
     * the address in input was not identifiable as a random address.
     */
    static ble_error_t getRandomAddressType(
        ble::address_t address,
        ble::random_address_type_t *addressType
    );

    /**
     * Reset the Gap instance.
     *
     * Reset process starts by notifying all registered shutdown event handlers
     * that the Gap instance is about to be shut down. Then, it clears all Gap state
     * of the associated object and then cleans the state present in the vendor
     * implementation.
     *
     * This function is meant to be overridden in the platform-specific
     * subclass. Nevertheless, the subclass only resets its
     * state and not the data held in Gap members. This is achieved by a
     * call to Gap::reset() from the subclass' reset() implementation.
     *
     * @return BLE_ERROR_NONE on success.
     *
     * @note Currently, a call to reset() does not reset the advertising and
     * scan parameters to default values.
     */
    ble_error_t reset();

        /**
     * Register a Gap shutdown event handler.
     *
     * The handler is called when the Gap instance is about to shut down.
     * It is usually issued after a call to BLE::shutdown().
     *
     * @param[in] callback Shutdown event handler to register.
     *
     * @note To unregister a shutdown event handler, use
     * onShutdown().detach(callback).
     */
    void onShutdown(const GapShutdownCallback_t &callback);

    /**
     * Register a Gap shutdown event handler.
     *
     * @param[in] objPtr Instance used to invoke @p memberPtr.
     * @param[in] memberPtr Shutdown event handler to register.
     */
    template<typename T>
    void onShutdown(T *objPtr, void (T::*memberPtr)(const Gap *)) {
        onShutdown(GapShutdownCallback_t(objPtr, memberPtr));
    }

    /**
     * Access the callchain of shutdown event handler.
     *
     * @note To register callbacks, use onShutdown().add(callback).
     *
     * @note To unregister callbacks, use onShutdown().detach(callback).
     *
     * @return A reference to the shutdown event callback chain.
     */
    GapShutdownCallbackChain_t &onShutdown();

#if !defined(DOXYGEN_ONLY)
    /*
     * Constructor from the private implementation.
     */
    Gap(impl::Gap* impl) : impl(impl) {}

    /*
     * Restrict copy and move.
     */
    Gap(const Gap&) = delete;
    Gap& operator=(const Gap&) = delete;

    /*
     * API reserved for the controller driver to set the random static address.
     * Setting a new random static address while the controller is operating is
     * forbidden by the Bluetooth specification.
     */
    ble_error_t setRandomStaticAddress(const ble::address_t& address);

    ble::address_t getRandomStaticAddress();
#endif // !defined(DOXYGEN_ONLY)

private:
    impl::Gap* impl;
};

/**
 * @}
 * @}
 */

} // namespace ble

/** @deprecated Use the namespaced ble::Gap instead of the global Gap. */
using ble::Gap;

#endif // BLE_GAP_GAP_H
