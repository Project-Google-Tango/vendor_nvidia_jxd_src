/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbct.h"
#include "nvbit.h"
#include "nvbu.h"
#include "nvos.h"
#include "nvcommon.h"
#include "nvbct_nvinternal_t30.h"
#include "nvsku.h"
#include "nverror.h"
#include "nvassert.h"

NvError NvProtectBctInvariant (NvBctHandle hBct, NvBitHandle hBit,
        NvU32 BctPartId, NvU8 *NvInternalBlobBuffer, NvU32 NvInternalBlobBufferSize)
{
    NvError e = NvSuccess;
    NvU32 BctSize = 0;
    NvU8* BctBuffer = NULL;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvInternalOneTimeRaw InternalInfoOneTimeRaw;

    NvOsMemset(&InternalInfoOneTimeRaw, 0, sizeof(NvInternalOneTimeRaw));
    /**
     *  Read sku data from flash.
     *  Insert the skuinfo into the bct in ram.
     */
    e = NvBuBctGetSize(hBct, hBit, BctPartId, &BctSize);
    if (e != NvSuccess) {
        goto fail;
    }
    BctBuffer = NvOsAlloc(BctSize);
    if(!BctBuffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    e = NvBuBctRead(hBct, hBit, BctPartId, BctBuffer);
    /// We get NvError_InvalidState when flash contains no bct
    /// Don't consider that as failure
    if (e != NvSuccess && e != NvError_InvalidState){
        goto fail;
    }

    if (e != NvError_InvalidState)
    {
        Size = 0;
        /// Get the NvInternalRawOneTime info from the flash
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData((NvBctHandle)BctBuffer, NvBctDataType_InternalInfoOneTimeRaw,
            &Size, &Instance, NULL)
        );
        NV_CHECK_ERROR_CLEANUP(
             NvBctGetData((NvBctHandle)BctBuffer, NvBctDataType_InternalInfoOneTimeRaw,
             &Size, &Instance, (void *)&InternalInfoOneTimeRaw)
        );
    }

    if (NvInternalBlobBufferSize)
    {
        /// Blob is in the format offset, size, and data
        /// Offset is from the start of nvinternalOneTime
        NvU32 i = 0;
        NvU8 *InternalInfoU8 = (NvU8*)&InternalInfoOneTimeRaw;

        while(i < NvInternalBlobBufferSize)
        {
            NvU32 Offset, Size;
            NvOsMemcpy(&Offset, &(NvInternalBlobBuffer[i]), sizeof(NvU32));
            i+= sizeof(NvU32);
            NvOsMemcpy(&Size, &(NvInternalBlobBuffer[i]), sizeof(NvU32));
            i+= sizeof(NvU32);
            NvOsMemcpy(&(InternalInfoU8[Offset]), &(NvInternalBlobBuffer[i]), Size);
            i+= Size;
        }
        if( i > NvInternalBlobBufferSize)
        {
            e = NvError_BadParameter;
            goto fail;
        }
    }

    Size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_InternalInfoOneTimeRaw,
        &Size, &Instance, NULL)
    );
    /// Update the in-ram copy of the BCT
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData((NvBctHandle) hBct, NvBctDataType_InternalInfoOneTimeRaw,
        &Size, &Instance, (void *)&InternalInfoOneTimeRaw)
    );

fail:
    if (BctBuffer)
        NvOsFree(BctBuffer);

    return e;
}

