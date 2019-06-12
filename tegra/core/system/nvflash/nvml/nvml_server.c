/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvml_utils.h"
#include "nvml.h"
#include "nvml_device.h"
#include "nvbct.h"
#include "nvuart.h"
#include "nvddk_avp.h"
#include "nvddk_fuse.h"
#include "aos_avp_board_info.h"

NvBootInfoTable *pBitTable;
NvBctHandle g_hBctHandle = NULL;
NvDdkFuseOperatingMode g_OpMode;
NvBootInfoTable BootInfoTable;
#define RSA_KEY_SIZE 256

#define IRAM_START_ADDRESS 0x40000000
#define USB_MAX_TRANSFER_SIZE 2048
#define NVBOOT_MAX_BOOTLOADERS 4 // number of bl info in bct

#ifndef NVBOOT_SSK_ENGINE
#define NVBOOT_SSK_ENGINE NvBootAesEngine_B
#endif

#ifndef NVBOOT_SBK_ENGINE
#define NVBOOT_SBK_ENGINE NvBootAesEngine_A
#endif

#define MISC_PA_BASE    0x70000000
#define MISC_PA_LEN    4096

#define VERIFY_DATA1 0xaa55aa55
#define VERIFY_DATA2 0x55aa55aa

#define SKIP_EVT_SDRAM_BYTES  64

#define VERIFY( exp, code ) \
    if ( !(exp) ) { \
        code; \
    }

#if NO_BOOTROM
static NvU32 NoBRBinary;
#endif

/**
 * Process 3P command
 *
 * Execution of received 3P commands are carried out by the COMMAND_HANDLER's
 *
 * The command handler must handle all errors related to the request internally,
 * except for those that arise from low-level 3P communication and protocol
 * errors.  Only in this latter case is the command handler allowed to report a
 * return value other than NvSuccess.
 *
 * Thus, the return value does not indicate whether the command itself was
 * carried out successfully, but rather that the 3P communication link is still
 * operating properly.  The success status of the command is generally returned
 * to the 3P client by sending a Nv3pCommand_Status message.
 *
 * @param hSock pointer to the handle for the 3P socket state
 * @param command 3P command to execute
 * @param args pointer to command arguments
 *
 * @retval NvSuccess 3P communication link/protocol still valid
 * @retval NvError_* 3P communication link/protocol failure
 */

#define COMMAND_HANDLER(name)                   \
    static NvError                              \
    name(                                       \
        Nv3pSocketHandle hSock,                 \
        Nv3pCommand command,                    \
        void *arg                               \
        )

static NvError
ReportStatus(
    Nv3pSocketHandle hSock,
    char *Message,
    Nv3pStatus Status,
    NvU32 Flags)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus CmdStatus;

    CmdStatus.Code = Status;
    CmdStatus.Flags = Flags;
    if (Message)
        NvOsStrncpy(CmdStatus.Message, Message, NV3P_STRING_MAX);
    else
        NvOsMemset(CmdStatus.Message, 0x0, NV3P_STRING_MAX);
    e = Nv3pCommandSend(hSock, Nv3pCommand_Status, (NvU8 *)&CmdStatus, 0);
    return e;
}

#define NV_CHECK_ERROR_FAIL_3P(expr, status)                                 \
    do                                                                       \
    {                                                                        \
        e = (expr);                                                          \
        if (e != NvSuccess)                                                  \
        {                                                                    \
            Status.Code= status;                                             \
            NvOsSnprintf(                                                    \
                       Message, NV3P_STRING_MAX, "nverror:0x%x (0x%x) %s %d",\
                e & 0xFFFFF, e, __FUNCTION__, __LINE__);                     \
            goto fail;                                                       \
        }                                                                    \
    } while (0)

#define NV_CHECK_ERROR_CLEAN_3P(expr, status)                               \
    do                                                                      \
    {                                                                       \
        e = (expr);                                                         \
        if (e != NvSuccess)                                                 \
        {                                                                   \
            NvOsSnprintf(                                                   \
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);\
                    Status.Code = status;                                   \
            goto clean;                                                     \
        }                                                                   \
    } while (0)

#if !__GNUC__
__asm void
ExecuteBootLoader(NvU32 entry)
{
    CODE32
    PRESERVE8

    IMPORT NvOsDataCacheWritebackInvalidate

    //First, turn off interrupts
    msr cpsr_fsxc, #0xdf

    //Clear TIMER1
    ldr r1, =TIMER1
    mov r2, #0
    str r2, [r1]

    //Clear IRQ handler/SWI handler
    ldr r1, =VECTOR_IRQ
    str r2, [r1]
    ldr r1, =VECTOR_SWI
    str r2, [r1]

    stmfd sp!, {r0}

    //Flush the cache
    bl NvOsDataCacheWritebackInvalidate

    ldmfd sp!, {r0}

    //Jump to BL
    bx r0
}
#else
NV_NAKED void
ExecuteBootLoader(NvU32 entry)
{
    //First, turn off interrupts
    asm volatile("msr cpsr_fsxc, #0xdf");

    asm volatile(
    //Clear TIMER1
    "ldr r1, =0x60005008    \n"
    "mov r2, #0         \n"
    "str r2, [r1]       \n"

    //Clear IRQ handler/SWI handler
    "ldr r1, =0x6000F218\n"
    "str r2, [r1]       \n"
    "ldr r1, =0x6000F208\n"
    "str r2, [r1]       \n"

    "stmfd sp!, {r0}    \n"

    //Flush the cache
    "bl NvOsDataCacheWritebackInvalidate\n"

    "ldmfd sp!, {r0}    \n"

    //Jump to BL
    "bx r0              \n"
    );
}
#endif

