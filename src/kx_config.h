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
#ifndef __CLUSTER_CONFIG_H__
#define __CLUSTER_CONFIG_H__

#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>

#include <kx_errors.h>
#include <kx_gossip.h>
#include <kx_member.h>
#include <kx_utils.h>
#include <kx_messages.h>
#include <kx_vectorclock.h>

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef PROTOCOL_VERSION
#define PROTOCOL_VERSION 0x01
#endif

/* The interval in milliseconds between retry attempts. */
#ifndef MESSAGE_RETRY_INTERVAL
#define MESSAGE_RETRY_INTERVAL 10000
#endif

/* The maximum number of attempts to deliver a message. */
#ifndef MESSAGE_RETRY_ATTEMPTS
#define MESSAGE_RETRY_ATTEMPTS 3
#endif

/* The number of members that are used for further gossip propagation. */
#ifndef MESSAGE_RUMOR_FACTOR
#define MESSAGE_RUMOR_FACTOR 3
#endif

/* The maximum supported size of the message 
 * including a protocol overhead. */
#ifndef MESSAGE_MAX_SIZE
#define MESSAGE_MAX_SIZE 512
#endif

/* The maximum number of unique messages that 
 * can be stored in the outbound message queue. */
#ifndef MAX_OUTPUT_MESSAGES
#define MAX_OUTPUT_MESSAGES 100
#endif

/* The time interval in milliseconds that determines 
 * how often the Gossip tick event should be triggered. */
#ifndef GOSSIP_TICK_INTERVAL
#define GOSSIP_TICK_INTERVAL 1000
#endif

#ifndef DATA_LOG_SIZE
#define DATA_LOG_SIZE 25
#endif

#define CLUSTER_NTOHS(i) ntohs((i))
#define CLUSTER_NTOHL(i) ntohl((i))
#define CLUSTER_HTONS(i) htons((i))
#define CLUSTER_HTONL(i) htonl((i))

typedef socklen_t                   cluster_socklen_t;
typedef struct sockaddr             cluster_sockaddr;
typedef struct sockaddr_in          cluster_sockaddr_in;
typedef struct sockaddr_in6         cluster_sockaddr_in6;
typedef struct sockaddr_storage     cluster_sockaddr_storage;

typedef struct cluster_member       cluster_member_t;
typedef struct cluster_member_set   cluster_member_set_t;
typedef enum cluster_error          cluster_error_t;
typedef enum cluster_bool           cluster_bool_t;
typedef int                         cluster_socket_fd;
typedef struct vector_record        vector_record_t;
typedef struct vector_clock         vector_clock_t;
typedef enum vector_clock_comp_res  vector_clock_comp_res_t;
typedef struct message_header       message_header_t;
typedef struct message_hello        message_hello_t;
typedef struct message_welcome      message_welcome_t;
typedef struct message_member_list  message_member_list_t;
typedef struct message_ack          message_ack_t;
typedef struct message_data         message_data_t;
typedef struct message_status       message_status_t;

#define CLUSTER_MEMBER_SIZE (sizeof(uint16_t) +                 \
                            sizeof(uint32_t) +                  \
                            sizeof(cluster_socklen_t) +         \
                            sizeof(cluster_sockaddr_storage))

#ifdef  __cplusplus
}
#endif

#endif