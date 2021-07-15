/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  ATT UUID constants.
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

#include "wsf_types.h"
#include "util/bstream.h"
#include "att_uuid.h"

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! Service UUIDs */
const uint8_t attGapSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_GAP_SERVICE)};
const uint8_t attGattSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_GATT_SERVICE)};
const uint8_t attIasSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_IMMEDIATE_ALERT_SERVICE)};
const uint8_t attLlsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_LINK_LOSS_SERVICE)};
const uint8_t attTpsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_TX_POWER_SERVICE)};
const uint8_t attCtsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_CURRENT_TIME_SERVICE)};
const uint8_t attRtusSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_REF_TIME_UPDATE_SERVICE)};
const uint8_t attNdcsSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_DST_CHANGE_SERVICE)};
const uint8_t attGlsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_GLUCOSE_SERVICE)};
const uint8_t attHtsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HEALTH_THERM_SERVICE)};
const uint8_t attDisSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE)};
const uint8_t attNwaSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_NETWORK_AVAIL_SERVICE)};
const uint8_t attWdsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_WATCHDOG_SERVICE)};
const uint8_t attHrsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HEART_RATE_SERVICE)};
const uint8_t attPassSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_PHONE_ALERT_SERVICE)};
const uint8_t attBattSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_BATTERY_SERVICE)};
const uint8_t attBpsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_BLOOD_PRESSURE_SERVICE)};
const uint8_t attAnsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_ALERT_NOTIF_SERVICE)};
const uint8_t attHidSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HID_SERVICE)};
const uint8_t attSpsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_SCAN_PARAM_SERVICE)};
const uint8_t attPlxsSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_PULSE_OXIMITER_SERVICE)};
const uint8_t attUdsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_USER_DATA_SERVICE)};
const uint8_t attMprvSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_MESH_PRV_SERVICE)};
const uint8_t attMprxSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_MESH_PROXY_SERVICE)};
const uint8_t attWssSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_WEIGHT_SCALE_SERVICE)};
const uint8_t attCteSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_CONSTANT_TONE_SERVICE)};
const uint8_t attAicsSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_AUDIO_INPUT_CTRL_SERVICE)};
const uint8_t attMicsSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_MICROPHONE_CTRL_SERVICE)};
const uint8_t attVcsSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_VOLUME_CTRL_SERVICE)};
const uint8_t attVocsSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_VOLUME_OFFSET_CTRL_SERVICE)};
const uint8_t attPacSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_PUB_AUDIO_CAP_SERVICE)};
const uint8_t attAscSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_AUDIO_STRM_CTRL_SERVICE)};
const uint8_t attBasSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_BCAST_AUDIO_SCAN_SERVICE)};
const uint8_t attBscAaSvcUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_BASIC_AUDIO_ANNC_SERVICE)};
const uint8_t attBcstAaSvcUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_BCAST_AUDIO_ANNC_SERVICE)};

/*! GATT UUIDs */
const uint8_t attPrimSvcUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_PRIMARY_SERVICE)};
const uint8_t attSecSvcUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_SECONDARY_SERVICE)};
const uint8_t attIncUuid[ATT_16_UUID_LEN] =        {UINT16_TO_BYTES(ATT_UUID_INCLUDE)};
const uint8_t attChUuid[ATT_16_UUID_LEN] =         {UINT16_TO_BYTES(ATT_UUID_CHARACTERISTIC)};

/*! Descriptor UUIDs */
const uint8_t attChExtUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_CHARACTERISTIC_EXT)};
const uint8_t attChUserDescUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_CHAR_USER_DESC)};
const uint8_t attCliChCfgUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_CLIENT_CHAR_CONFIG)};
const uint8_t attSrvChCfgUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_SERVER_CHAR_CONFIG)};
const uint8_t attChPresFmtUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_CHAR_PRES_FORMAT)};
const uint8_t attAggFmtUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_AGGREGATE_FORMAT)};
const uint8_t attValRangeUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_VALID_RANGE)};
const uint8_t attHidErmUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HID_EXT_REPORT_MAPPING)};
const uint8_t attHidRimUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HID_REPORT_ID_MAPPING)};

