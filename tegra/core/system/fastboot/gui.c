/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvddk_disp.h"
#include "nvddk_disp_structure.h"
#include "fastboot.h"
#include "prettyprint.h"
#include "nvrm_gpio.h"
#include "nvodm_backlight.h"
#include "nvodm_query_gpio.h"

static NvRmDeviceHandle       s_hRm      = NULL;
static NvDdkDispHandle        s_hDdkDisp = NULL;
static NvDdkDispDisplayHandle s_hDisplay = NULL;
static NvDdkDispWindowHandle  s_hWindow = NULL;
static NvRmSurface            s_Frontbuffer;
PrettyPrintContext     s_PrintCtx;

static NvBool                 s_UseTE = NV_FALSE;

NvU32                         surf_size;
NvU32                         surf_address;
NvU32                         bl_nDispController;

#ifndef BL_DISP_CONTROLLER
#define BL_DISP_CONTROLLER 0
#endif

#define BMP_HEADER_LENGTH 54

#define SWAP(a, b) {                            \
    (a) ^= (b);                                 \
    (b) ^= (a);                                 \
    (a) ^= (b);                                 \
}

static void FillBMPHeader(BitmapFile *bmf, NvU16 *hdr)
{
    /* Fill bitmap file header */
    bmf->bfh.Magic = hdr[0];
    bmf->bfh.FileSize = hdr[1] | hdr[2] << 16;
    bmf->bfh.Reserved1 = hdr[3];
    bmf->bfh.Reserved2 = hdr[4];
    bmf->bfh.StartOffset = hdr[5] | hdr[6] << 16;

    /* Fill bitmap information header */
    bmf->bih.HeaderSize = hdr[7] | hdr[8] << 16;
    bmf->bih.Width = hdr[9] | hdr[10] << 16;
    bmf->bih.Height = hdr[11] | hdr[12] << 16;
    bmf->bih.Planes = hdr[13];
    bmf->bih.Depth = hdr[14] | hdr[15] << 16;
    bmf->bih.CompressionType = hdr[15] | hdr[16] << 16;
    bmf->bih.ImageSize = hdr[17] | hdr[18] << 16;
    bmf->bih.HorizontalResolution = hdr[19] | hdr[20] << 16;
    bmf->bih.VerticalResolution = hdr[21] | hdr[22] << 16;
    bmf->bih.NumColors = hdr[23] | hdr[24] << 16;
    bmf->bih.NumImpColors = hdr[25] | hdr[26] << 16;
}

NvError FastbootBMPRead(
    NvAbootHandle hAboot,
    BitmapFile **bmf,
    const char *PartName)
{
    NvError e = NvSuccess;
    BitmapFile tmp;
    NvU16 *hdr = NULL;
    NvU8 *bitmap = NULL;

    if ((!hAboot) || (!bmf) || (!PartName))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    if (*bmf)
    {
        NvOsFree(*bmf);
        *bmf = NULL;
    }

    hdr = NvOsAlloc(BMP_HEADER_LENGTH);
    if (!hdr)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    /* Read  header */
    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(hAboot, PartName, 0,
            BMP_HEADER_LENGTH, (NvU8 *)hdr));

    FillBMPHeader(&tmp, hdr);

    if (NvOsMemcmp((void *)&(tmp.bfh.Magic), "BM", NvOsStrlen("BM")))
    {
        e = NvError_NotSupported;
        NvOsDebugPrintf("(%s) Only BMP image is supported\n", __func__);
        goto fail;
    }

    if ((tmp.bfh.FileSize < 0) || (tmp.bih.ImageSize < 0))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    if (tmp.bih.ImageSize == 0)
        tmp.bih.ImageSize = tmp.bfh.FileSize - BMP_HEADER_LENGTH;

    *bmf = NvOsAlloc(sizeof(BitmapFileHeader) +
        sizeof(BitmapInformationHeader) + tmp.bih.ImageSize);
    if (!*bmf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    FillBMPHeader(*bmf, hdr);
    bitmap = ((NvU8*)*bmf) + sizeof(BitmapFileHeader) +
        sizeof(BitmapInformationHeader);
    /* Read whole bmp file */
    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(hAboot, PartName, BMP_HEADER_LENGTH,
            tmp.bih.ImageSize, bitmap));

