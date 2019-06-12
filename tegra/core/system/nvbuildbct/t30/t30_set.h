/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 *  For setting the different parameters
 */


#ifndef INCLUDED_t30_SET_H
#define INCLUDED_t30_SET_H

#include "../nvbuildbct_config.h"
#include "../nvbuildbct.h"
#include "../parse.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Processes commands to set a bootloader.
 * @param Context The buildbact context
 * @param Filename Bootloader filename
 * @param LoadAddress load adress for the bootloader
 * @param EntryPoint the bootloader entry point for execution

 * @retval Returns the '0'
 */
int
t30SetBootLoader(BuildBctContext *Context,
                  NvU8 *Filename,
                  NvU32 LoadAddress,
                  NvU32 EntryPoint);

/**
 * Computation of the random aes block
 * @param Context The buildbact context
 */
void t30ComputeRandomAesBlock(BuildBctContext *Context);

/**
 * Sets the aes random block based on type
 * @param Context The buildbact context
 * @param value Aes Block Type
 * @param AesBlock pointer

 * @retval Returns the '0' or '1'
 */
int
t30SetRandomAesBlock(BuildBctContext *Context, NvU32 Value, NvU8* AesBlock);

/**
 * Sets sdmmc parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetSdmmcParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Nand parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetNandParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Nor parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetNorParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets SpiFlash parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetSpiFlashParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Sata parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetSataParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets  Sets an SDRAM parameter.
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetSdramParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Media emmc parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetOsMediaEmmcParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Media Nand Parameters
 * @param Context The buildbact context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetOsMediaNandParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * General handler for setting values in config files.
 * @param Context The buildbact context
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t30SetValue(BuildBctContext *Context,
                  ParseToken Token,
                  NvU32 Value);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_SET_H */

