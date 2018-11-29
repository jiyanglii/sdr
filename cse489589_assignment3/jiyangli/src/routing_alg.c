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

#include "../include/global.h"
#include "../include/routing_alg.h"
#include "../include/control_header_lib.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h"
#include "../include/data_handler.h"

#define DEBUG

struct ROUTER_INFO node_table[MAX_NODE_NUM] = {0};
uint16_t active_node_num = 0;

uint16_t local_port = 0;

static struct ROUTER_INFO local_node_info = {0};

static uint16_t router_update_ttl = 0;

struct IPV4_ADDR local_ip;

void router_init(char* init_payload){

    // Get local ip address first
    GetPrimaryIP(&local_ip);

    struct CONTROL_INIT_HEADER header;
    char * ptr = init_payload;

    header = *((struct CONTROL_INIT_HEADER *)ptr);
    ptr += sizeof(struct CONTROL_INIT_HEADER);

    active_node_num = header.router_num;
    router_update_ttl = header.ttl;

#ifdef DEBUG
    printf("active_node_num: %hu\n", active_node_num);
    printf("router_update_ttl: %hu\n", router_update_ttl);
#endif

    for(int i=0;i<active_node_num;i++){
        node_table[i].raw_data = *((struct CONTROL_INIT_ROUTER_INFO *)ptr);
        //node_table[i].raw_data.router_ip = node_table[i].raw_data.router_ip;
        inet_ntop(AF_INET, &(node_table[i].raw_data.router_ip), (char *)&(node_table[i].ip._ip_str) , sizeof(node_table[i].ip._ip_str));
        node_table[i].ip._ip = node_table[i].raw_data.router_ip;
        ptr += sizeof(struct CONTROL_INIT_ROUTER_INFO);

        node_table[i].next_hop_router_id = UINT16_MAX;

        if(node_table[i].ip._ip == local_ip._ip)
        {
            // This is the self node
            local_node_info = node_table[i];
            node_table[i].self = TRUE;
            node_table[i].next_hop_router_id = node_table[i].raw_data.router_id;
        }else node_table[i].self = FALSE;

        if(node_table[i].raw_data.router_cost != UINT16_MAX){
            node_table[i].neighbor = TRUE;
            node_table[i].next_hop_router_id = node_table[i].raw_data.router_id;
        } else node_table[i].neighbor = FALSE;

        node_table[i].cost_to = node_table[i].raw_data.router_cost;

        // General initialization
        node_table[i].link_status = FALSE;
        node_table[i].fd = -1;

#ifdef DEBUG
    printf("node_table[%d] router_ip: %d\n", i, node_table[i].raw_data.router_ip);
    printf("node_table[%d] router_ip_str: %s\n", i, node_table[i].ip._ip_str);
    printf("node_table[%d] router_router_port: %d\n", i, node_table[i].raw_data.router_router_port);
#endif

    }

    // Create router port and data port using the info
    routing_sock_init(local_node_info.raw_data.router_router_port, local_node_info.raw_data.router_data_port);

    // Establish/refresh tcp connection with all neighbouring routers
    refresh_data_links();

}

void router_update(char* update_payload){

    struct CONTROL_UPDATE cntrl_update;
    char * ptr = update_payload;

    cntrl_update = *((struct CONTROL_UPDATE *)ptr);

    uint16_t router_update_id = cntrl_update.router_id;
    uint16_t router_update_cost = cntrl_update.router_cost;

}

void routing_table_response(int sock_index){

    uint16_t payload_len, response_len;
    char *cntrl_response_header, *cntrl_response;
    struct CONTROL_ROUTING_TABLE cntrl_routing_table[MAX_NODE_NUM];

    payload_len = sizeof(cntrl_routing_table);
    char * cntrl_response_payload = (char *) malloc(payload_len);

    cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);

    response_len = CNTRL_RESP_HEADER_SIZE+payload_len;
    cntrl_response = (char *) malloc(response_len);

}

void filestats_response(int sock_index){

    uint16_t payload_len;
    char * cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);


}

void GetPrimaryIP(struct IPV4_ADDR * local_ip) {
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

        inet_ntop(AF_INET, (const void *)&name.sin_addr, local_ip->_ip_str, INET_ADDRSTRLEN);
        local_ip->_ip = name.sin_addr.s_addr;
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
            return node_table[i].fd;
        }
    }

    return 0;
}