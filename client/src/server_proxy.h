/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SERVER_PROXY_H
#define SERVER_PROXY_H

#include "coap_client.h"

typedef struct {
    coap_client_t client;
} server_proxy_t;

/**
 * @brief Initiate and start the specified server proxy.
 * 
 * @param proxy The server proxy to start.
 * @param peer_addr The address of the peer to connect to.
 * @param port The port of the peer to connect to.
 * @return int 0 if successful, otherwise a negative error code.
 */
int server_proxy_start(server_proxy_t *proxy, const char *const peer_addr, uint16_t port);

/**
 * @brief Stop the specified server proxy.
 * 
 * @param proxy The server proxy to stop.
 * @return int 0 if successful, otherwise a negative error code.
 */
int server_proxy_stop(server_proxy_t *proxy);

/**
 * @brief Print the specified message using the server proxy.
 * 
 * @param proxy The server proxy to use.
 * @param message The message to print.
 * @param timeout The timeout for the operation.
 * @return int 0 if successful, otherwise a negative error code.
 */
int server_proxy_print(server_proxy_t *proxy, const char *const message, k_timeout_t timeout);

#endif // SERVER_PROXY_H