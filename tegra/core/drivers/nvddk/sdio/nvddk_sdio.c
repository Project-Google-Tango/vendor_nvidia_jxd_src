/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *                 NvDDK SDIO Driver Implementation</b>
 *
 * @b Description: Implementation of the NvDDK SDIO API.
 *
 */

#include "arsdmmc.h"
#include "nvrm_interrupt.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvodm_pmu.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"
#include "nvodm_query.h"
#include "nvddk_sdio_private.h"
#include "nvddk_sdio_utils.h"
#include "arapb_misc.h"
#include "nvodm_query_gpio.h"
#include "nvrm_gpio.h"

// Macro to get expression for modulo value that is power of 2
// Expression: DIVIDEND % (pow(2, Log2X))
#define MACRO_MOD_LOG2NUM(DIVIDEND, Log2X) \
    ((DIVIDEND) & ((1 << (Log2X)) - 1))

// Macro to get expression for multiply by number which is power of 2
// Expression: VAL * (1 << Log2Num)
#define MACRO_POW2_LOG2NUM(Log2Num) \
    (1 << (Log2Num))

// Macro to get expression for multiply by number which is power of 2
// Expression: VAL * (1 << Log2Num)
#define MACRO_MULT_POW2_LOG2NUM(VAL, Log2Num) \
    ((VAL) << (Log2Num))

// Macro to get expression for div by number that is power of 2
// Expression: VAL / (1 << Log2Num)
#define MACRO_DIV_POW2_LOG2NUM(VAL, Log2Num) \
    ((VAL) >> (Log2Num))

#define SDMMC_DMA_BUFFER_SIZE    \
    SDMMC_BLOCK_SIZE_BLOCK_COUNT_0_HOST_DMA_BUFFER_SIZE_DMA16K
#define SDMMC_DMA_TRANSFER_SIZE      (1 << (12 + SDMMC_DMA_BUFFER_SIZE))

// Size of the memory buffer size
#define SDMMC_MEMORY_BUFFER_SIZE ((SDMMC_DMA_TRANSFER_SIZE) * (2))

// Maximum sdio transfer size using polling method
#ifndef BUILD_FOR_COSIM
#define SDMMC_MAX_POLLING_SIZE   4096
#else
// Disable polling until ASIM's sdhci model supports masking irq signals
#define SDMMC_MAX_POLLING_SIZE   0
#endif

// Maximum polling time for completing the sdio transfer
#define SDMMC_MIN_POLLING_TIME_USEC 1500000

// Mmc erase command and timeout
#define MMC_ERASE_COMMAND 38
#define MMC_ERASE_COMMAND_MIN_POLLING_TIME_USEC 10000000
#define MMC_ERASE_COMMAND_TIMEOUT_MSEC 10000
#define MMC_ERASE_COMMAND_SECURE_ERASE_TIMEOUT_MSEC 100000
#define MMC_AUTOCALIBRATION_TIMEOUT_USEC 10000

// Mmc sanitize command timeout
#define MMC_SANITIZE_COMMAND_TIMEOUT_MSEC 20000

// Polling size(1MB) for timeout calculation
#define SDMMC_TIMEOUT_CALCULATION_SIZE (1024*1024)
// Delay between polling the interrupt status register
#define SDMMC_POLLING_DELAY_USEC  50

#define SDMMC_ERROR_STATUS_VALUE     0xFFFF0000

// Frequency of the sdio controller when sdio controller is suspended
#define SDMMC_LOW_POWER_FREQ_KHZ 100

#define SDMMC_MAX_NUMBER_OF_BLOCKS   65536
#define NVDDK_SDMMC_COMMAND_TIMEOUT_MSEC 5000
#define NVDDK_SDMMC_DATA_TIMEOUT_MSEC 5000


#define SDMMC_INTERNAL_CLOCK_TIMEOUT_STEP_USEC 1000
#define SDMMC_INTERNAL_CLOCK_TIMEOUT_USEC 10000
#define SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL_STEP_USEC 1000
#define SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL_USEC 10000
#define SW_CONTROLLER_BUSY_TIMEOUT_STEP_USEC 1000
#define SW_CONTROLLER_BUSY_TIMEOUT_USEC 10000
#define SDMMC_CMD_INHIBIT 0x1
#define SDMMC_DATA_INHIBIT 0x2
#define MISC_BASE_ADDR 0x70000000

// values suggested by characterization team
#if NV_IF_T148
#define SDMMC_AUTO_CAL_PD_OFFSET_VALUE        1
#define SDMMC_AUTO_CAL_PU_OFFSET_VALUE        1
#else
#define SDMMC_AUTO_CAL_PD_OFFSET_VALUE        2
#define SDMMC_AUTO_CAL_PU_OFFSET_VALUE        2
#endif

enum
{
    SDMMC_MAX_BLOCK_SIZE_512 = 512,
    SDMMC_MAX_BLOCK_SIZE_1024 = 1024,
    SDMMC_MAX_BLOCK_SIZE_2048 = 2048
};

enum { SDMMC_ENABLE_ALL_INTERRUPTS = 0x000000FF };

typedef enum
{
    // data time out frequency = TMCLK/8K
    SdioDataTimeout_COUNTER_8K = 0,
    // data time out frequency = TMCLK/16K
    SdioDataTimeout_COUNTER_16K,
    // data time out frequency = TMCLK/32K
    SdioDataTimeout_COUNTER_32K,
    // data time out frequency = TMCLK/64K
    SdioDataTimeout_COUNTER_64K,
    // data time out frequency = TMCLK/128K
    SdioDataTimeout_COUNTER_128K,
    // data time out frequency = TMCLK/256K
    SdioDataTimeout_COUNTER_256K,
    // data time out frequency = TMCLK/512K
    SdioDataTimeout_COUNTER_512K,
    // data time out frequency = TMCLK/1M
    SdioDataTimeout_COUNTER_1M,
    // data time out frequency = TMCLK/2M
    SdioDataTimeout_COUNTER_2M,
    // data time out frequency = TMCLK/4M
    SdioDataTimeout_COUNTER_4M,
    // data time out frequency = TMCLK/8M
    SdioDataTimeout_COUNTER_8M,
    // data time out frequency = TMCLK/16M
    SdioDataTimeout_COUNTER_16M,
    // data time out frequency = TMCLK/32M
    SdioDataTimeout_COUNTER_32M,
    // data time out frequency = TMCLK/64M
    SdioDataTimeout_COUNTER_64M,
    // data time out frequency = TMCLK/128M
    SdioDataTimeout_COUNTER_128M,
    // data time out frequency = TMCLK/256M
    SdioDataTimeout_COUNTER_256M
}SdioDataTimeout;

// This is the default block size set whenever the nvddk sdio driver is open
enum { SDMMC_DEFAULT_BLOCK_SIZE = 512};

static NvBool SdioEnableInternalClock(NvDdkSdioDeviceHandle hSdio);
static NvError SdioEnableBusPower(NvDdkSdioDeviceHandle hSdio);
NvError SdioConfigureCardClock(NvDdkSdioDeviceHandle hSdio, NvBool IsEnable);

static void
SdioSetSlotClockRate(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioClkDivider Divider);

static NvError
    SdioSetDataTimeout(
        NvDdkSdioDeviceHandle hSdio,
        SdioDataTimeout SdioDataToCounter);

static NvU32
SdioIsControllerBusy(
    NvDdkSdioDeviceHandle hSdio,
    NvBool CheckforDataInhibit);
static NvError
SdioRegisterInterrupts(
    NvRmDeviceHandle hRm,
    NvDdkSdioDeviceHandle hSdio);

static void SdioIsr(void* args);

NvError
    SdioGetPhysAdd(
        NvRmDeviceHandle hRmDevice,
       NvRmMemHandle* hRmMemHandle,
        void** pVirtBuffer,
        NvU32 size,
        NvU32* pPhysBuffer);

static NvError SdioBlockTransfer(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfRWBytes,
    NvU32 pReadBuffer,
    NvDdkSdioCommand *pRWRequest,
    NvBool HWAutoCMD12Enable,
    NvBool IsRead);

static NvError
PrivSdioPollingRead(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToRead,
    void  *pReadBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus);

static NvError
PrivSdioPollingWrite(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToWrite,
    void *pWriteBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus);

NvError
PrivSdioSendCommandPolling(
        NvDdkSdioDeviceHandle hSdio,
        NvDdkSdioCommand *pCommand,
        NvU32* SdioStatus);

void
PrivSdioGetCaps(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioHostCapabilities *pHostCap,
    NvU32 instance);

static NvU32 SdioGetChipId(void);

void SdioProgramDmaAddress(
    NvDdkSdioDeviceHandle hSdio,
    NvU64 SystemAdress);

// function to check power of 2
static NvBool
UtilCheckPowerOf2(NvU32 Num)
{
    // A power of 2 satisfies condition (N & (N - 1)) == 0
    if ((Num & (Num - 1)) == 0)
        return NV_TRUE;
    else
        return NV_FALSE;
}

// Simple function to get log2, assumed value power of 2, else return
// returns log2 of immediately smaller number
NvU8
SdUtilGetLog2(NvU32 Val)
{
    NvU8 Log2Val = 0;
    NvU32 i;

    // Value should be non-zero
    NV_ASSERT(Val > 0);

    if (UtilCheckPowerOf2(Val) == NV_FALSE)
    {
        NvOsDebugPrintf("\nCalling simple log2 with value which is "
            "not power of 2 ");
        // In case of values that are not power of 2 we return the
        // integer part of the result of log2
    }

    // Value is power of 2
    if (Val > 0)
    {
        // Assumed that Val is NvU32
        for (i = 0; i < 32; i++)
        {
            // divide by 2
            Val = MACRO_DIV_POW2_LOG2NUM(Val, 1);
            if (Val == 0)
            {
                // Return 0 when Val is 1
                break;
            }
            Log2Val++;
        }
    }
    return Log2Val;
}

static NvBool IsCurrentPlatformEmulation(void)
{
    NvU32 RegValue = 0;
    NvU32 MajorId = 0;
    NV_MISC_READ(GP_HIDREV, RegValue);
    MajorId = NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, RegValue);
    if (MajorId == APB_MISC_GP_HIDREV_0_MAJORREV_EMULATION)
        return NV_TRUE;
    return NV_FALSE;
}

static NvError SdioEnablePowerRail(
    NvDdkSdioDeviceHandle hSdio,
    NvBool IsEnable)
{
    const NvOdmPeripheralConnectivity *pPeripheralConn = NULL;
    NvRmPmuVddRailCapabilities RailCaps;
    NvRmGpioHandle hRmGpio =  NULL;
    const NvOdmGpioPinInfo *pGpioPinTable;
    NvRmGpioPinHandle hPowerRailEnablePin;
    NvU32 GpioPinCount;
    NvU32 i;
    NvU32 PinValue;
    NvU32 Voltage;

    if (hSdio->Usage != NvOdmQuerySdioSlotUsage_Media)
        return NvSuccess;

    pPeripheralConn = NvOdmPeripheralGetGuid(
                          NV_ODM_GUID('N','V','D','D','S','D','I','O'));

    if (pPeripheralConn == NULL)
    {
        SD_ERROR_PRINT("%s: get guid failed\n", __func__);
        goto fail;
    }

    for (i = 0; i < pPeripheralConn->NumAddress; i++)
    {
        // Search for the vdd rail entry in sd
        if (pPeripheralConn->AddressList[i].Interface == NvOdmIoModule_Vdd)
        {
            NvRmPmuGetCapabilities(hSdio->hRm,
                pPeripheralConn->AddressList[i].Address,
                &RailCaps);

            Voltage = IsEnable ? RailCaps.requestMilliVolts : 0;

            NvRmPmuSetVoltage(hSdio->hRm, pPeripheralConn->AddressList[i].Address,
                Voltage, NULL);
        }
    }

    // check if any gpio needs to be set for power rail
    pGpioPinTable = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Sdio,
                        hSdio->Instance,
                        &GpioPinCount);
    if (GpioPinCount < 2 || pGpioPinTable[1].Port == NVODM_GPIO_INVALID_PORT ||
         pGpioPinTable[1].Pin == NVODM_GPIO_INVALID_PIN)
        return NvSuccess;

    // open gpio
    NvRmGpioOpen(hSdio->hRm, &hRmGpio);
    if (!hRmGpio)
    {
        SD_ERROR_PRINT("%s: gpio open failed\n", __func__);
        goto fail;
    }
    // get the gpio pin handle
    NvRmGpioAcquirePinHandle(hRmGpio,
                              pGpioPinTable[1].Port,
                              pGpioPinTable[1].Pin,
                              &hPowerRailEnablePin);

    // configure the pin as output
    NvRmGpioConfigPins(hRmGpio, &hPowerRailEnablePin,  1, NvRmGpioPinMode_Output);

    // set gpio pin output to high if enable, otherwise set to low
    PinValue = IsEnable ? NvRmGpioPinState_High : NvRmGpioPinState_Low;
    NvRmGpioWritePins(hRmGpio, &hPowerRailEnablePin, &PinValue, 1);
    NvRmGpioClose(hRmGpio);
    return NvSuccess;

