/*
 * Copyright (c) 2006-2011 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file nvddk_2d.h
 *
 * 2D driver NvDDK inteface
 *
 * NvDdk2d is NVIDIA's 2D device level interface for performing 2D operations
 * on handheld and embedded devices.  
 *   
 * The interface is part of the NVIDIA Driver Development Kit, namely NvDDK.
 *
 * Note that supported formats below are union of the supported formats for
 * different operations. Most of the source and destination format combinations
 * are not supported.
 *
 * The following NvColorFormat's are supported for source surface :
 *     - NvColorFormat_A4R4G4B4
 *     - NvColorFormat_A1R5G5B5
 *     - NvColorFormat_R5G6B5
 *     - NvColorFormat_A8R8G8B8
 *     - NvColorFormat_A8B8G8R8
 *     - NvColorFormat_R8_G8_B8
 *     - NvColorFormat_B8_G8_R8
 *     - NvColorFormat_I1
 *     - NvColorFormat_I8 (bit blit only)
 *     - NvColorFormat_UYVY
 *     - NvColorFormat_VYUY
 *     - NvColorFormat_YUYV
 *     - NvColorFormat_YVYU
 *     - NvColorFormat_Y8
 *     - NvColorFormat_U8
 *     - NvColorFormat_V8
 *     - NvColorFormat_U8_V8
 *     - NvColorFormat_Y8_RR
 *     - NvColorFormat_U8_RR
 *     - NvColorFormat_V8_RR
 *     - NvColorFormat_U8_V8_RR
 *
 * The following NvColorFormat's are supported for destination surface :
 *     - NvColorFormat_A4R4G4B4
 *     - NvColorFormat_A1R5G5B5
 *     - NvColorFormat_R5G6B5
 *     - NvColorFormat_A8R8G8B8
 *     - NvColorFormat_A8B8G8R8
 *     - NvColorFormat_I8 (bit blit only)
 *     - NvColorFormat_UYVY
 *     - NvColorFormat_VYUY
 *     - NvColorFormat_YUYV
 *     - NvColorFormat_YVYU
 *     - NvColorFormat_Y8
 *     - NvColorFormat_U8
 *     - NvColorFormat_V8
 *     - NvColorFormat_U8_V8
 *     - NvColorFormat_Y8_RR
 *     - NvColorFormat_U8_RR
 *     - NvColorFormat_V8_RR
 *     - NvColorFormat_U8_V8_RR
 *
 * The following NvColorFormat's are supported for brush surface :
 *     - NvColorFormat_A4R4G4B4
 *     - NvColorFormat_A1R5G5B5
 *     - NvColorFormat_R5G6B5
 *     - NvColorFormat_A8R8G8B8
 *     - NvColorFormat_A8B8G8R8
 *     - NvColorFormat_I1
 *
 * The following NvColorFormat's are supported for alpha surface :
 *     - NvColorFormat_A1
 *     - NvColorFormat_A2
 *     - NvColorFormat_A4
 *     - NvColorFormat_A8
 */

#ifndef INCLUDED_NVDDK_2D_OLD_H
#define INCLUDED_NVDDK_2D_OLD_H

#include "nvcommon.h"
#include "nvcolor.h"
#include "nvrm_surface.h"
#include "nvrm_channel.h"
#include "nvfxmath.h"

