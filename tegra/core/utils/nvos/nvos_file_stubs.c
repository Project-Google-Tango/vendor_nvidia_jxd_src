/*
 * Copyright (c) 2008 NVIDIA Corporation.  All Rights Reserved.
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

#include "nvos.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvOsSetFileHooks()
//===========================================================================
const NvOsFileHooks *NvOsSetFileHooks(NvOsFileHooks *newHooks)
{
    return 0;
}

//===========================================================================
// NvOsFopen()
//===========================================================================
NvError NvOsFopen(const char *path, NvU32 flags, NvOsFileHandle *file)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFclose()
//===========================================================================
void NvOsFclose(NvOsFileHandle stream)
{
}

//===========================================================================
// NvOsFwrite()
//===========================================================================
NvError NvOsFwrite(NvOsFileHandle stream, const void *ptr, size_t size)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFread()
//===========================================================================
NvError NvOsFread(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFread()
//===========================================================================
NvError NvOsFreadTimeout(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFgetc()
//===========================================================================
NvError NvOsFgetc(NvOsFileHandle stream, NvU8 *c)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFseek()
//===========================================================================
NvError NvOsFseek(
    NvOsFileHandle file,
    NvS64 offset,
    NvOsSeekEnum whence)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFtell()
//===========================================================================
NvError NvOsFtell(NvOsFileHandle file, NvU64 *position)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsStat()
//===========================================================================
NvError NvOsStat(const char *filename, NvOsStatType *stat)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFstat()
//===========================================================================
NvError NvOsFstat(NvOsFileHandle file, NvOsStatType *stat)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFflush()
//===========================================================================
NvError NvOsFflush(NvOsFileHandle stream)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFsync()
//===========================================================================
NvError NvOsFsync(NvOsFileHandle stream)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsFremove()
//===========================================================================
NvError NvOsFremove(const char *filename)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsIoctl()
//===========================================================================
NvError NvOsIoctl(
    NvOsFileHandle file,
    NvU32 IoctlCode,
    void *pBuffer,
    NvU32 InBufferSize,
    NvU32 InOutBufferSize,
    NvU32 OutBufferSize)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsOpendir()
//===========================================================================
NvError NvOsOpendir(const char *path, NvOsDirHandle *dir)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsReaddir()
//===========================================================================
NvError NvOsReaddir(NvOsDirHandle dir, char *name, size_t size)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsClosedir()
//===========================================================================
void NvOsClosedir(NvOsDirHandle dir)
{
}

//===========================================================================
// NvOsVfprintf()
//===========================================================================
NvError NvOsVfprintf(NvOsFileHandle stream, const char *format, va_list ap)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsPrintf()
//===========================================================================
void NvOsPrintf(const char *format, ...)
{
}

//===========================================================================
// NvOsFprintf()
//===========================================================================
NvError NvOsFprintf(NvOsFileHandle stream, const char *format, ...)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvOsDebugPrintf()
//===========================================================================
void NvOsDebugPrintf(const char *format, ...)
{
}

//===========================================================================
// NvOsDebugVprintf()
//===========================================================================
void NvOsDebugVprintf( const char *format, va_list ap )
{
}

//===========================================================================
// NvOsDebugNprintf()
//===========================================================================
NvS32 NvOsDebugNprintf( const char *format, ... )
{
}
