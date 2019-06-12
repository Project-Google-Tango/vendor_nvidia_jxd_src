/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "aos.h"
#include "nvaboot.h"
#include "fastboot.h"
#include "nvappmain.h"
#include "nv3p_transport.h"
#include "prettyprint.h"
#include "nvutil.h"
#include "nvbu.h"
#include "nvboot_bit.h"
#include "nvboot_bct.h"
#include "nvddk_blockdevmgr.h"
#include "nvrm_power_private.h"
#include "nvbct.h"
#include "nvbl_memmap_nvap.h"
#include "nvbl_query.h"
#include "crc32.h"
#include "nvrm_memmgr.h"

#include "nvodm_query.h"
#include "nvodm_pmu.h"
#include "nvodm_query_discovery.h"
#include "nvddk_keyboard.h"
#include "nvodm_services.h"

#include "recovery_utils.h"
#include "nv_rsa.h"
#include "nvfs_defs.h"
#include "nvstormgr.h"
#include "nvddk_fuse.h"
#include "nvstormgr.h"
#include "t30/arapbpm.h"
#include "libfdt/libfdt.h"
#ifdef NV_USE_CRASH_COUNTER
#include "crash_counter.h"
#endif
#ifdef HAVE_LED
#include "tegra_led.h"
#endif

#include "nvprofiling.h"
#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
#include "nvaboot_tf.h"
#endif
#if TSEC_EXISTS
#include "tsec_otf_keygen.h"
#endif
#ifdef NV_USE_NCT
#include "nct.h"
#endif

#ifndef BUILD_NUMBER
#define BUILD_NUMBER "UNDEF_BUILD"
#endif

#define SHOW_IMAGE_MENU 0
#define NVIDIA_GREEN 0x7fb900UL
#define DO_JINGLE 0
#define JINGLE_CHANNELS 1
#define JINGLE_BITSPERSAMPLE 16
#define JINGLE_SAMPLERATE 44100

//  ATAG values defined in linux kernel's arm/include/asm/setup.h file
#define ATAG_CORE         0x54410001
#define ATAG_REVISION     0x54410007
#define ATAG_INITRD2      0x54420005
#define ATAG_CMDLINE      0x54410009
#define ATAG_SERIAL       0x54410006

#define BITMAP_PARTITON "BMP"
#define LOWBAT_BITMAP_PARTITION "LBP"
#define BAT_CHARGING_PARTITION "CHG"
#define BAT_CHARGED_PARTITION "FBP"
#define BAT_FULLY_CHARGED_PARTITION "FCG"

#if TSEC_EXISTS
#define ATAG_NVIDIA_OTF   0x41000802
#endif
#define FDT_SIZE_BL_DT_NODES (2048)

#define LOKI_NFF_BOARDID 0x9F4
#define SHIELD_ERS_BOARDID  0x6F4
#define TN8_FFD_BOARDID  0x6E1
#define PMU_E1735 0x6C7
#define LOKI_THOR195_BOARDID 0x9F5
#define LOKI_FFD_BOARDID 0x9E2

#define MACHINE_TYPE_TEGRA_GENERIC      3333
#define MACHINE_TYPE_DALMORE            4304
#define MACHINE_TYPE_PLUTO              4306
#define MACHINE_TYPE_P1852              3651
#define MACHINE_TYPE_TEGRA_P852         3667
#define MACHINE_TYPE_TEGRA_FDT          0xFFFFFFFF
#define MACHINE_TYPE_E1853              4241
#define MACHINE_TYPE_M2601              4334
#define MACHINE_TYPE_BONAIRE              3438
#define MACHINE_TYPE_ARDBEG              4602
#define MACHINE_TYPE_LOKI                4715


#define APBDEV_PMC_SCRATCH0_0_FASTBOOT_FLAG_BIT 30
#define APBDEV_PMC_SCRATCH0_0_RCK_FLAG_BIT 31
#define APBDEV_PMC_SCRATCH0_0_FORCE_RECOVERY_FLAG_BIT 1
#define USB_BUF_ALIGNMENT_SIZE 4096

#define RST_STATUS_SW_MAIN 3
#define DISPLAY_TIMEOUT_UNITMS 3000

#define ALIGNMENT_SIZE     16384

#define MAX_SELECT_HOLD_TIME 3000000

extern Allocator s_RemainAllocator;
extern NvBool s_FpgaEmulation;
extern BootloaderProfiling BootLoader;

static NvBool InitDispDone = NV_FALSE;
static NvRmDeviceHandle s_hRm = NULL;
static NvU32 s_Chipreset = 0;
static NvU8 PowerOnSource = NVOdmPmuUndefined;
static NvBool BootToCharging = NV_FALSE;
static NvU8 RebootSource = NVOdmRebootUndefined;
NvU32 Reason = 1;

static void DisplayBoardInfo(void);

/* Adding the below lines to make sure code builds for
 * ldk as the flag has only been defined for Android.
 */
struct fdt_header *fdt_hdr = NULL;

static NvU32 NvDumperReserved;

typedef struct NvDdkKbdContextRec {
    NvU32 ReportedState;
    NvU32 HoldKeyPressed;
} NvDdkKbdContextRec;

static NvDdkKbdContextRec s_DdkKbcCntxt = {KEY_IGNORE, 0};

struct machine_map
{
    const char* name;
    NvU32       id;
};

static const struct machine_map machine_types[] =
{
    {"Dalmore",  MACHINE_TYPE_DALMORE},
    {"Pluto",  MACHINE_TYPE_PLUTO},
    {"P1852",  MACHINE_TYPE_P1852},
    {"P852", MACHINE_TYPE_TEGRA_P852},
    {"E1853",  MACHINE_TYPE_E1853},
    {"M2601",  MACHINE_TYPE_M2601},
    {"Bonaire",  MACHINE_TYPE_BONAIRE},
    {"Ardbeg",  MACHINE_TYPE_ARDBEG},
    {"TN8-FFD",  MACHINE_TYPE_ARDBEG},
    {"Loki",  MACHINE_TYPE_LOKI},
};

#define TEMP_BUFFER_SIZE (1*1024*1024)

#define BOOTLOADER_NAME "bootloader"
#define MICROBOOT_NAME "microboot"
#define BCT_NAME        "bcttable"

#define FB_CHECK_ERROR_CLEANUP(expr) \
    do \
    { \
        e = (expr); \
        if (e == NvError_Timeout) \
            continue; \
        if (e != NvSuccess) \
            goto fail; \
    } while (0);

static const char *BootloaderMenuString[]=
{
    "Continue\n",
    "Fastboot Protocol\n",
    "Recovery mode\n",
    "Reboot\n",
    "Poweroff\n",
    "Forced Recovery\n",
};

#if SHOW_IMAGE_MENU
#include "android.h"
#include "usb.h"
static Icon *s_RadioImages[] =
{
    &s_android,
    &s_usb
};
#endif
enum
{
    CONTINUE,
    REBOOT_BOOTLOADER,
    RECOVERY_KERNEL,
    REBOOT,
    POWEROFF,
    FORCED_RECOVERY,
    BOOT,
    FASTBOOT
};

const PartitionNameMap gs_PartitionTable[] =
{
    { "recovery", RECOVERY_KERNEL_PARTITION, FileSystem_Basic},
    { "boot", KERNEL_PARTITON, FileSystem_Basic},
    { "zimage", KERNEL_PARTITON, FileSystem_Basic},
    { "ramdisk", KERNEL_PARTITON, FileSystem_Basic},
    { "dtb", "DTB", FileSystem_Basic},
    { "ubuntu", "UBN", FileSystem_Basic},
    { "system", "APP", FileSystem_Ext4},
    { "cache", "CAC", FileSystem_Ext4},
    { "misc", "MSC", FileSystem_Basic},
    { "staging", "USP", FileSystem_Basic},
    { "userdata", "UDA", FileSystem_Ext4},
    { BCT_NAME, "BCT", FileSystem_Basic},
    { BOOTLOADER_NAME, "EBT", FileSystem_Basic},
    { MICROBOOT_NAME, "NVC", FileSystem_Basic},
#if defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    { "tos", "TOS", FileSystem_Basic }
#endif
};

enum
{
    PLATFORM_NAME_INDEX = 0,
    MSG_FASTBOOT_MENU_SEL_INDEX,
    MSG_NO_KEY_DRIVER_INDEX
};

static const char *s_DalmoreBLMsg[] =
{
    "Dalmore\0",
    "Press <Home> to select, <Volume Up/Down> for selection move\n",
    "Key driver not found.. Booting OS\n"
};

static const char *s_PlutoBLMsg[] =
{
    "Pluto\0",
    "Press <Menu> to select, <Home, Back> for selection move\n",
    "Key driver not found.. Booting OS\n"
};

static const char *s_BonaireBLMsg[] =
{
    "Bonaire\0",
    "Press <Home> to select, <Volume Up/Down> for selection move\n",
    "Key driver not found.. Booting OS\n"
};

static const char *s_ArdbegBLMsg[] =
{
    "Ardbeg\0",
    "Press <Home> to select, <Volume Up/Down> for selection move\n",
    "Key driver not found.. Booting OS\n"
};

static const char *s_TN8FFDBLMsg[] =
{
    "TN8-FFD\0",
    "Press <Power> to select, <Volume Up/Down> for selection move\n",
    "Key driver not found.. Booting OS\n"
};

static const char *s_LokiBLMsg[] =
{
    "Loki\0",
    "Press <SHIELD> to move selection down, hold <SHIELD> for 3 seconds to select \n",
    "Key driver not found.. Booting OS\n"
};

static const char *s_DefaultBLMsg[] =
{
    "Unknown\0"
    "Use scroll wheel or keyboard for movement and selection\n",
    "Neither Scroll Wheel nor Keyboard are detected ...Booting OS\n",
};

static const char **s_PlatformBlMsg = NULL;

NvAbootHandle gs_hAboot = NULL;
static NvBool s_IsKeyboardInitialised;
static NvU8 s_KeyFlag;
NvU32 g_wb0_address;

extern PrettyPrintContext s_PrintCtx;

static NvRmSurface s_Framebuffer;
static NvRmSurface s_Frontbuffer;
static void       *s_FrontbufferPixels = NULL;
static NvDdkBlockDevMgrDeviceId s_SecondaryBoot = 0;
extern NvU32        bl_nDispController;
static NvBool s_RecoveryKernel = NV_FALSE;

#ifdef LPM_BATTERY_CHARGING
#include "nvodm_charging.h"
#include "nvaboot_usbf.h"

static NvOdmChargingConfigData s_ChargingConfigData;
static char s_DispMessage[255];
static NvOdmPmuDeviceHandle s_pPmu;

//lpm header declarations
static void NvAppNotifyOnDisplay(NvOdmDisplayImage Image);
static NvError NvAppChargeBatteryIfLow(void);

static NvOsSemaphoreHandle s_hKeySema;

static void KeyReleaseIsr(void)
{
    NvOsSemaphoreSignal(s_hKeySema);
}

#endif

#if TSEC_EXISTS
NvOtfEncryptedKey g_OtfKey;
#endif

static void NvAppFinalize(void);
#ifdef LP0_SUPPORTED
static void NvAppSuspendInit(void);
#endif
extern void NvBlLp0CoreResume(void);

static NvError LoadKernel(NvU32 , const char *,NvU8 *, NvU32 );
static NvError StartFastbootProtocol(NvU32);

static NvBool HasValidKernelImage(NvU8**, NvU32 *, const char *);
static const PartitionNameMap *MapFastbootToNvPartition(const char*);
static NvError BootAndroid(
    void *KernelImage,
    NvU32 LoadSize,
    NvU32 RamBase);

static NvError SetupTags(
    const AndroidBootImg *hdr,
    NvUPtr RamdiskPa);
#if SHOW_IMAGE_MENU
static int BootloaderMenu(RadioMenu *Menu, NvU32 Timeout);
#endif
static NvError MainBootloaderMenu(NvU32 RamBase, NvU32 TimeOut);
static void ShowPlatformBlMsg(void);
static NvU32 BootloaderTextMenu(
    TextMenu *Menu,
    NvU32 Timeout);

static const char *ErrorName(NvError err)
{
    switch(err)
    {
        #define NVERROR(name, value, str) case NvError_##name: return #name;
        #include "nverrval.h"
        #undef NVERROR
        default:
            break;
    }
    return "Unknown error code";
}

#if DO_JINGLE
static NvU32 TwoHundredHzSquareWave(NvU8* Buff, NvU32 MaxSize)
{
    static NvU32 Remain = 0;
    static NvU32 Toggle = 0;
    const NvU32 OnSamples = JINGLE_SAMPLERATE / 200;
    const NvU32 OffSamples = (JINGLE_SAMPLERATE+199) / 200;
    NvU32 NumSamples = MaxSize / ((JINGLE_BITSPERSAMPLE/8) * JINGLE_CHANNELS);
    NvU32 i;

    #if JINGLE_BITSPERSAMPLE==16
    NvU16* Buff16 = (NvU16*)Buff;
    #elif JINGLE_BITSPERSAMPLE==32
    NvU32* Buff32 = (NvU32*)Buff;
    #endif

    if (!Remain)
        Remain = OnSamples;

    for (i=0; i < NumSamples; i += JINGLE_CHANNELS)
    {
        NvU32 j;
        for (j=0; j<JINGLE_CHANNELS; j++)
        {
            #if JINGLE_BITSPERSAMPLE==16
            Buff16[i+j] = (Toggle) ? 0x7fff : 0x8000;
            #elif JINGLE_BITSPERSAMPLE==32
            Buff32[i+j] = (Toggle) ? 0x7ffffffful : 0x80000000ul;
            #endif
        }

        Remain--;
        if (!Remain)
        {
            Toggle ^= 1;
            Remain = (Toggle) ? OffSamples : OnSamples;
        }
    }
    return MaxSize;
}
#endif

static void InitBlMessage(void)
{
    NvU32 i;
    const NvU8 *OdmPlatformName = NvOdmQueryPlatform();
    const char **BlMsgs[] =
    {
        s_DalmoreBLMsg,
        s_PlutoBLMsg,
        s_BonaireBLMsg,
        s_ArdbegBLMsg,
        s_TN8FFDBLMsg,
        s_LokiBLMsg,
        NULL
    };

    if (!OdmPlatformName)
    {
         s_PlatformBlMsg = s_DefaultBLMsg;
         return;
    }

    for (i =0; BlMsgs[i] != NULL; ++i)
    {
         if (!NvOsMemcmp(OdmPlatformName, BlMsgs[i][PLATFORM_NAME_INDEX],
                                            sizeof(OdmPlatformName)))
         {
              s_PlatformBlMsg = BlMsgs[i];
              return;
         }
    }
    s_PlatformBlMsg = s_DefaultBLMsg;
}

