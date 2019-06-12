/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: SDIO Interface</b>
 *
 * @b Description: SD memory (standard capacity as well
 * as high capacity), SDIO, MMC interface.
 */

#ifndef __INCLUDED_NVDDK_SDIO_PRIVATE_H
#define __INCLUDED_NVDDK_SDIO_PRIVATE_H

#include "nvodm_sdio.h"
#include "nvddk_sdio.h"
#include "nvrm_pinmux.h"
#include "nvrm_memmgr.h"
#include "nvrm_hardware_access.h"
#include "nvrm_drf.h"


/**
 * @brief Contains the sdio instance details . This information is shared
 * between the thread and the isr
 */
typedef struct NvDdkSdioInfoRec
{
    // Nvrm device handle
    NvRmDeviceHandle hRm;
    // Instance of the SDMMC module
    NvU32 Instance;
    // Physical base address of the specific sdio instance
    NvRmPhysAddr SdioPhysicalAddress;
    // Base address of PMC
    NvU32 PmcBaseAddress;
    // Virtual base address of the specific sdio instance
    NvU32* pSdioVirtualAddress;
    // Size of PMC register map
    NvU32 PmcBankSize;
    // size of the sdio register map
    NvU32 SdioBankSize;
    // Bus width
    NvU32 BusWidth;
    // Bus voltage
    NvU32 BusVoltage;
    // High speed mode
    NvU32 IsHighSpeedEnabled;
    // Clock frequency
    NvRmFreqKHz ConfiguredFrequency;
    // Indicates whether it is a read or a write transaction
    NvBool IsRead;
    // Request Type
    SdioRequestType RequestType;
    // Semaphore handle used for internal handshaking between the calling thread
    // and the isr
    NvOsSemaphoreHandle PrivSdioSema;
    // Rmmemory Handle of the buffer allocated
    NvRmMemHandle hRmMemHandle;
    // Physical Buffer Address of the memory
    NvU32 pPhysBuffer;
    // Virtual Buffer Address
    void* pVirtBuffer;
    // Controller and Error status
    NvDdkSdioStatus* ControllerStatus;
    NvOsSemaphoreHandle SdioPowerMgtSema;
    NvU32 SdioRmPowerClientId;
    // Maximum block size supported by the controller
    NvU32 MaxBlockLength;
    // sdmmc device usage like bootdevice, media etc
    NvU32 Usage;
    // Sdio Interrupt handle
    NvOsInterruptHandle InterruptHandle;
    NvBool ISControllerSuspended;
    NvOsIntrMutexHandle SdioThreadSafetyMutex;
    // Uhs mode
    NvU32 Uhsmode;
#if NVDDK_SDMMC_T124
    // whether dma address is 64 bit or 32 bit
    NvBool IsDma64BitEnable;
    NvBool IsHostv40Enable;
#endif
    /**
     * Sets the given tap value for sdmmc.
     *
     * @param hSdio             SDIO device handle
     * @param TapValue          The tap value that has to be set
     *
     */
    void (*NvDdkSdioSetTapValue)(NvDdkSdioDeviceHandle hSdio, NvU32 TapValue);
    /**
     * Sets the given trim value for sdmmc.
     *
     * @param hSdio             SDIO device handle
     * @param TrimValue         The trim value that has to be set
     *
     */
    void (*NvDdkSdioSetTrimValue)(NvDdkSdioDeviceHandle hSdio, NvU32 TrimValue);

    /**
     * Execute tuning for eMMC 4.5 cards.
     *
     * @param hSdio             SDIO device handle
     *
     */
    void (*NvDdkSdioExecuteTuning)(NvDdkSdioDeviceHandle hSdio);
    /**
     * Configures the uhs mode. Is valid for SDHOST v3.0 and above controllers.
     *
     * @param hSdio             SDIO device handle
     * @param Uhsmode        Uhs mode to be set(DDR50, SDR104, SDR50)
     *
     */
    void (*NvDdkSdioConfigureUhsMode)(NvDdkSdioDeviceHandle hSdio, NvU32 Uhsmode);
    /**
     * Gets the new higher capabilities like DDR mode support, SDR104 mode support etc.
     *
     * @param hSdio             SDIO device handle
     * @param pHostCap       SDIO host capabilities
     *
     */
    void (*NvDdkSdioGetHigherCapabilities)(NvDdkSdioDeviceHandle hSdio, NvDdkSdioHostCapabilities *pHostCap);
    /**
     * Puts SDMMC pins in Deep Power Down Mode (lowest power mode possible)
     *
     * @param hSdio             SDIO device handle
     */
    void (*NvDdkSdioDpdEnable)(NvDdkSdioDeviceHandle hSdio);
    /**
     * Gets SDMMC pins out of Deep Power Down
     *
     * @param hSdio             SDIO device handle
     */
    void (*NvDdkSdioDpdDisable)(NvDdkSdioDeviceHandle hSdio);
}NvDdkSdioInfo;

