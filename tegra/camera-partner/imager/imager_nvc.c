/**
 * Copyright (c) 2008-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#if (BUILD_FOR_AOS == 0)

#define NV_ENABLE_DEBUG_PRINTS 0

#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <stdint.h>
#include <nvc.h>
#include <nvc_image.h>

#include "imager_nvc.h"
#include "imager_hal.h"
#include "imager_util.h"

// the standard assert doesn't help us
#undef NV_ASSERT
#define NV_ASSERT(x) \
    do{if(!(x)){NvOsDebugPrintf("ASSERT at %s:%d\n", __FILE__,__LINE__);}}while(0)

//#define DEBUG_PRINTS

static const char *pCharDataNull = {
    ""
};


/**
 * Static functions
 * Some compilers require these to be declared upfront.
 */
static NvBool
NvcImager_Open(
    NvOdmImagerHandle hImager);

static void
NvcImager_Close(
    NvOdmImagerHandle hImager);

static void
NvcImager_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities);

static void
NvcImager_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumModes);

static NvBool
NvcImager_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult);

static NvBool
NvcImager_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel);

static NvBool
NvcImager_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue);

static NvBool
NvcImager_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue);

static void
NvcImager_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel);

static NvBool
NvcImager_GroupHold(
    NvcImagerContext *pCtxt,
    const NvOdmImagerSensorAE *sensorAE);

static NvBool
NvcImager_DefaultHalGain2Float(
    NvU32 x,
    NvF32 *NewGain,
    NvF32 MinGain,
    NvF32 MaxGain)
{
    NvOsDebugPrintf("%s: ERR: Missing Hal Gain2Float function!\n", __func__);
    return NV_FALSE;
}

static NvBool
NvcImager_DefaultHalFloat2Gain(
    NvF32 x,
    NvU32 *NewGain,
    NvF32 MinGain,
    NvF32 MaxGain)
{
    NvOsDebugPrintf("%s: ERR: Missing HAL Float2Gain function!\n", __func__);
    return NV_FALSE;
}

static NvBool
NvcImager_StaticNvcRead(
    NvcImagerContext *pCtxt)
{
    struct nvc_imager_cap *pCapsKernel;
    NvOdmImagerCapabilities *pCapsUser;
    struct nvc_imager_static_nvc *pSNvcKernel;
    NvcImagerStaticNvc *pSNvcUser;
    struct nvc_imager_mode_list mode_list;
    struct nvc_imager_mode *pModesKernel;
    NvU32 i;
    int err;
    NvBool Status;

    NV_DEBUG_PRINTF(("%s +++++\n", __func__));
    /* get capabilities */
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_CAPS_RD, &pCtxt->Cap);
    if (err)
    {
        NvOsDebugPrintf("%s: ioctl NVC_IOCTL_CAPS failed %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    /* convert floats */
    pCapsKernel = (struct nvc_imager_cap *)&pCtxt->Cap;
    pCapsUser = (NvOdmImagerCapabilities *)&pCtxt->Cap;
    for (i = 0; i < NVODMIMAGER_CLOCK_PROFILE_MAX; i++)
    {
        pCapsUser->ClockProfiles[i].ClockMultiplier =
                        (NvF32)pCapsKernel->clock_profiles[i].clock_multiplier;
        pCapsUser->ClockProfiles[i].ClockMultiplier /= 1000000;
    }

#ifdef DEBUG_PRINTS
    NvOsDebugPrintf("%s: identifier=%s\n",
                    __func__, pCtxt->Cap.identifier);
    NvOsDebugPrintf("%s: SensorOdmInterface=%d\n",
                    __func__, pCtxt->Cap.SensorOdmInterface);
    for (i=0; i < NVODMIMAGER_FORMAT_MAX; i++)
        NvOsDebugPrintf("%s: PixelTypes[%d]=%d\n",
                        __func__, i, pCtxt->Cap.PixelTypes[i]);
    NvOsDebugPrintf("%s: Orientation=%d\n",
                    __func__, pCtxt->Cap.Orientation);
    NvOsDebugPrintf("%s: Direction=%d\n",
                    __func__, pCtxt->Cap.Direction);
    NvOsDebugPrintf("%s: InitialSensorClockRateKHz=%u\n",
                    __func__, pCtxt->Cap.InitialSensorClockRateKHz);
    for (i=0; i < NVODMIMAGER_CLOCK_PROFILE_MAX; i++)
    {
        NvOsDebugPrintf("%s: ExternalClockKHz[%d]=%u\n",
                        __func__, i, pCtxt->Cap.ClockProfiles[i].ExternalClockKHz);
        NvOsDebugPrintf("%s: ClockMultiplier[%d]=%f\n",
                        __func__, i, pCtxt->Cap.ClockProfiles[i].ClockMultiplier);
    }
    NvOsDebugPrintf("%s: HSyncEdge=%d\n",
                    __func__, pCtxt->Cap.ParallelTiming.HSyncEdge);
    NvOsDebugPrintf("%s: VSyncEdge=%d\n",
                    __func__, pCtxt->Cap.ParallelTiming.VSyncEdge);
    NvOsDebugPrintf("%s: MClkOnVGP0=%x\n",
                    __func__, pCtxt->Cap.ParallelTiming.MClkOnVGP0);
    NvOsDebugPrintf("%s: CsiPort=%u\n",
                    __func__, (unsigned)pCtxt->Cap.MipiTiming.CsiPort);
    NvOsDebugPrintf("%s: DataLanes=%u\n",
                    __func__, (unsigned)pCtxt->Cap.MipiTiming.DataLanes);
    NvOsDebugPrintf("%s: VirtualChannelID=%u\n",
                    __func__, (unsigned)pCtxt->Cap.MipiTiming.VirtualChannelID);
    NvOsDebugPrintf("%s: DiscontinuousClockMode=%x\n",
                    __func__, pCtxt->Cap.MipiTiming.DiscontinuousClockMode);
    NvOsDebugPrintf("%s: CILThresholdSettle=%u\n",
                    __func__, (unsigned)pCtxt->Cap.MipiTiming.CILThresholdSettle);
    NvOsDebugPrintf("%s: width=%x\n",
                    __func__, pCtxt->Cap.MinimumBlankTime.width);
    NvOsDebugPrintf("%s: height=%x\n",
                    __func__, pCtxt->Cap.MinimumBlankTime.height);
    NvOsDebugPrintf("%s: PreferredModeIndex=%x\n",
                    __func__, pCtxt->Cap.PreferredModeIndex);
    NvOsDebugPrintf("%s: FocuserGUID=%016llx\n",
                    __func__, pCtxt->Cap.FocuserGUID);
    NvOsDebugPrintf("%s: FlashGUID=%016llx\n",
                    __func__, pCtxt->Cap.FlashGUID);
    NvOsDebugPrintf("%s: FlashControlEnabled=%x\n",
                    __func__, pCtxt->Cap.FlashControlEnabled);
    NvOsDebugPrintf("%s: AdjustableFlashTiming=%x\n",
                    __func__, pCtxt->Cap.AdjustableFlashTiming);
    NvOsDebugPrintf("%s: CapabilitiesEnd=%x\n",
                    __func__, pCtxt->Cap.CapabilitiesEnd);
#endif

    /* get static parameters */
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_STATIC_RD, &pCtxt->SNvc);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ERR: ioctl NVC_IOCTL_STATIC_RD failed %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    /* convert floats */
    pSNvcKernel = (struct nvc_imager_static_nvc *)&pCtxt->SNvc;
    pSNvcUser = (NvcImagerStaticNvc *)&pCtxt->SNvc;
    pSNvcUser->FocalLength = (NvF32)pSNvcKernel->focal_len;
    pSNvcUser->FocalLength /= NVC_IMAGER_INT2FLOAT_DIVISOR;
    pSNvcUser->MaxAperture = (NvF32)pSNvcKernel->max_aperture;
    pSNvcUser->MaxAperture /= NVC_IMAGER_INT2FLOAT_DIVISOR;
    pSNvcUser->FNumber = (NvF32)pSNvcKernel->fnumber;
    pSNvcUser->FNumber /= NVC_IMAGER_INT2FLOAT_DIVISOR;
    pSNvcUser->HorizontalViewAngle = (NvF32)pSNvcKernel->view_angle_h;
    pSNvcUser->HorizontalViewAngle /= NVC_IMAGER_INT2FLOAT_DIVISOR;
    pSNvcUser->VerticalViewAngle = (NvF32)pSNvcKernel->view_angle_v;
    pSNvcUser->VerticalViewAngle /= NVC_IMAGER_INT2FLOAT_DIVISOR;

#ifdef DEBUG_PRINTS
    NvOsDebugPrintf("%s: ApiVersion=%u\n", __func__, pCtxt->SNvc.ApiVersion);
    NvOsDebugPrintf("%s: SensorType=%x\n", __func__, pCtxt->SNvc.SensorType);
    NvOsDebugPrintf("%s: BitsPerPixel=%d\n", __func__, pCtxt->SNvc.BitsPerPixel);
    NvOsDebugPrintf("%s: SensorId=%x\n", __func__, pCtxt->SNvc.SensorId);
    NvOsDebugPrintf("%s: SensorIdMinor=%x\n", __func__,
                    pCtxt->SNvc.SensorIdMinor);
    NvOsDebugPrintf("%s: FocalLength=%f\n", __func__, pCtxt->SNvc.FocalLength);
    NvOsDebugPrintf("%s: MaxAperture=%f\n", __func__, pCtxt->SNvc.MaxAperture);
    NvOsDebugPrintf("%s: FNumber=%f\n", __func__, pCtxt->SNvc.FNumber);
    NvOsDebugPrintf("%s: HorizontalViewAngle=%f\n", __func__,
                    pCtxt->SNvc.HorizontalViewAngle);
    NvOsDebugPrintf("%s: VerticalViewAngle=%f\n", __func__,
                    pCtxt->SNvc.VerticalViewAngle);
    NvOsDebugPrintf("%s: StereoCapable=%d\n", __func__,
                    pCtxt->SNvc.StereoCapable);
    NvOsDebugPrintf("%s: SensorResChangeWaitTime=%u\n", __func__,
                    pCtxt->SNvc.SensorResChangeWaitTime);
#endif
    Status = NvcImagerHalGet(pCtxt->SNvc.SensorId,
                             pCtxt->SNvc.SensorIdMinor,
                             &pCtxt->Hal);
    if (!Status)
        NV_DEBUG_PRINTF(("%s: FYI: Missing device 0x%x HAL\n",
                         __func__, pCtxt->SNvc.SensorId));

    if (pCtxt->Hal.pCalibration == NULL)
    {
        pCtxt->Hal.pCalibration = pCharDataNull;
        NV_DEBUG_PRINTF(("%s: FYI: Missing Calibration data\n", __func__));
    }
    if (pCtxt->Hal.pfnFloat2Gain == NULL)
        pCtxt->Hal.pfnFloat2Gain = NvcImager_DefaultHalFloat2Gain;
    if (pCtxt->Hal.pfnGain2Float == NULL)
        pCtxt->Hal.pfnGain2Float = NvcImager_DefaultHalGain2Float;

    /* first get the size for memory allocation (p_modes = NULL) */
    pCtxt->NumModes = 0;
    mode_list.p_modes = NULL;
    mode_list.p_num_mode = &pCtxt->NumModes;
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_MODE_RD, &mode_list);
    if ((err < 0) || !pCtxt->NumModes)
    {
        NvOsDebugPrintf("%s: ERR: ioctl NVC_IOCTL_MODE_RD failed %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }
    NV_DEBUG_PRINTF(("%s: NumModes=%u\n", __func__, pCtxt->NumModes));
    /* allocate memory and populate the mode list */
    pCtxt->pModeList = NvOsAlloc(pCtxt->NumModes *
                                    sizeof(*pCtxt->pModeList));
    if (!pCtxt->pModeList)
    {
        NvOsDebugPrintf("%s: ERR: unable to allocate memory for mode list!\n",
                        __func__);
        return NV_FALSE;
    }

    mode_list.p_modes = (struct nvc_imager_mode *)pCtxt->pModeList;
    mode_list.p_num_mode = NULL;
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_MODE_RD, &mode_list);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ERR: ioctl NVC_IOCTL_MODE_RD failed %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    /* convert floating points and account for stereo */
    pModesKernel = (struct nvc_imager_mode *)pCtxt->pModeList;
    for (i = 0; i < pCtxt->NumModes; i++)
    {
        pCtxt->pModeList[i].PeakFrameRate =
                                    (NvF32)pModesKernel[i].peak_frame_rate;
        pCtxt->pModeList[i].PeakFrameRate /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pCtxt->pModeList[i].PixelAspectRatio =
                                 (NvF32)pModesKernel[i].pixel_aspect_ratio;
        pCtxt->pModeList[i].PixelAspectRatio /=
                                              NVC_IMAGER_INT2FLOAT_DIVISOR;
        pCtxt->pModeList[i].PLL_Multiplier =
                                     (NvF32)pModesKernel[i].pll_multiplier;
        pCtxt->pModeList[i].PLL_Multiplier /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        NV_DEBUG_PRINTF(("%s: %u=%dx%d\n", __func__, i,
                         pCtxt->pModeList[i].ActiveDimensions.width,
                         pCtxt->pModeList[i].ActiveDimensions.height));
    }

#ifdef DEBUG_PRINTS
    for (i = 0; i < pCtxt->NumModes; i++)
    {
        NvOsDebugPrintf("%s: i=%u width=%d\n", __func__, i,
                        pCtxt->pModeList[i].ActiveDimensions.width);
        NvOsDebugPrintf("%s: i=%u height=%d\n", __func__, i,
                        pCtxt->pModeList[i].ActiveDimensions.height);
        NvOsDebugPrintf("%s: i=%u x=%d\n", __func__, i,
                        pCtxt->pModeList[i].ActiveStart.x);
        NvOsDebugPrintf("%s: i=%u y=%d\n", __func__, i,
                        pCtxt->pModeList[i].ActiveStart.y);
        NvOsDebugPrintf("%s: i=%u PeakFrameRate=%f\n", __func__, i,
                        pCtxt->pModeList[i].PeakFrameRate);
        NvOsDebugPrintf("%s: i=%u PixelAspectRatio=%f\n", __func__, i,
                        pCtxt->pModeList[i].PixelAspectRatio);
        NvOsDebugPrintf("%s: i=%u PLL_Multiplier=%f\n", __func__, i,
                        pCtxt->pModeList[i].PLL_Multiplier);
        NvOsDebugPrintf("%s: i=%u CropMode=%d\n", __func__, i,
                        pCtxt->pModeList[i].CropMode);
    }
#endif

    NV_DEBUG_PRINTF(("%s -----\n", __func__));
    return NV_TRUE;
}