fail:
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("(%s) Unsuccesful attempt to read BMP image\n", __func__);
        if (*bmf)
            NvOsFree(*bmf);
    }
    if (hdr)
        NvOsFree(hdr);
    return e;
}

NvError FastbootDrawBMP(
    BitmapFile *bmf,
    NvRmSurface *pSurface,
    void **pPixels)
{
    NvError e = NvSuccess;
    NvU32 DrawHeight = 0;
    NvU32 DrawWidth = 0;
    NvU32 *pTemp1 = NULL;
    NvU32 SurfaceOffset, ImageOffset;
    NvU32 x, y;
    NvU32 r, g, b;
    NvU32 color = 0xffffff;
    NvU32 BytesPerPixel = 0;
    NvU32 PixelOffset = 0;
    NvU32 ScreenX = 0 , ScreenY = 0;
    NvBool Flip = NV_FALSE;
    NvU32 Rotateangle;

    if ((!bmf) || (!pSurface) || (!*pPixels))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    if (bmf->bih.CompressionType)
    {
        e = NvError_NotSupported;
        NvOsDebugPrintf("(%s) Only uncompressed BMP image is supported\n", __func__);
        goto fail;
    }

    if ((bmf->bih.Height < 0) || (bmf->bih.Width < 0))
    {
        e = NvError_BadParameter;
        NvOsDebugPrintf("(%s) Invalid height or width in BMP image\n", __func__);
        goto fail;
    }

    // Get Panel details before setting up logistics
    Rotateangle = NvOdmQueryRotateBMP();
    switch (Rotateangle) {
    case 90:
    case 270:
        SWAP(pSurface->Width, pSurface->Height);
        Flip = NV_TRUE;
        break;
    default:
        break;
    }

    if (pSurface->Width > (NvU32)bmf->bih.Width)
        ScreenX = (pSurface->Width - bmf->bih.Width) / 2;

    if (pSurface->Height > (NvU32)bmf->bih.Height)
        ScreenY = (pSurface->Height - bmf->bih.Height) / 2;

    DrawHeight = NV_MIN(pSurface->Height, (NvU32)bmf->bih.Height);
    DrawWidth = NV_MIN(pSurface->Width, (NvU32)bmf->bih.Width);
    BytesPerPixel = bmf->bih.Depth / 8;

    pTemp1 = NvOsAlloc(DrawWidth * DrawHeight * sizeof(NvU32));
    NvOsMemset(pTemp1, 0, DrawWidth * DrawHeight * sizeof(NvU32));

    for (y = 0; y < DrawHeight; y++)
    {
        ImageOffset =  y * FASTBOOT_ALIGN(bmf->bih.Width *
            (bmf->bih.Depth / (8 * sizeof(NvU8))),  sizeof(NvU32));

        for (x = 0; x < DrawWidth; x++)
        {
            PixelOffset = ImageOffset + (x * BytesPerPixel);
            if (BytesPerPixel == 2)
            {
                color = bmf->BitmapData[PixelOffset] |
                    (bmf->BitmapData[PixelOffset + 1] << 8);
                r = color & 0x1f;
                g = (color >> 5) & 0x1f;
                b = (color >> 10) & 0x1f;
            }
            else if (BytesPerPixel == 3 || BytesPerPixel == 4)
            {
                r = bmf->BitmapData[PixelOffset];
                g = bmf->BitmapData[PixelOffset + 1];
                b = bmf->BitmapData[PixelOffset + 2];
            }
            else
            {
                e = NvError_NotSupported;
                NvOsDebugPrintf("(%s) Only 16,24 and 32 bits per pixel is \
                    supported\n", __func__);
                goto fail;
            }
            color = b | (g << 8) | (r << 16);

            if (Rotateangle == 90)
            {
                SurfaceOffset = x * DrawHeight;
                pTemp1[SurfaceOffset + y] = color;
            }
            else if (Rotateangle == 180)
            {
                SurfaceOffset = y * DrawWidth;
                pTemp1[SurfaceOffset + x] = color;
            }
            else if (Rotateangle == 270)
            {
                SurfaceOffset = (DrawWidth - x -1) * DrawHeight;
                pTemp1[SurfaceOffset + (DrawHeight - y - 1)] = color;
            }
            else
            {
                SurfaceOffset = (DrawHeight - y - 1) * DrawWidth;
                pTemp1[SurfaceOffset + x] = color;
            }

        }
    }

    if (Flip)
        NvRmSurfaceWrite(pSurface, ScreenX, ScreenY, DrawHeight,
            DrawWidth, pTemp1);
    else
        NvRmSurfaceWrite(pSurface, ScreenX, ScreenY, DrawWidth,
            DrawHeight, pTemp1);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("(%s) Unsuccesful attempt to draw BMP image\n", __func__);
    if (pTemp1 != NULL)
        NvOsFree(pTemp1);
    return e;
}

