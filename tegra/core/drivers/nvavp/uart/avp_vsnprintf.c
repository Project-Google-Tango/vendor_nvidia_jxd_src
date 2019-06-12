/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/* Very simple vsnprintf implementation for situations where no other
   implementation is available. This implementation only supports enough for
   the fastboot bootloader to work, do not blindly rely on this! */

#include "avp_uart.h"

static int
IntegerToString (unsigned n, int sign, size_t pad, char* s, size_t size,
                     unsigned radix, char padchar)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    size_t len = 0;
    size_t total;
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
        *(s++) = padchar;

    // write integer
    x = n;
    while (len--)
    {
        s[len] = digits[x % radix];
        x /= radix;
    }

    return total;
}

int NvSimpleVsnprintf(
    char* buffer,
    size_t size,
    const char* format,
    va_list ap)
{
    const char* f = format;
    char* out = buffer;
    size_t remaining = size - 1;
    char padchar = ' ';

    while (remaining > 0)
    {
        int wrote = 0;
        char cur = *(f++);

        // end of format

        if (cur == '\0')
            break;

        // formatted argument

        if (cur != '%')
        {
            // print string literals
            *out = cur;
            wrote = 1;
        }
        else
        {
            const char* arg = f;
            int pad = 0;

            // support for %0N zero-padding
            if (*arg == '0')
            {
                padchar = '0';
                arg++;
            }
            while ((*arg >= '0') && (*arg <= '9'))
            {
                pad *= 10;
                pad += (*(arg++)) - '0';
            }

            switch (*(arg++))
            {
            case 'i':
            case 'I':
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

                    wrote = IntegerToString((unsigned)val, sign, pad, out,
                                               remaining, 10, padchar);
                }
                break;
            case 'u':
            case 'U':
                {
                    unsigned val = va_arg(ap, unsigned);
                    wrote = IntegerToString(val, 1, pad, out, remaining, 10,
                                               padchar);
                }
                break;
            case 'p':
            case 'x':
            case 'X':
                {
                    unsigned val = va_arg(ap, unsigned);
                    wrote = IntegerToString(val, 1, pad, out, remaining, 16,
                                               padchar);
                }
                break;
            case 's':
            case 'S':
                {
                    char* val = va_arg(ap, char *);
                    size_t len = 0;
                    while (*val != '\0')
                    {
                        if (len + 1 > remaining)
                        {
                            wrote = -1;
                            break;
                        }
                        out[len] = *val;
                        len++;
                        val++;
                    }
                    if (wrote != -1)
                    {
                        wrote = len;
                        if (wrote == 0)
                        {
                            f = arg;
                            wrote = -2;
                        }
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
            case '%':
                *out = '%';
                wrote = 1;
                break;
            default:
                // unsupported, just return with whatever already printed
                wrote = -1;
                break;
            }

            if (wrote >= 0)
                f = arg;
        }

        if (wrote == -1)
            break;

        // put this after the previous if statement (wrote == 0)
        if (wrote == -2)
        {
            wrote = 0;
        }

        remaining -= wrote;
        out += wrote;
    }

    *out = '\0';
    return (size - 1) - remaining;
}
