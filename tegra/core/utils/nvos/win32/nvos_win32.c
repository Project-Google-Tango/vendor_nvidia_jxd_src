/*
 * Copyright (c) 2006-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"
#include "nvutil.h"
#include "nvos_debug.h"

#if NVOS_TRACE || NV_DEBUG
#undef NvOsExecAlloc
#undef NvOsExecFree
#undef NvOsSharedMemAlloc
#undef NvOsSharedMemHandleFromFd
#undef NvOsSharedMemGetFd
#undef NvOsSharedMemMap
#undef NvOsSharedMemUnmap
#undef NvOsSharedMemFree
#undef NvOsIntrMutexCreate
#undef NvOsIntrMutexLock
#undef NvOsIntrMutexUnlock
#undef NvOsIntrMutexDestroy
#endif

#if NVOS_IS_WINDOWS
/* get better thread functions (SwitchToThread) */
#define _WIN32_WINNT 0x0400
#endif

// Macro used to enable buffered file read in NvOs
#ifndef NVOS_BUFFERED_READ
#define NVOS_BUFFERED_READ 1
#endif

// TODO: Fix this.  For now, temporarily disable a warning 
// that makes the Visual Studio 2005 and/or VS2008 compiler unhappy.  
#ifdef _WIN32
//#pragma warning( disable : 4996 )
#endif

// Find out if secure string functions (xxx_s) are available
#if defined(__STDC_WANT_SECURE_LIB__) && __STDC_WANT_SECURE_LIB__
#define NVOS_SECURE_STRING_FUNCS 1
#else
#define NVOS_SECURE_STRING_FUNCS 0
#endif

#include <process.h>
#include <errno.h>
#include <windows.h>
#include "nvos_win32.h"

// Constant Mask for lsb 32 bits of 64 bits
#define MASK_32L_OF_64 0xFFFFFFFFULL
// Macro used for expression to create 64 bit value from two 32 bit values
#define NVOS_MACRO_CONVERT_32BIT_ARGS_TO_64BIT(hi, lo) \
            (NvU64)(((NvU64)((hi) & MASK_32L_OF_64) << 32) | \
            ((lo) & MASK_32L_OF_64))
// Macro to extract least significant 32bits of 64 bit value
#define NVOS_MACRO_GET_32LO_FROM_64(Value64) \
            ((NvU64)(Value64) & MASK_32L_OF_64)
// Macro to extract most significant 32bits of 64 bit value
#define NVOS_MACRO_GET_32HI_FROM_64(Value64) \
            (((NvU64)(Value64) & (MASK_32L_OF_64 << 32)) >> 32)

#define SECONDS_BTN_1601_1970 11644473600

// Maximum string size that can be allocated on stack.
// Size include '\0' at end of string.
#define MAX_WIDE_STR_ON_STACK 128

// Initialized by NvOsProcessAttachCommonInternal()
static LARGE_INTEGER gs_freqCount;

// Initialized by NvOsThreadCreateInternal()
static volatile NvU32 gs_TlsTerminatorKey = NVOS_INVALID_TLS_INDEX;

typedef struct NvOsMutexRec
{
    NvBool bCritSection;  // is this a critical section or mutex?
    CRITICAL_SECTION CritSection;
    HANDLE hMutex;
} NvOsMutex;

// Translate a Win32 error to an NvError.
// Success is indicated by returning 1.
// Failure is indicated by returning 0.
static int Win32ErrorToNvError(NvU32 win32Err, NvError *nvErr)
{
    switch (win32Err) {

        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            *nvErr = NvError_InsufficientMemory;
            break;

        default:
            // undecoded win32Err, return failure
            return 0;
    }

    // return success
    return 1;
}

void NvOsProcessAttachCommonInternal(void)
{
    // Initialize frequency count, which is later used by NvOsGetTimeUS()
    if (!QueryPerformanceFrequency(&gs_freqCount))
        NV_ASSERT(!"NvOsGetTimeUS: QueryPerformanceFrequency() failed");
}

void NvOsCloseHandle(NvOsResourceType type, void *h)
{
    if (!h)
        return;
    if (!CloseHandle(h))
        NV_ASSERT(!"NvOsCloseHandle: CloseHandle() failed");
    NvOsResourceFreed(type, h);
}

void NvOsVirtualFree(void *p)
{
    if (!p)
        return;
    if (!VirtualFree(p, 0, MEM_RELEASE))
        NV_ASSERT(!"NvOsVirtualFree: VirtualFree() failed");
    NvOsResourceFreed(NvOsResourceType_NvOsVirtualAlloc, p);
}

NvOsCodePage
NvOsStrGetSystemCodePage(void)
{
#if UNICODE
    return NvOsCodePage_Utf16;
#else
    return NvOsCodePage_Windows1252;
#endif
}

// convert a string into a wide char string.
//
// str    - the UTF-8 string to convert (only ASCII supported currently)
// len    - length of str
// buf    - wide char buffer to use (if big enough)
// bufLen - size of buf (bytes)
//
// Returns: NULL terminated wide char string.  This may or may not be <buf>.
// If NOT <buf> then this returned string must be freed with NvOsFree() when
// done. Returns NULL on error or if <str> is NULL.  The returned string is
// always NULL terminated.
//
static wchar_t *
NvOsStringToWStringLen(const char *str, NvU32 len, wchar_t *buf, NvU32 bufLen)
{
    size_t utf16Length;
    wchar_t *pNewString = NULL;

    if (!str)
        return NULL;

    utf16Length = NvUStrlConvertCodePage(NULL, 0, NvOsCodePage_Utf16,
                                         str, (size_t)len, NvOsCodePage_Utf8);

    if (buf && (utf16Length <= (size_t)(bufLen)))
        pNewString = buf;
    else if (utf16Length)
        pNewString = (wchar_t *)NvOsAllocInternal(utf16Length);

    if (!pNewString)
        return NULL;

    (void)NvUStrlConvertCodePage(pNewString, (size_t)utf16Length,
        NvOsCodePage_Utf16, str, (size_t)len, NvOsCodePage_Utf8);

    return pNewString;
}

// convert a string into a wide char string.
//
// str    - the NUL terminated UTF-8 string to convert
// len    - length of str
// buf    - wide char buffer to use (if big enough)
// bufLen - size of buf (bytes)
//
// Returns: the wide char string.  This may or may not be <buf>.  If NOT
// <buf> then this returned string must be freed with NvOsFree() when done.
// If <str> is NULL returns NULL.
//
// The returned string is always NULL terminated.
// The input string <str> need not be NULL terminated.
//
static wchar_t *
NvOsStringToWString(const char *str, wchar_t *buf, NvU32 bufLen)
{
    if (!str)
        return NULL;
    return NvOsStringToWStringLen(str, 0, buf, bufLen);
}

// convert a wide char string into a UTF-8 char string.
//
// buf    - buffer for result
// buflen - size of buf (in bytes)
// w      - NUL terminated wchar string
//
static NvError
NvOsWStringToString(char *buf, size_t buflen, const wchar_t *w)
{
    size_t utf8Length;

    utf8Length = NvUStrlConvertCodePage(NULL, 0, NvOsCodePage_Utf8,
                                        w, 0, NvOsCodePage_Utf16);

    if (utf8Length > buflen)
        return NvError_InsufficientMemory;

    (void)NvUStrlConvertCodePage(buf, buflen, NvOsCodePage_Utf8,
                                 w, 0, NvOsCodePage_Utf16);

    return NvSuccess;
}


/** NvOs implementation for Windows XP and WinCE */

void
NvOsBreakPoint(const char* file, NvU32 line, const char* condition)
{
    if (file != NULL)
    {
        if (condition != NULL)
        {
            NvOsDebugPrintf("\nAssert on %s:%d: %s\n", file, line, condition);
        }
        else
        {
            NvOsDebugPrintf("\nAssert on %s:%d\n", file, line);
        }
    }
    DebugBreak();
}

/** NvOs_PrintToFile - expands and writes a buffer to a windows file handle.
 */
static NvError
NvOs_PrintToFile(NvOsFileHandle file, const char *format, va_list ap)
{
    char s_buf[512];
    NvError err;
    NvU32 size = 512;
    char *tmp = s_buf;

    for(;;)
    {
        NvS32 n;

        /* try to expand the print args */
        n = NvOsVsnprintf(tmp, size, format, ap);
        if (n > -1 && n < (NvS32)size)
        {
            break;
        }

        /* free any previously allocated buffer */
        if ( tmp != s_buf )
            NvOsFree(tmp);

        /* double the buffer size and try again */
        size *= 2;
        tmp = NvOsAlloc(size);
        if (!tmp)
        {
            err = NvError_InsufficientMemory;
            goto clean;
        }
    }

    /* actually write to the file */
    size = NvOsStrlen(tmp);
    err = NvOsFwrite(file, tmp, size);

clean:
    if (tmp != s_buf) NvOsFree(tmp);
    return err;
}

