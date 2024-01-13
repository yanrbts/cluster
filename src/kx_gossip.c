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
#include "kx_config.h"
#include "kx_gossip.h"

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

    for (int i = 0; i < log->size; ++i) {
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

        if (log->size < DATA_LOG_SIZE) ++log->size;
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
        oldest_envelope = oldest_envelope->next;
        gossip_envelope_remove(&self->outbound_messages, to_remove);
    }
    return chosen_buffer;
}

static uint32_t gossip_update_output_buffer_offset(cluster_gossip_t *self) {
    uint32_t offset = 0;
    if (self->outbound_messages.head != NULL) {
        offset = gossip_find_available_output_buffer(self) - self->output_buffer;
    }
    self->output_buffer_offset = offset;
    return offset;
}

static int gossip_enqueue_to_outbound(cluster_gossip_t *self,
                                      const uint8_t *buffer,
                                      size_t buffer_size,
                                      uint16_t max_attempts,
                                      const cluster_sockaddr_storage *receiver,
                                      cluster_socklen_t receiver_size) {
    uint32_t seq_num = ++self->sequence_num;
    message_envelope_out_t *new_envelope = gossip_envelope_create(seq_num,
                                                                  buffer, buffer_size,
                                                                  max_attempts,
                                                                  receiver, receiver_size);
    if (new_envelope == NULL) return CLUSTER_ERR_ALLOCATION_FAILED;
    gossip_envelope_enqueue(&self->outbound_messages, new_envelope);
    return CLUSTER_ERR_NONE;
}

typedef enum gossip_spreading_type {
    GOSSIP_DIRECT = 0,
    GOSSIP_RANDOM = 1,
    GOSSIP_BROADCAST = 2
} gossip_spreading_type_t;

static int gossip_encode_message(uint8_t msg_type, const void *msg, uint8_t *buffer, uint16_t *max_attempts) {
    *max_attempts = MESSAGE_RETRY_ATTEMPTS;
    int encode_result = 0;
    // Serialize the message.
    switch (msg_type) {
    case MESSAGE_HELLO_TYPE:
        encode_result = message_hello_encode((const message_hello_t*)msg, 
                                            buffer, MESSAGE_MAX_SIZE);
        break;
    case MESSAGE_WELCOME_TYPE:
        encode_result = message_welcome_encode((const message_welcome_t*)msg, 
                                                buffer, MESSAGE_MAX_SIZE);
        // Welcome message can't be acknowledged. It should be removed from the
        // outbound queue after the first attempt.
        *max_attempts = 1;
        break;
    case MESSAGE_MEMBER_LIST_TYPE:
        encode_result = message_member_list_encode((const message_member_list_t *)msg,
                                                    buffer, MESSAGE_MAX_SIZE);
        break;
    case MESSAGE_DATA_TYPE:
        encode_result = message_data_encode((const message_data_t *)msg,
                                            buffer, MESSAGE_MAX_SIZE);
        break;
    case MESSAGE_ACK_TYPE:
        encode_result = message_ack_encode((const message_ack_t *)msg,
                                            buffer, MESSAGE_MAX_SIZE);
        // ACK message can't be acknowledged. It should be removed from the
        // outbound queue after the first attempt.
        *max_attempts = 1;
        break;
    case MESSAGE_STATUS_TYPE:
        encode_result = message_status_encode((const message_status_t *)msg,
                                               buffer, MESSAGE_MAX_SIZE);
        break;
    default:
        return CLUSTER_ERR_INVALID_MESSAGE;
    }
    return encode_result;
}

