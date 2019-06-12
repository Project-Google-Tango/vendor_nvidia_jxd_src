/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_keyboard_private.h"
#ifdef TEGRA_12x_SOC
#include "t12x/arapbdev_kbc.h"
#else
#include "t11x/arapbdev_kbc.h"
#endif
#include "nvrm_drf.h"
#include "nvrm_module.h"
#include "nvrm_hardware_access.h"
#include "nvassert.h"
#include "nvrm_interrupt.h"
#include "nvodm_query_kbc.h"
#include "nvodm_kbc.h"
#include "nvrm_power.h"
#include "nvrm_pmu.h"
#include "nvodm_query_discovery.h"

#define KBC_REG_READ32(pKbcVirtAdd, reg) \
        NV_READ32(pKbcVirtAdd + ((APBDEV_KBC_##reg##_0)/4))

#define KBC_REG_WRITE32(pKbcVirtAdd, reg, Data2Write) \
    do \
    { \
        NV_WRITE32(pKbcVirtAdd + ((APBDEV_KBC_##reg##_0)/4), (Data2Write)); \
    }while (0)

#define GET_KEY_CODE(hKbc, col, row, reg, ent, k) \
    do \
    { \
        if (NV_DRF_VAL(APBDEV_KBC, KP_ENT##reg, KP_NEW_ENT##ent, k)) \
        { \
            col = NV_DRF_VAL(APBDEV_KBC, KP_ENT##reg, KP_COL_NUM_ENT##ent, k); \
            row = NV_DRF_VAL(APBDEV_KBC, KP_ENT##reg, KP_ROW_NUM_ENT##ent, k); \
            hKbc->RowNumbers[ValidKeyCount] = row; \
            hKbc->ColNumbers[ValidKeyCount] = col; \
            ValidKeyCount++; \
        } \
    }while (0)

enum
{
    /*
     * This is the value of the KBC Cycle time in microseconds. The KBC clock
     * is 32Khz. therefore the time-period is 31.25 usecs.
     */
    KBC_CYCLE_TIME_US = 32,
    KBC_CP_TIME_OUT_MS = 2000,
    KBC_KEY_QUEUE_DEPTH = 64
};

typedef struct
{
    // Is per-key masking supported by the controller;
    NvBool IsKeyMaskingSupported;

    // Is Minimum row required for proper functioning
    NvU32 MinRowCount;

    // Maximum number of events in the fifo
    NvU32 MaxEvents;
} NvDdkKbcCapability;

typedef struct KeyInfoRec
{
    // Key number.
    NvU32 KeyNumber;
    // Key event.
    NvDdkKbcKeyEvent KeyEvent;
    // Holds that time stamp.
    NvU32 TimeMs;
}KeyInfo;

typedef struct KbcKeyEventProcessInfoRec
{
    // Holds the client sema to be signaled.
    NvOsSemaphoreHandle KbcClientSema;
    // Holds the handle to KBC.
    NvDdkKbcHandle hKbc;
    // Repeat delay in Micro seconds.
    NvU32 RepeatDelayMs;
    // Pressed keys in the current cycle.
    NvU32 PressedKeys[APBDEV_KBC_MAX_ENT];
    // Pressed keys in the previous cycle.
    NvU32 PreviouslyPressedKeys[APBDEV_KBC_MAX_ENT];
    // Gives the number of keys that are pressed in the current cycle of scan.
    NvU32 CurrentPressedKeyCount;
    // Gives the number of keys that are pressed during the previous cycle of scan.
    NvU32 PreviouslyPressedKeyCount;
}KbcKeyEventProcessInfo;

// struct holding the kbc related info.
typedef struct NvDdkKbcRec
{
    // Rm device handle.
    NvRmDeviceHandle RmDevHandle;
    // Holds Kbc Controller registers physical base address.
    NvRmPhysAddr BaseAddress;
    // Holds the register map size.
    NvU32 BankSize;
    // Holds the virtual address for accessing registers.
    NvU32* pVirtualAddress;
    // Key event info
    KbcKeyEventProcessInfo KEPInfo;
    // Indicates whether Kbc is started.
    NvBool KbcStarted;
    // Contains the Power Client ID
    NvU32 KbcRmPowerClientID;
    // Peripheral DataBase
    const NvOdmPeripheralConnectivity *pConnectivity;
    // Number of Rows present in the key matrix.
    NvU32 NumberOfRows;
    // Number of Columns present in the key matrix.
    NvU32 NumberOfColumns;
    // Interrupt handle
    NvOsInterruptHandle InterruptHandle;
    // Time delay between key scans.
    NvU32 DelayBetweenScans;
    // Is Key Masking supported
    NvBool IsKeyMaskingSupported;

    // Minimum number of rows need to be enable
    NvU32 MinRowCount;

    // Maximum number of events in the fifo
    NvU32 MaxEvents;

    NvU32 RowNumbers[APBDEV_KBC_MAX_ENT];
    NvU32 ColNumbers[APBDEV_KBC_MAX_ENT];

    // Stores the the values of Row Configuration Registers
    NvU32 KbcRegRowCfg0;
    NvU32 KbcRegRowCfg1;
    NvU32 KbcRegRowCfg2;
    NvU32 KbcRegRowCfg3;
    // Stores the values of Column Configuration Registers
    NvU32 KbcRegColCfg0;
    NvU32 KbcRegColCfg1;
    NvU32 KbcRegColCfg2;
    NvU32 KbcSuspendedRowReg[4];
    NvU32 KbcSuspendedColReg[3];
    NvU32 KbcRowMaskReg[APBDEV_KBC_NUM_ROWS];
    NvBool IsSelectKeysWkUpEnabled;

    NvU32 OpenCount;
}NvDdkKbc;

static NvDdkKbcHandle s_hKbc = NULL;

static NvError NvddkPrivKbcEnableClock(NvDdkKbcHandle hKbc)
{
    NvError  Error = NvSuccess;

    Error = NvRmPowerVoltageControl(hKbc->RmDevHandle,
                NvRmModuleID_Kbc,
                hKbc->KbcRmPowerClientID,
                NvRmVoltsUnspecified,
                NvRmVoltsUnspecified,
                NULL,
                0,
                NULL);
    if (Error)
        return Error;

    Error = NvRmPowerModuleClockControl(hKbc->RmDevHandle,
                NvRmModuleID_Kbc,
                hKbc->KbcRmPowerClientID,
                NV_TRUE);
    return Error;
}

static NvError NvddkPrivKbcDisableClock(NvDdkKbcHandle hKbc)
{
    NvError Error;
    Error = NvRmPowerModuleClockControl(hKbc->RmDevHandle,
                NvRmModuleID_Kbc,
                hKbc->KbcRmPowerClientID,
                NV_FALSE);
    return Error;
}

static void NvDdkPrivKbcSuspendController(NvDdkKbcHandle hKbc)
{
    (void)NvRmPowerVoltageControl(hKbc->RmDevHandle,
                        NvRmModuleID_Kbc,
                        hKbc->KbcRmPowerClientID,
                        NvRmVoltsOff,
                        NvRmVoltsOff,
                        NULL,
                        0,
                        NULL);
}

static NvError NvDdkPrivKbcResumeController(NvDdkKbcHandle hKbc)
{
    NvError Error;

    Error = NvRmPowerVoltageControl(hKbc->RmDevHandle,
                NvRmModuleID_Kbc,
                hKbc->KbcRmPowerClientID,
                NvRmVoltsUnspecified,
                NvRmVoltsUnspecified,
                NULL,
                0,
                NULL);
    return Error;
}
static void SetPowerOnKeyboard(NvDdkKbcHandle hKbc, NvBool IsEnable)
{
    NvU32 i;
    NvRmPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;

    for (i = 0; i < (hKbc->pConnectivity->NumAddress); i++)
    {
        // Search for the vdd rail entry
        if (hKbc->pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
        {
            NvRmPmuGetCapabilities(hKbc->RmDevHandle,
                    hKbc->pConnectivity->AddressList[i].Address, &RailCaps);
            if (IsEnable)
            {
                NvRmPmuSetVoltage(hKbc->RmDevHandle,
                        hKbc->pConnectivity->AddressList[i].Address,
                            RailCaps.requestMilliVolts, &SettlingTime);
            }
            else
            {
                NvRmPmuSetVoltage(hKbc->RmDevHandle,
                        hKbc->pConnectivity->AddressList[i].Address,
                            ODM_VOLTAGE_OFF, &SettlingTime);
            }
            if (SettlingTime)
                NvOsWaitUS(SettlingTime);
        }
    }
}



static void SuspendedConfiguration(NvDdkKbcHandle hKbc)
{
    NvU32 i;
    NvU32 PinNumber;
    NvU32 NewVal;
    NvU32 ShiftVal;
    NvU32 RowCfgFldSize;
    NvU32 ColCfgFldSize;
    NvU32 RowCfgMask;
    NvU32 ColCfgMask;
    NvU32 RowsPerReg;
    NvU32 ColsPerReg;
    NvBool IsColumn;
    NvU32 RowConnectionNumber = 0;
    NvU32 ColumnConnectionNumber = 0;
    NvU32 *RowNums;
    NvU32 *ColNums;
    NvU32 NumOfKeys;
    NvU32 j;
    NvBool IsWakeupEnable = NV_FALSE;
    NvU32 ActiveRows[APBDEV_KBC_NUM_ROWS];
    NvU32 ActiveCols[APBDEV_KBC_NUM_COLS];
    NvU32 MaskRow;
    NvU32 MaskColumn;
    hKbc->IsSelectKeysWkUpEnabled = NvOdmKbcIsSelectKeysWkUpEnabled(
                                                                &RowNums,
                                                                &ColNums,
                                                                &NumOfKeys);
    if (!hKbc->IsSelectKeysWkUpEnabled)
        return;

    if (hKbc->IsKeyMaskingSupported)
    {
        for (i = 0; i < (hKbc->pConnectivity->NumAddress); i++)
        {
            if (hKbc->pConnectivity->AddressList[i].Interface != NvOdmIoModule_Kbd)
                continue;

            PinNumber = hKbc->pConnectivity->AddressList[i].Address;
            IsColumn = (NvBool)hKbc->pConnectivity->AddressList[i].Instance;

            if (IsColumn)
                ActiveCols[ColumnConnectionNumber++] = PinNumber;
            else
                ActiveRows[RowConnectionNumber++] = PinNumber;
        }


        for(i = 0; i < RowConnectionNumber; ++i)
            hKbc->KbcRowMaskReg[i] = (0xFF >> (APBDEV_KBC_NUM_COLS - ColumnConnectionNumber));

        for (i = 0; i < NumOfKeys; ++i)
        {
            // Get the row and column number index
            for (MaskRow = 0; MaskRow < RowConnectionNumber; ++MaskRow)
            {
                if (RowNums[i] == ActiveRows[MaskRow])
                    break;
            }

            for (MaskColumn = 0; MaskColumn < ColumnConnectionNumber; ++MaskColumn)
            {
                if (ColNums[i] == ActiveCols[MaskColumn])
                    break;
            }

            // Check for valid row and colum mask index
            NV_ASSERT(MaskRow < RowConnectionNumber);
            NV_ASSERT(MaskColumn < ColumnConnectionNumber);

            // Make clear to the corresponding row and column
            hKbc->KbcRowMaskReg[MaskRow] &= (~(1 << MaskColumn));
        }
    }
    else
    {
        /*
         * The variable RowCfgFldSize contains the number of bits needed to
         * configure a Gpio pins as a Row. This includes the number of bits for the
         * Row number as well as a Flag bit to indicate whether it has been mapped
         * as a row.
         */
        RowCfgFldSize = NV_FIELD_SIZE(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_NUM_RANGE) +
                        NV_FIELD_SIZE(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_EN_RANGE);
        // Caluclating the Mask for the RowCfgFldSize number of bits
        RowCfgMask = (NV_FIELD_MASK(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_NUM_RANGE) <<
                        NV_FIELD_SIZE(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_EN_RANGE)) |
                        NV_FIELD_MASK(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_EN_RANGE);
        /*
         * The variable ColCfgFldSize contains the number of bits needed to
         * configure a Gpio pins as a Column. This includes the number of bits
         * for the Col number as well as a Flag bit to indicate whether it
         * has been mapped as a Columns.
         */
        ColCfgFldSize = NV_FIELD_SIZE(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_NUM_RANGE) +
                        NV_FIELD_SIZE(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_EN_RANGE);
        // Caluclating the Mask for the ColCfgFldSize number of bits
        ColCfgMask = (NV_FIELD_MASK(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_NUM_RANGE) <<
                        NV_FIELD_SIZE(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_EN_RANGE)) |
                        NV_FIELD_MASK(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_EN_RANGE);
        // Number of Gpio pins that can be configured as a row per each register
        RowsPerReg = 32/RowCfgFldSize;
        // Number of Gpio pins that can be configured as a col per each register
        ColsPerReg = 32/ColCfgFldSize;

        for (i = 0; i < (hKbc->pConnectivity->NumAddress); i++)
        {
            if (hKbc->pConnectivity->AddressList[i].Interface != NvOdmIoModule_Kbd)
                continue;

            PinNumber = hKbc->pConnectivity->AddressList[i].Address;
            IsColumn = (NvBool)hKbc->pConnectivity->AddressList[i].Instance;

            if (IsColumn)
            {
                for (j=0; j < NumOfKeys; j++)
                {
                    if (ColNums[j] == ColumnConnectionNumber)
                    {
                        IsWakeupEnable = NV_TRUE;
                        break;
                    }
                    else
                    {
                        IsWakeupEnable = NV_FALSE;
                    }
                }
                if (IsWakeupEnable)
                {
                    ShiftVal = (PinNumber%ColsPerReg)*ColCfgFldSize;
                    NewVal = ((ColumnConnectionNumber << 1) | 0x1);
                    NewVal = NewVal << ShiftVal;
                    hKbc->KbcSuspendedColReg[PinNumber/ColsPerReg] &=
                                            (~(ColCfgMask << ShiftVal));
                    hKbc->KbcSuspendedColReg[PinNumber/ColsPerReg] |= NewVal;
                }
                ColumnConnectionNumber++;
            }
            else
            {

                for (j=0; j<NumOfKeys; j++)
                {
                    if (RowNums[j] == RowConnectionNumber)
                    {
                        IsWakeupEnable = NV_TRUE;
                        break;
                    }
                    else
                    {
                        IsWakeupEnable = NV_FALSE;
                    }
                }
                if (IsWakeupEnable)
                {
                    ShiftVal = (PinNumber%RowsPerReg)*RowCfgFldSize;
                    NewVal = (((RowConnectionNumber) << 1) | 0x1);
                    NewVal = NewVal << ShiftVal;
                    hKbc->KbcSuspendedRowReg[PinNumber/RowsPerReg] &=
                                                        (~(RowCfgMask << ShiftVal));
                    hKbc->KbcSuspendedRowReg[PinNumber/RowsPerReg] |= NewVal;
                }
                RowConnectionNumber++;
            }
        }
    }
}

static void ConfigRowAndColumnInfo(NvDdkKbcHandle hKbc)
{
    NvU32 i;
    NvU32 PinNumber;
    NvU32 NewVal;
    NvU32 ShiftVal;
    NvU32 ColCfg[3] = {0, 0, 0};
    NvU32 RowCfg[4] = {0, 0, 0, 0};
    NvU32 RowCfgFldSize;
    NvU32 ColCfgFldSize;
    NvU32 RowCfgMask;
    NvU32 ColCfgMask;
    NvU32 RowsPerReg;
    NvU32 ColsPerReg;
    NvBool IsColumn;
    NvU32 RowConnectionNumber = 0;
    NvU32 ColumnConnectionNumber = 0;

    SuspendedConfiguration(hKbc);

    /*
     * The variable RowCfgFldSize contains the number of bits needed to
     * configure a Gpio pins as a Row. This includes the number of bits for the
     * Row number as well as a Flag bit to indicate whether it has been mapped
     * as a row.
     */
    RowCfgFldSize = NV_FIELD_SIZE(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_NUM_RANGE) +
                    NV_FIELD_SIZE(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_EN_RANGE);
    // Caluclating the Mask for the RowCfgFldSize number of bits
    RowCfgMask = (NV_FIELD_MASK(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_NUM_RANGE) <<
                    NV_FIELD_SIZE(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_EN_RANGE)) |
                    NV_FIELD_MASK(APBDEV_KBC_ROW_CFG0_0_GPIO_0_ROW_EN_RANGE);
    /*
     * The variable ColCfgFldSize contains the number of bits needed to
     * configure a Gpio pins as a Column. This includes the number of bits
     * for the Col number as well as a Flag bit to indicate whether it
     * has been mapped as a Columns.
     */
    ColCfgFldSize = NV_FIELD_SIZE(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_NUM_RANGE) +
                    NV_FIELD_SIZE(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_EN_RANGE);
    // Caluclating the Mask for the ColCfgFldSize number of bits
    ColCfgMask = (NV_FIELD_MASK(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_NUM_RANGE) <<
                    NV_FIELD_SIZE(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_EN_RANGE)) |
                    NV_FIELD_MASK(APBDEV_KBC_COL_CFG0_0_GPIO_0_COL_EN_RANGE);
    // Number of Gpio pins that can be configured as a row per each register
    RowsPerReg = 32/RowCfgFldSize;
    // Number of Gpio pins that can be configured as a col per each register
    ColsPerReg = 32/ColCfgFldSize;

    for (i = 0; i < (hKbc->pConnectivity->NumAddress); i++)
    {
        if (hKbc->pConnectivity->AddressList[i].Interface != NvOdmIoModule_Kbd)
            continue;

        PinNumber = hKbc->pConnectivity->AddressList[i].Address;
        IsColumn = (NvBool)hKbc->pConnectivity->AddressList[i].Instance;

        if (IsColumn)
        {
            ShiftVal = (PinNumber%ColsPerReg)*ColCfgFldSize;
            NewVal = ((ColumnConnectionNumber << 1) | 0x1);
            NewVal = NewVal << ShiftVal;
            ColCfg[PinNumber/ColsPerReg] &= (~(ColCfgMask << ShiftVal));
            ColCfg[PinNumber/ColsPerReg] |= NewVal;
            ColumnConnectionNumber++;
        }
        else
        {
            ShiftVal = (PinNumber%RowsPerReg)*RowCfgFldSize;
            NewVal = (((RowConnectionNumber) << 1) | 0x1);
            NewVal = NewVal << ShiftVal;
            RowCfg[PinNumber/RowsPerReg] &= (~(RowCfgMask << ShiftVal));
            RowCfg[PinNumber/RowsPerReg] |= NewVal;
            RowConnectionNumber++;
        }
    }
    KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG0, ColCfg[0]);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG1, ColCfg[1]);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG2, ColCfg[2]);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG0, RowCfg[0]);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG1, RowCfg[1]);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG2, RowCfg[2]);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG3, RowCfg[3]);
    hKbc->KbcRegColCfg0 = ColCfg[0];
    hKbc->KbcRegColCfg1 = ColCfg[1];
    hKbc->KbcRegColCfg2 = ColCfg[2];
    hKbc->KbcRegRowCfg0 = RowCfg[0];
    hKbc->KbcRegRowCfg1 = RowCfg[1];
    hKbc->KbcRegRowCfg2 = RowCfg[2];
    hKbc->KbcRegRowCfg3 = RowCfg[3];

    hKbc->NumberOfRows = RowConnectionNumber;
    hKbc->NumberOfColumns = ColumnConnectionNumber;

    NV_ASSERT(RowConnectionNumber);
    NV_ASSERT(ColumnConnectionNumber);

    // We have minimum row count limitation on AP15/20. Is it satisfy?
    if(hKbc->MinRowCount > RowConnectionNumber)
    {
        NV_ASSERT(!"Minimum row count does not satisfy");
    }

}


static NvU32 GetKeyCode(NvDdkKbcHandle hKbc, NvU32 ColumnNumber, NvU32 RowNumber) {
    NvU32 KeyCode;
    KeyCode = NvOdmKbcGetKeyCode(RowNumber, ColumnNumber,
                    hKbc->NumberOfRows, hKbc->NumberOfColumns);
    return KeyCode;
}

static NvU32 ExtractPressedKeys(NvDdkKbcHandle hKbc, NvU32 index)
{
    NvU32 ColumnNumber;
    NvU32 RowNumber;
    NvU32 ValidKeyCount = 0;
    NvU32 KeyCode;
    NvU32 i=0;

#ifdef TEGRA_12x_SOC
    // In T124, this register holds the entries for 0 to 2 keys
    KeyCode = KBC_REG_READ32(hKbc->KEPInfo.hKbc->pVirtualAddress, KP_ENT0);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 0, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 1, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 2, KeyCode);

    // In T124, this register holds the entries for 3 to 5 keys
    KeyCode = KBC_REG_READ32(hKbc->KEPInfo.hKbc->pVirtualAddress, KP_ENT1);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 3, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 4, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 5, KeyCode);
#else
    KeyCode = KBC_REG_READ32(hKbc->KEPInfo.hKbc->pVirtualAddress, KP_ENT0);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 0, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 1, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 2, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 0, 3, KeyCode);

    KeyCode = KBC_REG_READ32(hKbc->KEPInfo.hKbc->pVirtualAddress, KP_ENT1);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 4, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 5, KeyCode);
    GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 6, KeyCode);

    // If more than 7 events are available then read that also.
    if (hKbc->MaxEvents > 7)
    {
        GET_KEY_CODE(hKbc, ColumnNumber, RowNumber, 1, 7, KeyCode);
    }
