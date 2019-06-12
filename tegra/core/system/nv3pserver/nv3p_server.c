/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nverror.h"
#include "nv3p.h"
#include "nvrm_memmgr.h"
#include "nvrm_module.h"
#include "nvrm_pmu.h"
#include "nvpartmgr.h"
#include "nvfs_defs.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvstormgr.h"
#include "nvsystem_utils.h"
#include "nvbct.h"
#include "nvbu.h"
#include "nvbl_query.h"
#include "nvbl_memmap_nvap.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_hash_defs.h"
#include "nvodm_pmu.h"
#include "nvcrypto_cipher.h"
#include "nv3p_server_utils.h"
#include "nvodm_fuelgaugefwupgrade.h"
#include "nvsku.h"
#include "nvddk_fuse.h"
#include "nvflash_version.h"
#include "nvddk_se_blockdev.h"
#include "nv_rsa.h"
#include "nvcrypto_common_defs.h"
#include "nv3p_server_private.h"
#include "fastboot.h"
#include "nvrm_hardware_access.h"
#include "arapb_misc.h"
#include "arfuse.h"
#include "arapbpm.h"
#include "nv3pserver.h"

#define NV_ADDRESS_MAP_MISC_BASE 0x70000000
#define NV_ADDRESS_MAP_FUSE_BASE 0x7000F800
#define NVBOOT_STRAP_SDRAM_CONFIGURATION_SHIFT (0)
#define NVBOOT_STRAP_SDRAM_CONFIGURATION_MASK (0x3)
#define NVBOOT_STRAP_DEVICE_CONFIGURATION_SHIFT (2)
#define NVBOOT_STRAP_DEVICE_CONFIGURATION_MASK (0x3)

#if NVODM_BOARD_IS_SIMULATION==0
#include "nvddk_issp.h"
#ifdef NV_USE_NCT
#include "nct.h"
#endif
#endif


#define SECONDS_BTN_1970_1980 315532800
#define SECONDS_BTN_1970_2009 1230768000
#define MICROBOOT_HEADER_SIGN 0xDEADBEEF
#define RSA_KEY_SIZE 256
#define RSA_KEY_PKT_KEY_SLOT_ONE 0
#define RSA_MAX_EXPONENT_SIZE 2048
#define USB_ALGINMENT (4 * 1024)

#if NVODM_BOARD_IS_SIMULATION==0
#include "nvrm_hardware_access.h"
#ifndef BIT
#define BIT(x) (1 << (x))
#endif
#ifndef PMC_PA_BASE
#define PMC_PA_BASE         0x7000E400  // Base address for arapbpm.h registers
#endif
#define   RECOVERY_MODE         BIT(31)
#define   BOOTLOADER_MODE       BIT(30)
#define   FORCED_RECOVERY_MODE  BIT(1)
#endif

#define CHECK_VERIFY_ENABLED() \
    if(gs_VerifyPartition) \
    { \
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, \
                    Nv3pStatus_InvalidCmdAfterVerify); \
    }

#define ALIGN_MEMORY(Ptr, Alignment)                        \
        Ptr = (NvU8 *)((((NvUPtr) (Ptr)) + ((NvUPtr)(Alignment) - 1)) & \
                ~((NvUPtr)(Alignment) - 1));

#ifndef TEST_RECOVERY
#define TEST_RECOVERY 0
#endif

#if ENABLE_THREADED_DOWNLOAD

/* Defines the number of circular buffers */
#define NUM_BUFFERS 3

/* Points to next buffer to be filled with data by main thread */
static NvU8 s_MainThreadNexBuffer = 0;

/* Points to next buffer to be epmtied by second thread by writing buffer
 * to storage device.
 */
static NvU8 s_WriteThreadNextBuffer = 0;
static NvOsThreadHandle s_hWriteToFileThread       = NULL;
static NvOsSemaphoreHandle s_WaitingForFullBuffer  = NULL;
static NvOsSemaphoreHandle s_WaitingForEmptyBuffer = NULL;
static NvRmMemHandle s_hRmMemHandle = NULL;

/* Defines structure which is shared between main thread and writer thread */
typedef struct CircularBufferConfigRec
{
    NvBool IsSparseImage;
    NvBool IsSparseStart;
    NvBool IsLastBuffer;
    NvBool SignRequired;
    NvU8  *Buff;
    NvU8  *HashPtr;
    NvU32 BytesToWrite;
    NvDdkFuseOperatingMode *pOpMode;
    NvStorMgrFileHandle hFile;
    NvError e;
}CircularBufferConfig;

static CircularBufferConfig s_CircularBuffer[NUM_BUFFERS];

/* Starting function of writer thread */
static void ThreadWriteBufferToFile(void *arg);

/* Initializes writer thread, semaphores for synchronization and resets the
 * number of free buffers to max buffers
 *
 * @return NvSucess if initialization is successfull else NvError
 */
static NvError InitParallelWrite(void);

/* Waits for all buffer to be emptied.
 *
 * @returns NvSuccess if no error by writer thread else NeError
 */
static NvError WaitForAllBufferEmpty(void);

/* Terminates the writer thread and deinitializes semaphores. */
static void StopWriterThread(void);

#endif

NV_CT_ASSERT((NvU64)(NV3P_STAGING_SIZE) < (NvU64)(~((NvU32)0x0)));

static NvBool gs_VerifyPartition = NV_FALSE;
static NvBool gs_VerifyPending = NV_FALSE;

/**
 * Hash for a particular partition
 */
typedef struct PartitionToVerifyRec
{
    NvU32 PartitionId;
    NvU8 PartitionHash[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    NvU64 PartitionDataSize;
    struct PartitionToVerify *pNext;
}PartitionToVerify;

/**
 * Parition Verification information
 */
typedef struct VerifyPartitionsRec
{
    /// Total number of partitions to be verified.
    NvU32 NumPartitions;
    /// Partition Ids to be verified.
    PartitionToVerify *LstPartitions;
}VerifyPartitions;

Nv3pServerState *s_pState;
static VerifyPartitions partInfo;
/**
 * Pointer to store the nvinternal data buf and its size
 */
static NvU8 *NvPrivDataBuffer = NULL;
static NvU32 NvPrivDataBufferSize = 0;

static
NvDdkFuseOperatingMode s_OpMode;

/**
 * Convert allocation policy from 3P-specific enum to NvPartMgr-specific enum
 *
 * @param AllocNv3p 3P allocation policy type
 * @param pAllocNvPartMgr pointer to NvPartMgr allocation policy type
 *
 * @retval NvSuccess Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable 3P allocation policy
 */
static NvError
ConvertAllocPolicy(
    Nv3pPartitionAllocationPolicy AllocNv3p,
    NvPartMgrAllocPolicyType *pAllocNvPartMgr);

/**
 * Convert file system type from 3P-specific enum to NvFsMgr-specific enum
 *
 * @param FsNv3p 3P file system type
 * @param pFsNvFsMgr pointer to NvFsMgr file system type
 *
 * @retval NvSuccess Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable 3P file system type
 */

static NvError
ConvertFileSystemType(
    Nv3pFileSystemType FsNv3p,
    NvFsMgrFileSystemType *pFsNvFsMgr);


/**
 * Convert partition type from 3P-specific enum to NvPartMgr-specific enum
 *
 * @param PartNv3p 3P partition type
 * @param pPartNvPartMgr pointer to NvPartMgr partition type
 *
 * @retval NvSuccess Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable 3P partition type
 */
static NvError
ConvertPartitionType(
    Nv3pPartitionType PartNv3p,
    NvPartMgrPartitionType *pPartNvPartMgr);


/**
 * Queries the boot device type from the boot information table, and returns
 * an NvBootDevType and instance for the boot device.
 *
 * @param bootDevId pointer to boot device type
 * @param bootDevInstance pointer to instance of blockDevId
 */
static NvError
GetSecondaryBootDevice(
    NvBootDevType *bootDevId,
    NvU32 *bootDevInstance);

/**
 * Unload partition table by resetting required global variables
 * required for --create in resume mode
 */
void
UnLoadPartitionTable(void);
/**
 * Allocate and initialize 3P Server's internal state structure
 *
 * Once allocated, pointer to state structure is stored in s_pState static
 * variable.
 *
 * @retval NvSuccess State allocated/initialized successfully
 * @retval NvError_InsufficientMemory Memory allocation failure
 */
static NvError
AllocateState(void);

/**
 * Deallocate 3P Server's internal state structure
 *
 * If state structure pointed to by s_pState is non-NULL, deallocates state
 * structure and its members.  s_pState static variable is set to NULL on
 * completion.
 */
static void
DeallocateState(void);

static NvError
UpdateBlInfo(Nv3pCmdDownloadPartition *a,
        NvU8 *hash,
        NvU32 padded_length,
        Nv3pStatus *status,
        NvBool IsMicroboot);
/**
 * Locates the partition by PartitionId in the list
 * of partitions to verify.
 *
 * @param PartitionId Id of partition to be located.
 * @param pPartition node in the list of partitions to verify (outputArg)
 *
 * @retval NvSuccess Partition was located
 * @retval NvError_BadParameter Unable to locate partition
 */
static NvError
LocatePartitionToVerify(NvU32 PartitionId, PartitionToVerify **pPartition);

/**
 * Reads and verifies the data of a partition.
 *
 * @param pPartitionToVerify Pointer to node in list of partitions to verify
 *
 * @retval NvSuccess Partition data was verified successfully.
 * @retval NvError_Nv3pBadReturnData Partition data verification failed.
 */
static NvError
ReadVerifyData(PartitionToVerify *pPartitionToVerify, Nv3pStatus *status);

/**
 * Verifies sign of the incoming data stream
 *
 * @param pBuffer pointer to data buffer
 * @param bytesToSign number of bytes in this part of the data stream.
 * @param StartMsg marks beginning of the data stream.
 * @param EndMsg Marks the end of the msg.
 * @param ExpectedHash Hash value expected.
 * @warning Caller of this function is responsible for setting
 *            StartMsg and EndMsg flags correctly.
 *
 * @retval NvSuccess if verification of the data was successful
 * @retval NvError_* There were errors verifying the data
 */
static NvError
VerifySignature(
    NvU8 *pBuffer,
    NvU32 bytesToSign,
    NvBool StartMsg,
    NvBool EndMsg,
    NvU8 *ExpectedHash);

/**
 * Adds the partition id to the list of partitions to verify
 *
 * @param PartitionId parition id to be added to the list of partitions to verify
 * @param pNode pointer to the node that was added to the list corresponding
         to the partitionId that was given.
 * @retval NvSuccess Partition id added successfully.
 * @retval NvError_InsufficientMemory Partition id could not be added due to
 *    lack of space.
 */
static NvError
SetPartitionToVerify(NvU32 PartitionId, PartitionToVerify **pNode);

/**
 * Initialize list of partitions to verify
 *
 * @retval None
 *
 */
static void
InitLstPartitionsToVerify(void);

/**
 * Free up the list of partitions to verify
 *
 * @retval None
 *
 */
static void
DeInitLstPartitionsToVerify(void);

/**
 * Upgrades the firmware of the FuelGauge.
 *
 * @param hSock Nv3p Socket handle.
 * @param command Nv3p Command.
 *
 * @retval NvSuccess Firmware upgraded successfully.
 * @retval NvError_* There was an error while upgrading the firmware.
 *
 */
static NvError
FuelGaugeFwUpgrade(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg);

/**
 * Upgdates the firmware of the uC.
 *
 * @param hSock Nv3p Socket handle.
 * @param command Nv3p Command.
 *
 * @retval NvSuccess Firmware upgraded successfully.
 * @retval NvError_* There was an error while updating the firmware.
 *
 */
static NvError
UpdateuCFw(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg);

#if !NVODM_BOARD_IS_SIMULATION
/* temporary functions for RAM dump */
static NvU8
NvBootStrapSdramConfigurationIndex(void);

static NvU8
NvBootStrapDeviceConfigurationIndex(void);

static NvBool
CheckIfHdmiEnabled(void);

static NvBool
CheckIfMacrovisionEnabled(void);

static NvBool
CheckIfJtagEnabled(void);
#endif /* !NVODM_BOARD_IS_SIMULATION */

/**
 * Writes buffer to a file. Uses thread if enabled
 *
 * @param hFile File handle
 * @param Buff Data to write
 * @paran BytesToWrite Bytes to write
 * @param HashBuf Buffer contining hash
 * @param IsSparseImage If true then buffer will be unsparsed
 * @param IsSparseStart Is sarting buffer of sparse image
 * @param IsLastBuffer Is last buffer of image
 * @param SignRequired Is signing required
 * @param pOpMode Operation mode of device
 * @param Status Error status will be written to this
 *
 * @returns NvSuccess if write successfule else NvError
 */
static NvError
Nv3pServerWriteToFile(
        NvStorMgrFileHandle hFile,
        NvU8 *Buff,
        NvU32 BytesToWrite,
        NvU8 *HashBuf,
        NvBool IsSparseImage,
        NvBool IsSparseStart,
        NvBool IsLastBuffer,
        NvBool SignRequired,
        NvDdkFuseOperatingMode *pOpMode,
        Nv3pStatus *Status);

static NvError
TegraTabFuseKeys(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg);

static NvError
TegraTabVerifyFuse(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg);

/* Read the BCT from SDMMC. This function does not replace the
 * in-memory BCT.
 *
 * @Return Buffer containig the BCT contents read from SD if
 *         no error else return NULL.
 */
static NvU8* ReadBctFromSD(void);

/**
 * Store the hash of bootloader & microboot present in host bct to device bct
 * @param hHostBct BctHandle of the host bct
 * @param PartitionId Partition Id of the bootloader partition

 * @returns NvSuccess if Hash is saved successfully in IRAM bct
 */

static NvError
StoreHashInfo(NvBctHandle hHostBct, NvU32 PartitionId);

static COMMAND_HANDLER(GetBct);
static COMMAND_HANDLER(DownloadBct);
static COMMAND_HANDLER(UpdateBct);
static COMMAND_HANDLER(SetDevice);
static COMMAND_HANDLER(DeleteAll);
static COMMAND_HANDLER(FormatAll);
static COMMAND_HANDLER(ReadPartitionTable);
static COMMAND_HANDLER(StartPartitionConfiguration);
static COMMAND_HANDLER(CreatePartition);
static COMMAND_HANDLER(EndPartitionConfiguration);
static COMMAND_HANDLER(QueryPartition);
static COMMAND_HANDLER(ReadPartition);
static COMMAND_HANDLER(RawWritePartition);
static COMMAND_HANDLER(RawReadPartition);
static COMMAND_HANDLER(DownloadPartition);
static COMMAND_HANDLER(SetBootPartition);
static COMMAND_HANDLER(OdmOptions);
static COMMAND_HANDLER(OdmCommand);
static COMMAND_HANDLER(Sync);
static COMMAND_HANDLER(Obliterate);
static COMMAND_HANDLER(VerifyPartitionEnable);
static COMMAND_HANDLER(VerifyPartition);
static COMMAND_HANDLER(FormatPartition);
static COMMAND_HANDLER(SetTime);
static COMMAND_HANDLER(GetDevInfo);
static COMMAND_HANDLER(DownloadNvPrivData);
static COMMAND_HANDLER(GetNv3pServerVersion);
static COMMAND_HANDLER(FuseWrite);
static COMMAND_HANDLER(GetPlatformInfo);
static COMMAND_HANDLER(SkipSync);
static COMMAND_HANDLER(Reset);
static COMMAND_HANDLER(ReadNctItem);
static COMMAND_HANDLER(WriteNctItem);

NvError
ConvertAllocPolicy(
    Nv3pPartitionAllocationPolicy AllocNv3p,
    NvPartMgrAllocPolicyType *pAllocNvPartMgr)
{

    switch (AllocNv3p)
    {
        case Nv3pPartitionAllocationPolicy_Absolute:
            *pAllocNvPartMgr = NvPartMgrAllocPolicyType_Absolute;
            break;

        case Nv3pPartitionAllocationPolicy_Sequential:
            *pAllocNvPartMgr = NvPartMgrAllocPolicyType_Relative;
            break;

        default:
            return NvError_BadParameter;
    }

    return NvSuccess;
}

NvError
ConvertFileSystemType(
    Nv3pFileSystemType FsNv3p,
    NvFsMgrFileSystemType *pFsNvFsMgr)
{

    switch (FsNv3p)
    {
        case Nv3pFileSystemType_Basic:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Basic;
            break;
#ifdef NV_EMBEDDED_BUILD
        case Nv3pFileSystemType_Enhanced:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Enhanced;
            break;

        case Nv3pFileSystemType_Ext2:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Ext2;
            break;

        case Nv3pFileSystemType_Yaffs2:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Yaffs2;
            break;

        case Nv3pFileSystemType_Ext3:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Ext3;
            break;

        case Nv3pFileSystemType_Ext4:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Ext4;
            break;

        case Nv3pFileSystemType_Qnx:
            *pFsNvFsMgr = NvFsMgrFileSystemType_Qnx;
            break;
#endif
        default:
            return NvError_BadParameter;
    }

    return NvSuccess;
}

NvError
ConvertPartitionType(
    Nv3pPartitionType PartNv3p,
    NvPartMgrPartitionType *pPartNvPartMgr)
{

    switch (PartNv3p)
    {
        case Nv3pPartitionType_Bct:
            *pPartNvPartMgr = NvPartMgrPartitionType_Bct;
            break;

        case Nv3pPartitionType_Bootloader:
            *pPartNvPartMgr = NvPartMgrPartitionType_Bootloader;
            break;

        case Nv3pPartitionType_BootloaderStage2:
            *pPartNvPartMgr = NvPartMgrPartitionType_BootloaderStage2;
            break;

        case Nv3pPartitionType_PartitionTable:
            *pPartNvPartMgr = NvPartMgrPartitionType_PartitionTable;
            break;

        case Nv3pPartitionType_NvData:
            *pPartNvPartMgr = NvPartMgrPartitionType_NvData;
            break;

        case Nv3pPartitionType_Data:
            *pPartNvPartMgr = NvPartMgrPartitionType_Data;
            break;

        case Nv3pPartitionType_Mbr:
            *pPartNvPartMgr = NvPartMgrPartitionType_Mbr;
            break;

        case Nv3pPartitionType_Ebr:
            *pPartNvPartMgr = NvPartMgrPartitionType_Ebr;
            break;
        case Nv3pPartitionType_GP1:
            *pPartNvPartMgr = NvPartMgrPartitionType_GP1;
            break;
        case Nv3pPartitionType_GPT:
            *pPartNvPartMgr = NvPartMgrPartitionType_GPT;
            break;
        case Nv3pPartitionType_FuseBypass:
            *pPartNvPartMgr = NvPartMgrPartitionType_FuseBypass;
            break;
        case Nv3pPartitionType_ConfigTable:
            *pPartNvPartMgr = NvPartMgrPartitionType_ConfigTable;
            break;
        case Nv3pPartitionType_WB0:
            *pPartNvPartMgr = NvPartMgrPartitionType_WB0;
            break;

        default:
            return NvError_BadParameter;
    }

    return NvSuccess;
}

NvError
ReportStatus(
    Nv3pSocketHandle hSock,
    char *Message,
    Nv3pStatus Status,
    NvU32 Flags)
{
    NvError e;
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

static NvError
StoreHashInfo(
    NvBctHandle hHostBct,
    NvU32 PartitionId)
{
    NvU32 Size = sizeof(NvU32);
    NvU32 InstanceHost = 0;
    NvU32 InstanceDev = 0;
    NvU32 BlAttributeHost;
    NvU32 BlAttributeDev;
    NvError e = NvSuccess;
    NvBool Found = NV_FALSE;
    NvBootHash Hash;

    NV_ASSERT(hHostBct);

    // Check if the bootloader partition Info exists in Host Bct
    for (InstanceHost = 0;
         InstanceHost < (NV3P_AES_HASH_BLOCK_LEN >> 2);
         InstanceHost++)
    {
        BlAttributeHost = 0;

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hHostBct, NvBctDataType_BootLoaderAttribute,
                     &Size, &InstanceHost, &BlAttributeHost));

        if (BlAttributeHost == PartitionId)
        {
            Found = NV_TRUE;
            break;
        }
    }

    if (!Found)
    {
        e = NvError_InvalidState;
        goto fail;
    }

    Found = NV_FALSE;

    // Check if the bootloader partition Info exists in IRAM
    for (InstanceDev = 0;
         InstanceDev < (NV3P_AES_HASH_BLOCK_LEN >> 2);
         InstanceDev++)
    {
        BlAttributeDev = 0;

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(s_pState->BctHandle,
                NvBctDataType_BootLoaderAttribute,
                    &Size, &InstanceDev, &BlAttributeDev));

        if (BlAttributeHost == BlAttributeDev)
        {
            Found = NV_TRUE;
            break;
        }
    }

    // Copy bootloader hash from Host Bct to IRAM
    if (Found)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hHostBct, NvBctDataType_BootLoaderCryptoHash,
                         &Size, &InstanceHost, &Hash));

        NV_CHECK_ERROR_CLEANUP(
           NvBctSetData(s_pState->BctHandle, NvBctDataType_BootLoaderCryptoHash,
                         &Size, &InstanceDev, &Hash));
    }
    else
    {
        e = NvError_InvalidState;
    }

fail:
    if (e)
    {
        NvOsDebugPrintf("Valid Bootloader Hash could not be found\n");
    }
    return e;
}

static NvError Nv3pGetDeviceInfo(
    NvDdkBlockDevInfo *pBlockDevInfo,
    Nv3pStatus *Status,
    char *Message)
{
    NvError e = NvSuccess;
    NvDdkBlockDevMgrDeviceId BlockDevId;
    NvBootDevType BootDevId;
    NvDdkBlockDevHandle hBlockDevHandle = NULL;
    NvU32 BlockDevInstance;
    Nv3pStatus s = Nv3pStatus_Ok;

    NV_CHECK_ERROR_FAIL_3P(
        GetSecondaryBootDevice(
            &BootDevId, &BlockDevInstance),
        Nv3pStatus_NotBootDevice);

    NV_CHECK_ERROR_FAIL_3P(
        NvBl3pConvertBootToRmDeviceType(
            BootDevId, &BlockDevId),
        Nv3pStatus_InvalidDevice);

    //open block device in raw access mode (partitionid=0)
    NV_CHECK_ERROR_FAIL_3P(
        NvDdkBlockDevMgrDeviceOpen(
            BlockDevId,
            BlockDevInstance,
            0,
            &hBlockDevHandle),
        Nv3pStatus_MassStorageFailure);

    // Get the device properties
    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
                           hBlockDevHandle, pBlockDevInfo);
