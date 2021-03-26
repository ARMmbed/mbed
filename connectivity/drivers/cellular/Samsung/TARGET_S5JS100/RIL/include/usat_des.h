/****************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef __USAT_DES_H__
#define __USAT_DES_H__

typedef enum {
    BER_COMP_command_detail_tag = 0x01,
    BER_COMP_device_identity_tag = 0x02,
    BER_COMP_result_tag = 0x03,
    BER_COMP_alpha_identifier_tag = 0x05,
    BER_COMP_address_tag = 0x06,
    BER_COMP_text_string_tag = 0x0D,
    BER_COMP_file_list = 0x12,
    BER_COMP_event_list_tag = 0x19,
    BER_COMP_icon_identifier_tag = 0x1E,
    BER_COMP_aid_tag = 0x2F,
    BER_COMP_bearer_description_tag = 0x35,
    BER_COMP_channel_data_tag = 0x36,
    BER_COMP_channel_data_length_tag = 0x37,
    BER_COMP_channel_status_tag = 0x38,
    BER_COMP_buffer_size_tag = 0x39,
    BER_COMP_uicc_uicc_terminal_interface_transport_level = 0x3C,
    BER_COMP_other_address_tag = 0x3E,
    BER_COMP_frame_identifier_tag = 0x38,
    BER_COMP_nerwork_access_name_tag = 0x47,
} BER_COMP_TAG;

/* TS 102 223 V12 8.6 */
typedef enum {
    UICC_COMMAND_REFRESH = 0x01,
    UICC_COMMAND_SETUP_EVENT_LIST = 0x05,
    UICC_COMMAND_OPEN_CHANNEL = 0x40,
    UICC_COMMAND_CLOSE_CHANNEL = 0x41,
    UICC_COMMAND_RECEIVE_DATA = 0x42,
    UICC_COMMAND_SEND_DATA = 0x43,
    UICC_COMMAND_GET_CHANNEL_STATUS = 0x44,
} UICC_COMMAND_TYPE;
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char number;
    unsigned char type;
    unsigned char qualifier;
} uicc_command_details;

/* TS 102 223 V12 9.2 + 8.6 */
typedef enum {
    BER_TLV_PROACTIVE_COMMAND = 0xD0,
    BER_TLV_EVENT_DOWNLOAD = 0xD6,
} BER_TLV_TAG;
typedef struct {
    unsigned char command_tag;
    unsigned char short_length;
    uicc_command_details command_details;
} uicc_proactive_cmd_hrd;

/* TS 102 223 V12 9.1 */
typedef struct {
    unsigned char command_tag;
    unsigned char short_length;
} uicc_cmd_hrd;

/* TS 102 223 V12 8.7 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char source_device;
    unsigned char destination_device;
} uicc_device_identities;

/* TS 102 223 V12 8.2 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char alpha_identifier[256];
} uicc_alpha_identifier;

/* TS 102 223 V12 8.15 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char data_coding_scheme;
    unsigned char text_string[256];
} uicc_text_string;

/* TS 102 223 V12 8.18 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char num_of_files;
    unsigned char files[256];
} uicc_file_list;

/* TS 102 223 V12 8.31 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char icon_qualifier;
    unsigned char icon_identifier;
} uicc_icon_identifier;

/* TS 102 223 V12 8.52 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char bearer_type;
    unsigned char bearer_parameters[256];
} uicc_bearer_description;

/* TS 102 223 V12 8.53 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char channel_data_string[256];
} uicc_channel_data;

/* TS 102 223 V12 8.54 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char channel_data_length;
} uicc_channel_data_length;

/* TS 102 223 V12 8.55 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned short buffer_size;
} uicc_buffer_size;

/* TS 102 223 V12 8.56 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned short channel_status;
} uicc_channel_status;

/* TS 102 223 V12 8.58 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char type_of_address;
    unsigned char address[256];
} uicc_other_address;

/* TS 102 223 V12 8.59 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char transport_protocol_type;
    unsigned char port_number[2];
} uicc_uicc_terminal_interface_transport_level;

/* TS 102 223 V12 8.60 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char aid[256];
} uicc_aid;

/* TS 102 223 V12 8.70 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char network_access_name[256];
} uicc_network_access_name;

/* TS 102 223 V12 8.72 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char text_formatting;
} uicc_text_attribute;

/* TS 102 223 V12 8.80 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char identifier;
} uicc_frame_identifier;

/* TS 102 223 V12 8.103 */
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char policy[256];
} uicc_refresh_enforcement_policy;

