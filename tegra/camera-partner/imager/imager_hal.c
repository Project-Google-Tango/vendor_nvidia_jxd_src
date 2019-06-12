/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#if (BUILD_FOR_AOS == 0)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "camera.h"
#include "imager_det.h"
#endif
#include "imager_hal.h"
#include "sensor_bayer_ar0832.h"
#include "sensor_bayer_ov5650.h"
#include "sensor_bayer_ov9726.h"
#include "sensor_bayer_ov2710.h"
#include "sensor_bayer_ov14810.h"
#include "sensor_yuv_ov7695.h"
#include "sensor_yuv_mt9m114.h"
#include "imager_nvc.h"
#include "sensor_bayer_ov5693.h"
#include "nvc_bayer_ov9772.h"
#include "nvc_bayer_imx091.h"
#include "sensor_bayer_imx179.h"
#include "focuser_ar0832.h"
#include "focuser_ad5820.h"
#include "focuser_ad5823.h"
#include "focuser_nvc.h"
#include "torch_nvc.h"
#include "sensor_yuv_soc380.h"
#include "sensor_yuv_ov5640.h"
#include "sensor_null.h"
#include "sensor_host.h"
#include "sensor_bayer_imx132.h"
#include "sensor_bayer_ar0261.h"
#include "nvodm_imager_build_date.h"
#include "sensor_bayer_imx135.h"
#include "sensor_bayer_ar0833.h"
#include "focuser_dw9718.h"
#include "sensor_yuv_sp2529.h"

#if ENABLE_NVIDIA_CAMTRACE
#include "nvcamtrace.h"
#endif

#if NV_ENABLE_DEBUG_PRINTS
#define HAL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define HAL_DB_PRINTF(...)
#endif

#define NVODM_IMAGER_MAX_CLOCKS (3)
#define MAX_BUILD_DATE_LENGTH   40

typedef struct DeviceHalTableRec
{
    NvU64 GUID;
    pfnImagerGetHal pfnGetHal;
}DeviceHalTable;

typedef struct NvcHalTableRec
{
    NvU32 DevId;
    pfnNvcImagerGetHal pfnGetHal;
}NvcHalTable;

typedef enum
{
    NvOdmImagerSubDeviceType_Sensor,
    NvOdmImagerSubDeviceType_Focuser,
    NvOdmImagerSubDeviceType_Flash,
} NvOdmImagerSubDeviceType;

static NV_INLINE NvOdmImagerSubDeviceType
GetSubDeviceForParameter(
    NvOdmImagerParameter Param);

static void
GetDefaultCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCap);

/**
 * g_SensorHalTable
 * Consist of currently supported sensor guids and their GetHal functions.
 * The sensors listed here must have a peripherial connection.
 */
DeviceHalTable g_SensorHalTable[] =
{
    {SENSOR_BAYER_OV5693_GUID,   SensorBayerOV5693_GetHal},
    {SENSOR_BAYER_OV5650_GUID,   SensorBayerOV5650_GetHal},
    {SENSOR_BYRRI_AR0832_GUID,   SensorBayerAR0832_GetHal},
    {SENSOR_BYRLE_AR0832_GUID,   SensorBayerAR0832_GetHal},
    {SENSOR_BYRST_OV5650_GUID,   SensorBayerOV5650_GetHal},
    {SENSOR_BYRST_AR0832_GUID,   SensorBayerAR0832_GetHal},
    {SENSOR_BAYER_OV9726_GUID,   SensorBayerOV9726_GetHal},
    {SENSOR_BAYER_OV2710_GUID,   SensorBayerOV2710_GetHal},
    {SENSOR_YUV_SOC380_GUID,     SensorYuvSOC380_GetHal},
    {SENSOR_YUV_OV5640_GUID,     SensorYuvOV5640_GetHal},
    {SENSOR_YUV_OV7695_GUID,     SensorYuvOV7695_GetHal},
    {SENSOR_YUV_MT9M114_GUID,    SensorYuvMT9M114_GetHal},
    {SENSOR_BAYER_OV14810_GUID,  SensorBayerOV14810_GetHal},
    {SENSOR_BAYER_IMX132_GUID,   SensorBayerIMX132_GetHal},
    {SENSOR_BAYER_AR0261_GUID,   SensorBayerAR0261_GetHal},
    {SENSOR_BAYER_IMX135_GUID,   SensorBayerIMX135_GetHal},
    {SENSOR_BAYER_IMX179_GUID,   SensorBayerIMX179_GetHal},
    {SENSOR_BAYER_AR0833_GUID,   SensorBayerAR0833_GetHal},
    {SENSOR_BAYER_OV5693_FRONT_GUID,   SensorBayerOV5693_GetHal},
	{SENSOR_YUV_SP2529_GUID,     SensorYuvSP2529_GetHal},
    {IMAGER_NVC_GUID,            NvcImager_GetHal},
    {IMAGER_NVC_GUID1,           NvcImager_GetHal},
    {IMAGER_NVC_GUID2,           NvcImager_GetHal},
    {IMAGER_NVC_GUID3,           NvcImager_GetHal},
    {IMAGER_ST_NVC_GUID1,        NvcImager_GetHal},
};

