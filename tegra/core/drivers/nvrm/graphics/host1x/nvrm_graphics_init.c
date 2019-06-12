/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvutil.h"
#include "nvassert.h"
#include "nvrm_init.h"
#include "nvrm_xpc.h"
#include "host1x_channel.h"
#include "host1x_hwcontext.h"
#include "nvrm_graphics_private.h"

/* No locking done. Should be take care off by the caller */

NvError NvRmGraphicsOpen(NvRmDeviceHandle rm)
{
    NvError err = NvSuccess;

    if (!(NVCPU_IS_X86 && NVOS_IS_WINDOWS))
    {
        err = NvRmPrivChannelInit(rm);
        if (NV_SHOW_ERROR(err))
            return err;
        NvRmPrivHostInit(rm);

    }
    return err;
}

void
NvRmGraphicsClose(NvRmDeviceHandle handle)
{
    if (!handle)
        return;

    /* shutdown the channels */
    NvRmPrivChannelDeinit(handle);

    /* Host shutdown */
    NvRmPrivHostShutdown(handle);

    /* TODO: [ahatala 2010-06-01] */
    // NvRmPrivDeInitAvp( handle );
}
