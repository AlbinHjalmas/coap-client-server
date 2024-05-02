/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/udp.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/coap_client.h>

#include <errno.h>

#include "server_proxy.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define PEER_PORT 5683
static server_proxy_t server_1;
static server_proxy_t server_2;

int main(void)
{
    int rc = server_proxy_start(&server_1, "2001:db8::1", PEER_PORT);
    if (rc < 0) {
        LOG_ERR("Failed to start CoAP server_1: %d", rc);
        goto exit;
    }

    rc = server_proxy_start(&server_2, "2001:db8::2", PEER_PORT);
    if (rc < 0) {
        LOG_ERR("Failed to start CoAP server_2: %d", rc);
        goto exit;
    }

    static const char *const path[] = { "print", NULL };
    uint8_t payload[128] = "Hello, World! N";
    uint8_t buf[MAX_COAP_MSG_LEN];

    for (int i = 0; i < 10; i++) {
        sprintf(payload, "Hello, World! %d To server 1 KUK", i);
        rc = server_proxy_print(&server_1, (const char *)payload, K_MSEC(500));
        if (rc < 0) {
            LOG_ERR("Failed to print message to server_1: %d", rc);
            goto exit;
        }

        sprintf(payload, "Hello, World! %d To server 2 KUK", i);
        rc = server_proxy_print(&server_2, (const char *)payload, K_MSEC(500));
        if (rc < 0) {
            LOG_ERR("Failed to print message to server_2: %d", rc);
            // goto exit;
        }

        k_sleep(K_SECONDS(1));
    }

    LOG_INF("CoAP server done");

exit:
    server_proxy_stop(&server_1);
    return rc;
}