static NvU32 NvMlGetChipId(void)
{
    NvU8 *pMisc;
    NvU32 RegData;
    NvU32 ChipId = 0;
    NvError e;

    NV_CHECK_ERROR_CLEANUP(
      NvOsPhysicalMemMap(MISC_PA_BASE, MISC_PA_LEN,
      NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void **)&pMisc)
      );

    RegData = NV_READ32(pMisc + APB_MISC_GP_HIDREV_0);

    NvOsPhysicalMemUnmap(pMisc, MISC_PA_LEN);
    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, RegData);

fail:
    return ChipId;

}

static NvBool CheckIfHdmiEnabled(void)
{
    NvU32 RegValue = NV_READ32 (
                        NV_ADDRESS_MAP_FUSE_BASE + FUSE_RESERVED_PRODUCTION_0);
    NvBool hdmiEnabled = NV_FALSE;

    if (RegValue & 0x2)
        hdmiEnabled = NV_TRUE;

    return hdmiEnabled;
}

static NvBool CheckIfMacrovisionEnabled(void)
{
    NvU32 RegValue = NV_READ32 (
                        NV_ADDRESS_MAP_FUSE_BASE + FUSE_RESERVED_PRODUCTION_0);
    NvBool macrovisionEnabled = NV_FALSE;

    if (RegValue & 0x1)
        macrovisionEnabled = NV_TRUE;

    return macrovisionEnabled;
}

static NvBool CheckIfJtagEnabled(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_SECURITY_MODE_0);
    if (RegData)
        return NV_FALSE;
    else
        return NV_TRUE;
}

static NvBool AllZero(NvU8 *p, NvU32 size)
{
    NvU32 i;
    for (i = 0; i < size; i++)
    {
        if (*p++)
            return NV_FALSE;
    }
    return NV_TRUE;
}

static NvError WriteBctToBit(NvU8 *pBct, NvU32 BctLength)
{
    NvU32 SdramIndex;
    NvU32 Size = sizeof(NvU32);
    NvU32 Instance = 0;
    NvError e =  NvSuccess;
    NvU32 NumSdramSets = 0;
    NvBootSdramParams SdramParams;

    SdramIndex = NvBootStrapSdramConfigurationIndex();

    //Copy the BCT to the location specified by the bootrom
    pBitTable->BctPtr = (NvBootConfigTable*)pBitTable->SafeStartAddr;
    NvOsMemcpy(pBitTable->BctPtr, pBct, BctLength);

    //Intialize bcthandle with Bctptr
    NV_CHECK_ERROR_CLEANUP(
        NvBctInit(&BctLength, pBitTable->BctPtr, &g_hBctHandle));

    //The sdram index we obtained from the straps
    //must be less that the max indicated in the bct
    Instance = SdramIndex;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(g_hBctHandle, NvBctDataType_SdramConfigInfo,
                     &Size, &Instance, &SdramParams)
    );

    Instance = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(g_hBctHandle, NvBctDataType_NumValidSdramConfigs,
                     &Size, &Instance, &NumSdramSets)
    );

#if NO_BOOTROM
    //checking if bootrom binary is not present and
    //sdram params in bct are dummy
    if (NoBRBinary &&
        AllZero((NvU8*)&SdramParams,
              sizeof(NvBootSdramParams))
        )
    {
        // bypassing the below functionality
        //till the valid BCT file is available
        //use scripts files to intiliaze sdram
        //Move up the safe ptr
        pBitTable->SdramInitialized = NV_TRUE;
        pBitTable->SafeStartAddr +=
            ((pBitTable->BctSize + BOOTROM_BCT_ALIGNMENT)
             & ~BOOTROM_BCT_ALIGNMENT);
        pBitTable->BctValid = NV_TRUE;

        return e;
    }
#endif

    //The sdram index we obtained from the straps
    //must be less that the max indicated in the bct
    if (SdramIndex < NumSdramSets ||
        // Workaround to use nvflash --download, --read, and --format
        // when Microboot is on the ROM.
        // When Microboot is written on the ROM, NumSdramSets is overwritten
        // by zero so BootROM won't initialize DRAM. Because of this hack,
        // once Microboot is on the ROM, nvflash --download, --read,
        // and --format stops working.
        // With this workaround, even in case of NumSdramSets == 0,
        // NvML still uses SdramParams[SdramIndex].
        // See bug 682227 for more info.
        !AllZero((NvU8*)&SdramParams,
                  sizeof(NvBootSdramParams)))
    {
        NvBootSdramParams *pData;
        NvU32 StableTime = 0;
        NvU32 ChipId = NvMlGetChipId();

        pData = &SdramParams;

        if (ChipId == 0x35)
            NvMlSetMemioVoltage(SdramParams.PllMFeedbackDivider);

        //Start PLLM for EMC/MC
        NvMlUtilsClocksStartPll(NvBootClocksPllId_PllM,
            pData,
            StableTime);

        //Now, Initialize SDRAM
        NvBootSdramInit(&SdramParams);

        //If QueryTotalSize fails, it means the sdram
        //isn't initialized properly
        if (NvBootSdramQueryTotalSize())
            pBitTable->SdramInitialized = NV_TRUE;
        else
            goto fail;

        //Move up the safe ptr
        pBitTable->SafeStartAddr +=
            ((pBitTable->BctSize + BOOTROM_BCT_ALIGNMENT) &
                ~BOOTROM_BCT_ALIGNMENT);

        pBitTable->BctValid = NV_TRUE;
    }
    else
    {
        //Sdram index is wrong. Return an error.
        goto fail;
    }

    return NvError_Success;
fail:
    return NvError_NotInitialized;
}