/**
 * g_VirtualSensorHalTable
 * Consist of currently supported virtual sensor guids and their GetHal
 * functions. The virtual sensors listed here do not need a peripherial
 * connection.
 */
DeviceHalTable g_VirtualSensorHalTable[] =
{
    // default virtual sensor listed first
    {NV_IMAGER_NULLYUV_ID,   SensorNullYuv_GetHal},
    {NV_IMAGER_NULLBAYER_ID, SensorNullBayer_GetHal},
    {NV_IMAGER_HOST_ID,      SensorHost_GetHal},
    {NV_IMAGER_NULLCSIA_ID,  SensorNullCSIA_GetHal},
    {NV_IMAGER_NULLCSIB_ID,  SensorNullCSIB_GetHal},
    {NV_IMAGER_TPGABAYER_ID, SensorNullTPGABayer_GetHal},
    {NV_IMAGER_TPGARGB_ID,   SensorNullTPGARgb_GetHal},
    {NV_IMAGER_TPGBBAYER_ID, SensorNullTPGBBayer_GetHal},
    {NV_IMAGER_TPGBRGB_ID,   SensorNullTPGBRgb_GetHal},
};

/**
 * g_FocuserHalTable
 * Consist of currently supported focuser guids and their GetHal functions.
 */
DeviceHalTable g_FocuserHalTable[] =
{
    {FOCUSER_AD5820_GUID, FocuserAD5820_GetHal},
    {FOCUSER_AR0832_GUID, FocuserAR0832_GetHal},
    {FOCUSER_AD5823_GUID, FocuserAD5823_GetHal},
    {FOCUSER_DW9718_GUID, FocuserDW9718_GetHal},
    {FOCUSER_NVC_GUID, FocuserNvc_GetHal},
    {FOCUSER_NVC_GUID1, FocuserNvc_GetHal},
    {FOCUSER_NVC_GUID2, FocuserNvc_GetHal},
    {FOCUSER_NVC_GUID3, FocuserNvc_GetHal},
    {FOCUSER_NVC_IMAGER, FocuserNvc_GetHal},
};

/**
 * g_FlashHalTable
 * Currently supported flash guids and their GetHal functions.
 */
DeviceHalTable g_FlashHalTable[] =
{
    {TORCH_NVC_GUID, TorchNvc_GetHal},
};

NvcHalTable g_NvcHalTable[] =
{
    {0x9772, NvcOV9772_GetHal},
    {0x0091, NvcIMX091_GetHal},
};

NvBool
NvOdmImagerGetDevices(
    NvS32 *pCount,
    NvOdmImagerHandle *phImagers)
{
    return NV_FALSE;
}

void
NvOdmImagerReleaseDevices(
    NvS32 Count,
    NvOdmImagerHandle *phImagers)
{
    NvS32 i;
    NvOdmImager *pImager = NULL;

    for (i = 0; i < Count; i++)
    {
        // Close sensor
        pImager = (NvOdmImager *)phImagers[i];
        if (pImager->pSensor)
            pImager->pSensor->pfnClose(pImager);

        //Close focuser
        if (pImager->pFocuser)
            pImager->pFocuser->pfnClose(pImager);

        //Close flash
        if (pImager->pFlash)
            pImager->pFlash->pfnClose(pImager);

        NvOdmOsFree(pImager->pSensor);
        NvOdmOsFree(pImager->pFocuser);
        NvOdmOsFree(pImager->pFlash);
        NvOdmOsFree(pImager);
    }
}

