#pragma once

int create_data_sock(uint16_t data_sock_num);
int new_data_conn(int sock_index);
bool isData(int sock_index);
bool data_recv_hook(int sock_index);
int new_data_conn_client(int router_ip, int router_data_port);