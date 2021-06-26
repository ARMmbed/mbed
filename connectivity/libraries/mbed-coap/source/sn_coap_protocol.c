/*
 * Copyright (c) 2011-2015 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file sn_coap_protocol.c
 *
 * \brief CoAP Protocol implementation
 *
 * Functionality: CoAP Protocol
 *
 */


/* * * * * * * * * * * * * * */
/* * * * INCLUDE FILES * * * */
/* * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h> /* For libary malloc() */
#include <string.h> /* For memset() and memcpy() */
#if defined __linux__ || defined TARGET_LIKE_MBED
#include <time.h>
#endif

#include "ns_types.h"
#include "mbed-coap/sn_coap_protocol.h"
#include "sn_coap_header_internal.h"
#include "sn_coap_protocol_internal.h"
#include "randLIB.h"
#include "mbed-trace/mbed_trace.h"

#define TRACE_GROUP "coap"
/* * * * * * * * * * * * * * * * * * * * */
/* * * * LOCAL FUNCTION PROTOTYPES * * * */
/* * * * * * * * * * * * * * * * * * * * */

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT/* If Message duplication detection is not used at all, this part of code will not be compiled */
static void                  sn_coap_protocol_linked_list_duplication_info_store(struct coap_s *handle, sn_nsdl_addr_s *src_addr_ptr, uint16_t msg_id, void *param);
static coap_duplication_info_s *sn_coap_protocol_linked_list_duplication_info_search(const struct coap_s *handle, const sn_nsdl_addr_s *scr_addr_ptr, const uint16_t msg_id);
static void                  sn_coap_protocol_linked_list_duplication_info_remove_old_ones(struct coap_s *handle);
static void                  sn_coap_protocol_duplication_info_free(struct coap_s *handle, coap_duplication_info_s *duplication_info_ptr);
static bool                  sn_coap_protocol_update_duplicate_package_data(const struct coap_s *handle, const sn_nsdl_addr_s *dst_addr_ptr, const sn_coap_hdr_s *coap_msg_ptr, const int_fast16_t data_size, const uint8_t *dst_packet_data_ptr);
static bool                  sn_coap_protocol_update_duplicate_package_data_all(const struct coap_s *handle, const sn_nsdl_addr_s *dst_addr_ptr, const sn_coap_hdr_s *coap_msg_ptr, const int_fast16_t data_size, const uint8_t *dst_packet_data_ptr);

#endif

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is not enabled, this part of code will not be compiled */
static void                     sn_coap_protocol_linked_list_blockwise_msg_remove(struct coap_s *handle, coap_blockwise_msg_s *removed_msg_ptr);
static void                     sn_coap_protocol_linked_list_blockwise_payload_store(struct coap_s *handle, sn_nsdl_addr_s *addr_ptr, uint16_t payload_len, uint8_t *payload_ptr, uint8_t *token_ptr, uint8_t token_len, uint32_t block_number, uint16_t block_size, uint32_t size1);
static uint8_t                  *sn_coap_protocol_linked_list_blockwise_payload_search(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, uint16_t *payload_length, const uint8_t *token_ptr, uint8_t token_len);
static coap_blockwise_payload_s *sn_coap_protocol_linked_list_blockwise_search(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, const uint8_t *token_ptr, uint8_t token_len);
static bool                     sn_coap_protocol_linked_list_blockwise_payload_search_compare_block_number(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, const uint8_t *token_ptr, uint8_t token_len, uint32_t block_number);
static void                     sn_coap_protocol_linked_list_blockwise_payload_remove(struct coap_s *handle, coap_blockwise_payload_s *removed_payload_ptr);
static uint32_t                 sn_coap_protocol_linked_list_blockwise_payloads_get_len(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, const uint8_t *token_ptr, uint8_t token_len);
static void                     sn_coap_protocol_handle_blockwise_timeout(struct coap_s *handle);
static sn_coap_hdr_s            *sn_coap_handle_blockwise_message(struct coap_s *handle, sn_nsdl_addr_s *src_addr_ptr, sn_coap_hdr_s *received_coap_msg_ptr, void *param, bool *keep_in_resend_queue);
static bool                     sn_coap_handle_last_blockwise(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, sn_coap_hdr_s *received_coap_msg_ptr);
static sn_coap_hdr_s            *sn_coap_protocol_copy_header(struct coap_s *handle, const sn_coap_hdr_s *source_header_ptr);
static coap_blockwise_msg_s     *search_sent_blockwise_message(struct coap_s *handle, uint16_t msg_id);
static int16_t                  store_blockwise_copy(struct coap_s *handle, const sn_coap_hdr_s *src_coap_msg_ptr, void *param, uint16_t original_payload_len, bool copy_payload);
#endif

#if ENABLE_RESENDINGS
static uint8_t               sn_coap_protocol_linked_list_send_msg_store(struct coap_s *handle, sn_nsdl_addr_s *dst_addr_ptr, uint_fast16_t send_packet_data_len, uint8_t *send_packet_data_ptr, uint32_t sending_time, void *param);
static void                  sn_coap_protocol_linked_list_send_msg_remove(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, uint16_t msg_id);
static coap_send_msg_s      *sn_coap_protocol_allocate_mem_for_msg(struct coap_s *handle, sn_nsdl_addr_s *dst_addr_ptr, uint_fast16_t packet_data_len);
static void                  sn_coap_protocol_release_allocated_send_msg_mem(struct coap_s *handle, coap_send_msg_s *freed_send_msg_ptr);
static uint_fast16_t         sn_coap_count_linked_list_size(const coap_send_msg_list_t *linked_list_ptr);
static uint32_t              sn_coap_calculate_new_resend_time(const uint32_t current_time, const uint8_t interval, const uint8_t counter);
#endif

static uint16_t              read_packet_msg_id(const coap_send_msg_s *stored_msg);
static uint16_t              get_new_message_id(void);

static bool                  compare_port(const sn_nsdl_addr_s *left, const sn_nsdl_addr_s *right);

/* * * * * * * * * * * * * * * * * */
/* * * * GLOBAL DECLARATIONS * * * */
/* * * * * * * * * * * * * * * * * */
static uint16_t message_id;

int8_t sn_coap_protocol_destroy(struct coap_s *handle)
{
    if (handle == NULL) {
        return -1;
    }
#if ENABLE_RESENDINGS /* If Message resending is not used at all, this part of code will not be compiled */

    sn_coap_protocol_clear_retransmission_buffer(handle);

#endif

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT /* If Message duplication detection is not used at all, this part of code will not be compiled */
    ns_list_foreach_safe(coap_duplication_info_s, tmp, &handle->linked_list_duplication_msgs) {

        ns_list_remove(&handle->linked_list_duplication_msgs, tmp);

        sn_coap_protocol_duplication_info_free(handle, tmp);
    }

#endif


#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwise is not enabled, this part of code will not be compiled */

    sn_coap_protocol_clear_sent_blockwise_messages(handle);

    sn_coap_protocol_clear_received_blockwise_messages(handle);
#endif

    handle->sn_coap_protocol_free(handle);
    return 0;
}

struct coap_s *sn_coap_protocol_init(void *(*used_malloc_func_ptr)(uint16_t), void (*used_free_func_ptr)(void *),
                                     uint8_t (*used_tx_callback_ptr)(uint8_t *, uint16_t, sn_nsdl_addr_s *, void *),
                                     int8_t (*used_rx_callback_ptr)(sn_coap_hdr_s *, sn_nsdl_addr_s *, void *param))
{
    /* Check paramters */
    if ((used_malloc_func_ptr == NULL) || (used_free_func_ptr == NULL) || (used_tx_callback_ptr == NULL)) {
        return NULL;
    }

    struct coap_s *handle;
    handle = used_malloc_func_ptr(sizeof(struct coap_s));
    if (handle == NULL) {
        return NULL;
    }

    memset(handle, 0, sizeof(struct coap_s));

    /* * * Handle tx callback * * */
    handle->sn_coap_tx_callback = used_tx_callback_ptr;

    handle->sn_coap_protocol_free = used_free_func_ptr;
    handle->sn_coap_protocol_malloc = used_malloc_func_ptr;

    /* * * Handle rx callback * * */
    /* If pointer = 0, then re-sending does not return error when failed */
    handle->sn_coap_rx_callback = used_rx_callback_ptr;

    // Handles internally all GET req responses
    handle->sn_coap_internal_block2_resp_handling = true;

#if ENABLE_RESENDINGS  /* If Message resending is not used at all, this part of code will not be compiled */
    /* * * * Create Linked list for storing active resending messages  * * * */
    ns_list_init(&handle->linked_list_resent_msgs);
    handle->sn_coap_resending_queue_msgs = SN_COAP_RESENDING_QUEUE_SIZE_MSGS;
    handle->sn_coap_resending_queue_bytes = SN_COAP_RESENDING_QUEUE_SIZE_BYTES;
    handle->sn_coap_resending_intervall = DEFAULT_RESPONSE_TIMEOUT;
    handle->sn_coap_resending_count = SN_COAP_RESENDING_MAX_COUNT;


#endif /* ENABLE_RESENDINGS */

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT /* If Message duplication detection is not used at all, this part of code will not be compiled */
    /* * * * Create Linked list for storing Duplication info * * * */
    ns_list_init(&handle->linked_list_duplication_msgs);
    handle->sn_coap_duplication_buffer_size = SN_COAP_DUPLICATION_MAX_MSGS_COUNT;
#endif

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is not enabled, this part of code will not be compiled */

    ns_list_init(&handle->linked_list_blockwise_sent_msgs);
    ns_list_init(&handle->linked_list_blockwise_received_payloads);
    handle->sn_coap_block_data_size = SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE;

#endif /* ENABLE_RESENDINGS */

    message_id = 0;
    return handle;
}

int8_t sn_coap_protocol_handle_block2_response_internally(struct coap_s *handle, uint8_t build_response)
{
    if (handle == NULL) {
        return -1;
    }

    handle->sn_coap_internal_block2_resp_handling = build_response;
    return 0;
}

int8_t sn_coap_protocol_set_block_size(struct coap_s *handle, uint16_t block_size)
{
    (void) handle;
    (void) block_size;
#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
    if (handle == NULL) {
        return -1;
    }
    if (sn_coap_convert_block_size(block_size) >= 0) {
        handle->sn_coap_block_data_size = block_size;
        return 0;
    }
#endif
    return -1;

}

void sn_coap_protocol_clear_sent_blockwise_messages(struct coap_s *handle)
{
    (void) handle;
#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
    if (handle == NULL) {
        return;
    }

    /* Loop all stored Blockwise messages in Linked list */
    ns_list_foreach_safe(coap_blockwise_msg_s, removed_blocwise_msg_ptr, &handle->linked_list_blockwise_sent_msgs) {
        sn_coap_protocol_linked_list_blockwise_msg_remove(handle, removed_blocwise_msg_ptr);
    }
#endif
}

void sn_coap_protocol_clear_received_blockwise_messages(struct coap_s *handle)
{
    (void) handle;
#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
    if (handle == NULL) {
        return;
    }

    /* Loop all stored Blockwise messages in Linked list */
    ns_list_foreach_safe(coap_blockwise_payload_s, removed_blockwise_payload_ptr, &handle->linked_list_blockwise_received_payloads) {
        sn_coap_protocol_linked_list_blockwise_payload_remove(handle, removed_blockwise_payload_ptr);
    }
#endif
}

int8_t sn_coap_protocol_set_duplicate_buffer_size(struct coap_s *handle, uint8_t message_count)
{
    (void) handle;
    (void) message_count;
#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
    if (handle == NULL) {
        return -1;
    }
    if (message_count <= SN_COAP_MAX_ALLOWED_DUPLICATION_MESSAGE_COUNT) {
        handle->sn_coap_duplication_buffer_size = message_count;
        return 0;
    }
#endif
    return -1;
}

int8_t sn_coap_protocol_set_retransmission_parameters(struct coap_s *handle,
                                                      uint8_t resending_count, uint8_t resending_intervall)
{
#if ENABLE_RESENDINGS
    if (handle == NULL) {
        return -1;
    }
    if (resending_count <= SN_COAP_MAX_ALLOWED_RESENDING_COUNT &&
            resending_intervall <= SN_COAP_MAX_ALLOWED_RESPONSE_TIMEOUT) {
        handle->sn_coap_resending_count = resending_count;

        if (resending_intervall == 0) {
            handle->sn_coap_resending_intervall = 1;
        } else {
            handle->sn_coap_resending_intervall = resending_intervall;
        }
        return 0;
    }
#endif
    return -1;
}

int8_t sn_coap_protocol_set_retransmission_buffer(struct coap_s *handle,
                                                  uint8_t buffer_size_messages, uint16_t buffer_size_bytes)
{
#if ENABLE_RESENDINGS
    if (handle == NULL) {
        return -1;
    }
    if (buffer_size_bytes <= SN_COAP_MAX_ALLOWED_RESENDING_BUFF_SIZE_BYTES &&
            buffer_size_messages <= SN_COAP_MAX_ALLOWED_RESENDING_BUFF_SIZE_MSGS) {
        handle->sn_coap_resending_queue_bytes = buffer_size_bytes;
        handle->sn_coap_resending_queue_msgs = buffer_size_messages;
        return 0;
    }

#endif
    return -1;

}

void sn_coap_protocol_clear_retransmission_buffer(struct coap_s *handle)
{
#if ENABLE_RESENDINGS /* If Message resending is not used at all, this part of code will not be compiled */
    if (handle == NULL) {
        return;
    }
    ns_list_foreach_safe(coap_send_msg_s, tmp, &handle->linked_list_resent_msgs) {
        ns_list_remove(&handle->linked_list_resent_msgs, tmp);
        sn_coap_protocol_release_allocated_send_msg_mem(handle, tmp);
    }
    handle->count_resent_msgs = 0;
#endif
}

int8_t sn_coap_protocol_delete_retransmission(struct coap_s *handle, uint16_t msg_id)
{
#if ENABLE_RESENDINGS /* If Message resending is not used at all, this part of code will not be compiled */
    if (handle == NULL) {
        return -1;
    }
    ns_list_foreach_safe(coap_send_msg_s, tmp, &handle->linked_list_resent_msgs) {
        if (tmp->send_msg_ptr.packet_ptr) {
            uint16_t temp_msg_id = read_packet_msg_id(tmp);
            if (temp_msg_id == msg_id) {
                ns_list_remove(&handle->linked_list_resent_msgs, tmp);
                --handle->count_resent_msgs;
                sn_coap_protocol_release_allocated_send_msg_mem(handle, tmp);
                return 0;
            }
        }
    }
#endif
    return -2;
}

