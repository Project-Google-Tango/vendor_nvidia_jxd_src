/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include <poll.h>
#include <sys/time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "nvos.h"
#include "nvassert.h"
#include "wm8731.h"

static int sampleRate = 44;
static int clockMode = 0;

static int ProcessArgs(int argc, char** argv)
{
    int idx, fdacc=0, fsample = 0;
    char c;

    if (argc < 2)
    {
        return 1;
    }

    for (idx = argc-1; idx ; idx--)
    {
        if (!strncmp(argv[idx],"--clk=", 6))
        {
            if (fdacc || sscanf(argv[idx],"--clk=%c",&c) <= 0
                || (c != 'm' && c != 's'))
                return 1;

            clockMode = c=='m'? 1 : 0;
        }
        else if (!strcmp(argv[idx],"--44k"))
        {
            if (fsample)
                return 1;

            sampleRate = 44;
            fsample = 1;
        }
        else if (!strcmp(argv[idx],"--48k"))
        {
            if (fsample)
                return 1;

            sampleRate = 48;
            fsample = 1;
        }
        else
            return 1;
    }

    return 0;
}

static void PrintUsage(char* arg0)
{
    printf("Usage:   %s [--clk=m | --clk=s] [--44k | --48k]\n", arg0);
    printf("Here,\n");
    printf("    --clk=m     : clock mode is master\n");
    printf("    --clk=s     : clock mode is slave\n");
    printf("    --44k       : I2S sample rate (default)\n");
    printf("    --48k       : I2S sample rate");
    printf("\n\n");
}

int main(int argc, char **argv)
{
    NvError err;

    if (ProcessArgs(argc, argv))
    {
        PrintUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    err = CodecConfigure(clockMode, sampleRate);

    if(err)
    {
        printf("Error : Couldnt Program WM8731 \n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