static void ClearDisplay(void)
{
    NvU32 Size = NvRmSurfaceComputeSize(&s_Frontbuffer);
    NvOsMemset(s_FrontbufferPixels, 0, Size);
}

static NvBool IsPartitionExposed(const char *PartitionName)
{
    NvU32 PartitionIdGPT;
    NvU32 PartitionId;
    NvU32 Id;
    NvError e;
    NvBool RetVal = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName("GP1", &Id));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName(PartitionName, &PartitionId));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName("GPT", &PartitionIdGPT));

    Id = NvPartMgrGetNextId(Id);
    while (Id != 0 && Id != PartitionIdGPT)
    {
        if (Id == PartitionId)
        {
            RetVal = NV_TRUE;
            break;
        }
        Id = NvPartMgrGetNextId(Id);
    }

fail:
    return RetVal;
}

static NvError BurnWarrantyFuse(void)
{
    NvU32 Size = 32;
    NvU32 Data[8];
    NvError e = NvSuccess;

    NvOsMemset(Data, 0, Size);
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_ReservedOdm, (NvU8 *)Data, &Size));

    if (Data[0] & 1)
        goto fail;

    Data[0] |= 1;

    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseSet(NvDdkFuseDataType_ReservedOdm, (NvU8 *)Data, &Size));
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseProgram());
fail:
    return e;
}

static NvError GetWarrantyFuse(NvBool *value)
{
    NvU32 Size = 32;
    NvU32 Data[8];
    NvError e = NvSuccess;

    NvOsMemset(Data, 0, Size);
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_ReservedOdm, (NvU8 *)Data, &Size));

    *value = Data[0] & 1;

fail:
    return e;
}

static NvError SetOdmData ( NvU32 OdmData)
{
    NvError e = NvSuccess;
    NvU32 Size = 0;
    NvU32 BctLength = 0;
    NvBctHandle Bct = NULL;
    NvBitHandle Bit = NULL;
    NvU32 Instance = 1;
    NvDdkFuseOperatingMode op_mode;
    NvU32 bct_part_id;
    NvU8 *buf = 0;
    NvRmDeviceHandle hRm = NULL;
    NvU32 startTime, endTime;

    startTime = NvOsGetTimeMS();
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, NULL));
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &Bct));
    Size = sizeof(OdmData);
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(Bct, NvBctDataType_OdmOption,
            &Size, &Instance, &OdmData)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName( "BCT", &bct_part_id )
    );

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&op_mode, (NvU32 *)&Size));

    /* re-sign and write the BCT */
    buf = NvOsAlloc(BctLength);
    NV_ASSERT(buf);
    if (!buf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvBuBctCryptoOps(Bct, op_mode, &BctLength, buf,
            NvBuBlCryptoOp_EncryptAndSign )
    );
    NV_CHECK_ERROR_CLEANUP(NvBitInit(&Bit));
    NV_CHECK_ERROR_CLEANUP(
        NvBuBctUpdate(Bct, Bit, bct_part_id, BctLength, buf )
    );

    /* Updating OdmData in SCRATCH20 */
    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));
    NV_REGW(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SCRATCH20_0, OdmData);
    NvRmClose(hRm);
    endTime = NvOsGetTimeMS();
    NvOsDebugPrintf("Time taken in updating odmdata: %u ms\n", endTime - startTime);

fail:
    return e;

}

#define WAIT_WHILE_LOCKING 300000
#define WAIT_IN_BOOTLOADER_MENU 10000
#define BLANK_LINES_BEFORE_MENU 1
#define BLANK_LINES_AFTER_MENU 1

#ifndef DISP_FONT_SCALE
#define DISP_FONT_SCALE 1
#endif
#define DISP_FONT_HEIGHT (DISP_FONT_SCALE * 16)

static NvBool s_IsUnlocked = NV_FALSE;
static const char *s_UnlockMsg[] =
{
    "Don't unlock\n",
    "Unlock\n"
};
static const char *s_LockMsg[] =
{
    "Don't lock\n",
    "Lock\n"
};
static const char *s_PartitionType[] =
{
    "basic",
    "yaffs2",
    "ext4"
};

#define CHECK_COMMAND(y) (!(NvOsMemcmp(CmdData, y, NvOsStrlen(y))))
#define COPY_RESPONSE(y) NvOsMemcpy(Response + NvOsStrlen(Response), y, \
    NvOsStrlen(y))
#define SEND_RESPONSE(y) \
    do \
    { \
        FB_CHECK_ERROR_CLEANUP(Nv3pTransportSendTimeOut( \
            hTrans, (NvU8*)y, NvOsStrlen(y), 0, TimeOutMs)); \
        NvOsDebugPrintf("Response sent: %s\n",y); \
    } while (0);

#define FASTBOOT_MAX_SIZE 256
#define FASTBOOT_BIT_SET 1
#define FASTBOOT_BIT_CLEAR 0
#define FASTBOOT_BIT_READ 2

static NvU32 FastbootBit(NvU8 state)
{
    NvU32 reg = 0;
    NvU32 temp = 0;
    NvRmDeviceHandle hRm = NULL;

    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));
    reg = NV_REGR(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SCRATCH0_0);

    if (state == FASTBOOT_BIT_SET)
       reg = reg | (1 << APBDEV_PMC_SCRATCH0_0_FASTBOOT_FLAG_BIT);

    else if (state == FASTBOOT_BIT_CLEAR)
       reg = reg & ~(1 << APBDEV_PMC_SCRATCH0_0_FASTBOOT_FLAG_BIT);

    else
       temp = reg & (1 << APBDEV_PMC_SCRATCH0_0_FASTBOOT_FLAG_BIT);

    NV_REGW(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SCRATCH0_0, reg);
    NvRmClose(hRm);
    return temp;
}

#define RCK_BIT_SET 1
#define RCK_BIT_CLEAR 0
#define RCK_BIT_READ 2

static NvU32 RCKBit(NvU8 state)
{
    NvU32 reg = 0;
    NvRmDeviceHandle hRm = NULL;
    NvU32 temp = 0;

    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));
    reg = NV_REGR(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SCRATCH0_0);

    if (state == RCK_BIT_SET)
       reg = reg | (1 << APBDEV_PMC_SCRATCH0_0_RCK_FLAG_BIT);

    else if (state == RCK_BIT_CLEAR)
       reg = reg & ~(1 << APBDEV_PMC_SCRATCH0_0_RCK_FLAG_BIT);

    else
       temp = reg & (1 << APBDEV_PMC_SCRATCH0_0_RCK_FLAG_BIT);

    NV_REGW(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SCRATCH0_0, reg);
    NvRmClose(hRm);
    return temp;
}

static void SetForceRecoveryBit(void)
{
    NvU32 reg = 0;
    NvRmDeviceHandle hRm = NULL;

    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));
    reg = NV_REGR(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
         APBDEV_PMC_SCRATCH0_0);
    reg = reg | (1 << APBDEV_PMC_SCRATCH0_0_FORCE_RECOVERY_FLAG_BIT);
    NV_REGW(hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SCRATCH0_0, reg);
    NvRmClose(hRm);
}

static NvU32 GetPartitionCrc32(const char *PartitionName)
{
    NvU32 ReadChunk = (1 << 24);
    NvU32 filecrc = 0;
    NvU32 len = 0, bytes = 0;
    NvU64 size = 0;
    NvU8 *buf = NULL;
    NvStorMgrFileHandle hReadFile = NULL;
    NvFileStat FileStat;
    NvError e = NvSuccess;

    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileQueryStat(PartitionName, &FileStat));
    size = FileStat.Size;
    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileOpen(PartitionName, NVOS_OPEN_READ,
        &hReadFile));

    buf = NvOsAlloc(NV_MIN(ReadChunk,size));
    if (!buf)
        goto fail;

    while (size)
    {
        len = (NvU32)NV_MIN(ReadChunk, size);
        NvOsMemset(buf, 0, len);
        NV_CHECK_ERROR_CLEANUP(NvStorMgrFileRead(hReadFile, buf, len, &bytes));
        filecrc = NvComputeCrc32(filecrc, buf, len);
        size -= len;
    }

fail:
    if (hReadFile)
        NvStorMgrFileClose(hReadFile);
    if (buf)
        NvOsFree(buf);
    return filecrc;
}

static NvError FastbootGetOneVar(const char *CmdData, char *Response)
{
    NvError e = NvSuccess;
    const char *YesStr = "yes";
    const char *NoStr = "no";

    if (CHECK_COMMAND("version-bootloader"))
        COPY_RESPONSE("1.0");
    else if (CHECK_COMMAND("version-baseband"))
        COPY_RESPONSE("2.0");
    else if (CHECK_COMMAND("version"))
        COPY_RESPONSE("0.4");
    else if (CHECK_COMMAND("max-download-size"))
    {
        /*
         * Leaving 32MB for other things like progress bar,
         * bitmap on screen etc.
         */
        if (s_RemainAllocator.size > 0x2000000)
            NvOsSnprintf(Response + NvOsStrlen(Response), 11, "0x%08x",
                s_RemainAllocator.size - 0x2000000);
    }
    else if (CHECK_COMMAND("serialno"))
    {
        e = FastbootGetSerialNo(
            Response + NvOsStrlen(Response), MAX_SERIALNO_LEN);
    }
    else if (CHECK_COMMAND("mid"))
        COPY_RESPONSE("001");
    else if (CHECK_COMMAND("product"))
        COPY_RESPONSE((char *)NvOdmQueryPlatform());
    else if (CHECK_COMMAND("secure"))
        COPY_RESPONSE(s_IsUnlocked ? NoStr : YesStr);
    else if (CHECK_COMMAND("unlocked"))
            COPY_RESPONSE(s_IsUnlocked ? YesStr : NoStr);
    else if (CHECK_COMMAND("partition-size:"))
    {
        NvU64 Start, Num;
        NvU32 SectorSize;
        char *Name = (char *)(CmdData + NvOsStrlen("partition-size:"));
        const PartitionNameMap *NvPart = MapFastbootToNvPartition(Name);

        if (NvPart)
        {
            NV_CHECK_ERROR_CLEANUP(
                NvAbootGetPartitionParameters(gs_hAboot, NvPart->NvPartName,
                    &Start, &Num, &SectorSize)
            );
            NvOsSnprintf(Response + NvOsStrlen(Response), 19, "0x%08x%08x",
                (NvU32)((Num*SectorSize)>>32),(NvU32)(Num*SectorSize));
        }
    }
    else if (CHECK_COMMAND("partition-type:"))
    {
        char *Name = (char *)(CmdData + NvOsStrlen("partition-type:"));
        const PartitionNameMap *NvPart = MapFastbootToNvPartition(Name);

        if (NvPart)
        {
            NvOsSnprintf(Response + NvOsStrlen(Response), 7, "%s",
                s_PartitionType[NvPart->FileSystem]);
        }
    }
    else if (CHECK_COMMAND("partition-crc32:"))
    {
        char *Name = (char *)(CmdData + NvOsStrlen("partition-crc32:"));
        const PartitionNameMap *NvPart = MapFastbootToNvPartition(Name);
        NvU32 filecrc = 0;

        if (NvPart)
            filecrc = GetPartitionCrc32(NvPart->NvPartName);
        NvOsSnprintf(Response + NvOsStrlen(Response), 11, "0x%08x", filecrc);
    }
fail:
    return e;
}

static void HandleHoldKeyPress(NvU32 *keyEvent, NvU64 *StartKeyPressTime)
{
    NvDdkKbdContextRec *kbdCtx = &s_DdkKbcCntxt;
    NvU64 EndKeyPressTime;

    if (kbdCtx->HoldKeyPressed && *keyEvent != KEY_HOLD)
    {
        EndKeyPressTime = NvOsGetTimeUS();
        if (!*StartKeyPressTime)
            return;
        if ((EndKeyPressTime - *StartKeyPressTime)
                            >= MAX_SELECT_HOLD_TIME)
        {
            *keyEvent = KEY_ENTER;
        }
    }
    else if (s_KeyFlag != kbdCtx->ReportedState &&
             s_KeyFlag == KEY_PRESS_FLAG)
    {
        *StartKeyPressTime = NvOsGetTimeUS();
        kbdCtx->ReportedState = *keyEvent;
        *keyEvent = KEY_IGNORE;
        kbdCtx->HoldKeyPressed = 1;
    }
    else if (s_KeyFlag != kbdCtx->ReportedState &&
             s_KeyFlag == KEY_RELEASE_FLAG)
    {
        kbdCtx->ReportedState = *keyEvent;
        if (kbdCtx->HoldKeyPressed)
            *keyEvent = KEY_DOWN;
        else
            *keyEvent = KEY_IGNORE;
        kbdCtx->HoldKeyPressed = 0;
    }
}

static NvError FastbootGetVar(Nv3pTransportHandle hTrans, const char *CmdData)
{
    NvError e = NvSuccess;
    NvU32 TimeOutMs = 1000;
    char Response[FASTBOOT_MAX_SIZE];

    if (CHECK_COMMAND("all"))
    {
        NvU32 i,j;
        char TempCmd[FASTBOOT_MAX_SIZE];
        const char *Variables[] =
        {
            "version-bootloader",
            "version-baseband",
            "version",
            "serialno",
            "mid",
            "product",
            "secure",
            "unlocked"
        };
        const char *PartitionCmd[]=
        {
            "partition-size:",
            "partition-type:"
        };
        const char *FlashablePartition[] =
        {
            "bootloader",
            "recovery",
            "boot",
            "dtb",
            "system",
            "cache",
            "userdata"
        };
        for (i = 0; i < NV_ARRAY_SIZE(Variables); ++i)
        {
            NvOsMemset(Response, 0, FASTBOOT_MAX_SIZE);
            COPY_RESPONSE("INFO");
            COPY_RESPONSE(Variables[i]);
            COPY_RESPONSE(": ");
            FastbootGetOneVar(Variables[i], Response);
            SEND_RESPONSE(Response);
        }

        /* partition-type and partition-size */
        for (i = 0; i < NV_ARRAY_SIZE(FlashablePartition); ++i)
        {
            for (j = 0; j < NV_ARRAY_SIZE(PartitionCmd); j++)
            {
                NvOsMemset(Response, 0, FASTBOOT_MAX_SIZE);
                NvOsMemset(TempCmd, 0, FASTBOOT_MAX_SIZE);
                COPY_RESPONSE("INFO");
                COPY_RESPONSE(PartitionCmd[j]);
                COPY_RESPONSE(FlashablePartition[i]);
                NvOsMemcpy(TempCmd, Response + NvOsStrlen("INFO"),
                NvOsStrlen(Response + NvOsStrlen("INFO")));
                COPY_RESPONSE(": ");
                FastbootGetOneVar(TempCmd, Response);
                SEND_RESPONSE(Response);
            }
        }
        NvOsMemset(Response, 0, FASTBOOT_MAX_SIZE);
        COPY_RESPONSE("OKAY");
        SEND_RESPONSE(Response);
    }
    else
    {
        NvOsMemset(Response, 0, FASTBOOT_MAX_SIZE);
        COPY_RESPONSE("OKAY");
        FastbootGetOneVar(CmdData, Response);
        SEND_RESPONSE(Response);
    }

fail:
    return e;
}

