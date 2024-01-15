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
#include "kx_gfs.h"
#include "kx_log.h"
#include "kx_linenoise.h"
#include "kx_ls.h"

#define KX_PROMPT      "gfs>"
char                   *kx_prompt = NULL;

struct cmd {
    char *name;
    int (*execute)(struct context *ctx);
};

clusternode gcsnode;
struct context *gctx;
const char DATA_MESSAGE[] = "Hello World";

static struct cmd const cmds[] =
{
    {.name = "ls", .execute = do_ls}
};
#define NUM_CMDS sizeof(cmds) / sizeof(cmds[0])

static const struct cmd* get_cmd(char *name) {
    const struct cmd *cmd = NULL;
    for (int j = 0; j < NUM_CMDS; j++) {
        if (strcmp (name, cmds[j].name) == 0) {
            cmd = &(cmds[j]);
            break;
        }
    }

    return cmd;
}

static int
split_str(char *line, char **argv[]) {
    int argc = 0;
    char *line_start;

    line_start = line;
    while (*line != '\0') {
        if (*line == ' ') {
            argc++;
        }
        line++;
    }

    argc++;
    line = line_start;
    *argv = malloc(sizeof(char*) * argc);
    if (*argv == NULL) {
        goto out;
    }

    int cur_arg = 0;
    while (cur_arg < argc && *line != '\0') {
        (*argv)[cur_arg] = line;

        while (*line != ' ' && *line != '\n' && *line != '\0') {
            line++;
        }

        *line = '\0';
        line++;
        cur_arg++;
    }
out:
    return argc;
}

void data_receiver(void *context, cluster_gossip_t *gossip, const uint8_t *data, size_t data_size) {
    // This function is invoked every time when a new data arrives.
    // printf("Data arrived: %s\n", data);
}

static void  
do_completion(char const *prefix, linenoiseCompletions* lc) {

}

static void kx_loop() {
    char            *line;
    const char      *file = "./history";
    kx_prompt        = KX_PROMPT;
    const struct cmd *cmd;

    linenoiseHistoryLoad(file);
    linenoiseSetCompletionCallback(do_completion);

    while ((line = linenoise(kx_prompt)) != NULL) {
        if (line[0] != '\0' && line[0] != '/') {
            gctx->argc = split_str(line, &gctx->argv);
            if (gctx->argc == 0) {
                continue;
            }
            cmd = get_cmd(gctx->argv[0]);
            cmd->execute(gctx);
        } else {

        }
        free(line);
    }

    linenoiseHistorySave(file);
}

