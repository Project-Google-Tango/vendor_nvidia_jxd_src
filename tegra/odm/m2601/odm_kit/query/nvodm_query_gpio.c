/*
 * Copyright (c) 2007-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query_gpio.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"

#define NVODM_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NVODM_PORT(x) ((x) - 'a')


static const NvOdmGpioPinInfo s_Sdio0[] = {
    {NVODM_PORT('i'), 5, NvOdmGpioPinActiveState_Low},  //GMI_IORDY, card detect
    {NVODM_GPIO_INVALID_PORT, NVODM_GPIO_INVALID_PIN,
        NvOdmGpioPinActiveState_Low},                  // power rail enable
};


const NvOdmGpioPinInfo *NvOdmQueryGpioPinMap(NvOdmGpioPinGroup Group,
    NvU32 Instance, NvU32 *pCount)
{
    switch (Group)
    {
        case NvOdmGpioPinGroup_Sdio:
            if (Instance == 0)
            {
                *pCount = NVODM_ARRAY_SIZE(s_Sdio0);
                return s_Sdio0;
            }
            else
            {
                *pCount = 0;
                return NULL;
            }

        default:
            *pCount = 0;
            return NULL;
    }
}
