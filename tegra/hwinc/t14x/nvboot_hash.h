/*
 * Copyright (c) 2006 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_hash.h
 *
 * Defines the NvBootHash data structure for the Boot ROM.
 *
 * The Boot ROM uses the AES-CMAC keyed hash function for validating objects
 * read from the secondary boot device.
 *
 * The AES-CMAC algorithm is defined in RFC 4493 published by the Internet 
 * Engineering Task Force (IETF).  The RFC is available at the following web 
 * site, among others --
 *
 *    http://tools.ietf.org/html/rfc4493
 *
 * The 128-bit variant is implemented here, i.e., AES-CMAC-128.  This variant
 * employs an underlying AES-128 encryption computataion.
 *
 * This implementation only handles messages that are a multiple of 16 bytes in
 * length.  The intent is to compute the hash value for AES-encrypted messages
 * which (by definition) are always a multiple of 16 bytes in length.  Hence,
 * the K2 constant will never be used and therefore is not even computed.
 * Similarly, there's no provision for padding packets since they're already
 * required to be a multiple of 16 bytes in length.
 */

#ifndef INCLUDED_NVBOOT_HASH_H
#define INCLUDED_NVBOOT_HASH_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** 
 * Defines the CMAC-AES-128 hash length in 32 bit words. (128 bits = 4 words)
 */
enum {NVBOOT_CMAC_AES_HASH_LENGTH = 4};

/**
 * Defines the storage for a hash value (128 bits).
 */
typedef struct NvBootHashRec 
{
    NvU32 hash[NVBOOT_CMAC_AES_HASH_LENGTH];
} NvBootHash;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_HASH_H
