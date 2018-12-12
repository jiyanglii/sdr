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

#include "../include/global.h"
#include "../include/network_util.h"
#include "../include/control_header_lib.h"
#include "../include/author.h"
#include "../include/routing_alg.h"
#include "../include/data_handler.h"
#include "../include/control_handler.h"

#ifdef TEST
#include "../include/test_bench.h"
#endif

#define CNTRL_CONTROL_CODE_OFFSET 0x04
#define CNTRL_PAYLOAD_LEN_OFFSET 0x06

uint8_t CRASH = FALSE;

/* Linked List for active control connections */
struct ControlConn
{
    int sockfd;
    LIST_ENTRY(ControlConn) next;
}*connection, *conn_temp;
LIST_HEAD(ControlConnsHead, ControlConn) control_conn_list;

int create_control_sock(void)
{
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(CONTROL_PORT);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    if(listen(sock, 5) < 0)
        ERROR("listen() failed");

    LIST_INIT(&control_conn_list);

    return sock;
}

int new_control_conn(int sock_index)
{
    int fdaccept, caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);
    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, (socklen_t *)&caddr_len);
    if(fdaccept < 0)
        ERROR("accept() failed");

    /* Insert into list of active control connections */
    connection = calloc(1, sizeof(struct ControlConn));
    connection->sockfd = fdaccept;
    LIST_INSERT_HEAD(&control_conn_list, connection, next);

    return fdaccept;
}

void remove_control_conn(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next) {
        if(connection->sockfd == sock_index) LIST_REMOVE(connection, next); // this may be unsafe?
        free(connection);
    }

    close(sock_index);
}

bool isControl(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next)
        if(connection->sockfd == sock_index) return TRUE;

    return FALSE;
}

bool control_recv_hook(int sock_index)
{
    char *cntrl_header, *cntrl_payload;
    uint8_t control_code;
    uint16_t payload_len;

    /* Get control header */
    cntrl_header = (char *) calloc(CNTRL_HEADER_SIZE, sizeof(char));
    bzero(cntrl_header, CNTRL_HEADER_SIZE);

    if(recvALL(sock_index, cntrl_header, CNTRL_HEADER_SIZE) < 0){
        remove_control_conn(sock_index);
        free(cntrl_header);
        return FALSE;
    }

    /* Get control code and payload length from the header */

    /** ASSERT(sizeof(struct CONTROL_HEADER) == 8)
      * This is not really necessary with the __packed__ directive supplied during declaration (see control_header_lib.h).
      * If this fails, comment #define PACKET_USING_STRUCT in control_header_lib.h
      */
    BUILD_BUG_ON(sizeof(struct CONTROL_HEADER) != CNTRL_HEADER_SIZE); // This will FAIL during compilation itself; See comment above.

    struct CONTROL_HEADER *header = (struct CONTROL_HEADER *) cntrl_header;
    control_code = header->control_code;
    payload_len = ntohs(header->payload_len);

    free(cntrl_header);

    /* Get control payload */
    if(payload_len != 0){
        cntrl_payload = (char *) calloc(payload_len, sizeof(char));

        if(recvALL(sock_index, cntrl_payload, payload_len) < 0){
            remove_control_conn(sock_index);
            free(cntrl_payload);
            return FALSE;
        }
    }

#ifdef DEBUG
    printf("Incoming control(cntl code: %d) payload:\n", control_code);
    payload_printer(payload_len, cntrl_payload);
    printf("\n");
#endif


    /* Triage on control_code */
    switch(control_code){
        case 0: author_response(sock_index);
                break;

        case 0x01:
            // INIT
            router_init(cntrl_payload);
            control_response(sock_index, control_code);
            break;


        case 0x02:
            // ROUTING TABLE
            routing_table_response(sock_index, control_code);
            break;

        case 0x03:
            // UPDATE
            cost_update(cntrl_payload);
            control_response(sock_index, control_code);
            break;

        case 0x04:
            // CRASH
            crash(sock_index, control_code);
            usleep(500000);
            break;

        case 0x05:
            // SENDFILE
            send_file_resp(sock_index, control_code);
            send_file(payload_len, cntrl_payload);
            break;

        case 0x06:
            // SENDFILE-STATS
            send_file_stats_response(sock_index, control_code, cntrl_payload);
            break;

        case 0x07:
            // LAST_DATA_PACKET
            send_prev_data(sock_index, control_code);
            break;

        case 0x08:
            // PE
            send_prev_data(sock_index, control_code);
            break;

        default:    break;

    }

    if(payload_len != 0) free(cntrl_payload);
    return TRUE;
}