void FastbootDeinitializeDisplay(
    NvRmSurface *pFramebuffer,
    void       **pPixelData)
{
    NvRmSurface Surf = *pFramebuffer;

    if (!s_hDdkDisp || !s_hDisplay)
        return;

    /* Turn off the Backlight */
    NvDdkDispSetDisplayAttribute(s_hDisplay,
            NvDdkDispDisplayAttribute_Backlight,
            NvDdkDispBacklightControl_Off, 0 );

    if (*pPixelData && surf_address)
        NvRmMemUnmap(Surf.hMem, *pPixelData, surf_size);

    if (Surf.hMem)
    {
        NvRmMemUnpin(Surf.hMem);
        NvRmMemHandleFree(Surf.hMem);
    }
    *pPixelData = NULL;

    NvDdkDispAttachDisplay(NULL, s_hDisplay, 0);
    NvDdkDispClose(s_hDdkDisp, 0);
    s_hDisplay = NULL;
    s_hDdkDisp = NULL;
}

NvBool FastbootInitializeDisplay(
    NvU32        Flags,
    NvRmSurface *pFramebuffer,
    NvRmSurface *pFrontbuffer,
    void       **pPixelData,
    NvBool       UseHdmi)
{
    NvDdkDispControllerHandle hControllers[NVDDK_DISP_MAX_CONTROLLERS];
    NvDdkDispControllerHandle hController = NULL;
    NvDdkDispControllerHandle hController_temp;
    NvDdkDispWindowHandle     hWindow;
    void                     *ptr;
    NvDdkDispMode             current_mode;
    NvDdkDispMode             best_mode;
    NvDdkDispMode             Mode;
    NvRmSurface               Surf;
    NvDdkDispWindowAttribute  Attrs[] =
    {
        NvDdkDispWindowAttribute_DestRect_Left,
        NvDdkDispWindowAttribute_DestRect_Top,
        NvDdkDispWindowAttribute_DestRect_Right,
        NvDdkDispWindowAttribute_DestRect_Bottom,
        NvDdkDispWindowAttribute_SourceRect_Left,
        NvDdkDispWindowAttribute_SourceRect_Top,
        NvDdkDispWindowAttribute_SourceRect_Right,
        NvDdkDispWindowAttribute_SourceRect_Bottom,
    };
    NvU32                     Vals[NV_ARRAY_SIZE(Attrs)];
    NvU32                     Size;
    NvU32                     Align;
    NvU32                     n;
    NvU32                     nControllers;
    NvError                   e;
    NvU32                     cntCont;
    NvU32                     type;

    if (s_hDisplay)
        return NV_TRUE;
    s_PrintCtx.x = s_PrintCtx.y = 0;

    NV_CHECK_ERROR_CLEANUP(NvRmOpenNew(&s_hRm));
    NV_CHECK_ERROR_CLEANUP(NvDdkDispOpen(s_hRm, &s_hDdkDisp, 0));
    nControllers = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkDispListControllers(s_hDdkDisp, &nControllers,0)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvDdkDispListControllers(s_hDdkDisp, &nControllers, hControllers)
    );

    best_mode.width = 0;
    best_mode.height = 0;

    for (cntCont=0; cntCont<nControllers; cntCont++){
        hController_temp = hControllers[cntCont];
        NV_CHECK_ERROR_CLEANUP(
            NvDdkDispGetDisplayByUsage(hController_temp,
                NvDdkDispDisplayUsage_Primary, &s_hDisplay)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvDdkDispGetDisplayAttribute(s_hDisplay,
                NvDdkDispDisplayAttribute_Type, &type)
        );

        if ((type==NvDdkDispDisplayType_HDMI)||(type==NvDdkDispDisplayType_CRT))
        {
            NV_CHECK_ERROR_CLEANUP(
                NvDdkDispAttachDisplay(hController_temp, s_hDisplay, 0)
            );
            NV_CHECK_ERROR_CLEANUP(
                NvDdkDispGetBestMode(hController_temp, &current_mode, 0)
            );

            if ((current_mode.width > best_mode.width) &&
                (current_mode.height > best_mode.height))
            {
                hController = hControllers[cntCont];
                bl_nDispController = cntCont;
                best_mode = current_mode;
            }
            NV_CHECK_ERROR_CLEANUP(
                NvDdkDispAttachDisplay(NULL, s_hDisplay, 0)
            );
        }
        else
        {
            hController = hControllers[BL_DISP_CONTROLLER]; //Display Controller used
            bl_nDispController = BL_DISP_CONTROLLER;
            break;
        }
    }

    NV_CHECK_ERROR_CLEANUP(
        NvDdkDispGetDisplayByUsage(hController,
            NvDdkDispDisplayUsage_Primary, &s_hDisplay)
    );
    n = 1;
    NV_CHECK_ERROR_CLEANUP(NvDdkDispListWindows(hController, &n, &hWindow));
    NV_CHECK_ERROR_CLEANUP(NvDdkDispAttachDisplay(hController, s_hDisplay, 0));
    NV_CHECK_ERROR_CLEANUP(
        NvDdkDispGetBestMode(hController, &Mode, 0)
    );

    if( Mode.flags & NVDDK_DISP_MODE_FLAG_USE_TEARING_EFFECT )
    {
        s_UseTE = NV_TRUE;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkDispSetMode(hController, &Mode, 0));
    Vals[0] = Vals[1] = Vals[4] = Vals[5] = 0;
    Vals[2] = Vals[6] = Mode.width;
    Vals[3] = Vals[7] = Mode.height;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkDispSetWindowAttributes(hWindow, Attrs, Vals,
            NV_ARRAY_SIZE(Attrs), 0)
    );

    NvOsMemset(&Surf, 0, sizeof(Surf));
    Surf.Width = Mode.width;
    Surf.Height = Mode.height;
    if (Flags & FRAMEBUFFER_DOUBLE)
        Surf.Height*=2;

    /* In order to make bootloader and kernel use the same color format, set it
     * to R8G8B8A8 if color depth is 32-bit. Otherwise, set it to B5G6R5. Note
     * on color formats: nvcolor.h specifies colors in msb-first whilst the
     * hardware specifies colors lsb-first. Therefore, NvColorFormat_A8B8G8R8 is
     * the same as DC_WIN_B_COLOR_DEPTH_0_B_COLOR_DEPTH_R8G8B8A8.
     * */
    if (Flags & FRAMEBUFFER_32BIT)
        Surf.ColorFormat = NvColorFormat_A8B8G8R8;
    else
        Surf.ColorFormat = NvColorFormat_R5G6B5;
    Surf.Layout = NvRmSurfaceLayout_Pitch;

    //  dev/fb expects that the framebuffer surface is 4K-aligned, so
    //  allocate with the maximum of the RM's computed alignment and 4K
    NvRmSurfaceComputePitch(s_hRm, 0, &Surf);
    Size = NvRmSurfaceComputeSize(&Surf);
    Align = NvRmSurfaceComputeAlignment(s_hRm, &Surf);
    Align = NV_MAX(4096, Align);

    NV_CHECK_ERROR_CLEANUP(NvRmMemHandleAlloc(s_hRm, NULL, 0, Align,
        NvOsMemAttribute_WriteCombined,
        Size, NVRM_MEM_TAG_SYSTEM_MISC, 1, &Surf.hMem));
    surf_size = Size;
    surf_address = NvRmMemPin(Surf.hMem);

    NV_CHECK_ERROR_CLEANUP(NvRmMemMap(Surf.hMem, 0, Size, 0, &ptr));

    if (Flags & FRAMEBUFFER_CLEAR)
        NvOsMemset(ptr, 0, Size);

    s_Frontbuffer = Surf;
    if (Flags & FRAMEBUFFER_DOUBLE)
        s_Frontbuffer.Height /= 2;

    NV_CHECK_ERROR_CLEANUP(
        NvDdkDispSetWindowSurface(hWindow, &s_Frontbuffer, 1, 0)
    );

    s_hWindow = hWindow;
    *pFramebuffer = Surf;
    *pFrontbuffer = s_Frontbuffer;
    *pPixelData   = ptr;

    return NV_TRUE;

 fail:
    return NV_FALSE;
}

