/*
* Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NV_ENC_MAKERNOTE_H
#define INCLUDED_NV_ENC_MAKERNOTE_H

#include "nvcommon.h"
#include "nvos.h"

#define NV_MNENC_BLOCK_SIZE 8

void encipher(NvU8 *src, NvU8 *dst);
NvError encodeMakerNote(NvU8 *ptr, NvU8 *makerNote, NvU32 len);
NvU32 MNoteTrimSize(NvU32 input);
NvU32 MNoteEncodeSize(NvU32 input);

#endif // INCLUDED_NV_ENC_MAKERNOTE_H
