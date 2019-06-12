/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvrm_surface.h"
#include "nvassert.h"
#include "nvrm_chip.h"
#include "nvrm_chipid.h"

#ifndef NV_IS_AVP
#define NV_IS_AVP=0
#endif

// Size of a subtile in bytes
#define SUBTILE_BYTES (NVRM_SURFACE_SUB_TILE_WIDTH * NVRM_SURFACE_SUB_TILE_HEIGHT)

void NvRmSurfaceInitNonRmPitch(
    NvRmSurface *pSurf,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat ColorFormat,
    NvU32 Pitch,
    void *pBase)
{
    NvOsMemset(pSurf, 0, sizeof(*pSurf));

    pSurf->Width = Width;
    pSurf->Height = Height;
    pSurf->ColorFormat = ColorFormat;
    pSurf->Layout = NvRmSurfaceLayout_Pitch;
    pSurf->Pitch = Pitch;
    pSurf->pBase = pBase;

    /* hMem and Offset do not apply for non-RM alloc'd memory */
    pSurf->hMem = NULL;

    /*
     * Surface base address must be aligned to
     * no less than the word size of ColorFormat.
     */
    NV_ASSERT( !( (NvU32)pBase % ((NV_COLOR_GET_BPP(pSurf->ColorFormat)+7) >> 3) ) );
}

void NvRmSurfaceInitRmPitch(
    NvRmSurface *pSurf,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat ColorFormat,
    NvU32 Pitch,
    NvRmMemHandle hMem,
    NvU32 Offset)
{
    NvOsMemset(pSurf, 0, sizeof(*pSurf));

    pSurf->Width = Width;
    pSurf->Height = Height;
    pSurf->ColorFormat = ColorFormat;
    pSurf->Layout = NvRmSurfaceLayout_Pitch;
    pSurf->Pitch = Pitch;
    pSurf->hMem = hMem;
    pSurf->Offset = Offset;

    /* pBase does not apply for RM alloc'd memory */
    pSurf->pBase = NULL;
}

void NvRmSurfaceFree(NvRmSurface *pSurf)
{
    if (pSurf->hMem)
    {
        /* Free surface from RM alloc'd memory */
        NvRmMemHandleFree(pSurf->hMem);
        pSurf->hMem = NULL;
    }
    else
    {
        /* Free surface from non-RM alloc'd memory */
        NvOsFree(pSurf->pBase);
        pSurf->pBase = NULL;
    }
}

// Simplified version of ComputeOffset that...
// 1. Only works for tiled surfaces
// 2. Takes x in units of bytes rather than pixels
static NvU32 TiledAddr(NvRmSurface *pSurf, NvU32 x, NvU32 y)
{
    NvU32 Offset;

    // Compute offset to the pixel within the subtile
    Offset  =  x & (NVRM_SURFACE_SUB_TILE_WIDTH-1);
    Offset += (y & (NVRM_SURFACE_SUB_TILE_HEIGHT-1)) << NVRM_SURFACE_SUB_TILE_WIDTH_LOG2;

    // Throw away low bits of x and y to get location at subtile granularity
    x &= ~(NVRM_SURFACE_SUB_TILE_WIDTH-1);
    y &= ~(NVRM_SURFACE_SUB_TILE_HEIGHT-1);

    // Add horizontal offset to subtile within its row of subtiles
    Offset += x << NVRM_SURFACE_SUB_TILE_HEIGHT_LOG2;

    // Add vertical offset to row of subtiles
    Offset += y*pSurf->Pitch;

    return Offset;
}

// Simplified version of ComputeOffset that...
// 1. Only works for block linear surfaces
// 2. Takes x in units of bytes rather than pixels
static NvU32 BlockLinearAddr(NvRmSurface *pSurf, NvU32 x, NvU32 y)
{
    NvU32 BlockHeight = 1 << pSurf->BlockHeightLog2;
    NvU32 GobOffset = (y / (8 * BlockHeight)) * pSurf->Pitch * 8 * BlockHeight +
            (x / 64) * 512 * BlockHeight + ((y % (8 * BlockHeight)) / 8) * 512;

    if (NvRmMemGetUncompressedKind(pSurf->Kind) == NvRmMemKind_Generic_16Bx2)
    {
        return GobOffset + ((x % 64) / 32) * 256 + ((y % 8) / 2) * 64 +
                            ((x % 32) / 16) * 32 + (y % 2) * 16 + (x % 16);
    }
    else
    {
        NV_ASSERT(!"Kind invalid or not supported by this function");
        return 0;
    }
}

NvU32 NvRmSurfaceComputeOffset(NvRmSurface *pSurf, NvU32 x, NvU32 y)
{
    NvU32 BitsPerPixel = NV_COLOR_GET_BPP(pSurf->ColorFormat);

    // Make sure we don't fall off the end of the surface
    NV_ASSERT(x < pSurf->Width);
    NV_ASSERT(y < pSurf->Height);

    // Convert x from units of pixels to units of bytes
    // We assert here that the pixel is aligned to a byte boundary
    x *= BitsPerPixel;
    NV_ASSERT(!(x & 7));
    x >>= 3;

    if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
        return y*pSurf->Pitch + x;
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Tiled)
    {
        return TiledAddr(pSurf, x, y);
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        return BlockLinearAddr(pSurf, x, y);
    }
    else
    {
        NV_ASSERT(!"Invalid Layout");
        return 0;
    }
}

