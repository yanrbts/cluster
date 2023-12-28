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

cluster_socket_fd cluster_socket_datagram(const cluster_sockaddr_storage *addr, 
    socklen_t addr_len)
{
    int domain;
    int ret;
    cluster_socket_fd fd;

    domain = addr->ss_family;
    if ((fd = cluster_socket(domain, SOCK_DGRAM)) < 0)
        return fd;
    
    if ((ret = fcntl(fd, F_SETFL, O_NONBLOCK)) < 0) {
        cluster_close(fd);
        return ret;
    }

    if ((ret = cluster_bind(fd, addr, addr_len)) < 0) {
        cluster_close(fd);
        return ret;
    }
    return fd;
}

cluster_socket_fd cluster_socket(int domain, int type) {
    return socket(domain, type, 0);
}

int cluster_bind(cluster_socket_fd fd, 
                const cluster_sockaddr_storage *addr, 
                cluster_socklen_t addr_len)
{
    return bind(fd, (const struct sockaddr *)addr, addr_len);
}

ssize_t cluster_recv_from(cluster_socket_fd fd, 
                            uint8_t *buffer, 
                            size_t buffer_size, 
                            cluster_sockaddr_storage *addr, 
                            cluster_socklen_t *addr_len)
{
    return recvfrom(fd, buffer, buffer_size, MSG_WAITALL, (struct sockaddr *)addr, addr_len);
}

ssize_t cluster_send_to(cluster_socket_fd fd, 
                        const uint8_t *buffer, 
                        size_t buffer_size, 
                        const cluster_sockaddr_storage *addr, 
                        cluster_socklen_t addr_len)
{
    return sendto(fd, buffer, buffer_size, 0, (const struct sockaddr *)addr, addr_len);
}

void cluster_close(cluster_socket_fd fd) {
    close(fd);
}

int cluster_get_sock_name(cluster_socket_fd fd, 
                          cluster_sockaddr_storage *addr, 
                          cluster_socklen_t *addr_len)
{
    return getsockname(fd, (struct sockaddr *)addr, addr_len);
}