/*
 * Copyright 2023-2024 yanruibinghxu
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
#ifndef __KX_GFS__
#define __KX_GFS__

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <pthread.h>

#include "kx_config.h"
#include "kx_gossip.h"

struct kx_option {
    struct kx_option *next;
    char *key;
    char *opt_string;
    char *value;
};

struct options {
    struct kx_option *kx_options;
};

struct context {
    struct options *options;
    int argc;
    char **argv;
};

typedef struct clusternode {
    char nodename[32];          /* node name */
    cluster_addr_t self;        /* node self addr */
    cluster_addr_t seed_node;   /* seed node addr */
    cluster_gossip_t *gossip;   /* gossip handle */
    pthread_t pthgossip;
    pthread_rwlock_t rwlock;
} clusternode;

extern clusternode gcsnode;

#endif