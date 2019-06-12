/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: SDIO module test</b>
 *
 * @b Description: Contains the imlementation of the test cases for the Sdio .
 */

#include "testsources.h"
#include "nvrm_hardware_access.h"
#include "nvbdk_pcb_api.h"
#include "nvddk_sdio.h"
#include "nvos.h"
#include "nvtest.h"
#include "nvrm_memmgr.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"
#include "nvbdk_pcb_interface.h"

#define __BDK_WIFI_DEBUG_
#ifdef __BDK_WIFI_DEBUG_
#define WIFI_DEBUG_PRINT(f,a...) NvOsDebugPrintf(f, ##a)
#else
#define WIFI_DEBUG_PRINT(f,a...) do {;} while (0)
#endif

/* card is not busy (sd mem card) */
#define SD_OCR_BUSY_CHK 0x80000000
#define SDHOST_SET_IO_ARGUMENT(dir,func,raw,addr,data) \
                        (((NvU32)dir << 31) | \
                         ((NvU32)func << 28) | \
                         ((NvU32)raw << 27) | \
                         ((NvU32)addr << 9) | (NvU32)data)
#define SDHOST_SET_IO_EXT_ARGUMENT(dir, func, mode, opcode, addr, count) \
        ((dir << 31) | (func << 28) |  (mode << 27) | \
         (opcode << 26) | (addr << 9) | count)

#define RETRY_CNT_FOR_OCR_CHECK 5

enum { SDIO_TEST_BLOCK_SIZE = 8 };
enum
{
    SDIO_COMMAND_DELAY = 10000
};

enum
{
    IO_CIS_POINTER_SIZE = 3,
    IO_CSA_POINTER_SIZE = 3
};

/* width */
enum
{
    IO_WIDTH_1 = 0x00,
    IO_WIDTH_4 = 0x02
};

enum
{
    IO_READ = 0x00,
    IO_WRITE = 0x01,
    IO_RAW = 0x01,
    IO_NO_RAW = 0x00,
    IO_ENABLE = 0x2,
    IO_READY = 0x03,
    IO_BUS_INTERFACE_CONTROL = 0x07,
    IO_CARD_CAPABILITY = 0x08,
    IO_BUS_SUSPEND = 0x0C,
    IO_FUNC_SELECT = 0xD
};
/* fbr register addresses */
enum
{
    IO_FBR_BASIC = 0x00,
    IO_FBR_CIS = 0x09,
    IO_FBR_CSA = 0x0C,
    IO_FBR_BLOCK_SIZE = 0x10
};

enum
{
    IO_INTR_ENABLE = 0x04,
    IO_PENDING = 0x05
};

enum
{
    FICODE_NONE = 0x0,
    FICODE_SDIO_UART,
    FICODE_SDIO_BT_TYPEA,    // Bluetooth Type-A
    FICODE_SDIO_BT_TYPEB,    // Bluetooth Type-B
    FICODE_SDIO_GPS,
    FICODE_SDIO_CAMERA,
    FICODE_SDIO_PHS,
    FICODE_SDIO_WLAN,
    FICODE_SDIO_ATA,    // Embedded SDIO-ATA
    FICODE_SDIO_BT_TYPEA_AMP,    // Alternate MAC PHY
    FICODE_SDIO_STD = 0xF,
    FICODE_MAX = 0x10
};

enum
{
    IO_IENM = 0x1,
    IO_IE_FUNCTION1 = 0x2
}  ;

enum
{
    IO_FUNCTION1_INTERRUPT_STATUS = 0x2
};

enum
{
    IO_BYTE_MODE = 0,
    IO_BLOCK_MODE = 0x1
};

enum { IO_FUNCTION_SIZE_BYTES = 256 };

enum
{
    CISTPL_NULL,
    CISTPL_MANFID = 0x20,
    CISTPL_FUNCID = 0x21,
    CISTPL_END = 0xFF,
};

enum
{
    SDIO_ALIGNMENT_SIZE = 512,

    SDIO_COMMAND_TIMEOUT = 1000,

    MAX_RESPONSE_SIZE = 5,
    SDHOST_CARD_STATUS_ACMD_RDY = 0x20,

    /* 2.7 to 3.3 volt */
    SDHOST_CMD8_CHK_PATTERN = 0x000001AA,

    /* 3.2 to 3.3 volt and no support for 2.0 spec for mem card */
    SDHOST_CMD41_PATTERN = 0x00100000,

    /* check for voltage range compatibility for mem card */
    SDHOST_OCR_CHECK = 0x00100000,    // bit20: 3.2-3.3V

