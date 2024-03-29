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
#ifndef __CLUSTER_MEMBER_H__
#define __CLUSTER_MEMBER_H__

#include "kx_config.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct cluster_member {
    char username[32];
    uint16_t version;
    uint32_t uid;
    cluster_socklen_t address_len;
    cluster_sockaddr_storage *address;
};

struct cluster_member_set {
    cluster_member_t **set;
    uint32_t size;
    uint32_t capacity;
};

int cluster_member_init(cluster_member_t *result, 
                        const cluster_sockaddr_storage *address, 
                        cluster_socklen_t address_len,
                        const char *uname,
                        uint16_t uname_len);
int cluster_member_equals(cluster_member_t *first, cluster_member_t *second);
void cluster_member_destroy(cluster_member_t *result);
int cluster_member_decode(const uint8_t *buffer, size_t buffer_size, cluster_member_t *member);
int cluster_member_encode(const cluster_member_t *member, uint8_t *buffer, size_t buffer_size);
int cluster_member_set_init(cluster_member_set_t *members);
int cluster_member_set_put(cluster_member_set_t *members, cluster_member_t *new_members, size_t new_members_size);
int cluster_member_set_remove(cluster_member_set_t *members, cluster_member_t *member);
cluster_member_t *cluster_member_set_find_by_addr(cluster_member_set_t *members,
                                                  const cluster_sockaddr_storage *addr,
                                                  cluster_socklen_t addr_size);
int cluster_member_set_remove_by_addr(cluster_member_set_t *members,
                                      const cluster_sockaddr_storage *addr,
                                      cluster_socklen_t addr_size);
size_t cluster_member_set_random_members(cluster_member_set_t *members,
                                         cluster_member_t **reservoir, size_t reservoir_size);
void cluster_member_set_destroy(cluster_member_set_t *members);

#ifdef  __cplusplus
}
#endif

#endif