/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * File Name: nv_hciattach.c
 * Description: Implements a helper deamon that is useful to configure 
 *    Bluetooth Chip on platforms based on nVidia Tegra Chips.
 *    - Configures CSR Bluetooth Chips using the bluecore6.psr file.
 *    - If requires, generates a BTADDR, in case platforms does not have one.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#ifndef BLUETOOTH_BCCMD_MODULE_PATH
#define BLUETOOTH_BCCMD_MODULE_PATH        "/system/bin/bccmd"
#endif

#ifndef BLUETOOTH_HCIATTACH_MODULE_PATH
#define BLUETOOTH_HCIATTACH_MODULE_PATH    "/system/bin/hciattach"
#endif

static void usage(void)
{
    printf("nv_hciattach - nVidia Bluetooth HCI Driver \
                                  initialization utility\n");
    printf("Usage:\n");
    printf("\tnv_hciattach [-n] [-p] [-b] [-t timeout] [-s initial_speed] \
                       <tty> <type | id> [speed] [flow|noflow] [bdaddr]\n");
}

//===========================================================================
// main() - Deamon program entry point
//===========================================================================
int main( int argc, char *argv[])
{
    pid_t pid = 0,w;
    int status = 0, opt, n;
    char *dev = NULL;

    printf(" Starting nv_hciattach deamon to configure BT Chip\n");

    // Parse the command arguments and get the important parameters.
    while ((opt=getopt(argc, argv, "bnpt:s:")) != EOF) {
        switch(opt) {
           case 'b':
           case 'n':
           case 'p':
           case 't':
           case 's':
                 break;
           default:
                 usage();
                 exit(1);
        }
    }

    n = argc - optind;
    if (n < 2) {
           usage();
           exit(1);
    }

    for (n = 0; optind < argc; n++, optind++) {
        char *opt;
        opt = argv[optind];

        switch(n) {
           case 0:
                 dev = (char *)malloc(strlen(opt)+10);
                 if(dev == NULL){
                     printf("nv_hciatatch:Error in memory allocation");
                     return -1;
                 }
                 dev[0] = 0;
                 if (!strchr(opt, '/'))
                     strcpy(dev, "/dev/");
                 strcat(dev, opt);
                 break;

            case 4:
                  break;
        }
    }

    if(dev == NULL) {
       printf("nv_hciattach: tty device not specified");
       return -1;
    }

    pid = fork();
    if(pid == 0) {
        printf("Using bccmd to configure the CSR Chip\n");
        // Forming BCCMD Utility Arguments
        char *bccmd_argv[15] = {NULL};
        int i=0;
        bccmd_argv[i++]=(char *)BLUETOOTH_BCCMD_MODULE_PATH;
        bccmd_argv[i++]=(char *)"-t";
        bccmd_argv[i++]=(char *)"bcsp";
        bccmd_argv[i++]=(char *)"-d";
        bccmd_argv[i++]=(char *)dev;
        bccmd_argv[i++]=(char *)"psload";
        bccmd_argv[i++]=(char *)"-r";
        bccmd_argv[i++]="/etc/bluez/bluecore6.psr";
        bccmd_argv[i++]=NULL;

        printf("BCCMD Command:");
        for(i=0; bccmd_argv[i]!=NULL; i++)
           printf("%s ", bccmd_argv[i]);
        printf("\n");

        if(execv(BLUETOOTH_BCCMD_MODULE_PATH, bccmd_argv) < 0) {
                printf("cannot execve('%s'): %s\n", 
                             BLUETOOTH_BCCMD_MODULE_PATH, strerror(errno));
        }
        free(dev);
        _exit(127);
    }
    else if(pid < 0) {
        free(dev);
        printf("fork() with error:%d(%s) %d\n",errno,strerror(errno),pid);
        return -1;
    }
    else {  
        do{
         w = waitpid(pid, &status, WUNTRACED);
         if (w == -1) {
                printf("waitpid");
                free(dev);
                return -1;
         }
         if (WIFEXITED(status)) {
                printf("exited, status=%d\n", WEXITSTATUS(status));
         } else if (WIFSIGNALED(status)) {
                printf("killed by signal %d\n", WTERMSIG(status));
         } else if (WIFSTOPPED(status)) {
                printf("stopped by signal %d\n", WSTOPSIG(status));
         //} else if (WIFCONTINUED(status)) {
         //       printf("continued\n");
         }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    printf("Starting Bluetooth hciattach with arguments : \n");

    // Forming hciattach Arguments.
    char *hciattach_argv[15]={NULL};
    int j=0;
    hciattach_argv[j++] = BLUETOOTH_HCIATTACH_MODULE_PATH;
    for(;j<argc; j++)
      hciattach_argv[j] = argv[j];

    //TODO: if btaddress is not provided by platform. generate a random 
    //address and pass it to hciattach.
    
    // Terminate a string with NULL.
    hciattach_argv[j++] = NULL;

    printf("hciattach command: ");
    for(j=0; hciattach_argv[j]!=NULL; j++)
           printf("%s  ", hciattach_argv[j]);
    printf("\n");

    if (execv(BLUETOOTH_HCIATTACH_MODULE_PATH, hciattach_argv) < 0) {
          printf("cannot execve('%s'): %s\n", 
                  BLUETOOTH_HCIATTACH_MODULE_PATH, strerror(errno));
    }

    free(dev);

    printf("Bluetooth HCI Driver initialisation Done.\n");

    return 0;
}