char *ImagerHalGetGuidStr(NvU64 guid, char *buf, NvU32 size)
{
    NvS32 i;

    if (size < 2)
        return NULL;

    if (guid < NVODM_IMAGER_MAX_REAL_IMAGER_GUID)
    {
        buf[0] = '0' + guid;
        buf[1] = 0;
        return buf;
    }

    for(i = (NvS32)size - 2; i >= 0; i--)
    {
        buf[i] = (char)(0xFFULL & guid);
        guid /= 0x100;
    }
    buf[size - 1] = 0;
    return buf;
}


NvBool
ImagerHalGuidGet(NvU64 *ImagerGUID, NvU64 *FocuserGUID, NvU64 *FlashGUID, NvBool *UseSensorCapGUIDs)
{
    NvBool Result = NV_TRUE;
    NvU64 guid = *ImagerGUID;
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

#if (BUILD_FOR_AOS == 0)
    if (guid == 0 || guid == 1)
    {
        // sensor auto detection for rear/front camera
        struct cam_device_layout *pLayout, *pDev;
        NvU32 i;

        Result = NvOdmImagerGetLayout(&pLayout, &i);
        if (Result == NV_FALSE || pLayout == NULL)
        {
            HAL_DB_PRINTF("%s cannot get device layout %d %p\n", __func__, Result, pLayout);
            // upon get device layout fail or layout empty, go to normal open procedule
            goto ImagerHalGuid_regular;
        }

        if (!NvOdmImagerHasSensorInPos(pLayout, guid))
            return NV_FALSE;

        pDev = pLayout;
        *ImagerGUID = *FocuserGUID = *FlashGUID = 0ULL;
        while (pDev->guid) {
            HAL_DB_PRINTF("%s PCL %s %x\n", __func__,
                ImagerHalGetGuidStr(pDev->guid, buf, sizeof(buf)), pDev->pos);
            switch (pDev->type)
            {
                case DEVICE_SENSOR:
                    if (*ImagerGUID)
                        break;
                    if (guid == pDev->pos)
                    {
                        *ImagerGUID = pDev->guid;
                        HAL_DB_PRINTF("Auto Detection %s ImagerGUID == %s\n",
                            pDev->pos? "Front" : "Rear", ImagerHalGetGuidStr(*ImagerGUID, buf, sizeof(buf)));
                    }
                    break;

                case DEVICE_FOCUSER:
                    if (*FocuserGUID)
                        break;

                    if (guid == pDev->pos)
                    {
                        *FocuserGUID = pDev->guid;
                        HAL_DB_PRINTF("Auto Detection %s FocuserGUID == %s\n",
                            pDev->pos? "Front" : "Rear", ImagerHalGetGuidStr(*FocuserGUID, buf, sizeof(buf)));
                    }
                    break;

                case DEVICE_FLASH:
                    if (*FlashGUID)
                        break;

                    if (guid == pDev->pos)
                    {
                        *FlashGUID = pDev->guid;
                        HAL_DB_PRINTF("Auto Detection %s FlashGUID == %s\n",
                            pDev->pos? "Front" : "Rear", ImagerHalGetGuidStr(*FlashGUID, buf, sizeof(buf)));
                    }
                    break;

                default:
                    break;
            }
            pDev++;
        }
        NvOsFree(pLayout);
        if (UseSensorCapGUIDs && (*ImagerGUID > 1))
        {
            *UseSensorCapGUIDs = NV_FALSE;
        }
    }
#endif

ImagerHalGuid_regular:
    Result = NV_TRUE;
    //find the sensor guid for ImagerGUID = 0 to NVODM_IMAGER_MAX_REAL_IMAGER_GUID
    if (*ImagerGUID < NVODM_IMAGER_MAX_REAL_IMAGER_GUID)
    {
        NvOdmPeripheralSearch FindImagerAttr =
            NvOdmPeripheralSearch_PeripheralClass;
        NvU32 FindImagerVal = (NvU32)NvOdmPeripheralClass_Imager;
        NvU32 Count;
        NvU64 GUIDs[NVODM_IMAGER_MAX_REAL_IMAGER_GUID];

        Count = NvOdmPeripheralEnumerate(&FindImagerAttr,
                                         &FindImagerVal, 1,
                                         GUIDs, NVODM_IMAGER_MAX_REAL_IMAGER_GUID);

        if (Count == 0)
        {
            // if no sensor in database, fall back to the default
            // virtual sensor
            HAL_DB_PRINTF("%s %d: no sensors found\n", __func__, __LINE__);
            *ImagerGUID = g_VirtualSensorHalTable[0].GUID;
        }
        else if (Count > guid)
        {
            *ImagerGUID = GUIDs[guid];
            HAL_DB_PRINTF("using ImagerGUID '%s' (%d)\n",
                ImagerHalGetGuidStr(*ImagerGUID, buf, sizeof(buf)), guid);
        }
        else
        {
            HAL_DB_PRINTF("%s %d: no sensors found\n", __func__, __LINE__);
            NV_ASSERT(!"Could not find any devices");
            Result = NV_FALSE;
        }
    }

    return Result;
}