static int gossip_enqueue_message(cluster_gossip_t *self,
                                  uint8_t msg_type,
                                  const void *msg,
                                  const cluster_sockaddr_storage *recipient,
                                  cluster_socklen_t recipient_len,
                                  gossip_spreading_type_t spreading_type) {
    uint32_t offset = gossip_update_output_buffer_offset(self);
    uint8_t *buffer = self->output_buffer + offset;
    uint16_t max_attempts = 0;
    int encode_result = gossip_encode_message(msg_type, msg, buffer, &max_attempts);
    if (encode_result < 0) return encode_result;

    int result = CLUSTER_ERR_NONE;
    // Distribute the message.
    switch (spreading_type) {
        case GOSSIP_DIRECT:
            // Send message to a single recipient.
            return gossip_enqueue_to_outbound(self, buffer, encode_result, max_attempts,
                                              recipient, recipient_len);
        case GOSSIP_RANDOM: {
            // Choose some number of random members to distribute the message.
            cluster_member_t *reservoir[MESSAGE_RUMOR_FACTOR];
            int receivers_num = cluster_member_set_random_members(&self->members,
                                                                  reservoir, MESSAGE_RUMOR_FACTOR);
            for (int i = 0; i < receivers_num; ++i) {
                // Create a new envelope for each recipient.
                // Note: all created envelopes share the same buffer.
                result = gossip_enqueue_to_outbound(self, buffer, encode_result, max_attempts,
                                                    reservoir[i]->address, reservoir[i]->address_len);
                if (result < 0) return result;
            }
            break;
        }
        case GOSSIP_BROADCAST: {
            // Distribute the message to all known members.
            for (int i = 0; i < self->members.size; ++i) {
                // Create a new envelope for each recipient.
                // Note: all created envelopes share the same buffer.
                cluster_member_t *member = self->members.set[i];
                result = gossip_enqueue_to_outbound(self, buffer, encode_result, max_attempts,
                                                    member->address, member->address_len);
                if (result < 0) return result;
            }
            break;
        }
    }
    return result;
}

static int gossip_enqueue_ack(cluster_gossip_t *self,
                              uint32_t sequence_num,
                              const cluster_sockaddr_storage *recipient,
                              cluster_socklen_t recipient_len) {
    message_ack_t ack_msg;
    message_header_init(&ack_msg.header, MESSAGE_ACK_TYPE, 0);
    ack_msg.ack_sequence_num = sequence_num;
    return gossip_enqueue_message(self, MESSAGE_ACK_TYPE, &ack_msg,
                                  recipient, recipient_len, GOSSIP_DIRECT);
}

static int gossip_enqueue_welcome(cluster_gossip_t *self,
                                  uint32_t hello_sequence_num,
                                  const cluster_sockaddr_storage *recipient,
                                  cluster_socklen_t recipient_len) {
    message_welcome_t welcome_msg;
    message_header_init(&welcome_msg.header, MESSAGE_WELCOME_TYPE, 0);
    welcome_msg.hello_sequence_num = hello_sequence_num;
    welcome_msg.this_member = &self->self_address;
    return gossip_enqueue_message(self, MESSAGE_WELCOME_TYPE, &welcome_msg,
                                  recipient, recipient_len, GOSSIP_DIRECT);
}

static int gossip_enqueue_hello(cluster_gossip_t *self,
                                const cluster_sockaddr_storage *recipient,
                                cluster_socklen_t recipient_len) {
    message_hello_t hello_msg;
    message_header_init(&hello_msg.header, MESSAGE_HELLO_TYPE, 0);
    hello_msg.this_member = &self->self_address;
    return gossip_enqueue_message(self, MESSAGE_HELLO_TYPE, &hello_msg,
                                  recipient, recipient_len, GOSSIP_DIRECT);
}

static int gossip_enqueue_data(cluster_gossip_t *self,
                               const uint8_t *data,
                               uint16_t data_size) {
    // Update the local data version.
    uint32_t clock_counter = ++self->data_counter;
    vector_record_t *record = (vector_record_t*)vector_clock_set(&self->data_version, &self->self_address,
                                              clock_counter);
    message_data_t data_msg;
    message_header_init(&data_msg.header, MESSAGE_DATA_TYPE, 0);
    vector_clock_record_copy(&data_msg.data_version, record);
    data_msg.data = (uint8_t *) data;
    data_msg.data_size = data_size;

    // Add the data to our internal log.
    gossip_data_log(&self->data_log, &data_msg);

    return gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &data_msg,
                                  NULL, 0, GOSSIP_RANDOM);
}

