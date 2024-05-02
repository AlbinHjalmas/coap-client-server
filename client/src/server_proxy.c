#include <zephyr/kernel.h>

#include "server_proxy.h"

static uint8_t sketch[CONFIG_COAP_CLIENT_MESSAGE_HEADER_SIZE + CONFIG_COAP_CLIENT_MESSAGE_SIZE];

int server_proxy_start(server_proxy_t *proxy, const char *const peer_addr, uint16_t port)
{
    return coap_client_start(&proxy->client, peer_addr, port);
}

int server_proxy_stop(server_proxy_t *proxy)
{
    return coap_client_stop(&proxy->client);
}

int server_proxy_print(server_proxy_t *proxy, const char *const message, k_timeout_t timeout)
{
    static const char *const PATH[] = { "print", NULL };
    int rc = coap_client_put(&proxy->client, PATH, (const uint8_t *)message, strlen(message) + 1);
    if (rc < 0) {
        return rc;
    }

    struct coap_packet reply;
    rc = coap_client_wait_and_receive(&proxy->client, &reply, sketch, sizeof(sketch), timeout);
    if (rc < 0) {
        return rc;
    }

    if (coap_header_get_code(&reply) != COAP_RESPONSE_CODE_CHANGED) {
        return -EIO;
    }

    return 0;
}