#endif
    ValidKeyCount = NvOdmKbcFilterKeys(
                            hKbc->RowNumbers,
                            hKbc->ColNumbers,
                            ValidKeyCount);

    for (i=0; i<ValidKeyCount; i++)
    {
        hKbc->KEPInfo.PressedKeys[i] = GetKeyCode(hKbc,
                                                  hKbc->ColNumbers[i],
                                                  hKbc->RowNumbers[i]);
    }
    return ValidKeyCount;
}

static NvBool IsKeyReleased(NvDdkKbcHandle hKbc, NvU32 Key)
{
    NvBool Error = NV_TRUE;
    NvU32 i;
    for (i = 0; i < hKbc->KEPInfo.CurrentPressedKeyCount; i++)
    {
        if (Key == hKbc->KEPInfo.PressedKeys[i])
        {
            Error = NV_FALSE;
            break;
        }
    }
    return Error;
}

static NvBool IsKeyEventAlreadyDispatched(NvDdkKbcHandle hKbc, NvU32 Key)
{
    NvBool Error = NV_FALSE;
    NvU32 i;
    for (i = 0; i < hKbc->KEPInfo.PreviouslyPressedKeyCount; i++)
    {
        if (Key == hKbc->KEPInfo.PreviouslyPressedKeys[i])
        {
            Error = NV_TRUE;
            break;
        }
    }
    return Error;
}