#if !(NV_IS_AVP)
void NvRmSurfaceWrite(
    NvRmSurface *pSurf,
    NvU32 x,
    NvU32 y,
    NvU32 Width,
    NvU32 Height,
    const void *pSrcPixels)
{
    const NvU8 *pSrc = pSrcPixels;
    NvU32 BitsPerPixel = NV_COLOR_GET_BPP(pSurf->ColorFormat);
    NvU32 Offset;
    NvU8 *pDst;
    NvU32 CurrentOffset;
    NvBool Interlaced;
    NvU32 Cury;
    NvU8 *pDstCurrent;

    // Convert x and Width from units of pixels to units of bytes
    // We assert here that the rectangle is aligned to byte boundaries
    x *= BitsPerPixel;
    NV_ASSERT(!(x & 7));
    x >>= 3;

    Width *= BitsPerPixel;
    NV_ASSERT(!(Width & 7));
    Width >>= 3;
    Interlaced = (pSurf->DisplayScanFormat == NvDisplayScanFormat_Interlaced);
    if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
        NvU32 SecondFieldOffset = pSurf->SecondFieldOffset - pSurf->Offset;
        if (pSurf->hMem)
        {
            // RM alloc'd memory
            Offset = pSurf->Offset + y*pSurf->Pitch + x;
            if (Interlaced)
            {
                Cury = 0;
                for ( ; Cury < Height; Cury++)
                {
                    if ((Cury + y) & 1)
                    {
                        CurrentOffset = Offset +
                            (Cury >> 1) * (pSurf->Pitch) + SecondFieldOffset;
                    }
                    else
                    {
                        CurrentOffset = Offset + (Cury >> 1) * (pSurf->Pitch);
                    }
                    NvRmMemWrite(pSurf->hMem, CurrentOffset, pSrc, Width);
                    pSrc = (const NvU8 *)pSrc + Width;
                 }
            }
            else
            {
                NvRmMemWriteStrided(
                    pSurf->hMem, Offset, pSurf->Pitch, // destination
                    pSrc, Width, // source
                    Width, Height); // elementsize, count
            }
        }
        else
        {
            // non-RM alloc'd memory
            pDst = (NvU8 *)pSurf->pBase + y*pSurf->Pitch + x;
            if (Interlaced)
            {
                Cury = 0;
                for ( ; Cury < Height; Cury++)
                {
                    if ((Cury + y) & 1)
                        pDstCurrent = pDst + (Cury >> 1) * (pSurf->Pitch) + SecondFieldOffset;
                    else
                        pDstCurrent = pDst + (Cury >> 1) * (pSurf->Pitch);

                     NvOsMemcpy(pDstCurrent, pSrc, Width);
                     pSrc += Width;
                }
            }
            else
            {
                while (Height--)
                {
                    NvOsMemcpy(pDst, pSrc, Width);
                    pSrc += Width;
                    pDst += pSurf->Pitch;
                }
            }
        }
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Tiled)
    {
        NvU32 WidthStart, MiddleTiles, WidthEnd;

        // Determine width of initial chunk to get us to a subtile-aligned spot
        WidthStart = 0;
        if (x & (NVRM_SURFACE_SUB_TILE_WIDTH-1))
        {
            WidthStart = NV_MIN(Width,
                NVRM_SURFACE_SUB_TILE_WIDTH - (x & (NVRM_SURFACE_SUB_TILE_WIDTH-1)));
            Width -= WidthStart;
        }
        NV_ASSERT(WidthStart < NVRM_SURFACE_SUB_TILE_WIDTH);

        // Determine number of full middle tiles and extra pixels at the end
        MiddleTiles = Width >> NVRM_SURFACE_SUB_TILE_WIDTH_LOG2;
        WidthEnd = Width & (NVRM_SURFACE_SUB_TILE_WIDTH-1);

        if (pSurf->hMem)
        {
            // RM alloc'd memory
            while (Height--)
            {
                // Get address of first pixel in row
                Offset = TiledAddr(pSurf, x, y) + pSurf->Offset;
                y++;

                // Write fractional part of first tile (if any)
                if (WidthStart)
                {
                    NvRmMemWrite(pSurf->hMem, Offset, pSrc, WidthStart);
                    pSrc += WidthStart;
                    Offset += WidthStart - NVRM_SURFACE_SUB_TILE_WIDTH + SUBTILE_BYTES;
                }

                if (MiddleTiles)
                {
                    // Write the middle tiles
                    NvRmMemWriteStrided(pSurf->hMem,
                                        Offset,
                                        SUBTILE_BYTES, // dest stride
                                        pSrc,
                                        NVRM_SURFACE_SUB_TILE_WIDTH, // src stride
                                        NVRM_SURFACE_SUB_TILE_WIDTH, // size
                                        MiddleTiles); // count
                    Offset += MiddleTiles * SUBTILE_BYTES;
                    pSrc += MiddleTiles * NVRM_SURFACE_SUB_TILE_WIDTH;
                }

                // Write fractional part of ending tile (if any)
                if (WidthEnd)
                {
                    NvRmMemWrite(pSurf->hMem, Offset, pSrc, WidthEnd);
                    pSrc += WidthEnd;
                }
            }
        }
        else
        {
            NvU32 n;

            // non-RM alloc'd memory
            while (Height--)
            {
                // Get address of first pixel in row
                pDst = (NvU8 *)pSurf->pBase + TiledAddr(pSurf, x, y);
                y++;

                // Write fractional part of first tile (if any)
                if (WidthStart)
                {
                    NvOsMemcpy(pDst, pSrc, WidthStart);
                    pSrc += WidthStart;
                    pDst += WidthStart - NVRM_SURFACE_SUB_TILE_WIDTH + SUBTILE_BYTES;
                }

                // Write the middle tiles
                n = MiddleTiles;
                while (n--)
                {
                    NvOsMemcpy(pDst, pSrc, NVRM_SURFACE_SUB_TILE_WIDTH);
                    pSrc += NVRM_SURFACE_SUB_TILE_WIDTH;
                    pDst += SUBTILE_BYTES;
                }

                // Write fractional part of ending tile (if any)
                if (WidthEnd)
                {
                    NvOsMemcpy(pDst, pSrc, WidthEnd);
                    pSrc += WidthEnd;
                }
            }
        }
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        NvU32 curx;
        void* UserPtr = pSurf->pBase;
        NvU32  SecondFieldOffset = (pSurf->SecondFieldOffset - pSurf->Offset);
        NvU32 Size = NvRmSurfaceComputeSize(pSurf);

        if (!UserPtr)
        {
            NvError err = NvRmMemMap(pSurf->hMem,
                                     pSurf->Offset,
                                     Size,
                                     NVOS_MEM_WRITE,
                                     &UserPtr);

            if (err == NvSuccess)
            {
                NvRmMemCacheSyncForCpu(pSurf->hMem, UserPtr, Size);
            }
            else
            {
                UserPtr = NULL;
            }
        }

        for (Cury = y; Cury < y + Height; Cury++)
        {
            for (curx = x; curx < x + Width; curx++)
            {
                Offset = BlockLinearAddr(pSurf, curx, (Interlaced ? (Cury >> 1) : Cury));
                if (Interlaced)
                    Offset += ((Cury & 1) ? SecondFieldOffset : 0);

                if (UserPtr)
                {
                    pDst = (NvU8*)UserPtr + Offset;
                    *pDst = *pSrc;
                }
                else
                {
                    NvRmMemWrite(pSurf->hMem, Offset, pSrc, 1);
                }
                pSrc++;
            }
        }

        if (UserPtr && pSurf->hMem)
        {
            NvRmMemCacheSyncForDevice(pSurf->hMem, UserPtr, Size);
            NvRmMemUnmap(pSurf->hMem, UserPtr, Size);
        }
    }
    else
    {
        NV_ASSERT(!"Invalid Layout");
    }
}

