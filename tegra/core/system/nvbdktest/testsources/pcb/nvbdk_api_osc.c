/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "testsources.h"
#include "nvrm_hardware_access.h"
#include "nvbdk_pcb_api.h"
#include "nvbdk_pcb_interface.h"

#ifndef RTC_PA_BASE
#define RTC_PA_BASE         0x7000E000  // Base address for RTC
#endif
#ifndef TIMERUS_PA_BASE
#define TIMERUS_PA_BASE     0x60005010  // Base address for artimerus
#endif

extern NvU32 nvaosDisableInterrupts(void);
extern void nvaosRestoreInterrupts(NvU32 state);
extern NvU64 nvaosGetTimeUS(void);

void NvApiOscCheck(NvBDKTest_TestPrivData param)
{
    PcbTestData *pPcbData = (PcbTestData *)param.pData;
    OscTestPrivateData *pOscData = (OscTestPrivateData *)pPcbData->privateData;
    NvBool b;
    NvU32 start_rtc;
    NvU32 stop_rtc;
    NvU32 end_rtc;
    NvU32 elapsed_timerus;
    NvU64 start_timerus;
    NvU64 end_timerus;
    NvU32 timerus_cfg;
    NvU32 osc_freq;
    NvU32 error_us;
    NvU32 int_state;

    if (pOscData->delay_loop_ms > 1000)
    {
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name,
                            "pcb", "OSC_DELAY_LOOP_MS has to be smaller than 1000");
    }

    int_state = nvaosDisableInterrupts();
    start_rtc = NV_READ32(RTC_PA_BASE + 0x10);
    while ((end_rtc = NV_READ32(RTC_PA_BASE + 0x10)) == start_rtc)
        ; /* ready */
    /* get the start-time to compare with the end-time*/
    start_timerus = nvaosGetTimeUS();
    start_rtc = end_rtc;

    if (pOscData->delay_loop_ms == 1000)
    {
        /* start the delay after 1ms */
        stop_rtc = start_rtc;
        while (NV_READ32(RTC_PA_BASE + 0x10) == stop_rtc)
            ;
    }
    else
    {
        stop_rtc = (start_rtc + pOscData->delay_loop_ms) % 1000;
    }

    /* delay pOscData->delay_loop_ms */
    while ((end_rtc = NV_READ32(RTC_PA_BASE + 0x10)) != stop_rtc)
        ;
    /* get the end-time to compare with the start-time*/
    end_timerus = nvaosGetTimeUS();
    nvaosRestoreInterrupts(int_state);

    timerus_cfg = NV_READ32(TIMERUS_PA_BASE + 0x4);

    elapsed_timerus = (NvU32)(end_timerus - start_timerus);

    error_us = NV_MAX(elapsed_timerus, (pOscData->delay_loop_ms * 1000)) -
                NV_MIN(elapsed_timerus, (pOscData->delay_loop_ms * 1000));

    osc_freq = ((timerus_cfg & 0xF) + 1) / (((timerus_cfg >> 8) & 0xF) + 1);

    if (error_us > pOscData->valid_range_us)
    {
        char szAssertBuf[1024];
        NvOsSnprintf(szAssertBuf, 1024,
                     "(start_rtc[%u], end_rtc[%u], stop_rtc[%u], "
                     "ST[%u:%u], ET[%u:%u], "
                     "elapsed_timerus[%u], timerus_cfg[0x%x], "
                     "osc_freq[%uMHz], error_us[%u])",
                     start_rtc, end_rtc, stop_rtc,
                     (NvU32)(start_timerus >> 32), (NvU32)start_timerus,
                     (NvU32)(end_timerus >> 32), (NvU32)end_timerus,
                     elapsed_timerus, timerus_cfg,
                     osc_freq, error_us);
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", szAssertBuf);
    }

fail:
    return;
}