NvError
NvOsFprintf(NvOsFileHandle stream, const char *format, ...)
{
    NvError err;
    va_list ap;

    va_start(ap, format);
    err = NvOs_PrintToFile(stream, format, ap);
    va_end(ap);

    return err;
}

NvError
NvOsVfprintf(NvOsFileHandle stream, const char *format, va_list ap)
{
    return NvOs_PrintToFile(stream, format, ap);
}

void
NvOsDebugString(const char *msg)
{
    wchar_t buf[MAX_WIDE_STR_ON_STACK], *t;
    
    t = NvOsStringToWString(msg, buf, sizeof(buf));
    if (t)
    {
        OutputDebugStringW(t);
        if (t != buf)
            NvOsFreeInternal(t);
    }
    else
    {
        OutputDebugStringW( L"MESSAGE DROPPED. LOW MEMORY\r\n" );
    }
}

void
NvOsDebugPrintf(const char *format, ...)
{
    va_list ap;
    char msg[MAX_WIDE_STR_ON_STACK * 2];

    va_start(ap, format);
    (void)NvOsVsnprintf(msg, sizeof(msg)-1, format, ap);
    // always zero-terminate in case the formatted string would not fit
    msg[sizeof(msg)-1] = '\0';
    va_end(ap);

    NvOsDebugString(msg);
}

void
NvOsDebugVprintf(const char *format, va_list ap)
{
    char msg[512];

    NvOsVsnprintf(msg, sizeof(msg)-1, format, ap);
    // always zero-terminate in case the formated string would not fit
    msg[sizeof(msg)-1] = '\0';

    NvOsDebugString(msg);
}

// DebugPrintf version returning number of characters printed
NvS32
NvOsDebugNprintf(const char *format, ...)
{
    va_list ap;

    char msg[MAX_WIDE_STR_ON_STACK * 2];
    int msgSize = MAX_WIDE_STR_ON_STACK * 2;
    int n;

    va_start(ap, format);
    n = NvOsVsnprintf(msg, msgSize, format, ap);
    // output longer than msgSize + null termination, crop it
    if ((n < 0) || (n == msgSize))
    {
        n = msgSize - 1;
        msg[n] = 0;
    }
    va_end(ap);

    NvOsDebugString(msg);
    return n;
}

size_t
NvOsStrlen(const char *s)
{
    NV_ASSERT(s);
    return strlen(s);
}

int
NvOsStrcmp(const char *s1, const char *s2)
{
    NV_ASSERT(s1);
    NV_ASSERT(s2);

    return strcmp(s1, s2);
}

int
NvOsStrncmp(const char *s1, const char *s2, size_t size)
{
    NV_ASSERT(s1);
    NV_ASSERT(s2);

    return strncmp(s1, s2, size);
}

void NvOsMemcpy(void *dest, const void *src, size_t size)
{
    (void)memcpy(dest, src, size);
}

int
NvOsMemcmp(const void *s1, const void *s2, size_t size)
{
    NV_ASSERT(s1);
    NV_ASSERT(s2);

    return memcmp(s1, s2, size);
}

void
NvOsMemset(void *s, NvU8 c, size_t size)
{
    NV_ASSERT(s);

    (void)memset(s, (int)c, size);
}

void
NvOsMemmove(void *dest, const void *src, size_t size)
{
    NV_ASSERT(dest);
    NV_ASSERT(src);

    (void)memmove(dest, src, size);
}

static BOOL FlushWriteBuffer(NvOsFileHandle stream)
{
    DWORD bytes;
    BOOL b;
    if((stream->IsWriteCacheEnabled == NV_TRUE) &&(stream->IsWriteBuffValid == NV_TRUE))
    {
        b = WriteFile(stream->h, stream->Data, (DWORD)stream->BytesWriten, &bytes, NULL);
        if (!b)
            return b;
        stream->IsWriteBuffValid = NV_FALSE;
        stream->BytesWriten = 0;
        // move the file pointer forward by bytes written
        stream->FilePointerPosition += (NvU64)bytes;
        // In case after write the size of the file
        // increases beyond saved eof update offset of eof
        if (stream->FilePointerPosition > stream->EofOffset)
        {
            stream->EofOffset = stream->FilePointerPosition;
        }
    }
    return NV_TRUE;
}

NvError
NvOsFopenInternal(
    const char *path,
    NvU32 flags,
    NvOsFileHandle *file )
{
    NvError ret;
    DWORD access;
    DWORD disp;
    DWORD share;
    DWORD attrib = FILE_ATTRIBUTE_NORMAL;
    HANDLE handle;
    NvU32 len;
    wchar_t buf[MAX_WIDE_STR_ON_STACK], *wpath=NULL;
    DWORD r;
    LONG high = 0;

    NV_ASSERT(path);
    NV_ASSERT(file);

    (*file) = (NvOsFileHandle)(NvOsAlloc(sizeof(struct NvOsFileRec)));
    if (!(*file))
    {
        ret = NvError_InsufficientMemory;
        goto clean;
    }
    (*file)->h = 0;
    (*file)->IsWriteCacheEnabled = NV_FALSE;

    switch (flags) {
    case NVOS_OPEN_READ:
        access = GENERIC_READ;
        disp = OPEN_EXISTING;
        break;
    case NVOS_OPEN_WRITE:
        access = GENERIC_WRITE;
        disp = TRUNCATE_EXISTING;
        (*file)->IsWriteCacheEnabled = NV_TRUE;
        break;
    case NVOS_OPEN_APPEND:
    case NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        access = GENERIC_WRITE;
        disp = OPEN_EXISTING;
        (*file)->IsWriteCacheEnabled = NV_TRUE;
        break;
    case NVOS_OPEN_READ | NVOS_OPEN_WRITE:
    case NVOS_OPEN_READ | NVOS_OPEN_APPEND:
    case NVOS_OPEN_READ | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        access = GENERIC_READ | GENERIC_WRITE;
        disp = OPEN_EXISTING;
        break;
    case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE:
        access = GENERIC_WRITE;
        disp = CREATE_ALWAYS;
        (*file)->IsWriteCacheEnabled = NV_TRUE;
        break;
    case NVOS_OPEN_CREATE | NVOS_OPEN_APPEND:
    case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        access = GENERIC_WRITE;
        disp = OPEN_ALWAYS;
        (*file)->IsWriteCacheEnabled = NV_TRUE;
        break;
    case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE:
        access = GENERIC_READ | GENERIC_WRITE;
        disp = CREATE_ALWAYS;
        break;
    case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_APPEND:
    case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        access = GENERIC_READ | GENERIC_WRITE;
        disp = OPEN_ALWAYS;
        break;
    default:
        ret = NvError_BadParameter;
        goto clean;
    }

    /* enable all sharing attributes */
    share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

    /* check for directory open */
    len = NvOsStrlen(path);
    if ((path[ len - 1 ] == '\\') ||
        (path[ len - 1 ] == '.'))
    {
        attrib |= FILE_FLAG_BACKUP_SEMANTICS;
    }
#if DEBUG_NVOSFILE_CODE
    // Save the file name as part of structure NvOsFileRec
    (*file)->MyFileName = NvOsAlloc(sizeof(char)*(len+1));
    NvOsStrncpy((*file)->MyFileName, path, (len+1));
    NvOsDebugPrintf(" FH: |0x%x|, filename: |%s|\n", (unsigned int)(*file), (*file)->MyFileName);
#endif
    wpath = NvOsStringToWString(path, buf, sizeof(buf));
    if (!wpath)
    {
        ret = NvError_InsufficientMemory;
        goto clean;
    }

    handle = CreateFileW(wpath, access, share, NULL, disp, attrib, NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();

        /* TODO: Convert error values with more granlarity (see nvos_linux.c)
         *       and document the return codes which are consistent across OS
         *       in nvos.h
         */
        switch(err)
        {
            case ERROR_FILE_NOT_FOUND:
                ret = NvError_FileNotFound;
                break;
            case ERROR_ACCESS_DENIED:
                ret = NvError_AccessDenied;
                break;
            default:
                ret = NvError_BadParameter;
                break;
        }

        goto clean;
    }

    NvOsResourceAllocated(NvOsResourceType_NvOsFile, handle);

    (*file)->IsReadBuffValid = NV_FALSE;
    (*file)->IsShadowValid = NV_FALSE;
    (*file)->ReadBytes = 0;
    (*file)->EndOfBuf = FS_BUF_SIZE;
    // After File Open file pointer is at offset 0 from beginning
    (*file)->FilePointerPosition = (NvU64)0;
    (*file)->ShadowFilePointerPosition = (NvU64)0;
    (*file)->h = handle;

    (*file)->BytesWriten= 0;
    (*file)->BufferSizeLog2 = LOG2_BUF_BYTES;
    (*file)->IsWriteBuffValid = NV_FALSE;

    // Get byte offset EOF initially
    r = GetFileSize((*file)->h, &high);
    (*file)->EofOffset = (((NvU64)high << 32) | ((NvU64)r));

    ret = NvSuccess;

clean:
    if (wpath != buf)
        NvOsFreeInternal(wpath);
    if (ret != NvSuccess) {
        NvOsFree((void *)(*file));
        *file = (NvOsFileHandle)NULL;
    }
    return ret;
}

