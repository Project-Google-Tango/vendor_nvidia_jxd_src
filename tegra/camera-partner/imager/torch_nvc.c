/*
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
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
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <asm/types.h>
#include <nvc.h>
#include <nvc_torch.h>
#endif //(BUILD_FOR_AOS == 0)

#include "torch_nvc.h"
#include "imager_util.h"
#include "nvodm_services.h"
#include "nvodm_imager_guid.h"

#define TORCH_OUTPUT_DELTA    0.001f

#define LEGACY_FLASH_CAP_SIZE   (sizeof(*FlashDrvrCaps) + sizeof(struct nvc_torch_level_info) \
	                            * NVODM_IMAGER_MAX_FLASH_LEVELS)
#define LEGACY_TORCHH_CAP_SIZE   (sizeof(*TorchDrvrCaps) + sizeof(__s32) * NVODM_IMAGER_MAX_FLASH_LEVELS)

#define EXT_FLASH_CAP_SIZE_V1   (sizeof(struct nvc_torch_flash_capabilities_v1) + \
                                 sizeof(struct nvc_torch_lumi_level_v1) * NVODM_IMAGER_MAX_FLASH_LEVELS)
#define EXT_FLASH_TIMER_CAP_SIZE_V1     (sizeof(struct nvc_torch_timer_capabilities_v1) + \
                                 sizeof(struct nvc_torch_timeout_v1) * NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS)
#define EXT_FLASH_SETTING_SIZE_V1 (EXT_FLASH_CAP_SIZE_V1 + EXT_FLASH_TIMER_CAP_SIZE_V1)

#define EXT_TORCH_CAP_SIZE_V1   (sizeof(struct nvc_torch_torch_capabilities_v1) + \
                                 sizeof(struct nvc_torch_lumi_level_v1) * NVODM_IMAGER_MAX_FLASH_LEVELS)
#define EXT_TORCH_TIMER_CAP_SIZE_V1     (sizeof(struct nvc_torch_timer_capabilities_v1) + \
                                 sizeof(struct nvc_torch_timeout_v1) * NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS)
#define EXT_TORCH_SETTING_SIZE_V1 (EXT_TORCH_CAP_SIZE_V1 + EXT_TORCH_TIMER_CAP_SIZE_V1)

#define GET_EXT_FLASH_KERNEL(p, i) \
        ((char *)(p)->pKernelCaps + EXT_FLASH_SETTING_SIZE_V1 * (i))
#define GET_EXT_TORCH_KERNEL(p, i) \
        ((char *)(p)->pKernelCaps + EXT_FLASH_SETTING_SIZE_V1 * (p)->Query.NumberOfFlash + \
        EXT_TORCH_SETTING_SIZE_V1 * (i))

#if (BUILD_FOR_AOS == 0)
typedef struct TorchNvcContextRec
{
    int torch_fd; /* file handle to the kernel nvc_torch driver */
    NvOdmImagerFlashTorchQuery   Query;
    NvOdmImagerFlashCapabilities FlashCaps[2];
    NvOdmImagerTorchCapabilities TorchCaps[2];
    NvU8 CapVersion;
    struct nvc_torch_capability_query k_query;
    void *pKernelCaps;
} TorchNvcContext;
#endif

static NvBool
TorchNvc_Open(
    NvOdmImagerHandle hImager);

static void
TorchNvc_Close(
    NvOdmImagerHandle hImager);

static void
TorchNvc_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities);

static NvBool
TorchNvc_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel);

static NvBool
TorchNvc_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue);

static NvBool
TorchNvc_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue);

#if (BUILD_FOR_AOS == 0)
static void TransferLegacyFlashCapabilities(
    struct nvc_torch_flash_capabilities *pFlashCaps_K, NvOdmImagerFlashCapabilities *pFlashCaps_Odm)
{
    unsigned i;

    NvOdmOsMemset((void *)pFlashCaps_Odm, 0, sizeof(NvOdmImagerFlashCapabilities));

    pFlashCaps_Odm->NumberOfLevels = pFlashCaps_K->numberoflevels;
    if (pFlashCaps_Odm->NumberOfLevels > NVODM_IMAGER_MAX_FLASH_LEVELS)
    {
        pFlashCaps_Odm->NumberOfLevels = NVODM_IMAGER_MAX_FLASH_LEVELS;
    }