static void
GetKeyEvents(
    NvDdkKbcHandle hKbc,
    NvU32* pKeyCount,
    NvU32* pKeyCodes,
    NvDdkKbcKeyEvent* pKeyEvents)
{
    NvU32 i;
    *pKeyCount = 0;

    /* This function compares the the currently pressed to keys to the Keys
     * that were pressed in the previous call of this function. Based on this
     * comparision the Key Release and Key Press events are queued in the
     * KeyEventsToDispatch Queue. */
    if (hKbc->KEPInfo.PreviouslyPressedKeyCount)
    {
        for (i = 0; i < hKbc->KEPInfo.PreviouslyPressedKeyCount; i++)
        {
            if (IsKeyReleased(hKbc, hKbc->KEPInfo.PreviouslyPressedKeys[i]))
            {
                pKeyCodes[*pKeyCount] = hKbc->KEPInfo.PreviouslyPressedKeys[i];
                pKeyEvents[*pKeyCount] = NvDdkKbcKeyEvent_KeyRelease;
                *pKeyCount = *pKeyCount + 1;
            }
        }
    }

    for (i = 0; i < hKbc->KEPInfo.CurrentPressedKeyCount; i++)
    {
        if (hKbc->KEPInfo.PreviouslyPressedKeyCount)
        {
            // Check if the KeyPress has already been dispatched in the queue
            if (IsKeyEventAlreadyDispatched(hKbc, hKbc->KEPInfo.PressedKeys[i]))
            {
                // if the Event has already been dispatched, goto the next
                // iteration or else add the key press event to queue.
                continue;
            }
        }

        pKeyCodes[*pKeyCount] = hKbc->KEPInfo.PressedKeys[i];
        pKeyEvents[*pKeyCount] = NvDdkKbcKeyEvent_KeyPress;
        *pKeyCount = *pKeyCount + 1;
    }

    // Move the Current Pressed Keys to Previously Pressed Keys array
    for (i = 0; i < hKbc->KEPInfo.CurrentPressedKeyCount; i++)
    {
        hKbc->KEPInfo.PreviouslyPressedKeys[i] = hKbc->KEPInfo.PressedKeys[i];
    }
    hKbc->KEPInfo.PreviouslyPressedKeyCount = hKbc->KEPInfo.CurrentPressedKeyCount;
}

