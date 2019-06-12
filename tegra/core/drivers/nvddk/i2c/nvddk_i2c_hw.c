/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_i2c.h"
#include "nvddk_i2c_debug.h"
#include "nvddk_i2c_hw.h"
#include "nvddk_i2c_controller.h"
#include "nvddk_i2c_apb.h"

static NvDdkI2cInfo s_I2cInstanceInfo[NvDdkMaxI2cInstances] = {
    {0, NV_FALSE, 0, {NV_FALSE, NV_FALSE, NV_FALSE}},
    {1, NV_FALSE, 0, {NV_FALSE, NV_FALSE, NV_FALSE}},
    {2, NV_FALSE, 0, {NV_FALSE, NV_FALSE, NV_FALSE}},
    {3, NV_FALSE, 0, {NV_FALSE, NV_FALSE, NV_FALSE}},
    {4, NV_FALSE, 0, {NV_FALSE, NV_FALSE, NV_FALSE}},
    {5, NV_FALSE, 0, {NV_FALSE, NV_FALSE, NV_FALSE}}          };

/* TODO: Move it to a common lib */
NvU32 NvDdkI2cGetChipId(void)
{
    NvU32 RegData;
    static NvU32 s_ChipId = 0;

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);

    if (s_ChipId == 0)
    {
        RegData = NV_READ32(APB_MISC_PA_BASE + APB_MISC_GP_HIDREV_0);
        s_ChipId = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegData);
    }

    return s_ChipId;
}

void NvDdkI2cGetCapabilities(NvDdkI2cHandle hI2c)
{
    NvU32 ChipId = 0;

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);
    NV_ASSERT(hI2c != NULL);

    ChipId = NvDdkI2cGetChipId();
    switch(ChipId)
    {
        case 0x14:
        case 0x40:
            hI2c->Capabilities.NeedLoadBitFields = NV_TRUE;
        case 0x35:
            hI2c->Capabilities.SupportsBusClear = NV_TRUE;
            hI2c->Capabilities.NeedsClDvfsEnabled = NV_TRUE;
        case 0x30:
            break;
        default:
            NV_ASSERT(!"Invalid Chip");
    }
}

NvDdkI2cHandle NvDdkI2cOpen(NvU32 Instance)
{
    NvDdkI2cHandle hI2c = NULL;
    static NvU32 s_I2cOffsetMap[] = { NV_I2C1_OFFSET, NV_I2C2_OFFSET,
                                      NV_I2C3_OFFSET, NV_I2C4_OFFSET,
                                      NV_I2C5_OFFSET, NV_I2C6_OFFSET};

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, Instance);
    NV_ASSERT(Instance != 0);

    if (Instance >= NvDdkMaxI2cInstances)
        goto fail;

    hI2c = &s_I2cInstanceInfo[Instance];

    if (hI2c->IsInitialized)
        return hI2c;

    NvDdkI2cResetController(Instance);
    hI2c->Instance = Instance;
    hI2c->Offset = s_I2cOffsetMap[Instance - NvDdkI2c1];
    NvDdkI2cGetCapabilities(hI2c);

    if (Instance == NvDdkI2c5 && hI2c->Capabilities.NeedsClDvfsEnabled)
        NvDdkI2cEnableClDvfs();

    hI2c->IsInitialized = NV_TRUE;
    return hI2c;

fail:
    return NULL;
}

NvError NvDdkI2cConfigLoadBits(const NvDdkI2cHandle hI2c)
{
    NvU32 LoadConfig = 0;
    NvU32 LoadConfigStatus = 0;
    NvU32 ControllerOffset = 0;
    NvS32 TimeOut = CNFG_LOAD_TIMEOUT_US;

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);
    NV_ASSERT(hI2c != NULL);

    ControllerOffset = hI2c->Offset;
    LoadConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_CONFIG_LOAD, MSTR_CONFIG_LOAD,
                                    ENABLE, LoadConfig);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CONFIG_LOAD, LoadConfig);

    do
    {
        NvDdkI2cStallUs(1);
        TimeOut--;

        if (TimeOut <= 0)
        {
            DDK_I2C_ERROR_PRINT("Load bit field time out");
            return NvError_Timeout;
        }

        LoadConfigStatus = NV_DDK_I2C_REG_READ(ControllerOffset,
                                               I2C_CONFIG_LOAD);
    } while (LoadConfigStatus & LoadConfig);

    return NvSuccess;
}

