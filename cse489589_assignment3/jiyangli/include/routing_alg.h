#pragma once

void router_init(char* init_payload);

#define PACKET_USING_STRUCT // Comment this out to use alternate packet crafting technique

#ifdef PACKET_USING_STRUCT

	struct __attribute__((__packed__)) ROUTING_TABLE
	{
	    uint16_t router_id;
	    uint16_t padding;
	    uint16_t next_hop_id
	    uint16_t router_cost;
	};

	struct __attribute__((__packed__)) DATA_PACKET_HEADER
	{
		uint32_t dest_ip_addr;
	    uint8_t transfer_id;
	    uint8_t ttl;
	    uint16_t seq;
	    uint8_t fin;
	    uint8_t padding;
	};

#endif