int8_t sn_coap_protocol_delete_retransmission_by_token(struct coap_s *handle, const uint8_t *token, uint8_t token_len)
{
#if ENABLE_RESENDINGS /* If Message resending is not used at all, this part of code will not be compiled */
    if (handle == NULL || token == NULL || token_len == 0) {
        tr_error("sn_coap_protocol_delete_retransmission_by_token NULL");
        return -1;
    }

    ns_list_foreach(coap_send_msg_s, stored_msg, &handle->linked_list_resent_msgs) {
        uint8_t stored_token_len = (stored_msg->send_msg_ptr.packet_ptr[0] & 0x0F);
        if (stored_token_len == token_len) {
            if (memcmp(&stored_msg->send_msg_ptr.packet_ptr[4], token, stored_token_len) == 0) {

                tr_debug("sn_coap_protocol_delete_retransmission_by_token - removed msg_id: %" PRIu16, read_packet_msg_id(stored_msg));
                ns_list_remove(&handle->linked_list_resent_msgs, stored_msg);
                --handle->count_resent_msgs;

                /* Free memory of stored message */
                sn_coap_protocol_release_allocated_send_msg_mem(handle, stored_msg);
                return 0;
            }
        }
    }
#endif
    return -2;
}

static uint16_t read_packet_msg_id(const coap_send_msg_s *stored_msg)
{
    uint16_t msg_id = (stored_msg->send_msg_ptr.packet_ptr[2] << 8);
    msg_id += (uint16_t)stored_msg->send_msg_ptr.packet_ptr[3];
    return msg_id;
}

int8_t prepare_blockwise_message(struct coap_s *handle, sn_coap_hdr_s *src_coap_msg_ptr)
{
    (void) handle;
    (void) src_coap_msg_ptr;

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is not enabled, this part of code will not be compiled */
    if ((src_coap_msg_ptr->payload_len > SN_COAP_MAX_NONBLOCKWISE_PAYLOAD_SIZE) &&
            (src_coap_msg_ptr->payload_len > handle->sn_coap_block_data_size) &&
            (handle->sn_coap_block_data_size > 0)) {
        /* * * * Add Blockwise option to send CoAP message * * */

        /* Allocate memory for less used options */
        if (sn_coap_parser_alloc_options(handle, src_coap_msg_ptr) == NULL) {
            tr_error("prepare_blockwise_message - failed to allocate options!");
            return -2;
        }

        /* Check if Request message */
        if (src_coap_msg_ptr->msg_code < COAP_MSG_CODE_RESPONSE_CREATED) {
            /* Add Blockwise option, use Block1 because Request payload */
            src_coap_msg_ptr->options_list_ptr->block1 = 0x08;      /* First block  (BLOCK NUMBER, 4 MSB bits) + More to come (MORE, 1 bit) */
            src_coap_msg_ptr->options_list_ptr->block1 |= sn_coap_convert_block_size(handle->sn_coap_block_data_size);

            /* Add size1 parameter */

            src_coap_msg_ptr->options_list_ptr->use_size1 = true;
            src_coap_msg_ptr->options_list_ptr->use_size2 = false;
            src_coap_msg_ptr->options_list_ptr->size1 = src_coap_msg_ptr->payload_len;
        } else { /* Response message */
            /* Add Blockwise option, use Block2 because Response payload */
            src_coap_msg_ptr->options_list_ptr->block2 = 0x08;      /* First block  (BLOCK NUMBER, 4 MSB bits) + More to come (MORE, 1 bit) */
            src_coap_msg_ptr->options_list_ptr->block2 |= sn_coap_convert_block_size(handle->sn_coap_block_data_size);

            src_coap_msg_ptr->options_list_ptr->use_size1 = false;
            src_coap_msg_ptr->options_list_ptr->use_size2 = true;
            src_coap_msg_ptr->options_list_ptr->size2 = src_coap_msg_ptr->payload_len;
        }
    }
#endif
    return 0;
}

int16_t sn_coap_protocol_build(struct coap_s *handle, sn_nsdl_addr_s *dst_addr_ptr,
                               uint8_t *dst_packet_data_ptr, sn_coap_hdr_s *src_coap_msg_ptr, void *param)
{
    int16_t  byte_count_built     = 0;
#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is not enabled, this part of code will not be compiled */
    uint16_t original_payload_len = 0;
#endif
    /* * * * Check given pointers  * * * */
    if ((dst_addr_ptr == NULL) || (dst_packet_data_ptr == NULL) || (src_coap_msg_ptr == NULL) || handle == NULL) {
        return -2;
    }

    if (dst_addr_ptr->addr_ptr == NULL) {
        return -2;
    }

    /* Check if built Message type is else than Acknowledgement or Reset i.e. message type is Confirmable or Non-confirmable */
    /* (for Acknowledgement and  Reset messages is written same Message ID than was in the Request message) */
    if (src_coap_msg_ptr->msg_type != COAP_MSG_TYPE_ACKNOWLEDGEMENT &&
            src_coap_msg_ptr->msg_type != COAP_MSG_TYPE_RESET &&
            src_coap_msg_ptr->msg_id == 0) {

        /* * * * Generate new Message ID * * * */
        src_coap_msg_ptr->msg_id = get_new_message_id();
    }

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is not enabled, this part of code will not be compiled */
    /* If blockwising needed */
    if ((src_coap_msg_ptr->payload_len > SN_COAP_MAX_NONBLOCKWISE_PAYLOAD_SIZE) &&
            (src_coap_msg_ptr->payload_len > handle->sn_coap_block_data_size) &&
            (handle->sn_coap_block_data_size > 0)) {
        /* Store original Payload length */
        original_payload_len = src_coap_msg_ptr->payload_len;
        /* Change Payload length of send message because Payload is blockwised */
        src_coap_msg_ptr->payload_len = handle->sn_coap_block_data_size;
    }
#endif
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    /* * * * Build Packet data from CoAP message by using CoAP Header builder  * * * */
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    byte_count_built = sn_coap_builder_2(dst_packet_data_ptr, src_coap_msg_ptr, handle->sn_coap_block_data_size);

    if (byte_count_built < 0) {
        tr_error("sn_coap_protocol_build - failed to build message!");
        return byte_count_built;
    }

#if ENABLE_RESENDINGS /* If Message resending is not used at all, this part of code will not be compiled */

    /* Check if built Message type was confirmable, only these messages are resent */
    if (src_coap_msg_ptr->msg_type == COAP_MSG_TYPE_CONFIRMABLE) {
        /* Store message to Linked list for resending purposes */
        uint32_t resend_time = sn_coap_calculate_new_resend_time(handle->system_time, handle->sn_coap_resending_intervall, 0);
        if (sn_coap_protocol_linked_list_send_msg_store(handle, dst_addr_ptr, byte_count_built, dst_packet_data_ptr,
                                                        resend_time,
                                                        param) == 0) {
            return -4;
        }
    }

#endif /* ENABLE_RESENDINGS */

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT

    // copy coap data buffer to duplicate list for resending purposes
    if (!sn_coap_protocol_update_duplicate_package_data(handle, dst_addr_ptr, src_coap_msg_ptr, byte_count_built, dst_packet_data_ptr)) {
        return -4;
    }

#endif

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is not enabled, this part of code will not be compiled */

    /* If blockwising needed */
    if ((original_payload_len > handle->sn_coap_block_data_size) && (handle->sn_coap_block_data_size > 0)) {

        /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
        /* * * * Manage rest blockwise messages sending by storing them to Linked list * * * */
        /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

        int status = store_blockwise_copy(handle, src_coap_msg_ptr, param, original_payload_len, true);
        if (status < 0) {
            return status;
        }

    } else if (src_coap_msg_ptr->msg_code <= COAP_MSG_CODE_REQUEST_DELETE &&
               src_coap_msg_ptr->msg_code != COAP_MSG_CODE_EMPTY) {

        int status = store_blockwise_copy(handle, src_coap_msg_ptr, param, original_payload_len, false);
        if (status < 0) {
            return status;
        }
    }

#endif /* SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE */

    /* * * * Return built CoAP message Packet data length  * * * */
    return byte_count_built;
}

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
static int16_t store_blockwise_copy(struct coap_s *handle, const sn_coap_hdr_s *src_coap_msg_ptr, void *param, uint16_t original_payload_len, bool copy_payload)
{
    coap_blockwise_msg_s *restrict stored_blockwise_msg_ptr;

    stored_blockwise_msg_ptr = sn_coap_protocol_calloc(handle, sizeof(coap_blockwise_msg_s));
    if (!stored_blockwise_msg_ptr) {
        //block payload save failed, only first block can be build. Perhaps we should return error.
        tr_error("sn_coap_protocol_build - blockwise message allocation failed!");
        return -2;
    }

    /* Fill struct */
    stored_blockwise_msg_ptr->timestamp = handle->system_time;

    sn_coap_hdr_s *restrict copied_msg_ptr = sn_coap_protocol_copy_header(handle, src_coap_msg_ptr);
    if (copied_msg_ptr == NULL) {
        handle->sn_coap_protocol_free(stored_blockwise_msg_ptr);
        tr_error("sn_coap_protocol_build - block header copy failed!");
        return -2;
    }
    stored_blockwise_msg_ptr->coap_msg_ptr = copied_msg_ptr;

    if (copy_payload) {
        copied_msg_ptr->payload_len = original_payload_len;
        copied_msg_ptr->payload_ptr = sn_coap_protocol_malloc_copy(handle, src_coap_msg_ptr->payload_ptr, stored_blockwise_msg_ptr->coap_msg_ptr->payload_len);

        if (!copied_msg_ptr->payload_ptr) {
            //block payload save failed, only first block can be build. Perhaps we should return error.
            sn_coap_parser_release_allocated_coap_msg_mem(handle, copied_msg_ptr);
            handle->sn_coap_protocol_free(stored_blockwise_msg_ptr);
            tr_error("sn_coap_protocol_build - block payload allocation failed!");
            return -2;
        }
    }

    stored_blockwise_msg_ptr->param = param;
    stored_blockwise_msg_ptr->msg_id = copied_msg_ptr->msg_id;

    ns_list_add_to_end(&handle->linked_list_blockwise_sent_msgs, stored_blockwise_msg_ptr);

    return 0;
}
#endif

