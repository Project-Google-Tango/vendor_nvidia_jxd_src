/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* linux_cmdline.c -
 *
 *  This file implements all of the Linux-specific command line and ATAG
 *  structure generation, for passing data to the operating system.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nverror.h"
#include "fastboot.h"
#include "nvaboot.h"
#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvboot_bit.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_fuse.h"
#include "nvassert.h"
#include "nvpartmgr.h"
#include "nvsku.h"
#include "nvodm_query_discovery.h"
#include "nvbct.h"
#include "nvrm_structure.h"
#include "nvsystem_utils.h"
#include "libfdt/libfdt.h"
#if TSEC_EXISTS
#include "tsec_otf_keygen.h"
#endif
#include "armc.h"
#ifdef NV_USE_NCT
#include "nct.h"
#endif
#include <string.h>

#define ATAG_CMDLINE      0x54410009
#define ATAG_SERIAL       0x54410006
#define COMMAND_LINE_SIZE 1024
#define GPT_BLOCK_SIZE    512
//  up to 2K of ATAG data, including the command line will be passed to the OS
NvU32 s_BootTags[BOOT_TAG_SIZE];
char  s_CmdLine[COMMAND_LINE_SIZE];

int nOrigCmdLen, nIgnoreFastBoot = 0;
static const char * const szIgnFBCmd = IGNORE_FASTBOOT_CMD;

extern NvU32 g_wb0_address;
extern NvU32 surf_size;
extern NvU32 surf_address;
extern struct fdt_header *fdt_hdr;

extern NvUPtr nvaos_GetMemoryStart( void );

#if TSEC_EXISTS
extern NvOtfEncryptedKey g_OtfKey;

extern void LockTsecAperture(NvAbootHandle hAboot);
#endif

#if VPR_EXISTS
extern void LockVprAperture(NvAbootHandle hAboot);
#endif

extern NvU64 NvAbootPrivGetDramRankSize(
            NvAbootHandle hAboot, NvU64 TotalRamSize, NvU32 RankNo);

static NvU32 GetPartitionByType(NvU32 PartitionType)
{
    NvU32 PartitionId = 0;
    NvError e=NvSuccess;
    NvPartInfo PartitionInfo;
    while(1)
    {
        PartitionId = NvPartMgrGetNextId(PartitionId);
        if(PartitionId == 0)
            return PartitionId;
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &PartitionInfo));
        if(PartitionInfo.PartitionType == PartitionType)
            break;
    }
    return PartitionId;
fail:
    return 0;
}

static NvU32 IsSpace(NvU32 c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static int CheckForIgnoreFastBootCmd(const char * szCmd, int nLen, int nStart, int nEnd)
{
    if ('"' == s_CmdLine[nStart]) /* quoted */
    {
        if (nLen == nEnd - nStart - 2 &&
            '"' == s_CmdLine[nEnd - 1])
            nIgnoreFastBoot = !NvOsMemcmp(&s_CmdLine[nStart + 1], szCmd, nLen);
    }
    else /* or not */
    {
        if (nLen == nEnd - nStart)
            nIgnoreFastBoot = !NvOsMemcmp(&s_CmdLine[nStart], szCmd, nLen);
    }

    return nIgnoreFastBoot;
}

static int InitCmdList(const unsigned char * CommandLine)
{
    int i, nNext, nMood = 0, nQuote = 0, nStart = 0;
    unsigned char CurCh;
    int IgnFBCmdLen = NvOsStrlen(szIgnFBCmd);

    for (i = nNext = 0; i < ANDROID_BOOT_CMDLINE_SIZE; i++)
    {
        CurCh = CommandLine[i];
        if (0 == nMood) /* skip while space */
        {
            if (!IsSpace(CurCh))
            {
                if (CurCh)
                {
                    s_CmdLine[nStart = nNext++] = CurCh;
                    if ('"' == CurCh) /* quote */
                        nQuote = !0;
                    nMood = 1;
                    continue;
                }
                else
                    break;
            }
        }
        else if (1 == nMood) /* a command option */
        {
            if (IsSpace(CurCh))
            {
                if (nQuote) /* quoted -> space does not count as a separator */
                    s_CmdLine[nNext++] = CurCh;
                else /* otherwise, end of a command */
                {
                    if (!nIgnoreFastBoot) /* check for fastboot ignore command */
                    {
                        if (CheckForIgnoreFastBootCmd(szIgnFBCmd, IgnFBCmdLen, nStart, nNext))
                            nNext = nStart; /* and do not include it in final command line */
                    }
                    if (!nIgnoreFastBoot)
                        s_CmdLine[nNext++] = ' ';
                    nMood = 0;
                }
                continue;
            }
            else if ('"' == CurCh) /* quote character */
                s_CmdLine[nNext++] = CurCh, nQuote = !nQuote;
            else
            {
                if (CurCh) /* non-space, non-quote, non-null */
                {
                    s_CmdLine[nNext++] = CurCh;
                    continue;
                }
                else /* null */
                {
                    if (!nIgnoreFastBoot) /* check for fastboot ignore command */
                    {
                        if (CheckForIgnoreFastBootCmd(szIgnFBCmd, IgnFBCmdLen, nStart, nNext))
                            nNext = nStart; /* and do not include it in final command line */
                    }
                    break;
                }
            }
        }
    }

    if (nNext)
    {
        /* if quoted, complete it */
        if (nQuote)
            s_CmdLine[nNext++] = '"';

        /* check for fastboot ignore command */
        if (!nIgnoreFastBoot)
        {
            if (CheckForIgnoreFastBootCmd(szIgnFBCmd, IgnFBCmdLen, nStart, nNext))
                nNext = nStart; /* and do not include it in final command line */
        }

        /* add an space at the end, if necessary */
        if (' ' != s_CmdLine[nNext - 1])
            s_CmdLine[nNext++] = ' ';
    }

    s_CmdLine[nNext] = 0; /* The END */

    return nOrigCmdLen = nNext;
}

static int DoesCommandExist(const char *Cmd)
{
    int i, nMood = 0, nQuote = 0;
    char CurCh;
    const char *p = Cmd;

    /* ignore fastboot */
    if (nIgnoreFastBoot) return !0;
    /* nothing to find */
    if (!p || !*p) return 0;

    for (i = 0; i < nOrigCmdLen; i++)
    {
        CurCh = s_CmdLine[i];
        if (0 == nMood) /* skip space */
        {
            if ('"' == CurCh) /* quote at the begining of a cmd */
                nMood = 4, nQuote = !0;
            else if (*p == CurCh)
                p++, nMood = 1;
            else
                nMood = 2;
            continue;
        }
        else if (1 == nMood) /* try matching cmd */
        {
            if (*p == CurCh) /* match character */
            {
                if ('"' == CurCh) /* keep the quote logic happy */
                    nQuote = !nQuote;
                p++;
                continue;
            }
            else if (*p) /* match failed */
            {
                if (' ' == CurCh) /* separator ? */
                {
                    if (nQuote) /* not end of cmd in quote */
                        nMood=2; /* wait until the next cmd */
                    else /* end of cmd, try next */
                        p = Cmd, nMood = 0;
                }
                else /* not a seperator */
                {
                    if ('"' == CurCh) /* keep the quote logic happy */
                        nQuote = !nQuote;
                    nMood = 2; /* wait until the next cmd */
                }
                continue;
            }
            else /* *p == 0 -> end of cmd, we are searching, cmd match? */
            {
                if (nQuote) /* quoted */
                {
                    if ('=' == CurCh)
                        return !0; /* a real match */
                    if ('"' == CurCh) /* and an end quote */
                        nQuote = !nQuote, nMood = 3; /* not yet, but there is a chance */
                    else
                        nMood = 2; /* alas, wait until the next cmd */
                }
                else if (' ' == CurCh || '=' == CurCh) /* end of cmd in cmdline */
                    return !0; /* a real match */
                else /* no match */
                {
                    if ('"' == CurCh) /* keep the quote logic happy */
                        nQuote = !nQuote;
                    nMood = 2; /* wait until the next cmd */
                }
            }
        }
        else if (2 == nMood) /* wait until the next cmd */
        {
            if ('"' == CurCh) /* quote */
                nQuote = !nQuote;
            else if (!nQuote && ' ' == CurCh) /* end of a cmd */
                p = Cmd, nMood = 0; /* try the next cmd */
            continue;
        }
        else if (3 == nMood) /* matched cmd string while quoted */
        {
            if ('=' == CurCh || ' ' == CurCh) /* match, as quote ends right */
                return !0; /* a real match */
            else  /* no match, wait until the next cmd */
                nMood = 2;
            continue;
        }
        else if (4 == nMood)
        {
            if (*p == CurCh) /* match, try rest */
                p++, nMood = 1;
            else /* no match, wait until the next cmd */
                nMood = 2;
            continue;
        }
    }

    return 0;
}

/*
 * Write the serial number of the device into str.
 */
NvError FastbootGetSerialNo(char *Str, NvU32 StrLength)
{
    NvError e = NvSuccess;
    NvU32 SerialLen = 0;
    NvU32 i;
    NvU32 ChipId;
    NvU32 *SerialNo = NULL;
    NvU32 Size = 0;
#ifdef NV_USE_NCT
    nct_item_type nct;
#endif /* NV_USE_NCT */

    if (!Str)
    {
        return NvError_BadParameter;
    }

#ifdef NV_USE_NCT
     if (NvNctReadItem(NCT_ID_SERIAL_NUMBER, &nct) == NvSuccess)
    {
       NvOsMemset(Str, 0, sizeof(nct.serial_number.sn) + 1);
       NvOsStrncpy(Str, nct.serial_number.sn,
                      sizeof(nct.serial_number.sn));
       if (NvOsStrcmp(Str, "") != 0)
           return 0;
    }
#endif /* NV_USE_NCT */

    /* Return chip UID */
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_Uid,
        NULL, &Size));
    SerialNo = NvOsAlloc(Size);
    if (!SerialNo)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_Uid,
        (void *)SerialNo, (void *)&Size));

    ChipId = NvSysUtilGetChipId();

    if(ChipId == 0x20 || ChipId == 0x30)
    {
        SerialLen = 17;
        if (StrLength >= SerialLen)
        {
            NvOsSnprintf(Str, SerialLen, "%08x%08x",
                SerialNo[1], SerialNo[0]);
        }
    }
    else
    {
        SerialLen = 26;
        if (StrLength >= SerialLen)
        {
            NvOsSnprintf(Str, SerialLen, "%01x%08x%08x%08x",
                SerialNo[3], SerialNo[2], SerialNo[1], SerialNo[0]);
        }
    }
    for (i = 0; i < SerialLen; i++)
    {
        if ((Str[i] >=  'a') && (Str[i] <=  'z'))
        {
            Str[i] -= 'a' - 'A';
        }
    }

