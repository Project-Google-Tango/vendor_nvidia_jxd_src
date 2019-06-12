/*
 * Copyright (c) 2006-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Platform Programming Protocol (Nv3p)</b>
 *
 * @b Description: Provides a packet-based communication protocol
 *    for a host PC to an NVIDIA Application Processor (NVAP).
 */
#ifndef INCLUDED_NV3P_H
#define INCLUDED_NV3P_H

/**
 * @defgroup nv3p_group Nv3p Protocol
 *
 * Defines a packet-based communication protocol for a host PC to an Nvidia
 * Application Processor (NVAP).
 *
 * The NVAP boot process is split into two parts:
 * - the mini-loader
 * - the full boot loader
 *
 * The mini-loader initializes memory and downloads
 * the full boot loader from the client. The transport connection is not
 * broken during the transition from mini-loader to full boot loader.
 *
 * Command and data packets may be encrypted when the ODM-secure fuse is blown
 * (see the nv3p_bytestream.h specification). When encryption is enabled, the
 * first error (NACK packet) causes the system to shutdown.
 *
 * ACK and NACK packets are always sent in plain text.
 *
 * @ingroup nv3p_modules
 * @{
 */
#include "nvcommon.h"
#include "nvos.h"
#include "nvbdktest.h"
#include "aos_avp_board_info.h"
#include "nvnct.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/** Defines the maximum length of a string, including null terminator. */
#define NV3P_STRING_MAX (64)

/** Defines the maximum length of partition name, including null terminator. */
#define MAX_PARTITION_NAME_LENGTH 4

/** Define the download chunk size at one go. */
#define NVFLASH_DOWNLOAD_CHUNK (1024 * 1024)
/**
 * Defines the Nv3p commands. The first set of enums are supported by the
 * mini-loader. The second set, are only supported by the boot loader. The
 * boot loader however supports all commands.
 */
typedef enum
{
    /** Supported by the mini-loader. */
    Nv3pCommand_GetPlatformInfo = 0x1,
    Nv3pCommand_GetBct,
    Nv3pCommand_GetBit,
    Nv3pCommand_DownloadBct,
    /** SetBlHash Deprecated **/
    Nv3pCommand_SetBlHash,
    Nv3pCommand_DownloadBootloader,
    Nv3pCommand_OdmOptions,
    Nv3pCommand_SetBootDevType,
    Nv3pCommand_SetBootDevConfig,

    /** Supported by the boot loader only (supports all commands). */
    Nv3pCommand_Status,
    Nv3pCommand_SetDevice,
    Nv3pCommand_StartPartitionConfiguration,
    /**
     * This command does not have paramters; use NULL --
     * must be sent after the last create partition command.
     */
    Nv3pCommand_EndPartitionConfiguration,
    Nv3pCommand_FormatPartition,
    Nv3pCommand_DownloadPartition,
    Nv3pCommand_QueryPartition,
    Nv3pCommand_CreatePartition,
    Nv3pCommand_ReadPartition,
    Nv3pCommand_SetBootPartition,
    Nv3pCommand_ReadPartitionTable,
    Nv3pCommand_DeleteAll,
    /**
     * Formats all partitions;
     * this command does not have paramters -- use NULL.
     */
    Nv3pCommand_FormatAll,
    /**
     * Deletes everything (including bad block tables);
     * this command does not have paramters -- use NULL.
     */
    Nv3pCommand_Obliterate,
    /**
     * The ODM command supported by both miniloader and boot loader.
     */
    Nv3pCommand_OdmCommand,
    /**
     * Exits the Nv3p server and boot;
     * this command does not have paramters -- use NULL.
     */
    Nv3pCommand_Go,
    /**
     * Commits changes to the BCT, etc., to mass-storage; enables
     * error handling corner-cases; Sync should be sent by the
     * client application before exiting;
     * this command does not have paramters -- use NULL.
     */
    Nv3pCommand_Sync,
    Nv3pCommand_VerifyPartitionEnable,
    Nv3pCommand_VerifyPartition,
    Nv3pCommand_EndVerifyPartition,
    Nv3pCommand_SetTime,
    Nv3pCommand_RawDeviceRead,
    Nv3pCommand_RawDeviceWrite,
    Nv3pCommand_UpdateBct,
    Nv3pCommand_GetDevInfo,
    /**
     * Stores the SKU info, serial ID, and MAC ID in the BCT structure.
     * This command takes the blob as input and updates the BCT.
     */
    Nv3pCommand_NvPrivData,
    Nv3pCommand_RunBDKTest,
    Nv3pCommand_GetNv3pServerVersion,
    Nv3pCommand_FuseWrite,
    Nv3pCommand_symkeygen,
    Nv3pCommand_DFuseBurn,
    /**
     * Dump RAM
     */
    Nv3pCommand_DumpRAM,
    Nv3pCommand_BDKGetSuiteInfo,
    Nv3pCommand_SkipSync,
    Nv3pCommand_Reset,
    Nv3pCommand_GetBoardDetails,
    Nv3pCommand_Recovery,
    Nv3pCommand_ReadBoardInfo,
    Nv3pCommand_SetBDKTest,
    Nv3pCommand_ReadNctItem,
    Nv3pCommand_WriteNctItem,
#if ENABLE_NVDUMPER
    Nv3pCommand_NvtbootRAMDump,
#endif

    /** Ignore -- Forces compilers to make 32-bit enums. */
    Nv3pCommand_Force32 = 0x7FFFFFFF,
} Nv3pCommand;

