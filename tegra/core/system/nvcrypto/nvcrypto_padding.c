/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_padding.h"

NvError
NvCryptoPaddingQueryIsExplicit(
    NvCryptoPaddingType PaddingType,
    NvBool *pIsExplicit)
{
    if (!pIsExplicit)
        return NvError_InvalidAddress;
    
    switch (PaddingType)
    {
    case NvCryptoPaddingType_ExplicitBitPaddingAlways:
    case NvCryptoPaddingType_ExplicitBitPaddingOptional:
        *pIsExplicit = NV_TRUE;
        break;
        
    case NvCryptoPaddingType_ImplicitBitPaddingAlways:
    case NvCryptoPaddingType_ImplicitBitPaddingOptional:
        *pIsExplicit = NV_FALSE;
        break;
        
    default:
        return NvError_BadParameter;
    }
    
    return NvSuccess;
}

NvError
NvCryptoPaddingQuerySize(
    NvCryptoPaddingType PaddingType,
    NvU32 BlockSize,
    NvU32 IsEmptyPayload,
    NvU32 PayloadSizeModuloBlockSize,
    NvU32 *pPaddingSize)
{
    NvU32 PaddingSize;

    if (!pPaddingSize)
        return NvError_InvalidAddress;

    if (IsEmptyPayload && PayloadSizeModuloBlockSize != 0)
        return NvError_BadParameter;
    
    switch (PaddingType)
    {
    case NvCryptoPaddingType_ExplicitBitPaddingAlways:
    case NvCryptoPaddingType_ImplicitBitPaddingAlways:
        PaddingSize = BlockSize - PayloadSizeModuloBlockSize;
        break;
        
    case NvCryptoPaddingType_ExplicitBitPaddingOptional:
    case NvCryptoPaddingType_ImplicitBitPaddingOptional:
        if (IsEmptyPayload || PayloadSizeModuloBlockSize != 0)
            PaddingSize = BlockSize - PayloadSizeModuloBlockSize;
        else
            PaddingSize = 0;
        break;
        
    default:
        return NvError_BadParameter;
    }
   
    *pPaddingSize = PaddingSize;
    
    return NvSuccess;
}

NvError
NvCryptoPaddingQueryData(
    NvCryptoPaddingType PaddingType,
    NvU32 PaddingSize,
    NvU32 BufferSize,
    NvU8 *pBuffer)
{
    NvU32 i;
    
    if (!PaddingSize)
        return NvSuccess;
    
    // note that buffer size checked must precede buffer pointer validation;
    // this allows client to determine required buffer size before actually
    // allocating the buffer

    if (BufferSize<PaddingSize)
        return NvError_InsufficientMemory;
    
    if (!pBuffer)
        return NvError_InvalidAddress;

    // copy padding data to user buffer
    *pBuffer++ = 0x80;
    for (i=1; i<PaddingSize; i++)
        *pBuffer++ = 0x0;
    
    return NvSuccess;
}