void NvRmSurfaceRead(
    NvRmSurface *pSurf,
    NvU32 x,
    NvU32 y,
    NvU32 Width,
    NvU32 Height,
    void *pDstPixels)
{
    NvU8 *pDst = pDstPixels;
    NvU32 BitsPerPixel = NV_COLOR_GET_BPP(pSurf->ColorFormat);
    NvU32 Offset;
    const NvU8 *pSrc;
    NvBool Interlaced;
    NvU32 CurrentOffset;
    NvU32 Cury;
    Interlaced = (pSurf->DisplayScanFormat == NvDisplayScanFormat_Interlaced);
#if NVOS_IS_LINUX || NVOS_IS_QNX
    NvU32 Size = 0;
#endif

    // Make sure that the surface has data associated with it
    NV_ASSERT(pSurf->hMem || pSurf->pBase);

    // Make sure we don't fall off the end of the surface
    NV_ASSERT(x + Width  <= pSurf->Width);
    NV_ASSERT(y + Height <= pSurf->Height);

    // Convert x and Width from units of pixels to units of bytes
    // We assert here that the rectangle is aligned to byte boundaries
    x *= BitsPerPixel;
    NV_ASSERT(!(x & 7));
    x >>= 3;

    Width *= BitsPerPixel;
    NV_ASSERT(!(Width & 7));
    Width >>= 3;

    if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
        NvU32 SecondFieldOffset = pSurf->SecondFieldOffset - pSurf->Offset;
        if (pSurf->hMem)
        {
            // RM alloc'd memory
            Offset = pSurf->Offset + y*pSurf->Pitch + x;
            if (Interlaced)
            {
                Cury = 0;
                for ( ; Cury < Height; Cury++)
                {
                    if ((Cury + y) & 1)
                        CurrentOffset = Offset +
                            (Cury >> 1) * (pSurf->Pitch) + SecondFieldOffset;
                    else
                        CurrentOffset = Offset + (Cury >> 1) * (pSurf->Pitch);
                    NvRmMemRead(pSurf->hMem, CurrentOffset, pDst, Width);
                    pDst += Width;
                }
            }
            else
            {
                NvRmMemReadStrided(
                    pSurf->hMem, Offset, pSurf->Pitch, // source
                    pDst, Width, // destination
                    Width, Height); // elementsize, count
            }
        }
        else
        {
            // non-RM alloc'd memory
            pSrc = (NvU8 *)pSurf->pBase + y*pSurf->Pitch + x;
            if (Interlaced)
            {
                Cury = 0;
                for ( ; Cury < Height; Cury++)
                {
                    if ((Cury + y) & 1)
                        pSrc = pSrc +(Cury >> 1) * (pSurf->Pitch) + SecondFieldOffset;
                    else
                        pSrc = pSrc + (Cury >> 1) * (pSurf->Pitch);

                     NvOsMemcpy(pDst, pSrc, Width);
                     pDst += Width;
                     pSrc = (NvU8 *)pSurf->pBase + y*pSurf->Pitch + x;
                }
            }
            else
            {
                while (Height--)
                {
                    NvOsMemcpy(pDst, pSrc, Width);
                    pDst += Width;
                    pSrc += pSurf->Pitch;
                }
            }
        }
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Tiled)
    {
        NvU32 WidthStart, MiddleTiles, WidthEnd;
        void* UserPtr = pSurf->pBase;

        // Determine width of initial chunk to get us to a subtile-aligned spot
        WidthStart = 0;
        if (x & (NVRM_SURFACE_SUB_TILE_WIDTH-1))
        {
            WidthStart = NV_MIN(Width,
                NVRM_SURFACE_SUB_TILE_WIDTH - (x & (NVRM_SURFACE_SUB_TILE_WIDTH-1)));
            Width -= WidthStart;
        }
        NV_ASSERT(WidthStart < NVRM_SURFACE_SUB_TILE_WIDTH);

        // Determine number of full middle tiles and extra pixels at the end
        MiddleTiles = Width >> NVRM_SURFACE_SUB_TILE_WIDTH_LOG2;
        WidthEnd = Width & (NVRM_SURFACE_SUB_TILE_WIDTH-1);

#if NVOS_IS_LINUX || NVOS_IS_QNX
        // On Android/Linux and QNX, NvRmRead calls have a considerable overhead.
        // For large surfaces repeated calls kill performance. It is faster to
        // just do three kernel calls, map, flush cache, and unmap, and un-tile
        // using memcpy. If mapping fails, we fall back to mem read.
        // For WinCE 6, mapping memory is much more expensive, so it always
        // immediately uses the fall-back.
        if (!UserPtr)
        {
            Size = NvRmSurfaceComputeSize(pSurf);
            // In tests copying from 5mb surfaces (to normal memory) comparing WB
            // with NvRmMemCacheMaint(~, ~, ~, NV_FALSE, NV_TRUE) and IWB with
            // NvRmMemCacheMaint(~, ~, ~, NV_FALSE, NV_TRUE), comparing the entire
            // time: map, cache maintenance, copy, cache maintenance, unmap;
            // then the WB method actually takes a longer than IWB by about 20-40%.
            NvError err = NvRmMemMap(pSurf->hMem,
                                     pSurf->Offset,
                                     Size,
                                     NVOS_MEM_READ|NVOS_MEM_MAP_INNER_WRITEBACK,
                                     &UserPtr);
            if (err != NvSuccess)
                UserPtr = NULL;
            else
                NvRmMemCacheMaint(pSurf->hMem, UserPtr, Size, NV_FALSE, NV_TRUE);
        }
#endif // NVOS_IS_LINUX || NVOS_IS_QNX

        if (!UserPtr)
        {
            // RM alloc'd memory
            while (Height--)
            {
                // Get address of first pixel in row
                Offset = pSurf->Offset + TiledAddr(pSurf, x, y);
                y++;

                // Read fractional part of first tile (if any)
                if (WidthStart)
                {
                    NvRmMemRead(pSurf->hMem, Offset, pDst, WidthStart);
                    pDst += WidthStart;
                    Offset += WidthStart - NVRM_SURFACE_SUB_TILE_WIDTH + SUBTILE_BYTES;
                }

                // Read the middle tiles
                if (MiddleTiles)
                {
                    NvRmMemReadStrided(pSurf->hMem,
                                       Offset,
                                       SUBTILE_BYTES, // src stride
                                       pDst,
                                       NVRM_SURFACE_SUB_TILE_WIDTH, // dest stride
                                       NVRM_SURFACE_SUB_TILE_WIDTH, // element size
                                       MiddleTiles); // count

                    Offset += MiddleTiles * SUBTILE_BYTES;
                    pDst += MiddleTiles * NVRM_SURFACE_SUB_TILE_WIDTH;
                }

                // Read fractional part of ending tile (if any)
                if (WidthEnd)
                {
                    NvRmMemRead(pSurf->hMem, Offset, pDst, WidthEnd);
                    pDst += WidthEnd;
                }
            }
        }
        else
        {
            NvU32 n;

            // non-RM alloc'd memory
            while (Height--)
            {
                // Get address of first pixel in row
                pSrc = (const NvU8 *)UserPtr + TiledAddr(pSurf, x, y);
                y++;

                // Read fractional part of first tile (if any)
                if (WidthStart)
                {
                    NvOsMemcpy(pDst, pSrc, WidthStart);
                    pDst += WidthStart;
                    pSrc += WidthStart - NVRM_SURFACE_SUB_TILE_WIDTH + SUBTILE_BYTES;
                }

                // Read the middle tiles
                n = MiddleTiles;
                while (n--)
                {
                    // Compute middle tile (x,y) offset address, then read segment
                    NvOsMemcpy(pDst, pSrc, NVRM_SURFACE_SUB_TILE_WIDTH);
                    pDst += NVRM_SURFACE_SUB_TILE_WIDTH;
                    pSrc += SUBTILE_BYTES;
                }

                // Read fractional part of ending tile (if any)
                if (WidthEnd)
                {
                    NvOsMemcpy(pDst, pSrc, WidthEnd);
                    pDst += WidthEnd;
                }
            }
        }
#if NVOS_IS_LINUX || NVOS_IS_QNX
        // We may have mapped pointer into user process previously. If so, unmap it.
        if (UserPtr && pSurf->hMem)
        {
            /* Cache Invalidate/Flush before unmap doesn't ensure that
             * data won't be in cache until next access. Data can get into
             * cache because of speculative fetches any time, if the memory
             * accessed here is WB/IWB.
             * It should be invalidated again before accessing it, after H/W
             * writes to memory.
             * It should be flushed before handing over to H/W, after CPU writes.
             * So, It is not worth invalidating here.
             */
            NvRmMemUnmap(pSurf->hMem, UserPtr, NvRmSurfaceComputeSize(pSurf));
        }
#endif // NVOS_IS_LINUX || NVOS_IS_QNX
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        NvU32 curx;
        void* UserPtr = pSurf->pBase;
        NvU32  SecondFieldOffset = (pSurf->SecondFieldOffset - pSurf->Offset);
        NvU32 Size = NvRmSurfaceComputeSize(pSurf);

        if (!UserPtr)
        {
            NvError err = NvRmMemMap(pSurf->hMem,
                                     pSurf->Offset,
                                     Size,
                                     NVOS_MEM_READ,
                                     &UserPtr);
            if (err == NvSuccess)
            {
                NvRmMemCacheSyncForCpu(pSurf->hMem, UserPtr, Size);
            }
            else
            {
                UserPtr = NULL;
            }
        }

        for (Cury = y; Cury < y + Height; Cury++)
        {
            for (curx = x; curx < x + Width; curx++)
            {
                Offset = BlockLinearAddr(pSurf, curx, (Interlaced ? (Cury >> 1) : Cury));
                if (Interlaced)
                    Offset += ((Cury & 1) ? SecondFieldOffset : 0);

                if (UserPtr)
                {
                    pSrc = (const NvU8*)UserPtr + Offset;
                    *pDst = *pSrc;
                }
                else
                {
                    NvRmMemRead(pSurf->hMem, Offset, pDst, 1);
                }
                pDst++;
            }
        }
        if (UserPtr && pSurf->hMem)
        {
            NvRmMemCacheSyncForDevice(pSurf->hMem, UserPtr, Size);
            NvRmMemUnmap(pSurf->hMem, UserPtr, Size);
        }
    }
    else
    {
        NV_ASSERT(!"Invalid Layout");
    }
}
#endif

