/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvos.h"
#include "nvflash_util.h"
#include "nvflash_util_private.h"
#include "nvboot_util_int.h"
#include "nvboothost.h"
#include "nvapputil.h"
#include "nvassert.h"
#include "t12x/nvboot_bct.h"
#include "t12x/nvboot_bit.h"
#include "t12x/nvboot_error.h"
#include "t12x/nvboot_warm_boot_0.h"
#include "nvbct.h"
#include "nvbct_customer_platform_data.h"
#include "nvflash_fuse_bypass.h"

/* default addressess for bootloaders */
#define NVFLASH_BL_ENTRY 0x80108000
#define NVFLASH_BL_ADDRESS 0x80108000

#define LP0_EXIT_RUN_ADDRESS    0x40020000

/*
 * Global data
 */
static NvBool             ForceBctDump = NV_FALSE;
static NvBool             EnableDebug = NV_FALSE;
static NvBool             EnableRegress = NV_FALSE;
static NvU32              MemoryBase = 0x40000000; /* iRAM base */
static NvBootConfigTable *Bct = NULL;
static NvBootInfoTable   *Bit = NULL;

static char *BootTypeTable[] =
{
    "NvBootType_None",
    "NvBootType_Cold",
    "NvBootType_Recovery",
    "NvBootType_Uart",
};

static char *BootDevTypeTable[] =
{
    "NvBootDevType_None",
    "NvBootDevType_Nand",
    "NvBootDevType_Snor",
    "NvBootDevType_Spi",
    "NvBootDevType_Sdmmc",
    "NvBootDevType_Irom",
    "NvBootDevType_Uart",
    "NvBootDevType_Usb",
    "NvBootDevType_Nand_x16",
    "NvBootDevType_MuxOneNand",
    "NvBootDevType_MobileLbaNand",
    "NvBootDevType_Sata",
    "NvBootDevType_Sdmmc3",
};

static char *BootClocksOscFreqTable[] =
{
    "NvBootClocksOscFreq_13",
    "NvBootClocksOscFreq_19_2",
    "NvBootClocksOscFreq_12",
    "NvBootClocksOscFreq_26",
    "NvBootClocksOscFreq_Num",
    "NvBootClocksOscFreq_Unknown",
};

static char *BootRdrStatusTable[] =
{
    "NvBootRdrStatus_None",
    "NvBootRdrStatus_Success",
    "NvBootRdrStatus_ValidationFailure",
    "NvBootRdrStatus_DeviceReadError",
};

static char *BctSearchTerminationTable[] =
{
    "<No information (bug)>",
    "Full Journal Block",
    "Validation Failure",
    "Device Read Error",
};

#define VERIFY( exp, code ) \
    if( !(exp) ) { \
        code; \
    }

void NvFlashUtilT12xDumpBit(NvU8 * BitBuffer, const char *dumpOption);
static NvU32 NvFlashUtilT12xGetBctSize(void);
static
NvU32 NvFlashUtilT12xGetBlEntry(void);
NvU32 NvFlashUtilT12xGetBlAddress(void);
void NvFlashUtilT12xSetNCTCustInfoFieldsInBct(NctCustInfo NCTCustInfo,
                                    NvU8 * BctBuffer);
NvError NvFlashUtilT12xSecureSignWB0Image(NvU8* WB0Buf, NvU32 WB0Length);


#define BOOL_TO_STRING(n)    (n) ? "NV_TRUE" : "NV_FALSE"

#define PRINT_DATA_NVU32(n)     NvAuPrintf("0x%08x (%d)\n", n, n)
#define PRINT_DATA_ENUM(n,f)    NvAuPrintf("0x%08x (%s)\n", n, f(n))
#define PRINT_DATA_BOOL(n)      PRINT_DATA_ENUM(n, BOOL_TO_STRING)
#define PRINT_DATA_PTR(n)       NvAuPrintf("0x%08x\n", PTR_TO_ADDR(n))
#define PRINT_DATA_LOG_BYTES(n) NvAuPrintf("0x%08x (%d bytes)\n", n, 1<<n)

#define PRINT_NAME(name) \
    PrintSpaces(4); \
    PrintAndPadSpaces(name, 36);