    /* supports 2.0 spec for mem card */
    SDHOST_OCR_CCS = 0x40000000,

    SDIO_UART_BLOCK_SIZE = 192
};

typedef enum
{
    SDIO_CARD_TYPE_INCOMPATIBLE,
    SDIO_CARD_TYPE_UNKNOWN_NEW_CARD,
    SDIO_CARD_TYPE_UNKNOWN_OLD_CARD,
    SDIO_CARD_TYPE_SD_IO,
    SDIO_CARD_TYPE_SD_IO_COMBO_OLD_CARD,
    SDIO_CARD_TYPE_SD_MEM,
    SDIO_CARD_TYPE_SD_MEM_OLD_CARD,
    SDIO_CARD_TYPE_SD_MEM_VERSION_2_0,
    SDIO_CARD_TYPE_SD_MEM_HC_VERSION_2_0,
    SDIO_CARD_TYPE_SD_IO_COMBO_VERSION_2_0,
    SDIO_CARD_TYPE_SD_IO_COMBO_HC_VERSION_2_0,
    SDIO_CARD_TYPE_Force32 = 0x7FFFFFFF
} SdioCardType;

enum
{
    SDIO_CMD0 = 0,
    SDIO_CMD1 = 1,
    SDIO_CMD2 = 2,
    SDIO_CMD3 = 3,
    SDIO_CMD5 = 5,
    SDIO_CMD6 = 6,
    SDIO_CMD7 = 7,
    SDIO_CMD8 = 8,
    SDIO_CMD9 = 9,
    SDIO_CMD13 = 13,
    SDIO_CMD16 = 16,
    SDIO_CMD17 = 17,
    SDIO_CMD18 = 18,
    SDIO_CMD24 = 24,
    SDIO_CMD25 = 25,
    SDIO_CMD41 = 41,
    SDIO_CMD52 = 52,
    SDIO_CMD53 = 53,
    SDIO_CMD55 = 55
};

const char *FICodeStr[FICODE_MAX] = {
    "NONE",
    "SDIO UART",
    "SDIO BT Type-A",
    "SDIO BT Type-B",
    "SDIO GPS",
    "SDIO Camera",
    "SDIO PHS",
    "SDIO WLAN",
    "SDIO-ATA",
    "SDIO BT Type-A AMP",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "SDIO STD"
};

typedef struct SdioFuncInfoRec
{
    NvU32 IOdevCode;
    NvU32 CisAddress;
    NvU32 CsaAddress;
} SdioFuncInfo;

typedef struct SdioDeviceInfoRec
{
    NvU32 rca;
    NvU32 NumOfFuncs;
    NvU32 Capability;
    NvU32 MemoryPresent;
    SdioFuncInfo func[IO_FUNCTION_MAX];
} SdioDeviceInfo;

static NvRmDeviceHandle s_hRmDevice = NULL;
static NvDdkSdioDeviceHandle s_hSdio = NULL;

static NvDdkSdioCommand *s_pCmd = NULL;
static SdioDeviceInfo s_SdioDevInfo;
static NvU32 Response[MAX_RESPONSE_SIZE];

static NvError SdioWiFiRead(NvU32 FuncNum, NvU32 RegAddr, NvU32* data);
static NvError SdioWiFiWrite(NvU32 FuncNum, NvU32 RegAddr, NvU32 data, NvU32 raw);
static void SdioWiFiEnableFunction(NvU32 FuncNum);
static void SdioWiFiSetBusWidth(NvU32 width);
static NvError SdioWiFiConfigureCardDetectResistor(NvBool IsCdResisorEnable);
static NvError SdioWiFiReadCapabilities(void);
static void SdioWiFiReadFunctionInfo(NvU32 FuncNum);
static NvError SdioWiFiSelectDevice(NvU32 rca);
static NvError SdioWiFiInitTest(NvU32 instance, NvU32 FuncNum);
static NvError SdioWiFiCisTest(NvU32 *VendorId, NvU32 *DeviceId);

static void ResetCard(void)
{
    NvError Error = NvError_BadParameter;
    NvU32 status = 0;

    // send CMD52 to reset SDIO
    s_pCmd->CommandCode = SDIO_CMD52;    // RW_DIRECT
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    s_pCmd->CmdArgument = 0x80000c08;    // WRITE
    s_pCmd->ResponseType = NvDdkSdioRespType_R5;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSendCommand(CMD%d) status 0x%08X\n",
            __func__, s_pCmd->CommandCode, Error);
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);

    if (status == NvDdkSdioError_CommandTimeout)
    {
        NvOsDebugPrintf("%s: Command timeout for CMD%d but skip the reset\n",
            __func__, s_pCmd->CommandCode);
    }
    Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioGetCommandResponse(CMD%d) status 0x%08X\n",
            __func__, s_pCmd->CommandCode, Error);
    }
    WIFI_DEBUG_PRINT("Card Reset Successful \n");
}

