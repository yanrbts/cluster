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
#include <stdbool.h>
#include <getopt.h>
#include <error.h>
#include <arpa/inet.h>
#include "kx_ls.h"

#define AUTHORS             "Written by Yan Ruibing."
#define PACKAGE_VERSION     "0.0.1"

struct state {
    bool ip;
    bool bn;
};

static struct state *state;
static struct option const long_options[] = {
    {"ip", no_argument, NULL, 'i'},
    {"version", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, no_argument, NULL, 0}
};

static void usage() {
    printf ("Usage: ls [OPTION]... \n"
                "list online nodes contents\n\n"
                "  -n             show user id .\n"
                "  -i, --ip       display online node address and port .\n"
                "      --help     display this help and exit\n"
                "      --version  output version information and exit\n\n"
                "Examples:\n"
                "  ls -n show user id\n"
                "      [*] 1705502321\n"
                "      [*] 1705502333\n"
                "  ls -i display online node address and port\n"
                "       127.0.0.1:6789\n");
}

/**
 * Parses command line flags into a global application state.
 */
static int
parse_options (int argc, char *argv[]) {
    int ret = -1;
    int opt = 0;
    int option_index = 0;

    if (argv[argc-1] == NULL || argv[argc-1][0] == '\0') {
        fprintf(stderr, "Invalid command line arguments\n");
        goto err;
    }

    optind = 0;
    while (true) {
        opt = getopt_long(argc, argv, "nihv", long_options, &option_index);
        
        if (opt == -1) break;

        switch (opt) {
            case 'n':
                state->bn = true;
                ret = 0;
                goto out;
            case 'i': 
                state->ip = true;
                ret = 0;
                goto out;
            case 'v':
                printf ("%s (%s) %s\n", argv[0], PACKAGE_VERSION, AUTHORS);
                ret = -2;
                goto out;
            case 'h':
                usage();
                ret = -2;
                goto out;
            default: goto err;
        }
    }

    if ((argc - option_index) < 2) {
        error(0, 0, "missing operand");
        goto err;
    }
err:
    error(0, 0, "Try ls --help for more information.");
out:
    return ret;
}

/**
 * Initializes the global application state.
 */
static struct state*
init_state()
{
    struct state *state = malloc(sizeof (*state));
    if (state == NULL)
        goto out;
    
    state->ip = false;
    state->bn = false;
out:
    return state;
}

static int ls() {
    cluster_member_set_t *mset;

    pthread_rwlock_rdlock(&gcsnode.rwlock);
    mset = cluster_gossip_member_list(gcsnode.gossip);
    for (int i = 0; i < mset->size; i++) {
        printf("[*] %u\n", mset->set[i]->uid);
    }
    pthread_rwlock_unlock(&gcsnode.rwlock);

    return 0;
}

static void ls_showaddr(cluster_sockaddr_storage *sa) {
    if (sa->ss_family == AF_INET) {
        struct sockaddr_in* sa4 = (struct sockaddr_in*)sa;
        char ip4[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sa4->sin_addr), ip4, INET_ADDRSTRLEN);
        printf("[*] %s:%d\n", ip4, ntohs(sa4->sin_port));
    } else if (sa->ss_family == AF_INET6) {
        struct sockaddr_in6* sa6 = (struct sockaddr_in6*)sa;
        char ip6[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(sa6->sin6_addr), ip6, INET6_ADDRSTRLEN);
        printf("[*] %s:%d\n", ip6, ntohs(sa6->sin6_port));
    } else {
        printf("[*] Unknown\n");
    }
}

static int ls_index() {
    cluster_member_set_t *mset;

    pthread_rwlock_rdlock(&gcsnode.rwlock);
    mset = cluster_gossip_member_list(gcsnode.gossip);
    for (int i = 0; i < mset->size; i++) {
        ls_showaddr(mset->set[i]->address);
    }
    pthread_rwlock_unlock(&gcsnode.rwlock);

    return 0;
}

int do_ls(struct context *ctx) {
    int ret = -1;
    int argc = ctx->argc;
    char **argv = ctx->argv;

    state = init_state();
    if (state == NULL) {
        error(0, errno, "failed to initialize state");
        goto out;
    }

    ret = parse_options(argc, argv);
    switch (ret) {
        case -2: ret = 0;
        case -1: goto out;
    }
    if (state->ip) {
        ret = ls_index();
    } else if (state->bn) {
        ret = ls();
    }
    
out:
    if (state) free(state);
    return ret;
}