void
FramebufferUpdate( void )
{
    NvPoint pt;
    NvRect rec;

    if( s_UseTE == NV_FALSE )
    {
        return;
    }

    pt.x = 0;
    pt.y = 0;
    rec.top = 0;
    rec.left = 0;
    rec.right = s_Frontbuffer.Width;
    rec.bottom = s_Frontbuffer.Height;

    (void)NvDdkDispDisplayUpdate( s_hDisplay, s_hWindow, &s_Frontbuffer,
        1, &pt, &rec, 0, 0, NVDDK_DISP_WAIT );
}

static void DrawIcon(
    NvU8 *pPixels,
    const Icon *Image,
    NvU32 Xofs,
    NvU32 Yofs,
    NvU32 Pitch,
    NvColorFormat Format)
{
    NvU32 x, y;

    for (y=0; y<Image->height; y++)
    {
        for (x=0; x<Image->width; x++)
        {
            NvU32 color = Image->data[Image->width*y + x];
            if (Image->bpp >= 24 && Format == NvColorFormat_R5G6B5)
            {
                NvU32 r = (color>>16) & 0xff;
                NvU32 g = (color>>8) & 0xff;
                NvU32 b = color & 0xff;
                r >>= 3;
                g >>= 2;
                b >>= 3;
                color = (r<<11) | (g<<5) | b;
            }
            else if (Image->bpp >= 24 && Format == NvColorFormat_A8B8G8R8)
            {
                NvU32 r = (color>>16) & 0xff;
                NvU32 g = (color>>8) & 0xff;
                NvU32 b = color & 0xff;
                color = r | (g<<8) | (b<<16);
            }
            else if (Image->bpp == 16 && Format == NvColorFormat_A8R8G8B8)
            {
                NvU32 r = (color>>11) & 0x1f;
                NvU32 g = (color>>5) & 0x3f;
                NvU32 b = color & 0x1f;
                r = (r<<3) | (r>>2);
                g = (g<<2) | (g>>4);
                b = (b<<3) | (b>>2);
                color = (r<<16) | (g<<8) | b;
            }

            if ((Format == NvColorFormat_A8R8G8B8) ||
                (Format == NvColorFormat_A8B8G8R8))
                *(NvU32*)(pPixels + Pitch*(y+Yofs) + 4*(x+Xofs)) = color;
            else
                *(NvU16*)(pPixels + Pitch*(y+Yofs) + 2*(x+Xofs)) = (NvU16)color;
        }
    }

    FramebufferUpdate();
}