void NvOsFcloseInternal(NvOsFileHandle stream)
{
    if (stream != NULL)
    {
        if (stream->IsWriteCacheEnabled == NV_TRUE)
            FlushWriteBuffer(stream);
        NvOsCloseHandle(NvOsResourceType_NvOsFile, stream->h);
#if DEBUG_NVOSFILE_CODE
        NvOsFree(stream->MyFileName);
        stream->MyFileName = NULL;
#endif
        NvOsFree((void *)stream);
        stream = NULL;
    }
}

// Function is called only when buffered File data is valid.
static NvError
FuncCorrectBufReadFP(NvOsFileHandle file)
{
    NvS64 CorrectionFP;
    DWORD r;
    LONG low;
    LONG high;
    DWORD w;
    if (file->IsShadowValid)
    {
        // Sync the file pointer to the shadow location obtained from
        // immediately preceeding file operation - Seek
        CorrectionFP = file->ShadowFilePointerPosition;
        w = FILE_BEGIN;
        file->IsShadowValid = NV_FALSE;
        // move the file pointer to last seek position indicated by Shadow
        // file position
        file->FilePointerPosition = file->ShadowFilePointerPosition;
    }
    else
    {
        // move file pointer back - from buffer end to where app last read
        // Application has read up to position of stream->ReadBytes in buffer.
        CorrectionFP = ((NvS64)file->EndOfBuf - (NvS64)file->ReadBytes);
        CorrectionFP = -CorrectionFP;
        w = FILE_CURRENT;
        // move the file pointer to last read position from buffer
        file->FilePointerPosition = (NvU64)((NvS64)file->FilePointerPosition +
                                        CorrectionFP);
    }
    // In case of write if file pointer was beyond EOF before write
    // update saved EOF
    if (file->FilePointerPosition > file->EofOffset)
    {
        file->EofOffset = file->FilePointerPosition;
    }
    // Invalidate the buffered data
    file->IsReadBuffValid = NV_FALSE;
    // Move the file pointer position as needed
    low = (LONG)NVOS_MACRO_GET_32LO_FROM_64(CorrectionFP);
    high = (LONG)NVOS_MACRO_GET_32HI_FROM_64(CorrectionFP);
    r = SetFilePointer(file->h, low, &high, w);
    if ((r == INVALID_SET_FILE_POINTER) &&
        (GetLastError() != NO_ERROR))
    {
        return NvError_FileOperationFailed;
    }
    return NvSuccess;
}

static NvError BufWriteFile(NvOsFileHandle stream, NvU8 *ptr, size_t size)
{
    DWORD bytes;
    NvS32 BytesToWrite = size;
    while(BytesToWrite)
    {
        //check if the write buffer has some data in it.
        if(stream->IsWriteBuffValid == NV_TRUE)
        {
            //we have some valid data in the write buffer,
            //check if bytes to write can fit in the remaining buffer
            NvS32 RemainingBytes = FS_BUF_SIZE - stream->BytesWriten;
            if(BytesToWrite < RemainingBytes)
            {
                //YES. we are good to fit in the buffer, copy these bytes to the write buffer
                NvOsMemcpy(&stream->Data[stream->BytesWriten], ptr, BytesToWrite);
                stream->BytesWriten += BytesToWrite;
                stream->IsWriteBuffValid = NV_TRUE;
                BytesToWrite = 0;
            }
            else
            {
                //We have more data than the write buffer can accommodate,
                NvOsMemcpy(&stream->Data[stream->BytesWriten], ptr, RemainingBytes);
                stream->BytesWriten += RemainingBytes;
                BytesToWrite -= RemainingBytes;
                ptr = ptr + RemainingBytes;
            }
            //if buffer is full, write it
            if(stream->BytesWriten == FS_BUF_SIZE)
            {
               if(!WriteFile(stream->h, stream->Data, (DWORD)stream->BytesWriten, &bytes, NULL))
                    return NvError_FileWriteFailed;
                stream->IsWriteBuffValid = NV_FALSE;
                stream->BytesWriten = 0;
                // move the file pointer forward by bytes written
                stream->FilePointerPosition += (NvU64)bytes;
                // In case after write the size of the file
                // increases beyond saved eof update offset of eof
                if (stream->FilePointerPosition > stream->EofOffset)
                {
                    stream->EofOffset = stream->FilePointerPosition;
                }
            }
        }
        else
        {
            //write buffer is empty,
            //check if the bytes to write will fit in the write buffer or not
            if(BytesToWrite < FS_BUF_SIZE)
            {
                //YES, we have enough buffer to cache the requsted write.
                NvOsMemcpy(stream->Data, ptr, BytesToWrite);
                stream->BytesWriten = BytesToWrite;
                stream->IsWriteBuffValid = NV_TRUE;
                BytesToWrite = 0;
            }
            else
            {
                //look how many multiples of FS_BUF_SIZE exists in bytes to write,
                //write those bytes to FS.
                NvS32 AlignedWriteBuffBytes = (BytesToWrite>>stream->BufferSizeLog2)
                                                                    << stream->BufferSizeLog2;
                if(!WriteFile(stream->h, ptr, (DWORD)AlignedWriteBuffBytes, &bytes, NULL))
                    return NvError_FileWriteFailed;
                stream->IsWriteBuffValid = NV_FALSE;
                stream->BytesWriten = 0;
                BytesToWrite -= bytes;
                ptr = ptr + bytes;
                // move the file pointer forward by bytes written
                stream->FilePointerPosition += (NvU64)bytes;
                // In case after write the size of the file
                // increases beyond saved eof update offset of eof
                if (stream->FilePointerPosition > stream->EofOffset)
                {
                    stream->EofOffset = stream->FilePointerPosition;
                }
            }
        }
    }
    return NvSuccess;
}
NvError
NvOsFwriteInternal(NvOsFileHandle stream, const void *ptr, size_t size)
{
    DWORD bytes;
    BOOL b;
    NvError err;

    NV_ASSERT(stream);
    NV_ASSERT(ptr);

    // Invalidate any buffered data
    if (stream->IsWriteCacheEnabled == NV_FALSE)
    {
        // We have either Read buffering or write buffering
        if ((stream->IsReadBuffValid == NV_TRUE) && (stream->ReadBytes < stream->EndOfBuf))
        {
            NV_ASSERT(stream->IsWriteCacheEnabled == NV_FALSE);
            // Move File pointer to location application last accessed
            err = FuncCorrectBufReadFP(stream);
            if (err != NvSuccess)
            {
                return err;
            }
            // In case of write if file pointer moved beyond EOF, update saved EOF
            // At end of this function accomodating FP movement due to write
        }
    }
    else //if(stream->IsWriteCacheEnabled == NV_TRUE)
    {
        return BufWriteFile(stream, (NvU8*)ptr, size);
    }
    b = WriteFile(stream->h, ptr, (DWORD)size, &bytes, NULL);
    
    // move the file pointer forward by bytes written
    stream->FilePointerPosition += (NvU64)bytes;
    // In case after write the size of the file
    // increases beyond saved eof update offset of eof
    if (stream->FilePointerPosition > stream->EofOffset)
    {
        stream->EofOffset = stream->FilePointerPosition;
    }
    if (!b)
    {
        // Print write error
        NV_DEBUG_PRINTF((" Write Error: GetLastError=%u \n", GetLastError()));
        return NvError_FileWriteFailed;
    }

    return NvSuccess;
}

