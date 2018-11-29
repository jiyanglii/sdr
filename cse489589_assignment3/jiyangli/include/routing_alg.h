#pragma once
#include <sys/time.h>
#include "../include/control_header_lib.h"

void router_init(char* init_payload);

#define MAX_NODE_NUM 5

#define PACKET_USING_STRUCT // Comment this out to use alternate packet crafting technique

struct __attribute__((__packed__)) DATA_PACKET_HEADER
{
    uint32_t dest_ip_addr;
    uint8_t transfer_id;
    uint8_t ttl;
    uint16_t seq;
    uint8_t fin;
    uint8_t padding;
};

struct __attribute__((__packed__)) ROUTING_UPDATE_HEADER
{
    uint16_t router_num;
    uint16_t source_router_port;
    uint32_t source_router_ip;
};

struct __attribute__((__packed__)) ROUTING_UPDATE
{
    uint32_t router_ip;
    uint16_t router_port;
    uint16_t padding;
    uint16_t router_id;
    uint16_t router_cost;
};

struct IPV4_ADDR
{
    uint32_t _ip;
    char _ip_str[INET_ADDRSTRLEN];
};

struct ROUTER_INFO
{
    struct CONTROL_INIT_ROUTER_INFO raw_data;
    struct IPV4_ADDR ip;
    bool self;
    bool neighbor;
    bool link_status;
    int fd;             // This is the FD when self is Client, I connect() others
    bool link_status_s;
    int fd_s;           // This is the FD when self is Server, others connect() me

    // Routing related
    uint16_t cost_to;
    uint16_t next_hop_router_id;
};

struct ROUTER_UPDATE_TIMER
{
    uint16_t router_id;
    struct timeval time;
};

void GetPrimaryIP(struct IPV4_ADDR * local_ip);
int get_next_hop(uint32_t dest_ip);
uint8_t new_data_link(uint32_t ip, int fd);

extern struct ROUTER_INFO node_table[MAX_NODE_NUM];
extern uint16_t active_node_num;
extern uint16_t local_port;