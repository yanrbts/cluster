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

#include "kx_config.h"

#ifdef  __cplusplus
extern "C" {
#endif

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
 * @return a new gossip descriptor instance.
 */
cluster_gossip_t *cluster_gossip_create(const cluster_addr_t *self_addr,
                                          data_receiver_t data_receiver, void *data_receiver_context);

#ifdef  __cplusplus
}
#endif

#endif