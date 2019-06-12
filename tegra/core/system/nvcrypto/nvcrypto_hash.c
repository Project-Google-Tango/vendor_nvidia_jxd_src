/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_hash.h"
#include "nvcrypto_hash_cmac.h"

NvError
NvCryptoHashSelectAlgorithm(
    NvCryptoHashAlgoType HashAlgoType,
    NvCryptoHashAlgoHandle *pHashAlgoHandle)
{
    NvError e = NvSuccess;
    
    switch (HashAlgoType)
    {
        case NvCryptoHashAlgoType_AesCmac:
            NV_CHECK_ERROR(NvCryptoHashSelectAlgorithmCmac(
                               HashAlgoType,
                               pHashAlgoHandle));
            break;
        default:
            e = NvError_BadParameter;
            break;
    }
    
    return e;
}

