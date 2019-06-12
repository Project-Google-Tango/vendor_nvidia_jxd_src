/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All Rights Reserved.
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

#include "nvcommon.h"
#include "nvassert.h"
#include "tio_local.h"
#include "tio_fcache.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

typedef struct CachedFileHandleRec
{
    NvTioFileCache *fc;
    NvU32 pos;
} *CachedFileHandle;

//###########################################################################
//############################### DECLARATIONS ##############################
//###########################################################################

static NvError NvTioFileCacheHookFopen(const char *path, NvU32 flags, NvOsFileHandle *file);
static void    NvTioFileCacheHookClose(NvOsFileHandle stream);
static NvError NvTioFileCacheHookFwrite(NvOsFileHandle stream, const void *ptr, size_t size);
static NvError NvTioFileCacheHookFread(
        NvOsFileHandle stream,
        void *ptr,
        size_t size,
        size_t *bytes,
        NvU32 timeout_msec);
static NvError NvTioFileCacheHookFseek(NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence);
static NvError NvTioFileCacheHookFtell(NvOsFileHandle file, NvU64 *position);
static NvError NvTioFileCacheHookFstat(NvOsFileHandle file, NvOsStatType *s);
static NvError NvTioFileCacheHookStat(const char *filename, NvOsStatType *stat);
static NvError NvTioFileCacheHookFflush(NvOsFileHandle stream);
static NvError NvTioFileCacheHookFsync(NvOsFileHandle stream);
static NvError NvTioFileCacheHookFremove(const char *filename);
static NvError NvTioFileCacheHookOpendir(const char *path, NvOsDirHandle *dir);
static NvError NvTioFileCacheHookReaddir(NvOsDirHandle dir, char *name, size_t size);
static void    NvTioFileCacheHookClosedir(NvOsDirHandle dir);

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvTioStreamOps s_TioFileCacheOps = {0};

//###########################################################################
//############################### CODE ######################################
//###########################################################################

static NvTioFileCache*
FindFileInCache( const char *path )
{
    NvTioFileCache *p;
    if (!NvOsStrncmp(path, "host:",5))
        path += 5;

    while ((path[0] == '.') || (path[0] == '/') || (path[0] == '\\'))
    {
        path ++;
    }
    for (p=&g_NvTioFileCache; p->Name; p++)
        if ((p->Name[0] == '.') && (p->Name[1] == '/') &&
            !NvOsStrcmp(path, &(p->Name[2])))
            return p;
        else if (!NvOsStrcmp(path,p->Name))
            return p;

    return NULL;
}

// always return success here. If read/listen/opendir fails
// the NvTioStreamOpen will try next stream.
static NvError
NvTioFileCacheCheckPath( const char *path )
{
    //return FindFileInCache(path) ? NvSuccess : NvError_BadParameter;
    return NvSuccess;
}

static NvError
NvTioFileCacheFopen(
    const char *path,
    NvU32 flags,
    NvTioStreamHandle stream)
{
    NvTioFileCache *f = FindFileInCache(path);
    CachedFileHandle h = NULL;

    if ((f == NULL) && (flags & NVOS_OPEN_READ))
        return NvError_FileNotFound;

    if ((f == NULL) && (flags & NVOS_OPEN_CREATE))
    {
        size_t len = NvOsStrlen(path)+1;
        f = NvOsAlloc(sizeof(NvTioFileCache));
        if (f == NULL)
            return NvError_InsufficientMemory;
        f->Name = NvOsAlloc(len);
        if (f->Name == NULL)
        {
            NvOsFree(f);
            return NvError_InsufficientMemory;
        }
        NvOsMemcpy(f->Name, path, len);
        f->Name[len] = 0;
        f->Length = 1024*1024;
        f->Start = NvOsAlloc(f->Length);
        if (f->Start == NULL)
        {
            NvOsFree(f->Name);
            NvOsFree(f);
            return NvError_InsufficientMemory;
        }
        f->padding = f->Length;
        f->Length = 0;
    }
    else if (f==NULL) // NVOS_OPEN_WRITE or NVOS_OPEN_READ only
    {
        return NvError_FileOperationFailed;
    }

    NvOsDebugPrintf("cached fopen %s (0x%x)\n", path, flags);

    h = NvOsAlloc(sizeof(*h));
    if (h == NULL)
        return NvError_InsufficientMemory;

    h->fc = f;
    h->pos = 0;
    stream->f.fp = h;
    return NvSuccess;
}

static void
NvTioFileCacheClose( NvTioStreamHandle stream )
{
    NvOsFree(stream->f.fp);
    stream->f.fp = NULL;
}

static NvError
NvTioFileCacheFread(
    NvTioStreamHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec)
{
    CachedFileHandle h = stream->f.fp;

    *bytes = 0;
    if (h->pos + size > h->fc->Length)
        size = h->fc->Length - h->pos;

    if (size == 0)
        return NvError_EndOfFile;

    NvOsMemcpy(ptr, h->fc->Start + h->pos, size);

    h->pos += size;
    if (bytes)
        *bytes = size;

    return NvSuccess;
}

static NvError NvTioFileCacheFseek(
    NvTioStreamHandle stream,
    NvS64 offset,
    NvU32 whence)      // NvOsSeekEnum
{
    CachedFileHandle h = stream->f.fp;
    NvU64 new_pos = 0;

    switch (whence)
    {
    case NvOsSeek_Set:
        if (offset < 0) return NvError_BadParameter;
        new_pos = offset;
        break;
    case NvOsSeek_Cur:
        if (offset < 0 && -offset > h->pos)
            return NvError_BadParameter;
        new_pos = h->pos + offset;
        break;
    case NvOsSeek_End:
        if (offset < 0 && -offset > h->fc->Length)
            return NvError_BadParameter;
        new_pos = h->fc->Length - offset;
        break;
    default:
        return NvError_BadParameter;
    }
    if (new_pos > h->fc->Length)
        return NvError_FileOperationFailed;

    h->pos = (NvU32)new_pos;
    return NvSuccess;
}