static NvError SdioWiFiRead(NvU32 FuncNum, NvU32 RegAddr, NvU32* data)
{
    NvError Error = NvSuccess;
    NvU32 status = 0;

    s_pCmd->CommandCode = SDIO_CMD52;    // RW DIRECT
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    s_pCmd->CmdArgument = SDHOST_SET_IO_ARGUMENT(IO_READ, FuncNum,\
                                            IO_NO_RAW, RegAddr, 0);
    s_pCmd->ResponseType = NvDdkSdioRespType_R5;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);

    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSendCommand status 0x%08X \n",
            __func__, Error);
        goto err;
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);
    if (status == NvDdkSdioError_CommandTimeout)
    {
        NvOsDebugPrintf("%s: Command timeout error \n", __func__);
    }

    Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioGetCommandResponse status 0x%08X\n",
            __func__, Error);
        goto err;
    }
    *data = Response[0] & 0xFF;
    err:
    return Error;
}

static NvError
SdioWiFiWrite(
    NvU32 FuncNum,
    NvU32 RegAddr,
    NvU32 data,
    NvU32 raw)
{
    NvError Error;
    NvU32 status = 0;

    s_pCmd->CommandCode = SDIO_CMD52;    // RW DIRECT
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    s_pCmd->CmdArgument = SDHOST_SET_IO_ARGUMENT(IO_WRITE, FuncNum,
                                            raw, RegAddr, data);
    s_pCmd->ResponseType = NvDdkSdioRespType_R5;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);

    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSendCommand status 0x%08X\n",
            __func__, Error);
        goto err;
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);
    if (status == NvDdkSdioError_CommandTimeout)
    {
        NvOsDebugPrintf("%s: Command timeout error \n", __func__);
    }

    Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioGetCommandResponse status 0x%08X\n",
            __func__, Error);
    }

    /* if it is RAW mode check
       whether the data read after write is same as the data written */
    if (raw == IO_RAW)
    {
        if ((Response[0] & 0xFF) != (data & 0xFF))
        {
            return NvError_BadParameter;
        }
    }
    err:
    return Error;
}

static void SdioWiFiEnableFunction(NvU32 FuncNum)
{
    NvU32 data = 0;

    SdioWiFiRead(IO_FUNCTION0, IO_ENABLE, &data);
    SdioWiFiWrite(IO_FUNCTION0, IO_ENABLE, (data | (1 << FuncNum)), IO_RAW);
    while (1)
    {
        SdioWiFiRead(IO_FUNCTION0, IO_READY, &data);
        if (data & (1 << FuncNum))
        {
            break;
        }
    }
}

static void SdioWiFiSetBusWidth(NvU32 width)
{
    NvU32 data = 0;

    SdioWiFiRead(IO_FUNCTION0, IO_BUS_INTERFACE_CONTROL, &data);
    // clear the bus width
    data &= ~0x3;
    data |= width;
    SdioWiFiWrite(IO_FUNCTION0, IO_BUS_INTERFACE_CONTROL, data, IO_RAW);

    if (IO_WIDTH_4 == width)
        NvDdkSdioSetHostBusWidth( s_hSdio, NvDdkSdioDataWidth_4Bit);
    else
        NvDdkSdioSetHostBusWidth( s_hSdio, NvDdkSdioDataWidth_1Bit);
}

static NvError SdioWiFiConfigureCardDetectResistor(NvBool IsCdResisorEnable)
{
    NvError Error;
    NvU32 data = 0;

    SdioWiFiRead(IO_FUNCTION0, IO_BUS_INTERFACE_CONTROL, &data);

    if (IsCdResisorEnable == NV_FALSE)
    {
        data |= 0x80;
    }
    else
    {
        data &= ~(0x80);
    }

    Error = SdioWiFiWrite(IO_FUNCTION0, IO_BUS_INTERFACE_CONTROL, data, IO_RAW);

    return Error;
}

static NvError SdioWiFiReadCapabilities(void)
{
    NvError Error;
    NvU32 Capability = 0;

    Error = SdioWiFiRead(IO_FUNCTION0, IO_CARD_CAPABILITY, &Capability);
    s_SdioDevInfo.Capability = Capability;

    return Error;
}

