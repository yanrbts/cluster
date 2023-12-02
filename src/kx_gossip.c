/*
 * Copyright 2023-2023 yanruibinghxu
 * Copyright 2016-2017 Iaroslav Zeigerman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <kx_config.h>
#include <kx_gossip.h>

#define RETURN_IF_NOT_CONNECTED(state)  if ((state) != STATE_CONNECTED) return CLUSTER_ERR_BAD_STATE;
#define INPUT_BUFFER_SIZE               MESSAGE_MAX_SIZE
#define OUTPUT_BUFFER_SIZE              MAX_OUTPUT_MESSAGES * MESSAGE_MAX_SIZE

typedef struct message_envelope_in {
    const cluster_sockaddr_storage *sender;
    cluster_socklen_t sender_len;
    const uint8_t *buffer;
    size_t buffer_size;
} message_envelope_in_t;

typedef struct message_envelope_out {
    cluster_sockaddr_storage recipient;
    cluster_socklen_t recipient_len;
    const uint8_t *buffer;
    size_t buffer_size;
    uint32_t sequence_num;
    uint64_t attempt_ts;
    uint16_t attempt_num;
    uint16_t max_attempts;
    struct message_envelope_out *prev;
    struct message_envelope_out *next;
} message_envelope_out_t;

typedef struct message_queue {
    message_envelope_out_t *head;
    message_envelope_out_t *tail;
} message_queue_t;

typedef struct data_log_record {
    vector_record_t version;
    uint16_t data_size;
    uint8_t data[MESSAGE_MAX_SIZE];
} data_log_record_t;

typedef struct data_log {
    data_log_record_t messages[DATA_LOG_SIZE];
    uint32_t size;
    uint32_t current_idx;
} data_log_t;

struct cluster_gossip {
    cluster_socket_fd socket;
    uint8_t input_buffer[INPUT_BUFFER_SIZE];
    uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
    size_t output_buffer_offset;
    message_queue_t outbound_messages;
    uint32_t sequence_num;
    uint32_t data_counter;
    vector_clock_t data_version;
    cluster_gossip_state_t state;
    cluster_member_t self_address;
    cluster_member_set_t members;
    data_log_t data_log;
    uint64_t last_gossip_ts;
    data_receiver_t data_receiver;
    void *data_receiver_context;
} cluster_gossip_t;

static int gossip_data_log_create_message(const data_log_record_t *record, message_data_t *msg) {
}

