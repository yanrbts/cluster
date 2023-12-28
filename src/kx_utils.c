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

uint64_t cluster_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

uint32_t cluster_random() {
    return random();
}

uint16_t uint16_decode(const uint8_t *buffer) {
    return CLUSTER_NTOHS(*(uint16_t *)buffer);
}

void uint16_encode(uint16_t n, uint8_t *buffer) {
    uint16_t network_n = CLUSTER_HTONS(n);
    memcpy(buffer, &network_n, sizeof(uint16_t));
}

uint32_t uint32_decode(const uint8_t *buffer) {
    return CLUSTER_NTOHL(*(uint32_t *)buffer);
}

void uint32_encode(uint32_t n, uint8_t *buffer) {
    uint32_t network_n = CLUSTER_HTONL(n);
    memcpy(buffer, &network_n, sizeof(uint32_t));
}