/** Defines the NACK code. */
typedef enum
{
    /* Nothing bad happened, this is here for completeness and for
     * ::Nv3pGetLastNackCode (if nothing was NACKed).
     */
    Nv3pNackCode_Success = 0x1,

    Nv3pNackCode_BadCommand,
    Nv3pNackCode_BadData,

    /** Ignore -- Forces compilers to make 32-bit enums. */
    Nv3pNackCode_Force32 = 0x7FFFFFFF,
} Nv3pNackCode;

/** Defines the partition types. */
typedef enum
{
    Nv3pPartitionType_Bct = 0x1,
    Nv3pPartitionType_Bootloader,
    Nv3pPartitionType_PartitionTable,
    Nv3pPartitionType_NvData,
    Nv3pPartitionType_Data,
    Nv3pPartitionType_Mbr,
    Nv3pPartitionType_Ebr,
    Nv3pPartitionType_GP1,
    Nv3pPartitionType_GPT,
    Nv3pPartitionType_BootloaderStage2,
    Nv3pPartitionType_Os,
    Nv3pPartitionType_FuseBypass,
    Nv3pPartitionType_ConfigTable,
    Nv3pPartitionType_WB0,

    Nv3pPartitionType_Force32 = 0x7FFFFFFF,
} Nv3pPartitionType;

/**
 * Defines the file system types.
 *
 * The internal file systems range is from 0x1 to 0x3FFFFFFF, and
 * the external file systems range is from 0x40000000 to 0x7FFFFFFF.
 *
 * @sa The NvFsMgrFileSystemType enumeration in nvfsmgr_defs.h.
 */
typedef enum
{
    Nv3pFileSystemType_Basic = 0x1,
#ifdef NV_EMBEDDED_BUILD
    Nv3pFileSystemType_Enhanced,
    Nv3pFileSystemType_Ext2,
    Nv3pFileSystemType_Yaffs2,
    Nv3pFileSystemType_Ext3,
    Nv3pFileSystemType_Ext4,
    Nv3pFileSystemType_Qnx,
#endif
    Nv3pFileSystemType_External = 0x40000000,

    Nv3pFileSystemType_Force32 = 0x7FFFFFFF,
} Nv3pFileSystemType;

/** Defines the allocation policy. */
typedef enum
{
    Nv3pPartitionAllocationPolicy_None = 0x1,
    Nv3pPartitionAllocationPolicy_Absolute,
    Nv3pPartitionAllocationPolicy_Sequential,

    Nv3pPartitionAllocationPolicy_Force32 = 0x7FFFFFFF,
} Nv3pPartitionAllocationPolicy;

