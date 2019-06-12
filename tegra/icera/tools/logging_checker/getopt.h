/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

#ifndef GETOPT_H
#define GETOPT_H

extern "C" int optind, opterr;
extern "C" char * optarg;

int Getopt(int argc, char * argv[], const char * optstring);

#endif // GETOPT_H