    NV_DEBUG_PRINTF(("%s %d\n", __func__, pFlashCaps_Odm->NumberOfLevels));
    for (i = 0; i < pFlashCaps_Odm->NumberOfLevels; i++)
    {
        pFlashCaps_Odm->levels[i].guideNum =
                        (NvF32)pFlashCaps_K->levels[i].guidenum;
        pFlashCaps_Odm->levels[i].sustainTime =
                        pFlashCaps_K->levels[i].sustaintime;
        pFlashCaps_Odm->levels[i].rechargeFactor =
                        (NvF32)pFlashCaps_K->levels[i].rechargefactor;
        NV_DEBUG_PRINTF(("%d: %f %d\n", i, pFlashCaps_Odm->levels[i].guideNum, pFlashCaps_Odm->levels[i].sustainTime));
    }
}

static void TransferLegacyTorchCapabilities(
    struct nvc_torch_torch_capabilities *pTorchCaps_K, NvOdmImagerTorchCapabilities *pTorchCaps_Odm)
{
    unsigned i;

    NvOdmOsMemset((void *)pTorchCaps_Odm, 0, sizeof(NvOdmImagerTorchCapabilities));

    pTorchCaps_Odm->NumberOfLevels = pTorchCaps_K->numberoflevels;
    if (pTorchCaps_Odm->NumberOfLevels > NVODM_IMAGER_MAX_TORCH_LEVELS)
    {
        pTorchCaps_Odm->NumberOfLevels = NVODM_IMAGER_MAX_TORCH_LEVELS;
    }

    NV_DEBUG_PRINTF(("%s %d\n", __func__, pTorchCaps_Odm->NumberOfLevels));
    for (i = 0; i < pTorchCaps_Odm->NumberOfLevels; i++)
    {
        pTorchCaps_Odm->guideNum[i] = (NvF32)pTorchCaps_K->guidenum[i];
        NV_DEBUG_PRINTF(("%d: %f\n", i, pTorchCaps_Odm->guideNum[i]));
    }
}

static NvBool Legacy_CapabilityQuery(TorchNvcContext *pCtxt)
{
    struct nvc_param params;
    struct nvc_torch_flash_capabilities *FlashDrvrCaps;
    struct nvc_torch_torch_capabilities *TorchDrvrCaps;
    int err;

    NV_DEBUG_PRINTF(("%s ++\n", __func__));
    FlashDrvrCaps = NvOdmOsAlloc(LEGACY_FLASH_CAP_SIZE + LEGACY_TORCHH_CAP_SIZE);
    pCtxt->pKernelCaps = FlashDrvrCaps;
    if (!FlashDrvrCaps)
    {
        NvOsDebugPrintf("%s err: NvOdmOsAlloc\n", __func__);
        return NV_FALSE;
    }
    NvOdmOsMemset((void *)FlashDrvrCaps, 0, LEGACY_FLASH_CAP_SIZE + LEGACY_TORCHH_CAP_SIZE);

    params.param = NVC_PARAM_FLASH_CAPS;
    params.sizeofvalue = LEGACY_FLASH_CAP_SIZE;
    params.p_value = FlashDrvrCaps;
    err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to get flash caps failed: %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    TorchDrvrCaps = (void *)((char *)FlashDrvrCaps + LEGACY_FLASH_CAP_SIZE);
    params.param = NVC_PARAM_TORCH_CAPS;
    params.sizeofvalue = LEGACY_TORCHH_CAP_SIZE;
    params.p_value = TorchDrvrCaps;
    err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to get torch caps failed: %s\n",
                        __func__, strerror(errno));
        return NV_FALSE;
    }

    pCtxt->Query.NumberOfFlash = 1;
    pCtxt->Query.NumberOfTorch = 1;
    pCtxt->CapVersion = NVC_TORCH_CAPABILITY_LEGACY;
    /* populate the flash capabilities converting int to float */
    TransferLegacyFlashCapabilities(FlashDrvrCaps, pCtxt->FlashCaps);
    /* populate the torch capabilities converting int to float */
    TransferLegacyTorchCapabilities(TorchDrvrCaps, pCtxt->TorchCaps);

    return NV_TRUE;
}