#define PRINT_FIELD_NAME(base, field) \
    PrintSpaces(4); \
    PrintFieldAndPadSpaces(base, field, 64);


#define PRINT_ITEM(type, prefix, name) \
    PRINT_NAME(#name) \
    PRINT_DATA_##type((prefix->name))

#define PRINT_ENUM(prefix, name, fn) \
    PRINT_NAME(#name) \
    PRINT_DATA_ENUM(prefix->name, fn)


#define PRINT_FIELD_ITEM(type, prefix, base, name) \
    PRINT_FIELD_NAME(#base, #name) \
    PRINT_DATA_##type((prefix->base.name))

#define PRINT_FIELD_ENUM(prefix, base, name, fn) \
    PRINT_FIELD_NAME(#base, #name) \
    PRINT_DATA_ENUM(prefix->base.name, fn)

#define PRINT_SECDEVSTATUS_ITEM(type, dev, field) \
    PRINT_FIELD_ITEM(type, Bit, SecondaryDevStatus.dev##Status, field)

#define PRINT_ARRAY_FIELD_ITEM(type, prefix, array, idx, field) \
    PrintSpaces(4); \
    PrintArrayFieldAndPadSpaces(#array, idx, #field, 36); \
    PRINT_DATA_##type(prefix->array[idx].field)

#define PRINT_ARRAY_FIELD_ENUM(prefix, array, idx, field, fn) \
    PrintSpaces(4); \
    PrintArrayFieldAndPadSpaces(#array, idx, #field, 36); \
    PRINT_DATA_ENUM(prefix->array[idx].field, fn)

#define PRINT_OFFSET(t,var,spaces) \
    NvAuPrintf("  %s:%s0x%04x (%04d)\n", #var, spaces, \
               offsetof(t, var), offsetof(t, var))

static void
DumpStructureOffsets(void)
{
    NvAuPrintf("BCT size: %d\n", sizeof(NvBootConfigTable));
    NvAuPrintf("BCT Offsets:\n");
    PRINT_OFFSET(NvBootConfigTable, Signature, "            ");
    PRINT_OFFSET(NvBootConfigTable, RandomAesBlock, "       ");
    PRINT_OFFSET(NvBootConfigTable, BootDataVersion, "      ");
    PRINT_OFFSET(NvBootConfigTable, BlockSizeLog2, "        ");
    PRINT_OFFSET(NvBootConfigTable, PageSizeLog2, "         ");
    PRINT_OFFSET(NvBootConfigTable, PartitionSize, "        ");
    PRINT_OFFSET(NvBootConfigTable, NumParamSets, "         ");
    PRINT_OFFSET(NvBootConfigTable, DevType, "              ");
    PRINT_OFFSET(NvBootConfigTable, DevParams, "            ");
    PRINT_OFFSET(NvBootConfigTable, NumSdramSets, "         ");
    PRINT_OFFSET(NvBootConfigTable, SdramParams, "          ");
    PRINT_OFFSET(NvBootConfigTable, BadBlockTable, "        ");
    PRINT_OFFSET(NvBootConfigTable, BootLoadersUsed, "      ");
    PRINT_OFFSET(NvBootConfigTable, BootLoader, "           ");
    PRINT_OFFSET(NvBootConfigTable, CustomerData, "         ");
    PRINT_OFFSET(NvBootConfigTable, EnableFailBack, "       ");
    PRINT_OFFSET(NvBootConfigTable, Reserved, "             ");
    PRINT_OFFSET(NvBootConfigTable, Key, "       ");
    PRINT_OFFSET(NvBootConfigTable, SecureJtagControl, "             ");

    NvAuPrintf("BIT size: %d\n", sizeof(NvBootInfoTable));
    NvAuPrintf("BIT Offsets:\n");
    PRINT_OFFSET(NvBootInfoTable, BootRomVersion, "       ");
    PRINT_OFFSET(NvBootInfoTable, DataVersion, "          ");
    PRINT_OFFSET(NvBootInfoTable, RcmVersion, "           ");
    PRINT_OFFSET(NvBootInfoTable, BootType, "             ");
    PRINT_OFFSET(NvBootInfoTable, PrimaryDevice, "        ");
    PRINT_OFFSET(NvBootInfoTable, SecondaryDevice, "      ");
    PRINT_OFFSET(NvBootInfoTable, OscFrequency, "         ");
    PRINT_OFFSET(NvBootInfoTable, DevInitialized, "       ");
    PRINT_OFFSET(NvBootInfoTable, SdramInitialized, "     ");
    PRINT_OFFSET(NvBootInfoTable, ClearedForceRecovery, " ");
    PRINT_OFFSET(NvBootInfoTable, ClearedFailBack, "      ");
    PRINT_OFFSET(NvBootInfoTable, InvokedFailBack, "      ");
    PRINT_OFFSET(NvBootInfoTable, BctValid, "             ");
    PRINT_OFFSET(NvBootInfoTable, BctStatus, "            ");
    PRINT_OFFSET(NvBootInfoTable, BctLastJournalRead, "   ");
    PRINT_OFFSET(NvBootInfoTable, BctBlock, "             ");
    PRINT_OFFSET(NvBootInfoTable, BctPage, "              ");
    PRINT_OFFSET(NvBootInfoTable, BctSize, "              ");
    PRINT_OFFSET(NvBootInfoTable, BctPtr, "               ");
    PRINT_OFFSET(NvBootInfoTable, BlState, "              ");
    PRINT_OFFSET(NvBootInfoTable, SecondaryDevStatus, "   ");
    PRINT_OFFSET(NvBootInfoTable, SafeStartAddr, "        ");
}

static char *
BootTypeName(NvBootType t)
{
    if (t < (sizeof(BootTypeTable)/sizeof(NvU8*)))
        return BootTypeTable[t];
    else return "Unknown";
}

static char *
BootDevTypeName(NvBootDevType t)
{
    if (t < (sizeof(BootDevTypeTable)/sizeof(NvU8*)))
        return BootDevTypeTable[t];
    else return "Unknown";
}

static char *
BootClocksOscFreqName(NvBootClocksOscFreq t)
{
    if (t < (sizeof(BootClocksOscFreqTable)/sizeof(NvU8*)))
        return BootClocksOscFreqTable[t];
    else return "Unknown";
}

static char *
BootRdrStatusName(NvBootRdrStatus t)
{
    if (t < (sizeof(BootRdrStatusTable)/sizeof(NvU8*)))
        return BootRdrStatusTable[t];
    else return "Unknown";
}

static void
PrintSpaces(NvU32 n)
{
    while(n--) NvAuPrintf(" ");
}

static void
PrintAndPadSpaces(char *s, NvU32 RegionWidth)
{
    NvU32 len = NvOsStrlen(s) + 1; // +1 is for colon
    NvU32 Spaces = (RegionWidth > len) ? (RegionWidth - len) : 1;

    NvAuPrintf("%s:", s);
    PrintSpaces(Spaces);
}

static void
PrintFieldAndPadSpaces(char *base, char *field, NvU32 RegionWidth)
{
    // +1 is for array reference and dot, +1 is for colon
    NvU32 len = NvOsStrlen(base) + 1 + NvOsStrlen(field) + 1;
    NvU32 Spaces = (RegionWidth > len) ? (RegionWidth - len) : 1;

    NvAuPrintf("%s.%s:", base, field);
    PrintSpaces(Spaces);
}

static void
PrintArrayFieldAndPadSpaces(char *array,
                            NvU32 idx,
                            char *field,
                            NvU32 RegionWidth)
{
    // +4 is for array reference and dot, +1 is for colon
    NvU32 len = NvOsStrlen(array) + 4 + NvOsStrlen(field) + 1;
    NvU32 Spaces = (RegionWidth > len) ? (RegionWidth - len) : 1;

    NvAuPrintf("%s[%d].%s:", array, idx, field);
    PrintSpaces(Spaces);
}

static void
DumpBasicContents(void)
{
    NvU32 i;

    NvAuPrintf("BIT Contents:\n");

    if (!EnableRegress)
    {
        PRINT_ITEM(NVU32, Bit, BootRomVersion);
    }

    PRINT_ITEM(NVU32, Bit, DataVersion);
    PRINT_ITEM(NVU32, Bit, RcmVersion);

    PRINT_ENUM(Bit, BootType,        BootTypeName);
    PRINT_ENUM(Bit, PrimaryDevice,   BootDevTypeName);
    PRINT_ENUM(Bit, SecondaryDevice, BootDevTypeName);
    PRINT_ENUM(Bit, OscFrequency,    BootClocksOscFreqName);

    PRINT_ITEM(BOOL, Bit, DevInitialized);
    PRINT_ITEM(BOOL, Bit, SdramInitialized);
    PRINT_ITEM(BOOL, Bit, ClearedForceRecovery);
    PRINT_ITEM(BOOL, Bit, ClearedFailBack);
    PRINT_ITEM(BOOL, Bit, InvokedFailBack);

    NvAuPrintf("\n");

    PRINT_ITEM(BOOL, Bit, BctValid);

    PRINT_NAME("BctStatus[]");
    for (i = 0; i < NVBOOT_BCT_STATUS_BYTES; i++)
    {
        NvAuPrintf("0x%02x ", Bit->BctStatus[i]);
    }
    NvAuPrintf("\n");

    PRINT_ENUM(Bit, BctLastJournalRead, BootRdrStatusName);

    PRINT_ITEM(NVU32, Bit, BctBlock);
    PRINT_ITEM(NVU32, Bit, BctPage);
    PRINT_ITEM(NVU32, Bit, BctSize);

    PRINT_ITEM(PTR,   Bit, BctPtr);

    for (i = 0; i < NVBOOT_MAX_BOOTLOADERS; i++)
    {
        NvAuPrintf("\n");

        PRINT_ARRAY_FIELD_ENUM(Bit,   BlState, i, Status, BootRdrStatusName);
        PRINT_ARRAY_FIELD_ITEM(NVU32, Bit, BlState, i, FirstEccBlock);
        PRINT_ARRAY_FIELD_ITEM(NVU32, Bit, BlState, i, FirstEccPage);
        PRINT_ARRAY_FIELD_ITEM(NVU32, Bit, BlState, i, FirstCorrectedEccBlock);
        PRINT_ARRAY_FIELD_ITEM(NVU32, Bit, BlState, i, FirstCorrectedEccPage);
        PRINT_ARRAY_FIELD_ITEM(BOOL,  Bit, BlState, i, HadEccError);
        PRINT_ARRAY_FIELD_ITEM(BOOL,  Bit, BlState, i, HadCrcError);
        PRINT_ARRAY_FIELD_ITEM(BOOL,  Bit, BlState, i, HadCorrectedEccError);
        PRINT_ARRAY_FIELD_ITEM(BOOL,  Bit, BlState, i, UsedForEccRecovery);
    }

    NvAuPrintf("\n");

//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FuseDataWidth);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FuseNumAddressCycles);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FuseDisableOnfiSupport);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FuseEccSelection);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FusePageSizeOffset);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FuseBlockSizeOffset);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FusePinmuxSelection);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, FusePinOrder);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, DiscoveredDataWidth);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, DiscoveredNumAddressCycles);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, DiscoveredEccSelection);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, IdRead);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, IdRead2);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, IsPartOnfi);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, NumPagesRead);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, NumUncorrectableErrorPages);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, NumCorrectableErrorPages);
//    PRINT_SECDEVSTATUS_ITEM(NVU32, Nand, MaxCorrectableErrorsEncountered);

    NvAuPrintf("\n");

    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, FuseDataWidth);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, FuseVoltageRange);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, FuseDisableBootMode);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, FuseDdrMode);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, DiscoveredCardType);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, DiscoveredVoltageRange);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, DataWidthUnderUse);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, PowerClassUnderUse);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, Cid[0]);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, Cid[1]);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, Cid[2]);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, Cid[3]);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, NumPagesRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, NumCrcErrors);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, BootFromBootPartition);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Sdmmc, BootModeReadSuccessful);

    NvAuPrintf("\n");

    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, ClockSource);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, ClockDivider);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, SnorConfig);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, TimingCfg0);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, TimingCfg1);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, LastBlockRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, LastPageRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Snor, SnorDriverStatus);


    NvAuPrintf("\n");

    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, ClockSource);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, ClockDivider);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, IsFastRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, NumPagesRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, LastBlockRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, LastPageRead);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, BootStatus);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, InitStatus);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, ReadStatus);
    PRINT_SECDEVSTATUS_ITEM(NVU32, Spi, ParamsValidated);

    NvAuPrintf("\n");

    PRINT_ITEM(NVU32, Bit, SafeStartAddr);

    NvAuPrintf("\n");
    NvAuPrintf("BCT Contents (a subset):\n");

    if (Bct && ((Bit->BctValid ||
        (ForceBctDump && Bit->BctSize > 0 && Bit->BctSize < 0x8000))))
    {
        PRINT_ITEM(NVU32,     Bct, BootDataVersion);
        PRINT_ITEM(LOG_BYTES, Bct, BlockSizeLog2);
        PRINT_ITEM(LOG_BYTES, Bct, PageSizeLog2);
        PRINT_ITEM(NVU32,     Bct, PartitionSize);
        PRINT_ITEM(NVU32,     Bct, NumParamSets);
        PRINT_ITEM(NVU32,     Bct, NumSdramSets);
        PRINT_ITEM(NVU32,     Bct, BootLoadersUsed);

        // TODO? DevType
        // TODO? DevParams
        // TODO? SdramParams

        for (i = 0; i < NVBOOT_MAX_BOOTLOADERS; i++)
        {
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, Version);
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, StartBlock);
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, StartPage);
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, Length);
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, LoadAddress);
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, EntryPoint);
            PRINT_ARRAY_FIELD_ITEM(NVU32, Bct, BootLoader, i, Attribute);
        }

        PRINT_ITEM(NVU32, Bct, BadBlockTable.EntriesUsed);
        PRINT_ITEM(NVU32, Bct, BadBlockTable.VirtualBlockSizeLog2);
        PRINT_ITEM(NVU32, Bct, BadBlockTable.BlockSizeLog2);

        // TODO? CustomerData
        PRINT_ITEM(BOOL,  Bct, EnableFailBack);

        PRINT_NAME("Reserved[]:");
        for (i = 0; i < NVBOOT_BCT_RESERVED_SIZE; i++)
        {
            NvAuPrintf("0x%02x ", Bct->Reserved[i]);
        }

        NvAuPrintf("\n");
    }
    else
    {
        NvAuPrintf("    No valid BCT was found.\n");
    }
}

