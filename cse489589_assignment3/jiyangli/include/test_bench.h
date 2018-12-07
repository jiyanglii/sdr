#pragma once

#define CMD_SIZE  256

void init_top();
void processCMD(int cmd);


void payload_printer(uint16_t payload_len, const char * payload);