/**
 * Used to issue softreset to Sdio Controller
 *
 * @param hSdio            The SDIO device handle.
 * @mask                   Reset ALL/DATA/CMD lines
 */
void PrivSdioReset(NvDdkSdioDeviceHandle hSdio, NvU32 mask);

/**
 * Used to enable/disable interrupts of Sdio Controller
 *
 * @param hSdio            The SDIO device handle.
 * @IntrEnableMask         Enable interrupts.
 * @IntrEnableMask         Disable interrupts.
 * @IntrStatusEnableMask  Enable only intterupt status.
 */
void ConfigureInterrupts(NvDdkSdioDeviceHandle hSdio, NvU32 IntrEnableMask,
        NvU32 IntrDisableMask, NvU32 IntrStatusEnableMask);

enum
{
    SDMMC_SW_RESET_FOR_ALL = 0x1000000,
    SDMMC_SW_RESET_FOR_CMD_LINE = 0x2000000,
    SDMMC_SW_RESET_FOR_DATA_LINE = 0x4000000,
};


enum
{
    SdioNormalModeMinFreq = 100,       // 100Khz
    SdioNormalModeMaxFreq = 25000,     // 25Mhz
    SdioDDR50ModeMaxFreq = 50000,      // 50 MHz
    SdioSDR104ModeMaxFreq = 208000     //208 MHz
};
// Defines various MMC card specific command types