fail:
    SD_ERROR_PRINT("%s: enable power rail failed\n", __func__);
    return NvError_SdioEnablePowerRailFailed;
}

void NvDdkSdioConfigureTapAndTrimValues(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 TapValue,
    NvU32 TrimValue)
{
    NV_ASSERT(hSdio);
    NV_ASSERT(TapValue < 0xFF);
    NV_ASSERT(TrimValue < 0xFF);

    hSdio->NvDdkSdioSetTapValue(hSdio, TapValue);
    hSdio->NvDdkSdioSetTrimValue(hSdio, TrimValue);
}

#if NV_IF_T148
static void ExecuteAutoCalibration(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 Reg = 0;
    NvU32 ActiveInProgress = 0;
    NvU64 StartTime = 0;
    NvU64 CurrentTime = 0;

    if (hSdio->Usage != NvOdmQuerySdioSlotUsage_Boot)
        return;

    Reg = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CAL_CONFIG);
    Reg = NV_FLD_SET_DRF_DEF(SDMMC, AUTO_CAL_CONFIG, AUTO_CAL_ENABLE, ENABLED, Reg);
    Reg = NV_FLD_SET_DRF_NUM(SDMMC,AUTO_CAL_CONFIG, AUTO_CAL_START, 1, Reg);
    Reg = NV_FLD_SET_DRF_NUM(SDMMC,AUTO_CAL_CONFIG, AUTO_CAL_STEP, 7, Reg);
    Reg = NV_FLD_SET_DRF_NUM(SDMMC,AUTO_CAL_CONFIG, AUTO_CAL_PD_OFFSET,
                             SDMMC_AUTO_CAL_PD_OFFSET_VALUE, Reg);
    Reg = NV_FLD_SET_DRF_NUM(SDMMC,AUTO_CAL_CONFIG, AUTO_CAL_PU_OFFSET,
                             SDMMC_AUTO_CAL_PU_OFFSET_VALUE, Reg);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, AUTO_CAL_CONFIG, Reg);

    // Wait till Auto cal active is cleared or timeout after 10ms
    StartTime = NvOsGetTimeUS();
    while (1)
    {
        Reg = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CAL_STATUS);
        ActiveInProgress = NV_DRF_VAL(SDMMC, AUTO_CAL_STATUS,
                               AUTO_CAL_ACTIVE, Reg);
        if (!ActiveInProgress)
            break;
        NvOsWaitUS(1);
        CurrentTime = NvOsGetTimeUS();
        if ((CurrentTime - StartTime) > MMC_AUTOCALIBRATION_TIMEOUT_USEC)
        {
            SD_ERROR_PRINT("\nExecuteAutoCalibration failed\n");
            break;
        }
    }
}
#endif

NvError NvDdkSdioOpen(
    NvRmDeviceHandle hDevice,
    NvDdkSdioDeviceHandle *phSdio,
    NvU8 Instance)
{

    NvError e = NvSuccess;
    NvBool IsClkStable = NV_FALSE;
    NvDdkSdioDeviceHandle hSdio = NULL;
    NvRmFreqKHz pConfiguredFrequencyKHz = 0;
    NvDdkSdioStatus* ControllerStatus = NULL;
    NvU32 MaxInstances = 0;
    NvU32 Reg = 0;
    NvDdkSdioHostCapabilities HostCap;
    const NvOdmQuerySdioInterfaceProperty* pSdioInterfaceCaps = NULL;
#if NVDDK_SDMMC_T124
    NvU32 RegValue;
#endif

    MaxInstances = NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Sdio);
    // Validate the instance number
    if (Instance >= MaxInstances)
    {
        return NvError_SdioInstanceTaken;
    }

    hSdio = NvOsAlloc(sizeof(NvDdkSdioInfo));
    if (!hSdio)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(hSdio, 0, sizeof(NvDdkSdioInfo));

    // initialise the members of the NvDdkSdioDeviceHandle struct
    hSdio->hRm = hDevice;
    hSdio->Instance = Instance;
    hSdio->ConfiguredFrequency = SdioNormalModeMinFreq;
    hSdio->ISControllerSuspended = NV_FALSE;

    // get the interface property from odm query
    pSdioInterfaceCaps = NvOdmQueryGetSdioInterfaceProperty(Instance);
    hSdio->Usage = pSdioInterfaceCaps->usage;

    NvRmModuleGetBaseAddress(
        hDevice,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, Instance),
        &hSdio->SdioPhysicalAddress,
        &hSdio->SdioBankSize);

    // PMC Base address is necessary to enable DPD mode
    NvRmModuleGetBaseAddress(
            hDevice,
            NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
            &hSdio->PmcBaseAddress,
            &hSdio->PmcBankSize);

    NV_CHECK_ERROR_CLEANUP(NvOsPhysicalMemMap(
        hSdio->SdioPhysicalAddress,
        hSdio->SdioBankSize,
        NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE,
        (void **)&hSdio->pSdioVirtualAddress));

    ControllerStatus = NvOsAlloc(sizeof(NvDdkSdioStatus));
    if (ControllerStatus == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    ControllerStatus->SDControllerStatus = NvDdkSdioCommandStatus_None;
    ControllerStatus->SDErrorStatus = NvDdkSdioError_None;
    hSdio->ControllerStatus = ControllerStatus;
    NV_CHECK_ERROR_CLEANUP(NvOsIntrMutexCreate(&hSdio->SdioThreadSafetyMutex));
    // Local semaphore to signal request completion
    NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&hSdio->PrivSdioSema, 0));
     // Event sema to register with the rm_power module
    NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&hSdio->SdioPowerMgtSema, 0));
    // register with the rm_power manager
    hSdio->SdioRmPowerClientId = NVRM_POWER_CLIENT_TAG('S','D','I','O');
    NV_CHECK_ERROR_CLEANUP(NvRmPowerRegister(hSdio->hRm,
        hSdio->SdioPowerMgtSema, &hSdio->SdioRmPowerClientId));

    // enable power rail, if required
    NV_CHECK_ERROR_CLEANUP(SdioEnablePowerRail(hSdio, NV_TRUE));

    // enable power
    NV_CHECK_ERROR_CLEANUP(NvRmPowerVoltageControl(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, Instance),
        hSdio->SdioRmPowerClientId,
        NvRmVoltsUnspecified,
        NvRmVoltsUnspecified,
        NULL,
        0,
        NULL));

    // now enable clock to sdio controller
    NV_CHECK_ERROR_CLEANUP(NvRmPowerModuleClockControl(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, Instance),
        hSdio->SdioRmPowerClientId,
        NV_TRUE));

    // reset controller
    NvRmModuleReset(hSdio->hRm, NVRM_MODULE_ID(NvRmModuleID_Sdio, Instance));

    // enable internal clock to sdio
    IsClkStable = SdioEnableInternalClock(hSdio);
    if (IsClkStable != NV_TRUE)
    {
        goto fail;
    }

    // Configure the clock frequency
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetClockFrequency(hSdio,
                                                      hSdio->ConfiguredFrequency,
                                                      &pConfiguredFrequencyKHz));

    PrivSdioGetCaps(hSdio, &HostCap, Instance);

    NvDdkSdioConfigureTapAndTrimValues(hSdio, pSdioInterfaceCaps->TapValue,
                                         pSdioInterfaceCaps->TrimValue);

    // Set the default block size
    NvDdkSdioSetBlocksize(hSdio, SDMMC_DEFAULT_BLOCK_SIZE);

    // set sd bus voltage
    NvDdkSdioSetSDBusVoltage(hSdio, HostCap.BusVoltage);

    // enable sd bus power
    SdioEnableBusPower(hSdio);

    // set data timeout counter value
    SdioSetDataTimeout(hSdio, SdioDataTimeout_COUNTER_128M);

    // register interrupt handler
    NV_CHECK_ERROR_CLEANUP(SdioRegisterInterrupts(hDevice, hSdio));

    // Enable ALL important Normal interrupts
    ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, ~SDIO_INTERRUPTS, 0);

    *phSdio = hSdio;
    SdioConfigureCardClock(hSdio, NV_TRUE);

#if NVDDK_SDMMC_T124
    // keeping it false, not implemented completely, just a prototype
    hSdio->IsDma64BitEnable= NV_FALSE;
    hSdio->IsHostv40Enable = NV_FALSE;
    if (hSdio->IsDma64BitEnable == NV_TRUE)
    {
        RegValue = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS);
        RegValue = NV_FLD_SET_DRF_DEF(SDMMC, AUTO_CMD12_ERR_STATUS,
                                         HOST_VERSION_4_EN, ENABLE, RegValue);
        RegValue = NV_FLD_SET_DRF_DEF(SDMMC, AUTO_CMD12_ERR_STATUS,
                                         ADDRESSING_64BIT_EN, ENABLE, RegValue);
        SDMMC_REGW(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS, RegValue);
        hSdio->IsHostv40Enable = NV_TRUE;
    }
#endif
    // set erase timeout limit to infinite
    Reg = SDMMC_REGR(hSdio->pSdioVirtualAddress, VENDOR_MISC_CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(SDMMC, VENDOR_MISC_CNTRL, ERASE_TIMEOUT_LIMIT, INFINITE, Reg);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, VENDOR_MISC_CNTRL, Reg);

    // Enable autocalibration for T148 emmc as suggested by characterisation team
#if NV_IF_T148
    ExecuteAutoCalibration(hSdio);
#endif
    // Allocate memory for sdio data transfers
    NV_CHECK_ERROR_CLEANUP(SdioGetPhysAdd(hSdio->hRm, &hSdio->hRmMemHandle,
        &hSdio->pVirtBuffer, SDMMC_MEMORY_BUFFER_SIZE, &hSdio->pPhysBuffer));
    return NvSuccess;
fail:
    NvOsIntrMutexDestroy(hSdio->SdioThreadSafetyMutex);
    hSdio->SdioThreadSafetyMutex = NULL;

    NV_CHECK_ERROR_CLEANUP(SdioEnablePowerRail(hSdio, NV_FALSE));

    if (hSdio->hRmMemHandle != NULL)
    {
        NvRmMemUnmap(hSdio->hRmMemHandle, hSdio->pVirtBuffer, SDMMC_MEMORY_BUFFER_SIZE);
        NvRmMemUnpin(hSdio->hRmMemHandle);
        NvRmMemHandleFree(hSdio->hRmMemHandle);
    }
     // disable power
    NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, Instance),
        hSdio->SdioRmPowerClientId,
        NvRmVoltsOff,
        NvRmVoltsOff,
        NULL,
        0,
        NULL));
     // unregister with the power manager
    NvRmPowerUnRegister(hSdio->hRm, hSdio->SdioRmPowerClientId);
    // unregister/disable interrupt handler
    NvRmInterruptUnregister(hDevice, hSdio->InterruptHandle);
    hSdio->InterruptHandle = NULL;
    if (hSdio != NULL)
    {
        // destory the internal sdio abort semaphore
        NvOsSemaphoreDestroy(hSdio->PrivSdioSema);
        hSdio->PrivSdioSema = NULL;
        NvOsPhysicalMemUnmap(hSdio->pSdioVirtualAddress, hSdio->SdioBankSize);
    }
    NvOsFree(ControllerStatus);
    ControllerStatus = NULL;
    hSdio->ControllerStatus = NULL;
    NvOsSemaphoreDestroy(hSdio->SdioPowerMgtSema);
    hSdio->SdioPowerMgtSema = NULL;
    NvOsFree(hSdio);
    hSdio = NULL;
    *phSdio = NULL;
    return e;
}

