/*
 * Copyright (c) 2010-2012, NVIDIA CORPORATION.  All rights reserved.
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
#include "codec.h"
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "nvos.h"
#include "nvassert.h"

/**
 * Global I2s/Tdm mode
 */
static int fd;
#define I2C_DEVICE "/dev/i2c-0"

static CodecMode g_dacController = CodecMode_None;
static int sampleRate = 44;

static int ProcessArgs(int argc, char** argv)
{
    int i, idx, fdacc=0, fsample = 0;

    if (argc < 2)
    {
        return 1;
    }

    for (idx = argc-1; idx ; idx--)
    {
        if (!strncmp(argv[idx],"--i2s=", 6))
        {
            if (fdacc || sscanf(argv[idx],"--i2s=%d",&i) <= 0
                || (i != 1 && i != 2))
                return 1;

            g_dacController = i==1?CodecMode_I2s1:CodecMode_I2s2;
            fdacc = 1;
        }
        else if (!strncmp(argv[idx],"--tdm=", 6))
        {
            if (fdacc || sscanf(argv[idx],"--tdm=%d",&i) <= 0
                || (i != 1 && i != 2))
                return 1;

            g_dacController = i==1?CodecMode_Tdm1:CodecMode_Tdm2;
            fdacc = 1;
        }
        else if (!strcmp(argv[idx],"--tdm"))
        {
            if (fdacc)
                return 1;

            g_dacController = CodecMode_Tdm1;
            fdacc = 1;
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
    return CodecMode_None == g_dacController;
}

static void PrintUsage(char* arg0)
{
    printf("Usage:   %s --i2s=N|--tdm[=N] [--44k | --48k]\n", arg0);
    printf("Here,\n");
    printf("    --i2s=N     : initialize I2S controller N, where N is 1 (slave) or 2(master)\n");
    printf("    --tdm[=N]   : initialize TDM controller N, where N is 1 (default) or 2.\n");
    printf("    --44k       : I2S sample rate (default)\n");
    printf("    --48k       : I2S sample rate");
    printf("\n\n");
}

int main(int argc, char **argv)
{
    NvError err;
    unsigned int sku_id = 0;

    if (ProcessArgs(argc, argv))
    {
        PrintUsage(argv[0]);
        exit(EXIT_FAILURE);
    }


    printf("Using %s controller with sample rate %d\n",
           (g_dacController == CodecMode_I2s1 ? "I2S-1" :
            (g_dacController == CodecMode_I2s2 ? "I2S-2":
             (g_dacController == CodecMode_Tdm1 ? "TDM-1": "TDM-2"))),
           sampleRate);

    fd = open(I2C_DEVICE, O_RDWR);
    if (fd < 0)
    {
        printf("Not able to open %s \n",I2C_DEVICE);
        return -1;
    }

    err = ConfigureBaseBoard(fd, g_dacController, &sku_id);
    if(err)
    {
        printf("Error : Couldnt Configure BaseBoard \n");
        exit(EXIT_FAILURE);
    }

    // Open Codec
    err = CodecOpen(fd, g_dacController, sampleRate, sku_id);
    if(err)
    {
        printf("Error : Couldnt Program ADV1937 \n");
        exit(EXIT_FAILURE);
    }
    close(fd);

    return 0;
}

