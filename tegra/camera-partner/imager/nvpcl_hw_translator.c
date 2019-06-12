/**
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

/**
 * standard includes
 */
#include "nvos.h"
#include "nvodm_services.h"

/**
 *  NvPcl includes
 */
#include "nvpcl_hw_translator.h"
#include "imager_det.h"


/**
 * NvPclHwWrite is a generic interface for kernel writing interactions.
 * @param controlType holds protocol or policy info for pData.
 * @param pBlobs pointer to list of hw blobs - we may send a list, or container, of blobs to represent each sensor, focuser, and flash in one command
 * @param numBlobs number of NvPclHwBlob passed in pBlobs
 * @returns error if kernel fail or couldn't construct a message to kernel (unsupported or invalid); otherwise success?
 */
NvError NvPclHwWrite(NvPclPackageType controlType, NvPclHwBlob *pBlobs, NvU32 numBlobs) {

    return NvError_Success;
}


/**
 * NvPclKernelRead is a generic interface for kernel reading interactions.
 * @param controlType holds protocol or policy info for pData.
 * @param pBlobs pointer to list of hw blobs to be read into
 * @param numData expected number of NvPcl_ControlReg passed back to pData
 * @returns error if kernel fail or couldn't construct a message to kernel (unsupported or invalid); otherwise success?
 */
NvError NvPclHwRead(NvPclPackageType controlType, NvPclHwBlob *pBlobs, NvU32 *pNumBlobs) {

    return NvError_Success;
}

/**
 * Temporary definition until HwTranslator block supports the platform module query
 */
#define NVPCL_NVC_SENSOR_NAME ("nvodm_nvc_sensor")
#define NVPCL_NVC_FOCUSER_NAME ("nvodm_nvc_focuser")
#define NVPCL_NVC_FLASH_NAME ("nvodm_nvc_torch")
NvPclHwCameraModule NvPclHwCameraModuleTempSample0 =
    {
        .Name = "GenericModuleSample0",
        .HwCamSubModule = {
                            {NVPCL_NVC_SENSOR_NAME, NULL},
                            {NVPCL_NVC_FOCUSER_NAME, NULL},
                            {PCL_STR_LIST_END, NULL},
                         },
        .FactoryCalibration = {NULL},
        .CalibrationOverride = {NULL},
        .CalibrationData = NULL,
    };

NvPclHwCameraModule NvPclHwCameraModuleTempSample1 =
    {
        .Name = "GenericModuleSample1",
        .HwCamSubModule = {
                            {NVPCL_NVC_SENSOR_NAME, NULL},
                            {PCL_STR_LIST_END, NULL},
                         },
        .FactoryCalibration = {NULL},
        .CalibrationOverride = {NULL},
        .CalibrationData = NULL,
    };

NvPclHwCameraModule NvPclHwCameraModuleTempVirtual0 =
    {
        .Name = "VirtualModuleSample0",
        .HwCamSubModule = {
                            {PCL_STR_LIST_END, NULL},
                         },
        .FactoryCalibration = {NULL},
        .CalibrationOverride = {NULL},
        .CalibrationData = NULL,
    };

/**
 * NvPclKernelGetProfile provides a list of devices found in the kernel.  It should identify and theoretically enable NvPclController to decide which NvPclDriver should be used (ie. AR0833, IMX135, etc)
 * @param ModuleList if not NULL, memory to be filled with module-blobs; should have NumModules*size allocated.  If a module member device is not used on camera, use blob device type NvPcl_DeviceType_Empty.
 * @param pNumModules if not NULL, number of camera modules
 * @param isVirtual boolean to state intent of a virtual device
 * @returns error if kernel fail or couldn't construct a message to kernel (unsupported or invalid); otherwise a device structure.  (We may want to try to utilize DeviceTree in future)
 */