static NvBool
NvcImager_ModeNvcRead(
    NvcImagerContext *pCtxt,
    NvS32 ResX,
    NvS32 ResY,
    NvOdmImagerSensorMode *pModeUser,
    NvcImagerDynamicNvc *pDNvcUser)
{
    struct nvc_imager_dnvc dnvc;
    struct nvc_imager_mode *pModeKernel;
    struct nvc_imager_dynamic_nvc *pDNvcKernel;
    int err;

    dnvc.res_x = ResX;
    dnvc.res_y = ResY;
    if (pModeUser)
        dnvc.p_mode = (struct nvc_imager_mode *)pModeUser;
    else
        dnvc.p_mode = NULL;
    if (pDNvcUser)
        dnvc.p_dnvc = (struct nvc_imager_dynamic_nvc *)pDNvcUser;
    else
        dnvc.p_dnvc = NULL;
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_DYNAMIC_RD, &dnvc);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ERR: ioctl NVC_IOCTL_DYNAMIC_RD failed\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }
    /* convert mode floats */
    if (pModeUser)
    {
        pModeKernel = (struct nvc_imager_mode *)pModeUser;
        pModeUser->PeakFrameRate = (NvF32)pModeKernel->peak_frame_rate;
        pModeUser->PeakFrameRate /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pModeUser->PixelAspectRatio = (NvF32)pModeKernel->pixel_aspect_ratio;
        pModeUser->PixelAspectRatio /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pModeUser->PLL_Multiplier = (NvF32)pModeKernel->pll_multiplier;
        pModeUser->PLL_Multiplier /= NVC_IMAGER_INT2FLOAT_DIVISOR;
    }
    /* convert nvc floats */
    if (pDNvcUser)
    {
        pDNvcKernel = (struct nvc_imager_dynamic_nvc *)pDNvcUser;
        pDNvcUser->DiffIntegrationTimeOfMode =
                                     (NvU32)pDNvcKernel->diff_integration_time;
        pDNvcUser->DiffIntegrationTimeOfMode /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pDNvcUser->MinGain = (NvF32)pDNvcKernel->min_gain;
        pDNvcUser->MinGain /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pDNvcUser->MaxGain = (NvF32)pDNvcKernel->max_gain;
        pDNvcUser->MaxGain /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pDNvcUser->ModeSwWaitFrames = (NvF32)pDNvcKernel->mode_sw_wait_frames;
        pDNvcUser->ModeSwWaitFrames /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pDNvcUser->InherentGain = (NvF32)pDNvcKernel->inherent_gain;
        pDNvcUser->InherentGain /= NVC_IMAGER_INT2FLOAT_DIVISOR;
        pDNvcUser->InherentGainBinEn =
                                      (NvF32)pDNvcKernel->inherent_gain_bin_en;
        pDNvcUser->InherentGainBinEn /= NVC_IMAGER_INT2FLOAT_DIVISOR;
    }

#ifdef DEBUG_PRINTS
    if (pModeUser != NULL) {
        NvOsDebugPrintf("%s: width=%d\n", __func__,
                        pModeUser->ActiveDimensions.width);
        NvOsDebugPrintf("%s: height=%d\n", __func__,
                        pModeUser->ActiveDimensions.height);
        NvOsDebugPrintf("%s: x=%d\n", __func__,
                        pModeUser->ActiveStart.x);
        NvOsDebugPrintf("%s: y=%d\n", __func__,
                        pModeUser->ActiveStart.y);
        NvOsDebugPrintf("%s: PeakFrameRate=%f\n", __func__,
                        pModeUser->PeakFrameRate);
        NvOsDebugPrintf("%s: PixelAspectRatio=%f\n", __func__,
                        pModeUser->PixelAspectRatio);
        NvOsDebugPrintf("%s: PLL_Multiplier=%f\n", __func__,
                        pModeUser->PLL_Multiplier);
        NvOsDebugPrintf("%s: CropMode=%d\n", __func__,
                        pModeUser->CropMode);
    }
    if (pDNvcUser != NULL) {
        NvOsDebugPrintf("%s: ApiVersion=%u\n", __func__,
                        pDNvcUser->ApiVersion);
        NvOsDebugPrintf("%s: BracketCaps=%x\n", __func__,
                        pDNvcUser->BracketCaps);
        NvOsDebugPrintf("%s: FlushCount=%u\n", __func__,
                        pDNvcUser->FlushCount);
        NvOsDebugPrintf("%s: InitialIntraFrameSkip=%u\n", __func__,
                        pDNvcUser->InitialIntraFrameSkip);
        NvOsDebugPrintf("%s: SteadyStateIntraFrameSkip=%u\n", __func__,
                        pDNvcUser->SteadyStateIntraFrameSkip);
        NvOsDebugPrintf("%s: SteadyStateFrameNumber=%u\n", __func__,
                        pDNvcUser->SteadyStateFrameNumber);
        NvOsDebugPrintf("%s: CoarseTime=%u\n", __func__,
                        pDNvcUser->CoarseTime);
        NvOsDebugPrintf("%s: MaxCoarseDiff=%u\n", __func__,
                        pDNvcUser->MaxCoarseDiff);
        NvOsDebugPrintf("%s: MinExposureCourse=%u`\n", __func__,
                        pDNvcUser->MinExposureCourse);
        NvOsDebugPrintf("%s: MaxExposureCourse=%u\n", __func__,
                        pDNvcUser->MaxExposureCourse);
        NvOsDebugPrintf("%s: DiffIntegrationTimeOfMode=%f\n", __func__,
                        pDNvcUser->DiffIntegrationTimeOfMode);
        NvOsDebugPrintf("%s: LineLength=%u\n", __func__,
                        pDNvcUser->LineLength);
        NvOsDebugPrintf("%s: FrameLength=%u\n", __func__,
                        pDNvcUser->FrameLength);
        NvOsDebugPrintf("%s: MinFrameLength=%u\n", __func__,
                        pDNvcUser->MinFrameLength);
        NvOsDebugPrintf("%s: MaxFrameLength=%u\n", __func__,
                        pDNvcUser->MaxFrameLength);
        NvOsDebugPrintf("%s: MinGain=%f\n", __func__, pDNvcUser->MinGain);
        NvOsDebugPrintf("%s: MaxGain=%f\n", __func__, pDNvcUser->MaxGain);
        NvOsDebugPrintf("%s: InherentGain=%f\n", __func__,
                        pDNvcUser->InherentGain);
        NvOsDebugPrintf("%s: InherentGainBinEn=%f\n", __func__,
                        pDNvcUser->InherentGainBinEn);
        NvOsDebugPrintf("%s: SupportsBinningControl=%x\n", __func__,
                        pDNvcUser->SupportsBinningControl);
        NvOsDebugPrintf("%s: SupportFastMode=%x\n", __func__,
                        pDNvcUser->SupportFastMode);
        NvOsDebugPrintf("%s: PllMult=%u\n", __func__, pDNvcUser->PllMult);
        NvOsDebugPrintf("%s: PllDiv=%u\n", __func__, pDNvcUser->PllDiv);
    }
#endif

    return NV_TRUE;
}

static void
NvcImager_ExposureUpdate(
    NvcImagerContext *pCtxt,
    NvU32 FrameLength,
    NvU32 CoarseTime)
{
    if (pCtxt->SNvc.SensorType == NVC_IMAGER_TYPE_SOC)
        return;

    // Update variables depending on FrameLength.
    pCtxt->FrameLength = FrameLength;
    pCtxt->FrameRate = (NvF32)pCtxt->ClkV.VtPixClkFreqHz /
                       (NvF32)(FrameLength * pCtxt->DNvc.LineLength);
    // Update variables depending on CoarseTime.
    pCtxt->CoarseTime = CoarseTime;
    pCtxt->Exposure = (((NvF32)CoarseTime -
                        (NvF32)pCtxt->DNvc.DiffIntegrationTimeOfMode) *
                       (NvF32)pCtxt->DNvc.LineLength) /
                      (NvF32)pCtxt->ClkV.VtPixClkFreqHz;
    if (pCtxt->BinningControlDirty)
    {
        pCtxt->BinningControlEnabled = NV_TRUE;
        pCtxt->InherentGain = pCtxt->DNvc.InherentGainBinEn;
    }
    else
    {
        pCtxt->BinningControlEnabled = NV_FALSE;
        pCtxt->InherentGain = pCtxt->DNvc.InherentGain;
    }
    NV_DEBUG_PRINTF(("FrameLength=%u CoarseTime=%u BinningEnabled=%x\n",
                     pCtxt->FrameLength,
                     pCtxt->CoarseTime,
                     pCtxt->BinningControlEnabled));
    NV_DEBUG_PRINTF(("Exposure=%f (%f, %f) InherentGain=%f\n",
                     pCtxt->Exposure,
                     pCtxt->ClkV.MinExposure,
                     pCtxt->ClkV.MaxExposure,
                     pCtxt->InherentGain));
    NV_DEBUG_PRINTF(("FrameRate=%f (%f, %f)\n",
                     pCtxt->FrameRate,
                     pCtxt->ClkV.MinFrameRate,
                     pCtxt->ClkV.MaxFrameRate));
}

