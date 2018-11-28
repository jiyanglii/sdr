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

#include <strings.h>
#include <sys/select.h>

#include "../include/connection_manager.h"
#include "../include/global.h"
#include "../include/control_handler.h"
#include "../include/data_handler.h"
#include "../include/routing_alg.h"
#include <netinet/in.h>
#include <arpa/inet.h>

fd_set master_list, watch_list;
int control_socket, router_socket, data_socket;
int head_fd;

void main_loop()
{
    int selret, sock_index, fdaccept;

    while(TRUE){
        watch_list = master_list;
        selret = select(head_fd+1, &watch_list, NULL, NULL, NULL);

        if(selret < 0)
            ERROR("select failed.");

        /* Loop through file descriptors to check which ones are ready */
        for(sock_index=0; sock_index<=head_fd; sock_index+=1){

            if(FD_ISSET(sock_index, &watch_list)){

                /* control_socket */
                if(sock_index == control_socket){
                    fdaccept = new_control_conn(sock_index);

                    /* Add to watched socket list */
                    FD_SET(fdaccept, &master_list);
                    if(fdaccept > head_fd) head_fd = fdaccept;
                }

                /* router_socket */
                else if(sock_index == router_socket){
                    //call handler that will call recvfrom() .....
                }

                /* data_socket */
                else if(sock_index == data_socket){
                    fdaccept = new_data_conn(sock_index);

                    /* Add to watched socket list */
                    FD_SET(fdaccept, &master_list);
                    if(fdaccept > head_fd) head_fd = fdaccept;
                }

                /* Existing connection */
                else{
                    if(isControl(sock_index)){
                        if(!control_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);
                    }
                    else if (isData(sock_index)){
                        if(!data_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);
                    }
                    else ERROR("Unknown socket index");
                }
            }
        }
    }
}

void init()
{
    control_socket = create_control_sock();

    //router_socket and data_socket will be initialized after INIT from controller

    FD_ZERO(&master_list);
    FD_ZERO(&watch_list);

    /* Register the control socket */
    FD_SET(control_socket, &master_list);
    head_fd = control_socket;

    main_loop();
}

void static router_sock_init(uint16_t router_sock_num)
{
    // create router and data sock

    // router socket
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        ERROR("socket() failed");
    }
    else{
        router_socket = sock;
    }

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(router_sock_num);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    /* Add to watched socket list */
    FD_SET(router_socket, &master_list);
    if(router_socket > head_fd) head_fd = router_socket;

}


void static data_sock_init(uint16_t data_sock_num)
{
    // create data sock

    // data socket
    data_socket = create_data_sock(data_sock_num);

    /* Add to watched socket list */
    FD_SET(data_socket, &master_list);
    if(data_socket > head_fd) head_fd = data_socket;

}

void routing_sock_init(uint16_t router_sock_num, uint16_t data_sock_num)
{
    router_sock_init(router_sock_num);
    data_sock_init(data_sock_num);
}

void refresh_data_links()
{
    // This function refresh all data TCP links with all neibouring routers,
    // It attampts to establish a new link when the link_status is NOT active
    int data_socket = -1;

    for(int i=0;i<active_node_num;i++){
        if((node_table[i].link_status == FALSE) && (node_table[i].self == FALSE)){
            // Create new data link
            data_socket = new_data_conn_client(node_table[i].ip._ip, node_table[i].raw_data.router_data_port);
            if(data_socket < 0){
                // Link creation failed
                node_table[i].link_status = FALSE;
                node_table[i].fd = 0;
            }
            else
            {
                // On success, update DATA_LINK list and node_info list
                node_table[i].link_status = TRUE;
                node_table[i].fd = data_socket;
                /* Add to watched socket list */
                FD_SET(data_socket, &master_list);
                if(data_socket > head_fd) head_fd = data_socket;
            }
        }
        //else do nothing
    }
}
