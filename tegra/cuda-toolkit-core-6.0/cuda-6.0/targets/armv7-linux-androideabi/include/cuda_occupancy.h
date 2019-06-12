/*
 * Copyright 1993-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

/*
 * Important Notice about Beta Status
 *
 * The CUDA Occupancy Calculator is in beta and is intended for
 * evaluation purpose only. The data structures and programming
 * interfaces are subject to change in future releases.
 */

#ifndef __cuda_occupancy_h__
#define __cuda_occupancy_h__

#define CUDA_OCCUPANCY_MAJOR 5
#define CUDA_OCCUPANCY_MINOR 0

#ifdef __CUDACC__
#define OCC_INLINE inline __host__ __device__
#elif defined _MSC_VER
#define OCC_INLINE __inline
#else //GNUCC assumed
#define OCC_INLINE inline
#endif

#define MIN_SHARED_MEM_PER_SM (16384)
#define MIN_SHARED_MEM_PER_SM_GK210 (81920)

#define     OCC_CHECK_ERROR(x) \
    do { \
        if ((x) < 0) { \
            return (x); \
        } \
    } while (0)

#define     OCC_CHECK_ZERO(x) \
    do { \
        if ((x) == 0) { \
            return CUDA_OCC_ERROR_INVALID_INPUT; \
        } \
    } while (0)

#include <stddef.h>

/*!  define our own cudaOccDeviceProp to avoid errors when #including
 *   this file in the absence of a CUDA installation
 */
typedef struct cudaOccDeviceProp {
    // mirror the type and spelling of cudaDeviceProp's members
    // keep these alphabetized
    int    major;
    int    minor;
    int    maxThreadsPerBlock;
    int    maxThreadsPerMultiProcessor;
    int    regsPerBlock;
    int    regsPerMultiprocessor;
    int    warpSize;
    size_t sharedMemPerBlock;
    size_t sharedMemPerMultiprocessor;
}cudaOccDeviceProp;


/*!  define our own cudaOccFuncAttributes to avoid errors when #including
 *   this file in the absence of a CUDA installation
 */
typedef struct cudaOccFuncAttributes {
    // mirror the type and spelling of cudaFuncAttributes' members
    // keep these alphabetized
    int    maxThreadsPerBlock;
    int    numRegs;
    size_t sharedSizeBytes;
}cudaOccFuncAttributes;

/*!
 * Occupancy Error types
 */
typedef enum cudaOccError_enum {
    CUDA_OCC_ERROR_INVALID_INPUT   = -1, /** input parameter is invalid */
    CUDA_OCC_ERROR_UNKNOWN_DEVICE  = -2, /** requested device is not supported in current implementation or device is invalid */
}cudaOccError;

/*!
 * Function cache configurations
 */
typedef enum cudaOccCacheConfig_enum {
    CACHE_PREFER_NONE    = 0x00, /**< no preference for shared memory or L1 (default) */
    CACHE_PREFER_SHARED  = 0x01, /**< prefer larger shared memory and smaller L1 cache */
    CACHE_PREFER_L1      = 0x02, /**< prefer larger L1 cache and smaller shared memory */
    CACHE_PREFER_EQUAL   = 0x03,  /**< prefer equal sized L1 cache and shared memory */
    //Add new values above this line
    CACHE_CONFIG_COUNT
}cudaOccCacheConfig;

/*!
 * Occupancy Limiting Factors
 */
typedef enum cudaOccLimitingFactors_enum {
    OCC_LIMIT_WARPS          = 0x01, /** occupancy limited due to warps available */
    OCC_LIMIT_REGISTERS      = 0x02, /** occupancy limited due to registers available */
    OCC_LIMIT_SHARED_MEMORY  = 0x04, /** occupancy limited due to shared memory available */
    OCC_LIMIT_BLOCKS         = 0x08  /** occupancy limited due to blocks available */
}cudaOccLimitingFactors;

typedef struct cudaOccResult {
    int activeBlocksPerMultiProcessor;
    int limitingFactors;
    int blockLimitRegs;
    int blockLimitSharedMem;
    int blockLimitWarps;
    int blockLimitBlocks;
    int allocatedRegistersPerBlock;
    int allocatedSharedMemPerBlock;
}cudaOccResult;

/*!  define cudaOccDeviceState to include any device property needed to be passed
 *   in future GPUs so that user interfaces don't change ; hence users are encouraged
 *   to declare the struct zero in order to handle the assignments of any field
 *   that might be added for later GPUs.
 */
