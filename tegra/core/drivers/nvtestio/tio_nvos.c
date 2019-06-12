/*
 * Copyright (c) 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited150G
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"
#include "nvos.h"
#include "nvapputil.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioOsAllocStream() - allocate a stream structure
//===========================================================================
NvTioStream *NvTioOsAllocStream(void)
{
    NvTioStream *stream = NvOsAlloc(sizeof(NvTioStream));
    if (stream)
        NvOsMemset(stream, 0, sizeof(NvTioStream));
    return stream;
}

//===========================================================================
// NvTioOsFreeStream() - free a stream structure
//===========================================================================
void NvTioOsFreeStream(NvTioStream *stream)
{
    if (stream)
        stream->magic = NV_TIO_FREE_MAGIC;
    NvOsFree(stream);
}

//===========================================================================
// NvTioOsSnprintf() - snprintf
//===========================================================================
NvS32 NvTioOsSnprintf(
                    char  *buf,
                    size_t len,
                    const char *format,
                    ...)
{
    va_list ap;

    va_start(ap, format);
    len = NvOsVsnprintf(buf, len, format, ap);
    va_end(ap);

    return len;
}


//===========================================================================
// NvTioVsnprintfAddStr() - add chars for NvTioVsnprintf
//===========================================================================
// Add str at ptr, do not exceed end, and return new ptr.
// Return value will be at most end
static char *NvTioVsnprintfAddStr(
                    char *ptr,
                    char *end,
                    const char *str,
                    size_t len)
{
    while (ptr != end && len) {
        *(ptr++) = *(str++);
        len--;
    }
    return ptr;
}

//===========================================================================
// NvTioVsnprintf() - snprintf (subset of standard functionality)
//===========================================================================
static NvS32 NvTioVsnprintf(
                    char  *dstBuffer,
                    size_t dstSize,
                    const char *format,
                    va_list ap)
{
    //
    // quick-and-dirty subset of vsnprintf
    //
    char *dst    = dstBuffer;
    char *dstEnd = dst + dstSize;
    char buf[20];
    NvU32 len;
    const char *s;
    const char *e = format;
    int err = 0;

    while (*e) {
        int hex0x=0;
        int left = 0;
        NvU32 field = 0;
        NvS32 prec = -1;
        char pad[1];
        char sign[1];
        NvU32 v,n;
        const char *pp;
        int i;
        pad[0] = ' ';
        sign[0] = 0;

        for (s=e; *e && (*e != '%' || err); e++);
        if (e-s) {
            dst = NvTioVsnprintfAddStr(dst, dstEnd, s, e-s);
        }
        if (!*e)
            break;

        // pp points to the %
        pp = e++;
        for (len=0, s=0; !s; ) {
            int c = *(e++);

            switch (c) {
                case '%':
                    s   = "%";
                    len = 1;
                    sign[0] = 0;
                    hex0x = 0;
                    field = 0;
                    break;

                case 'u':
                case 'd':
                case 'i':
                    v = (NvU32)va_arg(ap, int);
                    len = 0;
                    if (v>0x7fffffff && c!='u') {
                        sign[0] = '-';
                        v = (NvU32)-(NvS32)v;
                    }
                    n = 1000000000;
                    while (n > v) n/=10;
                    if (!n)
                        buf[len++] = '0';
                    while (n) {
                        buf[len] = (char)(v/n);
                        v -= buf[len] * n;
                        buf[len] += '0';
                        n /= 10;
                        len++;
                    }
                    s = buf;
                    hex0x = 0;
                    break;

                case 'p':
                    hex0x=1;
                case 'x':
                case 'X':
                    v = (NvU32)va_arg(ap, int);
                    for (i=0; i<8; i++) {
                        buf[i+2] = (char)((v >> ((7-i)*4)) & 0xf);
                        if (buf[i+2] <= 9) {
                            buf[i+2] += '0';
                        } else if (c=='X') {
                            buf[i+2] += 'A' - 10;
                        } else {
                            buf[i+2] += 'a' - 10;
                        }
                    }

                    s   = buf + 2;
                    len = 8;
                    while (len>1 && s[0] == '0') {
                        s++;
                        len--;
                    }
                    break;


                case 'c':
                    buf[0] = va_arg(ap, int);
                    buf[1] = 0;
                    s = buf;
                    len = 1;
                    sign[0] = 0;
                    hex0x = 0;
                    break;

                case 's':
                    s = (const char*)va_arg(ap, char*);
                    len = (NvU32)NvTioOsStrlen(s);
                    if (prec != -1 && (len>(NvU32)prec))
                        len = (NvU32)prec;
                    sign[0] = 0;
                    hex0x = 0;
                    pad[0] = ' ';
                    break;

                case '#': hex0x=1; break;
                case '+': sign[0]='+'; break;
                case ' ': sign[0]=' '; break;
                case '.': prec=0; break;
                case '-': left=1; break;

                case '\'':
                case 'l':
                case 'z':
                case 't':
                    break;

                case '0':
                    if (!field) pad[0] = '0';
                case '1': case '2': case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':
                    if (prec == -1) {
                        field *= 10;
                        field += c - '0';
                    } else {
                        prec *= 10;
                        prec += c - '0';
                    }
                    break;

                case 'e':
                case 'E':
                case 'f':
                case 'g':
                case 'G':
                case 'a':
                case 'A':
                    // TODO: implement doubles

                case 'o':
                    // TODO: implement octal

                default:
                    s = pp;
                    len = (NvU32)(e-pp);
                    field = 0;
                    hex0x=0;
                    err = 1;
                    break;
            }
        }
        if (len) {
            if (sign[0] && (field > 0))
                field--;
            if (hex0x) {
                field = field > 2 ? field-2 : 0;
                dst = NvTioVsnprintfAddStr(dst, dstEnd, "0x", 2);
                pad[0] = '0';
            }
            if (!left) {
                for (; len<field; field--) {
                    dst = NvTioVsnprintfAddStr(dst, dstEnd, pad, 1);
                }
            }
            if (sign[0]) {
                dst = NvTioVsnprintfAddStr(dst, dstEnd, sign, 1);
            }
            dst = NvTioVsnprintfAddStr(dst, dstEnd, s, len);
            if (left) {
                pad[0] = ' ';
                for (; len<field; field--) {
                    dst = NvTioVsnprintfAddStr(dst, dstEnd, pad, 1);
                }
            }
        }
    }
    if (dst == dstEnd)
        return -1;
    *dst = 0;
    return dst - dstBuffer;
}

//===========================================================================
// NvTioOsVsnprintfTest() - returns size of string (to test NvOsVsnprintf)
//===========================================================================
static NvS32 NvTioOsVsnprintfTest(const char *format, ...)
{
    NvS32 rv;
    char buf[20];
    va_list ap;
    va_start(ap, format);
    rv = NvOsVsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    return rv;
}

//===========================================================================
// NvTioOsVsnprintf() - snprintf
//===========================================================================
NvS32 NvTioOsVsnprintf(
                    char  *buf,
                    size_t len,
                    const char *format,
                    va_list ap)
{
    static NvS32 (*vsnprintf_func)(
                    char  *,
                    size_t,
                    const char *,
                    va_list) = 0;
    if (!vsnprintf_func) {
        NvS32 rv = NvTioOsVsnprintfTest("hi%d%s",22,"bye");
        vsnprintf_func = (rv == 7) ? NvOsVsnprintf : NvTioVsnprintf;
#define NV_TIO_DEBUG_USE_TIO_VSNPRINTF  0
#if     NV_TIO_DEBUG_USE_TIO_VSNPRINTF
        // FOR DEBUGGING:
        // This forces the use of NvTioVsnprintf
        // (normally it is used only on AVP; on MPCORE NvOsVsnprintf works.
        vsnprintf_func = NvTioVsnprintf;
#endif
#define NV_TIO_DEBUG_INDICATE_VSNPRINTF 0
#if     NV_TIO_DEBUG_INDICATE_VSNPRINTF
        // FOR DEBUGGING:
        // This indicates which is being used in the first character of the
        // first NvTioprintf() call.
        //   T = NvTioVsnprintf
        //   O = NvOsVsnprintf
        *(buf++) = (vsnprintf_func == NvTioVsnprintf) ? 'T' : 'O';
        len--;
#endif
    }
    return vsnprintf_func(buf, len, format, ap);
}

//===========================================================================
// NvTioOsAllocVsprintf() vsprintf to an allocated buffer
//===========================================================================
NvError NvTioOsAllocVsprintf(
                    char  **buf,
                    NvU32  *len,
                    const char *format,
                    va_list ap)
{
    NvU32 sz = 512;

    while(1) {
        NvU32 l;
        char *b = NvOsAlloc(sz);
        if (!b)
            return NvError_InsufficientMemory;

        l = (NvU32)NvTioOsVsnprintf(b, sz, format, ap);
        if (l < sz) {
            *buf = b;
            *len = l;
            return NvSuccess;
        }
        NvOsFree(b);
        sz *= 2;
    }
}

//===========================================================================
// NvTioOsFreeVsprintf() - free buffer allocated by NvTioVsprintfAlloc()
//===========================================================================
void NvTioOsFreeVsprintf(
                    char  *buf,
                    NvU32  len)
{
    NvOsFree(buf);
}

//===========================================================================
// NvTioOsStrcmp() - strncmp
//===========================================================================
int NvTioOsStrcmp(const void *s1, const void *s2)
{
    return NvOsStrcmp(s1, s2);
}

//===========================================================================
// NvTioOsStrncmp() - strncmp
//===========================================================================
int NvTioOsStrncmp(const void *s1, const void *s2, size_t size)
{
    return NvOsStrncmp(s1, s2, size);
}

//===========================================================================
// NvTioOsStrlen() - strlen
//===========================================================================
size_t NvTioOsStrlen(const char *s)
{
    return NvOsStrlen(s);
}

//===========================================================================
// NvTioNvosFopen() - NvOsFopen
//===========================================================================
static NvError NvTioNvosFopen(
                const char *path,
                NvU32 flags,
                NvTioStreamHandle stream)
{
    NvError err;
    NvOsFileHandle file;

    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);

    err = NvOsFopen(path, flags, &file);
    if (!err)
        stream->f.fp = file;
    return DBERR(err);
}

//===========================================================================
// NvTioNvosClose() - close file, stream, dir, or listener
//===========================================================================
static void    NvTioNvosClose(
                NvTioStreamHandle stream)
{
    if (stream->magic == NV_TIO_STREAM_MAGIC)
        NvOsFclose(stream->f.fp);
    else if (stream->magic == NV_TIO_DIR_MAGIC)
        NvOsClosedir(stream->f.fp);
}

//===========================================================================
// NvTioNvosFwrite() - NvOsFwrite
//===========================================================================
static NvError NvTioNvosFwrite(
                NvTioStreamHandle stream,
                const void *ptr,
                size_t size)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsFwrite(stream->f.fp, ptr, size);
}

//===========================================================================
// NvTioNvosFread() - NvOsFread
//===========================================================================
static NvError NvTioNvosFread(
                NvTioStreamHandle stream,
                void *ptr,
                size_t size,
                size_t *bytes,
                NvU32 timeout_msec)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsFread(stream->f.fp, ptr, size, bytes);
}

//===========================================================================
// NvTioNvosFseek() - NvOsFseek
//===========================================================================
static NvError NvTioNvosFseek(
                NvTioStreamHandle stream,
                NvS64 offset,
                NvU32 whence)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsFseek(stream->f.fp, offset, (NvOsSeekEnum)whence);
}

//===========================================================================
// NvTioNvosFtell() - NvOsFtell
//===========================================================================
static NvError NvTioNvosFtell(
                NvTioStreamHandle stream,
                NvU64 *position)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsFtell(stream->f.fp, position);
}

//===========================================================================
// NvTioNvosFflush() - NvOsFflush
//===========================================================================
static NvError NvTioNvosFflush(
                NvTioStreamHandle stream)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsFflush(stream->f.fp);
}

//===========================================================================
// NvTioNvosIoctl() - NvOsIoctl
//===========================================================================
static NvError NvTioNvosIoctl(
                NvTioStreamHandle stream,
                NvU32 ioctlCode,
                void *pBuffer,
                NvU32 inBufferSize,
                NvU32 inOutBufferSize,
                NvU32 outBufferSize)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsIoctl(
                stream->f.fp,
                ioctlCode,
                pBuffer,
                inBufferSize,
                inOutBufferSize,
                outBufferSize);
}

//===========================================================================
// NvTioNvosOpendir() - NvOsOpendir
//===========================================================================
static NvError NvTioNvosOpendir(
                const char *path,
                NvTioStreamHandle stream)
{
    NvError err;
    NvOsDirHandle dir;

    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    err = NvOsOpendir(path, &dir);
    if (!err)
        stream->f.fp = dir;
    return DBERR(err);
}

//===========================================================================
// NvTioNvosReaddir() - NvOsReaddir
//===========================================================================
static NvError NvTioNvosReaddir(
                NvTioStreamHandle stream,
                char *name,
                size_t size)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsReaddir(stream->f.fp, name, size);
}

//===========================================================================
// NvTioNvosVfprintf() - NvOsVfprintf
//===========================================================================
static NvError NvTioNvosVfprintf(
                NvTioStreamHandle stream,
                const char *format,
                va_list ap)
{
    if (!NvTioGlob.enableNvosTransport)
        return DBERR(NvError_InvalidState);
    return NvOsVfprintf(stream->f.fp, format, ap);
}

//===========================================================================
// NvTioNvosDebugVprintf() - Debug Vprintf()
//===========================================================================
NvError NvTioNvosDebugVprintf(
                const char *format,
                va_list ap)
{
#if !NVOS_IS_LINUX && !NVOS_IS_WINDOWS
    if (!NvTioGlob.enableNvosTransport)
        return NvError_InvalidState;        // DO NOT USE DBERR() HERE!!
#endif
    NvOsDebugVprintf(format, ap);
    return NvSuccess;
}

//===========================================================================
// NvTioNvosGetConfigString() - get string env var or registry value
//===========================================================================
NvError NvTioNvosGetConfigString(const char *name, char *value, NvU32 size)
{
    return NvOsGetConfigString(name, value, size);
}

//===========================================================================
// NvTioNvosGetConfigU32() - get integer env var or registry value
//===========================================================================
NvError NvTioNvosGetConfigU32(const char *name, NvU32 *value)
{
    return NvOsGetConfigU32(name, value);
}

//===========================================================================
// NvTioRegisterNvos() - register nvos file support
//===========================================================================
void NvTioRegisterNvos(void)
{
    static NvTioStreamOps ops = {0};

    if (ops.sopNext)
        return;

    ops.sopName     = "Nvos-file";
    ops.sopFopen    = NvTioNvosFopen;
    ops.sopClose    = NvTioNvosClose;
    ops.sopFwrite   = NvTioNvosFwrite;
    ops.sopFread    = NvTioNvosFread;
    ops.sopFseek    = NvTioNvosFseek;
    ops.sopFtell    = NvTioNvosFtell;
    ops.sopFflush   = NvTioNvosFflush;
    ops.sopIoctl    = NvTioNvosIoctl;
    ops.sopOpendir  = NvTioNvosOpendir;
    ops.sopReaddir  = NvTioNvosReaddir;
    ops.sopVfprintf = NvTioNvosVfprintf;

    NvTioRegisterOps(&ops);
}

//===========================================================================
// NvTioOsGetTimeMS()
//===========================================================================
NvU32 NvTioOsGetTimeMS(void)
{
    return NvOsGetTimeMS();
}
