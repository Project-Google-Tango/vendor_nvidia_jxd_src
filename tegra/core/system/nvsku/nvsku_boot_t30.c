/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvbct.h"
#include "nvsku.h"
#include "nvbct_nvinternal_t30.h"
#include "nvmachtypes.h"
#include "nvodm_query.h"
#include "nvsku_private.h"
#include "t30/nvboot_bct.h"

#define NVSKU(x) x

static NvError NvBctGetSkuInfo(NvSkuInfo *SkuInfo)
{
    NvError e = NvSuccess;
    NvInternalOneTimeRaw InternalInfoOneTimeRaw;
    NvBctHandle hBct = NULL;
    NvU32 Size = 0, Instance = 0;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &hBct));

    NV_CHECK_ERROR_CLEANUP(NvBctGetData(hBct, NvBctDataType_InternalInfoOneTimeRaw,
                &Size, &Instance, NULL));
    NV_CHECK_ERROR_CLEANUP(NvBctGetData(hBct, NvBctDataType_InternalInfoOneTimeRaw,
                &Size, &Instance, (void *)&InternalInfoOneTimeRaw));

    NvOsMemset(SkuInfo, 0, sizeof(NvSkuInfo));
    NvOsMemcpy(&SkuInfo->Version, &(InternalInfoOneTimeRaw.SkuInfoRaw.Version),
            sizeof(SkuInfo->Version));
    NvOsMemcpy(&SkuInfo->TestConfig, &(InternalInfoOneTimeRaw.SkuInfoRaw.TestConfig),
            sizeof(SkuInfo->TestConfig));
    NvOsMemcpy(&SkuInfo->BomPrefix, &(InternalInfoOneTimeRaw.SkuInfoRaw.BomPrefix[0]),
            sizeof(SkuInfo->BomPrefix));
    NvOsMemcpy(&SkuInfo->Project, &(InternalInfoOneTimeRaw.SkuInfoRaw.Project[0]),
            sizeof(SkuInfo->Project));
    NvOsMemcpy(&SkuInfo->SkuId, &(InternalInfoOneTimeRaw.SkuInfoRaw.SkuId[0]),
            sizeof(SkuInfo->SkuId));
    NvOsMemcpy(&SkuInfo->Revision, &(InternalInfoOneTimeRaw.SkuInfoRaw.Revision[0]),
            sizeof(SkuInfo->Revision));
    NvOsMemcpy(&SkuInfo->SkuVersion[0], &(InternalInfoOneTimeRaw.SkuInfoRaw.SkuVersion[0]),
            sizeof(SkuInfo->SkuVersion));

fail:
    NvBctDeinit(hBct);
    return e;
}

static NvError GuessSku (NvU32 *pSku)
{
    NvU32 RamSize;
    NvError e = NvSuccess;

    RamSize = NvOdmQueryMemSize(NvOdmMemoryType_Sdram);
    switch ( RamSize )
    {
        case 0x20000000:
            //  512MB SKU2
            *pSku =  TEGRA_P1852_SKU2_A00;
            break;
        case 0x80000000:
            // 2GB SKU8
            *pSku =  TEGRA_P1852_SKU8_A00;
            break;

        default:
            // Fallback to generic
            *pSku = TEGRA_P1852_SKU2_A00;
            break;
    }
    return e;
}

NvError NvBootGetSkuNumber(NvU32 *pSku)
{
    NvError e = NvSuccess;
    NvSkuInfo SkuInfo;

    NV_CHECK_ERROR_CLEANUP(NvBctGetSkuInfo(&SkuInfo));

    if ( SkuInfo.Version == 0 )
    {
        NV_CHECK_ERROR_CLEANUP(GuessSku(pSku));
    }
    else
    {
        if ( SkuInfo.Project != EMBEDDED_P1852_PROJECT )
        {
            e = NvError_BadValue;
            goto fail;
        }

        switch (SkuInfo.SkuId)
        {
            case NVSKU(8):
                if (SkuInfo.Revision >= 100 && SkuInfo.Revision < 400)
                     *pSku = TEGRA_P1852_SKU8_A00;
                else if (SkuInfo.Revision >= 400)
                     *pSku = TEGRA_P1852_SKU8_B00;
                else
                {
                    e = NvError_BadValue;
                    goto fail;
                }
                break;

            case NVSKU(2):
                if (SkuInfo.Revision >= 100 && SkuInfo.Revision < 400)
                    *pSku = TEGRA_P1852_SKU2_A00;
                else if (SkuInfo.Revision >= 400)
                    *pSku = TEGRA_P1852_SKU2_B00;
                else
                {
                    e = NvError_BadValue;
                    goto fail;
                }
                break;

            case NVSKU(5):
                if (SkuInfo.Revision >= 100 && SkuInfo.Revision < 400)
                    *pSku = TEGRA_P1852_SKU5_A00;
                else if (SkuInfo.Revision >= 400)
                    *pSku = TEGRA_P1852_SKU5_B00;
                else
                {
                    e = NvError_BadValue;
                    goto fail;
                }
                break;
            default:
                e = NvError_BadValue;
                goto fail;
        }
    }

fail:
    return e;
}

NvError NvBootGetSkuInfo(NvU8 *pInfo, NvU32 Stringify, NvU32 strlength)
{
    NvError e = NvSuccess;

    NvSkuInfo SkuInfo;

    NV_CHECK_ERROR_CLEANUP(NvBctGetSkuInfo(&SkuInfo));
    if (Stringify)
    {
        /* Use NvBootGetBaseSkuInfoVersion if needed */
        NvOsSnprintf((char*)pInfo, strlength, "nvsku=%03d-%05d-%04d-%03d SkuVer=%c%c",
                SkuInfo.BomPrefix, SkuInfo.Project,
                SkuInfo.SkuId, SkuInfo.Revision, SkuInfo.SkuVersion[0],
                SkuInfo.SkuVersion[1]);
    }
    else
    {
        NvOsMemcpy(pInfo, &SkuInfo, sizeof(NvSkuInfo));
    }

fail:
    return e;
}
