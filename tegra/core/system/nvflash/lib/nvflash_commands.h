/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_COMMANDS_H
#define INCLUDED_NVFLASH_COMMANDS_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvnct.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* maximum partitions that can be created using GPT partition table */
#define MAX_GPT_PARTITIONS_SUPPORTED 128

typedef enum
{
    NvFlashCommand_Help = 0x1,
    NvFlashCommand_CmdHelp,
    NvFlashCommand_Create,
    NvFlashCommand_Obliterate,
    NvFlashCommand_FormatPartition,
    NvFlashCommand_Download,
    NvFlashCommand_Read,
    NvFlashCommand_GetPartitionTable,
    NvFlashCommand_SetBoot,
    NvFlashCommand_GetBct,
    NvFlashCommand_GetBit,
    NvFlashCommand_DumpBit,
    NvFlashCommand_SetOdmCmd,
    NvFlashCommand_VerifyPartition,
    NvFlashCommand_SetTime,
    NvFlashCommand_RawDeviceRead,
    NvFlashCommand_RawDeviceWrite,
    NvFlashCommand_Sync,
    NvFlashCommand_Go,
    /**
     * This uses to store the sku-info, serial-id, mac-id in the bct struct
     * This command will take the blob as input and update the bct
     */
    NvFlashCommand_NvPrivData,
    NvFlashCommand_RunBDKTest,
    NvFlashCommand_FuseWrite,
    NvFlashCommand_Symkeygen,
    NvFlashCommand_DFuseBurn,

    /* skips sync operation */
    NvFlashCommand_SkipSync,

    NvFlashCommand_Reset,
    NvFlashCommand_ReadNctItem,
    NvFlashCommand_WriteNctItem,
#if ENABLE_NVDUMPER
    NvFlashCommand_NvtbootRAMDump,
#endif
    NvFlashCommand_Force32 = 0x7FFFFFFF,
} NvFlashCommand;

typedef enum
{
    NvFlashOption_Bct = 0x1,
    NvFlashOption_ConfigFile,
    NvFlashOption_Bootloader,
    NvFlashOption_DiskImgOpt,
    NvFlashOption_OdmData,
    NvFlashOption_SetBootDevType,
    NvFlashOption_SetBootDevConfig,
    NvFlashOption_FormatAll,
    NvFlashOption_SetBct,
    NvFlashOption_EmulationDevId,
    NvFlashOption_TransportMode,
    NvFlashOption_DevParams,
    /* not really options, but close enough */
    NvFlashOption_Quiet,
    NvFlashOption_Wait,

    NvFlashOption_VerifyEnabled,

    NvFlashOption_DevInstance,

    /* passing entry and address thru command line */
    NvFlashOption_EntryAndAddress,
    NvFlashOption_Blob,

    /* sdram verification params for odm command */
    NvFlashOption_SetOdmCmdVerifySdram,
    NvFlashOption_OdmCmdVerifySdramVal,

    /* allow partition to be skipped using --create */
    NvFlashOption_SkipPartition,
#ifdef NV_EMBEDDED_BUILD
    NvFlashOption_NvFlash_BaseDir,
#endif
    NvFlashOption_FuseBypass,
    NvFlashOption_Freq,
    NvFlashOption_Buildbct,
    NvFlashOption_RamDump,
    NvFLashOption_VersionInfo,

    /* skips sync operation */
    NvFlashOption_SkipSync,
    /* for updating sections of bct */
    NvFlashOption_UpdateBct,
    NvFlashOption_SkipAutoDetect,

    /* Assigns default values to mandatory options
     * for create command.
     */
    NvFlashOption_AutoFlash,
    /* Stores the supplied NCT txt file */
    NvFlashOption_NCTPart,

    NvFlashOption_ListBoardId,
    NvFlashOption_DtbFileName,
    NvFlashOption_ReportFlashProgress,
    NvFlashOption_OdmCmdLimitedPowerMode,
    NvFlashOption_DeviceOpMode,
    NvFlashOption_AppletFile,
    NvFlashOption_Force32 = 0x7FFFFFFF,
} NvFlashOption;

typedef enum
{
    NvFlashTransportMode_default=0x00,
    NvFlashTransportMode_Usb,
#if NVODM_BOARD_IS_FPGA
    NvFlashTransportMode_Jtag,
#endif
    NvFlashTransportMode_Simulation,
    NvFlashTransportMode_Force32=0x7FFFFFFF,
} NvFlashTransportMode;