static void KbcIsr(void* args)
{
    NvU32 RegValue;
    NvDdkKbcHandle hKbc = args;

    // Disable the interrupt first
    RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
    RegValue = NV_FLD_SET_DRF_NUM(APBDEV_KBC, CONTROL, FIFO_CNT_INT_EN, 0, RegValue);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, RegValue);

    // Clear the interrupt.
    RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, INT);
    RegValue = NV_FLD_SET_DRF_DEF(APBDEV_KBC, INT, FIFO_CNT_INT_STATUS,
                   DEFAULT_MASK, RegValue);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, INT, RegValue);
    // Signal the client sema.
    NvOsSemaphoreSignal(hKbc->KEPInfo.KbcClientSema);

    // Call interrupt done.
    // Fixme!! Not calling here as the KBC is generating the spurious interrupt.
    // NvRmInterruptDone(hKbc->InterruptHandle);
}

static void FindDelayBetweenScans(NvDdkKbcHandle hKbc)
{
    NvU32 RptTime;
    NvU32 DebounceVal;
    NvU32 DelayInClocks;

    // Findout the delay need to be given between between scans.
    RptTime = KBC_REG_READ32(hKbc->pVirtualAddress, RPT_DLY);
    DebounceVal = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
    DebounceVal = NV_DRF_VAL(APBDEV_KBC, CONTROL, DBC_CNT, DebounceVal);

    /*
     * The time delay between two consecutive reads is the sum of the
     * repeat time and the time taken for scanning the rows.
     * The time taken for scanning the rows is the product of the
     * number of rows and the Debounce value set (in number of KBC cycles).
     * So we have to wait for this much time before we read the
     * AV_FIFO_CNT value again.
     */
    DelayInClocks = 5 + hKbc->NumberOfRows* (DebounceVal + 16) + RptTime;
    hKbc->DelayBetweenScans = DelayInClocks * KBC_CYCLE_TIME_US;
    hKbc->DelayBetweenScans = (hKbc->DelayBetweenScans + 999)/1000;
}

