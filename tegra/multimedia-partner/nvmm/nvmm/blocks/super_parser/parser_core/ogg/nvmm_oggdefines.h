/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream

 ********************************************************************/

 #ifndef _INCLUDED_NVMM_OGGDEFINES_H
#define _INCLUDED_NVMM_OGGDEFINES_H
#include "nvos.h"


//#define REPLACE_FETCH_AND_PROCESS_FUNCTION_BY_DEFN
//#define MOVE_LAP_AFTER_SYNTHESIS
//#define MOVE_READ_TO_A_SEPERATE_FUNCTION
//#define LIMIT_OUTPUT_TO_500_FRAMES
//#define USE_SEEK_FUNCTIONS
//#define DBG_BUF_PRNT
//#define USE_STATIC_INBUF
#define DIFFERENT_STRUCT_FOR_READ_AND_DECODE
//#define USE_COMMAND_LINE
//#define OVERRIDE_RUNTIMEMEMALLOC_FUNC
#define CODEBOOK_INFO_CHANGE

//#ifdef OVERRIDE_RUNTIMEMEMALLOC_FUNC

#define memset NvOsMemset
#define memcpy NvOsMemcpy
//#endif

#endif //#ifndef _INCLUDED_NVMM_OGGDEFINES_H