sn_coap_hdr_s *sn_coap_protocol_parse(struct coap_s *restrict handle, sn_nsdl_addr_s *restrict src_addr_ptr, uint16_t packet_data_len, uint8_t *restrict packet_data_ptr, void *param)
{
    coap_version_e   coap_version              = COAP_VERSION_UNKNOWN;

    /* * * * Check given pointer * * * */
    if (src_addr_ptr == NULL || src_addr_ptr->addr_ptr == NULL ||
            packet_data_ptr == NULL || handle == NULL) {
        return NULL;
    }

    /* * * * Parse Packet data to CoAP message by using CoAP Header parser * * * */
    sn_coap_hdr_s *restrict returned_dst_coap_msg_ptr = sn_coap_parser(handle, packet_data_len, packet_data_ptr, &coap_version);

    /* Check status of returned pointer */
    if (returned_dst_coap_msg_ptr == NULL) {
        /* Memory allocation error in parser */
        tr_error("sn_coap_protocol_parse - allocation fail in parser!");
        return NULL;
    }
    /* * * * Send bad request response if parsing fails * * * */
    if (returned_dst_coap_msg_ptr->coap_status == COAP_STATUS_PARSER_ERROR_IN_HEADER) {
        sn_coap_protocol_send_rst(handle, returned_dst_coap_msg_ptr->msg_id, src_addr_ptr, param);
        sn_coap_parser_release_allocated_coap_msg_mem(handle, returned_dst_coap_msg_ptr);
        tr_error("sn_coap_protocol_parse - COAP_STATUS_PARSER_ERROR_IN_HEADER");
        return NULL;
    }

    /* * * * Check validity of parsed Header values  * * * */
    if (sn_coap_header_validity_check(returned_dst_coap_msg_ptr, coap_version) != 0) {
        /* If message code is in a reserved class (1, 6 or 7), send reset. Message code class is 3 MSB of the message code byte     */

        const uint8_t msg_code_class = returned_dst_coap_msg_ptr->msg_code >> 5;
        if ((msg_code_class == 1) ||        // if class == 1
                (msg_code_class == 6) ||    // if class == 6
                (msg_code_class == 7)) {    // if class == 7
            tr_error("sn_coap_protocol_parse - message code not valid!");
            sn_coap_protocol_send_rst(handle, returned_dst_coap_msg_ptr->msg_id, src_addr_ptr, param);
        }

        /* Release memory of CoAP message */
        sn_coap_parser_release_allocated_coap_msg_mem(handle, returned_dst_coap_msg_ptr);

        /* Return NULL because Header validity check failed */
        return NULL;
    }

    /* Check if we need to send reset message */
    /*  A recipient MUST acknowledge a Confirmable message with an Acknowledgement
        message or, if it lacks context to process the message properly
        (including the case where the message is Empty, uses a code with a
        reserved class (1, 6 or 7), or has a message format error), MUST
        reject it; rejecting a Confirmable message is effected by sending a
        matching Reset message and otherwise ignoring it. */
    if (returned_dst_coap_msg_ptr->msg_type == COAP_MSG_TYPE_CONFIRMABLE) {
        /* CoAP ping */
        if (returned_dst_coap_msg_ptr->msg_code == COAP_MSG_CODE_EMPTY) {
            sn_coap_protocol_send_rst(handle, returned_dst_coap_msg_ptr->msg_id, src_addr_ptr, param);

            /* Release memory of CoAP message */
            sn_coap_parser_release_allocated_coap_msg_mem(handle, returned_dst_coap_msg_ptr);

            /* Return NULL because Header validity check failed */
            return NULL;
        }
    }


#if !SN_COAP_BLOCKWISE_ENABLED && !SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE /* If Message blockwising is enabled, this part of code will not be compiled */
    /* If blockwising used in received message */
    if (returned_dst_coap_msg_ptr->options_list_ptr != NULL &&
            (returned_dst_coap_msg_ptr->options_list_ptr->block1 != COAP_OPTION_BLOCK_NONE ||
             returned_dst_coap_msg_ptr->options_list_ptr->block2 != COAP_OPTION_BLOCK_NONE)) {
        /* Set returned status to User */
        returned_dst_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_MSG_REJECTED;
        tr_error("sn_coap_protocol_parse - COAP_STATUS_PARSER_BLOCKWISE_MSG_REJECTED!");
        //todo: send response -> not implemented
        return returned_dst_coap_msg_ptr;
    }
#endif /* !SN_COAP_BLOCKWISE_ENABLED && !SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE */

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT/* If Message duplication is used, this part of code will not be compiled */

    // Manage received CoAP message duplicate detection

    // If no message duplication detected
    if ((returned_dst_coap_msg_ptr->msg_type == COAP_MSG_TYPE_CONFIRMABLE ||
            returned_dst_coap_msg_ptr->msg_type == COAP_MSG_TYPE_NON_CONFIRMABLE) &&
            handle->sn_coap_duplication_buffer_size != 0) {

        coap_duplication_info_s *response = sn_coap_protocol_linked_list_duplication_info_search(handle,
                                                                                                 src_addr_ptr,
                                                                                                 returned_dst_coap_msg_ptr->msg_id);
        if (response == NULL) {
            // No Message duplication: Store received message for detecting later duplication

            // Get count of stored duplication messages
            uint16_t stored_duplication_msgs_count = handle->count_duplication_msgs;

            // Check if there is no room to store message for duplication detection purposes
            if (stored_duplication_msgs_count >= handle->sn_coap_duplication_buffer_size) {
                tr_debug("sn_coap_protocol_parse - duplicate list full, dropping oldest");

                // Get oldest stored duplication message
                coap_duplication_info_s *stored_duplication_info_ptr = ns_list_get_first(&handle->linked_list_duplication_msgs);

                // Remove oldest stored duplication message for getting room for new duplication message
                sn_coap_protocol_linked_list_duplication_info_remove(handle,
                                                                     stored_duplication_info_ptr->address->addr_ptr,
                                                                     stored_duplication_info_ptr->address->port,
                                                                     stored_duplication_info_ptr->msg_id);
            }

            // Store Duplication info to Linked list
            sn_coap_protocol_linked_list_duplication_info_store(handle, src_addr_ptr, returned_dst_coap_msg_ptr->msg_id, param);
        } else { // Message duplication detected
            // Set returned status to User
            returned_dst_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_DUPLICATED_MSG;

            // Send ACK response
            if (response) {
                // Check that response has been created
                if (response->packet_ptr) {
                    tr_debug("sn_coap_protocol_parse - send ack for duplicate message");
                    handle->sn_coap_tx_callback(response->packet_ptr,
                                                response->packet_len, response->address, response->param);
                } else {
                    tr_error("sn_coap_protocol_parse - response not yet build");
                }
            }

            return returned_dst_coap_msg_ptr;
        }
    }
#endif

    /*** And here we check if message was block message ***/
    /*** If so, we call own block handling function and ***/
    /*** return to caller.                              ***/
    bool keep_in_resend_queue = false;
#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE

    if (returned_dst_coap_msg_ptr->options_list_ptr != NULL &&
            (returned_dst_coap_msg_ptr->options_list_ptr->block1 != COAP_OPTION_BLOCK_NONE ||
             returned_dst_coap_msg_ptr->options_list_ptr->block2 != COAP_OPTION_BLOCK_NONE)) {

        // the sn_coap_handle_blockwise_message() will return the given message on success or NULL on error
        if (sn_coap_handle_blockwise_message(handle, src_addr_ptr, returned_dst_coap_msg_ptr, param, &keep_in_resend_queue) == NULL) {

            tr_error("sn_coap_protocol_parse - handle blockwise returns null!");

            // the message is freed just here, not in every other path in sn_coap_handle_blockwise_message().
            sn_coap_parser_release_allocated_coap_msg_mem(handle, returned_dst_coap_msg_ptr);
            returned_dst_coap_msg_ptr = NULL;
        }

    } else if (returned_dst_coap_msg_ptr->payload_len > handle->sn_coap_block_data_size) {

        // If message comes without block1 information and payload length is too large to handle.
        // Send hint response to the server to start a blockwise transfer.

        tr_info("sn_coap_protocol_parse - payload too large, request blockwise transfer");

        uint8_t *packet_data_ptr = NULL;
        uint16_t packet_data_size = 0;
        sn_coap_hdr_s *resp = sn_coap_build_response(handle,
                                                     returned_dst_coap_msg_ptr,
                                                     COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_TOO_LARGE);

        if (resp == NULL) {
            tr_error("sn_coap_protocol_parse - payload too large, failed to build response!");
            goto cleanup;
        }

        if (sn_coap_parser_alloc_options(handle, resp) == NULL) {
            tr_error("sn_coap_protocol_parse - payload too large, failed to allocate options!");
            goto cleanup;
        }

        // set block1 size into response
        resp->options_list_ptr->block1 &= 0x08;
        resp->options_list_ptr->block1 |= sn_coap_convert_block_size(handle->sn_coap_block_data_size);

        packet_data_size = sn_coap_builder_calc_needed_packet_data_size_2(resp, handle->sn_coap_block_data_size);

        packet_data_ptr = handle->sn_coap_protocol_malloc(packet_data_size);

        if (packet_data_ptr == NULL) {
            tr_error("sn_coap_protocol_parse - payload too large, failed to allocate buffer!");
            goto cleanup;
        }

        if (sn_coap_builder_2(packet_data_ptr, resp, handle->sn_coap_block_data_size) < 0) {
            tr_error("sn_coap_protocol_parse - payload too large, builder failed!");
            goto cleanup;
        }

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
        // copy data buffer to duplicate list for resending purposes
        if (!sn_coap_protocol_update_duplicate_package_data(handle, src_addr_ptr, resp, packet_data_size, packet_data_ptr)) {
            tr_error("sn_coap_protocol_parse - failed to update duplicate info!");
            goto cleanup;
        }
#endif

        sn_coap_parser_release_allocated_coap_msg_mem(handle, resp);

        handle->sn_coap_tx_callback(packet_data_ptr, packet_data_size, src_addr_ptr, param);

        handle->sn_coap_protocol_free(packet_data_ptr);

        returned_dst_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_MSG_RECEIVING;

        return returned_dst_coap_msg_ptr;

cleanup:
        sn_coap_parser_release_allocated_coap_msg_mem(handle, resp);
        sn_coap_parser_release_allocated_coap_msg_mem(handle, returned_dst_coap_msg_ptr);
        handle->sn_coap_protocol_free(packet_data_ptr);
        return NULL;

    } else if (returned_dst_coap_msg_ptr->msg_code != COAP_MSG_CODE_EMPTY) {
        // Do not clean stored blockwise message when empty ack is received.
        // Stored message is mandatory when building a next (GET) blockwise message.
        // This will happen when non piggybacked response mode is selected.
        /* Get ... */
        coap_blockwise_msg_s *stored_blockwise_msg_temp_ptr = search_sent_blockwise_message(handle, returned_dst_coap_msg_ptr->msg_id);

        /* Remove from the list if not a notification message.
         * Initial notification message is needed for sending rest of the blocks (GET request).
        */
        if (stored_blockwise_msg_temp_ptr) {
            bool remove_from_the_list;
            if (stored_blockwise_msg_temp_ptr->coap_msg_ptr &&
                    stored_blockwise_msg_temp_ptr->coap_msg_ptr->options_list_ptr &&
                    stored_blockwise_msg_temp_ptr->coap_msg_ptr->options_list_ptr->observe != COAP_OBSERVE_NONE) {
                remove_from_the_list = false;
            } else {
                remove_from_the_list = true;
            }

            if (remove_from_the_list) {
                sn_coap_protocol_linked_list_blockwise_msg_remove(handle, stored_blockwise_msg_temp_ptr);
            }
        }
    }

    if (!returned_dst_coap_msg_ptr) {
        tr_error("sn_coap_protocol_parse - returned_dst_coap_msg_ptr null!");
        return NULL;
    }

#endif


#if ENABLE_RESENDINGS  /* If Message resending is not used at all, this part of code will not be compiled */

    /* Check if received Message type was acknowledgement */
    if ((returned_dst_coap_msg_ptr->msg_type == COAP_MSG_TYPE_ACKNOWLEDGEMENT) || (returned_dst_coap_msg_ptr->msg_type == COAP_MSG_TYPE_RESET)) {
        /* * * * Manage CoAP message resending by removing active resending message from Linked list * * */

        /* Get node count i.e. count of active resending messages */
        uint16_t stored_resending_msgs_count = handle->count_resent_msgs;
        /* Check if there is ongoing active message resendings */
        if (stored_resending_msgs_count > 0 && !keep_in_resend_queue) {
            /* Remove resending message from active message resending Linked list, if any exists */
            sn_coap_protocol_linked_list_send_msg_remove(handle, src_addr_ptr, returned_dst_coap_msg_ptr->msg_id);
        }
    }
#endif /* ENABLE_RESENDINGS */

    /* * * * Return parsed CoAP message  * * * */
    return returned_dst_coap_msg_ptr;
}


int8_t sn_coap_protocol_exec(struct coap_s *handle, uint32_t current_time)
{
    if (!handle) {
        return -1;
    }

    /* * * * Store current System time * * * */
    handle->system_time = current_time;

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
    /* * * * Handle block transfer timed outs * * * */
    sn_coap_protocol_handle_blockwise_timeout(handle);
#endif

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
    /* * * * Remove old duplication messages * * * */
    sn_coap_protocol_linked_list_duplication_info_remove_old_ones(handle);
#endif

#if ENABLE_RESENDINGS
    /* Check if there is ongoing active message sendings */
    /* foreach_safe isn't sufficient because callback routine could cancel messages. */
rescan:
    ns_list_foreach(coap_send_msg_s, stored_msg_ptr, &handle->linked_list_resent_msgs) {
        // First check that msg belongs to handle
        /* Check if it is time to send this message */
        if (current_time >= stored_msg_ptr->resending_time) {
            /* * * Increase Resending counter  * * */
            stored_msg_ptr->resending_counter++;

            /* Check if all re-sendings have been done */
            if (stored_msg_ptr->resending_counter > handle->sn_coap_resending_count) {
                coap_version_e coap_version = COAP_VERSION_UNKNOWN;


                /* Remove message from Linked list */
                ns_list_remove(&handle->linked_list_resent_msgs, stored_msg_ptr);
                --handle->count_resent_msgs;

                /* If RX callback have been defined.. */
                if (handle->sn_coap_rx_callback != 0) {
                    sn_coap_hdr_s *tmp_coap_hdr_ptr;
                    /* Parse CoAP message, set status and call RX callback */
                    tmp_coap_hdr_ptr = sn_coap_parser(handle, stored_msg_ptr->send_msg_ptr.packet_len, stored_msg_ptr->send_msg_ptr.packet_ptr, &coap_version);

                    if (tmp_coap_hdr_ptr != 0) {
#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
                        sn_coap_protocol_remove_sent_blockwise_message(handle, tmp_coap_hdr_ptr->msg_id);
#endif // SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
                        tmp_coap_hdr_ptr->coap_status = COAP_STATUS_BUILDER_MESSAGE_SENDING_FAILED;
                        handle->sn_coap_rx_callback(tmp_coap_hdr_ptr, &stored_msg_ptr->send_msg_ptr.dst_addr_ptr, stored_msg_ptr->param);

                        sn_coap_parser_release_allocated_coap_msg_mem(handle, tmp_coap_hdr_ptr);
                    }
                }

                /* Free memory of stored message */
                sn_coap_protocol_release_allocated_send_msg_mem(handle, stored_msg_ptr);
            } else {
                /* Send message  */
                handle->sn_coap_tx_callback(stored_msg_ptr->send_msg_ptr.packet_ptr,
                                            stored_msg_ptr->send_msg_ptr.packet_len, &stored_msg_ptr->send_msg_ptr.dst_addr_ptr, stored_msg_ptr->param);

                /* * * Count new Resending time  * * */
                stored_msg_ptr->resending_time = sn_coap_calculate_new_resend_time(current_time,
                                                                                   handle->sn_coap_resending_intervall,
                                                                                   stored_msg_ptr->resending_counter);
            }
            /* Callback routine could have wiped the list (eg as a response to sending failed) */
            /* Be super cautious and rescan from the start */
            goto rescan;
        }
    }

#endif /* ENABLE_RESENDINGS */

    return 0;
}

#if ENABLE_RESENDINGS  /* If Message resending is not used at all, this part of code will not be compiled */

/**************************************************************************//**
 * \fn static uint8_t sn_coap_protocol_linked_list_send_msg_store(sn_nsdl_addr_s *dst_addr_ptr, uint16_t send_packet_data_len, uint8_t *send_packet_data_ptr, uint32_t sending_time)
 *
 * \brief Stores message to Linked list for sending purposes.

 * \param *dst_addr_ptr is pointer to destination address where CoAP message will be sent
 *
 * \param send_packet_data_len is length of Packet data to be stored
 *
 * \param *send_packet_data_ptr is Packet data to be stored
 *
 * \param sending_time is stored sending time
 *
 * \return 0 Allocation or buffer limit reached
 *
 * \return 1 Msg stored properly
 *****************************************************************************/

