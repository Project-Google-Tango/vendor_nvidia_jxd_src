/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_random.h"
#include "nvcrypto_random_ansix931.h"

NvError
NvCryptoRandomSelectAlgorithm(
    NvCryptoRandomAlgoType RandomAlgoType,
    NvCryptoRandomAlgoHandle *pRandomAlgoHandle)
{
    NvError e = NvSuccess;
    
    if (!pRandomAlgoHandle)
        return NvError_InvalidAddress;

    switch (RandomAlgoType)
    {
        case NvCryptoRandomAlgoType_AnsiX931Aes:
            NV_CHECK_ERROR(NvCryptoRandomSelectAlgorithmAnsiX931Aes(
                               RandomAlgoType,
                               pRandomAlgoHandle));
            break;
        default:
            e = NvError_BadParameter;
            break;
    }
    
    return e;
}