typedef enum
{
    NvFlashOdmExtCmd_FuelGaugeFwUpgrade,
    NvFlashOdmExtCmd_VerifySdram,
    NvFlashOdmExtCmd_UpdateuCFw,
    NvFlashOdmExtCmd_GetTN_ID,
    NvFlashOdmExtCmd_VerifyT_id,
    NvFlashOdmExtCmd_LimitedPowerMode,
    NvFlashOdmExtCmd_TegraTabFuseKeys,
    NvFlashOdmExtCmd_TegraTabVerifyFuse,
    NvFlashOdmExtCmd_UpgradeDeviceFirmware,
} NvFlashOdmExtCmd;

typedef enum
{
    NvFlashResetType_default=0x00,
    NvFlashResetType_RecoveryMode,
    NvFlashResetType_Normal,
    NvFlashResetType_Force32=0x7FFFFFFF,
} NvFlashResetType;

/* command parameters */
typedef struct NvFlashCmdDownloadRec
{
    NvU32 PartitionID;
    const char *filename;
    const char *PartitionName;
} NvFlashCmdDownload;

typedef struct NvFlashCmdSetBootRec
{
    NvU32 PartitionID;
    const char *PartitionName;
} NvFlashCmdSetBoot;

typedef struct NvFlashCmdFormatPartitionRec
{
    NvU32 PartitionID;
    const char *PartitionName;
} NvFlashCmdFormatPartition;

typedef struct NvFlashCmdReadRec
{
    NvU32 PartitionID;
    const char *filename;
    const char *PartitionName;
} NvFlashCmdRead;

typedef struct NvFlashCmdGetPartitionTableRec
{
    const char *filename;
    NvU32 StartLogicalSector;
    NvU32 NumLogicalSectors;
    NvBool ReadBctFromSD;
} NvFlashCmdGetPartitionTable;

typedef struct NvFlashOdmExtCmdFuelGaugeFwUpgradeRec
{
    const char *filename1;
    const char *filename2;
} NvFlashOdmExtCmdFuelGaugeFwUpgrade;

typedef struct NvFlashOdmExtCmdTegraTabFuseKeysRec
{
    const char *filename;
} NvFlashOdmExtCmdTegraTabFuseKeys;

typedef struct NvFlashOdmExtCmdUpgradeDeviceFirmwareRec
{
    const char *filename;
} NvFlashOdmExtCmdUpgradeDeviceFirmware;

typedef struct NvFlashOdmExtCmdVerifySdramRec
{
    NvU32 Value;
} NvFlashOdmExtCmdVerifySdram;

typedef struct NvFlashOdmExtCmdGetTNIDRec
{
    const char *filename;
}NvFlashOdmExtCmdGetTNID;

typedef struct NvFlashOdmExtCmdUpdateuCFwRec
{
    const char *filename;
} NvFlashOdmExtCmdUpdateuCFw;

typedef struct NvFlashCmdSetOdmCmdRec
{
    NvFlashOdmExtCmd odmExtCmd;
    union
    {
        NvFlashOdmExtCmdFuelGaugeFwUpgrade fuelGaugeFwUpgrade;
        NvFlashOdmExtCmdVerifySdram verifySdram;
        NvFlashOdmExtCmdUpdateuCFw updateuCFw;
        NvFlashOdmExtCmdGetTNID tnid_file;
        NvFlashOdmExtCmdTegraTabFuseKeys tegraTabFuseKeys;
        NvFlashOdmExtCmdUpgradeDeviceFirmware upgradedevicefirmware;
    } odmExtCmdParam;
} NvFlashCmdSetOdmCmd;

typedef struct NvFlashCmdDiskImageOptionsRec
{
    NvU32 BlockSize;
} NvFlashCmdDiskImageOptions;

typedef struct NvFlashCmdGetBitRec
{
    const char *filename;
} NvFlashCmdGetBit;

typedef struct NvFlashCmdDumpBitRec
{
    const char *DumpbitOption;
} NvFlashCmdDumpBit;

typedef struct NvFlashCmdHelpRec
{
    const char *cmdname;
} NvFlashCmdHelp;

typedef struct NvFlashCmdRawDeviceReadWriteRec
{
    NvU32 StartSector;
    NvU32 NoOfSectors;
    const char *filename;
} NvFlashCmdRawDeviceReadWrite;

typedef struct NvFlashCmdDevParamOptionsRec
{
    NvU32 PageSize;
    NvU32 BlockSize;
    NvU32 TotalBlocks;
} NvFlashCmdDevParamOptions;

#if ENABLE_NVDUMPER
typedef struct NvFlashNvtbootRAMRDumpRec
{
    NvU32 Offset;
    NvU32 Length;
    const char *filename;
} NvFlashCmdNvtbootRAMDump;
#endif

// To store input config file and [optional] output bct file names
typedef struct NvFlashBctFilesRec
{
    const char *BctOrCfgFile;
    const char *OutputBctFile;
} NvFlashBctFiles;

