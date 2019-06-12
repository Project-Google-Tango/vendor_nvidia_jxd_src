/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_module_H
#define INCLUDED_nvrm_module_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_init.h"

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           Resource Manager %Module Control APIs</b>
 *
 * @b Description: Declares module information and control APIs.
 *
 */

/**
 * @defgroup nvrm_module RM Module Management Services
 *
 * This file declares Resource Management (RM) module information and control
 * APIs.
 *
 * @ingroup nvddk_rm
 *
 * @{
 */

#include "nvrm_drf.h"

/**
 * Defines SOC hardware controller class identifiers.
 */

typedef enum
{

    /// Specifies an invalid module ID.
        NvRmModuleID_Invalid = 0,

    /// Specifies the application processor.
        NvRmModuleID_Cpu,

    /// Specifies the audio/video processor
        NvRmModuleID_Avp,

    /// Specifies the vector co-processor.
        NvRmModuleID_Vcp,

    /// Specifies the display controller.
        NvRmModuleID_Display,

    /// Specifies the IDE controller.
        NvRmModuleID_Ide,

    /// Specifies the graphics host.
        NvRmModuleID_GraphicsHost,

    /// Specifies 2D graphics controller.
        NvRmModuleID_2D,

    /// Specifies 3D graphics controller.
        NvRmModuleID_3D,

    /// Specifies VG graphics controller.
        NvRmModuleID_VG,

    /// Specifies NVIDIA encoder pre-processor (EPP).
        NvRmModuleID_Epp,

    /// Specifies NVIDIA image signal processor (ISP).
        NvRmModuleID_Isp,

    /// Specifies NVIDIA video input.
        NvRmModuleID_Vi,

    /// Specifies the USB2 OTG controller.
        NvRmModuleID_Usb2Otg,

    /// Specifies the I2S controller.
        NvRmModuleID_I2s,

    /// Specifies the Pulse Width Modulator controller.
        NvRmModuleID_Pwm,

    /// Specifies the Three Wire controller.
        NvRmModuleID_Twc,

    /// Specifies the HSMMC controller.
        NvRmModuleID_Hsmmc,

    /// Specifies the SDIO controller.
        NvRmModuleID_Sdio,

    /// Specifies the NAND controller.
        NvRmModuleID_Nand,

    /// Specifies the I2C controller.
        NvRmModuleID_I2c,

    /// Specifies the Sony Phillips Digital Interface Format controller.
        NvRmModuleID_Spdif,

    /// Specifies the %UART controller.
        NvRmModuleID_Uart,

    /// Specifies the timer controller.
        NvRmModuleID_Timer,

    /// Specifies the timer controller microsecond counter.
        NvRmModuleID_TimerUs,

    /// Specifies the realtime clock controller.
        NvRmModuleID_Rtc,

    /// Specifies the Audio Codec 97 controller.
        NvRmModuleID_Ac97,

    /// Specifies the Audio Bit Stream Engine.
        NvRmModuleID_BseA,

    /// Specifies video decoder.
        NvRmModuleID_Vde,

    /// Specifies video encoder (Motion Picture Encoder).
        NvRmModuleID_Mpe,

    /// Specifies the Camera Serial Interface.
        NvRmModuleID_Csi,

    /// Specifies High-Bandwidth Digital Content Protection interface.
        NvRmModuleID_Hdcp,

    /// Specifies High definition Multimedia Interface.
        NvRmModuleID_Hdmi,

    /// Specifies MIPI baseband controller.
        NvRmModuleID_Mipi,

    /// Specifies TV out controller.
        NvRmModuleID_Tvo,

    /// Specifies serial display.
        NvRmModuleID_Dsi,

    /// Specifies Dynamic Voltage Controller.
        NvRmModuleID_Dvc,

    /// Specifies the eXtended I/O controller.
        NvRmModuleID_Xio,

    /// Specifies the SPI controller.
        NvRmModuleID_Spi,

    /// Specifies SLink controller.
        NvRmModuleID_Slink,

    /// Specifies FUSE controller.
        NvRmModuleID_Fuse,

    /// Specifies KFUSE controller.
        NvRmModuleID_KFuse,

    /// Specifies EthernetMIO controller.
        NvRmModuleID_Mio,

    /// Specifies keyboard controller.
        NvRmModuleID_Kbc,

    /// Specifies PMIF controller.
        NvRmModuleID_Pmif,

    /// Specifies Unified Command Queue.
        NvRmModuleID_Ucq,

    /// Specifies event controller.
        NvRmModuleID_EventCtrl,

    /// Specifies flow controller.
        NvRmModuleID_FlowCtrl,

    /// Specifies resource semaphore.
        NvRmModuleID_ResourceSema,

    /// Specifies arbitration semaphore.
        NvRmModuleID_ArbitrationSema,

    /// Specifies arbitration priority.
        NvRmModuleID_ArbPriority,

    /// Specifies cache memory controller.
        NvRmModuleID_CacheMemCtrl,

    /// Specifies very fast infra-red controller.
        NvRmModuleID_Vfir,

    /// Specifies exception vector.
        NvRmModuleID_ExceptionVector,

    /// Specifies boot strap controller.
        NvRmModuleID_BootStrap,

    /// Specifies system statistics monitor controllerl
        NvRmModuleID_SysStatMonitor,

    /// Specifies system.
        NvRmModuleID_Cdev,

    /// Specifies a miscellaneous module ID that contains registers for
    /// pinmux/DAP control, etc.
        NvRmModuleID_Misc,

    /// Specifies a PCIE Device attached to AP20.
        NvRmModuleID_PcieDevice,

    /// Specifies one-wire interface controller.
        NvRmModuleID_OneWire,

    /// Specifies sync NOR controller.
        NvRmModuleID_SyncNor,

    /// Specifies NOR memory aperture.
        NvRmModuleID_Nor,

    /// Specifies AVP UCQ module.
        NvRmModuleID_AvpUcq,

    /// Specifies clock and reset controller.
        NvRmPrivModuleID_ClockAndReset,

    /// Specifies interrupt controller.
        NvRmPrivModuleID_Interrupt,

    /// Specifies interrupt controller arbitration semaphore grant registers.
        NvRmPrivModuleID_InterruptArbGnt,

    /// Specifies interrupt controller DMA Tx/Rx DRQ registers.
        NvRmPrivModuleID_InterruptDrq,

    /// Specifies interrupt controller special SW interrupt.
        NvRmPrivModuleID_InterruptSw,

    /// Specifies interrupt controller special CPU interrupt.
        NvRmPrivModuleID_InterruptCpu,

    /// Specifies APB DMA controller.
        NvRmPrivModuleID_ApbDma,

    /// Specifies APB DMA channel.
        NvRmPrivModuleID_ApbDmaChannel,

    /// Specifies GPIO controller.
        NvRmPrivModuleID_Gpio,

    /// Specifies pin-mux controller.
        NvRmPrivModuleID_PinMux,

    /// Specifies memory configuation.
        NvRmPrivModuleID_Mselect,

    /// Specifies memory controller (internal memory and memory arbitration).
        NvRmPrivModuleID_MemoryController,

    /// Specifies external memory (DDR RAM, etc.).
        NvRmPrivModuleID_ExternalMemoryController,

    /// Specifies processor ID.
        NvRmPrivModuleID_ProcId,

    /// Specifies entire system (used for system reset).
        NvRmPrivModuleID_System,

    /* Specifies CC device ID (needed to
     * set the mem_init_done bit so that memory works).
     */
        NvRmPrivModuleID_CC,

    /// Specifies AHB arbitration control.
        NvRmPrivModuleID_Ahb_Arb_Ctrl,

    /// Specifies AHB gizmo control.
        NvRmPrivModuleID_Ahb_Gizmo_Ctrl,

    /// Specifies external memory.
        NvRmPrivModuleID_ExternalMemory,

    /// Specifies internal memory.
        NvRmPrivModuleID_InternalMemory,

    /// Specifies TCRAM.
        NvRmPrivModuleID_Tcram,

    /// Specifies IRAM.
        NvRmPrivModuleID_Iram,

    /// Specifies GART.
        NvRmPrivModuleID_Gart,

    /// MIO/EXIO
        NvRmPrivModuleID_Mio_Exio,

    /* Specifies external PMU. */
        NvRmPrivModuleID_PmuExt,

    /** Specifies one module ID for all peripherals, which includes cache
     *  controller, SCU, and interrupt controller. */
        NvRmPrivModuleID_ArmPerif,
    NvRmPrivModuleID_ArmInterruptctrl,

    /**
     * Specifies the PCIE Root Port internally is made up of 3 major blocks.
     * These 3 blocks have separate reset and clock domains. So, the driver
     * treats these:
     * - AFI is the wrapper on the top of the PCI core.
     * - PCIe refers to the core PCIe state machine module.
     * - PcieXclk refers to the transmit/receive logic, which runs at different
     *       clock and has different reset.
     * */
        NvRmPrivModuleID_Afi,
    NvRmPrivModuleID_Pcie,
    NvRmPrivModuleID_PcieXclk,

    /** Specifies PL310. */
        NvRmPrivModuleID_Pl310,

    /**
     * Specifies AHB re-map aperture seen from AVP. Use this aperture for AVP to
     * have uncached access to SDRAM.
     */
        NvRmPrivModuleID_AhbRemap,

    /** Specifies HD-Audio blocks.
     */
        NvRmModuleID_HDA,
    NvRmModuleID_Apbif,
    NvRmModuleID_Audio,
    NvRmModuleID_DAM,

    /* SATA */
        NvRmModuleID_Sata,

    /* DTV */
        NvRmModuleID_DTV,

    /* Tsensor */
        NvRmModuleID_Tsensor,

    /* ACTMON */
        NvRmModuleID_Actmon,

    /* Atomics */
        NvRmModuleID_Atomics,

    /* SE */
        NvRmModuleID_Se,
    NvRmModuleID_ExtPeriphClk,


    /* VIC */
        NvRmModuleID_Vic,

    /* GPU */
        NvRmModuleID_Gpu,


    /* Configurable external memory / memory-mapped I/O apertures */
        NvRmPrivModuleID_ExternalMemory_MMIO,

    /// Specifies video encoder (MSENC)
        NvRmModuleID_MSENC,

    /// Specifies security module (TSEC)
        NvRmModuleID_TSEC,

    /* Vp8 */
        NvRmModuleID_Vp8,

    /* Specifies XUSB host module */
        NvRmModuleID_XUSB_HOST,

    /* Specifies Mipibif module */
        NvRmModuleID_Mipibif,

    /* Specifies Virtual GPIO module */
        NvRmModuleID_Vgpio,

    /// Specifies CL DVFS
        NvRmModuleID_ClDvfs,
    NvRmModuleID_Num,
    NvRmModuleID_Force32 = 0x7FFFFFFF
} NvRmModuleID;