NvError
NvDdkI2cInitTransaction(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU32 DataLen,
    NvBool IsWrite)
{
    NvError e = NvSuccess;
    NvU32 Config = 0;
    NvU32 SlaveAddress = 0;
    NvU32 ControllerOffset;

    NV_ASSERT(hSlave != NULL);
    NV_ASSERT(hI2c != NULL);

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);

    ControllerOffset = hI2c->Offset;

    Config = NV_DRF_DEF(I2C, I2C_CNFG, NEW_MASTER_FSM, ENABLE);

    if (!IsWrite)
        Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, CMD1, ENABLE, Config);

    if (hSlave->Flags & NVDDK_I2C_NOACK)
        Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, NOACK, ENABLE, Config);

    if (hSlave->Flags & NVDDK_I2C_SEND_START_BYTE)
        Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, START, ENABLE, Config);

    if (!(hSlave->Flags & NVDDK_I2C_10BIT_ADDRESS))
    {
        SlaveAddress = hSlave->Address & 0xFE;

        if (!IsWrite)
            SlaveAddress = SlaveAddress | 0x01;
    }
    else
    {
        Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, A_MOD,
                                    TEN_BIT_DEVICE_ADDRESS, Config);
    }

    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_DATA1, 0);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_DATA2, 0);

    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_ADDR0, SlaveAddress);

    if (DataLen > 0)
        Config = NV_FLD_SET_DRF_NUM(I2C, I2C_CNFG, LENGTH, DataLen - 1, Config);

    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CNFG, Config);

    return e;
}

NvError NvDdkI2cClearBus(NvDdkI2cInstance Instance)
{
    NvError e  = NvSuccess;
    NvU32 TimeOut = BUS_CLEAR_TIMEOUT_US;
    NvU32 ControllerOffset;
    NvU32 Reg;
    NvU32 BusClearDone;
    NvDdkI2cHandle hI2c = NULL;

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, Instance);

    hI2c = NvDdkI2cOpen(Instance);
    if (hI2c == NULL)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_NotInitialized,
            "Error while opening controller"
        );
    }

    if (hI2c->Capabilities.SupportsBusClear == NV_FALSE)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_NotSupported,
            "Bus clear is not supported"
        );
    }

    ControllerOffset = hI2c->Offset;
    /* configure for run bus clear operation to run till 9 clock pulses
     * with immediate stop condition.
     */
    Reg = NV_DRF_DEF(I2C, I2C_BUS_CLEAR_CONFIG, BC_STOP_COND, NO_STOP);
    Reg = NV_FLD_SET_DRF_DEF(I2C, I2C_BUS_CLEAR_CONFIG,
                             BC_TERMINATE, IMMEDIATE, Reg);
    Reg = NV_FLD_SET_DRF_NUM(I2C, I2C_BUS_CLEAR_CONFIG,
                             BC_SCLK_THRESHOLD, 9, Reg);
    Reg = NV_FLD_SET_DRF_NUM(I2C, I2C_BUS_CLEAR_CONFIG, BC_ENABLE, 1, Reg);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_BUS_CLEAR_CONFIG, Reg);

    if (hI2c->Capabilities.NeedLoadBitFields)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvDdkI2cConfigLoadBits(hI2c),
            "Error while loading config bits"
        );
    }

    BusClearDone = NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER,
                              BUS_CLEAR_DONE, SET);
    do
    {
        NvDdkI2cStallMs(BUS_CLEAR_TIMEOUT_STEP);
        TimeOut -= BUS_CLEAR_TIMEOUT_STEP;

        if (TimeOut == 0)
        {
            NVDDK_I2C_CHECK_ERROR_CLEANUP(
                NvError_Timeout,
                "Bus clear timeout"
            );
        }

        Reg = NV_DDK_I2C_REG_READ(ControllerOffset, INTERRUPT_STATUS_REGISTER);
    } while (!(Reg & BusClearDone));

    /* clear the interrupt status register for bus clear done */
    Reg = NV_DDK_I2C_REG_READ(ControllerOffset, INTERRUPT_STATUS_REGISTER);
    Reg = NV_FLD_SET_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER,
                             BUS_CLEAR_DONE, SET, Reg);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, INTERRUPT_STATUS_REGISTER, Reg);

    /* check whether bus clear done is success */
    Reg = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_BUS_CLEAR_STATUS);

    if (!(Reg & NV_DRF_DEF(I2C, I2C_BUS_CLEAR_STATUS, BC_STATUS, CLEARED)))
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_I2cInternalError,
            "Invalid register value in bus clear status register"
        );
    }

