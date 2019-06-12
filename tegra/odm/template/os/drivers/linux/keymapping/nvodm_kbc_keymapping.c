/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Keyboard Controller virtual key mapping</b>
 *
 * @b Description: Implement the ODM keyboard mapping to the platform 
 *                  specific.
 */
#include "nvodm_kbc_keymapping.h"
#include <linux/input.h>

/* The total number of soc scan codes will be (first - last) */
#define NV_SOC_NORMAL_KEY_SCAN_CODE_TABLE_FIRST    0
#define NV_SOC_NORMAL_KEY_SCAN_CODE_TABLE_LAST     3

static NvU32 KbcLayOutVirtualKey[] =
{
 KEY_MENU,
 0,
 KEY_HOME,
 KEY_BACK
};

static struct NvOdmKeyVirtTableDetail s_ScvkKeyMap =
{
    NV_SOC_NORMAL_KEY_SCAN_CODE_TABLE_FIRST,    // scan code start
    NV_SOC_NORMAL_KEY_SCAN_CODE_TABLE_LAST,     // scan code end
    KbcLayOutVirtualKey          // Normal Qwerty keyboard
};


static const struct NvOdmKeyVirtTableDetail *s_pVirtualKeyTables[] =
     {&s_ScvkKeyMap};
     

NvU32 
NvOdmKbcKeyMappingGetVirtualKeyMappingList(
    const struct NvOdmKeyVirtTableDetail ***pVirtKeyTableList)
{
   *pVirtKeyTableList = s_pVirtualKeyTables;
   return NV_ARRAY_SIZE(s_pVirtualKeyTables);
}

