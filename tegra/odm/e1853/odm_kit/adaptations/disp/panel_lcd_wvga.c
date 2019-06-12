/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdlib.h> // strtoul()
#include <string.h> // memset()

#include "nvodm_disp.h"
#include "panel_lcd_wvga.h"
#include "nvodm_services.h"

static NvU32 s_SkuId;
static NvBool s_bIsDisplayE1188;

#if NVOS_IS_QNX
#include <fcntl.h>
#define NVSKUIDMAXLEN       30
#define NVQUERYISE1888LEN   5
#endif

/*
 * Get Sku Number.
 */
static NvU32
NvOdmGetSkuId(void)
{
#if NVOS_IS_QNX
    int skuFd = 0;
    char buffer[NVSKUIDMAXLEN + 1];
    NvU32 skuId;

    memset(buffer, 0, NVSKUIDMAXLEN + 1);
    /* Open /dev/nvsku/sku */
    skuFd = open("/dev/nvsku/sku", O_RDONLY);
    if (skuFd == -1)
        return -1;

    /* read the sku number. */
    read(skuFd, buffer, NVSKUIDMAXLEN);
    skuId = (NvU32) strtoul(buffer, NULL, 10);

    if (skuId == 0)
    {
        if (skuFd >= 0)
            close(skuFd);
        return -1;
    }

    close(skuFd);
    return skuId;
#else
    return -1;
#endif
}

/*
 * Check whether base board is E1888.
 */
static NvBool
NvOdmQueryIsE1888(void)
{
#if NVOS_IS_QNX
    int baseboardFd = 0;
    char buffer[NVQUERYISE1888LEN + 1];

    memset(buffer, 0, NVQUERYISE1888LEN + 1);
    /* Open /dev/nvsku/e1888 */
    baseboardFd = open("/dev/nvsku/e1888", O_RDONLY);
    if (baseboardFd == -1)
        return NV_FALSE;

    /* read the base board version. */
    read(baseboardFd, buffer, NVQUERYISE1888LEN);

    close(baseboardFd);

    if (strncmp(buffer, "1", 2) == 0)
        return NV_TRUE;
    else
        return NV_FALSE;
#else
    return NV_FALSE;
#endif
}

static NvOdmDispDeviceMode custom_lcd_modes[] =
{
    // 24 bit
    {
        1440, // width
        540, // height
        24, // bpp
        0, // refresh
        57143, // frequency
        NVODM_DISP_MODE_FLAG_NONE, // flags
        // timing
        {
            1, // h_ref_to_sync
            1, // v_ref_to_sync
            60, // h_sync_width
            2, // v_sync_width
            80, // h_back_porch
            8, // v_back_porch
            1440, // h_disp_active
            540, // v_disp_active
            81, // h_front_porch
            24, // v_front_porch
          },
      NV_FALSE // partial
    }
};

static NvOdmDispDeviceMode lcd_wvga_modes[] =
{
    // WVGA, 24 bit
    {
        800, // width
        480, // height
        24, // bpp
        0, // refresh
        32460, // frequency
        NVODM_DISP_MODE_FLAG_NONE, // flags
        // timing
        {
            1, // h_ref_to_sync
            1, // v_ref_to_sync
            64, // h_sync_width
            3, // v_sync_width
            128, // h_back_porch
            22, // v_back_porch
            800, // h_disp_active
            480, // v_disp_active
            64, // h_front_porch
            20, // v_front_porch
          },
      NV_FALSE // partial
    }
};

static NvBool
lcd_wvga_discover(NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid = LCD_WVGA_GUID;
    NvOdmPeripheralConnectivity const *conn;

    conn = NvOdmPeripheralGetGuid(guid);
    if (!conn)
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

static NvBool
lcd_wvga_Setup(NvOdmDispDeviceHandle hDevice)
{
    if (!lcd_wvga_discover(hDevice))
    {
        return NV_FALSE;
    }

    if (!hDevice->bInitialized)
    {
        hDevice->Type = NvOdmDispDeviceType_TFT;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;

        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        if ((s_SkuId == 5) && !s_bIsDisplayE1188)
        {
            hDevice->MaxHorizontalResolution = 1440;
            hDevice->MaxVerticalResolution = 540;
            hDevice->modes = custom_lcd_modes;
            hDevice->nModes = NV_ARRAY_SIZE(custom_lcd_modes);
        }
        else
        {
            hDevice->MaxHorizontalResolution = 800;
            hDevice->MaxVerticalResolution = 480;
            hDevice->modes = lcd_wvga_modes;
            hDevice->nModes = NV_ARRAY_SIZE(lcd_wvga_modes);
        }
        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

static void
lcd_wvga_Release(NvOdmDispDeviceHandle hDevice)
{
    hDevice->bInitialized = NV_FALSE;
}

static void
lcd_wvga_ListModes(NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes)
{
    NV_ASSERT(nModes);
    if (*nModes)
    {
        *nModes = 1;
        NV_ASSERT(modes);
        *modes = hDevice->modes[0];
    }
    else
    {
        *nModes = 1;
    }
}

static NvBool
lcd_wvga_GetParameter(NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val)
{
    NV_ASSERT(hDevice);
    NV_ASSERT(val);

    switch (param)
    {
        case NvOdmDispParameter_Type:
            *val = hDevice->Type;
            break;
        case NvOdmDispParameter_Usage:
            *val = hDevice->Usage;
            break;
        case NvOdmDispParameter_MaxHorizontalResolution:
            *val = hDevice->MaxHorizontalResolution;
            break;
        case NvOdmDispParameter_MaxVerticalResolution:
            *val = hDevice->MaxVerticalResolution;
            break;
        case NvOdmDispParameter_BaseColorSize:
            *val = hDevice->BaseColorSize;
            break;
        case NvOdmDispParameter_DataAlignment:
            *val = hDevice->DataAlignment;
            break;
        case NvOdmDispParameter_PinMap:
            *val = hDevice->PinMap;
            break;
        case NvOdmDispParameter_PwmOutputPin:
            *val = hDevice->PwmOutputPin;
            break;
        default:
            return NV_FALSE;
    }
    return NV_TRUE;
}

static void *
lcd_wvga_GetConfiguration(NvOdmDispDeviceHandle hDevice)
{
    NvOdmDispTftConfig *tft = &hDevice->Config.tft;
    return (void *)tft;
}

void custom_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 2;

    Pins[0] = NvOdmDispPin_HorizontalSync;
    Values[0] = NvOdmDispPinPolarity_High;

    Pins[1] = NvOdmDispPin_VerticalSync;
    Values[1] = NvOdmDispPinPolarity_High;
}

void
lcd_wvga_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    s_SkuId = NvOdmGetSkuId();
    /*
     * If the baseboard is E1888 => Display is E1188
     */
    s_bIsDisplayE1188 = NvOdmQueryIsE1888();
    hDevice->Setup = lcd_wvga_Setup;
    hDevice->Release = lcd_wvga_Release;
    hDevice->ListModes = lcd_wvga_ListModes;
    hDevice->GetParameter = lcd_wvga_GetParameter;
    hDevice->GetConfiguration = lcd_wvga_GetConfiguration;
    if (s_SkuId == 5)
    {
        /*
         * It seems E1188 supports both postive & negative polarities. So,
         * changing polarities all displays on SKU5.
         */
        hDevice->GetPinPolarities = custom_GetPinPolarities;
    }
}
