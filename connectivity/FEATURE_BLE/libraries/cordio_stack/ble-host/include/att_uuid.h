/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Attribute protocol UUIDs from the Bluetooth specification.
 *
 *  Copyright (c) 2011-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/
#ifndef ATT_UUID_H
#define ATT_UUID_H

#include "att_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \addtogroup STACK_ATT_API
 *  \{ */

/**************************************************************************************************
  Macros
**************************************************************************************************/

/** \name ATT Service UUIDs
 * Defined BLE Service UUID constants.
 */
/**@{*/
#define ATT_UUID_GAP_SERVICE                0x1800    /*!< \brief Generic Access Profile Service */
#define ATT_UUID_GATT_SERVICE               0x1801    /*!< \brief Generic Attribute Profile Service */
#define ATT_UUID_IMMEDIATE_ALERT_SERVICE    0x1802    /*!< \brief Immediate Alert Service */
#define ATT_UUID_LINK_LOSS_SERVICE          0x1803    /*!< \brief Link Loss Service */
#define ATT_UUID_TX_POWER_SERVICE           0x1804    /*!< \brief Tx Power Service */
#define ATT_UUID_CURRENT_TIME_SERVICE       0x1805    /*!< \brief Current Time Service */
#define ATT_UUID_REF_TIME_UPDATE_SERVICE    0x1806    /*!< \brief Reference Time Update Service */
#define ATT_UUID_DST_CHANGE_SERVICE         0x1807    /*!< \brief Next DST Change Service */
#define ATT_UUID_GLUCOSE_SERVICE            0x1808    /*!< \brief Glucose Service */
#define ATT_UUID_HEALTH_THERM_SERVICE       0x1809    /*!< \brief Health Thermometer Service */
#define ATT_UUID_DEVICE_INFO_SERVICE        0x180A    /*!< \brief Device Information Service */
#define ATT_UUID_NETWORK_AVAIL_SERVICE      0x180B    /*!< \brief Network Availability Service */
#define ATT_UUID_WATCHDOG_SERVICE           0x180C    /*!< \brief Watchdog Service */
#define ATT_UUID_HEART_RATE_SERVICE         0x180D    /*!< \brief Heart Rate Service */
#define ATT_UUID_PHONE_ALERT_SERVICE        0x180E    /*!< \brief Phone Alert Status Service */
#define ATT_UUID_BATTERY_SERVICE            0x180F    /*!< \brief Battery Service */
#define ATT_UUID_BLOOD_PRESSURE_SERVICE     0x1810    /*!< \brief Blood Pressure Service */
#define ATT_UUID_ALERT_NOTIF_SERVICE        0x1811    /*!< \brief Alert Notification Service */
#define ATT_UUID_HID_SERVICE                0x1812    /*!< \brief Human Interface Device Service */
#define ATT_UUID_SCAN_PARAM_SERVICE         0x1813    /*!< \brief Scan Parameter Service */
#define ATT_UUID_RUNNING_SPEED_SERVICE      0x1814    /*!< \brief Running Speed Service */
#define ATT_UUID_CYCLING_SPEED_SERVICE      0x1816    /*!< \brief Cycling Speed Service */
#define ATT_UUID_CYCLING_POWER_SERVICE      0x1818    /*!< \brief Cycling Power Service */
#define ATT_UUID_USER_DATA_SERVICE          0x181C    /*!< \brief User Data Service */
#define ATT_UUID_WEIGHT_SCALE_SERVICE       0x181D    /*!< \brief Weight Scale Service */
#define ATT_UUID_IP_SUPPORT_SERVICE         0x1820    /*!< \brief IP Support Service */
#define ATT_UUID_PULSE_OXIMITER_SERVICE     0x1822    /*!< \brief Pulse Oximeter Service */
#define ATT_UUID_MESH_PRV_SERVICE           0x1827    /*!< \brief Mesh Provisioning Service */
#define ATT_UUID_MESH_PROXY_SERVICE         0x1828    /*!< \brief Mesh Proxy Service */
#define ATT_UUID_AUDIO_INPUT_CTRL_SERVICE   0x1843    /*!< \brief Audio Input Control Service */
#define ATT_UUID_VOLUME_CTRL_SERVICE        0x1844    /*!< \brief Volume Control Service */
#define ATT_UUID_VOLUME_OFFSET_CTRL_SERVICE 0x1845    /*!< \brief Volume Offset Control Service */
#define ATT_UUID_CONSTANT_TONE_SERVICE      0x184A    /*!< \brief Constant Tone Extension */
#define ATT_UUID_MICROPHONE_CTRL_SERVICE    0x8FD4    /*!< \brief Microphone Control Service */
#define ATT_UUID_PUB_AUDIO_CAP_SERVICE      0x8FD9    /*!< \brief Published Audio Capability Service */
#define ATT_UUID_AUDIO_STRM_CTRL_SERVICE    0x8FDA    /*!< \brief Audio Stream Control Service */
#define ATT_UUID_BCAST_AUDIO_SCAN_SERVICE   0x8FDB    /*!< \brief Broadcast Audio Scan Service */
#define ATT_UUID_BASIC_AUDIO_ANNC_SERVICE   0x8FDC    /*!< \brief Basic Audio Announcement Service */
#define ATT_UUID_BCAST_AUDIO_ANNC_SERVICE   0x8FDD    /*!< \brief Broadcast Audio Announcement Service */
/**@}*/

/** \name GATT UUIDs
 * BLE Defined UUIDs of GATT Service components
 */
/**@{*/
#define ATT_UUID_PRIMARY_SERVICE            0x2800    /*!< \brief Primary Service */
#define ATT_UUID_SECONDARY_SERVICE          0x2801    /*!< \brief Secondary Service */
#define ATT_UUID_INCLUDE                    0x2802    /*!< \brief Include */
#define ATT_UUID_CHARACTERISTIC             0x2803    /*!< \brief Characteristic */
/**@}*/

/** \name GATT Characteristic Descriptor UUIDs
 * BLE Defined UUIDs of Characteristic Descriptors
 */
/**@{*/
#define ATT_UUID_CHARACTERISTIC_EXT         0x2900    /*!< \brief Characteristic Extended Properties */
#define ATT_UUID_CHAR_USER_DESC             0x2901    /*!< \brief Characteristic User Description */
#define ATT_UUID_CLIENT_CHAR_CONFIG         0x2902    /*!< \brief Client Characteristic Configuration */
#define ATT_UUID_SERVER_CHAR_CONFIG         0x2903    /*!< \brief Server Characteristic Configuration */
#define ATT_UUID_CHAR_PRES_FORMAT           0x2904    /*!< \brief Characteristic Presentation Format */
#define ATT_UUID_AGGREGATE_FORMAT           0x2905    /*!< \brief Characteristic Aggregate Format */
#define ATT_UUID_VALID_RANGE                0x2906    /*!< \brief Valid Range */
#define ATT_UUID_HID_EXT_REPORT_MAPPING     0x2907    /*!< \brief HID External Report ID Mapping */
#define ATT_UUID_HID_REPORT_ID_MAPPING      0x2908    /*!< \brief HID Report ID Mapping */
/**@}*/

/** \name GATT Characistic UUIDs
 * BLE Defined UUIDs of Characeristics
 */
/**@{*/
#define ATT_UUID_DEVICE_NAME                0x2A00    /*!< \brief Device Name */
#define ATT_UUID_APPEARANCE                 0x2A01    /*!< \brief Appearance */
#define ATT_UUID_PERIPH_PRIVACY_FLAG        0x2A02    /*!< \brief Peripheral Privacy Flag */
#define ATT_UUID_RECONN_ADDR                0x2A03    /*!< \brief Reconnection Address */
#define ATT_UUID_PREF_CONN_PARAM            0x2A04    /*!< \brief Peripheral Preferred Connection Parameters */
#define ATT_UUID_SERVICE_CHANGED            0x2A05    /*!< \brief Service Changed */
#define ATT_UUID_ALERT_LEVEL                0x2A06    /*!< \brief Alert Level */
#define ATT_UUID_TX_POWER_LEVEL             0x2A07    /*!< \brief Tx Power Level */
#define ATT_UUID_DATE_TIME                  0x2A08    /*!< \brief Date Time */
#define ATT_UUID_DAY_OF_WEEK                0x2A09    /*!< \brief Day of Week */
#define ATT_UUID_DAY_DATE_TIME              0x2A0A    /*!< \brief Day Date Time */
#define ATT_UUID_EXACT_TIME_100             0x2A0B    /*!< \brief Exact Time 100 */
#define ATT_UUID_EXACT_TIME_256             0x2A0C    /*!< \brief Exact Time 256 */
#define ATT_UUID_DST_OFFSET                 0x2A0D    /*!< \brief DST Offset */
#define ATT_UUID_TIME_ZONE                  0x2A0E    /*!< \brief Time Zone */
#define ATT_UUID_LOCAL_TIME_INFO            0x2A0F    /*!< \brief Local Time Information */
#define ATT_UUID_SECONDARY_TIME_ZONE        0x2A10    /*!< \brief Secondary Time Zone */
#define ATT_UUID_TIME_WITH_DST              0x2A11    /*!< \brief Time with DST */
#define ATT_UUID_TIME_ACCURACY              0x2A12    /*!< \brief Time Accuracy */
#define ATT_UUID_TIME_SOURCE                0x2A13    /*!< \brief Time Source */
#define ATT_UUID_REFERENCE_TIME_INFO        0x2A14    /*!< \brief Reference Time Information */
#define ATT_UUID_TIME_BROADCAST             0x2A15    /*!< \brief Time Broadcast */
#define ATT_UUID_TIME_UPDATE_CP             0x2A16    /*!< \brief Time Update Control Point */
#define ATT_UUID_TIME_UPDATE_STATE          0x2A17    /*!< \brief Time Update State */
#define ATT_UUID_GLUCOSE_MEAS               0x2A18    /*!< \brief Glucose Measurement */
#define ATT_UUID_BATTERY_LEVEL              0x2A19    /*!< \brief Battery Level */
#define ATT_UUID_BATTERY_POWER_STATE        0x2A1A    /*!< \brief Battery Power State */
#define ATT_UUID_BATTERY_LEVEL_STATE        0x2A1B    /*!< \brief Battery Level State */
#define ATT_UUID_TEMP_MEAS                  0x2A1C    /*!< \brief Temperature Measurement */
#define ATT_UUID_TEMP_TYPE                  0x2A1D    /*!< \brief Temperature Type */
#define ATT_UUID_INTERMEDIATE_TEMP          0x2A1E    /*!< \brief Intermediate Temperature */
#define ATT_UUID_TEMP_C                     0x2A1F    /*!< \brief Temperature Celsius */
#define ATT_UUID_TEMP_F                     0x2A20    /*!< \brief Temperature Fahrenheit */
#define ATT_UUID_MEAS_INTERVAL              0x2A21    /*!< \brief Measurement Interval */
#define ATT_UUID_HID_BOOT_KEYBOARD_IN       0x2A22    /*!< \brief HID Boot Keyboard In */
#define ATT_UUID_SYSTEM_ID                  0x2A23    /*!< \brief System ID */
#define ATT_UUID_MODEL_NUMBER               0x2A24    /*!< \brief Model Number String */
#define ATT_UUID_SERIAL_NUMBER              0x2A25    /*!< \brief Serial Number String */
#define ATT_UUID_FIRMWARE_REV               0x2A26    /*!< \brief Firmware Revision String */
#define ATT_UUID_HARDWARE_REV               0x2A27    /*!< \brief Hardware Revision String */
#define ATT_UUID_SOFTWARE_REV               0x2A28    /*!< \brief Software Revision String */
#define ATT_UUID_MANUFACTURER_NAME          0x2A29    /*!< \brief Manufacturer Name String */
#define ATT_UUID_11073_CERT_DATA            0x2A2A    /*!< \brief IEEE 11073-20601 Regulatory Certification Data List */
#define ATT_UUID_CURRENT_TIME               0x2A2B    /*!< \brief Current Time */
#define ATT_UUID_ELEVATION                  0x2A2C    /*!< \brief Elevation */
#define ATT_UUID_LATITUDE                   0x2A2D    /*!< \brief Latitude */
#define ATT_UUID_LONGITUDE                  0x2A2E    /*!< \brief Longitude */
#define ATT_UUID_POSITION_2D                0x2A2F    /*!< \brief Position 2D */
#define ATT_UUID_POSITION_3D                0x2A30    /*!< \brief Position 3D */
#define ATT_UUID_VENDOR_ID                  0x2A31    /*!< \brief Vendor ID */
#define ATT_UUID_HID_BOOT_KEYBOARD_OUT      0x2A32    /*!< \brief HID Boot Keyboard Out */
#define ATT_UUID_HID_BOOT_MOUSE_IN          0x2A33    /*!< \brief HID Boot Mouse In */
#define ATT_UUID_GLUCOSE_MEAS_CONTEXT       0x2A34    /*!< \brief Glucose Measurement Context */
#define ATT_UUID_BP_MEAS                    0x2A35    /*!< \brief Blood Pressure Measurement */
#define ATT_UUID_INTERMEDIATE_BP            0x2A36    /*!< \brief Intermediate Cuff Pressure */
#define ATT_UUID_HR_MEAS                    0x2A37    /*!< \brief Heart Rate Measurement */
#define ATT_UUID_HR_SENSOR_LOC              0x2A38    /*!< \brief Body Sensor Location */
#define ATT_UUID_HR_CP                      0x2A39    /*!< \brief Heart Rate Control Point */
#define ATT_UUID_REMOVABLE                  0x2A3A    /*!< \brief Removable */
#define ATT_UUID_SERVICE_REQ                0x2A3B    /*!< \brief Service Required */
#define ATT_UUID_SCI_TEMP_C                 0x2A3C    /*!< \brief Scientific Temperature in Celsius */
#define ATT_UUID_STRING                     0x2A3D    /*!< \brief String */
#define ATT_UUID_NETWORK_AVAIL              0x2A3E    /*!< \brief Network Availability */
#define ATT_UUID_ALERT_STATUS               0x2A3F    /*!< \brief Alert Status */
#define ATT_UUID_RINGER_CP                  0x2A40    /*!< \brief Ringer Control Point */
#define ATT_UUID_RINGER_SETTING             0x2A41    /*!< \brief Ringer Setting */
#define ATT_UUID_ALERT_CAT_ID_MASK          0x2A42    /*!< \brief Alert Category ID Bit Mask */
#define ATT_UUID_ALERT_CAT_ID               0x2A43    /*!< \brief Alert Category ID */
#define ATT_UUID_ALERT_NOTIF_CP             0x2A44    /*!< \brief Alert Notification Control Point */
#define ATT_UUID_UNREAD_ALERT_STATUS        0x2A45    /*!< \brief Unread Alert Status */
#define ATT_UUID_NEW_ALERT                  0x2A46    /*!< \brief New Alert */
#define ATT_UUID_SUP_NEW_ALERT_CAT          0x2A47    /*!< \brief Supported New Alert Category */
#define ATT_UUID_SUP_UNREAD_ALERT_CAT       0x2A48    /*!< \brief Supported Unread Alert Category */
#define ATT_UUID_BP_FEATURE                 0x2A49    /*!< \brief Blood Pressure Feature */
#define ATT_UUID_HID_INFORMATION            0x2A4A    /*!< \brief HID Information */
#define ATT_UUID_HID_REPORT_MAP             0x2A4B    /*!< \brief HID Report Map */
#define ATT_UUID_HID_CONTROL_POINT          0x2A4C    /*!< \brief HID Control Point */
#define ATT_UUID_HID_REPORT                 0x2A4D    /*!< \brief HID Report */
#define ATT_UUID_HID_PROTOCOL_MODE          0x2A4E    /*!< \brief HID Protocol Mode */
#define ATT_UUID_SCAN_INT_WIND              0x2A4F    /*!< \brief Scan Interval Window */
#define ATT_UUID_PNP_ID                     0x2A50    /*!< \brief PnP ID */
#define ATT_UUID_GLUCOSE_FEATURE            0x2A51    /*!< \brief Glucose Feature */
#define ATT_UUID_RACP                       0x2A52    /*!< \brief Record Access Control Point */
#define ATT_UUID_CAR                        0x2AA6    /*!< \brief Central Address Resolution */
#define ATT_UUID_RUNNING_SPEED_FEATURE      0x2A54    /*!< \brief Running Speed Feature */
#define ATT_UUID_RUNNING_SPEED_MEASUREMENT  0x2A53    /*!< \brief Running Speed Measurement */
#define ATT_UUID_PULSE_OX_FEATURES          0x2A60    /*!< \brief Pulse Oximeter Features */
#define ATT_UUID_PULSE_OX_SPOT_CHECK        0x2A5E    /*!< \brief Pulse Oximeter Features */
#define ATT_UUID_PULSE_OX_CONTINUOUS        0x2A5F    /*!< \brief Pulse Oximeter Features */
#define ATT_UUID_CYCLING_POWER_FEATURE      0x2A65    /*!< \brief Cycling Power Feature */
#define ATT_UUID_CYCLING_POWER_MEASUREMENT  0x2A63    /*!< \brief Cycling Power Measurement */
#define ATT_UUID_CYCLING_SPEED_FEATURE      0x2A5C    /*!< \brief Cycling Speed Feature */
#define ATT_UUID_CYCLING_SPEED_MEASUREMENT  0x2A5B    /*!< \brief Cycling Speed Measurement */
#define ATT_UUID_SENSOR_LOCATION            0x2A5D    /*!< \brief Sensor Location */
#define ATT_UUID_DB_CHANGE_INCREMENT        0x2A99    /*!< \brief Database Change Increment */
#define ATT_UUID_USER_INDEX                 0x2A9A    /*!< \brief User Index */
#define ATT_UUID_WEIGHT_MEAS                0x2A9D    /*!< \brief Weight Measurement */
#define ATT_UUID_WEIGHT_SCALE_FEATURE       0x2A9E    /*!< \brief Weight Scale Feature */
#define ATT_UUID_USER_CONTROL_POINT         0x2A9F    /*!< \brief User Control Point */
#define ATT_UUID_RPAO                       0x2AC9    /*!< \brief Resolvable Prviate Address Only */
#define ATT_UUID_MESH_PRV_DATA_IN           0x2ADB    /*!< \brief Mesh Provisioning Data In */
#define ATT_UUID_MESH_PRV_DATA_OUT          0x2ADC    /*!< \brief Mesh Provisioning Data Out */
#define ATT_UUID_MESH_PROXY_DATA_IN         0x2ADD    /*!< \brief Mesh Proxy Data In */
#define ATT_UUID_MESH_PROXY_DATA_OUT        0x2ADE    /*!< \brief Mesh Proxy Data Out */
#define ATT_UUID_CLIENT_SUPPORTED_FEATURES  0x2B29    /*!< \brief Client Supported Features */
#define ATT_UUID_DATABASE_HASH              0x2B2A    /*!< \brief Database Hash */
#define ATT_UUID_SERVER_SUPPORTED_FEATURES  0x2B3A    /*!< \brief Server Supported Features */
#define ATT_UUID_AIC_INPUT_STATE            0x2B77    /*!< \brief Audio Input Control Input State */
#define ATT_UUID_AIC_GAIN_SETTING_ATTR      0x2B78    /*!< \brief Audio Input Control Gain Setting Attributes */
#define ATT_UUID_AIC_INPUT_TYPE             0x2B79    /*!< \brief Audio Input Control Input Type */
#define ATT_UUID_AIC_INPUT_STATUS           0x2B7A    /*!< \brief Audio Input Control Input Status */
#define ATT_UUID_AIC_AUDIO_INPUT_CTRL       0x2B7B    /*!< \brief Audio Input Control Audio Input Control */
#define ATT_UUID_AIC_AUDIO_INPUT_DESC       0x2B7C    /*!< \brief Audio Input Control Audio Input Description */
#define ATT_UUID_VOLUME_STATE               0x2B7D    /*!< \brief Volume Control State */
#define ATT_UUID_VOLUME_CONTROL_POINT       0x2B7E    /*!< \brief Volume Control Point */
#define ATT_UUID_VOLUME_FLAGS               0x2B7F    /*!< \brief Volume Control Flags */
#define ATT_UUID_VOLUME_OFFSET_STATE        0x2B80    /*!< \brief Volume Offset State */
#define ATT_UUID_AUDIO_LOCATION             0x2B81    /*!< \brief Audio Location */
#define ATT_UUID_VOLUME_OFFSET_CONTROL_PT   0x2B82    /*!< \brief Volume Offset Control Point */
#define ATT_UUID_AUDIO_OUT_DESC             0x2B83    /*!< \brief Audio Output Description */
#define ATT_UUID_CTE_ENABLE                 0x2BAD    /*!< \brief Constant Tone Extension enable */
#define ATT_UUID_CTE_MIN_LEN                0x2BAE    /*!< \brief Constant Tone Extension minimum length */
#define ATT_UUID_CTE_TX_CNT                 0x2BAF    /*!< \brief Constant Tone Extension transmit count */
#define ATT_UUID_CTE_TX_DURATION            0x2BB0    /*!< \brief Constant Tone Extension transmit duration */
#define ATT_UUID_CTE_INTERVAL               0x2BB1    /*!< \brief Constant Tone Extension interval */
#define ATT_UUID_CTE_PHY                    0x2BB2    /*!< \brief Constant Tone Extension PHY */
#define ATT_UUID_MC_MUTE                    0x8FE1    /*!< \brief Microphone Control Mute */
#define ATT_UUID_SNK_PAC                    0x8F96    /*!< \brief Sink PAC */
#define ATT_UUID_SNK_AUDIO_LOC              0x8F97    /*!< \brief Sink audio locations */
#define ATT_UUID_SRC_PAC                    0x8F98    /*!< \brief Source PAC */
#define ATT_UUID_SRC_AUDIO_LOC              0x8F99    /*!< \brief Source audio locations */
#define ATT_UUID_AUDIO_CONT_AVAIL           0x8F9A    /*!< \brief Audio content availability */
#define ATT_UUID_SUP_AUDIO_CONT             0x8F9B    /*!< \brief Supported audio content */
#define ATT_UUID_ASE                        0x8F9C    /*!< \brief Audio Stream Endpoint (ASE) */
#define ATT_UUID_ASE_CP                     0x8F9D    /*!< \brief Audio Stream Endpoint (ASE) Control Point */
#define ATT_UUID_BAS_CP                     0x8F9E    /*!< \brief Broadcast Audio Scan (BAS) Control Point */
#define ATT_UUID_BCAST_RX_STATE             0x8F9F    /*!< \brief Broadcast Receive State */
/**@}*/

/** \name GATT Unit UUIDs
 * BLE Defined GATT Unit UUIDs.
 */
/**@{*/
#define ATT_UUID_UNITLESS                   0x2700    /*!< \brief unitless */
#define ATT_UUID_LENGTH_M                   0x2701    /*!< \brief length metre */
#define ATT_UUID_MASS_KG                    0x2702    /*!< \brief mass kilogram */
#define ATT_UUID_TIME_SEC                   0x2703    /*!< \brief time second */
#define ATT_UUID_ELECTRIC_CURRENT_AMP       0x2704    /*!< \brief electric current ampere */
#define ATT_UUID_THERMO_TEMP_K              0x2705    /*!< \brief thermodynamic temperature kelvin */
#define ATT_UUID_AMOUNT_OF_SUBSTANCE_MOLE   0x2706    /*!< \brief amount of substance mole */
#define ATT_UUID_LUMINOUS_INTENSITY_CAND    0x2707    /*!< \brief luminous intensity candela */
#define ATT_UUID_AREA_SQ_M                  0x2710    /*!< \brief area square metres */
#define ATT_UUID_VOLUME_CU_M                0x2711    /*!< \brief volume cubic metres */
#define ATT_UUID_VELOCITY_MPS               0x2712    /*!< \brief velocity metres per second */
#define ATT_UUID_ACCELERATION_MPS_SQ        0x2713    /*!< \brief acceleration metres per second squared */
#define ATT_UUID_WAVENUMBER_RECIPROCAL_M    0x2714    /*!< \brief wavenumber reciprocal metre */
#define ATT_UUID_DENSITY_KG_PER_CU_M        0x2715    /*!< \brief density kilogram per cubic metre */
#define ATT_UUID_SURFACE_DENS_KG_PER_SQ_M   0x2716    /*!< \brief surface density kilogram per square metre */
#define ATT_UUID_SPECIFIC_VOL_CU_M_PER_KG   0x2717    /*!< \brief specific volume cubic metre per kilogram */
#define ATT_UUID_CURRENT_DENS_AMP_PER_SQ_M  0x2718    /*!< \brief current density ampere per square metre */
#define ATT_UUID_MAG_FIELD_STR_AMP_PER_M    0x2719    /*!< \brief magnetic field strength ampere per metre */
#define ATT_UUID_AMOUNT_CONC_MOLE_PER_CU_M  0x271A    /*!< \brief amount concentration mole per cubic metre */
#define ATT_UUID_MASS_CONC_KG_PER_CU_M      0x271B    /*!< \brief mass concentration kilogram per cubic metre */
#define ATT_UUID_LUM_CAND_PER_SQ_M          0x271C    /*!< \brief luminance candela per square metre */
#define ATT_UUID_REFRACTIVE_INDEX           0x271D    /*!< \brief refractive index */
#define ATT_UUID_RELATIVE_PERMEABILITY      0x271E    /*!< \brief relative permeability */
#define ATT_UUID_PLANE_ANGLE_R              0x2720    /*!< \brief plane angle radian */
#define ATT_UUID_SOLID_ANGLE_STER           0x2721    /*!< \brief solid angle steradian */
#define ATT_UUID_FREQUENCY_HERTZ            0x2722    /*!< \brief frequency hertz */
#define ATT_UUID_FORCE_NEWT                 0x2723    /*!< \brief force newton */
#define ATT_UUID_PRESSURE_PASCAL            0x2724    /*!< \brief pressure pascal */
#define ATT_UUID_ENERGY_J                   0x2725    /*!< \brief energy joule */
#define ATT_UUID_POWER_W                    0x2726    /*!< \brief power watt */
#define ATT_UUID_ELECTRIC_CHG_C             0x2727    /*!< \brief electric charge coulomb */
#define ATT_UUID_ELECTRIC_POTENTIAL_VOLT    0x2728    /*!< \brief electric potential difference volt */
#define ATT_UUID_CAPACITANCE_F              0x2729    /*!< \brief capacitance farad */
#define ATT_UUID_ELECTRIC_RESISTANCE_OHM    0x272A    /*!< \brief electric resistance ohm */
#define ATT_UUID_ELECTRIC_COND_SIEMENS      0x272B    /*!< \brief electric conductance siemens */
#define ATT_UUID_MAGNETIC_FLEX_WEBER        0x272C    /*!< \brief magnetic flex weber */
#define ATT_UUID_MAGNETIC_FLEX_DENS_TESLA   0x272D    /*!< \brief magnetic flex density tesla */
#define ATT_UUID_INDUCTANCE_H               0x272E    /*!< \brief inductance henry */
#define ATT_UUID_C_TEMP_DEG_C               0x272F    /*!< \brief Celsius temperature degree Celsius */
#define ATT_UUID_LUMINOUS_FLUX_LUMEN        0x2730    /*!< \brief luminous flux lumen */
#define ATT_UUID_ILLUMINANCE_LUX            0x2731    /*!< \brief illuminance lux */
#define ATT_UUID_RADIONUCLIDE_BECQUEREL     0x2732    /*!< \brief activity referred to a radionuclide becquerel */
#define ATT_UUID_ABSORBED_DOSE_GRAY         0x2733    /*!< \brief absorbed dose gray */
#define ATT_UUID_DOSE_EQUIVALENT_SIEVERT    0x2734    /*!< \brief dose equivalent sievert */
#define ATT_UUID_CATALYTIC_ACTIVITY_KATAL   0x2735    /*!< \brief catalytic activity katal */
#define ATT_UUID_DYNAMIC_VISC_PASCAL_SEC    0x2740    /*!< \brief dynamic viscosity pascal second */
#define ATT_UUID_MOMENT_OF_FORCE_NEWT_M     0x2741    /*!< \brief moment of force newton metre */
#define ATT_UUID_SURFACE_TENSION_NEWT_PER_M 0x2742    /*!< \brief surface tension newton per metre */
#define ATT_UUID_ANG_VELOCITY_R_PER_SEC     0x2743    /*!< \brief angular velocity radian per second */
#define ATT_UUID_ANG_ACCEL_R_PER_SEC_SQD    0x2744    /*!< \brief angular acceleration radian per second squared */
#define ATT_UUID_HEAT_FLUX_DEN_W_PER_SQ_M   0x2745    /*!< \brief heat flux density watt per square metre */
#define ATT_UUID_HEAT_CAP_J_PER_K           0x2746    /*!< \brief heat capacity joule per kelvin */
#define ATT_UUID_SPEC_HEAT_CAP_J_PER_KG_K   0x2747    /*!< \brief specific heat capacity joule per kilogram kelvin */
#define ATT_UUID_SPEC_ENERGY_J_PER_KG       0x2748    /*!< \brief specific energy joule per kilogram */
#define ATT_UUID_THERMAL_COND_W_PER_M_K     0x2749    /*!< \brief thermal conductivity watt per metre kelvin */
#define ATT_UUID_ENERGY_DENSITY_J_PER_CU_M  0x274A    /*!< \brief energy density joule per cubic metre */
#define ATT_UUID_ELEC_FIELD_STR_VOLT_PER_M  0x274B    /*!< \brief electric field strength volt per metre */
#define ATT_UUID_ELEC_CHG_DENS_C_PER_CU_M   0x274C    /*!< \brief electric charge density coulomb per cubic metre */
#define ATT_UUID_SURF_CHG_DENS_C_PER_SQ_M   0x274D    /*!< \brief surface charge density coulomb per square metre */
#define ATT_UUID_ELEC_FLUX_DENS_C_PER_SQ_M  0x274E    /*!< \brief electric flux density coulomb per square metre */
#define ATT_UUID_PERMITTIVITY_F_PER_M       0x274F    /*!< \brief permittivity farad per metre */
#define ATT_UUID_PERMEABILITY_H_PER_M       0x2750    /*!< \brief permeability henry per metre */
#define ATT_UUID_MOLAR_ENERGY_J_PER_MOLE    0x2751    /*!< \brief molar energy joule per mole */
#define ATT_UUID_MOLAR_ENTROPY_J_PER_MOLE_K 0x2752    /*!< \brief molar entropy joule per mole kelvin */
#define ATT_UUID_EXPOSURE_C_PER_KG          0x2753    /*!< \brief exposure coulomb per kilogram */
#define ATT_UUID_DOSE_RATE_GRAY_PER_SEC     0x2754    /*!< \brief absorbed dose rate gray per second */
#define ATT_UUID_RT_INTENSITY_W_PER_STER    0x2755    /*!< \brief radiant intensity watt per steradian */
#define ATT_UUID_RCE_W_PER_SQ_METER_STER    0x2756    /*!< \brief radiance watt per square meter steradian */
#define ATT_UUID_CATALYTIC_KATAL_PER_CU_M   0x2757    /*!< \brief catalytic activity concentration katal per cubic metre */
#define ATT_UUID_TIME_MIN                   0x2760    /*!< \brief time minute */
#define ATT_UUID_TIME_HR                    0x2761    /*!< \brief time hour */
#define ATT_UUID_TIME_DAY                   0x2762    /*!< \brief time day */
#define ATT_UUID_PLANE_ANGLE_DEG            0x2763    /*!< \brief plane angle degree */
#define ATT_UUID_PLANE_ANGLE_MIN            0x2764    /*!< \brief plane angle minute */
#define ATT_UUID_PLANE_ANGLE_SEC            0x2765    /*!< \brief plane angle second */
#define ATT_UUID_AREA_HECTARE               0x2766    /*!< \brief area hectare */
#define ATT_UUID_VOLUME_L                   0x2767    /*!< \brief volume litre */
#define ATT_UUID_MASS_TONNE                 0x2768    /*!< \brief mass tonne */
#define ATT_UUID_PRESSURE_BAR               0x2780    /*!< \brief pressure bar */
#define ATT_UUID_PRESSURE_MM                0x2781    /*!< \brief pressure millimetre of mercury */
#define ATT_UUID_LENGTH_ANGSTROM            0x2782    /*!< \brief length angstrom */
#define ATT_UUID_LENGTH_NAUTICAL_MILE       0x2783    /*!< \brief length nautical mile */
#define ATT_UUID_AREA_BARN                  0x2784    /*!< \brief area barn */
#define ATT_UUID_VELOCITY_KNOT              0x2785    /*!< \brief velocity knot */
#define ATT_UUID_LOG_RADIO_QUANT_NEPER      0x2786    /*!< \brief logarithmic radio quantity neper */
#define ATT_UUID_LOG_RADIO_QUANT_BEL        0x2787    /*!< \brief logarithmic radio quantity bel */
#define ATT_UUID_LOG_RADIO_QUANT_DB         0x2788    /*!< \brief logarithmic radio quantity decibel */
#define ATT_UUID_LENGTH_YARD                0x27A0    /*!< \brief length yard */
#define ATT_UUID_LENGTH_PARSEC              0x27A1    /*!< \brief length parsec */
#define ATT_UUID_LENGTH_IN                  0x27A2    /*!< \brief length inch */
#define ATT_UUID_LENGTH_FOOT                0x27A3    /*!< \brief length foot */
#define ATT_UUID_LENGTH_MILE                0x27A4    /*!< \brief length mile */
#define ATT_UUID_PRESSURE_POUND_PER_SQ_IN   0x27A5    /*!< \brief pressure pound-force per square inch */
#define ATT_UUID_VELOCITY_KPH               0x27A6    /*!< \brief velocity kilometre per hour */
#define ATT_UUID_VELOCITY_MPH               0x27A7    /*!< \brief velocity mile per hour */
#define ATT_UUID_ANG_VELOCITY_RPM           0x27A8    /*!< \brief angular velocity revolution per minute */
#define ATT_UUID_ENERGY_GRAM_CALORIE        0x27A9    /*!< \brief energy gram calorie */
#define ATT_UUID_ENERGY_KG_CALORIE          0x27AA    /*!< \brief energy kilogram calorie */
#define ATT_UUID_ENERGY_KILOWATT_HR         0x27AB    /*!< \brief energy kilowatt hour */
#define ATT_UUID_THERM_TEMP_F               0x27AC    /*!< \brief thermodynamic temperature degree Fahrenheit */
#define ATT_UUID_PERCENTAGE                 0x27AD    /*!< \brief percentage */
#define ATT_UUID_PER_MILLE                  0x27AE    /*!< \brief per mille */
#define ATT_UUID_PERIOD_BEATS_PER_MIN       0x27AF    /*!< \brief period beats per minute */
#define ATT_UUID_ELECTRIC_CHG_AMP_HRS       0x27B0    /*!< \brief electric charge ampere hours */
#define ATT_UUID_MASS_DENSITY_MG_PER_DL     0x27B1    /*!< \brief mass density milligram per decilitre */
#define ATT_UUID_MASS_DENSITY_MMOLE_PER_L   0x27B2    /*!< \brief mass density millimole per litre */
#define ATT_UUID_TIME_YEAR                  0x27B3    /*!< \brief time year */
#define ATT_UUID_TIME_MONTH                 0x27B4    /*!< \brief time month */
/**@}*/

/** \name Arm Ltd. proprietary UUIDs
 * propertietary services defined by Arm Ltd.
 */
/**@{*/

/*! \brief Base UUID:  E0262760-08C2-11E1-9073-0E8AC72EXXXX */
#define ATT_UUID_ARM_BASE                   0x2E, 0xC7, 0x8A, 0x0E, 0x73, 0x90, \
                                            0xE1, 0x11, 0xC2, 0x08, 0x60, 0x27, 0x26, 0xE0

/*! \brief Macro for building Arm Ltd. UUIDs */
#define ATT_UUID_ARM_BUILD(part)            UINT16_TO_BYTES(part), ATT_UUID_ARM_BASE


/** \brief Partial proprietary service P1 UUID */
#define ATT_UUID_P1_SERVICE_PART            0x1001

/** \brief Partial proprietary characteristic data D1 UUID */
#define ATT_UUID_D1_DATA_PART               0x0001

/*! \brief Proprietary services */
#define ATT_UUID_P1_SERVICE                 ATT_UUID_ARM_BUILD(ATT_UUID_P1_SERVICE_PART)

/*! \brief Proprietary characteristics */
#define ATT_UUID_D1_DATA                    ATT_UUID_ARM_BUILD(ATT_UUID_D1_DATA_PART)
/**@}*/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/** \name ATT Service UUID Variables
 *
 */
/**@{*/
extern const uint8_t attGapSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Generic Access Profile Service */
extern const uint8_t attGattSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Generic Attribute Profile Service */
extern const uint8_t attIasSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Immediate Alert Service */
extern const uint8_t attLlsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Link Loss Service */
extern const uint8_t attTpsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Tx Power Service */
extern const uint8_t attCtsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Current Time Service */
extern const uint8_t attRtusSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Reference Time Update Service */
extern const uint8_t attNdcsSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Next DST Change Service */
extern const uint8_t attGlsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Glucose Service */
extern const uint8_t attHtsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Health Thermometer Service */
extern const uint8_t attDisSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Device Information Service */
extern const uint8_t attNwaSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Network Availability Service */
extern const uint8_t attWdsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Watchdog Service */
extern const uint8_t attHrsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Heart Rate Service */
extern const uint8_t attPassSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Phone Alert Status Service */
extern const uint8_t attBattSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Battery Service */
extern const uint8_t attBpsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Blood Pressure Service */
extern const uint8_t attAnsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Alert Notification Service */
extern const uint8_t attHidSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Human Interface Device Service */
extern const uint8_t attSpsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Scan Parameter Service */
extern const uint8_t attPlxsSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Pulse Oximeter Service */
extern const uint8_t attUdsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief User Data Service */
extern const uint8_t attMprvSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Mesh Provisioning Service */
extern const uint8_t attMprxSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Mesh Proxy Service */
extern const uint8_t attWssSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Weight scale service */
extern const uint8_t attCteSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Constant Tone Extension service */
extern const uint8_t attAicsSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Audio Input Control service */
extern const uint8_t attMicsSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Microphone Control service */
extern const uint8_t attVcsSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Volume Control service */
extern const uint8_t attVocsSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Volume Offset Control service */
extern const uint8_t attPacSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Audio capability service */
extern const uint8_t attAscSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Audio Stream Endpoint Service */
extern const uint8_t attBasSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Broadcast Audio Scan Service */
extern const uint8_t attBscAaSvcUuid[ATT_16_UUID_LEN];   /*!< \brief Basic Audio Announcement Service */
extern const uint8_t attBcstAaSvcUuid[ATT_16_UUID_LEN];  /*!< \brief Broadcast Audio Announcement Service */
/**@}*/

/** \name GATT UUID Variables
 *
 */
/**@{*/
extern const uint8_t attPrimSvcUuid[ATT_16_UUID_LEN];    /*!< \brief Primary Service */
extern const uint8_t attSecSvcUuid[ATT_16_UUID_LEN];     /*!< \brief Secondary Service */
extern const uint8_t attIncUuid[ATT_16_UUID_LEN];        /*!< \brief Include */
extern const uint8_t attChUuid[ATT_16_UUID_LEN];         /*!< \brief Characteristic */
/**@}*/

/** \name GATT Characteristic Descriptor UUID Variables
 *
 */
/**@{*/
extern const uint8_t attChExtUuid[ATT_16_UUID_LEN];      /*!< \brief Characteristic Extended Properties */
extern const uint8_t attChUserDescUuid[ATT_16_UUID_LEN]; /*!< \brief Characteristic User Description */
extern const uint8_t attCliChCfgUuid[ATT_16_UUID_LEN];   /*!< \brief Client Characteristic Configuration */
extern const uint8_t attSrvChCfgUuid[ATT_16_UUID_LEN];   /*!< \brief Server Characteristic Configuration */
extern const uint8_t attChPresFmtUuid[ATT_16_UUID_LEN];  /*!< \brief Characteristic Presentation Format */
extern const uint8_t attAggFmtUuid[ATT_16_UUID_LEN];     /*!< \brief Characteristic Aggregate Format */
extern const uint8_t attHidErmUuid[ATT_16_UUID_LEN];     /*!< \brief HID External Report Reference */
extern const uint8_t attHidRimUuid[ATT_16_UUID_LEN];     /*!< \brief HID Report ID Mapping */
extern const uint8_t attValRangeUuid[ATT_16_UUID_LEN];   /*!< \brief Valid Range */
/**@}*/

/** \name GATT Characteristic UUID Variables
 *
 */
/**@{*/
extern const uint8_t attDnChUuid[ATT_16_UUID_LEN];       /*!< \brief Device Name */
extern const uint8_t attApChUuid[ATT_16_UUID_LEN];       /*!< \brief Appearance */
extern const uint8_t attPpfChUuid[ATT_16_UUID_LEN];      /*!< \brief Peripheral Privacy Flag */
extern const uint8_t attRaChUuid[ATT_16_UUID_LEN];       /*!< \brief Reconnection Address */
extern const uint8_t attPpcpChUuid[ATT_16_UUID_LEN];     /*!< \brief Peripheral Preferred Connection Parameters */
extern const uint8_t attScChUuid[ATT_16_UUID_LEN];       /*!< \brief Service Changed */
extern const uint8_t attAlChUuid[ATT_16_UUID_LEN];       /*!< \brief Alert Level */
extern const uint8_t attTxpChUuid[ATT_16_UUID_LEN];      /*!< \brief Tx Power Level */
extern const uint8_t attDtChUuid[ATT_16_UUID_LEN];       /*!< \brief Date Time */
extern const uint8_t attDwChUuid[ATT_16_UUID_LEN];       /*!< \brief Day of Week */
extern const uint8_t attDdtChUuid[ATT_16_UUID_LEN];      /*!< \brief Day Date Time */
extern const uint8_t attEt100ChUuid[ATT_16_UUID_LEN];    /*!< \brief Exact Time 100 */
extern const uint8_t attEt256ChUuid[ATT_16_UUID_LEN];    /*!< \brief Exact Time 256 */
extern const uint8_t attDstoChUuid[ATT_16_UUID_LEN];     /*!< \brief DST Offset */
extern const uint8_t attTzChUuid[ATT_16_UUID_LEN];       /*!< \brief Time Zone */
extern const uint8_t attLtiChUuid[ATT_16_UUID_LEN];      /*!< \brief Local Time Information */
extern const uint8_t attStzChUuid[ATT_16_UUID_LEN];      /*!< \brief Secondary Time Zone */
extern const uint8_t attTdstChUuid[ATT_16_UUID_LEN];     /*!< \brief Time with DST */
extern const uint8_t attTaChUuid[ATT_16_UUID_LEN];       /*!< \brief Time Accuracy */
extern const uint8_t attTsChUuid[ATT_16_UUID_LEN];       /*!< \brief Time Source */
extern const uint8_t attRtiChUuid[ATT_16_UUID_LEN];      /*!< \brief Reference Time Information */
extern const uint8_t attTbChUuid[ATT_16_UUID_LEN];       /*!< \brief Time Broadcast */
extern const uint8_t attTucpChUuid[ATT_16_UUID_LEN];     /*!< \brief Time Update Control Point */
extern const uint8_t attTusChUuid[ATT_16_UUID_LEN];      /*!< \brief Time Update State */
extern const uint8_t attGlmChUuid[ATT_16_UUID_LEN];      /*!< \brief Glucose Measurement */
extern const uint8_t attBlChUuid[ATT_16_UUID_LEN];       /*!< \brief Battery Level */
extern const uint8_t attBpsChUuid[ATT_16_UUID_LEN];      /*!< \brief Battery Power State */
extern const uint8_t attBlsChUuid[ATT_16_UUID_LEN];      /*!< \brief Battery Level State */
extern const uint8_t attTmChUuid[ATT_16_UUID_LEN];       /*!< \brief Temperature Measurement */
extern const uint8_t attTtChUuid[ATT_16_UUID_LEN];       /*!< \brief Temperature Type */
extern const uint8_t attItChUuid[ATT_16_UUID_LEN];       /*!< \brief Intermediate Temperature */
extern const uint8_t attTcelChUuid[ATT_16_UUID_LEN];     /*!< \brief Temperature Celsius */
extern const uint8_t attTfahChUuid[ATT_16_UUID_LEN];     /*!< \brief Temperature Fahrenheit */
extern const uint8_t attSidChUuid[ATT_16_UUID_LEN];      /*!< \brief System ID */
extern const uint8_t attMnsChUuid[ATT_16_UUID_LEN];      /*!< \brief Model Number String */
extern const uint8_t attSnsChUuid[ATT_16_UUID_LEN];      /*!< \brief Serial Number String */
extern const uint8_t attFrsChUuid[ATT_16_UUID_LEN];      /*!< \brief Firmware Revision String */
extern const uint8_t attHrsChUuid[ATT_16_UUID_LEN];      /*!< \brief Hardware Revision String */
extern const uint8_t attSrsChUuid[ATT_16_UUID_LEN];      /*!< \brief Software Revision String */
extern const uint8_t attMfnsChUuid[ATT_16_UUID_LEN];     /*!< \brief Manufacturer Name String */
extern const uint8_t attIeeeChUuid[ATT_16_UUID_LEN];     /*!< \brief IEEE 11073-20601 Regulatory Certification Data List */
extern const uint8_t attCtChUuid[ATT_16_UUID_LEN];       /*!< \brief Current Time */
extern const uint8_t attElChUuid[ATT_16_UUID_LEN];       /*!< \brief Elevation */
extern const uint8_t attLatChUuid[ATT_16_UUID_LEN];      /*!< \brief Latitude */
extern const uint8_t attLongChUuid[ATT_16_UUID_LEN];     /*!< \brief Longitude */
extern const uint8_t attP2dChUuid[ATT_16_UUID_LEN];      /*!< \brief Position 2D */
extern const uint8_t attP3dChUuid[ATT_16_UUID_LEN];      /*!< \brief Position 3D */
extern const uint8_t attVidChUuid[ATT_16_UUID_LEN];      /*!< \brief Vendor ID */
extern const uint8_t attGlmcChUuid[ATT_16_UUID_LEN];     /*!< \brief Glucose Measurement Context */
extern const uint8_t attBpmChUuid[ATT_16_UUID_LEN];      /*!< \brief Blood Pressure Measurement */
extern const uint8_t attIcpChUuid[ATT_16_UUID_LEN];      /*!< \brief Intermediate Cuff Pressure */
extern const uint8_t attHrmChUuid[ATT_16_UUID_LEN];      /*!< \brief Heart Rate Measurement */
extern const uint8_t attBslChUuid[ATT_16_UUID_LEN];      /*!< \brief Body Sensor Location */
extern const uint8_t attHrcpChUuid[ATT_16_UUID_LEN];     /*!< \brief Heart Rate Control Point */
extern const uint8_t attRemChUuid[ATT_16_UUID_LEN];      /*!< \brief Removable */
extern const uint8_t attSrChUuid[ATT_16_UUID_LEN];       /*!< \brief Service Required */
extern const uint8_t attStcChUuid[ATT_16_UUID_LEN];      /*!< \brief Scientific Temperature in Celsius */
extern const uint8_t attStrChUuid[ATT_16_UUID_LEN];      /*!< \brief String */
extern const uint8_t attNwaChUuid[ATT_16_UUID_LEN];      /*!< \brief Network Availability */
extern const uint8_t attAsChUuid[ATT_16_UUID_LEN];       /*!< \brief Alert Status */
extern const uint8_t attRcpChUuid[ATT_16_UUID_LEN];      /*!< \brief Ringer Control Point */
extern const uint8_t attRsChUuid[ATT_16_UUID_LEN];       /*!< \brief Ringer Setting */
extern const uint8_t attAcbmChUuid[ATT_16_UUID_LEN];     /*!< \brief Alert Category ID Bit Mask */
extern const uint8_t attAcChUuid[ATT_16_UUID_LEN];       /*!< \brief Alert Category ID */
extern const uint8_t attAncpChUuid[ATT_16_UUID_LEN];     /*!< \brief Alert Notification Control Point */
extern const uint8_t attUasChUuid[ATT_16_UUID_LEN];      /*!< \brief Unread Alert Status */
extern const uint8_t attNaChUuid[ATT_16_UUID_LEN];       /*!< \brief New Alert */
extern const uint8_t attSnacChUuid[ATT_16_UUID_LEN];     /*!< \brief Supported New Alert Category */
extern const uint8_t attSuacChUuid[ATT_16_UUID_LEN];     /*!< \brief Supported Unread Alert Category */
extern const uint8_t attBpfChUuid[ATT_16_UUID_LEN];      /*!< \brief Blood Pressure Feature */
extern const uint8_t attHidBmiChUuid[ATT_16_UUID_LEN];   /*!< \brief HID Information */
extern const uint8_t attHidBkiChUuid[ATT_16_UUID_LEN];   /*!< \brief HID Information */
extern const uint8_t attHidBkoChUuid[ATT_16_UUID_LEN];   /*!< \brief HID Information */
extern const uint8_t attHidiChUuid[ATT_16_UUID_LEN];     /*!< \brief HID Information */
extern const uint8_t attHidRmChUuid[ATT_16_UUID_LEN];    /*!< \brief Report Map */
extern const uint8_t attHidcpChUuid[ATT_16_UUID_LEN];    /*!< \brief HID Control Point */
extern const uint8_t attHidRepChUuid[ATT_16_UUID_LEN];   /*!< \brief Report */
extern const uint8_t attHidPmChUuid[ATT_16_UUID_LEN];    /*!< \brief Protocol Mode */
extern const uint8_t attSiwChUuid[ATT_16_UUID_LEN];      /*!< \brief Scan Interval Window */
extern const uint8_t attPnpChUuid[ATT_16_UUID_LEN];      /*!< \brief PnP ID */
extern const uint8_t attGlfChUuid[ATT_16_UUID_LEN];      /*!< \brief Glucose Feature */
extern const uint8_t attRacpChUuid[ATT_16_UUID_LEN];     /*!< \brief Record Access Control Point */
extern const uint8_t attCarChUuid[ATT_16_UUID_LEN];      /*!< \brief Central Address Resolution */
extern const uint8_t attRsfChUuid[ATT_16_UUID_LEN];      /*!< \brief Running Speed Features */
extern const uint8_t attRsmChUuid[ATT_16_UUID_LEN];      /*!< \brief Running Speed Measurement */
extern const uint8_t attCpfChUuid[ATT_16_UUID_LEN];      /*!< \brief Cycling Power Features */
extern const uint8_t attCpmChUuid[ATT_16_UUID_LEN];      /*!< \brief Cycling Power Measurement */
extern const uint8_t attCsfChUuid[ATT_16_UUID_LEN];      /*!< \brief Cycling Speed Features */
extern const uint8_t attCsmChUuid[ATT_16_UUID_LEN];      /*!< \brief Cycling Speed Measurement */
extern const uint8_t attSlChUuid[ATT_16_UUID_LEN];       /*!< \brief Sensor Location */
extern const uint8_t attPlxfChUuid[ATT_16_UUID_LEN];     /*!< \brief Pulse Oximeter Features */
extern const uint8_t attPlxscmChUuid[ATT_16_UUID_LEN];   /*!< \brief Pulse Oximeter Spot Check Measurement */
extern const uint8_t attPlxcmChUuid[ATT_16_UUID_LEN];    /*!< \brief Pulse Oximeter Continuous Measurement */
extern const uint8_t attRpaoChUuid[ATT_16_UUID_LEN];     /*!< \brief Resolvable Private Address Only */
extern const uint8_t attDbciChUuid[ATT_16_UUID_LEN];     /*!< \brief Database Change Increment */
extern const uint8_t attUiChUuid[ATT_16_UUID_LEN];       /*!< \brief User Index */
extern const uint8_t attUcpChUuid[ATT_16_UUID_LEN];      /*!< \brief User Control Point */
extern const uint8_t attMprvDinChUuid[ATT_16_UUID_LEN];  /*!< \brief Mesh Provisioning Data In */
extern const uint8_t attMprvDoutChUuid[ATT_16_UUID_LEN]; /*!< \brief Mesh Provisioning Data Out */
extern const uint8_t attMprxDinChUuid[ATT_16_UUID_LEN];  /*!< \brief Mesh Proxy Data In */
extern const uint8_t attMprxDoutChUuid[ATT_16_UUID_LEN]; /*!< \brief Mesh Proxy Data Out */
extern const uint8_t attWmChUuid[ATT_16_UUID_LEN];       /*!< \brief Weight measurement */
extern const uint8_t attWsfChUuid[ATT_16_UUID_LEN];      /*!< \brief Weight scale feature */
extern const uint8_t attGattCsfChUuid[ATT_16_UUID_LEN];  /*!< \brief Client supported features */
extern const uint8_t attGattDbhChUuid[ATT_16_UUID_LEN];  /*!< \brief Database hash */
extern const uint8_t attCteEnChUuid[ATT_16_UUID_LEN];    /*!< \brief Constant Tone Extension enable */
extern const uint8_t attCteMinLenChUuid[ATT_16_UUID_LEN];/*!< \brief Constant Tone Extension minimum length */
extern const uint8_t attCteTxCntChUuid[ATT_16_UUID_LEN]; /*!< \brief Constant Tone Extension minimum transmit count */
extern const uint8_t attCteTxDurChUuid[ATT_16_UUID_LEN]; /*!< \brief Constant Tone Extension transmit duration */
extern const uint8_t attCteIntChUuid[ATT_16_UUID_LEN];   /*!< \brief Constant Tone Extension interval */
extern const uint8_t attCtePhyChUuid[ATT_16_UUID_LEN];   /*!< \brief Constant Tone Extension PHY */
extern const uint8_t attSsfChUuid[ATT_16_UUID_LEN];     /*!< \brief Server supported features */
extern const uint8_t attAicsStChUuid[ATT_16_UUID_LEN];   /*!< \brief Audio Input Control input status */
extern const uint8_t attAicsGsaChUuid[ATT_16_UUID_LEN];  /*!< \brief Audio Input Control gain settings attributes */
extern const uint8_t attAicsItChUuid[ATT_16_UUID_LEN];   /*!< \brief Audio Input Control input type */
extern const uint8_t attAicsStatChUuid[ATT_16_UUID_LEN]; /*!< \brief Audio Input Control input status */
extern const uint8_t attAicsAicChUuid[ATT_16_UUID_LEN];  /*!< \brief Audio Input Control audio input control point */
extern const uint8_t attAicsAidhUuid[ATT_16_UUID_LEN];   /*!< \brief Audio Input Control audio input description */
extern const uint8_t attMicsMuteChUuid[ATT_16_UUID_LEN]; /*!< \brief Microphone Control mute */
extern const uint8_t attVcsStateChUuid[ATT_16_UUID_LEN]; /*!< \brief Volume Control state */
extern const uint8_t attVcsCpChUuid[ATT_16_UUID_LEN];    /*!< \brief Volume Control Point */
extern const uint8_t attVcsFlagsChUuid[ATT_16_UUID_LEN]; /*!< \brief Volume Control flags */
extern const uint8_t attVocsStateChUuid[ATT_16_UUID_LEN];/*!< \brief Volume Offset Control state */
extern const uint8_t attVocsLocChUuid[ATT_16_UUID_LEN];  /*!< \brief Volume Offset Control audio location */
extern const uint8_t attVocsCpChUuid[ATT_16_UUID_LEN];   /*!< \brief Volume Offset Control control point */
extern const uint8_t attVocsDescChUuid[ATT_16_UUID_LEN]; /*!< \brief Volume Offset Control description */
extern const uint8_t attSnkPacChUuid[ATT_16_UUID_LEN];   /*!< \brief Sink PAC */
extern const uint8_t attSnkAudLocChUuid[ATT_16_UUID_LEN];/*!< \brief Sink audio locations */
extern const uint8_t attSrcPacChUuid[ATT_16_UUID_LEN];   /*!< \brief Source PAC */
extern const uint8_t attSrcAudLocChUuid[ATT_16_UUID_LEN];/*!< \brief Source audio locations */
extern const uint8_t attAudCntAvChUuid[ATT_16_UUID_LEN]; /*!< \brief Audio Content Availability */
extern const uint8_t attSupAudCntChUuid[ATT_16_UUID_LEN];/*!< \brief Supported Audio Content */
extern const uint8_t attAseChUuid[ATT_16_UUID_LEN];      /*!< \brief Audio Stream Endpoint (ASE) */
extern const uint8_t attAseCpChUuid[ATT_16_UUID_LEN];    /*!< \brief Audio Stream Endpoint (ASE) Control Point */
extern const uint8_t attBasCpChUuid[ATT_16_UUID_LEN];    /*!< \brief Broadcast Audio Scan (BAS) Control Point */
extern const uint8_t attBcRxStateChUuid[ATT_16_UUID_LEN];/*!< \brief Broadcast Receive State */
/**@}*/

/*! \} */    /* STACK_ATT_API */

#ifdef __cplusplus
};
#endif

#endif /* ATT_UUID_H */