fail:
    if (hBlockDevHandle)
        hBlockDevHandle->NvDdkBlockDevClose(hBlockDevHandle);
    *Status = s;
    return e;
}

static NvError Nv3pVerifyBlockSize(
    NvU32 PartitionId,
    Nv3pStatus *Status,
    char *Message)
{
    NvPartInfo PTInfo ;
    NvError e= NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU32 BlInstance = 0;
    NvU32 BlSize = sizeof(NvU32);
    NvU32 StartBlockBct = 0;
    NvU32 StartBlockDevice = 0;
    NvU32 BlockSize = 0;
    NvU32 Found = NV_FALSE;
    NvU32 BlId = 0;

    NvOsMemset((NvU8 *)(&PTInfo), 0x0, sizeof(NvPartInfo));

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetPartInfo(PartitionId, &PTInfo),
        Nv3pStatus_InvalidPartition
        );

    NvOsMemset((NvU8 *)(&BlockDevInfo), 0x0, sizeof(NvDdkBlockDevInfo));

    NV_CHECK_ERROR_CLEANUP(
        Nv3pGetDeviceInfo(&BlockDevInfo, &s, Message));

    for (BlInstance =0 ;BlInstance < 4; BlInstance++)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
            s_pState->BctHandle,
            NvBctDataType_BootLoaderAttribute,
            &BlSize,
            &BlInstance,
            &BlId));
        if (BlId == PartitionId)
        {
            Found = NV_TRUE;
            break;
        }
    }
    if (!Found)
    {
        e = NvError_BadParameter;
        s = Nv3pStatus_PartitionCreation;
        NvOsSnprintf(
            Message,
            NV3P_STRING_MAX,
            "Bootloader Partition no found in Bct",
            BlockDevInfo.SectorsPerBlock
        );
        NvOsDebugPrintf("Error: %s\n",Message);
    }

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(
        s_pState->BctHandle,
        NvBctDataType_BootLoaderStartBlock,
        &BlSize,
        &BlInstance,
        &StartBlockBct));

    BlInstance = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(
        s_pState->BctHandle,
        NvBctDataType_BootDeviceBlockSizeLog2,
        &BlSize,
        &BlInstance,
        &BlockSize));

    StartBlockDevice =
        (((NvU32)PTInfo.StartLogicalSectorAddress) *
            BlockDevInfo.BytesPerSector) / (1 << BlockSize);

    if (StartBlockDevice != StartBlockBct)
    {
        e = NvError_BadParameter;
        s = Nv3pStatus_PartitionCreation;
        NvOsSnprintf(
            Message,
            NV3P_STRING_MAX,
            "Run nvsecuretool with BlockSize of %d sectors",
            BlockDevInfo.SectorsPerBlock
        );
        NvOsDebugPrintf("Error: %s\n",Message);
    }
fail:
    *Status = s;
    return e;
}

COMMAND_HANDLER(GetBct)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};;

    CHECK_VERIFY_ENABLED();

    NV_CHECK_ERROR_FAIL_3P(NvError_NotImplemented,
        Nv3pStatus_NotImplemented);
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nGetBct failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(DownloadBct)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU8 *pData = NULL;
    NvU8 *pBct = NULL;
    NvU32 BytesLeftToTransfer = 0;
    NvU32 TransferSize = 0;
    NvU32 BctOffset = 0;
    NvBool IsFirstChunk = NV_TRUE;
    NvBool IsLastChunk = NV_FALSE;
    Nv3pCmdDownloadBct *a = (Nv3pCmdDownloadBct *)arg;
    NvU32 BctSize = 0;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvDdkFuseOperatingMode OpMode;
    NvU8 *pBlHash = NULL;

    if (!a->Length)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState,
            Nv3pStatus_InvalidState);

    Size = sizeof(NvU8);
    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(
            s_pState->BctHandle,
            NvBctDataType_BctSize,
            &Size,
            &Instance,
            &BctSize),
        Nv3pStatus_InvalidBCTSize);

    pBct = NvOsAlloc(a->Length);
    if (!pBct)
        NV_CHECK_ERROR_FAIL_3P(NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    if (a->Length != BctSize)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState,
            Nv3pStatus_InvalidState);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(
            NvDdkFuseDataType_OpMode,
            (void *)&OpMode,
            (NvU32 *)&Size));

    pBlHash = NvOsAlloc(NV3P_AES_HASH_BLOCK_LEN);
    if (!pBlHash)
    {
        e = NvError_InsufficientMemory;
        s = Nv3pStatus_InvalidState;
        NvOsSnprintf(
            Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
        goto clean;
    }

    BytesLeftToTransfer = a->Length;
    pData = pBct;
    do
    {
        TransferSize = (BytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                        NV3P_STAGING_SIZE :
                        BytesLeftToTransfer;

        NV_CHECK_ERROR_CLEAN_3P(
                    Nv3pDataReceive(hSock, pData, TransferSize, 0, 0),
                    Nv3pStatus_DataTransferFailure);

        if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
        {
            if (IsFirstChunk)
                BctOffset = NV3P_AES_HASH_BLOCK_LEN;
            else
                BctOffset = 0;

            if ((BytesLeftToTransfer - TransferSize) == 0)
                IsLastChunk = NV_TRUE;

            NV_CHECK_ERROR_CLEAN_3P(
                NvSysUtilSignData(
                    pData + BctOffset,
                    TransferSize - BctOffset,
                    IsFirstChunk,
                    IsLastChunk,
                    pBlHash,
                    &OpMode),
                Nv3pStatus_InvalidBCT);

            // need to decrypt the encrypted bct got from nvsbktool
            NV_CHECK_ERROR_CLEAN_3P(
                NvSysUtilEncryptData(
                    pData + BctOffset,
                    TransferSize - BctOffset,
                    IsFirstChunk,
                    IsLastChunk,
                    NV_TRUE,
                    OpMode),
                Nv3pStatus_InvalidBCT);
            // first chunk has been processed.
            IsFirstChunk = NV_FALSE;
        }
        pData += TransferSize;
        BytesLeftToTransfer -= TransferSize;
    } while (BytesLeftToTransfer);

    if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        // validate secure bct got from sbktool
        if (NvOsMemcmp(pBlHash, pBct, NV3P_AES_HASH_BLOCK_LEN))
        {
            e = NvError_BadParameter;
            s = Nv3pStatus_BLValidationFailure;
            NvOsSnprintf(
                Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
            goto clean;
        }
    }
    Size = BctSize;
    Instance = 0;

    NV_CHECK_ERROR_CLEAN_3P(
        NvBctSetData(
            s_pState->BctHandle,
            NvBctDataType_FullContents,
            &Size,
            &Instance,
            pBct),
        Nv3pStatus_InvalidBCT);

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
    }
clean:
    if(pBct)
        NvOsFree(pBct);
    if(pBlHash)
        NvOsFree(pBlHash);
    if (e)
        NvOsDebugPrintf("\nDownloadBct failed. NvError %u NvStatus %u\n", e, s);

    return ReportStatus(hSock, Message, s, 0);
}


COMMAND_HANDLER(UpdateBct)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU8 *pData = NULL;
    NvU32 BytesLeftToTransfer = 0;
    NvU32 TransferSize = 0;
    Nv3pCmdUpdateBct *a = (Nv3pCmdUpdateBct *)arg;
    NvU32 Bctsize = 0;
    NvU32 Size = sizeof(NvU8);
    NvU32 Instance = 0;
    NvBool IsFirstChunk = NV_TRUE;
    NvBool IsLastChunk = NV_FALSE;
    NvU32 BctOffset = 0;
    NvBctHandle hLocalBctHandle = NULL;
    NvBootHash HashSrc;
    NvBootHash HashSrcDev;
    NvU8 *pSignedSection = NULL;
    NvU8 *pUpdateBctData = NULL;
    NvU32 HostBctLength = 0;

    HostBctLength = a->Length;

    if (!HostBctLength)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidState);

    Size = sizeof(NvU8);
    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(
            s_pState->BctHandle,
            NvBctDataType_BctSize,
            &Size,
            &Instance,
            &Bctsize),
        Nv3pStatus_InvalidBCTSize);

    if (HostBctLength != Bctsize)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidState);

    pUpdateBctData = NvOsAlloc(HostBctLength);
    if (!pUpdateBctData)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);

    if (pUpdateBctData)
    {
        BytesLeftToTransfer = HostBctLength;
        pData = pUpdateBctData;
        do
        {
            TransferSize = (BytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                            NV3P_STAGING_SIZE :
                            BytesLeftToTransfer;

            NV_CHECK_ERROR_CLEAN_3P(
                Nv3pDataReceive(hSock, pData, TransferSize, 0, 0),
                Nv3pStatus_DataTransferFailure);

            if (s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
            {
                if (IsFirstChunk)
                    BctOffset = NvBctGetSignDataOffset();
                else
                    BctOffset = 0;

                if ((BytesLeftToTransfer - TransferSize) == 0)
                    IsLastChunk = NV_TRUE;

                NV_CHECK_ERROR_CLEAN_3P(
                    NvSysUtilSignData(
                        pData + BctOffset,
                        TransferSize - BctOffset,
                        IsFirstChunk,
                        IsLastChunk,
                        (NvU8*)&HashSrc,
                        &s_OpMode),
                    Nv3pStatus_InvalidBCT);

                // need to decrypt the encrypted bct
                NV_CHECK_ERROR_CLEAN_3P(
                    NvSysUtilEncryptData(
                        pData + BctOffset,
                        TransferSize - BctOffset,
                        IsFirstChunk,
                        IsLastChunk,
                        NV_TRUE,
                        s_OpMode),
                    Nv3pStatus_InvalidBCT);

                // first chunk has been processed.
                IsFirstChunk = NV_FALSE;
            }

            pData += TransferSize;
            BytesLeftToTransfer -= TransferSize;
        } while (BytesLeftToTransfer);
    }

    NV_CHECK_ERROR_CLEAN_3P(
        NvBctInit(
            &HostBctLength,
            pUpdateBctData,
            &hLocalBctHandle),
        Nv3pStatus_InvalidBCT);

    if (a->BctSection == Nv3pUpdatebctSectionType_BlInfo &&
        s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        Size = sizeof(NvU32);
        Instance = 0;

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
                hLocalBctHandle,
                NvBctDataType_CryptoHash,
                &Size,
                &Instance,
                &HashSrcDev));

        if (NvOsMemcmp(&HashSrc, &HashSrcDev, NV3P_AES_HASH_BLOCK_LEN))
        {
            e = NvError_BadParameter;
            s = Nv3pStatus_BLValidationFailure;
            goto clean;
        }

        NV_CHECK_ERROR_CLEAN_3P(
            StoreHashInfo(hLocalBctHandle, a->PartitionId),
            Nv3pStatus_InvalidState);
    }
    else if (a->BctSection == Nv3pUpdatebctSectionType_BlInfo &&
        s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NvU8 BctSig[RSA_KEY_SIZE] = {0};

        Size = 0;
        Instance = 0;

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
                hLocalBctHandle,
                NvBctDataType_SignedSection,
                &Size,
                &Instance,
                NULL));

        pSignedSection = NvOsAlloc(Size);

        if (!pSignedSection)
            NV_CHECK_ERROR_CLEAN_3P(
                NvError_InsufficientMemory,
                Nv3pStatus_BadParameter);

        NvOsMemset(pSignedSection, 0x0, Size);

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
                hLocalBctHandle,
                NvBctDataType_SignedSection,
                &Size,
                &Instance,
                pSignedSection));

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(
                s_pState->BctHandle,
                NvBctDataType_SignedSection,
                &Size,
                &Instance,
                pSignedSection));

        Size = RSA_KEY_SIZE;
        Instance = 0;

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(
                hLocalBctHandle,
                NvBctDataType_RsaPssSig,
                &Size,
                &Instance,
                BctSig));

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(
                s_pState->BctHandle,
                NvBctDataType_RsaPssSig,
                &Size,
                &Instance,
                BctSig));
    }
    else
    {
        if (s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
            NvBlUpdateBct(pUpdateBctData, a->BctSection);
    }
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nUpdateBct failed. NvError %u NvStatus %u\n", e, s);

    if (pUpdateBctData)
        NvOsFree(pUpdateBctData);
    if (pSignedSection)
        NvOsFree(pSignedSection);
    if (hLocalBctHandle)
        NvBctDeinit(hLocalBctHandle);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(SetDevice)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Nv3pCmdSetDevice *a = (Nv3pCmdSetDevice *)arg;

    CHECK_VERIFY_ENABLED();

    NV_CHECK_ERROR_FAIL_3P(
        NvBl3pConvert3pToRmDeviceType(
            a->Type,
            &s_pState->DeviceId),
        Nv3pStatus_InvalidDevice);

    s_pState->DeviceInstance = a->Instance;
    s_pState->IsValidDeviceInfo = NV_TRUE;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nSetDevice failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(DeleteAll)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvDdkBlockDevIoctl_FormatDeviceInputArgs InputArgs;
    NvDdkBlockDevHandle pDevHandle = NULL;
    char Message[NV3P_STRING_MAX] = {'\0'};

#ifdef NV_EMBEDDED_BUILD
    s = Nv3pStatus_NotSupported;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    return ReportStatus(hSock, Message, s, 0);
#endif

    //NvDdkBlockDevEraseType_GoodBlocks
    InputArgs.EraseType = NvDdkBlockDevEraseType_NonFactoryBadBlocks;

    CHECK_VERIFY_ENABLED();

    if (!s_pState->IsValidDeviceInfo)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidDevice);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkBlockDevMgrDeviceOpen(
            (NvDdkBlockDevMgrDeviceId)s_pState->DeviceId,
            s_pState->DeviceInstance,
            0,  /* MinorInstance */
            &pDevHandle),
        Nv3pStatus_MassStorageFailure);

    NV_CHECK_ERROR_FAIL_3P(
        pDevHandle->NvDdkBlockDevIoctl(
            pDevHandle,
            NvDdkBlockDevIoctlType_FormatDevice,
            sizeof(NvDdkBlockDevIoctl_FormatDeviceInputArgs),
            0,
            &InputArgs,
            NULL),
        Nv3pStatus_MassStorageFailure);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nDeleteAll failed. NvError %u NvStatus %u\n", e, s);
    if (pDevHandle)
        pDevHandle->NvDdkBlockDevClose(pDevHandle);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(FormatAll)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvU32 PartitionId = 0;
    NvU32 BctPartId;

    // error if PT not available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);

        s_pState->IsValidPt = NV_TRUE;
    }

    if (s_pState->IsCreatingPartitions)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidState);

    if (s_pState->NumPartitionsRemaining)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidState);

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetIdByName("BCT", &BctPartId),
        Nv3pStatus_InvalidState);

    PartitionId = NvPartMgrGetNextId(PartitionId);

    while(PartitionId)
    {
#ifdef NV_EMBEDDED_BUILD
        // Protect bct
        if (PartitionId != BctPartId)
#endif
        {
            NV_CHECK_ERROR_FAIL_3P(
                NvPartMgrGetNameById(PartitionId, FileName),
                Nv3pStatus_InvalidPartition);

            NV_CHECK_ERROR_FAIL_3P(
                NvStorMgrFormat(FileName),
                Nv3pStatus_InvalidPartition);
        }
        PartitionId = NvPartMgrGetNextId(PartitionId);
    }
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);

clean:
    if (e)
        NvOsDebugPrintf("\nFormatAll failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}


COMMAND_HANDLER(Obliterate)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvDdkBlockDevIoctl_FormatDeviceInputArgs InputArgs;
    NvDdkBlockDevHandle pDevHandle = NULL;
    char Message[NV3P_STRING_MAX] = {'\0'};
    InputArgs.EraseType = NvDdkBlockDevEraseType_NonFactoryBadBlocks;

    CHECK_VERIFY_ENABLED();

    if (!s_pState->IsValidDeviceInfo)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidDevice);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkBlockDevMgrDeviceOpen(
            (NvDdkBlockDevMgrDeviceId)s_pState->DeviceId,
            s_pState->DeviceInstance,
            0,  /* MinorInstance */
            &pDevHandle),
        Nv3pStatus_MassStorageFailure);

    NV_CHECK_ERROR_FAIL_3P(
        pDevHandle->NvDdkBlockDevIoctl(
            pDevHandle,
            NvDdkBlockDevIoctlType_FormatDevice,
            sizeof(NvDdkBlockDevIoctl_FormatDeviceInputArgs),
            0,
            &InputArgs,
            NULL),
        Nv3pStatus_MassStorageFailure);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (pDevHandle)
        pDevHandle->NvDdkBlockDevClose(pDevHandle);
    if (e)
        NvOsDebugPrintf("\nObliterate failed. NvError %u NvStatus %u\n", e, s);

    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(ReadPartitionTable)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvPartInfo PTInfo;
    NvU32 NumPartitions = 0;
    NvU32 PartitionCount = 0;
    NvU32 PartitionId = 0;
    NvU8* pPartitionData = 0;
    Nv3pPartitionInfo* pPartInfo = 0;
    Nv3pCmdReadPartitionTable *a = (Nv3pCmdReadPartitionTable*)arg;
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];

    CHECK_VERIFY_ENABLED();

    // verify PT is available
    if (!s_pState->IsValidPt)
    {
        Nv3pPartitionTableLayout arg;
        arg.NumLogicalSectors   = a->NumLogicalSectors;
        arg.StartLogicalAddress = a->StartLogicalSector;
        arg.ReadBctFromSD       = a->ReadBctFromSD;
        // Note : Incase of LoadPartitionTable call from nvflash_check_skiperror
        // partition table will get unloaded in the 'UnLoadPartitionTable()'call
        // in StartPartitionConfiguration.
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(&arg),
            Nv3pStatus_PartitionTableRequired);

        s_pState->IsValidPt = NV_TRUE;
    }

    PartitionId = 0;
    PartitionId = NvPartMgrGetNextId(PartitionId);
    while(PartitionId)
    {
        NumPartitions++;
        PartitionId = NvPartMgrGetNextId(PartitionId);
    }

    a->Length = sizeof(Nv3pPartitionInfo) * NumPartitions;
    pPartitionData = NvOsAlloc((NvU32)a->Length);
    if(!pPartitionData)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InsufficientMemory, Nv3pStatus_BadParameter);

    pPartInfo = (Nv3pPartitionInfo*)pPartitionData;

    PartitionId = 0;
    PartitionId = NvPartMgrGetNextId(PartitionId);
    while(PartitionId)
    {
        NvFsMountInfo minfo;
        NvDdkBlockDevInfo dinfo;
        NvDdkBlockDevHandle hDev = 0;

        NV_CHECK_ERROR_FAIL_3P(
            NvPartMgrGetPartInfo(PartitionId, &PTInfo),
            Nv3pStatus_InvalidPartition);

        NV_CHECK_ERROR_FAIL_3P(
            NvPartMgrGetNameById(PartitionId, FileName),
            Nv3pStatus_InvalidPartition);

        NV_CHECK_ERROR_FAIL_3P(
            NvPartMgrGetFsInfo(PartitionId, &minfo),
            Nv3pStatus_InvalidPartition);

        NV_CHECK_ERROR_FAIL_3P(
            NvDdkBlockDevMgrDeviceOpen(
                (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
                minfo.DeviceInstance,
                0,
                &hDev),
            Nv3pStatus_InvalidState);

        hDev->NvDdkBlockDevGetDeviceInfo(hDev, &dinfo);
        hDev->NvDdkBlockDevClose(hDev);

        NvOsMemcpy(
            pPartInfo[PartitionCount].PartName,
            FileName,
            MAX_PARTITION_NAME_LENGTH);
        pPartInfo[PartitionCount].PartId = PartitionId;
        pPartInfo[PartitionCount].DeviceId = minfo.DeviceId;

        pPartInfo[PartitionCount].StartLogicalAddress =
            (NvU32)PTInfo.StartLogicalSectorAddress;

        pPartInfo[PartitionCount].NumLogicalSectors =
            (NvU32)PTInfo.NumLogicalSectors;

        pPartInfo[PartitionCount].BytesPerSector = dinfo.BytesPerSector;;
        pPartInfo[PartitionCount].StartPhysicalAddress =
            (NvU32)PTInfo.StartPhysicalSectorAddress;

        pPartInfo[PartitionCount].EndPhysicalAddress =
            (NvU32)PTInfo.EndPhysicalSectorAddress;

        PartitionCount++;
        PartitionId = NvPartMgrGetNextId(PartitionId);
    }

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pDataSend(hSock, pPartitionData, (NvU32)a->Length, 0),
        Nv3pStatus_BadParameter);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nReadPartitionTable failed. NvError %u NvStatus %u\n", e, s);

    if(pPartitionData)
        NvOsFree(pPartitionData);

    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(StartPartitionConfiguration)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Nv3pCmdStartPartitionConfiguration *a =
        (Nv3pCmdStartPartitionConfiguration *)arg;

    CHECK_VERIFY_ENABLED();
    UnLoadPartitionTable();

    if (s_pState->IsValidPt)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_PartitionTableRequired);

    if (s_pState->IsCreatingPartitions)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidState);

    if (!a->nPartitions)
        NV_CHECK_ERROR_FAIL_3P(NvError_BadParameter, Nv3pStatus_BadParameter);

    s_pState->IsValidPt = NV_FALSE;
    s_pState->IsCreatingPartitions = NV_TRUE;
    s_pState->NumPartitionsRemaining = a->nPartitions;

    NV_CHECK_ERROR_FAIL_3P(
            NvPartMgrCreateTableStart(a->nPartitions),
            Nv3pStatus_PartitionCreation);

    NV_CHECK_ERROR_CLEAN_3P(
            Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nStartPartitionConfiguration failed. NvError %u NvStatus %u\n",
            e, s);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(SetTime)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdSetTime *a = (Nv3pCmdSetTime *)arg;
    NvOsOsInfo OsInfo;
    NvRmDeviceHandle pRmDevice = NULL;

    NV_CHECK_ERROR_FAIL_3P(
        NvOsGetOsInformation(&OsInfo),
        Nv3pStatus_BadParameter);
    //we are getting total time in seconds from nvflash host with base time as
    //1970 adjust time to base  to 1980 if we are running on windows
    if(OsInfo.OsType == NvOsOs_Windows)
        a->Seconds += SECONDS_BTN_1970_1980;

    //Setting Baseyear to 2009 to increase the clock life
    a->Seconds -= SECONDS_BTN_1970_2009;
