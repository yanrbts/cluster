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

#define RETURN_IF_INVALID_PAYLOAD(t, r) if (!message_is_payload_valid(buffer, buffer_size, (t))) return r;

const char PROTOCOL_ID[PROTOCOL_ID_LENGTH] = { 'p', 't', 'c', 's', '\0' };

int message_type_decode(const uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(message_header_t)) {
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;
    }
    return *(buffer + PROTOCOL_ID_LENGTH);
}

static int message_is_payload_valid(const uint8_t *buffer, size_t buffer_size, uint8_t type) {
    return message_type_decode(buffer, buffer_size) == type &&
            memcmp(buffer, PROTOCOL_ID, PROTOCOL_ID_LENGTH) == 0;
}

void message_header_init(message_header_t *header, uint8_t message_type, uint32_t sequence_number) {
    memcpy(header->protocol_id, PROTOCOL_ID, PROTOCOL_ID_LENGTH);
    header->message_type = message_type;
    header->reserved = 0;
    header->sequence_num = sequence_number;
}

static int message_header_encode(const message_header_t *msg, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(struct message_header))
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;

    uint8_t *cursor = buffer + PROTOCOL_ID_LENGTH;
    *cursor = msg->message_type;
    cursor += sizeof(uint8_t);

    uint16_encode(msg->reserved, cursor);
    cursor += sizeof(uint16_t);

    uint32_encode(msg->sequence_num, cursor);
    return sizeof(struct message_header);
}

static int message_header_decode(const uint8_t *buffer, size_t buffer_size, message_header_t *result) {
    if (buffer_size < sizeof(struct message_header))
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;
    
    memcpy(result->protocol_id, buffer, PROTOCOL_ID_LENGTH);
    const uint8_t *cursor = buffer + PROTOCOL_ID_LENGTH;

    result->message_type = *cursor;
    cursor += sizeof(uint8_t);

    result->reserved = uint16_decode(cursor);
    cursor += sizeof(uint16_t);

    result->sequence_num = uint32_decode(cursor);
    return sizeof(struct message_header);
}

int message_hello_decode(const uint8_t *buffer, size_t buffer_size, message_hello_t *result) {
    RETURN_IF_INVALID_PAYLOAD(MESSAGE_HELLO_TYPE, CLUSTER_ERR_INVALID_MESSAGE);

    size_t min_size = sizeof(message_header_t) 
                     + sizeof(cluster_member_t)
                     - sizeof(cluster_sockaddr_storage *);
    message_header_decode(buffer, buffer_size, &result->header);
    result->this_member = (cluster_member_t *)malloc(sizeof(cluster_member_t));
    if (result->this_member == NULL)
        return CLUSTER_ERR_ALLOCATION_FAILED;
    
    int member_bytes = cluster_member_decode(buffer + sizeof(message_header_t),
                                             buffer_size - sizeof(message_header_t),
                                             result->this_member);
    if (member_bytes < 0) return member_bytes;
    return sizeof(message_header_t) + member_bytes;
}

int message_hello_encode(const message_hello_t *msg, uint8_t *buffer, size_t buffer_size) {
    size_t expected_size = sizeof(message_header_t) 
                          + sizeof(cluster_member_t)
                          - sizeof(cluster_sockaddr_storage *)
                          + msg->this_member->address_len;
    if (buffer_size < expected_size)
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;
    
    int encode_result = message_header_encode(&msg->header, buffer, buffer_size);
    if (encode_result < 0) return encode_result;

    uint8_t *cursor = buffer + encode_result;
    const uint8_t *buffer_end = buffer + buffer_size;
    cursor += cluster_member_encode(msg->this_member, cursor, buffer_end - cursor);

    return cursor - buffer;
}

void message_hello_destroy(const message_hello_t *msg) {
    free(msg->this_member);
}

int message_welcome_decode(const uint8_t *buffer, size_t buffer_size, message_welcome_t *result) {
    RETURN_IF_INVALID_PAYLOAD(MESSAGE_WELCOME_TYPE, CLUSTER_ERR_INVALID_MESSAGE);

    size_t min_size = sizeof(message_header_t)
                     + sizeof(uint32_t)
                     + sizeof(cluster_member_t)
                     - sizeof(cluster_sockaddr_storage *);
    if (buffer_size < min_size)
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;
    
    int decode_result = message_header_decode(buffer, buffer_size, &result->header);

    const uint8_t *cursor = buffer + decode_result;
    const uint8_t *buffer_end = buffer + buffer_size;

    result->hello_sequence_num = uint32_decode(cursor);
    cursor += sizeof(uint32_t);

    result->this_member = (cluster_member_t *)malloc(sizeof(cluster_member_t));
    if (result->this_member == NULL)
        return CLUSTER_ERR_ALLOCATION_FAILED;
    
    decode_result = cluster_member_decode(cursor, buffer_end - cursor, result->this_member);
    if (decode_result < 0) return decode_result;
    cursor += decode_result;

    return cursor - buffer;
}

int message_welcome_encode(const message_welcome_t *msg, uint8_t *buffer, size_t buffer_size) {
    size_t expected_size = sizeof(message_header_t) 
                          + sizeof(uint32_t) 
                          + sizeof(cluster_member_t) 
                          - sizeof(cluster_sockaddr_storage *)
                          + msg->this_member->address_len;
    if (buffer_size < expected_size) 
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;
    int encode_result = message_header_encode(&msg->header, buffer, buffer_size);

    uint8_t *cursor = buffer + encode_result;
    uint32_encode(msg->hello_sequence_num, cursor);
    cursor += sizeof(uint32_t);

    const uint8_t *buffer_end = buffer + buffer_size;
    cursor += cluster_member_encode(msg->this_member, cursor, buffer_end - cursor);

    return cursor - buffer;
}

void message_welcome_destroy(const message_welcome_t *msg) {
    free(msg->this_member);
}

int message_data_decode(const uint8_t *buffer, size_t buffer_size, message_data_t *result) {
    RETURN_IF_INVALID_PAYLOAD(MESSAGE_DATA_TYPE, CLUSTER_ERR_INVALID_MESSAGE);

    const uint8_t *cursor = buffer;
    const uint8_t *buffer_end = buffer + buffer_size;
}
