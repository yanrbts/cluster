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
};

static int gossip_data_log_create_message(const data_log_record_t *record, message_data_t *msg) {
    message_header_init(&msg->header, MESSAGE_DATA_TYPE, 0);
    vector_clock_record_copy(&msg->data_version, &record->version);
    msg->data = (uint8_t *)record->data;
    msg->data_size = record->data_size;
    return CLUSTER_ERR_NONE;
}

static int gossip_data_log(data_log_t *log, const message_data_t *msg) {
    data_log_record_t *record = NULL;

    for (int i = 0; i < log->size; i++) {
        // Save only the latest data message from each originator.
        if (log->messages[i].version.member_id == msg->data_version.member_id) {
            record = &log->messages[i];
            record->version.sequence_number = msg->data_version.sequence_number;
            break;
        }
    }

    if (record == NULL) {
        // The data message with the same originator was not found.
        uint32_t new_idx = log->current_idx;
        record = &log->messages[new_idx];
        vector_clock_record_copy(&record->version, &msg->data_version);

        if (log->size < DATA_LOG_SIZE) log->size++;
        if (++log->current_idx >= DATA_LOG_SIZE) log->current_idx = 0;
    }
    record->data_size = msg->data_size;
    memcpy(record->data, msg->data, msg->data_size);

    return CLUSTER_ERR_NONE;
}

static message_envelope_out_t *gossip_envelope_create(
        uint32_t sequence_number,
        const uint8_t *buffer, size_t buffer_size,
        uint16_t max_attempts,
        const cluster_sockaddr_storage *recipient, cluster_socklen_t recipient_len) {
    message_envelope_out_t *envelope = (message_envelope_out_t *)malloc(sizeof(message_envelope_out_t));
    if (envelope == NULL) return NULL;

    envelope->sequence_num = sequence_number;
    envelope->next = NULL;
    envelope->prev = NULL;
    envelope->attempt_num = 0;
    envelope->attempt_ts = 0;
    envelope->buffer = buffer;
    envelope->buffer_size = buffer_size;
    memcpy(&envelope->recipient, recipient, recipient_len);
    envelope->recipient_len = recipient_len;
    envelope->max_attempts = max_attempts;

    return envelope;
}

static void gossip_envelope_destroy(message_envelope_out_t *envelope) {
    free(envelope);
}

static void gossip_envelope_clear(message_queue_t *queue) {
    message_envelope_out_t *head = queue->head;
    while (head != NULL) {
        message_envelope_out_t *current = head;
        head = head->next;
        gossip_envelope_destroy(current);
    }
    queue->head = NULL;
    queue->tail = NULL;
}

static int gossip_envelope_enqueue(message_queue_t *queue, message_envelope_out_t *envelope) {
    envelope->next = NULL;
    if (queue->head == NULL || queue->tail == NULL) {
        queue->head = envelope;
        queue->tail = envelope;
    } else {
        envelope->prev = queue->tail;
        queue->tail->next = envelope;
        queue->tail = envelope;
    }
    return CLUSTER_ERR_NONE;
}

static int gossip_envelope_remove(message_queue_t *queue, message_envelope_out_t *envelope) {
    message_envelope_out_t *prev = envelope->prev;
    message_envelope_out_t *next = envelope->next;
    if (next != NULL) {
        next->prev = prev;
    } else {
        queue->tail = prev;
    }
    if (prev != NULL) {
        prev->next = next;
    } else {
        queue->head = next;
    }
    gossip_envelope_destroy(envelope);
    return CLUSTER_ERR_NONE;
}

static message_envelope_out_t *
gossip_envelope_find_by_sequence_num(message_queue_t *queue, uint32_t sequence_num) {
    message_envelope_out_t *head = queue->head;
    while (head != NULL) {
        if (head->sequence_num == sequence_num) return head;
        head = head->next;
    }
    return NULL;
}

static const uint8_t *gossip_find_available_output_buffer(cluster_gossip_t *self) {
    cluster_bool_t buffer_is_occupied[MAX_OUTPUT_MESSAGES];
    memset(buffer_is_occupied, 0, MAX_OUTPUT_MESSAGES * sizeof(cluster_bool_t));

    message_envelope_out_t *oldest_envelope = NULL;
    message_envelope_out_t *head = self->outbound_messages.head;
    while (head != NULL) {
        if (oldest_envelope == NULL || head->attempt_num > oldest_envelope->attempt_num) {
            oldest_envelope = head;
        }
        const uint8_t *buffer = head->buffer;
        uint32_t index = (buffer - self->output_buffer) / MESSAGE_MAX_SIZE;
        buffer_is_occupied[index] = CLUSTER_TRUE;

        head = head->next;
    }

    // Looking for a buffer that is not used by any message in the outbound queue.
    for (int i = 0; i < MAX_OUTPUT_MESSAGES; ++i) {
        if (!buffer_is_occupied[i])
            return self->output_buffer + (i * MESSAGE_MAX_SIZE);
    }

    // No available buffers were found. Removing the oldest message in a queue
    // to overwrite its buffer.
    const uint8_t *chosen_buffer = oldest_envelope->buffer;
    while (oldest_envelope != NULL && oldest_envelope->buffer == chosen_buffer) {
        // Remove all messages that share the same buffer's region.
        message_envelope_out_t *to_remove = oldest_envelope;
    }
}