static void DrawBlendRoundRect(
    NvU8         *pPixels,
    NvU32         Pitch,
    NvColorFormat ImageFormat,
    NvU32         Xofs,
    NvU32         Yofs,
    NvU32         Width,
    NvU32         Height,
    NvU32         RectColor,
    NvU32         Alpha,
    NvU32         Bevel)
{
    NvU32 x, y;
    NvU32 rectr, rectg, rectb;

    rectr = (RectColor>>16) & 0xff;
    rectg = (RectColor>>8) & 0xff;
    rectb = RectColor & 0xff;

    for (y=0; y<Height; y++, Yofs++)
    {
        for (x=0; x<Width; x++)
        {
            NvU32 xPrime = NV_MIN(Width-x-1, x);
            NvU32 yPrime = NV_MIN(Height-y-1, y);
            NvU32 aPrime = Alpha;
            NvU32 color;
            NvU32 r, g, b;

            if (xPrime<=Bevel && yPrime<=Bevel)
            {
                xPrime = Bevel-xPrime;
                yPrime = Bevel-yPrime;
                if (xPrime*xPrime + yPrime*yPrime > Bevel*Bevel)
                    aPrime = 0;
            }

            if (ImageFormat == NvColorFormat_R5G6B5)
            {
                color = *(NvU16*)(pPixels + Pitch*Yofs + 2*(x+Xofs));
                r = (color >> 11) & 0x1f;
                r = (r<<3) | (r>>2);
                g = (color >> 5) & 0x3f;
                g = (g<<2) | (g>>4);
                b = color & 0x1f;
                b = (b<<3) | (b>>2);
            }
            else if (ImageFormat == NvColorFormat_A8R8G8B8)
            {
                color = *(NvU32*)(pPixels + Pitch*Yofs + 4*(x+Xofs));
                r = (color>>16) & 0xff;
                g = (color>>8) & 0xff;
                b = color & 0xff;
            }
            else
            {
                NV_ASSERT(ImageFormat == NvColorFormat_A8B8G8R8);
                color = *(NvU32*)(pPixels + Pitch*Yofs + 4*(x+Xofs));
                r = color & 0xff;
                g = (color>>8) & 0xff;
                b = (color>>16) & 0xff;
            }

            r = NV_MIN((r*0x100 + aPrime*rectr) >> 8, 0xff);
            g = NV_MIN((g*0x100 + aPrime*rectg) >> 8, 0xff);
            b = NV_MIN((b*0x100 + aPrime*rectb) >> 8, 0xff);

            if (ImageFormat == NvColorFormat_R5G6B5)
            {
                r = r>>3;
                g = g>>2;
                b = b>>3;
                color = (r<<11) | (g<<5) | b;
                *(NvU16*)(pPixels + Pitch*Yofs + 2*(x+Xofs)) = (NvU16)color;
            }
            else if (ImageFormat == NvColorFormat_A8R8G8B8)
            {
                color = (r<<16) | (g<<8) | b;
                *(NvU32*)(pPixels + Pitch*Yofs + 4*(x+Xofs)) = color;
            }
            else
            {
                NV_ASSERT(ImageFormat == NvColorFormat_A8B8G8R8);
                color = r | (g<<8) | (b<<16);
                *(NvU32*)(pPixels + Pitch*Yofs + 4*(x+Xofs)) = color;
            }
        }
    }

    FramebufferUpdate();
}