pfnImagerGetHal
ImagerHalTableSearch(NvU64 DevGUID, NvOdmImagerDevice DevType, NvBool VirtualSensor)
{
    DeviceHalTable *pTable;
    NvU32 SizeofTable;
    pfnImagerGetHal pfnGetHal = NULL;
    NvU32 i;
#if NV_ENABLE_DEBUG_PRINTS
    char buf1[9];
    char buf2[9];
#endif

    HAL_DB_PRINTF("%s: %s %d\n", __func__, ImagerHalGetGuidStr(DevGUID, buf1, sizeof(buf1)), DevType);
    switch (DevType) {
        case NvOdmImagerDevice_Sensor:
            if (VirtualSensor)
            {
                pTable = g_VirtualSensorHalTable;
                SizeofTable = NV_ARRAY_SIZE(g_VirtualSensorHalTable);
            }
            else
            {
                pTable = g_SensorHalTable;
                SizeofTable = NV_ARRAY_SIZE(g_SensorHalTable);
            }
            break;
        case NvOdmImagerDevice_Focuser:
            pTable = g_FocuserHalTable;
            SizeofTable = NV_ARRAY_SIZE(g_FocuserHalTable);
            break;
        case NvOdmImagerDevice_Flash:
            pTable = g_FlashHalTable;
            SizeofTable = NV_ARRAY_SIZE(g_FlashHalTable);
            break;
        default:
            NvOsDebugPrintf("%s - undefined device type %d\n", __func__, DevType);
            return NV_FALSE;
    }

    for (i = 0; i < SizeofTable; i++)
    {
        HAL_DB_PRINTF("%s vs %s %d\n",
            ImagerHalGetGuidStr(pTable[i].GUID, buf1, sizeof(buf1)),
            ImagerHalGetGuidStr(DevGUID, buf2, sizeof(buf2)),
            i);
        if (DevGUID == pTable[i].GUID)
        {
            pfnGetHal = pTable[i].pfnGetHal;
            HAL_DB_PRINTF("%s found a device (%d)\n", __func__, i);
            break;
        }
    }

    return pfnGetHal;
}