static void SdioWiFiReadFunctionInfo(NvU32 FuncNum)
{
    NvError Error;
    NvU32 i;
    NvU32 status = 0;

    s_pCmd->CommandCode = SDIO_CMD52;    // RW DIRECT
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    s_pCmd->CmdArgument = SDHOST_SET_IO_ARGUMENT (IO_READ, IO_FUNCTION0,
            IO_NO_RAW, ((IO_FUNCTION_SIZE_BYTES * FuncNum) + IO_FBR_BASIC), 0);
    s_pCmd->ResponseType = NvDdkSdioRespType_R5;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);

    if (Error)
    {
        NvOsDebugPrintf("%s: FBR_BASIC:NvDdkSdioSendCommand status 0x%08X\n",
            __func__, Error);
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);
    if (status == NvDdkSdioError_CommandTimeout)
    {
        NvOsDebugPrintf("%s: FBR_BASIC:Command timeout error for CMD[%d]\n",
            __func__, s_pCmd->CommandCode);
    }

    Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
    if (Error)
    {
        NvOsDebugPrintf("%s: FBR_BASIC:NvDdkSdioGetCommandResponse "
                        "status 0x%08X\n",
                        __func__, Error);
    }
    else {
        s_SdioDevInfo.func[FuncNum].IOdevCode = Response[0] & 0xF;
        WIFI_DEBUG_PRINT("%s: FuncNum:%d (0x%02x) %s\n",
                         __func__,
                         FuncNum,
                         s_SdioDevInfo.func[FuncNum].IOdevCode,
                         FICodeStr[s_SdioDevInfo.func[FuncNum].IOdevCode]);
    }


    s_SdioDevInfo.func[FuncNum].CisAddress = 0;
    // Read the CIS address
    for (i = 0; i < IO_CIS_POINTER_SIZE; i++)
    {
        s_pCmd->CommandCode = SDIO_CMD52;    // RW DIRECT
        s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
        s_pCmd->IsDataCommand = NV_FALSE;
        s_pCmd->CmdArgument = SDHOST_SET_IO_ARGUMENT (IO_READ, IO_FUNCTION0,
            IO_NO_RAW, ((IO_FUNCTION_SIZE_BYTES * FuncNum) + IO_FBR_CIS + i), 0);
        s_pCmd->ResponseType = NvDdkSdioRespType_R5;

        Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);
        if (Error)
        {
            NvOsDebugPrintf("%s: FBR_CIS:NvDdkSdioSendCommand status 0x%08X\n",
                __func__, Error);
        }
        NvOsWaitUS(SDIO_COMMAND_DELAY);
        if (status == NvDdkSdioError_CommandTimeout)
        {
            NvOsDebugPrintf("%s: FBR_CIS:Command timeout error for CMD[%d]\n",
                __func__, s_pCmd->CommandCode);
        }
        Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
        if (Error)
        {
            NvOsDebugPrintf("%s: FBR_CIS:NvDdkSdioGetCommandResponse "
                            "status 0x%08X \n",
                            __func__, Error);
        }
        s_SdioDevInfo.func[FuncNum].CisAddress |= ((Response[0] & 0xFF) << (i * 8));
    }

    // Read the CSA
    s_SdioDevInfo.func[FuncNum].CsaAddress = 0;
    for (i = 0; i < IO_CSA_POINTER_SIZE; i++)
    {
        s_pCmd->CommandCode = SDIO_CMD52;    // RW DIRECT
        s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
        s_pCmd->IsDataCommand = NV_FALSE;
        s_pCmd->CmdArgument = SDHOST_SET_IO_ARGUMENT (IO_READ, IO_FUNCTION0,
            IO_NO_RAW, ((IO_FUNCTION_SIZE_BYTES * FuncNum) + IO_FBR_CSA + i), 0);
        s_pCmd->ResponseType = NvDdkSdioRespType_R5;

        Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);
        if (Error)
        {
            NvOsDebugPrintf("%s: FBR_CSA:NvDdkSdioSendCommand status 0x%08X \n",
                __func__, Error);
        }
        NvOsWaitUS(SDIO_COMMAND_DELAY);
        if (status == NvDdkSdioError_CommandTimeout)
        {
             NvOsDebugPrintf("%s: FBR_CSA:Command timeout error for CMD[%d] \n",
                 __func__, s_pCmd->CommandCode);
        }
        Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
        if (Error)
        {
            NvOsDebugPrintf("%s: FBR_CSA:NvDdkSdioGetCommandResponse "
                            "status 0x%08X\n",
                            __func__, Error);
        }
        s_SdioDevInfo.func[FuncNum].CsaAddress |= ((Response[0] & 0xFF) <<
                                                   (i * 8));
    }
}

