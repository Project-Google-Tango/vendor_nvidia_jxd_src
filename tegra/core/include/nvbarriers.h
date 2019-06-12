/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Memory barrier intrinsics
 */

#if defined(__GNUC__)
# if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#  define NvMemBarrier()    __sync_synchronize()
#  define NV_MEM_BARRIER_SUPPORTED 1
# else
// TODO: centos builds fall into this category.
#  define NV_MEM_BARRIER_SUPPORTED 0
# endif
#elif defined(__CC_ARM)
extern void NvMemBarrier(void);
# define NV_MEM_BARRIER_SUPPORTED 1
#else
# define NV_MEM_BARRIER_SUPPORTED 0
#endif