static NvBool
VerifySdram(NvU32 Val, NvU32 *Size)
{
    NvU32 SdRamBaseAddress = 0x80000000;
    NvU32 Count = 0;
    NvU32 Data = 0;
    NvU32 Iterations = 0;
    NvU32 IterSize;

    if (!*Size)
        return NV_FALSE;

    IterSize = *Size;

    //sdram verification :soft test
    //known pattern  write and read back per MB
    if (Val == 0)
    {
        Iterations = IterSize / (1024 * 1024);

        for(Count = 0; Count < Iterations; Count ++)
            *(NvU32 *)(SdRamBaseAddress + Count * 1024 * 1024) = VERIFY_DATA1;

        for(Count = 0; Count < Iterations; Count ++)
        {
            Data = *(NvU32 *)(SdRamBaseAddress + Count * 1024 * 1024);
            if(Data != VERIFY_DATA1)
                return NV_FALSE;
        }

        for(Count = 0; Count < Iterations; Count ++)
            *(NvU32 *)(SdRamBaseAddress + Count * 1024 * 1024) = VERIFY_DATA2;

        for(Count = 0; Count < Iterations; Count ++)
        {
            Data = *(NvU32 *)(SdRamBaseAddress + Count * 1024 * 1024);
            if(Data != VERIFY_DATA2)
                return NV_FALSE;
        }
    }
    else if (Val == 1)
    {
        //sdram stress test
        //known pattern  write and read back of entire sdram area
        NvU32 offset = sizeof(NvU32);
        Iterations = IterSize;

        for(Count = 0; Count < Iterations; Count += offset)
            *(NvU32*)(SdRamBaseAddress + Count) = VERIFY_DATA1;

        for(Count = 0; Count < Iterations; Count += offset)
        {
            Data = *(NvU32*)(SdRamBaseAddress + Count);
            if(Data != VERIFY_DATA1)
                return NV_FALSE;
        }

        for(Count = 0; Count < Iterations; Count += offset)
            *(NvU32*)(SdRamBaseAddress + Count) = VERIFY_DATA2;

        for(Count = 0; Count < Iterations; Count += offset)
        {
            Data = *(NvU32*)(SdRamBaseAddress + Count);
            if(Data != VERIFY_DATA2)
                return NV_FALSE;
        }
        // Check whether memory setting has overlapping issue or not.
        for(Count = 0; Count < Iterations; Count += offset)
            *(NvU32*)(SdRamBaseAddress + Count) = (SdRamBaseAddress + Count);

        for(Count = 0; Count < Iterations; Count += offset)
        {
            Data = *(NvU32*)(SdRamBaseAddress + Count);
            if(Data != (SdRamBaseAddress + Count))
                return NV_FALSE;
        }
    }
    else if (Val == 2)
    {
        //sdram data/address bus test
        volatile NvDatum DataAddr = 0;
        if (VerifySdramDataBus(&DataAddr))
            return NV_FALSE;
        if (!VerifySdramAddressBus((volatile NvDatum *)SdRamBaseAddress, IterSize))
            return NV_FALSE;
    }
    else
    {
        return NV_FALSE;
    }
    return NV_TRUE;
}

/* Gets the processor board Id from eeprom
 *
 * @param pBoardId This will be updated the board id information
 *                 Should not be NULL
 *
 * @return NvSuccess if successful else NvError
 */
static NvError NvMlGetBoardId(Nv3pBoardId *pBoardId)
{
    NvBoardInfo BoardInfo;
    NvError e = NvSuccess;

    if (pBoardId == NULL)
        return NvError_BadParameter;

#if USE_ROTH_BOARD_ID
   pBoardId->BoardNo = 0x996;
   pBoardId->MemType = 0x0;
   pBoardId->BoardFab = 0x0;
   return e;
#endif

    NvBlInitI2cPinmux();
    NV_CHECK_ERROR_CLEANUP(NvBlReadBoardInfo(NvBoardType_ProcessorBoard, &BoardInfo));

    pBoardId->BoardNo  = BoardInfo.BoardId;
    pBoardId->MemType  = BoardInfo.MemType;
    pBoardId->BoardFab = BoardInfo.Fab;

fail:
    return e;
}

COMMAND_HANDLER(GetBoardDetails)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    Nv3pBoardDetails BoardDetails;
    NvU32 Size = 0;
    NvU32 ChipId = 0;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    Size = sizeof(NvU32);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_CpuSpeedo0,
                     &BoardDetails.CpuSpeedo[0], &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_CpuSpeedo1,
                     &BoardDetails.CpuSpeedo[1], &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_CpuSpeedo2,
                     &BoardDetails.CpuSpeedo[2], &Size),
        Nv3pStatus_BadParameter;
    );

    ChipId = NvMlGetChipId();

    /* For T11x and T14x we need to add 1024 to speedo values */
    switch (ChipId)
    {
        case 0x35:
        case 0x14:
            BoardDetails.CpuSpeedo[0] += 1024;
            BoardDetails.CpuSpeedo[1] += 1024;
            BoardDetails.CpuSpeedo[2] += 1024;
            break;
    }

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_CpuIddq,
                     &BoardDetails.CpuIddq, &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_SocSpeedo0,
                     &BoardDetails.SocSpeedo[0], &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_SocSpeedo1,
                     &BoardDetails.SocSpeedo[1], &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_SocSpeedo2,
                     &BoardDetails.SocSpeedo[2], &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_SocIddq,
                     &BoardDetails.SocIddq, &Size),
        Nv3pStatus_BadParameter;
    );

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(NvDdkFuseDataType_Sku,
                     &BoardDetails.ChipSku, &Size),
        Nv3pStatus_BadParameter
    );