/*! Characteristic UUIDs */
const uint8_t attDnChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_DEVICE_NAME)};
const uint8_t attApChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_APPEARANCE)};
const uint8_t attPpfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_PERIPH_PRIVACY_FLAG)};
const uint8_t attRaChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_RECONN_ADDR)};
const uint8_t attPpcpChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_PREF_CONN_PARAM)};
const uint8_t attScChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_SERVICE_CHANGED)};
const uint8_t attAlChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_ALERT_LEVEL)};
const uint8_t attTxpChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_TX_POWER_LEVEL)};
const uint8_t attDtChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_DATE_TIME)};
const uint8_t attDwChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_DAY_OF_WEEK)};
const uint8_t attDdtChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_DAY_DATE_TIME)};
const uint8_t attEt100ChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_EXACT_TIME_100)};
const uint8_t attEt256ChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_EXACT_TIME_256)};
const uint8_t attDstoChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_DST_OFFSET)};
const uint8_t attTzChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_TIME_ZONE)};
const uint8_t attLtiChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_LOCAL_TIME_INFO)};
const uint8_t attStzChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SECONDARY_TIME_ZONE)};
const uint8_t attTdstChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_TIME_WITH_DST)};
const uint8_t attTaChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_TIME_ACCURACY)};
const uint8_t attTsChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_TIME_SOURCE)};
const uint8_t attRtiChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_REFERENCE_TIME_INFO)};
const uint8_t attTbChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_TIME_BROADCAST)};
const uint8_t attTucpChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_TIME_UPDATE_CP)};
const uint8_t attTusChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_TIME_UPDATE_STATE)};
const uint8_t attGlmChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_GLUCOSE_MEAS)};
const uint8_t attBlChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_BATTERY_LEVEL)};
const uint8_t attBpsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_BATTERY_POWER_STATE)};
const uint8_t attBlsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_BATTERY_LEVEL_STATE)};
const uint8_t attTmChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_TEMP_MEAS)};
const uint8_t attTtChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_TEMP_TYPE)};
const uint8_t attItChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_INTERMEDIATE_TEMP)};
const uint8_t attTcelChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_TEMP_C)};
const uint8_t attTfahChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_TEMP_F)};
const uint8_t attSidChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SYSTEM_ID)};
const uint8_t attMnsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_MODEL_NUMBER)};
const uint8_t attSnsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SERIAL_NUMBER)};
const uint8_t attFrsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_FIRMWARE_REV)};
const uint8_t attHrsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_HARDWARE_REV)};
const uint8_t attSrsChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SOFTWARE_REV)};
const uint8_t attMfnsChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_MANUFACTURER_NAME)};
const uint8_t attIeeeChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_11073_CERT_DATA)};
const uint8_t attCtChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_CURRENT_TIME)};
const uint8_t attElChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_ELEVATION)};
const uint8_t attLatChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_LATITUDE)};
const uint8_t attLongChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_LONGITUDE)};
const uint8_t attP2dChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_POSITION_2D)};
const uint8_t attP3dChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_POSITION_3D)};
const uint8_t attVidChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_VENDOR_ID)};
const uint8_t attHbmiChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HID_BOOT_MOUSE_IN)};
const uint8_t attGlmcChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_GLUCOSE_MEAS_CONTEXT)};
const uint8_t attBpmChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_BP_MEAS)};
const uint8_t attIcpChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_INTERMEDIATE_BP)};
const uint8_t attHrmChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_HR_MEAS)};
const uint8_t attBslChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_HR_SENSOR_LOC)};
const uint8_t attHrcpChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HR_CP)};
const uint8_t attRemChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_REMOVABLE)};
const uint8_t attSrChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_SERVICE_REQ)};
const uint8_t attStcChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SCI_TEMP_C)};
const uint8_t attStrChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_STRING)};
const uint8_t attNwaChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_NETWORK_AVAIL)};
const uint8_t attAsChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_ALERT_STATUS)};
const uint8_t attRcpChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_RINGER_CP)};
const uint8_t attRsChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_RINGER_SETTING)};
const uint8_t attAcbmChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_ALERT_CAT_ID_MASK)};
const uint8_t attAcChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_ALERT_CAT_ID)};
const uint8_t attAncpChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_ALERT_NOTIF_CP)};
const uint8_t attUasChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_UNREAD_ALERT_STATUS)};
const uint8_t attNaChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_NEW_ALERT)};
const uint8_t attSnacChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_SUP_NEW_ALERT_CAT)};
const uint8_t attSuacChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_SUP_UNREAD_ALERT_CAT)};
const uint8_t attBpfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_BP_FEATURE)};
const uint8_t attHidBmiChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_MEAS_INTERVAL)};
const uint8_t attHidBkiChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_HID_BOOT_KEYBOARD_IN)};
const uint8_t attHidBkoChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_HID_BOOT_KEYBOARD_OUT)};
const uint8_t attHidiChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_HID_INFORMATION)};
const uint8_t attHidRmChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_HID_REPORT_MAP)};
const uint8_t attHidcpChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_HID_CONTROL_POINT)};
const uint8_t attHidRepChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_HID_REPORT)};
const uint8_t attHidPmChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_HID_PROTOCOL_MODE)};
const uint8_t attSiwChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SCAN_INT_WIND)};
const uint8_t attPnpChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_PNP_ID)};
const uint8_t attGlfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_GLUCOSE_FEATURE)};
const uint8_t attRacpChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_RACP)};
const uint8_t attCarChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_CAR)};
const uint8_t attRsfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_RUNNING_SPEED_FEATURE)};
const uint8_t attRsmChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_RUNNING_SPEED_MEASUREMENT)};
const uint8_t attCpfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_CYCLING_POWER_FEATURE)};
const uint8_t attCpmChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_CYCLING_POWER_MEASUREMENT)};
const uint8_t attCsfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_CYCLING_SPEED_FEATURE)};
const uint8_t attCsmChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_CYCLING_SPEED_MEASUREMENT)};
const uint8_t attSlChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_SENSOR_LOCATION)};
const uint8_t attPlxfChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_PULSE_OX_FEATURES)};
const uint8_t attPlxscmChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_PULSE_OX_SPOT_CHECK)};
const uint8_t attPlxcmChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_PULSE_OX_CONTINUOUS)};
const uint8_t attRpaoChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_RPAO)};
const uint8_t attDbciChUuid[ATT_16_UUID_LEN] =     {UINT16_TO_BYTES(ATT_UUID_DB_CHANGE_INCREMENT)};
const uint8_t attUiChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_USER_INDEX)};
const uint8_t attUcpChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_USER_CONTROL_POINT)};
const uint8_t attMprvDinChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_MESH_PRV_DATA_IN)};
const uint8_t attMprvDoutChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_MESH_PRV_DATA_OUT)};
const uint8_t attMprxDinChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_MESH_PROXY_DATA_IN)};
const uint8_t attMprxDoutChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_MESH_PROXY_DATA_OUT)};
const uint8_t attWmChUuid[ATT_16_UUID_LEN] =       {UINT16_TO_BYTES(ATT_UUID_WEIGHT_MEAS)};
const uint8_t attWsfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_WEIGHT_SCALE_FEATURE)};
const uint8_t attGattCsfChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_CLIENT_SUPPORTED_FEATURES)};
const uint8_t attGattDbhChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_DATABASE_HASH)};
const uint8_t attCteEnChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_CTE_ENABLE)};
const uint8_t attCteMinLenChUuid[ATT_16_UUID_LEN] ={UINT16_TO_BYTES(ATT_UUID_CTE_MIN_LEN)};
const uint8_t attCteTxCntChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_CTE_TX_CNT)};
const uint8_t attCteTxDurChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_CTE_TX_DURATION)};
const uint8_t attCteIntChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_CTE_INTERVAL)};
const uint8_t attCtePhyChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_CTE_PHY)};
const uint8_t attSsfChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_SERVER_SUPPORTED_FEATURES)};
const uint8_t attAicsStChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_AIC_INPUT_STATE)};
const uint8_t attAicsGsaChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_AIC_GAIN_SETTING_ATTR)};
const uint8_t attAicsItChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_AIC_INPUT_TYPE)};
const uint8_t attAicsStatChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_AIC_INPUT_STATUS)};
const uint8_t attAicsAicChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_AIC_AUDIO_INPUT_CTRL)};
const uint8_t attAicsAidhUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_AIC_AUDIO_INPUT_DESC)};
const uint8_t attMicsMuteChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_MC_MUTE)};
const uint8_t attVcsStateChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_VOLUME_STATE)};
const uint8_t attVcsCpChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_VOLUME_CONTROL_POINT)};
const uint8_t attVcsFlagsChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_VOLUME_FLAGS)};
const uint8_t attVocsStateChUuid[ATT_16_UUID_LEN] ={UINT16_TO_BYTES(ATT_UUID_VOLUME_OFFSET_STATE)};
const uint8_t attVocsLocChUuid[ATT_16_UUID_LEN] =  {UINT16_TO_BYTES(ATT_UUID_AUDIO_LOCATION)};
const uint8_t attVocsCpChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_VOLUME_OFFSET_CONTROL_PT)};
const uint8_t attVocsDescChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_AUDIO_OUT_DESC)};
const uint8_t attSnkPacChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_SNK_PAC)};
const uint8_t attSnkAudLocChUuid[ATT_16_UUID_LEN] ={UINT16_TO_BYTES(ATT_UUID_SNK_AUDIO_LOC)};
const uint8_t attSrcPacChUuid[ATT_16_UUID_LEN] =   {UINT16_TO_BYTES(ATT_UUID_SRC_PAC)};
const uint8_t attSrcAudLocChUuid[ATT_16_UUID_LEN] ={UINT16_TO_BYTES(ATT_UUID_SRC_AUDIO_LOC)};
const uint8_t attAudCntAvChUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_AUDIO_CONT_AVAIL)};
const uint8_t attSupAudCntChUuid[ATT_16_UUID_LEN] ={UINT16_TO_BYTES(ATT_UUID_SUP_AUDIO_CONT)};
const uint8_t attAseChUuid[ATT_16_UUID_LEN] =      {UINT16_TO_BYTES(ATT_UUID_ASE)};
const uint8_t attAseCpChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_ASE_CP)};
const uint8_t attBasCpChUuid[ATT_16_UUID_LEN] =    {UINT16_TO_BYTES(ATT_UUID_BAS_CP)};
const uint8_t attBcRxStateChUuid[ATT_16_UUID_LEN] ={UINT16_TO_BYTES(ATT_UUID_BCAST_RX_STATE)};