/* TS 102 223 V12 6.6.13 */
typedef struct {
    uicc_proactive_cmd_hrd hrd;
    uicc_device_identities device_identy;
    uicc_file_list file_list;
    uicc_aid aid;
    uicc_alpha_identifier alpha_identifier;
    uicc_icon_identifier icon_identifier;
    uicc_text_attribute text_attribute;
    uicc_frame_identifier frame_identifier;
    uicc_refresh_enforcement_policy refresh_enforcement_policy;
} uicc_refresh;

/* TS 102 223 V12 8.25 */
typedef enum {
    UICC_EVENT_MT_call = 0x00,
    UICC_EVENT_Call_connected = 0x01,
    UICC_EVENT_Call_disconnected = 0x02,
    UICC_EVENT_Location_status = 0x03,
    UICC_EVENT_User_activity = 0x04,
    UICC_EVENT_Idle_screen_available = 0x05,
    UICC_EVENT_Card_reader_status = 0x06,
    UICC_EVENT_Language_selection = 0x07,
    UICC_EVENT_Browser_termination = 0x08,
    UICC_EVENT_Data_available = 0x09,
    UICC_EVENT_Channel_status = 0x0A,
    UICC_EVENT_Access_Technology_Change_single_access_technology = 0x0B,
    UICC_EVENT_Display_parameters_changed = 0x0C,
    UICC_EVENT_Local_connection = 0x0D,
    UICC_EVENT_Network_Search_Mode_Change = 0x0E,
    UICC_EVENT_Browsing_status = 0x0F,
    UICC_EVENT_Frames_Information_Change = 0x10,
    UICC_EVENT_HCI_connectivity_event = 0x13,
    UICC_EVENT_Access_Technology_Change_multiple_access_technologies = 0x14,
    UICC_EVENT_Contactless_state_request = 0x16,
    UICC_EVENT_Profile_Container = 0x19,
    UICC_EVENT_Void = 0x1A,
    UICC_EVENT_Secured_Profile_Container = 0x1B,
    UICC_EVENT_Poll_Int = 0x1C,
} uicc_event_list_event;
typedef struct {
    unsigned char tag;
    unsigned char length;
    unsigned char events[256];
} uicc_event_list;

/* TS 102 223 V12 6.6.16 */
typedef struct {
    uicc_proactive_cmd_hrd *hrd;
    uicc_device_identities *device_identy;
    uicc_event_list *event_list;
} uicc_setup_event_list;

/* TS 102 223 V12 6.6.27.4 */
typedef struct {
    uicc_proactive_cmd_hrd *hrd;
    uicc_device_identities *device_identy;
    uicc_alpha_identifier *alpha_identifier;
    uicc_icon_identifier *icon_identifier;
    uicc_bearer_description *bearer_description;
    uicc_buffer_size *buffer_size;
    uicc_other_address *local_address;
    uicc_network_access_name *network_access_name;
    uicc_text_string *user_login;
    uicc_text_string *user_password;
    uicc_uicc_terminal_interface_transport_level *uicc_terminal_interface_transport_level;
    uicc_other_address *data_destination_address;
    uicc_text_attribute *text_attribute;
    uicc_frame_identifier *frame_identifier;
} uicc_open_channel_default;

/* TS 102 223 V12 6.6.28 */
typedef struct {
    uicc_proactive_cmd_hrd *hrd;
    uicc_device_identities *device_identy;
    uicc_alpha_identifier *alpha_identifier;
    uicc_icon_identifier *icon_identifier;
    uicc_text_attribute *text_attribute;
    uicc_frame_identifier *frame_identifier;
} uicc_close_channel;

/* TS 102 223 V12 6.6.29 */
typedef struct {
    uicc_proactive_cmd_hrd *hrd;
    uicc_device_identities *device_identy;
    uicc_alpha_identifier *alpha_identifier;
    uicc_icon_identifier *icon_identifier;
    uicc_channel_data_length *channel_data_length;
    uicc_text_attribute *text_attribute;
    uicc_frame_identifier *frame_identifier;
} uicc_receive_data;

/* TS 102 223 V12 6.6.30 */
typedef struct {
    uicc_proactive_cmd_hrd *hrd;
    uicc_device_identities *device_identy;
    uicc_alpha_identifier *alpha_identifier;
    uicc_icon_identifier *icon_identifier;
    uicc_channel_data *channel_data;
    uicc_text_attribute *text_attribute;
    uicc_frame_identifier *frame_identifier;
} uicc_send_data;

/* TS 102 223 V12 6.6.31 */
typedef struct {
    uicc_proactive_cmd_hrd *hrd;
    uicc_device_identities *device_identy;
} uicc_get_channel_status;

#endif