static int gossip_enqueue_status(cluster_gossip_t *self,
                                 const cluster_sockaddr_storage *recipient,
                                 cluster_socklen_t recipient_len) {
    message_status_t status_msg;
    message_header_init(&status_msg.header, MESSAGE_STATUS_TYPE, 0);
    vector_clock_copy(&status_msg.data_version, &self->data_version);

    gossip_spreading_type_t spreading_type = recipient == NULL ? GOSSIP_RANDOM : GOSSIP_DIRECT;
    return gossip_enqueue_message(self, MESSAGE_STATUS_TYPE, &status_msg,
                                  recipient, recipient_len, spreading_type);
}

#define MEMBER_LIST_SYNC_SIZE (MESSAGE_MAX_SIZE / CLUSTER_MEMBER_SIZE)

static int gossip_enqueue_member_list(cluster_gossip_t *self,
                                      const cluster_sockaddr_storage *recipient,
                                      cluster_socklen_t recipient_len) {
    message_member_list_t member_list_msg;
    message_header_init(&member_list_msg.header, MESSAGE_MEMBER_LIST_TYPE, 0);

    const cluster_member_set_t *members = &self->members;
    uint32_t members_num = (members->size > MEMBER_LIST_SYNC_SIZE) ? MEMBER_LIST_SYNC_SIZE : members->size;
    if (members_num == 0) return CLUSTER_ERR_NONE;

    // TODO: get rid of the redundant copying.
    cluster_member_t *members_to_send = (cluster_member_t *) malloc(members_num * sizeof(cluster_member_t));
    if (members_to_send == NULL) return CLUSTER_ERR_ALLOCATION_FAILED;

    int result = CLUSTER_ERR_NONE;
    int to_send_idx = 0;
    int member_idx = 0;
    while (member_idx < members->size) {
        // Send the list of all known members to a recipient.
        // The list can be pretty big, so we split it into multiple messages.
        while (to_send_idx < members_num && member_idx < members->size) {
            memcpy(&members_to_send[to_send_idx], members->set[member_idx], sizeof(cluster_member_t));
            ++to_send_idx;
            ++member_idx;
        }
        if (to_send_idx > 0) {
            member_list_msg.members_n = to_send_idx;
            member_list_msg.members = members_to_send;
            result = gossip_enqueue_message(self, MESSAGE_MEMBER_LIST_TYPE, &member_list_msg,
                                            recipient, recipient_len, GOSSIP_DIRECT);
            if (result < 0) {
                free(members_to_send);
                return result;
            }
        }
        to_send_idx = 0;
    }
    free(members_to_send);
    return result;
}

static int gossip_enqueue_data_log(cluster_gossip_t *self,
                                   vector_clock_t *recipient_version,
                                   const cluster_sockaddr_storage *recipient,
                                   cluster_socklen_t recipient_len) {
    int result = CLUSTER_ERR_NONE;
    for (int i = 0; i < self->data_log.size; ++i) {
        const data_log_record_t *record = &self->data_log.messages[i];
        if (vector_clock_compare_with_record(recipient_version, &record->version, CLUSTER_FALSE) == VC_BEFORE) {
            // The recipient data version is behind. Enqueue this data payload.
            message_data_t data_msg;
            result = gossip_data_log_create_message(record, &data_msg);
            if (result < 0) return result;

            result = gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &data_msg,
                                            recipient, recipient_len, GOSSIP_DIRECT);
            if (result < 0) return result;
        }
    }
    return result;
}

static int gossip_handle_hello(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_hello_t msg;
    int decode_result = message_hello_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        log_error("handle hello error : %d", decode_result);
        return decode_result;
    }

    // Send back a Welcome message.
    gossip_enqueue_welcome(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    // Send the list of known members to a newcomer node.
    if (self->members.size > 0) {
        gossip_enqueue_member_list(self, envelope_in->sender, envelope_in->sender_len);
    }

    // Notify other nodes about a newcomer.
    message_member_list_t member_list_msg;
    message_header_init(&member_list_msg.header, MESSAGE_MEMBER_LIST_TYPE, 0);
    member_list_msg.members = msg.this_member;
    member_list_msg.members_n = 1;
    gossip_enqueue_message(self, MESSAGE_MEMBER_LIST_TYPE, &member_list_msg, NULL, 0, GOSSIP_BROADCAST);

    // Update our local storage with a new member.
    cluster_member_set_put(&self->members, msg.this_member, 1);

    message_hello_destroy(&msg);
    return CLUSTER_ERR_NONE;
}

