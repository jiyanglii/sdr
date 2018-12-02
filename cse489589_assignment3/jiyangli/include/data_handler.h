#pragma once

#define DATA_HEADER_SIZE    12 // including the padding
#define MAX_DATA_PAYLOAD    1024

#define DATA_FIN_FLAG_MASK  0x10000000

struct __attribute__((__packed__)) DATA
{
    uint32_t dest_ip_addr;
    uint8_t transfer_id;
    uint8_t ttl;
    uint16_t seq_num;
    uint32_t padding;
    char payload[MAX_DATA_PAYLOAD];
};

int create_data_sock(uint16_t data_sock_num);
int new_data_conn(int sock_index);
bool isData(int sock_index);
bool data_recv_hook(int sock_index);
int new_data_conn_client(int router_ip, int router_data_port);
void send_file(uint16_t payload_len, char * cntrl_payload);
void update_data_record(struct DATA * _data);


extern struct DATA data_hist[2];