typedef struct NvRmModuleIdRec *NvRmModuleIdHandle;

/**
 * Defines platform of operation.
 */

typedef enum
{

    /* Silicon */
        NvRmModulePlatform_Silicon = 0,

    /* Simulation */
        NvRmModulePlatform_Simulation,

    /* Emulation */
        NvRmModulePlatform_Emulation,

    /* Quickturn */
        NvRmModulePlatform_Quickturn,
    NvRmModulePlatform_Num,
    NvRmModulePlatform_Force32 = 0x7FFFFFFF
} NvRmModulePlatform;

/* FIXME
 * Hack to make the existing drivers work.
 * NvRmPriv* should be renamed to NvRm*
 */
#define NvRmPrivModuleID_Num  NvRmModuleID_Num

/**
 * Module bitfields that are compatible with the NV_DRF macros.
 *
 * Multiple module instances are handled by packing the instance number into
 * the high bits of the module ID. This avoids ponderous APIs with both
 * module IDs and instance numbers.
 *
 */
#define NVRM_MODULE_0                   (0x0)
#define NVRM_MODULE_0_ID_RANGE          15:0
#define NVRM_MODULE_0_INSTANCE_RANGE    23:16
#define NVRM_MODULE_0_BAR_RANGE         27:24

/**
 * Creates a module ID with a given instance.
 */