static int gossip_handle_welcome(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    message_welcome_t msg;
    int decode_result = message_welcome_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }
    self->state = STATE_CONNECTED;

    // Now when the seed node responded we can
    // safely add it to the list of known members.
    cluster_member_set_put(&self->members, msg.this_member, 1);

    // Remove the hello message from the outbound queue.
    message_envelope_out_t *hello_envelope =
            gossip_envelope_find_by_sequence_num(&self->outbound_messages,
                                                 msg.hello_sequence_num);
    if (hello_envelope != NULL) gossip_envelope_remove(&self->outbound_messages, hello_envelope);

    message_welcome_destroy(&msg);
    return CLUSTER_ERR_NONE;
}

static int gossip_handle_member_list(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_member_list_t msg;
    int decode_result = message_member_list_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    };

    // Update our local collection of members with arrived records.
    cluster_member_set_put(&self->members, msg.members, msg.members_n);

    // Send ACK message back to sender.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    message_member_list_destroy(&msg);
    return CLUSTER_ERR_NONE;
}

static int gossip_handle_data(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_data_t msg;
    int decode_result = message_data_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }

    // Send ACK message back to sender.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    // Verify whether we saw the arrived message before.
    vector_clock_comp_res_t res = vector_clock_compare_with_record(&self->data_version,
                                                                   &msg.data_version, CLUSTER_TRUE);

    if (res == VC_BEFORE) {
        // Add the data to our internal log.
        gossip_data_log(&self->data_log, &msg);

        if (self->data_receiver) {
            // Invoke the data receiver callback specified by the user.
            self->data_receiver(self->data_receiver_context, self, msg.data, msg.data_size);
        }
        // Enqueue the same message to send it to N random members later.
        return gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &msg, NULL, 0, GOSSIP_RANDOM);
    }
    return CLUSTER_ERR_NONE;
}

static int gossip_handle_ack(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_ack_t msg;
    int decode_result = message_ack_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }

    // Removing the processed message from the outbound queue.
    message_envelope_out_t *ack_envelope =
            gossip_envelope_find_by_sequence_num(&self->outbound_messages,
                                                 msg.ack_sequence_num);
    if (ack_envelope != NULL) gossip_envelope_remove(&self->outbound_messages, ack_envelope);
    return CLUSTER_ERR_NONE;
}

static int gossip_handle_status(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_status_t msg;
    int decode_result = message_status_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }

    // Acknowledge the arrived Status message.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    int result = CLUSTER_ERR_NONE;

    vector_clock_comp_res_t comp_res = vector_clock_compare(&self->data_version, &msg.data_version, CLUSTER_FALSE);
    switch (comp_res) {
        case VC_AFTER:
            // The remote node is missing some of the data messages.
            result = gossip_enqueue_data_log(self, &msg.data_version,
                                             envelope_in->sender, envelope_in->sender_len);
            break;
        case VC_BEFORE:
            // This node is behind. Send back the Status message to request the data update.
            result = gossip_enqueue_status(self, envelope_in->sender, envelope_in->sender_len);
            break;
        case VC_CONFLICT:
            // The conflict occurred. Both nodes should exchange the data with each other.
            // Send the data messages from the log.
            result = gossip_enqueue_data_log(self, &msg.data_version,
                                             envelope_in->sender, envelope_in->sender_len);
            if (result < 0) return result;
            // Request the data update.
            result = gossip_enqueue_status(self, envelope_in->sender, envelope_in->sender_len);
            break;
        default:
            break;
    }

    return result;
}

