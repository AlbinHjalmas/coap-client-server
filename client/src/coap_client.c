/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include <fcntl.h>
#include <errno.h>

#include "coap_client.h"

LOG_MODULE_REGISTER(coap_client, LOG_LEVEL_INF);

int coap_client_start(coap_client_t *client, const char *const peer_addr, uint16_t port)
{
    if (client == NULL || peer_addr == NULL) {
        return -EINVAL;
    }

    int rc = 0;
    struct sockaddr_in6 addr6;

    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(port);
    addr6.sin6_scope_id = 0U;

    inet_pton(AF_INET6, peer_addr, &addr6.sin6_addr);

    client->sock = socket(addr6.sin6_family, SOCK_DGRAM, IPPROTO_UDP);
    if (client->sock < 0) {
        LOG_ERR("Failed to create UDP socket %d", errno);
        return -errno;
    }

    rc = connect(client->sock, (struct sockaddr *)&addr6, sizeof(addr6));
    if (rc < 0) {
        LOG_ERR("Cannot connect to UDP remote : %d", errno);
        return -errno;
    }

    // Set the socket to non-blocking mode.
    // Important when using a UDP socket.
    int flags = fcntl(client->sock, F_GETFL, 0);
    fcntl(client->sock, F_SETFL, flags | O_NONBLOCK);

    // Prepare the pollfd structure.
    // Polling is used to wait for data to be received on the socket.
    client->fds[0].fd = client->sock;
    client->fds[0].events = POLLIN;
    client->nfds = 1;

    return 0;
}

int coap_client_stop(coap_client_t *client)
{
    if (client == NULL) {
        return -EINVAL;
    }

    close(client->sock);

    client->sock = -1;
    client->nfds = 0;

    return 0;
}

int coap_client_put(coap_client_t *client, const char *const *path, const uint8_t *const payload,
                    size_t payload_len)
{
    if (client == NULL || path == NULL || payload == NULL || payload_len == 0) {
        return -EINVAL;
    }

    const size_t MESSAGE_SIZE = CONFIG_COAP_CLIENT_MESSAGE_HEADER_SIZE + payload_len;

    uint8_t *data = (uint8_t *)k_malloc(MESSAGE_SIZE);
    if (!data) {
        return -ENOMEM;
    }

    LOG_DBG("Initializing CoAP packet");

    struct coap_packet request;
    int rc;
    rc = coap_packet_init(&request, data, MESSAGE_SIZE, COAP_VERSION_1, COAP_TYPE_CON,
                          COAP_TOKEN_MAX_LEN, coap_next_token(), COAP_METHOD_PUT, coap_next_id());
    if (rc < 0) {
        LOG_ERR("Failed to initialize CoAP packet: %d", rc);
        goto exit;
    }

    LOG_DBG("Appending URI path to CoAP packet");

    for (const char *const *p = path; p && *p; p++) {
        rc = coap_packet_set_path(&request, *p);
        if (rc < 0) {
            LOG_ERR("Unable add option to request");
            goto exit;
        }
    }

    LOG_DBG("Appending payload marker to CoAP packet");

    rc = coap_packet_append_payload_marker(&request);
    if (rc < 0) {
        LOG_ERR("Failed to append payload marker to CoAP packet: %d", rc);
        goto exit;
    }

    LOG_DBG("Appending payload to CoAP packet");

    rc = coap_packet_append_payload(&request, payload, payload_len);
    if (rc < 0) {
        LOG_ERR("Failed to append payload to CoAP packet: %d", rc);
        goto exit;
    }

    LOG_DBG("Sending CoAP packet");

    ssize_t sent = send(client->sock, request.data, request.offset, 0);
    if (sent < 0) {
        LOG_ERR("Failed to send CoAP packet: %d", errno);
        rc = -errno;
        goto exit;
    }

exit:
    k_free(data);
    return rc;
}

int coap_client_wait_and_receive(coap_client_t *client, struct coap_packet *reply, void *buf,
                                 size_t buf_len, k_timeout_t timeout)
{
    if (client == NULL || buf == NULL) {
        return -EINVAL;
    }

    int rc;
    rc = poll(client->fds, client->nfds, k_ticks_to_ms_ceil32(timeout.ticks));
    if (rc < 0) {
        LOG_ERR("Failed to poll socket: %d", errno);
        return -errno;
    }

    ssize_t received = recv(client->sock, buf, buf_len, 0);
    if (received < 0) {
        LOG_ERR("Failed to receive data: %d", errno);
        return -errno;
    }

    return coap_packet_parse(reply, buf, received, NULL, 0);
}