NvError
NvDdkKbcOpen(
    NvRmDeviceHandle hDevice,
    NvDdkKbcHandle* phKbc)
{
    NvError Error = NvSuccess;
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvU64 KbdGuid = 0;
    NvU32 NumGuid = 0;
    static NvDdkKbcCapability KbcCaps[3];
    NvDdkKbcCapability *pKbcCapability = NULL;
    static NvRmModuleCapability s_KbcCap[] =
    {
            { 1, 0, 0, NvRmModulePlatform_Silicon, &KbcCaps[0]},
            { 1, 1, 0, NvRmModulePlatform_Silicon, &KbcCaps[1]},
            { 1, 2, 0, NvRmModulePlatform_Silicon, &KbcCaps[2]}
    };
    NvOdmPeripheralSearch SearchAttrs[] =
    {
        NvOdmPeripheralSearch_IoModule,
    };
    NvU32 SearchVals[] =
    {
        NvOdmIoModule_Kbd,
    };

    NV_ASSERT(phKbc);
    NV_ASSERT(hDevice);

    *phKbc = NULL;

    /*
     * The NvddkKbcOpen() is called more than once before closing the handle
     * that has been opened before NvError_AlreadyAllocated is returned.
     */
    if (s_hKbc)
    {
        *phKbc = s_hKbc;
        s_hKbc->OpenCount++;
        return NvError_AlreadyAllocated;
    }
    NumGuid = NvOdmPeripheralEnumerate(SearchAttrs, SearchVals,
       NV_ARRAY_SIZE(SearchAttrs), &KbdGuid, 1);
    if (!NumGuid)
        return NvError_ModuleNotPresent;

    pConnectivity = NvOdmPeripheralGetGuid(KbdGuid);

    if (!pConnectivity)
        return NvError_ModuleNotPresent;

    // Allocate memory for the handle.
    s_hKbc = NvOsAlloc(sizeof(NvDdkKbc));
    if (!s_hKbc)
    {
        return NvError_InsufficientMemory;
    }
    else
    {
        NvOsMemset(s_hKbc, 0, sizeof(NvDdkKbc));
    }

    s_hKbc->pConnectivity = pConnectivity;
    s_hKbc->RmDevHandle = hDevice;

    NvRmModuleGetBaseAddress(s_hKbc->RmDevHandle,
                        NVRM_MODULE_ID(NvRmModuleID_Kbc, 0),
                        &s_hKbc->BaseAddress, &s_hKbc->BankSize);

    Error = NvOsPhysicalMemMap(s_hKbc->BaseAddress, s_hKbc->BankSize,
                NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE,
                (void **)&s_hKbc->pVirtualAddress);
    if (Error != NvSuccess)
        goto cleanup_exit;

    KbcCaps[0].IsKeyMaskingSupported = NV_FALSE;
    KbcCaps[0].MinRowCount = 2;
    KbcCaps[0].MaxEvents = 7;

    KbcCaps[1].IsKeyMaskingSupported = NV_TRUE;
    KbcCaps[1].MinRowCount = 2;
    KbcCaps[1].MaxEvents = 8;

    KbcCaps[2].IsKeyMaskingSupported = NV_TRUE;
    KbcCaps[2].MinRowCount = 1;
    KbcCaps[2].MaxEvents = 8;

    // Getting the Mojor and minor version numbers of kbc controller
    Error = NvRmModuleGetCapabilities(hDevice,
                                        NVRM_MODULE_ID(NvRmModuleID_Kbc,0),
                                        s_KbcCap,
                                        NV_ARRAY_SIZE(s_KbcCap),
                                        (void **)&(pKbcCapability));
    if (Error)
        goto cleanup_exit;

    s_hKbc->IsKeyMaskingSupported = pKbcCapability->IsKeyMaskingSupported;
    s_hKbc->MinRowCount = pKbcCapability->MinRowCount;
    s_hKbc->MaxEvents = pKbcCapability->MaxEvents;

    /*
     * Registering with RmPower. The Power and Clock to the Module are enabled
     * in the NvDdkKbcStart() function.
     */
    Error = NvRmPowerRegister(s_hKbc->RmDevHandle, NULL, &s_hKbc->KbcRmPowerClientID);
    if (Error)
        goto cleanup_exit;
    *phKbc = s_hKbc;
    s_hKbc->OpenCount = 1;
    return NvSuccess;

cleanup_exit:
    // Freeing allocated stuff in reverse order.
    NvOsPhysicalMemUnmap(s_hKbc->pVirtualAddress, s_hKbc->BankSize);
    NvOsFree(s_hKbc);
    s_hKbc = *phKbc = NULL;
    return Error;
}