static void CopyTempImage(
    NvRmSurface *pSurface,
    void        *pPixels,
    const NvU8  *pSrc,
    NvU32        ScreenX,
    NvU32        ScreenY,
    NvU32        SrcPitch,
    NvU32        SrcHeight)
{
    NvU32 i;
    NvU8 *pDstPtr = (NvU8*)pPixels + ScreenY*pSurface->Pitch;

    if ((pSurface->ColorFormat == NvColorFormat_A8R8G8B8) ||
        (pSurface->ColorFormat == NvColorFormat_A8B8G8R8))
        pDstPtr += ScreenX*4;
    else
        pDstPtr += ScreenX*2;

    for (i=0; i<SrcHeight; i++)
    {
        NvOsMemcpy(pDstPtr, pSrc, SrcPitch);
        pSrc += SrcPitch;
        pDstPtr += pSurface->Pitch;
    }
    //Flush the WriteCombineBuffer since FrameBuffer Surface memory
    // is WriteCombined alloced
    NvOsFlushWriteCombineBuffer();
}

void FastbootDrawRadioMenu(
    NvRmSurface *pSurface,
    void        *pPixels,
    RadioMenu   *Menu)
{
    NvU32 Width = 0, Height = 0, MaxRowHeight = 0, MaxRowWidth = 0;
    NvU32 ScreenX, ScreenY;
    NvU32 NumDrawn;
    NvU32 i;
    NvU32 Pitch;
    NvU8 *pTemp = NULL;
    int   NextAnim;

    for (i=0; i<Menu->NumOptions; i++)
    {
        if (Width + Menu->Images[i]->width +Menu->HorzSpacing > pSurface->Width)
        {
            Height += Menu->VertSpacing + MaxRowHeight;
            MaxRowWidth = NV_MAX(Width, MaxRowWidth);
            MaxRowHeight = 0;
            Width = 0;
        }
        Width += Menu->HorzSpacing + Menu->Images[i]->width;
        MaxRowHeight = NV_MAX(MaxRowHeight, Menu->Images[i]->height);
    }
    Height += MaxRowHeight;
    MaxRowWidth = NV_MAX(Width, MaxRowWidth);

    ScreenX = (pSurface->Width - MaxRowWidth)/2;
    ScreenY = (pSurface->Height - Height)/2;

    Width = MaxRowWidth;

    Pitch = Width;
    if ((pSurface->ColorFormat == NvColorFormat_A8R8G8B8) ||
        (pSurface->ColorFormat == NvColorFormat_A8B8G8R8))
        Pitch *= 4;
    else
        Pitch *= 2;

    pTemp = (NvU8 *)NvOsAlloc(Pitch * Height);
    if (!pTemp)
        return;

    NvOsMemset(pTemp, 0, Pitch * Height);

    NumDrawn = 0;
    Height = 0;
    do
    {
        NvU32 NumInRow = 0;
        NvU32 RowWidth = 0;
        NvU32 RowHeight = 0;
        NvU32 Idx;
        NvU32 X;

        Idx = NumDrawn;
        do
        {
            Idx = NumDrawn + NumInRow;
            if (RowWidth + Menu->HorzSpacing + Menu->Images[Idx]->width <= Width)
            {
                RowWidth += Menu->HorzSpacing;
                RowWidth += Menu->Images[Idx]->width;
                RowHeight = NV_MAX(RowHeight, Menu->Images[Idx]->height);
                NumInRow++;
                Idx++;
            }
            else
                break;
        } while (Idx < Menu->NumOptions);

        X = (Width - RowWidth) / 2;
        for ( ; NumDrawn < Idx ; NumDrawn++)
        {
            NvU32 Y = Height + (RowHeight - Menu->Images[NumDrawn]->height)/2;
            DrawIcon(pTemp, Menu->Images[NumDrawn], X, Y, Pitch,
                pSurface->ColorFormat);
            if (NumDrawn == Menu->CurrOption)
                DrawBlendRoundRect(pTemp, Pitch, pSurface->ColorFormat, X, Y,
                    Menu->Images[NumDrawn]->width,
                    Menu->Images[NumDrawn]->height,
                    Menu->PulseColor, Menu->PulseAnim,
                    Menu->RoundRectRadius);
            X += Menu->HorzSpacing;
            X += Menu->Images[NumDrawn]->width;
        }
        Height += RowHeight + Menu->VertSpacing;
    } while (NumDrawn < Menu->NumOptions);

    NextAnim = (int)Menu->PulseAnim + (int)Menu->PulseSpeed;
    if (NextAnim > 0xff)
    {
        NextAnim = 0xff;
        Menu->PulseSpeed = -Menu->PulseSpeed;
    }
    else if (NextAnim < 0)
    {
        NextAnim = 0;
        Menu->PulseSpeed = -Menu->PulseSpeed;
    }
    Menu->PulseAnim = (NvU8)NextAnim;

    /* The loop above will result in Height being Menu->VertSpacing pixels
     * taller than the allocated image height, so the number of pixels to copy
     * is Height - Menu->VertSpacing */
    CopyTempImage(pSurface, pPixels, pTemp, ScreenX,
                  ScreenY, Pitch, Height - Menu->VertSpacing);
    NvOsFree(pTemp);

    FramebufferUpdate();
}

