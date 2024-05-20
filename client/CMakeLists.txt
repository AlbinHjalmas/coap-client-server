cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(coap-client-server)

target_sources(app PRIVATE 
    src/main.c
    src/coap_client.c
    src/server_proxy.c
    src/print_service.c
)

target_include_directories(app PRIVATE
    src 
    ${ZEPHYR_BASE}/subsys/net/ip
)

zephyr_linker_sources(DATA_SECTIONS sections-ram.ld)