fail:
    if(SerialNo)
        NvOsFree(SerialNo);
    return e;
}

#if TSEC_EXISTS
static void NvAbootCreateTsecCarveoutNode(NvU32 MemBase, NvU32 MemSize)
{
    int node;
    int err;

    if (fdt_hdr)
    {
        node = fdt_path_offset((void *)fdt_hdr, "/host1x/tsec/carveout");
        if (node < 0)
        {
            node = fdt_path_offset((void *)fdt_hdr, "/host1x/tsec");
            if (node < 0)
            {
                NvOsDebugPrintf("%s: /host1x/tsec doesn't exist\n", __func__);
                return;
            }
            node = fdt_add_subnode((void *)fdt_hdr, node, "carveout");
            if (node < 0)
            {
                NvOsDebugPrintf("%s: create /host1x/tsec/carveout failed\n", __func__);
                return;
            }
        }
        err = fdt_setprop_cell((void *)fdt_hdr, node, "carveout_addr", MemBase);
        if (err)
        {
            NvOsDebugPrintf("%s: unable to set carveout address\n", __func__);
            return;
        }
        err = fdt_setprop_cell((void *)fdt_hdr, node, "carveout_size", MemSize);
        if (err)
        {
            NvOsDebugPrintf("%s: unable to set carveout size\n", __func__);
            return;
        }
    }
}
#endif