#ifndef USE_ROTH_BOARD_ID
    NvBlInitI2cPinmux();
    NV_CHECK_ERROR_FAIL_3P(
        NvBlReadBoardInfo(NvBoardType_ProcessorBoard,
                              &BoardDetails.BoardInfo),
        Nv3pStatus_BadParameter
    );
#else
    BoardDetails.BoardInfo.BoardId = 0x996;
#endif

    Size = sizeof(BoardDetails);
    NV_CHECK_ERROR_FAIL_3P(
        Nv3pCommandComplete(hSock, command, &Size, 0),
        Nv3pStatus_CmdCompleteFailure
    );

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pDataSend(hSock, (NvU8 *)&BoardDetails, Size, 0),
        Nv3pStatus_DataTransferFailure
    );

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
    return ReportStatus(hSock, Message, Status.Code, 0);
}


COMMAND_HANDLER(GetPlatformInfo)
{
    NvError e;
    Nv3pCmdStatus Status;
    Nv3pCmdGetPlatformInfo a;
    Nv3pCmdGetPlatformInfo *pInfo = (Nv3pCmdGetPlatformInfo *)arg;
    NvU32 RegVal;
    NvBootFuseBootDevice FuseDev;
#if !defined(BOOT_MINIMAL_BL) && !defined(NVML_NAND_SUPPORT)
    NvMlContext  Context;
#endif
    NvU32 UidSize = 0;
    NvU32 SkuSize = 0;
    NvU32 BootConfigSize = 0;
    NvU32 Size = 0;
    NvU8 Data[32];
    char Message[NV3P_STRING_MAX] = {'\0'};

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_ReservedOdm,
            (NvU8 *)Data,
            &Size),
        Nv3pStatus_BadParameter);

    NvOsMemset(Data, 0, Size);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_ReservedOdm,
            (NvU8 *)Data,
            &Size),
        Nv3pStatus_BadParameter);

    a.WarrantyFuse = Data[0] & 1;

    Status.Code = Nv3pStatus_Ok;
    RegVal = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + APB_MISC_GP_HIDREV_0) ;

    a.ChipId.Id = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegVal);
    a.ChipId.Major = NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, RegVal);
    a.ChipId.Minor = NV_DRF_VAL(APB_MISC, GP_HIDREV, MINORREV, RegVal);

    NvOsMemset((void *)&a.BoardId, 0, sizeof(Nv3pBoardId));

#ifndef BOOT_MINIMAL_BL
    if (!pInfo->SkipAutoDetect && a.ChipId.Major)
        NV_CHECK_ERROR_FAIL_3P(
            NvMlGetBoardId(&a.BoardId), Nv3pStatus_InvalidState);
#endif

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_Uid,
            NULL,
            (void *)&UidSize),
        Nv3pStatus_BadParameter);

    NvOsMemset((void *)&a.ChipUid, 0, UidSize);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_Uid,
            (void *)&a.ChipUid,
            (void *)&UidSize),
        Nv3pStatus_BadParameter);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_Sku,
            NULL,
            (void *)&SkuSize),
        Nv3pStatus_BadParameter);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_Sku,
            (void *)&a.ChipSku,
            (void *)&SkuSize),
        Nv3pStatus_BadParameter);

    a.BootRomVersion = 1;
    // Get Boot Device Id from fuses

    Size = sizeof(NvU32);
    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_SecBootDeviceSelectRaw,
            (void *)&FuseDev,
            (NvU32 *)&Size),
        Nv3pStatus_BadParameter);

    a.DeviceConfigStrap = NvBootStrapDeviceConfigurationIndex();

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_SecBootDeviceConfig,
            NULL,
            (NvU32 *)&BootConfigSize),
        Nv3pStatus_BadParameter);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_SecBootDeviceConfig,
            (void *)&a.DeviceConfigFuse,
            (NvU32 *)&BootConfigSize),
        Nv3pStatus_BadParameter);

    NV_CHECK_ERROR_FAIL_3P(
        NvMlUtilsConvertBootFuseDeviceTo3p(
            FuseDev,
            &a.SecondaryBootDevice),
        Nv3pStatus_BadParameter);

#ifndef BOOT_MINIMAL_BL
    a.SbkBurned = NvMlCheckIfSbkBurned();
#else
    a.SbkBurned = NV_FALSE;
#endif

    //If the SBK isn't burned, we can figure out the DK.
    //If the SBK is burned, then we check to see if we're
    //not in ODM secure mode. If we're not in this mode,
    //we can read the dk fuse directly.
    if (a.SbkBurned == NV_FALSE)
    {
        // Now we check for the DK burn
#ifndef BOOT_MINIMAL_BL
        a.DkBurned = NvMlCheckIfDkBurned();
#else
        a.DkBurned = NV_FALSE;
#endif
    }
    else if (g_OpMode != NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        NvU32 dk;
        NvU32 DkSize  = sizeof (NvU32);

        NV_CHECK_ERROR_FAIL_3P(
            NvDdkFuseGet(
                NvDdkFuseDataType_DeviceKey,
                (void *)&dk,
                (NvU32 *)&DkSize),
            Nv3pStatus_BadParameter);

        if (dk)
            a.DkBurned = Nv3pDkStatus_Burned;
        else
            a.DkBurned = Nv3pDkStatus_NotBurned;
    }
    else
    {
        //We can't figure the DK out
        a.DkBurned = Nv3pDkStatus_Unknown;
    }

    a.SdramConfigStrap = NvBootStrapSdramConfigurationIndex();
    a.HdmiEnable = CheckIfHdmiEnabled();
    a.MacrovisionEnable = CheckIfMacrovisionEnabled();
    a.JtagEnable= CheckIfJtagEnabled();

    Size = sizeof(NvU32);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_OpMode,
            (void *)&a.OperatingMode,
            (NvU32 *)&Size),
        Nv3pStatus_BadParameter);

    g_OpMode = (NvDdkFuseOperatingMode)a.OperatingMode;

    // Make sure that the boot device gets initialized.
