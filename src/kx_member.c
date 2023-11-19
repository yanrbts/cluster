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

static const uint32_t MEMBERS_INITIAL_CAPACITY = 32;
static const uint8_t MEMBERS_EXTENSION_FACTOR = 2;
static const double MEMBERS_LOAD_FACTOR = 0.75;

static int cluster_member_copy(cluster_member_t *dst, cluster_member_t *src) {
    dst->uid = src->uid;
    dst->version = src->version;
    dst->address_len = src->address_len;
    dst->address = (cluster_sockaddr_storage *) malloc(src->address_len);

    if (dst->address == NULL)
        return CLUSTER_ERR_ALLOCATION_FAILED;
    memcpy(dst->address, src->address, src->address_len);

    return CLUSTER_ERR_NONE;
}

static cluster_member_set_t *
cluster_member_set_extend(cluster_member_set_t *members, uint32_t required_size) {
    int i;
    uint32_t new_capacity = members->capacity;

    while (required_size >= new_capacity * MEMBERS_LOAD_FACTOR)
        new_capacity *= MEMBERS_EXTENSION_FACTOR;
    
    cluster_member_t **new_member_set = (cluster_member_t **)calloc(new_capacity, sizeof(cluster_member_t *));
    if (new_member_set == NULL) return NULL;

    for (i = 0; i < members->size; ++i) {
        if (members->set[i] != NULL) {
            new_member_set[i] = members->set[i];
        }
    }
    free(members->set);
    members->capacity = new_capacity;
    members->set = new_member_set;

    return members;
}

static void cluster_member_set_shift(cluster_member_set_t *members, uint32_t idx) {
    for (int i = idx; i < members->size - 1; ++i) {
        members->set[i] = members->set[i + 1];
    }
    --members->size;
}

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

void cluster_member_destroy(cluster_member_t *result) {
    free(result->address);
}

int cluster_member_decode(const uint8_t *buffer, 
                          size_t buffer_size, 
                          cluster_member_t *member) 
{
    if (buffer_size < (2 * sizeof(uint32_t) + sizeof(uint16_t)))
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;
    
    const uint8_t *cursor = buffer;
    member->version = uint16_decode(cursor);
    cursor += sizeof(uint16_t);
    member->uid = uint32_decode(cursor);
    cursor += sizeof(uint32_t);
    member->address_len = uint32_decode(cursor);
    cursor += sizeof(uint32_t);
    member->address = (cluster_sockaddr_storage*)cursor;
    cursor += member->address_len;

    return cursor - buffer;
}

int cluster_member_encode(const cluster_member_t *member, 
                          uint8_t *buffer, 
                          size_t buffer_size) 
{
    if (buffer_size < (2 * sizeof(uint32_t) + sizeof(uint16_t) + member->address_len))
        return CLUSTER_ERR_BUFFER_NOT_ENOUGH;

    uint8_t *cursor = buffer;
    uint16_encode(member->version, cursor);
    cursor += sizeof(uint16_t);
    uint32_encode(member->uid, cursor);
    cursor += sizeof(uint32_t);
    uint32_encode(member->address_len, cursor);
    cursor += sizeof(uint32_t);
    memcpy(cursor, member->address, member->address_len);
    cursor += member->address_len;

    return cursor - buffer;
}

int cluster_member_set_init(cluster_member_set_t *members) {
    uint32_t capacity = MEMBERS_INITIAL_CAPACITY;

    cluster_member_t **member_set = (cluster_member_t **)calloc(capacity, sizeof(cluster_member_t *));
    if (member_set == NULL) 
        return CLUSTER_ERR_ALLOCATION_FAILED;

    members->size = 0;
    members->capacity = capacity;
    members->set = member_set;

    return CLUSTER_ERR_NONE;
}

