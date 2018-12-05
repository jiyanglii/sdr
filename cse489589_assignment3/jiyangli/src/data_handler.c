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
#include "../include/connection_manager.h"

/*
data_hist[0]  --->   LAST_DATA_PACKET
data_hist[1]  --->   SECOND_LAST_DATA_PACKET
*/
struct DATA data_hist[2] = {0};
LIST_HEAD(TransferRecord_list, TransferRecord) TransferRecordList;
struct TransferRecord * transfer_rec;

struct DataConn *connection, *conn_temp;
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

    if(recvALL(sock_index, (char *)&_data, sizeof(struct DATA)) < 0){
        remove_data_conn(sock_index);
        return FALSE;
    }

    update_data_record(&_data);

    data_packet_update(&_data);

    // Check if destination is self
    if(ntohl(_data.dest_ip_addr) == local_node_info->ip._ip)
    {
        save_data(&_data);
    }
    else{
        // Forwarding incoming data to next hop here
        next_hop_fd = get_next_hop(ntohl(_data.dest_ip_addr));

        if(_data.ttl>0){
            sendALL(next_hop_fd, (char *)&_data, sizeof(struct DATA));
            update_data_record(&_data);
            new_transfer_record(_data.transfer_id, _data.ttl, _data.seq_num);
        }
    }

    return TRUE;
}

void send_file(uint16_t payload_len, char * cntrl_payload)
{
    refresh_data_links();

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
    next_hop_fd = get_next_hop(ntohl(header.dest_ip_addr));
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
                update_data_record(&_data_packet_to_send);
                new_transfer_record(_data_packet_to_send.transfer_id, _data_packet_to_send.ttl, _data_packet_to_send.seq_num);
                memset(file_data_payload, '\0', MAX_DATA_PAYLOAD);
                _data_packet.seq_num++;
            }
            else{
                _data_packet.padding = DATA_FIN_FLAG_MASK;
                sendALL(next_hop_fd, (char *)&_data_packet_to_send, sizeof(struct DATA));
                update_data_record(&_data_packet_to_send);
                new_transfer_record(_data_packet_to_send.transfer_id, _data_packet_to_send.ttl, _data_packet_to_send.seq_num);
                memset(file_data_payload, '\0', MAX_DATA_PAYLOAD);
                break;
            }
            usleep(1500);
        }
    }



    fclose(fp);
    free(file_name);
}

void update_data_record(const struct DATA * _data)
{
    data_hist[1] = data_hist[0];
    data_hist[0] = *_data;
}

void save_data(const struct DATA * _data)
{
    FILE *fp;
    size_t read_s = 0;
    char file_name_str[8] = {0};

    if(_data->ttl > 0) // Drop the packet when ttl reaches 0
    {
        snprintf(&file_name_str[0], 6, "%s", "file-");
        snprintf(&file_name_str[5], 3, "%d", _data->transfer_id);

        fp = fopen(file_name_str, "a");

        if (fp) {
            printf("File opened\n");
            read_s = fwrite(_data->payload, 1, MAX_DATA_PAYLOAD, fp);
        }

        fclose(fp);
    }
}

struct TransferRecord * getExsitingTransfer(uint8_t _transfer_id)
{
    LIST_FOREACH(transfer_rec, &TransferRecordList, next)
        if(transfer_rec->transfer_id == _transfer_id) return transfer_rec;

    return (struct TransferRecord *)NULL;
}


void new_transfer(uint8_t _transfer_id)
{
    transfer_rec = (struct TransferRecord *)calloc(1, sizeof(struct TransferRecord));
    transfer_rec->transfer_id = _transfer_id;
    transfer_rec->ttl = 0;
    transfer_rec->fin = FALSE;
    LIST_INSERT_HEAD(&TransferRecordList, transfer_rec, next);
}

void new_transfer_record(uint8_t _transfer_id, uint8_t _ttl, uint16_t _seq_num)
{
    struct TransferRecord * temp = getExsitingTransfer(_transfer_id);
    if(!temp) // No record found, create a new one
    {
        new_transfer(_transfer_id);
        temp = getExsitingTransfer(_transfer_id);
    }

    temp->ttl = _ttl;
    struct SeqRecord temp_seq = {0};
    temp_seq.seq_num = _seq_num;
    TAILQ_INSERT_TAIL(&temp->_seq, &temp_seq, next);
}



