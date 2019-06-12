/*
 * Copyright (c) 2012 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_surface.h"
#include "nvassert.h"
#include "md5.h"

#define SURFACE_NUM_BYTES(s) \
    (((NV_COLOR_GET_BPP((s).ColorFormat) * (s).Width) >> 3) * (s).Height)

NvS32
NvRmSurfaceComputeName(char *buffer,
                       size_t bufferSize,
                       const NvRmSurface *surfaces,
                       NvU32 numSurfaces)
{
    NvU32 i;
    NvS32 offset = 0;
    NvS32 space = bufferSize;

    for (i=0; i<numSurfaces; ++i)
    {
        offset += NvOsSnprintf(buffer + offset,
                               NV_MAX(0, space - offset),
                               "%s%dx%dx%s",
                               i == 0 ? "" : "-",
                               surfaces[i].Width,
                               surfaces[i].Height,
                               NvColorFormatToString(surfaces[i].ColorFormat));
    }

    return offset;
}

NvError
NvRmSurfaceDump(NvRmSurface *surfaces,
                NvU32 numSurfaces,
                const char *filename)
{
    NvU32 i;
    NvU32 numBytes;
    NvError err;
    NvOsFileHandle f;
    void *pixels;

    if (surfaces == NULL || numSurfaces == 0 || filename == NULL)
        return NvError_BadParameter;

    err = NvOsFopen(filename, NVOS_OPEN_WRITE, &f);

    if (err != NvSuccess)
        return err;

    numBytes = 0;

    for (i=0; i<numSurfaces; ++i)
        numBytes = NV_MAX(numBytes, SURFACE_NUM_BYTES(surfaces[i]));

    if (numBytes == 0)
        return NvError_BadParameter;

    pixels = NvOsAlloc(numBytes);

    if (pixels == NULL)
    {
        NvOsFclose(f);
        return NvError_InsufficientMemory;
    }

    for (i=0; i<numSurfaces; ++i)
    {
        NvRmSurfaceRead(&surfaces[i], 0, 0,
                        surfaces[i].Width, surfaces[i].Height, pixels);
        NvOsFwrite(f, pixels, SURFACE_NUM_BYTES(surfaces[i]));
    }

    NvOsFree(pixels);
    NvOsFclose(f);

    return NvSuccess;
}

NV_CT_ASSERT(NVRM_SURFACE_MD5_BUFFER_SIZE == NV_MD5_OUTPUT_STRING_LENGTH);

NvError
NvRmSurfaceComputeMD5(NvRmSurface *surfaces,
                      NvU32 numSurfaces,
                      char *md5,
                      NvU32 size)
{
    NvU32 i;
    NvU32 numBytes;
    NvU8 *pixels;
    NvU8 *pixelCursor;
    MD5Context md5Ctx;
    NvU8 digest[NV_MD5_OUTPUT_LENGTH];

    if (surfaces == NULL || numSurfaces == 0)
        return NvError_BadParameter;

    NV_ASSERT(md5);
    NV_ASSERT(size == NVRM_SURFACE_MD5_BUFFER_SIZE);

    numBytes = 0;

    for (i=0; i<numSurfaces; ++i)
        numBytes += SURFACE_NUM_BYTES(surfaces[i]);

    if (numBytes == 0)
        return NvError_BadParameter;

    pixels = (NvU8*)NvOsAlloc(numBytes);
    NvOsMemset(pixels, 0, numBytes);

    pixelCursor = pixels;
    for (i = 0; i < numSurfaces; i++)
    {
        NvRmSurfaceRead(&surfaces[i], 0, 0,
                        surfaces[i].Width, surfaces[i].Height, pixelCursor);
        pixelCursor += SURFACE_NUM_BYTES(surfaces[i]);
    }
    NV_ASSERT(pixelCursor == pixels + numBytes);

    // Compute the hash
    MD5Init(&md5Ctx);
    MD5Update(&md5Ctx, pixels, numBytes);
    MD5Final(digest, &md5Ctx);

    // Convert md5 to printable string
    #define INT2HEX(a) (a<=9)?((char)'0'+a):((char)'A'+a-10)
    for (i=0; i<NV_MD5_OUTPUT_LENGTH; i++)
    {
        md5[2*i + 0] = INT2HEX(digest[i] / 16);
        md5[2*i + 1] = INT2HEX(digest[i] % 16);
    }
    md5[2*i] = 0;
    #undef INT2HEX

    NvOsFree(pixels);

    return NvSuccess;
}
