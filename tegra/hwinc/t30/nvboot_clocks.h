/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines useful constants for working with clocks.
 */


#ifndef INCLUDED_NVBOOT_CLOCKS_H
#define INCLUDED_NVBOOT_CLOCKS_H

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * Set of valid count ranges per frequency.
 * Measuring 13 gives exactly 406 e.g.
 * The chosen range parameter is:
 * - more than the expected combined frequency deviation
 * - less than half the  relative distance between 12 and 13
 * - expressed as a ratio against a power of two to avoid floating point
 * - so that intermediate overflow is not possible
 *
 * The chosen factor is 1/64 or slightly less than 1.6% = 2^-6
 * Rounding is performed in such way as to guarantee at least the range
 * that is down for min and up for max
 * the range macros receive the frequency in kHz as argument
 * division by 32 kHz then becomes a shift by 5 to the right 
 *
 * The macros are defined for a frequency of 32768 Hz (not 32000 Hz). 
 * They use 2^-5 ranges, or about 3.2% and dispense with the rounding.
 * Also need to use the full value in Hz in the macro
 */

#define NVBOOT_CLOCKS_MIN_RANGE(F) (( F - (F>>5) - (1<<15) + 1 ) >> 15)
#define NVBOOT_CLOCKS_MAX_RANGE(F) (( F + (F>>5) + (1<<15) - 1 ) >> 15)

// For an easier ECO (keeping same number of instructions), we need a
// special case for 12 min range
#define NVBOOT_CLOCKS_MIN_CNT_12 (NVBOOT_CLOCKS_MIN_RANGE(12000000) -1)
#define NVBOOT_CLOCKS_MAX_CNT_12 NVBOOT_CLOCKS_MAX_RANGE(12000000)

#define NVBOOT_CLOCKS_MIN_CNT_13 NVBOOT_CLOCKS_MIN_RANGE(13000000)
#define NVBOOT_CLOCKS_MAX_CNT_13 NVBOOT_CLOCKS_MAX_RANGE(13000000)

#define NVBOOT_CLOCKS_MIN_CNT_16_8 NVBOOT_CLOCKS_MIN_RANGE(16800000)
#define NVBOOT_CLOCKS_MAX_CNT_16_8 NVBOOT_CLOCKS_MAX_RANGE(16800000)

#define NVBOOT_CLOCKS_MIN_CNT_19_2 NVBOOT_CLOCKS_MIN_RANGE(19200000)
#define NVBOOT_CLOCKS_MAX_CNT_19_2 NVBOOT_CLOCKS_MAX_RANGE(19200000)

#define NVBOOT_CLOCKS_MIN_CNT_26 NVBOOT_CLOCKS_MIN_RANGE(26000000)
#define NVBOOT_CLOCKS_MAX_CNT_26 NVBOOT_CLOCKS_MAX_RANGE(26000000)

#define NVBOOT_CLOCKS_MIN_CNT_38_4 NVBOOT_CLOCKS_MIN_RANGE(38400000)
#define NVBOOT_CLOCKS_MAX_CNT_38_4 NVBOOT_CLOCKS_MAX_RANGE(38400000)

#define NVBOOT_CLOCKS_MIN_CNT_48 NVBOOT_CLOCKS_MIN_RANGE(48000000)
#define NVBOOT_CLOCKS_MAX_CNT_48 NVBOOT_CLOCKS_MAX_RANGE(48000000)

// The stabilization delay in usec 
#define NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY (300) 
// The stabilization delay after the lock bit indicates the PLL is lock 
#define NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY_AFTER_LOCK (100) 

// other values important when starting PLLP
#define NVBOOT_CLOCKS_PLLP_CPCON_DEFAULT (8)
#define NVBOOT_CLOCKS_PLLP_CPCON_19_2 (1)

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_CLOCKS_H */
