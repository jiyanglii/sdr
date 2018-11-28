#pragma once
#include <netinet/in.h>
#include <arpa/inet.h>

#define CNTRL_HEADER_SIZE 8
#define CNTRL_RESP_HEADER_SIZE 8

#define PACKET_USING_STRUCT // Comment this out to use alternate packet crafting technique

    struct __attribute__((__packed__)) CONTROL_HEADER
    {
        uint32_t dest_ip_addr;
        uint8_t control_code;
        uint8_t response_time;
        uint16_t payload_len;
    };

    struct __attribute__((__packed__)) CONTROL_RESPONSE_HEADER
    {
        uint32_t controller_ip_addr;
        uint8_t control_code;
        uint8_t response_code;
        uint16_t payload_len;
    };

// Below are the structs of control_payload and control_response_payload

    struct __attribute__((__packed__)) CONTROL_INIT_HEADER
    {
        uint16_t router_num;
        uint16_t ttl;
    };

    struct __attribute__((__packed__)) CONTROL_INIT_ROUTER_INFO
    {
        uint16_t router_id;
        uint16_t router_router_port;
        uint16_t router_data_port;
        uint16_t router_cost;
        uint32_t router_ip;
    };

    struct __attribute__((__packed__)) CONTROL_ROUTING_TABLE                      //Control code 0x02
    {
        uint16_t router_id;
        uint16_t padding;
        uint16_t next_hop_id;
        uint16_t router_cost;
    };

    struct __attribute__((__packed__)) CONTROL_UPDATE                     //Control code 0x03
    {
        uint16_t router_id;
        uint16_t router_cost;
    };

    struct __attribute__((__packed__)) CONTROL_SENDFILE_HEADER             //Control code 0x05
    {
        uint32_t dest_ip_addr;
        uint8_t init_ttl;
        uint8_t transfer_id;
        uint16_t seq;
    };

    struct __attribute__((__packed__)) CONTROL_FILESTATS_HEADER                    //Control code 0x06
    {
        uint8_t transfer_id;
        uint8_t ttl;
        uint16_t padding;
    };

char* create_response_header(int sock_index, uint8_t control_code, uint8_t response_code, uint16_t payload_len);