RadioMenu *FastbootCreateRadioMenu(
    NvU32 NumOptions,
    NvU32 InitOption,
    NvU32 PulseSpeed,
    NvU32 PulseColor,
    Icon **pIcons)
{
    RadioMenu *menu = (RadioMenu *)
        NvOsAlloc(sizeof(RadioMenu) + sizeof(Icon*)*(NumOptions-1));
    NvU32 i;

    if (!menu)
        return NULL;

    menu->PulseColor = PulseColor;
    menu->CurrOption = (NvU8) InitOption;
    menu->NumOptions = (NvU8) NumOptions;
    menu->PulseAnim = 0;
    menu->PulseSpeed = (NvS8) PulseSpeed;
    menu->RoundRectRadius = 5;
    menu->HorzSpacing = 20;
    menu->VertSpacing = 10;

    for (i=0; i<NumOptions; i++)
        menu->Images[i] = pIcons[i];

    return menu;
}


void FastbootRadioMenuSelect(
    RadioMenu *pMenu,
    NvBool Next)
{
    if (pMenu->PulseSpeed < 0)
        pMenu->PulseSpeed = -pMenu->PulseSpeed;

    if (Next)
    {
        pMenu->CurrOption = pMenu->CurrOption + 1;
        if (pMenu->CurrOption >= pMenu->NumOptions)
            pMenu->CurrOption = 0;
    }
    else
    {
        if (!pMenu->CurrOption)
            pMenu->CurrOption = pMenu->NumOptions;
        pMenu->CurrOption = pMenu->CurrOption - 1;
    }

    pMenu->PulseAnim = 0;
}

