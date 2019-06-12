/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

// A minimal libc containing only what strictly needed for compiling
// aos binaries with gcc. Only use this in situations where no other libc
// implementation is available.
//
// We can't include <string.h> here, as it results in various errors.
// We have to provide our own prototypes for these functions to silence
// the missing-prototypes errors.

#include "nvcommon.h"

#define NV_LIBC_BREAK() \
    do {                                                                    \
        NvU32 _x = (NvU32)__LINE__;                                         \
        volatile NvU32 *_px = (volatile NvU32 *) &_x;                       \
        while (*_px == *_px) { }                                            \
    } while (1);

// Stack smashing protection (Android code compiles with -fstack-protection
// by default)

unsigned int __stack_chk_guard = 0xaff;

void __attribute__((noreturn)) __stack_chk_fail(void);
void __attribute__((noreturn)) __stack_chk_fail(void)
{
    NV_LIBC_BREAK()
}

// Mem routines
void *memset(void *s, int c, size_t n);
void *memset(void *s, int c, size_t n)
{
    NvU8 *cur = (NvU8 *)s;
    NvU8 *end = cur + n;
    char cc = (char)c;
    NvU32 w = (c << 24) | (c << 16) | (c << 8) | c;

    // Set any bytes up to the first word boundary.
    while ( (((NvU32)cur) & (sizeof(NvU32) - 1)) && (cur < end) )
    {
        *(cur++) = cc;
    }

    // Do 4-word writes for as long as we can.
    while ((NvUPtr)(end - cur) >= sizeof(NvU32) * 4)
    {
        *((NvU32*)(cur+0)) = w;
        *((NvU32*)(cur+4)) = w;
        *((NvU32*)(cur+8)) = w;
        *((NvU32*)(cur+12)) = w;
        cur += sizeof(NvU32) * 4;
    }

    // Set any bytes after the last 4-word boundary.
    while (cur < end)
    {
        *(cur++) = cc;
    }

    return s;
}

void *memcpy(void *dst, const void *src, size_t size);
void *memcpy(void *dst, const void *src, size_t size)
{
    NvU8 *d;
    NvU8 *s;

    NvU32 *d32 = (NvU32*)dst;
    NvU32 *s32 = (NvU32*)src;

    if ( (( (NvUPtr)src) & 0x3) == 0 &&
         (( (NvUPtr)dst) & 0x3) == 0 )
    {
        // it's aligned, do 16-bytes at a time (RVDS optimizes this as LDM/STM)
        while (size >= 16)
        {
            *d32++ = *s32++;
            *d32++ = *s32++;
            *d32++ = *s32++;
            *d32++ = *s32++;

            size = size - 16;
        }

        // do the trailing words
        while (size >= 4)
        {
            *d32++ = *s32++;
            size = size - 4;
        }
    }

    if (!size)
        return d32;

    /* if it's unaligned or there are trailing bytes to be copied, copy them
     * here
     */
    d = (NvU8*)d32;
    s = (NvU8*)s32;

    do
    {
        // copy any residual bytes
        *d++ = *s++;
        size = size - 1;

    } while( size );

    return d;
}

int memcmp(const void *s1, const void *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n)
{
    const NvU8*  p1   = s1;
    const NvU8*  end1 = p1 + n;
    const NvU8*  p2   = s2;
    int          d    = 0;

    for (;;) {
        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;

        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;

        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;

        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;
    }
    return d;
}

/**
 * memchr - Find a character in an area of memory.
 * @s: The memory area
 * @c: The byte to search for
 * @n: The size of the area.
 *
 * returns the address of the first occurrence of @c, or %NULL
 * if @c is not found
 */
void *memchr(const void *s, int c, size_t n);
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;

    if (!s)
        return NULL;

    while (n-- != 0) {
        if ((unsigned char)c == *p++) {
            return (void *)(p - 1);
        }
    }
    return NULL;
}

/**
 * memmove - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area.
 *
 * Unlike memcpy(), memmove() copes with overlapping areas.
 */
void *memmove(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count)
{
    char *tmp;
    const char *s;

    if (!dest || !src)
        return NULL;

    if (dest <= src) {
        tmp = dest;
        s = src;
        while (count--)
            *tmp++ = *s++;
    } else {
        tmp = dest;
        tmp += count;
        s = src;
        s += count;
        while (count--)
            *--tmp = *--s;
    }
    return dest;
}

// String routines
char *strcat(char *s, const char* append);
char *strcat(char *s, const char* append)
{
    char *save = s;
    for (; *s; ++s)
        ;
    while ((*s++ = *append++) != '\0')
        ;
    return save;
}

char *__strcat_chk(char *dest, const char *src, size_t destlen);
char *__strcat_chk(char *dest, const char *src, size_t destlen)
{
    char *save = dest;
    while (*dest)
        dest++;
    size_t len = dest - save; // original length of dest string
    if (len >= destlen)
        NV_LIBC_BREAK();
    destlen -= len;
    while ((*dest++ = *src++) != '\0')
    {
        destlen--;
        if (!destlen)
            NV_LIBC_BREAK();
    }
    return save;
}

int strcmp(const char *s1, const char *s2);
int strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == 0)
            return 0;

    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

int strncmp(const char *s1, const char *s2, size_t n);
int strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0)
        return 0;

    do
    {
        if (*s1 != *s2++)
            return (*(unsigned char *)s1 - *(unsigned char *)--s2);
        if (*s1++ == 0)
            break;
    } while (--n != 0);

    return 0;
}

char *strcpy(char *to, const char *from);
char *strcpy(char *to, const char *from)
{
    char *save = to;
    for (; (*to = *from) != '\0'; ++from, ++to)
        ;
    return save;
}

char *strncpy(char *dst, const char *src, size_t n);
char *strncpy(char *dst, const char *src, size_t n)
{
    if (n != 0)
    {
        char* d = dst;
        const char* s = src;

        do {
            if ((*d++ = *s++) == 0)
            {
                /* NUL pad the remaining n-1 bytes */
                while (--n != 0)
                    *d++ = 0;
                break;
            }
        } while (--n != 0);
    }
    return dst;
}

size_t strlen(const char *str);
size_t strlen(const char *str)
{
    const char *s = str;
    while (*s)
        s++;
    return s - str;
}

size_t __strlen_chk(const char *str, size_t len);
size_t __strlen_chk(const char *str, size_t len)
{
    size_t ret = strlen(str);
    if (ret >= len)
        NV_LIBC_BREAK();
    return ret;
}

/**
 * strchr - Find the first occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 *
 * returns the address of the first occurrence of @c, or %NULL
 * if @c is not found
 */
char *strchr(const char *s, int c);
char *strchr(const char *s, int c)
{
    if(!s)
        return NULL;
    for (; *s != (char)c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *)s;
}

// unistd
void abort(void);
void abort(void)
{
    // in a single process environment there's not much we can do
    NV_LIBC_BREAK()
}
