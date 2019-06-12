/*
 * Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_SHA1_H
#define INCLUDED_SHA1_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * implementation of the Secure Hash Algorithm-1 (SHA-1), as specified in
 * FIPS 180-2.
 *
 * specification available on-line at
 * http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
 *
 * verification vectors available at
 * http://csrc.nist.gov/cryptval
 *
 * Digest should be an array of 5 values which will store the resulting
 * 160-bit digest.
 *
 * Message is the message which needs to be hashed, passed in as an array of
 * unsigned chars.  If the message length is not a multiple of 8 bits, the
 * bits of the last message are expected to be MSB-aligned.
 */
void sha1( __u32 *Digest, const __u8 *Message, __u64 MsgLenInBits );

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_SHA1_H