NvError
NvDdkSdioSendCommand(
        NvDdkSdioDeviceHandle hSdio,
        NvDdkSdioCommand *pCommand,
        NvU32* SdioStatus)
{
    NvU32 val = 0;
    NvBool IsCrcCheckEnable = NV_FALSE;
    NvBool IsIndexCheckEnable = NV_FALSE;
    NvU32 IsControllerBusy = 0;
    NvError status;
    NvDdkSdioStatus* ControllerStatus = NULL;
    NvU32 Timeout = 0;
    NvBool CheckforDataInhibit = NV_TRUE;

    NV_ASSERT(hSdio);
    NV_ASSERT(pCommand);

    ControllerStatus = hSdio->ControllerStatus;
    ControllerStatus->SDControllerStatus = NvDdkSdioCommandStatus_None;
    ControllerStatus->SDErrorStatus = NvDdkSdioError_None;
    hSdio->RequestType = NVDDK_SDIO_NORMAL_REQUEST;
    // Commands with response type as R1b will generate transfer complete.
    // Check if the command response is R1b. If the response is R1b enable
    // transfer complete interrupt.
    if (pCommand->ResponseType == NvDdkSdioRespType_R1b && pCommand->CommandCode != 12)
    {
        // Disable command complete and enable transfer complete
        ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, NvDdkSdioCommandStatus_CommandComplete, 0);
    }
    else if (pCommand->CommandCode == SdCommand_ExecuteTuning ||
                 pCommand->CommandCode == MmcCommand_ExecuteTuning)
    {
          ConfigureInterrupts(hSdio, SDIO_BUFFER_READ_READY, SDIO_INTERRUPTS, 0);
    }
    else
    {
        // Enable command complete and disable transfer complete
        ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, NvDdkSdioCommandStatus_TransferComplete, 0);
    }

    // set the command number
    val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX,
                pCommand->CommandCode, val);
    // check if any data transfer is involved in the command
    val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT,
                pCommand->IsDataCommand, val);
    if (pCommand->CommandCode == SdCommand_ExecuteTuning ||
                 pCommand->CommandCode == MmcCommand_ExecuteTuning)
    {
        val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL,
                    1, val);
        val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, DMA_EN,
                    0, val);
    }
    val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_TYPE,
                pCommand->CommandType, val);
    /* set the response type */
    switch (pCommand->ResponseType)
    {
        case NvDdkSdioRespType_NoResp:
            IsCrcCheckEnable = NV_FALSE;
            IsIndexCheckEnable = NV_FALSE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        NO_RESPONSE, val);
            break;
        case NvDdkSdioRespType_R1:
        case NvDdkSdioRespType_R5:
        case NvDdkSdioRespType_R6:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_TRUE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48, val);
            break;
        case NvDdkSdioRespType_R2:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_FALSE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_136, val);
            break;
        case NvDdkSdioRespType_R3:
        case NvDdkSdioRespType_R4:
            IsCrcCheckEnable = NV_FALSE;
            IsIndexCheckEnable = NV_FALSE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48, val);
            break;
        case NvDdkSdioRespType_R1b:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_TRUE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48BUSY, val);
            break;
        case NvDdkSdioRespType_R7:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_FALSE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48, val);
            break;
        default:
            NV_ASSERT(0);
    }
    // Enable CMD index check
    val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_INDEX_CHECK_EN,
                             IsIndexCheckEnable, val);
    // Enable CMD CRC check
    val = NV_FLD_SET_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_CRC_CHECK_EN,
                             IsCrcCheckEnable, val);

    if ((pCommand->CommandCode == 0) || (pCommand->CommandCode == 12) ||
        (pCommand->CommandCode == 13))
    {
        CheckforDataInhibit = NV_FALSE;
    }
    IsControllerBusy = SdioIsControllerBusy(hSdio, CheckforDataInhibit);
    if (IsControllerBusy)
    {
        return NvError_SdioControllerBusy;
    }

    SD_DEBUG_PRINT("NvDdkSdioSendCommand: Command Number = %d, Argument = %x, "
                      "Xfer mode = %x\n", pCommand->CommandCode,
                      pCommand->CmdArgument, val);

    // write the command argument
    SDMMC_REGW(hSdio->pSdioVirtualAddress, ARGUMENT, pCommand->CmdArgument);
    // now write to the command xfer register
    SDMMC_REGW(hSdio->pSdioVirtualAddress, CMD_XFER_MODE, val);

    if ((pCommand->CommandCode == MMC_ERASE_COMMAND) &&
        (pCommand->CmdArgument & MMC_SECURE_ERASE_MASK))
    {
        Timeout = MMC_ERASE_COMMAND_SECURE_ERASE_TIMEOUT_MSEC;
    }
    else if (pCommand->CommandCode == MMC_ERASE_COMMAND)
    {
        Timeout = MMC_ERASE_COMMAND_TIMEOUT_MSEC;
    }
    else if ((pCommand->CommandCode == MmcCommand_Switch) &&
        (pCommand->CmdArgument == MMC_SANITIZE_ARG))
    {
        Timeout = MMC_SANITIZE_COMMAND_TIMEOUT_MSEC;
    }
    else
    {
        Timeout = NVDDK_SDMMC_COMMAND_TIMEOUT_MSEC;
    }

    status = NvOsSemaphoreWaitTimeout(hSdio->PrivSdioSema, Timeout);
    if (status == NvSuccess)
    {
        // Reset the CMD lines if any cmd error occurred
        if (ControllerStatus->SDErrorStatus & SDIO_CMD_ERROR_INTERRUPTS)
        {
            PrivSdioReset(hSdio, SDMMC_SW_RESET_FOR_CMD_LINE);
        }
        // if a data error occurred, reset the data and cmd lines.
        if (ControllerStatus->SDErrorStatus & NvDdkSdioError_DataTimeout)
        {
            PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_DATA_LINE |
                SDMMC_SW_RESET_FOR_CMD_LINE));
        }
    }
    *SdioStatus = ControllerStatus->SDErrorStatus;
    return status;
}

NvError
PrivSdioSendCommandPolling(
        NvDdkSdioDeviceHandle hSdio,
        NvDdkSdioCommand *pCommand,
        NvU32* SdioStatus)
{
    NvU32 val = 0;
    NvBool IsCrcCheckEnable = NV_FALSE;
    NvBool IsIndexCheckEnable = NV_FALSE;
    NvU32 IsControllerBusy = 0;
    NvError status = NvSuccess;
    NvDdkSdioStatus* ControllerStatus = NULL;
    NvU32 PollTime = 0;
    NvU32 TotalPollingTime = SDMMC_MIN_POLLING_TIME_USEC;
    NvBool CheckforDataInhibit = NV_TRUE;

    if (pCommand->CommandCode == MMC_ERASE_COMMAND)
    {
        TotalPollingTime = MMC_ERASE_COMMAND_MIN_POLLING_TIME_USEC;
    }

    NV_ASSERT(hSdio);
    NV_ASSERT(pCommand);

    ControllerStatus = hSdio->ControllerStatus;
    ControllerStatus->SDControllerStatus = NvDdkSdioCommandStatus_None;
    ControllerStatus->SDErrorStatus = NvDdkSdioError_None;
    hSdio->RequestType = NVDDK_SDIO_NORMAL_REQUEST;
    // Commands with response type as R1b will generate transfer complete
    // Check if the command response is R1b. If the response is R1b enable
    // transfer complete interrupt.
    if (pCommand->ResponseType == NvDdkSdioRespType_R1b && pCommand->CommandCode != 12)
    {
        // Disable all interrupts
        ConfigureInterrupts(hSdio, 0, SDIO_INTERRUPTS,
                    (SDIO_ERROR_INTERRUPTS|NvDdkSdioCommandStatus_TransferComplete));
    }
    else
    {
        // Enable command complete and disable transfer complete
        ConfigureInterrupts(hSdio, 0, SDIO_INTERRUPTS,
                (SDIO_ERROR_INTERRUPTS | NvDdkSdioCommandStatus_CommandComplete));
    }

    // set the command number
    val |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX,
                pCommand->CommandCode);
    // check if any data transfer is involved in the command
    val |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT,
                pCommand->IsDataCommand);
    val |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_TYPE,
                pCommand->CommandType);
    /* set the response type */
    switch (pCommand->ResponseType)
    {
        case NvDdkSdioRespType_NoResp:
            IsCrcCheckEnable = NV_FALSE;
            IsIndexCheckEnable = NV_FALSE;
            val = NV_FLD_SET_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        NO_RESPONSE, val);
        break;
        case NvDdkSdioRespType_R1:
        case NvDdkSdioRespType_R5:
        case NvDdkSdioRespType_R6:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_TRUE;
            val |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48);
        break;
        case NvDdkSdioRespType_R2:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_FALSE;
            val |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_136);
        break;
        case NvDdkSdioRespType_R3:
        case NvDdkSdioRespType_R4:
            IsCrcCheckEnable = NV_FALSE;
            IsIndexCheckEnable = NV_FALSE;
            val |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48);
        break;
        case NvDdkSdioRespType_R1b:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_TRUE;
            val |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48BUSY);
        break;
        case NvDdkSdioRespType_R7:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_FALSE;
            val |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                        RESP_LENGTH_48);
        break;
        default:
            NV_ASSERT(0);
    }
    // set cmd index check
    val |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_INDEX_CHECK_EN,
                             IsIndexCheckEnable);
    // set cmd crc check
    val |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_CRC_CHECK_EN,
                             IsCrcCheckEnable);

    if ((pCommand->CommandCode == 0) || (pCommand->CommandCode == 12) ||
        (pCommand->CommandCode == 13))
    {
        CheckforDataInhibit = NV_FALSE;
    }
    IsControllerBusy = SdioIsControllerBusy(hSdio, CheckforDataInhibit);
    if (IsControllerBusy)
    {
        return NvError_SdioControllerBusy;
    }

    *SdioStatus = NvDdkSdioError_None;
    // write the command argument
    SDMMC_REGW(hSdio->pSdioVirtualAddress, ARGUMENT, pCommand->CmdArgument);
    // now write to the command xfer register
    SDMMC_REGW(hSdio->pSdioVirtualAddress, CMD_XFER_MODE, val);

    // poll for the transfer or command complete interrupt
    while (PollTime < TotalPollingTime)
    {
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS);
        if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, ERR_INTERRUPT, val))
        {
            ControllerStatus->SDErrorStatus = val & SDIO_ERROR_INTERRUPTS;
            if (ControllerStatus->SDErrorStatus & SDIO_CMD_ERROR_INTERRUPTS)
            {
                SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS,
                    (val & SDIO_CMD_ERROR_INTERRUPTS));
                // Reset the cmd line.
                PrivSdioReset(hSdio, SDMMC_SW_RESET_FOR_CMD_LINE);
            }
            if (ControllerStatus->SDErrorStatus & SDIO_DATA_ERROR_INTERRUPTS)
            {
                SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS,
                    (val & SDIO_DATA_ERROR_INTERRUPTS));
                // Reset both cmd and data lines.
                PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
                    SDMMC_SW_RESET_FOR_DATA_LINE));
            }
            *SdioStatus = val;
            status = NvSuccess;
            break;
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, val))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            status = NvSuccess;
            break;
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, CMD_COMPLETE, val))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, CMD_COMPLETE, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            status = NvSuccess;
            break;
        }
        NvOsWaitUS(SDMMC_POLLING_DELAY_USEC);
        PollTime += SDMMC_POLLING_DELAY_USEC;
    }
    if (PollTime >= TotalPollingTime)
    {
        PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
            SDMMC_SW_RESET_FOR_DATA_LINE));
        status = NvError_Timeout;
    }

    return status;
}