void NvDdkKbcClose(NvDdkKbcHandle hKbc)
{
    if (!hKbc)
        return;

    if (!hKbc->OpenCount)
        return;

    hKbc->OpenCount--;
    if (hKbc->OpenCount)
        return;

    // Release the kbc handle
    if (hKbc->KbcStarted)
        NvDdkKbcStop(hKbc);

    // Reset the KBC controller to clear all previous status and not to
    // leave any residuals.
    NvRmModuleReset(hKbc->RmDevHandle, NVRM_MODULE_ID(NvRmModuleID_Kbc, 0));

    // Disabling Module power
    NV_ASSERT((NvRmPowerVoltageControl(hKbc->RmDevHandle,
        NvRmModuleID_Kbc,
        hKbc->KbcRmPowerClientID,
        NvRmVoltsOff,
        NvRmVoltsOff,
        NULL,
        0,
        NULL)) == NvSuccess);
    // Un-Registering as Power-client
    NvRmPowerUnRegister(hKbc->RmDevHandle, hKbc->KbcRmPowerClientID);
    // unmap the memory that was mapped during the open call.
    NvOsPhysicalMemUnmap(hKbc->pVirtualAddress, hKbc->BankSize);
    // free the handle memory.
    NvOsFree(hKbc);
    s_hKbc = NULL;
}