static void
NvcImager_GainUpdate(
    NvcImagerContext *pCtxt,
    NvF32 Gains,
    NvU32 Gain)
{
    NvU32 i;

    for (i = 0; i < 4; i++)
        pCtxt->Gains[i] = Gains;
    pCtxt->Gain = Gain;
    NV_DEBUG_PRINTF(("Gain=%f (%f, %f) dev=%u\n",
                     pCtxt->Gains[1],
                     pCtxt->DNvc.MinGain,
                     pCtxt->DNvc.MaxGain,
                     pCtxt->Gain));
}

/**
 * NvcImager_WriteExposure
 * Calculate and write the values of the sensor registers for
 * the new exposure time. Without this, calibration will not be
 * able to capture images at various brightness levels, and the
 * final product won't be able to compensate for bright or dark
 * scenes.
 *
 * if pFrameLength or pCoarseTime is not NULL, return the resulting
 * frame length or coarse integration time instead of writing
 * the register.
 */
static NvBool
NvcImager_WriteExposure(
    NvcImagerContext *pCtxt,
    NvcImagerDynamicNvc *pDNvc,
    NvcImagerClockVariables *pClkV,
    const NvF32 *pNewExposureTime,
    NvU32 *pFrameLength,
    NvU32 *pCoarseTime)
{
    struct nvc_imager_bayer bayer;
    int err;
    NvF32 Freq = (NvF32)pClkV->VtPixClkFreqHz;
    NvF32 LineLength = (NvF32)pDNvc->LineLength;
    NvF32 ExpTime = *pNewExposureTime;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;
    if (pCtxt->TestPatternMode)
        return NV_FALSE;

    if (ExpTime > pClkV->MaxExposure)
    {
        NV_DEBUG_PRINTF(("Exposure %f > MaxExposure %f\n",
                         ExpTime, pClkV->MaxExposure));
        ExpTime = pClkV->MaxExposure;
    }
    if (ExpTime < pClkV->MinExposure)
    {
        NV_DEBUG_PRINTF(("Exposure %f < MinExposure %f\n",
                         ExpTime, pClkV->MinExposure));
        ExpTime = pClkV->MinExposure;
    }
    /* Here, we have to decide if the new exposure time is valid
     * based on the sensor and current sensor settings.
     */
    NewCoarseTime = (NvU32)((Freq * ExpTime) /
                    (LineLength + pDNvc->DiffIntegrationTimeOfMode));
    if (NewCoarseTime == 0)
        NewCoarseTime = 1;
    NewFrameLength = NewCoarseTime + pDNvc->MaxCoarseDiff;

    /* Consider requested max frame rate */
    if (pCtxt->RequestedMaxFrameRate > 0.0f)
    {
        NvU32 RequestedMinFrameLenLines;

        RequestedMinFrameLenLines =
                   (NvU32)(Freq / (LineLength * pCtxt->RequestedMaxFrameRate));
        if (NewFrameLength < RequestedMinFrameLenLines)
        {
            NV_DEBUG_PRINTF(("FrameLength %u < RequestedMinFrameLenLines %u\n",
                             NewFrameLength, RequestedMinFrameLenLines));
            NewFrameLength = RequestedMinFrameLenLines;
        }
    }
    /* Clamp to sensor's limit */
    if (NewFrameLength > pDNvc->MaxFrameLength)
    {
        NV_DEBUG_PRINTF(("FrameLength %u > MaxFrameLength %u\n",
                         NewFrameLength, pDNvc->MaxFrameLength));
        NewFrameLength = pDNvc->MaxFrameLength;
    }
    else if (NewFrameLength < pDNvc->MinFrameLength)
    {
        NV_DEBUG_PRINTF(("FrameLength %u < MinFrameLength %u\n",
                         NewFrameLength, pDNvc->MinFrameLength));
        NewFrameLength = pDNvc->MinFrameLength;
    }
    /* FrameLength should have been updated but we have to check again
     * in case FrameLength gets clamped.
     */
    if (NewCoarseTime > pDNvc->MaxExposureCourse)
    {
        NV_DEBUG_PRINTF(("CoarseTime %u > MaxExposureCourse %u\n",
                         NewCoarseTime, pDNvc->MaxExposureCourse));
        NewCoarseTime = pDNvc->MaxExposureCourse;
    }
    else if (NewCoarseTime < pDNvc->MinExposureCourse)
    {
        NV_DEBUG_PRINTF(("CoarseTime %u < MinExposureCourse %u\n",
                         NewCoarseTime, pDNvc->MinExposureCourse));
        NewCoarseTime = pDNvc->MinExposureCourse;
    }
    if (pFrameLength)
        *pFrameLength = NewFrameLength;
    if (pCoarseTime)
        *pCoarseTime = NewCoarseTime;
    if (!pFrameLength && !pCoarseTime)
    {
        if ((NewFrameLength != pCtxt->FrameLength) ||
                                          (pCtxt->CoarseTime != NewCoarseTime))
        {
            bayer.api_version = NVC_IMAGER_API_BAYER_VER;
            bayer.res_x = 0; /* no mode change when x && y == 0 */
            bayer.res_y = 0; /* but want to change other parameters */
            bayer.frame_length = NewFrameLength;
            bayer.coarse_time = NewCoarseTime;
            bayer.gain = pCtxt->Gain;
            bayer.bin_en = pCtxt->BinningControlDirty;
            err = ioctl(pCtxt->camera_fd, NVC_IOCTL_MODE_WR, &bayer);
            if (err < 0)
            {
                NvOsDebugPrintf("%s: ioctl to set exposure failed %s\n",
                                __func__, strerror(errno));
                return NV_FALSE;
            }

            NvcImager_ExposureUpdate(pCtxt, NewFrameLength, NewCoarseTime);
        }
    }
    return NV_TRUE;
}

/**
 * NvcImager_WriteGains
 * Writing the sensor registers for the new gains. Just like
 * exposure, this allows the sensor to brighten an image. If the
 * exposure time is insufficient to make the picture viewable,
 * gains are applied. During calibration, the gains will be
 * measured for maximum effectiveness before the noise level
 * becomes too high. If per-channel gains are available, they
 * are used to normalize the color. Most sensors output overly
 * greenish images, so the red and blue channels are increased.
 *
 * if pGain is not NULL, return the resulting gain instead of writing the
 * register.
 */
static NvBool
NvcImager_WriteGain(
    NvcImagerContext *pCtxt,
    const NvF32 *pGains,
    NvU32 *pGain)
{
    struct nvc_param param;
    NvS32 i = 1;
    int err;
    NvU32 NewGain;
    NvBool Status;

    Status = pCtxt->Hal.pfnFloat2Gain(pGains[i], &NewGain, pCtxt->DNvc.MinGain,
                                      pCtxt->DNvc.MaxGain);
    if (!Status)
        return NV_FALSE;

    if (pGain)
    {
        *pGain = NewGain;
        return NV_TRUE;
    }
    if (NewGain != pCtxt->Gain)
    {
        param.param = NVC_PARAM_GAIN;
        param.sizeofvalue = sizeof(NewGain);
        param.p_value = &NewGain;
        err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_WR, &param);
        if (err < 0)
        {
            NvOsDebugPrintf("%s: ioctl to set gain failed %s\n",
                            __func__, strerror(errno));
            return NV_FALSE;
        }
    }

    NvcImager_GainUpdate(pCtxt, pGains[i], NewGain);
    return NV_TRUE;
}


/**
 * NvcImager_GroupHold
 * Writing the sensor registers for the new exposure and gains.
 */
static NvBool
NvcImager_GroupHold(
    NvcImagerContext *pCtxt,
    const NvOdmImagerSensorAE *sensorAE)
{
    NvcImagerAEContext ae;
    struct nvc_param param;
    NvBool Status = NV_TRUE;
    int err;

    NvOdmOsMemset(&ae, 0, sizeof(NvcImagerAEContext));

    if (sensorAE->gains_enable==NV_TRUE) {
        Status = NvcImager_WriteGain(pCtxt, sensorAE->gains, &ae.gain);
        if (Status == NV_FALSE) {
            return NV_FALSE;
        }
        ae.gain_enable = NV_TRUE;
        NvcImager_GainUpdate(pCtxt, sensorAE->gains[1], ae.gain);
    }

    if (sensorAE->ET_enable==NV_TRUE) {
        Status = NvcImager_WriteExposure(pCtxt, &pCtxt->DNvc, &pCtxt->ClkV,
                                        &sensorAE->ET,
                                        &ae.frame_length,
                                        &ae.coarse_time);

        if (Status == NV_FALSE) {
            return NV_FALSE;
        }
        ae.frame_length_enable = NV_TRUE;
        ae.coarse_time_enable = NV_TRUE;
        NvcImager_ExposureUpdate(pCtxt, ae.frame_length, ae.coarse_time);
    }

    if (ae.gain_enable==NV_TRUE || ae.coarse_time_enable==NV_TRUE ||
        ae.frame_length_enable==NV_TRUE) {
        param.param = NVC_PARAM_GROUP_HOLD;
        param.sizeofvalue = sizeof(NvcImagerAEContext);
        param.p_value = &ae;
        err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_WR, &param);
        if (err < 0)
        {
            NvOsDebugPrintf("ioctl to set group hold failed %s\n", strerror(errno));
            return NV_FALSE;
        }
    }
    return NV_TRUE;
}


/**
 * NvcImager_SetInputClock
 * Setting the input clock and updating the variables based on
 * the input clock. The frame rate and exposure calculations are
 * directly related to the clock speed, so this is how the
 * camera driver (the controller of the clocks) can inform the
 * nvodm driver of the current clock speed.
 */
static void
NvcImager_SetInputClock(
    NvcImagerContext *pCtxt,
    NvcImagerClockVariables *pClkV,
    NvcImagerDynamicNvc *pDNvc)
{
    if (pCtxt->SNvc.SensorType == NVC_IMAGER_TYPE_SOC)
        return;

    pClkV->VtPixClkFreqHz = (NvU32)(pCtxt->Cap.ClockProfiles[0].ExternalClockKHz * pDNvc->PllMult /
                                    pDNvc->PllDiv / pCtxt->SNvc.BitsPerPixel *
                                    pCtxt->Cap.MipiTiming.DataLanes * 1000);
    pClkV->MaxExposure = (((NvF32)pDNvc->MaxExposureCourse -
                           pDNvc->DiffIntegrationTimeOfMode) *
                          (NvF32)pDNvc->LineLength ) /
                         (NvF32)pClkV->VtPixClkFreqHz;
    pClkV->MinExposure = (((NvF32)pDNvc->MinExposureCourse -
                           pDNvc->DiffIntegrationTimeOfMode) *
                          (NvF32)pDNvc->LineLength) /
                         (NvF32)pClkV->VtPixClkFreqHz;
    pClkV->MaxFrameRate = (NvF32)pClkV->VtPixClkFreqHz /
                          (NvF32)(pDNvc->MinFrameLength * pDNvc->LineLength);
    pClkV->MinFrameRate = (NvF32)pClkV->VtPixClkFreqHz /
                          (NvF32)(pDNvc->MaxFrameLength * pDNvc->LineLength);
}

