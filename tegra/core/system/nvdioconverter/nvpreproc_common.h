/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVPREPROC_COMMON_H
#define INCLUDED_NVPREPROC_COMMON_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Converts a .dio file to the new format (dio2) by inserting the required
 * compaction blocks.
 *
 * @param InputFile The dio file to convert
 * @param OutputFile The converted file name
 */
NvError
PostProcDioOSImage(NvU8 *InputFile, NvU8 *OutputFile);

/**
 * Converts a .dio file to the new format (dio2) by inserting the required
 * compaction blocks.
 *
 * @param InputFile The dio file to convert
 * @param BlockSize The blocksize of the media
 * @param OutputFile The converted file name
 */
NvError
NvConvertStoreBin(NvU8 *InputFile, NvU32 BlockSize, NvU8 *OutputFile);

#if defined(__cplusplus)
}
#endif
#endif //INCLUDED_NVPREPROC_COMMON_H
