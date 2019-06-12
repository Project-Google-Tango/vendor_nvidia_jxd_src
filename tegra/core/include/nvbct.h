/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Boot Configuration Table Management Framework</b>
 *
 * @b Description: This file declares the APIs for interacting with
 *    the Boot Configuration Table (BCT).
 */

#ifndef INCLUDED_NVBCT_H
#define INCLUDED_NVBCT_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvnct.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup boot_config_mgmt_group BCT Management Framework
 * Declares the APIs for interacting with
 * the Boot Configuration Table (BCT).
 * @ingroup nvbdk_modules
 * @{
 */

//FIXME: Need a cleaner way to obtain the odm option
//offset within the customerdata field of the BCT.
//This offset is a function of the chip as well
//as some pending bugs.
#define NV_AUXDATA_STRUCT_SIZE_AP15 28
#define NV_ODM_OPTION_OFFSET_AP15 (NVBOOT_BCT_CUSTOMER_DATA_SIZE-NV_AUXDATA_STRUCT_SIZE_AP15)
/**
 * Defines data elements of the BCT that can be queried and set via
 * the NvBct API.
 */
typedef enum
{
    /// Version of BCT structure
    NvBctDataType_Version,

    /// Device parameters with which the secondary boot device controller is
    /// initialized
    NvBctDataType_BootDeviceConfigInfo,

    /// Number of valid device parameter sets
    NvBctDataType_NumValidBootDeviceConfigs,

    /// SDRAM parameters with which the SDRAM controller is initialized
    NvBctDataType_SdramConfigInfo,

    /// Number of valid SDRAM parameter sets
    NvBctDataType_NumValidSdramConfigs,

    /// Generic Attribute for specified BL instance
    NvBctDataType_BootLoaderAttribute,

    /// Version number for specified BL instance
    NvBctDataType_BootLoaderVersion,

    /// Partition id of the BCT
    NvBctDataType_BctPartitionId,

    /// Start Block for specified BL instance
    NvBctDataType_BootLoaderStartBlock,

    /// Start Sector for specified BL instance
    NvBctDataType_BootLoaderStartSector,

    /// Length (in bytes) of specified BL instance
    NvBctDataType_BootLoaderLength,

    /// Load Address for specified BL instance
    NvBctDataType_BootLoaderLoadAddress,

    /// Entry Point for specified BL instance
    NvBctDataType_BootLoaderEntryPoint,

    /// Crypto Hash for specified BL instance
    NvBctDataType_BootLoaderCryptoHash,

    /// Number of Boot Loaders that are enabled for booting
    NvBctDataType_NumEnabledBootLoaders,

    /// Bad block table
    NvBctDataType_BadBlockTable,

    /// Partition size
    NvBctDataType_PartitionSize,

    /// Specifies the size of a physical block on the secondary boot device
    /// in log2(bytes).
    NvBctDataType_BootDeviceBlockSizeLog2,

    /// Specifies the size of a page on the secondary boot device in log2(bytes).
    NvBctDataType_BootDevicePageSizeLog2,

    /// Specifies a region of data available to customers of the BR.  This data
    /// region is primarily used by a manufacturing utility or BL to store
    /// useful information that needs to be shared among manufacturing utility,
    /// BL, and OS image.  BR only provides framework and does not use this
    /// data.
    ///
    /// @note Some of this space has already been allocated for use
    /// by NVIDIA.
    NvBctDataType_AuxData,
    NvBctDataType_AuxDataAligned,
    // Version of customer data stored in BCT
    NvBctDataType_CustomerDataVersion,
    // Boot device type stored in BCT customer data
    NvBctDataType_DevParamsType,

    /// Specifies the signature for the BCT structure.
    NvBctDataType_CryptoHash,

    /// Specifies random data used in lieu of a formal initial vector when
    /// encrypting and/or computing a hash for the BCT.
    NvBctDataType_CryptoSalt,

    /// Specifies extent of BCT data to be hashed
    NvBctDataType_HashDataOffset,
    NvBctDataType_HashDataLength,

    /// Customer defined configuration parameter
    NvBctDataType_OdmOption,

    /// Specifies entire BCT
    NvBctDataType_FullContents,

    /// Specifies size of bct
    NvBctDataType_BctSize,

    NvBctDataType_Num,

    /// Specifies a reserved area at the end of the BCT that must be
    /// filled with the padding pattern.
    NvBctDataType_Reserved,

    /// Specifies the type of device for parameter set DevParams[i].
    NvBctDataType_DevType,
    /// Partition Hash Enums
    NvBctDataType_HashedPartition_PartId,
    /// Crypto Hash for specified Partition
    NvBctDataType_HashedPartition_CryptoHash,
    /// Enable Failback
    NvBctDataType_EnableFailback,
    /// One time flashable infos
    NvBctDataType_InternalInfoOneTimeRaw,
    NvBctDataType_InternalInfoVersion,

    NvBctDataType_RsaKeyModulus,
    NvBctDataType_RsaPssSig,
    NvBctDataType_BootloaderRsaPssSig,
    NvBctDataType_NumValidDevType,
    NvBctDataType_UniqueChipId,

    /// Specifies the signed section of the BCT.
    NvBctDataType_SignedSection,

#if ENABLE_NVDUMPER
    /// Specifies Dumpram carveout location
    NvBctDataType_DumpRamCarveOut,
#endif
    NvBctDataType_Max = 0x7fffffff
} NvBctDataType;

typedef enum
{
    NvBctBootDevice_Nand8,
    NvBctBootDevice_Emmc,
    NvBctBootDevice_SpiNor,
    NvBctBootDevice_Nand16,
    NvBctBootDevice_Current,
    NvBctBootDevice_Num,
    NvBctBootDevice_Max = 0x7fffffff
} NvBctBootDevice;

/**
 * Defines the BootDevice Type available
 */
typedef enum
{
    NvStorageBootDevice_Sdmmc,
    NvStorageBootDevice_SnorFlash,
    NvStorageBootDevice_SpiFlash,
    NvStorageBootDevice_Sata,
    NvStorageBootDevice_Resvd,
    NvStorageBootDevice_Max, /* Must appear after the last legal item */
    NvStorageBootDevice_Force32 = 0x7fffffff
} NvStorageBootDevice;

/**
 * Holds the BCT auxiliary information.
 */
typedef struct NvBctAuxInfoRec
{
    /// Holds the BCT page table starting sector.
    NvU16      StartLogicalSector;

    /// Holds the BCT page table number of logical sectors.
    NvU16      NumLogicalSectors;

    /// Holds the BCT fuse_bypass partition starting sector.
    NvU32       StartFuseSector;

    /// Holds the BCT fuse_bypass partition number of sector.
    NvU16       NumFuseSector;

    /// Holds the sticky bit information.
    NvU8        StickyBit;

    /// Holds the NctBoardInfo.
    nct_board_info_type NCTBoardInfo;

    /// Holds the Storage Boot device type.
    NvStorageBootDevice BootDevice;

    /// Holds the Storage Boot device Instance.
    NvU32 Instance;

    /// Limited Power On for manufacturing flow.
    NvBool      LimitedPowerMode;

    /// Holds the starting sector for GP1
    NvU32      StartGP1PhysicalSector;

    /// Holds the NctBatteryMakeData.
    nct_battery_make_data NctBatteryMakeData;

    /// Holds the NctBatteryCountData.
    nct_battery_count_data NctBatteryCountData;

    nct_ddr_type   NctDdrType;

} NvBctAuxInfo;



typedef struct NvBctRec *NvBctHandle;

typedef struct NvBctAuxInternalDataRec
{
    /**
     * Do not change the position or size of any element in this structure,
     * relative to the \b end of the structure. In other words, add new fields
     * at the start of this structure.
     *
     * This ensures that when this struct is embedded into the BCT at the end
     * of the \c CustomerData[] field, all fields in this struct will be at the
     * same offset from the start of the BCT.
     *
     * This is required so that the various consumers and producers of this
     * data, not all of which are part of the NVIDIA SW tree, all agree on the
     * BCT-relative location of the fields in this struct, and that location
     * does not vary.
     */
#if ENABLE_NVDUMPER
    NvU32 DumpRamCarveOut; /** place holder for sdram location */
#endif
    NvU32 CustomerOption2; /**< Also known as ODMDATA2. */
    NvU32 CustomerOption; /**< Also known as ODMDATA. */
    NvU8  BctPartitionId;
} NvBctAuxInternalData;


/**
 * Create a handle to the specified BCT.
 *
 * The supplied buffer must contain a BCT, or if Buffer is NULL then the
 * system's default BCT (the BCT used by the Boot ROM to boot the chip) will be
 * used.
 *
 * This routine must be called before any of the other NvBct*() API's.  The
 * handle must be passed as an argument to all subsequent NvBct*() invocations.
 *
 * The size of the BCT can be queried by specifying a NULL phBct and a non-NULL
 * Size.  In this case, the routine will set *Size equal to the BCT size and
 * return NvSuccess.
 *
 * @param Size size of Buffer, in bytes
 * @param Buffer buffer containing a BCT, or NULL to select the system's default
 *        BCT
 * @param phBct pointer to handle for invoking subsequent operations on the BCT;
 *        can be specified as NULL to query BCT size
 *
 * @retval NvSuccess BCT handle creation successful or BCT size query successful
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_InsufficientMemory Not enough memory to allocate handle
 * @retval NvError_BadParameter No BCT found in Buffer
 */

NvError
NvBctInit(
    NvU32 *Size,
    void *Buffer,
    NvBctHandle *phBct);

/**
 * Release BCT handle and associated resources (but not the BCT itself).
 *
 * @param hBct handle to BCT instance
 */

void
NvBctDeinit(
    NvBctHandle hBct);

/**
 * Retrieve data from the BCT.
 *
 * This API allows the size of the data element to be retrieved, as well as the
 * number of instances of the data element present in the BCT.
 *
 * To retrieve that value of a given data element, allocate a Data buffer large
 * enough to hold the requested data, set *Size to the size of the buffer, and
 * set *Instance to the instance number of interest.
 *
 * To retrieve the size and number of instances of a given type of data element,
 * specified a *Size of zero and a non-NULL Instance (pointer).  As a result,
 * NvBctGetData() will set *Size to the actual size of the data element and
 * *Instance to the number of available instances of the data element in the
 * BCT, then report NvSuccess.
 *
 * @param hBct handle to BCT instance
 * @param DataType type of data to obtain from BCT
 * @param Size pointer to size of Data buffer; set *Size to zero in order to
 *        retrieve the data element size and number of instances
 * @param Instance pointer to instance number of the data element to retrieve
 * @param Data buffer for storing the retrieved instance of the data element
 *
 * @retval NvSuccess BCT data element retrieved successfully
 * @retval NvError_InsufficientMemory data buffer (*Size) too small
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Unknown data type, or illegal instance number
 */

NvError
NvBctGetData(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

/**
 * Set data in the BCT.
 *
 * This API works like NvBctGetData() to retrieve the size and number of
 * instances of the data element in the BCT.
 *
 * To set the value of a given instance of the data element, set *Size to the size
 * of the data element, *Instance to the desired instance, and provide the data
 * value in the Data buffer.
 *
 * @param hBct handle to BCT instance
 * @param DataType type of data to set in the BCT
 * @param Size pointer to size of Data buffer; set *Size to zero in order to
 *        retrieve the data element size and number of instances
 * @param Instance pointer to the instance number of the data element to set
 * @param Data buffer for storing the desired value of the data element
 *
 * @retval NvSuccess BCT data element set successfully
 * @retval NvError_InsufficientMemory data buffer (*Size) too small
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Unknown data type, or illegal instance number
 * @retval NvError_InvalidState Data type is read-only
 */

NvError
NvBctSetData(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data);

/**
 * Gets the offset of \a field(RandomAesBlock) in BCT from where BCT signing
 * starts.
 *
 * @return The Offset of the Sign data for chips more recent than T30/AP20;
 * otherwise returns 16 for T30/AP20.
 */
NvU32 NvBctGetBctSize(void);

/**
 * Get offset of \a Signature field in BCT in which sign is stored.
 *
 * @return Offset of \a Signature, or zero for AP20/T30.
 */
NvU32 NvBctGetSignatureOffset(void);

/**
 * Get offset of of \a RandomAesBlock field in BCT from where BCT signing starts.
 *
 * @return Offset of sign data, 16 for AP20/T30.
 */
NvU32 NvBctGetSignDataOffset(void);
/** @} */

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBCT_H
