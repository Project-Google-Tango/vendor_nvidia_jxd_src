/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SHADERMASTER_H
#define SHADERMASTER_H

#include "nvrm_channel.h"
#include "nvrm_surface.h"
#include "nvcommon.h"

/* ------------------------------------------------------------

TODO:
 . Add attributes and uniforms
 . Add textures
 . Control over texture coordinate clamping
 . control over register spill flag for frag shaders
 . primitive linking support for varyings
 . BUG: there seems to be an off-by-8 issue with vertex program length
 . Add Fence

------------------------------------------------------------ */

typedef struct NvShaderMasterRec *NvShaderMaster;
typedef struct NvSmProgramHandleOpaque *NvSmProgramHandle;

typedef enum {
    NvSmBufferId_0,
    NvSmBufferId_1,
    NvSmBufferId_2,
    NvSmBufferId_3,
    NvSmBufferId_4,
    NvSmBufferId_5,
    NvSmBufferId_6,
    NvSmBufferId_7,
    NvSmBufferId_8,
    NvSmBufferId_9,
    NvSmBufferId_10,
    NvSmBufferId_11,
    NvSmBufferId_12,
    NvSmBufferId_13,
    NvSmBufferId_14,
    NvSmBufferId_15,
    NvSmBufferId_Color0 = NvSmBufferId_1,
    NvSmBufferId_Color1 = NvSmBufferId_5,
    NvSmBufferId_Color2 = NvSmBufferId_6,
    NvSmBufferId_Color3 = NvSmBufferId_7,
    NvSmBufferId_Color4 = NvSmBufferId_8,
    NvSmBufferId_Color5 = NvSmBufferId_9,
    NvSmBufferId_Color6 = NvSmBufferId_10,
    NvSmBufferId_Color7 = NvSmBufferId_11,
    NvSmBufferId_Color8 = NvSmBufferId_12,
    NvSmBufferId_Color9 = NvSmBufferId_13,
    NvSmBufferId_Color10 = NvSmBufferId_14,
    NvSmBufferId_Color11 = NvSmBufferId_15,
    NvSmBufferId_Force32 = 0x7FFFFFFF
} NvSmBufferId;

typedef enum {
    NvSmBufferFormat_B2G3R3,
    NvSmBufferFormat_S8,
    NvSmBufferFormat_L8,
    NvSmBufferFormat_A8,
    NvSmBufferFormat_C4X4,

    NvSmBufferFormat_B5G5R5A1,
    NvSmBufferFormat_A1B5G5R5,
    NvSmBufferFormat_B4G4R4A4,
    NvSmBufferFormat_A4B4G4R4,
    NvSmBufferFormat_B5G6R5,
    NvSmBufferFormat_L8A8,
    NvSmBufferFormat_L16_float,
    NvSmBufferFormat_A16_float,

    NvSmBufferFormat_P32_float,
    NvSmBufferFormat_R8G8B8A8,
    NvSmBufferFormat_B8G8R8A8,
    NvSmBufferFormat_R11G11B10_float,
    NvSmBufferFormat_L16A16_float,

    NvSmBufferFormat_R16G16B16A16_float,

    NvSmBufferFormat_P128,

    NvSmBufferFormat_Force32 = 0x7FFFFFFF
} NvSmBufferFormat;

typedef enum {
    NvSmBufferLayout_Linear,
    NvSmBufferLayout_Swizzled,
    NvSmBufferLayout_XYTiledLinear,
    NvSmBufferLayout_XYTiledSwizzled,

    NvSmBufferLayout_Force32 = 0x7FFFFFFF
} NvSmBufferLayout;

// NvSmBufferDesc is in fact a pointer to the chip-specific structures below.
typedef struct NvSmAp15BufferRec {
    NvBool IsTexture;
    NvSmBufferId BufferId;
    NvSmBufferFormat BufferFormat;
    NvSmBufferLayout BufferLayout;
} NvSmAp15Buffer;

typedef NvSmAp15Buffer NvSmBuffer;

typedef enum {
    NvSmTexFlag_None = 0x00,
    NvSmTexFlag_Normalize = 0x01,
    NvSmTexFlag_MirrorX = 0x02,
    NvSmTexFlag_MirrorY = 0x04,
    NvSmTexFlag_PointSample = 0x08,
    NvSmTexFlag_ClampX = 0x10,
    NvSmTexFlag_ClampY = 0x20
} NvSmTexFlag;
#define NvSmTexFlag_Mirror (NvSmTexFlag_MirrorX | NvSmTexFlag_MirrorY)
#define NvSmTexFlag_Clamp (NvSmTexFlag_ClampX | NvSmTexFlag_ClampY)