static NvBool GetExtendedFlashCap_Ver1(
    TorchNvcContext *pCtxt,
    struct nvc_torch_flash_capabilities_v1 *pFCap,
    struct nvc_torch_timer_capabilities_v1 *pFlashTmr)
{
    NvOdmImagerFlashCapabilities *pFlashCaps_Odm = &pCtxt->FlashCaps[pFCap->led_idx];
    struct nvc_torch_timeout_v1 *pftm = pFlashTmr->timeouts;
    unsigned i;

    NvOdmOsMemset((void *)pFlashCaps_Odm, 0, sizeof(NvOdmImagerFlashCapabilities));

    pFlashCaps_Odm->LedIndex = pFCap->led_idx;
    pFlashCaps_Odm->attribute = pFCap->attribute;
    pFlashCaps_Odm->NumberOfTimers = pFlashTmr->timeout_num;
    if (pFlashCaps_Odm->NumberOfTimers > NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS)
    {
        pFlashCaps_Odm->NumberOfTimers = NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS;
    }
    pFlashCaps_Odm->NumberOfLevels = pFCap->numberoflevels;
    if (pFlashCaps_Odm->NumberOfLevels > NVODM_IMAGER_MAX_FLASH_LEVELS)
    {
        pFlashCaps_Odm->NumberOfLevels = NVODM_IMAGER_MAX_FLASH_LEVELS;
    }

    NV_DEBUG_PRINTF(("%s Flash #%d, levels %d, timers %d:\n", __func__,
        pFlashCaps_Odm->LedIndex, pFlashCaps_Odm->NumberOfLevels, pFlashCaps_Odm->NumberOfTimers));
    NV_DEBUG_PRINTF(("time outs:\n"));
    for (i = 0; i < pFlashCaps_Odm->NumberOfTimers; i++)
    {
        pFlashCaps_Odm->timeout_ms[i] = pftm[i].timeout;
        NV_DEBUG_PRINTF(("%d: %d\n", i, pFlashCaps_Odm->timeout_ms[i]));
    }

    NV_DEBUG_PRINTF(("flash levels:\n"));
    for (i = 0; i < pFlashCaps_Odm->NumberOfLevels; i++)
    {
        pFlashCaps_Odm->levels[i].guideNum =
                        (NvF32)pFCap->levels[i].luminance / pFCap->granularity;
        pFlashCaps_Odm->levels[i].sustainTime = pFlashCaps_Odm->timeout_ms[0];
        pFlashCaps_Odm->levels[i].rechargeFactor = 0;
        NV_DEBUG_PRINTF(("%d: %u / %u = %f, %d\n",
                i, pFCap->levels[i].luminance, pFCap->granularity,
                pFlashCaps_Odm->levels[i].guideNum, pFCap->levels[i].guidenum));
    }

    return NV_TRUE;
}

static NvBool GetExtendedTorchCap_Ver1(
    TorchNvcContext *pCtxt,
    struct nvc_torch_torch_capabilities_v1 *pTCap,
    struct nvc_torch_timer_capabilities_v1 *pTorchTmr)
{
    NvOdmImagerTorchCapabilities *pTorchCaps_Odm = &pCtxt->TorchCaps[pTCap->led_idx];
    struct nvc_torch_timeout_v1 *pftm = NULL;
    unsigned i;

    NvOdmOsMemset((void *)pTorchCaps_Odm, 0, sizeof(NvOdmImagerTorchCapabilities));

    pTorchCaps_Odm->LedIndex = pTCap->led_idx;
    pTorchCaps_Odm->attribute = pTCap->attribute;
    pTorchCaps_Odm->NumberOfTimers = pTorchTmr->timeout_num;
    if (pTorchCaps_Odm->NumberOfTimers > NVODM_IMAGER_MAX_TORCH_TIMER_LEVELS)
    {
        pTorchCaps_Odm->NumberOfTimers = NVODM_IMAGER_MAX_TORCH_TIMER_LEVELS;
    }
    pTorchCaps_Odm->NumberOfLevels = pTCap->numberoflevels;
    if (pTorchCaps_Odm->NumberOfLevels > NVODM_IMAGER_MAX_TORCH_LEVELS)
    {
        pTorchCaps_Odm->NumberOfLevels = NVODM_IMAGER_MAX_TORCH_LEVELS;
    }

