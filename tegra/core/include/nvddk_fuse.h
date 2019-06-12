/*
 * Copyright (c) 2010-2014, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Driver Development Kit: Tegra Fuse Programming API</b>
 *
 */

#ifndef INCLUDED_NVDDK_FUSE_H
#define INCLUDED_NVDDK_FUSE_H

/**
 * @defgroup nvddk_fuse_group Fuse Programming APIs
 *
 * Fuse burning is supported by the
 * <a href="#nvddk_if" class="el">NVIDIA Driver Development Kit Fuse interface</a>
 * (declared in this file), which provides direct NvDDK function calls to enable
 * fuse programming; the NvDDK interface is available in the boot loader and
 * as part of the main system image.
 *
 * @warning For factory production, after programming the Secure Boot Key (SBK)
 * or public key hash fuse, it is critical that the factory software reads
 * back the SBK or public key hash fuse to ensure that the value is
 * programmed as desired. (See note below.)
 * This readback verification MUST be done
 * prior to programming the ODM secure mode fuse. Once this fuse is burned,
 * there is no way to recover the SBK or public key hash fuse. For example,
 * if the SBK or public key hash fuse  somehow were programmed incorrectly,
 * or if the host factory software were to lose the key, there would be no
 * way to recover. There are many other scenarios possible, but the important
 * thing to recognize is that the factory software must ensure both that
 * 1) the key or hash fuse is correctly programmed to the device by a
 * separate readback mechanism, and that 2) it is correctly saved to your
 * external database. Failure to manage this properly will result in secure
 * non-functional units. The only way to recover these units will be
 * to physically remove the Tegra application processor and replace it.
 * Burning fuses is an irreversible process, which burns "1's" into the
 * selected bits. This means that while you can re-burn additional bits at any
 * future time, you cannot reset bits back to their '0' state.
 *
 * @note
 * For T11x, T14x, T12x, after burning fuses, the device @b must be rebooted
 * before reading burned fuses. Fuse values do not appear immediately after
 * they are burned. Reset (reboot) the device before attempting to confirm
 * new fuse values.
 *
 * <hr>
 * <a name="nvddk_if"></a><h2>NvDDK Fuse Interface</h2>
 *
 * This topic includes the following sections:
 * - Limitations
 * - Available Fuses in Boot Loader
 * - Fuse Sizes
 * - NvDdk Fuse Library Usage
 * - Fuse Programming Example Code
 *
 * <h3>Limitations</h3>
 *
 * This software has been tested on NVIDIA&reg; Tegra&reg;
 * code-name Ceres and Atlantis (T14x) platforms.
 *
 * @note This software is currently under development, and this early release
 * is intended to enable customers early access to some fuse burning capability
 * with the understanding that the interface is subject to change and is not
 * completed. Because of this, it is important to verify the desired fuses
 * are configured and the board is functional using the settings <b><em>before
 * burning large batches of chips</em></b>.
 *
 * Bytes are written to fuses in the order they are given; specific big or
 * little endian is not enforced. For user-defined fields, byte ordering may not
 * matter as long as the interface is consistent when reading/writing. However,
 * for multi-byte system fuses (like \a SecBootDeviceSelect) the byte order is
 * critical for the system.
 *
 * The \a SpareBits field is reserved for NVIDIA use; reads/writes to this
 * field are disallowed.
 *
 * Once the ODM production fuse is burned, the following operations are
 * disallowed:
 * - reading the SecureBootKey
 * - writing fuses
 *
 * <h3>Available Fuses in Boot Loader</h3>
 *
 * The following list shows the available fuse files:
 *
 * - device_key
 * - ignore_dev_sel_straps
 * - odm_production_mode
 * - odm_reserved
 * - sec_boot_dev_cfg
 * - jtag_disable
 * - sec_boot_dev_sel
 * - secure_boot_key
 * - sw_reserved
 * - pkc_disable
 * - vp8_enable
 * - odm_lock
 * - public_key_hash
 *
 * <h3>Fuse Sizes</h3>
 *
 * The following shows the fuse sizes, in bits:
 * - device_key = 32 bits
 * - ignore_dev_sel_straps = 1 bit
 * - jtag_disable = 1 bit
 * - odm_production_mode = 1 bit
 * - odm_reserved = 256 bits
 * - sec_boot_dev_cfg = 16 bits
 * - sec_boot_dev_sel = 3 bits
 * - secure_boot_key = 128 bits
 * - sw_reserved = 4 bits
 * - pkc_disable = 1 bit
 * - vp8_enable = 1 bit
 * - odm_lock = 4 bits
 * - public_key_hash = 256 bits
 *
 * <h3>NvDdk Library Usage</h3>
 *
 * -# NvDdkFuseGet ->Reads the fuse registers.
 * -# NvDdkFuseSet ->Schedules fuses to be programmed to the specified values.
 * -# NvDdkFuseProgram -> Programs all fuses based on cache data changed
 *     via the NvDdkFuseSet() API.The voltage requirements will also be handled
 *     by this API.
 * -# NvDdkFuseVerify -> Verifies all fuses scheduled via the \c NvDdkFuseSet()
 *     API.
 * -# NvDdkDisableFuseProgram ->Disables further fuse programming until
 *     the next system reset.
 *
 * <h3>Example Fuse Programming Code</h3>
 *
 * The following example shows one way to use Fuse Programming APIs.
 *
 * @code
 * //
 * // Fuse Programming Example
 * //
 * #include "nvddk_fuse.h"
 * #include "nvtest.h"
 * #include "nvrm_init.h"
 * #include "nvodm_query_discovery.h"
 * #include "nvodm_services.h"
 *
 * #define DISABLE_PROGRMING_TEST 0
 *
 * static NvError NvddkFuseProgramTest(NvTestApplicationHandle h )
 * {
 *     NvError err = NvSuccess;
 *     err = NvDdkFuseProgram();
 *     if(err != NvError_Success)
 *     {
 *         NvOsDebugPrintf("NvDdkFuseProgram failed \n");
 *         return err;
 *     }
 *     // Verify the fuses and return the result.
 *     err = NvDdkFuseVerify();
 *     if(err != NvError_Success)
 *         NvOsDebugPrintf("NvDdkFuseVerify failed \n");
 *     return err;
 * }
 *
 * static NvError PrepareFuseData(void)
 * {
 *     // Initialize argument sizes to zero to perform initial queries.
 *     NvU32 BootDevSel_Size = 0;
 *     NvU32 BootDevConfig_Size = 0;
 *     NvU32 ResevOdm_size = 0;
 *     NvU32 size = 0;
 *
 *     // Specify values to be programmed.
 *     NvU32 BootDevConfig_Data = 0x9; // Set device config value to 0x9.
 *     NvU32 BootDevSel_Data = 0x1;     // Set boot select to 0x1 for NAND.
 *     NvU32 ResevOdm_Data[8] =  {0x12345678, 0x01ABCDEF,
 *                                          0x12345678, 0x01ABCDEF,
 *                                          0x12345678, 0x12ABCDEF,
 *                                          0x12345678, 0x12ABCDEF};
 *
 *     NvU8 skpDevSelStrap_data = 1;
 *     NvError e;
 *
 *     // Query the sizes of the fuse values.
 *     e = NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelect,
 *                                    &BootDevSel_Data, &BootDevSel_Size);
 *     if (e != NvError_Success) return e;
 *
 *     e = NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceConfig,
 *                                    &BootDevConfig_Data, &BootDevConfig_Size);
 *     if (e != NvError_Success) return e;
 *
 * #ifdef DISABLE_PROGRMING_TEST
 *     NvDdkDisableFuseProgram();
 * #endif
 *
 *     e = NvDdkFuseGet(NvDdkFuseDataType_ReservedOdm,
 *                                    &ResevOdm_Data, &ResevOdm_size);
 *     if (e != NvError_Success) return e;
 *
 *     e = NvDdkFuseGet(NvDdkFuseDataType_SkipDevSelStraps,
 *                                    &skpDevSelStrap_data, &size);
 *     if (e != NvError_Success) return e;
 *
 *
 *     // Set the fuse values.
 *     e = NvDdkFuseSet(NvDdkFuseDataType_SecBootDeviceSelect,
 *                                    &BootDevSel_Data, &BootDevSel_Size);
 *     if (e != NvError_Success) return e;
 *
 *     e = NvDdkFuseSet(NvDdkFuseDataType_SecBootDeviceConfig,
 *                                    &BootDevConfig_Data, &BootDevConfig_Size);
 *     if (e != NvError_Success) return e;
 *
 *     e = NvDdkFuseSet(NvDdkFuseDataType_SkipDevSelStraps,
 *                                    &skpDevSelStrap_data, &size);
 *     if (e != NvError_Success) return e;
 *
 *
 *     e = NvDdkFuseSet(NvDdkFuseDataType_ReservedOdm,
 *                                    &ResevOdm_Data, &ResevOdm_size);
 *     if (e != NvError_Success) return e;
 *
 *     return e;
 * }
 *
 *
 * NvError NvTestMain(int argc, char *argv[])
 * {
 *     NvTestApplication h;
 *     NvError err;
 *
 *     NVTEST_INIT( &h );
 *     // Enable fuse clock and fuse voltage prior to programming fuses.
 *     err = PrepareFuseData();
 *     if( err != NvSuccess)
 *         return err;
 *
 *     NVTEST_RUN(&h, NvddkFuseProgramTest);
 *
 *     NVTEST_RESULT( &h );
 * }
 * @endcode
 *
 * @ingroup nvddk_modules
 * @{
 */