NvBool
NvOdmImagerOpenExpanded(
    NvU64 SensorGUID,
    NvU64 FocuserGUID,
    NvU64 FlashGUID,
    NvBool UseSensorCapGUIDs,
    NvOdmImagerHandle *phImager)
{
    NvOdmImager *pImager = NULL;
    NvBool Result = NV_TRUE;
    pfnImagerGetHal pfnGetHal = NULL;
    NvOdmImagerCapabilities SensorCaps;
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

    HAL_DB_PRINTF("%s +++\n", __func__);
    HAL_DB_PRINTF("%s: Trying SensorGUID(%s)\n", __func__,
            (SensorGUID) ? ImagerHalGetGuidStr(SensorGUID, buf, sizeof(buf)):"none");
    HAL_DB_PRINTF("%s: Trying FocuserGUID(%s)\n", __func__,
            (FocuserGUID) ? ImagerHalGetGuidStr(FocuserGUID, buf, sizeof(buf)):"none");
    HAL_DB_PRINTF("%s: Trying FlashGUID(%s)\n", __func__,
            (FlashGUID) ? ImagerHalGetGuidStr(FlashGUID, buf, sizeof(buf)):"none");

    // search real sensors first.
    pfnGetHal = ImagerHalTableSearch(SensorGUID, NvOdmImagerDevice_Sensor, NV_FALSE);

    // if no real sensor found, search virtual sensors
    if (!pfnGetHal)
    {
        HAL_DB_PRINTF("%s %d: couldn't get the sensor HAL table for an imager\n",
                        __func__, __LINE__);
        pfnGetHal = ImagerHalTableSearch(SensorGUID, NvOdmImagerDevice_Sensor, NV_TRUE);
    }

    if (!pfnGetHal)
        goto fail;

    pImager = (NvOdmImager*)NvOdmOsAlloc(sizeof(NvOdmImager));
    if (!pImager){
        HAL_DB_PRINTF("%s %d: couldn't allocate memory for an imager\n", __func__, __LINE__);
        goto fail;
    }

    NvOdmOsMemset(pImager, 0, sizeof(NvOdmImager));

    // Get sensor HAL
    pImager->pSensor =
        (NvOdmImagerSensor*)NvOdmOsAlloc(sizeof(NvOdmImagerSensor));
    if (!pImager->pSensor) {
        HAL_DB_PRINTF("%s %d: Sensor ERR\n", __func__, __LINE__);
        goto fail;
    }

    NvOdmOsMemset(pImager->pSensor, 0, sizeof(NvOdmImagerSensor));

    Result = pfnGetHal(pImager);
    if (!Result) {
        HAL_DB_PRINTF("%s %d: Sensor ERR\n", __func__, __LINE__);
        goto fail;
    }

    pImager->pSensor->GUID = SensorGUID;

    // Open the sensor
    Result = pImager->pSensor->pfnOpen(pImager);
    if (!Result)
    {
        HAL_DB_PRINTF("%s %d: Sensor ERR\n", __func__, __LINE__);
        goto fail;
    }

    /* --- Focuser code --- */
    // Call local function to get sensor's capabilities
    GetDefaultCapabilities(pImager, &SensorCaps);
    // When auto-detection is enabled, use detected Focuser/Flash GUIDs instead
    if(UseSensorCapGUIDs)
    {
        FocuserGUID = SensorCaps.FocuserGUID;
        FlashGUID = SensorCaps.FlashGUID;
    }

    // No focuser?
    if (FocuserGUID == 0)
        goto checkFlash;

    // Find focuser's GetHal
    pfnGetHal = ImagerHalTableSearch(FocuserGUID, NvOdmImagerDevice_Focuser, NV_FALSE);
    HAL_DB_PRINTF("Using FocuserGUID '%s'\n", ImagerHalGetGuidStr(FocuserGUID, buf, sizeof(buf)));
    if (!pfnGetHal) {
        HAL_DB_PRINTF("%s %d: couldn't get the sensor HAL table for an focuser\n", __func__, __LINE__);
        goto fail;
    }

    // Get focuser's HAL
    pImager->pFocuser =
        (NvOdmImagerSubdevice*)NvOdmOsAlloc(sizeof(NvOdmImagerSubdevice));
    if (!pImager->pFocuser) {
        HAL_DB_PRINTF("%s %d: Focuser ERR\n", __func__, __LINE__);
        goto fail;
    }

    NvOdmOsMemset(pImager->pFocuser, 0, sizeof(NvOdmImagerSubdevice));

    Result = pfnGetHal(pImager);
    if (!Result) {
        HAL_DB_PRINTF("%s %d: Focuser ERR\n", __func__, __LINE__);
        goto fail;
    }

    if (FocuserGUID != FOCUSER_NVC_IMAGER)
        /* 7:0 of GUID is instance of NVC driver */
        pImager->pFocuser->GUID = FocuserGUID;
    else
        /* flag that imager imagerFd is to be used instead */
        pImager->pFocuser->GUID = 0;
    Result = pImager->pFocuser->pfnOpen(pImager);
    if (!Result) {
        HAL_DB_PRINTF("%s %d: Focuser ERR\n", __func__, __LINE__);
        goto fail;
    }

checkFlash:
    /* --- Flash code --- */
    // No flash?
    if (FlashGUID == 0)
        goto done;

    // Find flash's GetHal
    pfnGetHal = ImagerHalTableSearch(FlashGUID, NvOdmImagerDevice_Flash, NV_FALSE);
    HAL_DB_PRINTF("Using FlashGUID '%s'\n", ImagerHalGetGuidStr(FlashGUID, buf, sizeof(buf)));
    if (!pfnGetHal) {
        HAL_DB_PRINTF("%s %d: couldn't get HAL table for flash\n", __func__, __LINE__);
        goto fail;
    }

    // Get flash's HAL
    pImager->pFlash =
        (NvOdmImagerSubdevice*)NvOdmOsAlloc(sizeof(NvOdmImagerSubdevice));
    if (!pImager->pFlash) {
        HAL_DB_PRINTF("%s %d: Flash ERR\n", __func__, __LINE__);
        goto fail;
    }

    NvOdmOsMemset(pImager->pFlash, 0, sizeof(NvOdmImagerSubdevice));

    Result = pfnGetHal(pImager);
    if (!Result) {
        HAL_DB_PRINTF("%s %d: Flash ERR\n", __func__, __LINE__);
        goto fail;
    }

    pImager->pFlash->GUID = FlashGUID;
    Result = pImager->pFlash->pfnOpen(pImager);
    if (!Result)
    {
        NvOsDebugPrintf("%s %d: cannot open flash driver: error(%d)\n",
                __func__, __LINE__, Result);
        // Still returns TRUE even if kernel flash driver is not available.
        // Just treat this as if the sensor does not have flash capability,
        // i.e. set flash GUID to 0 in the odm flash driver.
        goto done;
    }

done:

    HAL_DB_PRINTF("%s: Using SensorGUID(%s)\n", __func__,
            (SensorGUID) ? ImagerHalGetGuidStr(SensorGUID, buf, sizeof(buf)):"none");
    HAL_DB_PRINTF("%s: Using FocuserGUID(%s)\n", __func__,
            (FocuserGUID) ? ImagerHalGetGuidStr(FocuserGUID, buf, sizeof(buf)):"none");
    HAL_DB_PRINTF("%s: Using FlashGUID(%s)\n", __func__,
            (FlashGUID) ? ImagerHalGetGuidStr(FlashGUID, buf, sizeof(buf)):"none");
    HAL_DB_PRINTF("%s ---\n", __func__);

    *phImager = pImager;
    return NV_TRUE;

fail:
    *phImager = NULL;
    HAL_DB_PRINTF("%s FAILED!\n", __func__);
    NvOdmImagerClose(pImager);
    return NV_FALSE;
}