    NV_DEBUG_PRINTF(("%s Torch #%d, levels %d, timers %d:\n",
	__func__, pTorchCaps_Odm->LedIndex,
	pTorchCaps_Odm->NumberOfLevels, pTorchCaps_Odm->NumberOfTimers));

    NV_DEBUG_PRINTF(("time outs:\n"));
    pftm = pTorchTmr->timeouts;
    for (i = 0; i < pTorchCaps_Odm->NumberOfTimers; i++)
    {
        pTorchCaps_Odm->timeout_ms[i] = pftm[i].timeout / 1000;
        NV_DEBUG_PRINTF(("%d: %d mS\n", i, pTorchCaps_Odm->timeout_ms[i]));
    }

    NV_DEBUG_PRINTF(("torch levels:\n"));
    for (i = 0; i < pTorchCaps_Odm->NumberOfLevels; i++)
    {
        pTorchCaps_Odm->guideNum[i] = (NvF32)pTCap->levels[i].luminance / pTCap->granularity;
        NV_DEBUG_PRINTF(("%d: %f - %d\n", i, pTorchCaps_Odm->guideNum[i], pTCap->levels[i].guidenum));
    }

    return NV_TRUE;
}

static NvBool Extended_CapabilityQuery(TorchNvcContext *pCtxt)
{
    struct nvc_param params;
    struct nvc_torch_capability_query *pqry = &pCtxt->k_query;
    void *buffer = NULL;
    NvU32 buf_size = 0;
    NvU32 idx;
    int err;

    NV_DEBUG_PRINTF(("%s ++\n", __func__));
    NvOdmOsMemset(pqry, 0, sizeof(*pqry));
    params.param = NVC_PARAM_TORCH_QUERY;
    params.sizeofvalue = sizeof(*pqry);
    params.p_value = pqry;
    err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: not supported.\n", __func__);
        return NV_FALSE;
    }

    pCtxt->CapVersion = pqry->version;
    if (pqry->version == NVC_TORCH_CAPABILITY_VER_1)
    {
        struct nvc_torch_flash_capabilities_v1 *pFCap;
        struct nvc_torch_torch_capabilities_v1 *pTCap;
        struct nvc_torch_timer_capabilities_v1 *pFlashTmr;
        struct nvc_torch_timer_capabilities_v1 *pTorchTmr;

        if (pqry->flash_num > 1 && !(pqry->led_attr & NVC_TORCH_LED_ATTR_FLASH_SYNC))
        {
            pCtxt->Query.NumberOfFlash = 2;
        }
        else
        {
            pCtxt->Query.NumberOfFlash = 1;
        }
        if (pqry->torch_num > 1 && !(pqry->led_attr & NVC_TORCH_LED_ATTR_TORCH_SYNC))
        {
            pCtxt->Query.NumberOfTorch = 2;
        }
        else
        {
            pCtxt->Query.NumberOfTorch = 1;
        }
        NV_DEBUG_PRINTF(("%s Flash #%d, Torch #%d\n",
		__func__, pCtxt->Query.NumberOfFlash, pCtxt->Query.NumberOfTorch));

        buf_size = EXT_FLASH_SETTING_SIZE_V1 * pCtxt->Query.NumberOfFlash +
                    EXT_TORCH_SETTING_SIZE_V1 * pCtxt->Query.NumberOfTorch;
        buffer = NvOdmOsAlloc(buf_size);
        pCtxt->pKernelCaps = buffer;
        if (!buffer)
        {
            NvOsDebugPrintf("%s err: NvOdmOsAlloc\n", __func__);
            return NV_FALSE;
        }
        NvOdmOsMemset(buffer, 0, buf_size);

        for (idx = 0; idx < pCtxt->Query.NumberOfFlash; idx++)
        {
            pFCap = (void *)((char *)buffer + EXT_FLASH_SETTING_SIZE_V1 * idx);

            params.variant = idx;
            params.param = NVC_PARAM_FLASH_EXT_CAPS;
            params.sizeofvalue = EXT_FLASH_SETTING_SIZE_V1;
            params.p_value = pFCap;
            err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_RD, &params);
            if (err < 0)
            {
                NvOsDebugPrintf("%s: ioctl to get flash caps failed: %s\n",
                                __func__, strerror(errno));
                return NV_FALSE;
            }
            pFlashTmr = (void *)((char *)pFCap + pFCap->timeout_off);
            GetExtendedFlashCap_Ver1(pCtxt, pFCap, pFlashTmr);
        }

        pTCap = (void *)((char *)buffer + EXT_FLASH_SETTING_SIZE_V1 * pCtxt->Query.NumberOfFlash);
        for (idx = 0; idx < pCtxt->Query.NumberOfTorch; idx++)
        {
            pTCap = (void *)((char *)pTCap + EXT_TORCH_SETTING_SIZE_V1 * idx);

            params.variant = idx;
            params.param = NVC_PARAM_TORCH_EXT_CAPS;
            params.sizeofvalue = EXT_TORCH_SETTING_SIZE_V1;
            params.p_value = pTCap;
            err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_RD, &params);
            if (err < 0)
            {
                NvOsDebugPrintf("%s: ioctl to get torch caps failed: %s\n",
                                __func__, strerror(errno));
                return NV_FALSE;
            }
            pTorchTmr = (void *)((char *)pTCap + pTCap->timeout_off);
            GetExtendedTorchCap_Ver1(pCtxt, pTCap, pTorchTmr);
        }
    }
    else
    {
        NvOsDebugPrintf("%s: unsupported cap version %d.\n", __func__, pqry->version);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvU32 TorchNvc_SetLedLevels_V1(
        TorchNvcContext *pCtxt,
        NvOdmImagerFlashSetLevel *pLevels,
        struct nvc_torch_set_level_v1 *pled_params,
        NvBool IsFlash)
{
    NvOdmImagerFlashCapabilities *pFlashOdmCaps;
    NvOdmImagerTorchCapabilities *pTorchOdmCaps;
    struct nvc_torch_flash_capabilities_v1 *pFCap;
    struct nvc_torch_torch_capabilities_v1 *pTCap;
    NvU16 ledmask;
    NvU32 ledidx, index;
    NvBool bLedOff = NV_TRUE;

    NvOdmOsMemset(pled_params, 0, sizeof(*pled_params));

    NV_DEBUG_PRINTF(("%s %s: mask %d, levels %f %f, timer %d:\n",
	__func__, IsFlash ? "Flash" : "Torch", pLevels->ledmask,
	pLevels->value[0], pLevels->value[1], pLevels->timeout));

    pled_params->timeout = pLevels->timeout;
    pled_params->ledmask = pLevels->ledmask & 0x03;  // only support 2 leds

    if (pled_params->ledmask & 0x02)
    {
        if ((IsFlash && (pCtxt->Query.NumberOfFlash == 1)) ||
            (!IsFlash && (pCtxt->Query.NumberOfTorch == 1)))
        {
            pled_params->ledmask = 1;
        }
    }

    ledmask = pled_params->ledmask;
    ledidx = 0;
    if (IsFlash)
    {
        while (ledmask)
        {
            pFlashOdmCaps = &pCtxt->FlashCaps[ledidx];
            for (index = 0; index < pFlashOdmCaps->NumberOfLevels; index++)
            {
                NV_DEBUG_PRINTF(("#%d - %d = %f:\n", ledidx, index, pFlashOdmCaps->levels[index].guideNum));
                if (pLevels->value[ledidx] <= pFlashOdmCaps->levels[index].guideNum + TORCH_OUTPUT_DELTA)
                    break;
            }

            pFCap = (void *)GET_EXT_FLASH_KERNEL(pCtxt, ledidx);
            if (index >= pFCap->numberoflevels)
            {
                index = pFCap->numberoflevels - 1;
            }
            pled_params->levels[ledidx] = pFCap->levels[index].guidenum;
            bLedOff = NV_FALSE;

            ledmask >>= 1;
            ledidx++;
        }
    }
    else
    {
        while (ledmask)
        {
            pTorchOdmCaps = &pCtxt->TorchCaps[ledidx];
            for (index = 0; index < pTorchOdmCaps->NumberOfLevels; index++)
            {
                NV_DEBUG_PRINTF(("#%d - %d = %f:\n", ledidx, index, pTorchOdmCaps->guideNum[index]));
                if (pLevels->value[ledidx] <= pTorchOdmCaps->guideNum[index] + TORCH_OUTPUT_DELTA)
                    break;
            }

            pTCap = (void *)GET_EXT_TORCH_KERNEL(pCtxt, ledidx);
            if (index >= pTCap->numberoflevels)
            {
                index = pTCap->numberoflevels - 1;
            }
            pled_params->levels[ledidx] = pTCap->levels[index].guidenum;
            bLedOff = NV_FALSE;

            ledmask >>= 1;
            ledidx++;
        }
    }

    /* when ledmask equals 0, kernel will turn off leds */
    if (bLedOff)
    {
        pled_params->ledmask = 0;
    }

    NV_DEBUG_PRINTF(("return: mask %d, levels %d %d, timeout %d:\n",
	pled_params->ledmask, pled_params->levels[0], pled_params->levels[1], pled_params->timeout));
    return 0;
}
#endif

static NvBool TorchNvc_Open(NvOdmImagerHandle hImager)
{
#if (BUILD_FOR_AOS == 0)
    TorchNvcContext *pCtxt = NULL;
    unsigned Instance;
    char DevName[NVODMIMAGER_IDENTIFIER_MAX];

    if (!hImager || !hImager->pFlash)
    {
        NvOsDebugPrintf("%s err: No hImager->pFlash\n", __func__);
        return NV_FALSE;
    }

    pCtxt = (TorchNvcContext *)NvOdmOsAlloc(sizeof(*pCtxt));
    if (!pCtxt)
    {
        NvOsDebugPrintf("%s err: NvOdmOsAlloc\n", __func__);
        return NV_FALSE;
    }
    NvOdmOsMemset(pCtxt, 0, sizeof(*pCtxt));

    hImager->pFlash->pPrivateContext = pCtxt;
    Instance = (unsigned)(hImager->pFlash->GUID & 0xF);
    if (Instance)
        sprintf(DevName, "/dev/torch.%u", Instance);
    else
        sprintf(DevName, "/dev/torch");
#ifdef O_CLOEXEC
    pCtxt->torch_fd = open(DevName, O_RDWR|O_CLOEXEC);
#else
    pCtxt->torch_fd = open(DevName, O_RDWR);
#endif
    if (pCtxt->torch_fd < 0)
    {
        NvOsDebugPrintf("%s err: cannot open nvc torch driver: %s\n",
                        __func__, strerror(errno));
        TorchNvc_Close(hImager);
        return NV_FALSE;
    }
    NV_DEBUG_PRINTF(("%s: torch_fd opened as: %d\n", __func__, pCtxt->torch_fd));

    if (Extended_CapabilityQuery(pCtxt) == NV_FALSE &&
	Legacy_CapabilityQuery(pCtxt) == NV_FALSE)
    {
        TorchNvc_Close(hImager);
        return NV_FALSE;
    }

#endif //(BUILD_FOR_AOS == 0)
    return NV_TRUE;
}

static void TorchNvc_Close(NvOdmImagerHandle hImager)
{
#if (BUILD_FOR_AOS == 0)
    TorchNvcContext *pCtxt = NULL;

    if (!hImager || !hImager->pFlash || !hImager->pFlash->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFlash->pPrivateContext\n",
                        __func__);
        return;
    }

    pCtxt = (TorchNvcContext *)hImager->pFlash->pPrivateContext;
    close(pCtxt->torch_fd);
    if (pCtxt->pKernelCaps)
    {
        NvOdmOsFree(pCtxt->pKernelCaps);
    }
    NvOdmOsFree(pCtxt);
    hImager->pFlash->pPrivateContext = NULL;
    NV_DEBUG_PRINTF(("%s --\n", __func__));
#endif //(BUILD_FOR_AOS == 0)
}