#define NVRM_MODULE_ID( id, instance ) \
    (NvRmModuleID)( \
          NV_DRF_NUM( NVRM, MODULE, ID, (id) ) \
        | NV_DRF_NUM( NVRM, MODULE, INSTANCE, (instance) ) )

/**
 * Gets the actual module ID.
 */
#define NVRM_MODULE_ID_MODULE( id ) \
    NV_DRF_VAL( NVRM, MODULE, ID, (id) )

/**
 * Gets the instance number of the module ID.
 */
#define NVRM_MODULE_ID_INSTANCE( id ) \
    NV_DRF_VAL( NVRM, MODULE, INSTANCE, (id) )

/**
 * Gets the bar number for the module.
 */
#define NVRM_MODULE_ID_BAR( id ) \
    NV_DRF_VAL( NVRM, MODULE, BAR, (id) )

/**
 * Holds the module information.
 */

typedef struct NvRmModuleInfoRec
{
    NvU32 Instance;
    NvU32 Bar;
    NvRmPhysAddr BaseAddress;
    NvU32 Length;
} NvRmModuleInfo;

/**
 * Returns a list of available module instances and their information.
 *
 * @param hDevice The RM device handle.
 * @param module The module for which to get the number of instances.
 * @param pNum Unsigned integer indicating the number of module information
 *      structures in the array \a pModuleInfo.
 * @param pModuleInfo A pointer to an array of module information structures,
 *      where the size of array is determined by the value in \a pNum.
 *
 * @retval NvSuccess If successful, or the appropriate error.
 */

 NvError NvRmModuleGetModuleInfo(
    NvRmDeviceHandle hDevice,
    NvRmModuleID module,
    NvU32 * pNum,
    NvRmModuleInfo * pModuleInfo );