#if NVOS_BUFFERED_READ
static BOOL BufReadFile(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes)
{
    BOOL b = NV_TRUE;
    NvU32 Len = 0;
    NvU32 BufBytes = 0;
    NvU32 ResidueSz = 0, TmpVal = 0;
    NvU32 ReadSz = size;
    NvError err;

    if ((stream->IsWriteBuffValid == NV_FALSE) && (
        (stream->IsReadBuffValid == NV_TRUE) &&
        (stream->ReadBytes < stream->EndOfBuf)))
    {
        // If write buffering is enabled, we have un-buffered read

        // When buffered data from file is available,
        // and some seek operation is pending
        // move File pointer if Seek performed last outside buffer limits
        if (stream->IsShadowValid)
        {
            err = FuncCorrectBufReadFP(stream);
            if (err != NvSuccess)
            {
                return err;
            }
            // File pointer cannot move past EOF with Read.
            // SetEndOfFile or Write API needs to be called
        }
    }
    if ((stream->IsWriteBuffValid == NV_FALSE) && (size < FS_BUF_SIZE))
    {
        // If write buffering is enabled, we do not enable buffered read

        // Only when reading small sized data we use local buffering
        // Small size read read is assumed to be less than FREAD_BUF_SIZE,
        // and is typically less than 512bytes read.
        if ((stream->IsReadBuffValid == NV_TRUE) && (stream->ReadBytes < stream->EndOfBuf))
        {
            ResidueSz = stream->EndOfBuf - stream->ReadBytes;
            if (ResidueSz >= size) {
                // bytes to read are in buffer
                NvOsMemcpy(ptr, &stream->Data[stream->ReadBytes], size);
                stream->ReadBytes += size;
                if (stream->ReadBytes == stream->EndOfBuf)
                {
                    // This takes care of seek check to see if buffer data is still valid
                    stream->IsReadBuffValid = NV_FALSE;
                }
                Len = size;
            }
            else
            {
                NvOsMemcpy(ptr, &stream->Data[stream->ReadBytes], ResidueSz);
                Len = ResidueSz;
                b = ReadFile(stream->h, &stream->Data[0], FS_BUF_SIZE, &BufBytes, NULL);
                // update the end of buffer pointer to match EOF
                if (b)
                {
                    // move the file pointer forward by non-buffered bytes read
                    stream->FilePointerPosition += (NvU64)BufBytes;
                    TmpVal = (size - ResidueSz);
                    // BufBytes < FREAD_BUF_SIZE when EOF reached
                    stream->EndOfBuf = BufBytes;
                    if (TmpVal < stream->EndOfBuf)
                    {
                        // Data remains in buffer
                        stream->IsReadBuffValid = NV_TRUE;
                        stream->ReadBytes = TmpVal;
                    }
                    else
                    {
                        // Data requested is at least till end of file
                        // TmpVal could be < stream->EndOfBuf
                        // Invalidate buffer since no data remains buffered
                        stream->IsReadBuffValid = NV_FALSE;
                        TmpVal = stream->EndOfBuf;
                    }

                    NvOsMemcpy(((NvU8 *)ptr + ResidueSz), &stream->Data[0], TmpVal);
                    Len += TmpVal;
                }
                else
                {
                    // ReadFile call failed
                    stream->IsReadBuffValid = NV_FALSE;
                }
            }
        }
        else
        {
            // Read FREAD_BUF_SIZE equivalent data
            b = ReadFile(stream->h, &stream->Data[0], FS_BUF_SIZE, &BufBytes, NULL);
            // update the end of buffer pointer to match EOF
            if (b)
            {
                // move the file pointer forward by non-buffered bytes read
                stream->FilePointerPosition += (NvU64)BufBytes;
                stream->EndOfBuf = BufBytes;
                if (size < stream->EndOfBuf)
                {
                    // Data remains in buffer
                    stream->IsReadBuffValid = NV_TRUE;
                    stream->ReadBytes = size;
                    Len = size;
                }
                else
                {
                    // When amount of data requested is till end of file
                    // Len could be < stream->EndOfBuf
                    // no data remains buffered
                    stream->IsReadBuffValid = NV_FALSE;
                    Len = stream->EndOfBuf;
                }
                NvOsMemcpy(ptr, &stream->Data[0], Len);
            }
            else
            {
                // ReadFile call failed
                stream->IsReadBuffValid = NV_FALSE;
            }
        }
    }
    else
    {
        // If write buffering is enabled, we do not enable buffered read
        if ((stream->IsWriteBuffValid == NV_FALSE) &&
            (stream->IsReadBuffValid == NV_TRUE))
        {
            // Some buffered data already available
            ResidueSz = (stream->EndOfBuf - stream->ReadBytes);
            NvOsMemcpy(ptr, &stream->Data[stream->ReadBytes], ResidueSz);
            Len = ResidueSz;
            // Remaining data must be non-zero, since
            // ((size >= FREAD_BUF_SIZE) && (stream->ReadBytes >= 0))
            ReadSz = (size - ResidueSz);
            stream->IsReadBuffValid = NV_FALSE;
        }
        // No buffering for unread data, when bigger sized reads are done
        b = ReadFile(stream->h, ((NvU8 *)ptr + ResidueSz), ReadSz, &TmpVal, NULL);
        // move the file pointer forward by non-buffered bytes read
        stream->FilePointerPosition += (NvU64)TmpVal;
        Len += TmpVal;
    }

    *bytes = Len;
    return b;
}
#endif

NvError
NvOsFreadInternal(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec)
{
    NvU32 len;

    NV_ASSERT(stream);
    NV_ASSERT(ptr);
    NV_ASSERT(stream->IsWriteCacheEnabled == NV_FALSE);
#if DEBUG_NVOSFILE_CODE
    if (!size)
    {
        // Check for zero bytes read
        if (stream->MyFileName)
            NvOsDebugPrintf(" Error: Read zero size from file |%s|, FH: |0x%x|\n", stream->MyFileName, stream);
        else
            NvOsDebugPrintf(" Error: Null file name, FH: |0x%x|\n", stream);
    }
#endif

#if NVOS_BUFFERED_READ
    if (!BufReadFile(stream, ptr, size, &len))
#else
    if (!ReadFile(stream->h, ptr, size, &len, NULL))
#endif
    {
        // Print read error
        NvOsDebugPrintf(" Read Error: GetLastError=%u \n", GetLastError());
        return NvError_FileReadFailed;
    }

    if (bytes)
        *bytes = len;

    // ReadFile called inside BufReadFile does not check EOF condition
    return (size && !len) ? NvError_EndOfFile : NvSuccess;
}

NvError
NvOsFseekInternal(NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence)
{
    DWORD w;
    DWORD r;
    LONG low;
    LONG high;
    NvU64 NewFilePointer;

    NV_ASSERT(file);

    if (file->IsWriteCacheEnabled == NV_TRUE)
        FlushWriteBuffer(file);

    if (file->IsReadBuffValid == NV_TRUE) // condition implies some data still remains to read from buffer
    {
        // Calculate the effective position to seek to
        switch(whence)
        {
            case NvOsSeek_Set:
                NewFilePointer = offset;
                break;
            case NvOsSeek_End:
                // EndOffset will never be more than 63bits
                NewFilePointer = (NvU64)((NvS64)file->EofOffset + offset);
                break;
            case NvOsSeek_Cur:
            default:
                if (file->IsShadowValid)
                {
                    // New position is wrt Shadow file position
                    NewFilePointer = (NvU64)((NvS64)file->ShadowFilePointerPosition +
                                                                    offset);
                }
                else
                {
                    NewFilePointer = (NvU64)((NvS64)(file->FilePointerPosition -
                                        (NvU64)file->EndOfBuf +
                                        (NvU64)file->ReadBytes) +
                                        offset);
                }
                break;
        }
        // Sanity check that not seek past limits of file
        // Check if new position is after the file start
        if ((NvS64)NewFilePointer < 0)
        {
            return NvError_FileOperationFailed;
        }
        // NewFilePointer could move past end of file with
        // write call or SetEndOfFile API

        // FilePointerPosition will correspond to the end of buffer
        if ((NewFilePointer < file->FilePointerPosition) &&
            (NewFilePointer >= (file->FilePointerPosition -
                                (NvU64)file->EndOfBuf)))
        {
            // Buffer data still valid as seek within buffer
            // Update position in buffer
            file->ReadBytes = (NvU32)(NewFilePointer -
                                (file->FilePointerPosition -
                                (NvU64)file->EndOfBuf));
            if (file->IsShadowValid)
            {
                // new position is again in buffered region, do not need
                // shadow file position now
                file->IsShadowValid = NV_FALSE;
            }
            // Seek done in buffered data
            return NvSuccess;
        }
        else if (file->ReadBytes < file->EndOfBuf)
        {
            // Update Shadow FP used to track pending seek operation
            file->IsShadowValid = NV_TRUE;
            file->ShadowFilePointerPosition = NewFilePointer;
            // If pointer has moved beyond EOF, we correct Eof and
            // file pointer from the shadow position in function
            // FuncCorrectBufReadFP()
            return NvSuccess;
        }
    }

    switch (whence)
    {
    case NvOsSeek_Set: w = FILE_BEGIN; break;
    case NvOsSeek_Cur: w = FILE_CURRENT; break;
    case NvOsSeek_End: w = FILE_END; break;
    default:
        return NvError_BadParameter;
    }
    low = (LONG)NVOS_MACRO_GET_32LO_FROM_64(offset);
    high = (LONG)NVOS_MACRO_GET_32HI_FROM_64(offset);

    r = SetFilePointer(file->h, low, &high, w);
    if ((r == INVALID_SET_FILE_POINTER) &&
        (GetLastError() != NO_ERROR))
    {
        return NvError_FileOperationFailed;
    }
    // Set file pointer position depending on the seek.
    file->FilePointerPosition = NVOS_MACRO_CONVERT_32BIT_ARGS_TO_64BIT(high, r);
    // In case of write if file pointer was beyond EOF before write
    // update saved EOF
    if (file->FilePointerPosition > file->EofOffset)
    {
        file->EofOffset = file->FilePointerPosition;
    }

    return NvSuccess;
}