static void
TorchNvc_GetCapabilities(
    NvOdmImagerHandle hImager,
    NvOdmImagerCapabilities *pCapabilities)
{
}

static NvBool
TorchNvc_SetPowerLevel(
    NvOdmImagerHandle hImager,
    NvOdmImagerPowerLevel PowerLevel)
{
#if (BUILD_FOR_AOS == 0)
    int err;

    if (!hImager || !hImager->pFlash || !hImager->pFlash->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFlash->pPrivateContext\n",
                        __func__);
        return NV_FALSE;
    }

    NV_DEBUG_PRINTF(("%s %d\n", __func__, PowerLevel));
    TorchNvcContext *pCtxt =
        (TorchNvcContext *)hImager->pFlash->pPrivateContext;
    err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PWR_WR, PowerLevel);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to set power level (%d) failed: %s\n",
                        __func__, PowerLevel, strerror(errno));
        return NV_FALSE;
    }
#endif
    return NV_TRUE;
}

static NvBool
TorchNvc_SetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    const void *pValue)
{
#if (BUILD_FOR_AOS == 0)
    TorchNvcContext *pCtxt = NULL;
    struct nvc_param params;
    NvOdmImagerFlashSetLevel *flashLevel = (NvOdmImagerFlashSetLevel*)pValue;
    struct nvc_torch_set_level_v1 led_params;
    int err;

    if (!hImager || !hImager->pFlash || !hImager->pFlash->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFlash->pPrivateContext\n",
                        __func__);
        return NV_FALSE;
    }

    pCtxt = (TorchNvcContext *)hImager->pFlash->pPrivateContext;

    switch (Param)
    {
        case NvOdmImagerParameter_FlashLevel:
            if (pCtxt->CapVersion == NVC_TORCH_CAPABILITY_VER_1)
            {
                TorchNvc_SetLedLevels_V1(pCtxt, flashLevel, &led_params, NV_TRUE);
                params.sizeofvalue = sizeof(led_params);
                params.p_value = &led_params;
            }
            else
            {
                NvU32 DesiredLevel;
                for (DesiredLevel = 0;
                     DesiredLevel < pCtxt->FlashCaps[0].NumberOfLevels;
                     DesiredLevel++)
                {
                    if (flashLevel->value[0] == pCtxt->FlashCaps[0].levels[DesiredLevel].guideNum)
                        break;
                }
                params.sizeofvalue = sizeof(DesiredLevel);
                params.p_value = &DesiredLevel;
                NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_FlashLevel %f - %d\n", __func__, flashLevel->value[0], DesiredLevel));
            }
            params.param = NVC_PARAM_FLASH_LEVEL;
            break;

        case NvOdmImagerParameter_TorchLevel:
            if (pCtxt->CapVersion == NVC_TORCH_CAPABILITY_VER_1)
            {
                TorchNvc_SetLedLevels_V1(pCtxt, flashLevel, &led_params, NV_FALSE);
                params.sizeofvalue = sizeof(led_params);
                params.p_value = &led_params;
            }
            else
            {
                NvU32 DesiredLevel;
                for (DesiredLevel = 0;
                    DesiredLevel < pCtxt->TorchCaps[0].NumberOfLevels;
                     DesiredLevel++)
                {
                    if (flashLevel->value[0] == pCtxt->TorchCaps[0].guideNum[DesiredLevel])
                        break;
                }
                params.sizeofvalue = sizeof(DesiredLevel);
                params.p_value = &DesiredLevel;
                NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_TorchLevel %f - %d\n", __func__, flashLevel->value[0], DesiredLevel));
            }
            params.param = NVC_PARAM_TORCH_LEVEL;
            break;

        case NvOdmImagerParameter_FlashPinState:
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_FlashPinState\n", __func__));
            params.param = NVC_PARAM_FLASH_PIN_STATE;
            params.p_value = &pValue;
            params.sizeofvalue = (__u32)SizeOfValue;
            break;

        default:
            NvOsDebugPrintf("%s: unsupported parameter: %d\n", __func__, Param);
            return NV_FALSE;
    }
    err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_WR, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to set parameter %x failed: %s\n",
                        __func__, Param, strerror(errno));
        return NV_FALSE;
    }