typedef struct cudaOccDeviceState
{
    int cacheConfig;
}cudaOccDeviceState;

//////////////////////////////////////////
//    Mathematical Helper Functions     //
//////////////////////////////////////////

// get the minimum out of two parameters
static OCC_INLINE
int min_(
    int lhs,
    int rhs)
{
    return rhs < lhs ? rhs : lhs;
}


// x/y rounding towards +infinity for integers, used to determine # of blocks/warps etc.
static OCC_INLINE
int divide_ri(
    int x,
    int y)
{
    return (x + (y - 1)) / y;
}

// round x towards infinity to the next multiple of y
static OCC_INLINE
int round_i(
    int x,
    int y)
{
    return y * divide_ri(x, y);
}

//////////////////////////////////////////
//    Occupancy Helper Functions        //
//////////////////////////////////////////

/*!
 * Granularity of shared memory allocation
 */
static OCC_INLINE
int cudaOccSMemAllocationUnit(
    const cudaOccDeviceProp *properties)
{
    switch(properties->major)
    {
        case 1:  return 512;
        case 2:  return 128;
        case 3:
        case 5:  return 256;
        default: return CUDA_OCC_ERROR_UNKNOWN_DEVICE;
    }
}


/*!
 * Granularity of register allocation
 */
static OCC_INLINE
int cudaOccRegAllocationUnit(
    const cudaOccDeviceProp *properties,
    int regsPerThread)
{
    switch(properties->major)
    {
        case 1:  return (properties->minor <= 1) ? 256 : 512;
        case 2:  switch(regsPerThread)
                 {
                    case 21:
                    case 22:
                    case 29:
                    case 30:
                    case 37:
                    case 38:
                    case 45:
                    case 46:
                        return 128;
                    default:
                        return 64;
                 }
        case 3:
        case 5:  return 256;
        default: return CUDA_OCC_ERROR_UNKNOWN_DEVICE;
    }
}


/*!
 * Granularity of warp allocation
 */
static OCC_INLINE
int cudaOccWarpAllocationMultiple(
    const cudaOccDeviceProp *properties)
{
    return (properties->major <= 1) ? 2 : 1;
}

/*!
 * Number of "sides" into which the multiprocessor is partitioned
 */
static OCC_INLINE
int cudaOccSidesPerMultiprocessor(
    const cudaOccDeviceProp *properties)
{
    switch(properties->major)
    {
        case 1:  return 1;
        case 2:  return 2;
        case 3:  return 4;
        case 5:  return 4;
        default: return CUDA_OCC_ERROR_UNKNOWN_DEVICE;
    }
}

/*!
 * Maximum blocks that can run simultaneously on a multiprocessor
 */
static OCC_INLINE
int cudaOccMaxBlocksPerMultiprocessor(
    const cudaOccDeviceProp *properties)
{
    switch(properties->major)
    {
        case 1:  return 8;
        case 2:  return 8;
        case 3:  return 16;
        case 5:  return 32;
        default: return CUDA_OCC_ERROR_UNKNOWN_DEVICE;
    }
}

/*!
 * Map int to cudaOccCacheConfig
 */
static OCC_INLINE
cudaOccCacheConfig cudaOccGetCacheConfig(
    const cudaOccDeviceState *state)
{
    switch(state->cacheConfig)
    {
        case 0:  return CACHE_PREFER_NONE;
        case 1:  return CACHE_PREFER_SHARED;
        case 2:  return CACHE_PREFER_L1;
        case 3:  return CACHE_PREFER_EQUAL;
        default: return CACHE_PREFER_NONE;
    }
}

/*!
 * Shared memory based on config requested by User
 */
static OCC_INLINE
int cudaOccSMemPerMultiprocessor(
    const cudaOccDeviceProp *properties,
    cudaOccCacheConfig cacheConfig)
{
    int bytes = 0;
    int sharedMemPerMultiprocessorHigh = (int) properties->sharedMemPerMultiprocessor;
    int sharedMemPerMultiprocessorLow  = (properties->major==3 && properties->minor==7)
        ? MIN_SHARED_MEM_PER_SM_GK210
        : MIN_SHARED_MEM_PER_SM ;

    switch(properties->major)
    {
        case 1:
        case 2: bytes = (cacheConfig == CACHE_PREFER_L1)? sharedMemPerMultiprocessorLow : sharedMemPerMultiprocessorHigh;
                break;
        case 3: switch (cacheConfig)
                {
                    default :
                    case CACHE_PREFER_NONE:
                    case CACHE_PREFER_SHARED:
                            bytes = sharedMemPerMultiprocessorHigh;
                            break;
                    case CACHE_PREFER_L1:
                            bytes = sharedMemPerMultiprocessorLow;
                            break;
                    case CACHE_PREFER_EQUAL:
                            bytes = (sharedMemPerMultiprocessorHigh + sharedMemPerMultiprocessorLow) / 2;
                            break;
                }
                break;
        case 5:
        default: bytes = sharedMemPerMultiprocessorHigh;
                 break;
    }

    return bytes;
}