/**
 * NvcImager_Open.
 * Initialize sensor bayer and its private context
 */
static NvBool
NvcImager_Open(NvOdmImagerHandle hImager)
{
    NvcImagerContext *pCtxt;
    NvU32 Instance;
    char DevName[NVODMIMAGER_IDENTIFIER_MAX];
    NvBool Status;

    NV_DEBUG_PRINTF(("%s +++++\n", __func__));
    if (!hImager || !hImager->pSensor)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor\n", __func__);
        return NV_FALSE;
    }

    if (sizeof(NvOdmImagerCapabilities) != sizeof(struct nvc_imager_cap))
        NvOsDebugPrintf("%s: ERR NvOdmImagerCapabilities (%d) !=  nvc_imager_cap (%d)\n",
                        __func__,
                        sizeof(NvOdmImagerCapabilities),
                        sizeof(struct nvc_imager_cap));
    if (sizeof(NvOdmImagerSensorMode) != sizeof(struct nvc_imager_mode))
        NvOsDebugPrintf("%s: ERR NvOdmImagerSensorMode (%d) !=  nvc_imager_mode (%d)\n",
                        __func__,
                        sizeof(NvOdmImagerSensorMode),
                        sizeof(struct nvc_imager_mode));
    if (sizeof(NvcImagerDynamicNvc) != sizeof(struct nvc_imager_dynamic_nvc))
        NvOsDebugPrintf("%s: ERR NvcImagerDynamicNvc (%d) !=  nvc_imager_dynamic_nvc (%d)\n",
                        __func__,
                        sizeof(NvcImagerDynamicNvc),
                        sizeof(struct nvc_imager_dynamic_nvc));
    if (sizeof(NvcImagerStaticNvc) != sizeof(struct nvc_imager_static_nvc))
        NvOsDebugPrintf("%s: ERR NvcImagerStaticNvc (%d) !=  nvc_imager_static_nvc (%d)\n",
                        __func__,
                        sizeof(NvcImagerStaticNvc),
                        sizeof(struct nvc_imager_static_nvc));
    pCtxt = (NvcImagerContext *)NvOsAlloc(sizeof(NvcImagerContext));
    if (!pCtxt)
    {
        NvOsDebugPrintf("%s: NvOsAlloc failure\n", __func__);
        return NV_FALSE;
    }

    hImager->pSensor->pPrivateContext = pCtxt;
    NvOsMemset(pCtxt, 0, sizeof(*pCtxt));
    Instance = (unsigned)hImager->pSensor->GUID;
    Instance &= 0xF;
    if (Instance)
        sprintf(DevName, "/dev/camera.%u", Instance);
    else
        sprintf(DevName, "/dev/camera");

#ifdef O_CLOEXEC
    // use O_CLOEXEC when available, it means "close on execute".
    // So even if the media server process is forked and the sensor's
    // file descriptor is inherited, it will be closed by whichever
    // process calls close first; avoiding an fd leak.
    pCtxt->camera_fd = open(DevName, O_RDWR | O_CLOEXEC);
#else
    pCtxt->camera_fd = open(DevName, O_RDWR);
#endif
    if (pCtxt->camera_fd < 0)
    {
        NvOsDebugPrintf("%s: Can not open camera device %s: %s\n",
                        __func__, DevName, strerror(errno));
        NvcImager_Close(hImager);
        return NV_FALSE;
    }

    /* get capabilities and static parameters */
    Status = NvcImager_StaticNvcRead(pCtxt);
    if (!Status)
    {
        NvOsDebugPrintf("%s: ERR: StaticNvcRead failed\n", __func__);
        NvcImager_Close(hImager);
        return NV_FALSE;
    }

    /* Get default NVC data (x & y = 0) */
    Status = NvcImager_ModeNvcRead(pCtxt, 0, 0, &pCtxt->Mode, &pCtxt->DNvc);
    if (!Status)
    {
        NvOsDebugPrintf("%s: ERR: ModeNvcRead failed\n", __func__);
        NvcImager_Close(hImager);
        return NV_FALSE;
    }

    NvOsDebugPrintf("%s: Camera %s fd open as: %d\n",
                    __func__, DevName, pCtxt->camera_fd);
    NV_DEBUG_PRINTF(("%s -----\n", __func__));
    return NV_TRUE;
}

/**
 * NvcImager_Close
 * Free the sensor's private context
 */
static void NvcImager_Close(NvOdmImagerHandle hImager)
{
    NvcImagerContext *pCtxt;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                         __func__);
        return;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    close(pCtxt->camera_fd);
    NvOsFree(pCtxt->pModeList);
    NvOsFree(pCtxt);
    hImager->pSensor->pPrivateContext = NULL;
}

/**
 * NvcImager_GetCapabilities
 * Return sensor bayer's capabilities
 */
static void
NvcImager_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
    NvcImagerContext *pCtxt;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                        __func__);
        return;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    NvOsMemcpy(pCapabilities, &pCtxt->Cap, sizeof(pCtxt->Cap));
}

/**
 * NvcImager_ListModes
 * Return a list of modes that sensor bayer supports. If called
 * with a NULL ptr to pModes, then it just returns the count
 */
static void
NvcImager_ListModes(
    NvOdmImagerHandle hImager,
    NvOdmImagerSensorMode *pModes,
    NvS32 *pNumModes)
{
    NvcImagerContext *pCtxt;
    NvU32 i;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                        __func__);
        return;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    if (pNumModes)
    {
        *pNumModes = (NvS32)pCtxt->NumModes;
        if (pModes)
        {
            for (i = 0; i < pCtxt->NumModes; i++)
            {
                pModes[i] = pCtxt->pModeList[i];
                if (pCtxt->StereoCameraMode == StereoCameraMode_Stereo)
                    pModes[i].PixelAspectRatio /= 2.0;
            }
        }
    }
}

/**
 * NvcImager_SetMode. Phase 1.
 * Set sensor bayer to the mode of the desired resolution and frame rate.
 */
static NvBool
NvcImager_SetMode(
    NvOdmImagerHandle hImager,
    const SetModeParameters *pParameters,
    NvOdmImagerSensorMode *pSelectedMode,
    SetModeParameters *pResult)
{
    NvcImagerContext *pCtxt;
    struct nvc_imager_bayer bayer;
    int err;
    NvBool Status = NV_FALSE;
    NvU32 NewCoarseTime = 0;
    NvU32 NewFrameLength = 0;
    NvU32 NewGain = 0;

    NV_DEBUG_PRINTF(("Request mode to %dx%d, exposure %f, gains %f\n",
                 pParameters->Resolution.width, pParameters->Resolution.height,
                 pParameters->Exposure, pParameters->Gains[0]));
    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                         __func__);
        return NV_FALSE;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    Status = NvcImager_ModeNvcRead(pCtxt,
                                  pParameters->Resolution.width,
                                  pParameters->Resolution.height,
                                  &pCtxt->ModeTmp,
                                  &pCtxt->DNvcTmp);
    if (!Status)
        return NV_FALSE;

    NvcImager_SetInputClock(pCtxt, &pCtxt->ClkVTmp, &pCtxt->DNvcTmp);
    if (pParameters->Exposure == 0.0 || pCtxt->TestPatternMode)
    {
        NewFrameLength = pCtxt->DNvcTmp.FrameLength;
        NewCoarseTime = pCtxt->DNvcTmp.CoarseTime;
    }
    else
    {
        NvcImager_WriteExposure(pCtxt, &pCtxt->DNvcTmp, &pCtxt->ClkVTmp,
                               &pParameters->Exposure,
                               &NewFrameLength, &NewCoarseTime);
    }

    Status = NvcImager_WriteGain(pCtxt, pParameters->Gains, &NewGain);
    if (!Status)
        return NV_FALSE;

    bayer.api_version = NVC_IMAGER_API_BAYER_VER;
    bayer.res_x = pCtxt->ModeTmp.ActiveDimensions.width;
    bayer.res_y = pCtxt->ModeTmp.ActiveDimensions.height;
    bayer.frame_length = NewFrameLength;
    bayer.coarse_time = NewCoarseTime;
    bayer.gain = NewGain;
    bayer.bin_en = pCtxt->BinningControlDirty;
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_MODE_WR, &bayer);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to set mode failed %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    pCtxt->FrameErrorCount = 0;
    pCtxt->DNvc = pCtxt->DNvcTmp;
    pCtxt->Mode = pCtxt->ModeTmp;
    pCtxt->ClkV = pCtxt->ClkVTmp;
    NV_DEBUG_PRINTF(("-------------SetMode---------------\n"
                     "Resolution set to %dx%d\n",
                     pCtxt->Mode.ActiveDimensions.width,
                     pCtxt->Mode.ActiveDimensions.height));
    NvcImager_ExposureUpdate(pCtxt, NewFrameLength, NewCoarseTime);
    NvcImager_GainUpdate(pCtxt, pParameters->Gains[0], NewGain);

    if (pSelectedMode)
        *pSelectedMode = pCtxt->Mode;

    if (pResult)
    {
        pResult->Resolution = pCtxt->Mode.ActiveDimensions;
        pResult->Exposure = pCtxt->Exposure;
        NvOsMemcpy(pResult->Gains, &pCtxt->Gains, sizeof(NvF32) * 4);
    }

    // Do not sleep if we are calling set mode for the
    // first time after power on.
    if (pCtxt->SensorInitialized)
    {
        NvOsSleepMS((NvU32)(1000.0 * pCtxt->DNvc.ModeSwWaitFrames
                    / (NvF32)(pCtxt->FrameRate)));
    }
    pCtxt->SensorInitialized = NV_TRUE;

    if (pCtxt->TestPatternMode)
    {
        NvF32 Gains[4];
        NvU32 i;

        // reset gains
        for (i = 0; i < 4; i++)
            Gains[i] = pCtxt->DNvc.MinGain;

        Status = NvcImager_WriteGain(pCtxt, Gains, NULL);
        if (!Status)
            return NV_FALSE;
    }

    return NV_TRUE;
}

/**
 * NvcImager_SetPowerLevel. Phase 1
 * Set the sensor's power level
 */
static NvBool
NvcImager_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
    NvcImagerContext *pCtxt;
    struct nvc_imager_bayer bayer;
    int err;
    NvBool Status = NV_TRUE;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s(%d): No hImager->pSensor->pPrivateContext\n",
                         __func__, __LINE__);
        return NV_FALSE;
    }

    NV_DEBUG_PRINTF(("%s: PowerLevel=%d\n", __func__, PowerLevel));
    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PWR_WR, PowerLevel);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: IOCTL_PWR_WR (%d) failed: %s\n",
                        __func__, PowerLevel, strerror(errno));
        Status = NV_FALSE;
    }
    /* Here we use the _SetPowerLevel function to determine if we want to turn
     * off streaming.  This is still a GLOS (Guaranteed Level Of Service) call,
     * but since the ODM API doesn't have a call for turning on/off streaming,
     * we create the on/off mechanism here.
     */
    if (PowerLevel < NvOdmImagerPowerLevel_On)
    {
        /* The NVC kernel drivers use the _MODE_WR with the nvc_imager_bayer
         * structure members set to 0 as the message to turn off streaming.
         */
        NvOsMemset(&bayer, 0, sizeof(bayer));
        bayer.api_version = NVC_IMAGER_API_BAYER_VER;
        err = ioctl(pCtxt->camera_fd, NVC_IOCTL_MODE_WR, &bayer);
        if (err < 0)
            NvOsDebugPrintf("%s: IOCTL_MODE_WR failed to stop streaming %s\n",
                            __func__, strerror(errno));
        else
            NV_DEBUG_PRINTF(("%s: Streaming stopped\n", __func__));
    }
    return Status;
}