static uint8_t sn_coap_protocol_linked_list_send_msg_store(struct coap_s *restrict handle, sn_nsdl_addr_s *restrict dst_addr_ptr, uint_fast16_t send_packet_data_len,
                                                           uint8_t *restrict send_packet_data_ptr, uint32_t sending_time, void *param)
{

    coap_send_msg_s *restrict stored_msg_ptr;

    /* If both queue parameters are "0" or resending count is "0", then re-sending is disabled */
    if (((handle->sn_coap_resending_queue_msgs == 0) && (handle->sn_coap_resending_queue_bytes == 0)) || (handle->sn_coap_resending_count == 0)) {
        return 1;
    }

    if (handle->sn_coap_resending_queue_msgs > 0) {
        if (handle->count_resent_msgs >= handle->sn_coap_resending_queue_msgs) {
            tr_error("sn_coap_protocol_linked_list_send_msg_store - resend queue full!");
            return 0;
        }
    }

    /* Count resending queue size, if buffer size is defined */
    if (handle->sn_coap_resending_queue_bytes > 0) {
        if ((sn_coap_count_linked_list_size(&handle->linked_list_resent_msgs) + send_packet_data_len) > handle->sn_coap_resending_queue_bytes) {
            tr_error("sn_coap_protocol_linked_list_send_msg_store - resend buffer size reached!");
            return 0;
        }
    }

    /* Allocating memory for stored message */
    stored_msg_ptr = sn_coap_protocol_allocate_mem_for_msg(handle, dst_addr_ptr, send_packet_data_len);

    if (stored_msg_ptr == 0) {
        tr_error("sn_coap_protocol_linked_list_send_msg_store - failed to allocate message!");
        return 0;
    }

    /* Filling of coap_send_msg_s with initialization values */
    stored_msg_ptr->resending_counter = 0;
    stored_msg_ptr->resending_time = sending_time;

    /* Filling of sn_nsdl_transmit_s */
    stored_msg_ptr->send_msg_ptr.protocol = SN_NSDL_PROTOCOL_COAP;
    stored_msg_ptr->send_msg_ptr.packet_len = send_packet_data_len;
    memcpy(stored_msg_ptr->send_msg_ptr.packet_ptr, send_packet_data_ptr, send_packet_data_len);

    /* Filling of sn_nsdl_addr_s */
    stored_msg_ptr->send_msg_ptr.dst_addr_ptr.type = dst_addr_ptr->type;
    stored_msg_ptr->send_msg_ptr.dst_addr_ptr.addr_len = dst_addr_ptr->addr_len;
    memcpy(stored_msg_ptr->send_msg_ptr.dst_addr_ptr.addr_ptr, dst_addr_ptr->addr_ptr, dst_addr_ptr->addr_len);
    stored_msg_ptr->send_msg_ptr.dst_addr_ptr.port = dst_addr_ptr->port;

    stored_msg_ptr->param = param;

    /* Storing Resending message to Linked list */
    ns_list_add_to_end(&handle->linked_list_resent_msgs, stored_msg_ptr);
    ++handle->count_resent_msgs;
    return 1;
}


/**************************************************************************//**
 * \fn static void sn_coap_protocol_linked_list_send_msg_remove(sn_nsdl_addr_s *src_addr_ptr, uint16_t msg_id)
 *
 * \brief Removes stored resending message from Linked list
 *
 * \param *src_addr_ptr is searching key for searched message
 * \param msg_id is searching key for removed message
 *****************************************************************************/

static void sn_coap_protocol_linked_list_send_msg_remove(struct coap_s *restrict handle, const sn_nsdl_addr_s *restrict src_addr_ptr, uint16_t msg_id)
{
    /* Loop all stored resending messages in Linked list */
    ns_list_foreach(coap_send_msg_s, stored_msg_ptr, &handle->linked_list_resent_msgs) {
        /* Get message ID from stored resending message */
        uint16_t temp_msg_id = read_packet_msg_id(stored_msg_ptr);
        /* If message's Message ID is same than is searched */
        if (temp_msg_id == msg_id) {
            /* If message's Source address and port is same than is searched */

            if (compare_port(src_addr_ptr, &stored_msg_ptr->send_msg_ptr.dst_addr_ptr)) {
                /* * * Message found * * */
                /* Remove message from Linked list */
                ns_list_remove(&handle->linked_list_resent_msgs, stored_msg_ptr);
                --handle->count_resent_msgs;

                /* Free memory of stored message */
                sn_coap_protocol_release_allocated_send_msg_mem(handle, stored_msg_ptr);

                return;
            }
        }
    }
}

uint32_t sn_coap_calculate_new_resend_time(const uint32_t current_time, const uint8_t interval, const uint8_t counter)
{
    uint32_t resend_time = interval << counter;
    uint16_t random_factor = randLIB_get_random_in_range(100, RESPONSE_RANDOM_FACTOR * 100);
    return current_time + ((resend_time * random_factor) / 100);
}

#endif /* ENABLE_RESENDINGS */

void sn_coap_protocol_send_rst(struct coap_s *handle, uint16_t msg_id, sn_nsdl_addr_s *addr_ptr, void *param)
{
    uint8_t packet_ptr[4];

    /* Add CoAP version and message type */
    packet_ptr[0] = COAP_VERSION_1;
    packet_ptr[0] |= COAP_MSG_TYPE_RESET;

    /* Add message code */
    packet_ptr[1] = COAP_MSG_CODE_EMPTY;

    /* Add message ID */
    packet_ptr[2] = msg_id >> 8;
    packet_ptr[3] = (uint8_t)msg_id;

    /* Send RST */
    handle->sn_coap_tx_callback(packet_ptr, 4, addr_ptr, param);

}

uint16_t sn_coap_protocol_get_configured_blockwise_size(struct coap_s *handle)
{
    return handle->sn_coap_block_data_size;
}

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT /* If Message duplication detection is not used at all, this part of code will not be compiled */

/**************************************************************************//**
 * \fn static void sn_coap_protocol_linked_list_duplication_info_store(sn_nsdl_addr_s *addr_ptr, uint16_t msg_id)
 *
 * \brief Stores Duplication info to Linked list
 *
 * \param msg_id is Message ID to be stored
 * \param *addr_ptr is pointer to Address information to be stored
 *****************************************************************************/

static void sn_coap_protocol_linked_list_duplication_info_store(struct coap_s *restrict handle, sn_nsdl_addr_s *restrict addr_ptr,
                                                                uint16_t msg_id, void *param)
{
    coap_duplication_info_s *restrict stored_duplication_info_ptr = NULL;

    /* * * * Allocating memory for stored Duplication info * * * */

    /* Allocate memory for stored Duplication info's structure */
    stored_duplication_info_ptr = sn_coap_protocol_calloc(handle, sizeof(coap_duplication_info_s));

    if (stored_duplication_info_ptr == NULL) {
        tr_error("sn_coap_protocol_linked_list_duplication_info_store - failed to allocate duplication info!");
        return;
    }

    /* Allocate memory for stored Duplication info's address */
    stored_duplication_info_ptr->address = sn_coap_protocol_calloc(handle, sizeof(sn_nsdl_addr_s));
    if (stored_duplication_info_ptr->address == NULL) {
        tr_error("sn_coap_protocol_linked_list_duplication_info_store - failed to allocate address!");
        sn_coap_protocol_duplication_info_free(handle, stored_duplication_info_ptr);
        return;
    }

    stored_duplication_info_ptr->address->addr_ptr = handle->sn_coap_protocol_malloc(addr_ptr->addr_len);

    if (stored_duplication_info_ptr->address->addr_ptr == NULL) {
        tr_error("sn_coap_protocol_linked_list_duplication_info_store - failed to allocate address pointer!");
        sn_coap_protocol_duplication_info_free(handle, stored_duplication_info_ptr);
        return;
    }

    /* * * * Filling fields of stored Duplication info * * * */
    stored_duplication_info_ptr->timestamp = handle->system_time;
    stored_duplication_info_ptr->address->addr_len = addr_ptr->addr_len;
    memcpy(stored_duplication_info_ptr->address->addr_ptr, addr_ptr->addr_ptr, addr_ptr->addr_len);
    stored_duplication_info_ptr->address->port = addr_ptr->port;
    stored_duplication_info_ptr->msg_id = msg_id;

    stored_duplication_info_ptr->param = param;
    /* * * * Storing Duplication info to Linked list * * * */

    ns_list_add_to_end(&handle->linked_list_duplication_msgs, stored_duplication_info_ptr);
    ++handle->count_duplication_msgs;
}

/**************************************************************************//**
 * \fn static coap_duplication_info_s *sn_coap_protocol_linked_list_duplication_info_search(const struct coap_s *handle, const sn_nsdl_addr_s *scr_addr_ptr, const uint16_t msg_id)
 *
 * \brief Searches stored message from Linked list (Address and Message ID as key)
 *
 * \param *addr_ptr is pointer to Address key to be searched
 * \param msg_id is Message ID key to be searched
 *
 * \return Return value is 0 when message found and -1 if not found
 *****************************************************************************/

static coap_duplication_info_s *sn_coap_protocol_linked_list_duplication_info_search(const struct coap_s *handle,
                                                                                     const sn_nsdl_addr_s *addr_ptr, const uint16_t msg_id)
{
    /* Loop all nodes in Linked list for searching Message ID */
    ns_list_foreach(coap_duplication_info_s, stored_duplication_info_ptr, &handle->linked_list_duplication_msgs) {
        /* If message's Message ID is same than is searched */
        if (stored_duplication_info_ptr->msg_id == msg_id) {
            /* If message's Source address & port is same than is searched */
            if (compare_port(addr_ptr, stored_duplication_info_ptr->address)) {
                /* * * Correct Duplication info found * * * */
                return stored_duplication_info_ptr;
            }
        }
    }
    return NULL;
}



/**************************************************************************//**
 * \fn static void sn_coap_protocol_linked_list_duplication_info_remove_old_ones(struct coap_s *handle)
 *
 * \brief Removes old stored Duplication detection infos from Linked list
 *****************************************************************************/

static void sn_coap_protocol_linked_list_duplication_info_remove_old_ones(struct coap_s *handle)
{

    ns_list_foreach_safe(coap_duplication_info_s, removed_duplication_info_ptr, &handle->linked_list_duplication_msgs) {
        if ((handle->system_time - removed_duplication_info_ptr->timestamp)  > SN_COAP_DUPLICATION_MAX_TIME_MSGS_STORED) {
            /* * * * Old Duplication info found, remove it from Linked list * * * */
            ns_list_remove(&handle->linked_list_duplication_msgs, removed_duplication_info_ptr);
            --handle->count_duplication_msgs;

            /* Free memory of stored Duplication info */
            sn_coap_protocol_duplication_info_free(handle, removed_duplication_info_ptr);
        }
    }
}

#endif /* SN_COAP_DUPLICATION_MAX_MSGS_COUNT */

void sn_coap_protocol_linked_list_duplication_info_remove(struct coap_s *handle, const uint8_t *scr_addr_ptr, const uint16_t port, const uint16_t msg_id)
{
#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
    /* Loop all stored duplication messages in Linked list */
    ns_list_foreach(coap_duplication_info_s, removed_duplication_info_ptr, &handle->linked_list_duplication_msgs) {
        /* If message's Address is same than is searched */
        if (0 == memcmp(scr_addr_ptr,
                        removed_duplication_info_ptr->address->addr_ptr,
                        removed_duplication_info_ptr->address->addr_len)) {
            /* If message's Address prt is same than is searched */
            if (removed_duplication_info_ptr->address->port == port) {
                /* If Message ID is same than is searched */
                if (removed_duplication_info_ptr->msg_id == msg_id) {
                    /* * * * Correct Duplication info found, remove it from Linked list * * * */
                    tr_info("sn_coap_protocol_linked_list_duplication_info_remove - message id %d removed", msg_id);
                    ns_list_remove(&handle->linked_list_duplication_msgs, removed_duplication_info_ptr);
                    --handle->count_duplication_msgs;

                    /* Free memory of stored Duplication info */
                    sn_coap_protocol_duplication_info_free(handle, removed_duplication_info_ptr);
                    return;
                }
            }
        }
    }
#else
    (void)handle;
    (void)scr_addr_ptr;
    (void)port;
    (void)msg_id;
#endif //SN_COAP_DUPLICATION_MAX_MSGS_COUNT
}

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
static void sn_coap_protocol_duplication_info_free(struct coap_s *handle, coap_duplication_info_s *duplication_info_ptr)
{
    // General purpose free functions ignore null pointer inputs - this
    // private one knows it never receives null.
    if (duplication_info_ptr->address) {
        handle->sn_coap_protocol_free(duplication_info_ptr->address->addr_ptr);
        handle->sn_coap_protocol_free(duplication_info_ptr->address);
    }
    handle->sn_coap_protocol_free(duplication_info_ptr->packet_ptr);
    handle->sn_coap_protocol_free(duplication_info_ptr);
}
#endif // SN_COAP_DUPLICATION_MAX_MSGS_COUNT

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE
/**************************************************************************//**
 * \fn static void sn_coap_protocol_linked_list_blockwise_msg_remove(struct coap_s *handle, coap_blockwise_msg_s *removed_msg_ptr)
 *
 * \brief Removes stored blockwise message from Linked list
 *
 * \param removed_msg_ptr is message to be removed
 *****************************************************************************/

static void sn_coap_protocol_linked_list_blockwise_msg_remove(struct coap_s *handle, coap_blockwise_msg_s *removed_msg_ptr)
{
    ns_list_remove(&handle->linked_list_blockwise_sent_msgs, removed_msg_ptr);

    if (removed_msg_ptr->coap_msg_ptr) {
        handle->sn_coap_protocol_free(removed_msg_ptr->coap_msg_ptr->payload_ptr);

        sn_coap_parser_release_allocated_coap_msg_mem(handle, removed_msg_ptr->coap_msg_ptr);
    }

    handle->sn_coap_protocol_free(removed_msg_ptr);
}

/**************************************************************************//**
 * \fn static void sn_coap_protocol_linked_list_blockwise_payload_store(sn_nsdl_addr_s *addr_ptr, uint16_t payload_len, uint8_t *payload_ptr)
 *
 * \brief Stores blockwise payload to Linked list
 *
 * \param *addr_ptr is pointer to Address information to be stored
 * \param payload_len is length of incoming payload
 * \param *payload_ptr is pointer to stored incoming payload
 * \param *token_ptr is pointer to stored token
 * \param token_len is length of the stored token
 * \param block_number Block number to be stored
 * \param block_size Size on block to be stored
 * \param size1 Size of the whole incoming message
 *****************************************************************************/