#endif //(BUILD_FOR_AOS == 0)
    return NV_TRUE;
}

static NvBool
TorchNvc_GetParameter(
    NvOdmImagerHandle hImager,
    NvOdmImagerParameter Param,
    NvS32 SizeOfValue,
    void *pValue)
{
#if (BUILD_FOR_AOS == 0)
    TorchNvcContext *pCtxt = NULL;
    struct nvc_param params;
    NvU32 LedIdx;
    int err;

    if (!hImager || !hImager->pFlash || !hImager->pFlash->pPrivateContext)
    {
        NvOsDebugPrintf("%s: No hImager->pFlash->pPrivateContext\n",
                        __func__);
        return NV_FALSE;
    }

    pCtxt = (TorchNvcContext *)hImager->pFlash->pPrivateContext;

    switch(Param)
    {
        case NvOdmImagerParameter_FlashTorchQuery:
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_FlashTorchQuery\n", __func__));
            NvOdmOsMemcpy(pValue, &pCtxt->Query, sizeof(pCtxt->Query));
            return NV_TRUE;

        case NvOdmImagerParameter_FlashCapabilities:
            LedIdx = ((NvOdmImagerFlashCapabilities*)pValue)->LedIndex;
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_FlashCapabilities %d\n", __func__, LedIdx));
            NvOdmOsMemcpy(pValue, &pCtxt->FlashCaps[LedIdx], sizeof(pCtxt->FlashCaps[0]));
            return NV_TRUE;

        case NvOdmImagerParameter_TorchCapabilities:
            LedIdx = ((NvOdmImagerTorchCapabilities*)pValue)->LedIndex;
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_TorchCapabilities %d\n", __func__, LedIdx));
            NvOdmOsMemcpy(pValue, &pCtxt->TorchCaps[LedIdx], sizeof(pCtxt->TorchCaps[0]));
            return NV_TRUE;

        case NvOdmImagerParameter_FlashPinState:
            NV_DEBUG_PRINTF(("%s NvOdmImagerParameter_FlashPinState\n", __func__));
            params.param = NVC_PARAM_FLASH_PIN_STATE;
            params.sizeofvalue = (__u32)SizeOfValue;
            params.p_value = pValue;
            break;

        default:
            NvOsDebugPrintf("%s: Unsupported Param %x\n", __func__, Param);
            return NV_FALSE;
    }
    err = ioctl(pCtxt->torch_fd, NVC_IOCTL_PARAM_RD, &params);
    if (err < 0)
    {
        NvOsDebugPrintf("%s: ioctl to get parameter %d failed: %s\n",
                        __func__, Param, strerror(errno));
        return NV_FALSE;
    }
#endif //(BUILD_FOR_AOS == 0)
    return NV_TRUE;
}

