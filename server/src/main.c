/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_server, LOG_LEVEL_DBG);

#include <zephyr/net/coap_service.h>
#include <zephyr/net/net_ip.h>

#ifdef CONFIG_NET_IPV6
#include "ipv6.h"
#include "net_private.h"
#endif

#include "coap_event_handler.h"
#include "print_service.h"

static const uint16_t coap_port = 5683;

#ifdef CONFIG_NET_IPV6

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

static int join_coap_multicast_group(void)
{
    static struct in6_addr my_addr;
    static struct sockaddr_in6 mcast_addr = { .sin6_family = AF_INET6,
                                              .sin6_addr = ALL_NODES_LOCAL_COAP_MCAST,
                                              .sin6_port = htons(coap_port) };
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

int main(void)
{
    LOG_DBG("Starting CoAP server");

    int rc;

    coap_event_handler_init();

    rc = join_coap_multicast_group();
    if (rc < 0) {
        LOG_ERR("Failed to join CoAP multicast group (err %d)", rc);
        return rc;
    }

    rc = print_service_init();
    if (rc < 0) {
        LOG_ERR("Failed to initialize print service (err %d)", rc);
        return rc;
    }

    return 0;
}

#else
#error "IPv4 not supported"
#endif /* CONFIG_NET_IPV6 */

COAP_SERVICE_DEFINE(coap_server, NULL, &coap_port, COAP_SERVICE_AUTOSTART);