/**
 * Returns a physical address associated with a hardware module.
 * (To be depcreated and replaced by NvRmModuleGetModuleInfo.)
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param Module The module for which to get addresses.
 * @param pBaseAddress A pointer to the beginning of the
 *     hardware register bank.
 * @param pSize A pointer to the length of the aperture in bytes.
 */

 void NvRmModuleGetBaseAddress(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID Module,
    NvRmPhysAddr * pBaseAddress,
    NvU32 * pSize );

/**
 * Find a module given its physical register address
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param Address Physical base address of the module's registers
 * @param phModuleId Handle to output parameter to hold the Id of the module
 *  which also includes the module instance.
 *
 * @retval ::NvError_Success The module id was successfully identified and
 * \a phModuleId points to the combined module id and instance.
 * points to the actual size of the ID. .
 * @retval ::NvError_ModuleNotPresent No module exists at the specified
 *  physical base address.
 * @retval ::NvError_BadParameter
 */

 NvError NvRmFindModule(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 Address,
    NvRmModuleIdHandle * phModuleId );

/**
 * Returns the number of instances of a particular hardware module.
 * (To be depcreated and replaced by NvRmModuleGetModuleInfo.)
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param Module The module for which to get the number of instances.
 *
 * @returns Number of instances.
 */

 NvU32 NvRmModuleGetNumInstances(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID Module );

/**
 * Resets the module controller hardware.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param Module The module to reset.
 */

 void NvRmModuleReset(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID Module );

/**
 *  Resets the controller with an option to hold the controller in the reset.
 *
 *  @param hRmDeviceHandle RM device handle.
 *  @param Module The module to be reset
 *  @param bHold If NV_TRUE hold the module in reset; if NV_TRUE pulse the
 *  reset. So, to keep the module in reset and do something:
 * <pre>
 *  NvRmModuleResetWithHold(hRm, ModId, NV_TRUE)
 * </pre>
 *  To update some registers:
 * <pre>
 *  NvRmModuleResetWithHold(hRm, ModId, NV_FALSE)
 * </pre>
 */

 void NvRmModuleResetWithHold(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID Module,
    NvBool bHold );

/**
 * Module Capability API tracking API changes.
 *
 * 0 : original API
 * 1 : API changes:
 *         NvRmModulePlatform was added to NvRmModuleCapability.
 */
#define NV_RM_CAPS_API_MODULE_PLATFORM (1)

/**
 * Holds DDK capability encapsualtion. See \c NvRmModuleGetCapabilities().
 */

typedef struct NvRmModuleCapabilityRec
{
    NvU8 MajorVersion;
    NvU8 MinorVersion;
    NvU8 EcoLevel;
    NvRmModulePlatform Platform;
    void* Capability;
} NvRmModuleCapability;