NvError NvDdkKbcStart(NvDdkKbcHandle hKbc, NvOsSemaphoreHandle SemaphoreId)
{
    NvError Error = NvSuccess;
    NvU32 RegValue;
    NvU32 IrqList;
    NvU32 RepeatCycleTime;
    NvU32 RepeatCycleTimeInKbcCycles;
    NvU32 DebounceTimeInKbcCycles;
    NvOsInterruptHandler IntHandlers = KbcIsr;

    NV_ASSERT(hKbc);

    // store the sema to be signaled on key events.
    hKbc->KEPInfo.KbcClientSema = SemaphoreId;
    hKbc->KEPInfo.hKbc = hKbc;

    // Start the Kbc, if it has not been already started.
    if (hKbc->KbcStarted)
    {
        /*
         * If it is already started, return error. It cannot be started
         * multiple times.
         */
        Error = NvError_AlreadyAllocated;
        goto exit_gracefully;
    }

    Error = NvddkPrivKbcEnableClock(hKbc);
    if (Error)
        goto FailureExit;

    // Reset the KBC controller to clear all previous status.
    NvRmModuleReset(hKbc->RmDevHandle, NVRM_MODULE_ID(NvRmModuleID_Kbc, 0));

    hKbc->KbcStarted = NV_TRUE;
    // Register with RM for the interrupt.
    if (!hKbc->InterruptHandle)
    {
        IrqList = NvRmGetIrqForLogicalInterrupt(hKbc->RmDevHandle,
                    NvRmModuleID_Kbc, 0);
        Error = NvRmInterruptRegister(hKbc->RmDevHandle, 1, &IrqList,
                    & IntHandlers, hKbc, &hKbc->InterruptHandle, NV_TRUE);
        if (Error)
            goto FailureExit;
    }

    /*
     *Config the row info, column info, de-bounce time etc., which
     * comes from ODM.
     */
    ConfigRowAndColumnInfo(hKbc);
    // Power on the keyboard interface if required
    SetPowerOnKeyboard(hKbc, NV_TRUE);
    // Set de-bounce value.
    NvOdmKbcGetParameter(NvOdmKbcParameter_DebounceTime, 1,
        &DebounceTimeInKbcCycles);
    RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
    RegValue = NV_FLD_SET_DRF_NUM(APBDEV_KBC, CONTROL, DBC_CNT,
                       DebounceTimeInKbcCycles, RegValue);

    KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, RegValue);

    // Set RepeatCycleTime value.
    NvOdmKbcGetParameter(NvOdmKbcParameter_RepeatCycleTime, 1,
        &RepeatCycleTime);
    RepeatCycleTimeInKbcCycles = ((RepeatCycleTime*1000)/KBC_CYCLE_TIME_US);
    RegValue = NV_DRF_NUM(APBDEV_KBC, RPT_DLY, RPT_DLY_VAL,
                   RepeatCycleTimeInKbcCycles);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, RPT_DLY, RegValue);
    // enable the kbc interrupt.
    RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
    RegValue = NV_FLD_SET_DRF_NUM(APBDEV_KBC, CONTROL, FIFO_CNT_INT_EN, 1,
                   RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(APBDEV_KBC, CONTROL, FIFO_TH_CNT, 1,
                   RegValue);
    // enable the kbc.
    RegValue = NV_FLD_SET_DRF_DEF(APBDEV_KBC, CONTROL, EN, ENABLE, RegValue);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, RegValue);
    // Findout the delay need to be given between between scans.
    FindDelayBetweenScans(hKbc);
    NvDdkPrivKbcSuspendController(hKbc);
    return NvSuccess;

FailureExit:
    hKbc->KbcStarted = NV_FALSE;
exit_gracefully:
    return Error;
}

NvError NvDdkKbcStop(NvDdkKbcHandle hKbc)
{
    NvU32 RegValue;
    NvError Error;

    NV_ASSERT(hKbc);
    NV_ASSERT(hKbc->KbcStarted);

    // disable the kbc interrupt.
    RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
    RegValue = NV_FLD_SET_DRF_DEF(APBDEV_KBC, CONTROL, KP_INT_EN,
                   DISABLE, RegValue);
    // disable the kbc.
    RegValue = NV_FLD_SET_DRF_DEF(APBDEV_KBC, CONTROL, EN, DISABLE, RegValue);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, RegValue);

    if (hKbc->KbcStarted == NV_TRUE)
    {
        NvRmInterruptUnregister(hKbc->RmDevHandle, hKbc->InterruptHandle);
        hKbc->InterruptHandle = NULL;
        hKbc->KbcStarted = NV_FALSE;
        /*
         * Clear it so that on next start, it will not have previous
         * left over values.
         */
        NvOsMemset(&hKbc->KEPInfo, 0, sizeof(KbcKeyEventProcessInfo));
    }
    Error = NvddkPrivKbcDisableClock(hKbc);
    return Error;
}

void
NvDdkKbcSetRepeatTime(
    NvDdkKbcHandle hKbc,
    NvU32 RepeatTimeMs)
{
    NvU32 RegValue;
    NvU32 TimeInKbcCycles = (RepeatTimeMs *1000) / KBC_CYCLE_TIME_US;
    hKbc->KEPInfo.RepeatDelayMs = RepeatTimeMs;

    NV_ASSERT(hKbc);
    NV_ASSERT(RepeatTimeMs);
    RegValue = NV_DRF_NUM(APBDEV_KBC, RPT_DLY, RPT_DLY_VAL, TimeInKbcCycles);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, RPT_DLY, RegValue);
    // Findout the delay need to be given between between scans.
    FindDelayBetweenScans(hKbc);
}