NvError
NvOsFtellInternal(NvOsFileHandle file, NvU64 *position)
{
    LONG high;
    LONG low;

    NV_ASSERT(file);
    NV_ASSERT(position);

    if (file->IsWriteCacheEnabled == NV_TRUE)
        FlushWriteBuffer(file);
    // In case of buffered read, we have the file offset available already
    if (file->IsReadBuffValid)
    {
        if (file->IsShadowValid)
        {
            // Return the saved location in shadow file pointer position
            *position = file->ShadowFilePointerPosition;
        }
        else
        {
            // Calculate the offset and return it
            *position = (file->FilePointerPosition - (NvU64)file->EndOfBuf
                        + (NvU64)file->ReadBytes);
        }
        return NvSuccess;
    }

    high = 0;
    low = SetFilePointer(file->h, 0, &high, FILE_CURRENT);
    if ((low == INVALID_SET_FILE_POINTER) &&
        (GetLastError() != NO_ERROR))
    {
        return NvError_FileOperationFailed;
    }

    *position = NVOS_MACRO_CONVERT_32BIT_ARGS_TO_64BIT(high, low);
    return NvSuccess;
}

NvError
NvOsFstatInternal(NvOsFileHandle file, NvOsStatType *s)
{
    NvError err = NvError_FileOperationFailed;
    BOOL b;

    BY_HANDLE_FILE_INFORMATION info;

    NV_ASSERT(file);
    NV_ASSERT(s);

    b = GetFileInformationByHandle(file->h, &info);
    if (!b)
    {
        goto fail;
    }

    s->size = NVOS_MACRO_CONVERT_32BIT_ARGS_TO_64BIT(info.nFileSizeHigh,
        info.nFileSizeLow);
    s->mtime = NVOS_MACRO_CONVERT_32BIT_ARGS_TO_64BIT(info.ftLastWriteTime.dwLowDateTime,
        info.ftLastWriteTime.dwHighDateTime);
    s->type = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
        NvOsFileType_Directory : NvOsFileType_File;

    err = NvSuccess;

fail:
    return err;
}

NvError
NvOsFflushInternal(NvOsFileHandle stream)
{
    BOOL b;

    NV_ASSERT(stream);

    b = FlushFileBuffers(stream->h);
    if (!b)
    {
        return NvError_FileOperationFailed;
    }

    return NvSuccess;
}

NvError
NvOsFsyncInternal(NvOsFileHandle stream)
{
    /* no-op */
    NV_ASSERT(stream);
    return NvSuccess;
}

NvError
NvOsFremoveInternal(const char *fileName)
{
    BOOL b;
#ifdef UNICODE
    char *tmp;
    wchar_t *wTempFile = NULL;    
    NvU32 utf16Length=0;

    NV_ASSERT(fileName);

    // convert from char to wide char
    // find the length of the string
    utf16Length = NvUStrlConvertCodePage(NULL, 0, NvOsCodePage_Utf16,
        fileName, (size_t)NvOsStrlen(fileName),NvOsCodePage_Utf8);

    // Allocate memory based on the string length
    tmp = (char *)NvOsAlloc((sizeof(NvU8) * utf16Length) + sizeof(wchar_t) - 1);
    wTempFile = (wchar_t *)((size_t)(tmp + sizeof(wchar_t) - 1) & ~(sizeof(wchar_t) - 1));

    // convert to wide character
    (void)NvUStrlConvertCodePage(wTempFile, (size_t)utf16Length, NvOsCodePage_Utf16,
        fileName, (size_t)NvOsStrlen(fileName),NvOsCodePage_Utf8);

    // Delete the file.
    // Note: Delete accepts argument in wide char
    b = DeleteFile(wTempFile);

    NvOsFree(tmp);
    wTempFile = NULL;
#else
    b = DeleteFile(fileName);
#endif

    if (!b)
        return NvError_FileOperationFailed;
    else
        return NvSuccess;
}

NvError
NvOsFlockInternal(NvOsFileHandle stream, NvOsFlockType type)
{
    return NvError_NotImplemented;
}

NvError
NvOsFtruncateInternal(NvOsFileHandle stream, NvU64 length)
{
    return NvError_NotImplemented;
}

typedef enum NvOsDirStateEnum {
    NvOsDirState_Unknown,
    NvOsDirState_HaveFirstFile,
    NvOsDirState_HavePrevFile,
    NvOsDirState_NoMoreFiles,
    NvOsDirState_Error,
    NvOsDirState_Force32            = 0x7fffffff
} NvOsDirState;
typedef struct NvOsDirRec
{
    WIN32_FIND_DATAW data;
    HANDLE handle;
    NvOsDirState    state;
    //NvBool FirstRead;
    //NvU32 err;
} NvOsDir;

NvError
NvOsOpendirInternal(const char *path, NvOsDirHandle *dir)
{
    NvError err = NvSuccess;
    NvOsDir *d = 0;
    wchar_t *wpath = 0;
    wchar_t buf[MAX_WIDE_STR_ON_STACK];
    char *p = 0;
    NvU32 len;
    NvS32 n;

    NV_ASSERT(path);
    NV_ASSERT(dir);


    // check to see if it is really a directory
    {
        WIN32_FILE_ATTRIBUTE_DATA wfattr;
        BOOL b = GetFileAttributesEx(path, GetFileExInfoStandard, &wfattr);
        if (!b)
            return NvError_FileOperationFailed;
        if (((NvU64)wfattr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            return NvError_BadParameter;
    }

    d = NvOsAlloc(sizeof(NvOsDir));
    if (!d)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    len = NvOsStrlen(path);
    len += 3;
    p = NvOsAlloc(len);
    if (!p)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    n = NvOsSnprintf(p, len, "%s\\*", path);
    if (!(n > -1 && n < (NvS32)len))
    {
        /* not enough room in the buffer */
        err = NvError_FileOperationFailed;
        goto fail;
    }
    wpath = NvOsStringToWString(p, buf, sizeof(buf));
    if (!wpath)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }
    d->handle = FindFirstFileW(wpath, &d->data);
    d->state = NvOsDirState_HaveFirstFile;
    if (d->handle == INVALID_HANDLE_VALUE)
    {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_NO_MORE_FILES) {
            d->state = NvOsDirState_NoMoreFiles;
        } else {
            err = NvError_DirOperationFailed;
            goto fail;
        }
    }
    *dir = d;
    goto success;

fail:
    NvOsFree(d);

success:
    NvOsFree(p);
    if (wpath != buf)
    {
        NvOsFreeInternal(wpath);
    }
    return err;
}

NvError
NvOsReaddirInternal(NvOsDirHandle d, char *name, size_t size)
{
    NV_ASSERT(d);
    NV_ASSERT(name);
    NV_ASSERT(size);

    switch (d->state) {
        case NvOsDirState_HaveFirstFile:
            d->state = NvOsDirState_HavePrevFile;
            break;

        case NvOsDirState_HavePrevFile:
            if (!FindNextFileW(d->handle, &d->data))
            {
                if (GetLastError() == ERROR_NO_MORE_FILES)
                {
                    d->state = NvOsDirState_NoMoreFiles;
                    return NvError_EndOfDirList;
                }
                d->state = NvOsDirState_Error;
                return NvError_DirOperationFailed;
            }
            break;

        case NvOsDirState_NoMoreFiles:
            return NvError_EndOfDirList;

        case NvOsDirState_Error:
        default:
            return NvError_DirOperationFailed;
    }
    d->data.cFileName[MAX_PATH-1]=0;
    return NvOsWStringToString(name, size, d->data.cFileName);
}

void NvOsClosedirInternal(NvOsDirHandle d)
{
    if (d && d->handle != INVALID_HANDLE_VALUE)
        FindClose(d->handle);
    NvOsFree(d);
}