/**
 * Returns a pointer to a class-specific capabilities structure.
 *
 * Each DDK will supply a list of ::NvRmCapability structures sorted by module
 * Minor and Eco levels (assuming that no DDK supports two Major versions
 * simulatenously). The last cap in the list that matches the hardware's
 * version and eco level will be returned. If the current hardware's eco
 * level is higher than the given module capability list, the last module
 * capability with the highest eco level (the last in the list) will be
 * returned.
 *
 * @par Example usage:
 *
 * @code
 *
 * typedef struct FakeDdkCapRec
 * {
 *     NvU32 FeatureBits;
 * } FakeDdkCap;
 *
 * FakeDdkCap cap1;
 * FakeDdkCap cap2;
 * FakeDdkCap *cap;
 * NvRmModuleCapability caps[] =
 *       { { 1, 0, 0, &fcap1 },
 *         { 1, 1, 0, &fcap2 },
 *       };
 * cap1.bits = ...;
 * cap2.bits = ...;
 * err = NvRmModuleGetCapabilities( hDevice, NvRmModuleID_FakeDDK, caps, 2,
 *     (void *)&cap );
 * ...
 * if( cap->FeatureBits & FAKEDKK_SOME_FEATURE )
 * {
 *     ...
 * }
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param Module The target module.
 * @param pCaps A pointer to the capability list.
 * @param NumCaps The number of capabilities in the list.
 * @param [out] Capability The capability that maches the current hardware.
 */

 NvError NvRmModuleGetCapabilities(
    NvRmDeviceHandle hDeviceHandle,
    NvRmModuleID Module,
    NvRmModuleCapability * pCaps,
    NvU32 NumCaps,
    void* * Capability );

/**
 * @brief Queries for the device unique ID.
 *
 * @pre Not callable from early boot.
 *
 * @param hDevHandle The RM device handle
 * @param IdSize An input, a pointer to a variable containing the size of
 * the caller-allocated memory to hold the unique ID pointed to by \a pId.
 * Upon successful return, this value is updated to reflect the actual
 * size of the unique ID returned in \a pId.
 * @param pId A pointer to an area of caller-allocated memory to hold the
 * unique ID.
 *
 * @retval ::NvError_Success \a pId points to the unique ID and \a pIdSize
 * points to the actual size of the ID.
 * @retval ::NvError_BadParameter
 * @retval ::NvError_NotSupported
 * @retval ::NvError_InsufficientMemory
 */

 NvError NvRmQueryChipUniqueId(
    NvRmDeviceHandle hDevHandle,
    NvU32 IdSize,
    void* pId );

/**
 * @brief Returns random bytes using hardware sources of entropy.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param NumBytes Number of random bytes to return in \a pBytes.
 * @param pBytes Array where the random bytes should be stored.
 *
 * @retval ::NvError_Success
 * @retval ::NvError_BadParameter
 * @retval ::NvError_NotSupported If no hardware entropy source is available
 */

 NvError NvRmGetRandomBytes(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 NumBytes,
    void* pBytes );

/**
 * @brief Returns whether system is a valid SecureOS state.
 *
 * @param hRmDeviceHandle The RM device handle.
 *
 * @retval ::NvError_Success        running in a valid configuration
 * @retval ::NvError_NotSupported   running in an invalid configuraton
 * @retval ::NvError_AccessDenied   unable to check configuration
 */

 NvError NvRmCheckValidSecureState(
    NvRmDeviceHandle hRmDeviceHandle );

/**
 * @brief Returns whether system is in a valid NVSI state.
 *
 * @param hRmDeviceHandle The RM device handle.
 *
 * @retval ::NvError_Success        running in a valid configuration
 * @retval ::NvError_NotSupported   running in an invalid configuraton
 */

 NvError NvRmCheckValidNVSIState(
    NvRmDeviceHandle hRmDeviceHandle );


