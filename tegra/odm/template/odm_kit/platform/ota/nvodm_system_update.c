/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_system_update.h"
#include "nvodm_services.h"

NvBool
NvOdmSysUpdateSign( NvU8 *Data, NvU32 Size, NvBool bFirst, NvBool bLast )
{
    return NV_TRUE;
}

NvBool
NvOdmSysUpdateGetSignature( NvU8 *Signature, NvU32 Size )
{
    NvOdmOsMemset( Signature, 0, Size );
    return NV_TRUE;
}

NvBool
NvOdmSysUpdateIsUpdateAllowed(
    NvOdmSysUpdateBinaryType binary,
    NvU32 OldVersion,
    NvU32 NewVersion)
{
    NvBool result = NV_FALSE;

    switch (binary)
    {
        case NvOdmSysUpdateBinaryType_Bootloader:
            if (NewVersion >= OldVersion)
            {
                result = NV_TRUE;;
            }
            break;
        default:
            break;
    }
    return result;
}