void FastbootDrawTextMenu(
    TextMenu   *Menu,
    NvU32 X,
    NvU32 Y)
{
#ifndef BOOT_MINIMAL_BL

    PrettyPrintContext PrintCtx;
    NvU32 i;
    NvU32 Condition = FASTBOOT_STATUS;

    PrintCtx.y = Y;
    for (i = 0; i < Menu->NumOptions; i++)
    {
        PrintCtx.x = X;
        if (i != Menu->CurrOption)
            Condition = FASTBOOT_STATUS;
        else
            Condition = FASTBOOT_SELECTED_MENU;
        PrettyPrintf(&PrintCtx, &s_Frontbuffer, Condition, "%s\n", Menu->Texts[i]);
    }
#endif
}

void FastbootTextMenuSelect(
    TextMenu *pMenu,
    NvBool Next)
{
    if (Next)
    {
        pMenu->CurrOption = pMenu->CurrOption + 1;
        if (pMenu->CurrOption >= pMenu->NumOptions)
            pMenu->CurrOption = 0;
    }
    else
    {
        if (!pMenu->CurrOption)
            pMenu->CurrOption = pMenu->NumOptions;
        pMenu->CurrOption = pMenu->CurrOption - 1;
    }
}

TextMenu *FastbootCreateTextMenu(
    NvU32 NumOptions,
    NvU32 InitOption,
    const char **pTexts)
{
    NvU32 i;
    TextMenu *menu = (TextMenu *)
        NvOsAlloc(sizeof(TextMenu) + sizeof(char*)*(NumOptions-1));

    for (i=0; i<NumOptions; i++)
        menu->Texts[i] = pTexts[i];
    menu->CurrOption = (NvU8) InitOption;
    menu->NumOptions = (NvU8) NumOptions;

    return menu;
}


void FastbootError(const char *fmt, ...)
{
    char buff[128];
    va_list ap;

    va_start(ap, fmt);
    NvOsVsnprintf(buff, sizeof(buff), fmt, ap);
    va_end(ap);
#ifndef BOOT_MINIMAL_BL
    PrettyPrintf(&s_PrintCtx, &s_Frontbuffer, FASTBOOT_ERROR, "%s\n", buff);
#endif
    NvOsDebugPrintf("%s\n", buff);
}

void FastbootStatus(const char *fmt, ...)
{
    char buff[256];
    va_list ap;

    va_start(ap, fmt);
    NvOsVsnprintf(buff, sizeof(buff), fmt, ap);
    va_end(ap);
#ifndef BOOT_MINIMAL_BL
    PrettyPrintf(&s_PrintCtx, &s_Frontbuffer, FASTBOOT_STATUS, "%s\n", buff);
#endif
    NvOsDebugPrintf("%s\n", buff);
}

#define DEFAULT_CHARGING_BL_INTENSITY 77

void FastbootEnableBacklightTimer(FBBacklightCtrl ctrl, NvU32 time)
{
    if (!s_hDdkDisp || !s_hDisplay)
        return;

    switch (ctrl)
    {
        case FB_DisableBacklight:
            NvOdmBacklightIntensity(s_hDisplay->panel->hPanel, 0);
            break;
        case FB_TimeoutBacklight:
            NvOdmBacklightIntensity(s_hDisplay->panel->hPanel,
                DEFAULT_CHARGING_BL_INTENSITY);
            NvOsSleepMS(time);
            /*Turn off after 30 secs */
            NvOdmBacklightIntensity(s_hDisplay->panel->hPanel, 0);
            break;
        case FB_EnableBacklight:
        default:
            /* Turn on the Backlight */
            NvOdmBacklightIntensity(s_hDisplay->panel->hPanel,
                DEFAULT_CHARGING_BL_INTENSITY);
    }
}
