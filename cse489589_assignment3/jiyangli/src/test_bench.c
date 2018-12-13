#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <arpa/inet.h>
#include <stdarg.h>

#include "../include/global.h"
#include "../include/test_bench.h"
#include "../include/routing_alg.h"
#include "../include/control_header_lib.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h"
#include "../include/data_handler.h"

int ret_print, ret_log;

uint16_t test_init_payload[] = {    0x0200, 0x0600,
                                    0x0100, 3452,
                                    8888,   0000,
                                    0x000A, 0x0F02,
                                    0x0200, 4562,
                                    2222,   0002,
                                    0x000A, 0x0F02,
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

uint16_t test_init_payload1[] = {   0x0200, 0x0600,
                                    0x0100, 3452,
                                    8888,   0002,
                                    0x000A, 0x0F02,
                                    0x0200, 4562,
                                    2223,   0000,
                                    0x000A, 0x0F02,
                                };

uint8_t test_send_file[] = {    0x0A, 0x00, 0x02, 0x0F, // dest ip
                                0x04,                   // init ttl
                                0x37,                   // transfer id
                                0x00, 0x18,             // init seq
                                0x74, 0x65, 0x73, 0x74, 0x66, 0x69, 0x6c, 0x65, 0x31};

uint16_t init_set = FALSE;

// short test_send_file[] = {  0x1201, 0x000A,
//                             0x050A, 0x1111,
//                             ,

// }

void init_top()
{
    printf("Calling init_top()!!\n");
    if(!init_set){
        router_init((char *)&test_init_payload);
        init_set = TRUE;
    }
}


void processCMD(int cmd)
{
    if(cmd == 0){
        init_top();
    }
    else if(cmd == 11){
        router_init((char *)&test_init_payload1);
    }
    else if(cmd == 2){
        routing_table_response(0,2);
    }
    else if(cmd == 4){
        crash(0,4);
    }
    else if(cmd == 5){
        send_file(sizeof(test_send_file), (char *)&test_send_file[0]);
    }
    else if(cmd == 6){
        char * _payload = get_file_stats_payload(0x37);
        uint16_t _payload_len = *((uint16_t *)(_payload + 2));
        payload_printer((_payload_len), (char *)_payload);
    }
    else if(cmd == 66){
        // Auto generate file stats
        // Create 3 transfer records
        //new_transfer_record(0xaa,0x55,0x0000);
        new_transfer_record(0xbb,0x66,0x1000);
        new_transfer_record(0xcc,0x77,0x2000);

        for(int i=0;i<20;i++)
        {
            // add 20 new records for each file
            new_transfer_record(0xaa,(0x55-i),(0x0000+i));
            new_transfer_record(0xbb,(0x66-i*2),(0x1000+i*2));
            new_transfer_record(0xcc,(0x77-i*3),(0x2000+i*3));
        }

        // Get filestats payload
        char trans = 0xbb;
        send_file_stats_response(0,6,&trans);

        char * _payload = get_file_stats_payload(0xbb);
        if(_payload) {
            uint16_t _payload_len = *((uint16_t *)(_payload + 2));
            payload_printer((_payload_len), (char *)_payload);
        }


        _payload = get_file_stats_payload(0xaa);
        if(_payload) {
            uint16_t _payload_len = *((uint16_t *)(_payload + 2));
            payload_printer((_payload_len), (char *)_payload);
        }


        _payload = get_file_stats_payload(0xcc);
        if(_payload) {
            uint16_t _payload_len = *((uint16_t *)(_payload + 2));
            payload_printer((_payload_len), (char *)_payload);
        }

    }
    else if(cmd == 9){
        send_update_table();

    }
    else if(cmd == 666){
        exit(0);
    }
    // if (cmd == 1)
    // {
    //     /* code */
    // }
}

void payload_printer(uint16_t payload_len, const char * payload)
{

    uint16_t ptr = 0;
    for(ptr=0; ptr<=(payload_len - 16); ptr+=16)
    {
        printf("%08x", ptr);
        printf("  ");
        printf("%02x %02x %02x %02x %02x %02x %02x %02x", (uint8_t)payload[ptr+0], (uint8_t)payload[ptr+1], (uint8_t)payload[ptr+2], (uint8_t)payload[ptr+3], (uint8_t)payload[ptr+4], (uint8_t)payload[ptr+5], (uint8_t)payload[ptr+6], (uint8_t)payload[ptr+7]);
        printf("  ");
        printf("%02x %02x %02x %02x %02x %02x %02x %02x", (uint8_t)payload[ptr+8], (uint8_t)payload[ptr+9], (uint8_t)payload[ptr+10], (uint8_t)payload[ptr+11], (uint8_t)payload[ptr+12], (uint8_t)payload[ptr+13], (uint8_t)payload[ptr+14], (uint8_t)payload[ptr+15]);
        printf("\n");
    }

    uint16_t _ptr = 0;
    if((payload_len - ptr) > 0){

        printf("%08x", ptr);
        printf("  ");

        for(;ptr<payload_len;ptr++){
            printf("%02x ", (uint8_t)payload[ptr]);
            _ptr++;
            if(_ptr==8) printf(" ");
        }
    }
    printf("\n");
}


void debug_print(const char* format, ...)
{
    va_list args_pointer;

    va_start(args_pointer, format);
    ret_print = vprintf(format, args_pointer);

    FILE* fp;
    if((fp = fopen("LOGFILE.txt", "a")) == NULL){
        ret_log = -100;
        va_end(args_pointer);
    }

    va_start(args_pointer, format);
    ret_log = vfprintf(fp, format, args_pointer);

    fclose(fp);
    va_end(args_pointer);
}