static int gossip_handle_new_message(cluster_gossip_t *self, const message_envelope_in_t *envelope_in) {
    int message_type = message_type_decode(envelope_in->buffer, envelope_in->buffer_size);
    int result = 0;
    switch(message_type) {
        case MESSAGE_HELLO_TYPE:
            result = gossip_handle_hello(self, envelope_in);
            break;
        case MESSAGE_WELCOME_TYPE:
            result = gossip_handle_welcome(self, envelope_in);
            break;
        case MESSAGE_MEMBER_LIST_TYPE:
            result = gossip_handle_member_list(self, envelope_in);
            break;
        case MESSAGE_DATA_TYPE:
            result = gossip_handle_data(self, envelope_in);
            break;
        case MESSAGE_ACK_TYPE:
            result = gossip_handle_ack(self, envelope_in);
            break;
        case MESSAGE_STATUS_TYPE:
            result = gossip_handle_status(self, envelope_in);
            break;
        default:
            return CLUSTER_ERR_INVALID_MESSAGE;
    }
    return result;
}

static int cluster_gossip_init(cluster_gossip_t *self,
                                const cluster_addr_t *self_addr,
                                data_receiver_t data_receiver, void *data_receiver_context) {
    self->socket = cluster_socket_datagram((const cluster_sockaddr_storage *) self_addr->addr, self_addr->addr_len);
    if (self->socket < 0) {
        return CLUSTER_ERR_INIT_FAILED;
    }

    cluster_sockaddr_storage updated_self_addr;
    cluster_socklen_t updated_self_addr_size = sizeof(cluster_sockaddr_storage);
    if (cluster_get_sock_name(self->socket, &updated_self_addr, &updated_self_addr_size) < 0) {
        cluster_close(self->socket);
        return CLUSTER_ERR_INIT_FAILED;
    }

    self->output_buffer_offset = 0;

    self->outbound_messages = (message_queue_t ) { .head = NULL, .tail = NULL };

    self->sequence_num = 0;
    self->data_counter = 0;
    vector_clock_init(&self->data_version);

    self->state = STATE_INITIALIZED;
    cluster_member_init(&self->self_address, &updated_self_addr, updated_self_addr_size);
    cluster_member_set_init(&self->members);

    self->data_log.current_idx = 0;
    self->data_log.size = 0;

    self->last_gossip_ts = 0;

    self->data_receiver = data_receiver;
    self->data_receiver_context = data_receiver_context;
    return CLUSTER_ERR_NONE;
}

cluster_gossip_t *cluster_gossip_create(const cluster_addr_t *self_addr,
                                          data_receiver_t data_receiver, void *data_receiver_context) {
    cluster_gossip_t *result = (cluster_gossip_t *) malloc(sizeof(cluster_gossip_t));
    if (result == NULL) return NULL;

    int int_res = cluster_gossip_init(result, self_addr, data_receiver, data_receiver_context);
    if (int_res < 0) {
        free(result);
        return NULL;
    }
    return result;
}

int cluster_gossip_destroy(cluster_gossip_t *self) {
    cluster_close(self->socket);

    gossip_envelope_clear(&self->outbound_messages);

    self->state = STATE_DESTROYED;
    cluster_member_destroy(&self->self_address);
    cluster_member_set_destroy(&self->members);

    free(self);
    return CLUSTER_ERR_NONE;
}

int cluster_gossip_join(cluster_gossip_t *self, const cluster_addr_t *seed_nodes, uint16_t seed_nodes_len) {
    if (self->state != STATE_INITIALIZED) return CLUSTER_ERR_BAD_STATE;
    if (seed_nodes == NULL || seed_nodes_len == 0) {
        // No seed nodes were provided.
        self->state = STATE_CONNECTED;
    } else {
        for (int i = 0; i < seed_nodes_len; ++i) {
            cluster_addr_t node = seed_nodes[i];
            int res = gossip_enqueue_hello(self, (const cluster_sockaddr_storage *) node.addr, node.addr_len);
            if (res < 0) return res;
        }
        self->state = STATE_JOINING;
    }
    return CLUSTER_ERR_NONE;
}

int cluster_gossip_process_receive(cluster_gossip_t *self) {
    if (self->state != STATE_JOINING && self->state != STATE_CONNECTED) return CLUSTER_ERR_BAD_STATE;

    cluster_sockaddr_storage addr;
    cluster_socklen_t addr_len = sizeof(cluster_sockaddr_storage);
    // Read a new message.
    int read_result = cluster_recv_from(self->socket, self->input_buffer, INPUT_BUFFER_SIZE, &addr, &addr_len);
    if (read_result <= 0) return CLUSTER_ERR_READ_FAILED;

    message_envelope_in_t envelope;
    envelope.buffer = self->input_buffer;
    envelope.buffer_size = read_result;
    envelope.sender = &addr;
    envelope.sender_len = addr_len;

    return gossip_handle_new_message(self, &envelope);
}

