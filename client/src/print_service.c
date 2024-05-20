/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/coap_service.h>
#include <zephyr/net/net_ip.h>

LOG_MODULE_REGISTER(print_service, LOG_LEVEL_DBG);

static int print_put(struct coap_resource *resource, struct coap_packet *request,
                     struct sockaddr *addr, socklen_t addr_len)
{
    const uint8_t *payload;
    uint16_t payload_len;

    LOG_DBG("Received PUT request");

    payload = coap_packet_get_payload(request, &payload_len);
    if (payload_len == 0) {
        LOG_ERR("Invalid payload length");
        return COAP_RESPONSE_CODE_INTERNAL_ERROR;
    }

    LOG_HEXDUMP_DBG(payload, payload_len, "Payload");

    if (strlen(payload) + 1 != payload_len) {
        LOG_ERR("Invalid payload (strlen %zu, payload_len %u)", strlen(payload) + 1, payload_len);
        return COAP_RESPONSE_CODE_INTERNAL_ERROR;
    }

    LOG_INF("Print: %s", payload);

    return COAP_RESPONSE_CODE_CHANGED;
}

static const char *const PRINT_PATH[] = { "print", NULL };
COAP_RESOURCE_DEFINE(print, coap_server, { .put = print_put, .path = PRINT_PATH });

int print_service_init(void)
{
    LOG_INF("Print service initialized");
    return 0;
}
