/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvapputil.h"
#include "nvutil.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvAuGetOpt() - 
//===========================================================================
// Similar to getopt
//
NvError NvAuGetOpt(
            int    argc,
            char **argv,
            const char *optstring,
            int *index,
            int *returned_option,
            char **returned_value,
            int print_error)
{
    char *p;

    *returned_option = 0;
    *returned_value  = 0;

    if (*index == 0)
        (*index)++;

    if (*index >= argc)
        return NvSuccess;

    //
    // Does not start with - 
    //
    p = argv[*index];
    (*index)++;
    if (p[0] != '-') {
        *returned_value = p;
        (*index)++;
        return NvSuccess;
    }

    //
    // Does start with -
    //
    while(1) {
        if (!*optstring) {
            *returned_value = p;
            if (print_error)
                NV_DEBUG_PRINTF(("Error: Unrecognized option: '%s'\n",p));
            return NvError_TestCommandLineError;
        }
        if (p[1] == *optstring)
            break;
        optstring++;
    }

    if (p[1] == ':') {
        if (print_error)
            NV_DEBUG_PRINTF(("Error: Bad option: '%s'\n",p));
        return NvError_TestCommandLineError;
    }

    *returned_option = p[1];

    if (optstring[1] != ':') {
        if (p[2]) {
            if (print_error)
                NV_DEBUG_PRINTF(("Error: Garbage follows option: '%s'\n",p));
            return NvError_TestCommandLineError;
        }
        return NvSuccess;
    }

    if (p[2]) {
        *returned_value = p+2;
        return NvSuccess;
    }

    if (*index >= argc) {
        if (print_error)
            NV_DEBUG_PRINTF(("Error: Option requires argument: '%s'\n",p));
        return NvError_TestCommandLineError;
    }

    *returned_value  = argv[*index];
    (*index)++;
    
    return NvSuccess;
}

//===========================================================================
// NvAuGetArgValU32() - 
//===========================================================================
NvError NvAuGetArgValU32(const char *s, NvU32 *returned_val, int print_error)
{
    char *e;
    if (!s) {
        if (print_error)
            NV_DEBUG_PRINTF(("Error: Null argument\n"));
        return NvError_TestCommandLineError;
    }

    *returned_val = NvUStrtoul(s, &e, 0);
    if (!e || e == s || *e) {
        if (print_error)
            NV_DEBUG_PRINTF(("Error: Argument is not an integer: '%s'\n",s));
        return NvError_TestCommandLineError;
    }
    return NvSuccess;
}

//===========================================================================
// NvAuGetArgValS32() - 
//===========================================================================
NvError NvAuGetArgValS32(const char *s, NvS32 *returned_val, int print_error)
{
    int neg = 0;
    NvU32 val;

    // get sign
    if (s) {
        if (*s == '-') {
            neg = 1;
            s++;
        } else if (*s == '+') {
            s++;
        }
    }

    if (NvAuGetArgValU32(s, &val, print_error))
        return NvError_TestCommandLineError;

    *returned_val = neg ? -(NvS32)val : (NvS32)val;
    return NvSuccess;
}

#if NV_DEF_USE_FLOAT
//===========================================================================
// NvAuStrtod() - 
//===========================================================================
double NvAuStrtod(const char *s, char **e)
{
    int neg = 0;
    NvU32 dig;
    NvU32 cnt  = 0;
    NvU32 val  = 0;
    NvU32 frac = 0;
    NvU32 div  = 1;
    double d;

    if (e)
        *e = (char*)s;
    
    if (!s) 
        return 0.0;

    // get sign
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // get integer part
    while(1) {
        dig = *(s++);
        if (dig == '.')
            break;
        dig -= '0';
        if (dig > 9) {
            if (!cnt)
                return 0.0;
            if (e)
                *e = (char*)s-1;
            return neg ? -(double)val : (double)val;
        }
        val *= 10;
        val += dig;
        cnt++;
    }

    // get fractional part
    while(1) {
        dig = *(s++);
        dig -= '0';
        if (dig > 9)
            break;
        div  *= 10;
        frac *= 10;
        frac += dig;
        cnt++;
    }

    if (!cnt)
        return 0.0;

    if (e)
        *e = (char*)s-1;

    d = (double)frac/(double)div;
    d += (double)val;
    return neg ? -d : d;
}
#endif

//===========================================================================
// NvAuGetArgValF32() - 
//===========================================================================
NvError NvAuGetArgValF32(const char *s, float *returned_val, int print_error)
{
#if NV_DEF_USE_FLOAT
    char *e;
    
    if (!s) {
        if (print_error)
            NV_DEBUG_PRINTF(("Error: Null argument\n"));
        return NvError_TestCommandLineError;
    }

    *returned_val = (float)NvAuStrtod(s, &e);

    if (!e || e==s || *e) {
        if (print_error)
            NV_DEBUG_PRINTF(("Error: Argument is not a valid float: '%s'\n",s));
        return NvError_TestCommandLineError;
    }
    return NvSuccess;
#else
    return NvError_NotSupported;
#endif
}
