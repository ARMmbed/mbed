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

#ifndef MBED_BLE_ROLES_H__
#define MBED_BLE_ROLES_H__

/* we provide default values if no configuration is present (e.g. in unittests) */
#if !defined(BLE_ROLE_OBSERVER)
#define BLE_ROLE_OBSERVER 1
#endif
#if !defined(BLE_ROLE_BROADCASTER)
#define BLE_ROLE_BROADCASTER 1
#endif
#if !defined(BLE_ROLE_CENTRAL)
#define BLE_ROLE_CENTRAL 1
#endif
#if !defined(BLE_ROLE_PERIPHERAL)
#define BLE_ROLE_PERIPHERAL 1
#endif
#if !defined(BLE_FEATURE_GATT_CLIENT)
#define BLE_FEATURE_GATT_CLIENT 1
#endif
#if !defined(BLE_FEATURE_GATT_SERVER)
#define BLE_FEATURE_GATT_SERVER 1
#endif
#if !defined(BLE_FEATURE_SECURITY)
#define BLE_FEATURE_SECURITY 1
#endif
#if !defined(BLE_FEATURE_SECURE_CONNECTIONS)
#define BLE_FEATURE_SECURE_CONNECTIONS 1
#endif
#if !defined(BLE_FEATURE_SIGNING)
#define BLE_FEATURE_SIGNING 1
#endif
#if !defined(BLE_FEATURE_WHITELIST)
#define BLE_FEATURE_WHITELIST 1
#endif
#if !defined(BLE_FEATURE_PRIVACY)
#define BLE_FEATURE_PRIVACY 1
#endif
#if !defined(BLE_FEATURE_PHY_MANAGEMENT)
#define BLE_FEATURE_PHY_MANAGEMENT 1
#endif
#if !defined(BLE_FEATURE_EXTENDED_ADVERTISING)
#define BLE_FEATURE_EXTENDED_ADVERTISING 1
#endif
#if !defined(BLE_FEATURE_PERIODIC_ADVERTISING)
#define BLE_FEATURE_PERIODIC_ADVERTISING 1
#endif

#if !(BLE_ROLE_OBSERVER) && !(BLE_ROLE_BROADCASTER)
    #error "BLE requires at least one role 'BROADCASTER' or 'OBSERVER' to be enabled"
#endif

#if (BLE_ROLE_PERIPHERAL) || (BLE_ROLE_CENTRAL)
    #define BLE_FEATURE_CONNECTABLE 1
#endif

#if (BLE_FEATURE_GATT_CLIENT) || (BLE_FEATURE_GATT_SERVER)
    #define BLE_FEATURE_ATT 1
#endif

#if BLE_ROLE_PERIPHERAL
    #if !(BLE_ROLE_BROADCASTER)
        #error "BLE role 'PERIPHERAL' depends on role 'BROADCASTER'"
    #endif
#endif // BLE_ROLE_PERIPHERAL

#if BLE_ROLE_CENTRAL
    #if !(BLE_ROLE_OBSERVER)
        #error "BLE role 'CENTRAL' depends on role 'OBSERVER'"
    #endif
#endif // BLE_ROLE_CENTRAL

#if BLE_FEATURE_SECURITY
    #if !(BLE_ROLE_PERIPHERAL) && !(BLE_ROLE_CENTRAL)
        #error "BLE feature 'SECURITY' requires 'PERIPHERAL' or 'CENTRAL' role"
    #endif
#endif // BLE_FEATURE_SECURITY

#if BLE_FEATURE_SECURE_CONNECTIONS
    #if !(BLE_FEATURE_SECURITY)
        #error "BLE feature 'SECURE CONNECTIONS' requires the 'SECURITY' feature"
    #endif
#endif // BLE_FEATURE_SECURE_CONNECTIONS

#if BLE_FEATURE_SIGNING
    #if !(BLE_FEATURE_SECURITY)
        #error "BLE feature 'SIGNING' requires the 'SECURITY' feature"
    #endif
#endif // BLE_FEATURE_SIGNING

#if BLE_FEATURE_WHITELIST
    #if !(BLE_FEATURE_SECURITY)
        #error "BLE feature 'WHITELIST' requires the 'SECURITY' feature"
    #endif
#endif // BLE_FEATURE_WHITELIST

#if BLE_FEATURE_PRIVACY
    #if !(BLE_FEATURE_SECURITY)
        #error "BLE feature 'PRIVACY' requires the 'SECURITY' feature"
    #endif
#endif // BLE_FEATURE_PRIVACY

#if BLE_FEATURE_PERIODIC_ADVERTISING
    #if !(BLE_FEATURE_EXTENDED_ADVERTISING)
        #error "BLE feature 'PERIODIC ADVERTISING' requires the 'EXTENDED ADVERTISING' feature"
    #endif
#endif // BLE_FEATURE_PERIODIC_ADVERTISING

#if BLE_FEATURE_GATT_CLIENT
    #if !(BLE_FEATURE_CONNECTABLE)
        #error "BLE feature 'GATT CLIENT' requires 'PERIPHERAL' or 'CENTRAL' role"
    #endif
#endif // BLE_FEATURE_GATT_CLIENT

#if BLE_FEATURE_GATT_SERVER
    #if !(BLE_FEATURE_CONNECTABLE)
        #error "BLE feature 'GATT SERVER' requires 'PERIPHERAL' or 'CENTRAL' role"
    #endif
#endif // BLE_FEATURE_GATT_SERVER

#endif // MBED_BLE_ROLES_H__
