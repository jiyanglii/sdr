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

#ifdef DEBUG
#include "../include/test_bench.h"
#endif

/*
data_hist[0]  --->   LAST_DATA_PACKET
data_hist[1]  --->   SECOND_LAST_DATA_PACKET
*/
struct DATA data_hist[2] = {0};

LIST_HEAD(TransferRecord_list, TransferRecord) TransferRecordList = LIST_HEAD_INITIALIZER(TransferRecordList);
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
        new_recv_data_link(remote_controller_addr.sin_addr.s_addr, fdaccept);


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
    if(_data->ttl>0) _data->ttl--;
}

bool data_recv_hook(int sock_index)
{
    printf("New data recieved!!!\n");
    int next_hop_fd = 0;
    struct DATA _data = {0};

    if(recvALL(sock_index, (char *)&_data, sizeof(struct DATA)) < 0){
        printf("Recieve failed, removing FD.\n");
        remove_data_conn(sock_index);
        return FALSE;
    }

    data_packet_update(&_data); // Decrementing TTL

    if(_data.ttl>0) // Drop it when TTL reaches 0
    {
        update_data_record(&_data);

        // Check if destination is self
        printf("Destination of this incoming data is %x\n", _data.dest_ip_addr);
        if(_data.dest_ip_addr == local_node_info->ip._ip)
        {
            printf("Saving data to local...\n");
            save_data(&_data);
        }
        else{
            // Forwarding incoming data to next hop... here
            printf("Forwarding incoming data to next hop...\n");
            next_hop_fd = get_next_hop(_data.dest_ip_addr);

            if(next_hop_fd>0){
                sendALL(next_hop_fd, (char *)&_data, sizeof(struct DATA));
                new_transfer_record(_data.transfer_id, _data.ttl, ntohs(_data.seq_num));
            }
        }
    }

    return TRUE;
}

void send_file(uint16_t payload_len,const char * cntrl_payload)
{

    uint16_t data_count = 0;
    int next_hop_fd = 0;
    struct CONTROL_SENDFILE header = {0};
    struct DATA _data_packet = {0};
    struct DATA _data_packet_to_send = {0}; // A delayed version to detect EOF

    char * file_name_ptr = (char *)(cntrl_payload + sizeof(struct CONTROL_SENDFILE));
    uint16_t file_name_len = payload_len - sizeof(struct CONTROL_SENDFILE);
    char * file_name = (char *)calloc(file_name_len, sizeof(char));
    char * file_data_payload = &_data_packet.payload[0];

    header = *((struct CONTROL_SENDFILE *)cntrl_payload);

    memcpy(file_name, file_name_ptr, file_name_len);

    // Proccess header information
    next_hop_fd = get_next_hop(header.dest_ip_addr);
    //next_hop_fd = get_next_hop_by_id(0x0100);
    printf("next_hop_fd %d\n", next_hop_fd);
    _data_packet.dest_ip_addr = header.dest_ip_addr;
    _data_packet.transfer_id = header.transfer_id;
    _data_packet.ttl = header.init_ttl;
    _data_packet.seq_num = header.init_seq;
    _data_packet.padding = 0;

    printf("The name of the file to send is: %s\n", file_name);

    FILE *fp;
    size_t read_s = 0;
    fp = fopen(file_name, "r");

    if ((fp) && (next_hop_fd>0)) { // Only send when dest is found and file is opened
        printf("File opened\n");
        read_s = fread(file_data_payload, 1, MAX_DATA_PAYLOAD, fp);

        while (read_s > 0){
            _data_packet_to_send = _data_packet;
            memset(file_data_payload, '\0', MAX_DATA_PAYLOAD);
            read_s = fread(file_data_payload, 1, MAX_DATA_PAYLOAD, fp);
            if(read_s > 0)
            {
                data_count++;
#ifdef DEBUG
                if(data_count<2) // Only print the ferst and the last
                {
                    printf("Data package No.%d is ready:\n", data_count);
                    payload_printer(32, (char *)&_data_packet_to_send);
                    printf("......\n");
                    printf("Sending......\n");
                }
#endif
                // _data_packet_to_send is not EOF
                sendALL(next_hop_fd, (char *)&_data_packet_to_send, sizeof(struct DATA));
                update_data_record(&_data_packet_to_send);
                new_transfer_record(_data_packet_to_send.transfer_id, _data_packet_to_send.ttl, ntohs(_data_packet_to_send.seq_num));
                _data_packet.seq_num = ntohs(ntohs(_data_packet.seq_num) + 1);
            }
            else{
                data_count++;
                _data_packet_to_send.padding = htonl(DATA_FIN_FLAG_MASK);
#ifdef DEBUG
                printf("Data package No.%d is ready, this is the last packet for this transfer:\n", data_count);
                payload_printer(32, (char *)&_data_packet_to_send);
                printf("......\n");
                printf("Sending......\n");
#endif
                sendALL(next_hop_fd, (char *)&_data_packet_to_send, sizeof(struct DATA));
                update_data_record(&_data_packet_to_send);
                new_transfer_record(_data_packet_to_send.transfer_id, _data_packet_to_send.ttl, ntohs(_data_packet_to_send.seq_num));
                break;
            }
            usleep(0);
        }

        fclose(fp);
    }
    else printf("Open File Failed\n");

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
        snprintf(&file_name_str[5], 3, "%x", _data->transfer_id);

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
    LIST_FOREACH(transfer_rec, &TransferRecordList, entries){
#ifdef DEBUG_TRANSFERRECORD
        printf("transfer_rec->transfer_id: = %x\n", transfer_rec->transfer_id);
#endif
        if(transfer_rec->transfer_id == _transfer_id) return (struct TransferRecord *)transfer_rec;
    }

    return (struct TransferRecord *)NULL;
}