NvError
NvDdkSdioGetCommandResponse(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 CommandNumber,
    NvDdkSdioRespType ResponseType,
    NvU32 *pResponse)
{
    NvU32 *pTemp;
    NvU32 i;
    NV_ASSERT(hSdio);
    NV_ASSERT(pResponse);
    SD_DEBUG_PRINT("CommandResponse:\n");
    pTemp = pResponse;
    /* set the response type */
    switch (ResponseType)
    {
        case NvDdkSdioRespType_NoResp:
            *pTemp = 0;
        break;
        // SDMMC_RESP_LENGTH_48
        case NvDdkSdioRespType_R1:
        case NvDdkSdioRespType_R1b:
        case NvDdkSdioRespType_R3:
        case NvDdkSdioRespType_R4:
        case NvDdkSdioRespType_R5:
        case NvDdkSdioRespType_R6:
        case NvDdkSdioRespType_R7:
             *pTemp = SDMMC_REGR(hSdio->pSdioVirtualAddress, RESPONSE_R0_R1);
              SD_DEBUG_PRINT("%08X\n ", pResponse[0]);
         break;
        // SDMMC_RESP_LENGTH_136
        case NvDdkSdioRespType_R2:
             *pTemp = SDMMC_REGR(hSdio->pSdioVirtualAddress, RESPONSE_R0_R1);
             *(++pTemp) = SDMMC_REGR(hSdio->pSdioVirtualAddress, RESPONSE_R2_R3);
             *(++pTemp) = SDMMC_REGR(hSdio->pSdioVirtualAddress, RESPONSE_R4_R5);
             *(++pTemp) = SDMMC_REGR(hSdio->pSdioVirtualAddress, RESPONSE_R6_R7);
             for(i =0; i< 4; i++)
             {
                 SD_DEBUG_PRINT("%08X\n " ,pResponse[i]);
             }
             break;
        default:
             NV_ASSERT(0);
    }
    return NvSuccess;
}

void SdioProgramDmaAddress(
    NvDdkSdioDeviceHandle hSdio,
    NvU64 SystemAddress)
{
#if NVDDK_SDMMC_T124
    if (hSdio->IsHostv40Enable)
    {
        if (!hSdio->IsDma64BitEnable)
        {
            SDMMC_REGW(hSdio->pSdioVirtualAddress, ADMA_SYSTEM_ADDRESS,
                          SystemAddress & 0xFFFFFFFF);
        }
        else
        {
            SDMMC_REGW(hSdio->pSdioVirtualAddress, ADMA_SYSTEM_ADDRESS,
                          SystemAddress & 0xFFFFFFFF);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, UPPER_ADMA_SYSTEM_ADDRESS,
                          (SystemAddress >> 32) & 0xFFFFFFFF);
        }
    }
    else
#endif
    {
        SDMMC_REGW(hSdio->pSdioVirtualAddress, SYSTEM_ADDRESS,
                       SystemAddress & 0xFFFFFFFF);
    }
    return;
}

static NvError
PrivSdioPollingRead(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToRead,
    void  *pReadBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus)
{
    NvU32 val = 0;
    NvU32 PollTime = 0;
    NvU8* VirtAddr = (NvU8*)hSdio->pVirtBuffer;
    NvError Error = NvError_SdioReadFailed;
    NvU32 BytesReceived = 0;
    NvU32 BytesToBeCopied = 0;
    NvU32 BytesToBeReceived = 0;
    NvU8* ReadPtr = (NvU8*)pReadBuffer;
    NvU32 PhysAddr = hSdio->pPhysBuffer;
    NvBool IsTransferCompleted = NV_FALSE;
    NvU32 IntrStatus = 0;
    NvU32 PollingTimeOut = 0;

    BytesToBeReceived = (NumOfBytesToRead > SDMMC_DMA_TRANSFER_SIZE) ?
                  SDMMC_DMA_TRANSFER_SIZE : NumOfBytesToRead;

    PollingTimeOut = (NumOfBytesToRead > SDMMC_TIMEOUT_CALCULATION_SIZE) ?
                  (SDMMC_MIN_POLLING_TIME_USEC * (NumOfBytesToRead/
                  SDMMC_TIMEOUT_CALCULATION_SIZE)) : SDMMC_MIN_POLLING_TIME_USEC;
    if (NumOfBytesToRead > SDMMC_DMA_TRANSFER_SIZE)
    {
        ConfigureInterrupts(hSdio, 0, SDIO_INTERRUPTS,
                    (SDIO_ERROR_INTERRUPTS|NvDdkSdioCommandStatus_TransferComplete|
                                                               NvDdkSdioCommandStatus_DMA));
    }
    else
    {
        ConfigureInterrupts(hSdio, 0, SDIO_INTERRUPTS, (SDIO_ERROR_INTERRUPTS|
                                                                NvDdkSdioCommandStatus_TransferComplete));
    }
    /* this turns on the sdio clock */
    Error = SdioBlockTransfer(hSdio,
                    NumOfBytesToRead,
                    hSdio->pPhysBuffer,
                    pRWCommand,
                    HWAutoCMD12Enable,
                    NV_TRUE);
    if (Error != NvSuccess)
    {
        // Reset the cmd and data lines
        PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
            SDMMC_SW_RESET_FOR_DATA_LINE));
        *SdioStatus = Error;
        return Error;
    }
    // poll for the transfer complete interrupt
    while (PollTime < PollingTimeOut)
    {
        IntrStatus = SDMMC_REGR(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS);
        if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, ERR_INTERRUPT, IntrStatus))
        {
            // Clear the error interrupts
            IntrStatus &= SDIO_ERROR_INTERRUPTS;
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, IntrStatus);
            *SdioStatus = IntrStatus;
            // Reset the cmd and data lines
            PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
                SDMMC_SW_RESET_FOR_DATA_LINE));
            Error = NvError_SdioWriteFailed;
            SD_ERROR_PRINT("SDIO_DDK polling write failed error[0x%x] \
                        Instance[%d]\n", *SdioStatus, hSdio->Instance);
            break;
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, IntrStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, IntrStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            IsTransferCompleted = NV_TRUE;
        }
        else
        {
            NvOsWaitUS(SDMMC_POLLING_DELAY_USEC);
            PollTime += SDMMC_POLLING_DELAY_USEC;
            continue;
        }

        if (BytesToBeReceived)
        {
            BytesToBeCopied = BytesToBeReceived;
            BytesReceived += BytesToBeReceived;
            BytesToBeReceived = NumOfBytesToRead - BytesReceived;
        }
        if (BytesToBeReceived == 0)
        {
            if (IsTransferCompleted)
            {
                NvOsMemcpy(ReadPtr, VirtAddr, BytesToBeCopied);
                break;
            }
            else
            {
                NvOsWaitUS(SDMMC_POLLING_DELAY_USEC);
                PollTime += SDMMC_POLLING_DELAY_USEC;
                continue;
            }
        }
        else if (BytesToBeReceived > SDMMC_DMA_TRANSFER_SIZE)
        {
            BytesToBeReceived = SDMMC_DMA_TRANSFER_SIZE;
        }

        if (PhysAddr == hSdio->pPhysBuffer)
        {
            PhysAddr += SDMMC_DMA_TRANSFER_SIZE;
            // program the system start address
            SdioProgramDmaAddress(hSdio, PhysAddr);
            NvOsMemcpy(ReadPtr, VirtAddr, BytesToBeCopied);
            VirtAddr += SDMMC_DMA_TRANSFER_SIZE;
        }
        else
        {
            PhysAddr = hSdio->pPhysBuffer;
            // program the system start address
            SdioProgramDmaAddress(hSdio, hSdio->pPhysBuffer);
            NvOsMemcpy(ReadPtr, VirtAddr, BytesToBeCopied);
            VirtAddr = hSdio->pVirtBuffer;
        }
        ReadPtr += BytesToBeCopied;
        NvOsWaitUS(SDMMC_POLLING_DELAY_USEC);
        PollTime += SDMMC_POLLING_DELAY_USEC;
    }
    if (PollTime >= PollingTimeOut)
    {
        PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
            SDMMC_SW_RESET_FOR_DATA_LINE));
        Error = NvError_Timeout;
        SD_ERROR_PRINT("SDIO_DDK polling read timeout Instance[%d]\n",
                        hSdio->Instance);
    }

    return Error;
}

NvError
NvDdkSdioRead(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToRead,
    void  *pReadBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus)
{
    NvError Error = NvError_SdioReadFailed;
    NvDdkSdioStatus* ControllerStatus = NULL;
    NvU32 BytesToBeReceived = 0;
    NvU32 BytesReceived = 0;
    NvU8* ReadPtr = (NvU8*)pReadBuffer;
    NvU32 BytesToBeCopied = 0;
    NvU8* VirtAddr = (NvU8*)hSdio->pVirtBuffer;
    NvU32 PhysAddr = hSdio->pPhysBuffer;
    NvU32 UserBufferAddress;
    NvU32 DmaAddress;
    NvBool IsUserBufferAligned = NV_FALSE;
    NvBool IsUserBufferWriteCombined = NV_FALSE;
    NvBool IsUserBufferSupportsDma = NV_FALSE;

    NV_ASSERT(hSdio);
    NV_ASSERT(pReadBuffer);
    NV_ASSERT(pRWCommand);

    *SdioStatus = NvDdkSdioError_None;

    // To get better performance, use polling (to eliminate interrupt latency) for lower transfer sizes.
    if (NumOfBytesToRead <= SDMMC_MAX_POLLING_SIZE)
    {
        Error = PrivSdioPollingRead(hSdio, NumOfBytesToRead, pReadBuffer,
                            pRWCommand, HWAutoCMD12Enable, SdioStatus);
        return Error;
    }

    UserBufferAddress = (NvU32)pReadBuffer;

    // check whether user buffer is write combined
    IsUserBufferWriteCombined =
        NvOsIsMemoryOfGivenType(NvOsMemAttribute_WriteCombined,
                                    UserBufferAddress) &&
        NvOsIsMemoryOfGivenType(NvOsMemAttribute_WriteCombined,
                                    UserBufferAddress + NumOfBytesToRead - 1);

    // check whether user buffer aligned to dma transfer size
    if ((UserBufferAddress & (SDMMC_DMA_TRANSFER_SIZE - 1)) == 0)
        IsUserBufferAligned = NV_TRUE;

    // check whether user buffer supports for dma transfer
    if (IsUserBufferWriteCombined && IsUserBufferAligned)
        IsUserBufferSupportsDma = NV_TRUE;

    ControllerStatus = hSdio->ControllerStatus;

    BytesToBeReceived = (NumOfBytesToRead > SDMMC_DMA_TRANSFER_SIZE) ?
                          SDMMC_DMA_TRANSFER_SIZE : NumOfBytesToRead;
    if (NumOfBytesToRead == SDMMC_DMA_TRANSFER_SIZE)
    {
        ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, (NvDdkSdioCommandStatus_CommandComplete|NvDdkSdioCommandStatus_DMA), 0);
    }
    else
    {
        ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, NvDdkSdioCommandStatus_CommandComplete, 0);
    }

    // check if we are using userbuffer or local buffer for dma.
    // update accordingly
    if (IsUserBufferSupportsDma != NV_TRUE)
    {
        DmaAddress = PhysAddr;
    }
    else
    {
        DmaAddress = (NvU32) ReadPtr;
    }

    /* this function turns on the sdio clock */
    Error = SdioBlockTransfer(hSdio,
                              NumOfBytesToRead,
                              DmaAddress,
                              pRWCommand,
                              HWAutoCMD12Enable,
                              NV_TRUE);
    if (Error != NvSuccess)
    {
        *SdioStatus = Error;
        return Error;
    }

    // loop till we receive all the requested bytes
    while (BytesReceived != NumOfBytesToRead)
    {
        Error = NvOsSemaphoreWaitTimeout(hSdio->PrivSdioSema, NVDDK_SDMMC_DATA_TIMEOUT_MSEC);
        if (Error == NvSuccess)
        {
            if (ControllerStatus->SDErrorStatus != NvDdkSdioError_None)
            {
                *SdioStatus = ControllerStatus->SDErrorStatus;
                // If any error interrupts occured, reset the cmd and data lines and return.
                if (ControllerStatus->SDErrorStatus & SDIO_ERROR_INTERRUPTS)
                {
                    PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_DATA_LINE |
                        SDMMC_SW_RESET_FOR_CMD_LINE));
                }
                SD_ERROR_PRINT("SDIO_DDK read failed error[0x%x] Instance[%d]\n",
                    *SdioStatus, hSdio->Instance);
                return Error;
            }
            else
            {
                BytesToBeCopied = BytesToBeReceived;
                BytesReceived += BytesToBeReceived;
                BytesToBeReceived = NumOfBytesToRead - BytesReceived;
                if (BytesToBeReceived == 0)
                {
                    if (IsUserBufferSupportsDma != NV_TRUE)
                    {
                        NvOsMemcpy(ReadPtr, VirtAddr, BytesToBeCopied);
                    }
                    break;
                }
                else if (BytesToBeReceived == SDMMC_DMA_TRANSFER_SIZE)
                {
                    ConfigureInterrupts(hSdio, SDIO_INTERRUPTS,
                        (NvDdkSdioCommandStatus_DMA|NvDdkSdioCommandStatus_CommandComplete), 0);
                }
                else if (BytesToBeReceived > SDMMC_DMA_TRANSFER_SIZE)
                {
                    BytesToBeReceived = SDMMC_DMA_TRANSFER_SIZE;
                }

                // check if we are using userbuffer or local buffer for dma.
                // update accordingly
                if (IsUserBufferSupportsDma != NV_TRUE)
                {
                    if (PhysAddr == hSdio->pPhysBuffer)
                    {
                        PhysAddr += SDMMC_DMA_TRANSFER_SIZE;
                        // program the system start address
                        SDMMC_REGW(hSdio->pSdioVirtualAddress, SYSTEM_ADDRESS,
                             PhysAddr);
                        NvOsMemcpy(ReadPtr, VirtAddr, BytesToBeCopied);
                        VirtAddr += SDMMC_DMA_TRANSFER_SIZE;
                    }
                    else
                    {
                        PhysAddr = hSdio->pPhysBuffer;
                        // program the system start address
                        SDMMC_REGW(hSdio->pSdioVirtualAddress, SYSTEM_ADDRESS,
                             hSdio->pPhysBuffer);
                        NvOsMemcpy(ReadPtr, VirtAddr, BytesToBeCopied);
                        VirtAddr = hSdio->pVirtBuffer;
                    }
                    ReadPtr += BytesToBeCopied;
                }
                else
                {
                    ReadPtr += SDMMC_DMA_TRANSFER_SIZE;
                    DmaAddress = (NvU32) ReadPtr;
                    SDMMC_REGW(hSdio->pSdioVirtualAddress, SYSTEM_ADDRESS,
                        DmaAddress);
                }
            }
        }
        else
        {
             // break if there is any timeout
            *SdioStatus = NvError_Timeout;
             PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_DATA_LINE |
                 SDMMC_SW_RESET_FOR_CMD_LINE));
            SD_ERROR_PRINT("SDIO_DDK read timed out for Instance[%d]\n",
                                hSdio->Instance);
            return Error;
        }
    }

    *SdioStatus = ControllerStatus->SDErrorStatus;
    return Error;
}

