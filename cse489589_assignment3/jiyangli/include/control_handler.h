#pragma once

int create_control_sock();
int new_control_conn(int sock_index);
bool isControl(int sock_index);
bool control_recv_hook(int sock_index);
void send_file_resp(int sock_index);
void send_prev_data(int sock_index, uint8_t _control_code);

