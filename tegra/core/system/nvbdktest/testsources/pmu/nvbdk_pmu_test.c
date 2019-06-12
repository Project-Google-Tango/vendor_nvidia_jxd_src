/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvos.h"
#include "nvodm_services.h"
#include "nvtest.h"
#include "nvodm_pmu.h"
#include "nvrm_init.h"
#include "nvodm_query_discovery.h"
#include "testsources.h"

/* Processor Board  ID */
#define PROC_BOARD_E1611    0x064B
#define PROC_BOARD_E1545    0x0609
#define PROC_BOARD_E1569    0x0621
#define PROC_BOARD_E1612    0x064C
#define PROC_BOARD_E1613    0x064D
#define PROC_BOARD_E1580    0x062C
#define PROC_BOARD_E1680    0x0690
#define PROC_BOARD_E1681    0x0691
#define PROC_BOARD_E1690    0x069A
#define PROC_BOARD_E1671    0x0687
#define PROC_BOARD_E1670    0x0686

/* T124  Processor Board ID */
#define PROC_BOARD_E1780    0X06F4
#define PROC_BOARD_E1781    0X06F5
#define PROC_BOARD_PM358    0X0166
#define PROC_BOARD_PM359    0X0167
#define PROC_BOARD_PM363    0x016B
#define PROC_BOARD_E1790    0X06FE

/* PMU Board ID */
#define PMU_E1731           0x06c3 //TI PMU
#define PMU_E1733           0x06c5 //AMS3722/29 BGA
#define PMU_E1734           0x06c6 //AMS3722 CSP
#define PMU_E1735           0x06c7 //TI 913/4
#define PMU_E1736           0x06c8 //TI 913/4

static void Tps51632PowerRailTest(NvOdmServicesPmuHandle hPmu);
static void Tps65914PowerRailTest(NvOdmServicesPmuHandle hPmu);
static void Max77663PowerRailTest(NvOdmServicesPmuHandle hPmu);
static void Tps80036PowerRailTest(NvOdmServicesPmuHandle hPmu);
static void Max77660PowerRailTest(NvOdmServicesPmuHandle hPmu);
static void As3722PowerRailTest(NvOdmServicesPmuHandle hPmu);

// FIX ME: will be enabled once the IsRailEnabled issue in TPS65090 is fixed
// static void Tps65090PowerRailTest(NvOdmServicesPmuHandle hPmu);
static NvError StartPowerRailTest(NvOdmServicesPmuHandle hPmu,
                                  const NvOdmPeripheralConnectivity *pPerConnectivity);
static NvError NvOdmT124PmuSelect(NvOdmServicesPmuHandle hPmu);

NvError NvBdkPmuTestMain(void)
{
    NvOdmServicesPmuHandle hPmu = NULL;
    NvOdmBoardInfo BoardInfo;
    NvBool status;
    NvError e = NvError_Success;

    NvOdmOsMemset(&BoardInfo, 0, sizeof(NvOdmBoardInfo));

    status = NvOdmPeripheralGetBoardInfo(0, &BoardInfo);
    if (!status)
    {
        NvOsDebugPrintf("\n%s: Test Failed. Error reading boardId.\n",__FUNCTION__);
        e = NvError_BadParameter;
        return e;
    }

    hPmu = NvOdmServicesPmuOpen();
    if (hPmu != NULL)
        NvOsDebugPrintf("PMU Device Successfully opened..\n");
    else
    {
        NvOsDebugPrintf("\n%s: Test Failed. Error Opening PMU device.\n",__FUNCTION__);
        e = NvError_InvalidState;
        return e;
    }

    switch(BoardInfo.BoardID)
    {
        case PROC_BOARD_E1611:
             Tps51632PowerRailTest(hPmu);
             Tps65914PowerRailTest(hPmu);
             //Tps65090PowerRailTest(hPmu);
             break;

        case PROC_BOARD_E1612:
        case PROC_BOARD_E1613:
             Tps51632PowerRailTest(hPmu);
             Max77663PowerRailTest(hPmu);
             //Tps65090PowerRailTest(hPmu);
             break;

        case PROC_BOARD_E1545:
        case PROC_BOARD_E1569:
        case PROC_BOARD_E1580:
             Tps65914PowerRailTest(hPmu);
             break;

        case PROC_BOARD_E1670:
             Tps80036PowerRailTest(hPmu);
             break;

        case PROC_BOARD_E1680:
        case PROC_BOARD_E1681:
        case PROC_BOARD_E1690:
        case PROC_BOARD_E1671:
             Max77660PowerRailTest(hPmu);
             break;

        case PROC_BOARD_E1780:
        case PROC_BOARD_E1781:
        case PROC_BOARD_E1790:
        case PROC_BOARD_PM358:
        case PROC_BOARD_PM359:
        case PROC_BOARD_PM363:
             e = NvOdmT124PmuSelect(hPmu);
             break;

        default:
             NvOdmOsPrintf("BoardId: 0x%04x is not supported\n",
                            BoardInfo.BoardID);
             e = NvError_NotSupported;
    }

    NvOdmServicesPmuClose(hPmu);
    return e;
}

