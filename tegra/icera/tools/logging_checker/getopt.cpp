/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**** INCLUDES *********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "getopt.h"

/**** DATA *************************************************************************************/
char * optarg;		   // global argument pointer
int    optind = 0; 	// global argv index

/**** FUNCTIONS ********************************************************************************/
int Getopt(int argc, char * argv[], const char * optstring)
{
    static char * next = NULL;
    if (optind == 0)
        next = NULL;

    optarg = NULL;

    if (next == NULL || *next == '\0')
    {
        if (optind == 0)
            optind++;

        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
        {
            optarg = NULL;
            if (optind < argc)
                optarg = argv[optind];
            return EOF;
        }

        if (strcmp(argv[optind], "--") == 0)
        {
            optind++;
            optarg = NULL;
            if (optind < argc)
                optarg = argv[optind];
            return EOF;
        }

        next = argv[optind];
        next++;		// skip past -
        optind++;
    }

    char c = *next++;
    const char * cp = strchr(optstring, c);

    if (cp == NULL || c == ':')
        return ('?');

    cp++;
    if (*cp == ':')
    {
        if (*next != '\0')
        {
            optarg = next;
            next = NULL;
        }
        else if ((optind < argc) && (argv[optind][0] != '-'))
        {
            optarg = argv[optind];
            optind++;
        }
        else if (*(cp + 1) == ':')
        {
            // Missing optional argument
            optarg = NULL;
        }
        else
        {
            return '?';
        }
    }

    return c;
}