static void sn_coap_protocol_linked_list_blockwise_payload_store(struct coap_s *restrict handle, sn_nsdl_addr_s *restrict addr_ptr,
                                                                 uint16_t payload_len,
                                                                 uint8_t *restrict payload_ptr,
                                                                 uint8_t *restrict token_ptr,
                                                                 uint8_t token_len,
                                                                 uint32_t block_number,
                                                                 uint16_t block_size,
                                                                 uint32_t size1)
{
    if (!addr_ptr || !payload_len || !payload_ptr) {
        return;
    }

    // Do not add duplicates to list, this could happen if server needs to retransmit block message again
    if (sn_coap_protocol_linked_list_blockwise_payload_search_compare_block_number(handle,
                                                                                   addr_ptr,
                                                                                   token_ptr,
                                                                                   token_len,
                                                                                   block_number)) {
        return;
    }

    coap_blockwise_payload_s *restrict stored_blockwise_payload_ptr = sn_coap_protocol_linked_list_blockwise_search(handle, addr_ptr, token_ptr, token_len);

    if (stored_blockwise_payload_ptr && stored_blockwise_payload_ptr->use_size1) {
        memcpy(stored_blockwise_payload_ptr->payload_ptr + (block_number * block_size), payload_ptr, payload_len);
    } else if (stored_blockwise_payload_ptr) {
        uint16_t new_len = stored_blockwise_payload_ptr->payload_len + payload_len;
        tr_debug("sn_coap_protocol_linked_list_blockwise_payload_store - reallocate from %d to %d", stored_blockwise_payload_ptr->payload_len, new_len);

        uint8_t *restrict temp_ptr = handle->sn_coap_protocol_malloc(stored_blockwise_payload_ptr->payload_len);
        if (temp_ptr == NULL) {
            tr_error("sn_coap_protocol_linked_list_blockwise_payload_store - failed to allocate temp buffer!");
            sn_coap_protocol_linked_list_blockwise_payload_remove(handle, stored_blockwise_payload_ptr);
            return;
        }

        memcpy(temp_ptr, stored_blockwise_payload_ptr->payload_ptr, stored_blockwise_payload_ptr->payload_len);

        handle->sn_coap_protocol_free(stored_blockwise_payload_ptr->payload_ptr);
        stored_blockwise_payload_ptr->payload_ptr = handle->sn_coap_protocol_malloc(new_len);

        if (stored_blockwise_payload_ptr->payload_ptr == NULL) {
            tr_error("sn_coap_protocol_linked_list_blockwise_payload_store - failed to reallocate payload!");
            handle->sn_coap_protocol_free(temp_ptr);
            sn_coap_protocol_linked_list_blockwise_payload_remove(handle, stored_blockwise_payload_ptr);
            return;
        }

        memcpy(stored_blockwise_payload_ptr->payload_ptr, temp_ptr, stored_blockwise_payload_ptr->payload_len);
        memcpy(stored_blockwise_payload_ptr->payload_ptr + stored_blockwise_payload_ptr->payload_len, payload_ptr, payload_len);
        stored_blockwise_payload_ptr->payload_len = new_len;
        handle->sn_coap_protocol_free(temp_ptr);

    } else {
        stored_blockwise_payload_ptr = NULL;
        stored_blockwise_payload_ptr = handle->sn_coap_protocol_malloc(sizeof(coap_blockwise_payload_s));

        if (stored_blockwise_payload_ptr == NULL) {
            tr_error("sn_coap_protocol_linked_list_blockwise_payload_store - failed to allocate blockwise!");
            return;
        }

        // If size1 option present then we can allocate the whole payload before hand.
        stored_blockwise_payload_ptr->use_size1 = size1 ? 1 : 0;

        /* Allocate memory for stored Payload's data */
        if (stored_blockwise_payload_ptr->use_size1) {
            stored_blockwise_payload_ptr->payload_ptr = handle->sn_coap_protocol_malloc(size1);
            if (stored_blockwise_payload_ptr->payload_ptr) {
                memcpy(stored_blockwise_payload_ptr->payload_ptr, payload_ptr, payload_len);
                stored_blockwise_payload_ptr->payload_len = size1;
            }
        } else {
            stored_blockwise_payload_ptr->payload_ptr = sn_coap_protocol_malloc_copy(handle, payload_ptr, payload_len);
            stored_blockwise_payload_ptr->payload_len = payload_len;
        }

        if (stored_blockwise_payload_ptr->payload_ptr == NULL) {
            tr_error("sn_coap_protocol_linked_list_blockwise_payload_store - failed to allocate payload!");
            handle->sn_coap_protocol_free(stored_blockwise_payload_ptr);
            return;
        }

        /* Allocate memory for stored Payload's address */
        stored_blockwise_payload_ptr->addr_ptr = sn_coap_protocol_malloc_copy(handle, addr_ptr->addr_ptr, addr_ptr->addr_len);

        if (stored_blockwise_payload_ptr->addr_ptr == NULL) {
            tr_error("sn_coap_protocol_linked_list_blockwise_payload_store - failed to allocate address pointer!");
            handle->sn_coap_protocol_free(stored_blockwise_payload_ptr->payload_ptr);
            handle->sn_coap_protocol_free(stored_blockwise_payload_ptr);
            return;
        }

        /* Allocate & copy token number */
        if (token_ptr && token_len) {
            stored_blockwise_payload_ptr->token_ptr = sn_coap_protocol_malloc_copy(handle, token_ptr, token_len);

            if (!stored_blockwise_payload_ptr->token_ptr) {
                tr_error("sn_coap_protocol_linked_list_blockwise_payload_store - failed to allocate token pointer!");
                handle->sn_coap_protocol_free(stored_blockwise_payload_ptr->addr_ptr);
                handle->sn_coap_protocol_free(stored_blockwise_payload_ptr->payload_ptr);
                handle->sn_coap_protocol_free(stored_blockwise_payload_ptr);
                return;
            }

            stored_blockwise_payload_ptr->token_len = token_len;
        } else {
            stored_blockwise_payload_ptr->token_ptr = NULL;
            stored_blockwise_payload_ptr->token_len = 0;
        }

        /* * * * Filling fields of stored Payload  * * * */
        stored_blockwise_payload_ptr->port = addr_ptr->port;

        /* * * * Storing Payload to Linked list  * * * */
        ns_list_add_to_end(&handle->linked_list_blockwise_received_payloads, stored_blockwise_payload_ptr);
    }

    stored_blockwise_payload_ptr->block_number = block_number;
    stored_blockwise_payload_ptr->timestamp = handle->system_time;
}

/**************************************************************************//**
 * \fn static uint8_t *sn_coap_protocol_linked_list_blockwise_payload_search(sn_nsdl_addr_s *src_addr_ptr, uint16_t *payload_length)
 *
 * \brief Searches stored blockwise payload from Linked list (Address as key)
 *
 * \param *addr_ptr is pointer to Address key to be searched
 * \param *payload_length is pointer to returned Payload length
 *
 * \return Return value is pointer to found stored blockwise payload in Linked
 *         list or NULL if payload not found
 *****************************************************************************/

static uint8_t *sn_coap_protocol_linked_list_blockwise_payload_search(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, uint16_t *payload_length, const uint8_t *token_ptr, uint8_t token_len)
{
    /* Loop all stored blockwise payloads in Linked list */
    ns_list_foreach(coap_blockwise_payload_s, stored_payload_info_ptr, &handle->linked_list_blockwise_received_payloads) {
        /* If payload's Source address and port is same than is searched */
        if ((0 == memcmp(src_addr_ptr->addr_ptr, stored_payload_info_ptr->addr_ptr, src_addr_ptr->addr_len)) && (stored_payload_info_ptr->port == src_addr_ptr->port)) {
            /* Check token */
            if (token_ptr) {
                if (!stored_payload_info_ptr->token_ptr || (token_len != stored_payload_info_ptr->token_len) || (memcmp(stored_payload_info_ptr->token_ptr, token_ptr, token_len))) {
                    continue;
                }
            } else if (stored_payload_info_ptr->token_ptr) {
                continue;
            }
            /* * * Correct Payload found * * * */
            *payload_length = stored_payload_info_ptr->payload_len;
            return stored_payload_info_ptr->payload_ptr;
        }
    }

    return NULL;
}

/**************************************************************************//**
 * \fn static uint8_t *sn_coap_protocol_linked_list_blockwise_search(sn_nsdl_addr_s *src_addr_ptr, uint16_t *payload_length)
 *
 * \brief Searches stored blockwise message from Linked list (Address as key)
 *
 * \param *addr_ptr is pointer to Address key to be searched
 * \param *payload_length is pointer to returned Payload length
 *
 * \return Return value is pointer to found stored blockwise message in Linked
 *         list or NULL if message not found
 *****************************************************************************/
static coap_blockwise_payload_s *sn_coap_protocol_linked_list_blockwise_search(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, const uint8_t *token_ptr, uint8_t token_len)
{
    /* Loop all stored blockwise payloads in Linked list */
    ns_list_foreach(coap_blockwise_payload_s, stored_payload_info_ptr, &handle->linked_list_blockwise_received_payloads) {
        /* If payload's Source address and port is same than is searched */
        if ((0 == memcmp(src_addr_ptr->addr_ptr, stored_payload_info_ptr->addr_ptr, src_addr_ptr->addr_len)) && (stored_payload_info_ptr->port == src_addr_ptr->port)) {
            /* Check token */
            if (token_ptr) {
                if (!stored_payload_info_ptr->token_ptr || (token_len != stored_payload_info_ptr->token_len) || (memcmp(stored_payload_info_ptr->token_ptr, token_ptr, token_len))) {
                    continue;
                }
            } else if (stored_payload_info_ptr->token_ptr) {
                continue;
            }
            return stored_payload_info_ptr;
        }
    }

    return NULL;
}

static bool sn_coap_protocol_linked_list_blockwise_payload_search_compare_block_number(struct coap_s *handle,
                                                                                       const sn_nsdl_addr_s *src_addr_ptr,
                                                                                       const uint8_t *token_ptr,
                                                                                       uint8_t token_len,
                                                                                       uint32_t block_number)
{
    /* Loop all stored blockwise payloads in Linked list */
    ns_list_foreach(coap_blockwise_payload_s, stored_payload_info_ptr, &handle->linked_list_blockwise_received_payloads) {
        /* If payload's Source address and port is same than is searched */
        if ((0 == memcmp(src_addr_ptr->addr_ptr, stored_payload_info_ptr->addr_ptr, src_addr_ptr->addr_len)) && (stored_payload_info_ptr->port == src_addr_ptr->port)) {
            /* Check token number */
            if (token_ptr) {
                if (!stored_payload_info_ptr->token_ptr || (token_len != stored_payload_info_ptr->token_len) || (memcmp(stored_payload_info_ptr->token_ptr, token_ptr, token_len))) {
                    continue;
                }
            } else if (stored_payload_info_ptr->token_ptr) {
                continue;
            }
            // Check that stored block number matches to given one
            if (block_number == stored_payload_info_ptr->block_number) {
                return true;
            }
        }
    }

    return false;
}

/**************************************************************************//**
 * \fn static void sn_coap_protocol_linked_list_blockwise_payload_remove(struct coap_s *handle,
 *                                                      coap_blockwise_msg_s *removed_msg_ptr)
 *
 * \brief Removes stored blockwise payload from Linked list
 *
 * \param removed_payload_ptr is payload to be removed
 *****************************************************************************/

static void sn_coap_protocol_linked_list_blockwise_payload_remove(struct coap_s *handle,
                                                                  coap_blockwise_payload_s *removed_payload_ptr)
{
    ns_list_remove(&handle->linked_list_blockwise_received_payloads, removed_payload_ptr);
    /* Free memory of stored payload */
    handle->sn_coap_protocol_free(removed_payload_ptr->addr_ptr);
    handle->sn_coap_protocol_free(removed_payload_ptr->payload_ptr);
    handle->sn_coap_protocol_free(removed_payload_ptr->token_ptr);

    handle->sn_coap_protocol_free(removed_payload_ptr);
}

/**************************************************************************//**
 * \fn static uint32_t sn_coap_protocol_linked_list_blockwise_payloads_get_len(sn_nsdl_addr_s *src_addr_ptr)
 *
 * \brief Counts length of Payloads in Linked list (Address as key)
 *
 * \param *addr_ptr is pointer to Address key
 *
 * \return Return value is length of Payloads as bytes
 *****************************************************************************/

static uint32_t sn_coap_protocol_linked_list_blockwise_payloads_get_len(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, const uint8_t *token_ptr, uint8_t token_len)
{
    uint32_t ret_whole_payload_len = 0;
    /* Loop all stored blockwise payloads in Linked list */
    ns_list_foreach(coap_blockwise_payload_s, searched_payload_info_ptr, &handle->linked_list_blockwise_received_payloads) {
        /* If payload's Source address and port is same than is searched */
        if ((0 == memcmp(src_addr_ptr->addr_ptr, searched_payload_info_ptr->addr_ptr, src_addr_ptr->addr_len)) && (searched_payload_info_ptr->port == src_addr_ptr->port)) {
            /* Check token */
            if (token_ptr) {
                if (!searched_payload_info_ptr->token_ptr || (token_len != searched_payload_info_ptr->token_len) || (memcmp(searched_payload_info_ptr->token_ptr, token_ptr, token_len))) {
                    continue;
                }
            } else if (searched_payload_info_ptr->token_ptr) {
                continue;
            }
            /* * * Correct Payload found * * * */
            ret_whole_payload_len += searched_payload_info_ptr->payload_len;
        }
    }

    return ret_whole_payload_len;
}

/**************************************************************************//**
 * \fn static void sn_coap_protocol_handle_blockwise_timeout(struct coap_s *handle)
 *
 * \brief Check incoming and outgoing blockwise messages for time out.
 *        Remove timed out messages from lists. Notify application if
 *        outgoing message times out.
 *****************************************************************************/

static void sn_coap_protocol_handle_blockwise_timeout(struct coap_s *handle)
{
    /* Loop all incoming Blockwise messages */
    ns_list_foreach_safe(coap_blockwise_payload_s, removed_blocwise_payload_ptr, &handle->linked_list_blockwise_received_payloads) {
        if ((handle->system_time - removed_blocwise_payload_ptr->timestamp)  > SN_COAP_BLOCKWISE_MAX_TIME_DATA_STORED) {
            /* * * * This messages has timed out, remove it from Linked list * * * */
            sn_coap_protocol_linked_list_blockwise_payload_remove(handle, removed_blocwise_payload_ptr);
        }
    }
}

#endif /* SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE */


#if ENABLE_RESENDINGS  /* If Message resending is not used at all, this part of code will not be compiled */
/***************************************************************************//**
 * \fn int8_t sn_coap_protocol_allocate_mem_for_msg(sn_nsdl_addr_s *dst_addr_ptr, uint16_t packet_data_len, coap_send_msg_s *msg_ptr)
 *
 * \brief Allocates memory for given message (send or blockwise message)
 *
 * \param *dst_addr_ptr is pointer to destination address where message will be sent
 * \param packet_data_len is length of allocated Packet data
 * \param uri_path_len is length of messages path url
 *
 * \return pointer to allocated struct
 *****************************************************************************/