typedef struct NvFlashCmdBDKTestRec
{
    const char *cfgfilename;
    const char *outfilename;
    NvU32 opts;
} NvFlashCmdBDKTest;

/**
 * Structure to describe skipped partitions
 */
typedef struct NvFlashSkipPartRec
{
    /** Number of skipped partition(s) */
    NvU32 number;
    /** Name(s) of skipped partition(s) */
    const char *pt_name[MAX_GPT_PARTITIONS_SUPPORTED];
} NvFlashSkipPart;

/**
 * Structure to describe skipped partitions
 */
typedef struct NvFlashNCTPartRec
{
    /** Name  of NCT filename */
    const char *nctfilename;
} NvFlashNCTPart;

/**
 * Structure to describe NvInternal private data
 */
typedef struct NvFlashCmdNvPrivDataRec
{
    const char *filename;
} NvFlashCmdNvPrivData;

typedef struct NvFlashFuseBypass
{
    const char *config_file;
    const char *pSkuId;
    NvBool force_bypass;
    NvBool override_config_file;
    NvBool ForceDownload;
} NvFlashFuseBypass;

typedef struct NvFlashFuseWriteRec
{
    const char *config_file;
} NvFlashCmdFuseWrite;

/**
 * Structure to store symkeygen configfile
 */
typedef struct NvFlashCmdSymkeygenRec
{
    const char *spub_filename;
    const char *outfilename;
} NvFlashCmdSymkeygen;

typedef struct NvFlashCmdDFuseBurnRec
{
    const char *blob_dsession;
} NvFlashCmdDFuseBurn;

typedef struct NvFlashVersionStructRec
{
    NvBool IsVersionInfo;
    NvU32 MajorNum1;
    NvU32 MajorNum2;
} NvFlashVersionStruct;

/**
 * Structure to descript reset type
 */
typedef struct NvFlashCmdResetRec
{
    NvFlashResetType reset;
    NvU32 delay_ms;
} NvFlashCmdReset;

typedef struct NvFlashCmdReadWriteNctItemRec
{
    NvU32 EntryIdx;
    const char * Data;
} NvFlashCmdReadWriteNctItem;

/**
 * Defines the options for autoflash
 */
typedef struct NvFlashAutoFlashRec
{
    NvBool DoAutoFlash;
    NvU32 BoardId;
} NvFlashAutoFlash;

/**
 * Parse the command line. This must be done first.
 *
 * @param argc The number of arguments
 * @param argv String array of the arguments
 */
NvError
NvFlashCommandParse( NvU32 argc, const char *argv[] );

/**
 * Retrieve the number of commands parsed.
 */
NvU32
NvFlashGetNumCommands( void );

/**
 * Get the next command. Commands are in parse-order. This does no depencency
 * sanity checking.
 *
 * @param cmd Pointer to the parsed command
 * @param arg Pointer to the argument (if any, will be null if no argument is
 *      specified). This should be free'd after use.
 */
NvError
NvFlashCommandGetCommand( NvFlashCommand *cmd, void **arg );

/**
 * Retrieve a command-line option. 'data' will be null if the option was not
 * specified.
 *
 * @param opt The option to get
 * @param data Pointer to the option data
 */
NvError
NvFlashCommandGetOption( NvFlashOption opt, void **data );

/**
 * Retrieve an error string for the last error encountered. Will be "success"
 * if there is no error.
 */
const char *
NvFlashCommandGetLastError( void );

/**
 * Reverses a string
 *
 * @param pstr the string to be reversed
 */
void
NvFlashStrrev(char *pstr);

/**
 * Converts the usb path string into a decodable number
 * The input path string is in the format : ../xxx/yyy. This technique
 * encodes the string and results in a number : 1xxxyyy   . For e.g. input :
 * /dev/usb/bus/002/031 will give 1002031 as the encoded number
 *
 * @param usbpath the usb device path string given as input to instance
 */
NvU32
NvFlashEncodeUsbPath(const char *usbpath);

/**
 * Converts character to lower case character.
 * @param character character to be converted to lower case
 */
char NvFlashToLower(const char character);

/**
 * Case insensitive string comparision.
 * @params strings to be compared
 */
NvS32 NvFlashIStrcmp(const char *string1, const char *string2);

/**
 * Case insensitive string comparision upto specific number of characters
 *
 * @params pString1 & pString2, strings to be compared
 * @param NumChars number of chracters to be compared
 *
 * @return zero if both strings are same upto NumChars number of characters
 */
NvS32 NvFlashIStrncmp(const char *pString1, const char *pString2, NvU32 NumChars);

/**
 * Assigns the default values to all options mandatory for create command
 * if not provided.
 */
void NvFlashSetDefaultFlashValues(void);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVFLASH_COMMANDS_H