/* Returns true if this was a good BCT */

static NvBool
BctReadStatus(NvU32 Block, NvU32 Slot)
{
    NvU32  BctNum;
    NvU32  Index;
    NvU8   Mask;
    NvBool Success;

    BctNum = (Block == 0) ? Slot : Block + 1;
    Index  = BctNum >> 3;         /* BctNum / 8 */
    Mask   = 1 << (BctNum & 0x7); /* BctNum % 8 */

    NvAuPrintf("    Read from (%d, %d): ", Block, Slot);

    Success = Bit->BctValid & (Bit->BctBlock == Block);
    if (Block == 0)
    {
        /* This avoids computing pages per BCT */
        if (Slot == 0) Success &= (Bit->BctPage == 0);
        else           Success &= (Bit->BctPage != 0);
    }

    if (Success)
    {
        NvAuPrintf("Success!\n");
        return NV_TRUE;
    }
    else
    {
        NvAuPrintf("%s Failure\n",
                   (Bit->BctStatus[Index] & Mask) ? "Read" : "Validation");
        return NV_FALSE;
    }
}

static char *
BctSearchTerminationReason(NvBootRdrStatus t)
{
    if (t < (sizeof(BctSearchTerminationTable)/sizeof(NvU8*)))
        return BootRdrStatusTable[t];
    else return "Unknown";
}


