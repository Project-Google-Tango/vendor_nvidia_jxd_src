/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_init_H
#define INCLUDED_nvrm_init_H


#if defined(__cplusplus)
extern "C"
{
#endif


/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *     Resource Manager %Initialization API</b>
 *
 * @b Description: Declares the Resource Manager (RM) initialization API.
 */

/** @defgroup nvrm_init RM Initialization
 *
 * This file declares the Resource Manager initialization API.
 * @ingroup nvddk_rm
 *  @{
 */

#include "nvcommon.h"
#include "nverror.h"

/**
 * An opaque handle to an RM device.
 */

typedef struct NvRmDeviceRec *NvRmDeviceHandle;


// XXX We should probably get rid of this and just use NvU32.  It's rather
// difficult to explain what exactly NvRmPhysAddr is.  Also, what if some units
// are upgraded to do 64-bit addressing and others remain 32?  Would we really
// want to increase NvRmPhysAddr to NvU64 across the board?
//
// Another option would be to put the following types in nvcommon.h:
//   typedef NvU32 NvPhysAddr32;
//   typedef NvU64 NvPhysAddr64;
// Using these types would then be purely a form of documentation and nothing
// else.
//
// This header file is a somewhat odd place to put this type.  Putting it in
// memmgr would be even worse, though, because then a lot of header files would
// all suddenly need to #include nvrm_memmgr.h just to get the NvRmPhysAddr
// type.  (They already all include this header anyway.)

/**
 * A physical address type sized such that it matches the addressing support of
 * the hardware modules with which RM typically interfaces. May be smaller than
 * an ::NvOsPhysAddr.
 */

typedef NvU32 NvRmPhysAddr;

/**
 * Opens the Resource Manager (RM) for a given device.
 *
 * Can be called multiple times for a given device. Subsequent
 * calls will not necessarily return the same handle. Each call to
 * \c NvRmOpen must be paired with a corresponding call to NvRmClose().
 *
 * Assert encountered in debug mode if \a DeviceId value is invalid.
 *
 * This call is not intended to perform any significant hardware
 * initialization of the device; rather, its primary purpose is to
 * initialize RM's internal data structures that are involved in
 * managing the device.
 *
 * @param pHandle A pointer to the RM handle.
 * @param DeviceId Implementation-dependent value specifying the device
 *     to be opened. Currently must be set to zero.
 *
 * @retval NvSuccess Indicates that RM was successfully opened.
 * @retval NvError_InsufficientMemory Indicates that RM was unable to allocate
 *     memory for its internal data structures.
 */

 NvError NvRmOpen(
    NvRmDeviceHandle * pHandle,
    NvU32 DeviceId );

/**
 * Called by the platform/OS code to initialize the RM. Usage and
 * implementation of this API is platform-specific.
 *
 * This API should not be called by the normal clients of the RM.
 *
 * This APIs is guaranteed to succeed on the supported platforms.
 *
 * @param pHandle A pointer to the RM handle.
 */

 void NvRmInit(
    NvRmDeviceHandle * pHandle );

/**
 * Temporary version of NvRmOpen() lacking the \a DeviceId parameter.
 */

 NvError NvRmOpenNew(
    NvRmDeviceHandle * pHandle );

/**
 * Closes the Resource Manager (RM) for a given device.
 *
 * Each call to NvRmOpen() must be paired with a corresponding call
 * to \c NvRmClose.
 *
 * @param hDevice The RM handle. If \a hDevice is NULL, this API has no effect.
 */

 void NvRmClose(
    NvRmDeviceHandle hDevice );

/**
 * Shutdown the Resource Manager (RM).
 *
 * @param hDevice The RM handle. If \a hDevice is NULL, this API has no effect.
 */

 void NvRmShutdown(
    void  );

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
