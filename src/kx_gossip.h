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
#ifndef __CLUSTER_GOSSIP_H__
#define __CLUSTER_GOSSIP_H__

#ifdef  __cplusplus
extern "C" {
#endif

typedef int cluster_socket_fd;
typedef struct cluster_gossip cluster_gossip_t;

typedef enum cluster_gossip_state {
    STATE_INITIALIZED,
    STATE_JOINING,
    STATE_CONNECTED,
    STATE_LEAVING,
    STATE_DISCONNECTED,
    STATE_DESTROYED
} cluster_gossip_state_t;

/**
 * The definition of the user's data receiver.
 *
 * @param context a reference to the arbitrary context specified
 *                by a user.
 * @param gossip a reference to the gossip descriptor instance.
 * @param buffer a reference to the buffer where the data payload
 *               is stored.
 * @param buffer_size a size of the data buffer.
 * @return Void.
 */
typedef void (*data_receiver_t)(void *context, cluster_gossip_t *gossip,
                                const uint8_t *buffer, size_t buffer_size);

typedef struct cluster_addr {
    const cluster_sockaddr *addr;   /**< pointer to the address instance. */
    socklen_t addr_len;             /**< size of the address. */
} cluster_addr_t;

/**
 * Creates a new gossip descriptor instance.
 *
 * @param self_addr the address of the current node. This one is used
 *                  for binding as well as for the propagation of this
 *                  node destination address to other nodes.
 *                  Note: don't use "localhost" or INADDR_ANY because other
 *                  nodes won't be able to reach out to this node.
 * @param data_receiver a data receiver callback. It's invoked each time when
 *                      a new data message arrives.
 * @param data_receiver_context an arbitrary context that is always passed to
 *                              a data_receiver callback.
 * @param uname cluster node name
 * @return a new gossip descriptor instance.
 */
cluster_gossip_t *cluster_gossip_create(const cluster_addr_t *self_addr,
                                          data_receiver_t data_receiver, 
                                          void *data_receiver_context,
                                          const char *uname);

/**
 * Destroys a gossip descriptor instance.
 *
 * @param self a gossip descriptor instance.
 * @return zero on success or negative value if the operation failed.
 */
int cluster_gossip_destroy(cluster_gossip_t *self);

/**
 * Join the gossip cluster using the list of seed nodes.
 *
 * @param self a gossip descriptor instance.
 * @param seed_nodes a list of seed node addresses.
 * @param seed_nodes_len a size of the list.
 * @return zero on success or negative value if the operation failed.
 */
int cluster_gossip_join(cluster_gossip_t *self,
                         const cluster_addr_t *seed_nodes, uint16_t seed_nodes_len);

/**
 * Suggests Pittacus to read a next message from the socket.
 * Only one message will be read.
 *
 * @param self a gossip descriptor instance.
 * @return zero on success or negative value if the operation failed.
 */
int cluster_gossip_process_receive(cluster_gossip_t *self);

/**
 * Suggests Pittacus to write existing outbound messages to the socket.
 * All available messages will be written to the socket.
 *
 * @param self a gossip descriptor instance.
 * @return a number of sent messages or negative value if the operation failed.
 */
int cluster_gossip_process_send(cluster_gossip_t *self);

/**
 * Spreads the given data buffer within a gossip cluster.
 * Note: no network transmission will be performed at this
 * point. The message is added to a queue of outbound messages
 * and will be sent to a cluster during the next cluster_gossip_process_send()
 * invocation.
 *
 * @param self a gossip descriptor instance.
 * @param data a payload.
 * @param data_size a payload size.
 * @return zero on success or negative value if the operation failed.
 */
int cluster_gossip_send_data(cluster_gossip_t *self, const uint8_t *data, uint32_t data_size);

/**
 * Processes the Gossip tick event.
 * Note: no actions will be performed if the time for the next tick
 * has not yet come. However the return value will be recalculated according
 * to the time that has passed since the last tick.
 *
 * @param self a gossip descriptor instance.
 * @return a time interval in milliseconds when the next gossip
 *         tick should happen, or negative value if the error occurred.
 */
int cluster_gossip_tick(cluster_gossip_t *self);

/**
 * Retrieves a current state of this node.
 *
 * @param self a gossip descriptor instance.
 * @return this node's state.
 */
cluster_gossip_state_t cluster_gossip_state(cluster_gossip_t *self);

/**
 * Retrieves gossip socket descriptor.
 *
 * @param self  a gossip descriptor instance.
 * @return a socket descriptor.
 */
cluster_socket_fd cluster_gossip_socket_fd(cluster_gossip_t *self);

/**
 * find member list.
 *
 * @param self  a gossip descriptor instance.
 * @warning member list
 */
cluster_member_set_t *cluster_gossip_member_list(cluster_gossip_t *self);

#ifdef  __cplusplus
}
#endif

#endif