static NvError FastbootProtocol(Nv3pTransportHandle hTrans, NvU8** pBootImg,
        NvU32 *pLoadSize, NvU32 *ExitOption)
{
    NvError e = NvSuccess;
    NvError UsbError = NvError_UsbfCableDisConnected;
    NvBool IsOdmSecure = NV_FALSE;
    NvU8 *CmdData = NULL;
    NvU8 *Image = NULL;
    NvU8 *ImageAligned = NULL;
    NvU8 *data_buffer = NULL;
    NvU32 ImageSize = 0;
    NvU32 TimeOutMs = 1000;
    NvU32 OdmData = 0;
    NvU32 BytesRcvd = 0;
    NvU32 X = 0;
    NvU32 Y = 0;
    NvU32 OpMode;
    NvU32 Size;
    NvU32 FastbootMenuOption;
    TextMenu *FastbootProtocolMenu = NULL;
    const char *GetVarCmd = "getvar:";
    const char *DownloadCmd = "download:";
    const char *FlashCmd = "flash:";
    const char *EraseCmd = "erase:";
    const char *OemCmd = "oem ";
    const char *OkayResp = "OKAY";
    static NvU64 StartKeyPressTime = 0;
#ifdef NV_USE_NCT
    NvU32 DebugPort;
    nct_item_type buf;
#endif

    OdmData = nvaosReadPmcScratch();
    if (!(OdmData & (1 << NvOdmQueryBlUnlockBit())))
        s_IsUnlocked = NV_TRUE;

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                  (void *)&OpMode, (NvU32 *)&Size));

    if ((OpMode == NvDdkFuseOperatingMode_OdmProductionSecure) ||
        (OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC))
        IsOdmSecure = NV_TRUE;

    CmdData = NvOsAlloc(FASTBOOT_MAX_SIZE);
    if (!CmdData)
        return NvError_InsufficientMemory;

    Size = NV_ARRAY_SIZE(BootloaderMenuString);
    if (IsOdmSecure == NV_TRUE)
       Size = Size - 1;
    FastbootProtocolMenu = FastbootCreateTextMenu(
             Size, 0,BootloaderMenuString);
    ClearDisplay();
    s_PrintCtx.y = 0;
    DisplayBoardInfo();
    ShowPlatformBlMsg();
    Y = s_PrintCtx.y;
    FastbootMenuOption = BootloaderTextMenu(FastbootProtocolMenu, 0);

    while (1)
    {
        NvOsMemset(CmdData, 0, FASTBOOT_MAX_SIZE);

        /* Handle USB traffic */
        if ((UsbError == NvError_UsbfCableDisConnected) || BytesRcvd)
        {
            UsbError = Nv3pTransportReceiveTimeOut(hTrans, CmdData,
                FASTBOOT_MAX_SIZE, &BytesRcvd, 0, 0);
        }
        if (UsbError != NvError_UsbfCableDisConnected)
        {
            UsbError = Nv3pTransportReceiveIfComplete(hTrans, CmdData,
                FASTBOOT_MAX_SIZE, &BytesRcvd);
        }

        /* Handle key events */
        if (!BytesRcvd)
        {
            NvU32 keyEvent = NvDdkKeyboardGetKeyEvent(&s_KeyFlag);
            if (keyEvent == KEY_HOLD)
            {
                HandleHoldKeyPress(&keyEvent, &StartKeyPressTime);
            }
            else
            {
                if (s_DdkKbcCntxt.HoldKeyPressed)
                {
                    HandleHoldKeyPress(&keyEvent, &StartKeyPressTime);
                }
                if (s_KeyFlag != KEY_PRESS_FLAG)
                {
                    keyEvent = KEY_IGNORE;
                }
            }

            switch (keyEvent)
            {
                case KEY_UP:
                    FastbootTextMenuSelect(FastbootProtocolMenu, NV_FALSE);
                    FastbootDrawTextMenu(FastbootProtocolMenu, X, Y);
                    break;
                case KEY_DOWN:
                    FastbootTextMenuSelect(FastbootProtocolMenu, NV_TRUE);
                    FastbootDrawTextMenu(FastbootProtocolMenu, X, Y);
                    break;
                case KEY_ENTER:
                    *ExitOption = FastbootProtocolMenu->CurrOption;
                    e = NvSuccess;
                    goto fail;
            }
            continue;
        }
        NvOsDebugPrintf("Cmd Rcvd: %s\n", CmdData);

        if (Image && (!CHECK_COMMAND("boot")) && (!CHECK_COMMAND(FlashCmd)))
        {
            e = NvError_InvalidState;
            goto fail;
        }
        /* Handle Fastboot Command */
        if (CHECK_COMMAND(OemCmd))
        {
            if (!NvOsMemcmp(CmdData+NvOsStrlen(OemCmd), "lock",
                NvOsStrlen("lock")))
            {
                if (s_IsUnlocked == NV_TRUE)
                {
                    TextMenu *LockMenu = FastbootCreateTextMenu(
                        NV_ARRAY_SIZE(s_LockMsg), 0, s_LockMsg);
                    SEND_RESPONSE("INFOShowing Options on Display.");
                    SEND_RESPONSE("INFOUse device keys for selection.");
                    ClearDisplay();
                    s_PrintCtx.y = 0;
                    ShowPlatformBlMsg();
                    FastbootMenuOption =
                        BootloaderTextMenu(LockMenu, WAIT_WHILE_LOCKING);
                    if (FastbootMenuOption == 1)
                    {
                        FastbootStatus("Locking...");
                        SEND_RESPONSE("INFOerasing userdata...");
                        NV_CHECK_ERROR_CLEANUP(NvAbootBootfsFormat("UDA"));
                        SEND_RESPONSE("INFOerasing userdata done");
                        SEND_RESPONSE("INFOerasing cache...");
                        NV_CHECK_ERROR_CLEANUP(NvAbootBootfsFormat("CAC"));
                        SEND_RESPONSE("INFOerasing cache done");
                        SEND_RESPONSE("INFOLocking...");
                        NV_CHECK_ERROR_CLEANUP(
                            SetOdmData(OdmData |
                                (1 << NvOdmQueryBlUnlockBit()))
                        );
                        s_IsUnlocked = NV_FALSE;
                        SEND_RESPONSE("INFOBootloader is locked now.");
                    }
                    else
                    {
                        SEND_RESPONSE("INFOExit without locking.");
                    }
                    ClearDisplay();
                    s_PrintCtx.y = 0;
                    DisplayBoardInfo();
                    ShowPlatformBlMsg();
                    Y = s_PrintCtx.y;
                    FastbootMenuOption =
                        BootloaderTextMenu(FastbootProtocolMenu, 0);
                }
                else
                {
                    SEND_RESPONSE("INFOBootloader is already locked.");
                }
            }
            else if (!NvOsMemcmp(CmdData+NvOsStrlen(OemCmd), "unlock",
                NvOsStrlen("unlock")))
            {
                if (s_IsUnlocked == NV_FALSE)
                {
                    TextMenu *UnlockMenu = FastbootCreateTextMenu(
                         NV_ARRAY_SIZE(s_UnlockMsg), 0, s_UnlockMsg);
                    SEND_RESPONSE("INFOShowing Options on Display.");
                    SEND_RESPONSE("INFOUse device keys for selection.");
                    ClearDisplay();
                    s_PrintCtx.y = 0;
                    FastbootStatus("Unlock bootloader?\n");
                    FastbootStatus("       !!! READ THE FOLLOWING !!!\n");
                    FastbootStatus("Unlocking the bootloader allows critical");
                    FastbootStatus("parts of the system to be replaced, which");
                    FastbootStatus("can potentially leave your device in an");
                    FastbootStatus("irreversibly broken state.\n");
                    FastbootStatus("It will also delete all personal data");
                    FastbootStatus("(similarly to a factory reset) in order to");
                    FastbootStatus("prevent unauthorized access.\n");
                    FastbootStatus("Be sure to know what you are doing.\n");
                    FastbootStatus("YOUR WARRANTY WILL BE VOID IF YOU CONTINUE!\n\n");
                    ShowPlatformBlMsg();
                    FastbootMenuOption =
                        BootloaderTextMenu(UnlockMenu, WAIT_WHILE_LOCKING);
                    if (FastbootMenuOption == 1)
                    {
                        FastbootStatus("Unlocking...");
                        SEND_RESPONSE("INFOerasing userdata...");
                        NV_CHECK_ERROR_CLEANUP(NvAbootBootfsFormat("UDA"));
                        SEND_RESPONSE("INFOerasing userdata done");
                        SEND_RESPONSE("INFOerasing cache...");
                        NV_CHECK_ERROR_CLEANUP(NvAbootBootfsFormat("CAC"));
                        SEND_RESPONSE("INFOerasing cache done");
                        SEND_RESPONSE("INFOunlocking...");
                        NV_CHECK_ERROR_CLEANUP(BurnWarrantyFuse());
                        NV_CHECK_ERROR_CLEANUP(
                            SetOdmData(OdmData &
                                (~(1 << NvOdmQueryBlUnlockBit())))
                        );
                        s_IsUnlocked = NV_TRUE;
                        SEND_RESPONSE("INFOBootloader is unlocked now.");
                    }
                    else
                    {
                        SEND_RESPONSE("INFOExit without unlocking.");
                    }
                    ClearDisplay();
                    s_PrintCtx.y = 0;
                    DisplayBoardInfo();
                    ShowPlatformBlMsg();
                    Y = s_PrintCtx.y;
                    FastbootMenuOption =
                        BootloaderTextMenu(FastbootProtocolMenu, 0);
                }
                else
                {
                    SEND_RESPONSE("INFOBootloader is already unlocked.");
                }
            }
            else if (!NvOsMemcmp(CmdData+NvOsStrlen(OemCmd), "force-recovery",
                NvOsStrlen("force-recovery")))
            {
                ClearDisplay();
                SetForceRecoveryBit();
                NvAbootReset(gs_hAboot);
            }
#ifdef NV_USE_NCT
            else if (!NvOsMemcmp(CmdData+NvOsStrlen(OemCmd), "sd-uart",
                NvOsStrlen("sd-uart")))
            {
                DebugPort = NvOdmDebugConsole_UartSD - NvOdmDebugConsole_UartA;
                OdmData = (OdmData &
                           ~(7 << NvOdmQueryGetDebugConsoleOptBits())) |
                           (DebugPort << NvOdmQueryGetDebugConsoleOptBits());
                NV_CHECK_ERROR_CLEANUP(SetOdmData(OdmData));
            }
            else if (!NvOsMemcmp(CmdData+NvOsStrlen(OemCmd), "sd-no-uart",
                NvOsStrlen("sd-no-uart")))
            {
                /* Read default debug console id from NCT
                 * Set it back in odmdata to disable UART over uSD
                 */
                e = NvNctReadItem(NCT_ID_DEBUG_CONSOLE_PORT_ID, &buf);
                if (NvSuccess == e)
                {
                    DebugPort = buf.debug_port.port_id;
                }
                OdmData = (OdmData &
                           ~(7 << NvOdmQueryGetDebugConsoleOptBits())) |
                           (DebugPort << NvOdmQueryGetDebugConsoleOptBits());
                NV_CHECK_ERROR_CLEANUP(SetOdmData(OdmData));
            }
#endif
            else
            {
                SEND_RESPONSE("INFOUnknown Command!");
            }
            SEND_RESPONSE(OkayResp);
        }
        else if (CHECK_COMMAND(GetVarCmd))
        {
            NV_CHECK_ERROR_CLEANUP(FastbootGetVar(hTrans, (char *)CmdData +
                NvOsStrlen(GetVarCmd))
            );
        }
        else if (CHECK_COMMAND(DownloadCmd))
        {
            char Response[32];
            NvU32 temp_bytes_rcvd = 0;
            NvU32 so_far = 0;

            if (s_IsUnlocked == NV_FALSE)
            {
                SEND_RESPONSE("FAILBootloader is locked.");
                continue;
            }

            ImageSize =
                NvUStrtol((char*)(CmdData+NvOsStrlen(DownloadCmd)), NULL, 16);
            if (!(ImageSize > 0))
            {
                SEND_RESPONSE("FAILBad image size");
                continue;
            }
            /*
             *  Allocating 4K-1  bytes extra to align buffer to 4K bytes if not alinged
             *  As USB Driver needs 4K aligned buffer.
             */
            Image = NvOsAlloc(ImageSize + USB_BUF_ALIGNMENT_SIZE - 1);
            if (!Image)
            {
                SEND_RESPONSE("FAILInsufficient memory.");
                continue;
            }
            ImageAligned = (NvU8 *)((Image) + USB_BUF_ALIGNMENT_SIZE - 1);
            ImageAligned = (NvU8 *)((NvU32)ImageAligned & (~(USB_BUF_ALIGNMENT_SIZE - 1)));

            data_buffer = ImageAligned;

            NvOsSnprintf(Response, sizeof(Response), "DATA%08x\n",
                ImageSize);
            SEND_RESPONSE(Response);

            /*  write only maximum 1MB of data at a time */
            while (so_far < ImageSize)
            {
                NvU32 temp_size = TEMP_BUFFER_SIZE;
                if ((ImageSize - so_far) < temp_size)
                    temp_size = ImageSize - so_far;

                FB_CHECK_ERROR_CLEANUP(
                    Nv3pTransportReceiveTimeOut(hTrans, data_buffer,
                        temp_size, &temp_bytes_rcvd, 0, TimeOutMs)
                );

                if (temp_bytes_rcvd != temp_size)
                {
                    SEND_RESPONSE("INFOReceived Data of Bad Length.");
                    e = NvError_Nv3pBadReceiveLength;
                    goto fail;
                }

                data_buffer += temp_size;
                so_far += temp_size;
            }
            SEND_RESPONSE(OkayResp);
        }
        else if (CHECK_COMMAND("boot"))
        {
            if (s_IsUnlocked == NV_FALSE)
            {
                SEND_RESPONSE("FAILBootloader is locked.");
                continue;
            }

            if ((Image == NULL) || (ImageSize == 0))
            {
                e = NvError_InvalidState;
                goto fail;
            }

            *pBootImg = ImageAligned;
            *pLoadSize = ImageSize;
            SEND_RESPONSE(OkayResp);
            *ExitOption = BOOT;
            goto fail;
        }
        else if (CHECK_COMMAND("reboot-bootloader"))
        {
            *ExitOption = REBOOT_BOOTLOADER;
            SEND_RESPONSE(OkayResp);
            //  reboot-bootloader breaks out of the loop
            goto fail;
        }
        else if (CHECK_COMMAND("reboot"))
        {
            *ExitOption = REBOOT;
            SEND_RESPONSE(OkayResp);
            //  reboot breaks out of the loop
            goto fail;
        }
        else if (CHECK_COMMAND("continue"))
        {
            *ExitOption = CONTINUE;
            SEND_RESPONSE(OkayResp);
            goto fail;
        }
        else if (CHECK_COMMAND(FlashCmd))
        {
            char *Name = (char *)(CmdData + NvOsStrlen(FlashCmd));
            const PartitionNameMap *NvPart = MapFastbootToNvPartition(Name);
            NvBool isExposed = NV_FALSE;

            if (s_IsUnlocked == NV_FALSE)
            {
                SEND_RESPONSE("FAILBootloader is locked.");
                continue;
            }

            if (!NvPart)
            {
                SEND_RESPONSE("FAILInvalid Partition Name.");
                continue;
            }

            if ((Image == NULL) || (ImageSize == 0))
            {
                e = NvError_InvalidState;
                goto fail;
            }

            isExposed = IsPartitionExposed(NvPart->NvPartName);
            if (IsOdmSecure && isExposed == NV_FALSE)
            {
                NvU32 SectorSize = 0;
                NvU64 Start = 0;
                NvU64 Num = 0;
                NvS64 StagingPartitionSize = 0;
                char partName[NVPARTMGR_PARTITION_NAME_LENGTH];

                NvOsMemset(partName, 0x0, NVPARTMGR_PARTITION_NAME_LENGTH);
                NvOsStrncpy(partName, "USP", NvOsStrlen("USP"));
                e = NvAbootGetPartitionParameters(gs_hAboot, partName, &Start,
                    &Num, &SectorSize);
                StagingPartitionSize = (e == NvSuccess) ? (Num * SectorSize) : 0;
                if (StagingPartitionSize < ImageSize)
                {
                    NvOsStrncpy(partName, "CAC", NvOsStrlen("CAC"));
                    e = NvAbootGetPartitionParameters(gs_hAboot, partName, &Start,
                        &Num, &SectorSize);
                    StagingPartitionSize = (e == NvSuccess) ? (Num * SectorSize) : 0;
                    if (StagingPartitionSize < ImageSize)
                    {
                        SEND_RESPONSE("FAILStaging partition size is not big enough.");
                        continue;
                    }
                }

                e = NvAbootBootfsWrite(gs_hAboot, partName,
                    ImageAligned, ImageSize);
                if (e == NvSuccess)
                    e = NvInstallBlob(partName);
                NvAbootBootfsFormat(partName);
            }
            else
            {
                e = NvFastbootUpdateBin(ImageAligned, ImageSize, NvPart);
            }

            ImageSize = 0;
            NvOsFree(Image);
            Image = NULL;
            if (e != NvSuccess)
                goto fail;
            SEND_RESPONSE(OkayResp);
        }
        else if (CHECK_COMMAND(EraseCmd))
        {
            char *Name = (char *)(CmdData + NvOsStrlen(EraseCmd));
            const PartitionNameMap *NvPart = MapFastbootToNvPartition(Name);
            NvBool isExposed = NV_FALSE;

            if (!NvPart)
            {
                SEND_RESPONSE("FAILInvalid Partition Name.");
                continue;
            }

            if (s_IsUnlocked == NV_FALSE)
            {
                SEND_RESPONSE("FAILBootloader is locked.");
                continue;
            }

            isExposed = IsPartitionExposed(NvPart->NvPartName);
            if (IsOdmSecure && (isExposed == NV_FALSE))
            {
                SEND_RESPONSE("FAILCan not erase.");
            }
            else
            {
                NV_CHECK_ERROR_CLEANUP(
                    NvAbootBootfsFormat(NvPart->NvPartName)
                );
                SEND_RESPONSE(OkayResp);
            }
        }
        else
        {
            e = NvError_Nv3pUnrecoverableProtocol;
            goto fail;
        }
    }

