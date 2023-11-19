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
#ifndef __CLUSTER_ERRORS_H__
#define __CLUSTER_ERRORS_H__

#ifdef  __cplusplus
extern "C" {
#endif

enum cluster_error {
    CLUSTER_ERR_NONE                = 0,
    CLUSTER_ERR_INIT_FAILED         = -1,
    CLUSTER_ERR_ALLOCATION_FAILED   = -2,
    CLUSTER_ERR_BAD_STATE           = -3,
    CLUSTER_ERR_INVALID_MESSAGE     = -4,
    CLUSTER_ERR_BUFFER_NOT_ENOUGH   = -5,
    CLUSTER_ERR_NOT_FOUND           = -6,
    CLUSTER_ERR_WRITE_FAILED        = -7,
    CLUSTER_ERR_READ_FAILED         = -8
};

#ifdef  __cplusplus
}
#endif

#endif