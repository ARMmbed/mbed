/**
  ******************************************************************************
  * @file    DTM_cmd_db.h
  * @author  AMG - RF Application team
  * @version V1.0.0
  * @date    31 May 2018
  * @brief   Autogenerated files, do not edit!!
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT STMicroelectronics</center></h2>
  ******************************************************************************
  */
#ifndef _DTM_CMD_DB_H_
#define _DTM_CMD_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "bluenrg1_api.h"

#define HCI_MAX_PAYLOAD_SIZE 256
//#define MIN(a, b) ((a <= b) ? (a) : (b))

typedef uint16_t (*hci_command_process_and_response_type)(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length) ;
typedef struct hci_command_table_type_s {
  uint16_t opcode;
  hci_command_process_and_response_type execute;
} hci_command_table_type;

uint16_t hci_disconnect_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_remote_version_information_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_set_event_mask_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_reset_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_transmit_power_level_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_local_version_information_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_local_supported_commands_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_local_supported_features_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_bd_addr_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_read_rssi_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_event_mask_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_buffer_size_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_local_supported_features_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_random_address_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_advertising_parameters_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_advertising_channel_tx_power_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_advertising_data_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_scan_response_data_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_advertise_enable_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_scan_parameters_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_scan_enable_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_create_connection_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_create_connection_cancel_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_white_list_size_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_clear_white_list_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_add_device_to_white_list_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_remove_device_from_white_list_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_connection_update_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_host_channel_classification_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_channel_map_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_remote_used_features_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_encrypt_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_rand_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_start_encryption_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_long_term_key_request_reply_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_long_term_key_requested_negative_reply_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_supported_states_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_receiver_test_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_transmitter_test_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_test_end_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_data_length_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_suggested_default_data_length_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_write_suggested_default_data_length_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_local_p256_public_key_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_generate_dhkey_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_add_device_to_resolving_list_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_remove_device_from_resolving_list_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_clear_resolving_list_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_resolving_list_size_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_peer_resolvable_address_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_local_resolvable_address_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_address_resolution_enable_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_set_resolvable_private_address_timeout_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t aci_hal_write_config_data_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t aci_hal_read_config_data_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);
uint16_t hci_le_read_maximum_data_length_process(uint8_t *buffer_in, uint16_t buffer_in_length, uint8_t *buffer_out, uint16_t buffer_out_max_length);

void rcv_callback(uint8_t *data, uint16_t len);

extern const hci_command_table_type hci_command_table[56];

//extern const hci_command_table_type hci_command_table[156];
//extern void rcv_callback(uint8_t *buffer_out, uint16_t buffer_out_length, int8_t overflow_index);
//extern void send_event_isr(uint8_t *buffer_out, uint16_t buffer_out_length, int8_t overflow_index);
/* NOTE: aci_hal_updater_start() function prototype: to be implemented on DTM context */
//tBleStatus aci_hal_updater_start(void);
/* NOTE: aci_hal_get_firmware_details() function prototype: to be implemented on DTM context */
//tBleStatus aci_hal_get_firmware_details(uint8_t *DTM_version_major,uint8_t *DTM_version_minor,uint8_t *DTM_version_patch,uint8_t *DTM_variant,uint16_t *DTM_Build_Number,uint8_t *BTLE_Stack_version_major,uint8_t *BTLE_Stack_version_minor,uint8_t *BTLE_Stack_version_patch,uint8_t *BTLE_Stack_development,uint16_t *BTLE_Stack_variant,uint16_t *BTLE_Stack_Build_Number);

#ifdef __cplusplus
}
#endif

#endif //_DTM_CMD_DB_H_