#if NVODM_BOARD_IS_SIMULATION==0
    e = NvRmOpenNew(&pRmDevice);
    if (e != NvSuccess)
    {
        s = Nv3pStatus_InvalidState;
        NvOsSnprintf(
            Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
        goto fail1;
    }
    if (!NvRmPmuWriteRtc(pRmDevice, a->Seconds))
        goto fail1;
    NvRmClose(pRmDevice);
fail1:
   if (e != NvError_BadValue)
        NvRmClose(pRmDevice);
#endif
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nSetTime failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}


COMMAND_HANDLER(CreatePartition)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;

    Nv3pCmdCreatePartition *a = (Nv3pCmdCreatePartition *)arg;
    NvPartAllocInfo AllocInfo;
    NvFsMountInfo MountInfo;
    NvPartMgrPartitionType PartitionType = (NvPartMgrPartitionType)0; // invalid
    NvU8 *pCustomerData = NULL;
    char Message[NV3P_STRING_MAX] = {'\0'};

    CHECK_VERIFY_ENABLED();

    NvOsMemset(&MountInfo, 0, sizeof(NvFsMountInfo));
    // get device info
    if (!s_pState->IsValidDeviceInfo)
        NV_CHECK_ERROR_FAIL_3P(NvError_BadParameter, Nv3pStatus_InvalidDevice);

    // check that partition definition is in progress
    if (!s_pState->IsCreatingPartitions)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidState);

    // check that caller isn't attempting to create too many partitions
    if (!s_pState->NumPartitionsRemaining)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidState);

    NV_CHECK_ERROR_FAIL_3P(
        ConvertPartitionType(
            a->Type,
            &PartitionType),
        Nv3pStatus_BadParameter);

    // check that all special partitions are allocated on the boot device
    if (a->Type == Nv3pPartitionType_PartitionTable ||
        a->Type == Nv3pPartitionType_Bct)
    {
        NvBootDevType BootDevice;
        NvU32 BootInstance;

        NV_CHECK_ERROR_FAIL_3P(
            GetSecondaryBootDevice(&BootDevice, &BootInstance),
            Nv3pStatus_NotBootDevice);

        if (NvBlValidateRmDevice(BootDevice, s_pState->DeviceId) != NV_TRUE)
            NV_CHECK_ERROR_FAIL_3P(NvError_BadParameter,
                Nv3pStatus_InvalidDevice);
    }

    // populate mount information
    MountInfo.DeviceId = s_pState->DeviceId;
    MountInfo.DeviceInstance = s_pState->DeviceInstance;
#if NV3P_STRING_MAX < NVPARTMGR_MOUNTPATH_NAME_LENGTH
    NvOsStrncpy(MountInfo.MountPath, a->Name, NV3P_STRING_MAX);
    MountInfo.MountPath[NV3P_STRING_MAX] = '\0';
#else
    if(NvOsStrlen(a->Name) >= NVPARTMGR_MOUNTPATH_NAME_LENGTH)
    {
        NV_CHECK_ERROR_FAIL_3P(
            NvError_BadParameter,
            Nv3pStatus_InvalidPartitionName);
    }

    NvOsStrncpy(MountInfo.MountPath, a->Name, NVPARTMGR_MOUNTPATH_NAME_LENGTH);
#endif
    // FIXME -- change FileSystemType to an enum in NvFsMountInfo
    // Convert Nv File system types
    if (a->FileSystem < Nv3pFileSystemType_External)
    {
        NV_CHECK_ERROR_FAIL_3P(
            ConvertFileSystemType(
                a->FileSystem,
                (NvFsMgrFileSystemType *)&MountInfo.FileSystemType),
            Nv3pStatus_BadParameter);
    }
    else
    {
        MountInfo.FileSystemType = a->FileSystem;
    }
    MountInfo.FileSystemAttr = a->FileSystemAttribute;

    // populate allocation information
    NV_CHECK_ERROR_FAIL_3P(
            ConvertAllocPolicy(a->AllocationPolicy,
                        &AllocInfo.AllocPolicy),
                              Nv3pStatus_BadParameter);
    AllocInfo.StartAddress = a->Address;
    AllocInfo.Size = a->Size;
    AllocInfo.PercentReserved = a->PercentReserved;
    AllocInfo.AllocAttribute = a->AllocationAttribute;
#ifdef NV_EMBEDDED_BUILD
    AllocInfo.IsWriteProtected = a->IsWriteProtected;
#endif

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrAddTableEntry(
            a->Id,
            a->Name,
            PartitionType,
            a->PartitionAttribute,
            &AllocInfo,
            &MountInfo),
        Nv3pStatus_PartitionCreation);

    NV_CHECK_ERROR_CLEAN_3P(
         Nv3pCommandComplete(hSock, command, arg, 0),
         Nv3pStatus_CmdCompleteFailure);

    // latch info about special partitions
    if (a->Type == Nv3pPartitionType_PartitionTable)
    {
        NvBctAuxInfo *pAuxInfo;
        NvPartInfo PTInfo;
        NvU32 Size = 0;
        NvU32 Instance = 0;

        NV_CHECK_ERROR_CLEAN_3P(
            NvBctGetData(
                s_pState->BctHandle,
                NvBctDataType_AuxDataAligned,
                &Size,
                &Instance,
                NULL),
            Nv3pStatus_InvalidBCT
        );

        pCustomerData = NvOsAlloc(Size);

        if (!pCustomerData)
            NV_CHECK_ERROR_FAIL_3P(
                NvError_InsufficientMemory,
                Nv3pStatus_InvalidState);

        NvOsMemset(pCustomerData, 0, Size);

        // get the pCustomer data
        NV_CHECK_ERROR_CLEAN_3P(
            NvBctGetData(
                s_pState->BctHandle,
                NvBctDataType_AuxDataAligned,
                &Size,
                &Instance,
                pCustomerData),
            Nv3pStatus_InvalidBCT
        );

        //Fill out partinfo stuff
        pAuxInfo = (NvBctAuxInfo*)pCustomerData;

        NV_CHECK_ERROR_CLEAN_3P(
            NvPartMgrGetPartInfo(a->Id, &PTInfo),
            Nv3pStatus_InvalidPartition
            );

        pAuxInfo->StartLogicalSector = (NvU16)PTInfo.StartLogicalSectorAddress;
        NV_ASSERT(
            pAuxInfo->StartLogicalSector == PTInfo.StartLogicalSectorAddress);

        pAuxInfo->NumLogicalSectors = (NvU16)PTInfo.NumLogicalSectors;
        pAuxInfo->NumFuseSector = 0;
        pAuxInfo->StartFuseSector = 0;

        s_pState->PtPartitionId = a->Id;

        NV_CHECK_ERROR_CLEAN_3P(
            NvBctSetData(
                s_pState->BctHandle,
                NvBctDataType_AuxDataAligned,
                &Size,
                &Instance,
                pCustomerData),
            Nv3pStatus_InvalidBCT);
    }
    else if (a->Type == Nv3pPartitionType_Bct)
    {
        NvU8 BctPartId = (NvU8)a->Id;
        NvU32 Size, Instance = 0;

        Size = sizeof(NvU8);
        Instance = 0;
        NV_CHECK_ERROR_CLEAN_3P(
            NvBctSetData(
                s_pState->BctHandle,
                NvBctDataType_BctPartitionId,
                &Size,
                &Instance,
                &BctPartId),
            Nv3pStatus_InvalidBCT);
    }
    else if (a->Type == Nv3pPartitionType_Bootloader)
    {
        if (s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC ||
            s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecureSBK)
            NV_CHECK_ERROR_CLEANUP(
                Nv3pVerifyBlockSize(a->Id, &s, Message));

        if (s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC &&
            s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecureSBK)
        {
            NV_CHECK_ERROR_CLEAN_3P(
                NvBuAddBlPartition(s_pState->BctHandle, a->Id),
                Nv3pStatus_TooManyBootloaders);
        }
    }
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
    else if (a->Type == Nv3pPartitionType_BootloaderStage2)
    {
        NV_CHECK_ERROR_CLEAN_3P(
            NvBuAddHashPartition(s_pState->BctHandle, a->Id),
            Nv3pStatus_TooManyHashPartition);
    }
#endif
    else
    {
        //Unknown partition type
    }

    s_pState->NumPartitionsRemaining--;
fail:

    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nCreatePartition failed. NvError %u NvStatus %u\n", e, s);
    NvOsFree(pCustomerData);
    return ReportStatus(hSock, Message, s, 0);
}


COMMAND_HANDLER(EndPartitionConfiguration)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    // Force Signing of the Partition Table which is
    // necessary & sufficient for its verification.
    NvBool SignTable = NV_TRUE;
    NvBool EncryptTable = NV_FALSE;

    CHECK_VERIFY_ENABLED();

    if (s_pState->IsValidPt)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_PartitionTableRequired);

    if (!s_pState->IsCreatingPartitions)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidState);

    if (s_pState->NumPartitionsRemaining)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState, Nv3pStatus_InvalidState);

    if (!s_pState->PtPartitionId)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidPartition);

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrCreateTableFinish(),
        Nv3pStatus_PartitionCreation);

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrSaveTable(
            s_pState->PtPartitionId,
            SignTable,
            EncryptTable),
        Nv3pStatus_MassStorageFailure);

    s_pState->IsValidPt = NV_TRUE;
    s_pState->IsCreatingPartitions = NV_FALSE;
    s_pState->NumPartitionsRemaining = 0; // redundant, but add clarity

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean :
    if (e)
        NvOsDebugPrintf(
            "\nEndPartitionConfiguration failed. NvError %u NvStatus %u\n",
            e, s);

    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(FormatPartition)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];

    Nv3pCmdFormatPartition *a = (Nv3pCmdFormatPartition *)arg;

    // error if PT not available
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);
        s_pState->IsValidPt = NV_TRUE;

#ifdef NV_EMBEDDED_BUILD
        {
            NvPartInfo PartInfo;
            NV_CHECK_ERROR_FAIL_3P(
                NvPartMgrGetPartInfo(a->PartitionId, &PartInfo),
                Nv3pStatus_InvalidPartition);
            // Protect bct
            if (PartInfo.PartitionType == NvPartMgrPartitionType_Bct) {
                // just skip format
                NV_CHECK_ERROR_CLEAN_3P(
                    Nv3pCommandComplete(hSock, command, arg, 0),
                    Nv3pStatus_CmdCompleteFailure);
                goto fail;
            }
        }
#endif
    if (s_pState->IsCreatingPartitions)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidState);

    if (s_pState->NumPartitionsRemaining)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidState);

    if (!a->PartitionId)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InvalidState,
            Nv3pStatus_InvalidPartition);

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetNameById(a->PartitionId, FileName),
        Nv3pStatus_InvalidPartition);

    // PT partition has data before this call, hence it cannot be formatted
    if (s_pState->PtPartitionId != a->PartitionId)
    {
        NV_CHECK_ERROR_FAIL_3P(
            NvStorMgrFormat(FileName),
            Nv3pStatus_InvalidPartition);
    }

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nFormatPartition failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(QueryPartition)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdQueryPartition *a = (Nv3pCmdQueryPartition *)arg;
    NvPartInfo PartInfo;
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvFileStat FileStat;
    NvDdkBlockDevHandle pDevHandle = NULL;
    NvFsMountInfo FsMountInfo;
    NvDdkBlockDevInfo BlockDevInfo;

    // error if PT not available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);

        s_pState->IsValidPt = NV_TRUE;
    }

    // look up id; return error if not found
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetPartInfo(a->Id, &PartInfo),
        Nv3pStatus_InvalidPartition);

    // open device to query bytes per sector
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetFsInfo(a->Id, &FsMountInfo),
        Nv3pStatus_InvalidPartition);

    NV_CHECK_ERROR_FAIL_3P(
        NvDdkBlockDevMgrDeviceOpen(
            (NvDdkBlockDevMgrDeviceId)FsMountInfo.DeviceId,
            FsMountInfo.DeviceInstance,
            a->Id,
            &pDevHandle),
        Nv3pStatus_MassStorageFailure);

    pDevHandle->NvDdkBlockDevGetDeviceInfo(pDevHandle, &BlockDevInfo);
    a->Address =
        PartInfo.StartLogicalSectorAddress * BlockDevInfo.BytesPerSector;

    // get size of file from file system mounted on partition
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetNameById(a->Id, FileName),
        Nv3pStatus_InvalidPartition);

    NV_CHECK_ERROR_FAIL_3P(
        NvStorMgrFileQueryStat(FileName, &FileStat),
        Nv3pStatus_InvalidPartition);

    a->Size = FileStat.Size;
    a->PartType = PartInfo.PartitionType;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);
fail:

    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nQueryPartition failed. NvError %u NvStatus %u\n", e, s);
    if (pDevHandle)
        pDevHandle->NvDdkBlockDevClose(pDevHandle);
    return ReportStatus(hSock, Message, s, 0);
}


COMMAND_HANDLER(RawWritePartition)
{
    NvBootDevType BootDevId;
    NvDdkBlockDevMgrDeviceId BlockDevId;
    NvDdkBlockDevHandle hBlockDevHandle;
    NvU32 BlockDevInstance;
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdRawDeviceAccess *a = (Nv3pCmdRawDeviceAccess *)arg;
    NvU64 BytesRemaining;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU32 StartSector = a->StartSector;

    NV_CHECK_ERROR_FAIL_3P(
        GetSecondaryBootDevice(&BootDevId, &BlockDevInstance),
        Nv3pStatus_NotBootDevice);

    NV_CHECK_ERROR_FAIL_3P(
        NvBl3pConvertBootToRmDeviceType(BootDevId, &BlockDevId),
        Nv3pStatus_DeviceFailure);

    if(a->NoOfSectors <= 0)
    {
        e = NvError_BadParameter;
        s = Nv3pStatus_BadParameter;
        NvOsSnprintf(
            Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
        goto fail;
    }
//open block device in raw access mode (partitionid=0)
    NV_CHECK_ERROR_FAIL_3P(
        NvDdkBlockDevMgrDeviceOpen(
            BlockDevId,
            BlockDevInstance,
            0,
            &hBlockDevHandle),
        Nv3pStatus_InvalidState);
    /// Get the device properties
    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
                           hBlockDevHandle,
                           &BlockDevInfo);

    //return expected input data size to host
    a->NoOfBytes = (NvU64)a->NoOfSectors * BlockDevInfo.BytesPerSector;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    BytesRemaining = a->NoOfBytes;

    while (BytesRemaining)
    {
        NvU32 BytesToWrite;
        NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs WrPhysicalSectorArgs;
        WrPhysicalSectorArgs.SectorNum = StartSector;
        WrPhysicalSectorArgs.pBuffer = s_pState->pStaging;

        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToWrite = NV3P_STAGING_SIZE;
        else
            BytesToWrite = (NvU32)BytesRemaining;

        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pDataReceive(
                hSock,
                s_pState->pStaging,
                BytesToWrite,
                &BytesToWrite,
                0),
            Nv3pStatus_DataTransferFailure);

        WrPhysicalSectorArgs.NumberOfSectors =
            BytesToWrite/BlockDevInfo.BytesPerSector;

        //This write mechanism will work only with emmc  block device drivers.
        //It is not supported for nand block device drivers. Because for nand we
        //have take of bad blocks in case of raw read/write operations
        NV_CHECK_ERROR_CLEAN_3P(
            hBlockDevHandle->NvDdkBlockDevIoctl(
                hBlockDevHandle,
                NvDdkBlockDevIoctlType_WritePhysicalSector,
                sizeof(NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs),
                0,
                &WrPhysicalSectorArgs,
                NULL),
            Nv3pStatus_DataTransferFailure);

        BytesRemaining -= BytesToWrite;
        StartSector += WrPhysicalSectorArgs.NumberOfSectors;
    }

fail:

    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean :
    if (e)
        NvOsDebugPrintf("\nRawWritePartition failed. NvError %u NvStatus %u\n",
            e, s);
    if(hBlockDevHandle)
        hBlockDevHandle->NvDdkBlockDevClose(hBlockDevHandle);

    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(RawReadPartition)
{
    NvBootDevType BootDevId;
    NvDdkBlockDevMgrDeviceId BlockDevId;
    NvDdkBlockDevHandle hBlockDevHandle;
    NvU32 BlockDevInstance;
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdRawDeviceAccess *a = (Nv3pCmdRawDeviceAccess *)arg;
    NvU64 BytesRemaining;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU32 StartSector = a->StartSector;

    NV_CHECK_ERROR_FAIL_3P(
        GetSecondaryBootDevice(&BootDevId, &BlockDevInstance),
            Nv3pStatus_NotBootDevice);

    NV_CHECK_ERROR_FAIL_3P(
        NvBl3pConvertBootToRmDeviceType(BootDevId, &BlockDevId),
            Nv3pStatus_DeviceFailure);

    if(a->NoOfSectors <= 0)
    {
        e = NvError_BadParameter;
        s = Nv3pStatus_BadParameter;
        NvOsSnprintf(
            Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
        goto fail;
    }

    //open block device in raw access mode (partitionid=0)
    NV_CHECK_ERROR_FAIL_3P(
        NvDdkBlockDevMgrDeviceOpen(
            BlockDevId,
            BlockDevInstance,
            0,
            &hBlockDevHandle),
        Nv3pStatus_InvalidState);

    // Get the device properties
    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
        hBlockDevHandle,
        &BlockDevInfo);

    //return expected input data size to host
    a->NoOfBytes = (NvU64)a->NoOfSectors * BlockDevInfo.BytesPerSector;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    BytesRemaining = a->NoOfBytes;

    while (BytesRemaining)
    {
        NvU32 BytesToRead;
        NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs RdPhysicalSectorArgs;

        RdPhysicalSectorArgs.SectorNum = StartSector;
        RdPhysicalSectorArgs.pBuffer = s_pState->pStaging;

        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToRead = NV3P_STAGING_SIZE;
        else
            BytesToRead = (NvU32)BytesRemaining;

        RdPhysicalSectorArgs.NumberOfSectors =
            BytesToRead/BlockDevInfo.BytesPerSector;
        //This write mechanism will work only with emmc block device drivers.
        //It does not support nand block device drivers. For nand we hav
        // take of bad blocks in case of raw read/write operations
        NV_CHECK_ERROR_FAIL_3P(
            hBlockDevHandle->NvDdkBlockDevIoctl(
                hBlockDevHandle,
                NvDdkBlockDevIoctlType_ReadPhysicalSector,
                sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs),
                0,
                &RdPhysicalSectorArgs,
                NULL),
            Nv3pStatus_DataTransferFailure);

        NV_CHECK_ERROR_FAIL_3P(
            Nv3pDataSend(hSock, s_pState->pStaging, (NvU32)BytesToRead, 0),
            Nv3pStatus_DataTransferFailure);

        BytesRemaining -= BytesToRead;
        StartSector += RdPhysicalSectorArgs.NumberOfSectors;
    }

fail:

    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nRawReadPartition failed. NvError %u NvStatus %u\n",
                                                                        e, s);
    if(hBlockDevHandle)
        hBlockDevHandle->NvDdkBlockDevClose(hBlockDevHandle);

    return ReportStatus(hSock, Message, s, 0);
}

NvU8* ReadBctFromSD()
{
    NvError e = NvSuccess;
    NvU32 BctLength = 0;
    NvU32 BufSize   = 0;
    NvU32 FuseSize  = sizeof(NvU32);
    NvBootDevType BootDevId;
    NvDdkBlockDevMgrDeviceId BlockDevId;
    NvDdkBlockDevHandle hBlockDevHandle = NULL;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU32 BlockDevInstance;
    NvU8 *pBctBuffer = NULL;
    NvU32 NumSectors = 0;
    NvDdkFuseOperatingMode OpMode;
    NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs RdPhysicalSectorArgs;

    /* Get the operatig mode of device. If secure device then
     * fail.
     */
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&OpMode, (NvU32 *)&FuseSize));

    /* Retrieve the size of BCT. */
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, NULL));
    NV_CHECK_ERROR_CLEANUP(
        GetSecondaryBootDevice(&BootDevId, &BlockDevInstance)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvBl3pConvertBootToRmDeviceType(BootDevId, &BlockDevId)
    );

    /* Initialise block device */
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen(
            BlockDevId,
            BlockDevInstance,
            0,
            &hBlockDevHandle));

    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
        hBlockDevHandle,
        &BlockDevInfo);

    NumSectors = BctLength / BlockDevInfo.BytesPerSector;
    /* Take ceil of above division operation. */
    if(BctLength > (NumSectors * BlockDevInfo.BytesPerSector))
            NumSectors++;
    /* Always allocate size multiple of number of sectors, as block
     * driver reads full sectors to buffer.
     */
    BufSize = NumSectors * BlockDevInfo.BytesPerSector;
    pBctBuffer = NvOsAlloc(BufSize);

    if(pBctBuffer == NULL)
        goto fail;
    NvOsMemset(pBctBuffer, 0, BufSize);

    /* Read the 0th sector where bct is stored. */
    RdPhysicalSectorArgs.SectorNum = 0;
    RdPhysicalSectorArgs.pBuffer   = pBctBuffer;
    RdPhysicalSectorArgs.NumberOfSectors = NumSectors;

    NV_CHECK_ERROR_CLEANUP (
        hBlockDevHandle->NvDdkBlockDevIoctl(
            hBlockDevHandle,
            NvDdkBlockDevIoctlType_ReadPhysicalSector,
            sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs),
            0,
            &RdPhysicalSectorArgs,
            NULL));

    if (OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NvU32 j = 0;
        NvU32 HashDataOffset;
        NvU32 HashDataLength;
        NvU32 Flag = 0;
        NvU32 Size= RSA_KEY_SIZE;
        NvU8 CryptoHashInBct[NvDdkSeAesConst_BlockLengthBytes] = {0};
        NvU8 BctHashComputed[NvDdkSeAesConst_BlockLengthBytes] = {0};
        NvBctHandle hLocalBctHandle;

        NV_CHECK_ERROR_CLEANUP (
            NvBctInit (&BctLength, pBctBuffer, &hLocalBctHandle)
        );
        NV_CHECK_ERROR_CLEANUP (
            NvBctGetData (
                hLocalBctHandle,
                NvBctDataType_HashDataOffset,
                &Size,
                &j,
                &HashDataOffset));

        NV_CHECK_ERROR_CLEANUP (
            NvBctGetData (
                hLocalBctHandle,
                NvBctDataType_HashDataLength,
                &Size,
                &j,
                &HashDataLength));

        NV_CHECK_ERROR_CLEANUP (
            NvBctGetData (
                hLocalBctHandle,
                NvBctDataType_CryptoHash,
                &Size,
                &j,
                &CryptoHashInBct));

        //compute Cmac Hash for signed section of BCT
        NvSysUtilSignData (
            pBctBuffer + HashDataOffset,
            HashDataLength,
            NV_TRUE,
            NV_TRUE,
            BctHashComputed,
            &OpMode);

        //Cpmpare hash computed against hash stored in BCT
        for (j = 0; j < NvDdkSeAesConst_BlockLengthBytes; j++)
        {
            if (BctHashComputed[j] != CryptoHashInBct[j])
            {
                Flag = 1;
                break;
            }
        }
        if (Flag)
        {
            NvOsDebugPrintf("\nBCT verification failure\n");
            goto fail;
        }
        else
            NvOsDebugPrintf("\nBCT verification success\n");
    }
    else
    {
        NvU32 BctSignature[RSA_KEY_SIZE / sizeof(NvU32)] = {0};
        NvU32 Size = RSA_KEY_SIZE;
        NvU32 j = 0;
        NvBctHandle hLocalBctHandle;
        NvU32 SignDataOffset;
        NvU32 SignDataLength;

        NV_CHECK_ERROR_CLEANUP (
            NvBctInit (&BctLength, pBctBuffer, &hLocalBctHandle)
        );
        NV_CHECK_ERROR_CLEANUP (
            NvBctGetData (
                hLocalBctHandle,
                NvBctDataType_RsaPssSig,
                &Size,
                &j,
                (NvU8 *)BctSignature));

        NV_CHECK_ERROR_CLEANUP (
            NvBctGetData (
                hLocalBctHandle,
                NvBctDataType_HashDataOffset,
                &Size,
                &j,
                &SignDataOffset));

        NV_CHECK_ERROR_CLEANUP (
            NvBctGetData (
                hLocalBctHandle,
                NvBctDataType_HashDataLength,
                &Size,
                &j,
                &SignDataLength));

#if !NVODM_BOARD_IS_SIMULATION
        //Verify RSA_PSS signature for signed section of BCT
        if (NvDdkSeRsaPssSignatureVerify (
                RSA_KEY_PKT_KEY_SLOT_ONE,
                RSA_MAX_EXPONENT_SIZE,
                (NvU32 *) (pBctBuffer + SignDataOffset),
                NULL,
                SignDataLength,
                (NvU32 *) BctSignature,
                NvDdkSeShaOperatingMode_Sha256,
                NULL,
                NvDdkSeShaResultSize_Sha256 / sizeof(NvU64)))
        {
            NvOsDebugPrintf("\nBCT Verification Failure\n");
            goto fail;
        }
        else
            NvOsDebugPrintf("\nBCT Verification Success\n");
#endif
    }
    goto clean;