/*  Creates the command line, and adds it to the TAG list */
NvError FastbootCommandLine(
    NvAbootHandle hAboot,
    const PartitionNameMap *PartitionList,
    NvU32 NumPartitions,
    NvU32 SecondaryBootDevice,
    const unsigned char *CommandLine,
    NvBool HasInitRamDisk,
    NvU32 NvDumperReserved,
    const unsigned char *ProductName,
    NvBool RecoveryKernel,
    NvBool BootToCharging)
{
    NvRmDeviceHandle hRm;
    NvAbootDramPart DramPart;
    int Remain;
    NvU32 Idx, i;
    NvOdmBoardInfo BoardInfo;
    char *ptr;
    NvU32 DramPartNum = 0;
    NvU64 MemBase;
    NvU64 MemSize;
    NvU32 NumDevices;
    NvOdmDebugConsole DbgConsole;
    NvOdmPmuBoardInfo PmuBoardInfo;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;
    NvOdmTouchInfo TouchInfo;
    NvOdmPowerSupplyInfo PowerSupplyInfo;
    NvOdmAudioCodecBoardInfo AudioCodecInfo;
    NvOdmMaxCpuCurrentInfo CpuCurrInfo;
    NvOdmCameraBoardInfo CameraBoardInfo;
    NvFsMountInfo pFsMountInfo;
    NvU8 ModemId;
    NvBool PmicWdtDisable;
    NvU8 SkuOverride;
    NvU32 CommChipVal;
    NvU32 Usb_Owner_Info;
    NvS32 Lane_Owner_Info;
    NvU32 Emc_Max_Dvfs;
    NvU32 PartitionIdGpt1;
    NvU32 SKU;
    NvU32 size = sizeof(NvU32);
    int node, err;
    NvError e = NvSuccess;
    const char *CmdStr;
    NvOdmOsOsInfo OsInfo;

    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));

    NvOsMemset(s_CmdLine, 0, sizeof(s_CmdLine));

    i = InitCmdList(CommandLine);
    Remain = sizeof(s_CmdLine) / sizeof(char) - i;
    ptr = &s_CmdLine[i];

    CmdStr = "tegraid";
    if (!DoesCommandExist(CmdStr))
    {
        NvOsSnprintf(ptr, Remain, "%s=%x.%x.%x.%x.%x",
                        CmdStr,
                        hRm->ChipId.Id,
                        hRm->ChipId.Major,
                        hRm->ChipId.Minor,
                        hRm->ChipId.Netlist,
                        hRm->ChipId.Patch);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;

        if (hRm->ChipId.Private)
            NvOsSnprintf(ptr, Remain, ".%s ", hRm->ChipId.Private);
        else
            NvOsStrncpy(ptr, " ", Remain - 1);

        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    /* Find base/size of Primary partition */
    CmdStr = "mem";
    if (!DoesCommandExist(CmdStr))
    {
        NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Primary, &MemBase, &MemSize);

        MemSize >>= 20;
        MemBase >>= 20;

        /* Kernel crashes if BL passes odd number of Megabytes.
          * If Kernel uses the three level page table with LPAE, 2nd level page table
          * is pre-allocated before paging_init, which is not the case for the third level.
          * So if BL team pass in an odd number DRAM size to kernel, which triggers the
          * third level page table init, it will cause kernel tyring to access virtual memory
          * address that is not mapped. Then page fault happens in the end. */
        MemSize -= ((MemSize & 0x1) ? 1 : 0);

        NvOsSnprintf(ptr, Remain, "%s=%uM@%uM ", CmdStr, (NvU32)MemSize, (NvU32)MemBase);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    /* Find size of Extended partitions (memory beyond the contiguous Primary) */
    NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Extended, &MemBase, &MemSize);

    if (MemSize > 0)
    {
        MemSize >>= 20;
        MemBase >>= 20;

        /* Kernel crashes if BL passes odd number of Megabytes.
        * If Kernel uses the three level page table with LPAE, 2nd level page table
        * is pre-allocated before paging_init, which is not the case for the third level.
        * So if BL team pass in an odd number DRAM size to kernel, which triggers the
        * third level page table init, it will cause kernel tyring to access virtual memory
        * address that is not mapped. Then page fault happens in the end. */
        MemSize -= ((MemSize & 0x1) ? 1 : 0);

        NvOsSnprintf(ptr, Remain, "%s=%uM@%uM ", CmdStr, (NvU32)MemSize, (NvU32)MemBase);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
        MemSize = 0;
    }

    while ((MemSize > 0) &&
           NvAbootGetDramPartitionParameters(hAboot, &DramPart, ++DramPartNum))
    {
        NvU32 BankSize;

        /* Get the physical limits of the extended DRAM aperture. */
        BankSize = DramPart.Top - DramPart.Bottom;
        BankSize = NV_MIN(BankSize, MemSize);
        MemSize -= BankSize;

        BankSize >>= 20;
        DramPart.Bottom >>= 20;

        NvOsSnprintf(ptr, Remain, "mem=%uM@%uM ", BankSize, DramPart.Bottom);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    if (MemSize > 0)
    {
        NvOsDebugPrintf("fastboot: %d bytes of unmappable DRAM\n", MemSize);
    }

    CmdStr = "memtype";
    if (!DoesCommandExist(CmdStr))
    {
        NvU16 MemType;
        if (NvOdmQueryGetUpatedBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                 &MemType, NvOdmUpdatedBoardModuleInfoType_MemType) == NvSuccess)
        {
            NvOsSnprintf(ptr, Remain, "%s=%u ", CmdStr, (NvU8)MemType);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }
#if VPR_EXISTS
    /* Find base/size of VPR */
    CmdStr = "vpr";
    if (!DoesCommandExist(CmdStr))
    {
        NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Vpr, &MemBase, &MemSize);
        if (MemSize)
        {
            LockVprAperture(hAboot);
            MemSize >>= 20;
            MemBase >>= 20;

            NvOsSnprintf(ptr, Remain, "%s=%uM@%uM ", CmdStr, (NvU32)MemSize, (NvU32)MemBase);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
	else
	    /* even with 0 size (VPR disabled), bootloader must still set lock bit to
	     * indicate the programming is completed.
             */
	    LockVprAperture(hAboot);
    }
#endif
#if TSEC_EXISTS
    /* Find base/size of TSEC */
    CmdStr = "tsec";
    if (!DoesCommandExist(CmdStr))
    {
        NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Tsec, &MemBase, &MemSize);
        NvAbootCreateTsecCarveoutNode((NvU32)MemBase, (NvU32)MemSize);
        if (MemSize)
        {
            LockTsecAperture(hAboot);
            MemSize >>= 20;
            MemBase >>= 20;

            NvOsSnprintf(ptr, Remain, "%s=%uM@%uM ", CmdStr, (NvU32)MemSize, (NvU32)MemBase);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    /* Pass encrypted OTF key to kernel */
    CmdStr = "otf_key";
    if (!DoesCommandExist(CmdStr))
    {
        char otf_str[ENCRYPTED_OTF_KEY_SIZE_BYTES * 2 + 1];
        NvU32 i = 0;
        for (i = 0; i < ENCRYPTED_OTF_KEY_SIZE_BYTES; i++)
            NvOsSnprintf(&otf_str[i * 2], 3, "%02X",((NvU8*)&g_OtfKey)[i]);
        otf_str[ENCRYPTED_OTF_KEY_SIZE_BYTES * 2] = 0;
        NvOsSnprintf(ptr, Remain, "%s=%s ", CmdStr, otf_str);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }
#endif
#if defined(CONFIG_TRUSTED_FOUNDATIONS) || \
    defined(CONFIG_TRUSTED_LITTLE_KERNEL) || \
    defined(CONFIG_NONTZ_BL)
    /* Find base/size of TZRAM */
    CmdStr = "tzram";
    if (!DoesCommandExist(CmdStr))
    {
        NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_SecureOs, &MemBase, &MemSize);
        if (MemSize)
        {
            MemSize >>= 20;
            MemBase >>= 20;

            NvOsSnprintf(ptr, Remain, "%s=%uM@%uM ", CmdStr, (NvU32)MemSize, (NvU32)MemBase);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }
#endif

    /* Populate PASR information*/
    NvRmPhysAddr MemBaseTmp;
    NvU32 MemSizeTmp; // unused

    /* The memory configuration of the bootloader may not be the same
       as that of the OS the bootloader is loading. Retrieve the memory
       size of the target OS. */
    NvOsMemset(&OsInfo, 0, sizeof(NvOdmOsOsInfo));
    OsInfo.OsType = NvOdmOsOs_Linux;
#if defined(ANDROID)
    OsInfo.Sku = NvOdmOsSku_Android;
#else
    OsInfo.Sku = NvOdmOsSku_Unknown;
#endif

    NvRmModuleGetBaseAddress(hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ExternalMemory, 0),
        &MemBaseTmp, &MemSizeTmp);
    MemBase = (NvU64)MemBaseTmp;
    MemSize = NvOdmQueryOsMemSize(NvOdmMemoryType_Sdram, &OsInfo);

    MemSize >>= 20;
    MemBase >>= 20;

    NumDevices = NV_REGR(hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
                             MC_EMEM_ADR_CFG_0);
    NumDevices &= 1;

    if (MemSize)
    {
        NvU64 DeviceSize = 0;
        NvU64 SectionSize = 0;
        NvU64 Base = MemBase;

        for(i = 0; ( i <= NumDevices ) && (Base < (MemBase + MemSize)); i++)
        {
            DeviceSize = NvAbootPrivGetDramRankSize(hAboot, MemSize, i);
            SectionSize = DeviceSize >> 3;

            NvOsSnprintf(ptr, Remain, "ddr_die=%uM@%uM ",
                              (NvU32)DeviceSize, (NvU32)Base);
            Base += DeviceSize;

            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }

        NvOsSnprintf(ptr, Remain, "section=%uM ",
                          (NvU32)SectionSize);

        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    CmdStr = "commchip_id";
    if (!DoesCommandExist(CmdStr)) {
        CommChipVal = NvOdmQueryGetWifiOdmData();
        NvOsSnprintf(ptr, Remain, "%s=%u ", CmdStr, CommChipVal);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    CmdStr = "usb_port_owner_info";
    if (!DoesCommandExist(CmdStr))
    {
        Usb_Owner_Info = NvOdmQueryGetUsbData();
        NvOsSnprintf(ptr, Remain, "%s=%u ", CmdStr, Usb_Owner_Info);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    /* Required for T124 where 6 lanes are shared b/w PCIe, SATA & USB3 */
    {
        NvU32 chip_id;
        chip_id = NvSysUtilGetChipId();
        if (chip_id == 0x40)
        {
            CmdStr = "lane_owner_info";
            if (!DoesCommandExist(CmdStr))
            {
                Lane_Owner_Info = NvOdmQueryGetLaneData();
                if (Lane_Owner_Info >= 0)
                {
                    NvOsSnprintf(ptr, Remain, "%s=%u ", CmdStr, Lane_Owner_Info);
                    Idx = NvOsStrlen(ptr);
                    Remain -= Idx;
                    ptr += Idx;
                }
            }
        }
    }
    CmdStr = "emc_max_dvfs";
    if (!DoesCommandExist(CmdStr))
    {
        Emc_Max_Dvfs = NvOdmQueryGetPLLMFreqData();
        NvOsSnprintf(ptr, Remain, "%s=%u ", CmdStr, Emc_Max_Dvfs);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

#if defined(ANDROID)
    {
        CmdStr = "androidboot.serialno";
        if (!DoesCommandExist(CmdStr))
        {
            char Buff[MAX_SERIALNO_LEN] = {0};

            e = FastbootGetSerialNo(Buff, MAX_SERIALNO_LEN);
            if (e == NvSuccess)
            {
                NvOsSnprintf(ptr, Remain, "%s=%s ", CmdStr, Buff);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }

        CmdStr = "androidboot.commchip_id";
        if (!DoesCommandExist(CmdStr))
        {
            CommChipVal = NvOdmQueryGetWifiOdmData();
            NvOsSnprintf(ptr, Remain, "%s=%u ", CmdStr, CommChipVal);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }

        CmdStr = "androidboot.modem";
        if (!DoesCommandExist(CmdStr)) {
            NvOdmQueryGetModemId(&ModemId);
            switch (ModemId)
            {
            case 1:
            case 2:
                NvOsSnprintf(ptr, Remain, "%s=icera ", CmdStr);
                break;
            case 3:
                NvOsSnprintf(ptr, Remain, "%s=rmc ", CmdStr);
                break;
            case 4:
                NvOsSnprintf(ptr, Remain, "%s=imc ", CmdStr);
                break;
            case 5:
                NvOsSnprintf(ptr, Remain, "%s=ste ", CmdStr);
                break;
            case 7:
                NvOsSnprintf(ptr, Remain, "%s=via ", CmdStr);
                break;
            default:
                NvOsSnprintf(ptr, Remain, "%s=none ", CmdStr);
                break;
            }
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }

        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_Touch,
                          &TouchInfo, sizeof(TouchInfo))) {
            CmdStr = "androidboot.touch_vendor_id";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s=%u ",
                    CmdStr, TouchInfo.TouchVendorId);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }

            CmdStr = "androidboot.touch_panel_id";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s=%u ",
                    CmdStr, TouchInfo.TouchPanelId);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }

            CmdStr = "androidboot.touch_feature";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s=%u ",
                    CmdStr, TouchInfo.TouchFeature);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }
