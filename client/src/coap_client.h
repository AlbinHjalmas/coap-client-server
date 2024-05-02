/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COAP_CLIENT_H
#define COAP_CLIENT_H

#include <zephyr/net/socket.h>
#include <zephyr/net/coap.h>

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int sock; // Socket descriptor. Used to send and receive data
    struct pollfd fds[1]; // Polling structure used to wait for data
    int nfds; // Number of file descriptors to poll
} coap_client_t;

/**
 * @brief Initiate and start the specified CoAP client.
 * 
 * @param client The CoAP client to start.
 * @param peer_addr The address of the peer to connect to.
 * @param port The port of the peer to connect to.
 * @return int 0 if successful, otherwise a negative error code.
 */
int coap_client_start(coap_client_t *client, const char *const peer_addr, uint16_t port);

/**
 * @brief Stop the specified CoAP client.
 * 
 * @param client The CoAP client to stop.
 * @return int 0 if successful, otherwise a negative error code.
 */
int coap_client_stop(coap_client_t *client);

/**
 * @brief Send a CoAP PUT request to the specified path.
 * 
 * @param client The CoAP client to use.
 * @param path The path to send the PUT request to.
 * @return int 0 if successful, otherwise a negative error code.
 */
int coap_client_put(coap_client_t *client, const char *const *path, const uint8_t *const payload,
                    size_t payload_len);

/**
 * @brief Wait for a CoAP reply and receive it.
 * 
 * @param client The CoAP client to use.
 * @param reply The CoAP packet to receive the reply into.
 * @param buf Buffer to store the reply in.
 * @param buf_len The length of the buffer.
 * @param timeout The timeout to wait for the reply.
 * @return int 0 if successful, otherwise a negative error code.
 */
int coap_client_wait_and_receive(coap_client_t *client, struct coap_packet *reply, void *buf,
                                 size_t buf_len, k_timeout_t timeout);

#endif // COAP_CLIENT_H
