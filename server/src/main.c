/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_server, LOG_LEVEL_DBG);

#include <zephyr/net/coap.h>
#include <zephyr/net/coap_link_format.h>
#include <zephyr/net/coap_service.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>

#ifdef CONFIG_NET_IPV6
#include "ipv6.h"
#include "net_private.h"
#endif

#include "coap_event_handler.h"
#include "print_service.h"

static const uint16_t coap_port = 5683;

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
                0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01                            \
            }                                                                                      \
        }                                                                                          \
    }

#define STACK_SIZE 2048
#define PRIORITY 7

K_THREAD_STACK_DEFINE(thread_stack, STACK_SIZE);
struct k_thread thread_data;

static int join_multicast_group(struct in6_addr *mcast_addr)
{
    struct net_if_mcast_addr *if_maddr;
    struct net_if *iface;

    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Could not get the default interface");
        return -ENOENT;
    }

    if_maddr = net_if_ipv6_maddr_add(iface, mcast_addr);
    if (!if_maddr) {
        char addr_str[NET_IPV6_ADDR_LEN];
        net_addr_ntop(AF_INET6, mcast_addr, addr_str, sizeof(addr_str));
        LOG_ERR("Could not add multicast address %s to interface", addr_str);
        return -EINVAL;
    }

    if_maddr->is_joined = true;

    return 0;
}

static int join_coap_multicast_group(void)
{
    static struct in6_addr coap_mcast_addr = ALL_NODES_LOCAL_COAP_MCAST;
    static struct in6_addr line_node_mcast_addr = LINE_NODE_MCAST_ADDR;

    int ret;

    ret = join_multicast_group(&coap_mcast_addr);
    if (ret < 0) {
        return ret;
    }

    ret = join_multicast_group(&line_node_mcast_addr);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static void process_received_message(void *p1, void *p2, void *p3)
{
    int sock = (int)(intptr_t)p1;
    struct sockaddr_in6 src_addr;
    socklen_t addr_len = sizeof(src_addr);
    char buffer[128];
    int len;

    while (1) {
        len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr, &addr_len);
        if (len < 0) {
            LOG_ERR("Failed to receive data");
            break;
        }

        buffer[len] = '\0'; // Ensure the received data is null-terminated
        LOG_INF("Received data: %s", buffer);
    }
}

int main(void)
{
    LOG_DBG("Starting CoAP server");

    int coap_sock;

    coap_event_handler_init();

    int ret = join_coap_multicast_group();
    if (ret < 0) {
        LOG_ERR("Failed to join multicast groups (err %d)", ret);
        return ret;
    }

    coap_sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (coap_sock < 0) {
        LOG_ERR("Failed to create socket");
        return -errno;
    }

    struct sockaddr_in6 bind_addr = {
        .sin6_family = AF_INET6,
        .sin6_addr = IN6ADDR_ANY_INIT,
        .sin6_port = htons(coap_port),
    };

    ret = bind(coap_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
    if (ret < 0) {
        LOG_ERR("Failed to bind socket");
        close(coap_sock);
        return -errno;
    }

    k_thread_create(&thread_data, thread_stack, K_THREAD_STACK_SIZEOF(thread_stack),
                    process_received_message, (void *)(intptr_t)coap_sock, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    k_thread_name_set(&thread_data, "coap_recv_thread");

    int rc = print_service_init();
    if (rc < 0) {
        LOG_ERR("Failed to initialize print service (err %d)", rc);
        return rc;
    }

    return 0;
}

#ifdef CONFIG_NET_IPV6
COAP_SERVICE_DEFINE(coap_server, NULL, &coap_port, COAP_SERVICE_AUTOSTART);
#else
#error "IPv4 not supported"
#endif /* CONFIG_NET_IPV6 */