/** Holds the handle to the Nv3p socket. */
typedef struct Nv3pSocketRec *Nv3pSocketHandle;

/**
 * Specifies the chip name.
 */
typedef enum
{
    Nv3pChipName_Development=0x00,

    Nv3pChipName_Apx2600=0x04,
    Nv3pChipName_Tegra600=0x08,
    Nv3pChipName_Tegra650=0x10,

    Nv3pChipName_Ap20 = 0x01,
    Nv3pChipName_T20 = 0x08,

    Nv3pChipName_Force32=0x7FFFFFFF,
} Nv3pChipName;

/**
 * Defines mass storage device types.
 */
typedef enum
{
    Nv3pDeviceType_Nand = 0x1,
    Nv3pDeviceType_Emmc,
    Nv3pDeviceType_Spi,
    Nv3pDeviceType_Ide,
    Nv3pDeviceType_Nand_x16,

    Nv3pDeviceType_Snor,
    Nv3pDeviceType_MuxOneNand,
    Nv3pDeviceType_MobileLbaNand,
    Nv3pDeviceType_Usb3,
    Nv3pDeviceType_Sata,
    Nv3pDeviceType_Force32 = 0x7FFFFFFF,
} Nv3pDeviceType;

/** Defines the Nv3p status. */
typedef enum
{
#define NV3P_STATUS(_name_, _value_, _desc_) Nv3pStatus_##_name_ = _value_,
    /* header included for macro expansion of status codes */
    #include "nv3p_status.h"
#undef NV3P_STATUS

    /// An alias for success.
    Nv3pOk = Nv3pStatus_Ok,

    Nv3pStatus_Force32 = 0x7FFFFFFF,
} Nv3pStatus;

/**
 * Defines the SBK status.
 */
typedef enum
{
    Nv3pSbkStatus_NotBurned = 0,
    Nv3pSbkStatus_Burned,
    Nv3pSbkStatus_Unknown,

    Nv3pSbkStatus_Force32 = 0x7FFFFFFF,
} Nv3pSbkStatus;

/**
 * Defines the DK status. Will be unknown if the SBK is nonzero
 * and the mode is ODM_Secure.
 */
typedef enum
{
    Nv3pDkStatus_NotBurned = 1,
    Nv3pDkStatus_Burned,
    Nv3pDkStatus_Unknown,

    Nv3pDkStatus_Force32 = 0x7FFFFFFF,
} Nv3pDkStatus;

/**
 * Defines the transport mode used by Nv3p.
 */
typedef enum
{
    Nv3pTransportMode_default=0x00,
    Nv3pTransportMode_Usb,
#if NVODM_BOARD_IS_FPGA
    Nv3pTransportMode_Jtag,
#endif
    Nv3pTransportMode_Sema,
    Nv3pTransportMode_Force32=0x7FFFFFFF,
} Nv3pTransportMode;

/** Defines the update BCT section types. */
typedef enum
{
    Nv3pUpdatebctSectionType_None = 0x0,
    Nv3pUpdatebctSectionType_Sdram,
    Nv3pUpdatebctSectionType_DevParam,
    Nv3pUpdatebctSectionType_BootDevInfo,
    Nv3pUpdatebctSectionType_BlInfo,

    Nv3pUpdatebctSectionType_Force32 = 0x7FFFFFFF,
} Nv3pUpdatebctSectionType;

/**
 * Defines the fuel gauge firmware upgrade and run SD diagnostics types.
 */