NvBool
NvOdmImagerOpen(
    NvU64 ImagerGUID,
    NvOdmImagerHandle *phImager)
{
    NvBool result = NV_FALSE;
    NvBool UseSensorCapGUIDs = NV_TRUE;
    NvU64 SensorGUID = 0;
    NvU64 FocuserGUID = 0;
    NvU64 FlashGUID = 0;
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

    HAL_DB_PRINTF("%s +++ ImagerGUID(%s)\n", __func__, ImagerHalGetGuidStr(ImagerGUID, buf, sizeof(buf)));

    result = ImagerHalGuidGet(&ImagerGUID, &FocuserGUID, &FlashGUID, &UseSensorCapGUIDs);
    if (result == NV_FALSE)
        return NV_FALSE;

    SensorGUID = ImagerGUID;

    result = NvOdmImagerOpenExpanded(SensorGUID, FocuserGUID, FlashGUID, UseSensorCapGUIDs, phImager);

    HAL_DB_PRINTF("%s ---\n", __func__);

    return result;
}


void NvOdmImagerClose(NvOdmImagerHandle hImager)
{
    if (!hImager)
        return;

    // Close the focuser
    if (hImager->pFocuser)
        hImager->pFocuser->pfnClose(hImager);

    // Close the flash
    if (hImager->pFlash)
        hImager->pFlash->pfnClose(hImager);

    // Close the sensor
    if (hImager->pSensor)
        hImager->pSensor->pfnClose(hImager);

    // Cleanup
    NvOdmOsFree(hImager->pSensor);
    NvOdmOsFree(hImager->pFocuser);
    NvOdmOsFree(hImager->pFlash);
    NvOdmOsFree(hImager);
}

