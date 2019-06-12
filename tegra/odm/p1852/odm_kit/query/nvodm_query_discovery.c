/*
 * Copyright (c) 2008-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: The peripheral connectivity database implementation.
 */

#include "nvcommon.h"
#include "nvodm_query_gpio.h"
#include "nvodm_modules.h"
#include "nvodm_query_discovery.h"
#include "subboards/nvodm_query_discovery_p1852_addresses.h"

static const NvOdmIoAddress s_SdioAddresses[] =
{
    { NvOdmIoModule_Sdio, 0x0, 0x0, 0 },
    { NvOdmIoModule_Sdio, 0x2, 0x0, 0 },
    { NvOdmIoModule_Sdio, 0x3, 0x0, 0 },
};

static NvOdmPeripheralConnectivity s_Peripherals[] =
{
#include "subboards/nvodm_query_discovery_p1852_peripherals.h"

    //  Sdio module
    {
        NV_ODM_GUID('s','d','i','o','_','m','e','m'),
        s_SdioAddresses,
        NV_ARRAY_SIZE(s_SdioAddresses),
        NvOdmPeripheralClass_Other,
    },
};

static const NvOdmPeripheralConnectivity*
NvApGetAllPeripherals (NvU32 *pNum)
{
    if (!pNum) {
        return NULL;
    }

    *pNum = NV_ARRAY_SIZE(s_Peripherals);
    return (const NvOdmPeripheralConnectivity *)s_Peripherals;
}

// This implements a simple linear search across the entire set of currently-
// connected peripherals to find the set of GUIDs that Match the search
// criteria.  More clever implementations are possible, but given the
// relatively small search space (max dozens of peripherals) and the relative
// infrequency of enumerating peripherals, this is the easiest implementation.
const NvOdmPeripheralConnectivity *
NvOdmPeripheralGetGuid(NvU64 SearchGuid)
{
    const NvOdmPeripheralConnectivity *pAllPeripherals;
    NvU32 NumPeripherals;
    NvU32 i;

    pAllPeripherals = NvApGetAllPeripherals(&NumPeripherals);

    if (!pAllPeripherals || !NumPeripherals) {
        return NULL;
    }

    for (i=0; i<NumPeripherals; i++) {
        if (SearchGuid == pAllPeripherals[i].Guid) {
            return &pAllPeripherals[i];
        }
    }

    return NULL;
}

static NvBool
IsBusMatch(
        const NvOdmPeripheralConnectivity *pPeriph,
        const NvOdmPeripheralSearch *pSearchAttrs,
        const NvU32 *pSearchVals,
        NvU32 offset,
        NvU32 NumAttrs)
{
    NvU32 i, j;
    NvBool IsMatch = NV_FALSE;

    for (i=0; i<pPeriph->NumAddress; i++)
    {
        j = offset;
        do
        {
            switch (pSearchAttrs[j])
            {
                case NvOdmPeripheralSearch_IoModule:
                    IsMatch = (pSearchVals[j] == (NvU32)(pPeriph->AddressList[i].Interface));
                    break;
                case NvOdmPeripheralSearch_Address:
                    IsMatch = (pSearchVals[j] == pPeriph->AddressList[i].Address);
                    break;
                case NvOdmPeripheralSearch_Instance:
                    IsMatch = (pSearchVals[j] == pPeriph->AddressList[i].Instance);
                    break;
                case NvOdmPeripheralSearch_PeripheralClass:
                default:
                    NV_ASSERT(!"Bad Query!");
                    break;
            }
            j++;
        } while (IsMatch && j<NumAttrs && pSearchAttrs[j]!=NvOdmPeripheralSearch_IoModule);

        if (IsMatch)
        {
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

static NvBool
IsPeripheralMatch(
        const NvOdmPeripheralConnectivity *pPeriph,
        const NvOdmPeripheralSearch *pSearchAttrs,
        const NvU32 *pSearchVals,
        NvU32 NumAttrs)
{
    NvU32 i;
    NvBool IsMatch = NV_TRUE;

    for (i=0; i<NumAttrs && IsMatch; i++)
    {
        switch (pSearchAttrs[i])
        {
            case NvOdmPeripheralSearch_PeripheralClass:
                IsMatch = (pSearchVals[i] == (NvU32)(pPeriph->Class));
                break;
            case NvOdmPeripheralSearch_IoModule:
                IsMatch = IsBusMatch(pPeriph, pSearchAttrs, pSearchVals, i, NumAttrs);
                break;
            case NvOdmPeripheralSearch_Address:
            case NvOdmPeripheralSearch_Instance:
                // In correctly-formed searches, these parameters will be parsed by
                // IsBusMatch, so we ignore them here.
                break;
            default:
                NV_ASSERT(!"Bad search attribute!");
                break;
        }
    }
    return IsMatch;
}

NvU32
NvOdmPeripheralEnumerate(
        const NvOdmPeripheralSearch *pSearchAttrs,
        const NvU32 *pSearchVals,
        NvU32 NumAttrs,
        NvU64 *pGuidList,
        NvU32 NumGuids)
{
    const NvOdmPeripheralConnectivity *pAllPeripherals;
    NvU32 NumPeripherals;
    NvU32 Matches;
    NvU32 i;

    pAllPeripherals = NvApGetAllPeripherals(&NumPeripherals);

    if (!pAllPeripherals || !NumPeripherals)
    {
        return 0;
    }

    if (!pSearchAttrs || !pSearchVals)
    {
        NumAttrs = 0;
    }

    for (i=0, Matches=0; i<NumPeripherals && (Matches < NumGuids || !pGuidList); i++)
    {
        if ( !NumAttrs || IsPeripheralMatch(&pAllPeripherals[i], pSearchAttrs, pSearchVals, NumAttrs) )
        {
            if (pGuidList)
                pGuidList[Matches] = pAllPeripherals[i].Guid;

            Matches++;
        }
    }

    return Matches;
}

NvBool
NvOdmPeripheralGetBoardInfo(
        NvU16 BoardId,
        NvOdmBoardInfo *pBoardInfo)
{
    return NV_FALSE;
}

NvBool
NvOdmQueryGetBoardModuleInfo(
        NvOdmBoardModuleType mType,
        void *BoardModuleData,
        int DataLen)
{

    NvOdmPmuBoardInfo *pPmuInfo;

    switch (mType)
    {
        case NvOdmBoardModuleType_PmuBoard:
            if (DataLen != sizeof(NvOdmPmuBoardInfo))
                return NV_FALSE;
            pPmuInfo = BoardModuleData;
            pPmuInfo->core_edp_mv = 1250;
            pPmuInfo->isPassBoardInfoToKernel = NV_FALSE;

            return NV_TRUE;

        default:
            break;
    }
    return NV_FALSE;
}

NvError
NvOdmQueryGetUpatedBoardModuleInfo(
    NvOdmBoardModuleType mType,
    NvU16 *BoardModuleData,
    NvOdmUpdatedBoardModuleInfoType datatype)
{
    return NvError_NotSupported;
}