/* Analyze BCT behavior */
static void
AnalyzeBctData(void)
{
    NvU32  JournalBlock;
    NvBool IsGoodBct;

    NvAuPrintf("\nBCT Search Path:\n");

    if (Bit->BootType == NvBootType_Uart)
    {
        NvAuPrintf("    BootType was NvBootType_Uart.\n");
        NvAuPrintf("    -> No BCTs are loaded when booting from Uart\n");
        return;
    }

    if (Bit->ClearedForceRecovery)
    {
        NvAuPrintf("    BR entered RCM via forced recovery.\n");
        NvAuPrintf("    -> No BCTs are loaded under this condition.\n");
        return;
    }

    if (Bit->InvokedFailBack)
    {
        NvAuPrintf("    BR used FailBack to locate a valid BL.\n");
        return;
    }

    if (Bit->ClearedFailBack)
    {
        NvAuPrintf("    BR cleared the FailBack AO bit.\n");
        return;
    }

    if (Bit->ClearedForceRecovery)
    {
        NvAuPrintf("    BR entered RCM via forced recovery.\n");
        NvAuPrintf("    -> No BCTs are loaded under this condition.\n");
        return;
    }

    if (Bit->DevInitialized)
    {
        NvAuPrintf("    Device was successfully initialized.\n");
        NvAuPrintf("    -> Cold boot was attempted.\n");
    }
    else
    {
        NvAuPrintf("    Device was not successfully initialized.\n");
        NvAuPrintf("    -> Cold boot was not attempted or there was");
        NvAuPrintf("a device error during initialization.\n");
        return;
    }

    if (Bit->BctSize == 0)
    {
        NvAuPrintf("    BctSize == 0\n");
        NvAuPrintf("    -> No BCT loading was attempted.");
        return;
    }

    IsGoodBct = BctReadStatus(0, 0);
    if (IsGoodBct)
    {
        NvAuPrintf("    -> BCT was primary\n");
        return;
    }
    else
    {
        NvAuPrintf("->Device was uninitialized or an update was interrupted\n");
    }

    IsGoodBct = BctReadStatus(0, 1);
    if (IsGoodBct)
    {
        NvAuPrintf("    -> Update of JournalBlock was interrupted.\n");
        return;
    }
    else
    {
        NvAuPrintf(" -> BCT search continued by looking for journal block.\n");
    }

    for (JournalBlock = 1;
         JournalBlock < NVBOOT_MAX_BCT_SEARCH_BLOCKS;
         JournalBlock++)
    {
        IsGoodBct = BctReadStatus(JournalBlock, 0);
        if (IsGoodBct)
        {
            NvAuPrintf("    -> Journal Block located.\n");
            NvAuPrintf("    -> Good BCT located at (%d, %d)\n",
                       Bit->BctBlock, Bit->BctPage);

            NvAuPrintf("    -> Search within block terminated by %s\n",
                       BctSearchTerminationReason(Bit->BctLastJournalRead));
            return;
        }
        else
        {
            NvAuPrintf("    -> Not the Journal Block.\n");
        }
    }

    if (JournalBlock == NVBOOT_MAX_BCT_SEARCH_BLOCKS)
    {
        NvAuPrintf("    -> No Journal Block was found.\n");
        NvAuPrintf("    -> Fail to recovery mode.\n");
        return;
    }
}