/*
   Get Default camera capabilities.
*/
static void
GetDefaultCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCap)
{
    HAL_DB_PRINTF("%s +++\n", __func__);
    // Get sensor capabilities
    if (hImager->pSensor)
    {
        hImager->pSensor->pfnGetCapabilities(hImager, pCap);
        if (pCap->CapabilitiesEnd != NVODM_IMAGER_CAPABILITIES_END)
        {
            HAL_DB_PRINTF(
                "Warning, this NvOdm adaptation %s is out of date, the ",
                pCap->identifier);
            HAL_DB_PRINTF(
                "last value in the caps should be 0x%x, but is 0x%x\n",
                NVODM_IMAGER_CAPABILITIES_END,
                pCap->CapabilitiesEnd);
        }
    }

    // Get focuser capabilities
    if (hImager->pFocuser)
    {
        hImager->pFocuser->pfnGetCapabilities(hImager, pCap);
    }

    // Get flash capabilities
    if (hImager->pFlash)
    {
        hImager->pFlash->pfnGetCapabilities(hImager, pCap);
    }
}

void
NvOdmImagerGetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    HAL_DB_PRINTF("%s +++\n", __func__);
    GetDefaultCapabilities(hImager, pCapabilities);

    // In the case focuser/flash are not present, set their GUID as NULL
    if (!hImager->pFocuser)
    {
        pCapabilities->FocuserGUID = NULL;
    }

    if (!hImager->pFlash)
    {
        pCapabilities->FlashGUID = NULL;
    }
}



void
NvOdmImagerListSensorModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumberOfModes)
{
    if (hImager && hImager->pSensor && hImager->pSensor->pfnListModes)
    {
        hImager->pSensor->pfnListModes(hImager, pModes, pNumberOfModes);
    }
    else
    {
        HAL_DB_PRINTF("%s NULL: %p, %p, %p\n", __func__,
            hImager,
            hImager ? hImager->pSensor : NULL,
            hImager ? (hImager->pSensor ? hImager->pSensor->pfnListModes : NULL) : NULL);
        *pNumberOfModes = 0;
    }
}

NvBool
NvOdmImagerSetSensorMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult)
{
    NvBool ret;
    if (!hImager->pSensor)
        return NV_FALSE;

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_ODM_SETMODE_ENTRY,
            NvOsGetTimeUS(),
            0);
#endif

    ret = hImager->pSensor->pfnSetMode(hImager, pParameters, pSelectedMode,
           pResult);

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_ODM_SETMODE_EXIT,
            NvOsGetTimeUS(),
            0);
#endif

    return ret;
}

NvBool
NvOdmImagerSetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerDeviceMask Devices,
    NvOdmImagerPowerLevel PowerLevel)
{
    NvBool Status = NV_TRUE;

    if (!hImager)
        return NV_FALSE;

    if (hImager->pSensor)
    {
        Status = hImager->pSensor->pfnSetPowerLevel(hImager, PowerLevel);
        if (!Status)
            return Status;
    }

    if (hImager->pFocuser)
    {
        Status = hImager->pFocuser->pfnSetPowerLevel(hImager, PowerLevel);
        if (!Status)
            return Status;
    }

    if (hImager->pFlash)
    {
        Status = hImager->pFlash->pfnSetPowerLevel(hImager, PowerLevel);
        if (!Status)
            return Status;
    }

    return Status;
}

static NV_INLINE NvOdmImagerSubDeviceType
GetSubDeviceForParameter(
    NvOdmImagerParameter Param)
{
    switch(Param)
    {
        case NvOdmImagerParameter_FocuserLocus:
        case NvOdmImagerParameter_FocalLength:
        case NvOdmImagerParameter_MaxAperture:
        case NvOdmImagerParameter_FNumber:
        case NvOdmImagerParameter_FocuserCapabilities:
        case NvOdmImagerParameter_FocuserStereo:
            return NvOdmImagerSubDeviceType_Focuser;

        case NvOdmImagerParameter_FlashTorchQuery:
        case NvOdmImagerParameter_FlashCapabilities:
        case NvOdmImagerParameter_FlashLevel:
        case NvOdmImagerParameter_TorchCapabilities:
        case NvOdmImagerParameter_TorchLevel:
        case NvOdmImagerParameter_FlashPinState:
            return NvOdmImagerSubDeviceType_Flash;

        default:
            return NvOdmImagerSubDeviceType_Sensor;
    }
}