static NvError
PrivSdioPollingWrite(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToWrite,
    void *pWriteBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus)
{
    NvU32 val = 0;
    NvU32 PollTime = 0;
    NvU8* VirtAddr = (NvU8*)hSdio->pVirtBuffer;
    NvError Error = NvError_SdioWriteFailed;
    NvU32 BytesToBeSent = 0;
    NvU32 BytesSent = 0;
    NvU8* WritePtr = (NvU8*)pWriteBuffer;
    NvU32 PhysAddr = hSdio->pPhysBuffer;
    NvBool IsTransferCompleted = NV_FALSE;
    NvU32 IntrStatus = 0;
    NvBool IsDmaStarted = NV_FALSE;
    NvU32 PollingTimeOut = 0;

    BytesToBeSent = (NumOfBytesToWrite > SDMMC_DMA_TRANSFER_SIZE) ?
                          SDMMC_DMA_TRANSFER_SIZE : NumOfBytesToWrite;

    PollingTimeOut = (NumOfBytesToWrite > SDMMC_TIMEOUT_CALCULATION_SIZE) ?
                  (SDMMC_MIN_POLLING_TIME_USEC * (NumOfBytesToWrite/
                  SDMMC_TIMEOUT_CALCULATION_SIZE)) : SDMMC_MIN_POLLING_TIME_USEC;
    if (NumOfBytesToWrite > SDMMC_DMA_TRANSFER_SIZE)
    {
        ConfigureInterrupts(hSdio, 0, SDIO_INTERRUPTS,
                    (SDIO_ERROR_INTERRUPTS|NvDdkSdioCommandStatus_TransferComplete|
                                                               NvDdkSdioCommandStatus_DMA));
    }
    else
    {
        ConfigureInterrupts(hSdio, 0, SDIO_INTERRUPTS, (SDIO_ERROR_INTERRUPTS|
                                                                NvDdkSdioCommandStatus_TransferComplete));
    }

    NvOsMemcpy(VirtAddr, pWriteBuffer, BytesToBeSent);
    NvOsFlushWriteCombineBuffer();

    /* this function enables the sdio clock */
    Error = SdioBlockTransfer(hSdio,
                    NumOfBytesToWrite,
                    hSdio->pPhysBuffer,
                    pRWCommand,
                    HWAutoCMD12Enable,
                    NV_FALSE);
    if (Error != NvSuccess)
    {
        // Reset the cmd and data lines
        PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
            SDMMC_SW_RESET_FOR_DATA_LINE));
        *SdioStatus = Error;
        return Error;
    }

    IsDmaStarted = NV_TRUE;
    // poll for the transfer complete interrupt
    while (PollTime < PollingTimeOut)
    {
        if (IsDmaStarted)
        {
            WritePtr += BytesToBeSent;
            BytesSent += BytesToBeSent;
            BytesToBeSent = NumOfBytesToWrite - BytesSent;

            BytesToBeSent =
                (BytesToBeSent > SDMMC_DMA_TRANSFER_SIZE) ? SDMMC_DMA_TRANSFER_SIZE : BytesToBeSent;

            if (BytesToBeSent)
            {
                PhysAddr = (PhysAddr == hSdio->pPhysBuffer) ?
                    (PhysAddr + SDMMC_DMA_TRANSFER_SIZE) : hSdio->pPhysBuffer;
                VirtAddr = (VirtAddr == hSdio->pVirtBuffer) ?
                    (VirtAddr + SDMMC_DMA_TRANSFER_SIZE) : hSdio->pVirtBuffer;
                NvOsMemcpy(VirtAddr, WritePtr, BytesToBeSent);
                NvOsFlushWriteCombineBuffer();
            }
            IsDmaStarted = NV_FALSE;
        }
        IntrStatus = SDMMC_REGR(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS);
        if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, ERR_INTERRUPT, IntrStatus))
        {
            // Clear the error interrupts
            IntrStatus &= SDIO_ERROR_INTERRUPTS;
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, IntrStatus);
            *SdioStatus = IntrStatus;
            // Reset the cmd and data lines
            PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
                SDMMC_SW_RESET_FOR_DATA_LINE));
            Error = NvError_SdioWriteFailed;
            SD_ERROR_PRINT("SDIO_DDK polling write failed error[0x%x] \
                        Instance[%d]\n", *SdioStatus, hSdio->Instance);
            break;
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, IntrStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, GEN_INT);
            // clearing the DMA interrupt
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, IntrStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            IsTransferCompleted = NV_TRUE;
        }
        else
        {
            NvOsWaitUS(SDMMC_POLLING_DELAY_USEC);
            PollTime += SDMMC_POLLING_DELAY_USEC;
            continue;
        }

    if (BytesToBeSent)
    {
       SdioProgramDmaAddress(hSdio, PhysAddr);
       IsDmaStarted = NV_TRUE;
    }
    else if (IsTransferCompleted)
    {
        break;
    }
    NvOsWaitUS(SDMMC_POLLING_DELAY_USEC);
    PollTime += SDMMC_POLLING_DELAY_USEC;
    }

    if (PollTime >= PollingTimeOut)
    {
        PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_CMD_LINE |
            SDMMC_SW_RESET_FOR_DATA_LINE));
        Error = NvError_Timeout;
        SD_ERROR_PRINT("SDIO_DDK polling write timed out for Instance[%d]\n",
                        hSdio->Instance);
    }

    return Error;
}

NvError
NvDdkSdioWrite(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToWrite,
    void *pWriteBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus)
{
    NvError Error = NvError_SdioWriteFailed;
    NvDdkSdioStatus* ControllerStatus = NULL;
    NvU32 BytesToBeSent = 0;
    NvU32 BytesSent = 0;
    NvU8* WritePtr = (NvU8*)pWriteBuffer;
    NvU8* VirtAddr = (NvU8*)hSdio->pVirtBuffer;
    NvU32 PhysAddr = hSdio->pPhysBuffer;
    NvBool DisableDmaInterrupt = NV_FALSE;

    NV_ASSERT(hSdio);
    NV_ASSERT(pWriteBuffer);
    NV_ASSERT(pRWCommand);

    *SdioStatus = NvDdkSdioError_None;

    // To get better performance, use polling (to eliminate interrupt latency) for lower transfer sizes.
    if (NumOfBytesToWrite <= SDMMC_MAX_POLLING_SIZE)
    {
        Error = PrivSdioPollingWrite(hSdio, NumOfBytesToWrite, pWriteBuffer,
                            pRWCommand, HWAutoCMD12Enable, SdioStatus);
        return Error;
    }

    ControllerStatus = hSdio->ControllerStatus;
    BytesToBeSent = (NumOfBytesToWrite > SDMMC_DMA_TRANSFER_SIZE) ?
                          SDMMC_DMA_TRANSFER_SIZE : NumOfBytesToWrite;

    if (NumOfBytesToWrite == SDMMC_DMA_TRANSFER_SIZE)
    {
        ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, (NvDdkSdioCommandStatus_CommandComplete|NvDdkSdioCommandStatus_DMA), 0);
    }
    else
    {
        ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, NvDdkSdioCommandStatus_CommandComplete, 0);
    }

    NvOsMemcpy(VirtAddr, WritePtr, BytesToBeSent);
    NvOsFlushWriteCombineBuffer();

    /* this enables the sdio clock */
    Error = SdioBlockTransfer(hSdio,
                              NumOfBytesToWrite,
                              PhysAddr,
                              pRWCommand,
                              HWAutoCMD12Enable,
                              NV_FALSE);
    if (Error != NvSuccess)
    {
        *SdioStatus = Error;
        return Error;
    }

    // loop till we receive all the requested bytes
    while (BytesSent != NumOfBytesToWrite)
    {
        WritePtr += BytesToBeSent;
        BytesSent += BytesToBeSent;
        BytesToBeSent = NumOfBytesToWrite - BytesSent;
        if (BytesToBeSent)
        {
            if (BytesToBeSent <= SDMMC_DMA_TRANSFER_SIZE)
            {
                DisableDmaInterrupt = NV_TRUE;
            }
            else if (BytesToBeSent > SDMMC_DMA_TRANSFER_SIZE)
            {
                BytesToBeSent = SDMMC_DMA_TRANSFER_SIZE;
            }
            if (PhysAddr == hSdio->pPhysBuffer)
            {
                PhysAddr += SDMMC_DMA_TRANSFER_SIZE;
                VirtAddr += SDMMC_DMA_TRANSFER_SIZE;
            }
            else
            {
                 PhysAddr = hSdio->pPhysBuffer;
                 VirtAddr = hSdio->pVirtBuffer;
            }
            NvOsMemcpy(VirtAddr, WritePtr, BytesToBeSent);
            NvOsFlushWriteCombineBuffer();
        }
        Error = NvOsSemaphoreWaitTimeout(hSdio->PrivSdioSema,
                                                                NVDDK_SDMMC_DATA_TIMEOUT_MSEC);
        if (Error == NvSuccess)
        {
            if (ControllerStatus->SDErrorStatus != NvDdkSdioError_None)
            {
                 *SdioStatus = ControllerStatus->SDErrorStatus;
                if (ControllerStatus->SDErrorStatus & SDIO_ERROR_INTERRUPTS)
                {
                    PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_DATA_LINE |
                        SDMMC_SW_RESET_FOR_CMD_LINE));
                }
                SD_ERROR_PRINT("SDIO_DDK write failed error[0x%x] \
                            Instance[%d]\n", *SdioStatus, hSdio->Instance);
                return Error;
            }
            if (BytesToBeSent == 0)
            {
                break;
            }
            else
            {
                if (DisableDmaInterrupt)
                {
                    ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, (NvDdkSdioCommandStatus_DMA|NvDdkSdioCommandStatus_CommandComplete), 0);
                }
                SdioProgramDmaAddress(hSdio, PhysAddr);
            }
        }
        else
        {
            *SdioStatus = NvError_Timeout;
             PrivSdioReset(hSdio, (SDMMC_SW_RESET_FOR_DATA_LINE |
                 SDMMC_SW_RESET_FOR_CMD_LINE));
            SD_ERROR_PRINT("SDIO_DDK write timed out for Instance[%d]\n",
                                hSdio->Instance);
            return Error;
        }
    }

    *SdioStatus = ControllerStatus->SDErrorStatus;
    return Error;
}

