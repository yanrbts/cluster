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

int cluster_member_init(cluster_member_t *result, 
                        const cluster_sockaddr_storage *address, 
                        cluster_socklen_t address_len)
{
    result->uid = cluster_time();
    result->version = PROTOCOL_VERSION;
    result->address_len = address_len;
    result->address = (cluster_sockaddr_storage*)malloc(address_len);
    if (result->address == NULL)
        return CLUSTER_ERR_ALLOCATION_FAILED;
    memcpy(result->address, address, address_len);
    return CLUSTER_ERR_NONE;
}

int cluster_member_equals(cluster_member_t *first, cluster_member_t *second) {
    return first->uid == second->uid &&
            first->version == second->version &&
            first->address_len == second->address_len &&
            memcmp(first->address, second->address, first->address_len) == 0;
}