NvBool
NvOdmImagerSetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    // Forward to appropriate sub-device
    switch(GetSubDeviceForParameter(Param))
    {
        case NvOdmImagerSubDeviceType_Sensor:
            if (!hImager->pSensor)
                return NV_FALSE;
            return hImager->pSensor->pfnSetParameter(
                hImager, Param, SizeOfValue, pValue);

        case NvOdmImagerSubDeviceType_Focuser:
            if (!hImager->pFocuser)
                return NV_FALSE;
            return hImager->pFocuser->pfnSetParameter(
                hImager, Param, SizeOfValue, pValue);

        case NvOdmImagerSubDeviceType_Flash:
            if (!hImager->pFlash)
                return NV_FALSE;
            return hImager->pFlash->pfnSetParameter(
                hImager, Param, SizeOfValue, pValue);

        default:
            NV_ASSERT(!"Impossible code path");
            return NV_FALSE;
    }
}

NvBool
NvOdmImagerGetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    NvOdmImagerSubDeviceType SubDevice = NvOdmImagerSubDeviceType_Sensor;

    // return the build date of this library
    if (Param == NvOdmImagerParameter_ImagerBuildDate)
    {
        char *str = (char *) pValue;

        if(pValue == NULL ||
           SizeOfValue == 0 ||
           SizeOfValue > MAX_BUILD_DATE_LENGTH)
            return NV_FALSE;

        NvOsStrncpy(str, g_BuildDate, SizeOfValue);
        str[SizeOfValue - 1] = '\0';
        return NV_TRUE;
    }

    // Find the proper sub-device for the parameters
    // If the sub-device doesn't exist, always choose the sensor
    switch(GetSubDeviceForParameter(Param))
    {
        case NvOdmImagerSubDeviceType_Focuser:
            if (hImager->pFocuser)
                SubDevice = NvOdmImagerSubDeviceType_Focuser;
            break;
        case NvOdmImagerSubDeviceType_Flash:
            if (hImager->pFlash)
                SubDevice = NvOdmImagerSubDeviceType_Flash;
            break;
        default:
            break;
    }

    // Forward to appropriate sub-device
    switch(SubDevice)
    {
        case NvOdmImagerSubDeviceType_Sensor:
            if (!hImager->pSensor)
                return NV_FALSE;
            return hImager->pSensor->pfnGetParameter(
                hImager, Param, SizeOfValue, pValue);

        case NvOdmImagerSubDeviceType_Focuser:
            if (!hImager->pFocuser)
                return NV_FALSE;
            return hImager->pFocuser->pfnGetParameter(
                hImager, Param, SizeOfValue, pValue);

        case NvOdmImagerSubDeviceType_Flash:
            if (!hImager->pFlash)
                return NV_FALSE;
            return hImager->pFlash->pfnGetParameter(
                hImager, Param, SizeOfValue, pValue);

        default:
            NV_ASSERT(!"Impossible code path");
            return NV_FALSE;
    }
}

void
NvOdmImagerGetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerDevice Device,
    NvOdmImagerPowerLevel *pPowerLevel)
{
    NV_ASSERT("!Not Implemented!");
}
NvBool
NvcImagerHalGet(
    NvU32 DevId,
    NvU32 DevIdMinor,
    NvcImagerHal *hNvcHal)
{
    NvU32 i;

    HAL_DB_PRINTF("%s %x %x\n", __func__, DevId, DevIdMinor);
    for (i = 0; i < NV_ARRAY_SIZE(g_NvcHalTable); i++)
    {
        if (DevId == g_NvcHalTable[i].DevId)
        {
            g_NvcHalTable[i].pfnGetHal(hNvcHal, DevIdMinor);
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}
