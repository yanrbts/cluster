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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include "kx_config.h"
#include "kx_gossip.h"
#include "kx_log.h"

const char DATA_MESSAGE[] = "Hello World";

void data_receiver(void *context, cluster_gossip_t *gossip, const uint8_t *data, size_t data_size) {
    // This function is invoked every time when a new data arrives.
    printf("Data arrived: %s\n", data);
}

int main(int argc, char **argv) {
    char *node_id = "0";
    if (argc > 1) {
        node_id = argv[1];
    }
    char message_with_ts[256];
    size_t message_with_ts_size = 0;
    char message[256];
    sprintf(message, "[%s]: %s", node_id, DATA_MESSAGE);

    struct sockaddr_in self_in;
    self_in.sin_family = AF_INET;
    self_in.sin_port = 0; // pick up a random port.
    inet_aton("127.0.0.1", &self_in.sin_addr);

    // Filling in the address of the current node.
    cluster_addr_t self_addr = {
        .addr = (const cluster_sockaddr *) &self_in,
        .addr_len = sizeof(struct sockaddr_in)
    };

    // Create a new Pittacus descriptor instance.
    cluster_gossip_t *gossip = cluster_gossip_create(&self_addr, &data_receiver, NULL);
    if (gossip == NULL) {
        log_error("Gossip initialization failed: %s\n", strerror(errno));
        return -1;
    }

    // Connect to the active seed node.
    struct sockaddr_in seed_node_in;
    seed_node_in.sin_family = AF_INET;
    seed_node_in.sin_port = htons(6500);
    inet_aton("127.0.0.1", &seed_node_in.sin_addr);

    cluster_addr_t seed_node_addr = {
        .addr = (const cluster_sockaddr *) &seed_node_in,
        .addr_len = sizeof(struct sockaddr_in)
    };

    int join_result = cluster_gossip_join(gossip, &seed_node_addr, 1);
    if (join_result < 0) {
        log_error("Gossip join failed: %d\n", join_result);
        cluster_gossip_destroy(gossip);
        return -1;
    }

    // Force Pittacus to send a Hello message.
    if (cluster_gossip_process_send(gossip) < 0) {
        log_error("Failed to send hello message to a cluster.\n");
        cluster_gossip_destroy(gossip);
        return -1;
    }

    // Retrieve the socket descriptor.
    cluster_socket_fd gossip_fd = cluster_gossip_socket_fd(gossip);
    struct pollfd gossip_poll_fd = {
        .fd = gossip_fd,
        .events = POLLIN,
        .revents = 0
    };

    int poll_interval = GOSSIP_TICK_INTERVAL;
    int recv_result = 0;
    int send_result = 0;
    int poll_result = 0;
    int send_data_interval = 5; // send data every 5 seconds
    time_t previous_data_msg_ts = time(NULL);
    while (1) {
        gossip_poll_fd.revents = 0;

        poll_result = poll(&gossip_poll_fd, 1, poll_interval);
        if (poll_result > 0) {
            if (gossip_poll_fd.revents & POLLERR) {
                log_error("Gossip socket failure: %s\n", strerror(errno));
                cluster_gossip_destroy(gossip);
                return -1;
            } else if (gossip_poll_fd.revents & POLLIN) {
                // Tell Pittacus to read a message from the socket.
                recv_result = cluster_gossip_process_receive(gossip);
                if (recv_result < 0) {
                    log_error("Gossip receive failed: %d\n", recv_result);
                    cluster_gossip_destroy(gossip);
                    return -1;
                }
            }
        } else if (poll_result < 0) {
            log_error("Poll failed: %s\n", strerror(errno));
            cluster_gossip_destroy(gossip);
            return -1;
        }
        // Try to trigger the Gossip tick event and recalculate
        // the poll interval.
        poll_interval = cluster_gossip_tick(gossip);
        if (poll_interval < 0) {
            log_error("Gossip tick failed: %d\n", poll_interval);
            return -1;
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
            cluster_gossip_send_data(gossip, (const uint8_t *) message_with_ts, message_with_ts_size);
        }
        // Tell Pittacus to write existing messages to the socket.
        send_result = cluster_gossip_process_send(gossip);
        if (send_result < 0) {
            log_error("Gossip send failed: %d, %s\n", send_result, strerror(errno));
            cluster_gossip_destroy(gossip);
            return -1;
        }
    }
    cluster_gossip_destroy(gossip);

    return 0;
}