void new_transfer(uint8_t _transfer_id)
{
    transfer_rec = NULL;
    transfer_rec = (struct TransferRecord *)malloc(sizeof(struct TransferRecord));
    transfer_rec->transfer_id = _transfer_id;
    transfer_rec->ttl = 0;
    transfer_rec->fin = FALSE;
    TAILQ_INIT(&transfer_rec->_seq);
    //transfer_rec->_seq.tqh_first = NULL;

    LIST_INSERT_HEAD(&TransferRecordList, transfer_rec, entries);
}

void new_transfer_record(uint8_t _transfer_id, uint8_t _ttl, uint16_t _seq_num)
{
#ifdef DEBUG_TRANSFERRECORD
    printf("New transfer record for ID: %x, TTL: %x, Seq: %x.\n", _transfer_id, _ttl, _seq_num);
#endif
    struct TransferRecord * temp = getExsitingTransfer(_transfer_id);
    if(!temp) // No record found, create a new one
    {
        printf("Transfer ID %x does not exist now, creating new entry!\n", _transfer_id);
        new_transfer(_transfer_id);

        temp = getExsitingTransfer(_transfer_id);
    }

    temp->ttl = _ttl;
    //struct SeqRecord temp_seq = {0};
    struct SeqRecord * temp_seq = (struct SeqRecord *)malloc(sizeof(struct SeqRecord));
    temp_seq->seq_num = _seq_num;

    // Problem with expanding TAILQ_INSERT_TAIL, manually do the expansion
    //printf("temp->_seq->tqh_last%s\n", temp->_seq.tqh_last);
    temp_seq->next.tqe_next = NULL;
    temp_seq->next.tqe_prev = temp->_seq.tqh_last;
    *(temp->_seq).tqh_last = (temp_seq);
    temp->_seq.tqh_last = &temp_seq->next.tqe_next;

    //TAILQ_INSERT_TAIL(temp->_seq, &temp_seq, next);
}

char * get_file_stats_payload(uint16_t _transfer_id)
{
    struct TransferRecord * _trecord = getExsitingTransfer(_transfer_id);

    if(!_trecord) return NULL;

    uint16_t seq_entry_num = 0;
    struct SeqRecord * seq_rec;

    // Find out how many seq_num record on file for this transfer id
    TAILQ_FOREACH(seq_rec, &_trecord->_seq, next)
        seq_entry_num++;

    //printf("seq_entry_num: %d\n", seq_entry_num);
    //printf("payload len: %d\n", (seq_entry_num*2 + 4), sizeof(uint8_t));

    char * file_stats_payload = calloc((seq_entry_num*2 + 4), sizeof(uint8_t));

    char * ptr = file_stats_payload;

    *ptr = _transfer_id;
    ptr++;

    *ptr = _trecord->ttl;
    ptr++;

    // Use the padding field to pass the payload length info to upper layer
    uint16_t len = (seq_entry_num*2 + 4);
    *(uint16_t *)ptr = (uint16_t)len;
    ptr+=2;

    TAILQ_FOREACH(seq_rec, &_trecord->_seq, next)
    {
        *(uint16_t *)ptr = htons((uint16_t)(seq_rec->seq_num));
        ptr+=2;
    }


    return file_stats_payload;
}



