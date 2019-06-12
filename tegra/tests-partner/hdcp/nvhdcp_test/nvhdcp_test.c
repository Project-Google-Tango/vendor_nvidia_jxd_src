/*
 * Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

// Program to test HDCP transitions with HDCP test tool. Results are self-
// checking. Could be used as a tool for detecting regressions associated
// with HDCP display DRM.
//

#include <stdio.h>
#include <unistd.h>
#include "hdcp_up.h"

static void usage(const char *progName)
{
    printf("Usage: hdcp_test <cycle_num>\n");
    printf("Note: Target must be connected to an HDMI monitor.\n");
}

char * hdcp_status_str[] = {
    "HDCP_RET_SUCCESS",
    "HDCP_RET_UNSUCCESSFUL",
    "HDCP_RET_INVALIDE_PARAMETER",
    "HDCP_RET_NO_MEMORY",
    "HDCP_RET_STATUSR_FAILED",
    "HDCP_RET_READM_FAILED",
    "HDCP_RET_LINK_PENDING"
};

int main(int argc, char **argv)
{
    HDCP_CLIENT_HANDLE hClient;
    int cycles;
    unsigned int delay_sec = 3;
    HDCP_RET_ERROR ok;

    if (argc <= 1)
    {
        usage(argv[0]);
        return -1;
    }

    sscanf(argv[1], "%d", &cycles);
    printf("starting nvhdcp_test %d\n", cycles);

    ok = hdcp_open(&hClient, 1);
    if (HDCP_RET_SUCCESS != ok)
    {
        printf("Failed to open HDCP target\n");
        return -1;
    }

    while (cycles--)
    {
        ok = hdcp_status(hClient);
        printf("hdcp_status return %s\n", hdcp_status_str[ok]);

        sleep(delay_sec);
    }

    hdcp_close(hClient);

    return 0;
}

