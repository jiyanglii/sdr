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

#include "../include/global.h"
#include "../include/connection_manager.h"

#define TEST

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
#ifndef TEST
int main(int argc, char **argv)
{
    /*Start Here*/

    sscanf(argv[1], "%" SCNu16, &CONTROL_PORT);
    init(); // Initialize connection manager; This will block

    return 0;
}
#endif

#ifdef TEST
#include "../include/routing_alg.h"

int main(int argc, char **argv)
{
    /*Start Here*/

    short test_init_payload[] = {   0x0005, 0x0002,
                                    0x0001, 3452,
                                    2344,   0002,
                                    0x80cd,   0x2422,
                                    0x0002, 4562,
                                    2345,   0002,
                                    0x80cd,   0x242e,
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
    router_init((char *)&test_init_payload);

    return 0;
}
#endif
