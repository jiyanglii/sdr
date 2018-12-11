/**
 * @jiyangli_assignment3
 * @author  Jiyang Li <jiyangli@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "../include/global.h"
#include "../include/routing_alg.h"
#include "../include/control_header_lib.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h"
#include "../include/data_handler.h"
#include "../include/network_util.h"


#ifdef TEST
#include "../include/test_bench.h"
#endif

#define DEBUG

struct ROUTER_INFO node_table[MAX_NODE_NUM] = {0};
uint16_t active_node_num = 0;

uint16_t local_port = 0;

const struct ROUTER_INFO * local_node_info;

struct timeval router_update_ttl = {0};

// static struct CONTROL_ROUTING_TABLE routing_table[MAX_NODE_NUM] = {0};

struct IPV4_ADDR local_ip = {0};

void router_init(char* init_payload){
    uint8_t self_found = FALSE;

    // Get local ip address first
    GetPrimaryIP(&local_ip);
    timerclear(&router_update_ttl);

#ifdef DEBUG
    printf("local_ip->_ip: %x\n", local_ip._ip);
    printf("local_ip->_ip_str: %s\n", local_ip._ip_str);
#endif

    struct CONTROL_INIT_HEADER header;
    char * ptr = init_payload;

    header = *((struct CONTROL_INIT_HEADER *)ptr);
    ptr += sizeof(struct CONTROL_INIT_HEADER);

    active_node_num = ntohs(header.router_num);
    router_update_ttl.tv_sec = ntohs(header.ttl);

#ifdef DEBUG
    printf("active_node_num: %hu\n", active_node_num);
    printf("router_update_ttl: %ld\n", router_update_ttl.tv_sec);
#endif

    for(int i=0;i<active_node_num;i++){
        node_table[i].raw_data = *((struct CONTROL_INIT_ROUTER_INFO *)ptr);

#ifdef DEBUG
        printf("raw_data: node_table[%d] router_ip: %x\n", i, node_table[i].raw_data.router_ip);
        printf("raw_data: node_table[%d] router_router_port: %x\n", i, node_table[i].raw_data.router_router_port);
#endif

        // Endian
        node_table[i].raw_data.router_ip = (node_table[i].raw_data.router_ip); //ntohl
        node_table[i].raw_data.router_router_port = ntohs(node_table[i].raw_data.router_router_port);
        node_table[i].raw_data.router_data_port = ntohs(node_table[i].raw_data.router_data_port);

        //node_table[i].raw_data.router_ip = node_table[i].raw_data.router_ip;
        inet_ntop(AF_INET, &(node_table[i].raw_data.router_ip), (char *)&(node_table[i].ip._ip_str) , sizeof(node_table[i].ip._ip_str));
        node_table[i].ip._ip = node_table[i].raw_data.router_ip;
        ptr += sizeof(struct CONTROL_INIT_ROUTER_INFO);

        node_table[i].next_hop_router_id = UINT16_MAX;
        node_table[i].cost_to = ntohs(node_table[i].raw_data.router_cost);

        if((node_table[i].ip._ip == local_ip._ip) && (node_table[i].cost_to == 0))
        {
            // This is the self node
            self_found = TRUE;
            local_node_info = &node_table[i];
            node_table[i].self = TRUE;
            node_table[i].next_hop_router_id = node_table[i].raw_data.router_id;

            local_port = node_table[i].raw_data.router_data_port;

            // Start the self update timer
            gettimeofday(&node_table[i]._timer.time_last, NULL);
            timeradd(&node_table[i]._timer.time_last, &router_update_ttl, &node_table[i]._timer.time_next);
        }else node_table[i].self = FALSE;

        if(node_table[i].raw_data.router_cost != UINT16_MAX){
            node_table[i].neighbor = TRUE;
            node_table[i].next_hop_router_id = node_table[i].raw_data.router_id;
        } else {
            node_table[i].neighbor = FALSE;
            node_table[i].next_hop_router_id = UINT16_MAX;
        }

        // General initialization
        node_table[i].link_status = FALSE;
        node_table[i].fd = -1;
        node_table[i].link_status_s = FALSE;
        node_table[i].fd_s = -1;

        node_table[i]._timer.timer_pending = FALSE;
        node_table[i]._timer.time_outs = 0;
        timerclear(&node_table[i]._timer.time_next);
        timerclear(&node_table[i]._timer.time_last);
        timerclear(&node_table[i]._timer.ttl);

        // uint16_t update_table_len;
        // update_table_len = sizeof()

#ifdef DEBUG
        printf("node_table[%d] router_ip: %x\n", i, node_table[i].ip._ip);
        printf("node_table[%d] router_ip_str: %s\n", i, node_table[i].ip._ip_str);
        printf("node_table[%d] router_router_port: %d\n", i, node_table[i].raw_data.router_router_port);
#endif

    }

    if(self_found){
        // Create router port and data port using the info
        routing_sock_init(local_node_info->raw_data.router_router_port, local_node_info->raw_data.router_data_port);

        // Establish/refresh tcp connection with all neighbouring routers
        //refresh_data_links();
    }
    else{
        // Let local_node_info point to something
        local_node_info = &node_table[0];
    }

}

void send_update_table(void)
{
    struct   ROUTING_UPDATE local_update_info = {0};
    struct   ROUTING_UPDATE_HEADER local_update_header = {0};
    uint16_t payload_len = 0;;
    char     *table_response = (char *) malloc(MAX_ROUTING_UPDATE_PAYLOAD_SIZE);
    char     *ptr = table_response;


    local_update_header.router_num = htons(active_node_num);
    local_update_header.source_router_port = htons(local_node_info->raw_data.router_router_port);
    local_update_header.source_router_ip   = (local_node_info->ip._ip);

    memcpy(ptr, (char *)&local_update_header, sizeof(struct ROUTING_UPDATE_HEADER));
    ptr += sizeof(struct ROUTING_UPDATE_HEADER);
    payload_len += sizeof(struct ROUTING_UPDATE_HEADER);


    for(int i=0;i<active_node_num;i++)
    {

        local_update_info.router_ip   = (node_table[i].raw_data.router_ip);
        local_update_info.router_port = htons(node_table[i].raw_data.router_router_port);
        local_update_info.router_id   = node_table[i].raw_data.router_id;
        local_update_info.router_cost = htons(node_table[i].cost_to);

        memcpy(ptr, (char *)&local_update_info, sizeof(struct ROUTING_UPDATE));
        ptr += sizeof(struct ROUTING_UPDATE);
        payload_len += sizeof(struct ROUTING_UPDATE);
    }

#ifdef DEBUG
    printf("Outgoing routing table update payload:\n");
    payload_printer(payload_len, table_response);
    printf("\n");
#endif

    udp_router_update(table_response, payload_len);
}

void BellmanFord_alg(const char * update_packet){

    struct ROUTING_UPDATE_HEADER header;
    struct ROUTING_UPDATE router_info[MAX_NODE_NUM];
    char * ptr = (char *)update_packet;
    uint16_t    base_cost = 0;
    uint16_t    temp = 0;
    uint16_t    source_id = 0;


    header = *((struct ROUTING_UPDATE_HEADER *)ptr);
    ptr += sizeof(struct CONTROL_INIT_HEADER);

    uint16_t update_fields = ntohs(header.router_num);
    uint32_t source_ip = (header.source_router_ip);

#ifdef DEBUG
    printf("BellmanFord_alg:\n");
    printf("update_fields %x (%d)\n", update_fields, (uint16_t)update_fields);
    printf("Incoming routing table update payload from %x:\n", source_ip);
    payload_printer(update_fields*sizeof(struct ROUTING_UPDATE), ptr);
    printf("\n");
#endif

    for(int i=0;i<update_fields;i++){
        router_info[i] = *((struct ROUTING_UPDATE *)ptr);
        ptr += sizeof(struct ROUTING_UPDATE);
    }

    for(int i=0;i<MAX_NODE_NUM;i++){
        if (node_table[i].raw_data.router_ip == source_ip)
        {
            base_cost = node_table[i].cost_to;
            source_id = node_table[i].raw_data.router_id;
        }
    }

    printf("Update from neighbour:%d with old cost%d\n", source_id,base_cost);

    // update routing table of node_table
    for (int i = 0; i < MAX_NODE_NUM; i++)
    {
        if ((node_table[i].self == FALSE) && (source_id != node_table[i].raw_data.router_id))
        {
            for (int j = 0; j < update_fields; j++)
            {
                if ((router_info[j].router_ip == node_table[i].raw_data.router_ip) && (router_info[j].router_cost != UINT16_MAX))
                {
                    temp = base_cost + ntohs(router_info[j].router_cost);
                    if (temp < node_table[i].cost_to)
                    {
                        printf("Updated new cost:%d to neighbour:%d\n",temp, source_id);
                        node_table[i].cost_to = temp;
                        node_table[i].next_hop_router_id = source_id;
                    }
                }

            }

        }


    }

}

void router_update(char* update_payload){

    struct CONTROL_UPDATE cntrl_update;
    char * ptr = update_payload;

    cntrl_update = *((struct CONTROL_UPDATE *)ptr);

    for (int i = 0; i < MAX_NODE_NUM; i++)
    {
        if (node_table[i].raw_data.router_id == cntrl_update.router_id)
        {
            node_table[i].cost_to = cntrl_update.router_cost;
        }
    }
}

void GetPrimaryIP(struct IPV4_ADDR * _local_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock <0) {
      perror("Can not create socket!");
    }
    const char*         GoogleIp = "8.8.8.8";
    int                 GooglePort = 53;
    struct              sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family      = AF_INET;
    serv.sin_addr.s_addr = inet_addr(GoogleIp);
    serv.sin_port        = htons(GooglePort);

    if(connect(sock,(struct sockaddr*) &serv,sizeof(serv)) <0)
       perror("can not connect");
    else{
        struct sockaddr_in name;
        socklen_t namelen = sizeof(name);
        if(getsockname(sock, (struct sockaddr *) &name, &namelen) <0)
            perror("can not get host name");

        inet_ntop(AF_INET, (const void *)&name.sin_addr, _local_ip->_ip_str, INET_ADDRSTRLEN);
        _local_ip->_ip = name.sin_addr.s_addr;
        close(sock);
    }
}

int get_next_hop(uint32_t dest_ip)
{
    uint16_t next_hop_id = -1;
    // Return next hop fd by destination IP
    for(int i=0;i<active_node_num;i++){
        if(node_table[i].ip._ip == dest_ip){
            next_hop_id = node_table[i].next_hop_router_id;
        }
    }

    for(int i=0;i<active_node_num;i++){
        if(node_table[i].raw_data.router_id == next_hop_id){
            new_send_data_link(node_table[i].ip._ip);
            return node_table[i].fd;
        }
    }

    return 0;
}

uint8_t new_recv_data_link(uint32_t ip, int fd){
    for(int i=0;i<active_node_num;i++){
        if(node_table[i].ip._ip == ip){
            if(node_table[i].fd_s < 0)
            {
                node_table[i].fd_s = fd;
                node_table[i].link_status_s = TRUE;
                return TRUE;
            }
        }
    }

    return FALSE;
}

void cost_update(const char * _payload)
{
    struct COST_UPDATE new_cost = {0};

    new_cost = *((struct COST_UPDATE *)_payload);

    new_cost.router_id = (new_cost.router_id);
    new_cost.cost = ntohs(new_cost.cost);

    for(int i=0;i<active_node_num;i++)
    {
        if(node_table[i].raw_data.router_id == new_cost.router_id)
        {
            printf("Router %s now have new cost: %x\n, changed from %x", node_table[i].ip._ip_str, new_cost.cost, node_table[i].cost_to);
            node_table[i].cost_to = new_cost.cost;
        }
    }

}