static NvError SdioWiFiSelectDevice(NvU32 rca)
{
    NvError Error = NvError_BadParameter;
    NvU32 status = 0;

    // send CMD 7 to select the card
    s_pCmd->CommandCode = SDIO_CMD7;    // SELECT CARD
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    // hence upper 16 bits will have new RCA and lower 16 bits stuffed
    s_pCmd->CmdArgument = ((rca) << 16);
    s_pCmd->ResponseType = NvDdkSdioRespType_R1;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);

    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSendCommand status 0x%08X\n",
            __func__, Error);
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);
    if (status == NvDdkSdioError_CommandTimeout)
    {
        NvOsDebugPrintf("%s: Command timeout error for CMD[%d]\n",
            __func__, s_pCmd->CommandCode);
    }

    Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioGetCommandResponse status 0x%08X\n",
            __func__, Error);
    }

    WIFI_DEBUG_PRINT("%s: Card selected with RCA = 0x%x \n", __func__, rca);
    return Error;
}

static NvError SdioWiFiInitTest(NvU32 instance, NvU32 FuncNum)
{
    NvError Error;
    NvU32 status = 0;
    NvU32 idx;
    NvRmFreqKHz CurrentFreq = 0;
    NvU32 SdioClk = 100;
    NvU32 retry = RETRY_CNT_FOR_OCR_CHECK;

    Error = NvDdkSdioOpen(
                s_hRmDevice,
                &s_hSdio,
                instance);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioOpen() status 0x%08X\n", __func__, Error);
        goto err;
    }

    s_pCmd = NvOsAlloc(sizeof(NvDdkSdioCommand));
    if (!s_pCmd)
    {
        NvOsDebugPrintf("%s: NvOsAlloc(s_pCmd) status 0x%08X\n",
                        __func__, Error);
        goto err;
    }

    Error = NvDdkSdioSetClockFrequency(s_hSdio, SdioClk, &CurrentFreq);
    WIFI_DEBUG_PRINT("%s: SdioCLK %d \n", __func__, SdioClk);

    ResetCard();

    s_pCmd->CommandCode = SDIO_CMD5;    // SEND OP COND
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    s_pCmd->CmdArgument = 0x0;
    s_pCmd->ResponseType = NvDdkSdioRespType_R4;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSendCommand(CMD5) status 0x%08X\n",
            __func__, Error);
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);

    if (status == NvDdkSdioError_CommandTimeout)
    {
        NvOsDebugPrintf("%s: Command timeout error for CMD[%d]\n",
            __func__, s_pCmd->CommandCode);
    }
    else
    {
        // check card version
        Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                    s_pCmd->CommandCode,
                                    s_pCmd->ResponseType,
                                    Response);
        if (Error)
        {
            NvOsDebugPrintf("%s: NvDdkSdioGetCommandResponse(CMD5) "
                            "status 0x%08X\n", __func__, Error);
        }
        else
        {
            WIFI_DEBUG_PRINT("%s: OCR: 0x%08x\n", __func__, Response[0]);
        }
    }

    retry = RETRY_CNT_FOR_OCR_CHECK;
    s_SdioDevInfo.MemoryPresent = (Response[0] & 0x8000000)? 1 : 0;
    if (s_SdioDevInfo.MemoryPresent)
        WIFI_DEBUG_PRINT("%s: This card contains SD memory\n", __func__);
    else
        WIFI_DEBUG_PRINT("%s: This card is I/O only\n", __func__);

    s_SdioDevInfo.NumOfFuncs = ((Response[0] >> 28) & 0x7);
    if (s_SdioDevInfo.NumOfFuncs > 0)
    {
        WIFI_DEBUG_PRINT("%s: # of Functions: %d\n", __func__,
                         s_SdioDevInfo.NumOfFuncs);
        // Test OCR
        // ocr = Response[0] & 0xFFFFFF;
    }

    while (retry--)
    {
        s_pCmd->CommandCode = SDIO_CMD5;    // SEND OP COND
        s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
        s_pCmd->IsDataCommand = NV_FALSE;
        s_pCmd->CmdArgument = SDHOST_OCR_CHECK;
        s_pCmd->ResponseType = NvDdkSdioRespType_R4;

        Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);
        if (Error)
        {
            NvOsDebugPrintf("%s: SetVolt:NvDdkSdioSendCommand(CMD5) "
                            "status 0x%08X \n",
                            __func__, Error);
        }
        NvOsWaitUS(SDIO_COMMAND_DELAY);

        if (status == NvDdkSdioError_CommandTimeout)
        {
            NvOsDebugPrintf("%s: SetVolt:Command timeout error for "
                            "CMD[%d] \n",
                            __func__, s_pCmd->CommandCode);
        }
        else
        {
            // check card version
            Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                    s_pCmd->CommandCode,
                                    s_pCmd->ResponseType,
                                    Response);
            if (Error)
            {
                 NvOsDebugPrintf("%s: SetVolt:Command timeout error for "
                                 "CMD[%d] \n", __func__, s_pCmd->CommandCode);
            }
        }
        if (Response[0] & SD_OCR_BUSY_CHK)
        {
            break;
        }
    }

    s_pCmd->CommandCode = SDIO_CMD3; // SEND RELATIVE ADDR
    s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
    s_pCmd->IsDataCommand = NV_FALSE;
    s_pCmd->CmdArgument = 0;
    s_pCmd->ResponseType = NvDdkSdioRespType_R6;

    Error = NvDdkSdioSendCommand(s_hSdio, s_pCmd, &status);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSendCommand(CMD3) "
                        "status 0x%08X \n",
                        __func__, Error);
    }
    NvOsWaitUS(SDIO_COMMAND_DELAY);
    if (status == NvDdkSdioError_CommandTimeout)
        NvOsDebugPrintf("%s: Command timeout error for "
                        "CMD[%d] \n",
                        __func__, s_pCmd->CommandCode);
    else
    {
        // check card version
        Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                    s_pCmd->CommandCode,
                                    s_pCmd->ResponseType,
                                    Response);
        if (Error)
        {
            NvOsDebugPrintf("%s: Command timeout error for "
                            "CMD[%d] \n",
                            __func__, s_pCmd->CommandCode);
        }
    }

    // if there is no error read the RCA value
    s_SdioDevInfo.rca = ((Response[0] >> 16) & 0xFFFF);
    if (s_SdioDevInfo.rca == 0)
        NvOsDebugPrintf("%s: L:%d, rca:%x\n",
                        __func__, __LINE__, s_SdioDevInfo.rca);
    Error = SdioWiFiSelectDevice(s_SdioDevInfo.rca);
    if (Error != NvSuccess)
        NvOsDebugPrintf("%s: L:%d, Err:%x\n", __func__, __LINE__, Error);
    Error = SdioWiFiConfigureCardDetectResistor(NV_FALSE);
    if (Error != NvSuccess)
        NvOsDebugPrintf("%s: L:%d, Err:%x\n", __func__, __LINE__, Error);
    Error = SdioWiFiReadCapabilities();
    if (Error != NvSuccess)
        NvOsDebugPrintf("%s: L:%d, Err:%x\n", __func__, __LINE__, Error);

    for (idx = 0; idx < s_SdioDevInfo.NumOfFuncs; idx++)
        SdioWiFiReadFunctionInfo(idx);

    SdioWiFiEnableFunction(FuncNum);
    SdioWiFiSetBusWidth(IO_WIDTH_4);

    return NvSuccess;

