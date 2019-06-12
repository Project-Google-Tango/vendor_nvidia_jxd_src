/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/sysmacros.h>

/* Specifies the correct usage of the mknod tool */
static void helper(void)
{
    printf("Usage: mknod [OPTION]... NAME TYPE [MAJOR MINOR]\nCreate the special file NAME of the given TYPE.\
            \n-m, --mode=MODE    set file permission bits to MODE, not a=rw - umask\
            \n-h, --help         shows the usage\
            \n\nBoth MAJOR and MINOR must be specified when TYPE is b, c, and they\
            \nmust be omitted when TYPE is p.  If MAJOR or MINOR begins with 0x or 0X,\
            \nit is interpreted as hexadecimal; otherwise, if it begins with 0, as octal;\
            \notherwise, as decimal.  TYPE may be:\
            \n\nb      create a block (buffered) special file\
            \nc      create a character (unbuffered) special file\
            \np      create a FIFO\n");
}

int main(int argc, char *argv[])
{
    int opt, flag = 0, major = 0, minor = 0, mode = 0666;
    char *path = NULL;
    char *end = NULL;

    /* long_opts array specifies the flag parameters that can be
     * passed to the mknod tool. User can add more
     * parameters to improve the functionality of the
     * tool
     */
    const struct option long_opts[] = {
        {"help", no_argument, NULL, 'h'},
        {"mode", required_argument, NULL, 'm'},
        {NULL, 0, NULL, 0}
    };

    /* Must have at least two arguments */
    if (2 > argc) {
        fprintf(stderr, "mknod: Missing operand");
        goto failed;
    }

    while (-1 != (opt = getopt_long(argc, argv, "m:h", long_opts, NULL))) {
        switch (opt) {
            case 'm':
                mode = atoi(optarg);
                break;
            case 'h':
                helper();
                return 0;
            case '?':
                goto failed;
                return -1;
            default:
                break;
        }
    }

    if (optind == argc) {
        goto failed;
    }

    path = argv[optind++];

    if (!strcmp(argv[optind], "p")) {
        mode |= S_IFIFO;
    } else if(!strcmp(argv[optind], "c")) {
        mode |= S_IFCHR;
        flag = 1;
    } else if(!strcmp(argv[optind], "b")) {
        mode |= S_IFBLK;
        flag = 1;
    } else {
        fprintf(stderr, "mknod: INVALID FILE TYPES");
        goto failed;
    }
    /* Must specify MAJOR/MINOR numbers for block and char file types.
     * For FIFO file type these numbers are ignored
     */
    if ( 1 == flag) {
        if (2 != (argc - optind - 1)) {
            fprintf(stderr, "mknod: Missing MAJOR/MINOR operands");
            goto failed;
        }
        major = strtol(argv[++optind], &end, 0);
        if (*end) {
            fprintf(stderr, "mknod: INVALID MAJOR operands");
            goto failed;
        }
        minor = strtol(argv[++optind], &end, 0);
        if (*end) {
            fprintf(stderr, "mknod: INVALID MINOR operands");
            goto failed;
        }
    }
    if (mknod(path, mode, makedev(major, minor))) {
        fprintf(stderr, "mknod failed %s", strerror(errno));
        goto failed;
    }
    return 0;

failed:
    fprintf(stderr, "\nTry mknod --help\n");
    return -1;
}
