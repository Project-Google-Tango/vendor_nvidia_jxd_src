/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_WIN32_H
#define INCLUDED_NVOS_WIN32_H

/* File read Buffer size in bytes */
#define LOG2_BUF_BYTES 14
#define FS_BUF_SIZE (1 << LOG2_BUF_BYTES) /* 4KB bytes */

#define DEBUG_NVOSFILE_CODE 0

/* Structure pointed by NvOsFileHandle in wince */
struct NvOsFileRec
{
    // File Handle created by Win32 File Create API in library
    HANDLE h;
    // Byte count of data already read from buffer when buffer is Valid.
    NvU32 ReadBytes;
    // Total number of data bytes that is valid in buffer 
    NvU32 EndOfBuf;
    // Byte offset of file pointer from beginning of file
    // irrespective of the validity of data in buffer.
    // It points to the actual position of the file pointer 
    // and not to the application viewed position in file.
    NvU64 FilePointerPosition;
    // Byte offset - EOF
    NvU64 EofOffset;
    // Stores the Seek location that need to set before  
    // there is a read or write
    NvU64 ShadowFilePointerPosition;
    // Array containing the buffered data read from file
    NvU8 Data[FS_BUF_SIZE];
    // Boolean flag if set NV_TRUE indicates some data remains in buffer 
    // that is already read from file but not passed to application
    // that opened the file for read. No buffered data remains 
    // in buffer when this flag is NV_FALSE.
    BOOL IsReadBuffValid;
    // Flag indicates if Shadow File pointer position is valid
    BOOL IsShadowValid;

    //Byte count of data written to the write buffer
    NvU32 BytesWriten;
    //Log2 of buffer size
    NvU32 BufferSizeLog2;
    // Boolean flag if set NV_TRUE indicates some data remains in buffer 
    // that is already written by application that opened the file in write mode.
    //  No buffered data remains  in buffer when this flag is NV_FALSE.
    BOOL IsWriteBuffValid;
    //Boolean flag if set NV_TRUE indicates write caching is enabled and any data
    //written by application will go through write cache.
    //No write caching when this flag is set to NV_FALSE
    BOOL IsWriteCacheEnabled;
#if DEBUG_NVOSFILE_CODE
    // Save file name as part of structure NvOsFileRec for Debugging
    NvU8 *MyFileName;
#endif
};

/* called by DllMain when a thread terminates to execute all of the registered
 * TLS terminator functions */
void NvOsTlsRunTerminatorsWin32(void);

#endif