#endif
    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_Touch,
                &TouchInfo, sizeof(TouchInfo))) {
        CmdStr = "touch_id";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=%u@%u ",
                CmdStr, TouchInfo.TouchVendorId, TouchInfo.TouchPanelId);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    CmdStr = "video";
    if (!DoesCommandExist(CmdStr)) {
        NvOsSnprintf(ptr, Remain, "%s=tegrafb ", CmdStr);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    CmdStr = "no_console_suspend";
    if (!DoesCommandExist(CmdStr)) {
        NvOsSnprintf(ptr, Remain, "%s=1 ", CmdStr);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    DbgConsole = NvOdmQueryDebugConsole();
    if (DbgConsole == NvOdmDebugConsole_None)
    {
        CmdStr = "console";
        if (!DoesCommandExist(CmdStr)) {
            NvOsSnprintf(ptr, Remain, "%s=none ", CmdStr);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }

        CmdStr = "debug_uartport";
        if (!DoesCommandExist(CmdStr)) {
            NvOsSnprintf(ptr, Remain, "%s=hsport ", CmdStr);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }
    else if (DbgConsole & NvOdmDebugConsole_Automation)
    {
        CmdStr = "console";
        if (!DoesCommandExist(CmdStr)) {
            NvOsSnprintf(ptr, Remain, "%s=none ", CmdStr);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }

        CmdStr = "debug_uartport";
        if (!DoesCommandExist(CmdStr)) {
            NvOsSnprintf(ptr, Remain, "%s=lsport,%d ", CmdStr,
                          (DbgConsole & 0xF) - NvOdmDebugConsole_UartA);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }
    else
    {
        CmdStr = "console";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=ttyS0,115200n8 ", CmdStr);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;

        }

        CmdStr = "debug_uartport";
        if (!DoesCommandExist(CmdStr))
        {
            if (NvOdmQueryUartOverSDEnabled() == NV_TRUE)
            {
                NvOsSnprintf(ptr, Remain, "%s=lsport,%d ", CmdStr,
                      NvOdmDebugConsole_UartSD - NvOdmDebugConsole_UartA);
            }
            else
            {
                NvOsSnprintf(ptr, Remain, "%s=lsport,%d ", CmdStr,
                      DbgConsole - NvOdmDebugConsole_UartA);
            }
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    if (!HasInitRamDisk)
    {
        CmdStr = "console";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=tty1 ", CmdStr);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    if (NvOdmQueryGetSkuOverride(&SkuOverride))
    {
        CmdStr = "sku_override";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=%d ", CmdStr, SkuOverride);
            Idx = NvOsStrlen(ptr);
            ptr += Idx;
            Remain -= Idx;
        }
    }

    CmdStr = "usbcore.old_scheme_first";
    if (!DoesCommandExist(CmdStr))
    {
        NvOsSnprintf(ptr, Remain, "%s=1 ", CmdStr);
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }

    CmdStr = "lp0_vec";
    if (!DoesCommandExist(CmdStr))
    {
        NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_lp0_vec, &MemBase, &MemSize);
        NvOsSnprintf(ptr, Remain, "%s=%u@0x%8x ", CmdStr, (NvU32)MemSize,(NvU32)MemBase);
        Idx = NvOsStrlen(ptr);
        ptr += Idx;
        Remain -= Idx;
    }
#ifndef TARGET_SOC_T12X
    CmdStr = "tegra_fbmem";
    if (!DoesCommandExist(CmdStr))
    {
        NvOsSnprintf(ptr, Remain, "%s=%d@0x%x ", CmdStr, surf_size, surf_address);
        Idx = NvOsStrlen(ptr);
        ptr += Idx;
        Remain -= Idx;
    }
#endif
    if (NvDumperReserved != 0)
    {
        CmdStr = "nvdumper_reserved";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=0x%x ", CmdStr, NvDumperReserved);
            Idx = NvOsStrlen(ptr);
            ptr += Idx;
            Remain -= Idx;
        }
    }

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_CpuCurrent,
                      &CpuCurrInfo, sizeof(CpuCurrInfo))) {
        CmdStr = "max_cpu_cur_ma";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=%d ",CmdStr,
                         CpuCurrInfo.MaxCpuCurrentmA);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                      &PmuBoardInfo, sizeof(PmuBoardInfo))) {
        CmdStr = "core_edp_mv";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=%d ", CmdStr,
                         PmuBoardInfo.core_edp_mv);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }

        CmdStr = "core_edp_ma";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=%d ", CmdStr,
                         PmuBoardInfo.core_edp_ma);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
        // Pass the pmu board info if it is require
        if (PmuBoardInfo.isPassBoardInfoToKernel) {
            CmdStr = "pmuboard";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain,
                    "%s=0x%04x:0x%04x:0x%02x:0x%02x:0x%02x ", CmdStr,
                    PmuBoardInfo.BoardInfo.BoardID, PmuBoardInfo.BoardInfo.SKU,
                    PmuBoardInfo.BoardInfo.Fab, PmuBoardInfo.BoardInfo.Revision,
                    PmuBoardInfo.BoardInfo.MinorRevision);
                 Idx = NvOsStrlen(ptr);
                 Remain -= Idx;
                 ptr += Idx;
            }
        }
    }

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo))) {
        if (DisplayBoardInfo.IsPassBoardInfoToKernel)
        {
            CmdStr = "displayboard";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain,
                    "%s=0x%04x:0x%04x:0x%02x:0x%02x:0x%02x ", CmdStr,
                    DisplayBoardInfo.BoardInfo.BoardID,
                    DisplayBoardInfo.BoardInfo.SKU,
                    DisplayBoardInfo.BoardInfo.Fab,
                    DisplayBoardInfo.BoardInfo.Revision,
                    DisplayBoardInfo.BoardInfo.MinorRevision);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_CameraBoard,
                      &CameraBoardInfo, sizeof(CameraBoardInfo))) {
        if (CameraBoardInfo.IsPassBoardInfoToKernel)
        {
            CmdStr = "cameraboard";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain,
                    "%s=0x%04x:0x%04x:0x%02x:0x%02x:0x%02x ", CmdStr,
                    CameraBoardInfo.BoardInfo.BoardID,
                    CameraBoardInfo.BoardInfo.SKU,
                    CameraBoardInfo.BoardInfo.Fab,
                    CameraBoardInfo.BoardInfo.Revision,
                    CameraBoardInfo.BoardInfo.MinorRevision);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }

    NvU32 chip_id;
    chip_id = NvSysUtilGetChipId();
    if (!NvOsStrcmp("Dalmore",(const char *)NvOdmQueryPlatform()) ||
        !NvOsStrcmp("Pluto",(const char *)NvOdmQueryPlatform()) ||
        !NvOsStrcmp("Ardbeg",(const char *)NvOdmQueryPlatform()))
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
                          &DisplayPanelInfo, sizeof(DisplayPanelInfo))) {
            CmdStr = "display_panel";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain,
                   "%s=%d ", CmdStr,
                   DisplayPanelInfo.DisplayPanelId);

                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }
    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PowerSupply,
                      &PowerSupplyInfo, sizeof(PowerSupplyInfo))) {
        CmdStr = "power_supply";
        if (!DoesCommandExist(CmdStr))
        {
            if (PowerSupplyInfo.SupplyType == NvOdmBoardPowerSupply_Adapter)
                NvOsSnprintf(ptr, Remain, "%s=Adapter ", CmdStr);
            else
                NvOsSnprintf(ptr, Remain, "%s=Battery ", CmdStr);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    chip_id = NvSysUtilGetChipId();
    if (chip_id == 0x35)
    {
        e = NvDdkFuseGet(NvDdkFuseDataType_Sku, &SKU, &size);
        if(e == NvSuccess)
        {
            if (SKU == 0x20)
            {
                CmdStr = "nr_cpus";
                if (!DoesCommandExist(CmdStr))
                {
                    NvOsSnprintf(ptr, Remain, "%s=2 ", CmdStr);
                    Idx = NvOsStrlen(ptr);
                    Remain -= Idx;
                    ptr += Idx;
                }
            }
        }
    }

#ifdef NV_EMBEDDED_BUILD
    {
        NvError e;
        NvU8 *InfoArg;
        NvU32 Size = COMMAND_LINE_SIZE;

        InfoArg =  NvOsAlloc(Size);
        if(InfoArg != NULL)
        {
            e = NvBootGetSkuInfo(InfoArg, 1, Size);
            if(e == NvSuccess)
            {
                NvOsSnprintf(ptr, Remain, "%s ", InfoArg);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }
#endif
    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_AudioCodec,
                      &AudioCodecInfo, sizeof(AudioCodecInfo))) {
        if (NvOsStrlen(AudioCodecInfo.AudioCodecName)) {
            CmdStr = "audio_codec";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s=%s ", CmdStr, AudioCodecInfo.AudioCodecName);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_CameraBoard,
                      &CameraBoardInfo, sizeof(CameraBoardInfo))) {
        if (CameraBoardInfo.IsPassBoardInfoToKernel)
        {
            CmdStr = "cameraboard";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s=0x%04x:0x%04x:0x%02x:0x%02x:0x%02x ", CmdStr,
                    CameraBoardInfo.BoardInfo.BoardID, CameraBoardInfo.BoardInfo.SKU,
                    CameraBoardInfo.BoardInfo.Fab, CameraBoardInfo.BoardInfo.Revision,
                    CameraBoardInfo.BoardInfo.MinorRevision);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }

    if (NvOdmPeripheralGetBoardInfo(0x0, &BoardInfo)) {
        CmdStr = "board_info";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=0x%04x:0x%04x:0x%02x:0x%02x:0x%02x ", CmdStr,
                BoardInfo.BoardID, BoardInfo.SKU, BoardInfo.Fab,
                BoardInfo.Revision, BoardInfo.MinorRevision);

            Idx = NvOsStrlen(ptr);
            ptr += Idx;
            Remain -= Idx;
        }
    }

    if (ProductName[0])
    {
        if (!NvOsMemcmp(ProductName, "up", NvOsStrlen("up")))
        {
            ProductName += NvOsStrlen("up");
            CmdStr = "nosmp";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s ", CmdStr);
                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
            if (ProductName[0]=='_')
                ProductName++;
        }

        if (!HasInitRamDisk && ProductName[0])
        {
            CmdStr = "root";
            if (!DoesCommandExist(CmdStr))
            {
                if (!NvOsMemcmp(ProductName, "usb", NvOsStrlen("usb")))
                {
                    ProductName += NvOsStrlen("usb");
                    NvOsSnprintf(ptr, Remain,
                        "%s=/dev/sd%c%c rw rootwait ", CmdStr,
                        ProductName[0], ProductName[1]);
                    ProductName+=2;
                }
                else if (!NvOsMemcmp(ProductName, "eth", NvOsStrlen("eth")))
                {
                    ProductName += NvOsStrlen("eth");
                    NvOsSnprintf(ptr, Remain,
                        "%s=/dev/nfs rw netdevwait ip=:::::eth%c:on ",
                        CmdStr, ProductName[0]);
                    ProductName++;
                }
                else if (!NvOsMemcmp(ProductName, "sd", NvOsStrlen("sd")))
                {
                    ProductName += NvOsStrlen("sd");
                    NvOsSnprintf(ptr, Remain,
                        "%s=/dev/sd%c%c rw rootwait ", CmdStr,
                        ProductName[0], ProductName[1]);
                    ProductName += 2;
                }
                else if (!NvOsMemcmp(ProductName, "mmchd", NvOsStrlen("mmchd")))
                {
                    ProductName += NvOsStrlen("mmchd");
                    NvOsSnprintf(ptr, Remain,
                        "%s=/dev/mmchd%c%c%c rw rootwait ", CmdStr,
                        ProductName[0], ProductName[1], ProductName[2]);
                    ProductName += 3;
                }
                else if (!NvOsMemcmp(ProductName, "mtdblock", NvOsStrlen("mtdblock")))
                {
                    ProductName += NvOsStrlen("mtdblock");
                    NvOsSnprintf(ptr, Remain,
                        "%s=/dev/mtdblock%c rw rootwait ", CmdStr, ProductName[0]);
                    ProductName += 1;
                }
                else if (!NvOsMemcmp(ProductName, "mmcblk", NvOsStrlen("mmcblk")))
                {
                    ProductName += NvOsStrlen("mmcblk");
                    NvOsSnprintf(ptr, Remain,
                        "%s=/dev/mmcblk%c%c%c rw rootwait ", CmdStr,
                        ProductName[0], ProductName[1], ProductName[2]);
                    ProductName += 3;
                }
                else
                    FastbootError("Unrecognized root device: %s\n", ProductName);

                Idx = NvOsStrlen(ptr);
                Remain -= Idx;
                ptr += Idx;
            }
        }
    }