/*!
 * Assign parameters to cudaOccDeviceProp
 */
static OCC_INLINE
void cudaOccSetDeviceProp(
    cudaOccDeviceProp* properties,
    int major,
    int minor,
    size_t sharedMemPerBlock,
    size_t sharedMemPerMultiprocessor,
    int regsPerBlock,
    int regsPerMultiprocessor,
    int warpSize,
    int maxThreadsPerBlock ,
    int maxThreadsPerMultiProcessor)
{
    properties->major                       = major;
    properties->minor                       = minor;
    properties->sharedMemPerBlock           = sharedMemPerBlock;
    properties->sharedMemPerMultiprocessor  = sharedMemPerMultiprocessor;
    properties->regsPerBlock                = regsPerBlock;
    properties->regsPerMultiprocessor       = regsPerMultiprocessor;
    properties->warpSize                    = warpSize;
    properties->maxThreadsPerBlock          = maxThreadsPerBlock;
    properties->maxThreadsPerMultiProcessor = maxThreadsPerMultiProcessor;
}

/*!
 * Assign parameters to cudaOccFuncAttributes
 */
static OCC_INLINE
void cudaOccSetFuncAttributes(
    cudaOccFuncAttributes* attributes,
    int maxThreadsPerBlock,
    int numRegs,
    size_t sharedSizeBytes)
{
    attributes->maxThreadsPerBlock = maxThreadsPerBlock;
    attributes->numRegs            = numRegs;
    attributes->sharedSizeBytes    = sharedSizeBytes;
}

///////////////////////////////////////////////
//    Occupancy calculation Functions        //
//////////////////////////////////////////////

/*!  Determine the maximum number of CTAs that can be run simultaneously per SM
 *   This is equivalent to the calculation done in the CUDA Occupancy Calculator
 *   spreadsheet
 */