#if !defined(BOOT_MINIMAL_BL) && !defined(NVML_NAND_SUPPORT)
    if (a.SecondaryBootDevice != Nv3pDeviceType_Spi)
        NvMlUtilsSetupBootDevice(&Context);
#endif

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, &a, 0),
        Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(GetBct)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    NvU8 *pBct = NULL;
    Nv3pCmdGetBct a;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    a.Length = NvMlUtilsGetBctSize();

    //For testing purposes, we create
    // different BCTs to differentiate between
    // valid and invalid BCTs.
    if (pBitTable->BctValid == NV_TRUE)
    {
        //Valid BCT implies it's in in the BIT
        pBct = (NvU8*) pBitTable->BctPtr;
        VERIFY(pBct, goto fail);
    }
    else
    {
        NvBootFuseBootDevice BootDev;
        // Get Boot Device Id from fuses
        // if boot device is undefined get the user defined value
        NvMlGetBootDeviceType(&BootDev);
        // Return error if boot device type is undefined
        if (BootDev >= NvBootFuseBootDevice_Max)
        {
            Status.Code = Nv3pStatus_NotBootDevice;
            NvOsSnprintf(
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
            goto fail;
        }
        //Go get it from storage if possible
        NV_CHECK_ERROR_FAIL_3P(
            (NvError)NvMlBootGetBct(&pBct, g_OpMode),
            Nv3pStatus_BctNotFound);

        pBitTable->BctSize = NvMlUtilsGetBctSize();

        VERIFY(pBct, goto fail);

        NV_CHECK_ERROR_FAIL_3P(
            WriteBctToBit(pBct, a.Length),
            Nv3pStatus_InvalidBCT);
    }

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pCommandComplete(hSock, command, &a, 0),
        Nv3pStatus_CmdCompleteFailure);

    ReportStatus(hSock, Message, Status.Code, 0);

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pDataSend(hSock, pBct, a.Length, 0),
        Nv3pStatus_DataTransferFailure);
    return NvSuccess;
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);

    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(GetBit)
{
    NvError e;
    Nv3pCmdGetBit a;
    Nv3pCmdStatus Status;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    a.Length = NvMlUtilsGetBctSize();
    pBitTable->BctSize = NvMlUtilsGetBctSize();

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pCommandComplete(hSock, command, &a, 0),
        Nv3pStatus_CmdCompleteFailure);

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pDataSend(hSock, (NvU8*)pBitTable, a.Length, 0),
        Nv3pStatus_DataTransferFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);

    if (e)
        return ReportStatus(hSock, Message, Status.Code, 0);
    else
        return NvSuccess;
}

COMMAND_HANDLER(DownloadBct)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    NvU8 *pData, *pBct = NULL;
    NvU32 BytesLeftToTransfer = 0;
    NvU32 TransferSize = 0;
    NvU32 BctOffset = 0;
    NvBool IsFirstChunk = NV_TRUE;
    NvBool IsLastChunk = NV_FALSE;
    Nv3pCmdDownloadBct *a = (Nv3pCmdDownloadBct *)arg;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    VERIFY(a->Length, goto fail);

    //If a valid BCT is already present, just overwrite it
    if (pBitTable->BctValid == NV_TRUE)
    {
        pBct = (NvU8*) pBitTable->BctPtr;
    }
    else
    {
        pBct = (NvU8*)pBitTable->SafeStartAddr;
    }

    pBitTable->BctSize = a->Length;

    VERIFY(pBct, goto fail);
    VERIFY(a->Length == NvMlUtilsGetBctSize(), goto fail);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

    BytesLeftToTransfer = a->Length;
    pData = pBct;
    do
    {
        TransferSize = (BytesLeftToTransfer > USB_MAX_TRANSFER_SIZE) ?
                        USB_MAX_TRANSFER_SIZE :
                        BytesLeftToTransfer;

        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(hSock, pData, TransferSize, 0, 0),
            Nv3pStatus_DataTransferFailure);

        if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
        {
            // exclude crypto hash of bct for decryption and signing.
            if (IsFirstChunk)
                BctOffset = NvBctGetSignDataOffset();
            else
                BctOffset = 0;
            // need to decrypt the encrypted bct got from nvsbktool
            if ((BytesLeftToTransfer - TransferSize) == 0)
                IsLastChunk = NV_TRUE;
            if (!NvMlDecryptValidateImage(
                    pData + BctOffset, TransferSize - BctOffset,
                    IsFirstChunk, IsLastChunk, NV_TRUE, NV_TRUE, NULL))
            {
                Status.Code = Nv3pStatus_BLValidationFailure;
                NvOsSnprintf(
                    Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__,
                    __LINE__);
                goto clean;
            }
            // first chunk has been processed.
            IsFirstChunk = NV_FALSE;
        }
        pData += TransferSize;
        BytesLeftToTransfer -= TransferSize;
    } while (BytesLeftToTransfer);
#if PKC_ENABLE
    if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        if (!NvMlVerifySignature(pBct, NV_TRUE, pBitTable->BctSize, NULL))
        {
            Status.Code = Nv3pStatus_BLValidationFailure;
            NvOsSnprintf(
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
            goto clean;
        }
    }