#ifndef NV_EMBEDDED_BUILD
    else if (!HasInitRamDisk)
    {
        NvOsSnprintf(ptr, Remain,
           "root=/dev/sda1 rw rootwait ");
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }
#endif
    CmdStr = "tegraboot";
    if (!DoesCommandExist(CmdStr))
    {
        if ((PartitionIdGpt1 = GetPartitionByType(
                 NvPartMgrPartitionType_GP1)) != 0)
        {
            NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(
                PartitionIdGpt1, &pFsMountInfo));

            switch (pFsMountInfo.DeviceId)
            {
                case NvDdkBlockDevMgrDeviceId_Nand:
                    NvOsSnprintf(ptr, Remain, "%s=nand ", CmdStr);
                    break;
                case NvDdkBlockDevMgrDeviceId_Nor:
                    NvOsSnprintf(ptr, Remain, "%s=nor ", CmdStr);
                    break;
                case NvDdkBlockDevMgrDeviceId_SDMMC:
                    NvOsSnprintf(ptr, Remain, "%s=sdmmc ", CmdStr);
                    break;
                case NvDdkBlockDevMgrDeviceId_Sata:
                    NvOsSnprintf(ptr, Remain, "%s=sata ", CmdStr);
                    break;
                default:
                    break;
            }

            Idx = NvOsStrlen(ptr);
            ptr += Idx;
            Remain -= Idx;
        }
    }

    if ((SecondaryBootDevice == NvDdkBlockDevMgrDeviceId_Nand)
            || (SecondaryBootDevice == NvDdkBlockDevMgrDeviceId_Nor))
    {
        CmdStr = "mtdparts";
        if (!DoesCommandExist(CmdStr))
        {
            NvBool first = NV_TRUE;
            for (i=0; i<NumPartitions; i++)
            {
                NvU64 Start, Num;
                NvU32 SectorSize;
                if (NvAbootGetPartitionParameters(hAboot,
                    PartitionList[i].NvPartName, &Start, &Num,
                    &SectorSize) == NvSuccess)
                {
                    if (first)
                    {
                        if (SecondaryBootDevice == NvDdkBlockDevMgrDeviceId_Nand)
                            NvOsSnprintf(ptr, Remain, "%s=tegra_nand:", CmdStr);
                        else
                            NvOsSnprintf(ptr, Remain, "%s=tegra-nor:", CmdStr);
                        Idx = NvOsStrlen(ptr);
                        ptr += Idx;
                        Remain -= Idx;
                        first = NV_FALSE;
                    }

                    NvOsSnprintf(ptr, Remain, "%uK@%uK(%s),",
                        (NvU32)(Num * SectorSize >> 10),
                        (NvU32)(Start * SectorSize >> 10),
                        PartitionList[i].LinuxPartName);
                    Idx = NvOsStrlen(ptr);
                    ptr += Idx;
                    Remain -= Idx;
                }
            }
            if (!first)
            {
                ptr--;
                *ptr = ' ';
                ptr++;
            }
        }
    }
    else
    {
        if ((PartitionIdGpt1 = GetPartitionByType(NvPartMgrPartitionType_GP1)) != 0)
        {
            CmdStr = "gpt";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s ", CmdStr);
                Idx = NvOsStrlen(ptr);
                ptr += Idx;
                Remain -= Idx;
            }

            CmdStr = "gpt_sector";
            if (!DoesCommandExist(CmdStr))
            {
                NvU64 PartitionStart, PartitionSize;
                NvU32 TempSectorSize;
                NvU32 GptSector;
                if (NvAbootGetPartitionParametersbyId(hAboot,
                    PartitionIdGpt1, &PartitionStart, &PartitionSize,
                    &TempSectorSize) == NvSuccess)
                {
                    GptSector = (PartitionStart + PartitionSize)
                                * (TempSectorSize / GPT_BLOCK_SIZE) - 1;
                    NvOsSnprintf(ptr, Remain, "%s=%d ", CmdStr, GptSector);
                    Idx = NvOsStrlen(ptr);
                    ptr += Idx;
                    Remain -= Idx;
                }
                else
                {
                    FastbootError("Unable to get partition parameters by \
                        id: %d\n", PartitionIdGpt1);
                }
            }
        }
        else
        {
            CmdStr = "tegrapart";
            if (!DoesCommandExist(CmdStr))
            {
                NvOsSnprintf(ptr, Remain, "%s=", CmdStr);
                Idx = NvOsStrlen(ptr);
                ptr += Idx;
                Remain -= Idx;

                for (i=0; i<NumPartitions; i++)
                {
                    NvU64 Start, Num;
                    NvU32 SectorSize;
                    if (NvAbootGetPartitionParameters(hAboot,
                        PartitionList[i].NvPartName, &Start, &Num,
                        &SectorSize) == NvSuccess)
                    {
                        NvOsSnprintf(ptr, Remain, "%s:%x:%x:%x%c",
                            PartitionList[i].LinuxPartName, (NvU32)Start, (NvU32)Num,
                            SectorSize, (i==(NumPartitions-1))?' ':',');
                        Idx = NvOsStrlen(ptr);
                        ptr += Idx;
                        Remain -= Idx;
                    }
                    else
                    {
                        FastbootError("Unable to query partition %s\n", PartitionList[i].NvPartName);
                    }
                }
            }
        }
    }

    if (NvOdmQueryGetModemId(&ModemId))
    {
        CmdStr = "modem_id";
        if (!DoesCommandExist(CmdStr))
        {
            NvOsSnprintf(ptr, Remain, "%s=%d ", CmdStr, ModemId);
            Idx = NvOsStrlen(ptr);
            ptr += Idx;
            Remain -= Idx;
        }
    }

    if (NvOdmQueryGetPmicWdtDisableSetting(&PmicWdtDisable))
    {
        CmdStr = "watchdog";
        if (!DoesCommandExist(CmdStr))
        {
            /* odmdata 31-bit when set indicates watchdog disable */
            NvOsSnprintf(ptr, Remain, "%s=%s ", CmdStr,
                (PmicWdtDisable == NV_TRUE)? "disable" : "enable");
            Idx = NvOsStrlen(ptr);
            ptr += Idx;
            Remain -= Idx;
        }
    }

    {
        NvU32 Wb0Address = 0;
        NvU32 Instances, BlockSize;
        CmdStr = "wb0_params";

        if (NvAbootGetWb0Params(&Wb0Address, &Instances, &BlockSize))
        {
          if ( (Wb0Address != 0) && (!DoesCommandExist(CmdStr)) )
          {
             NvOsSnprintf(ptr, Remain, "%s=%u,%d,%d ", CmdStr, Wb0Address, Instances, BlockSize);
             Idx = NvOsStrlen(ptr);
             ptr += Idx;
             Remain -= Idx;
          }
        }
    }