typedef enum
{
    Nv3pOdmExtCmd_FuelGaugeFwUpgrade,
    Nv3pOdmExtCmd_Reserved1,
    Nv3pOdmExtCmd_VerifySdram,
    Nv3pOdmExtCmd_Reserved2,
    Nv3pOdmExtCmd_Reserved3,
    Nv3pOdmExtCmd_Reserved4,
    Nv3pOdmExtCmd_UpdateuCFw,
    Nv3pOdmExtCmd_Get_Tnid,
    Nv3pOdmExtCmd_Verify_Tid,
    Nv3pOdmExtCmd_LimitedPowerMode,
    Nv3pOdmExtCmd_TegraTabFuseKeys,
    Nv3pOdmExtCmd_TegraTabVerifyFuse,
    Nv3pOdmExtCmd_UpgradeDeviceFirmware,

    Nv3pOdmExtCmd_Force32 = 0x7FFFFFFF,
} Nv3pOdmExtCmd;

/**
 * Defines the reset types.
 */
typedef enum
{
    Nv3pResetType_RecoveryMode = 0x1,
    Nv3pResetType_NormalBoot,

    Nv3pResetType_Force32 = 0x7FFFFFFF,
} Nv3pResetType;

/**
 * Holds the chip ID.
 */
typedef struct Nv3pChipIdRec
{
    NvU16 Id;
    NvU8 Major;
    NvU8 Minor;
} Nv3pChipId;

/**
 * Holds partition specific information
 */
typedef struct Nv3pPartitionInfoRec
{
    NvU32 PartId;
    NvU8 PartName[MAX_PARTITION_NAME_LENGTH];
    NvU32 DeviceId;
    NvU32 StartLogicalAddress;
    NvU32 NumLogicalSectors;
    NvU32 BytesPerSector;
    NvU32 StartPhysicalAddress;
    NvU32 EndPhysicalAddress;
} Nv3pPartitionInfo;

/**
 * Holds arguments for the ::Nv3pCommand functions that must be executed on a
 * separate thread.
 */
typedef struct Nv3pNonBlockingCmdsRec
{
    Nv3pCommand cmd;
    void* CmdArgs;
} Nv3pNonBlockingCmdsArgs;

/**
 * Holds the board specific information
 */
typedef struct Nv3pBoardDetailsRec
{
    /* Contains the board id, revision etc. */
    NvBoardInfo BoardInfo;
    /* CPU Speedo values */
    NvU32 CpuSpeedo[3];
    /* CPU Iddq value */
    NvU32 CpuIddq;
    /* SOC Speedo values */
    NvU32 SocSpeedo[3];
    /* SOC Iddq value */
    NvU32 SocIddq;
    /* Chip Sku */
    NvU32 ChipSku;
}Nv3pBoardDetails;

/**
 * Holds the chip's unique ID.
 */
typedef struct Nv3pChipUniqueIdRec
{
    NvU32 ecid_0;
    NvU32 ecid_1;
    NvU32 ecid_2;
    NvU32 ecid_3;
} Nv3pChipUniqueId;

/**
 * Holds the EEPROM Board ID.
 */
typedef struct Nv3pBoardIdRec
{
    NvU32 BoardNo;
    NvU32 BoardFab;
    NvU32 MemType;
    NvU32 Freq;
    NvU32 SkuType;
} Nv3pBoardId;

/*
 * Command arguments.
 */

/**
 * Holds high-level ACK/NACK for commands. This may be used in
 * the event of a mass-storage device failure, etc.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdStatusRec
{
    char Message[NV3P_STRING_MAX];
    Nv3pStatus Code;
    NvU32 Flags; // reseved for now
} Nv3pCmdStatus;

/**
 * Retrieves the system information. All paramters
 * are output parameters.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdGetPlatformInfoRec
{
    Nv3pChipUniqueId ChipUid;
    Nv3pChipId ChipId;
    NvU32 ChipSku;
    NvU32 BootRomVersion;

    Nv3pDeviceType SecondaryBootDevice;

    NvU32 OperatingMode;
    NvU32 DeviceConfigStrap;
    NvU32 DeviceConfigFuse;
    NvU32 SdramConfigStrap;

    NvBool HdmiEnable;
    NvBool MacrovisionEnable;
    NvBool SbkBurned;
    Nv3pDkStatus DkBurned;
    NvBool JtagEnable;
    Nv3pBoardId BoardId;
    NvU32 WarrantyFuse;

    /* Input */
    NvBool SkipAutoDetect; // Used to avoid boardId Read at ML
} Nv3pCmdGetPlatformInfo;