int cluster_member_set_put(cluster_member_set_t *members, 
                            cluster_member_t *new_members, 
                            size_t new_members_size) 
{
    cluster_member_t *current;
    uint32_t new_size = members->size + new_members_size;

    // increase the capacity of the set if the new size is >= 0.75 of the current capacity.
    if (new_size >= members->capacity * MEMBERS_LOAD_FACTOR) {
        if (cluster_member_set_extend(members, new_size) == NULL)
            return CLUSTER_ERR_ALLOCATION_FAILED;
    }

    for (current = new_members; current < (new_members + new_members_size); ++current) {
        cluster_bool_t exists = CLUSTER_FALSE;
        for (int i = 0; i < members->size; ++i) {
            if (cluster_member_equals(members->set[i], current)) {
                exists = CLUSTER_TRUE;
                break;
            }
        }
        if (!exists) {
            // new member
            cluster_member_t *new_member = (cluster_member_t *)malloc(sizeof(cluster_member_t));
            if (new_member == NULL)
                return CLUSTER_ERR_ALLOCATION_FAILED;
            
            cluster_member_copy(new_member, current);
            members->set[members->size] = new_member;
            ++members->size;
        }
    }
    return CLUSTER_ERR_NONE;
}

static void cluster_member_set_item_destroy(cluster_member_t *member) {
    if (member != NULL) {
        cluster_member_destroy(member);
        free(member);
    }
}

void cluster_member_set_destroy(cluster_member_set_t *members) {
    int i;
    for (i = 0; i < members->size; ++i) {
        cluster_member_set_item_destroy(members->set[i]);
    }
    free(members->set);
}

int cluster_member_set_remove(cluster_member_set_t *members, cluster_member_t *member) {
    int i;
    for (i = 0; i < members->size; ++i) {
        if (members->set[i] == member) {
            cluster_member_set_item_destroy(member);
            cluster_member_set_shift(members, i);
            return CLUSTER_TRUE;
        }
    }
    return CLUSTER_FALSE;
}

cluster_member_t *cluster_member_set_find_by_addr(cluster_member_set_t *members,
                                                  const cluster_sockaddr_storage *addr,
                                                  cluster_socklen_t addr_size) 
{
    for (int i = 0; i < members->size; ++i) {
        if (memcmp(members->set[i]->address, addr, addr_size) == 0)
            return members->set[i];
    }
    return NULL;
}

int cluster_member_set_remove_by_addr(cluster_member_set_t *members,
                                      const cluster_sockaddr_storage *addr,
                                      cluster_socklen_t addr_size) 
{
    int i;
    for (i = 0; i < members->size; ++i) {
        if (memcmp(members->set[i]->address, addr, addr_size) == 0) {
            cluster_member_set_item_destroy(members->set[i]);
            cluster_member_set_shift(members, i);
            return CLUSTER_TRUE;
        }
    }
    return CLUSTER_FALSE;
}

size_t cluster_member_set_random_members(cluster_member_set_t *members,
                                         cluster_member_t **reservoir, 
                                         size_t reservoir_size) 
{
    size_t reservoir_idx = 0;
    size_t member_idx = 0;
    /*Randomly choosing the specified number of elements using the
     * reservoir sampling algorithm. */
    if (members->size == 0) return 0;

    size_t actual_reservoir_size = (members->size > reservoir_size) 
                                    ? reservoir_size 
                                    : members->size;
    
    /* Fill in the reservoir with first set elements. */
    while (reservoir_idx < actual_reservoir_size) {
        reservoir[reservoir_idx] = members->set[member_idx];
        ++member_idx;
        ++reservoir_idx;
    }

    /* Randomly replace reservoir's elements with items from the member's set. */
    if (actual_reservoir_size < members->size) {
        for (; member_idx < members->size; ++member_idx) {
            size_t random_idx = pt_random() % (member_idx + 1);
            if (random_idx < actual_reservoir_size) {
                reservoir[random_idx] = members->set[member_idx];
            }
        }
    }

    return actual_reservoir_size;
}