err:
    if (Error != NvSuccess)
        NvOsDebugPrintf("%s: L:%d, Err:%x\n", __func__, __LINE__, Error);
    return Error;
}

static NvError SdioWiFiCisTest(NvU32 *VendorId, NvU32 *DeviceId)
{
    NvU32 tpl_code = 0, tpl_link = 0, tpl_data = 0;
    NvU32 IsCISTPL_MANFIDFound = 0;
    NvU32 offset = 0, i;
    NvU8 data[256];

    do {
        SdioWiFiRead(IO_FUNCTION0,
                     s_SdioDevInfo.func[0].CisAddress + (offset++),
                     &tpl_code);

        if (tpl_code == CISTPL_END)
        {
            NvOsDebugPrintf("%s: CISTPL_END Founded (L:%d)\n",
                            __func__, __LINE__);
            break;
        }

        if (tpl_code == CISTPL_NULL)
            continue;

        SdioWiFiRead(IO_FUNCTION0,
                     s_SdioDevInfo.func[0].CisAddress + (offset++),
                     &tpl_link);

        if (tpl_link == CISTPL_END) {
            NvOsDebugPrintf("%s: CISTPL_END Founded (L:%d)\n",
                            __func__, __LINE__);
            break;
        }

        if (tpl_code == CISTPL_MANFID)
        {
            IsCISTPL_MANFIDFound = 1;
            if (tpl_link > 256)
                tpl_link = 256;
            for (i = 0; i < tpl_link; i++) {
                SdioWiFiRead(IO_FUNCTION0,
                             s_SdioDevInfo.func[0].CisAddress + offset + i,
                             &tpl_data);
                data[i] = (NvU8)tpl_data;
            }
            *VendorId = (data[1] << 8) | data[0];
            *DeviceId = (data[3] << 8) | data[2];
            WIFI_DEBUG_PRINT("%s: VendorId: 0x%04x, DeviceId: 0x%04x\n",
                             __func__, *VendorId, *DeviceId);
            break;
        }
        else
        {
            for (i = 0; i < tpl_link; i++)
            {
                SdioWiFiRead(IO_FUNCTION0,
                             s_SdioDevInfo.func[0].CisAddress + offset + i,
                             &tpl_data);
            }
        }

        offset += tpl_link;
    } while (1);

    if (!IsCISTPL_MANFIDFound)
        return NvError_TestDataVerificationFailed;

    return NvSuccess;
}

