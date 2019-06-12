/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvapputil.h"
#include "nvassert.h"

NvError NvAuTokenize(const char *str, char delim, NvU32 *num, char ***tokens)
{
    char *s;
    char *start;
    char *end;
    char *copy = 0;
    NvU32 len = 0;
    NvU32 index;

    NV_ASSERT(str);
    NV_ASSERT(num);
    NV_ASSERT(tokens);    

    len = NvOsStrlen(str);
    if (!len)
    {
        goto notoken;
    }

    s = (char *)str;

    /* the tokens will point into single allocation */
    copy = NvOsAlloc(sizeof(char) * len +  1);
    if (!copy)
    {
        return NvError_InsufficientMemory;
    }

    /* chomp leading delimiters */
    while (*s && *s == delim)
    {
        s++;
    }

    if (!(*s))
    {
        goto notoken;
    }

    start = s;

    /* find last char that's not a delimiter */
    end = (char *)str + len - 1;
    while (end > start && *end == delim)
    {
        end--;
    }

    /* only copy the interesting bits */
    len = (end - start) + 1;
    NvOsStrncpy(copy, start, len);
    copy[ len ] = 0;

    /* count tokens */
    len = 0;
    while (s <= end)
    {
        while (s <= end && *s == delim)
        {
            s++;
        }

        len++;

        while (s <= end && *s != delim)
        {
            s++;
        }
    }

    s = copy;
    if (!*s)
        goto notoken;

    len++; // null terminate
    *num = len - 1;
    *tokens = NvOsAlloc(len * sizeof(char *));
    if (!(*tokens))
    {
        goto notoken;
    }

    (*tokens)[len - 1] = 0;

    index = 0;
    while (*s)
    {
        (*tokens)[index] = s;
        index++;

        while (*s && *s != delim)
        {
            s++;
        }

        while (*s && *s == delim)
        {
            *s = 0;
            s++;
        }
    }

    return NvSuccess;

notoken:
    NvOsFree(copy);
    *num = 0;
    *tokens = 0;
    return NvSuccess;
}

void NvAuTokenFree(NvU32 num, char **tokens)
{
    if (!tokens || !(*tokens))
    {
        return;
    }

    NvOsFree(*tokens);
    NvOsFree(tokens);
}