coap_send_msg_s *sn_coap_protocol_allocate_mem_for_msg(struct coap_s *handle, sn_nsdl_addr_s *dst_addr_ptr, uint_fast16_t packet_data_len)
{

    coap_send_msg_s *msg_ptr = sn_coap_protocol_calloc(handle, sizeof(coap_send_msg_s));

    if (msg_ptr == NULL) {
        return 0;
    }

    msg_ptr->send_msg_ptr.packet_ptr = handle->sn_coap_protocol_malloc(packet_data_len);

    msg_ptr->send_msg_ptr.dst_addr_ptr.addr_ptr = sn_coap_protocol_calloc(handle, dst_addr_ptr->addr_len);

    if ((msg_ptr->send_msg_ptr.dst_addr_ptr.addr_ptr == NULL) ||
            (msg_ptr->send_msg_ptr.packet_ptr == NULL)) {

        sn_coap_protocol_release_allocated_send_msg_mem(handle, msg_ptr);
        return 0;
    }

    return msg_ptr;
}


/**************************************************************************//**
 * \fn static void sn_coap_protocol_release_allocated_send_msg_mem(struct coap_s *handle, coap_send_msg_s *freed_send_msg_ptr)
 *
 * \brief Releases memory of given Sending message (coap_send_msg_s)
 *
 * \param *freed_send_msg_ptr is pointer to released Sending message
 *****************************************************************************/

static void sn_coap_protocol_release_allocated_send_msg_mem(struct coap_s *handle, coap_send_msg_s *freed_send_msg_ptr)
{
    if (freed_send_msg_ptr != NULL) {

        handle->sn_coap_protocol_free(freed_send_msg_ptr->send_msg_ptr.dst_addr_ptr.addr_ptr);

        handle->sn_coap_protocol_free(freed_send_msg_ptr->send_msg_ptr.packet_ptr);

        handle->sn_coap_protocol_free(freed_send_msg_ptr);
    }
}

/**************************************************************************//**
 * \fn static uint16_t sn_coap_count_linked_list_size(const coap_send_msg_list_t *linked_list_ptr)
 *
 * \brief Counts total message size of all messages in linked list
 *
 * \param const coap_send_msg_list_t *linked_list_ptr pointer to linked list
 *****************************************************************************/
static uint_fast16_t sn_coap_count_linked_list_size(const coap_send_msg_list_t *linked_list_ptr)
{
    uint_fast16_t total_size = 0;

    ns_list_foreach(coap_send_msg_s, stored_msg_ptr, linked_list_ptr) {
        total_size += stored_msg_ptr->send_msg_ptr.packet_len;
    }

    return total_size;
}

#endif

#if SN_COAP_BLOCKWISE_ENABLED || SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE

static coap_blockwise_msg_s *search_sent_blockwise_message(struct coap_s *handle, uint16_t msg_id)
{
    ns_list_foreach(coap_blockwise_msg_s, tmp, &handle->linked_list_blockwise_sent_msgs) {
        if (tmp->coap_msg_ptr && tmp->coap_msg_ptr->msg_id == msg_id) {
            return tmp;
        }
    }

    return NULL;
}

void sn_coap_protocol_remove_sent_blockwise_message(struct coap_s *handle, uint16_t msg_id)
{
    if (!handle) {
        return;
    }

    coap_blockwise_msg_s *tmp = search_sent_blockwise_message(handle, msg_id);

    if (tmp) {
        handle->sn_coap_protocol_free(tmp->coap_msg_ptr->payload_ptr);
        sn_coap_parser_release_allocated_coap_msg_mem(handle, tmp->coap_msg_ptr);
        ns_list_remove(&handle->linked_list_blockwise_sent_msgs, tmp);
        handle->sn_coap_protocol_free(tmp);
    }
}

void sn_coap_protocol_block_remove(struct coap_s *handle, sn_nsdl_addr_s *source_address, uint16_t payload_length, void *payload)
{
    if (!handle || !source_address || !payload) {
        return;
    }

    /* Loop all stored blockwise payloads in Linked list */
    ns_list_foreach(coap_blockwise_payload_s, stored_payload_info_ptr, &handle->linked_list_blockwise_received_payloads) {
        /* If payload's Source address is not the same than is searched */
        if (memcmp(source_address->addr_ptr, stored_payload_info_ptr->addr_ptr, source_address->addr_len)) {
            continue;
        }

        /* If payload's Source address port is not the same than is searched */
        if (stored_payload_info_ptr->port != source_address->port) {
            continue;
        }

        /* Check the payload */
        if (payload_length != stored_payload_info_ptr->payload_len) {
            continue;
        }

        if (!memcmp(stored_payload_info_ptr->payload_ptr, payload, stored_payload_info_ptr->payload_len)) {
            /* Everything matches, remove and return. */
            sn_coap_protocol_linked_list_blockwise_payload_remove(handle, stored_payload_info_ptr);
            return;
        }
    }
}

/****************************************************************************
 * \fn coap_blockwise_msg_s *sn_coap_stored_blockwise_msg_get(struct coap_s *handle, sn_coap_hdr_s *received_coap_msg_ptr)
 *
 * \brief Get blockwise message from list
 *
 * \param *handle Pointer to the coap handle structure
 * \param *received_coap_msg_ptr Pointer to parsed CoAP message structure
 *****************************************************************************/
static coap_blockwise_msg_s *sn_coap_stored_blockwise_msg_get(struct coap_s *handle, sn_coap_hdr_s *received_coap_msg_ptr)
{
    ns_list_foreach(coap_blockwise_msg_s, msg, &handle->linked_list_blockwise_sent_msgs) {
        if (!received_coap_msg_ptr->token_ptr && !msg->coap_msg_ptr->token_ptr) {
            return msg;
        } else if ((received_coap_msg_ptr->token_len == msg->coap_msg_ptr->token_len) && (!memcmp(received_coap_msg_ptr->token_ptr, msg->coap_msg_ptr->token_ptr, received_coap_msg_ptr->token_len))) {
            return msg;
        }
    }

    return ns_list_get_first(&handle->linked_list_blockwise_sent_msgs);
}

/**************************************************************************//**
 * \fn static int8_t sn_coap_handle_blockwise_message(void)
 *
 * \brief Handles all received blockwise messages
 *
 * \param *src_addr_ptr pointer to source address information struct
 * \param *received_coap_msg_ptr pointer to parsed CoAP message structure
 *****************************************************************************/