/* Analyze SDRAM init */
static void
AnalyzeSdramData(void)
{
    NvAuPrintf("\nSDRAM init behavior:\n");

    if (!Bit->BctValid)
    {
        NvAuPrintf("    No valid BCT was found.\n");
        NvAuPrintf("    -> There are no parameters for SDRAM init.\n");
        NvAuPrintf("    -> SDRAM could not be initialized.\n");
        return;
    }

    if (Bct && (Bct->NumSdramSets == 0))
    {
        NvAuPrintf("    Number of parameter sets in BCT == 0.\n");
        NvAuPrintf("    -> There are no parameters for SDRAM init.\n");
        NvAuPrintf("    -> SDRAM could not be initialized.\n");
        return;
    }

    if (!Bit->SdramInitialized)
    {
        NvAuPrintf("    SDRAM was not initialized.\n");
        NvAuPrintf("    -> An error must have occured.\n");
        return;
    }

    NvAuPrintf("    SDRAM was succesfully initialized.\n");
    NvAuPrintf("    -> There are parameters for SDRAM init.\n");
    NvAuPrintf("    -> No errors occured.\n");
}

/* Analyze BL behavior */
static void
AnalyzeBlData(void)
{
    NvU32 BlIndex;
    NvU32 PrimaryBlIndex = 0;
    NvU32 Generation = 0;
    NvU32 CurrentVersion = 0;

    NvAuPrintf("\nBL Load Results:\n");

    if (!Bit->BctValid)
    {
        NvAuPrintf("    No valid BCT was found.\n");
        return;
    }

    if (Bct)
        CurrentVersion = Bct->BootLoader[0].Version;

    for (BlIndex = 0; BlIndex < NVBOOT_MAX_BOOTLOADERS; BlIndex++)
    {
        NvAuPrintf("    Bootloader %d:\n", BlIndex);

        /* Update the PrimaryBlIndex & Generation values. */
        if (Bct && (BlIndex > 0) &&
                Bct->BootLoader[BlIndex].Version != CurrentVersion)
        {
            CurrentVersion = Bct->BootLoader[BlIndex].Version;
            PrimaryBlIndex = BlIndex;
            Generation++;
        }

        NvAuPrintf("        This BL is %s copy of the generation %d BL.\n",
                   BlIndex == PrimaryBlIndex ? "the primary" :  "a redundant",
                   Generation);

        switch (Bit->BlState[BlIndex].Status)
        {
            case NvBootRdrStatus_None:
                NvAuPrintf("        There was no attempt to load this BL.\n");
                break;

            case NvBootRdrStatus_Success:
                NvAuPrintf("        This BL successfully loaded.\n");
                break;

            case NvBootRdrStatus_ValidationFailure:
                NvAuPrintf("        This BL was unable to load due");
                NvAuPrintf("to validation faliure.\n");
                break;
            case NvBootRdrStatus_DeviceReadError:
                NvAuPrintf("        This BL was unable to load due to");
                NvAuPrintf("unrecoverable device read error(s).\n");
                break;

            default:
                NvAuPrintf("        Illegal status code.\n");
                break;
        }

        if (Bit->BlState[BlIndex].HadEccError)
        {
            NvAuPrintf("        This BL had at least one device read error");
            NvAuPrintf("at block %d, page %d.\n",
                       Bit->BlState[BlIndex].FirstEccBlock,
                       Bit->BlState[BlIndex].FirstEccPage);
        }
        else
        {
            NvAuPrintf("        This BL had no device read errors.\n");
        }


        if (Bit->BlState[BlIndex].HadCorrectedEccError)
        {
            NvAuPrintf("        This BL had at least one correctable ECC error");
            NvAuPrintf("at block %d, page %d.\n",
                       Bit->BlState[BlIndex].FirstCorrectedEccBlock,
                       Bit->BlState[BlIndex].FirstCorrectedEccPage);
            NvAuPrintf("        This ECC is nearly uncorrectable.\n");
        }
        else
        {
            NvAuPrintf("        This BL had no correctable ECC errors.\n");
        }

        if (Bit->BlState[BlIndex].HadCrcError)
        {
            NvAuPrintf("        This BL had at least one CRC error.\n");
        }
        else
        {
            NvAuPrintf("        This BL had no CRC errors.\n");
        }

        NvAuPrintf("        This BL was %sused to recover from a read error.\n",
                   Bit->BlState[BlIndex].UsedForEccRecovery ? "" : "not ");
    }
}

