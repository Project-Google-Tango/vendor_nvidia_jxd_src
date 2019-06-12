/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


/**
 * standard includes
 */
#include "nvos.h"
#include "nvodm_services.h"
#include "nvcamera_isp.h"
/**
 *  PCL includes
 */
#include "nvpcl_api.h"

#if NV_ENABLE_DEBUG_PRINTS
#define PCL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define PCL_DB_PRINTF(...)
#endif


/**
 * This creates a new instance of Physical Camera Layer (PCL)
 * A new PCL API context is returned to the calling function for future
 * use of PCL calls to this client. Until a complete transition from NvOdmImager
 * to PCL calls occur, an ImagerGUID and hImager parameter must be used to
 * facilitate a mixture of calls from camera/core being PCL and NvOdmImager
 * @param ImagerGUID: The camera ID to use for front or back
 *      (See Android's CAMERA_FACING_BACK and CAMERA_FACING_FRONT)
 * @param phPcl: The pointer to the handle where the instance will be created
 * @param hImager: A handle to an NvOdmImager instance for PCL to work in
 *      conjunction with; if hImager is NULL, a separate instance will be used
 */
NvError
NvPclOpen(
    NvU64 ImagerGUID,
    NvPclContext_Handle *phPcl,
    NvOdmImagerHandle hImager)
{
    NvError e = NvSuccess;
    NvPclContext *pContext = NULL;

    PCL_DB_PRINTF("%s: ++++++++++++++++++++++\n", __func__);

    if(!phPcl)
        return NvError_BadParameter;

    pContext = NvOsAlloc(sizeof(NvPclContext));
    if(!pContext)
    {
        NvOsDebugPrintf("%s: Failure to allocate memory\n",__func__);
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pContext, 0, sizeof(NvPclContext));

    NV_CHECK_ERROR_CLEANUP(NvPclStateControllerOpen(ImagerGUID,
                            &pContext->hPclController, hImager));

    pContext->hFrameData = NULL;
    *phPcl = pContext;

    PCL_DB_PRINTF("%s: ----------------------\n", __func__);

    return NvSuccess;

fail:
    NvOsDebugPrintf("%s: PCL Open Failed. Error: 0x%x\n", __func__, e);
    NvPclClose(pContext);
    PCL_DB_PRINTF("%s: ----------------------\n", __func__);
    return e;
}

/**
 * NvPclGetPlatformData is provided to get a snapshot of the platform configuration
 * @param phPcl: The pointer to the handle where the instance will be created
 * @param phData: A handle to a NULL pointer that a platform reference will be assigned
 */
NvError
NvPclGetPlatformData(
    NvPclContext_Handle hPcl,
    NvPclPlatformData **phData)
{
    NvError e = NvSuccess;

    if(!hPcl || !phData)
        return NvError_BadParameter;

    e = NvPclStateControllerGetPlatformData(hPcl->hPclController, phData);

    return e;
}

/**
 * NvPclClose to destruct the PCL instance.
 * @param hPcl: The PCL API handle
 */
void NvPclClose(NvPclContext_Handle hPcl)
{
    PCL_DB_PRINTF("%s: ++++++++++++++++++++++\n", __func__);

    if(hPcl)
    {
        NvPclStateControllerClose(hPcl->hPclController);
        NvOsFree(hPcl);
    }

    PCL_DB_PRINTF("%s: ----------------------\n", __func__);

    return;
}

static NvError
NvPclCheckAndGrabDataItem(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemID,
    void **phBuf)
{
    NvError e = NvSuccess;
    NvBool exists = NV_FALSE;

    exists = NvCamFrameData_CheckDataItemPresence(hFrameData, ItemID);

    if(exists)
    {
        *phBuf = NvCamFrameData_GetDataItemValue(hFrameData, ItemID);
        if(*phBuf)
            NV_CHECK_ERROR(NvCamFrameData_DataItemRelease(hFrameData, ItemID));
    } else {
        return NvError_NotInitialized;
    }

    return NvSuccess;
}

/**
 * NvPclExtractFrameData translates framedata PCL targeted
 * settings to PCL settings format.
 * @param pSettings_Input is filled in to be used in PCL translated from the framedata settings.
 *      Current values of pSettings_Input only change when the appropriate
 *      members are found in hFrameData
 * @param hFrameData is the framedata is examined to extract new settings to be used
 */