static sn_coap_hdr_s *sn_coap_handle_blockwise_message(struct coap_s *handle, sn_nsdl_addr_s *src_addr_ptr, sn_coap_hdr_s *received_coap_msg_ptr, void *param, bool *keep_in_resend_queue)
{
    sn_coap_hdr_s *src_coap_blockwise_ack_msg_ptr = NULL;
    uint16_t dst_packed_data_needed_mem = 0;
    uint8_t *restrict dst_ack_packet_data_ptr = NULL;
    uint8_t block_temp = 0;

    uint16_t original_payload_len = 0;
    uint8_t *original_payload_ptr = NULL;

    /* Block1 Option in a request (e.g., PUT or POST) */
    // Blocked request sending, received ACK, sending next block..
    if (received_coap_msg_ptr->options_list_ptr->block1 != COAP_OPTION_BLOCK_NONE) {
        if (received_coap_msg_ptr->msg_code > COAP_MSG_CODE_REQUEST_DELETE) {

            coap_blockwise_msg_s *stored_blockwise_msg_temp_ptr;
            stored_blockwise_msg_temp_ptr = search_sent_blockwise_message(handle, received_coap_msg_ptr->msg_id);

            if (received_coap_msg_ptr->options_list_ptr->block1 & 0x08) {
                received_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_ACK;

                if (stored_blockwise_msg_temp_ptr) {
                    /* Build response message */

                    uint_fast16_t block_size;
                    uint32_t block_number;
                    uint32_t req_block_number;

                    src_coap_blockwise_ack_msg_ptr = stored_blockwise_msg_temp_ptr->coap_msg_ptr;

                    /* Get block option parameters from received message */
                    block_number = received_coap_msg_ptr->options_list_ptr->block1 >> 4;
                    block_temp = received_coap_msg_ptr->options_list_ptr->block1 & 0x07;
                    block_size = 16u << block_temp;

                    if (src_coap_blockwise_ack_msg_ptr->options_list_ptr) {
                        req_block_number =  src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 >> 4;
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 = COAP_OPTION_BLOCK_NONE;
                        // Do not clear block2 as it might have been set in the original request to request
                        // specific size blocks
                    } else {
                        if (!sn_coap_parser_alloc_options(handle, src_coap_blockwise_ack_msg_ptr)) {
                            tr_error("sn_coap_handle_blockwise_message - (send block1) failed to allocate ack message!");
                            return 0;
                        }

                        // pass through to send a next request
                        req_block_number = block_number;
                    }

                    // Make sure that block number is the one we requested. If it's the old one just ignore it and wait for next response.
                    if (req_block_number == block_number) {

                        /* Build next block message */
                        block_number++;
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 = (block_number << 4) | block_temp;

                        original_payload_len = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len;
                        original_payload_ptr = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_ptr;

                        if ((block_size * (block_number + 1)) > stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len) {
                            src_coap_blockwise_ack_msg_ptr->payload_len = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len - (block_size * (block_number));
                            src_coap_blockwise_ack_msg_ptr->payload_ptr = src_coap_blockwise_ack_msg_ptr->payload_ptr + (block_size * block_number);
                        }

                        /* Not last block */
                        else {
                            /* set more - bit */
                            src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 |= 0x08;
                            src_coap_blockwise_ack_msg_ptr->payload_len = block_size;
                            src_coap_blockwise_ack_msg_ptr->payload_ptr = src_coap_blockwise_ack_msg_ptr->payload_ptr + (block_size * block_number);
                        }

                        /* Build and send block message */
                        dst_packed_data_needed_mem = sn_coap_builder_calc_needed_packet_data_size_2(src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

                        dst_ack_packet_data_ptr = handle->sn_coap_protocol_malloc(dst_packed_data_needed_mem);
                        if (!dst_ack_packet_data_ptr) {
                            tr_error("sn_coap_handle_blockwise_message - (send block1) failed to allocate ack message!");
                            handle->sn_coap_protocol_free(src_coap_blockwise_ack_msg_ptr->options_list_ptr);
                            handle->sn_coap_protocol_free(original_payload_ptr);
                            handle->sn_coap_protocol_free(src_coap_blockwise_ack_msg_ptr);
                            stored_blockwise_msg_temp_ptr->coap_msg_ptr = NULL;
                            return NULL;
                        }
                        src_coap_blockwise_ack_msg_ptr->msg_id = get_new_message_id();

                        sn_coap_builder_2(dst_ack_packet_data_ptr, src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

                        handle->sn_coap_tx_callback(dst_ack_packet_data_ptr, dst_packed_data_needed_mem, src_addr_ptr, param);

#if ENABLE_RESENDINGS
                        uint32_t resend_time = sn_coap_calculate_new_resend_time(handle->system_time, handle->sn_coap_resending_intervall, 0);

                        if (src_coap_blockwise_ack_msg_ptr->msg_type == COAP_MSG_TYPE_CONFIRMABLE) {
                            sn_coap_protocol_linked_list_send_msg_store(handle, src_addr_ptr,
                                                                        dst_packed_data_needed_mem,
                                                                        dst_ack_packet_data_ptr,
                                                                        resend_time, param);
                        }
#endif

                        handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                        dst_ack_packet_data_ptr = 0;

                        stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len = original_payload_len;
                        stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_ptr = original_payload_ptr;

                        // Remove original message from the list when last block has been sent.
                        if (!((src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1) & 0x08)) {
                            sn_coap_protocol_remove_sent_blockwise_message(handle, stored_blockwise_msg_temp_ptr->coap_msg_ptr->msg_id);
                        }
                    } else {
                        tr_warn("sn_coap_handle_blockwise_message - blocks not in order, requested: %"PRIu32" received: %"PRIu32"  --> ignore", req_block_number, block_number);
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 = (req_block_number << 4) | block_temp;
                        *keep_in_resend_queue = true;
                    }
                }
            } else {
                if (stored_blockwise_msg_temp_ptr) {
                    // Last block received but some blocks are not yet sent. Ignore it and wait for next response.
                    tr_warn("sn_coap_handle_blockwise_message - last block received but some blocks are missing --> ignore");
                    received_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_ACK;
                    *keep_in_resend_queue = true;
                } else {
                    // XXX what was this trying to free?
                    received_coap_msg_ptr->coap_status = COAP_STATUS_OK;
                }
            }
        }

        // Blocked request receiving
        else {
            if (received_coap_msg_ptr->payload_len > handle->sn_coap_block_data_size) {
                received_coap_msg_ptr->payload_len = handle->sn_coap_block_data_size;
            }

            // Check that incoming block number is in order.
            const uint32_t block_number = received_coap_msg_ptr->options_list_ptr->block1 >> 4;
            bool blocks_in_order = true;

            if (block_number > 0 &&
                    !sn_coap_protocol_linked_list_blockwise_payload_search_compare_block_number(handle,
                                                                                                src_addr_ptr,
                                                                                                received_coap_msg_ptr->token_ptr,
                                                                                                received_coap_msg_ptr->token_len,
                                                                                                block_number - 1)) {
                blocks_in_order = false;
            }

            /* If not last block (more value is set) */
            /* Block option length can be 1-3 bytes. First 4-20 bits are for block number. Last 4 bits are ALWAYS more bit + block size. */
            if (received_coap_msg_ptr->options_list_ptr->block1 & 0x08) {

                src_coap_blockwise_ack_msg_ptr = sn_coap_parser_alloc_message_with_options(handle);
                if (src_coap_blockwise_ack_msg_ptr == NULL) {
                    tr_error("sn_coap_handle_blockwise_message - (recv block1) failed to allocate ack message!");
                    return NULL;
                }

                if (!blocks_in_order) {
                    tr_error("sn_coap_handle_blockwise_message - (recv block1) COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_INCOMPLETE!");
                    src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_INCOMPLETE;
                } else if (received_coap_msg_ptr->msg_code == COAP_MSG_CODE_REQUEST_GET) {
                    src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_CONTENT;
                } else if (received_coap_msg_ptr->msg_code == COAP_MSG_CODE_REQUEST_POST) {
                    src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_CONTINUE;
                } else if (received_coap_msg_ptr->msg_code == COAP_MSG_CODE_REQUEST_PUT) {
                    src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_CONTINUE;
                } else if (received_coap_msg_ptr->msg_code == COAP_MSG_CODE_REQUEST_DELETE) {
                    src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_DELETED;
                }

                // Response with COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_TOO_LARGE if the payload size is more than we can handle
                if (received_coap_msg_ptr->options_list_ptr->size1 > SN_COAP_MAX_INCOMING_BLOCK_MESSAGE_SIZE) {
                    // Include maximum size that stack can handle into response
                    tr_info("sn_coap_handle_blockwise_message - (recv block1) entity too large");
                    src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_TOO_LARGE;
                } else {
                    src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 = received_coap_msg_ptr->options_list_ptr->block1;
                    src_coap_blockwise_ack_msg_ptr->msg_type = COAP_MSG_TYPE_ACKNOWLEDGEMENT;

                    /* Check block size */
                    block_temp = (src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 & 0x07);
                    uint_fast16_t block_size = 16u << block_temp;

                    if (block_size >  handle->sn_coap_block_data_size) {
                        // Include maximum size that stack can handle into response
                        tr_info("sn_coap_handle_blockwise_message - (recv block1) entity too large");
                        src_coap_blockwise_ack_msg_ptr->msg_code = COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_TOO_LARGE;
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->size1 = handle->sn_coap_block_data_size;
                    }

                    if (block_temp > sn_coap_convert_block_size(handle->sn_coap_block_data_size)) {
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 &= 0xFFFFFFF8;
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 |= sn_coap_convert_block_size(handle->sn_coap_block_data_size);
                    }
                }

                src_coap_blockwise_ack_msg_ptr->msg_id = received_coap_msg_ptr->msg_id;

                // Copy token to response
                src_coap_blockwise_ack_msg_ptr->token_ptr = sn_coap_protocol_malloc_copy(handle, received_coap_msg_ptr->token_ptr, received_coap_msg_ptr->token_len);
                if (src_coap_blockwise_ack_msg_ptr->token_ptr) {
                    src_coap_blockwise_ack_msg_ptr->token_len = received_coap_msg_ptr->token_len;
                }

                dst_packed_data_needed_mem = sn_coap_builder_calc_needed_packet_data_size_2(src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

                dst_ack_packet_data_ptr = handle->sn_coap_protocol_malloc(dst_packed_data_needed_mem);
                if (!dst_ack_packet_data_ptr) {
                    tr_error("sn_coap_handle_blockwise_message - (recv block1) message allocation failed!");
                    handle->sn_coap_protocol_free(src_coap_blockwise_ack_msg_ptr->options_list_ptr);
                    handle->sn_coap_protocol_free(src_coap_blockwise_ack_msg_ptr);
                    return NULL;
                }

                sn_coap_builder_2(dst_ack_packet_data_ptr, src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
                // copy coap data buffer to duplicate list for resending purposes
                if (!sn_coap_protocol_update_duplicate_package_data(handle,
                                                                    src_addr_ptr,
                                                                    src_coap_blockwise_ack_msg_ptr,
                                                                    dst_packed_data_needed_mem,
                                                                    dst_ack_packet_data_ptr)) {
                    sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                    handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                    return NULL;
                }
#endif
                // Store only in success case
                if (src_coap_blockwise_ack_msg_ptr->msg_code != COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_INCOMPLETE &&
                        src_coap_blockwise_ack_msg_ptr->msg_code != COAP_MSG_CODE_RESPONSE_REQUEST_ENTITY_TOO_LARGE) {
                    uint16_t block_size = 1u << ((received_coap_msg_ptr->options_list_ptr->block1 & 0x07) + 4);
                    sn_coap_protocol_linked_list_blockwise_payload_store(handle,
                                                                         src_addr_ptr,
                                                                         received_coap_msg_ptr->payload_len,
                                                                         received_coap_msg_ptr->payload_ptr,
                                                                         received_coap_msg_ptr->token_ptr,
                                                                         received_coap_msg_ptr->token_len,
                                                                         block_number,
                                                                         block_size,
                                                                         received_coap_msg_ptr->options_list_ptr->size1);
                }

                handle->sn_coap_tx_callback(dst_ack_packet_data_ptr, dst_packed_data_needed_mem, src_addr_ptr, param);

                sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                dst_ack_packet_data_ptr = 0;

                received_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_MSG_RECEIVING;

            } else {

                /* * * This is the last block when whole Blockwise payload from received * * */
                /* * * blockwise messages is gathered and returned to User               * * */
                uint16_t block_size = 1u << ((received_coap_msg_ptr->options_list_ptr->block1 & 0x07) + 4);
                sn_coap_protocol_linked_list_blockwise_payload_store(handle,
                                                                     src_addr_ptr,
                                                                     received_coap_msg_ptr->payload_len,
                                                                     received_coap_msg_ptr->payload_ptr,
                                                                     received_coap_msg_ptr->token_ptr,
                                                                     received_coap_msg_ptr->token_len,
                                                                     block_number,
                                                                     block_size,
                                                                     received_coap_msg_ptr->options_list_ptr->size1);

                if (!sn_coap_handle_last_blockwise(handle, src_addr_ptr, received_coap_msg_ptr)) {

                    return NULL;
                }
            }
        }
    }


    /* Block2 Option in a response (e.g., a 2.05 response for GET) */
    /* Message ID must be same than in received message */
    else {
        //This is response to request we made
        if (received_coap_msg_ptr->msg_code > COAP_MSG_CODE_REQUEST_DELETE) {
#if SN_COAP_BLOCKWISE_INTERNAL_BLOCK_2_HANDLING_ENABLED
            if (handle->sn_coap_internal_block2_resp_handling) {
                uint32_t block_number = 0;
                /* Store blockwise payload to Linked list */
                //todo: add block number to stored values - just to make sure all packets are in order
                uint16_t block_size = 1u << ((received_coap_msg_ptr->options_list_ptr->block1 & 0x07) + 4);
                sn_coap_protocol_linked_list_blockwise_payload_store(handle,
                                                                     src_addr_ptr,
                                                                     received_coap_msg_ptr->payload_len,
                                                                     received_coap_msg_ptr->payload_ptr,
                                                                     received_coap_msg_ptr->token_ptr,
                                                                     received_coap_msg_ptr->token_len,
                                                                     received_coap_msg_ptr->options_list_ptr->block2 >> 4,
                                                                     block_size,
                                                                     received_coap_msg_ptr->options_list_ptr->size1);
                /* If not last block (more value is set) */
                if (received_coap_msg_ptr->options_list_ptr->block2 & 0x08) {
                    coap_blockwise_msg_s *previous_blockwise_msg_ptr;
                    //build and send ack
                    received_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_MSG_RECEIVING;

                    previous_blockwise_msg_ptr = search_sent_blockwise_message(handle, received_coap_msg_ptr->msg_id);

                    if (!previous_blockwise_msg_ptr || !previous_blockwise_msg_ptr->coap_msg_ptr) {
                        tr_error("sn_coap_handle_blockwise_message - (send block2) previous message null!");
                        return 0;
                    }

                    /* * * Then build CoAP Acknowledgement message * * */
                    src_coap_blockwise_ack_msg_ptr = sn_coap_parser_alloc_message_with_options(handle);
                    if (src_coap_blockwise_ack_msg_ptr == NULL) {
                        tr_error("sn_coap_handle_blockwise_message - (send block2) failed to allocate message!");
                        return NULL;
                    }

                    src_coap_blockwise_ack_msg_ptr->msg_id = get_new_message_id();

                    /* Update block option */
                    block_temp = received_coap_msg_ptr->options_list_ptr->block2 & 0x07;

                    block_number = received_coap_msg_ptr->options_list_ptr->block2 >> 4;
                    block_number ++;

                    src_coap_blockwise_ack_msg_ptr->options_list_ptr->block2 = (block_number << 4) | block_temp;


                    /* Set BLOCK2 (subsequent) GET msg code and copy uri path from previous msg*/
                    src_coap_blockwise_ack_msg_ptr->msg_code = previous_blockwise_msg_ptr->coap_msg_ptr->msg_code;
                    if (previous_blockwise_msg_ptr->coap_msg_ptr->uri_path_ptr) {
                        src_coap_blockwise_ack_msg_ptr->uri_path_ptr = sn_coap_protocol_malloc_copy(handle, previous_blockwise_msg_ptr->coap_msg_ptr->uri_path_ptr, previous_blockwise_msg_ptr->coap_msg_ptr->uri_path_len);

                        if (!src_coap_blockwise_ack_msg_ptr->uri_path_ptr) {
                            sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                            tr_error("sn_coap_handle_blockwise_message - failed to allocate for uri path ptr!");
                            return NULL;
                        }
                        src_coap_blockwise_ack_msg_ptr->uri_path_len = previous_blockwise_msg_ptr->coap_msg_ptr->uri_path_len;
                    }
                    if (previous_blockwise_msg_ptr->coap_msg_ptr->token_ptr) {

                        src_coap_blockwise_ack_msg_ptr->token_ptr = sn_coap_protocol_malloc_copy(handle, previous_blockwise_msg_ptr->coap_msg_ptr->token_ptr, previous_blockwise_msg_ptr->coap_msg_ptr->token_len);
                        if (!src_coap_blockwise_ack_msg_ptr->token_ptr) {
                            sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                            tr_error("sn_coap_handle_blockwise_message - failed to allocate for token ptr!");
                            return NULL;
                        }
                        src_coap_blockwise_ack_msg_ptr->token_len = received_coap_msg_ptr->token_len;
                    }

                    sn_coap_protocol_linked_list_blockwise_msg_remove(handle, previous_blockwise_msg_ptr);

                    previous_blockwise_msg_ptr = 0;

                    /* Then get needed memory count for Packet data */
                    dst_packed_data_needed_mem = sn_coap_builder_calc_needed_packet_data_size_2(src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

                    /* Then allocate memory for Packet data */
                    dst_ack_packet_data_ptr = sn_coap_protocol_calloc(handle, dst_packed_data_needed_mem);

                    if (dst_ack_packet_data_ptr == NULL) {
                        tr_error("sn_coap_handle_blockwise_message - (send block2) failed to allocate packet!");
                        sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                        return NULL;
                    }

                    /* * * Then build Acknowledgement message to Packed data * * */
                    if ((sn_coap_builder_2(dst_ack_packet_data_ptr, src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size)) < 0) {
                        tr_error("sn_coap_handle_blockwise_message - (send block2) builder failed!");
                        handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                        sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                        return NULL;
                    }

                    /* * * Save to linked list * * */
                    coap_blockwise_msg_s *stored_blockwise_msg_ptr;

                    stored_blockwise_msg_ptr = sn_coap_protocol_calloc(handle, sizeof(coap_blockwise_msg_s));
                    if (!stored_blockwise_msg_ptr) {
                        tr_error("sn_coap_handle_blockwise_message - (send block2) failed to allocate blockwise message!");
                        handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                        sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                        return 0;
                    }

                    stored_blockwise_msg_ptr->timestamp = handle->system_time;

                    stored_blockwise_msg_ptr->coap_msg_ptr = src_coap_blockwise_ack_msg_ptr;
                    stored_blockwise_msg_ptr->param = param;
                    stored_blockwise_msg_ptr->msg_id = stored_blockwise_msg_ptr->coap_msg_ptr->msg_id;
                    ns_list_add_to_end(&handle->linked_list_blockwise_sent_msgs, stored_blockwise_msg_ptr);

                    /* * * Then release memory of CoAP Acknowledgement message * * */
                    handle->sn_coap_tx_callback(dst_ack_packet_data_ptr,
                                                dst_packed_data_needed_mem, src_addr_ptr, param);

#if ENABLE_RESENDINGS
                    uint32_t resend_time = sn_coap_calculate_new_resend_time(handle->system_time, handle->sn_coap_resending_intervall, 0);
                    sn_coap_protocol_linked_list_send_msg_store(handle, src_addr_ptr,
                                                                dst_packed_data_needed_mem,
                                                                dst_ack_packet_data_ptr,
                                                                resend_time, param);
#endif
                    handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                    dst_ack_packet_data_ptr = 0;
                }

                //Last block received
                else {
                    /* * * This is the last block when whole Blockwise payload from received * * */
                    /* * * blockwise messages is gathered and returned to User               * * */

                    if (!sn_coap_handle_last_blockwise(handle, src_addr_ptr, received_coap_msg_ptr)) {

                        return NULL;
                    }

                    //todo: remove previous msg from list
                }
            }
#else
            // If the internal handling is disabled, this code should not be ran unless
            // the client messed up in its own handler.
            tr_error("sn_coap_handle_blockwise_message - (send block2) - callback not set");
            return NULL;
#endif
        }

        //Now we send data to request
        else {
            //Get message by using block number
            //NOTE: Getting the first from list might not be correct one
            coap_blockwise_msg_s *stored_blockwise_msg_temp_ptr = sn_coap_stored_blockwise_msg_get(handle, received_coap_msg_ptr);
            if (stored_blockwise_msg_temp_ptr) {

                block_temp = received_coap_msg_ptr->options_list_ptr->block2 & 0x07;

                /* Resolve block parameters */
                const uint_fast16_t block_size = 16u << block_temp;
                const uint32_t block_number = received_coap_msg_ptr->options_list_ptr->block2 >> 4;


                /* Build response message */
                src_coap_blockwise_ack_msg_ptr = stored_blockwise_msg_temp_ptr->coap_msg_ptr;

                if (src_coap_blockwise_ack_msg_ptr->options_list_ptr) {
                    src_coap_blockwise_ack_msg_ptr->options_list_ptr->block1 = COAP_OPTION_BLOCK_NONE;
                    // Do not clear block2 as it might have been set in the original request to request
                    // specific size blocks
                } else {
                    if (sn_coap_parser_alloc_options(handle, src_coap_blockwise_ack_msg_ptr) == NULL) {
                        tr_error("sn_coap_handle_blockwise_message - (recv block2) failed to allocate options!");
                        return 0;
                    }
                }

                src_coap_blockwise_ack_msg_ptr->msg_id = received_coap_msg_ptr->msg_id;

                src_coap_blockwise_ack_msg_ptr->options_list_ptr->block2 = received_coap_msg_ptr->options_list_ptr->block2;

                /* * Payload part * */

                /* Check if last block */

                original_payload_len = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len;
                original_payload_ptr = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_ptr;

                if ((block_size * (block_number + 1)) >= stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len) {
                    src_coap_blockwise_ack_msg_ptr->payload_len = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len - (block_size * block_number);
                    src_coap_blockwise_ack_msg_ptr->payload_ptr = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_ptr + (block_size * block_number);
                }
                /* Not last block */
                else {
                    /* set more - bit */
                    src_coap_blockwise_ack_msg_ptr->options_list_ptr->block2 |= 0x08;
                    src_coap_blockwise_ack_msg_ptr->payload_len = block_size;
                    src_coap_blockwise_ack_msg_ptr->payload_ptr = stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_ptr + (block_size * block_number);
                }

                /* Update token to match one which is in GET request.
                 * This is needed only in case of notification message.
                */
                if (src_coap_blockwise_ack_msg_ptr->options_list_ptr &&
                        src_coap_blockwise_ack_msg_ptr->options_list_ptr->observe != COAP_OBSERVE_NONE) {
                    if (src_coap_blockwise_ack_msg_ptr->token_ptr) {
                        handle->sn_coap_protocol_free(src_coap_blockwise_ack_msg_ptr->token_ptr);
                        if (received_coap_msg_ptr->token_len) {
                            src_coap_blockwise_ack_msg_ptr->token_ptr = sn_coap_protocol_malloc_copy(handle, received_coap_msg_ptr->token_ptr, received_coap_msg_ptr->token_len);
                            if (src_coap_blockwise_ack_msg_ptr->token_ptr) {
                                src_coap_blockwise_ack_msg_ptr->token_len = received_coap_msg_ptr->token_len;
                            }
                        } else {
                            src_coap_blockwise_ack_msg_ptr->token_ptr = NULL;
                            src_coap_blockwise_ack_msg_ptr->token_len = 0;
                        }
                    }
                }

                /* Build and send block message */
                dst_packed_data_needed_mem = sn_coap_builder_calc_needed_packet_data_size_2(src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

                dst_ack_packet_data_ptr = handle->sn_coap_protocol_malloc(dst_packed_data_needed_mem);
                if (!dst_ack_packet_data_ptr) {
                    tr_error("sn_coap_handle_blockwise_message - (recv block2) failed to allocate packet!");
                    handle->sn_coap_protocol_free(original_payload_ptr);
                    sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                    stored_blockwise_msg_temp_ptr->coap_msg_ptr = NULL;
                    return NULL;
                }

                sn_coap_builder_2(dst_ack_packet_data_ptr, src_coap_blockwise_ack_msg_ptr, handle->sn_coap_block_data_size);

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
                // copy coap data buffer to duplicate list for resending purposes
                if (!sn_coap_protocol_update_duplicate_package_data_all(handle,
                                                                        src_addr_ptr,
                                                                        src_coap_blockwise_ack_msg_ptr,
                                                                        dst_packed_data_needed_mem,
                                                                        dst_ack_packet_data_ptr)) {
                    sn_coap_parser_release_allocated_coap_msg_mem(handle, src_coap_blockwise_ack_msg_ptr);
                    handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                    return NULL;
                }
#endif

                handle->sn_coap_tx_callback(dst_ack_packet_data_ptr, dst_packed_data_needed_mem, src_addr_ptr, param);

                handle->sn_coap_protocol_free(dst_ack_packet_data_ptr);
                dst_ack_packet_data_ptr = 0;

                stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len = original_payload_len;
                stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_ptr = original_payload_ptr;

                if ((block_size * (block_number + 1)) >= stored_blockwise_msg_temp_ptr->coap_msg_ptr->payload_len) {

                    if (handle->sn_coap_rx_callback) {
                        stored_blockwise_msg_temp_ptr->coap_msg_ptr->coap_status = COAP_STATUS_BUILDER_BLOCK_SENDING_DONE;
                        stored_blockwise_msg_temp_ptr->coap_msg_ptr->msg_id = stored_blockwise_msg_temp_ptr->msg_id;
                        handle->sn_coap_rx_callback(stored_blockwise_msg_temp_ptr->coap_msg_ptr, NULL, stored_blockwise_msg_temp_ptr->param);
                    }

                    sn_coap_protocol_linked_list_blockwise_msg_remove(handle, stored_blockwise_msg_temp_ptr);
                }

                received_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_ACK;
            }
        }
    }
    return received_coap_msg_ptr;
}

static bool sn_coap_handle_last_blockwise(struct coap_s *handle, const sn_nsdl_addr_s *src_addr_ptr, sn_coap_hdr_s *received_coap_msg_ptr)
{
    uint16_t payload_len            = 0;
    uint32_t whole_payload_len      = sn_coap_protocol_linked_list_blockwise_payloads_get_len(handle, src_addr_ptr, received_coap_msg_ptr->token_ptr, received_coap_msg_ptr->token_len);
    uint8_t *payload_ptr            = sn_coap_protocol_linked_list_blockwise_payload_search(handle, src_addr_ptr, &payload_len, received_coap_msg_ptr->token_ptr, received_coap_msg_ptr->token_len);

    tr_debug("sn_coap_handle_last_blockwise - whole len %" PRIu32, whole_payload_len);
    if (!whole_payload_len) {
        return false;
    }

#if SN_COAP_REDUCE_BLOCKWISE_HEAP_FOOTPRINT
    received_coap_msg_ptr->payload_ptr = payload_ptr;
    received_coap_msg_ptr->payload_len = whole_payload_len;
#else
    received_coap_msg_ptr->payload_ptr = sn_coap_protocol_malloc_copy(handle, payload_ptr, whole_payload_len);
    if (received_coap_msg_ptr->payload_ptr == NULL) {
        tr_error("sn_coap_handle_last_blockwise - failed to allocate whole package!");
        return false;
    }
    received_coap_msg_ptr->payload_len = whole_payload_len;
#endif
    received_coap_msg_ptr->coap_status = COAP_STATUS_PARSER_BLOCKWISE_MSG_RECEIVED;

    return true;
}

int8_t sn_coap_convert_block_size(uint16_t block_size)
{
    for (int n = 0; n <= 6; n++) {
        if (block_size == (16 << n)) {
            return n;
        }
    }
    return -1;
}

static sn_coap_hdr_s *sn_coap_protocol_copy_header(struct coap_s *restrict handle, const sn_coap_hdr_s *restrict source_header_ptr)
{
    sn_coap_hdr_s *restrict destination_header_ptr;

    destination_header_ptr = sn_coap_parser_alloc_message(handle);
    if (!destination_header_ptr) {
        tr_error("sn_coap_protocol_copy_header - failed to allocate message!");
        return 0;
    }

    destination_header_ptr->coap_status = source_header_ptr->coap_status;
    destination_header_ptr->msg_type = source_header_ptr->msg_type;
    destination_header_ptr->msg_code = source_header_ptr->msg_code;
    destination_header_ptr->msg_id = source_header_ptr->msg_id;

    if (source_header_ptr->uri_path_ptr) {
        destination_header_ptr->uri_path_len = source_header_ptr->uri_path_len;
        destination_header_ptr->uri_path_ptr = sn_coap_protocol_malloc_copy(handle, source_header_ptr->uri_path_ptr, source_header_ptr->uri_path_len);
        if (!destination_header_ptr->uri_path_ptr) {
            tr_error("sn_coap_protocol_copy_header - failed to allocate uri path!");
            sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
            return 0;
        }
    }

    if (source_header_ptr->token_ptr) {
        destination_header_ptr->token_len = source_header_ptr->token_len;
        destination_header_ptr->token_ptr = sn_coap_protocol_malloc_copy(handle, source_header_ptr->token_ptr, source_header_ptr->token_len);
        if (!destination_header_ptr->token_ptr) {
            sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
            tr_error("sn_coap_protocol_copy_header - failed to allocate token!");
            return 0;
        }
    }

    destination_header_ptr->content_format = source_header_ptr->content_format;

    /* Options list */
    if (source_header_ptr->options_list_ptr) {
        if (sn_coap_parser_alloc_options(handle, destination_header_ptr) == NULL) {
            sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
            tr_error("sn_coap_protocol_copy_header - failed to allocate options!");
            return 0;
        }

        const sn_coap_options_list_s *restrict source_options_list_ptr = source_header_ptr->options_list_ptr;
        sn_coap_options_list_s *restrict destination_options_list_ptr = destination_header_ptr->options_list_ptr;

        destination_options_list_ptr->max_age = source_options_list_ptr->max_age;

        if (source_options_list_ptr->proxy_uri_ptr) {
            destination_options_list_ptr->proxy_uri_len = source_options_list_ptr->proxy_uri_len;
            destination_options_list_ptr->proxy_uri_ptr = sn_coap_protocol_malloc_copy(handle, source_options_list_ptr->proxy_uri_ptr, source_options_list_ptr->proxy_uri_len);
            if (!destination_options_list_ptr->proxy_uri_ptr) {
                sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
                tr_error("sn_coap_protocol_copy_header - failed to allocate proxy uri!");
                return 0;
            }
        }

        if (source_options_list_ptr->etag_ptr) {
            destination_options_list_ptr->etag_len = source_options_list_ptr->etag_len;
            destination_options_list_ptr->etag_ptr = sn_coap_protocol_malloc_copy(handle, source_options_list_ptr->etag_ptr, source_options_list_ptr->etag_len);
            if (!destination_options_list_ptr->etag_ptr) {
                sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
                tr_error("sn_coap_protocol_copy_header - failed to allocate etag!");
                return 0;
            }
        }

        if (source_options_list_ptr->uri_host_ptr) {
            destination_options_list_ptr->uri_host_len = source_options_list_ptr->uri_host_len;
            destination_options_list_ptr->uri_host_ptr = sn_coap_protocol_malloc_copy(handle, source_options_list_ptr->uri_host_ptr, source_options_list_ptr->uri_host_len);
            if (!destination_options_list_ptr->uri_host_ptr) {
                sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
                tr_error("sn_coap_protocol_copy_header - failed to allocate uri host!");
                return 0;
            }
        }

        if (source_options_list_ptr->location_path_ptr) {
            destination_options_list_ptr->location_path_len = source_options_list_ptr->location_path_len;
            destination_options_list_ptr->location_path_ptr = sn_coap_protocol_malloc_copy(handle, source_options_list_ptr->location_path_ptr, source_options_list_ptr->location_path_len);
            if (!destination_options_list_ptr->location_path_ptr) {
                tr_error("sn_coap_protocol_copy_header - failed to allocate location path!");
                sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
                return 0;
            }
        }

        destination_options_list_ptr->uri_port = source_options_list_ptr->uri_port;

        if (source_options_list_ptr->location_query_ptr) {
            destination_options_list_ptr->location_query_len = source_options_list_ptr->location_query_len;
            destination_options_list_ptr->location_query_ptr = sn_coap_protocol_malloc_copy(handle, source_options_list_ptr->location_query_ptr, source_options_list_ptr->location_query_len);
            if (!destination_options_list_ptr->location_query_ptr) {
                sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
                tr_error("sn_coap_protocol_copy_header - failed to allocate location query!");
                return 0;
            }
        }

        destination_options_list_ptr->observe = source_options_list_ptr->observe;
        destination_options_list_ptr->accept = source_options_list_ptr->accept;

        if (source_options_list_ptr->uri_query_ptr) {
            destination_options_list_ptr->uri_query_len = source_options_list_ptr->uri_query_len;
            destination_options_list_ptr->uri_query_ptr = sn_coap_protocol_malloc_copy(handle, source_options_list_ptr->uri_query_ptr, source_options_list_ptr->uri_query_len);
            if (!destination_options_list_ptr->uri_query_ptr) {
                sn_coap_parser_release_allocated_coap_msg_mem(handle, destination_header_ptr);
                tr_error("sn_coap_protocol_copy_header - failed to allocate uri query!");
                return 0;
            }
        }

        destination_options_list_ptr->block1 = source_options_list_ptr->block1;
        destination_options_list_ptr->block2 = source_options_list_ptr->block2;
    }

    return destination_header_ptr;
}
#endif

#if SN_COAP_DUPLICATION_MAX_MSGS_COUNT
static bool sn_coap_protocol_update_duplicate_package_data(const struct coap_s *handle,
                                                           const sn_nsdl_addr_s *dst_addr_ptr,
                                                           const sn_coap_hdr_s *coap_msg_ptr,
                                                           const int_fast16_t data_size,
                                                           const uint8_t *dst_packet_data_ptr)
{
    if (coap_msg_ptr->msg_type == COAP_MSG_TYPE_ACKNOWLEDGEMENT &&
            handle->sn_coap_duplication_buffer_size != 0) {
        return sn_coap_protocol_update_duplicate_package_data_all(handle, dst_addr_ptr, coap_msg_ptr, data_size, dst_packet_data_ptr);
    }
    return true;
}

static bool sn_coap_protocol_update_duplicate_package_data_all(const struct coap_s *handle,
                                                               const sn_nsdl_addr_s *dst_addr_ptr,
                                                               const sn_coap_hdr_s *coap_msg_ptr,
                                                               const int_fast16_t data_size,
                                                               const uint8_t *dst_packet_data_ptr)
{
    coap_duplication_info_s *info = sn_coap_protocol_linked_list_duplication_info_search(handle,
                                                                                         dst_addr_ptr,
                                                                                         coap_msg_ptr->msg_id);

    /* Update package data to duplication info struct if it's not there yet */
    if (info && info->packet_ptr == NULL) {
        info->packet_ptr = handle->sn_coap_protocol_malloc(data_size);
        if (info->packet_ptr) {
            tr_debug("sn_coap_protocol_update_duplication_package_data - added to duplicate list!");
            memcpy(info->packet_ptr, dst_packet_data_ptr, data_size);
            info->packet_len = data_size;
        } else {
            tr_error("sn_coap_protocol_update_duplication_package_data - failed to allocate duplication info!");
            return false;
        }
    }
    return true;
}
#endif

void *sn_coap_protocol_malloc_copy(struct coap_s *handle, const void *source, uint_fast16_t length)
{
    void *dest = handle->sn_coap_protocol_malloc(length);

    if ((dest) && (source)) {
        memcpy(dest, source, length);
    }
    return dest;
}

/*
 * This should logically be part and accessed via of the coap_s just as malloc() & free()
 * are, but that would require the client to fill one up, as a wrapper filled from this
 * class would need access to the handle itself.
 */
void *sn_coap_protocol_calloc(struct coap_s *handle, uint_fast16_t length)
{
    void *result = handle->sn_coap_protocol_malloc(length);

    if (result) {
        memset(result, 0, length);
    }
    return result;
}

static bool compare_port(const sn_nsdl_addr_s *left, const sn_nsdl_addr_s *right)
{
    bool match = false;
    if (left->port == right->port) {
        match = true;
    }

    return match;
}

static uint16_t get_new_message_id(void)
{
    if (message_id == 0) {
        /* Randomize global message ID */
        randLIB_seed_random();
        message_id = randLIB_get_16bit();
    }
    message_id++;
    if (message_id == 0) {
        message_id = 1;
    }
    return message_id;
}