void
NvFlashUtilT12xDumpBit(NvU8 * BitBuffer, const char *dumpOption)
{
    if ( NvOsStrcmp( dumpOption, "debug" ) == 0 )
        EnableDebug = NV_TRUE;
    else if ( NvOsStrcmp( dumpOption, "regress" ) == 0 )
        EnableRegress = NV_TRUE;
    else if ( NvOsStrcmp( dumpOption, "force" ) == 0 )
        ForceBctDump = NV_TRUE;

    if (EnableDebug)
    {
        /* Debugging information... */
        DumpStructureOffsets();
    }

    /* Set Bit and Bct pointers! */
    Bit = (NvBootInfoTable*)BitBuffer;
    if (Bit->BctSize > 0)
    {
        /* Bit->BCT is 'valid' */
        // Check for the proper bct pointer
        if ((PTR_TO_ADDR(Bit->BctPtr) >= MemoryBase))
            Bct = (NvBootConfigTable*)(BitBuffer +
                                   (PTR_TO_ADDR(Bit->BctPtr) - MemoryBase));
    }
    /* Dump basic contents. */
    DumpBasicContents();

    if (!EnableRegress)
    {
        /* Analyze BCT behavior */
        AnalyzeBctData();

        /* Analyze SDRAM init */
        AnalyzeSdramData();

        /* Analyze BL behavior */
        AnalyzeBlData();
    }

}