fail:
    if (e != NvSuccess)
    {
        if (hTrans)
        {
            char FailCmd[30];
            NvOsSnprintf(FailCmd, sizeof(FailCmd), "FAIL(%s)", ErrorName(e));
            Nv3pTransportSendTimeOut(hTrans, (NvU8 *)FailCmd,
                NvOsStrlen(FailCmd), 0, TimeOutMs);
        }
        if (Image)
        {
            ImageSize = 0;
            NvOsFree(Image);
            Image = NULL;
        }
        NvOsDebugPrintf("Failed to process command %s error(0x%x)\n",
            (char*)CmdData, e);
    }

    if (CmdData)
    {
        NvOsFree(CmdData);
        CmdData = NULL;
    }

    return e;
}

static void NvAppInitDisplay(void)
{
    if (InitDispDone)
        return;
    BootLoader.Start_Display = (NvU32)NvOsGetTimeUS();
    NvOsDebugPrintf("Initializing Display\n");
    FastbootInitializeDisplay(
            FRAMEBUFFER_DOUBLE | FRAMEBUFFER_32BIT | FRAMEBUFFER_CLEAR,
            &s_Framebuffer, &s_Frontbuffer, &s_FrontbufferPixels, NV_FALSE
            );
    BootLoader.End_Display = (NvU32)NvOsGetTimeUS();
    InitDispDone = NV_TRUE;
}

#ifdef LP0_SUPPORTED
static void NvAppShutDownDisplay(void)
{
    NvOsDebugPrintf("Shutting down Display\n");
    FastbootDeinitializeDisplay(&s_Framebuffer, &s_FrontbufferPixels);
}
#endif

#ifdef LPM_BATTERY_CHARGING
void NvAppNotifyOnDisplay(NvOdmDisplayImage Image)
{
    BitmapFile *bmf = NULL;
    NvError err = NvSuccess;

    // Do not display anything for RTC wake up
   // if(PowerOnSource == NvOdmPmuRTCAlarm)
    //    return;

    NvAppInitDisplay();

    switch (Image)
    {
        case NvOdmImage_Lowbattery:
            err = FastbootBMPRead(gs_hAboot, &bmf, (const char *) LOWBAT_BITMAP_PARTITION);
            break;
        case NvOdmImage_Charging:
            err = FastbootBMPRead(gs_hAboot, &bmf, (const char *) BAT_CHARGING_PARTITION);
            break;
        case NvOdmImage_Charged:
            err = FastbootBMPRead(gs_hAboot, &bmf, (const char *) BAT_CHARGED_PARTITION);
            break;
        case NvOdmImage_Nvidia:
            err = FastbootBMPRead(gs_hAboot, &bmf, (const char *) BITMAP_PARTITON);
            break;
        case NvOdmImage_FullyCharged:
            err = FastbootBMPRead(gs_hAboot, &bmf, (const char *) BAT_FULLY_CHARGED_PARTITION);
            break;
        default:
            err = NvError_NotSupported;
    }

    if (err == NvSuccess)
        err = FastbootDrawBMP(bmf, &s_Frontbuffer, &s_FrontbufferPixels);
    if (err != NvSuccess)
        FastbootStatus(s_DispMessage);

    if (Image != NvOdmImage_Nvidia)
        NvOsSleepMS(s_ChargingConfigData.NotifyTimeOut);
}

#ifdef LP0_SUPPORTED
static NvError NvAppDoCharge(NvU32 OSChrgInPercent, NvU32 ExitChrgInPercent)
{
    NvError e = NvError_Success;
    NvBool Status;
    static NvBool SuspendInit = NV_FALSE;

    // Set charging current.
    NvOsDebugPrintf("Start charging\n");
    Status = NvOdmChargingInterface(NvOdmChargingID_StartCharging, s_pPmu);
    NV_ASSERT(Status);

   if(NvOdmQueryLp0Supported())
   {
        NvOsDebugPrintf("Closing USB\n");
        NvBlUsbClose();
        if (SuspendInit == NV_FALSE)
        {
            NvAppSuspendInit();
            SuspendInit = NV_TRUE;
        }
        NV_CHECK_ERROR_CLEANUP(NvAppSuspend());
   }

fail:
    return e;
}
#endif

static NvBool CheckIfBoardIsLoki(NvU16 BoardId)
{
    switch(BoardId)
    {
        case LOKI_NFF_BOARDID:
        case LOKI_THOR195_BOARDID:
        case LOKI_FFD_BOARDID:
            return NV_TRUE;
        default:
            return NV_FALSE;
    }
}

static void GracefulShutDown(void)
{
    NvOdmBoardInfo BoardInfo;
    DateTime date;

    if(!NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                &BoardInfo, sizeof(BoardInfo)))
        NvOsDebugPrintf("Cannot read board info....shutting down anyway\n");

    NvOdmPmuSetChargingCurrent(s_pPmu,NvOdmPmuChargingPath_UsbBus,
            3000,NvOdmUsbChargerType_UsbHost);

    if(CheckIfBoardIsLoki(BoardInfo.BoardID) ||
        BoardInfo.BoardID == TN8_FFD_BOARDID ||
        BoardInfo.BoardID == SHIELD_ERS_BOARDID)
    {
        NvOdmPmuClearAlarm(s_pPmu);
        NvOdmPmuGetDate(s_pPmu, &date);
        NvOsDebugPrintf("Setting Alarm after 1hr to check charge complete \n\n");
        NvOdmPmuSetAlarm(s_pPmu, NvAppAddAlarmSecs(date, 3600));
    }

    NvOdmPmuPowerOff(s_pPmu);
}

static NvError BootToKernel(void)
{
    NvError e = NvSuccess;
    ClearDisplay();
    NvOsDebugPrintf("Booting to kernel \n");
    NvOdmPmuUnSetChargingInterrupts(s_pPmu);
    FastbootEnableBacklightTimer(FB_EnableBacklight,0);
    NV_CHECK_ERROR_CLEANUP(NvAbootLowPowerMode(NvAbootLpmId_ComeOutOfLowPowerMode));
fail:
    return e;
}

static NvU32 get_power_key(void)
	{
	
	NvOdmGpioPinHandle hOdmPins;
	NvOdmServicesGpioHandle hOdmGpio;
	NvU32 state=0;
	hOdmGpio = NvOdmGpioOpen();
	hOdmPins = NvOdmGpioAcquirePinHandle(hOdmGpio, 'q' - 'a', 0);
	NvOdmGpioConfig(hOdmGpio, hOdmPins, NvOdmGpioPinMode_InputData );
	NvOdmGpioGetState(hOdmGpio,hOdmPins, &state);
	return state;
}

enum
{
    RED,
    GREEN,
};
enum
{
    OFF,
    ON,
};

static NvBool set_charging_led_onoff(NvU8 led, NvU8 status)
{

	NvOdmGpioPinHandle hOdmPinsLedRed,hOdmPinsLedGreen;
	NvOdmServicesGpioHandle hOdmGpio;

	hOdmGpio = NvOdmGpioOpen();

	if (!hOdmGpio)
    {
        NV_ASSERT(!"GPIO open failed \n");
        return NV_FALSE;
    }

	if(led == RED)
	{
		hOdmPinsLedRed = NvOdmGpioAcquirePinHandle(hOdmGpio, 'k' - 'a', 2); //red
		if (!hOdmPinsLedRed){
			NvOsDebugPrintf("[yunboa]:GPIO pin k2 acquisition failed\n");
			return NV_FALSE;
		}
		NvOdmGpioConfig(hOdmGpio, hOdmPinsLedRed, NvOdmGpioPinMode_Output );
		NvOdmGpioSetState(hOdmGpio,hOdmPinsLedRed, status);
		NvOdmGpioClose(hOdmGpio);
	}
	else if(led == GREEN)
	{
		hOdmPinsLedGreen = NvOdmGpioAcquirePinHandle(hOdmGpio, 'k' - 'a', 3); //green
		if (!hOdmPinsLedGreen){
			NvOsDebugPrintf("[yunboa]:GPIO pin k3 acquisition failed\n");
			return NV_FALSE;
		}
		NvOdmGpioConfig(hOdmGpio, hOdmPinsLedGreen, NvOdmGpioPinMode_Output );
		NvOdmGpioSetState(hOdmGpio,hOdmPinsLedGreen, status);
		NvOdmGpioClose(hOdmGpio);
	}

	return NV_TRUE;
}

static NvError NvChargerLoop(void)
{
    NvOdmBoardInfo BoardInfo;
    DateTime date;
    NvU32 BatteryChrgInPercent;
    NvBool ChargerConnected = NV_FALSE;
    NvError e = NvSuccess;
	NvU32 BatteryAdcVal = 0;
	NvU32 BatteryVoltage = 0;

    if(!NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                &BoardInfo, sizeof(BoardInfo)))
        NvOsDebugPrintf("[%u] Cannot read board info....continuing anyway\n",NvOsGetTimeMS());

    //Go to low power mode
    NV_CHECK_ERROR_CLEANUP(NvAbootLowPowerMode(NvAbootLpmId_GoToLowPowerMode,
        s_ChargingConfigData.LpmCpuKHz));

    NvOsSemaphoreCreate(&s_hKeySema,0);

    //Power key detection can be from PMU or GPIO
    NvOdmRegisterPowerKey(KeyReleaseIsr);