static NvError NvOdmT124PmuSelect(NvOdmServicesPmuHandle hPmu)
{
    NvOdmBoardInfo PmuBoard;
    NvBool status;
    NvError e = NvError_Success;

    NV_ASSERT(hPmu);
    NvOdmOsMemset(&PmuBoard, 0, sizeof(NvOdmBoardInfo));

    status = NvOdmPeripheralGetBoardInfo(PMU_E1731, &PmuBoard);

    if (!status)
        status = NvOdmPeripheralGetBoardInfo(PMU_E1733, &PmuBoard);
    if (!status)
        status = NvOdmPeripheralGetBoardInfo(PMU_E1734, &PmuBoard);
    if (!status)
        status = NvOdmPeripheralGetBoardInfo(PMU_E1735, &PmuBoard);
    if (!status)
        status = NvOdmPeripheralGetBoardInfo(PMU_E1736, &PmuBoard);
    if (!status)
    {
        NvOdmOsPrintf("Pmu BoardId is not supported.\n");
        e = NvError_NotSupported;
        return e;
    }

    switch(PmuBoard.BoardID)
    {
         case PMU_E1731:
              Tps80036PowerRailTest(hPmu);
              break;

         case PMU_E1733:
         case PMU_E1734:
              As3722PowerRailTest(hPmu);
              break;

         case PMU_E1735:
         case PMU_E1736:
              Tps65914PowerRailTest(hPmu);
              break;
    }

    return e;
}

static void Tps51632PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvError e = NvSuccess;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('5','1','6','3','2','T','S','T'));
    if (pPerConnectivity == NULL)
        goto fail;

    e = StartPowerRailTest(hPmu, pPerConnectivity);
    if (e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}

static void As3722PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvError e = NvSuccess;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('3','7','2','2','T','S','T',' '));
    if (pPerConnectivity == NULL)
        goto fail;

    e = StartPowerRailTest(hPmu, pPerConnectivity);
    if (e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}

static void Tps65914PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvError e = NvSuccess;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('6','5','9','1','4','T','S','T'));
    if (pPerConnectivity == NULL)
        goto fail;

    e = StartPowerRailTest(hPmu, pPerConnectivity);
    if (e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}

static void Max77663PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvError e = NvSuccess;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('7','7','6','6','3','T','S','T'));
    if (pPerConnectivity == NULL)
        goto fail;

    e = StartPowerRailTest(hPmu, pPerConnectivity);
    if (e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}

static void Tps80036PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvError e = NvSuccess;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('8','0','0','3','6','T','S','T'));
    if (pPerConnectivity == NULL)
        goto fail;

    e = StartPowerRailTest(hPmu, pPerConnectivity);
    if (e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}

static void Max77660PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvError e = NvSuccess;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('7','7','6','6','0','T','S','T'));
    if (pPerConnectivity == NULL)
        goto fail;

    e = StartPowerRailTest(hPmu, pPerConnectivity);
    if (e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}