NvU32
NvFlashUtilT12xGetBctSize(void)
{
    return sizeof(NvBootConfigTable);
}

NvU32
NvFlashUtilT12xGetBlEntry(void)
{
    return NVFLASH_BL_ENTRY;
}

NvU32
NvFlashUtilT12xGetBlAddress(void)
{
    return NVFLASH_BL_ADDRESS;
}

void
NvFlashUtilT12xSetNCTCustInfoFieldsInBct(NctCustInfo NCTCustInfo,
    NvU8 * BctBuffer)
{
    NvU8 *ptr = NULL;
    NvBctAuxInfo *auxInfo = NULL;
    NvBootConfigTable *tempBct = NULL;

    /* Fill Customer data with NCTBoardInfo */
    tempBct = (NvBootConfigTable*)(BctBuffer);
    ptr = tempBct->CustomerData + sizeof(NvBctCustomerPlatformData);
    ptr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2) << 2);
    auxInfo = (NvBctAuxInfo *)ptr;

    auxInfo->NCTBoardInfo = NCTCustInfo.NCTBoardInfo;
    auxInfo->NctBatteryCountData = NCTCustInfo.BatteryCount;
    auxInfo->NctBatteryMakeData= NCTCustInfo.BatteryMake;
    auxInfo->NctDdrType= NCTCustInfo.DDRType;
}