fail:
    if(pBctBuffer)
    {
        NvOsFree(pBctBuffer);
        pBctBuffer = NULL;
    }
clean:
    if(hBlockDevHandle)
        hBlockDevHandle->NvDdkBlockDevClose(hBlockDevHandle);
    return pBctBuffer;
}

COMMAND_HANDLER(ReadPartition)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvBool IsGoodHeader = NV_FALSE;
    NvBool IsTransfer   = NV_FALSE;

    Nv3pCmdReadPartition *a = (Nv3pCmdReadPartition *)arg;
    NvPartInfo PartInfo;
    NvFileStat FileStat;

    NvU64 BytesRemaining;

    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];

    NvStorMgrFileHandle hFile = NULL;

    CHECK_VERIFY_ENABLED();

    // NOTE: treat command like "read file"; later come back and add a
    // "read raw partition call"

    // TODO -- API should return size itself, not take if from caller?

    // verify offset is zero; non-zero offset not supported
    if (a->Offset)
        NV_CHECK_ERROR_FAIL_3P(NvError_BadParameter, Nv3pStatus_BadParameter);

    a->Length = 0;

    // verify PT is available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);

        s_pState->IsValidPt = NV_TRUE;
    }

    // verify requested partition exists
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetPartInfo(a->Id, &PartInfo),
        Nv3pStatus_InvalidPartition);

    // get file size
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetNameById(a->Id, FileName),
        Nv3pStatus_InvalidPartition);

    NV_CHECK_ERROR_FAIL_3P(
        NvStorMgrFileQueryStat(FileName, &FileStat),
        Nv3pStatus_InvalidPartition);

    a->Length = FileStat.Size;

    // Decreasing bootloader partition (Bootloader or Microboot) size by 16
    // bytes while reading bootloader partition so as to avoid failure of
    // download command after read command for bootloader partition due to
    // addition of 16 bytes padding as a WAR of bootrom bug 378464
    // Decreasing it everytime we read this partition because whenever we read a
    // partition, we read whole parition
    if (PartInfo.PartitionType == NvPartMgrPartitionType_Bootloader)
        a->Length-= NV3P_AES_HASH_BLOCK_LEN;

    // return file size to nv3p client
    IsGoodHeader = NV_TRUE;
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    // open file
    NV_CHECK_ERROR_FAIL_3P(
        NvStorMgrFileOpen(
            FileName,
            NVOS_OPEN_READ,
            &hFile),
        Nv3pStatus_MassStorageFailure);

    BytesRemaining = a->Length;

    while (BytesRemaining)
    {
        NvU32 BytesToRead;
        NvU32 BytesRead = 0;

        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToRead = NV3P_STAGING_SIZE;
        else
            BytesToRead = (NvU32)BytesRemaining;

        NV_CHECK_ERROR_FAIL_3P(
            NvStorMgrFileRead(
                hFile,
                s_pState->pStaging,
                BytesToRead,
                &BytesRead),
            Nv3pStatus_MassStorageFailure);

        NV_CHECK_ERROR_FAIL_3P(
            Nv3pDataSend(
                hSock,
                s_pState->pStaging,
                (NvU32)BytesRead,
                0),
            Nv3pStatus_DataTransferFailure);

        IsTransfer = NV_TRUE;

        BytesRemaining -= BytesRead;
    }

    IsTransfer = NV_FALSE;

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nReadPartition failed. NvError %u NvStatus %u\n", e, s);

    // dealloc any resources
    if (hFile)
    {
        e = NvStorMgrFileClose(hFile);
    }

    if (!IsGoodHeader)
    {
        //Do something interesting here
    }

    if (IsTransfer)
    {
      //Do something interesting here
    }
    /* If something fails before command complete then also need to send
     * command complete else next command will not work.
     */
    if(a->Length == 0)
        Nv3pCommandComplete(hSock, command, arg, 0);

    return ReportStatus(hSock, Message, s, 0);
}

#if !NVODM_BOARD_IS_SIMULATION
static NvU8
NvBootStrapSdramConfigurationIndex(void)
{
    NvU32 regVal ;

#ifdef PMC_BASE
    regVal = NV_READ32(PMC_BASE + APBDEV_PMC_STRAPPING_OPT_A_0);

    return (( NV_DRF_VAL(APBDEV_PMC, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_SDRAM_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_SDRAM_CONFIGURATION_MASK);
#else
    regVal = NV_READ32(
                NV_ADDRESS_MAP_MISC_BASE + APB_MISC_PP_STRAPPING_OPT_A_0);

    return ((NV_DRF_VAL(APB_MISC_PP, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_SDRAM_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_SDRAM_CONFIGURATION_MASK);
#endif
}

static NvU8
NvBootStrapDeviceConfigurationIndex(void)
{
    NvU32 regVal ;

#ifdef PMC_BASE
    regVal = NV_READ32(PMC_BASE + APBDEV_PMC_STRAPPING_OPT_A_0);
    return (( NV_DRF_VAL(APBDEV_PMC, STRAPPING_OPT_A, RAM_CODE, regVal) >>
            NVBOOT_STRAP_DEVICE_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_DEVICE_CONFIGURATION_MASK);
#else
    regVal = NV_READ32(
                NV_ADDRESS_MAP_MISC_BASE + APB_MISC_PP_STRAPPING_OPT_A_0);

    return ((NV_DRF_VAL(APB_MISC_PP, STRAPPING_OPT_A, RAM_CODE, regVal) >>
              NVBOOT_STRAP_DEVICE_CONFIGURATION_SHIFT) &
            NVBOOT_STRAP_DEVICE_CONFIGURATION_MASK);
#endif
}

static NvBool
CheckIfHdmiEnabled(void)
{
    NvU32 RegValue = NV_READ32 (
                        NV_ADDRESS_MAP_FUSE_BASE +  FUSE_RESERVED_PRODUCTION_0);
    NvBool hdmiEnabled = NV_FALSE;

    if (RegValue & 0x2)
        hdmiEnabled = NV_TRUE;

    return hdmiEnabled;
}

static NvBool
CheckIfMacrovisionEnabled(void)
{
    NvU32 RegValue = NV_READ32 (
                        NV_ADDRESS_MAP_FUSE_BASE +  FUSE_RESERVED_PRODUCTION_0);
    NvBool macrovisionEnabled = NV_FALSE;

    if (RegValue & 0x1)
        macrovisionEnabled = NV_TRUE;

    return macrovisionEnabled;
}

static NvBool
CheckIfJtagEnabled(void)
{
    NvU32 RegData;

    RegData = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_SECURITY_MODE_0);
    if (RegData)
        return NV_FALSE;
    else
        return NV_TRUE;
}
#endif /* !NVODM_BOARD_IS_SIMULATION */

COMMAND_HANDLER(GetPlatformInfo)
{
    NvError e;

    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU32 regVal;

    Nv3pCmdGetPlatformInfo a;

    NvOsMemset((void *)&a, 0, sizeof(Nv3pCmdGetPlatformInfo));
#if !NVODM_BOARD_IS_SIMULATION
    regVal = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + APB_MISC_GP_HIDREV_0) ;

    a.ChipId.Id = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, regVal);
    a.ChipId.Major = NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, regVal);
    a.ChipId.Minor = NV_DRF_VAL(APB_MISC, GP_HIDREV, MINORREV, regVal);

    a.ChipSku = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_SKU_INFO_0);

    a.BootRomVersion = 1;
    // Get Boot Device Id from fuses
    a.DeviceConfigStrap = NvBootStrapDeviceConfigurationIndex();

    a.SbkBurned = 0; // FIXME
    a.DkBurned = Nv3pDkStatus_Unknown; // FIXME
    a.OperatingMode = 0; // FIXME

    a.SdramConfigStrap = NvBootStrapSdramConfigurationIndex();
    a.HdmiEnable = CheckIfHdmiEnabled();
    a.MacrovisionEnable = CheckIfMacrovisionEnabled();
    a.JtagEnable= CheckIfJtagEnabled();
#endif /* !NVODM_BOARD_IS_SIMULATION */

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, &a, 0),
        Nv3pStatus_CmdCompleteFailure
    );

clean:
    if (e)
        NvOsDebugPrintf(
            "\nGetPlatformInfo failed. NvError %u NvStatus %u\n", e, s);

    return ReportStatus(hSock, Message, s, 0);
}

NvError
VerifySignature(
    NvU8 *pBuffer,
    NvU32 bytesToSign,
    NvBool StartMsg,
    NvBool EndMsg,
    NvU8 *ExpectedHash)
{
    NvError e = NvSuccess;
    //Need to use a constant instead of a value
    NvU32 paddingSize = NV3P_AES_HASH_BLOCK_LEN -
                            (bytesToSign % NV3P_AES_HASH_BLOCK_LEN);
    NvU8 *bufferToSign = NULL;
    static NvCryptoHashAlgoHandle pAlgo = (NvCryptoHashAlgoHandle)NULL;
    NvBool isValidHash = NV_FALSE;

    bufferToSign = pBuffer;

    if (StartMsg)
    {
        NvCryptoHashAlgoParams Params;
        NV_CHECK_ERROR_CLEANUP(NvCryptoHashSelectAlgorithm(
                                        NvCryptoHashAlgoType_AesCmac,
                                         &pAlgo));

        NV_ASSERT(pAlgo);

        // configure algorithm
        Params.AesCmac.IsCalculate = NV_TRUE;
        Params.AesCmac.KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
        Params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
        NvOsMemset(Params.AesCmac.KeyBytes, 0, sizeof(Params.AesCmac.KeyBytes));
        Params.AesCmac.PaddingType =
                                NvCryptoPaddingType_ExplicitBitPaddingOptional;

        NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));
    }
    else
    {
        NV_ASSERT(pAlgo);
    }

    // Since Padding for the cmac algorithm is going to be decided at runtime,
    // we need to compute padding size on the fly and decide on the padding
    // size accordingly.
    // Since the staging buffer is utilized whose size is always x16 bytes,
    // there's enough room for padding bytes.

    NV_CHECK_ERROR_CLEANUP(pAlgo->QueryPaddingByPayloadSize(pAlgo, bytesToSign,
                                        &paddingSize,
                                      ((NvU8 *)(bufferToSign) + bytesToSign)));

    bytesToSign += paddingSize;

    // process data
    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, bytesToSign,
                             (void *)(bufferToSign),
                             StartMsg, EndMsg));
    if (EndMsg == NV_TRUE)
    {
        e = pAlgo->VerifyHash(pAlgo,
                                    ExpectedHash,
                                    &isValidHash);
        if (!isValidHash)
        {
            e = NvError_InvalidState;
            NV_CHECK_ERROR_CLEANUP(e);
        }
    }

fail:
    if (EndMsg == NV_TRUE)
    {
        pAlgo->ReleaseAlgorithm(pAlgo);
    }
    if (e)
        NvOsDebugPrintf(
            "\nVerifySignature failed. NvError %u NvStatus %u\n", e, 0);
    return e;
}

NvError
ReadVerifyData(PartitionToVerify *pPartitionToVerify, Nv3pStatus *status)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvPartInfo PartInfo;
    NvU64 BytesRemaining;
    char Message[NV3P_STRING_MAX] = {'\0'};
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];

    NvStorMgrFileHandle hFile = NULL;
    NvBool StartMsg = NV_TRUE;
    NvBool EndMsg = NV_FALSE;

    BytesRemaining = pPartitionToVerify->PartitionDataSize;
    // verify requested partition exists
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetPartInfo(
            pPartitionToVerify->PartitionId,
            &PartInfo),
        Nv3pStatus_InvalidPartition);

    if (PartInfo.PartitionType == NvPartMgrPartitionType_Bootloader)
    {
        NvDdkFuseOperatingMode OpMode;
        NvU32 Size;

        Size =  sizeof(NvU32);
        NV_CHECK_ERROR_FAIL_3P(
            NvDdkFuseGet(
                NvDdkFuseDataType_OpMode,
                (void *)&OpMode,
                (NvU32 *)&Size),
            Nv3pStatus_BadParameter);

        NV_CHECK_ERROR_CLEANUP(NvSysUtilBootloaderReadVerify(
        s_pState->pStaging,
        pPartitionToVerify->PartitionId, OpMode, BytesRemaining,
        pPartitionToVerify->PartitionHash, NV3P_STAGING_SIZE));
        return e;
    }
    // get file size
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetNameById(
            pPartitionToVerify->PartitionId,
            FileName),
        Nv3pStatus_InvalidPartition);

    // open file
    NV_CHECK_ERROR_FAIL_3P(
        NvStorMgrFileOpen(FileName,
            NVOS_OPEN_READ,
            &hFile),
        Nv3pStatus_MassStorageFailure);

    while (BytesRemaining)
    {
        NvU32 BytesToRead;
        NvU32 BytesRead = 0;

        // Use the staging buffer to verify partition data--this is questionable
        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToRead = NV3P_STAGING_SIZE;
        else
            BytesToRead = (NvU32)BytesRemaining;

        NV_CHECK_ERROR_FAIL_3P(
            NvStorMgrFileRead(
                hFile,
                s_pState->pStaging,
                BytesToRead,
                &BytesRead),
            Nv3pStatus_MassStorageFailure);

        // Keep computing hash
        EndMsg = (BytesRemaining == BytesRead);

        NV_CHECK_ERROR_FAIL_3P(
            VerifySignature(
                s_pState->pStaging,
                BytesRead,
                StartMsg,
                EndMsg,
                pPartitionToVerify->PartitionHash),
            Nv3pStatus_CryptoFailure);

        StartMsg = NV_FALSE;
        BytesRemaining -= BytesRead;
    }

fail:
    if (e)
        NvOsDebugPrintf(
            "\nReadVerifyData failed. NvError %u NvStatus %u\n", e, s);
    *status = s;
    // dealloc any resources
    if (hFile)
    {
        e = NvStorMgrFileClose(hFile);
    }
    if (s != Nv3pStatus_Ok)
    {
        e = NvError_Nv3pBadReturnData;
        NvOsDebugPrintf(
            "\nReadVerifyData failed. NvError %u NvStatus %u\n", e, s);
    }
    return e;
}

NvError
UpdateBlInfo(Nv3pCmdDownloadPartition *a,
        NvU8 *hash,
        NvU32 padded_length, Nv3pStatus *status, NvBool IsMicroboot)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvU32 instance = 0;
    NvU32 size = 0;
    NvU32 sectorsPerBlock;
    NvU32 bytesPerSector;
    NvU32 physicalSector;
    NvU32 logicalSector;
    NvU32 bootromBlockSizeLog2;
    NvU32 bootromPageSizeLog2;
    NvU32 loadAddress;
    NvU32 entryPoint;
    NvU32 version;
    NvU32 startBlock;
    NvU32 startPage;
    NvPartInfo PartInfo;
    NvDdkFuseOperatingMode OpMode;
    char Message[NV3P_STRING_MAX] = {'\0'};

    NV_CHECK_ERROR_FAIL_3P(
            NvPartMgrGetPartInfo(a->Id, &PartInfo),
                              Nv3pStatus_InvalidPartition);
    e = NvSysUtilImageInfo((NvU32)a->Id,
                        &loadAddress,
                        &entryPoint,
                        &version, (NvU32)a->Length);

    // If we can't obtain the bootloader entry point
    // info, its probably an AOS/Quickboot image. We hardcode
    // this info for AOS.
    if (e != NvSuccess)
    {
        if (!IsMicroboot)
        {
            loadAddress = NV_AOS_ENTRY_POINT;
            entryPoint = NV_AOS_ENTRY_POINT;
            version = 1;
        }
        else
        {
            // Suppress DRAM initialization.
            NvU32 size = sizeof(NvU32);
            NvU32 instance = 0;
            NvU32 zero = 0;
            NV_CHECK_ERROR_FAIL_3P(
                NvBctSetData(s_pState->BctHandle,
                             NvBctDataType_NumValidSdramConfigs,
                             &size, &instance, &zero),
                Nv3pStatus_InvalidBCT);
            loadAddress = NV_MICROBOOT_ENTRY_POINT;
            entryPoint = NV_MICROBOOT_ENTRY_POINT;
            version = 1;
        }
    }

    // make hash null to avoid updating hash,
    // load and entry address of bl in secure mode
    size =  sizeof(NvU32);
    NV_CHECK_ERROR_FAIL_3P(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&OpMode, (NvU32 *)&size), Nv3pStatus_InvalidPartition);

    if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
        hash = NULL;

    logicalSector = (NvU32)PartInfo.StartLogicalSectorAddress;

    //Get various device parameters, as part-id=0 as we call logical2physical
    if (NvSysUtilGetDeviceInfo(a->Id, logicalSector, &sectorsPerBlock,
        &bytesPerSector, &physicalSector) == NV_FALSE)
    {
        NV_CHECK_ERROR_FAIL_3P(NvError_BadValue,
                                  Nv3pStatus_InvalidPartitionTable);
    }

    //Get the blockSize/PageSize from the BCT

    size = sizeof(NvU32);

    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(s_pState->BctHandle, NvBctDataType_BootDeviceBlockSizeLog2,
                     &size, &instance, &bootromBlockSizeLog2),
        Nv3pStatus_InvalidBCT
        );

    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(s_pState->BctHandle, NvBctDataType_BootDevicePageSizeLog2,
                     &size, &instance, &bootromPageSizeLog2),
        Nv3pStatus_InvalidBCT
        );

    // Convert the logicalSectors into a startBlock and StartPage in terms
    // of the page/block size that the bootrom uses (ie, in terms of the log2
    // values stored in the bct)
    startBlock = (physicalSector * bytesPerSector) /
        (1 << bootromBlockSizeLog2);
    startPage = (physicalSector * bytesPerSector) %
        (1 << bootromBlockSizeLog2);
    startPage /= (1 << bootromPageSizeLog2);

    //Set the bootloader in the BCT
    NV_CHECK_ERROR_FAIL_3P(
        NvBuUpdateBlInfo(s_pState->BctHandle,
                          a->Id,
                          version,
                          startBlock,
                          startPage,
                          padded_length,
                          loadAddress,
                          entryPoint,
                          NVCRYPTO_CIPHER_AES_BLOCK_BYTES,
                          hash),
        Nv3pStatus_InvalidBCT);

    // Make bootloader bootable
    NV_CHECK_ERROR_FAIL_3P(
        NvBuSetBlBootable(s_pState->BctHandle,
                          s_pState->BitHandle,
                          a->Id),
        Nv3pStatus_InvalidBCT);

fail:
    *status = s;
    if (e)
        NvOsDebugPrintf(
            "\nUpdateBlInfo failed. NvError %u NvStatus %u\n", e, s);
    return e;
}

NvError
LocatePartitionToVerify(NvU32 PartitionId, PartitionToVerify **pPartition)
{
    NvError e = NvError_BadParameter;
    PartitionToVerify *pLookupPart;

    *pPartition = NULL;
    pLookupPart = partInfo.LstPartitions;

    while (pLookupPart)
    {
        if (PartitionId == pLookupPart->PartitionId)
        {
            *pPartition = pLookupPart;
            e = NvSuccess;
            break;
        }
        pLookupPart = (PartitionToVerify *)pLookupPart->pNext;
    }

    if (e)
        NvOsDebugPrintf(
            "\nLocatePartitionToVerify failed. NvError %u NvStatus %u\n", e, 0);
    return e;
}

void
InitLstPartitionsToVerify(void)
{
    partInfo.NumPartitions = 0;
    partInfo.LstPartitions = NULL;
}

