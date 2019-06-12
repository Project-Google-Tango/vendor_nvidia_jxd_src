/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvapputil.h"
#include "nvgetdtb_platform.h"

static char *
nvgetdtb_map_boardid(NvGetDtbBoardType BoardType)
{
    char *pRet = NULL;

    switch(BoardType)
    {
        case NvGetDtbBoardType_ProcessorBoard:
            pRet = "Processor";
            break;
        case NvGetDtbBoardType_PmuBoard:
            pRet = "PMU";
            break;
        case NvGetDtbBoardType_DisplayBoard:
            pRet = "Display";
            break;
        case NvGetDtbBoardType_AudioCodec:
            pRet = "Audio";
            break;
        case NvGetDtbBoardType_CameraBoard:
            pRet = "Camera";
            break;
        case NvGetDtbBoardType_Sensor:
            pRet = "Sensor";
            break;
        case NvGetDtbBoardType_NFC:
            pRet = "NFC";
            break;
        case NvGetDtbBoardType_Debug:
            pRet = "Debug";
            break;
        default:
            pRet = "Unknown";
    }
    return pRet;
}

static char *nvgetdtb_get_dtb_name(NvGetDtbBoardInfo BoardInfo[])
{
    char *pRet = NULL;
    switch (BoardInfo[NvGetDtbBoardType_ProcessorBoard].BoardId)
    {
        case NvGetDtbPlatformType_Pluto:
            pRet = "tegra114-pluto.dtb";
            break;
        case NvGetDtbPlatformType_Dalmore:
            if (BoardInfo[NvGetDtbBoardType_ProcessorBoard].Sku == 1000)
                pRet = "tegra114-dalmore-e1611-1000-a00-00.dtb";
            else
                pRet = "tegra114-dalmore-e1611-1001-a00-00.dtb";
            break;
        case NvGetDtbPlatformType_Atlantis:
            pRet = "tegra148-atlantis.dtb";
            break;
        case NvGetDtbPlatformType_Ceres:
            pRet = "tegra148-ceres.dtb";
            break;
        case NvGetDtbPlatformType_Macallan:
            pRet = "tegra114-macallan.dtb";
            break;
        case NvGetDtbPlatformType_Ardbeg:
        {
            switch (BoardInfo[NvGetDtbBoardType_ProcessorBoard].Sku)
            {
                case NvGetDtbSkuType_TegraNote:
                    if (BoardInfo[NvGetDtbBoardType_PmuBoard].BoardId ==
                        NvGetDtbPMUModuleType_E1769) {
                            pRet = "tegra124-tn8-e1780-1100-a03-01.dtb";
                            break;
                    }
                    switch (BoardInfo[NvGetDtbBoardType_Sensor].BoardId)
                    {
                        case NvGetDtbSensorModuleType_E1845:
                            pRet = "tegra124-tn8-e1780-1100-a03.dtb";
                            break;
                        default:
                            pRet = "tegra124-tn8-e1780-1100-a02.dtb";
                            break;
                    }
                    break;
                case NvGetDtbSkuType_ShieldErs:
                    switch (BoardInfo[NvGetDtbBoardType_ProcessorBoard].Fab)
                    {
                        case BOARD_FAB_A03:
                            pRet = "tegra124-ardbeg-a03-00.dtb";
                            break;
                        default:
                            pRet = "tegra124-ardbeg.dtb";
                            break;
                    }
                    break;
                default:
                    pRet = "tegra124-ardbeg.dtb";
                    break;
            }
            break;
        }
        case NvGetDtbPlatformType_TN8FFD:
        {
            switch (BoardInfo[NvGetDtbBoardType_ProcessorBoard].MemType)
            {
                case 0:
                    pRet = "tegra124-tn8-p1761-1270-a00.dtb";
                    break;
                default:
                    pRet = "tegra124-tn8-p1761-1470-a00.dtb";
                    break;
            }
            break;
        }
        case NvGetDtbPlatformType_TN8ERSPOP:
            pRet = "tegra124-tn8-e1922-1100-a00.dtb";
            break;
        case NvGetDtbPlatformType_Laguna:
        case NvGetDtbPlatformType_LagunaErsS:
        case NvGetDtbPlatformType_LagunaFfd:
            pRet = "tegra124-laguna.dtb";
            break;
        default:
            pRet = "unknown";
    }

    return pRet;
}

NvError
nvgetdtb_read_boardid(void)
{
    NvGetDtbBoardInfo BoardInfo[NvGetDtbBoardType_MaxNumBoards];
    NvU32 i = 0;
    NvBool ListBoardId = NV_FALSE;
    char *pBoardType = NULL;
    NvBool b = NV_TRUE;
    NvError e = NvSuccess;

    NvOsMemset((NvU8 *)&BoardInfo, 0x0, sizeof(BoardInfo));

    e = nvgetdtb_getdata((NvU8 *)BoardInfo, sizeof(BoardInfo), &ListBoardId);

    if (e)
        goto fail;

    if (ListBoardId)
    {
        for (i = 0; i < NvGetDtbBoardType_MaxNumBoards; i++)
        {
            pBoardType = nvgetdtb_map_boardid((NvGetDtbBoardType )i);
            NvAuPrintf(
                "   Type                : %s\n"
                "   Version             : %d\n"
                "   BoardID             : %d\n"
                "   SKU                 : %d\n"
                "   Fab                 : %d\n"
                "   Revision            : %d\n"
                "   MinorRevision       : %d\n"
                "   MemType             : %d\n"
                "   PowerConfig         : %d\n"
                "   MiscConfig          : %d\n"
                "   ModemBands          : %d\n"
                "   TouchScreenConfigs  : %d\n"
                "   DisplayConfigs      : %d\n\n",
                pBoardType,
                BoardInfo[i].Version,
                BoardInfo[i].BoardId,
                BoardInfo[i].Sku,
                BoardInfo[i].Fab,
                BoardInfo[i].Revision,
                BoardInfo[i].MinorRevision,
                BoardInfo[i].MemType,
                BoardInfo[i].PowerConfig,
                BoardInfo[i].MiscConfig,
                BoardInfo[i].ModemBands,
                BoardInfo[i].TouchScreenConfigs,
                BoardInfo[i].DisplayConfigs
                );
        }
    }
    else
    {
        NvAuPrintf("%s\n", nvgetdtb_get_dtb_name(BoardInfo));
    }

    e = nvgetdtb_forcerecovery();

fail:
    if (!b)
        e = NvError_UsbfTxfrFail;
    return e;
}
