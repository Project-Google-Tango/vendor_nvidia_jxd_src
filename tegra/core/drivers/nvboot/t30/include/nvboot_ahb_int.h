/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvboot_ahb_int.h - AHB coherency interface
 *
 * AHB master IDs used in AHB implementation:
 * BSEA engine --> ARAHB_MST_ID_BSEA
 * BSEV engine --> ARAHB_MST_ID_BSEV
 * MOBILE LBA NAND --> ARAHB_MST_ID_NAND
 * MUX ONE NAND --> ARAHB_MST_ID_NAND
 * NAND controller --> ARAHB_MST_ID_NAND
 * SDMMC3 controller --> ARAHB_MST_ID_SDMMC3
 * SDMMC4 controller --> ARAHB_MST_ID_SDMMC4
 * SNOR controller --> ARAHB_MST_ID_SNOR
 * SPI controller --> ARAHB_MST_ID_APBDMA
 *
 */

#ifndef INCLUDED_NVBOOT_AHB_INT_H
#define INCLUDED_NVBOOT_AHB_INT_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Check if an AHB master has any outstanding data in its write queue to be
 * written to extermal memory. This just checks the corresponding
 * AHB master's AHB Memory Write Queue bit once.
 *
 * @param AhbMasterID The AHB master ID number of the AHB master to check.
 *
 * @retval NV_TRUE if there are no more writes in the write queue.
 *         NV_FALSE if there are writes in the write queue outstanding.
 */
NvBool NvBootAhbCheckCoherency(NvU8 AhbMasterID);

/**
 * Check if an AHB master has any outstanding data in its write queue to be
 * written to extermal memory. This function will poll the AHB master's AHB
 * Memory Write Queue bit until it is cleared to zero. Time out after 200ms
 * (defined in NVBOOT_AHB_WAIT_COHERENCY_TIMEOUT_US).
 *
 * @param AhbMasterID The AHB master ID number of the AHB master to check.
 *
 * @retval none.
 */
void NvBootAhbWaitCoherency(NvU8 AhbMasterID);


/**
 * Check if a 32-bit address is an external memory address or not.
 * NV_ADDRESS_MAP_EMEM_BASE and NV_ADDRESS_MAP_EMEM_LIMIT are used to determine
 * the external memory region.
 *
 * @param Address Pointer to 32-bit address
 *
 * @retval none.
 */
NvBool NvBootAhbCheckIsExtMemAddr(NvU32 *Address);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_AHB_INT_H