#if defined(__cplusplus)
extern "C"
{
#endif


#include "nvcommon.h"
#include "nverror.h"

/**
 * Defines the maximum number of fuses that can be burned simultaneously.
 */
#define MAX_NUM_FUSE 8


/* ----------- Program/Verify API's -------------- */

/**
 * Defines types of fuse data to set or query.
 */
typedef enum
{
    /// Specifies a default (unset) value.
    NvDdkFuseDataType_None = 0,
    /// Specifies a device key (DK).
    NvDdkFuseDataType_DeviceKey,

    /**
     * Specifies a JTAG disable flag:
     * - NV_TRUE specifies to permanently disable JTAG-based debugging.
     * - NV_FALSE indicates that JTAG-based debugging is not permanently
     *   disabled.
     */
    NvDdkFuseDataType_JtagDisable,
    NvDdkFuseDataType_ArmDebugDisable =
    NvDdkFuseDataType_JtagDisable,

    /**
     * Specifies a key programmed flag (read-only):
     * - NV_TRUE indicates that either the SBK or the DK value is nonzero.
     * - NV_FALSE indicates that both the SBK and DK values are zero (0).
     *
     * @note Once the ODM production fuse is set to NV_TRUE, applications
     * can no longer read back the SBK or DK value; however, by using
     * this \c KeyProgrammed query it is still possible to determine
     * whether or not the SBK or DK has been programmed to a nonzero value.
     */
    NvDdkFuseDataType_KeyProgrammed,

    /**
     * Specifies an ODM production flag:
     * - NV_TRUE specifies the chip is in ODM production mode.
     * - NV_FALSE indicates that the chip is not in ODM production mode.
     */
    NvDdkFuseDataType_OdmProduction,

    /**
     * Specifies a secondary boot device configuration.
     * This value is chip-dependent.
     * For Tegra APX 2600, use the NVBOOT_FUSE_*_CONFIG_*
     * defines from:
     * <pre>
     *  /src/drivers/hwinc/ap15/nvboot_fuse.h
     * </pre>
     */
    NvDdkFuseDataType_SecBootDeviceConfig,

    /**
     * Specifies a secondary boot device selection.
     * This value is chip-independent and is described in
     * \c nvddk_bootdevices.h. The chip-dependent version of this data is
     * ::NvDdkFuseDataType_SecBootDeviceSelectRaw.
     * For Tegra APX 2600, the values for \c SecBootDeviceSelect and
     * \c SecBootDeviceSelectRaw are identical.
     */
    NvDdkFuseDataType_SecBootDeviceSelect,

    /** Specifies a secure boot key (SBK). */
    NvDdkFuseDataType_SecureBootKey,

    /**
     * Specifies a stock-keeping unit (read-only).
     * This value is chip-dependent.
     * See chip-specific documentation for legal values.
     */
    NvDdkFuseDataType_Sku,

    /**
     * Specifies spare fuse bits (read-only).
     * Reserved for future use by NVIDIA.
     */
    NvDdkFuseDataType_SpareBits,

    /**
     * Specifies software reserved fuse bits (read-only).
     * Reserved for future use by NVIDIA.
     */
    NvDdkFuseDataType_SwReserved,

    /**
     * Specifies skip device select straps (applies to Tegra 200 series only):
     * - NV_TRUE specifies to ignore the device selection straps setting
     *   and that the boot device is specified via the
     *   \c SecBootDeviceSelect and \c SecBootDeviceConfig fuse settings.
     * - NV_FALSE indicates that the boot device is specified via the device
     *   selection straps setting.
     */
    NvDdkFuseDataType_SkipDevSelStraps,

    /**
     * Specifies a secondary boot device selection.
     * This value is chip-dependent.
     * The chip-independent version of this data is
     * ::NvDdkFuseDataType_SecBootDeviceSelect.
     * For Tegra APX 2600, use the \c NvBootFuseBootDevice enum
     * values found at:
     * <pre>
     *  /src/drivers/hwinc/ap15/nvboot_fuse.h
     * </pre>
     */
    NvDdkFuseDataType_SecBootDeviceSelectRaw,

    /**
     * Specifies raw field for reserved the ODM.
     * This value is ODM-specific. Reserved for customers.
     */
    NvDdkFuseDataType_ReservedOdm,

    /**
     * Specifies the pacakge info type for the chip.
     * See chip-specific documentation for legal values.
     */
    NvDdkFuseDataType_PackageInfo,

    /**
     * Specifies the unique ID for the chip.
     */
    NvDdkFuseDataType_Uid,

    /**
     * Specifies the operating mode with modes
     * from the ::NvDdkFuseOperatingMode enumeration.
     */
    NvDdkFuseDataType_OpMode,

    /**
     * Specifies secondary boot device controller instance.
     */
    NvDdkFuseDataType_SecBootDevInst,

    /**
      * Gets the hash of the input public key.
      */
    NvDdkFuseDataType_PublicKeyHash,

    /**
      * Checks if watchdog timer is enabled or not.
      */
    NvDdkFuseDataType_WatchdogEnabled,

    /**
      * Checks if Usb charger detection is enabled or not.
      */
    NvDdkFuseDataType_UsbChargerDetectionEnabled,

    /**
     * Reports whether chip is in PKC secure boot mode or
     * symmetric key based secure boot mode.
     * If burned, \a pkc_disable disables the use of PKC secure boot, and
     * the BR reverts back to symmetric key based secure boot infrastructure.
     * @note The \a pkc_disable fuse has no effect if the \a security_mode fuse
     * (i.e., ODM production mode fuse) has not been burned. In this case,
     * the \a pkc_disable fuse is always 0.
     */
    NvDdkFuseDataType_PkcDisable,

    /**
      * Checks whether or not VP8 video encode/decode is enabled.
      */
    NvDdkFuseDataType_Vp8Enable,

    /**
      * Checks whether or not reserved_odm[3:0] fields are write-protected.t
      */
    NvDdkFuseDataType_OdmLock,
    /**
      * Gets the CPU speedo value.
      */
    NvDdkFuseDataType_CpuSpeedo0,
    /**
      * Gets the CPU speedo value.
      */
    NvDdkFuseDataType_CpuSpeedo1,
    /**
      * Gets the CPU speedo value.
      */
    NvDdkFuseDataType_CpuSpeedo2,
    /**
      * Gets the CPU IDDQ value.
      */
    NvDdkFuseDataType_CpuIddq,
    /**
      * Gets the SoC speedo value.
      */
    NvDdkFuseDataType_SocSpeedo0,
    /**
      * Gets the SoC speedo value.
      */
    NvDdkFuseDataType_SocSpeedo1,
    /**
      * Gets the SoC speedo value.
      */
    NvDdkFuseDataType_SocSpeedo2,
    /**
      * Gets the SoC IDDQ value.
      */
    NvDdkFuseDataType_SocIddq,

    /**
    * Specifies whether to do fuse burning or fuse bypassing.
    */
    NvDdkFuseDataType_SkipFuseBurn,
    /**
       * Gets the T_ID stored in the device.
       */
    NvDdkFuseDataType_T_ID,

    /**
    * Gets the fuse age value.
    */
    NvDdkFuseDataType_GetAge,

    /** The following must be last. */
    NvDdkFuseDataType_Num,

    /** Stores USB PID and other ODM info that is permanently burnt */
    NvDdkFuseDataType_OdmInfo,

    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkFuseDataType_Force32 = 0x7FFFFFFF
} NvDdkFuseDataType;

/**
 * specifies different operating modes.
 */
typedef enum
{
    NvDdkFuseOperatingMode_None = 0,
    /**
     * Preproduction mode
     */
    NvDdkFuseOperatingMode_Preproduction,
    /**
     * Failrue Analysis mode
     */
    NvDdkFuseOperatingMode_FailureAnalysis,
    /**
     * Nvproduction mode
     */
    NvDdkFuseOperatingMode_NvProduction,

    /**
     * ODM Production mode.
     */
    NvDdkFuseOperatingMode_OdmProductionNonSecure,
    NvDdkFuseOperatingMode_OdmProductionOpen =
    NvDdkFuseOperatingMode_OdmProductionNonSecure,

    /**
     * ODM Production secure mode.
     */
    NvDdkFuseOperatingMode_OdmProductionSecureSBK,
    NvDdkFuseOperatingMode_OdmProductionSecure =
    NvDdkFuseOperatingMode_OdmProductionSecureSBK,

    /**
     * Specifies production mode; boot loader is validated using the public key.
    */
    NvDdkFuseOperatingMode_OdmProductionSecurePKC,

    /**
     * Undefined.
    */
    NvDdkFuseOperatingMode_Undefined,

    /**
     * Ignore -- Forces compilers to make 32-bit enums.
    */
    NvDdkFuseOperatingMode_Force32 = 0x7FFFFFFF

} NvDdkFuseOperatingMode;


/**
 * Gets a value from the fuse registers.
 *
 * @pre Do not call this function while the programming power is applied!
 *
 * After programming fuses and removing the programming power,
 * NvDdkFuseSense() must be called to read the new values.
 *
 * By passing a size of 0, the caller is requesting to be told the
 * expected size.
 *
 * Treatment of fuse data depends upon its size:
 * - if \a *pSize == 1, treat \a *pData as an NvU8
 * - if \a *pSize == 2, treat \a *pData as an NvU16
 * - if \a *pSize == 4, treat \a *pData as an NvU32
 * - else, treat \a *pData as an array of NvU8 values (i.e., NvU8[]).
 *
 * For warnings and other information on fuses, see
 * @link nvddk_fuse_group Fuse Programming APIs @endlink.
 */
NvError NvDdkFuseGet(NvDdkFuseDataType Type, void *pData, NvU32 *pSize);

/**
 * Schedules fuses to be programmed to the specified values when the next
 * NvDdkFuseProgram() operation is performed.
 *
 * @note Attempting to program the ODM production fuse at the same
 * time as the SBK or DK causes an error, because it is not
 * possible to verify that the SBK or DK were programmed correctly.
 * After triggering this error, all further attempts to set fuse
 * values will fail, as will \c NvDdkFuseProgram, until NvDdkFuseClear()
 * has been called.
 *
 * By passing a size of 0, the caller is requesting to be told the
 * expected size.
 *
 * Treatment of fuse data depends upon its size:
 * - if \a *pSize == 1, treat \a *pData as an NvU8
 * - if \a *pSize == 2, treat \a *pData as an NvU16
 * - if \a *pSize == 4, treat \a *pData as an NvU32
 * - else, treat \a *pData as an array of NvU8 values (i.e., NvU8[]).
 *
 * For warnings and other information on fuses, see
 * @link nvddk_fuse_group Fuse Programming APIs @endlink.
 *
 * @retval NvError_BadValue If other than "reserved ODM fuse" is set in ODM
 *             production  mode.
 * @retval NvError_AccessDenied If programming to fuse registers is disabled.
 */
NvError NvDdkFuseSet(NvDdkFuseDataType Type, void *pData, NvU32 *pSize);

/**
 * Reads the current fuse data into the fuse registers.
 *
 * \c NvDdkFuseSense must be called at least once, either:
 * - After programming fuses and removing the programming power,
 * - Prior to calling NvDdkFuseVerify(), or
 * - Prior to calling NvDdkFuseGet().
 *
 * For warnings and other information on fuses, see
 * @link nvddk_fuse_group Fuse Programming APIs @endlink.
 */
void NvDdkFuseSense(void);

/**
 * Programs all fuses based on cache data changed via the NvDdkFuseSet() API.
 *
 * @pre Prior to invoking this routine, the caller is responsible for supplying
 * valid fuse programming voltage.
 *
 * For warnings and other information on fuses, see
 * @link nvddk_fuse_group Fuse Programming APIs @endlink.
 *
 * @retval NvError_AccessDenied If programming to fuse registers is disabled.
 * @return An error if an invalid combination of fuse values was provided.
 */
NvError NvDdkFuseProgram(void);

/**
 * Verify all fuses scheduled via the NvDdkFuseSet() API.
 *
 * @pre Prior to invoking this routine, the caller is responsible for ensuring
 * that fuse programming voltage is removed and subsequently calling
 * NvDdkFuseSense().
 */
NvError NvDdkFuseVerify(void);

/**
 * Clears the cache of fuse data, once NvDdkFuseProgram() and NvDdkFuseVerify()
 * API are called to clear all buffers.
 */
void NvDdkFuseClear(void);

/**
 * Disables further fuse programming until the next system reset.
 * @note For T114 and T148, users must use reset/coldboot/LP0 exit for immediate
 * reflectance of burnt values via \c NvDdkFuseRead (called after NvDdkFuseProgram)
 */
void NvDdkDisableFuseProgram(void);

/**
 * Clears global fuse data arrays.
 */
void NvDdkFuseClearArrays(void);

/**
 * Defines the fuse by enum and the value to be written.
 */
typedef struct
{
    NvDdkFuseDataType fuse_type;
    NvU32 offset;
    NvU32 fuse_value[10];
}NvFuseData;

/**
 * Defines the structure for fuse write.
 */
typedef struct
{
    NvFuseData fuses[MAX_NUM_FUSE];
}NvFuseWrite;

/**
 * Defines the information required for bypassing fuses.
 */
typedef struct NvFuseBypassInfoRec
{
    NvU32  sku_type;
    NvU32  cpu_speedo_min[8];
    NvU32  cpu_speedo_max[8];
    NvU32  cpu_iddq_min  [8];
    NvU32  cpu_iddq_max  [8];
    NvU32  soc_speedo_min[8];
    NvU32  soc_speedo_max[8];
    NvU32  soc_iddq_min  [8];
    NvU32  soc_iddq_max  [8];
    NvBool force_bypass;
    NvBool force_download;
    NvU32  num_fuses;
    NvFuseData fuses[MAX_NUM_FUSE];
}NvFuseBypassInfo;

/**
 * Checks if the chip meets the SPEEDO and IDDQ limits and bypasses the fuses.
 *
 * @param BypassInfo A pointer to a structure containing information of SPEEDO and
 *        IDDQ limits and fuses that are to be bypassed.
 *
 * @retval NvSuccess If chip meets the SPEEDO and IDDQ limits and fuse bypass
 *         is successful.
 * @retval NvError_InvalidConfigVar If chip does not meet SPEEDO and IDDQ limits.
 */
NvError NvDdkFuseBypassSku(NvFuseBypassInfo *BypassInfo);

/**
 * Reads the fuse register.
 *
 * @param RegAddr Address of the fuse register to be written.
 * @param RegVal A pointer to the value to be updated with fuse register value.
 *
 * @retval NvSuccess If regester read is successful.
 * @retval NvError_NotSupported If register programming is disabled.
 * @retval NvError_BadParameter If \a Regval pointer is null.
 */
NvError NvDdkFuseBypassGet(NvU32 RegAddr, NvU32 *RegVal);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVDDK_FUSE_H