/**
 * Retrieves the system's BCT.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdGetBctRec
{
    /** Output. */
    NvU32 Length;
} Nv3pCmdGetBct;

/**
 * Retrieves the system's BIT.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdGetBitRec
{
    /** Output. */
    NvU32 Length;
} Nv3pCmdGetBit;

/**
 * Downloads the system's BCT.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdDownloadBctRec
{
    NvU32 Length;
} Nv3pCmdDownloadBct;

/**
 * Downloads BCT and sets the hash of downloading the boot loader
 * into system's BCT.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdBlHashRec
{
    NvU32 Length;
    NvU32 BlIndex;
} Nv3pCmdBlHash;

/**
 * Updates the system's BCT.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdUpdateBctRec
{
    NvU32 Length;
    Nv3pUpdatebctSectionType BctSection;
    NvU32 PartitionId;
} Nv3pCmdUpdateBct;

/**
 * Downloads the system's bootloader.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdDownloadBootloaderRec
{
    NvU64 Length;
    NvU32 Address; /**< Load address. */
    NvU32 EntryPoint; /**< Execution entry point. */
} Nv3pCmdDownloadBootloader;

/**
 * Configures a mass storage device.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdSetDeviceRec
{
    Nv3pDeviceType Type;
    NvU32 Instance;
} Nv3pCmdSetDevice;

/**
 * Writes data to the given partition ID.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdDownloadPartitionRec
{
    NvU32 Id;
    NvU64 Length;
} Nv3pCmdDownloadPartition;

/**
 * Retrieves paritition attributes.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdQueryPartitionRec
{
    /** Input. */
    NvU32 Id;

    /** Output. */
    NvU64 Size;
    NvU64 Address;
    NvU64 PartType;
} Nv3pCmdQueryPartition;

/**
 * Starts partition configuration.
 * Send this operation before creating parition commands.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdStartPartitionConfigurationRec
{
    NvU32 nPartitions;
} Nv3pCmdStartPartitionConfiguration;

/**
 * Creates a new partition.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdCreatePartitionRec
{
    /** Input. */
    char Name[NV3P_STRING_MAX];
    NvU64 Size;
    NvU64 Address;
    NvU32 Id;
    Nv3pPartitionType Type;
    Nv3pFileSystemType FileSystem;
    Nv3pPartitionAllocationPolicy AllocationPolicy;
    NvU32 FileSystemAttribute;
    NvU32 PartitionAttribute;
    NvU32 AllocationAttribute;
    NvU32 PercentReserved;
#ifdef NV_EMBEDDED_BUILD
    NvU32 IsWriteProtected;
#endif
} Nv3pCmdCreatePartition;


/**
 * Verifies the given partition.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdVerifyPartitionRec
{
    NvU32 Id;
} Nv3pCmdVerifyPartition;

/**
 * Reads the partition data.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdReadPartitionRec
{
    NvU32 Id;
    NvU64 Offset;
    NvU64 Length;
} Nv3pCmdReadPartition;

/**
 * Sets the boot partition.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdSetBootPartitionRec
{
    NvU32 Id;
    NvU32 LoadAddress;
    NvU32 EntryPoint;
    NvU32 Version;
    NvU32 Slot;
} Nv3pCmdSetBootPartition;

/**
 * Gets the partition table.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdReadPartitionTableRec
{
    /** Output. */
    NvU64 Length;
    NvU32 StartLogicalSector;
    NvU32 NumLogicalSectors;
    NvBool ReadBctFromSD;
} Nv3pCmdReadPartitionTable;