static OCC_INLINE
int cudaOccMaxActiveBlocksPerMultiprocessor(
    const cudaOccDeviceProp *properties,
    const cudaOccFuncAttributes *attributes,
    int blockSize,
    size_t dynamic_smem_bytes,
    const cudaOccDeviceState *state,
    cudaOccResult *result)
{
    int regAllocationUnit = 0, warpAllocationMultiple = 0, maxBlocksPerSM=0;
    int ctaLimitWarps = 0, ctaLimitBlocks = 0, smemPerCTA = 0, smemBytes = 0, smemAllocationUnit = 0;
    int cacheConfigSMem = 0, sharedMemPerMultiprocessor = 0, ctaLimitRegs = 0, regsPerCTA=0;
    int regsPerWarp = 0, numSides = 0, numRegsPerSide = 0, ctaLimit=0;
    int maxWarpsPerSm = 0, warpsPerCTA = 0, ctaLimitSMem=0, limitingFactors = 0;

    if(properties == NULL || attributes == NULL || blockSize <= 0)
    {
        return CUDA_OCC_ERROR_INVALID_INPUT ;
    }

    if(state == NULL || state->cacheConfig < CACHE_PREFER_NONE ||  state->cacheConfig >= CACHE_CONFIG_COUNT)
    {
        return CUDA_OCC_ERROR_INVALID_INPUT ;
    }

    //////////////////////////////////////////
    // Limits due to warps/SM or blocks/SM
    //////////////////////////////////////////
    OCC_CHECK_ZERO(properties->warpSize);
    maxWarpsPerSm   = properties->maxThreadsPerMultiProcessor / properties->warpSize;
    warpAllocationMultiple = cudaOccWarpAllocationMultiple(properties);
    OCC_CHECK_ERROR(warpAllocationMultiple);

    OCC_CHECK_ZERO(warpAllocationMultiple);
    warpsPerCTA = round_i(divide_ri(blockSize, properties->warpSize), warpAllocationMultiple);

    maxBlocksPerSM  = cudaOccMaxBlocksPerMultiprocessor(properties);
    OCC_CHECK_ERROR(maxBlocksPerSM);

    // Calc limits
    OCC_CHECK_ZERO(warpsPerCTA);
    ctaLimitWarps  = (blockSize <= properties->maxThreadsPerBlock) ? maxWarpsPerSm / warpsPerCTA : 0;
    ctaLimitBlocks = maxBlocksPerSM;

    //////////////////////////////////////////
    // Limits due to shared memory/SM
    //////////////////////////////////////////
    smemAllocationUnit     = cudaOccSMemAllocationUnit(properties);
    OCC_CHECK_ERROR(smemAllocationUnit);
    smemBytes  = (int)(attributes->sharedSizeBytes + dynamic_smem_bytes);
    OCC_CHECK_ZERO(smemAllocationUnit);
    smemPerCTA = round_i(smemBytes, smemAllocationUnit);

    // Calc limit
    cacheConfigSMem = cudaOccSMemPerMultiprocessor(properties,cudaOccGetCacheConfig(state));

    // sharedMemoryPerMultiprocessor is by default limit set in hardware but user requested shared memory
    // limit is used instead if it is greater than total shared memory used by function .
    sharedMemPerMultiprocessor = (cacheConfigSMem >= smemPerCTA)
        ? cacheConfigSMem
        : (int)properties->sharedMemPerMultiprocessor;
    // Limit on blocks launched should be calculated with shared memory per SM but total shared memory
    // used by function should be limited by shared memory per block
    ctaLimitSMem = 0;
    if(properties->sharedMemPerBlock >= (size_t)smemPerCTA)
    {
        ctaLimitSMem = smemPerCTA > 0 ? sharedMemPerMultiprocessor / smemPerCTA : maxBlocksPerSM;
    }

    //////////////////////////////////////////
    // Limits due to registers/SM
    //////////////////////////////////////////
    regAllocationUnit      = cudaOccRegAllocationUnit(properties, attributes->numRegs);
    OCC_CHECK_ERROR(regAllocationUnit);
    OCC_CHECK_ZERO(regAllocationUnit);

    // Calc limit
    ctaLimitRegs = 0;
    if(properties->major <= 1)
    {
        // GPUs of compute capability 1.x allocate registers to CTAs
        // Number of regs per block is regs per thread times number of warps times warp size, rounded up to allocation unit
        regsPerCTA = round_i(attributes->numRegs * properties->warpSize * warpsPerCTA, regAllocationUnit);
        ctaLimitRegs = regsPerCTA > 0 ? properties->regsPerMultiprocessor / regsPerCTA : maxBlocksPerSM;
    }
    else
    {
        // GPUs of compute capability 2.x and higher allocate registers to warps
        // Number of regs per warp is regs per thread times number of warps times warp size, rounded up to allocation unit
        regsPerWarp = round_i(attributes->numRegs * properties->warpSize, regAllocationUnit);
        regsPerCTA = regsPerWarp * warpsPerCTA;
        if(properties->regsPerBlock >= regsPerCTA)
        {
            numSides = cudaOccSidesPerMultiprocessor(properties);
            OCC_CHECK_ERROR(numSides);
            OCC_CHECK_ZERO(numSides);
            numRegsPerSide = properties->regsPerMultiprocessor / numSides;
            ctaLimitRegs = regsPerWarp > 0 ? ((numRegsPerSide / regsPerWarp) * numSides) / warpsPerCTA : maxBlocksPerSM;
        }
    }

    //////////////////////////////////////////
    // Overall limit is min() of limits due to above reasons
    //////////////////////////////////////////
    ctaLimit = min_(ctaLimitRegs, min_(ctaLimitSMem, min_(ctaLimitWarps, ctaLimitBlocks)));

    // Determine occupancy limiting factors
    if(result != NULL)
    {
        result->activeBlocksPerMultiProcessor = ctaLimit;

        if(ctaLimit==ctaLimitWarps)
        {
            limitingFactors |= OCC_LIMIT_WARPS;
        }
        if(ctaLimit==ctaLimitRegs && regsPerCTA > 0)
        {
            limitingFactors |= OCC_LIMIT_REGISTERS;
        }
        if(ctaLimit==ctaLimitSMem && smemPerCTA > 0)
        {
            limitingFactors |= OCC_LIMIT_SHARED_MEMORY;
        }
        if(ctaLimit==ctaLimitBlocks)
        {
            limitingFactors |= OCC_LIMIT_BLOCKS;
        }
        result->limitingFactors = limitingFactors;

        result->blockLimitRegs = ctaLimitRegs;
        result->blockLimitSharedMem = ctaLimitSMem;
        result->blockLimitWarps = ctaLimitWarps;
        result->blockLimitBlocks = ctaLimitBlocks;

        result->allocatedRegistersPerBlock = regsPerCTA;
        result->allocatedSharedMemPerBlock = smemPerCTA;
    }

    return ctaLimit;
}