/*
    if(CheckIfBoardIsLoki(BoardInfo.BoardID) ||
        BoardInfo.BoardID == TN8_FFD_BOARDID ||
        BoardInfo.BoardID == SHIELD_ERS_BOARDID)
    {
        NvOsDebugPrintf("[%u] Setting Alarm to 30secs for WDT service[%s] \n",
            NvOsGetTimeMS(),__func__);
        NvOdmPmuGetDate(s_pPmu, &date);
        NvOdmPmuSetAlarm(s_pPmu, NvAppAddAlarmSecs(date, 30));
    }
*/
    // Enable charger interrupts for cable disconnect, WDT fault and charge complete
    NvOdmPmuSetChargingInterrupts(s_pPmu);

    // Start charging
    NvOdmChargingInterface(NvOdmChargingID_StartCharging, &ChargerConnected);

	// Control Charging Led
	set_charging_led_onoff(RED,ON);
	set_charging_led_onoff(GREEN,OFF);

    while (1)
    {
        NvOdmChargingInterface(NvOdmChargingID_GetBatteryCharge,s_pPmu, &BatteryChrgInPercent);
		NvOdmChargingInterface(NvOdmChargingID_IsChargerConnected,&ChargerConnected);
		NvOdmPmuGetBatteryAdcVal(s_pPmu ,2 ,&BatteryAdcVal);
		NvOdmPmuBatteryAdc2Voltage(BatteryAdcVal ,&BatteryVoltage);
		 if (!ChargerConnected){
		 	NvOsDebugPrintf(" deatch usb line, power off\n");
			 NvOdmPmuPowerOff(s_pPmu);
			 while(1);

		 }
	 	
        NvOsDebugPrintf("\n[%u] Charging with  Charger > 500mA...\n", NvOsGetTimeMS());
        NvOsSemaphoreWait(s_hKeySema);
        NvOsDebugPrintf("\n Power key press down \n");
		


//        if(BatteryChrgInPercent >= s_ChargingConfigData.OsChargeLevel)              // jxd edit
		if (0)
        {
            if(NvOdmPeripheralGetBoardInfo(PMU_E1735,&BoardInfo))
                NvOdmChargingInterface(NvOdmChargingID_StopCharging, s_pPmu);
            return BootToKernel();
        }
        else
        {
            FastbootEnableBacklightTimer(FB_EnableBacklight, 0);
#ifdef READ_BATTERY_SOC
            NvOsDebugPrintf("[%u] Battery SOC = %u(percentage) \n", NvOsGetTimeMS(), BatteryChrgInPercent);
#else
            NvOsDebugPrintf("[%u] Battery Voltage = %u(mv) \n", NvOsGetTimeMS(), BatteryChrgInPercent);
#endif		
			if (BatteryChrgInPercent < 20)
           	 	NvAppNotifyOnDisplay(NvOdmImage_Charging);
			else if (BatteryChrgInPercent < 80)
				NvAppNotifyOnDisplay(NvOdmImage_Charged);
			else
			{
				NvAppNotifyOnDisplay(NvOdmImage_FullyCharged);
				set_charging_led_onoff(RED,OFF);
				set_charging_led_onoff(GREEN,ON);
			}

			int ret =0;	
			ret=NvOsSemaphoreWaitTimeout(s_hKeySema,10000);
			if ((ret == NvSuccess && BatteryVoltage > 3620))      // press power key again && battery SOC > 5% ,then boot kernel
				return BootToKernel();
	        NvOsDebugPrintf("\n get power key again   \n");

            FastbootEnableBacklightTimer(FB_DisableBacklight, 0);
            NvOsDebugPrintf("\n[%u] Not enough power to boot..continuing charging\n", NvOsGetTimeMS());
        }
    }
fail:
    return e;
}

static void UsbHostChargingLoop(NvOdmUsbChargerType ChargerType)
{
    NvBool ChargerConnected = NV_FALSE;

    // Check if charger was disconnected
    NvOdmChargingInterface(NvOdmChargingID_IsChargerConnected,
        &ChargerConnected);
    if(ChargerConnected)
    {
        // Check if a different charger was connected in the meantime
        NvOdmChargingInterface(NvOdmChargingID_GetChargerType,
            &ChargerType);
        if((ChargerType == NvOdmUsbChargerType_UsbHost) ||
            (ChargerType == NvOdmUsbChargerType_NonCompliant)){
            // For this type, shutdown with alarm to wake up after 1 hr
            
			 NvChargerLoop();
           // GracefulShutDown();
       } else
            NvChargerLoop();
    }
    else
    {
        NvOsDebugPrintf("\nCharger detached!\n");
        NvOdmPmuPowerOff(s_pPmu);
    }
}

static void MainChargingLoop(NvU32 OSChrgInPercent,
        NvU32 ExitChrgInPercent)
{
    NvBool ChargerConnected = NV_FALSE;
    NvU32 BatteryChrgInPercent = 0;
    NvOdmUsbChargerType ChargerType = NvOdmUsbChargerType_Dummy;
    NvOdmDisplayImage Image = NvOdmImage_Charging;
    NvOdmBoardInfo BoardInfo;
	NvU32 BatteryAdcVal = 0;
	NvU32 BatteryVoltage = 0;

    if(!NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                &BoardInfo, sizeof(BoardInfo)))
        NvOsDebugPrintf("[%u] Cannot read board info....continuing anyway\n",NvOsGetTimeMS());

    NvOdmChargingInterface(NvOdmChargingID_IsChargerConnected,
                    &ChargerConnected);
    NvOdmChargingInterface(NvOdmChargingID_GetBatteryCharge,
        s_pPmu, &BatteryChrgInPercent);

    if(!ChargerConnected)
    {
        NvOsDebugPrintf("\nCharger NOT connected!\n");

        if(BatteryChrgInPercent < OSChrgInPercent)
        {
            // Display LOWBAT image
            NvAppNotifyOnDisplay(NvOdmImage_Lowbattery);
        }

        // Check if charger is connected now
        NvOdmChargingInterface(NvOdmChargingID_IsChargerConnected,
            &ChargerConnected);
        // If charger's still not connected, shutdown
        if(!ChargerConnected)
        {
            NvOsDebugPrintf("\nNo Charger connected! Powering off\n");
            NvOdmPmuPowerOff(s_pPmu);
        }
    }

    // For Loki just use kernel charging loop
    if(CheckIfBoardIsLoki(BoardInfo.BoardID))
    {
        BootToCharging = NV_TRUE;
        NvOsDebugPrintf("Booting to Kernel Charging\n");
        BootToKernel();
        return;
    }

#ifdef READ_BATTERY_SOC
	NvOdmPmuGetBatteryAdcVal(s_pPmu ,2 ,&BatteryAdcVal);
	NvOdmPmuBatteryAdc2Voltage(BatteryAdcVal ,&BatteryVoltage);

	if (BatteryVoltage < 3700)
		Image = NvOdmImage_Charging;
    if (BatteryChrgInPercent < 4000)
        Image = NvOdmImage_Charged;
   else 
        Image = NvOdmImage_FullyCharged;

	/*if (BatteryChrgInPercent < 20)
		Image = NvOdmImage_Charging;
    if (BatteryChrgInPercent < 80)
        Image = NvOdmImage_Charged;
   else 
        Image = NvOdmImage_FullyCharged;*/
#endif
    NvAppNotifyOnDisplay(Image);
    FastbootEnableBacklightTimer(FB_DisableBacklight, 0);

    /* Charger connected */
    if(NvOdmQueryLp0Supported())
    {
    //It wont return, Entering into LP0
#ifdef LP0_SUPPORTED
        NvAppDoCharge(OSChrgInPercent, ExitChrgInPercent);
#endif
    }
    NvOdmChargingInterface(NvOdmChargingID_GetChargerType,
        &ChargerType);

    if((ChargerType == NvOdmUsbChargerType_UsbHost) ||
        (ChargerType == NvOdmUsbChargerType_NonCompliant))
        UsbHostChargingLoop(ChargerType);
    else
    {
        /* Connected to NvCharger/CDP/DCP */
        NvChargerLoop();
    }
}

static NvError NvAppChargeBatteryIfLow(void)
{
    NvError e = NvError_Success;
    NvBool Status = NV_FALSE;
    NvBool BatteryConnected  = NV_FALSE;
    NvU32 BatteryChrgInPercent = 0;
    NvU32 OSChrgInPercent           = 0;
    NvU32 ExitChrgInPercent         = 0;
    NvOdmPowerSupplyInfo PowerSupplyInfo;
    NvOdmBoardInfo BoardInfo;
	NvU32 ChrgInPercent = 0;
	NvU32 BatteryAdcVal = 0;
	NvU32 BatteryVoltage = 0;
	NvU32 Time = 0;

    /* Battery charging will be supported only on specific boards */
    if(!NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                &BoardInfo, sizeof(BoardInfo)))
        return NvError_NotSupported;
#if 0
    /* Battery charging will only be supported if device powered by battery */
    if (!NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PowerSupply,
                &PowerSupplyInfo, sizeof(PowerSupplyInfo)))
        return NvSuccess;

    if (PowerSupplyInfo.SupplyType == NvOdmBoardPowerSupply_Adapter)
        return NvSuccess;
#endif	

    // Open PMU
    Status = NvOdmPmuDeviceOpen(&s_pPmu);
    NV_ASSERT(s_pPmu);
    NV_ASSERT(Status);

    PowerOnSource = NvOdmPmuGetPowerOnSource(s_pPmu);
	NvOdmPmuReadRstReason(s_pPmu,&Reason);
    NvOsDebugPrintf("\n[%u] Power-on source: 0x%x , Reason = 0x%x\n", NvOsGetTimeMS(),PowerOnSource,Reason);

    // Initialize the Odm charging interface
    NvOsDebugPrintf("[%u] Initializing charging interface\n",NvOsGetTimeMS());
    Status = NvOdmChargingInterface(NvOdmChargingID_InitInterface, s_pPmu);
    NV_ASSERT(Status);

    // Check if battery is connected
    NvOsDebugPrintf("[%u] Looking for battery\n",NvOsGetTimeMS());
    Status = NvOdmChargingInterface(NvOdmChargingID_IsBatteryConnected, &BatteryConnected);
    NV_ASSERT(Status);
    if(BatteryConnected)
    {
        NvOsDebugPrintf("[%u] Battery connected\n", NvOsGetTimeMS());
        // Get/show initial battery charge level
        Status = NvOdmChargingInterface(NvOdmChargingID_GetBatteryCharge, s_pPmu, &BatteryChrgInPercent);
        NV_ASSERT(Status);
#ifdef READ_BATTERY_SOC
        NvOsDebugPrintf("[%u] Battery SOC = %u(percentage) \n", NvOsGetTimeMS(), BatteryChrgInPercent);
#else
        NvOsDebugPrintf("[%u] Battery Voltage = %u(mv) \n", NvOsGetTimeMS(), BatteryChrgInPercent);
#endif
		NvOdmPmuGetBatteryCapacity(s_pPmu ,&ChrgInPercent);
		NvOsDebugPrintf("Read Battery Capacity from ADC saved = %u(percentage) \n", ChrgInPercent);
		NvOdmPmuGetBatteryAdcVal(s_pPmu ,2 ,&BatteryAdcVal);
		NvOdmPmuBatteryAdc2Voltage(BatteryAdcVal ,&BatteryVoltage);
		NvOsDebugPrintf("Battery AdcVal = %d, BatteryVoltage =%d \n", BatteryAdcVal,BatteryVoltage);

    }
    else
    {
        // Battery not connected (most likely a development system)
        // Boot to kernel in this case
        NvOsDebugPrintf("[%u] No battery!!! Ignoring and booting to kernel\n",NvOsGetTimeMS());
        return BootToKernel();
    }

    // Get charging configuration data
    NvOsDebugPrintf("[%u] Getting Charging Data.\n", NvOsGetTimeMS());
    Status = NvOdmChargingInterface(NvOdmChargingID_GetChargingConfigData, &s_ChargingConfigData);
    if (!Status)
    {
        NvOsDebugPrintf("[%u] Charging Not Supported !!!.\n", NvOsGetTimeMS());
        goto fail;
    }
    OSChrgInPercent = s_ChargingConfigData.OsChargeLevel;
    ExitChrgInPercent = s_ChargingConfigData.ExitChargeLevel;

#ifdef READ_BATTERY_SOC
    NvOsDebugPrintf("[%u] OS Boot SOC  = %u(percentage) \n", NvOsGetTimeMS(), OSChrgInPercent);
#else
    NvOsDebugPrintf("[%u] OS Boot Voltage = %u(mv) \n", NvOsGetTimeMS(), OSChrgInPercent);
#endif

    // Check for SW Reboot
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&s_hRm, 0));
    s_Chipreset = NV_REGR(s_hRm,
        NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_RST_STATUS_0);
    NvRmClose(s_hRm);
	Time = NvOsGetTimeMS();
	if((Time >= 1150)&&(Time <= 1250))
		RebootSource = NvOdmRebootRebootKey;
	//else if((Time >= 1330)&&(Time <= 1445))
	//	RebootSource = NvOdmRebootAdbReboot;

    NvOsDebugPrintf("[%u] s_Chipreset: %d ,RebootSource: %d\n", Time, s_Chipreset,RebootSource);



    //Boot to kernel only when battery has enough voltage and power on source is
    //either power on key or s/w reset(adb reboot)


    NvOsDebugPrintf("   charing_soc=%d , os_percent=%d ,vbus_state=%d\n",BatteryChrgInPercent,OSChrgInPercent,get_bqvbus());
           // whatever soc   jxd edit
#if 0
		if (s_Chipreset != RST_STATUS_SW_MAIN) {
		  	if((PowerOnSource == NvOdmPmuPowerKey || PowerOnSource == NvOdmPmuRTCAlarm || PowerOnSource == NvOdmUsbHotPlug)&&get_bqvbus())
	               MainChargingLoop(OSChrgInPercent, ExitChrgInPercent);
		}
#endif

#if 1	
	if ((BatteryChrgInPercent >= OSChrgInPercent)&&(BatteryVoltage >= 3620))
    {
		if ((s_Chipreset == RST_STATUS_SW_MAIN) ||(PowerOnSource == NvOdmPmuPowerKey))
		//{
			//if((PowerOnSource == NvOdmPmuPowerKey || PowerOnSource == NvOdmPmuRTCAlarm || PowerOnSource == NvOdmUsbHotPlug)&&get_bqvbus())
				//MainChargingLoop(OSChrgInPercent, ExitChrgInPercent);

				return BootToKernel();
		// }
    }
	else
	{	
		 // Display LOWBAT image
         NvAppNotifyOnDisplay(NvOdmImage_Lowbattery);
	}
	MainChargingLoop(OSChrgInPercent, ExitChrgInPercent);