int cluster_gossip_process_send(cluster_gossip_t *self) {
    if (self->state != STATE_JOINING && self->state != STATE_CONNECTED) return CLUSTER_ERR_BAD_STATE;
    message_envelope_out_t *head = self->outbound_messages.head;
    int msg_sent = 0;
    while (head != NULL) {
        message_envelope_out_t *current = head;
        head = head->next;

        if (current->attempt_num >= current->max_attempts) {
            // The message exceeded the maximum number of attempts.

            if (current->max_attempts > 1) {
                // If the number of maximum attempts is more than 1, then
                // the message required acknowledgement but we've never received it.
                // Remove node from the list since it's unreachable.
                cluster_member_set_remove_by_addr(&self->members,
                                                  &current->recipient,
                                                  current->recipient_len);
                // Quite often the same recipient has several messages in a row.
                // Check whether the next message should be removed as well.
                message_envelope_out_t *next = current->next;
                message_envelope_out_t *to_remove = NULL;
                while (next != NULL && memcmp(&next->recipient, &current->recipient, next->recipient_len) == 0) {
                    to_remove = next;
                    next = next->next;
                    gossip_envelope_remove(&self->outbound_messages, to_remove);
                }
                head = next;
            }
            // Remove this message from the queue.
            gossip_envelope_remove(&self->outbound_messages, current);
            continue;
        }

        uint64_t current_ts = cluster_time();
        if (current->attempt_num != 0 && current->attempt_ts + MESSAGE_RETRY_INTERVAL > current_ts) {
            // It's not yet time to retry this message.
            continue;
        }

        // Update the sequence number in the buffer in order to correspond to
        // a sequence number stored in envelope. This approach violates
        // the protocol interface but prevents copying of the whole
        // buffer each time when a single message has multiple recipients.
        uint32_t seq_num_n = CLUSTER_HTONL(current->sequence_num);
        uint32_t offset = sizeof(message_header_t) - sizeof(uint32_t);
        uint8_t *seq_num_buf = (uint8_t *) current->buffer + offset;
        memcpy(seq_num_buf, &seq_num_n, sizeof(uint32_t));

        int write_result = cluster_send_to(self->socket, current->buffer, current->buffer_size,
                                      &current->recipient, current->recipient_len);

        if (write_result < 0) {
            printf("error : %s", strerror(errno));
            return CLUSTER_ERR_WRITE_FAILED;
        }
        current->attempt_ts = current_ts;
        ++current->attempt_num;
        ++msg_sent;
        if (current->max_attempts <= 1) {
            // The message must be sent only once. Remove it immediately.
            gossip_envelope_remove(&self->outbound_messages, current);
        }
    }
    return msg_sent;
}

int cluster_gossip_send_data(cluster_gossip_t *self, const uint8_t *data, uint32_t data_size) {
    RETURN_IF_NOT_CONNECTED(self->state);
    return gossip_enqueue_data(self, data, data_size);
}

int cluster_gossip_tick(cluster_gossip_t *self) {
    if (self->state != STATE_CONNECTED) return GOSSIP_TICK_INTERVAL;
    uint64_t next_gossip_ts = self->last_gossip_ts + GOSSIP_TICK_INTERVAL;
    uint64_t current_ts = cluster_time();
    if (next_gossip_ts > current_ts) {
        return next_gossip_ts - current_ts;
    }
    int enqueue_result = gossip_enqueue_status(self, NULL, 0);
    if (enqueue_result < 0) return enqueue_result;
    self->last_gossip_ts = current_ts;

    return GOSSIP_TICK_INTERVAL;
}

cluster_gossip_state_t cluster_gossip_state(cluster_gossip_t *self) {
    return self->state;
}

cluster_socket_fd cluster_gossip_socket_fd(cluster_gossip_t *self) {
    return self->socket;
}

void cluster_gossip_remove_timeout_member(cluster_gossip_t *self) {
}