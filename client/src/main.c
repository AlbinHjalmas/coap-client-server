/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
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
#define MULTICAST_PORT 5685
#define MESSAGE_INTERVAL K_SECONDS(1)

static const uint16_t LOCAL_COAP_SERVER_PORT = 5684;

static server_proxy_t server_1;
static server_proxy_t local_server;

#include <zephyr/net/coap_service.h>
#include <zephyr/net/net_ip.h>

#ifdef CONFIG_NET_IPV6
#include "ipv6.h"
#include "net_private.h"
#endif

#include "print_service.h"

/**
 * @brief Address for all nodes on the local network segment that support CoAP.
 *
 * @note The specific address being defined here (0xff02::fd) is a reserved
 * multicast address used for CoAP nodes on the local network segment. The
 * 0xff02 at the start of the address indicates that this is a link-local
 * multicast address, meaning it is only intended for devices on the same
 * network segment. The ::fd at the end is the specific identifier for CoAP
 * nodes.
 */
#define ALL_NODES_LOCAL_COAP_MCAST                                                                 \
    {                                                                                              \
        {                                                                                          \
            {                                                                                      \
                0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfd                            \
            }                                                                                      \
        }                                                                                          \
    }

#define LINE_NODE_MCAST_ADDR                                                                       \
    {                                                                                              \
        {                                                                                          \
            {                                                                                      \
                0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02                            \
            }                                                                                      \
        }                                                                                          \
    }

static int join_coap_multicast_group(void)
{
    static struct in6_addr my_addr;
    static struct sockaddr_in6 mcast_addr = { .sin6_family = AF_INET6,
                                              .sin6_addr = ALL_NODES_LOCAL_COAP_MCAST,
                                              .sin6_port = htons(LOCAL_COAP_SERVER_PORT) };
    struct net_if_addr *ifaddr;
    struct net_if *iface;
    int ret;

    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Could not get the default interface");
        return -ENOENT;
    }

    LOG_DBG("Interface %p", iface);

#if defined(CONFIG_NET_CONFIG_SETTINGS)
    if (net_addr_pton(AF_INET6, CONFIG_NET_CONFIG_MY_IPV6_ADDR, &my_addr) < 0) {
        LOG_ERR("Invalid IPv6 address %s", CONFIG_NET_CONFIG_MY_IPV6_ADDR);
    }
#endif

    ifaddr = net_if_ipv6_addr_add(iface, &my_addr, NET_ADDR_MANUAL, 0);
    if (!ifaddr) {
        LOG_ERR("Could not add unicast address to interface");
        return -EINVAL;
    }

    LOG_DBG("Interface %p addr %p", iface, ifaddr);

    ifaddr->addr_state = NET_ADDR_PREFERRED;

    ret = net_ipv6_mld_join(iface, &mcast_addr.sin6_addr);
    if (ret < 0) {
        LOG_ERR("Cannot join %s IPv6 multicast group (%d)",
                net_sprint_ipv6_addr(&mcast_addr.sin6_addr), ret);
        return ret;
    }

    LOG_DBG("Joined %s IPv6 multicast group", net_sprint_ipv6_addr(&mcast_addr.sin6_addr));

    return 0;
}

static int create_multicast_socket(void)
{
    int sock = -1;
    struct sockaddr_in6 addr = { .sin6_family = AF_INET6,
                                 .sin6_port = htons(MULTICAST_PORT),
                                 .sin6_addr = IN6ADDR_ANY_INIT };

    sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        return -errno;
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("Failed to bind socket: %d", errno);
        close(sock);
        return -errno;
    }

    return sock;
}

// static void send_multicast_message(int sock)
// {
//     struct sockaddr_in6 mcast_addr = {
//         .sin6_family = AF_INET6,
//         .sin6_port = htons(MULTICAST_PORT),
//         .sin6_addr = LINE_NODE_MCAST_ADDR
//     };

//     char payload[128];
//     unsigned int counter = 0;

//     while (1) {
//         snprintf(payload, sizeof(payload), "Hello, World! %d", counter++);
//         int ret = sendto(sock, payload, strlen(payload), 0,
//                          (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
//         if (ret < 0) {
//             LOG_ERR("Failed to send message: %d", errno);
//         } else {
//             LOG_DBG("Sent multicast message: %s", payload);
//         }

//         k_sleep(MESSAGE_INTERVAL);
//     }
// }

int main(void)
{
    // Join the CoAP multicast group
    LOG_DBG("Joining CoAP multicast group");
    int rc = join_coap_multicast_group();
    if (rc < 0) {
        LOG_ERR("Failed to join CoAP multicast group: %d", rc);
        return rc;
    }

    LOG_DBG("Initializing print service");
    rc = print_service_init();
    if (rc < 0) {
        LOG_ERR("Failed to initialize print service (err %d)", rc);
        return rc;
    }

    LOG_DBG("Connecting to remote CoAP server_1");
    rc = server_proxy_start(&server_1, "2001:db8::1", PEER_PORT);
    if (rc < 0) {
        LOG_ERR("Failed to start CoAP server_1: %d", rc);
        goto exit;
    }

    LOG_DBG("Connecting to local CoAP server");
    rc = server_proxy_start(&local_server, "::1", LOCAL_COAP_SERVER_PORT);
    if (rc < 0) {
        LOG_ERR("Failed to start CoAP local_server: %d", rc);
        goto exit;
    }

    int multicast_sock = 0;
    multicast_sock = create_multicast_socket();
    if (multicast_sock < 0) {
        LOG_ERR("Failed to create multicast socket: %d", multicast_sock);
        goto exit;
    }

    struct sockaddr_in6 mcast_addr = { .sin6_family = AF_INET6,
                                       .sin6_port = htons(MULTICAST_PORT),
                                       .sin6_addr = LINE_NODE_MCAST_ADDR };

    uint8_t payload[128] = "Hello, World! N";

    for (unsigned int i = 0; i < UINT32_MAX; i++) {
        sprintf(payload, "Hello, World! %d To server 1 KUK", i);
        rc = server_proxy_print(&server_1, (const char *)payload, K_MSEC(500));
        if (rc < 0) {
            LOG_ERR("Failed to print message to server_1: %d", rc);
        }

        sprintf(payload, "Hello, World! %d To local server KUK", i);
        rc = server_proxy_print(&local_server, (const char *)payload, K_MSEC(500));
        if (rc < 0) {
            LOG_ERR("Failed to print message to local server: %d", rc);
        }

        snprintf(payload, sizeof(payload), "Hello, World! %d", i);
        int ret = sendto(multicast_sock, payload, strlen(payload), 0,
                         (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
        if (ret < 0) {
            LOG_ERR("Failed to send multicast message: %d", errno);
        } else {
            LOG_DBG("Sent multicast message: %s", payload);
        }

        k_sleep(K_SECONDS(1));
    }

    LOG_INF("CoAP client done");

exit:
    server_proxy_stop(&server_1);
    close(multicast_sock);
    return rc;
}

COAP_SERVICE_DEFINE(coap_server, NULL, &LOCAL_COAP_SERVER_PORT, COAP_SERVICE_AUTOSTART);