void
DeInitLstPartitionsToVerify(void)
{
    PartitionToVerify *nextElem = NULL;

    if (partInfo.LstPartitions)
    {
        nextElem = (PartitionToVerify *)partInfo.LstPartitions->pNext;
        while(nextElem)
        {
            nextElem = (PartitionToVerify *)partInfo.LstPartitions->pNext;
            NvOsFree(partInfo.LstPartitions);
            partInfo.LstPartitions = nextElem;
        }
        partInfo.LstPartitions = NULL;
    }
    partInfo.NumPartitions = 0;
}

NvError
SetPartitionToVerify(NvU32 PartitionId, PartitionToVerify **pNode)
{
    NvError e = NvError_Success;
    NvU32 i;
    PartitionToVerify *pListElem;

    pListElem = (PartitionToVerify *)partInfo.LstPartitions;
    if(partInfo.NumPartitions)
    {
        for (i = 0;i< (partInfo.NumPartitions-1); i++)
        {
            pListElem = (PartitionToVerify *)pListElem->pNext;
        }
        pListElem->pNext = NvOsAlloc(sizeof(PartitionToVerify));
        /// If there's no more memory for storing the partition Ids
        /// return an error.
        if (!pListElem->pNext)
        {
                e = NvError_InsufficientMemory;
                goto fail;
        }
        ((PartitionToVerify *)(pListElem->pNext))->PartitionId = PartitionId;
        ((PartitionToVerify *)(pListElem->pNext))->PartitionDataSize = 0;
        ((PartitionToVerify *)(pListElem->pNext))->pNext = NULL;
        *pNode = (PartitionToVerify *)pListElem->pNext;
    }
    else
    {
        partInfo.LstPartitions = NvOsAlloc(sizeof(PartitionToVerify));
        /// If there's no more memory for storing the partition Ids
        /// return an error.
        if (!partInfo.LstPartitions)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        partInfo.LstPartitions->PartitionId = PartitionId;
        partInfo.LstPartitions->PartitionDataSize = 0;
        partInfo.LstPartitions->pNext = NULL;
        *pNode = (PartitionToVerify *)partInfo.LstPartitions;
    }

    partInfo.NumPartitions++;

fail:
    if (e)
        NvOsDebugPrintf(
            "\nSetPartitionToVerify failed. NvError %u NvStatus %u\n", e, 0);
    return e;
}

COMMAND_HANDLER(DownloadNvPrivData)
{
    NvError e = NvSuccess;
    NvU32 BytesReceived;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};

    Nv3pCmdNvPrivData *a = (Nv3pCmdNvPrivData *)arg;
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

    NvPrivDataBufferSize = a->Length;
    NvPrivDataBuffer = NvOsAlloc(a->Length);
    if (NvPrivDataBuffer == NULL)
        NV_CHECK_ERROR_CLEAN_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);
    NvOsMemset(NvPrivDataBuffer, 0, NvPrivDataBufferSize);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pDataReceive(
            hSock,
            NvPrivDataBuffer,
            NvPrivDataBufferSize,
            &BytesReceived,
            0),
        Nv3pStatus_DataTransferFailure);

clean:
    if (e)
        NvOsDebugPrintf(
            "\nDownloadNvPrivData failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}

#if NVODM_BOARD_IS_SIMULATION == 0
static NvError
SkuFuseBypass(Nv3pSocketHandle hSock, NvU8 *buff)
{
    NvError e      = NvSuccess;
    Nv3pStatus s   = Nv3pStatus_Ok;
    NvU32 fuseVal  = 0;
    NvU32 i        = 0;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvFuseBypassInfo *fusebypass_info = NULL;

    fusebypass_info = (NvFuseBypassInfo *) buff;

    e = NvDdkFuseBypassSku(fusebypass_info);

    /* If chip does not meet the SPEEDO/IDDQ limits and fuse bypassing
     * is not forced then exit, else if bypassing is forced then
     * continue. Also return the status to host.
     */
    if(e == NvError_InvalidConfigVar)
    {
        s = Nv3pStatus_FuseBypassSpeedoIddqCheckFailure;
        e = (fusebypass_info->force_bypass == NV_FALSE ? e : NvSuccess);
    }

    if (e != NvSuccess && fusebypass_info->force_download == NV_FALSE)
    {
        s = Nv3pStatus_FuseBypassFailed;
    }

    ReportStatus(hSock, Message, s, 0);

    /* If fuse bypass information is forcefully downloaded then
     * do not error out.
     */
    if (fusebypass_info->force_download && e != NvSuccess)
    {
        NvOsDebugPrintf("Warning: Failed to bypass fuses\n");
        e = NvSuccess;
        goto fail;
    }

    if(e != NvSuccess)
        goto fail;

    NvOsDebugPrintf("Fuses Bypassed: \n");
    for (i = 0; i < fusebypass_info->num_fuses; i++)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseBypassGet(fusebypass_info->fuses[i].offset, &fuseVal)
        );
        NvOsDebugPrintf("Offset: 0x%x Value: 0x%x\n",
                         fusebypass_info->fuses[i].offset, fuseVal);
        if(fuseVal != fusebypass_info->fuses[i].fuse_value[0])
            break;
    }
    if(fusebypass_info->num_fuses != i)
    {
        NvOsDebugPrintf("Warning: Value of fuse 0x%x is 0x%x, which"
                        " is different than the passed value 0x%x\n",
                        fusebypass_info->fuses[i].offset, fuseVal,
                        fusebypass_info->fuses[i].fuse_value[0]);
    }

fail:
    return e;
}
#endif

#if ENABLE_THREADED_DOWNLOAD
void
ThreadWriteBufferToFile(void *arg)
{
    NvU32 BytesWritten = 0;
    NvError e = NvSuccess;

    NvOsDebugPrintf("Parallel write enabled\n");
    while (1)
    {
        NvOsSemaphoreWait(s_WaitingForFullBuffer);

        if (s_CircularBuffer[s_WriteThreadNextBuffer].hFile == NULL)
            goto fail;

        if (s_CircularBuffer[s_WriteThreadNextBuffer].IsSparseImage)
        {
            e = NvSysUtilUnSparse(
                    s_CircularBuffer[s_WriteThreadNextBuffer].hFile,
                    s_CircularBuffer[s_WriteThreadNextBuffer].Buff,
                    s_CircularBuffer[s_WriteThreadNextBuffer].BytesToWrite,
                    s_CircularBuffer[s_WriteThreadNextBuffer].IsSparseStart,
                    s_CircularBuffer[s_WriteThreadNextBuffer].IsLastBuffer,
                    s_CircularBuffer[s_WriteThreadNextBuffer].SignRequired,
                    NV_FALSE,
                    s_CircularBuffer[s_WriteThreadNextBuffer].HashPtr,
                    s_CircularBuffer[s_WriteThreadNextBuffer].pOpMode
                );
        }
        else
        {
            e = NvStorMgrFileWrite(
                    s_CircularBuffer[s_WriteThreadNextBuffer].hFile,
                    s_CircularBuffer[s_WriteThreadNextBuffer].Buff,
                    s_CircularBuffer[s_WriteThreadNextBuffer].BytesToWrite,
                    &BytesWritten
                );
        }

        s_CircularBuffer[s_WriteThreadNextBuffer].hFile = NULL;

        if (e != NvSuccess)
            goto fail;

        NvOsSemaphoreSignal(s_WaitingForEmptyBuffer);

        s_WriteThreadNextBuffer++;
        s_WriteThreadNextBuffer %= NUM_BUFFERS;
    }

fail:
    s_CircularBuffer[s_WriteThreadNextBuffer].e = e;
    NvOsSemaphoreSignal(s_WaitingForEmptyBuffer);
    NvOsDebugPrintf("WriterThread: Exiting\n");
}

NvError
InitParallelWrite(void)
{
    NvError e = NvSuccess;
    NvU32 EmptyBuffers;

    if (s_hWriteToFileThread == NULL)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvOsSemaphoreCreate(&s_WaitingForFullBuffer, 0)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvOsSemaphoreCreate(&s_WaitingForEmptyBuffer, 0)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvOsThreadCreate(ThreadWriteBufferToFile, NULL,
                             &s_hWriteToFileThread)
        );

        if (s_pState->pStaging == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
    }

    /* Increment the semaphore count to NUM_BUFFERS, as initially
     * all buffers are empty.
     */
    for(EmptyBuffers = 0; EmptyBuffers < NUM_BUFFERS; EmptyBuffers++)
    {
        NvOsSemaphoreSignal(s_WaitingForEmptyBuffer);
    }

fail:
    return e;
}

NvError
WaitForAllBufferEmpty(void)
{
    NvU32 NumBufChecked;
    NvU32 CurBuffer;
    NvError e = NvSuccess;

    CurBuffer = s_WriteThreadNextBuffer;
    for(NumBufChecked = 0; NumBufChecked < NUM_BUFFERS; NumBufChecked++)
    {
        e = s_CircularBuffer[CurBuffer].e;

        if(e != NvSuccess)
            break;

        NvOsSemaphoreWait(s_WaitingForEmptyBuffer);
        CurBuffer = (CurBuffer + 1) % NUM_BUFFERS;
    }

    e = s_CircularBuffer[CurBuffer].e;

    return e;
}

void
StopWriterThread(void)
{
    if (s_hWriteToFileThread != NULL)
    {
        if(s_CircularBuffer[s_WriteThreadNextBuffer].e == NvSuccess)
        {
            s_CircularBuffer[s_MainThreadNexBuffer].hFile = NULL;
            NvOsSemaphoreSignal(s_WaitingForFullBuffer);
            NvOsSemaphoreWait(s_WaitingForEmptyBuffer);
        }
    }

    if (s_WaitingForFullBuffer != NULL)
    {
        NvOsSemaphoreDestroy(s_WaitingForEmptyBuffer);
        s_WaitingForFullBuffer = NULL;
    }

    if (s_WaitingForEmptyBuffer != NULL)
    {
        NvOsSemaphoreDestroy(s_WaitingForEmptyBuffer);
        s_WaitingForEmptyBuffer = NULL;
    }
}
#endif

NvError
Nv3pServerWriteToFile(
        NvStorMgrFileHandle hFile,
        NvU8 *Buff,
        NvU32 BytesToWrite,
        NvU8 *HashBuf,
        NvBool IsSparseImage,
        NvBool IsSparseStart,
        NvBool IsLastBuffer,
        NvBool SignRequired,
        NvDdkFuseOperatingMode *pOpMode,
        Nv3pStatus *Status)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;

#if ENABLE_THREADED_DOWNLOAD
    /* Check if the writer thread has failed to write any previous
     * buffers.
     */
    if(s_CircularBuffer[s_WriteThreadNextBuffer].e != NvSuccess)
    {
        e = s_CircularBuffer[s_WriteThreadNextBuffer].e;
        s = IsSparseImage ? Nv3pStatus_UnsparseFailure :
                            Nv3pStatus_MassStorageFailure;
        goto clean;
    }

    s_CircularBuffer[s_MainThreadNexBuffer].e     = NvSuccess;
    s_CircularBuffer[s_MainThreadNexBuffer].hFile = hFile;
    s_CircularBuffer[s_MainThreadNexBuffer].BytesToWrite  = BytesToWrite;
    s_CircularBuffer[s_MainThreadNexBuffer].IsSparseImage = IsSparseImage;
    s_CircularBuffer[s_MainThreadNexBuffer].Buff = Buff;

    if(IsSparseImage)
    {
        s_CircularBuffer[s_MainThreadNexBuffer].IsSparseStart = IsSparseStart;
        s_CircularBuffer[s_MainThreadNexBuffer].IsLastBuffer  = IsLastBuffer;
        s_CircularBuffer[s_MainThreadNexBuffer].SignRequired  = SignRequired;
        s_CircularBuffer[s_MainThreadNexBuffer].HashPtr = HashBuf;
        s_CircularBuffer[s_MainThreadNexBuffer].pOpMode = pOpMode;
    }

    s_MainThreadNexBuffer = (s_MainThreadNexBuffer + 1) % NUM_BUFFERS;
    NvOsSemaphoreSignal(s_WaitingForFullBuffer);

    /* Give more priority to writer thread. */
    NvOsThreadYield();

#else
    char Message[NV3P_STRING_MAX] = {'\0'};

    if(!IsSparseImage)
    {
        while (BytesToWrite)
        {
            NvU32 BytesWritten = 0;

            NV_CHECK_ERROR_CLEAN_3P(
                NvStorMgrFileWrite(hFile, Buff, BytesToWrite, &BytesWritten),
                Nv3pStatus_MassStorageFailure
            );

            BytesToWrite -= BytesWritten;
        }
    }
    else
    {
        NV_CHECK_ERROR_CLEAN_3P(
            NvSysUtilUnSparse(hFile, Buff, BytesToWrite, IsSparseStart,
                        IsLastBuffer, SignRequired, NV_FALSE, HashBuf, pOpMode),
            Nv3pStatus_UnsparseFailure
        );
    }
#endif

clean:
    *Status = s;
    return e;
}

COMMAND_HANDLER(DownloadPartition)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvBool IsGoodHeader = NV_FALSE;
    NvBool IsTransfer = NV_FALSE;
    NvBool IsLastBuffer = NV_FALSE;
    NvBool LookForSparseHeader = NV_TRUE;
    NvU8 *pCustomerFuseData = NULL;

    Nv3pCmdDownloadPartition *a = (Nv3pCmdDownloadPartition *)arg;
    NvPartInfo PartInfo;
    NvPartitionStat PartitionStat;
    NvU64 BytesRemaining;
    NvU32 PaddedLength = 0;
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvError partitionLocated = NvError_NotInitialized;
    NvStorMgrFileHandle hFile = NULL;
    PartitionToVerify *pPartitionToLocate = NULL;
    PartitionToVerify BlPartitionInfo;
    NvBool SparseImage = NV_FALSE;
    NvBool IsSparseStart = NV_FALSE;
    NvU8 *pMbInfo = NULL;
    NvBool  IsMicroboot = NV_FALSE;
    NvU8 *pFuseBypassBuf = NULL;
    NvU32 StartTime = 0;
    NvU32 EndTime   = 0;

    StartTime = NvOsGetTimeMS();

    // verify PT is available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);

        s_pState->IsValidPt = NV_TRUE;
    }

    // verify requested partition exists
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetPartInfo(a->Id, &PartInfo),
        Nv3pStatus_InvalidPartition);

    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetNameById(a->Id, FileName),
        Nv3pStatus_InvalidPartition);

    if ((s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC
         || s_OpMode == NvDdkFuseOperatingMode_OdmProductionSecureSBK )&&
            PartInfo.PartitionType == NvPartMgrPartitionType_Bootloader)
        NV_CHECK_ERROR_CLEANUP(
            Nv3pVerifyBlockSize(a->Id, &s, Message));

#if NVODM_BOARD_IS_SIMULATION == 0
    // verify partition is large enough to hold data
    NV_CHECK_ERROR_FAIL_3P(
        NvStorMgrPartitionQueryStat(
            FileName,
            &PartitionStat),
        Nv3pStatus_InvalidPartition);

    if (a->Length > PartitionStat.PartitionSize)
        NV_CHECK_ERROR_FAIL_3P(NvError_BadParameter, Nv3pStatus_BadParameter);
#endif

    if (PartInfo.PartitionType == NvPartMgrPartitionType_Bct ||
        PartInfo.PartitionType == NvPartMgrPartitionType_PartitionTable)
    {
        //Downloading a BCT to a partition makes no sense.
        //This functionality is covered by the DownloadBct
        //commmand already.
        Nv3pNack(hSock, Nv3pNackCode_BadCommand);
    }
    else
    {
        NvBool StartMsg = NV_TRUE;
        NvBool EndMsg = NV_FALSE;
        NvBool SignRequired = NV_FALSE;
        NvDdkFuseOperatingMode OpMode;
        NvDdkFuseOperatingMode* pOpMode = NULL;
        NvBool DownloadingBl = NV_FALSE;
        NvU8 *HashPtr = NULL;
        NvU32 Size;
        NvU32 Offset = 0;

        // Get current operating mode
        Size =  sizeof(NvU32);
        NV_CHECK_ERROR_FAIL_3P(
            NvDdkFuseGet(
                NvDdkFuseDataType_OpMode,
                (void *)&OpMode,
                (NvU32 *)&Size),
            Nv3pStatus_InvalidPartition);

        if (OpMode == NvDdkFuseOperatingMode_Undefined)
            NV_CHECK_ERROR_FAIL_3P(
                NvError_InvalidState, Nv3pStatus_InvalidState);

        if(PartInfo.PartitionType == NvPartMgrPartitionType_Bootloader ||
            PartInfo.PartitionType == NvPartMgrPartitionType_BootloaderStage2)
        {
            DownloadingBl = NV_TRUE;
            pOpMode = &OpMode;
        }


        NV_CHECK_ERROR_FAIL_3P(
            NvStorMgrFileOpen(
                FileName,
                NVOS_OPEN_WRITE,
                &hFile),
            Nv3pStatus_MassStorageFailure);

        // ack the command now that we know we can complete the requested
        // operation, barring a mass storage error

        // NOTE! Nv3p protocol requires that after this command is ack'ed,
        // at least one call to Nv3pDataReceive() must be attempted.  Only after
        // this requirement has been met can Nv3pTransferFail() be called to
        //terminate the transfer in case of an error.

        IsGoodHeader = NV_TRUE;
        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);

        BytesRemaining = a->Length;

        // If verification is required, store hash information for this partition
        if(DownloadingBl)
        {
            // If BootLoader is to be verified, set its partition for
            // verification as for other partitions.
            if (gs_VerifyPartition)
            {
                partitionLocated =
                    SetPartitionToVerify(a->Id, &pPartitionToLocate);
                gs_VerifyPartition = NV_FALSE;
            }
            // If BootLoader is not set for verification
            else
                pPartitionToLocate = &BlPartitionInfo;
        }
        else if (gs_VerifyPartition)
        {
            partitionLocated = SetPartitionToVerify(a->Id, &pPartitionToLocate);
            gs_VerifyPartition = NV_FALSE;
        }
        if ((partitionLocated == NvSuccess) || DownloadingBl)
        {
            NvOsMemset(
                &(pPartitionToLocate->PartitionHash[0]),
                0,
                NV3P_AES_HASH_BLOCK_LEN);
        }
        NvOsDebugPrintf("\nStart Downloading %s\n", FileName);

#if ENABLE_THREADED_DOWNLOAD
        /* Initialize threads and semaphores. For each download partition
         * call initially all buffers are empty.
         */
        NV_CHECK_ERROR_CLEANUP(InitParallelWrite());
#endif
        while (BytesRemaining)
        {
            NvU8 *pStaging;
            NvU32 BytesToWrite, TmpBytesToWrite;
            NvU32 BytesToReceive = 0;

            if (BytesRemaining > NV3P_STAGING_SIZE)
                BytesToReceive = NV3P_STAGING_SIZE;
            else
                BytesToReceive = (NvU32)BytesRemaining;

#if ENABLE_THREADED_DOWNLOAD
            /* Wait for empty buffer */
            NvOsSemaphoreWait(s_WaitingForEmptyBuffer);
            pStaging = s_pState->pStaging +
                       s_MainThreadNexBuffer * NV3P_STAGING_SIZE;
#else
            pStaging = s_pState->pStaging;
#endif
            ALIGN_MEMORY(pStaging, USB_ALGINMENT);
            BytesToWrite = 0;
            while(1)
            {
                e = Nv3pDataReceive(
                        hSock,
                        pStaging+BytesToWrite,
                        BytesToReceive-BytesToWrite,
                        &TmpBytesToWrite,
                        0);
                if (e)
                    goto fail;
                BytesToWrite += TmpBytesToWrite;
                if (!DownloadingBl || !(BytesToWrite % NV3P_AES_HASH_BLOCK_LEN))
                    break;
            }

            IsTransfer = NV_TRUE;
            // If verification is required, check if it is sparsed image or not
            if (LookForSparseHeader)
            {
                if(NvSysUtilCheckSparseImage(pStaging, BytesToWrite))
                {
                    SparseImage = NV_TRUE;
                    IsSparseStart = NV_TRUE;
                }
                LookForSparseHeader = NV_FALSE;
            }

            //Compute hash over the data here on the staging buffer.
            // End of data received when BytesRemaining = BytesToWrite
            if ((partitionLocated == NvSuccess) || DownloadingBl)
            {
                EndMsg = (BytesRemaining == BytesToWrite);
                if(DownloadingBl && EndMsg)
                {
                    // We just got last buffer, see if the buffer is alligned to
                    // AES_BLOCK length
                    PaddedLength = (NvU32)a->Length;
                    if ((PaddedLength % NV3P_AES_HASH_BLOCK_LEN) != 0)
                    {
                        NvOsDebugPrintf(
                            "size not aligned to AES block length\n");
                        e = NvError_InvalidSize;
                        s = Nv3pStatus_InvalidBCTSize;
                        NvOsSnprintf(
                            Message,
                            NV3P_STRING_MAX,
                            " %s %d",
                            __FUNCTION__,
                            __LINE__);

                        goto clean;
                    }
                    BytesRemaining = BytesToWrite;
                }
                // If the Image is not sparsed image, compute the hash on the
                // input buffer, else do it in NvSysUtilUnSparse
                if (!SparseImage)
                {

                    if (s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC ||
                        (partitionLocated == NvSuccess))
                    {
                       NV_CHECK_ERROR_CLEAN_3P(
                            NvSysUtilSignData(
                                pStaging,
                                BytesToWrite,
                                StartMsg,
                                EndMsg,
                                &(pPartitionToLocate->PartitionHash[0]),
                                pOpMode),
                                Nv3pStatus_InvalidBCT
                        );
                     }
                     if (DownloadingBl && StartMsg)
                     {
                        pMbInfo = NvOsAlloc(NV3P_AES_HASH_BLOCK_LEN);
                        if (!pMbInfo)
                            NV_CHECK_ERROR_CLEAN_3P(
                                NvError_InsufficientMemory,
                                Nv3pStatus_InvalidState);

                        NvOsMemset(pMbInfo, 0x0, NV3P_AES_HASH_BLOCK_LEN);
                        NvOsMemcpy(
                            pMbInfo,
                            pStaging,
                            NV3P_AES_HASH_BLOCK_LEN);
                        //we dont have to decrypt data if it is PKC or
                        //nvproduction operating mode
                        if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
                            NV_CHECK_ERROR_CLEAN_3P(
                                NvSysUtilEncryptData(
                                    pMbInfo,
                                    NV3P_AES_HASH_BLOCK_LEN,
                                    StartMsg,
                                    EndMsg,
                                    NV_TRUE,
                                    *pOpMode),
                                Nv3pStatus_BadParameter);
                        // Adding 1 to bypass the branch instrunction of 4 bytes
                        // present at the starting of microboot.
                        // Verify if the signature is present to distinguish
                        // between bootloader & microboot.
                        if (*((NvU32 *)pMbInfo + 1) == MICROBOOT_HEADER_SIGN)
                             IsMicroboot = NV_TRUE;
                     }
                    StartMsg = NV_FALSE;
                }
                else
                    SignRequired = NV_TRUE;
            }
            if (PartInfo.PartitionType == NvPartMgrPartitionType_FuseBypass)
            {
#if NVODM_BOARD_IS_SIMULATION == 0
                if(sizeof(NvFuseBypassInfo) > NV3P_AES_HASH_BLOCK_LEN)
                {
                    if(pFuseBypassBuf == NULL)
                        pFuseBypassBuf = NvOsAlloc(sizeof(NvFuseBypassInfo));

                    if (!pFuseBypassBuf)
                         NV_CHECK_ERROR_CLEAN_3P(
                             NvError_InsufficientMemory,
                             Nv3pStatus_InvalidState);

                    NvOsMemcpy(pFuseBypassBuf + Offset, pStaging, BytesToWrite);
                    Offset += BytesToWrite;
                }
                else
                {
                    pFuseBypassBuf = pStaging;
                }
#endif
            }

            if (!(BytesRemaining - BytesToWrite))
                IsLastBuffer = NV_TRUE;

            if (SparseImage && SignRequired)
                HashPtr = &(pPartitionToLocate->PartitionHash[0]);

            NV_CHECK_ERROR_CLEANUP(
                Nv3pServerWriteToFile(
                    hFile,
                    pStaging,
                    BytesToWrite,
                    HashPtr,
                    SparseImage,
                    IsSparseStart,
                    IsLastBuffer,
                    SignRequired,
                    pOpMode,
                    &s));

            IsSparseStart = NV_FALSE;
            BytesRemaining -= BytesToWrite;
        }