typedef struct NvSmAp15BufferTextureRec {
    NvSmAp15Buffer common;
    NvU32 flags;   /* bitwise or of NvSmTexFlags. */
    NvU32 nSlices; /* non-zero => array slice count. */
} NvSmAp15BufferTexture;

typedef struct NvSmAp15BufferFramebufferRec {
    NvSmAp15Buffer common;
    NvBool ViewportHack;
} NvSmAp15BufferFramebuffer;

/* return 0 on fail.
   Allocates a context object
   Only one active context allowed per process
*/
NvShaderMaster NvSmInit(NvRmDeviceHandle hRmDev);

/* Set the output surface.  Surface must be not be swizzled, and must
   be appropriately aligned for 3D HW. */
NvError NvSmSetDest(NvShaderMaster sm, const NvRmSurface *surf,
                    const NvSmBuffer *pBuffer);
void NvSmSetViewportHackSize(NvShaderMaster sm, NvU32 Width, NvU32 Height);
// Return commands to change surface address in pp pointer.
void NvSmChangeDestAddress(NvShaderMaster sm, NvRmMemHandle hMem, NvU32 Offset,
                           const NvSmBuffer *buffer, NvU32 **pp);

#define NVSM_VERTEX_SHADER_COUNT 8
#define NVSM_FRAGMENT_SHADER_COUNT 8

NvError NvSmVertexShaderBinary(NvShaderMaster sm, NvU32 n,
                               const void *VertexShader);
NvError NvSmVertexShaderUcode(NvShaderMaster sm, NvU32 n,
        const void *VertexShader, NvU32 VertexShaderSizeBytes);
void NvSmVertexShaderFree(NvShaderMaster sm, NvU32 n);
NvError NvSmUseVertexShader(NvShaderMaster sm, NvU32 n);

NvError NvSmFragmentShaderBinary(NvShaderMaster sm, NvU32 n,
                                 const void *VertexShader);
NvError NvSmFragmentShaderUcode(NvShaderMaster sm, NvU32 n,
        const void *FragmentShader, NvU32 FragmentShaderSizeBytes);
NvError NvSmMultiEpochUcode(NvShaderMaster sm, NvU32 n,
                            const NvU8 *shader[],
                            NvU32 nEpochs, NvU32 *epochSize);
void NvSmFragmentShaderFree(NvShaderMaster sm, NvU32 n);
NvError NvSmUseFragmentShader(NvShaderMaster sm, NvU32 n);

/* VertexShader and FragShader are pointers to binary blobs, which are
   in .ar20 format.  (That is, shaderfix has been run on them.)
   This format is described in opengles2/al/ar20/include/glshaderbinary.h

   Note: The pointers for vertex and fragment shaders must remain
   valid in the caller's memory until the program object is destroyed.
   NvSM does not copy this data.
*/
NvSmProgramHandle NvSmCreateProgram(NvShaderMaster sm,
                                    const void *VertexShader, NvU32 vsSizeBytes,
                                    const void *FragShader, NvU32 fsSizeBytes);

NvSmProgramHandle NvSmCreateProgramFromShaders(NvShaderMaster sm,
                                               NvU32 hVertex, NvU32 hFragment,
                                               const void *LinkingInfo);

/* delete storage associated with this program.  The program handle is
   not valid after this call.  All programs associated with a
   ShaderMaster are autmatically deleted when NvSmDestroy() is
   called */
NvError NvSmDeleteProgram(NvShaderMaster sm, NvSmProgramHandle program);

NvError NvSmUseProgram(NvShaderMaster sm, NvSmProgramHandle program);

/* XXX comment! */
void NvSmSetVertexAttribute(NvShaderMaster sm,
        NvRmMemHandle hMem, NvU32 Offset, NvU32 Stride);
void NvSmSetBigTriangleAttributes(NvShaderMaster sm);
/* Renders Count squares.  Points array specifies upper-left corners. */
void NvSmRenderSquares(NvShaderMaster sm, NvU32 Size,
                        const NvPoint *Points, NvU32 Count);
/* Renders Count rectangeles.  This is done by setting scissor rectangle and
 * rendering one triangle covering entire framebuffer.
 */
void NvSmRenderRectangles(NvShaderMaster sm,
                           const NvRect *Rectangles, NvU32 Count);