NvError
SdioGetPhysAdd(
    NvRmDeviceHandle hRmDevice,
    NvRmMemHandle* hRmMemHandle,
    void** pVirtBuffer,
    NvU32 size,
    NvU32* pPhysBuffer)
{
    NvU32 SDMMC_ALIGNMENT_SIZE = SDMMC_DMA_TRANSFER_SIZE;
    NvError e = NvError_InvalidAddress;

    // Initialise the handle to NULL
    *pPhysBuffer = 0;
    *hRmMemHandle = NULL;

    /* Currently NvRmMemAlloc() can't return memory address that is aligned
     * to the given alignment value (which is more than page size in particular)
     * So, (size + SDMMC_ALIGNMENT_SIZE - 1) is alloced and then the address is
     * aligned to the required alignment explicitely.
     */
    size += (SDMMC_ALIGNMENT_SIZE - 1);

    // Create the Memory Handle and allocate memory
    NV_CHECK_ERROR(NvRmMemHandleAlloc(hRmDevice, NULL, 0,
        SDMMC_ALIGNMENT_SIZE, NvOsMemAttribute_WriteCombined,
        size, 0, 1, hRmMemHandle));

    // Pin the memory and Get Physical Address
    *pPhysBuffer = NvRmMemPin(*hRmMemHandle);
    *pPhysBuffer = (*pPhysBuffer + SDMMC_ALIGNMENT_SIZE - 1) &
                   (~(SDMMC_ALIGNMENT_SIZE - 1));

    e = NvRmMemMap(*hRmMemHandle, 0, size, NVOS_MEM_READ_WRITE, pVirtBuffer);
    if (e != NvSuccess)
    {
        NvRmMemUnpin(*hRmMemHandle);
        goto fail;
    }
    *pVirtBuffer = (void*)((NvU32)((NvU32)(*pVirtBuffer) + SDMMC_ALIGNMENT_SIZE - 1) &
                          (~(SDMMC_ALIGNMENT_SIZE - 1)));

    return NvSuccess;
fail:
    if (hRmMemHandle)
        NvRmMemHandleFree(*hRmMemHandle);
    return e;
}

NvError
SdioBlockTransfer(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfRWBytes,
    NvU32 pReadBuffer,
    NvDdkSdioCommand *pRWRequest,
    NvBool HWAutoCMD12Enable,
    NvBool IsRead)
{
    NvBool IsCrcCheckEnable = NV_FALSE;
    NvBool IsIndexCheckEnable = NV_FALSE;
    NvU32 IsControllerBusy = 0;
    NvBool IsDataCommand = NV_TRUE;
    NvU32 NumOfBlocks = 0;
    NvU32 CmdXferReg = 0;
    NvU32 BlkSizeCountReg = 0;
    NvU32 SdioBlockSize = 512;
    NvDdkSdioStatus* ControllerStatus = NULL;

    if (pRWRequest->BlockSize > hSdio->MaxBlockLength)
    {
         return NvError_SdioBadBlockSize;
    }

    ControllerStatus = hSdio->ControllerStatus;
    ControllerStatus->SDControllerStatus = NvDdkSdioCommandStatus_None;
    ControllerStatus->SDErrorStatus = NvDdkSdioError_None;

    hSdio->RequestType = NVDDK_SDIO_NORMAL_REQUEST;
    hSdio->IsRead = IsRead;
    SdioBlockSize = pRWRequest->BlockSize;
    NumOfBlocks = NumOfRWBytes/SdioBlockSize;
    if (NumOfBlocks > SDMMC_MAX_NUMBER_OF_BLOCKS)
        return NvError_InvalidSize;
    // set DMA (system) address
    SdioProgramDmaAddress(hSdio, pReadBuffer);
    if (NumOfBlocks > 1)
    {
        // Enable multi block in Cmd xfer register
        CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, MULTI_BLOCK_SELECT, ENABLE);
        // Enable Block Count (used in case of multi-block transfers)
        CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, BLOCK_COUNT_EN, ENABLE);
        // set Auto CMD12 (stop transmission)
        CmdXferReg |=
            NV_DRF_NUM(SDMMC, CMD_XFER_MODE, AUTO_CMD12_EN, HWAutoCMD12Enable);
    }
    BlkSizeCountReg |= NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT,
        XFER_BLOCK_SIZE_11_0, SdioBlockSize);
    // set number of blocks to be read/written (block count)
    BlkSizeCountReg |=
        NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT, BLOCKS_COUNT, NumOfBlocks);
    BlkSizeCountReg |= NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT,
                                        HOST_DMA_BUFFER_SIZE, SDMMC_DMA_BUFFER_SIZE);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, BLOCK_SIZE_BLOCK_COUNT, BlkSizeCountReg);

    // set the command number
    CmdXferReg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX,
        pRWRequest->CommandCode);
    // set the data command bit
    CmdXferReg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT,
        IsDataCommand);
    // set the transfer direction
    CmdXferReg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL, IsRead);
    // enable DMA
    CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DMA_EN, ENABLE);
    // set the cmd type
    CmdXferReg |=
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_TYPE, pRWRequest->CommandType);
    /* set the response type */
    switch (pRWRequest->ResponseType)
    {
        case NvDdkSdioRespType_NoResp:
            IsCrcCheckEnable = NV_FALSE;
            IsIndexCheckEnable = NV_FALSE;
            CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                NO_RESPONSE);
        break;
        case NvDdkSdioRespType_R1:
        case NvDdkSdioRespType_R5:
        case NvDdkSdioRespType_R6:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_TRUE;
            CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                RESP_LENGTH_48);
        break;
        case NvDdkSdioRespType_R2:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_FALSE;
            CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                RESP_LENGTH_136);
        break;
        case NvDdkSdioRespType_R3:
        case NvDdkSdioRespType_R4:
            IsCrcCheckEnable = NV_FALSE;
            IsIndexCheckEnable = NV_FALSE;
            CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                RESP_LENGTH_48);
        break;
        case NvDdkSdioRespType_R1b:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_TRUE;
            CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                RESP_LENGTH_48BUSY);
            break;
        case NvDdkSdioRespType_R7:
            IsCrcCheckEnable = NV_TRUE;
            IsIndexCheckEnable = NV_FALSE;
            CmdXferReg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
                RESP_LENGTH_48);
            break;
        default:
             NV_ASSERT(0);
    }
    // enable cmd index check
    CmdXferReg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_INDEX_CHECK_EN,
        IsIndexCheckEnable);
    // enable cmd crc check
    CmdXferReg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_CRC_CHECK_EN,
        IsCrcCheckEnable);

    IsControllerBusy = SdioIsControllerBusy(hSdio, NV_TRUE);
    if (IsControllerBusy)
    {
        NV_DEBUG_PRINTF(("controller is busy\n"));
        return NvError_SdioControllerBusy;
    }
    /* write the command argument */
    SDMMC_REGW(hSdio->pSdioVirtualAddress, ARGUMENT, pRWRequest->CmdArgument);
    /* now write to the command xfer register */
    SDMMC_REGW(hSdio->pSdioVirtualAddress, CMD_XFER_MODE, CmdXferReg);

    SD_DEBUG_PRINT("NvDdkSdioSendCommand: Command Number = %d, Argument = %x, "
                      "Xfer mode = %x\n", pRWRequest->CommandCode,
                      pRWRequest->CmdArgument, CmdXferReg);
    return NvSuccess;
}

static NvU32 SdioGetChipId(void)
{
    NvU32 ChipId = 0;
    NvU32        reg;
    NvU32        misc = MISC_BASE_ADDR + APB_MISC_GP_HIDREV_0;

    reg = NV_READ32(misc);
    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, reg);

    return ChipId;
}

void
PrivSdioGetCaps(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioHostCapabilities *pHostCap,
    NvU32 instance)
{
    NvRmDeviceHandle hDevice;
    static NvDdkSdioHostCapabilities* s_pSdioCap = NULL;
    NvU32 capabilities = 0;
    NvU32 val = 0;
    NvU32 ChipId;

    NV_ASSERT(hSdio);
    NV_ASSERT(pHostCap);

    hDevice = hSdio->hRm;
    ChipId = SdioGetChipId();

    /* Form T148 onwards, h/w is not updating major/minor numbers for SDMMC
    So, defining capabilities based on chip id */

    s_pSdioCap = NvOsAlloc(sizeof(NvDdkSdioHostCapabilities));
    NvOsMemset(s_pSdioCap, 0, sizeof(NvDdkSdioHostCapabilities));
    s_pSdioCap->IsAutoCMD12Supported = NV_TRUE;
    s_pSdioCap->MaxInstances = NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Sdio);

    switch (ChipId)
    {
        case 0x30:
            s_pSdioCap->HwInterface = HWINTERFACE_T30;
            s_pSdioCap->IsDDR50modeSupported = NV_TRUE;
            T30SdioOpenHWInterface(hSdio);
            break;
        case 0x35:
        case 0x14:
        case 0x40:
            s_pSdioCap->HwInterface = HWINTERFACE_T1xx;
            s_pSdioCap->IsDDR50modeSupported = NV_TRUE;
            T1xxSdioOpenHWInterface(hSdio);
            break;
        default:
            NV_ASSERT(0);
    }

    // Fill the client capabilities structure.
    NvOsMemcpy(pHostCap, s_pSdioCap, sizeof(NvDdkSdioHostCapabilities));

    capabilities = SDMMC_REGR(hSdio->pSdioVirtualAddress, CAPABILITIES);
    val = NV_DRF_VAL(SDMMC, CAPABILITIES, MAX_BLOCK_LENGTH, capabilities);
    switch (val)
    {
      case SDMMC_CAPABILITIES_0_MAX_BLOCK_LENGTH_BYTE512:
           pHostCap->MaxBlockLength = SDMMC_MAX_BLOCK_SIZE_512;
           break;
      case SDMMC_CAPABILITIES_0_MAX_BLOCK_LENGTH_BYTE1024:
           pHostCap->MaxBlockLength = SDMMC_MAX_BLOCK_SIZE_1024;
           break;
      case SDMMC_CAPABILITIES_0_MAX_BLOCK_LENGTH_BYTE2048:
           pHostCap->MaxBlockLength = SDMMC_MAX_BLOCK_SIZE_2048;
           break;
     default:
           pHostCap->MaxBlockLength = SDMMC_DEFAULT_BLOCK_SIZE;
           break;
    }
    hSdio->MaxBlockLength = pHostCap->MaxBlockLength;

    val = NV_DRF_VAL(SDMMC, CAPABILITIES, VOLTAGE_SUPPORT_3_3_V, capabilities);
    if (val)
    {
        pHostCap->BusVoltage = NvDdkSdioSDBusVoltage_3_3;
    }
    else
    {
        val = NV_DRF_VAL(SDMMC, CAPABILITIES, VOLTAGE_SUPPORT_3_0_V, capabilities);
        if (val)
        {
                pHostCap->BusVoltage = NvDdkSdioSDBusVoltage_3_0;
        }
        else
        {
            val = NV_DRF_VAL(SDMMC, CAPABILITIES, VOLTAGE_SUPPORT_1_8_V, capabilities);
            if (val)
            {
                pHostCap->BusVoltage = NvDdkSdioSDBusVoltage_1_8;
            }
            else
            {
                // Invalid bus voltage
                NV_ASSERT(0);
            }
        }
    }

    val = NV_DRF_VAL(SDMMC, CAPABILITIES, EXTENDED_MEDIA_BUS_SUPPORT, capabilities);
    if (val)
        pHostCap->Is8bitmodeSupported = NV_TRUE;
    val = NV_DRF_VAL(SDMMC, CAPABILITIES, ADMA2_SUPPORT, capabilities);
    if (val)
        pHostCap->IsAdma2Supported = NV_TRUE;
    val = NV_DRF_VAL(SDMMC, CAPABILITIES, HIGH_SPEED_SUPPORT, capabilities);
    if (val)
        pHostCap->IsHighSpeedSupported= NV_TRUE;

    hSdio->NvDdkSdioGetHigherCapabilities(hSdio, pHostCap);
}