/**
 * Sets the ODM-customizable options.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdOdmOptionsRec
{
    NvU32 Options;
} Nv3pCmdOdmOptions;

/**
 * Sets the boot device type fuse value.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdSetBootDevTypeRec
{
    Nv3pDeviceType DevType;
} Nv3pCmdSetBootDevType;

/**
 * Sets the boot device configuration.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdSetBootDevConfigRec
{
    NvU32 DevConfig;
} Nv3pCmdSetBootDevConfig;

/**
 * Downloads the bqfs/dffs file
 * for the fuel gauge firmware upgrade.
 *
 * This structure is a part of ::Nv3pCmdOdmCommand.
 */
typedef struct Nv3pOdmExtCmdFuelGaugeFwUpgradeRec
{
   NvU64 FileLength1;
   NvU64 FileLength2;
} Nv3pOdmExtCmdFuelGaugeFwUpgrade;

typedef struct Nv3pOdmExtCmdTegraTabFuseKeysRec
{
   NvU32 FileLength;
} Nv3pOdmExtCmdTegraTabFuseKeys;

typedef struct Nv3pOdmExtCmdUpgradeDeviceFirmware
{
    NvU64 FileLength;
} Nv3pOdmExtCmdUpgradeDeviceFirmware;

/**
 * Runs SD diagnostics.
 *
 * This structure is a part of ::Nv3pCmdOdmCommand.
 */
typedef struct Nv3pOdmExtCmdRunSdDiagRec
{
   NvU32 Value;
   NvU32 TestType;
} Nv3pOdmExtCmdRunSdDiag;

/**
 * Runs SE diagnostics.
 *
 * This structure is a part of ::Nv3pCmdOdmCommand.
 */
typedef struct Nv3pOdmExtCmdRunSeDiagRec
{
   NvU32 Value;
} Nv3pOdmExtCmdRunSeDiag;

/**
 * Provides the length of the tnid
 */
typedef struct Nv3pOdmExtCmdGet_tnidRec
{
    NvU32 length;
} Nv3pOdmExtCmdGet_tnid;

/**
 * Verifies SDRAM initialization
 * for the SDRAM verification.
 *
 * This structure is a part of ::Nv3pCmdOdmCommand.
 */
typedef struct Nv3pOdmExtCmdVerifySdramRec
{
    NvU32 Value;
} Nv3pOdmExtCmdVerifySdram;

/**
 * Updates the thor microcontroller firmware.
 *
 * This structure is a part of ::Nv3pCmdOdmCommand.
 */

typedef struct Nv3pOdmExtCmdUpdateuCFwRec
{
    NvU32 FileSize;
} Nv3pOdmExtCmdUpdateuCFw;

/**
 * Holds data about the escape mechanism for protocol extension by the ODM.
 *
 * This structure should be passed into the ::Nv3pCommand functions.
*/
typedef struct Nv3pCmdOdmCommandRec
{
    NvU32 Command;
    NvU32 Data;
    Nv3pOdmExtCmd odmExtCmd;
    union
    {
        Nv3pOdmExtCmdFuelGaugeFwUpgrade fuelGaugeFwUpgrade;
        Nv3pOdmExtCmdRunSdDiag sdDiag;
        Nv3pOdmExtCmdVerifySdram verifySdram;
        Nv3pOdmExtCmdRunSeDiag seDiag;
        Nv3pOdmExtCmdUpdateuCFw updateuCFw;
        Nv3pOdmExtCmdGet_tnid tnid_info;
        Nv3pOdmExtCmdTegraTabFuseKeys tegraTabFuseKeys;
        Nv3pOdmExtCmdUpgradeDeviceFirmware upgradeDeviceFirmware;
    } odmExtCmdParam;
} Nv3pCmdOdmCommand;

/**
 * Holds data for formatting the requested partition.
 *
 * This structure should be passed into the ::Nv3pCommand functions.
 */
typedef struct Nv3pCmdFormatPartitionRec
{
    NvU32 PartitionId;
} Nv3pCmdFormatPartition;