#define SDMMC_REGR(pSdioHwRegsVirtBaseAdd, reg) \
        NV_READ32((pSdioHwRegsVirtBaseAdd) + ((SDMMC_##reg##_0)/4))

#define SDMMC_REGW(pSdioHwRegsVirtBaseAdd, reg, val) \
    do\
    {\
        NV_WRITE32((((pSdioHwRegsVirtBaseAdd) + ((SDMMC_##reg##_0)/4))), (val));\
    }while (0)
    #define DIFF_FREQ(x, y) \
    (x>y)?(x-y):(y-x)

#define PMC_REGR(PmcHwRegsVirtBaseAdd, reg) \
            NV_READ32((PmcHwRegsVirtBaseAdd) + ((APBDEV_PMC_##reg##_0)))

#define PMC_REGW(PmcHwRegsVirtBaseAdd, reg, val) \
        do\
        {\
            NV_WRITE32(((PmcHwRegsVirtBaseAdd) + (APBDEV_PMC_##reg##_0)), (val));\
        }while (0)

#define SDIO_BUFFER_READ_READY    0x20
#define MMC_SANITIZE_ARG 0x03A50100
#define MMC_SECURE_ERASE_MASK     0x80000000

#define MMC_HS200_TX_CLOCK_KHZ 136000

// sdio interrupts used by the driver
#define SDIO_INTERRUPTS     0x7F000F
#define SDIO_BUFFER_READ_READY 0x20
// sdio error interrupts
#define SDIO_ERROR_INTERRUPTS 0x7F0000
#define SDIO_CMD_ERROR_INTERRUPTS    0xF0001
#define SDIO_DATA_ERROR_INTERRUPTS    0x700000

#define SD_SET_COMMAND_PARAMETERS(p, code, type, isData, arg, rType, bSize) \
    do \
    { \
        (p)->CommandCode = (code); \
        (p)->CommandType = (type); \
        (p)->IsDataCommand = (isData); \
        (p)->CmdArgument = (arg); \
        (p)->ResponseType = (rType); \
        (p)->BlockSize = (bSize); \
    } while (0)

/// Defines Various Sd Commands as per spec.
typedef enum
{
    SdCommand_GoIdleState = 0,
    SdCommand_AllSendCid = 2,
    SdCommand_SetRelativeAddress = 3,
    SdCommand_SetDsr = 4,
    SdCommand_Switch = 6,
    SdCommand_SelectDeselectCard = 7,
    SdCommand_SendIfCond = 8,
    SdCommand_SendCsd = 9,
    SdCommand_SendCid = 10,
    SdCommand_StopTransmission = 12,
    SdCommand_SendStatus = 13,
    SdCommand_GoInactiveState = 15,
    SdCommand_SetBlockLength = 16,
    SdCommand_ReadSingle = 17,
    SdCommand_ReadMultiple = 18,
    SdCommand_ExecuteTuning = 19,
    SdCommand_WriteSingle = 24,
    SdCommand_WriteMultiple = 25,
    SdCommand_ProgramCsd = 27,
    SdCommand_SetWriteProtect = 28,
    SdCommand_ClearWriteProtect = 29,
    SdCommand_SendWriteProtect = 30,
    SdCommand_EraseWriteBlockStart = 32,
    SdCommand_EraseWriteBlockEnd = 33,
    SdCommand_Erase = 38,
    SdCommand_LockUnlock = 42,
    SdCommand_ApplicationCommand = 55,
    SdCommand_GeneralCommand = 56,
    SdCommand_Force32 = 0x7FFFFFFF
} SdCommands;

/// Defines various MMC card specific command types
typedef enum
{
    MmcCommand_MmcInit = 1,
    MmcCommand_AllSendCid = 2,
    MmcCommand_AssignRCA = 3,
    MmcCommand_Switch = 6,
    MmcCommand_SelectDeselectCard = 7,
    MmcCommand_ReadExtCSD = 8,
    MmcCommand_SendCsd = 9,
    MmcCommand_StopTransmissionCommand = 12,
    MmcCommand_SetBlockLength = 16,
    MmcCommand_ExecuteTuning = 21,
    MmcCommand_SetWriteProt = 28,
    MmcCommand_ClrWriteProt = 29,
    MmcCommand_SendWriteProt = 30,
    MmcCommand_EraseWriteBlockStart = 35,
    MmcCommand_EraseWriteBlockEnd = 36,
    MmcCommand_Erase = 38,
    MmcCommand_Force32 = 0x7FFFFFFF
} MmcCommands;


#define ENABLE_ERROR_PRINTS 1
#define ENABLE_DEBUG_PRINTS 0
#define ENABLE_INFO_PRINTS 0


#define SEND_CMD_NUM_WITH_ERR_CODES 1

#if SEND_CMD_NUM_WITH_ERR_CODES
#define SET_ERROR(errorcode, command) \
    (errorcode | (command << 24))
#else
#define SET_ERROR(errorcode, command) \
    errorcode
#endif

#define SD_DEFAULT_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)

#if ENABLE_ERROR_PRINTS
#define SD_ERROR_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)
#else
#define SD_ERROR_PRINT(...)
#endif

#if ENABLE_DEBUG_PRINTS
#define SD_DEBUG_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)
#else
#define SD_DEBUG_PRINT(...)
#endif

#if ENABLE_INFO_PRINTS
#define SD_INFO_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)
#else
#define SD_INFO_PRINT(...)
#endif

#endif