#endif
	 if (!get_bqvbus()&&BatteryChrgInPercent < 2){
	 //if (get_bqvbus()&&BatteryChrgInPercent < 2){
		 MainChargingLoop(2, ExitChrgInPercent);
	 }
    return BootToKernel();

    
fail:
    return e;
}
#endif

static NvError StartFastbootProtocol(NvU32 RamBase)
{
    Nv3pTransportHandle hTrans = NULL;
    NvError e = NvSuccess;
    NvU8 *KernelImage = NULL;
    NvU32 LoadSize = 0;
    NvU32 ExitOption = FASTBOOT;
    NvOdmPmuDeviceHandle hPmu;

    // Configure USB driver to return the fastboot config descriptor
    FastbootSetUsbMode(NV_FALSE);
    Nv3pTransportSetChargingMode(NV_FALSE);
    e = Nv3pTransportOpenTimeOut(&hTrans, Nv3pTransportMode_default, 0,1000);

    FastbootStatus("Starting Fastboot USB download protocol\n");
    e = NvDdkKeyboardInit(&s_IsKeyboardInitialised);
    if ( (e != NvSuccess) || (s_IsKeyboardInitialised != NV_TRUE))
    {
        NvOsDebugPrintf("Keyboard driver initilization failed\n");
        goto fail;
    }

    while (ExitOption == FASTBOOT)
    {
        e = FastbootProtocol(hTrans, &KernelImage, &LoadSize, &ExitOption);
    }

    NvDdkKeyboardDeinit();
    if (hTrans)
    {
        Nv3pTransportClose(hTrans);
        hTrans = NULL;
    }

    switch (ExitOption)
    {
        case CONTINUE:
            FastbootStatus("Cold Booting Linux\n");
            NV_CHECK_ERROR_CLEANUP(
                LoadKernel(RamBase, KERNEL_PARTITON, NULL, 0));
            break;
        case REBOOT_BOOTLOADER:
            FastbootBit(FASTBOOT_BIT_SET);
            NvAbootReset(gs_hAboot);
            break;
        case RECOVERY_KERNEL:
            FastbootStatus("Booting Recovery kernel image\n");
            s_RecoveryKernel = NV_TRUE;
            NV_CHECK_ERROR_CLEANUP(
                LoadKernel(RamBase, RECOVERY_KERNEL_PARTITION, NULL, 0));
            break;
        case FORCED_RECOVERY:
            SetForceRecoveryBit();
            if (!s_FpgaEmulation)
               NvAbootReset(gs_hAboot);
            break;
        case REBOOT:
            NvAbootReset(gs_hAboot);
            break;
        case POWEROFF:
            if (NvOdmPmuDeviceOpen(&hPmu))
            {
                if (!NvOdmPmuPowerOff(hPmu))
                    NvOsDebugPrintf("Poweroff failed\n");
            }
            break;
        case BOOT:
            if (KernelImage)
            {
                FastbootStatus("Booting downloaded image\n");
                NV_CHECK_ERROR_CLEANUP(
                    LoadKernel(RamBase, NULL, KernelImage, LoadSize));
            }
            break;
    }

fail:
    if (hTrans)
        Nv3pTransportClose(hTrans);
    return e;
}

static NvError LoadKernel(NvU32 RamBase, const char *PartName,
        NvU8 *KernelImage, NvU32 LoadSize)
{
    NvError e = NvSuccess;

    if ((LoadSize == 0 || KernelImage == NULL) && PartName != NULL)
    {
        if (!HasValidKernelImage(&KernelImage, &LoadSize, PartName))
            goto fail;
    }

    if (KernelImage)
    {
        NvOsDebugPrintf("Platform Pre OS Boot configuration...\n");
        if (!NvOdmPlatformConfigure(NvOdmPlatformConfig_OsLoad))
        {
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(
            BootAndroid(KernelImage, LoadSize, RamBase)
        );
    }

fail:
    FastbootStatus(" Booting failed\n", PartName);
    NV_CHECK_ERROR_CLEANUP(StartFastbootProtocol(RamBase));
    return e;
}

/**
 * Gets the number of logical sectors of PT from BCT
 * @params NumLogicalSector Read back the number of logical sector from bct
 *
 * @return NvSuccess if NumLogicalSector read from BCT is succeesful
 */
static NvError GetPTNumLogicalSectors(NvU16 *NumLogicalSector)
{
    NvU32 BctLength = 0;
    NvBctHandle hBct = NULL;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvU8 *pBuffer = NULL;
    NvBctAuxInfo *pAuxInfo = NULL;
    NvError e = NvSuccess;

    if (!NumLogicalSector)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    // Gets the BCT length
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, NULL));

    // BCT Handle Init
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, &hBct));

    // Gets the size of Auxiliary Data
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_AuxDataAligned,
            &Size, &Instance, NULL)
    );

    pBuffer = NvOsAlloc(Size);
    if (!pBuffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(pBuffer, 0x0, Size);

    // Read Auxiliary Data from BCT
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_AuxDataAligned,
            &Size, &Instance, pBuffer)
    );

    pAuxInfo = (NvBctAuxInfo *)pBuffer;

    // Store the number of logical sectors
    *NumLogicalSector = pAuxInfo->NumLogicalSectors;

fail:
    if (pBuffer)
        NvOsFree(pBuffer);
    if (hBct)
        NvBctDeinit(hBct);
    return e;
}

NvError NvAppMain(int argc, char *argv[])
{
    NvError e = NvError_Success;
    NvU32 EnterRecoveryMode = 0;
    NvU32 SecondaryBootDevice = 0;
    NvU16 NumLogicalSectors = 0;
    NvBool PmicWdtDisable;
    NvBctAuxInfo *auxInfo = NULL;
    NvBool LimitedPowerMode = NV_FALSE;

    NvOsDebugPrintf("\n[bootloader] (version %s)\n", BUILD_NUMBER);
    EnterRecoveryMode = (NvU32)NvBlQueryGetNv3pServerFlag();

    BootLoader.MainStart = NvOsGetTimeUS();

    NvOsDebugPrintf("Platform Pre Boot configuration...\n");
    if (!NvOdmPlatformConfigure(NvOdmPlatformConfig_BlStart))
    {
        goto fail;
    }

    e = NvAbootOpen(&gs_hAboot, &EnterRecoveryMode, &SecondaryBootDevice);
    // Early development platform without the complete nvflash foodchain?
    if ((e == NvError_NotImplemented) || (e == NvError_NotSupported))
    {
        NvAbootPrivSanitizeUniverse(gs_hAboot);
        NvOsDebugPrintf("Load OS now via JTAG backdoor....\r\n");
        nvaosHaltCpu();
    }

    if (e != NvSuccess)
    {
        FastbootError("Failed to initialize Aboot\n");
        goto fail;
    }


    s_SecondaryBoot = (NvDdkBlockDevMgrDeviceId)SecondaryBootDevice;

#if ENABLE_NVDUMPER
    NvDumperReserved = CalcNvDumperReserved(gs_hAboot);
#endif /* ENABLE_NVDUMPER */

    if (EnterRecoveryMode)
    {
#if !defined(BUILD_FOR_COSIM) && !defined(BOOT_MINIMAL_BL)
        // Disable display for now
        auxInfo = NvOsAlloc(sizeof(NvBctAuxInfo));
        if (!auxInfo)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NvOsMemset(auxInfo, 0, sizeof(NvBctAuxInfo));
        if (AbootPrivGetOsInfo(auxInfo)== NvSuccess)
            LimitedPowerMode  = auxInfo->LimitedPowerMode;
        if (!LimitedPowerMode)
            NvAppInitDisplay();
#endif
        FastbootStatus("Entering NvFlash recovery mode / Nv3p Server\n");
        FastbootSetUsbMode(NV_TRUE);  // Configure USB driver for Nv3p mode
        NV_CHECK_ERROR_CLEANUP(NvAboot3pServer(gs_hAboot));

        NV_CHECK_ERROR_CLEANUP(
            GetPTNumLogicalSectors(&NumLogicalSectors));

        // MBR should not be written if PT info is not in BCT
        if (NumLogicalSectors)
            NV_CHECK_ERROR_CLEANUP(NvAbootRawWriteMBRPartition(gs_hAboot));
#ifdef NV_EMBEDDED_BUILD
        // Nv3pServer exits implies flashing is complete. So, stop here.
        // User should be forced to reset the board here
        FastbootStatus("NvFlash Completed\n");
        while(1);
#endif
    }

#ifndef NV_EMBEDDED_BUILD
    // FIXME: Write the MBR to the disk if the boot device is SDMMC.
    // There is a small race condition in which the user can potentially
    // turn off the system after nvflash completes and just before
    // the MBR is written. If this does happen, the system won't boot.
    if (s_SecondaryBoot == NvDdkBlockDevMgrDeviceId_Usb3 ||
        s_SecondaryBoot == NvDdkBlockDevMgrDeviceId_SDMMC)
        NV_CHECK_ERROR_CLEANUP(NvAbootRawWriteMBRPartition(gs_hAboot));
#endif

#if !defined(ANDROID) || defined(CONFIG_NONTZ_BL)
    if (EnterRecoveryMode)
    {
        // FIXME: There is race condition between writing of MBR/GPT to the
        // device and the following reboot. The 1sec delay is kept for the same.
        NvOsSleepMS(1000);
        NvOsDebugPrintf("Rebooting device after flashing.\n");
        NvAbootReset(gs_hAboot);
    }
#endif /* !defined(ANDROID) || defined(CONFIG_NONTZ_BL) */

#ifdef NV_USE_NCT
    /* Initialize NCK after nvflash */
    NvNctInitPart();
    NvNctInitNCKPart(gs_hAboot);
#endif

#ifdef LPM_BATTERY_CHARGING
    /* Charge battery if low */
    if (!EnterRecoveryMode) {
        NvAppChargeBatteryIfLow();
    }
#endif

#ifdef HAVE_LED
    NvOdmInitLed();
#endif

    if (NvOdmQueryGetPmicWdtDisableSetting(&PmicWdtDisable))
    {
        NvOdmPmuDeviceHandle hPmu;
        NvBool Status;
        Status = NvOdmPmuDeviceOpen(&hPmu);
        NV_ASSERT(Status);
        NV_ASSERT(hPmu);
        if (Status)
        {
            if (PmicWdtDisable)
                NvOdmPmuDisableWdt(hPmu);
            else
                NvOdmPmuEnableWdt(hPmu);
        }
    }

#if !defined(BUILD_FOR_COSIM) && !defined(BOOT_MINIMAL_BL)
    NvAppInitDisplay();  // Disable display for now
#endif

    NvAppFinalize();

fail:
    FastbootError("Unrecoverable bootloader error (0x%08x).\n", e);
    NV_ABOOT_BREAK();
}

#ifdef LP0_SUPPORTED
static void NvAppSuspendInit(void)
{
    NvError e = NvError_Success;
#ifdef CONFIG_TRUSTED_FOUNDATIONS
    extern NvU32 a1dff673f0bb86260b5e7fc18c0460ad859d66dc3;
    NvU32 *wbPtr;
    /* Patch LP0 warmboot code with CPU resume pointer */
    wbPtr = &a1dff673f0bb86260b5e7fc18c0460ad859d66dc3;
    wbPtr[0] = (NvU32)NvBlLp0CoreResume;
#endif
#ifndef ENABLE_NVTBOOT
    NV_CHECK_ERROR_CLEANUP(NvAbootInitializeCodeSegment(gs_hAboot));
#endif
    NV_CHECK_ERROR_CLEANUP(NvAbootPrivSaveSdramParams(gs_hAboot));
    return;
fail:
    NvOsDebugPrintf("%s failed. Error: (0x%08x).\n", __func__, e);
}

NvError NvAppSuspend(void)
{
    NvError e = NvError_Success;

#ifndef BUILD_FOR_COSIM
    NvAppShutDownDisplay();
#endif

    NV_CHECK_ERROR_CLEANUP(NvBlLp0StartSuspend(gs_hAboot));

fail:
    FastbootError("NvAppSuspend failed (0x%08x).\n", e);
    return e;
}

void NvAppResume(void)
{
#ifndef BUILD_FOR_COSIM // Disable display for now
    NvAppInitDisplay();
#endif

    FastbootStatus("Charge Battery, if Critical\n");

#ifdef LPM_BATTERY_CHARGING
          /* Charge battery if low */
          NvAppChargeBatteryIfLow();
#endif

    FastbootStatus("Booting Linux after BL LP0\n");

    NvAppFinalize();
}
#endif

#define KEY_UNUSE		0XFE

static void NvAppFinalize(void)
{
    NvError e;
    NvRmDeviceHandle hRm = NULL;
#if DO_JINGLE
    FastbootWaveHandle hWave;
#endif
    CommandType Command;
    NvU32 EnterFastboot = 0;
    NvU32 EnterRecoveryKernel = 0;
    NvU32 keyEvent = KEY_IGNORE;
    NvU32 RamSize;
    NvU32 RamBase;
    NvBool WaitForBootInput = NV_TRUE;
    NvU32 OdmData = 0;
#ifdef NV_USE_CRASH_COUNTER
    NvU32 crashCounter = 0;
#endif
#ifdef NV_USE_NCT
    nct_item_type buf;
    NvError err;
#endif

    /* Get the physical base address of SDRAM. */
    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));
    NvRmModuleGetBaseAddress(hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ExternalMemory, 0),
        &RamBase, &RamSize);

    /* Increase the CPU clocks before booting to OS */
#if NVODM_CPU_BOOT_PERF_SET
    NvRmPrivSetMaxCpuClock(hRm);
#endif

#ifndef BOOT_MINIMAL_BL
    /* Initilaize the BL message string based on platform */
    InitBlMessage();
#endif

#ifndef BUILD_FOR_COSIM // Disable TSEC for now
#if TSEC_EXISTS
    if (NvInitOtfKeyGeneration() != NvSuccess)
    {
        e = NvError_NotSupported;
        FastbootStatus("OTF key generation initialization failed\n");
        goto fail;
    }
#endif
#endif