/**
 * NvcImager_GetPowerLevel
 * Get the sensor's current power level
 */
static void
NvcImager_GetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel *pPowerLevel)
{
    NvcImagerContext *pCtxt;
    int err;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                        __func__);
        return;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PWR_RD, pPowerLevel);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl failed: %s\n", __func__, strerror(errno));
        *pPowerLevel = 0;
    }
}

static NvBool
NvcImager_TurnOnFlash(
    NvcImagerContext *pCtxt,
    NvOdmImagerSensorFlashControl *pmode)
{
    struct nvc_param param;
    union nvc_imager_flash_control fm;
    int ret;

    fm.mode = 0;
    NV_DEBUG_PRINTF(("%s: Flash %s, Pulse %s, EoF %s, Rep %s, Dly %d\n", __func__,
                        pmode->FlashEnable ? "Enabled" : "Disabled",
                        pmode->FlashPulseEnable ? "Enabled" : "Disabled",
                        pmode->PulseAtEndofFrame ? "TRUE" : "FALSE",
                        pmode->PulseRepeats ? "Yes" : "No",
                        pmode->PulseDelayFrames));
    if (pmode->FlashEnable)
    {
        fm.settings.enable = 1;
        if (pmode->FlashPulseEnable)
        {
            fm.settings.edge_trig_en = 1;
            if (pmode->PulseAtEndofFrame)
            {
                fm.settings.start_edge = 1;
            }
            if (pmode->PulseRepeats)
            {
                fm.settings.repeat = 1;
            }
            fm.settings.delay_frm = pmode->PulseDelayFrames & 0x03;
        }
    }
    param.param = NVC_PARAM_SET_SENSOR_FLASH_MODE;
    param.sizeofvalue = sizeof(fm);
    param.p_value = &fm;
    ret = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_WR, &param);
    if (ret < 0)
    {
        NvOsDebugPrintf("ioctl to turn on/off flash failed: %s\n", strerror(errno));
        return NV_FALSE;
    }

    return NV_TRUE;
}

static int
NvcImager_SetParamIspSwitch(
    NvOdmImagerHandle hImager,
    NvOdmImagerISPSetting *pIsp)
{
    switch(pIsp->attribute)
    {
        case NvCameraIspAttribute_AutoExposureRegions:
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusTrigger:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusTrigger"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusStatus:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusStatus"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AntiFlicker:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AntiFlicker"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFrameRate:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFrameRate"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_EdgeEnhancement:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_EdgeEnhancement"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Stylize:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_Stylize"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContinuousAutoFocus:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContinuousAutoFocus"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContinuousAutoExposure:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContinuousAutoExposure"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContinuousAutoWhiteBalance:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContinuousAutoWhiteBalance"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoNoiseReduction:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoNoiseReduction"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_EffectiveISO:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_EffectiveISO"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureTime:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ExposureTime"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FlickerFrequency:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_FlickerFrequency"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_WhiteBalanceCompensation:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_WhiteBalanceCompensation"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureCompensation:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ExposureCompensation"));
            return NV_TRUE;

        case NvCameraIspAttribute_MeteringMode:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_MeteringMode"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_StylizeMode:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_StylizeMode"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_LumaBias:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_LumaBias"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_EmbossStrength:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_EmbossStrength"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ImageGamma:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ImageGamma"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContrastEnhancement:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ContrastEnhancement"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FocusPosition:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_FocusPosition"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FocusPositionRange:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_FocusPositionRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_WhiteBalanceCCTRange:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_WhiteBalanceCCTRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFrameRateRange:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFrameRateRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_StylizeColorMatrix:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_StylizeColorMatrix"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_DefaultYUVConversionMatrix:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_DefaultYUVConversionMatrix"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusRegions:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusRegions"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusRegionMask:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusRegionMask"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusRegionStatus:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusRegionStatus"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Hue:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_Hue"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Saturation:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_Saturation"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Brightness:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_Brightness"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Warmth:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_Warmth"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Stabilization:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_Stabilization"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureTimeRange:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ExposureTimeRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ISORange:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_ISORange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_SceneBrightness:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_SceneBrightness"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusControl:
            NV_DEBUG_PRINTF(("NvCameraIspAttribute_AutoFocusControl"));
            return NvcParamRetExitFalse; /* no support yet */

        default:
            NV_DEBUG_PRINTF(("undefined attribute"));
            return NvcParamRetExitFalse;
    }

    return NvcParamRetContinueTrue;
}

static int
NvcImager_SetParamIspSwitchExit(
    NvOdmImagerHandle hImager,
    NvOdmImagerISPSetting *pIsp,
    int err)
{
    return NvcParamRetContinueTrue;
}

static int
NvcImager_SetParamSwitch(
    NvOdmImagerHandle hImager,
    struct nvc_param *param)
{
    NvcImagerContext *pCtxt = (NvcImagerContext *)
                              hImager->pSensor->pPrivateContext;
    NvBool Status;

    switch (param->param)
    {
        case NvOdmImagerParameter_SensorGain:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             4 * sizeof(NvF32));
            NV_DEBUG_PRINTF(("%s: WriteGains %f %f %f %f\n",
                            __func__,
                            ((NvF32 *)(param->p_value))[0],
                            ((NvF32 *)(param->p_value))[1],
                            ((NvF32 *)(param->p_value))[2],
                            ((NvF32 *)(param->p_value))[3]));
            Status = NvcImager_WriteGain(pCtxt, param->p_value, NULL);
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_SensorExposure:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            NV_DEBUG_PRINTF(("%s: SensorExposure=%f\n",
                             __func__, *((NvF32 *)param->p_value)));
            Status = NvcImager_WriteExposure(pCtxt, &pCtxt->DNvc,
                                             &pCtxt->ClkV,
                                             (NvF32*)param->p_value,
                                             NULL, NULL);
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_SensorGroupHold:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                        sizeof(NvOdmImagerSensorAE));
            Status = NvcImager_GroupHold(pCtxt,
                                (NvOdmImagerSensorAE *)param->p_value);
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;
        break;

        case NvOdmImagerParameter_MaxSensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            if (pCtxt->RequestedMaxFrameRate != *((NvF32 *)param->p_value))
                NV_DEBUG_PRINTF(("%s: MaxSensorFrameRate=%f\n",
                                 __func__, *((NvF32 *)param->p_value)));
            pCtxt->RequestedMaxFrameRate = *((NvF32 *)param->p_value);
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorInherentGainAtResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvBool));
            CHECK_SENSOR_RETURN_NOT_INITIALIZED(pCtxt);
            NV_DEBUG_PRINTF(("%s: SensorInherentGainAtResolution=%x\n",
                             __func__, *((NvBool *)param->p_value)));
            if (pCtxt->DNvc.SupportsBinningControl)
            {
                NvBool flag = *((NvBool *)param->p_value);

                if (flag)
                    pCtxt->BinningControlDirty = 1;
                else
                    pCtxt->BinningControlDirty = 0;
                return NvcParamRetExitTrue;
            }
            NV_DEBUG_PRINTF(("%s: Binning not supported at full resolution\n",
                             __func__));
            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_StereoCameraMode:
            param->param = NVC_PARAM_STEREO;
            return NvcParamRetContinueTrue;

        case NvOdmImagerParameter_OptimizeResolutionChange:
            NV_DEBUG_PRINTF(("%s: OptimizeResolutionChange\n", __func__));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_CustomizedBlockInfo:
            NV_DEBUG_PRINTF(("%s: CustomizedBlockInfo\n", __func__));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SelfTest:
            NV_DEBUG_PRINTF(("%s: SelfTest\n", __func__));
            param->param = NVC_PARAM_SELF_TEST;
            return NvcParamRetContinueTrue;

        case NvOdmImagerParameter_TestMode:
        {
            NV_DEBUG_PRINTF(("%s: TestMode\n", __func__));
            param->param = NVC_PARAM_TESTMODE;
            pCtxt->TestPatternMode = *((NvBool *)param->p_value);

            if (pCtxt->TestPatternMode)
            {
                // reset gains
                Status = NvcImager_WriteGain(pCtxt, &pCtxt->DNvc.MinGain, NULL);
                if (!Status)
                    return NV_FALSE;
            }
            return NvcParamRetContinueTrue;
        }

        case NvOdmImagerParameter_Reset:
            NV_DEBUG_PRINTF(("%s: Reset\n", __func__));
            param->param = NVC_PARAM_RESET;
            return NvcParamRetContinueTrue;

        case NvOdmImagerParameter_SensorFlashSet:
            Status = NvcImager_TurnOnFlash(pCtxt, (NvOdmImagerSensorFlashControl *)param->p_value);
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_SensorFlashSet %d\n", __func__, Status));

            if (Status)
                return NvcParamRetExitTrue;
            return NvcParamRetExitFalse;

        default:
            NV_DEBUG_PRINTF(("%s: Unknown parameter: %d\n",
                             __func__, param->param));
            return NvcParamRetExitFalse;
    }

    return NvcParamRetContinueTrue;
}

static int
NvcImager_SetParamSwitchExit(
    NvOdmImagerHandle hImager,
    struct nvc_param *param,
    int err)
{
    NvcImagerContext *pCtxt = (NvcImagerContext *)
                              hImager->pSensor->pPrivateContext;

    switch (param->param)
    {
        case NvOdmImagerParameter_StereoCameraMode:
            if (!err)
            {
                pCtxt->StereoCameraMode =
                              *((NvOdmImagerStereoCameraMode *)param->p_value);
                return NvcParamRetExitTrue;
            }

            return NvcParamRetExitFalse;

        default:
            return NvcParamRetContinueTrue;
    }

    return NvcParamRetContinueFalse;
}

/**
 * NvcImager_SetParameter
 * Set sensor specific parameters. This can return NV_TRUE.
 */