fail:
    return e;
}

NvError NvDdkI2cStartTransaction(const NvDdkI2cHandle hI2c, NvU16 TimeOut)
{
    NvError e = NvSuccess;
    NvU32 Config = 0;
    NvU32 ControllerStatus;
    NvU32 ControllerOffset;
    NvU32 TimeStart;

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);
    NV_ASSERT(hI2c != NULL);

    ControllerOffset = hI2c->Offset;

    if (hI2c->Capabilities.NeedLoadBitFields)
        e = NvDdkI2cConfigLoadBits(hI2c);

    Config = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_CNFG);
    Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, SEND, GO, Config);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CNFG, Config);

    TimeStart = NvDdkI2cGetTimeMS();
    do
    {
        ControllerStatus = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_STATUS);

        if (TimeOut == (NvU16) NV_WAIT_INFINITE)
            continue;

        if (NvDdkI2cGetTimeMS() > (TimeStart + TimeOut))
        {
            NVDDK_I2C_CHECK_ERROR_CLEANUP(
                NvError_Timeout,
                "I2C Transaction could not complete within time"
            );
        }
    } while (ControllerStatus & I2C_I2C_STATUS_0_BUSY_FIELD);

fail:
    return e;
}

NvError
NvDdkI2cRead(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 *pBuffer,
    NvU32 NumBytes)
{
    NvError e = NvSuccess;
    NvU32 Byte = 0;
    NvU32 DataRx[2] = {0, 0};
    NvU8 *pData;
    NvU32 ControllerOffset;

    NV_ASSERT(NumBytes <= MAX_I2C_TRANSFER_SIZE);
    NV_ASSERT(hSlave != NULL);
    NV_ASSERT(hI2c != NULL);
    NV_ASSERT(pBuffer != NULL);

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);

    if (!hI2c->IsInitialized)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_NotInitialized, "I2c controller is not initialized"
        );
    }

    ControllerOffset = hI2c->Offset;

    NVDDK_I2C_CHECK_ERROR_CLEANUP(
        NvDdkI2cInitTransaction(hI2c, hSlave, NumBytes, NV_FALSE),
        "Error while initiating read transaction"
    );
    NVDDK_I2C_CHECK_ERROR_CLEANUP(
        NvDdkI2cStartTransaction(hI2c, hSlave->MaxWaitTime),
        "Error while starting read transaction"
    );

    DataRx[0] = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_CMD_DATA1);
    DataRx[1] = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_CMD_DATA2);
    pData = (NvU8 *) &DataRx[0];

    DDK_I2C_DEBUG_PRINT("Data: ");
    while (Byte < NumBytes)
    {
        DDK_I2C_DEBUG_PRINT("%02x ", pData[Byte]);
        pBuffer[Byte] = pData[Byte];
        Byte++;
    }
    DDK_I2C_DEBUG_PRINT("\n");