#if defined(ANDROID)
        {
            NvU32 chip_id;
            NvU16 MiscConfig;
            chip_id = NvSysUtilGetChipId();
            CmdStr = "pwr_i2c";
            if (!DoesCommandExist(CmdStr))
            {
#ifdef TARGET_SOC_T114
                if ((chip_id == 0x35) && !NvOsStrcmp("Dalmore",(const char *)NvOdmQueryPlatform()))
                {
                    NvOdmQueryGetUpatedBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                        &MiscConfig,NvOdmUpdatedBoardModuleInfoType_MiscConfig);
                    if(MiscConfig & 0x1)
                        NvOsSnprintf(ptr, Remain, "%s=1000 ", CmdStr);
                     Idx = NvOsStrlen(ptr);
                     Remain -= Idx;
                     ptr += Idx;
                }
#endif
            }
        }
#endif

#ifdef NV_USE_NCT
    /* Find base/size of NCK */
    CmdStr = "nck";
    if (!DoesCommandExist(CmdStr))
    {
        NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Nck, &MemBase, &MemSize);
        if (MemSize && (NV_TRUE == NvNctIsInit()))
        {
            NvOsSnprintf(ptr, Remain, "%s=%u@0x%8x ", CmdStr, (NvU32)MemSize, (NvU32)MemBase);
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
    }

    CmdStr = "androidboot.mode";
    if (BootToCharging) {
        NvOsSnprintf(ptr, Remain, "%s=%s ", CmdStr, "charger");
        Idx = NvOsStrlen(ptr);
        Remain -= Idx;
        ptr += Idx;
    }
    else if (!DoesCommandExist(CmdStr)) {
        nct_item_type buf;
        NvError err;

        err = NvNctReadItem(NCT_ID_FACTORY_MODE, &buf);
        if (NvSuccess == err)
        {
            NvOsSnprintf(ptr, Remain, "%s=%s ", CmdStr,
                buf.factory_mode.flag ? "factory2" : "normal");
            if (buf.factory_mode.flag)
                NvOsDebugPrintf("FACTORY MODE BOOT !!!\n");
            Idx = NvOsStrlen(ptr);
            Remain -= Idx;
            ptr += Idx;
        }
        else
            NvOsDebugPrintf("%s: getting factory_mode is failed (err:%x)\n",
                __func__, err);
    }