/*!  Determine the potential block size that allows maximum number 
 *   of CTAs that can run on multiprocessor simultaneously
 */
static OCC_INLINE
int cudaOccMaxPotentialOccupancyBlockSize(
    const cudaOccDeviceProp *properties,
    const cudaOccFuncAttributes *attributes,
    const cudaOccDeviceState *state,
    size_t (*blockSizeToDynamicSMemSize)(int))
{
    int maxOccupancy       = properties->maxThreadsPerMultiProcessor;
    int largestBlockSize   = min_(properties->maxThreadsPerBlock, attributes->maxThreadsPerBlock);
    int granularity        = properties->warpSize;
    int maxBlockSize  = 0;
    int blockSize     = 0;
    int highestOccupancy   = 0;

    for(blockSize = largestBlockSize; blockSize > 0; blockSize -= granularity)
    {
        int occupancy = cudaOccMaxActiveBlocksPerMultiprocessor(properties, attributes, blockSize, (*blockSizeToDynamicSMemSize)(blockSize), state, NULL);
        OCC_CHECK_ERROR(occupancy);

        occupancy = blockSize*occupancy;

        if(occupancy > highestOccupancy)
        {
            maxBlockSize = blockSize;
            highestOccupancy = occupancy;
        }

        // can not get higher occupancy
        if(highestOccupancy == maxOccupancy)
            break;
    }

    return maxBlockSize;
}


#if defined(__cplusplus)

namespace {

struct __cudaOccMaxPotentialOccupancyBlockSizeHelper
{
    OCC_INLINE
    __cudaOccMaxPotentialOccupancyBlockSizeHelper(size_t n) : n(n) {}

    template<typename T>
    OCC_INLINE
    size_t operator()(T)
    {
        return n;
    }

    private:
        size_t n;
};

template <typename UnaryFunction>
OCC_INLINE
int cudaOccMaxPotentialOccupancyBlockSize(
    const cudaOccDeviceProp   *properties,
    const cudaOccFuncAttributes *attributes,
    const cudaOccDeviceState *state,
    UnaryFunction blockSizeToDynamicSMemSize)
{
    int maxOccupancy       = properties->maxThreadsPerMultiProcessor;
    int largestBlockSize   = min_(properties->maxThreadsPerBlock, attributes->maxThreadsPerBlock);
    int granularity        = properties->warpSize;
    int maxBlockSize       = 0;
    int blockSize          = 0;
    int highestOccupancy   = 0;

    for(blockSize = largestBlockSize; blockSize != 0; blockSize -= granularity)
    {
        int occupancy = cudaOccMaxActiveBlocksPerMultiprocessor(properties, attributes, blockSize, blockSizeToDynamicSMemSize(blockSize), state, NULL);
        OCC_CHECK_ERROR(occupancy);

        occupancy = blockSize*occupancy;

        if(occupancy > highestOccupancy)
        {
            maxBlockSize = blockSize;
            highestOccupancy = occupancy;
        }

        // can not get higher occupancy
        if(highestOccupancy == maxOccupancy)
            break;
    }

    return maxBlockSize;
}

OCC_INLINE
int cudaOccMaxPotentialOccupancyBlockSize(
    const cudaOccDeviceProp   *properties,
    const cudaOccFuncAttributes *attributes,
    const cudaOccDeviceState *state,
    size_t numDynamicSmemBytes = 0)
{
    __cudaOccMaxPotentialOccupancyBlockSizeHelper helper(numDynamicSmemBytes) ;
    return cudaOccMaxPotentialOccupancyBlockSize(properties, attributes, state, helper);
}

OCC_INLINE
int cudaOccMaxPotentialOccupancyBlockSize(
    const cudaOccDeviceProp   *properties,
    const cudaOccFuncAttributes *attributes,
    const cudaOccDeviceState *state,
    int numDynamicSmemBytes)
{
    return cudaOccMaxPotentialOccupancyBlockSize(properties, attributes, state, (size_t)numDynamicSmemBytes);
}

} // namespace anonymous

#endif /*__cplusplus */

#undef OCC_INLINE

#endif /*__cuda_occupancy_h__*/