#endif
    NV_CHECK_ERROR_CLEAN_3P(
        WriteBctToBit(pBct, NvMlUtilsGetBctSize()),
                    Nv3pStatus_InvalidBCT);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(DownloadBootloader)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    NvU8 *pData = NULL;
    NvU8 *pBct = NULL;
    NvU32 TransferSize, BytesLeftToTransfer;
    Nv3pCmdDownloadBootloader *a = (Nv3pCmdDownloadBootloader *)arg;
    NvBootFuseBootDevice BootDev;
    NvBool IsFirstChunk = NV_TRUE;
    NvBool IsLastChunk = NV_FALSE;
    NvU32 BlEntryPoint = 0;
    NvU8 StoredBlhash[RSA_KEY_SIZE + 1];
    NvU32 Size;
    NvU32 Instance;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    VERIFY(a->Length, goto fail);
    VERIFY(a->Address, goto fail);
    VERIFY(a->EntryPoint, goto fail);

    // Get Boot Device Id from fuses
    // if boot device is undefined get the user defined value
    NvMlGetBootDeviceType(&BootDev);
    // Return error if boot device type is undefined
    if (BootDev >= NvBootFuseBootDevice_Max)
    {
        Status.Code = Nv3pStatus_NotBootDevice;
        NvOsSnprintf(Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
        goto fail;
    }
    //If we don't have a valid BCT,
    //try to get it from the boot device
    if (pBitTable->BctValid == NV_FALSE)
    {
        NV_CHECK_ERROR_FAIL_3P(
            (NvError)NvMlBootGetBct(&pBct, g_OpMode),
            Nv3pStatus_BctNotFound);

        pBitTable->BctSize = NvMlUtilsGetBctSize();
        VERIFY(pBct, goto fail);

        NV_CHECK_ERROR_FAIL_3P(
            WriteBctToBit(pBct, NvMlUtilsGetBctSize()),
            Nv3pStatus_InvalidBCT);
    }

#if __GNUC__ && !NVML_BYPASS_PRINT
    NvAvpUartInit(NvMlUtilsGetPLLPFreq());
#endif

    if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        NvU32 i;
        NvU32 LoadAddress;
        // this is required for eboot case
        for (i = 0; i < NVBOOT_MAX_BOOTLOADERS; i++)
        {
            // iterate over blinfo in bct till bootloader is found
            Size = sizeof(NvU32);
            Instance = i;

            NV_CHECK_ERROR_FAIL_3P(
                NvBctGetData(
                    g_hBctHandle,
                    NvBctDataType_BootLoaderEntryPoint,
                    &Size, &Instance, &BlEntryPoint),
                Nv3pStatus_InvalidBCT);

            if ((BlEntryPoint & IRAM_START_ADDRESS) == IRAM_START_ADDRESS)
                continue; // found microboot since address starts with 4G,continue
            NV_CHECK_ERROR_FAIL_3P(
                NvBctGetData(
                    g_hBctHandle, NvBctDataType_BootLoaderLoadAddress,
                    &Size, &Instance, &LoadAddress),
                Nv3pStatus_InvalidBCT);
            pData = (NvU8*)LoadAddress;
            break; //bootloader found
        }
    }

    else
    {
        pData = (NvU8*)a->Address;
        BlEntryPoint = a->EntryPoint;
    }
    VERIFY(pData, goto fail);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

    NV_CHECK_ERROR_CLEANUP(
        ReportStatus(hSock, Message, Status.Code, 0));

    // store --bl bootloader hash info to be validated
    if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(
                hSock, StoredBlhash, 1 << NVBOOT_AES_BLOCK_LENGTH_LOG2, 0, 0),
                Nv3pStatus_DataTransferFailure);

        if (!NvMlDecryptValidateImage(
                StoredBlhash, 1 << NVBOOT_AES_BLOCK_LENGTH_LOG2,
                NV_TRUE, NV_TRUE, NV_FALSE, NV_FALSE, NULL))
        {
            Status.Code = Nv3pStatus_BLValidationFailure;
            NvOsSnprintf(
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
            goto clean;
        }
    }
    if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(hSock, StoredBlhash, RSA_KEY_SIZE, 0, 0)
        );
    }
    BytesLeftToTransfer = a->Length;
    do
    {
        TransferSize = (BytesLeftToTransfer > USB_MAX_TRANSFER_SIZE) ?
                        USB_MAX_TRANSFER_SIZE :
                        BytesLeftToTransfer;
        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(hSock, pData, TransferSize, 0, 0),
            Nv3pStatus_InvalidBCT);

        if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
        {
            // need to decrypt and validate sign of the encrypted bootloader
            // from nvsecuretool

            if ((BytesLeftToTransfer - TransferSize) == 0)
                IsLastChunk = NV_TRUE;
            if (!NvMlDecryptValidateImage(
                    pData, TransferSize, IsFirstChunk,
                    IsLastChunk, NV_TRUE, NV_FALSE, StoredBlhash))
            {
                Status.Code = Nv3pStatus_BLValidationFailure;
                NvOsSnprintf(
                    Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
                goto clean;
            }
            IsFirstChunk = NV_FALSE;
        }
        pData += TransferSize;
        BytesLeftToTransfer -= TransferSize;
    } while (BytesLeftToTransfer);