#endif /* NV_USE_NCT */
        {
            NvU32 chip_id;
            NvU16 PowerConfig;
            chip_id = NvSysUtilGetChipId();
            CmdStr = "power-config";
            if (!DoesCommandExist(CmdStr))
            {
                 if (chip_id == 0x35 && (!NvOsStrcmp("Dalmore",(const char *)NvOdmQueryPlatform()) ||
                                         !NvOsStrcmp("Pluto",(const char *)NvOdmQueryPlatform())))
                 {
                     NvOdmQueryGetUpatedBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                        &PowerConfig,NvOdmUpdatedBoardModuleInfoType_PowerConfig);
                     PowerConfig= ((PowerConfig & 0x1) << 4) | (PowerConfig & 0x2);
                     NvOsSnprintf(ptr, Remain, "%s=0x%02x ", CmdStr,PowerConfig);
                     Idx = NvOsStrlen(ptr);
                     Remain -= Idx;
                     ptr += Idx;
                 }
                 else if (chip_id == 0x40 && (!NvOsStrcmp("Ardbeg",(const char *)NvOdmQueryPlatform())))
                 {
                     NvOdmQueryGetUpatedBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                        &PowerConfig,NvOdmUpdatedBoardModuleInfoType_PowerConfig);
                     PowerConfig= ((PowerConfig & 0x1) << 4) | (PowerConfig & 0x2);
                     NvOsSnprintf(ptr, Remain, "%s=0x%02x ", CmdStr,PowerConfig);
                     Idx = NvOsStrlen(ptr);
                     Remain -= Idx;
                     ptr += Idx;
                 }
            }
        }

    if (RecoveryKernel)
        NvOsSnprintf(ptr, Remain, "android.kerneltype=recovery ");
    else
        NvOsSnprintf(ptr, Remain, "android.kerneltype=normal ");
    Idx = NvOsStrlen(ptr);
    ptr += Idx;
    Remain -= Idx;

