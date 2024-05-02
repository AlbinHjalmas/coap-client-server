/*
 * Copyright (c) 2024 Albin Hjalmas (albin@bitman.se)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_event_handler, LOG_LEVEL_INF);

#include <zephyr/net/coap_mgmt.h>
#include <zephyr/net/coap_service.h>

#define COAP_EVENTS_SET                                                                            \
    (NET_EVENT_COAP_OBSERVER_ADDED | NET_EVENT_COAP_OBSERVER_REMOVED |                             \
     NET_EVENT_COAP_SERVICE_STARTED | NET_EVENT_COAP_SERVICE_STOPPED)

static void coap_event_handler_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
                                  struct net_if *iface)
{
    ARG_UNUSED(iface);

    switch (mgmt_event) {
    case NET_EVENT_COAP_OBSERVER_ADDED:
        LOG_INF("CoAP observer added");
        break;
    case NET_EVENT_COAP_OBSERVER_REMOVED:
        LOG_INF("CoAP observer removed");
        break;
    case NET_EVENT_COAP_SERVICE_STARTED:
        if (cb->info != NULL && cb->info_length == sizeof(struct net_event_coap_service)) {
            struct net_event_coap_service *net_event = cb->info;

            LOG_INF("CoAP service %s started", net_event->service->name);
        } else {
            LOG_INF("CoAP service started");
        }
        break;
    case NET_EVENT_COAP_SERVICE_STOPPED:
        if (cb->info != NULL && cb->info_length == sizeof(struct net_event_coap_service)) {
            struct net_event_coap_service *net_event = cb->info;

            LOG_INF("CoAP service %s stopped", net_event->service->name);
        } else {
            LOG_INF("CoAP service stopped");
        }
        break;
    default:
        LOG_WRN("Unknown CoAP event %d", mgmt_event);
        break;
    }
}

static struct net_mgmt_event_callback coap_event_handler;

void coap_event_handler_init(void)
{
    net_mgmt_init_event_callback(&coap_event_handler, coap_event_handler_cb, COAP_EVENTS_SET);
    net_mgmt_add_event_callback(&coap_event_handler);

    LOG_INF("CoAP event handler initialized");
}