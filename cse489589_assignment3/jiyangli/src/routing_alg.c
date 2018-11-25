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
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/routing_alg.h"
#include "../include/control_header_lib.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h"

#define DEBUG

#define MAX_NODE_NUM 5

static struct ROUTER_INFO node_table[MAX_NODE_NUM] = {0};

static uint16_t active_node_num = 0;
static uint16_t router_update_ttl = 0;

void router_init(char* init_payload){
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
        node_table[i].raw_data.router_ip = node_table[i].raw_data.router_ip;
        inet_ntop(AF_INET, &(node_table[i].raw_data.router_ip), (char *)&(node_table[i].router_ip_str) , sizeof(node_table[i].router_ip_str));
        ptr += sizeof(struct CONTROL_INIT_ROUTER_INFO);

#ifdef DEBUG
    printf("node_table[%d] router_ip: %d\n", i, node_table[i].raw_data.router_ip);
    printf("node_table[%d] router_ip_str: %s\n", i, node_table[i].router_ip_str);
    printf("node_table[%d] router_port_a: %d\n", i, node_table[i].raw_data.router_port_a);
#endif

    }


    // For now, assume the first node in the list is self
    // Create router port and data port using the info


}

void router_update(char* update_payload){

    struct CONTROL_UPDATE cntrl_update;
    char * ptr = update_payload;

    cntrl_update = *((struct CONTROL_UPDATE *)ptr);

    router_update_id = cntrl_update.router_id;
    router_update_cost = cntrl_update.router_cost;

}

void routing_table_response(int sock_index){

    uint16_t payload_len, response_len;
    char *cntrl_response_header, *cntrl_response;
    struct CONTROL_ROUTING_TABLE cntrl_routing_table[MAX_NODE_NUM];

    payload_len = sizeof(cntrl_routing_table); 
    cntrl_response_payload = (char *) malloc(payload_len);

    cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);

    response_len = CNTRL_RESP_HEADER_SIZE+payload_len;
    cntrl_response = (char *) malloc(response_len);

}

void filestats_response(int sock_index){

    cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);


}

