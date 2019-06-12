/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbdk_pcb_interface.h"
#include "nvbdk_pcb_api.h"

/* pcb:osc:xxxxx */
static OscTestPrivateData sOscData = {
    500, 100
};

/* default data table */
static PcbTestData sPcbTestDataList_Default[] = {
    { "osc", "osc_test", NvApiOscCheck, (void *)&sOscData },
    { NULL, NULL, NULL, NULL },
};

PcbTestData *NvGetPcbTestDataList(void)
{
    return sPcbTestDataList_Default;
}