// FIX ME: will be enabled once the IsRailEnabled issue in TPS65090 is fixed
#if 0
static void Tps65090PowerRailTest(NvOdmServicesPmuHandle hPmu)
{
    NvU32 voltCheck;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 Index, SettlingTime, VddId;
    NvError e = NvSuccess;
    char *test = "PmuPowerRailTest";
    char *suite = "pmu";
    NvBool b;

    NvOsDebugPrintf("\n%s starts. \n", __FUNCTION__);

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('6','5','0','9','0','T','S','T'));
    if (pPerConnectivity == NULL)
        goto fail;

    SettlingTime = 0;

    // Search for the Vdd rail and set the proper voltage to the rail
    for (Index = 0; Index < pPerConnectivity->NumAddress; Index++)
    {
        if (pPerConnectivity->AddressList[Index].Interface==NvOdmIoModule_Vdd)
        {
            voltCheck = 0;
            VddId = pPerConnectivity->AddressList[Index].Address;
            NvOdmServicesPmuGetCapabilities(hPmu, VddId, &RailCaps);
            NvOdmServicesPmuSetVoltage(hPmu, VddId, RailCaps.requestMilliVolts,
                                      &SettlingTime);
            if (SettlingTime)
                NvOdmOsWaitUS(SettlingTime);

            NvOdmServicesPmuGetVoltage(hPmu, VddId, &voltCheck);
            NvOsDebugPrintf("VddId=%u, requestMilliVolts=%u getMilliVolts=%u\n",
                           VddId, RailCaps.requestMilliVolts, voltCheck);
            // FIX ME: Check IsRailEnabled and assign e depending on it
            TEST_ASSERT_EQUAL(e, NvSuccess, test ,suite, "\nTest Failed\n");
        }
    }
    NvOsDebugPrintf("\n%s finishes. \n", __FUNCTION__);

fail:
    if (e != NvSuccess)
        NvOsDebugPrintf("\n%s Test Failed\n",__FUNCTION__);
}
#endif

static NvError StartPowerRailTest(NvOdmServicesPmuHandle hPmu,
                               const NvOdmPeripheralConnectivity *pPerConnectivity)
{
    NvU32 voltCheck;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 Index, SettlingTime, VddId;
    NvError e = NvSuccess;
    char *test = "PmuPowerRailTest";
    char *suite = "pmu";
    NvBool b;

    SettlingTime = 0;

    // Search for the Vdd rail and set the proper voltage to the rail
    for (Index = 0; Index < pPerConnectivity->NumAddress; Index++)
    {
        if (pPerConnectivity->AddressList[Index].Interface==NvOdmIoModule_Vdd)
        {
            voltCheck = 0;
            VddId = pPerConnectivity->AddressList[Index].Address;
            NvOdmServicesPmuGetCapabilities(hPmu, VddId, &RailCaps);
            NvOdmServicesPmuSetVoltage(hPmu, VddId, RailCaps.requestMilliVolts,
                                      &SettlingTime);
            if (SettlingTime)
                NvOdmOsWaitUS(SettlingTime);

            NvOdmServicesPmuGetVoltage(hPmu, VddId, &voltCheck);
            NvOsDebugPrintf("VddId=%u, requestMilliVolts=%u getMilliVolts=%u\n",
                           VddId, RailCaps.requestMilliVolts, voltCheck);

            if (voltCheck == 0 ||
               (voltCheck > (RailCaps.requestMilliVolts + 50)) ||
               (voltCheck < (RailCaps.requestMilliVolts - 50)))
                e=!NvSuccess;
            TEST_ASSERT_EQUAL(e, NvSuccess, test ,suite, "\nTest Failed\n");
        }
    }

fail:
    return e;
}

NvError pmu_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest = NULL;
    NvError e = NvSuccess;

    e = NvBDKTest_AddSuite("pmu", &pSuite);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("%s: Adding suite pmu failed: NvError 0x%x\n",
                        __FUNCTION__, e);
        goto fail;
    }

    e = NvBDKTest_AddTest(pSuite, &ptest, "NvBdkPmuTestMain",
                          (NvBDKTest_TestFunc)NvBdkPmuTestMain, "basic",
                          NULL, NULL);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("%s: Adding test NvBdkPmuTestMain failed: NvError 0x%x\n",
                        __FUNCTION__, e);
        goto fail;
    }

fail:
    return e;
}