#if ENABLE_THREADED_DOWNLOAD
        /* Wait for all buffers to empty before doing any further operations. */
        NV_CHECK_ERROR_CLEANUP(WaitForAllBufferEmpty());
#endif

#if NVODM_BOARD_IS_SIMULATION == 0
        if (PartInfo.PartitionType == NvPartMgrPartitionType_FuseBypass)
        {
            NvBctAuxInfo *aux1Info;
            NvU32 size = 0;
            NvU32 instance = 0;

            /* FIXME:If size of bypass info structure is more than 4Kb then
             * error out as it will lead hang in bootloader.
             */
            if(sizeof(NvFuseBypassInfo) > 4096)
            {
                e = NvError_InvalidSize;
                goto fail;
            }

            /* First write fuses and then edit the customer data. */
            NV_CHECK_ERROR_CLEANUP(SkuFuseBypass(hSock, pFuseBypassBuf));

            NV_CHECK_ERROR_CLEAN_3P(
                NvBctGetData(
                    s_pState->BctHandle,
                    NvBctDataType_AuxDataAligned,
                    &size,
                    &instance,
                    NULL),
                Nv3pStatus_InvalidBCT
            );

            pCustomerFuseData = NvOsAlloc(size);
            if (!pCustomerFuseData)
                NV_CHECK_ERROR_CLEAN_3P(
                    NvError_InsufficientMemory,
                    Nv3pStatus_InvalidState);

            NvOsMemset(pCustomerFuseData, 0, size);
            NV_CHECK_ERROR_CLEAN_3P(
                NvBctGetData(s_pState->BctHandle, NvBctDataType_AuxDataAligned,
                             &size, &instance, pCustomerFuseData),
                Nv3pStatus_InvalidBCT
            );
            aux1Info = (NvBctAuxInfo*)pCustomerFuseData;

            aux1Info->StartFuseSector =
                                     (NvU32) PartInfo.StartLogicalSectorAddress;

            aux1Info->NumFuseSector = (NvU16)PartInfo.NumLogicalSectors;

            NV_CHECK_ERROR_CLEAN_3P(
                NvBctSetData(s_pState->BctHandle, NvBctDataType_AuxDataAligned,
                             &size, &instance, pCustomerFuseData),
                Nv3pStatus_InvalidBCT);
        }
#endif
        if ((partitionLocated == NvSuccess) || DownloadingBl)
        {
            pPartitionToLocate->PartitionDataSize = a->Length;
        }
        if (DownloadingBl)
        {
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
            /// For T114 onward, we will append hash to QB2 image
            if(hFile && PartInfo.PartitionType ==
                NvPartMgrPartitionType_BootloaderStage2) // Stage2 bootloader
            {
                NvU32 BytesWritten;
                NV_CHECK_ERROR_CLEAN_3P(
                    NvStorMgrFileWrite(
                        hFile,
                        &BlPartitionInfo.PartitionHash[0],
                        sizeof(BlPartitionInfo.PartitionHash),
                        &BytesWritten),
                    Nv3pStatus_MassStorageFailure);
            }
#endif
            if (hFile)
            {
                NV_CHECK_ERROR_CLEAN_3P(
                    NvStorMgrFileClose(hFile),
                        Nv3pStatus_MassStorageFailure);
            }
            hFile = 0;
            // Stage1 bootloader
            if(PartInfo.PartitionType == NvPartMgrPartitionType_Bootloader)
            {
                if (s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
                    UpdateBlInfo(a,
                        &(pPartitionToLocate->PartitionHash[0]),
                        PaddedLength, &s, IsMicroboot);
            }
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
            else if(PartInfo.PartitionType ==
                NvPartMgrPartitionType_BootloaderStage2) // Stage2 bootloader
            {
                NV_CHECK_ERROR_CLEAN_3P(
                    NvBuUpdateHashPartition(s_pState->BctHandle,
                        a->Id,
                        sizeof(BlPartitionInfo.PartitionHash),
                        &BlPartitionInfo.PartitionHash[0]
                        ),
                    Nv3pStatus_InvalidBCT);
            }
#endif
        }
        IsTransfer = NV_FALSE;
        NvOsDebugPrintf("\nEnd Downloading %s\n", FileName);
    }
fail:
    if (e)
       Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nDownloadPartition failed. NvError %u NvStatus %u\n", e, s);
    // dealloc any resources
    if(pCustomerFuseData)
        NvOsFree(pCustomerFuseData);
    if(pFuseBypassBuf && !(sizeof(NvFuseBypassInfo) > NV3P_AES_HASH_BLOCK_LEN))
        NvOsFree(pFuseBypassBuf);
    if(pMbInfo)
        NvOsFree(pMbInfo);

    if(hFile)
    {
        // In case of Sparsed image , where the verification is required, we
        // need to calculate the unsparsed image size here by knowing the
        // current position of handle before close.so that this value can be
        // used in verify data
#if NVODM_BOARD_IS_SIMULATION == 0
        //verifypart not supported in simulation mode.returns error if used.
        if ((partitionLocated == NvSuccess) && SparseImage)
        {
            NvU64 Offset = 0;
            e = NvStorMgrFtell(hFile, &Offset);
            if (e != NvSuccess)
                s = Nv3pStatus_MassStorageFailure;
            pPartitionToLocate->PartitionDataSize = Offset;
        }
#endif
        e = NvStorMgrFileClose(hFile);
        if (e != NvSuccess && s != Nv3pStatus_Ok)
            s = Nv3pStatus_MassStorageFailure;
    }

    // short circuit if a low-level Nv3p protocol error occurred

    if (!IsGoodHeader)
    {
        //Do something interesting here
    }

    if (IsTransfer)
    {
        //Do something interesting here
    }

    EndTime = NvOsGetTimeMS();
    NvOsDebugPrintf("Time taken to download partition: %d ms\n",
                    EndTime - StartTime);

    return ReportStatus(hSock, Message, s, 0);
}


COMMAND_HANDLER(SetBootPartition)
{
    NvError e;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdSetBootPartition *a = (Nv3pCmdSetBootPartition *)arg;
    NvPartInfo PartInfo;

    CHECK_VERIFY_ENABLED();

    // error if PT not available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(
            LoadPartitionTable(NULL),
                Nv3pStatus_PartitionTableRequired);
        s_pState->IsValidPt = NV_TRUE;
    }

    // look up id; return error if not found
    NV_CHECK_ERROR_FAIL_3P(
        NvPartMgrGetPartInfo(a->Id, &PartInfo),
                  Nv3pStatus_InvalidPartition);

    // verify that partition contains a bootloader
    if (PartInfo.PartitionType != NvPartMgrPartitionType_Bootloader)
        NV_CHECK_ERROR_FAIL_3P(NvError_BadParameter,
                                  Nv3pStatus_InvalidPartition);

    // set partition as bootable
    if (s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
        NV_CHECK_ERROR_FAIL_3P(
            NvBuSetBlBootable(s_pState->BctHandle,
                          s_pState->BitHandle,
                          a->Id),
            Nv3pStatus_InvalidBCT);

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)

        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nSetBootPartition failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(OdmOptions)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdOdmOptions *a = (Nv3pCmdOdmOptions *)arg;
    NvU32 size = 0, instance = 0;

    CHECK_VERIFY_ENABLED();

    //Fill in customer opt
    size = sizeof(NvU32);

    NV_CHECK_ERROR_FAIL_3P(
        NvBctSetData(s_pState->BctHandle, NvBctDataType_OdmOption,
                     &size, &instance, &a->Options),
        Nv3pStatus_InvalidBCT);
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nOdmOptions failed. NvError %u NvStatus %u\n", e, s);

    return ReportStatus(hSock, Message, s, 0);
}

#if NVODM_BOARD_IS_SIMULATION==0

static NvError swap(NvU8 *Str, NvU32 One, NvU32 Two)
{
    NvU8 temp;

    if (Str == NULL)
        return NvError_InvalidAddress;
    temp = Str[One];
    Str[One] = Str[Two];
    Str[Two] = temp;
    return NvSuccess;
}

static NvError ChangeEndian(NvU8 *Str, NvU32 Size)
{
    NvU32 i;
    NvError e = NvSuccess;

    if (Str == NULL)
        return NvError_InvalidAddress;

    for (i = 0; i < Size; i += 4)
    {
        if ((e = swap(Str, i, i + 3)) != NvSuccess)
            return e;
        if ((e = swap(Str, i + 1, i + 2)) != NvSuccess)
            return e;
    }
    return NvSuccess;
}

static NvError FuseVerify(NvU8 *Str1, NvU8 *Str2, NvU32 Size)
{
    NvU32 i = 0;
    NvError ret = NvSuccess;
    for (i=0; i < Size; i++)
    {
        if (*Str1++ != *Str2++)
        {
            NvOsDebugPrintf(" Fuse Verify Failed!! \n");
            return NvError_BadValue;
        }
    }
    return ret;
}

NvError
TegraTabFuseKeys(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvU8 *ReceivedBlobBuffer = NULL;
    NvU8 *TempBuffer = NULL;
    NvU8 Sbk[16] = {0};
    NvU8 Dk[4] = {0};
    NvU8 Tid[8] = {0};
    NvU8 PkcHash[32] = {0};
    NvU8 Data;
    NvU8 OdmReservedFuse[32] = {0};
    NvU32 Size = 0;
    NvU32 i;
    Nv3pCmdOdmCommand *b = (Nv3pCmdOdmCommand *)arg;
    Nv3pOdmExtCmdTegraTabFuseKeys *a = (Nv3pOdmExtCmdTegraTabFuseKeys*)
                                                            &b->odmExtCmdParam.tegraTabFuseKeys;

    NvU32 BytesRemaining = a->FileLength;
    TempBuffer = NvOsAlloc(a->FileLength);
    ReceivedBlobBuffer = TempBuffer;

    e = Nv3pCommandComplete(hSock, command, arg, 0);

    if (e)
        goto fail;

    while (BytesRemaining)
    {
        NvU32 BytesToWrite;

        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToWrite = NV3P_STAGING_SIZE;
        else
            BytesToWrite = BytesRemaining;

        e = Nv3pDataReceive(hSock, s_pState->pStaging, BytesToWrite,
                                                &BytesToWrite, 0);
        if (e)
            goto fail;

        BytesRemaining -= BytesToWrite;
        NvOsMemcpy(TempBuffer, s_pState->pStaging, BytesToWrite);
        TempBuffer = TempBuffer + BytesToWrite;
    }

    // Fill the FuseData

    // Fill TT_ID buffer
    for (i = 0; i < 4; i++)
        Tid[i + 4] = ReceivedBlobBuffer[i];

    // Fill SBK key buffer
    for (i = 0; i < 16; i++)
        Sbk[i] = ReceivedBlobBuffer[i + 4];

    // Fill Device Key buffer
    for (i = 0; i < 4; i++)
        Dk[i] = ReceivedBlobBuffer[i + 20];

    // Fill Pkc Hash buffer
    for (i = 0; i < 32; i++)
        PkcHash[i] = ReceivedBlobBuffer[i + 24];

    // Perform ChangeEndian before fusing
    ChangeEndian(Tid, 8);

    // Write the fuses
    if (s_OpMode == NvDdkFuseOperatingMode_NvProduction)
    {
        // Write the Sbk fuse
        Size = 0;
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseGet(NvDdkFuseDataType_SecureBootKey, &Data, &Size)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseSet(NvDdkFuseDataType_SecureBootKey, Sbk, &Size)
        );

        // Write the device key fuse
        Size = 0;
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseGet(NvDdkFuseDataType_DeviceKey, &Data, &Size)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseSet(NvDdkFuseDataType_DeviceKey, Dk, &Size)
        );
    }
    // Write the Pkc Hash fuse
    NvOsDebugPrintf("Write the Pkc Hash fuse \n");
    Size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_PublicKeyHash, &Data, &Size)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseSet(NvDdkFuseDataType_PublicKeyHash, PkcHash, &Size)
    );

    // Write the Reserved Odm fuse with Tid
    NvOsDebugPrintf("Write the Odm Fuse \n");
    Size = 32;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_ReservedOdm, OdmReservedFuse, &Size)
    );

    // Mask Odm_reserve_0 fuse since we will burn only odm_reserve_1 fuse
    for (i = 4; i < 8; i++)
        OdmReservedFuse[i] = Tid[i];

    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseSet(NvDdkFuseDataType_ReservedOdm, OdmReservedFuse, &Size)
    );

    NvOsDebugPrintf("Program SBK, DK, PkcHsh, ReserveOdm fuses\n");
    // Program SBK, DK, PkcHsh, ReserveOdm fuses
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseProgram()
    );

    NvDdkFuseClear();

    goto clean;

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nTegratabFuseKeys failed. NvError %u \n", e);
    }
    return e;

clean:
    e = ReportStatus(hSock, "", s, 0);
    return e;
}

NvError
TegraTabVerifyFuse(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvU8 *ReceivedBlobBuffer = NULL;
    NvU8 *TempBuffer = NULL;
    NvU8 Sbk[16] = {0};
    NvU8 Dk[4] = {0};
    NvU8 Tid[8] = {0};
    NvU8 PkcHash[32] = {0};
    NvU8 Data;
    NvU8 OdmLock[4] = {0};
    NvU8 OdmProduction[4] = {1, 0, 0, 0};
    NvU32 Size = 0;
    NvU32 i;
    Nv3pCmdOdmCommand *b = (Nv3pCmdOdmCommand *)arg;
    Nv3pOdmExtCmdTegraTabFuseKeys *a = (Nv3pOdmExtCmdTegraTabFuseKeys*)
                                                            &b->odmExtCmdParam.tegraTabFuseKeys;

    NvU8 KeyTemp[32] = {0};
    NvU32 BytesRemaining = a->FileLength;
    TempBuffer = NvOsAlloc(a->FileLength);
    ReceivedBlobBuffer = TempBuffer;

    e = Nv3pCommandComplete(hSock, command, arg, 0);

    if (e)
        goto fail;

    while (BytesRemaining)
    {
        NvU32 BytesToWrite;

        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToWrite = NV3P_STAGING_SIZE;
        else
            BytesToWrite = BytesRemaining;

        e = Nv3pDataReceive(hSock, s_pState->pStaging, BytesToWrite,
                                                &BytesToWrite, 0);
        if (e)
            goto fail;

        BytesRemaining -= BytesToWrite;
        NvOsMemcpy(TempBuffer, s_pState->pStaging, BytesToWrite);
        TempBuffer = TempBuffer + BytesToWrite;
    }

    // Fill the FuseData

    // Fill TT_ID buffer
    for (i = 0; i < 4; i++)
        Tid[i + 4] = ReceivedBlobBuffer[i];

    // Fill SBK key buffer
    for (i = 0; i < 16; i++)
        Sbk[i] = ReceivedBlobBuffer[i + 4];

    // Fill Device Key buffer
    for (i = 0; i < 4; i++)
        Dk[i] = ReceivedBlobBuffer[i + 20];

    // Fill Pkc Hash buffer
    for (i = 0; i < 32; i++)
        PkcHash[i] = ReceivedBlobBuffer[i + 24];

    // Perform ChangeEndian before fusing
    ChangeEndian(Tid, 8);
//    ChangeEndian(Sbk, 16);
//    ChangeEndian(Dk, 4);
//    ChangeEndian(PkcHash, 32);

    NvOsMemset(KeyTemp, 0, sizeof(KeyTemp));

    NvOsDebugPrintf("Verify fuses\n");
    if (s_OpMode == NvDdkFuseOperatingMode_NvProduction)
    {
        // Read the Sbk fuse
        Size = 0;
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseGet(NvDdkFuseDataType_SecureBootKey, &Data, &Size)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseGet(NvDdkFuseDataType_SecureBootKey, KeyTemp, &Size)
        );
        e = FuseVerify(Sbk, KeyTemp, Size);
        if (e != NvSuccess)
        {
            NvOsDebugPrintf("\nVerification of Sbk, Sbk fuse failed!\n");
            goto fail;
        }

        // Read the Device key fuse
        NvOsMemset(KeyTemp, 0, sizeof(KeyTemp));
        Size = 0;
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseGet(NvDdkFuseDataType_DeviceKey, &Data, &Size)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvDdkFuseGet(NvDdkFuseDataType_DeviceKey, KeyTemp, &Size)
        );
        ChangeEndian(KeyTemp, 4);
        e = FuseVerify(Dk, KeyTemp, Size);
        if (e != NvSuccess)
        {
            NvOsDebugPrintf("\nVerification of Dk, Dk fuse failed!\n");
            goto fail;
        }
    }

    // Read the Pkc Hash fuse
    NvOsMemset(KeyTemp, 0, sizeof(KeyTemp));
    Size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_PublicKeyHash, &Data, &Size)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_PublicKeyHash, KeyTemp, &Size)
    );
    e = FuseVerify(PkcHash, KeyTemp, Size);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nVerification of PkcHash, PkcHash fuse failed!\n");
        goto fail;
    }

    // Read the Reserved Odm fuse with Tid
    NvOsMemset(KeyTemp, 0, sizeof(KeyTemp));
    Size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_ReservedOdm, &Data, &Size)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_ReservedOdm, KeyTemp, &Size)
    );
    e = FuseVerify(Tid+4, KeyTemp+4, 4);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nVerification of OdmReservedFuse,"
                "OdmReservedFuse fuse failed!\n");
        goto fail;
    }

    // Write OdmLock fuse to write lock reserveOdm_1 fuse
    Size = 4;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_OdmLock, OdmLock, &Size)
    );

    OdmLock[0] |= 0x2;

    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseSet(NvDdkFuseDataType_OdmLock, OdmLock, &Size)
    );

    // Set OdmProduction Fuse
    Size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseGet(NvDdkFuseDataType_OdmProduction, &Data, &Size)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseSet(NvDdkFuseDataType_OdmProduction, OdmProduction, &Size)
    );

    NvOsDebugPrintf("\nProgram OdmLock and OdmProduction fuses\n");

    // Program OdmLock and OdmProduction fuses
    NV_CHECK_ERROR_CLEANUP(
        NvDdkFuseProgram()
    );
    goto clean;

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nTegratabFuseKeys failed. NvError %u \n", e);
    }
    return e;

clean:
    e = ReportStatus(hSock, "", s, 0);
    return e;
}

NvError
UpdateuCFw(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg)
{
    NvBool b;
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdOdmCommand *cmd = (Nv3pCmdOdmCommand *)arg;
    Nv3pOdmExtCmdUpdateuCFw *a = (Nv3pOdmExtCmdUpdateuCFw *)
                                 &cmd->odmExtCmdParam.updateuCFw;
    NvU32 BytesRemaining = 0;
    NvU32 Offset = 0;
    NvU32 BytesWritten;
    NvU8* Buf = NULL;

    e = Nv3pCommandComplete(hSock, command, arg, 0);

    if (e)
        goto fail;

    BytesRemaining = a->FileSize;
    Buf = NvOsAlloc(BytesRemaining);

    if(Buf == NULL)
    {
        e =  NvError_InsufficientMemory;
        goto fail;
    }

    while(BytesRemaining)
    {

        e = Nv3pDataReceive(hSock, Buf + Offset,
                            BytesRemaining, &BytesWritten, 0);
        if(e)
            goto fail;

        Offset += BytesWritten;
        BytesRemaining -= BytesWritten;
    }

    b = IsspParseHexFile(Buf, a->FileSize);
    if(b == NV_FALSE)
    {
        e = NvError_ParserFailure;
        goto fail;
    }

    b = IsspProgramFirmware();
    if(b == NV_FALSE)
    {
        NvOsDebugPrintf("Programming uC failed\n");
        goto fail;
    }

    goto clean;

fail:
    s = Nv3pStatus_UpdateuCFwFailure;
    if (e)
        NvOsDebugPrintf("\nUpdate uC Fw failed. NvError %u NvStatus %u\n",
                                                                        e, s);
clean:
    e = ReportStatus(hSock, "", s, 0);
    return e;
}