#if PKC_ENABLE
    pData = (NvU8*)a->Address;
    if (g_OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        if (!NvMlVerifySignature(pData, NV_FALSE, a->Length, StoredBlhash))
        {
            Status.Code = Nv3pStatus_BLValidationFailure;
            NvOsSnprintf(
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
            goto clean;
        }
    }
#endif
    //Write to a specific memory address indicating
    //that we are entering the bootloader from the
    //miniloader. The bootloader will examine
    //bitTable->SafeStartAddr - sizeof(NvU32) and
    //determine whether to turn on the nv3p server or not
    *(NvU32*)pBitTable->SafeStartAddr = MINILOADER_SIGNATURE;
    pBitTable->SafeStartAddr += sizeof(NvU32);
    //Claim that we booted off the first bootloader
    pBitTable->BlState[0].Status = NvBootRdrStatus_Success;

    // Enables Pllm and and sets PLLM in recovery path which is normally
    //done by BOOT ROM in all paths except recovery
    SetPllmInRecovery();
#if __GNUC__ && !NVML_BYPASS_PRINT
    NvOsAvpDebugPrintf("Transferring control to Bootloader\n");
#endif
    ExecuteBootLoader(BlEntryPoint);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (g_hBctHandle)
        NvBctDeinit(g_hBctHandle);
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(Go)
{
    NvError e;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdStatus Status;
    Status.Code = Nv3pStatus_Ok;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, 0, 0),
        Nv3pStatus_CmdCompleteFailure);

    return NvSuccess;
clean:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(OdmOptions)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    Nv3pCmdOdmOptions *a = (Nv3pCmdOdmOptions *)arg;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvU8 *pBct = NULL;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    //If we don't have a valid BCT,
    //try to get it from the boot device
    if (pBitTable->BctValid == NV_FALSE)
    {
        NvBootFuseBootDevice BootDev;
        // Get Boot Device Id from fuses
        // if boot device is undefined get the user defined value
        NvMlGetBootDeviceType(&BootDev);
        // Return error if boot device type is undefined
        if (BootDev >= NvBootFuseBootDevice_Max)
        {
            Status.Code = Nv3pStatus_NotBootDevice;
            NvOsSnprintf(
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
            goto fail;
        }
        NV_CHECK_ERROR_FAIL_3P(
            (NvError)NvMlBootGetBct(&pBct, g_OpMode), Nv3pStatus_BctNotFound);

        pBitTable->BctSize = NvMlUtilsGetBctSize();
        VERIFY(pBct, goto fail);

        NV_CHECK_ERROR_FAIL_3P(
            WriteBctToBit(pBct, NvMlUtilsGetBctSize()), Nv3pStatus_InvalidBCT);
    }

    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(
            g_hBctHandle, NvBctDataType_OdmOption,
            &Size, &Instance, NULL),
        Nv3pStatus_InvalidBCTSize);

    NV_CHECK_ERROR_FAIL_3P(
        NvBctSetData(
            g_hBctHandle, NvBctDataType_OdmOption,
            &Size, &Instance, (void *)&a->Options),
        Nv3pStatus_InvalidBCTSize);

#if __GNUC__ && !NVML_BYPASS_PRINT
    // reinit uart since debug port info is available now
    NvAvpUartInit(NvMlUtilsGetPLLPFreq());
    NvOsAvpDebugPrintf("Starting Miniloader\n");