NvError
NvOsMkdirInternal(char *dirname)
{
    return NvError_NotImplemented;
}

NvError
NvOsSetConfigU32Internal(const char *name, NvU32 value)
{
    return NvError_NotSupported;
}

NvError
NvOsSetConfigStringInternal(const char *name, const char *value)
{
    return NvError_NotSupported;
}

NvError
NvOsSetSysConfigStringInternal(const char *name, const char *value)
{
    return NvOsSetConfigStringInternal(name, value);
}

NvError
NvOsGetConfigU32Internal(const char *name, NvU32 *value)
{
    return NvError_NotImplemented;
}

NvError
NvOsGetConfigStringInternal(const char *name, char *value, NvU32 size)
{
    char *env = getenv(name);
    if (env == NULL)
        return NvError_ConfigVarNotFound;

    (void)strncpy(value, env, size);
    value[size-1] = 0;
    return NvSuccess;
}

NvError
NvOsGetSysConfigStringInternal(const char *name, char *value, NvU32 size)
{
    return NvOsGetConfigStringInternal(name, value, size);
}

NvError
NvOsConfigSetStateInternal(int stateId, const char *name, const char *value,
        int valueSize, int flags)
{
    return NvError_NotSupported;
}

NvError
NvOsConfigGetStateInternal(int stateId, const char *name, char *value,
        int valueSize, int flags)
{
    return NvError_NotSupported;
}

NvError
NvOsConfigQueryStateInternal(int stateId, const char **ppKeys, int *pNumKeys,
        int maxKeySize)
{
    return NvError_NotSupported;
}

#if NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE
static void* gs_AllocList[NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE] = { 0 };

void *NvOsAllocInternal(size_t size)
{
    void* ptr = malloc(size);
    if(ptr)
    {
        NvU32 i;
        for(i = 0; i < NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE; i++)
        {
            if(!gs_AllocList[i])
            {
                gs_AllocList[i] = ptr;
                break;
            }
        }

        // Set i value with a break point set to break on allocation to
        // help localize an unfreed allocation.
        if(i == 0)
        {
            return ptr;
        }
    }

    return ptr;
}

void *NvOsReallocInternal(void *ptr, size_t size)
{
    NvU32 i;
    void* newPtr = realloc(ptr, size);
    if(newPtr)
    {
        for(i = 0; i < NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE; i++)
        {
            if(gs_AllocList[i] == ptr)
            {
                gs_AllocList[i] = newPtr;
                break;
            }
        }
    }

    return newPtr;
}

void NvOsFreeInternal(void *ptr)
{
    NvU32 i;
    for(i = 0; i < NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE; i++)
    {
        if(gs_AllocList[i] == ptr)
        {
            gs_AllocList[i] = 0;
            break;
        }
    }

    // ok to free null
    free(ptr);
}

void AssertCheckAllocationTrackingList(void)
{
    NvU32 i;
    NvU32 printWarning = 0;
    for(i = 0; i < NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE; i++)
    {
        if(gs_AllocList[i])
        {
            if(!printWarning)
            {
                NvOsDebugPrintf("[nvos]     Detectable leaks:");
                printWarning = 1;
            }

            NvOsDebugPrintf("[nvos]     entry[%d] = 0x%08x",i,gs_AllocList[i]);
        }
    }
}

#else // !NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE

void *NvOsAllocInternal(size_t size)
{
    return malloc(size);
}

void *NvOsReallocInternal(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void NvOsFreeInternal(void *ptr)
{
    // ok to free null
    free(ptr);
}

#endif //

void *
NvOsExecAlloc(size_t size)
{
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (ptr)
        NvOsResourceAllocated(NvOsResourceType_NvOsExecAlloc, ptr);
    return ptr;
}

void
NvOsExecFree(void *ptr, size_t size)
{
    NvOsVirtualFree(ptr);
}

NvError
NvOsSharedMemAlloc(const char *key, size_t size,
    NvOsSharedMemHandle *descriptor)
{
    HANDLE map;
    NvError ret = NvError_InsufficientMemory;
    wchar_t buf[MAX_WIDE_STR_ON_STACK], *wkey;

    *descriptor = 0;

    /* if the shared memory already exists, the existing handle is returned
     * and the size is ignored.
     */
    wkey = NvOsStringToWString(key, buf, sizeof(buf));
    if (!key)
    {
        goto clean;
    }
    map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        0, size, wkey);
    if (map == 0)
    {
        goto clean;
    }
    NvOsResourceAllocated(NvOsResourceType_NvOsSharedMemAlloc, map);

    *descriptor = (NvOsSharedMemHandle)map;
    ret = NvSuccess;

clean:
    if (wkey != buf)
    {
        NvOsFreeInternal(wkey);
    }

    return ret;
}

NvError
NvOsSharedMemHandleFromFd(int fd,
    NvOsSharedMemHandle *descriptor)
{
    NV_ASSERT(descriptor != NULL);
    /* TODO: Just treat 'int' as 'HANDLE'?  Will that work? */
    return NvError_NotImplemented;
}

NvError
NvOsSharedMemGetFd( NvOsSharedMemHandle descriptor,
    int *fd )
{
    NV_ASSERT(fd != NULL);
    /* TODO: Just treat 'HANDLE' as 'int'?  Will that work? */
    return NvError_NotImplemented;
}

NvError
NvOsSharedMemMap(NvOsSharedMemHandle descriptor, size_t offset,
    size_t size, void **ptr)
{
    DWORD perm = FILE_MAP_WRITE | FILE_MAP_READ | FILE_MAP_COPY;
    void *addr = MapViewOfFile(descriptor, perm, 0, offset, size);
    if (addr == NULL)
    {
        *ptr = 0;
        return NvError_MemoryMapFailed;
    }
    NvOsResourceAllocated(NvOsResourceType_NvOsSharedMemMap, addr);

    *ptr = addr;
    return NvSuccess;
}

void
NvOsSharedMemUnmap(void *ptr, size_t size)
{
    if (!ptr)
        return;
    if (!UnmapViewOfFile(ptr))
        NV_ASSERT(!"NvOsSharedMemUnmap: UnmapViewOfFile() failed");
    NvOsResourceFreed(NvOsResourceType_NvOsSharedMemMap, ptr);
}

void
NvOsSharedMemFree(NvOsSharedMemHandle descriptor)
{
    NvOsCloseHandle(NvOsResourceType_NvOsSharedMemAlloc, descriptor);
}


NvError
NvOsLibraryLoad(const char *name, NvOsLibraryHandle *library)
{
    HMODULE lib;
    NvError ret;
    wchar_t buf[MAX_WIDE_STR_ON_STACK], *wname;

    NV_ASSERT(name);
    NV_ASSERT(library);

    *library = 0;

    wname = NvOsStringToWString(name, buf, sizeof(buf));
    if (!wname)
    {
        ret = NvError_InsufficientMemory;
        goto clean;
    }
    lib = LoadLibraryW(wname);

    if (lib == NULL)
    {
        if (!Win32ErrorToNvError(GetLastError(), &ret)) {
            ret = NvError_LibraryNotFound;
        }
        goto clean;
    }
    NvOsResourceAllocated(NvOsResourceType_NvOsLibrary, lib);

    *library = (NvOsLibraryHandle)lib;
    ret = NvSuccess;

clean:
    if (wname != buf)
    {
        NvOsFreeInternal(wname);
    }

    return ret;
}

void*
NvOsLibraryGetSymbol(NvOsLibraryHandle library, const char *symbol)
{
    NV_ASSERT(symbol);
    return GetProcAddress((HMODULE)library, symbol);
}

void
NvOsLibraryUnload(NvOsLibraryHandle library)
{
    if (!library)
        return;
    if (!FreeLibrary((HMODULE)library))
        NV_ASSERT(!"NvOsLibraryUnload: FreeLibrary() failed");
    NvOsResourceFreed(NvOsResourceType_NvOsLibrary, library);
}

void
NvOsSleepMSInternal(NvU32 msec)
{
    Sleep(msec);
}


void
NvOsWaitUS(NvU32 usec)
{
    NvU64 NumOfticksToWait;
    NvU64 Delta;
    LARGE_INTEGER startTime, endTime;

    if (!QueryPerformanceCounter(&startTime))
        NV_ASSERT(!"NvOsGetTimeUS: QueryPerformanceCounter() failed");

    if ((NvU64)gs_freqCount.QuadPart==1000000ULL)
        NumOfticksToWait = usec;
    else
        NumOfticksToWait = (usec * ((NvU64)gs_freqCount.QuadPart)) / 1000000;

    /* Minimum wait time is one tick */
    if (!NumOfticksToWait)
    {
        NumOfticksToWait = 1;
    }

    do
    {
        QueryPerformanceCounter(&endTime);
        Delta = (NvU64)endTime.QuadPart - (NvU64)startTime.QuadPart;
    } while ((NvU32)Delta < NumOfticksToWait);
}

