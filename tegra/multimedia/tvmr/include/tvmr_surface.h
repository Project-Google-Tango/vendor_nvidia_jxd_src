/*
 * Copyright (c) 2010-2014, NVIDIA CORPORATION. All rights reserved. All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

#ifndef _TVMR_SURFACE_H
#define _TVMR_SURFACE_H

#include "nvcommon.h"

typedef enum {
   TVMRSurfaceType_Y_V_U_420,
   TVMRSurfaceType_Y_V_U_422,
   TVMRSurfaceType_Y_V_U_422T,
   TVMRSurfaceType_Y_V_U_444,
   TVMRSurfaceType_Y_V_U_Y_V_U_422,
   TVMRSurfaceType_Y_V_U_Y_V_U_420,
   TVMRSurfaceType_Y_UV_420,
   TVMRSurfaceType_Y_UV_420_Interlaced,
   TVMRSurfaceType_Y_UV_Y_UV_420,
   TVMRSurfaceType_Y_UV_422,
   TVMRSurfaceType_Y_UV_422T,
   TVMRSurfaceType_Y_UV_444,
   TVMRSurfaceType_YUYV_422,
   TVMRSurfaceType_YUYV_422_Interlaced,
   TVMRSurfaceType_R8G8B8A8,
   TVMRSurfaceType_B5G6R5
} TVMRSurfaceType;

#define TVMRSurfaceType_YV12            TVMRSurfaceType_Y_V_U_420
#define TVMRSurfaceType_NV12            TVMRSurfaceType_Y_UV_420
#define TVMRSurfaceType_YV16x2          TVMRSurfaceType_Y_V_U_Y_V_U_422
#define TVMRSurfaceType_YV12x2          TVMRSurfaceType_Y_V_U_Y_V_U_420
#define TVMRSurfaceType_YV16            TVMRSurfaceType_Y_V_U_422
#define TVMRSurfaceType_YV24            TVMRSurfaceType_Y_V_U_444
#define TVMRSurfaceType_YV16_Transposed TVMRSurfaceType_Y_V_U_422T
#define TVMRSurfaceType_NV12x2          TVMRSurfaceType_Y_UV_Y_UV_420
#define TVMRSurfaceType_NV24            TVMRSurfaceType_Y_UV_420_Interlaced

typedef struct {
   NvU32 pitch;
   void * mapping;
   void * priv;
} TVMRSurface;

typedef struct {
   TVMRSurfaceType type;
   NvU32 width;
   NvU32 height;
   TVMRSurface * surface;
} TVMROutputSurface;

typedef struct {
   TVMRSurfaceType type;
   NvU32 width;
   NvU32 height;
   TVMRSurface * surfaces[6];
} TVMRVideoSurface;

typedef void* TVMRFence;

#endif /* _TVMR_SURFACE_H */