static NvBool TorchNvc_GetStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerStaticProperty *pProperties)
{
    NvBool Status;
    NvBool bNewInst = NV_FALSE;
    TorchNvcContext *pCtxt;
    NvU32 idx;

    NV_DEBUG_PRINTF(("%s\n", __func__));
    if (!hImager || !hImager->pFlash)
        return NV_FALSE;

    // skip in the case camera-core already opened the device
    if (!hImager->pFlash->pPrivateContext)
    {
        Status = TorchNvc_Open(hImager);
        if (Status == NV_FALSE)
        {
            return NV_FALSE;
        }
        bNewInst = NV_TRUE;
    }

    pCtxt = (TorchNvcContext *)hImager->pFlash->pPrivateContext;
    for (idx = 0; idx < pCtxt->Query.NumberOfFlash; idx++) {
        pProperties->AvailableLeds.Property[idx] = 1;
    }
    pProperties->AvailableLeds.Size = idx;
    pProperties->FlashChargeDuration = 0;
    /* these flash properties are not supported currently:
       FlashMaxEnergy, we have the max-current value but not lumen-senconds
       FlashColorTemperature, there is such item in the flash cap, but it's empty
    */

    if (bNewInst)
    {
        TorchNvc_Close(hImager);
    }

    return NV_TRUE;
}

NvBool TorchNvc_GetHal(NvOdmImagerHandle hImager)
{
    if (!hImager || !hImager->pFlash)
    {
        NvOsDebugPrintf("%s err: No hImager->pFlash\n", __func__);
        return NV_FALSE;
    }

    hImager->pFlash->pfnOpen = TorchNvc_Open;
    hImager->pFlash->pfnClose = TorchNvc_Close;
    hImager->pFlash->pfnGetCapabilities = TorchNvc_GetCapabilities;
    hImager->pFlash->pfnSetPowerLevel = TorchNvc_SetPowerLevel;
    hImager->pFlash->pfnSetParameter = TorchNvc_SetParameter;
    hImager->pFlash->pfnGetParameter = TorchNvc_GetParameter;
    hImager->pFlash->pfnStaticQuery = TorchNvc_GetStaticProperties;
    return NV_TRUE;
}