static void *thread_start(void *arg){
    int                 result;
    struct sockaddr_in  self_in, seed_node_in;
    char                message_with_ts[256];
    size_t              message_with_ts_size = 0;
    char                message[256];
    cluster_socket_fd   gossip_fd;
    int                 poll_interval = GOSSIP_TICK_INTERVAL;
    int                 recv_result = 0;
    int                 send_result = 0;
    int                 poll_result = 0;
    int                 send_data_interval = 5; // send data every 5 seconds
    time_t              previous_data_msg_ts = time(NULL);

    self_in.sin_family = AF_INET;
    self_in.sin_port = 0; // pick up a random port.
    inet_aton("127.0.0.1", &self_in.sin_addr);

    seed_node_in.sin_family = AF_INET;
    seed_node_in.sin_port = htons(6500);
    inet_aton("127.0.0.1", &seed_node_in.sin_addr);

    // Filling in the address of the current node.
    gcsnode.self.addr = (const cluster_sockaddr *)&self_in;
    gcsnode.self.addr_len = sizeof(struct sockaddr_in);

    gcsnode.seed_node.addr = (const cluster_sockaddr *)&seed_node_in;
    gcsnode.seed_node.addr_len = sizeof(struct sockaddr_in);

    // Create a new Pittacus descriptor instance.
    gcsnode.gossip = cluster_gossip_create(&gcsnode.self, &data_receiver, NULL);
    if (gcsnode.gossip == NULL) {
        log_error("Gossip initialization failed: %s\n", strerror(errno));
        return NULL;
    }

    result = cluster_gossip_join(gcsnode.gossip, &gcsnode.seed_node, 1);
    if (result < 0) {
        log_error("Gossip join failed: %d\n", result);
        cluster_gossip_destroy(gcsnode.gossip);
        goto err;
    }

    // Force Pittacus to send a Hello message.
    if (cluster_gossip_process_send(gcsnode.gossip) < 0) {
        log_error("Failed to send hello message to a cluster.\n");
        cluster_gossip_destroy(gcsnode.gossip);
        goto err;
    }

    // Retrieve the socket descriptor.
    gossip_fd = cluster_gossip_socket_fd(gcsnode.gossip);
    struct pollfd gossip_poll_fd = {
        .fd = gossip_fd,
        .events = POLLIN,
        .revents = 0
    };

    sprintf(message, "[%s]: %s", gcsnode.nodename, DATA_MESSAGE);
    while (1) {
        gossip_poll_fd.revents = 0;

        poll_result = poll(&gossip_poll_fd, 1, poll_interval);
        if (poll_result > 0) {
            if (gossip_poll_fd.revents & POLLERR) {
                log_error("Gossip socket failure: %s\n", strerror(errno));
                cluster_gossip_destroy(gcsnode.gossip);
                goto err;
            } else if (gossip_poll_fd.revents & POLLIN) {
                // Tell Pittacus to read a message from the socket.
                recv_result = cluster_gossip_process_receive(gcsnode.gossip);
                if (recv_result < 0) {
                    log_error("Gossip receive failed: %d\n", recv_result);
                    cluster_gossip_destroy(gcsnode.gossip);
                    goto err;
                }
            }
        } else if (poll_result < 0) {
            log_error("Poll failed: %s\n", strerror(errno));
            cluster_gossip_destroy(gcsnode.gossip);
            goto err;
        }
        // Try to trigger the Gossip tick event and recalculate
        // the poll interval.
        poll_interval = cluster_gossip_tick(gcsnode.gossip);
        if (poll_interval < 0) {
            log_error("Gossip tick failed: %d\n", poll_interval);
            goto err;
        }
        // Send some data periodically.
        time_t current_time = time(NULL);
        if (previous_data_msg_ts + send_data_interval <= current_time) {
            previous_data_msg_ts = current_time;
            message_with_ts_size = snprintf(message_with_ts,
                                            sizeof(message_with_ts),
                                            "%s (ts = %ld)", 
                                            message, current_time) + 1;
            // message_with_ts_size = sprintf(message_with_ts, "%s (ts = %ld)", message, current_time) + 1;
            cluster_gossip_send_data(gcsnode.gossip, (const uint8_t *) message_with_ts, message_with_ts_size);
        }
        // Tell Pittacus to write existing messages to the socket.
        send_result = cluster_gossip_process_send(gcsnode.gossip);
        if (send_result < 0) {
            log_error("Gossip send failed: %d, %s\n", send_result, strerror(errno));
            cluster_gossip_destroy(gcsnode.gossip);
            goto err;
        }
    }
    cluster_gossip_destroy(gcsnode.gossip);
err:
    return NULL;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        strncpy(gcsnode.nodename, argv[1], sizeof(gcsnode.nodename)-1);
        gcsnode.nodename[sizeof(gcsnode.nodename)-1] = '\0';
    }

    if (pthread_rwlock_init(&gcsnode.rwlock, NULL) != 0) {
        return -1;
    }
    
    pthread_create(&gcsnode.pthgossip, NULL, thread_start, NULL);

    gctx = malloc(sizeof (*gctx));
    if (gctx == NULL) {
        log_error("failed to initialize context");
    }
    gctx->argc = 0;
    gctx->argv = NULL;
    kx_loop();

    pthread_rwlock_destroy(&gcsnode.rwlock);
    return 0;
}