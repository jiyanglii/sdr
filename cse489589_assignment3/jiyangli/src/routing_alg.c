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

#include "../include/routing_alg.h"
#include "../include/control_header_lib.h"
#include "../include/connection_manager.h"

#define DEBUG

#define MAX_NODE_NUM 5

static struct CONTROL_INIT_ROUTER_INFO node_table[MAX_NODE_NUM] = {0};

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
        node_table[i] = *((struct CONTROL_INIT_ROUTER_INFO *)ptr);
        ptr += sizeof(struct CONTROL_INIT_ROUTER_INFO);

#ifdef DEBUG
    printf("node_table[%d] router_port_a: %hu\n", i, node_table[i].router_port_a);
#endif
    }
}

