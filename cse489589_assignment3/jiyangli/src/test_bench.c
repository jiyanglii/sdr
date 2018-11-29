#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/test_bench.h"
#include "../include/routing_alg.h"
#include "../include/control_header_lib.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h"
#include "../include/data_handler.h"

short test_init_payload[] = {       0x0002, 0x0002,
                                    0x0001, 3452,
                                    2344,   0002,
                                    0xA8C0,   0xB201,
                                    0x0002, 4562,
                                    2345,   0002,
                                    0xA8C0,   0xB201,
                                    0x0003, 8356,
                                    1635,   0002,
                                    0x80cd,   0x2421,
                                    0x0004, 4573,
                                    1678,   0002,
                                    0x80cd,   0x2423,
                                    0x0005, 3456,
                                    1946,   0002,
                                    0x80cd,   0x2424,
                            };


uint16_t init_set = FALSE;

void init_top()
{
    printf("Calling init_top()!!\n");
    if(!init_set){
        router_init((char *)&test_init_payload);
        init_set = TRUE;
    }
}