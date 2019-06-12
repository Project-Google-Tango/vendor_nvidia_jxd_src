//
// Copyright (c) 2008-2013, NVIDIA CORPORATION.  All Rights Reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA CORPORATION
// is strictly prohibited.
//

/**
 * @file
 * <b>NVIDIA Tegra Boot Loader API</b>
 *
 */

#ifndef INCLUDED_NVBL_QUERY_H
#define INCLUDED_NVBL_QUERY_H

#include "nvbl_common.h"
#include "nvbl_bootdevices.h"
#include "nvddk_blockdevmgr_defs.h"
#include "nvddk_fuse.h"

#if defined(__cplusplus)
extern "C"
{
#endif
/**
 * @defgroup nvbl_query_group NvBL Query API
 *
 *
 * @ingroup nvbl_group
 * @{
 */

/**
 * @brief Queries for a hardware-based random number.
 *
 * @pre Not callable from early boot.
 *
 * @returns A random number generated from free-running timer and/or
 * clock sources within the chip. Returns zero if this feature is not
 * supported.
 */
NvU64 NvBlQueryRandomSeed(void);


/**
 * @brief Queries for the device unique ID.
 *
 * @pre Not callable from early boot.
 *
 * @param pId A pointer to an area of caller-allocated memory to hold the
 * unique ID.
 * @param pIdSize On input, a pointer to a variable containing the size of
 * the caller-allocated memory to hold the unique ID pointed to by \a pId.
 * Upon successful return, this value is updated to reflect the actual
 * size of the unique ID returned in \a pId.
 *
 * @retval ::NvError_Success \a pId points to the unique ID and \a pIdSize
 * points to the actual size of the ID.
 * @retval ::NvError_BadParameter
 * @retval ::NvError_NotSupported
 * @retval ::NvError_InsufficientMemory
 */
NvError NvBlQueryUniqueId(void* pId, NvU32* pIdSize);


/**
 * Defines workarounds required in OS-specific code as
 * a bit mask.
 */
typedef enum
{
    /// Specifies an invalid workaround ID.
    NvBlWorkAround_Invalid = 0,

    /// Specifies an MIO workaround.
    NvBlWorkAround_Mio,

    /// Specifies that an L1 cache flush requires an L2 cache flush.
    NvBlWorkAround_L1FlushRequiresL2Flush,

    /// Specifies that floating point support is not available.
    NvBlWorkAround_NoVfpSupport,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvBlWorkAround_Force32 = 0x7FFFFFFF

} NvBlWorkAround;


/**
 * @brief Queries specific workarounds.
 *
 * @pre Not callable from early boot.
 *
 * @param WorkAround Specifies the workaround identifier.
 *
 * @returns NV_TRUE if the workaround is required, or NV_FALSE if it is not.
 */
NvBool NvBlQueryWorkAround(NvBlWorkAround WorkAround);


/**
 * @brief Queries for the core type string.
 *
 * @returns A pointer to a null-terminated ASCII string containing
 * the name of the processor type (e.g., "MPCore").
 */
const NvU8* NvBlQueryCoreTypeString(void);

/**
 * @brief Queries for the processor name string.
 *
 * @returns A pointer to a null-terminated ASCII string containing
 * the name of the processor (e.g., "APX 2600").
 */
const NvU8* NvBlQueryProcessorNameString(void);

/**
 * @brief Queries for the processor manufacturer and model.
 *
 * @returns A pointer to a null-terminated ASCII string containing
 * the processor manufacturer and model (e.g., "NVIDIA APX2600").
 */
const NvU8* NvBlQueryManufacturerModelName(void);

/**
 * @brief Queries the number of processors in the CPU complex.
 *
 * @returns The number of processors (does not include the AVP).
 */
NvU32 NvBlQueryNumberOfCpus(void);

/**
 * @brief Queries the number of active processors in the CPU complex.
 *
 * @returns The number of active processors (does not include the AVP).
 */
NvS32 NvBlQueryNumberOfActiveCpus(void);

/**
 * @brief Queries the processor number of the current CPU.
 *
 * @returns The zero-relative processor number of the requesting CPU.
 */
NvU32 NvBlQueryCpuNumber(void);

/**
 * @brief Queries whether the SOC is an FPGA or real silicon.
 *
 * @returns NV_TRUE if the SOC is an FPGA, or NV_FALSE otherwise.
 */
NvBool NvBlQuerySocIsFpga(void);

/*
 * @brief Queries for the maximum CPU clock speed.
 *
 * @returns The maximum CPU clock speed.
 */
NvU32 NvBlQueryMaxCpuClockSpeed(void);

/*
 * @brief Queries whether to launch the nv3p server.
 *
 * @returns True if the bootloader was invoked from recovery mode.
 */
NvBool NvBlQueryGetNv3pServerFlag(void);

/*
 * @brief Queries whether to do RAM repair.
 *
 * @returns True if RAM repair is needed.
 */
NvBool NvBlRamRepairRequired(void);

/*
 * @brief Initializes PLLX.  Only available on T114.
 */
void NvBlInitPllX_t11x(void);

/** @} */
#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_NVBL_QUERY_H

