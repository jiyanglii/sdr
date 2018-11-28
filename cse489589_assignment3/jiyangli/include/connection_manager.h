#pragma once

#include <netinet/in.h>
#include <arpa/inet.h>

fd_set master_list, watch_list;

int control_socket, router_socket, data_socket;

void init();
void main_loop();

void routing_sock_init(uint16_t router_sock_num, uint16_t data_sock_num);
void refresh_data_links();