static NvError SdioWiFiByteModeReadWrite(NvU32 FuncNum, NvU32 TestAddr, NvU32 TestCount)
{
    NvError Error = NvError_BadParameter;
    NvU32 TransferSize = 128;
    NvU8* pSrcVirtBuffer = NULL;
    NvU8* pDstVirtBuffer = NULL;
    NvRmFreqKHz CurrentFreq = 0;
    NvU32 i = 0, j = 0;
    NvU32 status = 0, data_match = 1;
    NvU32 SdioClk = 25000;
    NvU32 successRate = 0, failureRate = 0;

    Error = NvDdkSdioSetClockFrequency(s_hSdio, SdioClk, &CurrentFreq);
    WIFI_DEBUG_PRINT("%s: SdioCLK %d \n", __func__, SdioClk);
    if (Error)
    {
        NvOsDebugPrintf("%s: NvDdkSdioSetClockFrequency status 0x%08X \n",
                        __func__, Error);
    }

    pSrcVirtBuffer = NvOsAlloc(TransferSize);
    if (!pSrcVirtBuffer)
    {
        goto err;
    }
    pDstVirtBuffer = NvOsAlloc(TransferSize);
    if (!pDstVirtBuffer)
    {
        goto err;
    }

    while(i != TestCount)
    {
       i++;
       TransferSize = 1;
       for (j = 0; j < TransferSize; j++)
          pSrcVirtBuffer[j] = (i + j);

       data_match = 1;

       // send CMD 53 to write single blk to the card
       s_pCmd->CommandCode = SDIO_CMD53;    // RW EXTENDED
       s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
       s_pCmd->IsDataCommand = NV_TRUE;
       s_pCmd->CmdArgument = SDHOST_SET_IO_EXT_ARGUMENT(IO_WRITE, FuncNum,
                IO_BYTE_MODE, 0, TestAddr, TransferSize);
       s_pCmd->ResponseType = NvDdkSdioRespType_R1;
       s_pCmd->BlockSize = TransferSize;

       Error = NvDdkSdioWrite(s_hSdio,
                              TransferSize,
                              pSrcVirtBuffer,
                              s_pCmd,
                              NV_FALSE,
                              &status);
       if (Error)
       {
          NvOsDebugPrintf("%s: WiFiWrite: error status reg: 0x%08X \n",
                          __func__, Error);
       }

       // check response
       Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                s_pCmd->CommandCode,
                                s_pCmd->ResponseType,
                                Response);
       if (Error)
       {
           NvOsDebugPrintf("%s: WiFiWrite: CommandResponse(CMD53) "
                           "status 0x%08X \n",
                           __func__, Error);
       }

       // now read back the same data into local buffer
       s_pCmd->CommandCode = SDIO_CMD53;    // RW EXTENDED
       s_pCmd->CommandType = NvDdkSdioCommandType_Normal;
       s_pCmd->IsDataCommand = NV_TRUE;
       s_pCmd->CmdArgument = SDHOST_SET_IO_EXT_ARGUMENT(IO_READ,
                                                        FuncNum,
                                                        IO_BYTE_MODE,
                                                        0,
                                                        TestAddr,
                                                        TransferSize);
       s_pCmd->ResponseType = NvDdkSdioRespType_R1;
       s_pCmd->BlockSize = TransferSize;

       Error = NvDdkSdioRead(s_hSdio,
                             TransferSize,
                             pDstVirtBuffer,
                             s_pCmd,
                             NV_FALSE,
                             &status);
       if (Error)
       {
           NvOsDebugPrintf("%s: WiFiRead : error status 0x%08X \n",
                           __func__, Error);
       }

       // check response
       Error = NvDdkSdioGetCommandResponse(s_hSdio,
                                           s_pCmd->CommandCode,
                                           s_pCmd->ResponseType,
                                           Response);
       if (Error)
       {
           NvOsDebugPrintf("%s: WiFiRead : CommandResponse (CMD53) "
                           "status 0x%08X \n",
                           __func__, Error);
       }

       for (j = 0; j < TransferSize; j++)
       {
           if (pDstVirtBuffer[j] != pSrcVirtBuffer[j])
           {
               NvOsDebugPrintf("%s: Data Mismatch - WR/RD: %02X/%02X\n",
                               __func__, pSrcVirtBuffer[j], pDstVirtBuffer[j]);
               data_match = 0;
               break;
           }
       }

       if (data_match)
           successRate++;
       else
           failureRate++;

       if ((i % 100) == 0)
            NvOsDebugPrintf("%s: Iter:  %d, Sucesses: %d, Failures: %d\n",
                            __func__, i, successRate, failureRate);
    }

    WIFI_DEBUG_PRINT("%s: Total: %d, Sucesses: %d, Failures: %d\n",
                     __func__, TestCount, successRate, failureRate);

    NvOsFree(pSrcVirtBuffer);
    pSrcVirtBuffer = NULL;
    NvOsFree(pDstVirtBuffer);
    pDstVirtBuffer = NULL;

    return NvSuccess;

