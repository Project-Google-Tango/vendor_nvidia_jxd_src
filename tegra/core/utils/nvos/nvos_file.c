/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### NOTES #####################################
//###########################################################################

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################
#if NV_DEBUG || NVOS_TRACE
#undef NvOsFopen
#endif

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvOsFileHooks s_hooksDefault =
{
    NvOsFopenInternal,
    NvOsFcloseInternal,
    NvOsFwriteInternal,
    NvOsFreadInternal,
    NvOsFseekInternal,
    NvOsFtellInternal,
    NvOsFstatInternal,
    NvOsStatInternal,
    NvOsFflushInternal,
    NvOsFsyncInternal,
    NvOsFremoveInternal,
    NvOsFlockInternal,
    NvOsFtruncateInternal,
    NvOsOpendirInternal,
    NvOsReaddirInternal,
    NvOsClosedirInternal,
    NvOsMkdirInternal,
};

static const NvOsFileHooks *s_fileHooks = &s_hooksDefault;

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvOsAtomicExchangePointer()
//===========================================================================
static NV_INLINE void *NvOsAtomicExchangePointer( void **pTarget, void *Value)
{
#if NVCPU_IS_64_BITS
#if defined(__GNUC__)
    return __sync_lock_test_and_set(pTarget, Value);
#else
#error NvOsAtomicExchangePointer needs to be fixed for 64-bit non-GCC, see bug 1408380
#endif
#else
    return (void*)NvOsAtomicExchange32( (NvS32*)pTarget, (NvS32)Value );
#endif
}

//===========================================================================
// NvOsSetFileHooks()
//===========================================================================
const NvOsFileHooks *NvOsSetFileHooks(NvOsFileHooks *newHooks)
{
    if( !newHooks )
    {
        newHooks = &s_hooksDefault;
    }

    return NvOsAtomicExchangePointer((void**)&s_fileHooks, newHooks);
}

//===========================================================================
// NvOsFopen()
//===========================================================================
NvError NvOsFopen(const char *path, NvU32 flags, NvOsFileHandle *file)
{
    if (flags & NVOS_OPEN_APPEND)
        flags |= NVOS_OPEN_WRITE;
    return s_fileHooks->hookFopen(path, flags, file );
}

//===========================================================================
// NvOsFclose()
//===========================================================================
void NvOsFclose(NvOsFileHandle stream)
{
    if (stream)
        s_fileHooks->hookFclose(stream);
}

//===========================================================================
// NvOsFwrite() -
//===========================================================================
NvError NvOsFwrite(NvOsFileHandle stream, const void *ptr, size_t size)
{
    return s_fileHooks->hookFwrite(stream, ptr, size);
}

//===========================================================================
// NvOsFreadTimeout()
//===========================================================================
NvError NvOsFreadTimeout(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32  timeout)
{
    size_t dummy;
    bytes = bytes ? bytes : &dummy;
    return s_fileHooks->hookFread(stream, ptr, size, bytes, timeout);
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
    return NvOsFreadTimeout(stream, ptr, size, bytes, NV_WAIT_INFINITE);
}

//===========================================================================
// NvOsFgetc()
//===========================================================================
NvError NvOsFgetc(NvOsFileHandle stream, NvU8 *c)
{
    return NvOsFread(stream, c, 1, NULL);
}

//===========================================================================
// NvOsFseek()
//===========================================================================
NvError NvOsFseek(
    NvOsFileHandle file,
    NvS64 offset,
    NvOsSeekEnum whence)
{
    return s_fileHooks->hookFseek(file, offset, whence);
}

//===========================================================================
// NvOsFtell()
//===========================================================================
NvError NvOsFtell(NvOsFileHandle file, NvU64 *position)
{
    return s_fileHooks->hookFtell(file, position);
}

//===========================================================================
// NvOsStat()
//===========================================================================
NvError NvOsStat(const char *filename, NvOsStatType *stat)
{
    stat->type = NvOsFileType_Unknown;
    stat->size = 0;
    return s_fileHooks->hookStat(filename, stat);
}

//===========================================================================
// NvOsFstat()
//===========================================================================
NvError NvOsFstat(NvOsFileHandle file, NvOsStatType *stat)
{
    stat->type = NvOsFileType_Unknown;
    stat->size = 0;
    return s_fileHooks->hookFstat(file, stat);
}

//===========================================================================
// NvOsStatInternal() - generic stat implementation
//===========================================================================
NvError NvOsStatInternal(const char *filename, NvOsStatType *stat)
{
    NvOsFileHandle  file;
    NvOsDirHandle   dir;
    NvError         err;

    stat->type = NvOsFileType_Unknown;
    stat->size = 0;

    //
    // regular file?
    //
    if (!NvOsFopen(filename, NVOS_OPEN_READ, &file))
    {
        err = NvOsFstat(file, stat);
        NvOsFclose(file);
        return err;
    }

    //
    // directory?
    //
    if (!NvOsOpendir(filename, &dir))
    {
        stat->type = NvOsFileType_Directory;
        stat->size = 0;
        NvOsClosedir(dir);
        return NvSuccess;
    }

    return NvError_BadParameter;
}

//===========================================================================
// NvOsFflush()
//===========================================================================
NvError NvOsFflush(NvOsFileHandle stream)
{
    return s_fileHooks->hookFflush(stream);
}

//===========================================================================
// NvOsFsync()
//===========================================================================
NvError NvOsFsync(NvOsFileHandle stream)
{
    return s_fileHooks->hookFsync(stream);
}

//===========================================================================
// NvOsFremove()
//===========================================================================
NvError NvOsFremove(const char *filename)
{
    return s_fileHooks->hookFremove(filename);
}

//===========================================================================
// NvOsFlock()
//===========================================================================
NvError NvOsFlock(NvOsFileHandle stream, NvOsFlockType type)
{
    return s_fileHooks->hookFlock(stream, type);
}

//===========================================================================
// NvOsFtruncate()
//===========================================================================
NvError NvOsFtruncate(NvOsFileHandle stream, NvU64 length)
{
    return s_fileHooks->hookFtruncate(stream, length);
}

//===========================================================================
// NvOsOpendir()
//===========================================================================
NvError NvOsOpendir(const char *path, NvOsDirHandle *dir)
{
    return s_fileHooks->hookOpendir(path, dir);
}

//===========================================================================
// NvOsReaddir()
//===========================================================================
NvError NvOsReaddir(NvOsDirHandle dir, char *name, size_t size)
{
    return s_fileHooks->hookReaddir(dir, name, size);
}

//===========================================================================
// NvOsClosedir()
//===========================================================================
void NvOsClosedir(NvOsDirHandle dir)
{
    if (dir)
        s_fileHooks->hookClosedir(dir);
}

//===========================================================================
// NvOsMkdir()
//===========================================================================
NvError NvOsMkdir(char *dirname)
{
    return s_fileHooks->hookMkdir(dirname);
}