#ifdef NV_USE_CRASH_COUNTER
    // If we exceed MAX boot crashes:
    //   1) force a boot into recovery mode,
    //   2) reset the crash counter to zero.
    // If we are less than MAX, increment the crash counter.
    // Crash counter will be reset to zero after a clean boot.
    e = getCrashCounter(&crashCounter);
    if ((e == NvSuccess) && (crashCounter < MAX_GTV_CRASH_COUNTER))
    {
        setCrashCounter(++crashCounter);
    }
    else
    {
        forceRecoveryBoot(gs_hAboot);
        setCrashCounter(0);
        s_RecoveryKernel = NV_TRUE;
        NV_CHECK_ERROR_CLEANUP(LoadKernel(RamBase,
            RECOVERY_KERNEL_PARTITION, NULL, 0));
    }
#endif

    /* Read bit for fastboot */
    EnterFastboot = FastbootBit(FASTBOOT_BIT_READ);
    /* Read bit for recovery */
    EnterRecoveryKernel = RCKBit(RCK_BIT_READ);

    //Read the key event
    if (NvDdkKeyboardInit(&s_IsKeyboardInitialised) == NvSuccess)
    {
    	NvOdmPmuDeviceHandle temp_pmu;
		NvU32 state;
        keyEvent = NvDdkKeyboardGetKeyEvent(&s_KeyFlag);
		state = get_power_key();
		//NvOsDebugPrintf("get keyEvent value:%d , flag = %d,state = %d , EnterFastboot:%d , EnterRecoveryKernel:%d \n,",keyEvent,s_KeyFlag,state,EnterFastboot,EnterRecoveryKernel);

		int vbus_state = get_bqvbus();//vbus_state:0  usb unplug, 1 usb plug
		if((RebootSource != NvOdmRebootRebootKey) && (Reason == 1))
		//if((RebootSource != NvOdmRebootRebootKey)&&(RebootSource != NvOdmRebootAdbReboot))
		{
			//if (keyEvent == KEY_IGNORE && vbus_state ==0 && s_Chipreset != RST_STATUS_SW_MAIN && state ==1){
			if (keyEvent == KEY_IGNORE && s_Chipreset != RST_STATUS_SW_MAIN && state ==1){
				NvOdmPmuDeviceOpen(&temp_pmu);
				NvOdmPmuPowerOff(temp_pmu);
			}
		}
        NvDdkKeyboardDeinit();

    }
    if (EnterFastboot && EnterRecoveryKernel)
    {
        /* Clearing fastboot and RCK flag bits */
        FastbootBit(FASTBOOT_BIT_CLEAR);
        RCKBit(RCK_BIT_CLEAR);
        FastbootStatus("Invalid boot configuration \n");
    }
    else if (EnterFastboot)
    {
        FastbootBit(FASTBOOT_BIT_CLEAR);
        NvOsDebugPrintf("Entering Fastboot based on booting mode configuration \n");

        NV_CHECK_ERROR_CLEANUP(
            StartFastbootProtocol(RamBase));
    }
    else if (((keyEvent == KEY_HOLD) || (keyEvent == KEY_DOWN)) &&
        (s_KeyFlag == KEY_PRESS_FLAG))
    {
        NvOsDebugPrintf("Entering Fastboot based on Key press \n");

        NV_CHECK_ERROR_CLEANUP(
            StartFastbootProtocol(RamBase));
    }
    else if (EnterRecoveryKernel)
    {
        RCKBit(RCK_BIT_CLEAR);
        NvOsDebugPrintf("Entering RCK based on booting mode configuration \n");

#ifdef NV_USE_NCT
        /* Clear Factory mode flag */
        err = NvNctReadItem(NCT_ID_FACTORY_MODE, &buf);
        if (NvSuccess == err && buf.factory_mode.flag)
        {
            buf.factory_mode.flag = 0;
            NvNctWriteItem(NCT_ID_FACTORY_MODE, &buf);
        }
#endif
        s_RecoveryKernel = NV_TRUE;
        NV_CHECK_ERROR_CLEANUP(
            LoadKernel(RamBase, RECOVERY_KERNEL_PARTITION, NULL, 0));
    }
    NvRmClose(hRm);

#ifndef  BOOT_MINIMAL_BL
    NvOsDebugPrintf("Checking for android ota recovery \n");
    e = NvCheckForUpdateAndRecovery(gs_hAboot, &Command);
    if (e == NvSuccess && Command == CommandType_Recovery)
    {
        FastbootStatus("Booting Recovery kernel image\n");
        s_RecoveryKernel = NV_TRUE;
        NV_CHECK_ERROR_CLEANUP(LoadKernel(RamBase,
                               RECOVERY_KERNEL_PARTITION, NULL, 0));
    }
#endif

#if DO_JINGLE
    if (FastbootPlayWave(TwoHundredHzSquareWave,
                         JINGLE_SAMPLERATE, (JINGLE_CHANNELS==2) ? NV_TRUE : NV_FALSE,
                         NV_TRUE, JINGLE_BITSPERSAMPLE, &hWave) == NvSuccess)
    {
        NvOsSleepMS(400);
        FastbootCloseWave(hWave);
    }
#endif

    OdmData = nvaosReadPmcScratch();
    //if (OdmData & (1 << NvOdmQueryBlWaitBit()))
        WaitForBootInput = NV_FALSE;
    if (WaitForBootInput == NV_TRUE)
    {
        /* Display Board information */
        DisplayBoardInfo();
        NV_CHECK_ERROR_CLEANUP(
            MainBootloaderMenu(RamBase, WAIT_IN_BOOTLOADER_MENU));
    }
    else
    {
        BitmapFile *bmf = NULL;
        NvError err = NvSuccess;

        err = FastbootBMPRead(gs_hAboot, &bmf, (const char *)BITMAP_PARTITON);
        if (err == NvSuccess)
            err = FastbootDrawBMP(bmf, &s_Frontbuffer, &s_FrontbufferPixels);
        if (err != NvSuccess)
            NvOsDebugPrintf("Could not render BMP on screen\n");
        NvOsDebugPrintf("Cold-booting Linux\n");
        NV_CHECK_ERROR_CLEANUP(
            LoadKernel(RamBase,KERNEL_PARTITON, NULL, 0));
    }

fail:
    FastbootError("Unrecoverable bootloader error (0x%08x).\n", e);
    NV_ABOOT_BREAK();
}

static NvError MainBootloaderMenu(NvU32 RamBase, NvU32 TimeOut)
{
    NvError e = NvSuccess;
    NvU32 MenuOption = CONTINUE;
    NvOdmPmuDeviceHandle hPmu;
    NvU32 OpMode;
    NvU32 Size;
    NvBool IsOdmSecure = NV_FALSE;
#if SHOW_IMAGE_MENU
    RadioMenu *BootMenu = NULL;
#else
    TextMenu *BootMenu = NULL;
#endif

#if !defined(BUILD_FOR_COSIM) && !defined(BOOT_MINIMAL_BL)

    /* Initialise the input keys*/
    e = NvDdkKeyboardInit(&s_IsKeyboardInitialised);
    if ( (e != NvSuccess) || (s_IsKeyboardInitialised != NV_TRUE))
    {
        NvOsDebugPrintf("Keyboard driver initilization failed\n");
        goto fail;
    }
    ShowPlatformBlMsg();

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                  (void *)&OpMode, (NvU32 *)&Size));

    if ((OpMode == NvDdkFuseOperatingMode_OdmProductionSecure) ||
        (OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC))
        IsOdmSecure = NV_TRUE;

    Size = NV_ARRAY_SIZE(BootloaderMenuString);
    if (IsOdmSecure == NV_TRUE)
       Size = Size - 1;

    /* only run the menu if there is a selection that can be made */
    if (TimeOut != 0)
    {
#if SHOW_IMAGE_MENU
        BootMenu = FastbootCreateRadioMenu(
            NV_ARRAY_SIZE(s_RadioImages), MenuOption, 3, NVIDIA_GREEN,
                s_RadioImages);
        MenuOption = BootloaderMenu(BootMenu, TimeOut);
#else
        BootMenu = FastbootCreateTextMenu(
            Size, MenuOption, BootloaderMenuString);
        MenuOption = BootloaderTextMenu(BootMenu, TimeOut);
#endif
    }

    NvDdkKeyboardDeinit();
#endif
    switch (MenuOption)
    {
        case REBOOT_BOOTLOADER:
            NV_CHECK_ERROR_CLEANUP(StartFastbootProtocol(RamBase));
            break;

        case CONTINUE:
            FastbootStatus("Cold-booting Linux\n");
            NV_CHECK_ERROR_CLEANUP(
                LoadKernel(RamBase,KERNEL_PARTITON, NULL, 0));
            break;

        case RECOVERY_KERNEL:
            FastbootStatus("Booting Recovery kernel image\n");
            s_RecoveryKernel = NV_TRUE;
            NV_CHECK_ERROR_CLEANUP(
                LoadKernel(RamBase, RECOVERY_KERNEL_PARTITION, NULL, 0));
            break;

        case FORCED_RECOVERY:
            SetForceRecoveryBit();
            if (!s_FpgaEmulation)
               NvAbootReset(gs_hAboot);
        case REBOOT:
            NvAbootReset(gs_hAboot);
            break;
        case POWEROFF:
           if (NvOdmPmuDeviceOpen(&hPmu))
            {
              if (!NvOdmPmuPowerOff(hPmu))
                 NvOsDebugPrintf("Poweroff failed\n");
            }
            break;
        default:
            FastbootError("Unsupported menu option selected: %d\n",
                MenuOption);
            e = NvError_NotSupported;
            goto fail;
    }
fail:
    NvDdkKeyboardDeinit();
    return e;
}

static NvBool HasValidDtbImage(void **Image, NvU32 *Size, const char *PartName)
{
    NvError e = NvError_Success;
    void *DtbImage = NULL;
    NvU32 DtbSize = 0;
    struct fdt_header hdr;
    int err;

    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(gs_hAboot, PartName, 0, sizeof(struct fdt_header),
            (NvU8*)&hdr)
    );

    err = fdt_check_header(&hdr);
    if (err)
    {
        NvOsDebugPrintf("%s: Invalid fdt header. err: %s\n", __func__, fdt_strerror(err));
        goto fail;
    }

    DtbSize = fdt_totalsize(&hdr);

    /* Reserve space for nodes which will be added later in BL */
    DtbImage = NvOsAlloc(DtbSize + FDT_SIZE_BL_DT_NODES);
    if (!DtbImage)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(gs_hAboot, PartName, 0, DtbSize, (NvU8 *)DtbImage)
    );

    if (!DtbImage || !DtbSize)
        goto fail;

    *Image = fdt_hdr = (struct fdt_header *)DtbImage;
    *Size = fdt_totalsize(fdt_hdr);
    return NV_TRUE;

fail:
    if (DtbImage)
        NvOsFree(DtbImage);
    return NV_FALSE;
}

static NvBool HasValidKernelImage(NvU8 **Image, NvU32 *Size, const char *PartName)
{
    NvError e = NvSuccess;
    NvU8 *KernelImage = NULL;
    NvU32 LoadSize = 0;
    NvU32 KernelPages = 0;
    NvU32 RamdiskPages = 0;
    AndroidBootImg hdr;
    NvU32 TempAddr;
    NvRmDeviceHandle hRm;
    NvRmMemHandle hRmMemHandle = NULL;

    /* Read  header */
    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(gs_hAboot, PartName, 0, sizeof(AndroidBootImg),
            (NvU8*)&hdr)
    );

    if (NvOsMemcmp(hdr.magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE))
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    /* LoadSize is sum of size of header, kernel, ramdisk and second stage
    * loader. */
    KernelPages = (hdr.kernel_size + hdr.page_size - 1) / hdr.page_size;
    RamdiskPages = (hdr.ramdisk_size + hdr.page_size - 1) / hdr.page_size;
    LoadSize = hdr.page_size * (1 + KernelPages + RamdiskPages) +
        hdr.second_size;
    BootLoader.Kernel_Size = LoadSize;
    if (!LoadSize)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    // allocate writecombined buffer and align it required size
    NvRmOpen(&hRm, 0);
    NvRmMemHandleAlloc(hRm, NULL, 0, ALIGNMENT_SIZE, NvOsMemAttribute_WriteCombined,
                              LoadSize + ALIGNMENT_SIZE - 1, 0, 1, &hRmMemHandle);
    TempAddr = (NvU32)NvRmMemPin(hRmMemHandle);
    TempAddr = (TempAddr + ALIGNMENT_SIZE - 1) & (~(ALIGNMENT_SIZE - 1));
    KernelImage = (NvU8*)TempAddr;

    if (!KernelImage)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    BootLoader.Start_KernelRead = NvOsGetTimeMS();
    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(gs_hAboot, PartName, 0, LoadSize, KernelImage)
    );
    BootLoader.End_KernelRead = NvOsGetTimeMS();

fail:
    if (e == NvSuccess)
    {
        if (Image)
        {
            if (*Image)
                NvOsFree(*Image);
            *Size = LoadSize;
            *Image = KernelImage;
            NvRmClose(hRm);
            return NV_TRUE;
        }
    }

    if(KernelImage)
    {
        NvRmMemUnpin(hRmMemHandle);
        NvRmMemHandleFree(hRmMemHandle);
        NvRmClose(hRm);
    }

    return (e==NvSuccess) ? NV_TRUE : NV_FALSE;
}

/* 4 images, plus 1 used by TF code */
#define NB_IMAGES 5