err:

    NvOsFree(pSrcVirtBuffer);
    pSrcVirtBuffer = NULL;
    NvOsFree(pDstVirtBuffer);
    pDstVirtBuffer = NULL;

    return Error;
}

static NvError NvApiWifiCheck(WifiTestPrivateData *pWifiData, char *testName)
{
    NvBool b;
    NvError e = NvError_TestDataVerificationFailed;
    NvU32 VendorId = 0, DeviceId = 0;

    NvOsMemset(&s_SdioDevInfo, 0, sizeof(s_SdioDevInfo));

    // Open the Rm device handle.
    e = NvRmOpenNew(&s_hRmDevice);
    if (NvSuccess != e)
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, testName, "pcb", "NvRmOpenNew FAILED");

    WIFI_DEBUG_PRINT("NvBDK WIFI - Tests\n");
    WIFI_DEBUG_PRINT("------------------------\n");

    WIFI_DEBUG_PRINT("NvBDK WIFI test: WiFiInitTest (Instance:%d)\n",
                     pWifiData->instance);

    e = SdioWiFiInitTest(pWifiData->instance, pWifiData->testFuncId);
    if (NvSuccess != e)
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, testName, "pcb", "WiFi Init Test Failed");

    WIFI_DEBUG_PRINT("NvBDK WIFI test: WiFiCisTest (Instance:%d)\n",
                     pWifiData->instance);
    e = SdioWiFiCisTest(&VendorId, &DeviceId);
    if (NvSuccess != e)
    {
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, testName, "pcb", "WiFi Cis Test Failed");
    }
    else
    {
        pWifiData->vendorId    = VendorId;
        pWifiData->deviceId = DeviceId;
    }

    WIFI_DEBUG_PRINT("NvBDK WIFI test: WiFiByteModeReadWriteTest "
                     "(Instance:%d)\n",
                     pWifiData->instance);

    e = SdioWiFiByteModeReadWrite(pWifiData->testFuncId,
                                  pWifiData->testAddr,
                                  pWifiData->testCount);
    if (NvSuccess != e)
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, testName, "pcb", "WiFi ByteAccess Test Failed");

fail:
    // cleaup stuff
    if (s_hSdio)
    {
        NvDdkSdioClose(s_hSdio);
        s_hSdio = NULL;
    }
    if (s_hRmDevice)
    {
        NvRmClose(s_hRmDevice);
        s_hRmDevice = NULL;
    }
    if (s_pCmd)
    {
        NvOsFree(s_pCmd);
        s_pCmd = NULL;
    }

    return e;
}

void NvPcbWifiTest(NvBDKTest_TestPrivData param)
{
    PcbTestData *pPcbData = (PcbTestData *)param.pData;
    WifiTestPrivateData *pWifiData = (WifiTestPrivateData *)pPcbData->privateData;
    NvBool b;
    NvError e = NvSuccess;

    if (pWifiData->pInit)
    {
        if (pWifiData->pInit(NULL) != NvSuccess)
            TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", "pInit Failed");
    }

    e = NvApiWifiCheck(pWifiData, pPcbData->name);
    if (e != NvSuccess)
        NvOsDebugPrintf("%s failed : NvError 0x%x\n", __func__, e);
    else if ((pWifiData->vendorId != pWifiData->verifyVendorId)
             || (pWifiData->deviceId !=  pWifiData->verifyDeviceId))
        NvOsDebugPrintf( "[%s:%d] <%s> %s\n", __FUNCTION__, __LINE__,
                         pPcbData->name, "WiFi Test Failed");

    if (pWifiData->pDeinit)
        pWifiData->pDeinit(NULL);

fail:
    return;
}