#if NV_MOBILE_DGPU
    NvOsSnprintf(ptr, Remain, " vmalloc=256MB");
    Idx = NvOsStrlen(ptr);
    ptr += Idx;
    Remain -= Idx;
#endif

    if (fdt_hdr)
    {
        node = fdt_path_offset((void *)fdt_hdr, "/chosen");
        if (node < 0)
        {
            NvOsDebugPrintf("%s: Unable to open dtb image (%s)\n", __func__, fdt_strerror(node));
            e = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop((void *)fdt_hdr, node, "bootargs", s_CmdLine, strlen(s_CmdLine));
        if (err < 0)
        {
             NvOsDebugPrintf("%s: Unable to set bootargs (%s)\n", __func__, fdt_strerror(err));
             e = NvError_ResourceError;
             goto fail;
        }
    }
    else
    {
        FastbootAddAtag(ATAG_CMDLINE, sizeof(s_CmdLine), s_CmdLine);
    }

fail:
    NvRmClose(hRm);
    return e;
}

NvError AddSerialBoardInfoTag(void)
{
    NvU32 BoardInfoData[2];
    NvOdmBoardInfo BoardInfo;
    NvBool Ret = NV_FALSE;
    int node, newnode, err;
    NvError NvErr = NvError_Success;

    BoardInfoData[0] = 0;
    BoardInfoData[1] = 0;

    Ret = NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                 &BoardInfo, sizeof(NvOdmBoardInfo));
    if (Ret) {
        // BoardId:SKU:Fab:Revision:MinorRevision
        BoardInfoData[1] = (BoardInfo.BoardID << 16) | BoardInfo.SKU;
        BoardInfoData[0] = (BoardInfo.Fab << 24) | (BoardInfo.Revision << 16) |
                                     (BoardInfo.MinorRevision << 8);
    }

    if (fdt_hdr)
    {
        node = fdt_path_offset((void *)fdt_hdr, "/chosen/board_info");
        if (node < 0)
        {
            node = fdt_path_offset((void *)fdt_hdr, "/chosen");
            if (node >= 0)
                goto skip_chosen;

            node = fdt_add_subnode((void *)fdt_hdr, 0, "chosen");
            if (node < 0)
            {
                NvOsDebugPrintf("%s: Unable to create /chosen (%s)\n", __func__,
                    fdt_strerror(node));
                NvErr = NvError_ResourceError;
                goto fail;
            }

skip_chosen:
            newnode = fdt_add_subnode((void *)fdt_hdr, node, "board_info");
            if (newnode < 0)
            {
                NvOsDebugPrintf("%s: Unable to create board_info (%s)\n", __func__,
                    fdt_strerror(node));
                NvErr = NvError_ResourceError;
                goto fail;
            }

            node = newnode;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "id", BoardInfo.BoardID);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set id (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "sku", BoardInfo.SKU);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set sku (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "fab", BoardInfo.Fab);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set fab (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "major_revision",
                BoardInfo.Revision);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set major_revision (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }

        err = fdt_setprop_cell((void *)fdt_hdr, node, "minor_revision",
                BoardInfo.MinorRevision);
        if (err)
        {
            NvOsDebugPrintf("%s: Unable to set minor_revision (%s)\n", __func__,
                fdt_strerror(err));
            NvErr = NvError_ResourceError;
            goto fail;
        }
    }
    else
    {
        FastbootAddAtag(ATAG_SERIAL, sizeof(BoardInfoData), BoardInfoData);
    }

fail:
    return NvErr;
}

/*  Adds an ATAG to the list */
NvBool FastbootAddAtag(
    NvU32        Atag,
    NvU32        DataSize,
    const void  *Data)
{
    static NvU32 *tag = (NvU32 *)s_BootTags;
    NvU32 TagWords = ((DataSize+3)>>2) + 2;

    if (fdt_hdr)
        NvOsDebugPrintf("%s: setting ATAG (%d)\n", __func__, Atag);

    if (!Atag)
    {
        *tag++ = 0;
        *tag++ = 0;
        tag = NULL;
        return NV_TRUE;
    }
    else if (tag)
    {
        tag[0] = TagWords;
        tag[1] = Atag;
        if (Data)
            NvOsMemcpy(&tag[2], Data, DataSize);
        tag += TagWords;
        return NV_TRUE;
    }
    return NV_FALSE;
}