static NvBool
NvcImager_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
    NvcImagerContext *pCtxt;
    struct nvc_param param;
    int err;
    int Ret;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                         __func__);
        return NV_FALSE;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    if (Param == NvOdmImagerParameter_ISPSetting)
    {
        NvOdmImagerISPSetting *pIsp;
        NvOdmImagerISPSetting Isp;

        CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                                         sizeof(NvOdmImagerISPSetting));

        pIsp = (NvOdmImagerISPSetting *)pValue;
        Isp.attribute = pIsp->attribute;
        Isp.pData = pIsp->pData;
        Isp.dataSize = pIsp->dataSize;
        /* See the NvcParamRet notes in imager_nvc.h */
        if (pCtxt->Hal.pfnParamIspRd != NULL)
        {
            Ret = pCtxt->Hal.pfnParamIspWr(hImager, &Isp);
            if (Ret == NvcParamRetContinueFalse)
                Ret = NvcImager_SetParamIspSwitch(hImager, &Isp);
        }
        else
        {
            Ret = NvcImager_SetParamIspSwitch(hImager, &Isp);
        }
        if (Ret == NvcParamRetExitTrue)
            return NV_TRUE;

        if ((Ret == NvcParamRetExitFalse) || (Ret == NvcParamRetContinueFalse))
            return NV_FALSE;

        /* NvcParamRetContinueTrue */
        err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_ISP_WR,
                    (struct nvc_param_isp *)&Isp);
        Isp.attribute = pIsp->attribute;
        /* See the NvcParamRet notes in imager_nvc.h */
        if (pCtxt->Hal.pfnParamIspWrExit != NULL)
        {
            Ret = pCtxt->Hal.pfnParamIspWrExit(hImager, &Isp, err);
            if (Ret == NvcParamRetExitTrue)
                return NV_TRUE;

            if (Ret == NvcParamRetExitFalse)
                return NV_FALSE;
        }

        Ret = NvcImager_SetParamIspSwitchExit(hImager, &Isp, err);
        if (Ret == NvcParamRetExitTrue)
            return NV_TRUE;

        if (Ret == NvcParamRetExitFalse)
            return NV_FALSE;

        if (err < 0)
        {
            NvOsDebugPrintf("%s: NVC_IOCTL_PARAM_ISP_WR (%d) failed: %s\n",
                            __func__, pIsp->attribute, strerror(errno));
            return NV_FALSE;
        }

        return NV_TRUE;
    }

    param.param = Param;
    param.sizeofvalue = SizeOfValue;
    param.p_value = (void *)pValue;
    /* See the NvcParamRet notes in imager_nvc.h */
    if (pCtxt->Hal.pfnParamWr != NULL)
    {
        Ret = pCtxt->Hal.pfnParamWr(hImager, &param);
        if (Ret == NvcParamRetContinueFalse)
            Ret = NvcImager_SetParamSwitch(hImager, &param);
    }
    else
    {
        Ret = NvcImager_SetParamSwitch(hImager, &param);
    }
    if (Ret == NvcParamRetExitTrue)
        return NV_TRUE;

    if ((Ret == NvcParamRetExitFalse) || (Ret == NvcParamRetContinueFalse))
        return NV_FALSE;

    /* NvcParamRetContinueTrue */
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_WR, &param);
    param.param = Param;
    /* See the NvcParamRet notes in imager_nvc.h */
    if (pCtxt->Hal.pfnParamWrExit != NULL)
    {
        Ret = pCtxt->Hal.pfnParamWrExit(hImager, &param, err);
        if (Ret == NvcParamRetExitTrue)
            return NV_TRUE;

        if (Ret == NvcParamRetExitFalse)
            return NV_FALSE;
    }

    Ret = NvcImager_SetParamSwitchExit(hImager, &param, err);
    if (Ret == NvcParamRetExitTrue)
        return NV_TRUE;

    if (Ret == NvcParamRetExitFalse)
        return NV_FALSE;

    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl NVC_IOCTL_PARAM_WR (%d) failed: %s\n",
                        __func__, Param, strerror(errno));
        return NV_FALSE;
    }

    return NV_TRUE;
}