NvError NvPclHwGetModuleList(NvPclHwCameraModule *pModules, NvU32 *pNumModules, NvBool isVirtual) {
    NvBool Result = NV_FALSE;
    struct cam_device_layout *pLayout, *pDev;
    struct cam_device_layout *pFocuser = NULL;
    struct cam_device_layout *pSensor = NULL;
    struct cam_device_layout *pFlash = NULL;
    NvU32 devCount = 0;
    NvU8 subCount = 0;
    NvU8 modCount = 0;
    NvU8 modIndex = 0;
    NvU8 pDevCnt = 0;
    NvU32 i = 0;

    /**
     * Temporary solution for resolving rear/front virtuals
     */
    if(isVirtual == NV_TRUE)
    {
        *pNumModules = 4;
        if(pModules != NULL) {
            NvOsMemcpy(&pModules[0], &NvPclHwCameraModuleTempVirtual0, sizeof(NvPclHwCameraModule));
            NvOsMemcpy(&pModules[1], &NvPclHwCameraModuleTempVirtual0, sizeof(NvPclHwCameraModule));
            NvOsMemcpy(&pModules[2], &NvPclHwCameraModuleTempVirtual0, sizeof(NvPclHwCameraModule));
            NvOsMemcpy(&pModules[3], &NvPclHwCameraModuleTempVirtual0, sizeof(NvPclHwCameraModule));
        }
        return NvSuccess;
    }

    Result = NvOdmImagerGetLayout(&pLayout, &devCount);

    if(Result == NV_TRUE)
    {
        if(devCount == 0)
        {
            *pNumModules = 0;
            return NvSuccess;
        }
        /**
         * NvOdmImagerGetLayout does not return a sorted array
         * Doing simple iteration of devices to check max pos value
         */
        pDev = pLayout;
        for(i = 0; i < devCount; i++)
        {
            if(modCount <= (pDev->pos))
            {
                modCount = pDev->pos;
            }
            pDev++;
        }
        /**
         * Incrementing one more time as front imager is conventionally 1,
         * but means there are two imagers.
         */
        modCount++;

        if (pModules != NULL)
        {
            modIndex = 0;
            while (modIndex < modCount)
            {
                // Forcing generic name for now
                NvOsStrncpy((char *)pModules[modIndex].Name,
                    NVPCL_GENERICMODULENAME,
                    sizeof(NVPCL_GENERICMODULENAME));

                subCount = 0;
                pFocuser = NULL;
                pFlash = NULL;
                pSensor = NULL;
                pDev = pLayout;
                while (pDev->guid)
                {
                    if (pDev->pos == modIndex)
                    {
                        switch (pDev->type)
                        {
                            case DEVICE_SENSOR:
                                pSensor = (void *)pDev;
                                break;
                            case DEVICE_FOCUSER:
                                pFocuser = (void *)pDev;
                                break;
                            case DEVICE_FLASH:
                                pFlash = (void *)pDev;
                                break;
                            default:
                                NvOsDebugPrintf("%s: Unknown driver match type %llu\n", __func__, pDev->type);
                                break;
                        }
                    }
                    pDev++;
                    pDevCnt++;
                }

                if (pFocuser)
                {
                    NvOsStrncpy((char *)pModules[modIndex].HwCamSubModule[subCount].Name,
                                                    NVPCL_NVC_FOCUSER_NAME, sizeof(NVPCL_NVC_FOCUSER_NAME));
                    pModules[modIndex].HwCamSubModule[subCount++].hDev = (void *)pFocuser;
                }

                if (pSensor)
                {
                    NvOsStrncpy((char *)pModules[modIndex].HwCamSubModule[subCount].Name,
                                                    NVPCL_NVC_SENSOR_NAME, sizeof(NVPCL_NVC_SENSOR_NAME));
                    pModules[modIndex].HwCamSubModule[subCount++].hDev = (void *)pSensor;
                }

                if (pFlash)
                {
                    NvOsStrncpy((char *)pModules[modIndex].HwCamSubModule[subCount].Name,
                                                    NVPCL_NVC_FLASH_NAME, sizeof(NVPCL_NVC_FLASH_NAME));
                    pModules[modIndex].HwCamSubModule[subCount++].hDev = (void *)pFlash;
                }

                NvOsStrncpy((char *)pModules[modIndex].HwCamSubModule[subCount].Name,
                        PCL_STR_LIST_END, sizeof(PCL_STR_LIST_END));
                modIndex++;
            }
        }

        *pNumModules = modCount;
        NvOsFree(pLayout);
    }
    else
    {
        // Temporarily hardcoding rear module instance
        *pNumModules = 2;
        if(pModules != NULL) {
            NvOsMemcpy(&pModules[0], &NvPclHwCameraModuleTempSample0, sizeof(NvPclHwCameraModule));
            NvOsMemcpy(&pModules[1], &NvPclHwCameraModuleTempSample1, sizeof(NvPclHwCameraModule));
        }
        return NvError_NotSupported;
    }

    return NvSuccess;
}


NvError NvPclHwInitializeModule(NvPclHwCameraModule *pModule) {

    return NvError_Success;
}

void NvPclHwPrintCameraSubModule(NvU32 index, NvPclHwCameraSubModule dev) {
    NvOsDebugPrintf("%s -- HwCamSubModule[%d].HwDev.Name: %s\n",
                        __func__, index, dev.Name);
    return;
}

void NvPclHwPrintModuleDefinition(NvPclHwCameraModule hwModule) {
    NvU8 i = 0;

    NvOsDebugPrintf("%s -- Name: %s\n", __func__, hwModule.Name);
    for(i = 0; (NvOsStrcmp(hwModule.HwCamSubModule[i].Name, PCL_STR_LIST_END) != 0); i++) {
        NvPclHwPrintCameraSubModule(i, hwModule.HwCamSubModule[i]);
    }

    return;
}

void NvPclPrintToStrEnd(const char * typeStr, const char *strArray[]) {
    NvU8 i = 0;


    for(i = 0; strArray[i] != PCL_STR_LIST_END; i++) {
        NvOsDebugPrintf("%s -- %s[%d]:%s", __func__, typeStr, i, strArray[i]);
    }

    return;
}