NvYuvColorFormat NvRmSurfaceGetYuvColorFormat(NvRmSurface **surf, NvU32 numSurfaces)
{
    NV_ASSERT(surf[0]->ColorFormat == NvColorFormat_Y8);

    if (numSurfaces == 3)
    {
        NvU32 chromaWidth = surf[1]->Width;
        NvU32 chromaHeight = surf[1]->Height;
        NvU32 lumaWidth = surf[0]->Width;
        NvU32 lumaHeight = surf[0]->Height;

        /* YUV:420  - chroma width and height will be half the width and height of luma surface */
        if (((((lumaWidth + 0x1) & ~0x1) >> 1) == chromaWidth) &&
            ((((lumaHeight + 0x1) & ~0x1) >> 1) == chromaHeight))
        {
            return NvYuvColorFormat_YUV420;
        } else
        /* YUV:422  - chroma width be half the width of luma surface,
         * height being the same  */
        if (((((lumaWidth + 0x1) & ~0x1) >> 1) == chromaWidth) &&
            (lumaHeight  == chromaHeight))
        {
            return NvYuvColorFormat_YUV422;
        } else
        /* YUV:422R  - chroma height be half the height of luma surface,
         * width being the same  */
        if (((((lumaHeight + 0x1) & ~0x1) >> 1) == chromaHeight) &&
            (lumaWidth  == chromaWidth))
        {
            return NvYuvColorFormat_YUV422R;
        } else
        /* YUV:444  - chroma height and width are same as luma width and height
         * */
        if ((lumaHeight == chromaHeight) && (lumaWidth == chromaWidth))
        {
            return NvYuvColorFormat_YUV444;
        } else
        {
            return NvYuvColorFormat_Unspecified;
        }
    }
    return NvYuvColorFormat_Unspecified;
}