void crash(int sock_index, uint8_t _control_code){
    // Stop broadcasting routing table to neighbors
    CRASH = TRUE;
    // Send response message to controller
    char *cntrl_response_header;

    cntrl_response_header = create_response_header(sock_index, _control_code, 0, 0);
    if(sock_index>0) sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);

    // Stop self update timer
    for (int i = 0; i < active_node_num; i++)
    {
        if(node_table[i].self == TRUE){
            timerclear(&node_table[i]._timer.time_last);
            timerclear(&node_table[i]._timer.time_next);
            node_table[i]._timer.timer_pending = FALSE;
        }
    }
    free(cntrl_response_header);
}


void routing_table_response(int sock_index, uint8_t _control_code){

    uint16_t payload_len, response_len;
    char *cntrl_response_header, *cntrl_response;
    struct CONTROL_ROUTING_TABLE cntrl_routing_table[MAX_NODE_NUM] = {0};

    for (int i = 0; i < active_node_num; i++)
    {
        cntrl_routing_table[i].router_id   = node_table[i].raw_data.router_id;
        cntrl_routing_table[i].next_hop_id = node_table[i].next_hop_router_id;
        cntrl_routing_table[i].router_cost = ntohs(node_table[i].cost_to);
    }

    payload_len = active_node_num * sizeof(struct CONTROL_ROUTING_TABLE);
    cntrl_response_header = create_response_header(sock_index, _control_code, 0, payload_len);

    response_len = CNTRL_RESP_HEADER_SIZE+payload_len;

    cntrl_response = (char *) calloc(response_len, sizeof(uint8_t));
    memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);

    memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, (char *)&cntrl_routing_table[0], payload_len);

#ifdef DEBUG
    printf("routing_table_response payload:\n");
    payload_printer(response_len, cntrl_response);
    printf("\n");
#endif

    if(sock_index > 0) sendALL(sock_index, cntrl_response, response_len);

    free(cntrl_response);
    free(cntrl_response_header);
}


// if fin==1 send response
void send_file_resp(int sock_index, uint8_t _control_code){

    char *cntrl_response_header;

    cntrl_response_header = create_response_header(sock_index, _control_code, 0, 0);
    sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);
}

void send_prev_data(int sock_index, uint8_t _control_code)
{
    uint16_t payload_len, response_len;
    char *cntrl_response_header, *cntrl_response;

    payload_len = sizeof(struct DATA);
    response_len = CNTRL_RESP_HEADER_SIZE+payload_len;

    cntrl_response = (char *) calloc(response_len, sizeof(uint8_t));

    cntrl_response_header = create_response_header(sock_index, _control_code, 0, payload_len);
    memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);


    if(_control_code == 0x07){
        memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, (char *) &data_hist[0], payload_len);
    }
    else if(_control_code == 0x08){
        memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, (char *) &data_hist[1], payload_len);
    }

    sendALL(sock_index, cntrl_response, response_len);

    free(cntrl_response);
    free(cntrl_response_header);
}

void control_response(int sock_index, uint8_t _control_code){

    char *cntrl_response_header;

    cntrl_response_header = create_response_header(sock_index, _control_code, 0, 0);
    sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);
}


void send_file_stats_response(int sock_index, uint8_t _control_code, const char * _payload)
{
    char *cntrl_response_header;
    char *cntrl_response;
    uint8_t _transfer_id = *((uint8_t *)_payload);
    uint16_t _payload_len = 0;

    char * file_stats_payload = get_file_stats_payload(_transfer_id);
    if(file_stats_payload){

        _payload_len = *((uint16_t *)(_payload + 2));

        // Recover the padding to 0
        *((uint16_t *)(_payload + 2)) = 0x0000;

        cntrl_response_header = create_response_header(sock_index, _control_code, 0, _payload_len);

        cntrl_response = calloc((CNTRL_RESP_HEADER_SIZE + _payload_len),sizeof(char));
        memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
        memcpy((cntrl_response + CNTRL_RESP_HEADER_SIZE), file_stats_payload, _payload_len);

        sendALL(sock_index, cntrl_response, (CNTRL_RESP_HEADER_SIZE + _payload_len));

        free(cntrl_response);
        free(cntrl_response_header);
        free(file_stats_payload);
    }
    else{
        cntrl_response_header = create_response_header(sock_index, _control_code, 1, 0);
        sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
        free(cntrl_response_header);
    }


}





