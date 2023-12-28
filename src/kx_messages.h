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
#ifndef __CLUSTER_MESSAGE_H__
#define __CLUSTER_MESSAGE_H__

#include "kx_config.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define PROTOCOL_ID_LENGTH          5
#define MESSAGE_HELLO_TYPE          0x01
#define MESSAGE_WELCOME_TYPE        0x02
#define MESSAGE_MEMBER_LIST_TYPE    0x03
#define MESSAGE_ACK_TYPE            0x04
#define MESSAGE_DATA_TYPE           0x05
#define MESSAGE_STATUS_TYPE         0x06

struct message_header {
    char protocol_id[PROTOCOL_ID_LENGTH];
    uint8_t message_type;
    uint16_t reserved;
    uint32_t sequence_num;
};

struct message_hello {
    message_header_t header;
    cluster_member_t *this_member;
};

struct message_welcome {
    message_header_t header;
    uint32_t hello_sequence_num;
    cluster_member_t *this_member;
};

struct message_member_list {
    message_header_t header;
    uint16_t members_n;
    cluster_member_t *members;
};

struct message_ack {
    message_header_t header;
    uint32_t ack_sequence_num;
};

struct message_data {
    message_header_t header;
    vector_record_t data_version;
    uint16_t data_size;
    uint8_t *data;
};

struct message_status {
    message_header_t header;
    vector_clock_t data_version;
};

void message_header_init(message_header_t *header, uint8_t message_type, uint32_t sequence_number);
int message_type_decode(const uint8_t *buffer, size_t buffer_size);
int message_hello_decode(const uint8_t *buffer, size_t buffer_size, message_hello_t *result);
int message_welcome_decode(const uint8_t *buffer, size_t buffer_size, message_welcome_t *result);
int message_data_decode(const uint8_t *buffer, size_t buffer_size, message_data_t *result);
int message_member_list_decode(const uint8_t *buffer, size_t buffer_size, message_member_list_t *result);
int message_ack_decode(const uint8_t *buffer, size_t buffer_size, message_ack_t *result);
int message_status_decode(const uint8_t *buffer, size_t buffer_size, message_status_t *result);
void message_hello_destroy(const message_hello_t *msg);
void message_welcome_destroy(const message_welcome_t *msg);
void message_member_list_destroy(const message_member_list_t *msg);
int message_hello_encode(const message_hello_t *msg, uint8_t *buffer, size_t buffer_size);
int message_welcome_encode(const message_welcome_t *msg, uint8_t *buffer, size_t buffer_size);
int message_data_encode(const message_data_t *msg, uint8_t *buffer, size_t buffer_size);
int message_member_list_encode(const message_member_list_t *msg, uint8_t *buffer, size_t buffer_size);
int message_ack_encode(const message_ack_t *msg, uint8_t *buffer, size_t buffer_size);
int message_status_encode(const message_status_t *msg, uint8_t *buffer, size_t buffer_size);

#ifdef  __cplusplus
}
#endif

#endif