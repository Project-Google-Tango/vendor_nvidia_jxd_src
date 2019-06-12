/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Description: General-purpose memory testing functions.
 *
 * Notes:       This software can be easily ported to systems with
 *              different data bus widths by redefining 'datum'.
 *
 * Copyright (c) 1998 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 */

#include "nvml.h"

/**********************************************************************
 *
 * Function:    VerifySdramDataBus()
 *
 * Description: Test the data bus wiring in a memory region by
 *              performing a walking 1's test at a fixed address
 *              within that region.  The address (and hence the
 *              memory region) is selected by the caller.
 *
 * Notes:
 *
 * Returns:     0 if the test succeeds.
 *              A non-zero result is the first pattern that failed.
 *
 **********************************************************************/
NvDatum VerifySdramDataBus(volatile NvDatum *addr)
{
    NvDatum pattern;

    for (pattern = 1; pattern != 0; pattern <<= 1) {
        *addr = pattern;
        NvOsDataCacheWritebackInvalidate();
        if (*addr != pattern)
            return pattern;
    }
    return 0;
}

/**********************************************************************
 *
 * Function:    VerifySdramAddressBus()
 *
 * Description: Test the address bus wiring in a memory region by
 *              performing a walking 1's test on the relevant bits
 *              of the address and checking for aliasing. This test
 *              will find single-bit address failures such as stuck
 *              -high, stuck-low, and shorted pins.  The base address
 *              and size of the region are selected by the caller.
 *
 * Notes:       For best results, the selected base address should
 *              have enough LSB 0's to guarantee single address bit
 *              changes.  For example, to test a 64-Kbyte region,
 *              select a base address on a 64-Kbyte boundary.  Also,
 *              select the region size as a power-of-two--if at all
 *              possible.
 *
 * Returns:     NV_TRUE if the test succeeds.
 *              NV_FALSE if the test fails.
 *
 **********************************************************************/
NvBool VerifySdramAddressBus(volatile NvDatum *testBaseAddr, NvU32 testSize)
{
    unsigned long addressMask;
    unsigned long offset;
    unsigned long testOffset;
    NvDatum pattern     = (NvDatum) 0xAAAAAAAA;
    NvDatum antipattern = (NvDatum) 0x55555555;
    NvBool b = NV_TRUE;


    addressMask = (testSize/sizeof(NvDatum) - 1);
    /* Write the default pattern at each of the power-of-two offsets */
    for (offset = 1; (offset & addressMask) != 0; offset <<= 1)
        testBaseAddr[offset] = pattern;

    /* Check for address bits stuck high  */
    testOffset = 0;
    testBaseAddr[testOffset] = antipattern;
    NvOsDataCacheWritebackInvalidate();

    /* Check for address bits stuck high  */
    for (offset = 1; (offset & addressMask) != 0; offset <<= 1)
    {
        if (testBaseAddr[offset] != pattern)
        {
            b = NV_FALSE;
            goto clean;
        }
    }
    testBaseAddr[testOffset] = pattern;
    NvOsDataCacheWritebackInvalidate();

    /* Check for address bits stuck low or shorted */
    for (testOffset = 1; (testOffset & addressMask) != 0; testOffset <<= 1)
    {
        testBaseAddr[testOffset] = antipattern;
        NvOsDataCacheWritebackInvalidate();
        if (testBaseAddr[0] != pattern)
        {
            b = NV_FALSE;
            goto clean;
        }
        for (offset = 1; (offset & addressMask) != 0; offset <<= 1)
        {
            if ((testBaseAddr[offset] != pattern) && (offset != testOffset))
            {
                b = NV_FALSE;
                goto clean;
            }
        }
        testBaseAddr[testOffset] = pattern;
        NvOsDataCacheWritebackInvalidate();
    }

clean:
    return b;
}