static int
NvcImager_GetParamIspSwitch(
    NvOdmImagerHandle hImager,
    NvOdmImagerISPSetting *pIsp)
{
    switch(pIsp->attribute)
    {
        case NvCameraIspAttribute_AntiFlicker:
            NV_DEBUG_PRINTF(("ISP AntiFlicker"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFrameRate:
            NV_DEBUG_PRINTF(("ISP AutoFrameRate"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_EdgeEnhancement:
            NV_DEBUG_PRINTF(("ISP EdgeEnhancement"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Stylize:
            NV_DEBUG_PRINTF(("ISP Stylize"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContinuousAutoFocus:
            NV_DEBUG_PRINTF(("ISP ContinuousAutoFocus"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContinuousAutoExposure:
            NV_DEBUG_PRINTF(("ISP ContinuousAutoExposure"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContinuousAutoWhiteBalance:
            NV_DEBUG_PRINTF(("ISP ContinuousAutoWhiteBalance"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoNoiseReduction:
            NV_DEBUG_PRINTF(("ISP AutoNoiseReduction"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_EffectiveISO:
            NV_DEBUG_PRINTF(("ISP EffectiveISO"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureTime:
            NV_DEBUG_PRINTF(("ISP ExposureTime"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FlickerFrequency:
            NV_DEBUG_PRINTF(("ISP FlickerFrequency"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_WhiteBalanceCompensation:
            NV_DEBUG_PRINTF(("ISP WhiteBalanceCompensation"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureCompensation:
            NV_DEBUG_PRINTF(("ISP ExposureCompensation"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_MeteringMode:
            NV_DEBUG_PRINTF(("ISP MeteringMode"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_StylizeMode:
            NV_DEBUG_PRINTF(("ISP StylizeMode"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_LumaBias:
            NV_DEBUG_PRINTF(("ISP LumaBias"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_EmbossStrength:
            NV_DEBUG_PRINTF(("ISP EmbossStrength"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ImageGamma:
            NV_DEBUG_PRINTF(("ISP ImageGamma"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ContrastEnhancement:
            NV_DEBUG_PRINTF(("ISP ContrastEnhancement"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FocusPosition:
            NV_DEBUG_PRINTF(("ISP FocusPosition"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FocusPositionRange:
            NV_DEBUG_PRINTF(("ISP FocusPositionRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_WhiteBalanceCCTRange:
            NV_DEBUG_PRINTF(("ISP WhiteBalanceCCTRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFrameRateRange:
            NV_DEBUG_PRINTF(("ISP AutoFrameRateRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_StylizeColorMatrix:
            NV_DEBUG_PRINTF(("ISP StylizeColorMatrix"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_DefaultYUVConversionMatrix:
            NV_DEBUG_PRINTF(("ISP DefaultYUVConversionMatrix"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusRegions:
            NV_DEBUG_PRINTF(("ISP AutoFocusRegions"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusRegionMask:
            NV_DEBUG_PRINTF(("ISP AutoFocusRegionMask"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusRegionStatus:
            NV_DEBUG_PRINTF(("ISP AutoFocusRegionStatus"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Hue:
            NV_DEBUG_PRINTF(("ISP Hue"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Saturation:
            NV_DEBUG_PRINTF(("ISP Saturation"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Brightness:
            NV_DEBUG_PRINTF(("ISP Brightness"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Warmth:
            NV_DEBUG_PRINTF(("ISP Warmth"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_Stabilization:
            NV_DEBUG_PRINTF(("ISP Stabilization"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureTimeRange:
            NV_DEBUG_PRINTF(("ISP ExposureTimeRange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ISORange:
            NV_DEBUG_PRINTF(("ISP ISORange"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_SceneBrightness:
            NV_DEBUG_PRINTF(("ISP SceneBrightness"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_AutoFocusControl:
            NV_DEBUG_PRINTF(("ISP AutoFocusControl"));
            pIsp->attribute = NVC_PARAM_ISP_FOCUS_CTRL;
            break;

        case NvCameraIspAttribute_AutoFocusTrigger:
            NV_DEBUG_PRINTF(("ISP AutoFocusTrigger"));
            pIsp->attribute = NVC_PARAM_ISP_FOCUS_TRGR;
            break;

        case NvCameraIspAttribute_AutoFocusStatus:
            NV_DEBUG_PRINTF(("ISP AutoFocusStatus\n"));
            pIsp->attribute = NVC_PARAM_ISP_FOCUS_STS;
            break;

        case NvCameraIspAttribute_ExposureProgram:
            NV_DEBUG_PRINTF(("ISP ExposureProgram"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_ExposureMode:
            NV_DEBUG_PRINTF(("ISP ExposureMode"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_SceneCaptureType:
            NV_DEBUG_PRINTF(("ISP SceneCaptureType"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_FlashStatus:
            NV_DEBUG_PRINTF(("ISP FlashStatus"));
            return NvcParamRetExitFalse; /* no support yet */

        case NvCameraIspAttribute_WhiteBalanceMode:
            NV_DEBUG_PRINTF(("ISP WhiteBalanceMode"));
            return NvcParamRetExitFalse; /* no support yet */

        default:
            NV_DEBUG_PRINTF(("ISPSetting: undefined attribute"));
            return NvcParamRetContinueFalse;
    }

    return NvcParamRetContinueTrue;
}

static int
NvcImager_GetParamIspSwitchExit(
    NvOdmImagerHandle hImager,
    NvOdmImagerISPSetting *pIsp,
    int err)
{
    NvF32 *pNvF32Val = (NvF32 *)pIsp->pData;
    NvU32 *pNvU32Val = (NvU32 *)pIsp->pData;

    switch (pIsp->attribute)
    {
        case NvCameraIspAttribute_Hue:
        case NvCameraIspAttribute_Saturation:
        case NvCameraIspAttribute_Brightness:
        case NvCameraIspAttribute_Warmth:
            if (!err)
            {
                *pNvF32Val = (NvF32)(*(NvF32 *)pNvU32Val /
                                     NVC_IMAGER_INT2FLOAT_DIVISOR);
            }
            return NvcParamRetExitTrue;

        default:
            return NvcParamRetContinueFalse;
    }

    return NvcParamRetContinueTrue;
}

static int
NvcImager_GetParamSwitch(
    NvOdmImagerHandle hImager,
    struct nvc_param *param)
{
    NvcImagerContext *pCtxt = (NvcImagerContext *)
                              hImager->pSensor->pPrivateContext;
    NvBool *pBL = (NvBool*)param->p_value;
    NvF32 *pNvF32Val = (NvF32 *)param->p_value;
    NvU32 *pNvU32Val = (NvU32 *)param->p_value;
    NvBool Status = NV_TRUE;

    switch (param->param)
    {
        case NvOdmImagerParameter_SensorFrameRateLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             (2 * sizeof(NvF32)));
            pNvF32Val[0] = pCtxt->ClkV.MinFrameRate;
            pNvF32Val[1] = pCtxt->ClkV.MaxFrameRate;
            NV_DEBUG_PRINTF(("%s: SensorFrameRateLimits min=%f max=%f\n",
                             __func__, pNvF32Val[0], pNvF32Val[1]));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            *pNvF32Val = pCtxt->FrameRate;
            NV_DEBUG_PRINTF(("%s: SensorFrameRate=%f\n",
                             __func__, *pNvF32Val));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorExposureLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             2 * sizeof(NvF32));
            pNvF32Val[0] = pCtxt->ClkV.MinExposure;
            pNvF32Val[1] = pCtxt->ClkV.MaxExposure;
            NV_DEBUG_PRINTF(("%s: SensorExposureLimits min=%f max=%f\n",
                             __func__, pNvF32Val[0], pNvF32Val[1]));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorGainLimits:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             2 * sizeof(NvF32));
            pNvF32Val[0] = pCtxt->DNvc.MinGain;
            pNvF32Val[1] = pCtxt->DNvc.MaxGain;
            NV_DEBUG_PRINTF(("%s: SensorGainLimits min=%f max=%f\n",
                             __func__, pNvF32Val[0], pNvF32Val[1]));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_LinesPerSecond:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            *pNvF32Val = pCtxt->FrameRate *
                         (NvF32)pCtxt->Mode.ActiveDimensions.height;
            NV_DEBUG_PRINTF(("%s: LinesPerSecond=%f\n", __func__, *pNvF32Val));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_MaxSensorFrameRate:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            *pNvF32Val = pCtxt->RequestedMaxFrameRate;
            NV_DEBUG_PRINTF(("%s: MaxSensorFrameRate=%f\n",
                             __func__, *pNvF32Val));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_ExpectedValues:
            NV_DEBUG_PRINTF(("%s: ExpectedValues\n", __func__));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorGain:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             4 * sizeof(NvF32));
            NvOsMemcpy((NvF32 *)param->p_value,
                       pCtxt->Gains, sizeof(NvF32) * 4);
            NV_DEBUG_PRINTF(("%s: SensorGain=%f %f %f %f\n",
                             __func__,
                             pCtxt->Gains[0],
                             pCtxt->Gains[1],
                             pCtxt->Gains[2],
                             pCtxt->Gains[3]));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorExposure:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            *pNvF32Val = pCtxt->Exposure;
            NV_DEBUG_PRINTF(("%s: SensorExposure=%f\n",
                             __func__, pCtxt->Exposure));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorGroupHold:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                        sizeof(NvBool));
            *pBL = NV_TRUE;
            return NvcParamRetExitTrue;
        break;

        case NvOdmImagerParameter_SensorFrameRateLimitsAtResolution:
        {
            NvOdmImagerFrameRateLimitAtResolution *pFrameRates;

            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                sizeof(NvOdmImagerFrameRateLimitAtResolution));
            pFrameRates = (NvOdmImagerFrameRateLimitAtResolution *)
                          param->p_value;
            if ((pFrameRates->Resolution.width ==
                                        pCtxt->Mode.ActiveDimensions.width &&
                                        pFrameRates->Resolution.height ==
                                        pCtxt->Mode.ActiveDimensions.height) ||
                                        (!pFrameRates->Resolution.width &&
                                         !pFrameRates->Resolution.height))
            {
                pFrameRates->MaxFrameRate = pCtxt->ClkV.MaxFrameRate;
                pFrameRates->MinFrameRate = pCtxt->ClkV.MinFrameRate;
            }
            else
            {
                Status = NvcImager_ModeNvcRead(pCtxt,
                                              pFrameRates->Resolution.width,
                                              pFrameRates->Resolution.height,
                                              NULL,
                                              &pCtxt->DNvcTmp);
                if (Status)
                {
                    NvcImager_SetInputClock(pCtxt, &pCtxt->ClkVTmp,
                                           &pCtxt->DNvcTmp);
                    pFrameRates->MaxFrameRate = pCtxt->ClkVTmp.MaxFrameRate;
                    pFrameRates->MinFrameRate = pCtxt->ClkVTmp.MinFrameRate;
                }
            }
            NV_DEBUG_PRINTF(("%s: SensorFrameRateLimitsAtResolution %dx%d"
                             " min=%f max=%f return=%x\n", __func__,
                             pFrameRates->Resolution.width,
                             pFrameRates->Resolution.height,
                             pFrameRates->MinFrameRate,
                             pFrameRates->MaxFrameRate, Status));
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;
        }

        case NvOdmImagerParameter_SensorInherentGainAtResolution:
        {
            NvOdmImagerInherentGainAtResolution *pInherentGain;

            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                sizeof(NvOdmImagerFrameRateLimitAtResolution));


            pInherentGain = (NvOdmImagerInherentGainAtResolution *)
                            param->p_value;
            if ((pInherentGain->Resolution.width ==
                                        pCtxt->Mode.ActiveDimensions.width &&
                                        pInherentGain->Resolution.height ==
                                        pCtxt->Mode.ActiveDimensions.height) ||
                                        (!pInherentGain->Resolution.width &&
                                         !pInherentGain->Resolution.height))
            {
                pInherentGain->InherentGain = pCtxt->InherentGain;
                pInherentGain->SupportsBinningControl =
                                            pCtxt->DNvc.SupportsBinningControl;
                pInherentGain->BinningControlEnabled =
                                             pCtxt->BinningControlEnabled;
            }
            else
            {
                Status = NvcImager_ModeNvcRead(pCtxt,
                                              pInherentGain->Resolution.width,
                                              pInherentGain->Resolution.height,
                                              NULL,
                                              &pCtxt->DNvcTmp);
                if (Status)
                {
                    pInherentGain->InherentGain = pCtxt->DNvcTmp.InherentGain;
                    pInherentGain->SupportsBinningControl =
                                         pCtxt->DNvcTmp.SupportsBinningControl;
                    pInherentGain->BinningControlEnabled = NV_FALSE;
                }
            }
            NV_DEBUG_PRINTF(("%s: SensorInherentGainAtResolution %dx%d=%f"
                             " BinCtrl=%x BinEn=%x return=%x\n", __func__,
                             pInherentGain->Resolution.width,
                             pInherentGain->Resolution.height,
                             pInherentGain->InherentGain,
                             pInherentGain->SupportsBinningControl,
                             pInherentGain->BinningControlEnabled, Status));
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;
        }

        case NvOdmImagerParameter_RegionUsedByCurrentResolution:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvOdmImagerRegion));
            {
                NvOdmImagerRegion *pRegion = (NvOdmImagerRegion*)
                                             param->p_value;

                pRegion->RegionStart.x = pCtxt->DNvc.RegionStartX;
                pRegion->RegionStart.y = pCtxt->DNvc.RegionStartY;
                pRegion->xScale = pCtxt->DNvc.xScale;
                pRegion->yScale = pCtxt->DNvc.yScale;
            }
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_SensorExposureLatchTime:
            // FIXME: implement this
            NV_DEBUG_PRINTF(("%s: SensorExposureLatchTime\n", __func__));
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_DeviceStatus:
            param->param = NVC_PARAM_STS;
            break;

        case NvOdmImagerParameter_StereoCameraMode:
            param->param = NVC_PARAM_STEREO;
            break;

        case NvOdmImagerParameter_StereoCapable:
            NV_DEBUG_PRINTF(("%s: StereoCapable=%d\n",
                             __func__, pCtxt->SNvc.StereoCapable));
            if (pCtxt->SNvc.StereoCapable)
            {
                *pBL = NV_TRUE;
                return NvcParamRetExitTrue;
            }
            else
                return NvcParamRetExitFalse;

        case NvOdmImagerParameter_SensorResChangeWaitTime:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvU32));
            if (pCtxt->SNvc.SensorResChangeWaitTime)
            {
                 *pNvU32Val = pCtxt->SNvc.SensorResChangeWaitTime;
                 return NvcParamRetExitTrue;
            }

            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_SensorIspSupport:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvBool));
            if (pCtxt->SNvc.SupportIsp)
            {
                *(NvBool*)param->p_value = NV_TRUE;
                return NvcParamRetExitTrue;
            }
            else
                return NvcParamRetExitFalse;

        case NvOdmImagerParameter_BracketCaps:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                      sizeof(NvOdmImagerBracketConfiguration));
            if (pCtxt->DNvc.BracketCaps)
            {
                NvOdmImagerBracketConfiguration *pData;

                pData = (NvOdmImagerBracketConfiguration *)param->p_value;
                pData->FlushCount = pCtxt->DNvc.FlushCount;
                pData->InitialIntraFrameSkip =
                                             pCtxt->DNvc.InitialIntraFrameSkip;
                pData->SteadyStateIntraFrameSkip =
                                         pCtxt->DNvc.SteadyStateIntraFrameSkip;
                pData->SteadyStateFrameNumer =
                                            pCtxt->DNvc.SteadyStateFrameNumber;
                NV_DEBUG_PRINTF(("%s: BracketCaps: FlushCount=%u "
                                 "InitialIntraFrameSkip=%u "
                                 "SteadyStateIntraFrameSkip=%u "
                                 "SteadyStateFrameNumber=%u\n",
                                 __func__, pCtxt->DNvc.FlushCount,
                                 pCtxt->DNvc.InitialIntraFrameSkip,
                                 pCtxt->DNvc.SteadyStateIntraFrameSkip,
                                 pCtxt->DNvc.SteadyStateFrameNumber));
                return NvcParamRetExitTrue;
            }

            NV_DEBUG_PRINTF(("%s: BracketCaps=FALSE\n", __func__));
            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_FocalLength:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            if (pCtxt->SNvc.FocalLength)
            {
                *pNvF32Val = pCtxt->SNvc.FocalLength;
                NV_DEBUG_PRINTF(("%s: FocalLength=%f\n",
                                 __func__, *pNvF32Val));
                return NvcParamRetExitTrue;
            }

            param->param = NVC_PARAM_FOCAL_LEN;
            break;

        case NvOdmImagerParameter_MaxAperture:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            if (pCtxt->SNvc.MaxAperture)
            {
                *pNvF32Val = pCtxt->SNvc.MaxAperture;
                NV_DEBUG_PRINTF(("%s: MaxAperture=%f\n",
                                 __func__, *pNvF32Val));
                return NV_TRUE;
            }

            param->param = NVC_PARAM_MAX_APERTURE;
            break;

        case NvOdmImagerParameter_FNumber:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            if (pCtxt->SNvc.FNumber)
            {
                *pNvF32Val = pCtxt->SNvc.FNumber;
                NV_DEBUG_PRINTF(("%s: FNumber=%f\n",
                                 __func__, *pNvF32Val));
                return NV_TRUE;
            }

            param->param = NVC_PARAM_FNUMBER;
            break;

        case NvOdmImagerParameter_HorizontalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            if (pCtxt->SNvc.HorizontalViewAngle)
            {
                *pNvF32Val = pCtxt->SNvc.HorizontalViewAngle;
                NV_DEBUG_PRINTF(("%s: HorizontalViewAngle=%f\n",
                                 __func__, *pNvF32Val));
                return NV_TRUE;
            }

            param->param = NVC_PARAM_VIEW_ANGLE_H;
            break;

        case NvOdmImagerParameter_VerticalViewAngle:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvF32));
            if (pCtxt->SNvc.VerticalViewAngle)
            {
                *pNvF32Val = pCtxt->SNvc.VerticalViewAngle;
                NV_DEBUG_PRINTF(("%s: VerticalViewAngle=%f\n",
                                 __func__, *pNvF32Val));
                return NV_TRUE;
            }

            param->param = NVC_PARAM_VIEW_ANGLE_V;
            break;

        case NvOdmImagerParameter_SensorType:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                             sizeof(NvOdmImagerSensorType));
            if (pCtxt->SNvc.SensorType)
            {
                *pNvU32Val = pCtxt->SNvc.SensorType;
                NV_DEBUG_PRINTF(("%s: SensorType=%d\n",
                                 __func__, *pNvU32Val));
                return NV_TRUE;
            }

            param->param = NVC_PARAM_SENSOR_TYPE;
            break;

        case NvOdmImagerParameter_CalibrationData:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                           sizeof(NvOdmImagerCalibrationData));
            NV_DEBUG_PRINTF(("%s: CalibrationData\n", __func__));
            {
                NvOdmImagerCalibrationData *pCalibration =
                                   (NvOdmImagerCalibrationData*)param->p_value;

                pCalibration->NeedsFreeing = NV_FALSE;
                pCalibration->CalibrationData = pCtxt->Hal.pCalibration;
            }
            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_FactoryCalibrationData:
            Status = LoadBlobFile(pCtxt->Hal.pFactoryBlobFiles,
                                  pCtxt->Hal.FactoryBlobFilesCount,
                                  (NvU8 *)param->p_value, param->sizeofvalue);
            NV_DEBUG_PRINTF(("%s: FactoryCalibrationData return=%x\n",
                             __func__, Status));
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_CalibrationOverrides:
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                           sizeof(NvOdmImagerCalibrationData));
            {
                NvOdmImagerCalibrationData *pCalibration =
                                   (NvOdmImagerCalibrationData*)param->p_value;

                if (pCtxt->Hal.pOverrideFiles && pCtxt->Hal.OverrideFilesCount)
                    pCalibration->CalibrationData =
                                   LoadOverridesFile(pCtxt->Hal.pOverrideFiles,
                                                pCtxt->Hal.OverrideFilesCount);
                else
                    pCalibration->CalibrationData = NULL;
                Status = (pCalibration->CalibrationData != NULL);
                pCalibration->NeedsFreeing = Status;
                NV_DEBUG_PRINTF(("%s: CalibrationOverrides return=%x\n",
                                 __func__, Status));
            }
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;

        case NvOdmImagerParameter_FuseID:
#if (BUILD_FOR_AOS == 0)
            CHECK_PARAM_SIZE_RETURN_MISMATCH(param->sizeofvalue,
                                           sizeof(struct nvc_fuseid));
            {
                struct nvc_fuseid fuse_id;
                int ret;
                NvOdmImagerPowerLevel PreviousPowerLevel;
                NvcImager_GetPowerLevel(hImager, &PreviousPowerLevel);
                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = NvcImager_SetPowerLevel(hImager, NvOdmImagerPowerLevel_On);
                    if (!ret)
                    {
                        NvOsDebugPrintf("failed to set power to ON. Aborting fuse ID read.\n");
                        Status = NV_FALSE;
                        break;
                    }
                }
                ret = ioctl(pCtxt->camera_fd, NVC_IOCTL_FUSE_ID, &fuse_id);
                if (ret < 0)
                {
                    NvOsDebugPrintf("ioctl to get sensor data failed %s\n", strerror(errno));
                    Status = NV_FALSE;
                }
                else
                {
                    NvOsMemset(param->p_value, 0, param->sizeofvalue);
                    NvOsMemcpy((struct nvc_fuseid *)(param->p_value),
                               & fuse_id, sizeof(struct nvc_fuseid));
                    Status = NV_TRUE;
                }
                if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
                {
                    ret = NvcImager_SetPowerLevel(hImager, PreviousPowerLevel);
                    if (!ret)
                        NvOsDebugPrintf("failed to set power to previous state after fuse id read.\n");
                }
            }
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;
#endif
            return NvcParamRetExitTrue;

        default:
            NV_DEBUG_PRINTF(("%s: Unknown parameter: %d\n",
                             __func__, param->param));
            return NvcParamRetContinueFalse;
    }

    return NvcParamRetContinueTrue;
}

static int
NvcImager_GetParamSwitchExit(
    NvOdmImagerHandle hImager,
    struct nvc_param *param,
    int err)
{
    NvcImagerContext *pCtxt = (NvcImagerContext *)
                              hImager->pSensor->pPrivateContext;
    NvF32 *pNvF32Val = (NvF32 *)param->p_value;
    NvU32 *pNvU32Val = (NvU32 *)param->p_value;
    NvBool Status = NV_TRUE;

    switch (param->param)
    {
        case NvOdmImagerParameter_StereoCameraMode:
            *pNvU32Val = pCtxt->StereoCameraMode;
            NV_DEBUG_PRINTF(("%s: StereoCameraMode=%u\n",
                             __func__, *pNvU32Val));
            if (err < 0)
                return NvcParamRetExitFalse;

            return NvcParamRetExitTrue;

        case NvOdmImagerParameter_DeviceStatus:
        {
            NvOdmImagerDeviceStatus *pStatus;
            SetModeParameters Parameters;
            pStatus = (NvOdmImagerDeviceStatus *)param->p_value;
            int status;

            if (err < 0)
            {
                NvOsDebugPrintf("%s: ioctl to gets status failed %s\n",
                                __func__, strerror(errno));
                return NvcParamRetExitFalse;
            }

            /* Assume this is status request due to a frame timeout */
            pCtxt->FrameErrorCount++;
            /* Multiple errors (in succession?) */
            if (pCtxt->FrameErrorCount > 4)
            {
                pCtxt->FrameErrorCount = 0;
                /* sensor has reset or locked up (ESD discharge?) */
                /* Hard reset the sensor and reconfigure it */
                NvOsDebugPrintf("%s Bad sensor state, reset & reconfigure\n",
                                __func__);
                status = 0; /* hard reset */
                param->param = NVC_PARAM_RESET;
                param->sizeofvalue = sizeof(status);
                param->p_value = &status;
                err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_WR, param);
                if (err < 0)
                    Status = NV_FALSE;
                Parameters.Resolution.width =
                                            pCtxt->Mode.ActiveDimensions.width;
                Parameters.Resolution.height =
                                           pCtxt->Mode.ActiveDimensions.height;
                Parameters.Exposure = pCtxt->Exposure;
                Parameters.Gains[0] = pCtxt->Gains[0];
                Parameters.Gains[1] = pCtxt->Gains[1];
                Parameters.Gains[2] = pCtxt->Gains[2];
                Parameters.Gains[3] = pCtxt->Gains[3];
                NvcImager_SetMode(hImager, &Parameters, NULL, &Parameters);
            }
            pStatus->Count = 1;
            if (Status)
                return NvcParamRetExitTrue;

            return NvcParamRetExitFalse;
        }

        case NvOdmImagerParameter_FocalLength:
        case NvOdmImagerParameter_MaxAperture:
        case NvOdmImagerParameter_FNumber:
        case NvOdmImagerParameter_HorizontalViewAngle:
        case NvOdmImagerParameter_VerticalViewAngle:
            if (!err)
            {
                if (!*pNvU32Val)
                    return NvcParamRetExitFalse;

                *pNvF32Val = (NvF32)(*(NvF32 *)pNvU32Val /
                                     NVC_IMAGER_INT2FLOAT_DIVISOR);
            }
            return NvcParamRetExitTrue;

        default:
            return NvcParamRetContinueFalse;
    }

    return NvcParamRetContinueTrue;
}

/**
 * NvcImager_GetParameter. Phase 1.
 * Get sensor specific parameters
 */
static NvBool
NvcImager_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
    NvcImagerContext *pCtxt;
    struct nvc_param param;
    int err;
    int Ret;

    if (!hImager || !hImager->pSensor || !hImager->pSensor->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor->pPrivateContext\n",
                         __func__);
        return NV_FALSE;
    }

    pCtxt = (NvcImagerContext *)hImager->pSensor->pPrivateContext;
    if (Param == NvOdmImagerParameter_ISPSetting)
    {
        NvOdmImagerISPSetting *pIsp;
        NvOdmImagerISPSetting Isp;

        CHECK_PARAM_SIZE_RETURN_MISMATCH(SizeOfValue,
                                         sizeof(NvOdmImagerISPSetting));

        pIsp = (NvOdmImagerISPSetting *)pValue;
        Isp.attribute = pIsp->attribute;
        Isp.pData = pIsp->pData;
        Isp.dataSize = pIsp->dataSize;
        /* See the NvcParamRet notes in imager_nvc.h */
        if (pCtxt->Hal.pfnParamIspRd != NULL)
        {
            Ret = pCtxt->Hal.pfnParamIspRd(hImager, &Isp);
            if (Ret == NvcParamRetContinueFalse)
                Ret = NvcImager_GetParamIspSwitch(hImager, &Isp);
        }
        else
        {
            Ret = NvcImager_GetParamIspSwitch(hImager, &Isp);
        }
        if (Ret == NvcParamRetExitTrue)
            return NV_TRUE;

        if ((Ret == NvcParamRetExitFalse) || (Ret == NvcParamRetContinueFalse))
            return NV_FALSE;

        /* NvcParamRetContinueTrue */
        err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_ISP_RD,
                    (struct nvc_param_isp *)&Isp);
        Isp.attribute = pIsp->attribute;
        /* See the NvcParamRet notes in imager_nvc.h */
        if (pCtxt->Hal.pfnParamIspWrExit != NULL)
        {
            Ret = pCtxt->Hal.pfnParamIspRdExit(hImager, &Isp, err);
            if (Ret == NvcParamRetExitTrue)
                return NV_TRUE;

            if (Ret == NvcParamRetExitFalse)
                return NV_FALSE;
        }

        Ret = NvcImager_GetParamIspSwitchExit(hImager, &Isp, err);
        if (Ret == NvcParamRetExitTrue)
            return NV_TRUE;

        if (Ret == NvcParamRetExitFalse)
            return NV_FALSE;

        if (err < 0)
        {
            NvOsDebugPrintf("%s: NVC_IOCTL_PARAM_ISP_RD (%d) failed: %s\n",
                            __func__, pIsp->attribute, strerror(errno));
            return NV_FALSE;
        }

        return NV_TRUE;
    }

    param.param = Param;
    param.sizeofvalue = SizeOfValue;
    param.p_value = (void *)pValue;
    /* See the NvcParamRet notes in imager_nvc.h */
    if (pCtxt->Hal.pfnParamRd != NULL)
    {
        Ret = pCtxt->Hal.pfnParamRd(hImager, &param);
        if (Ret == NvcParamRetContinueFalse)
            Ret = NvcImager_GetParamSwitch(hImager, &param);
    }
    else
    {
        Ret = NvcImager_GetParamSwitch(hImager, &param);
    }
    if (Ret == NvcParamRetExitTrue)
        return NV_TRUE;

    if ((Ret == NvcParamRetExitFalse) || (Ret == NvcParamRetContinueFalse))
        return NV_FALSE;

    /* NvcParamRetContinueTrue */
    err = ioctl(pCtxt->camera_fd, NVC_IOCTL_PARAM_RD, &param);
    param.param = Param;
    /* See the NvcParamRet notes in imager_nvc.h */
    if (pCtxt->Hal.pfnParamRdExit != NULL)
    {
        Ret = pCtxt->Hal.pfnParamRdExit(hImager, &param, err);
        if (Ret == NvcParamRetExitTrue)
            return NV_TRUE;

        if (Ret == NvcParamRetExitFalse)
            return NV_FALSE;
    }

    Ret = NvcImager_GetParamSwitchExit(hImager, &param, err);
    if (Ret == NvcParamRetExitTrue)
        return NV_TRUE;

    if (Ret == NvcParamRetExitFalse)
        return NV_FALSE;

    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl NVC_IOCTL_PARAM_WR (%d) failed: %s\n",
                        __func__, Param, strerror(errno));
        return NV_FALSE;
    }

    return NV_TRUE;
}

/**
 * NvcImager_GetHal
 * return the hal functions associated with sensor bayer
 */
NvBool NvcImager_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pSensor)
    {
        NvOsDebugPrintf("%s: No hImager->pSensor\n", __func__);
        return NV_FALSE;
    }

    hImager->pSensor->pfnOpen = NvcImager_Open;
    hImager->pSensor->pfnClose = NvcImager_Close;
    hImager->pSensor->pfnGetCapabilities = NvcImager_GetCapabilities;
    hImager->pSensor->pfnListModes = NvcImager_ListModes;
    hImager->pSensor->pfnSetMode = NvcImager_SetMode;
    hImager->pSensor->pfnSetPowerLevel = NvcImager_SetPowerLevel;
    hImager->pSensor->pfnGetPowerLevel = NvcImager_GetPowerLevel;
    hImager->pSensor->pfnSetParameter = NvcImager_SetParameter;
    hImager->pSensor->pfnGetParameter = NvcImager_GetParameter;
    return NV_TRUE;
}
#endif /* (BUILD_FOR_AOS == 0) */