NvBool NvFlashUtilGetT12xHal(NvFlashUtilHal *pHal)
{
    if (pHal->devId == 0x40)
    {
        pHal->GetBctSize = NvFlashUtilT12xGetBctSize;
        pHal->GetBlEntry= NvFlashUtilT12xGetBlEntry;
        pHal->GetBlAddress = NvFlashUtilT12xGetBlAddress;
        pHal->GetDumpBit = NvFlashUtilT12xDumpBit;
        pHal->SetNCTCustInfoFieldsInBct = NvFlashUtilT12xSetNCTCustInfoFieldsInBct;
        pHal->NvSecureSignWB0Image = NvFlashUtilT12xSecureSignWB0Image;
        return NV_TRUE;
    }
    return NV_FALSE;
}

NvError
NvFlashUtilT12xSecureSignWB0Image(
        NvU8* WB0Buf,
        NvU32 WB0Length)
{
    NvError e       = NvSuccess;
    NvU32 offset;
    NvU32 Key[4] = {0};
    NvU32 NumAesBlocks;
    NvU8 HashBuffer[NV3P_AES_HASH_BLOCK_LEN];
    NvBootWb0RecoveryHeader *pSrcHeader = (NvBootWb0RecoveryHeader*)WB0Buf;
    NvU32 *FuseWriteStart = NULL;

    /* The fusebypass info is stored after 4 bytes from the
        * end of the WB0 header. There is a jump instruction after the
        * WB0 Header in the warmboot code.
        */
    FuseWriteStart = (NvU32 *)(WB0Buf + sizeof(NvBootWb0RecoveryHeader) + 4);
    NvFlashWriteFusesToWarmBoot(FuseWriteStart);

    pSrcHeader->LengthInsecure     = WB0Length;
    pSrcHeader->LengthSecure       = WB0Length;
    pSrcHeader->Destination        = LP0_EXIT_RUN_ADDRESS;
    pSrcHeader->EntryPoint         = LP0_EXIT_RUN_ADDRESS;
    pSrcHeader->RecoveryCodeLength = WB0Length - sizeof(NvBootWb0RecoveryHeader);

    offset = NV_OFFSETOF(NvBootWb0RecoveryHeader, RandomAesBlock);
    NumAesBlocks = (WB0Length - offset) / NV3P_AES_HASH_BLOCK_LEN;
    NvBootHostCryptoEncryptSignBuffer(NV_FALSE, NV_TRUE, ((NvU8*)(Key)),
                           &WB0Buf[offset], NumAesBlocks, HashBuffer, NV_FALSE);

    /* Copy the generated hash into warm boot header */
    NvOsMemcpy(pSrcHeader->Signature.CryptoHash.hash, HashBuffer,
               NV3P_AES_HASH_BLOCK_LEN);
    return e;
}
