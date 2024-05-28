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
static const uint16_t multicast_port = 5685; // Different port for multicast group

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

#define STACK_SIZE 2048
#define PRIORITY 7

K_THREAD_STACK_DEFINE(thread_stack, STACK_SIZE);
struct k_thread thread_data;

static int join_multicast_group(struct in6_addr *mcast_addr)
{
    struct net_if_mcast_addr *if_maddr;
    struct net_if *iface;
    char addr_str[NET_IPV6_ADDR_LEN];

    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Could not get the default interface");
        return -ENOENT;
    }

    if (!net_if_is_up(iface)) {
        LOG_ERR("Default interface is down");
        return -ENETDOWN;
    }

    net_addr_ntop(AF_INET6, mcast_addr, addr_str, sizeof(addr_str));
    LOG_DBG("Joining multicast group %s", addr_str);

    if_maddr = net_if_ipv6_maddr_add(iface, mcast_addr);
    if (!if_maddr) {
        LOG_ERR("Could not add multicast address %s to interface", addr_str);
        return -EINVAL;
    }

    if_maddr->is_joined = true;
    LOG_DBG("Joined multicast group %s", addr_str);

    return 0;
}

static int join_multicast_groups(void)
{
    static struct in6_addr all_nodes_local_coap_mcast = ALL_NODES_LOCAL_COAP_MCAST;
    static struct in6_addr line_node_mcast_addr = LINE_NODE_MCAST_ADDR;

    int ret;

    ret = join_multicast_group(&all_nodes_local_coap_mcast);
    if (ret < 0) {
        return ret;
    }

    ret = join_multicast_group(&line_node_mcast_addr);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static void process_received_message(int sock)
{
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

    int multicast_sock;

    coap_event_handler_init();

    int ret = join_multicast_groups();
    if (ret < 0) {
        LOG_ERR("Failed to join multicast groups (err %d)", ret);
        return ret;
    }

    multicast_sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (multicast_sock < 0) {
        LOG_ERR("Failed to create multicast socket: %d", errno);
        return -errno;
    }

    struct sockaddr_in6 bind_addr = {
        .sin6_family = AF_INET6,
        .sin6_addr = IN6ADDR_ANY_INIT,
        .sin6_port = htons(multicast_port), // Use a different port for multicast
    };

    ret = bind(multicast_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
    if (ret < 0) {
        LOG_ERR("Failed to bind multicast socket: %d", errno);
        close(multicast_sock);
        return -errno;
    }

    k_thread_create(&thread_data, thread_stack, K_THREAD_STACK_SIZEOF(thread_stack),
                    (k_thread_entry_t)process_received_message, (void *)(intptr_t)multicast_sock,
                    NULL, NULL, PRIORITY, 0, K_NO_WAIT);

    k_thread_name_set(&thread_data, "multicast_recv_thread");

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