static NvError
NvPclExtractFrameData(
    NvPclModuleSetting *pSettings_Input,
    NvCamFrameData_Handle hFrameData)
{
    NvError e = NvSuccess;
    NvPclFocuserSettings *pFocuserSettings = NULL;
    NvPclExposureList *pExposureData = NULL;
    NvPclFrameRate *pFrameRate = NULL;
    NvU32 *pFrameRateScheme = NULL;
    NvPclFlashSettings *pFlashData = NULL;

    if(!pSettings_Input)
        return NvError_BadParameter;

    e = NvPclCheckAndGrabDataItem(hFrameData,
            NvCamFrameDataItemID_PclFocuserSetting, (void**)&pFocuserSettings);
    if(e == NvSuccess)
        pSettings_Input->Focuser = *pFocuserSettings;

    e = NvPclCheckAndGrabDataItem(hFrameData,
            NvCamFrameDataItemID_PclExposure, (void**)&pExposureData);
    if(e == NvSuccess)
        pSettings_Input->Sensor.ExposureList = *pExposureData;

    e = NvPclCheckAndGrabDataItem(hFrameData,
            NvCamFrameDataItemID_PclFrameRate, (void**)&pFrameRate);
    if(e == NvSuccess)
        pSettings_Input->Sensor.FrameRate = *pFrameRate;

    e = NvPclCheckAndGrabDataItem(hFrameData,
            NvCamFrameDataItemID_PclFrameRateScheme, (void**)&pFrameRateScheme);
    if(e == NvSuccess)
        pSettings_Input->Sensor.FrameRateScheme = *pFrameRateScheme;

    e = NvPclCheckAndGrabDataItem(hFrameData,
            NvCamFrameDataItemID_PclFlashSetting, (void**)&pFlashData);
    if(e == NvSuccess)
        pSettings_Input->Flash = *pFlashData;

    pSettings_Input->Initialized = NV_TRUE;

    return NvSuccess;
}

/**
 * NvPclArchiveFrameData adds PCL info to FrameData contents
 * @param pRunningInfo is the PCL state info to be added to framedata
 * @param hFrameData is the handle of the FrameData that will receive the new PCL info
 */
static NvError
NvPclArchiveFrameData(
    NvPclRunningModuleInfo *pRunningInfo,
    NvCamFrameData_Handle hFrameData)
{
    NvError e = NvSuccess;
    NvBool dataItemExists = NV_FALSE;
    NvPclRunningModuleInfo *pArchiveInfo = NULL;

    if(!hFrameData || !pRunningInfo)
        return NvError_BadParameter;


    dataItemExists =
        NvCamFrameData_CheckDataItemPresence(hFrameData, NvCamFrameDataItemID_PclSettingsOutput);

    if(dataItemExists) {
        pArchiveInfo = NvCamFrameData_GetDataItemValue(hFrameData,
            NvCamFrameDataItemID_PclSettingsOutput);

        NvOsMemcpy(pArchiveInfo, pRunningInfo, sizeof(NvPclRunningModuleInfo));

        NvCamFrameData_DataItemRelease(hFrameData, NvCamFrameDataItemID_PclSettingsOutput);
    } else {

        pArchiveInfo = NvOsAlloc(sizeof(NvPclRunningModuleInfo));
        if(!pArchiveInfo) {
            NvOsDebugPrintf("%s: Failure to allocate memory\n",__func__);
            return NvError_InsufficientMemory;
        }

        NvOsMemcpy(pArchiveInfo, pRunningInfo, sizeof(NvPclRunningModuleInfo));

        e = NvCamFrameData_SetDataItem(hFrameData,
                        NvCamFrameDataItemID_PclSettingsOutput, pArchiveInfo);
    }

    return NvSuccess;
}

/**
 * NvPclEstimateSettings is provided as a mechanism to test PCL output effects.
 * It will examine the FrameData for new settings, and provide the output settings
 * PCL would actually resolve.  No settings are applied to hardware in this function.
 * To apply settings to hardware, see NvPclUpdate and NvPclApply functions.
 * @param hPcl: The PCL API handle
 * @param hFrameData is the framedata is examined to extract test settings to be used
 */