static NV_INLINE NvU32 AlignValue(NvU32 value, NvU32 alignment)
{
    return (value + (alignment-1)) & ~(alignment-1);
}

static NV_INLINE NvBool
IsNpot(NvU32 val)
{
    return !NV_IS_POWER_OF_2(val);
}

static NV_INLINE NvBool
HasAurora(void)
{
    NvRmChipIdLimited ChipId = NvRmPrivGetChipIdLimited();
    if (ChipId.Id == NVRM_AP20_ID || ChipId.Id == NVRM_T30_ID ||
        ChipId.Id == NVRM_T114_ID || ChipId.Id == NVRM_T148_ID)
        return NV_TRUE;
    return NV_FALSE;
}

NvRmSurfaceLayout NvRmSurfaceGetDefaultLayout(void)
{
    NvError err;
    char buffer[20];
    NvRmSurfaceLayout layout = NvRmSurfaceLayout_Pitch;
    NvBool tiledLayout = NV_FALSE;
    NvBool tiledLinearTextures = NV_FALSE;
    NvBool blocklinearLayout = NV_FALSE;

    NvRmChipGetCapabilityBool(
            NvRmChipCapability_System_TiledLayout,
            &tiledLayout);

    NvRmChipGetCapabilityBool(
            NvRmChipCapability_GPU_TiledLinearTextures,
            &tiledLinearTextures);

    // Make tiled layout the default on T114
    if (tiledLayout && tiledLinearTextures)
    {
        layout = NvRmSurfaceLayout_Tiled;
    }

    NvRmChipGetCapabilityBool(
            NvRmChipCapability_System_BlocklinearLayout,
            &blocklinearLayout);

    // Make blocklinear layout the default on T124
    if (blocklinearLayout)
    {
        layout = NvRmSurfaceLayout_Blocklinear;
    }

    // Allow overriding with system properties
    err = NvOsGetConfigString("default_layout", buffer, sizeof(buffer));
    if (err == NvSuccess && buffer[0])
    {
        if (NvOsStrcmp(buffer, "linear") == 0 || NvOsStrcmp(buffer, "pitch") == 0)
        {
            layout = NvRmSurfaceLayout_Pitch;
        }
        else if (tiledLayout && NvOsStrcmp(buffer, "tiled") == 0)
        {
            layout = NvRmSurfaceLayout_Tiled;
        }
        else if (blocklinearLayout && NvOsStrcmp(buffer, "blocklinear") == 0)
        {
            layout = NvRmSurfaceLayout_Blocklinear;
        }
        else
        {
            NvOsDebugPrintf("Unrecognized default layout: %s\n", buffer);
        }
    }
    return layout;
}

// FR (GR2D Fast Rotate) requires 16-byte width and height alignment
#define COMPUTE_FR_ALIGNMENT(format) \
    (128 / NV_COLOR_GET_BPP(format))

// FR requires 16-row height alignment for 8-bit formats.
#define NVRM_SURFACE_LINEAR_HEIGHT_ALIGN_POT    16
// 3D NPOT pitchlinear textures require 16-row height alignment.
#define NVRM_SURFACE_LINEAR_HEIGHT_ALIGN_NPOT   16
// Worst-case height requirements come from VIC
#define NVRM_SURFACE_LINEAR_HEIGHT_ALIGN        4
#define NVRM_SURFACE_BLOCKLINEAR_GOB_HEIGHT     8

static NvU32 getBlockLinearHeight(NvRmSurface *pSurf)
{
    NvU32 AlignedHeight;

    if (pSurf->DisplayScanFormat == NvDisplayScanFormat_Interlaced)
        AlignedHeight = AlignValue((pSurf->Height >> 1),
            (NVRM_SURFACE_BLOCKLINEAR_GOB_HEIGHT * (1 << pSurf->BlockHeightLog2)));
    else
        AlignedHeight = AlignValue(pSurf->Height,
            (NVRM_SURFACE_BLOCKLINEAR_GOB_HEIGHT * (1 << pSurf->BlockHeightLog2)));

    return AlignedHeight;
}

static NvU32 getPitchLinearHeight(NvRmSurface *pSurf)
{
    int extraReadRow = (pSurf->Height > 0) ? 1 : 0;
    NvU32 AlignedHeight = 0;
    if (!HasAurora())
    {
        AlignedHeight = AlignValue(pSurf->Height,
            NVRM_SURFACE_LINEAR_HEIGHT_ALIGN);
    }
    else
    {
         NvU32 Npot = IsNpot(pSurf->Width) || IsNpot(pSurf->Height);
         AlignedHeight = AlignValue(pSurf->Height + extraReadRow,
             Npot ? NVRM_SURFACE_LINEAR_HEIGHT_ALIGN_NPOT
             : NVRM_SURFACE_LINEAR_HEIGHT_ALIGN_POT);
    }
    return AlignedHeight;
}

// Preferred page size is 128k.
#define NVRM_SURFACE_PREFERRED_PAGE_SIZE (128*1024)

