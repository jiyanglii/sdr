#pragma once

#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_TIMEOUT_CT    3

fd_set master_list, watch_list;

int control_socket, router_socket, data_socket;

void init();
void main_loop();

void routing_sock_init(uint16_t router_sock_num, uint16_t data_sock_num);
void refresh_data_links();
void udp_router_update(char * payload, uint16_t payload_len);
void udp_router_update_recv(int udp_fd);
void timer_handler();
void timer_timeout_handler();