static NvError
NvTioFileCacheFtell(
    NvTioStreamHandle stream,
    NvU64 *position)
{
    CachedFileHandle h = stream->f.fp;
    *position = h->pos;
    return NvSuccess;
}

static NvError
NvTioFileCacheFwrite(
    NvTioStreamHandle stream,
    const void *ptr,
    size_t size)
{
#if 0
    /* this needs to be re-factored, because 1M can't fit into
     * IRAM when we use AVP-RUN-FROM-DRAM model. so this function
     * is temporarily disabled.
     */
    static char data[1024*1024]; // reserve 1M data
    CachedFileHandle h = stream->f.fp;
    NvTioFileCache *fc = h->fc;
    if (fc->padding == 0) {
        fc->padding = 0xff;
        fc->Start = data;
        fc->Length = 0;
    }
    if (fc->Length + size > 1024*1024)
        return NvError_BadParameter;
    NvOsMemcpy(data+fc->Length, (NvS8 *)ptr, size);
    fc->Length += size;
#endif
    CachedFileHandle h = stream->f.fp;
    NvTioFileCache *fc = h->fc;
    if (fc == NULL)
        return NvSuccess;
    if (fc->Length + size > fc->padding)
        return NvError_BadParameter;
    NvOsMemcpy(fc->Start+fc->Length, (NvS8 *)ptr, size);
    fc->Length += size;
    return NvSuccess;
}

static NvError
NvTioFileCacheHookFopen(const char *path, NvU32 flags, NvOsFileHandle *file)
{
    NvError e = NvSuccess;
    e = NvTioFopen(path, flags, (NvTioStreamHandle *)file);
    return e;
}

static void
NvTioFileCacheHookClose(NvOsFileHandle stream)
{
    NvTioClose((NvTioStreamHandle)stream);
}

static NvError
NvTioFileCacheHookFwrite(NvOsFileHandle stream, const void *ptr, size_t size)
{
    NvError e = NvSuccess;
    e = NvTioFwrite((NvTioStreamHandle)stream, ptr, size);
    return e;
}

static NvError
NvTioFileCacheHookFread(
        NvOsFileHandle stream,
        void *ptr,
        size_t size,
        size_t *bytes,
        NvU32 timeout_msec)
{
    NvError e = NvSuccess;
    e = NvTioFread((NvTioStreamHandle)stream, ptr, size, bytes);
    return e;
}

static NvError
NvTioFileCacheHookFseek(NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence)
{
    NvError e = NvSuccess;
    e = NvTioFseek((NvTioStreamHandle)file, offset, whence);
    return e;
}

static NvError
NvTioFileCacheHookFtell(NvOsFileHandle file, NvU64 *position)
{
    NvError e = NvSuccess;
    e = NvTioFtell((NvTioStreamHandle)file, position);
    return e;
}

static NvError
NvTioFileCacheHookFstat(NvOsFileHandle file, NvOsStatType *s)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileCacheHookStat(const char *filename, NvOsStatType *stat)
{
    NvError e = NvSuccess;
    NvTioFileCache *cache;
    if ((cache = FindFileInCache(filename)) != NULL)
    {
        stat->size = cache->Length;
        stat->type = NvOsFileType_File;
    }
    return e;
}

static NvError
NvTioFileCacheHookFflush(NvOsFileHandle stream)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileCacheHookFsync(NvOsFileHandle stream)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileCacheHookFremove(const char *filename)
{
    NvError e = NvError_NotImplemented;
    return e;
}



static NvError
NvTioFileCacheHookOpendir(const char *path, NvOsDirHandle *dir)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileCacheHookReaddir(NvOsDirHandle dir, char *name, size_t size)
{
    NvError e = NvSuccess;
    return e;
}

static void
NvTioFileCacheHookClosedir(NvOsDirHandle dir)
{
}

void
NvTioRegisterFileCache()
{
    s_TioFileCacheOps.sopName      = "FileCacheOps";
    s_TioFileCacheOps.sopFopen     = NvTioFileCacheFopen;
    s_TioFileCacheOps.sopFwrite    = NvTioFileCacheFwrite;
    s_TioFileCacheOps.sopFread     = NvTioFileCacheFread;
    s_TioFileCacheOps.sopClose     = NvTioFileCacheClose;
    s_TioFileCacheOps.sopFseek     = NvTioFileCacheFseek;
    s_TioFileCacheOps.sopFtell     = NvTioFileCacheFtell;
    s_TioFileCacheOps.sopCheckPath = NvTioFileCacheCheckPath;

    NvTioRegisterOps(&s_TioFileCacheOps);
}

void
NvTioSetFileCacheHooks()
{
    static NvOsFileHooks cacheHooks =
    {
        NvTioFileCacheHookFopen,
        NvTioFileCacheHookClose,
        NvTioFileCacheHookFwrite,
        NvTioFileCacheHookFread,
        NvTioFileCacheHookFseek,
        NvTioFileCacheHookFtell,
        NvTioFileCacheHookFstat,
        NvTioFileCacheHookStat,
        NvTioFileCacheHookFflush,
        NvTioFileCacheHookFsync,
        NvTioFileCacheHookFremove,
        NvTioFileCacheHookOpendir,
        NvTioFileCacheHookReaddir,
        NvTioFileCacheHookClosedir
    };
    NvOsSetFileHooks(&cacheHooks);
}
