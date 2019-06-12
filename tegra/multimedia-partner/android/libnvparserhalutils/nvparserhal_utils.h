/* Copyright (c) 2011-2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV_PARSER_HAL_UTILS_H
#define INCLUDED_NV_PARSER_HAL_UTILS_H

#define COMMON_MAX_INPUT_BUFFER_SIZE       64 * 1024

typedef struct {
    uint32_t minBuffers;
    uint32_t maxBuffers;
    uint32_t minBufferSize;
    uint32_t maxBufferSize;
    uint16_t byteAlignment;
} NvParserHalBufferRequirements;

#endif // #ifndef INCLUDED_NV_PARSER_HAL_UTILS_H