#if defined(__cplusplus)
extern "C"
{
#endif  

/** macro to convert ROP2 code to ROP3 */
#define NVDDK2D_CONVERT_ROP2_TO_ROP3(x) (((x) << 4) | (x))

/** NvDdk2dHandle is an opaque handle to the NvDdk2d interface  */
typedef struct NvDdk2dRec *NvDdk2dHandle;

/** NvDdk2dClip identifies the clipping behaviour */
typedef enum 
{
    /* Destination pixels outside the clipping rectangle passes */
    NvDdk2dClip_Exclusive = 0x1,
       
    /** Destination pixels inside the clipping rectangle passes */
    NvDdk2dClip_Inclusive, 
       
    NvDdk2dClip_Force32 = 0x7FFFFFFF
} NvDdk2dClip;

/** 
 * NvDdk2dColorCompare identifies the color comparison operation when
 * performing a transparent blt
 */
typedef enum
{
    /** 
     * no color comparison operation is performed. This is equivalent to
     * disabling color comparison 
     */
    NvDdk2dColorCompare_None = 0x1,

    /** 
     * color comparison is successful when the pixel color matches that
     * of the key color
     */
    NvDdk2dColorCompare_Equal,

    /** 
     * color comparison is successful when the pixel color does not match 
     * that of the key color
     */
    NvDdk2dColorCompare_NotEqual,

    NvDdk2dColorCompare_Force32 = 0x7FFFFFFF,
} NvDdk2dColorCompare;

/** 
 * NvDdk2dBlendFactor identifies the blending factors for R,G,B,A color
 * components
 */
typedef enum 
{      
    /** 
     * R,G,B color components gets scaled by alpha factor
     * where alpha is from src
     */
    NvDdk2dBlendFactor_SrcAlpha = 0x1,
      
    /** 
     * R,G,B color components gets scaled by (1-alpha) factor
     * where alpha is from src
     */
    NvDdk2dBlendFactor_OneMinusSrcAlpha, 
    
    /** 
     * R,G,B color components gets scaled by alpha factor
     * where alpha is a constant value
     */
    NvDdk2dBlendFactor_ConstantAlpha,
      
    /** 
     * R,G,B color components gets scaled by (1-alpha) factor
     * where alpha is a constant value
     */
    NvDdk2dBlendFactor_OneMinusConstantAlpha, 
    
    /** 
     * R,G,B color components gets scaled by alpha factor
     * where alpha is from separate surface
     */
    NvDdk2dBlendFactor_Src2Alpha,
      
    /** 
     * R,G,B color components gets scaled by (1-alpha) factor
     * where alpha is a from separate surface
     */
    NvDdk2dBlendFactor_OneMinusSrc2Alpha, 
    
    /** Scale factor for alpha component is one */
    NvDdk2dBlendFactor_One,
    
    /** Scale factor for alpha component is zero */
    NvDdk2dBlendFactor_Zero,
    
    NvDdk2dBlendFactor_Force32 = 0x7FFFFFFF,
 } NvDdk2dBlendFactor;
  
/** 
 * NvDdk2dBlend identifies the different blending modes.
 */
typedef enum 
{     
    /** dst = src */
    NvDdk2dBlend_Disabled = 0x1,
    
    /** dst = rop(src,dst,pat) */  
    NvDdk2dBlend_Rop3, 
    
    /** dst.rgb = src.rgb * srcf + dst.rgb *dstf 
     *  dst.a = src.a * srcaf + dst.a * dstaf 
     */
    NvDdk2dBlend_Add, 
    
    NvDdk2dBlend_Force32 = 0x7FFFFFFF,
} NvDdk2dBlend;
 
/** 
 * NvDdk2dStretchFilter identifies the filtering used when performing 
 * scaling 
 */
typedef enum
{
    /** Disable the horizontal and vertical filtering */
    NvDdk2dStretchFilter_Nearest = 0x1,

    /** Enable the best quality filtering 
         *  Both Horizontal and vertical filtering is turned on
         */
    NvDdk2dStretchFilter_Nicest,

        /** Enable intelligent filtering 
         *  Horizontal or Vertical filtering is turned on only if needed 
         */
    NvDdk2dStretchFilter_Smart,
    
    /** Enable 2 tap filtering in both horizontal and vertical direction */
    NvDdk2dStretchFilter_Bilinear,
    
    NvDdk2dStretchFilter_Force32 = 0x7FFFFFFF
} NvDdk2dStretchFilter;

/** 
 * NvDdk2dTransform identifies the transformations that can be applied
 * as a combination of rotation and mirroring
 *
 * Specifically, given a hypothetical 2x2 image, applying these operations 
 * would yield the following results
 *  
 *
 *       INPUT                      OUTPUT
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |     IDENTITY     | 0 | 1 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 2 | 3 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |    ROTATE_90     | 1 | 3 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 0 | 2 |
 *     +---+---+                  +---+---+
 *
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |    ROTATE_180    | 3 | 2 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 1 | 0 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |     ROTATE_270   | 2 | 0 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 3 | 1 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |  FLIP_HORIZONTAL | 1 | 0 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 3 | 2 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |  FLIP_VERTICAL   | 2 | 3 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 0 | 1 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |     TRANSPOSE    | 0 | 2 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 1 | 3 |
 *     +---+---+                  +---+---+
 *
 *     +---+---+                  +---+---+    
 *     | 0 | 1 |  INVTRANSPOSE    | 3 | 1 |
 *     +---+---+ ---------------> +---+---+
 *     | 2 | 3 |                  | 2 | 0 |
 *     +---+---+                  +---+---+
 */
typedef enum
{
    /** no transformation */
    NvDdk2dTransform_None = 0x1,

    /** rotation by 90 degrees */
    NvDdk2dTransform_Rotate90,

    /** rotation by 180 degrees */
    NvDdk2dTransform_Rotate180,

    /** rotation by 270 degrees */
    NvDdk2dTransform_Rotate270,

    /** mirroring in the horizontal */
    NvDdk2dTransform_FlipHorizontal,

    /** mirroring in the vertical direction */
    NvDdk2dTransform_FlipVertical,

    /** 
     * mirroring along a diagonal axis from the top left to the bottom
     * right of the rectangular region
     */
    NvDdk2dTransform_Transpose,

    /** 
     * mirroring along a diagonal axis from the top right to the bottom
     * left of the rectangular region
     */
    NvDdk2dTransform_InvTranspose,

    NvDdk2dTransform_Force32 = 0x7FFFFFFF
} NvDdk2dTransform;

/** NvDdk2dPrimitives identifies the 2D primitives that can be drawn */
typedef enum
{
    /** 
     * draws each pair of points as an independent line segment.
     * Points 2n-1 and 2n define a line n. N/2 lines are drawn
     */
    NvDdk2dPrimitiveType_Lines = 0x1,

    /** 
     * draws a connected group of line segments from the first point 
     * to the last. Points n and n+1 define line n. N-1 lines are drawn
     */
    NvDdk2dPrimitiveType_LineStrip,

    /** 
     * each triplet of points are treated as an independent triangle.
     * Points 3n-2, 3n-1, and 3n define triangle n. N/3 triangles are 
     * drawn
     */
    NvDdk2dPrimitiveType_Triangles,

    NvDdk2dPrimitiveType_Force32 = 0x7FFFFFFF
} NvDdk2dPrimitiveType;

/** NvDdk2dBrush identifies the Color/Monochrome Brush properties */
typedef enum
{
    /** No Brush is used */
    NvDdk2dBrush_None = 0x1,

    /** Color Brush has a solid color */
    NvDdk2dBrush_SolidColor,

    /** 
     * Color brush should be rendered as transparent.
     * Not applicable to Monochrome surfaces
     */ 
    NvDdk2dBrush_ColorTransparent,
    
    /** 
     * Monochrome brush should be rendered as opaque. 
     * So the Monochrome brush pixels set 
     * to the value 0x1 are rendered as foreground where-as brush pixels set 
     * to 0x0 are rendered as background.
     */
    NvDdk2dBrush_MonoOpaque,
    
    /** 
     * Monochrome brush should be rendered as transparent. 
     * So the Monochrome brush pixels set 
     * to the value 0x1 are rendered where-as brush pixels set to 0x0 are
     * not rendered hence they are transparent
     */
    NvDdk2dBrush_MonoTransparent,
    
    /** 
     * Monochrome brush should be rendered as Inverse transparent. 
     * So the Monochrome brush pixels set 
     * to the value 0x0 are rendered where-as brush pixels set to 0x1 are
     * not rendered hence they are transparent
     */
    NvDdk2dBrush_MonoInvTransparent,
    
    NvDdk2dBrush_Force32 = 0x7FFFFFFF
} NvDdk2dBrush;

/** 
 * NvDdk2d supports all 256 Microsoft Windows-compatible ROP3 operations, and
 * 16 ROP2 operations.  For programmer-convenience, convenient enums have 
 * been provided for all ROP2 modes and their equivalent ROP3's can be obtained
 * by using the macro NVDDK2D_CONVERT_ROP2_TO_ROP3(x).
 */

/** 
 * NvDdk2dRop2 defines the raster operations with two operands namely 
 * source and destination
 */
typedef enum 
{
    /** resulting value = 0 */
    NvDdk2dRop2_Clear = 0x00, 
    
    /** resulting value = ~(src | dst) */
    NvDdk2dRop2_NOR,  
    
    /** resulting value = ~src & dst */
    NvDdk2dRop2_AndInverted,  

    /** resulting value = ~src */
    NvDdk2dRop2_CopyInverted,  
    
    /** resulting value = src & ~dst */
    NvDdk2dRop2_AndReverse,  
    
    /** resulting value = ~dst */
    NvDdk2dRop2_Invert,  

    /** resulting value = src ^ dst */
    NvDdk2dRop2_XOR,  
    
    /** resulting value = ~(src & dst) */
    NvDdk2dRop2_Nand,  

    /** resulting value = src & dst */
    NvDdk2dRop2_And,  

    /** resulting value = ~(src ^ dst) */
    NvDdk2dRop2_Equiv,  

    /** resulting value = dst */
    NvDdk2dRop2_NoOp,  
    
    /** resulting value = ~src | dst */
    NvDdk2dRop2_ORInverted,  

    /** resulting value = src */
    NvDdk2dRop2_Copy,  
 
    /** resulting value = src | ~dst */
    NvDdk2dRop2_ORReverse,  

    /** resulting value = src | dst */
    NvDdk2dRop2_OR,  
    
    /** resulting value = 1 */
    NvDdk2dRop2_Set,  

    NvDdk2dRop2_Force32 = 0x7FFFFFFF
} NvDdk2dRop2;

/** NvDdk2dCSCMatrix specifies which color space conversion matrix will be
    used for YUV to RGB conversion */
typedef enum
{
    /** 
     * Default - ITU.601 limited range conversion
     */
    NvDdk2dCSCMatrix_Default = 0x1,

    /** 
     * Matrix specified by JFIF standard
     */
    NvDdk2dCSCMatrix_JFIF,

    NvDdk2dCSCMatrix_Force32 = 0x7FFFFFFF
} NvDdk2dCSCMatrix;


/** NvDdk2dColor describes the color  */
typedef struct NvDdk2dColorRec
{
/**
 * The following NvColorFormat's are supported:
 *     - NvColorFormat_A4R4G4B4
 *     - NvColorFormat_A1R5G5B5
 *     - NvColorFormat_R5G6B5
 *     - NvColorFormat_A8R8G8B8
 *     - NvColorFormat_A8B8G8R8
 */
    NvColorFormat Format; 
    NvU32 Color;
} NvDdk2dColor;

/** 
 * NvDdk2dTransparentParam contains the parameters used for color comparison  
 * operations.
 * The color comparison operation specified is first applied against each
 * pixel from the source.  Any source pixels that pass the color comparison
 * operation are then use to perform the specified ROP.  The output from
 * the ROP is applied to the destination only for destination pixels that
 * pass the color comparison operation.
 */
typedef struct NvDdk2dTransparentParamRec
{
    /** color comparison operation performed against the source surface */
    NvDdk2dColorCompare ColorCmpSrc;
    
    /** color comparison operation performed against the destination surface */
    NvDdk2dColorCompare ColorCmpDst;
    
    /** 
     * key is the pointer to the color key used for both the source and 
     * destination color comparison operations 
     */
    NvDdk2dColor *pKey;

} NvDdk2dTransparentParam;

/** 
 * NvDdk2dTextureParam contains the parameters used for stretchblt.
 * s, t are the texture coordinate and dsdx, dtdy are their derivatives.
 * The texture coordniates are not normalized so the texture coordinates
 * that fall within the clamped source image range from (0, 0) to
 * (clamped.w, clamped.h).
 * When performing strectblt, (s, t) is used to sample the first destination
 * pixel, say (x, y). (s + dsdx, t) is used to sample the destination pixel
 * (x +1, y) and (s, t + dtdy) is used to sample the destination pixel 
 * (x, y + 1) and so on.
 */
typedef struct NvDdk2dTextureParamRec 
{
    /* horizontal texture coordinate, unnormalized */
    NvSFx s; 

    /* vertical texture coordinate, unnormalized */
    NvSFx t; 

    /* horizontal texture coordinate derivative = ds/dx */
    NvSFx dsdx;

    /* vertical texture coordinate derivative = dt/dy */
    NvSFx dtdy;

} NvDdk2dTextureParam; 

/** 
 * NvDdk2dOpenOld to open the interface to the 2D API
 *
 * This api must be called before any other NvDdk2d api's can be used. It
 * initializes all client specific data for this instance of the API. 
 *  
 * Each client of this interface has it's own client specific context, so
 * any rendering state specific to this client is maintained when multiple
 * clients use the interface at the same time.
 *
 * Any call to the 2D API is allowed to change the class. 
 *
 * It is called "Old" so that it's symbol doesn't collide with the NvDdk2d_v2
 * function of the same name.
 *
 * @param hDevice handle to the device
 *
 * @param pStream pointer to the Stream bound to an existing Channel.
 *                If a valid pointer is passed then all the subsequent
 *                2D api commands would go through the Stream being pointed
 *                to by pStream.
 *                If NULL pointer is passed then NvDdk2dOpenOld would take care
 *                of opening the Channel and initializing a new Stream 
 *                associated with the Channel.
 *                
 * @param ph2d pointer to the handle to the NvDdk2d API 
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dOpenOld(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle hChannel,
    NvDdk2dHandle *ph2d);

/** 
 * NvDdk2dCloseOld to close the API
 *
 * This api must be called when the client has completed using NvDdk2d.Any
 * resources allocated for this instance of the interface are released.
 *
 * If the Stream was being initialized in NvDdkOpenOld() then NvDdkCloseOld() would
 * free the Stream on exiting. In other case, when the Stream was initialized
 * on the Client side, then its the Client responsibity to free the Stream.
 *  
 * It is called "Old" so that it's symbol doesn't collide with the NvDdk2d_v2
 * function of the same name.
 *
 * @param h2d    handle to the 2D API. It is OK to pass a NULL handle.
 *               Basically the api would do nothing if you try to close 
 *               a NULL handle.
 *
 * @retval None.
 */
void 
NvDdk2dCloseOld(
    NvDdk2dHandle h2d);
               
/** 
 * NvDdk2dSetDestinationSurface to set the current destination surface for 
 * 2D rendering. This api basically saves a copy of the NvRmSurface pointed 
 * by pDst.
 *
 * The default destination surface is NULL which implies that the destination 
 * surface should be set before calling any of the below NvDdk2d api's
 *      - NvDdk2dBlt()
 *      - NvDdk2dTransformBlt()
 *      - NvDdk2dCoverageResolve()
 *      - NvDdk2dStretchBlt()
 *      - NvDdk2dDrawPrimitives()
 *
 * @param h2d handle to the 2D API
 *
 * @param ppDst pointer to an array of destination surface pointers
 *
 * @param numDstSurf number of destination surface pointers in ppDst
 *               For YUV420 surface format:
 *                 - when 3 planes Y,U and V are in separate planes, the valid
 *                   numDstSurf is 3.
 *                 - when Y is in one plane and UV in another plane, the valid
 *                   numDstSurf is 2.
 *               For rest of Dst surface formats, the valid numDstSurf is 1.
 *
 * @retval NvSuccess on success
 *   
 * @retval NvError_SurfaceNotSupported when the destination surface is not 
 *          supported
 */
NvError 
NvDdk2dSetDestinationSurface(
    NvDdk2dHandle h2d,
    const NvRmSurface **ppDst,
    NvU32 numDstSurf);
                              
/** 
 * NvDdk2dGetDestinationSurface to get the current destination surface for 
 * 2D rendering
 *
 * This api should be used to query the current destination surface for 2D 
 * rendering 
 *   
 * @param h2d handle to the 2D API
 *   
 * @param ppDst valid pointer to the requested destination surface pointers. 
 *
 * @param pNumDstSurf pointer which returns the number of destination 
 *                    surfaces set.
 *                
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dGetDestinationSurface(
    NvDdk2dHandle h2d,
    NvRmSurface **ppDst,
    NvU32 *pNumDstSurf);
 
/** 
 * NvDdk2dSetColorExpForMonoBlt to set the color expansion 
 * used when drawing mono source data
 *
 * Sets the foreground and background colors used
 * when drawing mono source data
 *
 * @param h2d handle to the 2D API
 *
 * @param pFgColor pointer to the foreground color used when rendering 
 *                 monochrome src data. The color should be formatted  
 *                 with the same color format as the intended destination 
 *                 surface.
 *
 * @param pBgColor pointer to the background color used when rendering 
 *                 monochrome src data. The color should be formatted with  
 *                 the same color format as the intended destination surface.
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dSetColorExpForMonoBlt(
    NvDdk2dHandle h2d,
    const NvDdk2dColor *pFgColor,
    const NvDdk2dColor *pBgColor);

/** 
 * NvDdk2dSetColorExpForMonoBrush Method to set the color expansion 
 * used for monochrome brush
 *
 * Sets the foreground and background colors used
 * when monochrome brush is used
 *
 * @param h2d handle to the 2D API
 *
 * @param pFgColor pointer to the foreground color used when rendering using 
 *                 monochrome brush. The color should be formatted with the   
 *                 same color format as the intended destination surface.
 *
 * @param pBgColor pointer to the background color used when rendering 
 *                 monochrome brush. The color
 *                 should be formatted with the same color format as the 
 *                 intended destination surface.
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dSetColorExpForMonoBrush(
    NvDdk2dHandle h2d,
    const NvDdk2dColor *pFgColor,
    const NvDdk2dColor *pBgColor);

/** 
 * NvDdk2dMaxClipRect to get the maximum number of clipping rectangles 
 * supported. 
 *  
 * @param h2d handle to the 2D API
 *   
 * @param pCount a non NULL pointer to the maximum number of clipping 
 *               rectangles as returned by the api 
 *                
 * @retval NvSuccess on success
 */
 
NvError 
NvDdk2dGetMaxClipRect(
    NvDdk2dHandle h2d,
    NvU32 *pCount);

/** 
 * NvDdk2dGetClip to get the destination clipping rectangle and the clipping
 * attribute
 *
 * @param h2d handle to the 2D API
 *   
 * @param index specifies the index of the clipping rectangle in the array of 
 *              clipping rectangles
 *
 * @param pClipRect a valid pointer which returns the clipping rectangle 
 *
 * @param pClipAttribute a valid pointer which returns the clipping attribute  
 *                        
 * @retval NvSuccess on success
 *
 * @retval NvError_BadParameter when index is greater than N-1 where N is  
 *                                 the total number of clipping rectangles 
 *                                 set using NvDdk2dSetClip().
 *                                 
 */
 
NvError 
NvDdk2dGetClip(
    NvDdk2dHandle h2d,
    NvU32 index,
    NvRect *pClipRect,
    NvDdk2dClip *pClipAttribute);
                    
/** 
 * NvDdk2dSetClip to set the clipping rectangles as well as the clipping
 * behaviour.
 * If two clipping rectangles overlap with each other then the result is 
 * undefined.
 *  
 * @param h2d handle to the 2D API
 *
 * @param pClipRects pointer to an array of destination clipping rectangles.  
 *                   If this  parameter is NULL then the count and the 
 *                   clipAttribute parameters are ignored and the case 
 *                   is equivalent to enabling NvDdk2dClip_Inclusive
 *                   with the clipping rectangle set to the bounds of the  
 *                   current destination surface. 
 *
 * @param ClipAttribute clipping attributes which defines the clipping 
 *                      behaviour
 *                    
 * @param count specifies the number of clipping rectangles
 *
 * @retval NvSuccess on success
 *
 * @retval NvError_InvalidOperation when count is zero and pClipRects is not a 
 *                                 NULL pointer.
 * @retval NvError_InvalidOperation when count is greater than the maximum 
 *                                 number of clipping rectangles supported 
 *                                 by the implementation
 *
 */
NvError 
NvDdk2dSetClip(
    NvDdk2dHandle h2d,
    const NvRect *pClipRects,
    NvDdk2dClip ClipAttribute,
    NvU32 count); 
    
/** 
 * NvDdk2dSetBrush sets the brush used for ROP3 operations
 *
 * @param h2d handle to the 2D API
 *
 * @param pBrush pointer to a surface to be used as a brush.
 *  
 * @param BrushProp brush properties 
 *
 * @param pAlignPoint pointer to a point structure indicating the alignment of 
 *                    the brush relative to the top left corner of a 
 *                    destination surface. 
 *                    For cases when the flag NVDDK2D_BRUSH_MONOTILED is not 
 *                    defined then the resulting brush size after 
 *                    setting the alignment should exactly match the blt wxh 
 *                    dimension.
 *                     
 * @param pColor pointer specifying the color of the brush when 
 *               NVDDK2D_BRUSH_SOLIDCOLOR flag is set. The pBrush pointer is 
 *               ignored for this case.
 *   
 * @param flags a 32 bit value 
 *
 * @retval NvSuccess on success
 *
 * @retval NvError_InvalidOperation
 *         - when pBrush is NULL except for NvDdk2dBrush_SolidColor
 *      
 *         - when NVDDK2D_BRUSH_MONOTILED is enabled and the brush surface is 
 *           not a Monochrome surface
 *
 *         - when NVDDK2D_BRUSH_MONOTILED is enabled with 
 *           NvDdk2dBrush_SolidColor or NvDdk2dBrush_ColorTransparent
 *          
 *         - when NVDDK2D_BRUSH_MONOTILED is enabled and the brush surface 
 *           size is less than or greater than 16x16 pixels  
 */                                               
NvError 
NvDdk2dSetBrush(
    NvDdk2dHandle h2d,
    const NvRmSurface *pBrush,
    NvDdk2dBrush BrushProp,
    const NvPoint *pAlignPoint,
    const NvDdk2dColor *pColor,
    NvU32 flags);

 /** 
  * Monochrome brush should be rendered as tiled pattern.
  * Tiling is supported for 16x16 Monochrome brush size only.
  * This flag can be only combined with NvDdk2dBrush_MonoOpaque or
  * NvDdk2dBrush_MonoTransparent
  */
#define NVDDK2D_BRUSH_MONOTILED   0x00000001

/** 
 * NvDdk2dSetPen Sets the pen color used for drawing 2D primitives using 
 * NvDdk2dDrawPrimitives()
 *
 * @param h2d handle to the 2D API
 *
 * @param pColor color for the pen, matching the color
 *               format of the intended destination surface
 *               for the pen.
 *
 * @retval NvSuccess on success
 */                                               
NvError 
NvDdk2dSetPen(
    NvDdk2dHandle h2d,
    const NvDdk2dColor *pColor);
                   
/** 
 * NvDdk2dSetStretchFilter to specify filtering when scaling
 *
 * Sets the type of filtering used when scaling via the NvDdk2dStretchBlt 
 * function.
 *
 * @param h2d handle to the NvDdk2d API
 *
 * @param Filter specifies the filter used when stretching/shrinking.
 *
 * @retval NvSuccess on success
 */                                               
NvError 
NvDdk2dSetStretchFilter(
    NvDdk2dHandle h2d,
    NvDdk2dStretchFilter Filter);
                             
/** 
 * NvDdk2dSetBlend to set the blend mode between source, destination and 
 * pattern
 *
 * @param h2d handle to the 2D API
 *
 * @param Blend mode set using NvDdk2dBlend enum
 *
 * @retval NvSuccess on success
 *
 */
NvError 
NvDdk2dSetBlend(
    NvDdk2dHandle h2d,
    NvDdk2dBlend Blend);
    
/** 
 * NvDdk2dSetBlendFunc sets the operation of blending when the blend mode 
 * is NvDdk2dBlend_Add.
 *
 * Alpha Blending is a way of mixing of two images to get the final image.
 * The operation is defined as:
 *   
 * dst.rgb = src.rgb * srcf + dst.rgb * dstf
 *
 * where 
 * dst.rgb and src.rgb are dst and src R,G,B component respectively
 * dstf and srcf are the scaling factors for the src and dst R,G,B components 
 * 
 * dst.a = src.a * srcaf + dst.a * dstaf
 *
 * where 
 * dst.a and src.a are dst and src alpha components respectively
 * dstaf and srcaf are the scaling factors for the  
 * dst and src alpha components.The valid value for dstaf and srcaf
 * is 0 or 1
 *
 * If the destination format has no alpha, then the alpha blend factors are
 * ignored.
 * If the source format has no alpha, then by definition src.a = 1.
 *
 * Legal combinations for : 
 *    
 * SrcBlendFactor / DstBlendFactor 
 *                    
 * NvDdk2dBlendFactor_SrcAlpha / NvDdk2dBlendFactor_OneMinusSrcAlpha      
 * NvDdk2dBlendFactor_OneMinusSrcAlpha / NvDdk2dBlendFactor_SrcAlpha
 * NvDdk2dBlendFactor_ConstantAlpha / NvDdk2dBlendFactor_OneMinusConstantAlpha 
 * NvDdk2dBlendFactor_OneMinusConstantAlpha / NvDdk2dBlendFactor_ConstantAlpha
 * NvDdk2dBlendFactor_Src2Alpha / NvDdk2dBlendFactor_OneMinusSrc2Alpha     
 * NvDdk2dBlendFactor_OneMinusSrc2Alpha / NvDdk2dBlendFactor_Src2Alpha
 *
 * SrcAlphaBlendFactor / DstAlphaBlendFactor 
 *
 * NvDdk2dBlendFactor_Zero / NvDdk2dBlendFactor_One 
 * NvDdk2dBlendFactor_One  / NvDdk2dBlendFactor_Zero
 *
 *
 * @param h2d handle to the 2D API
 *
 * @param SrcBlendFactor specify the scaling factor for src R,G,B component
 *
 * @param DstBlendFactor specify the scaling factor for dst R,G,B component
 *
 * @param SrcAlphaBlendFactor specify the scaling factor for src alpha
 * component
 *
 * @param DstAlphaBlendFactor specify the scaling factor for dst alpha
 * component
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dSetBlendFunc(
    NvDdk2dHandle h2d,
    NvDdk2dBlendFactor SrcBlendFactor,
    NvDdk2dBlendFactor DstBlendFactor,
    NvDdk2dBlendFactor SrcAlphaBlendFactor,
    NvDdk2dBlendFactor DstAlphaBlendFactor);
    
/** 
 * NvDdk2dSetConstantAlpha sets the constant alpha value.
 *  
 * This api should be called when the blend mode is NvDdk2dBlend_Add AND:
 *        
 *    Case 1 : the src or dst blend factors are being set to either
 *             NvDdk2dBlendFactor_ConstantAlpha or
 *             NvDdk2dBlendFactor_OneMinusConstantAlpha
 *
 *    Case 2 : the src or dst blend factors are being set to either
 *             NvDdk2dBlendFactor_SrcAlpha or
 *             NvDdk2dBlendFactor_OneMinusSrcAlpha and the src surface
 *             is of NvColorFormat_A1R5G5B5 format
 *
 *    Case 3 : the src or dst blend factors are being set to either
 *             NvDdk2dBlendFactor_Src2Alpha or
 *             NvDdk2dBlendFactor_OneMinusSrc2Alpha and the second src
 *             surface is of NvColorFormat_A1 format
 *
 * @param h2d handle to the 2D API
 *
 * @param Alpha alpha value which ranges from 0 to 0xFF. Treated as 
 *              constant alpha value for Case 1 and 
 *              foreground alpha value for the Case 2 and Case 3 
 *              described as above.
 *
 * @param AlphaBg background alpha value which ranges from 0 to 0xFF. 
 *                It is ignored for Case 1 as described above.
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dSetConstantAlpha(
    NvDdk2dHandle h2d,
    NvU8 Alpha,
    NvU8 AlphaBg);
 
/** 
 * NvDdk2dSetTransparency to enable source or destination transparency
 *
 * @param h2d handle to the 2D API
 *
 * @param pTransparentParam pointer to transparency parameters. When 
 *                          pTransparentParam is NULL, it is equivalent 
 *                          to disable transparency
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dSetTransparency(
    NvDdk2dHandle h2d,
    const NvDdk2dTransparentParam *pTransparentParam);
   
/** 
 * NvDdk2dSetROP to set the raster operation
 *
 * @param h2d handle to the 2D API
 *
 * @param Rop describes the rop3 code
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dSetROP(
    NvDdk2dHandle h2d,
    NvU32 Rop);  

/** 
 * NvDdk2dSetCSCMatrix to set the color space conversion matrix for
 * the next conversion if done. This setting only affects YUV->RGB conversion
 * in the following NvDdk2dStretchBlt call. The CSCMatrix is reset upon leaving
 * NvDdk2dStretchBlt to retain backward compatibility.
 *
 * @param h2d handle to the 2D API
 *
 * @param CSCMatrix describes the CSC code
 *
 * @retval NvSuccess on success
 */
NvError
NvDdk2dSetCSCMatrix(
    NvDdk2dHandle h2d,
    NvDdk2dCSCMatrix CSCMatrix);

/** 
 * NvDdk2dSetWaitForSyncPtIncrs to insert N host wait for syncpt incrs for 
 * the SyncPtID before any 2D rendering primitive.
 * This api enables the client to hold off the following 2D operation 
 * until the host wait for syncpt incrs is fulfilled.
 * The wait will only be applied to the next immediate 2D rendering primitive.
 * For example , if here is the client's sequence
 *
 * NvDdk2dSetWaitForSyncPtIncrs (h2d, syncptid, 2);
 * NvDdk2dBlt()
 * NvDdk2dFillRect()
 *
 * Then 2 wait for syncpt incrs will be inserted only before the blt commands 
 * in NvDdk2dBlt() and not in NvDdk2dFillRect().
 *
 * @param h2d handle to the 2D API
 *
 * @param SyncPtID describes the syncpt id 
 *
 * @param Count describes the number of waits to be inserted.
 *
 * @retval NvSuccess on success
 */
    
NvError
NvDdk2dSetWaitForSyncPtIncrs(NvDdk2dHandle h2d,
                             NvU32 SyncPtID,
                             NvU32 Count);


/** 
 * NvDdk2dWaitForSyncPt immediately enters a host wait into the 2D command
 * stream. No subsequent commands entered will be processed before the syncpt
 * given as parameter "Fence" has been reached.
 *
 * This functionality should be used when synchronizing the operation of a
 * different hardware module (say, 3D) with the 2D operations.
 *
 * This function does not flush the stream or block the caller, it simply enters
 * the command for a host wait.
 * 
 * @param h2d handle to the 2D API
 * @param Fence the syncpoint to do a host wait on
 */

void
NvDdk2dWaitForSyncPt(NvDdk2dHandle h2d,
                     const NvRmFence* Fence);


/** 
 * NvDdk2dFillRect to fill a rectangle on destination surface using the 
 * existing blending or rop state with current brush settings. 
 * Being said that, this api takes care of blitting using all the rop codes 
 * with constant color (standing for source in rop).
 * Instead the constant color source can also be blended with the destination 
 * using constant alpha value.
 *  
 * The color format of the source color should be the same as the destination
 * color format
 * 
 * @param h2d handle to the 2D API
 *
 * @param pColor pointer to the color 
 *
 * @param pDstRect pointer to the destination rectangle
 *  
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dFillRect(
    NvDdk2dHandle h2d,
    const NvDdk2dColor *pColor,
    const NvRect  *pDstRect);  
    
/** 
* NvDdk2dFillRects to fill rectangles on destination surface using the 
* existing blending or rop state with current brush settings.
* Being said that, this api takes care of blitting using all the rop codes
* with constant color (standing for source in rop).
* Instead the constant color source can also be blended with the destination 
* using constant alpha value.
*  
* The color format of the source color should be the same as the destination
* color format
* 
* @param h2d handle to the 2D API
*
* @param pColor pointer to the color 
*
* @param pDstRect pointer to an array of destination rectangles
*
* @param count number of destination rectangles
*  
* @retval NvSuccess on success
*
* Note: Number of rectangles should be such that NvDdk2dFillRects does not
* starve the other users of 2d HW.
*/
NvError 
NvDdk2dFillRects(
    NvDdk2dHandle h2d,
    const NvDdk2dColor *pColor,
    const NvRect  *pDstRect,
    NvU32 count);

/** 
 * NvDdk2dBlt performs Blit Block Transfers(Blt).
 * The color format of the source surface must be identical to that of the
 * destination surface.
 *
 * @param h2d handle to the 2D API
 *   
 * @param ppSrc pointer to an array of source surface pointers
 *              This parameter is ignored
 *              when the rop specified does not include the source.
 *
 * @param numSrcSurf number of surface pointers in ppSrc. 
 *                   For YUV420 surface format:
 *                      - Case 1 : 3 planes Y,U and V are in separate planes, 
 *                        the valid numSrcSurf is 3.
 *
 *                      - Case 2 : Y is in one plane and UV in another plane, 
 *                        the valid numSrcSurf is 2.
 *
 *                   For RGB surfaces
 *                      - Case 1: alpha blending is enabled 
 *                                if the src/dst alpha factors are set to 
 *                                either NvDdk2dBlendFactor_Src2Alpha
 *                                or NvDdk2dBlendFactor_OneMinusSrc2Alpha 
 *                                then valid numSrcSurf is 2. The first 
 *                                surface pointer will be the source surface
 *                                while the second surface pointer should be
 *                                pointing to alpha surface.
 *
 *                      - Case 2: alpha blending is disabled 
 *                                the valid numSrcSurf is 1.
 *
 *                   For I8 surface format:
 *                      - Plain memory copy, "pixels" (i.e. bytes) are not interpreted.
 *                      - Destination surface must also be I8.
 *                      - Blend must be NvDdk2dBlend_Disabled, no rops.
 *
 * @param pSrcLoc pointer to the start location for the blt on  
 *                the source surface.  This parameter is ignored when the 
 *                rop specified does not include the source or if the src   
 *                is solid color.
 *
 * @param pDstRect pointer to a destination rectangle indicating
 *                 the destination area of the blt.  
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dBlt(
    NvDdk2dHandle h2d,
    const NvRmSurface **ppSrc,
    NvU32 numSrcSurf,
    const NvPoint *pSrcLoc,
    const NvRect  *pDstRect);
 
/** 
 * NvDdk2dTransformBlt to perform 2D transformations
 *
 * This api is used to perform one or more bit block transfers with
 * the specified transformation.  Transformations include rotation and 
 * mirroring.
 * The color format of the source surface must match that of the
 * destination surface.
 *   
 * @param h2d handle to the 2D API
 *   
 * @param ppSrc pointer to an array of source surface pointers
 *
 * @param numSrcSurf number of surface pointers in ppSrc. 
 *                   For YUV420 surface format:
 *                      - Case 1 : 3 planes Y,U and V are in separate planes, 
 *                        the valid numSrcSurf is 3.
 *
 *                      - Case 2 : Y is in one plane and UV in another plane, 
 *                        the valid numSrcSurf is 2.
 *
 *                   For RGB surfaces
 *                      - Case 1: alpha blending is enabled 
 *                                if the src/dst alpha factors are set to 
 *                                either NvDdk2dBlendFactor_Src2Alpha
 *                                or NvDdk2dBlendFactor_OneMinusSrc2Alpha 
 *                                then valid numSrcSurf is 2. The first 
 *                                surface pointer will be the source surface
 *                                while the second surface pointer should be
 *                                pointing to alpha surface.
 *
 *                      - Case 2: alpha blending is disabled 
 *                                the valid numSrcSurf is 1.
 *
 * @param Transform transformation applied to the blt operations
 *
 * @param pSrcLoc  pointer to source location
 *                   
 * @param pDstRect pointer to destination rectangle indicating 
 *                 the destination area of the blt.  
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dTransformBlt(
    NvDdk2dHandle h2d,
    const NvRmSurface **ppSrc,
    NvU32 numSrcSurf,
    NvDdk2dTransform Transform,
    const NvPoint *pSrcLoc,
    const NvRect  *pDstRect);

/** 
 * NvDdk2dStretchBlt performs image rescaling operation along with specified 
 * transformation
 *
 * @param h2d handle to the 2D API
 *   
 * @param ppSrc pointer to an array of source surface pointers
 *
 * @param numSrcSurf number of surface pointers in ppSrc. 
 *                   For YUV420 surface format:
 *                      - Case 1 : 3 planes Y,U and V are in separate planes, 
 *                        the valid numSrcSurf is 3.
 *
 *                      - Case 2 : Y is in one plane and UV in another plane, 
 *                        the valid numSrcSurf is 2.
 *
 *                   For RGB surfaces
 *                      - Case 1: alpha blending is enabled 
 *                                if the src/dst alpha factors are set to 
 *                                either NvDdk2dBlendFactor_Src2Alpha
 *                                or NvDdk2dBlendFactor_OneMinusSrc2Alpha 
 *                                then valid numSrcSurf is 2. The first 
 *                                surface pointer will be the source surface
 *                                while the second surface pointer should be
 *                                pointing to alpha surface.
 *
 *                      - Case 2: alpha blending is disabled 
 *                                the valid numSrcSurf is 1.
 *
 * @param pSrcRect pointer to the source clamp rectangle. Any pixel used to
 *                 sample and outside this region is clamped to this rectangle.
 *
 * @param pDstRect pointer to destination rectangle indicating the
 *                 destination area after the stretch/shrink and transformation
 *                 being done. 
 *
 * @param pTexParam pointer to texture parameters for stretchblt.
 *                  This is specific to stretchblt only (not transform)
 *                  so the texture parameters
 *                  needs to be calculated based on destination rectangle before
 *                  transformation.
 *
 * @param Transform transformation applied to the stretchblt
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dStretchBlt(
    NvDdk2dHandle h2d,
    const NvRmSurface **ppSrc,
    NvU32 numSrcSurf,
    const NvRect *pSrcClampRect,
    const NvRect *pDstRect,
    const NvDdk2dTextureParam *pTexParam,
    NvDdk2dTransform Transform);

typedef struct NvDdk2dCompatibleTextureParamRec NvDdk2dCompatibleTextureParam;

NvU32
NvDdk2dComputeCompatibleTexParam(
    const NvRect* DstRect,
    const NvRect* SrcRect,
    NvDdk2dCompatibleTextureParam** TexParam);

NvError 
NvDdk2dCompatibleStretchBlt(
    NvDdk2dHandle h2d,
    const NvRmSurface **ppSrc,
    NvU32 numSrcSurf,
    const NvRect *pSrcClampRect,
    const NvRect *pDstRect,
    const NvDdk2dCompatibleTextureParam *pTexParam,
    NvU32 numTexParam,
    NvDdk2dTransform Transform);

/** 
 * NvDdk2dCoverageResolve to perform a coverage resolve operation.
 *
 * Performs a Virtual Coverage AA resolve operation, using color data in the
 * source surface and coverage data in the coverage surface.
 *
 * The color format of the coverage surface should be NvColorFormat_A8
 * (PLEASE NOTE :Still need to make sure whether this is the correct color 
 * format for the coverage surface)
 *
 * @param h2D handle to the 2D API
 *  
 * @param pSrc pointer to source surface pointer.
 *
 * @param pCov pointer to the coverage surface.  
 *
 * @param pSrcLoc  pointer to source location
 *
 * @param pDstRect pointer to destination rectangle indicating the
 *                 destination area.  
 *
 * @retval NvSuccess on success
 */ 
NvError 
NvDdk2dCoverageResolve(
    NvDdk2dHandle h2d,
    const NvRmSurface *pSrc,
    const NvRmSurface *pCov,
    const NvPoint *pSrcLoc,
    const NvRect  *pDstRect);

/** 
 * NvDdk2dDrawPrimitivesOld to draw 2D Primitives like lines, lineStrip and
 * triangles. 
 * For drawing lines, the count parameter has to be a multiple
 * of 2 and for drawing triangles it should be a multiple of 3.
 * 
 * It is called "Old" so that it's symbol doesn't collide with the NvDdk2d_v2
 * function of the same name.
 *   
 * @param h2d handle to the 2D API
 *
 * @param PrimitiveType selects the primitive that needs to be drawn
 *
 * @param pLinesPoint pointer to an array of points.  The count parameter
 *                    indicates the number of points in the array. 
 *
 * @param count count indicating the number of points in the pLinesPoint 
 *              parameter.Count must be greater than 1.  
 *
 * @retval NvSuccess on success
 */
NvError 
NvDdk2dDrawPrimitivesOld(
    NvDdk2dHandle h2d,
    NvDdk2dPrimitiveType PrimitiveType,
    const NvPoint *pLinesPoint,
    NvU32 count);

/** 
 * NvDdk2dFlush to flush all outstanding 2D commands into the channel
 * with the added option of an output pointer of NvRmFence type which
 * provides information to the client about the SyncPointID and Value
 * that should be used by the client if he intends to wait for the 2d
 * operation to finish.
 *  
 * @param h2d handle to the NvDdk2d API
 *   
 * @param pFence pointer to the NvRmFence struct
 *               The sync point id and value needed by the client
 *               are returned by NvDdk2dFlush in this pointer.
 *               It is valid for the client to pass this pointer as NULL.
 *
 * @retval NvSuccess on success.
 */   
NvError 
NvDdk2dFlush(
    NvDdk2dHandle h2d,
    NvRmFence *pFence);
 
#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVDDK_2D_OLD_H 