static NvError BootAndroid(void *KernelImage, NvU32 ImageSize, NvU32 RamBase)
{
    AndroidBootImg* hdr = (AndroidBootImg *)KernelImage;
    const char *AndroidMagic = "ANDROID!";
    unsigned KernelPages;
    unsigned RamdiskPages;
    const NvU8 *Platform;
    void    *SrcImages[NB_IMAGES];
    NvUPtr   DstImages[NB_IMAGES];
    NvU32    Sizes[NB_IMAGES];
    NvU32    Registers[3];
    NvU32    Cnt;
    NvUPtr BootAddr    = RamBase + ZIMAGE_START_ADDR;
    /* Approximately 32 MB free memory for kernel decompression.
     * If the size of uncompressed and compressed kernel combined grows
     * beyond this size, ramdisk corruption will occur!
     */
    const NvUPtr RamdiskAddr = BootAddr + AOS_RESERVE_FOR_KERNEL_IMAGE_SIZE;
    void     *DtbImage = NULL;
    NvU32    DtbSize           = 0;
    NvU32    newlen;
    int      err;
    NvError  NvErr = NvError_Success;
#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    TFSW_PARAMS nTFSWParams;
#endif
    const    NvUPtr TagsAddr    = RamBase + 0x100UL;
    const    NvUPtr DTBAddr     = RamdiskAddr + RAMDISK_MAX_SIZE;
    NvU32    i;

    if (NvOsMemcmp(hdr->magic, AndroidMagic, NvOsStrlen(AndroidMagic)))
        FastbootError("Magic value mismatch: %c%c%c%c%c%c%c%c\n",
            hdr->magic[0], hdr->magic[1], hdr->magic[2], hdr->magic[3],
            hdr->magic[4], hdr->magic[5], hdr->magic[6], hdr->magic[7]);

    KernelPages = (hdr->kernel_size + hdr->page_size - 1) / hdr->page_size;
    RamdiskPages = (hdr->ramdisk_size + hdr->page_size - 1) / hdr->page_size;

    SrcImages[0] = (NvU8 *)KernelImage + hdr->page_size;
    DstImages[0] = BootAddr;

    /* FIXME:  With large RAM disks, the kernel refuses to boot unless the
     * kernel and RAM disk are located contiguously in memory.  I have no
     * idea why this is the case.
     */
    Sizes[0]     = KernelPages * hdr->page_size;

    if (HasValidDtbImage(&DtbImage, &DtbSize, "DTB"))
    {
        SrcImages[1] = (NvU8 *)DtbImage;
        DstImages[1] = DTBAddr;
        Sizes[1] = DtbSize;
        Cnt = 2;
    }
    else
    {
        SrcImages[1] = (NvU8 *)s_BootTags;
        DstImages[1] = TagsAddr;
        Sizes[1]     = sizeof(s_BootTags);
        Cnt = 2;
    }

    if (hdr->ramdisk_size > RAMDISK_MAX_SIZE)
    {
        FastbootError("Ramdisk size is more than expected(%d MB)\n",(RAMDISK_MAX_SIZE>>20));
        return NvError_NotSupported;
    }

    if (hdr->ramdisk_size)
    {
        SrcImages[Cnt] = (NvU8 *)KernelImage + hdr->page_size*(1+KernelPages);
        DstImages[Cnt] = RamdiskAddr;
        Sizes[Cnt]     = RamdiskPages * hdr->page_size;
        Cnt++;
    }
    if (hdr->second_size)
    {
        SrcImages[Cnt] = (NvU8 *)KernelImage +
            hdr->page_size*(1+KernelPages + RamdiskPages);
        DstImages[Cnt] = (NvUPtr)hdr->second_addr;
        Sizes[Cnt]     = hdr->second_size;
        Cnt++;
    }

    /* create additional space for the new command line and nodes */
    if (DtbImage)
    {
        newlen = fdt_totalsize((void *)DtbImage) + FDT_SIZE_BL_DT_NODES;
        err = fdt_open_into((void *)DtbImage, (void *)DtbImage, newlen);
        if (err < 0)
        {
            NvOsDebugPrintf("FDT: fdt_open_into fail (%s)\n", fdt_strerror(err));
            return NvError_ResourceError;
        }
    }

    Registers[0] = 0;
    Platform = NvOdmQueryPlatform();

    if (DtbImage)
    {
        Registers[1] = MACHINE_TYPE_TEGRA_FDT;
        Sizes[1] = fdt_totalsize((void *)DtbImage);
    }
    else
    {
        Registers[1] = MACHINE_TYPE_TEGRA_GENERIC;

        for (i = 0; i < NV_ARRAY_SIZE(machine_types); ++i)
        {
            if (!NvOsMemcmp(Platform, machine_types[i].name,
                            NvOsStrlen(machine_types[i].name)))
            {
                Registers[1] = machine_types[i].id;
                break;
            }
        }
    }

    if (DtbImage)
        Registers[2] = DTBAddr;
    else
        Registers[2] = TagsAddr;

#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    /* setup bootparams and patch warmboot code */
    NvOsMemset(&nTFSWParams, 0, sizeof(TFSW_PARAMS));
    nTFSWParams.ColdBootAddr = BootAddr;
    NvAbootTFPrepareBootParams(gs_hAboot, &nTFSWParams, Registers);
    BootAddr = (NvUPtr)&nTFSWParams;

    SrcImages[Cnt] = (NvU8 *)nTFSWParams.pTFAddr;
    DstImages[Cnt] = nTFSWParams.TFStartAddr;
    Sizes[Cnt]     = nTFSWParams.TFSize;
    Cnt++;
#endif

    NvErr = SetupTags(hdr, RamdiskAddr);
    if (NvErr != NvError_Success)
        return NvErr;

    NvAbootCopyAndJump(gs_hAboot, DstImages, SrcImages, Sizes,
        Cnt, BootAddr, Registers, 3);
    FastbootError("Critical failure: Unable to start kernel.\n");

    return NvError_NotSupported;
}

static const PartitionNameMap *MapFastbootToNvPartition(const char* name)
{
    NvU32 i;
    NvU32 NumPartitions;

    NumPartitions = NV_ARRAY_SIZE(gs_PartitionTable);

    for (i=0; i<NumPartitions; i++)
    {
        if (NvOsStrlen(name) !=
            NvOsStrlen(gs_PartitionTable[i].LinuxPartName))
            continue;
        if (!NvOsMemcmp(gs_PartitionTable[i].LinuxPartName, name,
                 NvOsStrlen(gs_PartitionTable[i].LinuxPartName)))
            return &gs_PartitionTable[i];
    }

    return NULL;
}

static NvError SetupTags(
    const AndroidBootImg *hdr,
    NvUPtr RamdiskPa)
{
    NvU32 CoreTag[3];
#ifdef NV_EMBEDDED_BUILD
    NvU32 SkuRev = 0;
#endif
    NvU32 NumPartitions;
    NvError NvErr = NvError_Success;

    /* Same as defaults from kernel/arch/arm/setup.c */
    CoreTag[0] = 1;
    CoreTag[1] = 4096;
    CoreTag[2] = 0xff;

    FastbootAddAtag(ATAG_CORE, sizeof(CoreTag), CoreTag);

#ifdef NV_EMBEDDED_BUILD
    NvBootGetSkuNumber(&SkuRev);
    FastbootAddAtag(ATAG_REVISION, sizeof(SkuRev), &SkuRev);
#endif
    if (hdr->name[0])
        FastbootStatus("%s\n", hdr->name);
    if (fdt_hdr)
    {
        int node, err;

        node = fdt_path_offset((void *)fdt_hdr, "/chosen");
        if (node < 0)
        {
            NvOsDebugPrintf("%s: Unable to find /chosen (%s)\n", __func__,
                fdt_strerror(node));
            NvErr = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "linux,initrd-start", RamdiskPa);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set initrd-start (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "linux,initrd-end", RamdiskPa + hdr->ramdisk_size);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set initrd-end (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }
    }
    else
    {
        NvU32 RdTags[2];
        RdTags[0] = RamdiskPa;
        RdTags[1] = hdr->ramdisk_size;
        FastbootAddAtag(ATAG_INITRD2, sizeof(RdTags), RdTags);
    }

#ifndef BUILD_FOR_COSIM
#if TSEC_EXISTS
    if (NvFinalizeOtfKeyGeneration(&g_OtfKey) != NvSuccess)
    {
        NvOsDebugPrintf("OTF Key generation failed\n");
        NV_ASSERT(0);
    }
#endif
#ifndef ENABLE_NVTBOOT
    NvErr = NvAbootInitializeCodeSegment(gs_hAboot);
    if (NvErr != NvError_Success)
        goto fail;
#endif
#ifndef BOOT_MINIMAL_BL
    NvAbootDestroyPlainLP0ExitCode();

    /* Add serial tag for the board serial number */
    NvErr = AddSerialBoardInfoTag();
    if (NvErr != NvError_Success)
        goto fail;
#endif
#endif

    NumPartitions = NV_ARRAY_SIZE(gs_PartitionTable);

    NvErr = FastbootCommandLine(gs_hAboot, gs_PartitionTable,
        NumPartitions, s_SecondaryBoot, hdr->cmdline, (hdr->ramdisk_size>0),
        NvDumperReserved, (const unsigned char *)hdr->name, s_RecoveryKernel,BootToCharging);
    if (NvErr != NvError_Success)
            goto fail;

    FastbootAddAtag(0,0,NULL);

fail:
    return NvErr;
}

#define MENU_OPTION_HIGHLIGHT_LEVEL 0xC0

static void ShowPlatformBlMsg()
{
    if (s_IsKeyboardInitialised != NV_TRUE)
        FastbootStatus(s_PlatformBlMsg[MSG_NO_KEY_DRIVER_INDEX]);
    else
        FastbootStatus(s_PlatformBlMsg[MSG_FASTBOOT_MENU_SEL_INDEX]);
    s_PrintCtx.y += DISP_FONT_HEIGHT * BLANK_LINES_BEFORE_MENU;
}

static NvU32 BootloaderTextMenu(
    TextMenu *Menu,
    NvU32 Timeout)
{
    NvU64 TimeoutInUs = Timeout * 1000;
    NvU64 StartTime;
    NvBool IgnoreTimeout = NV_FALSE;
    NvU32 keyEvent = KEY_IGNORE;
    NvU32 X = 0;
    NvU32 Y = 0;
    static NvU64 StartKeyPressTime = 0;

    if (s_IsKeyboardInitialised != NV_TRUE)
        return 0;

    StartTime = NvOsGetTimeUS();
    Y = s_PrintCtx.y;
    FastbootDrawTextMenu(Menu, X, Y);
    if (Timeout == NV_WAIT_INFINITE)
        IgnoreTimeout = NV_TRUE;

    while (IgnoreTimeout || (TimeoutInUs > (NvOsGetTimeUS() - StartTime)))
    {
        keyEvent = NvDdkKeyboardGetKeyEvent(&s_KeyFlag);
        if (keyEvent == KEY_HOLD)
        {
            HandleHoldKeyPress(&keyEvent, &StartKeyPressTime);
        }
        else
        {
            if (s_DdkKbcCntxt.HoldKeyPressed)
            {
                HandleHoldKeyPress(&keyEvent, &StartKeyPressTime);
            }
            if (s_KeyFlag != KEY_PRESS_FLAG)
            {
                keyEvent = KEY_IGNORE;
            }
        }

        switch (keyEvent)
        {
            case KEY_UP:
                FastbootTextMenuSelect(Menu, NV_FALSE);
                FastbootDrawTextMenu(Menu, X, Y);
                IgnoreTimeout = NV_TRUE;
                break;
            case KEY_DOWN:
                FastbootTextMenuSelect(Menu, NV_TRUE);
                FastbootDrawTextMenu(Menu, X, Y);
                IgnoreTimeout = NV_TRUE;
                break;
            case KEY_ENTER:
                goto done;
        }
    }

done:
    s_PrintCtx.y += DISP_FONT_HEIGHT *
        (Menu->NumOptions + BLANK_LINES_AFTER_MENU);

    if (!Timeout)
        return 0;
    return (int)(Menu->CurrOption);
}

#if SHOW_IMAGE_MENU
static int BootloaderMenu(RadioMenu *Menu, NvU32 Timeout)
{
    NvBool IgnoreTimeout = NV_FALSE;
    NvU32 keyEvent = KEY_IGNORE;
    NvU64 TimeoutInUs = Timeout * 1000;
    NvU64 StartTime = NvOsGetTimeUS();
    static NvU64 StartKeyPressTime = 0;

    if (s_IsKeyboardInitialised != NV_TRUE)
        return 0;

    FastbootDrawRadioMenu(&s_Frontbuffer, s_FrontbufferPixels, Menu);
    //Please do not remove the below print used for tracking boot time
    NvOsDebugPrintf("Logo Displayed at:%d ms\n", NvOsGetTimeMS());
    if (Timeout != NV_WAIT_INFINITE)
        FastbootStatus("OS will cold boot in %u seconds if no input "
            "is detected\n", Timeout/1000);
    else
        IgnoreTimeout = NV_TRUE;

    // To get highlighted look.
    Menu->PulseAnim = MENU_OPTION_HIGHLIGHT_LEVEL;
    FastbootDrawRadioMenu(&s_Frontbuffer, s_FrontbufferPixels, Menu);

    while (IgnoreTimeout || (TimeoutInUs > (NvOsGetTimeUS() - StartTime)))
    {
        keyEvent = NvDdkKeyboardGetKeyEvent(&s_KeyFlag);
        if (keyEvent == KEY_HOLD)
        {
            HandleHoldKeyPress(&keyEvent, &StartKeyPressTime);
        }
        else
        {
            if (s_DdkKbcCntxt.HoldKeyPressed)
            {
                HandleHoldKeyPress(&keyEvent, &StartKeyPressTime);
            }
            if (s_KeyFlag != KEY_PRESS_FLAG)
            {
                keyEvent = KEY_IGNORE;
            }
        }

        switch (keyEvent)
        {
            case KEY_UP:
                FastbootRadioMenuSelect(Menu, NV_FALSE);
                IgnoreTimeout = NV_TRUE;
                break;
            case KEY_DOWN:
                FastbootRadioMenuSelect(Menu, NV_TRUE);
                IgnoreTimeout = NV_TRUE;
                break;
            case KEY_ENTER:
                goto fail;
        }
        FastbootDrawRadioMenu(&s_Frontbuffer, s_FrontbufferPixels, Menu);
    }

fail:
    if (!Timeout)
        return 0;
    return (int)Menu->CurrOption;
}
#endif

static void DisplayBoardInfo(void)
{
    NvU32 OdmData;
    char  buff[MAX_SERIALNO_LEN] = {0};
    NvBool warrantyFuse = NV_FALSE;
    NvError e;

    FastbootStatus("\n[bootloader] (version %s)\n", BUILD_NUMBER);

    s_IsUnlocked = NV_FALSE;
    OdmData = nvaosReadPmcScratch();
    if (!(OdmData & (1 << NvOdmQueryBlUnlockBit())))
        s_IsUnlocked = NV_TRUE;
    if (s_IsUnlocked == NV_TRUE)
        FastbootStatus("Device - Unlocked \n");
    else
        FastbootStatus("Device - locked \n");

    e = GetWarrantyFuse(&warrantyFuse);
    if (e != NvError_Success)
        FastbootStatus("Could not retrieve warranty status\n");
    else if (warrantyFuse)
        FastbootError("WARRANTY VOID DUE TO UNLOCKING\n");

    FastbootStatus("\n");

    e = FastbootGetSerialNo(buff, MAX_SERIALNO_LEN);
    if (e == NvSuccess)
        FastbootStatus("Serial Number  %s \n", buff);
}
