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
#ifndef __CLUSTER_NETWORK_H__
#define __CLUSTER_NETWORK_H__

#include "kx_config.h"

#ifdef  __cplusplus
extern "C" {
#endif

cluster_socket_fd cluster_socket_datagram(const cluster_sockaddr_storage *addr, socklen_t addr_len);
cluster_socket_fd cluster_socket(int domain, int type);
int cluster_bind(cluster_socket_fd fd, const cluster_sockaddr_storage *addr, 
    cluster_socklen_t addr_len);
ssize_t cluster_recv_from(cluster_socket_fd fd, uint8_t *buffer, 
    size_t buffer_size, cluster_sockaddr_storage *addr, cluster_socklen_t *addr_len);
ssize_t cluster_send_to(cluster_socket_fd fd, const uint8_t *buffer, 
    size_t buffer_size, const cluster_sockaddr_storage *addr, cluster_socklen_t addr_len);
void cluster_close(cluster_socket_fd fd);
int cluster_get_sock_name(cluster_socket_fd fd, cluster_sockaddr_storage *addr, 
    cluster_socklen_t *addr_len);

#ifdef  __cplusplus
}
#endif

#endif