#endif
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(SetBootDevType)
{
    NvError e;
    Nv3pCmdStatus Status;
    Nv3pCmdSetBootDevType *a = (Nv3pCmdSetBootDevType *)arg;
    NvBootFuseBootDevice FuseDev;
    NvBootFuseBootDevice OdmFuseDev;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    // Get Boot Device Id fuses
    NvMlUtilsGetBootDeviceType(&FuseDev);
    NvOsMemset(&Status, 0, sizeof(Status));
    NV_CHECK_ERROR_FAIL_3P(
        NvMlUtilsConvert3pToBootFuseDevice(a->DevType, &OdmFuseDev),
        Nv3pStatus_NotBootDevice);

    // handle this command only for the system for which no customer
    // fuses have been burned and if customer fuses are burned then odm
    //set fuses should not differ with them.
    if ((FuseDev != NvBootFuseBootDevice_Max) &&
        (FuseDev != OdmFuseDev))
    {
        Status.Code = Nv3pStatus_NotSupported;
        NvOsSnprintf(
            Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
        goto fail;
    }

    NvMlSetBootDeviceType(OdmFuseDev);

#if NAND_SUPPORT
    // update BIT with the user specified boot device type
    if (OdmFuseDev == NvBootFuseBootDevice_NandFlash_x16)
        pBitTable->SecondaryDevice = NvBootDevType_Nand_x16;
    else
        pBitTable->SecondaryDevice = (NvBootDevType)OdmFuseDev;
#endif

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(SetBootDevConfig)
{
    NvError e;
    Nv3pCmdStatus Status;
    Nv3pCmdSetBootDevConfig *a = (Nv3pCmdSetBootDevConfig *)arg;
    NvBootFuseBootDevice FuseDev;
    NvU32 DevConfig;
    NvU32 BootDevConfigSize = 0;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Status.Code = Nv3pStatus_Ok;
    // Get Boot Device Id fuses
    NvMlUtilsGetBootDeviceType(&FuseDev);

    BootDevConfigSize = sizeof(NvU32);
    // Get boot dev config
    NV_CHECK_ERROR_FAIL_3P(
        NvDdkFuseGet(
            NvDdkFuseDataType_SecBootDeviceConfig,
            (void *)&DevConfig, (NvU32 *)&BootDevConfigSize),
        Nv3pStatus_BadParameter);

    NvOsMemset(&Status, 0, sizeof(Status));
    NvMlSetBootDeviceConfig(a->DevConfig);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    return ReportStatus(hSock, Message, Status.Code, 0);
}

COMMAND_HANDLER(OdmCommand)
{
    NvError e = NvSuccess;
    Nv3pCmdStatus Status;
    Nv3pCmdOdmCommand *a = (Nv3pCmdOdmCommand *)arg;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvBctAuxInfo *AuxInfo = NULL;

    Status.Code = Nv3pStatus_Ok;
    switch(a->odmExtCmd)
    {
        case Nv3pOdmExtCmd_VerifySdram:
        {
            NvBool Res = NV_TRUE;
            Nv3pCmdOdmCommand b;
            Nv3pOdmExtCmdVerifySdram *c = (Nv3pOdmExtCmdVerifySdram*)
                                               &a->odmExtCmdParam.verifySdram;

            b.odmExtCmd = Nv3pOdmExtCmd_VerifySdram;
            b.Data = NvBootSdramQueryTotalSize();

            Res = VerifySdram(c->Value, &b.Data);
            if (Res != NV_TRUE)
            {
                Status.Code = Nv3pStatus_VerifySdramFailure;
                NvOsSnprintf(
                    Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
                goto fail;
            }

            NV_CHECK_ERROR_CLEAN_3P(
                Nv3pCommandComplete(hSock, command, &b, 0),
                Nv3pStatus_CmdCompleteFailure);
            break;
        }
        case Nv3pOdmExtCmd_LimitedPowerMode:
        {
            Nv3pCmdOdmCommand b;
            NvU32 Instance = 0;
            NvU32 Size = 0;
            NvU8 *bct = NULL;

            b.odmExtCmd = Nv3pOdmExtCmd_LimitedPowerMode;
            //If we don't have a valid BCT,
            //try to get it from the boot device
            if (pBitTable->BctValid == NV_FALSE)
            {
                NvBootFuseBootDevice BootDev;
                // Get Boot Device Id from fuses
                // if boot device is undefined get the user defined value
                NvMlGetBootDeviceType(&BootDev);
                // Return error if boot device type is undefined
                if (BootDev >= NvBootFuseBootDevice_Max)
                {
                    Status.Code = Nv3pStatus_NotBootDevice;
                    goto fail;
                }
                e = (NvError)NvMlBootGetBct(&bct, g_OpMode);
                if (e != NvSuccess)
                {
                    Status.Code = Nv3pStatus_BctNotFound;
                    goto fail;
                }
                pBitTable->BctSize = NvMlUtilsGetBctSize();
                VERIFY( bct, goto clean );
                e = WriteBctToBit(bct, NvMlUtilsGetBctSize());
                if (e != NvSuccess)
                {
                    Status.Code = Nv3pStatus_InvalidBCT;
                    goto fail;
                }
            }
            NV_CHECK_ERROR_CLEANUP(
                NvBctGetData(g_hBctHandle, NvBctDataType_AuxDataAligned,
                             &Size, &Instance, NULL));
            AuxInfo = NvOsAlloc(Size);
            if (!AuxInfo)
            {
                goto fail;
            }
            NvOsMemset(AuxInfo, 0, Size);
            NV_CHECK_ERROR_CLEANUP(
                NvBctGetData(g_hBctHandle, NvBctDataType_AuxDataAligned,
                             &Size, &Instance, AuxInfo));
            AuxInfo->LimitedPowerMode = NV_TRUE;
            NV_CHECK_ERROR_CLEANUP(
                NvBctSetData(g_hBctHandle, NvBctDataType_AuxDataAligned,
                    &Size, &Instance, AuxInfo));

            NV_CHECK_ERROR_CLEANUP(
                Nv3pCommandComplete( hSock, command, &b, 0 )
            );
            break;
        }
        default:
            break;
    }

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (AuxInfo)
    {
        NvOsFree(AuxInfo);
        AuxInfo = NULL;
    }
    return ReportStatus(hSock, Message, Status.Code, 0);
}

void
server(void)
{
    NvError e;
    Nv3pSocketHandle hSock = 0;
    Nv3pCommand command;
    void *arg;

    ReadBootInformationTable(&pBitTable);

#if NO_BOOTROM
    //check if bootrom binary is available
    if ((pBitTable->BootRomVersion == 0xffffffff
        || pBitTable->DataVersion == 0xffffffff
        || pBitTable->RcmVersion == 0xffffffff)
       )
        NoBRBinary = 1;

    if (NoBRBinary)
    {
        //BootROM does the usb enumeration
        //since BR is not present, we are doing
        //enumeration here using nvboot drivers
        NvBool b = NvMlUtilsStartUsbEnumeration();
        VERIFY(b == NV_TRUE, goto clean);

        NvMlUtilsPreInitBit(pBitTable);
    }
#endif

    NvOsMemcpy(&BootInfoTable, pBitTable, sizeof(NvBootInfoTable));

    e = Nv3pOpen(&hSock, Nv3pTransportMode_default, 0);
    VERIFY(e == NvSuccess, goto clean);

    for( ;; )
    {
        e = Nv3pCommandReceive(hSock, &command, &arg, 0);
        VERIFY(e == NvSuccess, goto clean);

        switch(command) {
        case Nv3pCommand_GetPlatformInfo:
            e = GetPlatformInfo(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_GetBct:
            e = GetBct(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_GetBit:
            e = GetBit(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_DownloadBct:
            e = DownloadBct(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_DownloadBootloader:
            e = DownloadBootloader(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_Go:
            e = Go(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_OdmOptions:
            e = OdmOptions(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_SetBootDevType:
            e = SetBootDevType(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_SetBootDevConfig:
            e = SetBootDevConfig(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_OdmCommand:
            e = OdmCommand(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        case Nv3pCommand_GetBoardDetails:
            e = GetBoardDetails(hSock, command, arg);
            VERIFY(e == NvSuccess, goto clean);
            break;
        default:
            Nv3pNack(hSock, Nv3pNackCode_BadCommand);
            break;
        }
    }

clean:
    Nv3pClose(hSock);
}

void NvOsAvpShellThread(int argc, char **argv)
{
    server();
}