NvError
NvOsMutexCreateInternal(NvOsMutexHandle *mutex)
{
    NvError ret;
    NvOsMutex *pMutex = NULL;

    /* the os will reference count the mutex for you. */
    NV_ASSERT(mutex);
    *mutex = 0;

    pMutex = NvOsAllocInternal(sizeof(*pMutex));
    if (!pMutex)
    {
        ret = NvError_InsufficientMemory;
        goto clean;
    }
    NvOsMemset(pMutex, 0, sizeof(*pMutex));

    pMutex->bCritSection = NV_TRUE;
    InitializeCriticalSection(&pMutex->CritSection);
    NvOsResourceAllocated(NvOsResourceType_NvOsMutex, pMutex);

    *mutex = pMutex;
    ret = NvSuccess;

clean:
    if (ret != NvSuccess)
        NvOsFreeInternal(pMutex);

    return ret;
}

void
NvOsMutexLockInternal(NvOsMutexHandle mutex)
{
    if (mutex->bCritSection)
    {
        // FIXME: check the compiler output to make sure it uses this as
        //        the likely path
        EnterCriticalSection(&mutex->CritSection);
        return;
    }

    if (WAIT_OBJECT_0 != WaitForSingleObject(mutex->hMutex, INFINITE))
    {
        NV_ASSERT(!"NvOsMutexLockInternal: WaitForSingleObject() failed");
    }
}

void
NvOsMutexUnlockInternal(NvOsMutexHandle mutex)
{
    if (mutex->bCritSection)
    {
        // FIXME: check the compiler output to make sure it uses this as
        //        the likely path
        LeaveCriticalSection(&mutex->CritSection);
        return;
    }

    if (!ReleaseMutex(mutex->hMutex))
    {
        NV_ASSERT(!"NvOsMutexUnlockInternal: ReleaseMutex() failed");
    }
}

void
NvOsMutexDestroyInternal(NvOsMutexHandle mutex)
{
    if (mutex->bCritSection)
    {
        DeleteCriticalSection(&mutex->CritSection);
        NvOsResourceFreed(NvOsResourceType_NvOsMutex, mutex);
    }
    else
    {
        NvOsCloseHandle(NvOsResourceType_NvOsMutex, mutex->hMutex);
    }
    NvOsFreeInternal(mutex);
}

NvError
NvOsIntrMutexCreate(NvOsIntrMutexHandle *mutex)
{
    NvError err;
    NvOsMutexHandle h;

    err = NvOsMutexCreate(&h);
    if (err != NvSuccess)
    {
        return err;
    }

    *mutex = (NvOsIntrMutexHandle)h;
    return NvSuccess;
}

void
NvOsIntrMutexLock(NvOsIntrMutexHandle mutex)
{
    NvOsMutexLock((NvOsMutexHandle)mutex);
}

void
NvOsIntrMutexUnlock(NvOsIntrMutexHandle mutex)
{
    NvOsMutexUnlock((NvOsMutexHandle)mutex);
}

void
NvOsIntrMutexDestroy(NvOsIntrMutexHandle mutex)
{
    NvOsMutexDestroy((NvOsMutexHandle)mutex);
}

