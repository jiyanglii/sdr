#pragma once

#include <netinet/in.h>
#include <arpa/inet.h>

extern fd_set master_list, watch_list;

extern int control_socket, router_socket, data_socket;

void init();
void main_loop();