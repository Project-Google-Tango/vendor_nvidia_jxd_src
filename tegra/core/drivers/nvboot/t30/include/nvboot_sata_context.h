/**
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvboot_sata_context.h - Definitions for the SATA context structure.
 */

#ifndef INCLUDED_NVBOOT_SATA_CONTEXT_H
#define INCLUDED_NVBOOT_SATA_CONTEXT_H

#include "t30/nvboot_sata_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define WORD_TO_BYTES(x) x*4
#define BITS_TO_BYTES(x)     x/8

// Read data at 32MB and then send it back in the destination buffer
#define SDRAM_DATA_BUFFER_BASE    0x82000000

// Note: Command list has to be 1KB aligned, Command tables have to be 128 byte
//          aligned and FISes have to be 256 byte aligned.

// Sdram buffers base for SATA (= 24MB)- default value
#define SDRAM_SATA_BUFFERS_BASE    0x81800000

#define MAX_PRDT_ENTRIES                      0x1
// Need to configure the below macros based on size of  command lists/tables
// and FIS sizes and the number of commands and FISes that might ever be used
// at any given point of time.
#define MAX_COMMAND_LIST_SIZE NVBOOT_SATA_MAX_SUPPORTED_COMMANDS_IN_Q * \
                WORD_TO_BYTES(NVBOOT_SATA_CMD0_0_WORD_COUNT)

// Need to calculate the values of total command table size
#define MAX_COMMAND_TABLES_SIZE NVBOOT_SATA_MAX_SUPPORTED_COMMANDS_IN_Q * \
                WORD_TO_BYTES(NVBOOT_SATA_CFIS_0_WORD_COUNT + \
                NVBOOT_SATA_ATAPI_CMD_0_WORD_COUNT + \
                NVBOOT_SATA_RESERVED_0_WORD_COUNT + \
                (NVBOOT_SATA_PRDT0_0_WORD_COUNT * MAX_PRDT_ENTRIES))

// Need to calculate the values of FIS size
#define TOTAL_FIS_SIZE 0

/*
 * NvBootSataContext - The context structure for the Sata driver.
 * A pointer to this structure is passed into the driver's Init() routine.
 * This pointer is the only data that can be kept in a global variable for
 * later reference.
 * Don't know for now, what might be need storage in SATA context during
 * implementation.
 */
typedef struct NvBootSataContextRec
{
    // Currently used Sata mode.
    NvBootSataMode SataMode;

    // Currently used SATA transfer mode.
    NvBootSataTransferMode XferMode;

    // Indicates if sata controlelr is up
    NvBool IsSataInitialized;

    // Indicates the log2 of  page size, page size multiplier being
    // selected through boot dev config fuse. Page size is primarily for logical
    // organization of data on sata and not really pertains to the physical layer
    // organization. The transfer unit for SATA is 512byte sector and the data
    // on SATA device is also orgnanized in terms of 512 byte sectors.
    NvU32 PageSizeLog2;

    NvU32 PagesPerBlock;

    NvU32 SectorsPerPage;

    // Base for Cmd and FIS structures(in SDRAM) for AHCI dma
    // This will serve as the command list base
    NvU32 SataBuffersBase;

    // Command tables base
    NvU32 CmdTableBase;

    // FIS base
    NvU32 FisBase;

    // Base for data buffers(in SDRAM) for AHCI dma
    NvU32 DataBufferBase;

} NvBootSataContext;


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SATA_CONTEXT_H */