NvError NvOsConditionCreate(NvOsConditionHandle *cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionDestroy(NvOsConditionHandle cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionBroadcast(NvOsConditionHandle cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionSignal(NvOsConditionHandle cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionWait(NvOsConditionHandle cond, NvOsMutexHandle mutex)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionWaitTimeout(NvOsConditionHandle cond, NvOsMutexHandle mutex, NvU32 microsecs)
{
    return NvError_NotImplemented;
}

NvError NvOsSemaphoreCreateInternal(NvOsSemaphoreHandle *semaphore,
    NvU32 value)
{
    HANDLE s = CreateSemaphore(NULL, value, INT_MAX, NULL);
    if (!s)
        return NvError_InsufficientMemory;
    NvOsResourceAllocated(NvOsResourceType_NvOsSemaphore, s);
    *semaphore = (NvOsSemaphoreHandle)s;
    return NvSuccess;
}

NvError
NvOsSemaphoreCloneInternal(NvOsSemaphoreHandle orig,
    NvOsSemaphoreHandle *semaphore)
{
    BOOL b;
    HANDLE h;

    b = DuplicateHandle(GetCurrentProcess(), orig,
        GetCurrentProcess(), &h, 0, FALSE,
        DUPLICATE_SAME_ACCESS);
    if (!b)
    {
        return NvError_BadParameter;
    }
    NvOsResourceAllocated(NvOsResourceType_NvOsSemaphore, h);

    *semaphore = (NvOsSemaphoreHandle)h;
    return NvSuccess;
}

void
NvOsSemaphoreWaitInternal(NvOsSemaphoreHandle semaphore)
{
    if (WAIT_OBJECT_0 != WaitForSingleObject(semaphore, INFINITE))
        NV_ASSERT(!"NvOsSemaphoreWaitInternal: WaitForSingleObject() failed");
}

NvError
NvOsSemaphoreWaitTimeoutInternal(NvOsSemaphoreHandle semaphore, NvU32 msec)
{
    DWORD dwRet;

    dwRet = WaitForSingleObject(semaphore, msec);
    if (dwRet == WAIT_TIMEOUT)
        return NvError_Timeout;
    NV_ASSERT(dwRet == WAIT_OBJECT_0);
    return NvSuccess;
}

void
NvOsSemaphoreSignalInternal(NvOsSemaphoreHandle semaphore)
{
    if (!ReleaseSemaphore(semaphore, 1, NULL))
        NV_ASSERT(!"NvOsSemaphoreSignalInternal: ReleaseSemaphore() failed");
}

void
NvOsSemaphoreDestroyInternal(NvOsSemaphoreHandle semaphore)
{
    NvOsCloseHandle(NvOsResourceType_NvOsSemaphore, semaphore);
}

NvU32 NvOsTlsAllocInternal(void)
{
    NV_ASSERT(NVOS_INVALID_TLS_INDEX == 0xffffffff);
    return TlsAlloc();
}

void NvOsTlsFreeInternal(NvU32 TlsIndex)
{
    if (TlsIndex != NVOS_INVALID_TLS_INDEX)
    {
        NvBool success = TlsFree(TlsIndex);
        NV_ASSERT(success);
        (void)success;
    }
}

void *NvOsTlsGetInternal(NvU32 TlsIndex)
{
    void *val;
    NV_ASSERT(TlsIndex != NVOS_INVALID_TLS_INDEX);
    val = TlsGetValue(TlsIndex);
    NV_ASSERT(val || GetLastError() == NO_ERROR);
    return val;
}

void NvOsTlsSetInternal(NvU32 TlsIndex, void *Value)
{
    NvBool success;
    NV_ASSERT(TlsIndex != NVOS_INVALID_TLS_INDEX);
    success = TlsSetValue(TlsIndex, Value);
    NV_ASSERT(success);
    (void)success;
}

typedef struct
{
    NvOsThreadFunction function;
    void *thread_args;
} NvWindowsThreadArgs;

static DWORD WINAPI thread_wrapper(void *p)
{
    NvWindowsThreadArgs *args = (NvWindowsThreadArgs *)p;

    args->function(args->thread_args);
    NvOsFree(args);
    return 0;
}

typedef struct NvOsTlsTerminatorRec
{
    void (*terminator)(void *);
    void *context;
    struct NvOsTlsTerminatorRec *next;
} NvOsTlsTerminator;

NvError NvOsTlsAddTerminatorInternal(void (*func)(void *), void *context)
{
    NvOsTlsTerminator *term;
    NvOsTlsTerminator *term_list;

    if (gs_TlsTerminatorKey == NVOS_INVALID_TLS_INDEX)
    {
        NvU32 TlsKey = NvOsTlsAlloc();
        if (TlsKey != NVOS_INVALID_TLS_INDEX)
        {
            if (NvOsAtomicCompareExchange32((NvS32*)&gs_TlsTerminatorKey,
                    NVOS_INVALID_TLS_INDEX, TlsKey) != NVOS_INVALID_TLS_INDEX)
                NvOsTlsFree(TlsKey);
        }
    }

    if (gs_TlsTerminatorKey == NVOS_INVALID_TLS_INDEX)
        return NvError_InsufficientMemory;

    term_list = (NvOsTlsTerminator*)NvOsTlsGetInternal(gs_TlsTerminatorKey);

    term = NvOsAlloc(sizeof(*term));
    if (!term)
        return NvError_InsufficientMemory;

    term->terminator = func;
    term->context = context;
    term->next = term_list;
    NvOsTlsSetInternal(gs_TlsTerminatorKey, term);
    return NvSuccess;
}

NvBool NvOsTlsRemoveTerminatorInternal(void (*func)(void *), void *context)
{
    NvOsTlsTerminator *term;
    NvOsTlsTerminator *term_list;
    NvOsTlsTerminator *term_prev;

    if (gs_TlsTerminatorKey == NVOS_INVALID_TLS_INDEX)
    {
        return NV_FALSE;
    }

    term_list = (NvOsTlsTerminator*)NvOsTlsGetInternal(gs_TlsTerminatorKey);
    term_prev = NULL;
    while ((term = term_list) != NULL)
    {
        term_list = term->next;
        if (term->terminator == func && term->context == context)
        {
            if (term_prev == NULL)
            {
                //first terminator
                //set the thread key to be the next guy
                //When term_list is NULL the thread destructor won't be called
                NvOsTlsSetInternal(gs_TlsTerminatorKey, term_list);
            }
            else
            {
                term_prev->next = term_list;
            }
            NvOsFree(term);
            return NV_TRUE;
        }
        term_prev = term;
    }
    return NV_FALSE;
}

void NvOsTlsRunTerminatorsWin32(void)
{
    NvOsTlsTerminator *term_list, *term;

    if (gs_TlsTerminatorKey == NVOS_INVALID_TLS_INDEX)
        return;

    term_list = NvOsTlsGetInternal(gs_TlsTerminatorKey);

    while ((term = term_list)!=NULL)
    {
        term_list = term->next;
        term->terminator(term->context);
        NvOsFree(term);
    }
}

NvError NvOsThreadCreateInternal(NvOsThreadFunction function,
    void *args, NvOsThreadHandle *thread, NvOsThreadPriorityType priority)
{
    HANDLE t = 0;
    NvWindowsThreadArgs *ta = 0;
    NvError err = NvError_InsufficientMemory;

    NV_ASSERT(thread);

    /* create the thread args then the thread */
    ta = NvOsAlloc(sizeof(NvWindowsThreadArgs));
    if (!ta)
    {
        return err;
    }

    ta->function = function;
    ta->thread_args = args;

    t = (HANDLE)_beginthreadex(NULL,  // security attributes
                               0,     // stack size, 0 indicates default size
                               thread_wrapper, // thread start functions
                               ta,             // arguments
                               0,
                               NULL);
    if (t == (HANDLE)0)
    {
        goto fail;
    }
    NvOsResourceAllocated(NvOsResourceType_NvOsThread, t);

    *thread = t;
    return NvSuccess;

fail:
    NvOsFree(ta);
    return err;
}

void
NvOsThreadJoinInternal(NvOsThreadHandle thread)
{
    if (WAIT_OBJECT_0 != WaitForSingleObject(thread, INFINITE))
        NV_ASSERT(!"NvOsThreadJoinInternal: WaitForSingleObject() failed");
    NvOsCloseHandle(NvOsResourceType_NvOsThread, thread);
}

void
NvOsThreadYieldInternal(void)
{
    (void)SwitchToThread();
}

NvS32 NvOsAtomicCompareExchange32(NvS32 *pTarget, NvS32 OldValue,
    NvS32 NewValue)
{
    return InterlockedCompareExchange((LONG *)pTarget, NewValue, OldValue);
}

NvS32 NvOsAtomicExchange32(NvS32 *pTarget, NvS32 Value)
{
    return InterlockedExchange((LONG *)pTarget, Value);
}

NvS32 NvOsAtomicExchangeAdd32(NvS32 *pTarget, NvS32 Value)
{
    return InterlockedExchangeAdd((LONG *)pTarget, Value);
}

NvError
NvOsGetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    SYSTEMTIME SystemTime;
    FILETIME FileTime;
    int retval = 0;
    NvU64 TotalTime;
    GetSystemTime(&SystemTime);
    retval = SystemTimeToFileTime(&SystemTime, &FileTime);
    if(!retval) goto fail;
    TotalTime = FileTime.dwHighDateTime ;
    TotalTime = (TotalTime << 32);
    TotalTime |=  (FileTime.dwLowDateTime);
    //convert total time in 100-nano seconds to seconds
    TotalTime = TotalTime/10000000;
    //change the time base from 1601 to 1970
    hNvOsSystemtime->Seconds = (NvU32)(TotalTime - SECONDS_BTN_1601_1970);
    hNvOsSystemtime->Milliseconds = SystemTime.wMilliseconds;
    return NvSuccess;
    fail:
        return NvError_InvalidState;
}

NvError
NvOsSetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    return NvError_NotImplemented;

}

NvU32
NvOsGetTimeMS(void)
{
    FILETIME ft;
    NvU64 tmp;

    GetSystemTimeAsFileTime(&ft);

    /* filetime is the number of 100-nanosecond intervals since
     *   January 1, 1601.
     *
     * convert to milliseconds.
     */
    tmp = (NvU64)(ft.dwLowDateTime |
        ((NvU64)ft.dwHighDateTime << 32));
    tmp /= 10;   // microseconds
    tmp /= 1000; // milliseconds

    return (NvU32)(tmp & 0xFFFFFFFF);
}

NvU64
NvOsGetTimeUS(void)
{
    LARGE_INTEGER curCount;
    NvU64 usec = 0;

    if (!QueryPerformanceCounter(&curCount))
        NV_ASSERT(!"NvOsGetTimeUS: QueryPerformanceCounter() failed");

    // PerfCount returns the number of "counts" that have elapsed.
    // PerfFreq is the number of "counts/sec". So, to convert to usec,
    // we divide curCount/FreqCount * 1000000;
    NV_ASSERT(gs_freqCount.QuadPart != 0);

    if ((NvU64)gs_freqCount.QuadPart==1000000ULL)
        usec = (NvU64)curCount.QuadPart;
    else
        usec = (1000000 * ((NvU64)curCount.QuadPart)) /
            ((NvU64)gs_freqCount.QuadPart);

    return usec;
}

void
NvOsProfileApertureSizes( NvU32 *apertures, NvU32 *sizes )
{
    *apertures = 0;
}

void
NvOsProfileStart( void **apertures )
{
}

void
NvOsProfileStop( void **apertures )
{
}

NvError
NvOsProfileWrite( NvOsFileHandle file, NvU32 index, void *apertures )
{
    return NvError_NotSupported;
}

NvError NvOsGetOsInformation(NvOsOsInfo *pOsInfo)
{
    if (pOsInfo != NULL)
    {
        pOsInfo->OsType = NvOsOs_Unknown;
        pOsInfo->Sku = NvOsSku_Unknown;
        return NvError_NotSupported;
    }
    else
    {
        return NvError_BadParameter;
    }
}

#if NVOS_SECURE_STRING_FUNCS

NvS32
NvOsSnprintf(char *str, size_t size, const char *format, ...)
{
    int n;
    va_list ap;

    va_start(ap, format);
    n = _vsnprintf_s(str, size, _TRUNCATE, format, ap);
    va_end(ap);

    return n;
}

NvS32
NvOsVsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return _vsnprintf_s(str, size, _TRUNCATE, format, ap);
}

void
NvOsStrncpy(char *dest, const char *src, size_t size)
{
    errno_t e;

    NV_ASSERT(dest);
    NV_ASSERT(src);

    if ((!dest) || (!src) || (!size))
        return;

    NvOsMemset(dest, size, 0);
    e = strncpy_s(dest, size, src, _TRUNCATE);
    if (e==STRUNCATE)
        dest[size-1] = src[size-1];
}

#else

NvS32
NvOsSnprintf(char *str, size_t size, const char *format, ...)
{
    int n;
    va_list ap;

    va_start(ap, format);
    n = _vsnprintf(str, size, format, ap);
    va_end(ap);

    return n;
}


NvS32
NvOsVsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return _vsnprintf(str, size, format, ap);
}

void
NvOsStrncpy(char *dest, const char *src, size_t size)
{
    NV_ASSERT(dest);
    NV_ASSERT(src);

    (void)strncpy(dest, src, size);
}

#endif

void
NvOsStrcpy(char *dest, const char *src)
{
    NV_ASSERT(dest);
    NV_ASSERT(src);

    (void)strcpy(dest, src);
}

int NvOsSetFpsTarget(int target) { return -1; }
int NvOsModifyFpsTarget(int fd, int target) { return -1; }
void NvOsCancelFpsTarget(int fd) { }
int NvOsSendThroughputHint (const char *usecase, NvOsTPHintType type, NvU32 value, NvU32 timeout_ms) { return 0; }
int NvOsVaSendThroughputHints (NvU32 client_tag, ...) { return 0; }
void NvOsCancelThroughputHint (const char *usecase) { }