/**
 *  Holds data used to set/get the system RTC.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdSetTimeRec
{
    NvU32   Seconds;
    NvU32   Milliseconds;
} Nv3pCmdSetTime;

/**
 * Holds data used to read/write data to any physical sector of the device
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdRawDeviceAccessRec
{
    /* input */
    NvU32   StartSector;
    NvU32 NoOfSectors;

    /* output */
    NvU64   NoOfBytes;
} Nv3pCmdRawDeviceAccess;

/**
 * Holds read device info.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdGetDevInfoRec
{
    NvU32 BytesPerSector;
    NvU32 SectorsPerBlock;
    NvU32 TotalBlocks;
} Nv3pCmdGetDevInfo;

/**
  * Holds data used during Skip Error check for comparing parition
  * layouts and in communicating PT attributes to \c LoadPartitionTable.
  */
typedef struct Nv3pPartitionTableLayoutRec
{
    NvU32 StartLogicalAddress;
    NvU32 NumLogicalSectors;
    NvBool ReadBctFromSD;
}Nv3pPartitionTableLayout;
/**
 * Downloads the NVIDIA internal blob.
 */
typedef struct Nv3pCmdNvPrivDataRec
{
    NvU32 Length;
} Nv3pCmdNvPrivData;

/**
 * Sends the test plan file.
 *
 * This structure must be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdRunBDKTestRec
{
    char Suite[NV3P_STRING_MAX];
    char Argument[NV3P_STRING_MAX];
    NvBDKTest_TestPrivData PrivData;
    NvU32 NumTests;
} Nv3pCmdRunBDKTest;


/**
 * Used to get the no. of suites for bdktest
 */
typedef struct Nv3pCmdGetSuitesInfoRec
{
    NvU32 size;
} Nv3pCmdGetSuitesInfo;


/**
 * Sets the bdk test environment
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 *
 * Timeout: It is for each testcase, second.
 */
typedef struct Nv3pCmdSetBDKTestRec
{
    NvBool StopOnFail;
    NvU32 Timeout;
} Nv3pCmdSetBDKTest;

/**
 * Sets the reset type and timeout.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdResetRec
{
    Nv3pResetType Reset;
    NvU32 DelayMs;
} Nv3pCmdReset;

/**
 * Gets Nv3pServer version.
 */
typedef struct Nv3pCmdGetNv3pServerVersionRec
{
    char Version[NV3P_STRING_MAX];
} Nv3pCmdGetNv3pServerVersion;

/*
 * Gives the length of the keys.
 */
typedef struct Nv3pCmdSymKeyGenRec
{
    NvU32 Length;
} Nv3pCmdSymKeyGen;

/**
 * Gives the fuse details to be written.
 *
 * This structure must be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdFuseWriteRec
{
    NvU32 Length;
} Nv3pCmdFuseWrite;

typedef struct Nv3pCmdDFuseBurnRec
{
    NvU32 bct_length;
    NvU32 fuse_length;
} Nv3pCmdDFuseBurn;

typedef struct Nv3pCmdReadWriteNctItemRec
{
    NvU32 EntryIdx;
    NvU32 Type;
    nct_item_type * Data;
} Nv3pCmdReadWriteNctItem;


#if ENABLE_NVDUMPER
/**
 * Retrieves the system's SDRAM Content.
 *
 * This structure should be passed into the ::Nv3pCommand functions
 * as a void pointer.
 */
typedef struct Nv3pCmdNvtbootRAMDumpRec
{
    NvU32 Offset;
    NvU32 Length;
} Nv3pCmdNvtbootRAMDump;
#endif

/**
 * Creates a 3P socket and connects it to the other side.
 *
 * @param[out] h3p A pointer to the handle to the socket state.
 * @param[in] mode The transport mode of Nv3p.
 * @param[in] instance The device instance number.
 */
NvError
Nv3pOpen( Nv3pSocketHandle *h3p, Nv3pTransportMode mode, NvU32 instance );

/**
 * Initialize handle to socket with socket state.
 *
 * @param[out] h3p A pointer to the handle to the socket state.
 * @param[in] mode The transport mode of Nv3p.
 * @param[in] instance The device instance number.
 */

