#pragma once

void router_init(char* init_payload);

struct __attribute__((__packed__)) ROUTING_TABLE
    {
        uint16_t router_id;
        uint16_t padding;
        uint16_t next_hop_id;
        uint16_t router_cost;
    };