NvError
NvDdkSdioGetCapabilities(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioHostCapabilities *pHostCap,
    NvDdkSdioInterfaceCapabilities *pInterfaceCap,
    NvU32 instance)
{
    NvRmDeviceHandle hDevice = hSdio->hRm;
    NvRmModuleSdmmcInterfaceCaps SdioCaps;
    const NvOdmQuerySdioInterfaceProperty* pSdioInterfaceCaps = NULL;
    NvError Error = NvSuccess;

    PrivSdioGetCaps(hSdio, pHostCap, instance);

    Error = NvRmGetModuleInterfaceCapabilities(
                    hDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Sdio, instance),
                    sizeof(NvRmModuleSdmmcInterfaceCaps),
                    &SdioCaps);
    if (Error != NvSuccess)
        return Error;

    pInterfaceCap->SDIOCardSettlingDelayMSec = 0;
    pSdioInterfaceCaps = NvOdmQueryGetSdioInterfaceProperty(instance);
    if (pSdioInterfaceCaps)
    {
        pInterfaceCap->SDIOCardSettlingDelayMSec =  pSdioInterfaceCaps->SDIOCardSettlingDelayMSec;
        pInterfaceCap->MmcInterfaceWidth = SdioCaps.MmcInterfaceWidth;
    }

    return NvSuccess;
}

NvError NvDdkSdioSetUhsmode(NvDdkSdioDeviceHandle hSdio, NvDdkSdioUhsMode Uhsmode)
{
    if (Uhsmode > 4)
        return NvError_BadParameter;

    // Set uhs mode
    hSdio->NvDdkSdioConfigureUhsMode(hSdio, Uhsmode);

    SD_INFO_PRINT("NvDdkSdioSetUshmode: Uhsmode = %d\n", Uhsmode);
    // In DDR mode, the I/O clock should be half of controller clock.
    if (Uhsmode == NvDdkSdioUhsMode_DDR50)
        SdioSetSlotClockRate(hSdio, NvDdkSdioClkDivider_DIV_2);

    hSdio->Uhsmode = Uhsmode;
    return NvSuccess;
}

NvError
NvDdkSdioSetHostBusWidth(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioDataWidth CardDataWidth)
{
    NvU32 DataWidthReg = 0;
    NvRmModuleSdmmcInterfaceCaps SdioCaps;

    NV_ASSERT(hSdio);

    // set the buswidth
    if (CardDataWidth != NvDdkSdioDataWidth_8Bit)
    {
        DataWidthReg = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
        DataWidthReg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST, DATA_XFER_WIDTH,
                                 CardDataWidth, DataWidthReg);
        DataWidthReg = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                            EXTENDED_DATA_TRANSFER_WIDTH, NOBIT_8, DataWidthReg);
    }
    else
    {
        NV_ASSERT_SUCCESS(NvRmGetModuleInterfaceCapabilities(
                hSdio->hRm,
                NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
                sizeof(NvRmModuleSdmmcInterfaceCaps),
                &SdioCaps));
        // check if 8bit mode is supported
        if (SdioCaps.MmcInterfaceWidth != 8)
        {
            return NvError_NotSupported;
        }

        DataWidthReg = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
        DataWidthReg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST, DATA_XFER_WIDTH,
                                 0, DataWidthReg);
        DataWidthReg = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                                EXTENDED_DATA_TRANSFER_WIDTH, BIT_8, DataWidthReg);
    }
    // now write to the power control host register
    SDMMC_REGW(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST, DataWidthReg);
    hSdio->BusWidth = CardDataWidth;
    return NvSuccess;
}

NvError NvDdkSdioHighSpeedEnable(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 val = 0;

    NV_ASSERT(hSdio);
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, CAPABILITIES);
    val = NV_DRF_VAL(SDMMC, CAPABILITIES, HIGH_SPEED_SUPPORT, val);
    if (val)
    {
        // set the high speed enable
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
        val = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST, HIGH_SPEED_EN,
                    HIGH_SPEED, val);
        SDMMC_REGW(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST, val);
    }
    return NvSuccess;
}

NvError NvDdkSdioHighSpeedDisable(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 val = 0;

    NV_ASSERT(hSdio);
    // disable high speed mode
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
    val = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST, HIGH_SPEED_EN,
                NORMAL_SPEED, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST, val);
    return NvSuccess;
}

void NvDdkSdioResetController(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 val = 0;

    NV_ASSERT(hSdio);
    // reset the controller write (1) to the reset_all field.
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
    val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                 SW_RESET_FOR_ALL, RESETED, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
}

void NvDdkSdioClose(NvDdkSdioDeviceHandle hSdio)
{
    if (hSdio != NULL)
    {
        /* disable power */
        NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(hSdio->hRm,
            NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
            hSdio->SdioRmPowerClientId,
            NvRmVoltsOff,
            NvRmVoltsOff,
            NULL,
            0,
            NULL));

        NvRmMemUnmap(hSdio->hRmMemHandle, hSdio->pVirtBuffer, SDMMC_MEMORY_BUFFER_SIZE);
        NvRmMemUnpin(hSdio->hRmMemHandle);
        NvRmMemHandleFree(hSdio->hRmMemHandle);

        SdioConfigureCardClock(hSdio, NV_FALSE);

        // disable clock to sdio controller
        NvRmPowerModuleClockControl(hSdio->hRm,
            NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
            hSdio->SdioRmPowerClientId,
            NV_FALSE);

        // unregister with the power manager
        NvRmPowerUnRegister(hSdio->hRm, hSdio->SdioRmPowerClientId);

        // unregister/disable interrupt handler
        NvRmInterruptUnregister(hSdio->hRm, hSdio->InterruptHandle);
        hSdio->InterruptHandle = NULL;

        // destory the internal sdio semaphore
        NvOsSemaphoreDestroy(hSdio->PrivSdioSema);
        hSdio->PrivSdioSema = NULL;

        // Unmap the sdio register virtual address space
        NvOsPhysicalMemUnmap(hSdio->pSdioVirtualAddress,
                             hSdio->SdioBankSize);

        // disable power rail, if applicable
        NV_ASSERT_SUCCESS(SdioEnablePowerRail(hSdio, NV_FALSE));

        NvOsIntrMutexDestroy(hSdio->SdioThreadSafetyMutex);
        hSdio->SdioThreadSafetyMutex = NULL;
        NvOsFree(hSdio->ControllerStatus);
        hSdio->ControllerStatus = NULL;
        NvOsSemaphoreDestroy(hSdio->SdioPowerMgtSema);
        hSdio->SdioPowerMgtSema = NULL;
        NvOsFree(hSdio);
        hSdio = NULL;
  }
}

NvError
NvDdkSdioIsCardInserted(
        NvDdkSdioDeviceHandle hSdio,
        NvBool *IscardInserted)
{
    return NvError_SdioCardAlwaysPresent;
}

NvError
NvDdkSdioIsWriteProtected(
        NvDdkSdioDeviceHandle hSdio,
        NvBool *IsWriteprotected)
{
    *IsWriteprotected = NV_FALSE;
    return NvSuccess;
}

NvError
SdioRegisterInterrupts(
    NvRmDeviceHandle hRm,
    NvDdkSdioDeviceHandle hSdio)
{
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers;

    if (hSdio->InterruptHandle)
    {
        return NvSuccess;
    }
    IrqList = NvRmGetIrqForLogicalInterrupt(
        hRm, NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance), 0);
    IntHandlers = SdioIsr;
    return NvRmInterruptRegister(hRm, 1, &IrqList, &IntHandlers, hSdio,
            &hSdio->InterruptHandle, NV_TRUE);
}

static void SdioIsr(void* args)
{
    NvDdkSdioDeviceHandle hSdio;
    volatile NvU32 InterruptStatus = 0;
    volatile NvU32 val = 0;
    NvDdkSdioStatus* ControllerStatus = NULL;

    hSdio = args;

    // get the interrupt status
    InterruptStatus = SDMMC_REGR(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS);
    ControllerStatus = hSdio->ControllerStatus;

    SD_DEBUG_PRINT("SdioIsr: Interrupt Status: %08x\n", InterruptStatus);
    if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, BUFFER_READ_READY, InterruptStatus))
    {
        val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, BUFFER_READ_READY, GEN_INT);
        SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
        NvOsSemaphoreSignal(hSdio->PrivSdioSema);
    }
    if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, BLOCK_GAP_EVENT, InterruptStatus))
    {
        if (hSdio->IsRead == NV_TRUE)
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, GEN_INT);
            val |= NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, BLOCK_GAP_EVENT, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS,val);
        }
        else
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, GEN_INT);
            val |= NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, BLOCK_GAP_EVENT, GEN_INT);
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
        }
        NvOsSemaphoreSignal(hSdio->PrivSdioSema);
    }
    else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, ERR_INTERRUPT, InterruptStatus))
    {
        SD_ERROR_PRINT("SdioIsr: Error Interrupt Status: %08x\n", InterruptStatus);
        ControllerStatus->SDErrorStatus = InterruptStatus & SDIO_ERROR_INTERRUPTS;
        if (ControllerStatus->SDErrorStatus & SDIO_CMD_ERROR_INTERRUPTS)
        {
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS,
                (InterruptStatus & SDIO_CMD_ERROR_INTERRUPTS));
        }
        if (ControllerStatus->SDErrorStatus & SDIO_DATA_ERROR_INTERRUPTS)
        {
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS,
                (InterruptStatus & SDIO_DATA_ERROR_INTERRUPTS));
        }
        NvOsSemaphoreSignal(hSdio->PrivSdioSema);
    }
    else
    {
        val = 0;
        if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, CMD_COMPLETE, InterruptStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, CMD_COMPLETE, GEN_INT);
            ControllerStatus->SDControllerStatus |=
                NvDdkSdioCommandStatus_CommandComplete;
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            NvOsSemaphoreSignal(hSdio->PrivSdioSema);
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, InterruptStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, GEN_INT);
            ControllerStatus->SDControllerStatus |= NvDdkSdioCommandStatus_DMA;
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            NvOsSemaphoreSignal(hSdio->PrivSdioSema);
        }
        else if (NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, InterruptStatus))
        {
            val = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, GEN_INT);
            ControllerStatus->SDControllerStatus |=
                NvDdkSdioCommandStatus_TransferComplete;
            SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS, val);
            NvOsSemaphoreSignal(hSdio->PrivSdioSema);
        }
        else
        {
            SD_DEBUG_PRINT("Interrupt Status 0x%x Instance[%d]\n",
                    InterruptStatus, hSdio->Instance);
            //NV_ASSERT(!"SDIO_DDK Invalid Interrupt");
        }
    }

    if (hSdio->InterruptHandle)
    {
        NvRmInterruptDone(hSdio->InterruptHandle);
    }
}

void
NvDdkSdioSetSDBusVoltage(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioSDBusVoltage Voltage)
{
    NvU32 val = 0;

    NV_ASSERT(hSdio);
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
    val |= NV_DRF_NUM(SDMMC, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, Voltage);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST, val);
    hSdio->BusVoltage = Voltage;
}

static void
SdioSetSlotClockRate(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioClkDivider Divider)
{
    NvU32 val = 0;
    SD_INFO_PRINT("Divider: %d\n", Divider);
    // disable clk
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
    val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                             SD_CLOCK_EN, DISABLE, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress,
              SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
    val = NV_FLD_SET_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                                      SDCLK_FREQUENCYSELECT, Divider, val);
    val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                             SD_CLOCK_EN, ENABLE, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
}