NvError
Nv3pReOpen(Nv3pSocketHandle *h3p, Nv3pTransportMode mode, NvU32 instance);

/**
 * Closes the connection with the other side.
 *
 * @param h3p A handle to the socket state.
 */
void
Nv3pClose( Nv3pSocketHandle h3p );

/**
 * Sends a command packet.
 *
 * @param h3p A handle to the socket state.
 * @param command The command ID. One of ::Nv3pCommand.
 * @param args The command struture. This will be marshalled into the byte
 *      stream.
 * @param flags Reserved, must be zero.
 *
 * @return This returns NvSuccess if the receiver ACKs the packet or a the NACK
 * error code if not. This is a blocking interface.
 */
NvError
Nv3pCommandSend(
    Nv3pSocketHandle h3p,
    Nv3pCommand command,
    void *args,
    NvU32 flags );

/**
 * Receives a command packet.
 *
 * Received command packets must be ACKed or NACKed.
 *  @sa Nv3pAck, Nv3pNack
 *
 * @param h3p A handle to the socket state.
 * @param command [out] A pointer to the command that was received.
 * @param args [out] A pointer to the command arguments. This should be cast
 *      to the correct type depending on 'command'. The pointer should NOT
 *      be freed.
 * @param flags Reserved, must be zero.
 *
 */
NvError
Nv3pCommandReceive(
    Nv3pSocketHandle h3p,
    Nv3pCommand *command,
    void **args,
    NvU32 flags );

/**
 * Finishes command handling and sends the return data for a received command.
 *
 * This must be called for every received command.
 *
 * @param h3p A handle to the socket state.
 * @param command The command ID. One of ::Nv3pCommand.
 * @param args The command struture. The output data will be marshalled into
 *  the byte stream.
 * @param flags Reserved, must be zero.
 */
NvError
Nv3pCommandComplete(
    Nv3pSocketHandle h3p,
    Nv3pCommand command,
    void *args,
    NvU32 flags );

/**
 * Sends a data packet.
 *
 * @param h3p A handle to the socket state.
 * @param data The data to send.
 * @param length The length of the data in bytes.
 * @param flags Reserved, must be zero.
 *
 * @return NvSuccess if the receiver ACKs the packet or a NACK
 * error code if not. This is a blocking interface.
 */
NvError
Nv3pDataSend(
    Nv3pSocketHandle h3p,
    NvU8 *data,
    NvU32 length,
    NvU32 flags );

/**
 * Receives a data packet.
 *
 * Received data packets will be ACKed or NACKed automatically.
 *
 * @param h3p A handle to the socket state.
 * @param data The data to send.
 * @param length The length of the data in bytes. \a length may be of
 *    any size, regardless of the data packet size.
 * @param bytes The number of bytes received.
 * @param flags Reserved, must be zero.
 */
NvError
Nv3pDataReceive(
    Nv3pSocketHandle h3p,
    NvU8 *data,
    NvU32 length,
    NvU32 *bytes,
    NvU32 flags );

/**
 * Fails a data send or receive.
 *
 * This will abort the current data transfer operation and send
 * a NACK to other side.
 *
 * @param h3p A handle to the socket state.
 * @param code The NACK code to send to the sender.
 */
void
Nv3pTransferFail( Nv3pSocketHandle h3p, Nv3pNackCode code );

/**
 * Acknowleges a successful processing of a command or data packet.
 *
 * @param h3p A handle to the socket state.
 */
void
Nv3pAck( Nv3pSocketHandle h3p );

/**
 * Indicates failure to the sender of a command or data packet.
 *
 * @param h3p A handle to the socket state.
 * @param code The error code.
 */
void
Nv3pNack(
    Nv3pSocketHandle h3p,
    Nv3pNackCode code );

/**
 * Retreives the last NACK code.
 *
 * @param h3p A handle to the socket state.
 */
Nv3pNackCode
Nv3pGetLastNackCode( Nv3pSocketHandle h3p );

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NV3P_H
