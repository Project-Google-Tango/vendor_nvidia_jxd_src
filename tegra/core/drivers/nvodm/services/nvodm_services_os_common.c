/*
 * Copyright (c) 2007-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_services.h"
#include "nvos.h"

NvBool
NvOdmOsGetOsInformation( NvOdmOsOsInfo *pOsInfo )
{
    NvOsOsInfo info;
    NvError e;

    if( !pOsInfo )
    {
        return NV_FALSE;
    }

    e = NvOsGetOsInformation( &info );
    if( e != NvSuccess )
    {
        NvOsMemset(pOsInfo, 0, sizeof(*pOsInfo));
        return NV_FALSE;
    }

    switch( info.OsType ) {
    case NvOsOs_Windows:
        pOsInfo->OsType = NvOdmOsOs_Windows;
        break;
    case NvOsOs_Linux:
        pOsInfo->OsType = NvOdmOsOs_Linux;
        break;
    case NvOsOs_Aos:
        pOsInfo->OsType = NvOdmOsOs_Aos;
        break;
    case NvOsOs_Unknown:
    default:
        pOsInfo->OsType = NvOdmOsOs_Unknown;
        break;
    }

    switch( info.Sku ) {
    case NvOsSku_CeBase:
        pOsInfo->Sku = NvOdmOsSku_CeBase;
        break;
    case NvOsSku_Mobile_SmartFon:
        pOsInfo->Sku = NvOdmOsSku_Mobile_SmartFon;
        break;
    case NvOsSku_Mobile_PocketPC:
        pOsInfo->Sku = NvOdmOsSku_Mobile_PocketPC;
        break;
    case NvOsSku_Android:
        pOsInfo->Sku = NvOdmOsSku_Android;
        break;
    case NvOsSku_Unknown:
    default:
        pOsInfo->Sku = NvOdmOsSku_Unknown;
        break;
    }

    pOsInfo->MajorVersion = info.MajorVersion;
    pOsInfo->MinorVersion = info.MinorVersion;
    pOsInfo->SubVersion = info.SubVersion;
    pOsInfo->Caps = info.Caps;

    return NV_TRUE;
}

