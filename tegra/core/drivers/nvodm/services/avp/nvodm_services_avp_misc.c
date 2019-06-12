/*
 * Copyright (c) 2010-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nvodm_query.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvrm_gpio.h"
#include "nvrm_pmu.h"
#include "nvrm_keylist.h"
#include "nvrm_power.h"
#include "nvrm_analog.h"
#include "nvrm_pinmux.h"
#include "nvrm_hardware_access.h"
#include "t30/artimerus.h"
#include "t30/arslink.h"
#include "t30/arapb_misc.h"
#include "t30/arclk_rst.h"
#include "t30/arapbpm.h"

#define PMC_PA_BASE      0x7000E400


void NvOdmEnableOtgCircuitry(NvBool Enable)
{
    NV_ASSERT(! "not implemented");
}

// From nvodm_services_ext.c.
NvOdmServicesKeyListHandle
NvOdmServicesKeyListOpen(void)
{
    return (NvOdmServicesKeyListHandle)0x1;
}

// From nvodm_services_ext.c.
void NvOdmServicesKeyListClose(NvOdmServicesKeyListHandle handle)
{

}

// From nvodm_services_common.c.
NvU32 NvOdmServicesGetKeyValue(
    NvOdmServicesKeyListHandle handle,
    NvU32 Key)
{
    return NV_READ32(PMC_PA_BASE + APBDEV_PMC_SCRATCH20_0);
}