NvError
FuelGaugeFwUpgrade(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    void *arg)
{
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdOdmCommand *b = (Nv3pCmdOdmCommand *)arg;
    Nv3pOdmExtCmdFuelGaugeFwUpgrade *a = (Nv3pOdmExtCmdFuelGaugeFwUpgrade *)
                                   &b->odmExtCmdParam.fuelGaugeFwUpgrade;
    struct NvOdmFFUBuff *pHeadBuff1 = NULL;
    struct NvOdmFFUBuff *pHeadBuff2 = NULL;
    struct NvOdmFFUBuff *pTempBuff = NULL;
    NvU64 BytesRemaining = 0;
    NvU32 BytesWritten = 0;
    NvU32 count = 0;
    NvU32 BytesToReceive;
    NvU32 NoOfFiles = 1;
    NvU64 max_filelen2 = 0;

    NvOsSnprintf(
        Message, NV3P_STRING_MAX, " %s %d", __FUNCTION__, __LINE__);
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);

    max_filelen2 = a->FileLength2;
    BytesRemaining = a->FileLength1;

    if (max_filelen2)
        NoOfFiles = 2;
    // Creating the First Buffer node
    pHeadBuff1 = (struct NvOdmFFUBuff *)NvOsAlloc(sizeof(struct NvOdmFFUBuff));
    if (pHeadBuff1 == NULL)
    {
        NvOsDebugPrintf("\nError in memory allocation\n");
        e =  NvError_InsufficientMemory;
        goto clean;
    }
    pHeadBuff1->pNext = NULL;

    if (max_filelen2)
    {
        // Creating the First Buffer node for Buffer2
        pHeadBuff2 =
            (struct NvOdmFFUBuff *)NvOsAlloc(sizeof(struct NvOdmFFUBuff));
        if (pHeadBuff2 == NULL)
        {
            NvOsDebugPrintf("\nError in memory allocation\n");
            e =  NvError_InsufficientMemory;
            goto clean;
        }
        pHeadBuff2->pNext = NULL;
    }

    pTempBuff = pHeadBuff1;
    do
    {
        count++;
        do
        {
            // Deciding the Bytes to receive
            if (BytesRemaining > NvOdmFFU_BUFF_LEN)
                BytesToReceive = NvOdmFFU_BUFF_LEN;
            else
                BytesToReceive = (NvU32)BytesRemaining;

            // Upgradation file reception
            NV_CHECK_ERROR_CLEAN_3P(
                Nv3pDataReceive(hSock, (void *)pTempBuff->data, BytesToReceive,
                             &BytesWritten, 0), Nv3pStatus_DataTransferFailure);
            pTempBuff->data_len = BytesWritten;

            BytesRemaining -= BytesWritten;

            if (BytesRemaining)
            {
                /*
                 * If the file is not received completely create
                 * the next buffer node
                 */
                pTempBuff->pNext = (struct NvOdmFFUBuff *)
                                    NvOsAlloc(sizeof(struct NvOdmFFUBuff));
                if (pTempBuff->pNext == NULL)
                {
                    NvOsDebugPrintf("\nError in memory allocation\n");
                    e = NvError_InsufficientMemory;
                    goto clean;
                }
                pTempBuff = pTempBuff->pNext;
                pTempBuff->pNext = NULL;
            }
        } while (BytesRemaining);

        /*
         * Condiion will pass if two files are passed
         * and only one file is received
         */
        if (count < NoOfFiles)
        {
            pTempBuff = pHeadBuff2;
            BytesRemaining = max_filelen2;
        }

    } while (BytesRemaining);

    e = NvOdmFFUMain(pHeadBuff1, pHeadBuff2) ;
    if (!e)
        return ReportStatus(hSock, "", s, 0);
clean:
    s = Nv3pStatus_FuelGaugeFwUpgradeFailure;
    if (e)
        NvOsDebugPrintf(
            "\nFuelGaugeFwUpgrade failed. NvError %u NvStatus %u\n", e, s);
    return ReportStatus(hSock, Message, s, 0);
}
#endif

static NvError
UpgradeDeviceFirmware(
    Nv3pSocketHandle hSock,
    Nv3pCommand command,
    char *Message,
    void *arg)
{
    NvU8 *pBuffer = NULL;
    NvU8 *pFirmwareData = NULL;
    NvDdkBlockDevHandle DevHandle = NULL;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdOdmCommand *b = (Nv3pCmdOdmCommand *)arg;
    Nv3pOdmExtCmdUpgradeDeviceFirmware *a =
        (Nv3pOdmExtCmdUpgradeDeviceFirmware*)
        &b->odmExtCmdParam.upgradeDeviceFirmware;
    NvDdkBlockDevIoctl_DeviceFirmwareUpgradeInputArgs InArgs;
    NvError e;
    NvU32 BytesRemaining = a->FileLength;

    NvOsDebugPrintf("Firmware file length = %d\n", BytesRemaining);
    pFirmwareData = NvOsAlloc(a->FileLength);
    if (!pFirmwareData)
    {
        NV_CHECK_ERROR_CLEAN_3P(
            NvError_InsufficientMemory, Nv3pStatus_InvalidState);
    }

    pBuffer = pFirmwareData;
    e = Nv3pCommandComplete(hSock, command, arg, 0);
    if (e)
        goto clean;

    while (BytesRemaining)
    {
        NvU32 BytesToWrite;
        if (BytesRemaining > NV3P_STAGING_SIZE)
            BytesToWrite = NV3P_STAGING_SIZE;
        else
            BytesToWrite = (NvU32)BytesRemaining;

        e = Nv3pDataReceive(hSock, s_pState->pStaging, BytesToWrite,
                           &BytesToWrite, 0);
        if (e)
            goto clean;
        BytesRemaining -= BytesToWrite;
        NvOsMemcpy(pBuffer, s_pState->pStaging, BytesToWrite);
        pBuffer = pBuffer + BytesToWrite;
    }

    NvOsDebugPrintf("Upgrade Device Firmware\n");

    NV_CHECK_ERROR_CLEAN_3P(NvDdkBlockDevMgrDeviceOpen(
                NvRmModuleID_Sdio,
                3,
                0,  /* MinorInstance */
                &DevHandle),
                Nv3pStatus_MassStorageFailure);

    InArgs.pData = (void*)pFirmwareData;
    NV_CHECK_ERROR_CLEAN_3P(DevHandle->NvDdkBlockDevIoctl(
                DevHandle,
                NvDdkBlockDevIoctlType_UpgradeDeviceFirmware,
                sizeof(InArgs),
                0,
                (const void*)&InArgs,
                NULL),
                Nv3pStatus_MassStorageFailure);

    DevHandle->NvDdkBlockDevClose(DevHandle);
    NvOsDebugPrintf("Upgrade Device Firmware done\n");
    return e;
clean:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nUpgrade Device Firmware failed. NvError %u NvStatus %u\n", e, s);
    }
    return e;
}

COMMAND_HANDLER(OdmCommand)
{
    NvError e = NvError_NotInitialized;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU32 size = 0;
    NvU8 *buffer = NULL;
    NvU8 dummy;
    NvU8 *tn_id = NULL;
    NvU8 i = 0;
    Nv3pCmdOdmCommand *a = (Nv3pCmdOdmCommand *)arg;

#if NVODM_BOARD_IS_SIMULATION==0
    CHECK_VERIFY_ENABLED();

    switch (a->odmExtCmd)
    {
        case Nv3pOdmExtCmd_TegraTabFuseKeys:
            return TegraTabFuseKeys( hSock, command, arg);

        case Nv3pOdmExtCmd_TegraTabVerifyFuse:
            return TegraTabVerifyFuse( hSock, command, arg);

        case Nv3pOdmExtCmd_FuelGaugeFwUpgrade:
            return  FuelGaugeFwUpgrade(hSock, command, arg);

        case Nv3pOdmExtCmd_UpdateuCFw:
            return UpdateuCFw(hSock, command, arg);
        case Nv3pOdmExtCmd_VerifySdram:
            NvOsDebugPrintf(
                    "sdram validation can not be done at bootloader level\n");
            NV_CHECK_ERROR_FAIL_3P(
                NvError_NotImplemented,
                Nv3pStatus_NotImplemented
            );
            break;
        case Nv3pOdmExtCmd_Get_Tnid:
            {
                NvOsDebugPrintf("Reading TN_ID from the device \n");
                NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(
                    NvDdkFuseDataType_ReservedOdm, &dummy, &size));
                buffer = NvOsAlloc(size);
                if (!buffer)
                    NV_CHECK_ERROR_FAIL_3P(
                        NvError_InsufficientMemory,
                        Nv3pStatus_InvalidState);

                NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(
                    NvDdkFuseDataType_ReservedOdm, buffer, &size));
                a->odmExtCmdParam.tnid_info.length = 4;
                tn_id = NvOsAlloc(a->odmExtCmdParam.tnid_info.length);
                for (i = 0; i < a->odmExtCmdParam.tnid_info.length; i++)
                {
                    tn_id[i] = buffer[7 - i];
                }
                NV_CHECK_ERROR_CLEANUP(Nv3pCommandComplete(hSock, command, arg, 0));
                NV_CHECK_ERROR_FAIL_3P(Nv3pDataSend(
                    hSock, tn_id, (a->odmExtCmdParam.tnid_info.length), 0),
                                        Nv3pStatus_BadParameter);
                NvOsDebugPrintf("TN_ID Read success \n");
            }
            break;
        case Nv3pOdmExtCmd_Verify_Tid:
            {
                NvU32 file_size = 0;
                NvU32 tid_size = 0;
                NvU32 OpMode;
                char response[NV3P_STRING_MAX] = {'\0'};

               NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(
                    NvDdkFuseDataType_T_ID, &dummy, &size));
                buffer = NvOsAlloc(size);
                if (!buffer)
                    NV_CHECK_ERROR_FAIL_3P(
                        NvError_InsufficientMemory,
                        Nv3pStatus_InvalidState);

                NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(
                    NvDdkFuseDataType_T_ID, buffer, &size));

                file_size = a->odmExtCmdParam.tnid_info.length;

                tid_size = size;
                size = sizeof(NvU32);
                NV_CHECK_ERROR_CLEAN_3P(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                            (void *)&OpMode, (NvU32 *)&size),
                        Nv3pStatus_InvalidPartition);

                a->odmExtCmdParam.tnid_info.length = NV3P_STRING_MAX;

                NV_CHECK_ERROR_CLEAN_3P(
                    Nv3pCommandComplete(hSock, command, arg, 0),
                        Nv3pStatus_CmdCompleteFailure);

                NV_CHECK_ERROR_FAIL_3P(Nv3pDataReceive(
                    hSock, s_pState->pStaging, file_size, 0, 0),
                    Nv3pStatus_DataTransferFailure);

                if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecureSBK ||
                    OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
                {
                    if (file_size < NV3P_AES_HASH_BLOCK_LEN)
                        {
                            NvOsStrcpy(response, "invalid T_ID");
                            FastbootError("%s\n", response);
                            goto send_status;
                        }
                     NvSysUtilEncryptData(s_pState->pStaging, file_size,
                            NV_TRUE, NV_TRUE, NV_TRUE, OpMode);
                }
                if (!NvOsMemcmp(s_pState->pStaging, buffer, tid_size))
                {
                    NvOsStrcpy(response, "valid T_ID");
                    FastbootStatus("%s\n", response);
                }
                else
                {
                    NvOsStrcpy(response, "invalid T_ID");
                    FastbootError("%s\n", response);
                }
send_status:
                NV_CHECK_ERROR_FAIL_3P(Nv3pDataSend(
                hSock, (NvU8 *)response, NV3P_STRING_MAX, 0),
                                    Nv3pStatus_DataTransferFailure);
            }
            break;

        case Nv3pOdmExtCmd_UpgradeDeviceFirmware:
            size = NV_TRUE;
            e = UpgradeDeviceFirmware(hSock, command,Message,arg);
            break;

        default:
            NV_CHECK_ERROR_FAIL_3P(
                NvError_NotImplemented,
                Nv3pStatus_NotImplemented
            );
            break;
    }
    if (!size)
        NV_CHECK_ERROR_CLEAN_3P(
            Nv3pCommandComplete(hSock, command, arg, 0),
                Nv3pStatus_CmdCompleteFailure);
fail:
    if (buffer)
        NvOsFree(buffer);
    if (tn_id)
        NvOsFree(tn_id);
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf("\nOdmCommand failed. NvError %u NvStatus %u\n", e, s);
#endif
    return ReportStatus(hSock, Message, s, 0);
}

NvError
LoadPartitionTable(Nv3pPartitionTableLayout *arg)
{
    NvError e = NvSuccess;
    NvU8 *customerData = NULL;
    NvU32 size = 0, instance = 0;
    NvBctAuxInfo *auxInfo;
    NvU32 startLogicalSector, numLogicalSectors;
    NvU32 blockDevInstance;
    NvBootDevType bootDevId;
    NvDdkBlockDevMgrDeviceId blockDevId;
    NvBctHandle hBctHandle = NULL;

    NV_CHECK_ERROR_CLEANUP(
        GetSecondaryBootDevice(&bootDevId, &blockDevInstance)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvBl3pConvertBootToRmDeviceType(bootDevId, &blockDevId)
    );

    // When ReadPartitionTable command is called, but not from --skip.
    if (!arg || (!arg->NumLogicalSectors && !arg->StartLogicalAddress))
    {
        /* If BCT is to be read from SD then read it, but do not
         * replace in-memory BCT.
         */
        if(arg != NULL && arg->ReadBctFromSD)
        {
            hBctHandle = (NvBctHandle) ReadBctFromSD();
            if(hBctHandle == NULL)
            {
                e = NvError_NotInitialized;
                goto fail;
            }
        }
        else
        {
            hBctHandle = s_pState->BctHandle;
        }

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hBctHandle, NvBctDataType_AuxDataAligned,
                         &size, &instance, NULL)
        );

        customerData = NvOsAlloc(size);

        if (!customerData)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NvOsMemset(customerData, 0, size);
        // get the customer data
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hBctHandle, NvBctDataType_AuxDataAligned,
                         &size, &instance, customerData)
        );
        auxInfo = (NvBctAuxInfo*)customerData;
        startLogicalSector = auxInfo->StartLogicalSector;
        numLogicalSectors  = auxInfo->NumLogicalSectors;
    }
    // This handles ReadPartitionTable command called from --skip &
    // values(StartLogicalSector & NumLogicalSectors) of PT partition
    // are passed as arguments.
    else
    {
        startLogicalSector = arg->StartLogicalAddress;
        numLogicalSectors = arg->NumLogicalSectors;
    }

    //FIXME: Should probably look at the opmode to
    //decide whether the table is encrypted or not.
    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrLoadTable(blockDevId, blockDevInstance, startLogicalSector,
                           numLogicalSectors, NV_TRUE, NV_FALSE)
    );

fail:
    if (customerData)
        NvOsFree(customerData);
    if(arg != NULL && arg->ReadBctFromSD && hBctHandle != NULL)
        NvOsFree(hBctHandle);
    if (e)
        NvOsDebugPrintf(
            "\nLoadPartitionTable failed. NvError %u NvStatus %u\n", e, 0);
    return e;
}

void
UnLoadPartitionTable(void)
{
    if (s_pState->IsValidPt)
    {
        s_pState->IsValidPt = NV_FALSE;
        NvPartMgrUnLoadTable();
    }
}

/* this may be called with hSock=NULL (during a Go command) */
COMMAND_HANDLER(Sync)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvU32 size, instance = 0, numBootloaders;
    NvU32 OpModeSize;
    NvDdkBlockDevHandle DevHandle = NULL;
    NvU32 BctLength = 0;
    NvU8* buffer = 0;
    NvDdkFuseOperatingMode OpMode;
    NvU8 bctPartId;
    NvBctAuxInfo* AuxInfo = 0;
    char Message[NV3P_STRING_MAX] = {'\0'};
#if TEST_RECOVERY
    NvU8 *tempPtr = NULL;
#endif
    NvFsMountInfo minfo;

    // clear LimitedPowerMode flag
    CHECK_VERIFY_ENABLED();

    size = 0;
    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(s_pState->BctHandle, NvBctDataType_AuxDataAligned,
                 &size, &instance, NULL),
        Nv3pStatus_InvalidBCTSize);

    AuxInfo = NvOsAlloc(size);
    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(s_pState->BctHandle, NvBctDataType_AuxDataAligned,
                 &size, &instance, AuxInfo),
        Nv3pStatus_InvalidBCTSize);
    if (AuxInfo->LimitedPowerMode)
    {
        AuxInfo->LimitedPowerMode = NV_FALSE;
        NV_CHECK_ERROR_FAIL_3P(
            NvBctSetData(s_pState->BctHandle, NvBctDataType_AuxDataAligned,
                     &size, &instance, AuxInfo),
            Nv3pStatus_InvalidBCTSize);
    }

    size = sizeof(NvU8);
    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(s_pState->BctHandle, NvBctDataType_BctSize,
                 &size, &instance, &BctLength),
        Nv3pStatus_InvalidBCTSize);

    buffer = NvOsAlloc(BctLength);
    if(!buffer)
        NV_CHECK_ERROR_FAIL_3P(
            NvError_InsufficientMemory,
            Nv3pStatus_InvalidState);

    size = sizeof(NvU8);
    NV_CHECK_ERROR_FAIL_3P(
        NvBctGetData(s_pState->BctHandle, NvBctDataType_BctPartitionId,
                 &size, &instance, &bctPartId),
        Nv3pStatus_InvalidBCTPartitionId);

    NV_CHECK_ERROR_FAIL_3P(
        LoadPartitionTable(NULL),
            Nv3pStatus_InvalidPartitionTable);

    if (bctPartId)
    {
        size = sizeof(NvU32);
        NV_CHECK_ERROR_FAIL_3P(
            NvBctGetData(s_pState->BctHandle,
                NvBctDataType_NumEnabledBootLoaders,
                     &size, &instance, &numBootloaders),
            Nv3pStatus_InvalidBCT);

        if (!numBootloaders)
            NV_CHECK_ERROR_FAIL_3P(
                NvError_InvalidState, Nv3pStatus_NoBootloader);

        NV_CHECK_ERROR_FAIL_3P(
            NvBuBctCreateBadBlockTable(s_pState->BctHandle, bctPartId),
            Nv3pStatus_ErrorBBT);
#ifdef NV_EMBEDDED_BUILD
        {
            NvU8 EnableFailBack = 1;
            NV_CHECK_ERROR_FAIL_3P(
                NvBctSetData(s_pState->BctHandle, NvBctDataType_EnableFailback,
                    &size, &instance, &EnableFailBack),
                Nv3pStatus_InvalidBCT
            );
        }

        NV_CHECK_ERROR_FAIL_3P(
            NvProtectBctInvariant(s_pState->BctHandle,
                s_pState->BitHandle, bctPartId, NvPrivDataBuffer,
                NvPrivDataBufferSize),
                Nv3pStatus_BctInvariant
        );
        NvOsFree(NvPrivDataBuffer);
        NvPrivDataBufferSize = 0;
#endif
        size = BctLength;

        // Get current operating mode
        OpModeSize =  sizeof(NvU32);
        NV_CHECK_ERROR_FAIL_3P(
                NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                    (void *)&OpMode, (NvU32 *)&OpModeSize),
                Nv3pStatus_InvalidPartition);

        if (OpMode == NvDdkFuseOperatingMode_Undefined)
            NV_CHECK_ERROR_FAIL_3P(
                NvError_InvalidState, Nv3pStatus_InvalidState);
        //Sign/Write the BCT here
        NV_CHECK_ERROR_FAIL_3P(
            NvBuBctCryptoOps(s_pState->BctHandle, OpMode, &size, buffer,
                NvBuBlCryptoOp_EncryptAndSign), Nv3pStatus_CryptoFailure);
#if TEST_RECOVERY
        //FIXME: Remove explicit BCT corruption before release
        //Don't corrupt the BCT hash, don't do an explicit recovery
        tempPtr = buffer;
        tempPtr[0]++;
#endif
        size = BctLength;

        NV_CHECK_ERROR_FAIL_3P(
            NvBuBctUpdate(
                s_pState->BctHandle, s_pState->BitHandle,
                    bctPartId, size, buffer),
                        Nv3pStatus_BctWriteFailure);

#if TEST_RECOVERY
        NV_CHECK_ERROR_FAIL_3P(
            NvBuBctRecover(
                s_pState->BctHandle, s_pState->BitHandle,
                    bctPartId),
                        Nv3pStatus_MassStorageFailure);
#endif

        if (s_OpMode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
            NV_CHECK_ERROR_FAIL_3P(
                NvBuBctReadVerify(s_pState->BctHandle,
                    s_pState->BitHandle, bctPartId, OpMode, size),
                Nv3pStatus_BctReadVerifyFailure);
    }
    // sanity check
    if (s_pState->IsValidDeviceInfo)
    {
        // Alternate way to get the device Id for region table verification
        // Assuming that device for PT partition has the region table
        NV_CHECK_ERROR_FAIL_3P(
            NvPartMgrGetFsInfo(s_pState->PtPartitionId, &minfo),
                                    Nv3pStatus_InvalidPartition);

        // verify region table
        NV_CHECK_ERROR_FAIL_3P(
            NvDdkBlockDevMgrDeviceOpen(
                (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
                    minfo.DeviceInstance,
                              0,  /* MinorInstance */
                              &DevHandle),
                              Nv3pStatus_MassStorageFailure);
        NV_CHECK_ERROR_FAIL_3P(
                              DevHandle->NvDdkBlockDevIoctl(
                              DevHandle,
                              NvDdkBlockDevIoctlType_VerifyCriticalPartitions,
                              0,
                              0,
                              0,
                              NULL),
                              Nv3pStatus_MassStorageFailure);
    }
if (hSock)
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        if (hSock)
            Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if(buffer)
        NvOsFree(buffer);
    if(AuxInfo)
        NvOsFree(AuxInfo);
    if (e)
        NvOsDebugPrintf("\nSync failed. NvError %u NvStatus %u\n",
                                                            e, s);
    if(hSock)
    {
        return ReportStatus(hSock, Message, s, 0);
    }
    else
    {
        return e;
    }
}

COMMAND_HANDLER(VerifyPartitionEnable)
{

    char Message[NV3P_STRING_MAX] = {'\0'};
    NvError e = NvError_InvalidState;
    Nv3pStatus s = Nv3pStatus_Ok;

    // If verification for a partition has not been enabled,
    // enable it else return error.
    // For the first request for verification, mark verification
    // as pending.
    if (!gs_VerifyPartition)
    {
        gs_VerifyPartition= NV_TRUE;
        gs_VerifyPending = NV_TRUE;
        e = NvSuccess;
    }
    if (hSock)
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
            Nv3pStatus_CmdCompleteFailure);
    if(e)
        if (hSock)
            Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
        NvOsDebugPrintf(
            "\nVerifyPartitionEnable failed. NvError %u NvStatus %u\n", e, s);
    if(hSock)
    {
        return ReportStatus(hSock, Message, s, 0);
    }
    else
    {
        return e;
    }

}