NvU32 NvRmSurfaceComputeSize(NvRmSurface *pSurf)
{
    NvU32 NumBytes;
    NvU32 AlignedHeight = pSurf->Height;

    if (pSurf->Width == 0 ||
        pSurf->Height == 0 ||
        NV_COLOR_GET_BPP(pSurf->ColorFormat) == 0)
        return 0;

    // WAR, Bug 1190396: looks like VCAA (on T30) is accessing outside
    // Allocate one extra swath height if this is a coverage
    // buffer
    if (pSurf->ColorFormat == NvColorFormat_X4C4) {
        AlignedHeight += 16; // swath size is 16
    }

    // Align for GR2D Fast Rotate
    AlignedHeight = AlignValue(AlignedHeight, COMPUTE_FR_ALIGNMENT(pSurf->ColorFormat));

    switch (pSurf->Layout)
    {
        case NvRmSurfaceLayout_Pitch:
        {
            // Align for 3D NPOT textures
            if (IsNpot(pSurf->Width) || IsNpot(pSurf->Height))
                AlignedHeight = AlignValue(AlignedHeight, NVRM_SURFACE_LINEAR_HEIGHT_ALIGN_NPOT);

            break;
        }
        case NvRmSurfaceLayout_Tiled:
        {
            // Round height up to a multiple of NVRM_SURFACE_TILE_HEIGHT lines
            AlignedHeight = AlignValue(AlignedHeight, NVRM_SURFACE_TILE_HEIGHT);

            // Pitch must be a multiple of NVRM_SURFACE_TILE_WIDTH bytes
            NV_ASSERT(!(pSurf->Pitch & (NVRM_SURFACE_TILE_WIDTH-1)));

            break;
        }
        case NvRmSurfaceLayout_Blocklinear:
        {
            AlignedHeight = getBlockLinearHeight(pSurf);
            break;
        }
        default:
        {
            NV_ASSERT(!"Invalid Layout");
            return 0;
        }
    }

    NumBytes = pSurf->Pitch * AlignedHeight;
    // TODO: Layout check required only to work around so that
    // sanity for the older chips than T124 are not broken
    if ((pSurf->Layout == NvRmSurfaceLayout_Blocklinear) &&
       (pSurf->DisplayScanFormat == NvDisplayScanFormat_Interlaced))
    {
        NV_ASSERT(pSurf->SecondFieldOffset);
        NumBytes += pSurf->SecondFieldOffset - pSurf->Offset;
    }

    // GR2D reads groups of 4 pixels (2x2) and may read memory outside the
    // surface area causing errors when reading memory mapped into the SMMU.
    // Adding extra row and a pixel fixes these read errors (MC DECERR).
    NumBytes += pSurf->Pitch + (NV_COLOR_GET_BPP(pSurf->ColorFormat) >> 3);

    // For non-Aurora chips, pad the allocation to the preferred page size.
    // This will reduce the required page table size (see discussion in bug
    // 1321091), and also acts as a WAR for bug 1325421.
    if (!HasAurora())
    {
        NumBytes = AlignValue(NumBytes, NVRM_SURFACE_PREFERRED_PAGE_SIZE);
    }

    return NumBytes;
}

// Values pulled from AP15's project.h file.  If these values change,
// we will either need to update them or create a new tiling format.
#define NV_MC_TILE_BALIGN       256

#define NVRM_SURFACE_LINEAR_BASE_ALIGN_POT      256
// 3D NPOT (untiled unswizzled) textures require 1024-alignment.
#define NVRM_SURFACE_LINEAR_BASE_ALIGN_NPOT     1024

// 256B alignment required to support 40-bit clients.
#define NVRM_SURFACE_LINEAR_BASE_ALIGN          256
// 1 GOB (512B) required for block linear, 8kB preferred for perf.
#define NVRM_SURFACE_BLOCKLINEAR_BASE_ALIGN     8192
// 128 kB required for compression (big gmmu page size)
#define NVRM_SURFACE_BLOCKLINEAR_COMPRESSED_BASE_ALIGN     (128*1024)

NvU32 NvRmSurfaceComputeAlignment(NvRmDeviceHandle hDevice, NvRmSurface *pSurf)
{
    NvU32 Alignment = 0;

    if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
        if (!HasAurora())
        {
            Alignment = NVRM_SURFACE_LINEAR_BASE_ALIGN;
        }
        else
        {
            NvBool Npot = IsNpot(pSurf->Width) || IsNpot(pSurf->Height);
            Alignment = Npot ? NVRM_SURFACE_LINEAR_BASE_ALIGN_NPOT
                             : NVRM_SURFACE_LINEAR_BASE_ALIGN_POT;
        }
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Tiled)
    {
        /**
         * Recommended tiled surface alignment (bytes) in MC
         * (see "Programmers Guide to Tiling in AP15")
         */
        Alignment = NV_MC_TILE_BALIGN;
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        Alignment = NVRM_SURFACE_BLOCKLINEAR_BASE_ALIGN;
        if (NvRmMemKindIsCompressible(pSurf->Kind))
        {
            NV_ASSERT(pSurf->Kind == NvRmMemKind_C32_2CRA);
            Alignment = NVRM_SURFACE_BLOCKLINEAR_COMPRESSED_BASE_ALIGN;
        }
    }
    else
    {
        NV_ASSERT(!"Invalid Layout");
    }
    return Alignment;
}

// Values pulled from AP15's project.h file.  If these values change,
// we will either need to update them or create a new tiling format.
#define NV_MC_TILE_MAXK         15
#define NV_MC_TILE_MAXN         6
#define NV_MC_TILE_PCONSTLOG2   6
#define NV_MC_TILE_SIZE         1024

// derived values
#define NVRM_SURFACE_TILE_MAX_PITCH \
            (NV_MC_TILE_MAXK << (NV_MC_TILE_PCONSTLOG2 + NV_MC_TILE_MAXN))

// These are strict requirements for GR3D (untiled unswizzled) textures.
// For other T20/30 units, 16-byte pitch alignment is sufficient,
// although 32-byte alignment gives better performance.
#define NVRM_SURFACE_LINEAR_PITCH_ALIGN_EXACT_POT   16
#define NVRM_SURFACE_LINEAR_PITCH_ALIGN_EXACT_NPOT  64

// 256B is VIC's worst-case requirement for pitch surfaces
#define NVRM_SURFACE_LINEAR_PITCH_ALIGN             256
// For block linear the pitch is not programmable in HW, it is always aligned
// to a GOB.
#define NVRM_SURFACE_BLOCKLINEAR_PITCH_ALIGN_EXACT  64

