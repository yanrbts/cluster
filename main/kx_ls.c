/*
 * Copyright 2023-2023 yanruibinghxu
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
#include "kx_ls.h"

int do_ls(struct context *ctx) {
    cluster_member_set_t *mset;

    pthread_rwlock_rdlock(&gcsnode.rwlock);
    mset = cluster_gossip_member_list(gcsnode.gossip);
    for (int i = 0; i < mset->size; i++) {
        printf("[*] %u\n", mset->set[i]->uid);
    }
    pthread_rwlock_unlock(&gcsnode.rwlock);

    return 0;
}