COMMAND_HANDLER(VerifyPartition)
{
    NvError e = NvError_InvalidState;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};

    PartitionToVerify *pPartitionToVerify;
    Nv3pCmdVerifyPartition *a = (Nv3pCmdVerifyPartition *)arg;

    CHECK_VERIFY_ENABLED();

    // check if partition was marked for verification
    NV_CHECK_ERROR_FAIL_3P(
        LocatePartitionToVerify(a->Id, &pPartitionToVerify),
                                    Nv3pStatus_InvalidPartition);

    // read and verify partition data
    NV_CHECK_ERROR_FAIL_3P(
        ReadVerifyData(pPartitionToVerify, &s), Nv3pStatus_DataTransferFailure
   );
    if (hSock)
        NV_CHECK_ERROR_CLEAN_3P(
                Nv3pCommandComplete(hSock, command, arg, 0),
                                Nv3pStatus_CmdCompleteFailure);

fail:
    if(e)
    if (hSock)
        Nv3pNack(hSock, Nv3pNackCode_BadData);

clean:
    if (e)
        NvOsDebugPrintf(
            "\nVerifyPartition failed. NvError %u NvStatus %u\n", e, s);
    if(hSock)
    {
        return ReportStatus(hSock, Message, s, 0);
    }
    else
    {
        return e;
    }
}

COMMAND_HANDLER(GetDevInfo)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    Nv3pCmdGetDevInfo a;
    NvDdkBlockDevInfo BlockDevInfo;

    CHECK_VERIFY_ENABLED();

    NV_CHECK_ERROR_CLEANUP(Nv3pGetDeviceInfo(&BlockDevInfo, &s, Message));

    a.BytesPerSector = BlockDevInfo.BytesPerSector;
    a.SectorsPerBlock = BlockDevInfo.SectorsPerBlock;
    a.TotalBlocks = BlockDevInfo.TotalBlocks;

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, &a, 0),
            Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e)
     NvOsDebugPrintf("\nGetDevInfo failed. NvError %u NvStatus %u\n", e, s);

     return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(GetNv3pServerVersion)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdGetNv3pServerVersion a;
    char Message[NV3P_STRING_MAX] = {'\0'};

    NvOsSnprintf(a.Version, sizeof(a.Version), "%s", NVFLASH_VERSION);
    NvOsDebugPrintf("Nv3pServer Version %s\n", NVFLASH_VERSION);

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pCommandComplete(hSock, command, &a, 0),
            Nv3pStatus_CmdCompleteFailure);
fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf(
            "\nGetNv3pServerVersion failed. NvError %u NvStatus %u\n", e, s);
    }

    return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(FuseWrite)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};
    NvU8 *data = NULL;
    NvU8 *data_buffer = NULL;
    NvU32 bytesLeftToTransfer = 0;
    NvU32 transferSize = 0;
    Nv3pCmdFuseWrite *a = (Nv3pCmdFuseWrite *)arg;
    NvFuseWrite *buffer = NULL;
    NvU8 i = 0;
    NvU32 size = 0;
    NvU8 pData;

    if (!a->Length)
    NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState,
                              Nv3pStatus_InvalidState);
    data = NvOsAlloc(a->Length);
    data_buffer = data;
    if (!data)
        NV_CHECK_ERROR_FAIL_3P(NvError_InvalidState,
                              Nv3pStatus_InvalidState);
    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, a, 0),
        Nv3pStatus_CmdCompleteFailure);
    bytesLeftToTransfer = a->Length;
    do
    {
        transferSize = (bytesLeftToTransfer > NV3P_STAGING_SIZE) ?
                        NV3P_STAGING_SIZE :
                        bytesLeftToTransfer;
        NV_CHECK_ERROR_CLEAN_3P(Nv3pDataReceive(
                hSock, data, transferSize, 0, 0),
                        Nv3pStatus_DataTransferFailure);
        data += transferSize;
        bytesLeftToTransfer -= transferSize;
    } while (bytesLeftToTransfer);

    buffer = (NvFuseWrite *)data_buffer;
#if NVODM_BOARD_IS_SIMULATION == 0
    for (i = 0; buffer->fuses[i].fuse_type != 0; i++)
    {
        size = 0;
        NV_CHECK_ERROR_CLEAN_3P(
            NvDdkFuseGet(buffer->fuses[i].fuse_type, &pData, &size),
            Nv3pStatus_BadParameter);
        NV_CHECK_ERROR_CLEAN_3P(
            NvDdkFuseSet(buffer->fuses[i].fuse_type,
                  (NvU8 *)buffer->fuses[i].fuse_value, &size),
                            Nv3pStatus_BadParameter);
    }
    NvOsDebugPrintf("Programming fuse ...\n");
    NV_CHECK_ERROR_CLEAN_3P(NvDdkFuseProgram(),
        Nv3pStatus_BadParameter);
    NvOsDebugPrintf("Programmed fuse successsfully\n");
#endif

fail:
    if (e)
        Nv3pNack(hSock, Nv3pNackCode_BadData);
clean:
    if (e == NvError_SecurityModeBurn)
        s = Nv3pStatus_SecurityModeFuseBurn;
    if (e == NvError_InvalidCombination)
        s = Nv3pStatus_InvalidCombination;
    if (e)
        NvOsDebugPrintf("\nFuseWrite failed. NvError %u NvStatus %u\n", e, s);
    if (data_buffer)
        NvOsFree(data_buffer);
     return ReportStatus(hSock, Message, s, 0);
}

COMMAND_HANDLER(SkipSync)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    char Message[NV3P_STRING_MAX] = {'\0'};

    NV_CHECK_ERROR_CLEAN_3P(
        Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_CmdCompleteFailure);

clean:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\n SkipSync failed. NvError %u NvStatus %u\n", e, s);
    }

    return ReportStatus(hSock, "", s, 0);
}

#if NVODM_BOARD_IS_SIMULATION==0
COMMAND_HANDLER(Reset)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    NvU32 pmc_scratch0 = 0;
    Nv3pCmdReset *a = (Nv3pCmdReset *)arg;
    NvRmDeviceHandle vRmDevice = NULL;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandComplete(hSock, command, arg, 0)
    );

    NvOsSleepMS(a->DelayMs);
    switch (a->Reset)
    {
        case Nv3pResetType_RecoveryMode:
            pmc_scratch0 = NV_READ32(PMC_PA_BASE + 0x50) | FORCED_RECOVERY_MODE;
            NV_WRITE32(PMC_PA_BASE + 0x50, pmc_scratch0);
            break;
        case Nv3pResetType_NormalBoot:
            break;
        default:
            break;
    }

fail:
    if (e)
    {
        s = Nv3pStatus_InvalidState;
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nBootloader Reset failed. NvError %u NvStatus %u\n", e, s);
        return ReportStatus(hSock, "", s, 0);
    }

    e = ReportStatus(hSock, "", s, 0);
    NV_CHECK_ERROR_CLEANUP(NvRmOpenNew(&vRmDevice));
    NvRmModuleReset(vRmDevice, NvRmPrivModuleID_System);
    NvRmClose(vRmDevice);

    return e;
}
#endif

COMMAND_HANDLER(ReadNctItem)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)arg;

#ifdef NV_USE_NCT
    char Message[NV3P_STRING_MAX] = {'\0'};

    // verify PT is available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);
        s_pState->IsValidPt = NV_TRUE;
    }
#if NVODM_BOARD_IS_SIMULATION==0
    a->Type = NvNctGetItemType(a->EntryIdx);
#endif

    NV_CHECK_ERROR_FAIL_3P(Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_BadParameter);

    a->Data = NvOsAlloc(sizeof(nct_item_type));

#if NVODM_BOARD_IS_SIMULATION==0
    NV_CHECK_ERROR_CLEANUP(NvNctInitPart());
    NV_CHECK_ERROR_CLEANUP(NvNctReadItem(a->EntryIdx, a->Data));
#endif

    NV_CHECK_ERROR_FAIL_3P(
        Nv3pDataSend(hSock, (NvU8 *)(a->Data), sizeof(nct_item_type), 0),
        Nv3pStatus_BadParameter);

#else
    NvOsDebugPrintf("\nReadNctItem command is not supported\n");
    e = NvError_NotSupported;
    s = Nv3pStatus_NotSupported;
    goto fail;
#endif

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nRead nct item(%d) failed. NvError %u NvStatus %u\n", e, s);
    }

    NvOsFree(a->Data);
    return ReportStatus(hSock, "", s, 0);
}

COMMAND_HANDLER(WriteNctItem)
{
    NvError e = NvSuccess;
    Nv3pStatus s = Nv3pStatus_Ok;
    Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)arg;

#ifdef NV_USE_NCT
    char Message[NV3P_STRING_MAX] = {'\0'};

    // verify PT is available
    if (!s_pState->IsValidPt)
    {
        NV_CHECK_ERROR_FAIL_3P(LoadPartitionTable(NULL),
            Nv3pStatus_PartitionTableRequired);
        s_pState->IsValidPt = NV_TRUE;
    }

    NV_CHECK_ERROR_FAIL_3P(Nv3pCommandComplete(hSock, command, arg, 0),
        Nv3pStatus_BadParameter);

    a->Data = NvOsAlloc(sizeof(nct_item_type));
    NvOsMemset(a->Data, 0, (sizeof(nct_item_type)));

    NV_CHECK_ERROR_CLEANUP(Nv3pDataReceive(hSock, (NvU8 *)(a->Data),
        sizeof(nct_item_type), 0, 0));

#if NVODM_BOARD_IS_SIMULATION==0
    NV_CHECK_ERROR_CLEANUP(NvNctInitPart());
    NV_CHECK_ERROR_CLEANUP(NvNctWriteItem(a->EntryIdx, a->Data));
#endif
#else
    NvOsDebugPrintf("\nWriteNctItem command is not supported\n");
    e = NvError_NotSupported;
    s = Nv3pStatus_NotSupported;
    goto fail;

#endif

fail:
    if (e)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadData);
        NvOsDebugPrintf("\nWrite nct item(%d) failed. NvError %u NvStatus %u\n", e, s);
    }

    NvOsFree(a->Data);
    return ReportStatus(hSock, "", s, 0);
}

NvError
AllocateState()
{
    NvError e;
    Nv3pServerState *p = NULL;
    NvU32 BctLength = 0;
    NvU32 Staging_buffer_size = 0;
#if NVODM_BOARD_IS_SIMULATION==0

    void *pPhysBuffer = 0;
    NvRmDeviceHandle hRm;
#endif
#if NVODM_BOARD_IS_SIMULATION
    NvU8 *BctBuffer = 0;
#endif
    NvU32 Size;
    p = (Nv3pServerState *)NvOsAlloc(sizeof(Nv3pServerState));
    if (!p)
        return NvError_InsufficientMemory;

#if NVODM_BOARD_IS_SIMULATION==0
#if ENABLE_THREADED_DOWNLOAD
    Staging_buffer_size = (NV3P_STAGING_SIZE *  NUM_BUFFERS) + USB_ALGINMENT;
#else
    Staging_buffer_size = NV3P_STAGING_SIZE + USB_ALGINMENT;
#endif

    NvRmOpen(&hRm, 1);
    e = NvRmMemHandleAlloc(hRm, NULL, 0,
                4, NvOsMemAttribute_WriteCombined,
                Staging_buffer_size, 0, 1, &s_hRmMemHandle);
    if (e != NvSuccess)
    {
        goto fail;
    }
    // Pin the memory and Get Physical Address
    pPhysBuffer = (NvU8 *)NvRmMemPin(s_hRmMemHandle);
    p->pStaging = (NvU8 *)(pPhysBuffer);
#else
    p->pStaging = NvOsAlloc(NV3P_STAGING_SIZE);
#endif

    if (!p->pStaging)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    p->IsValidDeviceInfo = NV_FALSE;
    p->IsCreatingPartitions = NV_FALSE;
    p->NumPartitionsRemaining = 0;
    p->PtPartitionId = 0;

    p->IsLocalBct = NV_FALSE;
    p->IsLocalPt = NV_FALSE;
    p->IsValidPt = NV_FALSE;

    p->BctHandle = NULL;
    p->BitHandle = NULL;
#if NVODM_BOARD_IS_SIMULATION
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, NULL));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrInit());
    NV_CHECK_ERROR_CLEANUP(NvDdkBlockDevMgrInit());
    BctBuffer = NvOsAlloc(BctLength);
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, BctBuffer, &p->BctHandle));
#else
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, &p->BctHandle));
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, &p->BctHandle));
#endif
    NV_CHECK_ERROR_CLEANUP(NvBitInit(&p->BitHandle));

    s_pState = p;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                          (void *)&s_OpMode, (NvU32 *)&Size));

    return NvSuccess;

fail:
    NvOsFree(p);
    if (e)
        NvOsDebugPrintf(
            "\nAllocateState failed. NvError %u NvStatus %u\n", e, 0);
    return e;
}

void
DeallocateState(void)
{
    if (s_pState)
    {
        NvBctDeinit(s_pState->BctHandle);
        NvBitDeinit(s_pState->BitHandle);
#if NVODM_BOARD_IS_SIMULATION==0
       if(NULL != s_hRmMemHandle)
            NvRmMemHandleFree(s_hRmMemHandle);
#else
        NvOsFree(s_pState->pStaging);
#endif
        NvOsFree(s_pState);
        s_pState = NULL;
    }
}

static NvError
GetSecondaryBootDevice(
    NvBootDevType *bootDevId,
    NvU32         *bootDevInstance)
{
    NvBitHandle   hBit = NULL;
    NvU32         Size ;
    NvError       e;
    *bootDevInstance = 0;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelect,
                (void *)bootDevId, (NvU32 *)&Size));
    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_SecBootDevInst,
                (void *)bootDevInstance, (NvU32 *)&Size));
    if (*bootDevId != NvBootDevType_None)
        return NvSuccess;

    Size = sizeof(NvBootDevType);
    NV_CHECK_ERROR_CLEANUP(NvBitInit(&hBit));
    NV_CHECK_ERROR_CLEANUP(
        NvBitGetData(hBit, NvBitDataType_SecondaryDevice,
           &Size, bootDevInstance, bootDevId)
    );
    e = NvSuccess;

 fail:
    NvBitDeinit(hBit);
    if (e)
        NvOsDebugPrintf(
            "\nGetSecondaryBootDevice failed. NvError %u NvStatus %u\n", e, 0);
    return e;
}

NvError Nv3pServer(void)
{
    NvError e;
    Nv3pSocketHandle hSock = 0;
    Nv3pCommand command;
    Nv3pCmdStatus cmd_stat;
    void *arg;
    NvBool bDone = NV_FALSE;
    NvBool bSyncDone = NV_FALSE;

    NV_CHECK_ERROR(AllocateState());

    InitLstPartitionsToVerify();

#if NVODM_BOARD_IS_SIMULATION
    NV_CHECK_ERROR_CLEANUP(Nv3pOpen(&hSock, Nv3pTransportMode_Sema, -1));
#else
    NV_CHECK_ERROR_CLEANUP(Nv3pOpen(&hSock, Nv3pTransportMode_default, 0));

    /* send an ok -- the nvflash application waits for the bootloader
     * (this code) to initialize.
     */
    command = Nv3pCommand_Status;
    cmd_stat.Code = Nv3pStatus_Ok;
    cmd_stat.Flags = 0;

    NvOsMemset(cmd_stat.Message, 0, NV3P_STRING_MAX);
    NV_CHECK_ERROR_CLEANUP(Nv3pCommandSend(hSock, command,
        (NvU8 *)&cmd_stat, 0));
#endif

    for( ;; )
    {
        NV_CHECK_ERROR_CLEANUP(Nv3pCommandReceive(hSock, &command, &arg, 0));

        switch(command) {
            case Nv3pCommand_GetBct:
                NV_CHECK_ERROR_CLEANUP(
                    GetBct(hSock, command, arg)
                );
                break;
            case Nv3pCommand_DownloadBct:
                NV_CHECK_ERROR_CLEANUP(
                    DownloadBct(hSock, command, arg)
                );
                break;
            case Nv3pCommand_UpdateBct:
                NV_CHECK_ERROR_CLEANUP(
                    UpdateBct(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_SetDevice:
                NV_CHECK_ERROR_CLEANUP(
                    SetDevice(hSock, command, arg)
                );
                break;
            case Nv3pCommand_StartPartitionConfiguration:
                NV_CHECK_ERROR_CLEANUP(
                    StartPartitionConfiguration(hSock, command, arg)
                );
                break;
            case Nv3pCommand_EndPartitionConfiguration:
                NV_CHECK_ERROR_CLEANUP(
                    EndPartitionConfiguration(hSock, command, arg)
                );
                break;
            case Nv3pCommand_FormatPartition:
                NV_CHECK_ERROR_CLEANUP(
                    FormatPartition(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_DownloadPartition:
                NV_CHECK_ERROR_CLEANUP(
                    DownloadPartition(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_QueryPartition:
                NV_CHECK_ERROR_CLEANUP(
                    QueryPartition(hSock, command, arg)
                );
                break;
            case Nv3pCommand_CreatePartition:
                NV_CHECK_ERROR_CLEANUP(
                    CreatePartition(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_ReadPartition:
                NV_CHECK_ERROR_CLEANUP(
                    ReadPartition(hSock, command, arg)
                );
                break;
            case Nv3pCommand_RawDeviceRead:
                NV_CHECK_ERROR_CLEANUP(
                    RawReadPartition(hSock, command, arg)
                );
                break;
            case Nv3pCommand_RawDeviceWrite:
                NV_CHECK_ERROR_CLEANUP(
                    RawWritePartition(hSock, command, arg)
                );
                break;
            case Nv3pCommand_SetBootPartition:
                NV_CHECK_ERROR_CLEANUP(
                    SetBootPartition(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_ReadPartitionTable:
                NV_CHECK_ERROR_CLEANUP(
                    ReadPartitionTable(hSock, command, arg)
                );
                break;
            case Nv3pCommand_DeleteAll:
                NV_CHECK_ERROR_CLEANUP(
                    DeleteAll(hSock, command, arg)
                );
                break;
            case Nv3pCommand_FormatAll:
                NV_CHECK_ERROR_CLEANUP(
                    FormatAll(hSock, command, arg)
                );
                break;
            case Nv3pCommand_Obliterate:
                NV_CHECK_ERROR_CLEANUP(
                    Obliterate(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_OdmOptions:
                NV_CHECK_ERROR_CLEANUP(
                    OdmOptions(hSock, command, arg)
                );
                break;
            case Nv3pCommand_OdmCommand:
                NV_CHECK_ERROR_CLEANUP(
                    OdmCommand(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_Go:
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pCommandComplete(hSock, command, arg, 0)
                );

                NV_CHECK_ERROR_CLEANUP(
                    ReportStatus(hSock, "", Nv3pStatus_Ok, 0)
                );
                bDone = NV_TRUE;

                break;
            case Nv3pCommand_Sync:
                NV_CHECK_ERROR_CLEANUP(
                    Sync(hSock, command, arg)
                );
                bSyncDone = NV_TRUE;
                break;
            case Nv3pCommand_VerifyPartitionEnable:
                NV_CHECK_ERROR_CLEANUP(
                    VerifyPartitionEnable(hSock, command, arg)
                );
                break;
            case Nv3pCommand_VerifyPartition:
                NV_CHECK_ERROR_CLEANUP(
                    VerifyPartition(hSock, command, arg)
                );
                break;
            case Nv3pCommand_EndVerifyPartition:
                gs_VerifyPending = NV_FALSE;
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pCommandComplete(hSock, command, arg, 0)
                );
                break;
            case Nv3pCommand_SetTime:
                NV_CHECK_ERROR_CLEANUP(
                    SetTime(hSock, command, arg)
                );
                break;
            case Nv3pCommand_GetDevInfo:
                NV_CHECK_ERROR_CLEANUP(
                    GetDevInfo(hSock, command, arg)
                );
                break;
            case Nv3pCommand_NvPrivData:
                NV_CHECK_ERROR_CLEANUP(
                    DownloadNvPrivData(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_GetNv3pServerVersion:
                NV_CHECK_ERROR_CLEANUP(
                    GetNv3pServerVersion(hSock, command, arg)
                );
                break;
            case Nv3pCommand_FuseWrite:
                NV_CHECK_ERROR_CLEANUP(
                    FuseWrite(hSock, command, arg)
                );
                bSyncDone = NV_FALSE;
                break;
            case Nv3pCommand_symkeygen:
                NV_CHECK_ERROR_CLEANUP(
                    Symkeygen(hSock, command, arg)
                );
                break;
            case Nv3pCommand_DFuseBurn:
                NV_CHECK_ERROR_CLEANUP(
                    DSession_fuseBurn(hSock, command, arg)
                );
                bSyncDone = NV_TRUE;
                break;
            case Nv3pCommand_SkipSync:
                NV_CHECK_ERROR_CLEANUP(
                    SkipSync(hSock, command, arg)
                );
                bSyncDone = NV_TRUE;
                break;
#if NVODM_BOARD_IS_SIMULATION==0
            case Nv3pCommand_Reset:
                NV_CHECK_ERROR_CLEANUP(
                    Reset(hSock, command, arg)
                );
                bDone = NV_TRUE;
                break;
#endif
            case Nv3pCommand_ReadNctItem:
                NV_CHECK_ERROR_CLEANUP(
                    ReadNctItem(hSock, command, arg)
                );
                break;

            case Nv3pCommand_WriteNctItem:
                NV_CHECK_ERROR_CLEANUP(
                    WriteNctItem(hSock, command, arg)
                );
                break;

            default:
                NvOsDebugPrintf("Warning: Request for an unknown command %d ", command);
                Nv3pNack(hSock, Nv3pNackCode_BadCommand);
                break;
        }

        /* Break when go command was received, sync done
          * and partition verification is not pending.
          */
        if(bDone  && bSyncDone && !gs_VerifyPending)
        {
            break;
        }

    }

    goto clean;

fail:
    if (hSock)
    {
        Nv3pNack(hSock, Nv3pNackCode_BadCommand);
        NV_CHECK_ERROR_CLEANUP(
           ReportStatus(hSock, "", Nv3pStatus_Unknown, 0)
        );
    }

clean:

#if ENABLE_THREADED_DOWNLOAD
    StopWriterThread();
#endif
    // Free memory occupied for Partitionverification, if any
    DeInitLstPartitionsToVerify();

    DeallocateState();

    if (hSock)
        Nv3pClose(hSock);

    return e;
}