NvU32 NvSmRenderTriangleStrip(NvShaderMaster sm, const NvPoint *points,
                              NvU32 count);
// XXX need better way to specify texture coordinates
/* NOTE: Texture coordinates are specified in pixels, and are
   normalized by the implementation. */
NvU32 NvSmRenderTriangleStrip2(NvShaderMaster sm, const NvPoint *points,
                               const NvPoint *texture_coords,
                               NvU32 count);
// XXX comment!
NvU32 NvSmRenderTriangleStrip2FV(NvShaderMaster sm, NvRmMemHandle hMem,
                                NvU32 Offset1, NvU32 Stride1,
                                NvU32 Offset2, NvU32 Stride2,
                                NvU32 count);
NvU32 NvSmRenderTriangleStrip2IV(NvShaderMaster sm, NvRmMemHandle hMem,
                                NvU32 Offset1, NvU32 Stride1,
                                NvU32 Offset2, NvU32 Stride2,
                                NvU32 count);
// Legacy support - assumes signed long data
NvU32 NvSmRenderTriangleStrip2V(NvShaderMaster sm, NvRmMemHandle hMem,
                                NvU32 Offset1, NvU32 Stride1,
                                NvU32 Offset2, NvU32 Stride2,
                                NvU32 count);
NvU32 NvSmRenderTriangles2V(NvShaderMaster sm, NvRmMemHandle hMem,
                            NvU32 Offset1, NvU32 Stride1,
                            NvU32 Offset2, NvU32 Stride2,
                            NvU32 count);
NvU32 NvSmRenderTriangles3V(NvShaderMaster sm, NvRmMemHandle hMem,
                            NvU32 Offset1, NvU32 Stride1,
                            NvU32 Offset2, NvU32 Stride2,
                            NvU32 Offset3, NvU32 Stride3,
                            NvU32 count);
NvU32 NvSmRenderTriangleStrip3(NvShaderMaster sm, const NvPoint *points,
                               const NvPoint *texCoord0,
                               const NvPoint *texCoord1,
                               NvU32 count);

NvU32 NvSmSyncPointIncrementOpDone(NvShaderMaster sm);
/* Waits for a render operation to complete.  Pass a return value of
 * NvSmSyncPointIncrementOpDone. */
void NvSmWaitForRender(NvShaderMaster sm, NvU32 value);
/* Inserts a host wait for 3D. */
void NvSmHostWait(NvShaderMaster sm);

void NvSmSetVertexUniformByIndex(NvShaderMaster sm, NvU32 Index, NvU32 Count,
                                 const NvF32 *Constants);
void NvSmSetFragmentUniformByIndex(NvShaderMaster sm, NvU32 Index, NvU32 Count,
                                   const NvF32 *Constants);
void NvSmSetRawFragmentUniformByIndex(NvShaderMaster sm, NvU32 Index,
        NvU32 Count, const NvU32 *Constants);

/* set a float uniform.  For a single float value, nvals should be 1.
   For a vec<n>, nvals should == <n>.  For a matrix, nvals should be 16.
*/
NvError NvSmSetUniformf(NvShaderMaster sm, const char *name,
                        NvF32 *vals, int nvals);

#if 0
/* set an int uniform.  For a single value, nvals should be 1.
   For an array[n], nvals should == <n>.
*/
NvError NvSmSetUniformi(NvShaderMaster sm, const char *name,
                        NvS32 *vals, int nvals);

/* bind a texture to a texture uniform.  The <pos> parameter
   corresonds to the <texno> parameter in SetTex */
NvError NvSmBindTexUniform(NvShaderMaster sm, const char *name, int pos);
#endif


/* flush pending work to the HW. */
void NvSmFlush(NvShaderMaster sm);

void NvSmSendRawCommands(NvShaderMaster sm, const NvU32 *commands, NvU32 count);
void NvSmSendRawDrawCommands(NvShaderMaster sm, const NvU32 *commands, NvU32 count);

NvRmChannelHandle NvSmGetChannel(NvShaderMaster sm);

/* delete sm and associated state objects */
void NvSmDestroy(NvShaderMaster sm);

void NvSmAp15ChangeFragmentProgramStartAndCount(NvShaderMaster sm,
        NvU8 start, NvU8 count);
NvError NvSmAp15AllocateSpillSurface(NvShaderMaster sm,
        NvU32 stride, NvU32 elems, NvU32 format);
void NvSmAp15FlushFragmentDataCache(NvShaderMaster sm);
#endif