static NvU32 getTiledPitch(NvU32 pitch, NvU32 flags)
{
    NvU32 k = (pitch + (NVRM_SURFACE_TILE_WIDTH-1)) / NVRM_SURFACE_TILE_WIDTH;
    NvU32 n = NV_MC_TILE_PCONSTLOG2;
    NvU32 maxn = NV_MC_TILE_PCONSTLOG2 + NV_MC_TILE_MAXN;
    NvBool k_is_valid = NV_FALSE;
    NvU32 mask;

    if (pitch == 0)
        return pitch;       // allow a pitch of 0 to be specified

    NV_ASSERT(pitch <= NVRM_SURFACE_TILE_MAX_PITCH);

    if (!(flags & NV_RM_SURFACE_TILE_PITCH_NON_RESTRICTIVE)) {

        while (!k_is_valid)
        {
            // k needs to be an odd number
            if (!(k & 1))
            {
                if (k > NV_MC_TILE_MAXK)
                {
                    k >>= 1;
                    n++;
                }
                else
                {
                    // remaining bits that can be shifted out before n exceeds maxn
                    mask = (1 << ((maxn + 1) - n)) - 1;
                    if (k & mask)
                    {
                        // factor out all powers of 2, since an odd k will be reached
                        // before exceeding maxn.
                        while (!(k & 1))
                        {
                            k >>= 1;
                            n++;
                        }
                    }
                    else
                    {
                        // can't factor out all powers of 2, before n exceeds maxn
                        // so just add 1 to k to make it odd (yields the smallest
                        // needed padding to get to an odd pitch).
                        k++;
                    }
                }
            }
            else if (k > NV_MC_TILE_MAXK)
            {
                k = (k + 1) >> 1;
                n++;
            }
            else
                k_is_valid = NV_TRUE;
        }

        if ((pitch > (k << n)) || !(k & 1) || (k > NV_MC_TILE_MAXK) || (n > NV_MC_TILE_PCONSTLOG2 + NV_MC_TILE_MAXN))
        {
            NV_ASSERT( !"Error: failed to find tiled pitch" );
        }
        pitch = (k << n);
    }
    else
    {
        // round up to nearest tile boundary
        pitch = AlignValue(pitch, NVRM_SURFACE_TILE_WIDTH);
    }

    NV_ASSERT(pitch <= NVRM_SURFACE_TILE_MAX_PITCH);
    return pitch;
}


void NvRmSurfaceComputePitch(
    NvRmDeviceHandle hDevice,
    NvU32 flags,
    NvRmSurface *pSurf)
{
    NvU32 Pitch;

    NV_ASSERT(pSurf);
    /* Calculate Pitch & align (by adding pad bytes) */
    if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
        Pitch = pSurf->Width * NV_COLOR_GET_BPP(pSurf->ColorFormat);
        Pitch = (Pitch + 7) >> 3;
        if (!HasAurora())
        {
            Pitch = AlignValue(Pitch, NVRM_SURFACE_LINEAR_PITCH_ALIGN);
        }
        else
        {
            NvU32 Npot = IsNpot(pSurf->Width) || IsNpot(pSurf->Height);
            Pitch = AlignValue(Pitch, Npot ? NVRM_SURFACE_LINEAR_PITCH_ALIGN_EXACT_NPOT
                                           : NVRM_SURFACE_LINEAR_PITCH_ALIGN_EXACT_POT);
        }
        pSurf->Pitch = Pitch;
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Tiled)
    {
        Pitch = pSurf->Width * NV_COLOR_GET_BPP(pSurf->ColorFormat);
        Pitch = (Pitch + 7) >> 3;
        pSurf->Pitch = getTiledPitch(Pitch, flags);
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        Pitch = pSurf->Width * NV_COLOR_GET_BPP(pSurf->ColorFormat);
        Pitch = (Pitch + 7) >> 3;
        Pitch = AlignValue(Pitch, NVRM_SURFACE_BLOCKLINEAR_PITCH_ALIGN_EXACT);
        pSurf->Pitch = Pitch;
    }
    else
    {
        NV_ASSERT(!"Invalid Layout");
    }
    return;
}

void NvRmSurfaceComputeSecondFieldOffset(
    NvRmDeviceHandle hDevice,
    NvRmSurface *pSurf)
{
    NV_ASSERT(pSurf);
    NV_ASSERT(pSurf->DisplayScanFormat == NvDisplayScanFormat_Interlaced);

    if (pSurf->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        NvU32 AlignedHeight = getBlockLinearHeight(pSurf);
        NvU32 SecondFieldOffset = pSurf->Offset + (AlignedHeight * pSurf->Pitch);
        SecondFieldOffset = AlignValue(SecondFieldOffset,NVRM_SURFACE_BLOCKLINEAR_BASE_ALIGN);
        pSurf->SecondFieldOffset = SecondFieldOffset;
    }
    else if (pSurf->Layout == NvRmSurfaceLayout_Pitch)
    {
         NvU32 Alignment = NvRmSurfaceComputeAlignment(hDevice, pSurf);
         NvU32 AlignedHeight = getPitchLinearHeight(pSurf);
         NvU32 SecondFieldOffset = (pSurf->Offset + ((AlignedHeight * pSurf->Pitch) >> 1));
         SecondFieldOffset = AlignValue(SecondFieldOffset, Alignment);
         pSurf->SecondFieldOffset = SecondFieldOffset;
    }
    else
    {
        NV_ASSERT(!"Invalid Layout");
    }
    return;
}

