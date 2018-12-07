#pragma once

#define CMD_SIZE  256

#define printf debug_print

void init_top();
void processCMD(int cmd);


void payload_printer(uint16_t payload_len, const char * payload);
void debug_print(const char* format, ...);

