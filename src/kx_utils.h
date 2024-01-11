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
#ifndef __CLUSTER_UTILS_H__
#define __CLUSTER_UTILS_H__

#ifdef  __cplusplus
extern "C" {
#endif

enum cluster_bool {
    CLUSTER_FALSE = 0,
    CLUSTER_TRUE = 1
};

/**
 * This function passes the function gettimeofday 
 * to the current time in milliseconds.
 * 
 * @return Returns the number of milliseconds
 */
uint64_t cluster_time();
uint32_t cluster_random();
uint16_t uint16_decode(const uint8_t *buffer);
void uint16_encode(uint16_t n, uint8_t *buffer);
uint32_t uint32_decode(const uint8_t *buffer);
void uint32_encode(uint32_t n, uint8_t *buffer);

#ifdef  __cplusplus
}
#endif

#endif