NvError
NvPclEstimateSettings(
    NvPclContext_Handle hPcl,
    NvCamFrameData_Handle hFrameData)
{
    NvError e = NvSuccess;
    NvPclModuleSetting Settings_Output;
    NvPclRunningModuleInfo runningInfo;
    NvU32 runningInfoSize = 0;

    PCL_INIT_VERSION_PSETTING(&Settings_Output);

    if(!hPcl || !hFrameData)
        return NvError_BadParameter;

    NvOsMemset(&runningInfo, 0, sizeof(NvPclRunningModuleInfo));
    runningInfoSize = sizeof(NvPclRunningModuleInfo);

    NV_CHECK_ERROR(NvPclStateControllerRunningInfo(hPcl->hPclController,
                                    &runningInfo, &runningInfoSize));

    NV_CHECK_ERROR(NvPclExtractFrameData(&runningInfo.Settings, hFrameData));

    NV_CHECK_ERROR(NvPclStateControllerUpdate(hPcl->hPclController,
                    &runningInfo.Settings, &Settings_Output, NV_FALSE));

    runningInfo.Settings = Settings_Output;
    NV_CHECK_ERROR(NvPclArchiveFrameData(&runningInfo, hFrameData));

    return NvSuccess;
}

/**
 * NvPclUpdate is used to load the latest framedata settings.A handle to the
 * FrameData is passed on. Apply function has to be called to apply the
 * loaded settings.
 * @param hPcl: The PCL API handle
 * @param hFrameData: Handle to the FrameData containing PCL data item settings to be loaded.
 */
NvError
NvPclUpdate(
    NvPclContext_Handle hPcl,
    NvCamFrameData_Handle hFrameData)
{
    NvPclModuleSetting Settings_Input;
    NvPclModuleSetting Settings_Output;
    NvPclRunningModuleInfo runningInfo;
    NvU32 runningInfoSize = 0;
    NvError e = NvSuccess;

    NV_DEBUG_PRINTF(("%s: +++++++++++\n", __func__));

    if(!hPcl || !hFrameData)
        return NvError_BadParameter;

    hPcl->hFrameData = hFrameData;

    PCL_INIT_VERSION_PSETTING(&Settings_Input);
    PCL_INIT_VERSION_PSETTING(&Settings_Output);

    NvOsMemset(&runningInfo, 0, sizeof(NvPclRunningModuleInfo));
    runningInfoSize = sizeof(NvPclRunningModuleInfo);
    NV_CHECK_ERROR(NvPclStateControllerRunningInfo(hPcl->hPclController,
                                    &runningInfo, &runningInfoSize));

    // In case of a bubble, use last known given settings
    // Otherwise, build on top of running configuration settings
    if(hPcl->PrevInputSettings.Initialized == NV_TRUE)
        Settings_Input = hPcl->PrevInputSettings;
    else
        Settings_Input = runningInfo.Settings;

    NV_CHECK_ERROR(NvPclExtractFrameData(&Settings_Input, hFrameData));

    NV_DEBUG_PRINTF(("%s: Sending Updated Settings through PCL\n", __func__));
    NV_CHECK_ERROR(NvPclStateControllerUpdate(hPcl->hPclController,
                                    &Settings_Input, &Settings_Output, NV_TRUE));

    hPcl->PrevInputSettings = Settings_Input;

    NV_DEBUG_PRINTF(("%s: -----------\n", __func__));

    return NvSuccess;
}

/**
 * NvPclApply triggers the first enqueued settings to pass through the PCL layer.
 * Currently does both both the "Update" and "Apply" concept. This may change
 * when further frame synching details are realized in camera/core.
 * @param hPcl: The PCL API handle
 */
NvError NvPclApply(NvPclContext_Handle hPcl)
{
    NvError e = NvSuccess;
    NvPclRunningModuleInfo runningInfo;
    NvU32 runningInfoSize = 0;

    NV_DEBUG_PRINTF(("%s: +++++++++++\n", __func__));

    if(!hPcl)
        return NvError_BadParameter;

    NV_DEBUG_PRINTF(("%s: Applying last settings through PCL\n", __func__));
    NV_CHECK_ERROR(NvPclStateControllerApply(hPcl->hPclController));

    NvOsMemset(&runningInfo, 0, sizeof(NvPclRunningModuleInfo));
    runningInfoSize = sizeof(NvPclRunningModuleInfo);

    NV_DEBUG_PRINTF(("%s: Reading PCL settings\n", __func__));
    NV_CHECK_ERROR(NvPclStateControllerRunningInfo(hPcl->hPclController,
                                    &runningInfo, &runningInfoSize));

    NV_CHECK_ERROR(NvPclArchiveFrameData(&runningInfo, hPcl->hFrameData));

    NV_DEBUG_PRINTF(("%s: -----------\n", __func__));

    return NvSuccess;
}