/**
 * Registers read from hardware.
 *
 * The aperture comes from the RM's private module ID enumeration,
 * which is a superset of the public enumeration from nvrm_module.h.
 *
 * @note Resource Manager (RM) doesn't gaurantee access to all the modules as it
 * only maps a few modules. This is not meant to be a primary mechanism to
 * access the module registers. Clients should map their register address and
 * access the registers.
 *
 * @param rm The RM istance.
 * @param aperture The register aperture.
 * @param offset The offset inside the aperture.
 */
#define NV_REGR(rm, aperture, offset) \
    NvRegr((rm),(aperture),(offset))

/**
 * Register write to hardware.
 *
 * The aperture comes from the RM's private module ID enumeration,
 * which is a superset of the public enumeration from nvrm_module.h.
 *
 * @note Resource Manager (RM) doesn't gaurantee access to all the modules as it
 * only maps a few modules. This is not meant to be a primary mechanism to
 * access the module registers. Clients should map their register address and
 * access the registers.
 *
 * @param rm The resource manager istance.
 * @param aperture The register aperture.
 * @param offset The offset inside the aperture.
 * @param data The data to write.
 */
#define NV_REGW(rm, aperture, offset, data) \
    NvRegw((rm),(aperture),(offset),(data))


 NvU32 NvRegr(
    NvRmDeviceHandle hDeviceHandle,
    NvRmModuleID aperture,
    NvU32 offset );

 void NvRegw(
    NvRmDeviceHandle hDeviceHandle,
    NvRmModuleID aperture,
    NvU32 offset,
    NvU32 data );


/**
 * Reads multiple registers from hardware.
 *
 * @note Resource Manager (RM) doesn't gaurantee access to all the modules as it
 * only maps a few modules. This is not meant to be a primary mechanism to
 * access the module registers. Clients should map their register address
 * and access the registers.
 *
 * @param rm The resource manager istance.
 * @param aperture The register aperture.
 * @param num The number of registers.
 * @param offsets The register offsets.
 * @param values The register values.
 */
#define NV_REGR_MULT(rm, aperture, num, offsets, values) \
    NvRegrm((rm),(aperture),(num),(offsets),(values))

/**
 * Write multiple registers from hardware.
 *
 * @note Resource Manager (RM) doesn't gaurantee access to all the modules as it
 * only maps a few modules. This is not meant to be a primary mechanism to
 * access the module registers. Clients should map their register address and
 * access the registers.
 *
 * @param rm The resource manager istance.
 * @param aperture The register aperture.
 * @param num The number of registers.
 * @param offsets The register offsets.
 * @param values The register values.
 */
#define NV_REGW_MULT(rm, aperture, num, offsets, values) \
    NvRegwm((rm),(aperture),(num),(offsets),(values))

/**
 * Writes a block of registers to hardware.
 *
 * @note Resource Manager (RM) doesn't gaurantee access to all the modules as it
 * only maps a few modules. This is not meant to be a primary mechanism to
 * access the module registers. Clients should map their register address and
 * access the registers.
 *
 * @param rm The resource manager istance.
 * @param aperture The register aperture.
 * @param num The number of registers.
 * @param offset The beginning register offset.
 * @param values The register values.
 */
#define NV_REGW_BLOCK(rm, aperture, num, offset, values) \
    NvRegwb((rm),(aperture),(num),(offset),(values))

/**
 * Reads a block of registers from hardware.
 *
 * @note Resource Manager (RM) doesn't gaurantee access to all the modules as it
 * only maps a few modules. This is not meant to be a primary mechanism to
 * access the module registers. Clients should map their register address and
 * access the registers.
 *
 * @param rm The resource manager istance.
 * @param aperture The register aperture.
 * @param num The number of registers.
 * @param offset The beginning register offset.
 * @param values The register values.
 */
#define NV_REGR_BLOCK(rm, aperture, num, offset, values) \
    NvRegrb((rm),(aperture),(num),(offset),(values))

 void NvRegrm(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID aperture,
    NvU32 num,
    const NvU32 * offsets,
    NvU32 * values );

 void NvRegwm(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID aperture,
    NvU32 num,
    const NvU32 * offsets,
    const NvU32 * values );

 void NvRegwb(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID aperture,
    NvU32 num,
    NvU32 offset,
    const NvU32 * values );

 void NvRegrb(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID aperture,
    NvU32 num,
    NvU32 offset,
    NvU32 * values );

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