// Adapted from
// $TEGRA_TOP/gpu/drv/drivers/common/src/nvBlockLinear.c:nvBlockLinearShrinkBlock_512BGOB
static NvU32 NvRmSurfaceShrinkBlockDim(NvU32 BlockDimLog2, NvU32 ImageDim, NvU32 GobDim)
{
    // If we have >1 gob/block, we will shrink the block size in half and see
    // if the image still fits inside the block. If it does, we repeat until
    // (a) the proposed block size is too small, or
    // (b) we hit 1 gob/block in that dimension.
    if (BlockDimLog2 > 0)
    {
        // Compute a new proposedBlockSize, based on a gob size that is half
        // of the current value (log2 - 1). The if statement above keeps this
        // value always >= 0.
        NvU32 proposedBlockSize = GobDim << (BlockDimLog2 - 1);

        while (proposedBlockSize >= ImageDim)
        {
            // It's safe to cut the gobs per block in half.
            BlockDimLog2--;

            if (BlockDimLog2 == 0)
            {
                break;
            }
            proposedBlockSize /= 2;
        }
    }
    return BlockDimLog2;
}

void NvRmSurfaceSetup(
    NvRmSurface *Surface,
    NvU32 Width,
    NvU32 Height,
    NvColorFormat ColorFormat,
    const NvU32 *Attributes)
{
    NvU32 blockHeightLog2 = 4;
    NvBool compression = NV_FALSE;
    NvU32 computePitchFlags = 0;
    NvU32 unitSupport = NvRmSurfaceUnitSupport_All;

    NvOsMemset(Surface, 0, sizeof(*Surface));
    Surface->Width = Width;
    Surface->Height = Height;
    Surface->ColorFormat = ColorFormat;
    Surface->Layout = NvRmSurfaceGetDefaultLayout();
    Surface->DisplayScanFormat = NvDisplayScanFormat_Progressive;

    while (Attributes && *Attributes != NvRmSurfaceAttribute_None)
    {
        NvRmSurfaceAttribute a = Attributes[0];
        NvU32 v = Attributes[1];
        switch (a)
        {
            case NvRmSurfaceAttribute_Layout:
                Surface->Layout = (NvRmSurfaceLayout)v;
                break;
            case NvRmSurfaceAttribute_DisplayScanFormat:
                Surface->DisplayScanFormat = (NvDisplayScanFormat)v;
                break;
            case NvRmSurfaceAttribute_OptimizeForDisplay:
                blockHeightLog2 = 0;
                break;
            case NvRmSurfaceAttribute_Compression:
                compression = v;
                break;
            case NvRmSurfaceAttribute_UnitSupport:
                unitSupport = v;
                break;
            default:
                NV_ASSERT(!"Invalid attribute");
                break;
        }
        Attributes += 2;
    }

    switch (Surface->Layout)
    {
        case NvRmSurfaceLayout_Blocklinear:
            Surface->Kind = NvRmMemKind_Generic_16Bx2;
            if (compression)
            {
                switch (NV_COLOR_GET_BPP(Surface->ColorFormat))
                {
                    case 8:
                    case 16:
                        break;
                    case 32:
                        Surface->Kind = NvRmMemKind_C32_2CRA;
                        break;
                    default:
                        NV_ASSERT(!"Not implemented");
                        break;
                }
            }
            Surface->BlockHeightLog2 = blockHeightLog2;
            // Texture units require shrunken block layout.
            // Technically, we'd need to shrink the block size only if
            // (unitSupport & NvRmSurfaceUnitSupport_Gpu) != 0, but we
            // choose to shrink always to keep things simpler. We may
            // need to revisit this decision if it has perf implications.
            Surface->BlockHeightLog2 = NvRmSurfaceShrinkBlockDim(
                    Surface->BlockHeightLog2,
                    Surface->Height,
                    NVRM_SURFACE_BLOCKLINEAR_GOB_HEIGHT);
            break;
        case NvRmSurfaceLayout_Pitch:
            Surface->Kind = NvRmMemKind_Pitch;
            break;
        case NvRmSurfaceLayout_Tiled:
            if (!(unitSupport & NvRmSurfaceUnitSupport_2D))
            {
                // Don't need to support Gr2d, use 3d-native pitch
                computePitchFlags |= NV_RM_SURFACE_TILE_PITCH_NON_RESTRICTIVE;
            }
            break;
        default:
            NV_ASSERT(!"Invalid layout");
            break;
    }

    NvRmSurfaceComputePitch(0, computePitchFlags, Surface);

    if (Surface->DisplayScanFormat == NvDisplayScanFormat_Interlaced)
    {
        NvRmSurfaceComputeSecondFieldOffset(0, Surface);
    }
}

void NvRmMultiplanarSurfaceSetup(
    NvRmSurface *Surfaces,
    NvU32 NumSurfaces,
    NvU32 Width,
    NvU32 Height,
    NvYuvColorFormat YuvFormat,
    NvColorFormat *ColorFormats,
    const NvU32 *Attributes)
{
    unsigned int i;
    int ChromaScaleX = 1;
    int ChromaScaleY = 1;

    switch (YuvFormat)
    {
        case NvYuvColorFormat_YUV420:
            ChromaScaleX = 2;
            ChromaScaleY = 2;
            break;
        case NvYuvColorFormat_YUV422:
            ChromaScaleX = 2;
            break;
        case NvYuvColorFormat_YUV422R:
            ChromaScaleY = 2;
            break;
        case NvYuvColorFormat_YUV444:
        case NvYuvColorFormat_Unspecified:
            break;
        default:
            NV_ASSERT(!"Unsupported YuvFormat");
            break;
    }

    NV_ASSERT(Width % ChromaScaleX == 0);
    NV_ASSERT(Height % ChromaScaleY == 0);
    NV_ASSERT(NumSurfaces >= 1 && NumSurfaces <= 3);
    NV_ASSERT(YuvFormat != NvYuvColorFormat_Unspecified || NumSurfaces == 1);

    for (i = 0; i < NumSurfaces; i++)
    {
        int PlaneWidth = (i == 0) ? Width : Width / ChromaScaleX;
        int PlaneHeight = (i == 0) ? Height : Height / ChromaScaleY;

        NvRmSurfaceSetup(&Surfaces[i],
                         PlaneWidth,
                         PlaneHeight,
                         ColorFormats[i],
                         Attributes);
    }

    // Block height needs to be the same for all planes (see bug 1348810).
    // Since GPU sets a maximum limit on block height for small surfaces, we
    // set all planes to the value computed for chroma planes.
    for (i = 0; i < NumSurfaces; i++)
        Surfaces[i].BlockHeightLog2 = Surfaces[1].BlockHeightLog2;
}
