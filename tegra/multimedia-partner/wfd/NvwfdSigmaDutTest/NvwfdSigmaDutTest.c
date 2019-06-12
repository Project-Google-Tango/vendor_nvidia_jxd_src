/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <stdio.h>

extern  int wfa_dut_main(int argc, char *argv[]);
int main(void)
{
     char *argv[4];
     int argc = sizeof(argv)/sizeof(argv[0]) - 1;
     int rc;
     argv[0] = "wfa_dut_thread";
     argv[1] = "lo";
     argv[2] = "6565";
     argv[3] = NULL;

     rc= wfa_dut_main(argc, argv);

     return rc;
}

