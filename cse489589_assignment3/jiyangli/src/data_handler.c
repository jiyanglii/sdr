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
#include "../include/data_handler.h"
#include "../include/routing_alg.h"


/* Linked List for active data connections */
struct DataConn
{
    int sockfd;
    LIST_ENTRY(DataConn) next;
}*connection, *conn_temp;
LIST_HEAD(DataConnsHead, DataConn) data_conn_list;

int create_data_sock(uint16_t data_sock_num)
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
    control_addr.sin_port = htons(data_sock_num);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    if(listen(sock, 5) < 0)
        ERROR("listen() failed");

    return sock;

}

int new_data_conn(int sock_index)
{
    int fdaccept, caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);
    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, (socklen_t *)&caddr_len);
    if(fdaccept < 0){
        ERROR("accept() failed");
    }
    else{
        new_data_link(remote_controller_addr.sin_addr.s_addr, fdaccept);


        /* Insert into list of active control connections*/
        connection = calloc(1, sizeof(struct DataConn));
        connection->sockfd = fdaccept;
        LIST_INSERT_HEAD(&data_conn_list, connection, next);
    }



    return fdaccept;
}

int new_data_conn_client(int router_ip, int router_data_port)
{
    if(local_port == 0) return -2;

    int fdsocket, len;
    struct sockaddr_in remote_server_addr;

    fdsocket = socket(AF_INET, SOCK_STREAM, 0);
    if(fdsocket < 0)
        perror("Failed to create socket");

    bzero(&remote_server_addr, sizeof(remote_server_addr));
    remote_server_addr.sin_family = AF_INET;
    remote_server_addr.sin_addr.s_addr = router_ip;
    //inet_pton(AF_INET, router_ip, &remote_server_addr.sin_addr); //Convert IP addresses from human-readable to binary
    remote_server_addr.sin_port = htons(router_data_port);

    if(connect(fdsocket, (struct sockaddr*)&remote_server_addr, sizeof(remote_server_addr)) < 0){
        close(fdsocket);
        fdsocket = -1;
        perror("Connect failed");
    }
    else{
        /* Insert into list of active control connections*/
        connection = calloc(1, sizeof(struct DataConn));
        connection->sockfd = fdsocket;
        LIST_INSERT_HEAD(&data_conn_list, connection, next);
    }

    return fdsocket;
}

bool isData(int sock_index)
{
    LIST_FOREACH(connection, &data_conn_list, next)
        if(connection->sockfd == sock_index) return TRUE;

    return FALSE;
}


void remove_data_conn(int sock_index)
{
    LIST_FOREACH(connection, &data_conn_list, next) {
        if(connection->sockfd == sock_index) LIST_REMOVE(connection, next); // this may be unsafe?
        free(connection);
    }

    close(sock_index);
}

void data_packet_update(struct DATA * _data)
{
    _data->ttl--;
}

bool data_recv_hook(int sock_index)
{
    int next_hop_fd = 0;
    struct DATA _data = {0};
    //char * raw_data = (char *) malloc(sizeof(char)*(DATA_HEADER_SIZE + MAX_DATA_PAYLOAD));;


    if(recvALL(sock_index, (char *)&_data, sizeof(struct DATA)) < 0){
        remove_data_conn(sock_index);
        return FALSE;
    }

    //_data = *((struct DATA *)raw_data);

    // Forwarding incoming data to next hop here
    data_packet_update(&_data);
    next_hop_fd = get_next_hop(_data.dest_ip_addr);

    if(_data.ttl>0){
        sendALL(next_hop_fd, (char *)&_data, sizeof(struct DATA));
    }

    //free(raw_data);
    return TRUE;
}

void send_file(uint16_t payload_len, char * cntrl_payload)
{
    int next_hop_fd = 0;
    struct CONTROL_SENDFILE header = {0};
    struct DATA _data_packet = {0};
    struct DATA _data_packet_to_send = {0}; // A delayed version to detect EOF

    char * file_name_ptr = cntrl_payload + sizeof(struct CONTROL_SENDFILE);
    uint16_t file_name_len = payload_len - sizeof(struct CONTROL_SENDFILE);
    char * file_name = (char *)calloc(file_name_len, sizeof(char));
    char * file_data_payload = &_data_packet.payload[0];

    header = *((struct CONTROL_SENDFILE *)&cntrl_payload);

    memcpy(&file_name, file_name_ptr, file_name_len);

    // Proccess header information
    next_hop_fd = get_next_hop(header.dest_ip_addr);
    _data_packet.dest_ip_addr = header.dest_ip_addr;
    _data_packet.transfer_id = header.transfer_id;
    _data_packet.ttl = header.init_ttl;
    _data_packet.seq_num = header.init_seq;
    _data_packet.padding = 0;

    printf("The name of the file to send is: %s\n", file_name);

    FILE *fp;
    size_t read_s = 0;
    fp = fopen(file_name, "r");

    if (fp) {
        printf("File opened\n");
        read_s = fread(file_data_payload, 1, MAX_DATA_PAYLOAD, fp);

        while (read_s > 0){
            _data_packet_to_send = _data_packet;
            read_s = fread(file_data_payload, 1, MAX_DATA_PAYLOAD, fp);
            if(read_s > 0)
            {
                // _data_packet_to_send is not EOF
                sendALL(next_hop_fd, (char *)&_data_packet_to_send, sizeof(struct DATA));
                memset(file_data_payload, '\0', MAX_DATA_PAYLOAD);
                _data_packet.seq_num++;
            }
            else{
                _data_packet.padding = DATA_FIN_FLAG_MASK;
                sendALL(next_hop_fd, (char *)&_data_packet_to_send, sizeof(struct DATA));
                memset(file_data_payload, '\0', MAX_DATA_PAYLOAD);
                break;
            }
            usleep(1500);
        }
    }



    fclose(fp);
    free(file_name);
}