fail:
    if (e != NvSuccess)
        e = NvError_I2cReadFailed;

    return e;
}

NvError
NvDdkI2cWrite(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 *pBuffer,
    NvU32 NumBytes)
{
    NvError e = NvSuccess;
    NvU32 Byte = 0;
    NvU32 DataTx[2] = {0, 0};
    NvU32 ControllerOffset;
    NvU8 *pData;

    NV_ASSERT(NumBytes < MAX_I2C_TRANSFER_SIZE);
    NV_ASSERT(hSlave != NULL);
    NV_ASSERT(hI2c != NULL);
    NV_ASSERT(pBuffer != NULL);

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);

    if (!hI2c->IsInitialized)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_NotInitialized, "I2c controller is not initialized"
        );
    }

    ControllerOffset = hI2c->Offset;

    NVDDK_I2C_CHECK_ERROR_CLEANUP(
        NvDdkI2cInitTransaction(hI2c, hSlave, NumBytes, NV_TRUE),
        "Error while initiating write transaction"
    );

    pData = (NvU8 *) &DataTx[0];

    DDK_I2C_DEBUG_PRINT("Data: ");
    while (Byte < NumBytes)
    {
        DDK_I2C_DEBUG_PRINT("%02x ", pBuffer[Byte]);
        pData[Byte] = pBuffer[Byte];
        Byte++;
    }
    DDK_I2C_DEBUG_PRINT("\n");

    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_DATA1, DataTx[0]);
    if (NumBytes > MAX_I2C_2SLAVE_TRANSFER_SIZE)
        NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_DATA2, DataTx[1]);

    NVDDK_I2C_CHECK_ERROR_CLEANUP(
        NvDdkI2cStartTransaction(hI2c, hSlave->MaxWaitTime),
        "Error while starting write transaction"
    );

fail:
    if (e != NvSuccess)
        e = NvError_I2cWriteFailed;

    return e;
}

NvError
NvDdkI2cRepeatedStartRead(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 Offset,
    NvU8 *pBuffer)
{
    NvError e = NvSuccess;
    NvU32 DataRx;
    NvU32 DataTx;
    NvU8 *pData;
    NvU32 ControllerOffset;
    NvU32 Config = 0;

    NV_ASSERT(hSlave != NULL);
    NV_ASSERT(hI2c != NULL);
    NV_ASSERT(pBuffer != NULL);

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);

    if (!hI2c->IsInitialized)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_NotInitialized, "I2c controller is not initialized"
        );
    }

    ControllerOffset = hI2c->Offset;

    NvDdkI2cSetFrequency(hI2c, hSlave->MaxClockFreqency);

    NVDDK_I2C_CHECK_ERROR_CLEANUP(
        NvDdkI2cInitTransaction(hI2c, hSlave, 1, NV_TRUE),
        "Error while initiating repeat start read transaction"
    );

    DataTx = Offset;
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_DATA1, DataTx);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CMD_ADDR1,
                         hSlave->Address | 0x01);

    Config = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_CNFG);
    Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, SLV2, ENABLE, Config);
    Config = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, CMD2, ENABLE, Config);
    NV_DDK_I2C_REG_WRITE(ControllerOffset, I2C_CNFG, Config);

    NVDDK_I2C_CHECK_ERROR_CLEANUP(
        NvDdkI2cStartTransaction(hI2c, hSlave->MaxWaitTime),
        "Error while starting repeat start read transaction"
    );

    DataRx = NV_DDK_I2C_REG_READ(ControllerOffset, I2C_CMD_DATA2);
    pData = (NvU8 *) &DataRx;

    DDK_I2C_DEBUG_PRINT("Data: ");
    DDK_I2C_DEBUG_PRINT("%02x ", pData[0]);
    pBuffer[0] = pData[0];
    DDK_I2C_DEBUG_PRINT("\n");

fail:
    if (e != NvSuccess)
        e = NvError_I2cReadFailed;

    return e;
}