NvU32
NvDdkKbcGetKeyEvents(
    NvDdkKbcHandle hKbc,
    NvU32* pKeyCount,
    NvU32* pKeyCodes,
    NvDdkKbcKeyEvent* pKeyEvents)
{
    NvU32 RegValue;
    NvU32 KeyCount;
    NvError Error;

    Error = NvDdkPrivKbcResumeController(hKbc);
    if (Error)
    {
        *pKeyCount = 0;
        RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
        RegValue = NV_FLD_SET_DRF_NUM(APBDEV_KBC, CONTROL,
                        FIFO_CNT_INT_EN, 1, RegValue);
        KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, RegValue);
        // Call interrupt done here to reenable the interrupt.
        NvRmInterruptDone(hKbc->InterruptHandle);
        return 0;
    }

    /*
     * Read the AV_FIFO_CNT value. A non-zero value indicates that
     * new key press events are available in the Key Press
     * Entry Reg.
     */
    hKbc->KEPInfo.CurrentPressedKeyCount = 0;
    KeyCount = KBC_REG_READ32(hKbc->pVirtualAddress, INT);
    KeyCount = NV_DRF_VAL(APBDEV_KBC, INT, AV_FIFO_CNT, KeyCount);

    if (KeyCount > 0)
    {
        // Read the Key-Press Entries Register to find the new key
        // presses and store them in the PressedKeys[] array.
        hKbc->KEPInfo.CurrentPressedKeyCount = ExtractPressedKeys(hKbc, 0);
        /*
         * Compare the newly pressed keys to the Keys pressed in
         * the previous iteration and signal the client sema if
         * there are any new Key Press or Release events
         */
        GetKeyEvents(hKbc, pKeyCount, pKeyCodes, pKeyEvents);

        // If the number of keys in fifo is more than 1 then need not to wait
        // till full scan is complete as the client can read it immediately.
        // Can not return 0 as return of 0 to the client means there is no
        // more key event in the fifo so returning 1.
        if(KeyCount > 1)
            return 1;
        else
            return hKbc->DelayBetweenScans;
    }
    else
    {
        // Since there are no keys pressed, send out the Release
        // events
        GetKeyEvents(hKbc, pKeyCount, pKeyCodes, pKeyEvents);
        RegValue = KBC_REG_READ32(hKbc->pVirtualAddress, CONTROL);
        RegValue = NV_FLD_SET_DRF_NUM(APBDEV_KBC, CONTROL,
                        FIFO_CNT_INT_EN, 1, RegValue);
        KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, RegValue);
        // Call interrupt done here to reenable the interrupt.
        NvRmInterruptDone(hKbc->InterruptHandle);
        NvDdkPrivKbcSuspendController(hKbc);
        return 0;
    }
}

NvError NvDdkKbcSuspend(NvDdkKbcHandle hKbc)
{
    NvU32 RowCount;
    NV_ASSERT(hKbc);
    if (hKbc->IsSelectKeysWkUpEnabled)
    {
        if (hKbc->IsKeyMaskingSupported)
        {
            for (RowCount =0; RowCount < hKbc->NumberOfRows; ++RowCount)
                KBC_REG_WRITE32(hKbc->pVirtualAddress + RowCount,
                                           ROW0_MASK, hKbc->KbcRowMaskReg[RowCount]);
        }
        else
        {
            KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG0, hKbc->KbcSuspendedColReg[0]);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG1, hKbc->KbcSuspendedColReg[1]);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG2, hKbc->KbcSuspendedColReg[2]);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG0, hKbc->KbcSuspendedRowReg[0]);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG1, hKbc->KbcSuspendedRowReg[1]);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG2, hKbc->KbcSuspendedRowReg[2]);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG3, hKbc->KbcSuspendedRowReg[3]);
        }
    }

    // We are not suspending the kbc driver here because the driver is suspended
    // after every keypress. We need not suspend the driver again here
    return NvSuccess;
}

NvError NvDdkKbcResume(NvDdkKbcHandle hKbc)
{
    NvU32 RowCount;
    NV_ASSERT(hKbc);
    if (hKbc->IsSelectKeysWkUpEnabled)
    {
        if (hKbc->IsKeyMaskingSupported)
        {
            for (RowCount =0; RowCount < hKbc->NumberOfRows; ++RowCount)
                KBC_REG_WRITE32(hKbc->pVirtualAddress + RowCount, ROW0_MASK, 0);
        }
        else
        {
            KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG0, hKbc->KbcRegColCfg0);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG1, hKbc->KbcRegColCfg1);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG2, hKbc->KbcRegColCfg2);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG0, hKbc->KbcRegRowCfg0);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG1, hKbc->KbcRegRowCfg1);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG2, hKbc->KbcRegRowCfg2);
            KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG3, hKbc->KbcRegRowCfg3);
        }
    }

    // We resume the kbc controller only when there is key press detected. So
    // there is no need to resume the controller here.
    return NvSuccess;
}

void NvDdkLp0ConfigureKbc(NvDdkKbcHandle hKbc)
{
#ifdef TEGRA_11x_SOC
    KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG0, 0x1461);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG1, 0x531000);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, 0x00047FFB);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, TO_CNT, 0x27100);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, INT, 0x04);
#endif
#ifdef TEGRA_3x_SOC
    KBC_REG_WRITE32(hKbc->pVirtualAddress, CONTROL, 0x680B);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, ROW_CFG0, 0x1461);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, COL_CFG2, 0x0531);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, TO_CNT, 0xE6C3);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, INIT_DLY, 0x05);
    KBC_REG_WRITE32(hKbc->pVirtualAddress, RPT_DLY, 0x01);
#endif
}

void NvDdkLp0KbcResume(void)
{
    NvRmPowerRegister(s_hKbc->RmDevHandle, NULL, &s_hKbc->KbcRmPowerClientID);
    NvRmPowerModuleClockControl(s_hKbc->RmDevHandle,
                NvRmModuleID_Kbc,
                s_hKbc->KbcRmPowerClientID,
                NV_TRUE);
}