NvError
NvDdkSdioSetClockFrequency(
    NvDdkSdioDeviceHandle hSdio,
    NvRmFreqKHz FrequencyKHz,
    NvRmFreqKHz* pConfiguredFrequencyKHz)
{
    NvError e = NvSuccess;
    NvRmFreqKHz PrefFreqList[1];
    NvRmFreqKHz CurrentFreq;
    NvRmFreqKHz NewFreq;
    NvU32 i, Divider = 0;
    NvU32 Difference = 0xFFFFFFFF;

    NV_ASSERT(hSdio);

    PrefFreqList[0] = FrequencyKHz;
    //enable clock to sdio controller
    NV_CHECK_ERROR(NvRmPowerModuleClockControl(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
        hSdio->SdioRmPowerClientId,
        NV_TRUE));

    SD_INFO_PRINT("PrefFreqList = %d\n", PrefFreqList[0]);
    // request for clk
    NV_CHECK_ERROR(NvRmPowerModuleClockConfig(hSdio->hRm,
           NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
           hSdio->SdioRmPowerClientId,
           SdioNormalModeMinFreq,
           SdioSDR104ModeMaxFreq,
           &PrefFreqList[0],
           1,
           &CurrentFreq, NvRmClockConfig_QuietOverClock));

    SD_INFO_PRINT("CurrentFreq = %d\n", CurrentFreq);

    // In DDR mode, the IO clock should be half of controller clock
    // Use clock divider 1
    if (hSdio->Uhsmode == NvDdkSdioUhsMode_DDR50)
    {
        SdioSetSlotClockRate(hSdio, NvDdkSdioClkDivider_DIV_2);
    }
    else
    {
        if (PrefFreqList[0] < CurrentFreq)
        {
            for (i = 0; i < 9; i++)
            {
                NewFreq = (CurrentFreq >> i);
                if ((DIFF_FREQ(PrefFreqList[0], NewFreq)) < Difference)
                {
                    Difference = DIFF_FREQ(PrefFreqList[0], NewFreq);
                    Divider = i;
                }
            }
        }

        if (pConfiguredFrequencyKHz != NULL)
        {
            *pConfiguredFrequencyKHz = (CurrentFreq >> Divider);
        }

        // set the clk divider to zero
        if (Divider == 0)
        {
            // divider should be non zero on FPGA platform.
            // This is due to FPGA limitation.
            if (IsCurrentPlatformEmulation() == NV_TRUE)
                SdioSetSlotClockRate(hSdio, NvDdkSdioClkDivider_DIV_2);
            else
                SdioSetSlotClockRate(hSdio, NvDdkSdioClkDivider_DIV_BASE);
        }
        else
        {
            SdioSetSlotClockRate(hSdio, (NvDdkSdioClkDivider)(1 <<(Divider - 1)));
        }
    }

    hSdio->ConfiguredFrequency = CurrentFreq;
    return NvSuccess;
}

NvError NvDdkSdioSetBlocksize(NvDdkSdioDeviceHandle hSdio, NvU32 Blocksize)
{
    NvU32 val = 0;

    NV_ASSERT(hSdio);

    if (Blocksize > hSdio->MaxBlockLength)
    {
         return NvError_SdioBadBlockSize;
    }

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, BLOCK_SIZE_BLOCK_COUNT);
    val = NV_FLD_SET_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT, XFER_BLOCK_SIZE_11_0,
               Blocksize, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, BLOCK_SIZE_BLOCK_COUNT, val);

    // read back the block size
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, BLOCK_SIZE_BLOCK_COUNT);
    val = NV_DRF_VAL(SDMMC, BLOCK_SIZE_BLOCK_COUNT, XFER_BLOCK_SIZE_11_0, val);
    if (val != Blocksize)
    {
        return NvError_SdioBadBlockSize;
    }

    return NvSuccess;
}

NvBool SdioEnableInternalClock(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 val = 0;
    NvU32 Timeout = 0;

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress,
                    SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
    val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                             INTERNAL_CLOCK_EN, OSCILLATE, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
    while (Timeout != SDMMC_INTERNAL_CLOCK_TIMEOUT_USEC)
    {
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress,
                        SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
        val = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                         INTERNAL_CLOCK_STABLE, val);
        if (val)
        {
            break;
        }
        NvOsWaitUS(SDMMC_INTERNAL_CLOCK_TIMEOUT_STEP_USEC);
        Timeout += SDMMC_INTERNAL_CLOCK_TIMEOUT_STEP_USEC;
    }

    return (val);
}

NvError SdioConfigureCardClock(NvDdkSdioDeviceHandle hSdio, NvBool IsEnable)
{
    NvU32 val = 0;
    NvU32 IsControllerBusy = 0;

    NV_ASSERT(hSdio);

    NvOsIntrMutexLock(hSdio->SdioThreadSafetyMutex);
    if (IsEnable)
    {
        hSdio->NvDdkSdioDpdDisable(hSdio);
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
        val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                     SD_CLOCK_EN, ENABLE, val);
        SDMMC_REGW(hSdio->pSdioVirtualAddress,
                  SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
    }
    else
    {
         // check if controller is bsuy
        IsControllerBusy = SdioIsControllerBusy(hSdio, NV_TRUE);
        if (IsControllerBusy)
        {
            NvOsIntrMutexUnlock(hSdio->SdioThreadSafetyMutex);
            return NvError_SdioControllerBusy;
        }
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress,
                        SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
        val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SD_CLOCK_EN,
                                 DISABLE, val);
        SDMMC_REGW(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
        hSdio->NvDdkSdioDpdEnable(hSdio);
    }
    NvOsIntrMutexUnlock(hSdio->SdioThreadSafetyMutex);
    return NvSuccess;
}

NvError SdioEnableBusPower(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 val = 0;

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
    val = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST, SD_BUS_POWER, POWER_ON, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST, val);
    return NvSuccess;
}

NvError
    SdioSetDataTimeout(
        NvDdkSdioDeviceHandle hSdio,
        SdioDataTimeout SdioDataToCounter)
{
    NvU32 val = 0;

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
    val |= NV_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                  DATA_TIMEOUT_COUNTER_VALUE, SdioDataToCounter);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
    return NvSuccess;
}

static NvU32
SdioIsControllerBusy(
    NvDdkSdioDeviceHandle hSdio,
    NvBool CheckforDataInhibit)
{
    volatile NvU32 val = 0;
    NvU32 IsControllerBusy = 0;
    NvU32 timeout = 0;
    NvU32 mask = SDMMC_CMD_INHIBIT | SDMMC_DATA_INHIBIT;

    if (CheckforDataInhibit == NV_FALSE)
        mask &= ~SDMMC_DATA_INHIBIT;

    while (timeout <= SW_CONTROLLER_BUSY_TIMEOUT_USEC)
    {
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress, PRESENT_STATE);
        if (!(val & mask))
            break;
        NvOsWaitUS(SW_CONTROLLER_BUSY_TIMEOUT_STEP_USEC);
        timeout += SW_CONTROLLER_BUSY_TIMEOUT_STEP_USEC;
    }
    return IsControllerBusy;
}

void
    NvDdkSdioAbort(
    NvDdkSdioDeviceHandle hSdio,
    SdioRequestType RequestType,
    NvU32 FunctionNumber)
{
    NvU32 val;

     hSdio->RequestType = RequestType;

    // enable the stop at block gap
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST);
    val = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                STOP_AT_BLOCK_GAP_REQUEST, TRANSFER, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, POWER_CONTROL_HOST, val);
}

void PrivSdioReset(NvDdkSdioDeviceHandle hSdio, NvU32 mask)
{
    NvU32 val = 0;
    NvU32 Timeout = 0;

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress,
                 SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
    if (mask & SDMMC_SW_RESET_FOR_ALL)
    {
        val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                     SW_RESET_FOR_ALL, RESETED, val);
    }
    else
    {
        if (mask & SDMMC_SW_RESET_FOR_CMD_LINE)
            val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                         SW_RESET_FOR_CMD_LINE, RESETED, val);
        if (mask & SDMMC_SW_RESET_FOR_DATA_LINE)
            val = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                         SW_RESET_FOR_DAT_LINE, RESETED, val);
    }
    SDMMC_REGW(hSdio->pSdioVirtualAddress,
        SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, val);
     while (Timeout != SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL_USEC)
    {
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress,
                        SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
        if (!(val & mask))
            break;
        NvOsWaitUS(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL_STEP_USEC);
        Timeout += SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL_STEP_USEC;
    }
    if (Timeout >= SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL_USEC)
    {
        SD_ERROR_PRINT("SDIO_DDK COMD and DAT line reset failed\n");
    }
}

void ConfigureInterrupts(NvDdkSdioDeviceHandle hSdio, NvU32 IntrEnableMask, NvU32 IntrDisableMask, NvU32 IntrStatusEnableMask)
{
    NvU32 val = 0;

    NvOsIntrMutexLock(hSdio->SdioThreadSafetyMutex);

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS_ENABLE);
    val |= IntrEnableMask;
    val &= ~IntrDisableMask;
    SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_STATUS_ENABLE, (val |IntrStatusEnableMask));

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, INTERRUPT_SIGNAL_ENABLE);
    val |= IntrEnableMask;
    val &= ~IntrDisableMask;
    SDMMC_REGW(hSdio->pSdioVirtualAddress, INTERRUPT_SIGNAL_ENABLE, val);

    NvOsIntrMutexUnlock(hSdio->SdioThreadSafetyMutex);

}

void NvDdkSdioSuspend(NvDdkSdioDeviceHandle hSdio, NvBool SwitchOffSDDevice)
{
    NvU32 val = 0;


    // switch off the voltage to the sd device
    if (SwitchOffSDDevice == NV_TRUE)
    {
        NV_ASSERT_SUCCESS(SdioEnablePowerRail(hSdio, NV_FALSE));
    }

    if (!hSdio->ISControllerSuspended)
    {
         // disable the card clock
        SdioConfigureCardClock(hSdio, NV_FALSE);
        // Disable SDMMC_CLK bit in VENDOR_CLOCK_CONTROL register
        val = SDMMC_REGR(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL);
        val = NV_FLD_SET_DRF_DEF(SDMMC, VENDOR_CLOCK_CNTRL,
                    SDMMC_CLK, DISABLE, val);
        SDMMC_REGW(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL, val);

        // disable clock to sdio controller
        NvRmPowerModuleClockControl(hSdio->hRm,
            NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
            hSdio->SdioRmPowerClientId,
            NV_FALSE);
    }

    /* Report RM to disable power */
    NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
        hSdio->SdioRmPowerClientId,
        NvRmVoltsOff,
        NvRmVoltsOff,
        NULL,
        0,
        NULL));
        hSdio->ISControllerSuspended = NV_TRUE;
}

NvError
NvDdkSdioResume(
    NvDdkSdioDeviceHandle hSdio,
    NvBool SwitchOnSDDevice)
{
    NvError e = NvSuccess;
    NvBool IsClkStable = NV_FALSE;
    NvRmFreqKHz pConfiguredFrequencyKHz = 0;
    NvU32 val = 0;

    // switch on the voltage to the sd device
    if (SwitchOnSDDevice == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(SdioEnablePowerRail(hSdio, NV_TRUE));
    }

     /* enable power */
    NV_CHECK_ERROR_CLEANUP(NvRmPowerVoltageControl(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
        hSdio->SdioRmPowerClientId,
        NvRmVoltsUnspecified,
        NvRmVoltsUnspecified,
        NULL,
        0,
        NULL));

    // now enable clock to sdio controller
    NV_CHECK_ERROR_CLEANUP(NvRmPowerModuleClockControl(
        hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance),
        hSdio->SdioRmPowerClientId,
        NV_TRUE));

    NvRmModuleReset(hSdio->hRm,
        NVRM_MODULE_ID(NvRmModuleID_Sdio, hSdio->Instance));

    // enable internal clock to sdio
    IsClkStable = SdioEnableInternalClock(hSdio);
    if (IsClkStable == NV_FALSE)
    {
        goto fail;
    }

    // Enable SDMMC_CLK bit in VENDOR_CLOCK_CONTROL register
    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL);
    val = NV_FLD_SET_DRF_DEF(SDMMC, VENDOR_CLOCK_CNTRL,
                                SDMMC_CLK, ENABLE, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL, val);

    // Restore current voltage setting
    NvDdkSdioSetSDBusVoltage(hSdio, hSdio->BusVoltage);

    // enable sd bus power
    NV_CHECK_ERROR_CLEANUP(SdioEnableBusPower(hSdio));

    // Initialize the block size again
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetBlocksize(hSdio, SDMMC_DEFAULT_BLOCK_SIZE));

    // set data timeout counter value
    NV_CHECK_ERROR_CLEANUP(SdioSetDataTimeout(hSdio, SdioDataTimeout_COUNTER_128M));


    ConfigureInterrupts(hSdio, SDIO_INTERRUPTS, ~SDIO_INTERRUPTS, 0);

    // Set the previous bus width
    NV_ASSERT_SUCCESS(NvDdkSdioSetHostBusWidth(hSdio, hSdio->BusWidth));

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetClockFrequency(
        hSdio,
        hSdio->ConfiguredFrequency,
        &pConfiguredFrequencyKHz));

    // Enable card clock
    NV_CHECK_ERROR_CLEANUP(SdioConfigureCardClock(hSdio, NV_TRUE));

    hSdio->ISControllerSuspended = NV_FALSE;
    return NvSuccess;
fail:
    NV_DEBUG_PRINTF(("resume failed [%x]\n", e));
    NV_ASSERT(!"Sdio DDK Resume Failed");

    return e;
}

