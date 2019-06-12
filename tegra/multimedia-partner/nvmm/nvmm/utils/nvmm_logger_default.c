/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved. 
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_logger.h"
#include "nvmm_logger_internal.h"

#if NV_IS_AVP
static int
IntegerToString (unsigned n, int sign, int pad, char* s, size_t size, unsigned radix)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int len = 0;
    int total;
    unsigned x = n;

    // calculate length
    do
    {
        x /= radix;
        len++;
    } while (x > 0);    

    if (sign < 0)
        len++;

    if (len > pad)
    {
        total = len;
        pad = 0;
    }
    else
    {
        total = pad;
        pad -= len;
    }

    // doesn't fit, just bail out
    if (total > size) return -1;

    // write sign
    if (sign < 0)
    {
        *(s++) = '-';
        len--;
    }
    
    // pad
    while (pad--)
        *(s++) = '0';

    // write integer
    x = n;
    while (len--)
    {
        s[len] = digits[x % radix];
        x /= radix;
    }
    
    return total;
}

static int NvAosVsnprintf(
    char* buffer,
    size_t size,
    const char* format,
    va_list ap)
{
    const char* f = format;
    char* out = buffer;
    size_t remaining = size - 1;
    
    while (remaining > 0)
    {
        int wrote = 0;
        char cur = *(f++);

        // end of format
        
        if (cur == '\0')
            break;

        // formatted argument
        
        if (cur == '%')
        {
            const char* arg = f;
            int pad = 0;

            // support for %0N zero-padding
            
            if (*arg == '0')
            {
                arg++;
                
                while ((*arg >= '0') && (*arg <= '9'))
                {
                    pad *= 10;
                    pad += (*(arg++)) - '0';
                }
            }
            
            switch (*(arg++))
            {
            case 'd':
            case 'D':
                {
                    int val = va_arg(ap, int);
                    int sign = 1;

                    if (val < 0)
                    {
                        sign = -1;
                        val = -val;
                    }
                    
                    wrote = IntegerToString((unsigned)val, sign, pad, out, remaining, 10);
                }
                break;
            case 'u':
            case 'U':
                {
                    unsigned val = va_arg(ap, unsigned);
                    wrote = IntegerToString(val, 1, pad, out, remaining, 10);
                }
                break;
            case 'p':
            case 'x':
            case 'X':
                {
                    unsigned val = va_arg(ap, unsigned);
                    wrote = IntegerToString(val, 1, pad, out, remaining, 16);
                }
                break;
            case 's':
            case 'S':
                {
                    char* val = va_arg(ap, char *);
                    int len = NvOsStrlen(val);

                    if (len > remaining)
                    {
                        wrote = -1;
                    }
                    else
                    {                                            
                        NvOsStrncpy(out, val, len);
                        wrote = len;
                    }
                }
                break;
            case 'c':
            case 'C':
                {
                    char val = (char)va_arg(ap, unsigned);

                    *out = val;
                    wrote = 1;
                }
                break;
            default:
                // unsupported -- print as is
                (void)va_arg(ap, unsigned);
                break;
            }

            if (wrote > 0)
                f = arg;      
        }

        if (wrote == -1)
        {
            break;
        }

        if (wrote == 0)
        {
            *out = cur;
            wrote = 1;
        }
        
        remaining -= wrote;
        out += wrote;
    }

    *out = '\0';
    return (size - 1) - remaining;
}

#endif

static char* LoggerGetLevelString(NvLoggerLevel Level)
{
    switch(Level)
    {
        case NVLOG_VERBOSE:
            return "V/";
        case NVLOG_DEBUG:
            return "D/";
        case NVLOG_INFO:
            return "I/";
        case NVLOG_WARN:
            return "W/";
        case NVLOG_ERROR:
            return "E/";
        case NVLOG_FATAL:
            return "F/";
        default:
            return "/";      
    }
}

void NvLoggerPrintf(NvLoggerLevel Level, const char* pClientTag, const char* pFormat, va_list ap)
{
    char OutputString[128]; /* we can only support 128 char string prints using NvOs */
    NvS32 OutputLen=0;

    /* generate the output string from the format */
    OutputLen = NvOsSnprintf(OutputString, sizeof(OutputString), 
                    "%s%s :: ", LoggerGetLevelString(Level), pClientTag);

    if (OutputLen > -1 && OutputLen < sizeof(OutputString)-1)
    {
#if !NV_IS_AVP    
        OutputLen += NvOsVsnprintf(OutputString + OutputLen, sizeof(OutputString)-OutputLen, 
                            pFormat, ap);   
#else // TODO: FIX ME. There is no NvOsVsnprintf for AOS platform
        OutputLen += NvAosVsnprintf(OutputString + OutputLen, sizeof(OutputString)-OutputLen, 
                            pFormat, ap);     
#endif
    }

    /* print the output string */
    NvOsDebugPrintf(OutputString);        
}

