/*
 * Copyright (c) 2006-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */


/**
 * @file
 * <b> NVIDIA Operating System Abstraction</b>
 *
 * @b Description: Provides interfaces that enable unification of code
 * across all supported operating systems.
 */


#ifndef INCLUDED_NVOS_H
#define INCLUDED_NVOS_H

/**
 * @defgroup nvos_group Operating System Abstraction (NvOS)
 *
 * This provides a basic set of interfaces to unify code
 * across all supported operating systems. This layer does @b not
 * handle any hardware specific functions, such as interrupts.
 * "Platform" setup and GPU access are done by other layers.
 *
 * @warning Drivers and applications should @b not make any operating system
 * calls outside of this layer, @b including stdlib functions. Doing so will
 * result in non-portable code.
 *
 * For APIs that take key parameters, keys may be of ::NVOS_KEY_MAX length.
 * Any characters beyond this maximum is ignored.
 *
 * All strings passed to or from NvOS functions are encoded in UTF-8. For
 * character values below 128, this is the same as simple ASCII. For more
 * information, see:
 * <a href="http://en.wikipedia.org/wiki/UTF-8"
 *    target="_blank">http://en.wikipedia.org/wiki/UTF-8</a>
 *
 * @par Important:
 *
 *  At interrupt time there are only a handful of NvOS functions that are safe
 *  to call:
 *  - ::NvOsSemaphoreSignal
 *  - ::NvOsIntrMutexLock
 *  - ::NvOsIntrMutexUnlock
 *  - ::NvOsWaitUS
 *
 * @note Currently ::NvOsWaitUS for ISR has @b only been implemented for AOS and
 * WinCE. Use with caution.
 *
 * <a name="nvos_cfg_support"></a>
 * <h2>NvOs Configuration Data Storage Support</h2>
 *
 * NvOs supports named configuration variables that may have
 * string values or unsigned integer values.
 *
 * The storage of the configuration data is highly platform
 * dependent. The platform may provide the means to set the initial
 * data for the storage in the platform build phase. Also it may
 * be possible to set and retrieve the values in a running system
 * via OS provided mechanisms.
 *
 * Not all platforms support setting configuration variables via
 * the ::NvOsSetConfigU32 and ::NvOsSetConfigString functions. This
 * is indicated by a \c NvError_NotSupported return value. When
 * setting is supported, the values are guaranteed to be persistent
 * over system reboots.
 *
 * Information about the configuration data storages for various
 * operating systems follows.
 *
 * @par Linux / Win32 x86 Host
 *
 * On x86 host systems and Linux the configuration data storage is the
 * environment inherited from the shell that launches the application.
 * To set configuration variable values export the appropriate
 * environment variables from the launching shell. Storing
 * configuration variables is not supported.
 *
 * @par Windows CE / Window Mobile
 *
 * On target Windows systems the configuration data is stored in
 * the Windows registry, under the registry path:
 * <pre>
 *     HKEY_LOCAL_MACHINE\\Software\\NVIDIA Corporation\\NvOs
 * </pre>
 * The configuration data can be set by \c .reg files in the image build.
 * Storing of configuration variables is not yet supported, but
 * will be added.
 *
 * @par Android
 *
 * On Android the configuration store is the Android property
 * mechanism. NvOs config variables are stored in the "persist.tegra"
 * namespace. Values can be set via the "setprop" tool and values
 * queried via the "getprop" tool.
 *
 * As with other Linux systems, the environment acts as a read-only
 * configuration data store. Values read from the environment override
 * those read from Android properties.
 *
 * @par Other Target Systems
 *
 * The default for target systems is to use a file on the file
 * system as the data storage. The path to the file on the
 * target file system is:
 * <pre>
 *    /etc/tegra_config.txt
 * </pre>
 * @{
 */

#include <stdarg.h>
#include "nvcommon.h"
#include "nverror.h"
#include "nvos_trace.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * A physical address. Must be 64 bits for OSs that support more than 64 bits
 * of physical addressing, not necessarily correlated to the size of a virtual
 * address.
 *
 * Currently, 64-bit physical addressing is supported by NvOS on WinNT only.
 *
 * XXX 64-bit phys addressing really should be supported on Linux/x86, since
 * all modern x86 CPUs have 36-bit (or more) physical addressing. We might
 * need to control a PCI card that the SBIOS has placed at an address above
 * 4 GB.
 */
#if NVOS_IS_WINDOWS
typedef NvU64 NvOsPhysAddr;
#else
typedef NvU32 NvOsPhysAddr;
#endif

/** The maximum length of a shared resource identifier string.
 */
#define NVOS_KEY_MAX 128

/** The maximum length for a file system path.
 */
#define NVOS_PATH_MAX 256

/** Indicates that ::NvOsStatType structure has \c mtime.
 */
#define NVOS_HAS_MTIME 1

/** Indicates that NvOs has NvOsMkdir().
 */
#define NVOS_HAS_MKDIR 1

/** Indicates that NvOs has NvOsFtruncate().
 */
#define NVOS_HAS_FTRUNCATE 1

/** Indicates that NvOs has NvOsFlock().
 */
#define NVOS_HAS_FLOCK 1

/** Indicates that NvOs has condition variables.
 */
#define NVOS_HAS_CONDITION 1

/** @name Print Operations
 */
/*@{*/

/** Printf family. */
typedef struct NvOsFileRec *NvOsFileHandle;

/** Prints a string to a file stream.
 *
 *  @param stream The file stream to which to print.
 *  @param format The format string.
 */
NvError
NvOsFprintf(NvOsFileHandle stream, const char *format, ...);

// Doxygen requires escaping backslash characters (\) with another \ so in
// @return, ignore the first backslash if you are reading this in the header.
/** Expands a string into a given string buffer.
 *
 *  @param str A pointer to the target string buffer.
 *  @param size The size of the string buffer.
 *  @param format A pointer to the format string.
 *
 *  @return The number of characters printed (not including the \\0).
 *  The buffer was printed to successfully if the returned value is
 *  greater than -1 and less than \a size.
 */
NvS32
NvOsSnprintf(char *str, size_t size, const char *format, ...);

/** Prints a string to a file stream using a va_list.
 *
 *  @param stream The file stream.
 *  @param format A pointer to the format string.
 *  @param ap The va_list structure.
 */
NvError
NvOsVfprintf(NvOsFileHandle stream, const char *format, va_list ap);

/** Expands a string into a string buffer using a va_list.
 *
 *  @param str A pointer to the target string buffer.
 *  @param size The size of the string buffer.
 *  @param format A pointer to the format string.
 *  @param ap The va_list structure.
 *
 *  @return The number of characters printed (not including the \\0).
 *  The buffer was printed to successfully if the returned value is
 *  greater than -1 and less than \a size.
 */
NvS32
NvOsVsnprintf(char *str, size_t size, const char *format, va_list ap);

/**
 * Outputs a message to the debugging console, if present. All device driver
 * debug printfs should use this. Do not use this for interacting with a user
 * from an application; in that case, use NvTestPrintf() instead.
 *
 * @param format A pointer to the format string.
 */
void
NvOsDebugPrintf(const char *format, ...);

/**
 * Same as ::NvOsDebugPrintf, except takes a va_list.
 */
void
NvOsDebugVprintf( const char *format, va_list ap );

/**
 * Same as ::NvOsDebugPrintf, except returns the number of chars written.
 *
 * @return number of chars written or -1 if that number is unavailable.
 */
NvS32
NvOsDebugNprintf( const char *format, ...);

void
NvOsDebugString(const char *str);

/**
 * Prints an error and the line it appeared on.
 * Does nothing if err==NvSuccess.
 *
 * @param err The error to return.
 * @param file A pointer to the file the error occurred in.
 * @param line The line number the error occurred on.
 * @returns The \a err value, on susccess.
 */
NvError
NvOsShowError(NvError err, const char *file, int line);

// Doxygen requires escaping # with a backslash, so in the examples below
// ignore the backslash before the # if reading this in the header file.
/**
 * Helper macro to go along with ::NvOsDebugPrintf. Usage:
 * <pre>
 *     NV_DEBUG_PRINTF(("foo: %s\n", bar));
   </pre>
 *
 * The debug print will be disabled by default in all builds, debug and
 * release. @note Usage requires double parentheses.
 *
 * To enable debug prints in a particular .c file, add the following
 * to the top of the .c file and rebuild:
 * <pre>
 *     \#define NV_ENABLE_DEBUG_PRINTS 1
   </pre>
 *
 * To enable debug prints in a particular module, add the following
 * to the makefile and rebuild:
 * <pre>
 *     LCDEFS += -DNV_ENABLE_DEBUG_PRINTS=1
   </pre>
 *
 */
#if !defined(NV_ENABLE_DEBUG_PRINTS)
#define NV_ENABLE_DEBUG_PRINTS 0
#endif
#if NV_ENABLE_DEBUG_PRINTS
// put the print in an if statement so that the compiler will always parse it
#define NV_DEBUG_PRINTF(x) \
    do { if (NV_ENABLE_DEBUG_PRINTS) { NvOsDebugPrintf x ; } } while (0)
#else
#define NV_DEBUG_PRINTF(x) do {} while (0)
#endif

/*@}*/
/** @name OS Version
 */
/*@{*/

typedef enum
{
    NvOsOs_Unknown,
    NvOsOs_Windows,
    NvOsOs_Linux,
    NvOsOs_Aos,
    NvOsOs_Qnx,
    NvOsOs_Force32 = 0x7fffffffUL,
} NvOsOs;

typedef enum
{
    NvOsSku_Unknown,
    NvOsSku_CeBase,
    NvOsSku_Mobile_SmartFon,
    NvOsSku_Mobile_PocketPC,
    NvOsSku_Android,
    NvOsSku_Force32 = 0x7fffffffUL,
} NvOsSku;

typedef struct NvOsOsInfoRec
{
    NvOsOs  OsType;
    NvOsSku Sku;
    NvU16   MajorVersion;
    NvU16   MinorVersion;
    NvU32   SubVersion;
    NvU32   Caps;
} NvOsOsInfo;

/**
 * Gets the current OS version.
 *
 * @param pOsInfo A pointer to the operating system information structure.
 */
NvError
NvOsGetOsInformation(NvOsOsInfo *pOsInfo);

/*@}*/

/** @name String Operations
 */
/*@{*/

/** Copies a string.
 *
 *  The \a src string must be NUL terminated.
 *
 *  @param dest A pointer to the destination of the copy.
 *  @param src A pointer to the source string.
 */
void
NvOsStrcpy(char *dest, const char *src);

/** Copies a string, length-limited.
 *
 *  @param dest A pointer to the destination of the copy.
 *  @param src A pointer to the source string.
 *  @param size The length of the \a dest string buffer plus NULL terminator.
 */
void
NvOsStrncpy(char *dest, const char *src, size_t size);

/** Defines straight-forward mappings to international language encodings.
 *  Commonly-used encodings on supported operating systems are provided.
 *  @note NvOS string (and file/directory name) processing functions expect
 *  UTF-8 encodings. If the system-default encoding is not UTF-8,
 *  conversion may be required. @see NvUStrConvertCodePage.
 *
 **/
typedef enum
{
    NvOsCodePage_Unknown,
    NvOsCodePage_Utf8,
    NvOsCodePage_Utf16,
    NvOsCodePage_Windows1252,
    NvOsCodePage_Force32 = 0x7fffffffUL,
} NvOsCodePage;

/** @return The default code page for the system.
 *
 */
NvOsCodePage
NvOsStrGetSystemCodePage(void);

/** Gets the length of a string.
 *
 *  @param s A pointer to the string.
 */
size_t
NvOsStrlen(const char *s);

/** Compares two strings.
 *
 *  @param s1 A pointer to the first string.
 *  @param s2 A pointer to the second string.
 *
 *  @return 0 If the strings are identical.
 */
int
NvOsStrcmp(const char *s1, const char *s2);

/** Compares two strings up to the given length.
 *
 *  @param s1 A pointer to the first string.
 *  @param s2 A pointer to the second string.
 *  @param size The length to compare.
 *
 *  @return 0 If the strings are identical.
 */
int
NvOsStrncmp(const char *s1, const char *s2, size_t size);

/*@}*/
/** @name Memory Operations (Basic)
 */
/*@{*/

/** Copies memory.
 *
 *  @param dest A pointer to the destination of the copy.
 *  @param src A pointer to the source memory.
 *  @param size The length of the copy.
 */
void NvOsMemcpy(void *dest, const void *src, size_t size);

/** Compares two memory regions.
 *
 *  @param s1 A pointer to the first memory region.
 *  @param s2 A pointer to the second memory region.
 *  @param size The length to compare.
 *
 *  This returns 0 If the memory regions are identical
 */
int
NvOsMemcmp(const void *s1, const void *s2, size_t size);

/** Sets a region of memory to a value.
 *
 *  @param s A pointer to the memory region.
 *  @param c The value to set.
 *  @param size The length of the region.
 */
void
NvOsMemset(void *s, NvU8 c, size_t size);

/** Moves memory to a new location (may overlap).
 *
 *  @param dest A pointer to the destination memory region.
 *  @param src A pointer to the source region.
 *  @param size The size of the region to move.
 */
void
NvOsMemmove(void *dest, const void *src, size_t size);

/**
 * Like NvOsMemcpy(), but used to safely copy data from an application pointer
 * (usually embedded inside an \c ioctl() struct) into a driver pointer. Does not
 * make any assumptions about whether the application pointer is valid--will
 * return an error instead of crashing if it isn't. Must also validate that
 * the application pointer points to memory that the application owns; for
 * example, it should point to the user mode region of the address space and
 * not the kernel mode region, if such a distinction exists.
 *
 * @see NvOsCopyOut
 *
 * @param pDst A pointer to the destination (driver).
 * @param pSrc A pointer to the source (client/application).
 * @param Bytes The number of bytes to copy.
 */
NvError
NvOsCopyIn(
    void *pDst,
    const void *pSrc,
    size_t Bytes);

/**
 * Like NvOsMemcpy(), but used to safely copy data to an application pointer
 * (usually embedded inside an \c ioctl() struct) from a driver pointer. Does not
 * make any assumptions about whether the application pointer is valid--will
 * return an error instead of crashing if it isn't. Must also validate that
 * the application pointer points to memory that the application owns; for
 * example, it should point to the user mode region of the address space and
 * not the kernel mode region, if such a distinction exists.
 *
 * @see NvOsCopyIn
 *
 * @param pDst A pointer to the destination (client/application).
 * @param pSrc A pointer to the source (driver).
 * @param Bytes The number of bytes to copy.
 */
NvError
NvOsCopyOut(
    void *pDst,
    const void *pSrc,
    size_t Bytes);

/*@}*/
/** @name File Input/Output
 */
/*@{*/

/**
 *
 *  Defines wrappers over stdlib's file stream functions,
 *  with some changes to the API.
 */
typedef enum
{
    /** See the fseek manual page for details of Set, Cur, and End. */
    NvOsSeek_Set = 0,
    NvOsSeek_Cur = 1,
    NvOsSeek_End = 2,

    NvOsSeek_Force32 = 0x7FFFFFFF
} NvOsSeekEnum;

typedef enum
{
    NvOsFileType_Unknown = 0,
    NvOsFileType_File,
    NvOsFileType_Directory,
    NvOsFileType_Fifo,
    NvOsFileType_CharacterDevice,
    NvOsFileType_BlockDevice,

    NvOsFileType_Force32 = 0x7FFFFFFF
} NvOsFileType;

typedef enum
{
    /** See the flock manual page for details on shared and exclusive locks. */
    NvOsFlockType_Shared = 0,
    NvOsFlockType_Exclusive = 1,
    NvOsFlockType_Unlock = 2,

    NvOsFlockType_Force32 = 0x7FFFFFFF
} NvOsFlockType;

typedef struct NvOsStatTypeRec
{
    NvU64 size;
    NvOsFileType type;
    /// last modified time of the file
    NvU64 mtime;
} NvOsStatType;

/** Opens a file with read permissions. */
#define NVOS_OPEN_READ    0x1

/** Opens a file with write persmissions. */
#define NVOS_OPEN_WRITE   0x2

/** Creates a file if is not present on the file system. */
#define NVOS_OPEN_CREATE  0x4

/** Open file in append mode. Implies WRITE. */
#define NVOS_OPEN_APPEND  0x8

/** Opens a file stream.
 *
 *  If the ::NVOS_OPEN_CREATE flag is specified, ::NVOS_OPEN_WRITE must also
 *  be specified.
 *
 *  If \c NVOS_OPEN_WRITE is specified, the file will be opened for write and
 *  will be truncated if it was previously existing.
 *
 *  If \c NVOS_OPEN_WRITE and ::NVOS_OPEN_READ are specified, the file will not
 *  be truncated.
 *
 *  @param path A pointer to the path to the file.
 *  @param flags Or'd flags for the open operation (\c NVOS_OPEN_*).
 *  @param [out] file A pointer to the file that will be opened, if successful.
 */
NvError
NvOsFopen(const char *path, NvU32 flags, NvOsFileHandle *file);

/** Closes a file stream.
 *
 *  @param stream The file stream to close.
 *  Passing in a null handle is okay.
 */
void NvOsFclose(NvOsFileHandle stream);

/** Writes to a file stream.
 *
 *  @param stream The file stream.
 *  @param ptr A pointer to the data to write.
 *  @param size The length of the write.
 *
 *  @retval NvError_FileWriteFailed Returned on error.
 */
NvError
NvOsFwrite(NvOsFileHandle stream, const void *ptr, size_t size);

/** Reads a file stream.
 *
 *  Buffered read implementation if available for a particular OS may
 *  return corrupted data if multiple threads read from the same
 *  stream simultaneously.
 *
 *  To detect short reads (less that specified amount), pass in \a bytes
 *  and check its value to the expected value. The \a bytes parameter may
 *  be null.
 *
 *  @param stream The file stream.
 *  @param ptr A pointer to the buffer for the read data.
 *  @param size The length of the read.
 *  @param [out] bytes A pointer to the number of bytes readd; may be null.
 *
 *  @retval NvError_FileReadFailed If the file read encountered any
 *      system errors.
 *  @retval NvError_EndOfFile Indicates end of file reached. Bytes
 *      must be checked to determine whether data was read into the
 *      buffer.
 */
NvError
NvOsFread(NvOsFileHandle stream, void *ptr, size_t size, size_t *bytes);

/** Reads a file stream with timeout.
 *
 *  Buffered read implementation if available for a particular OS may
 *  return corrupted data if multiple threads read from the same
 *  stream simultaneously.
 *
 *  To detect short reads (less that specified amount), pass in \a bytes
 *  and check its value to the expected value. The \a bytes parameter may
 *  be null.
 *
 *  @param stream The file stream.
 *  @param ptr A pointer to the buffer for the read data.
 *  @param size The length of the read.
 *  @param [out] bytes A pointer to the number of bytes read; may be null.
 *  @param timeout_msec Timeout for function to return if no bytes available.
 *
 *  @retval NvError_FileReadFailed If the file read encountered any
 *      system errors.
 *  @retval NvError_Timeout If no bytes are available to read.
 */
NvError
NvOsFreadTimeout(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec);

/** Gets a character from a file stream.
 *
 *  @param stream The file stream.
 *  @param [out] c A pointer to the character from the file stream.
 *
 *  @retval NvError_EndOfFile When the end of file is reached.
 */
NvError
NvOsFgetc(NvOsFileHandle stream, NvU8 *c);

/** Changes the file position pointer.
 *
 *  @param file The file.
 *  @param offset The offset from whence to seek.
 *  @param whence The starting point for the seek.
 *
 *  @retval NvError_FileOperationFailed On error.
 */
NvError
NvOsFseek(NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence);

/** Gets the current file position pointer.
 *
 *  @param file The file.
 *  @param [out] position A pointer to the file position.
 *
 *  @retval NvError_FileOperationFailed On error.
 */
NvError
NvOsFtell(NvOsFileHandle file, NvU64 *position);

/** Gets file information.
 *
 *  @param filename A pointer to the file about which to get information.
 *  @param [out] stat A pointer to the information structure.
 */
NvError
NvOsStat(const char *filename, NvOsStatType *stat);

/** Gets file information from an already open file.
 *
 *  @param file The open file.
 *  @param [out] stat A pointer to the information structure.
 */
NvError
NvOsFstat(NvOsFileHandle file, NvOsStatType *stat);

/** Flushes any pending writes to the file stream.
 *
 *  @param stream The file stream.
 */
NvError
NvOsFflush(NvOsFileHandle stream);

/** Commits any pending writes to storage media.
 *
 *  After this completes, any pending writes are guaranteed to be on the
 *  storage media associated with the stream (if any).
 *
 *  @param stream The file stream.
 */
NvError
NvOsFsync(NvOsFileHandle stream);

/** Removes a file from the storage media. If the file is open,
 *  this function marks the file for deletion upon close.
 *
 *  @param filename A pointer to the file to remove.
 *
 *  @retval NvError_FileOperationFailed If cannot remove file.
 */
NvError
NvOsFremove(const char *filename);

/** Causes the file to have a size of length bytes. The file must
 *  be opened for writing.
 *
 *  @param stream Holds the file stream.
 *  @param length Specifies the size of file in bytes to which to truncate.
 *
 *  @retval NvError_FileOperationFailed If file is not open for writing.
 *  @retval NvError_ResourceError If I/O error occurred when writing to the file.
 */
NvError
NvOsFtruncate(NvOsFileHandle stream, NvU64 length);

/** Applies or removes an advisory lock on an open file.
 *
 * @note This is a non-blocking lock.
 *
 * This is currently only implemented on Linux.
 *
 * @param file Specifies the file to apply lock to or remove lock from.
 * @param type Locks can be shared (more than one process can hold
 * a shared lock) or exclusive (only one process may hold an exclusive
 * lock). Locks are advisory only, which means that a process is free
 * to bypass it and perform I/O on the file. A process may only hold
 * one lock on a file. Subsequent lock calls will convert an existing
 * lock to the new lock mode.
 *
 * @retval NvSuccess If the lock operation succeeded.
 * @retval NvError_Busy or NvError_AccessDenied If a conflicting lock
 * is held by another process.
 * @retval NvError_FileOperationFailed If file handle is not valid or the
 * open mode does not match the type of lock requested.
 */
NvError
NvOsFlock(NvOsFileHandle file, NvOsFlockType type);

/**
 * Thunk into the device driver implementing this file (usually a device file)
 * to perform an I/O control (IOCTL) operation.
 *
 * @param hFile The file on which to perform the IOCTL operation.
 * @param IoctlCode The IOCTL code (which operation to perform).
 * @param pBuffer A pointer to the buffer containing the data for the IOCTL
 *     operation. This buffer must first consist of \a InBufferSize bytes of
 *     input-only data, followed by \a InOutBufferSize bytes of input/output
 *     data, and finally \a OutBufferSize bytes of output-only data. Its total
 *     size is therefore:
 * <pre>
 *     InBufferSize + InOutBufferSize + OutBufferSize
   </pre>
 * @param InBufferSize The number of input-only data bytes in the buffer.
 * @param InOutBufferSize The number of input/output data bytes in the buffer.
 * @param OutBufferSize The number of output-only data bytes in the buffer.
 */
NvError
NvOsIoctl(
    NvOsFileHandle hFile,
    NvU32 IoctlCode,
    void *pBuffer,
    NvU32 InBufferSize,
    NvU32 InOutBufferSize,
    NvU32 OutBufferSize);

/*@}*/
/** @name Directories
 */
/*@{*/

/** A handle to a directory. */
typedef struct NvOsDirRec *NvOsDirHandle;

/** Opens a directory.
 *
 *  @param path A pointer to the path of the directory to open.
 *  @param [out] dir A pointer to the directory that will be opened, if successful.
 *
 *  @retval NvError_DirOperationFailed Returned upon failure.
 */
NvError
NvOsOpendir(const char *path, NvOsDirHandle *dir);

/** Gets the next entry in the directory.
 *
 *  @param dir The directory pointer.
 *  @param [out] name A pointer to the name of the next file.
 *  @param size The size of the name buffer.
 *
 *  @retval NvError_EndOfDirList When there are no more entries in the
 *      directory.
 *  @retval NvError_DirOperationFailed If there is a system error.
 */
NvError
NvOsReaddir(NvOsDirHandle dir, char *name, size_t size);

/** Closes the directory.
 *
 *  @param dir The directory to close.
 *      Passing in a null handle is okay.
 */
void NvOsClosedir(NvOsDirHandle dir);

/** Creates the directory.
 *
 *  This is currently only implemented on Linux.
 *
 *  @param dirname A pointer to the name of the directory to create.
 *
 *  @retval NvError_PathAlreadyExists If directory could not be
 *  created because the path already exists.
 *  @retval NvSuccess If the directory was created.
 *  @return Other nonzero error code if the directory could not be
 *  created for some other reason.
 */
NvError NvOsMkdir(char *dirname);

/** Virtual filesystem hook. */
typedef struct NvOsFileHooksRec {

    NvError (*hookFopen)(
                    const char *path,
                    NvU32 flags,
                    NvOsFileHandle *file );
    void    (*hookFclose)(
                    NvOsFileHandle stream);
    NvError (*hookFwrite)(
                    NvOsFileHandle stream,
                    const void *ptr,
                    size_t size);
    NvError (*hookFread)(
                    NvOsFileHandle stream,
                    void *ptr,
                    size_t size,
                    size_t *bytes,
                    NvU32 timeout_msec);
    NvError (*hookFseek)(
                    NvOsFileHandle file,
                    NvS64 offset,
                    NvOsSeekEnum whence);
    NvError (*hookFtell)(
                    NvOsFileHandle file,
                    NvU64 *position);
    NvError (*hookFstat)(
                    NvOsFileHandle file,
                    NvOsStatType *stat);
    NvError (*hookStat)(
                    const char *filename,
                    NvOsStatType *stat);
    NvError (*hookFflush)(
                    NvOsFileHandle stream);
    NvError (*hookFsync)(
                    NvOsFileHandle stream);
    NvError (*hookFremove)(
                    const char *filename);
    NvError (*hookFlock)(
                    NvOsFileHandle file,
                    NvOsFlockType type);
    NvError (*hookFtruncate)(
                    NvOsFileHandle stream,
                    NvU64 length);
    NvError (*hookOpendir)(
                    const char *path,
                    NvOsDirHandle *dir);
    NvError (*hookReaddir)(
                    NvOsDirHandle dir,
                    char *name,
                    size_t size);
    void    (*hookClosedir)(
                    NvOsDirHandle dir);
    NvError (*hookMkdir)(
                    char *dirname);
} NvOsFileHooks;

/** Sets up hook functions for extra stream functionality.
 *
 *  @note All function pointers must be non-NULL.
 *
 *  @param newHooks A pointer to the new set of functions to handle file I/O.
 *  NULL for defaults.
 */
const NvOsFileHooks *NvOsSetFileHooks(NvOsFileHooks *newHooks);


/** Stores an unsigned integer variable to the config store.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param name A pointer to the name of the variable.
 *  @param value The value to write.
 */
NvError
NvOsSetConfigU32(const char *name, NvU32 value);

/** Stores a string variable to the config store.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param name A pointer to the name of the variable.
 *  @param value A pointer to the value to write.
 */
NvError
NvOsSetConfigString(const char *name, const char *value);

/** Stores a string variable to the config store, with
 * a persist.sys prefix.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param name A pointer to the name of the variable.
 *  @param value A pointer to the value to write.
 */
NvError
NvOsSetSysConfigString(const char *name, const char *value);

/** Retrieves an unsigned integer variable from the config store.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param name A pointer to the name of the variable.
 *  @param [out] value A pointer to the value to write.
 *
 *  @retval NvError_ConfigVarNotFound If the name is not found in the
 *      config store.
 *  @retval NvError_InvalidConfigVar If the configuration variable cannot
 *      be converted into an unsigned integer.
 */
NvError
NvOsGetConfigU32(const char *name, NvU32 *value);

/** Retrieves a string variable from the config store.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param name A pointer to the name of the variable.
 *  @param value A pointer to the value to write into.
 *  @param size The size of the value buffer.
 *
 *  @retval NvError_ConfigVarNotFound If the name is not found in the
 *      config store.
 */
NvError
NvOsGetConfigString(const char *name, char *value, NvU32 size);

/** Retrieves a string variable from the config store, with
 * a persist.sys prefix.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param name A pointer to the name of the variable.
 *  @param value A pointer to the value to write into.
 *  @param size The size of the value buffer.
 *
 *  @retval NvError_ConfigVarNotFound If the name is not found in the
 *      config store.
 */
NvError
NvOsGetSysConfigString(const char *name, char *value, NvU32 size);

/**
 * Defines related to stereo profiles.
 */
#define    MAX_ARRAY_SIZE    128

/**
 *  Defines stereo control enumerations.
 */
typedef enum
{
    /// Anything non-zero indicates that stereo is enabled.
    NvOsStereoCtrl_Disable = 0,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOsStereoCtrl_Force32 = 0x7fffffffUL,
} NvOsStereoCtrl;

/**
 * Defines a set of values that \a stateId can take on.
 */
typedef enum
{
    NvOsStateId_Default = 0,
    NvOsStateId_3dv_Controls = 1,
    NvOsStateId_3dv_Profiles = 2,
    NvOsStateId_3dv_Gles_Settings = 3,
    NvOsStateId_3dv_Video_Settings = 4,
    NvOsStateId_3dv_Camera_Settings = 5,
    NvOsStateId_3dv_Common_Settings = 6,
    NvOsStateId_Mjolnir_Controls = 7, // For Mjolnir, remote gaming solution
    NvOsStateId_Force32 = 0x7FFFFFFF
} NvOsStateId;

/** Sets the specified state.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param stateId Specifies the type of state that is
 *      being written.
 *  @param name A pointer to the name of the variable.
 *  @param value A pointer to the value to write.
 *  @param valueSize Size of the value buffer.
 *  @param flags Miscellaneous flags.
 *
 */
NvError
NvOsConfigSetState(int stateId, const char *name, const char *value,
        int valueSize, int flags);

/** Gets the specified state.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param stateId Specifies the type of state that is
 *      being written.
 *  @param name A pointer to the name of the variable.
 *  @param value A pointer to the value to write into.
 *  @param valueSize Size of the value buffer.
 *  @param flags Miscellaneous flags.
 *
 */
NvError
NvOsConfigGetState(int stateId, const char *name, char *value,
        int valueSize, int flags);

/** Queries the specified state for the available keys. If called with
 * \a ppKeys as NULL, then just the \a maxNumKeys value will be filled and
 * returned. This is used by the caller to allocate the \a ppKeys array.
 *
 * See also: <a class="el" href="#nvos_cfg_support">
 *     NvOs Configuration Data Storage Support</a>
 *
 *  @param stateId Specifies the type of state that is
 *      being written.
 *  @param ppKeys A pointer an array of keyname buffers.
 *  @param pNumKeys A pointer to the number of keynames.
 *  @param maxKeySize The maximum size of each keyname.
 *
 */
NvError NvOsConfigQueryState(int stateId, const char **ppKeys, int *pNumKeys,
        int maxKeySize);

/*@}*/
/** @name Memory Allocation
 */
/*@{*/

/** Dynamically allocates memory.
 *  Alignment, if desired, must be done by the caller.
 *
 *  @param size The size of the memory to allocate.
 */
void *NvOsAlloc(size_t size);

/** Re-sizes a previous dynamic allocation.
 *
 *  @param ptr A pointer to the original allocation.
 *  @param size The new size to allocate.
 */
void *NvOsRealloc(void *ptr, size_t size);

/** Frees a dynamic memory allocation.
 *
 *  Freeing a null value is okay.
 *
 *  @param ptr A pointer to the memory to free, which should be from
 *      NvOsAlloc().
 */
void NvOsFree(void *ptr);

/**
 * Alocates a block of executable memory.
 *
 * @param size The size of the memory to allocate.
 */
void *NvOsExecAlloc(size_t size);

/**
 * Frees a block of executable memory.
 *
 * @param ptr A pointer from NvOsExecAlloc() to the memory to free; may be null.
 * @param size The size of the allocation.
 */
void NvOsExecFree(void *ptr, size_t size);

/** An opaque handle returned by shared memory allocations.
 */
typedef struct NvOsSharedMemRec *NvOsSharedMemHandle;

/** Dynamically allocates multiprocess shared memory.
 *
 *  The memory will be zero initialized when it is first created.
 *
 *  @param key A pointer to the global key to identify the shared allocation.
 *  @param size The size of the allocation.
 *  @param [out] descriptor A pointer to the result descriptor.
 *
 *  @return If the shared memory for \a key already exists, then this returns
 *  the already allocated shared memory; otherwise, it creates it.
 */
NvError
NvOsSharedMemAlloc(const char *key, size_t size,
    NvOsSharedMemHandle *descriptor);

/**
 *
 * Enables sharing ::NvOsSharedMemHandle memory that was created by
 * a call to NvOsSharedMemAlloc().
 *
 *  @param fd A pointer to the global key to identify the shared allocation.
 *  @param [out] descriptor A pointer to the result descriptor.
 *
 *  @retval NvError_SharedMemMapFailed on failure.
 */
NvError
NvOsSharedMemHandleFromFd(int fd, NvOsSharedMemHandle *descriptor);

/** Extracts the file descriptor from a handle.
 *
 *  @param descriptor The memory descriptor to map.
 *  @param [out] fd A pointer to the result.
 *
 *  @return A file descriptor that can be used to create a new
 *  handle.
 */
NvError
NvOsSharedMemGetFd(NvOsSharedMemHandle descriptor, int *fd);

/** Maps a shared memory region into the process virtual memory.
 *
 *  @param descriptor The memory descriptor to map.
 *  @param offset The offset in bytes into the mapped area.
 *  @param size The size area to map.
 *  @param [out] ptr A pointer to the result pointer.
 *
 *  @retval NvError_SharedMemMapFailed on failure.
 */
NvError
NvOsSharedMemMap(NvOsSharedMemHandle descriptor, size_t offset,
    size_t size, void **ptr);

/** Unmaps a mapped region of shared memory.
 *
 *  @param ptr A pointer to the pointer to virtual memory.
 *  @param size The size of the mapped region.
 */
void NvOsSharedMemUnmap(void *ptr, size_t size);

/** Frees shared memory from NvOsSharedMemAlloc().
 *
 *  It is valid to call \c NvOsSharedMemFree while mappings are still
 *  outstanding.
 *
 *  @param descriptor The memory descriptor.
 */
void NvOsSharedMemFree(NvOsSharedMemHandle descriptor);

/** Defines memory attributes. */
typedef enum
{
    NvOsMemAttribute_Uncached       = 0,
    NvOsMemAttribute_WriteBack      = 1,
    NvOsMemAttribute_WriteCombined  = 2,
    NvOsMemAttribute_InnerWriteBack = 3,
    NvOsMemAttribute_Secured        = 4,

    NvOsMemAttribute_Force32 = 0x7FFFFFFF
} NvOsMemAttribute;

/** Specifies no memory flags. */
#define NVOS_MEM_NONE     0x0

/** Specifies the memory may be read. */
#define NVOS_MEM_READ     0x1

/** Specifies the memory may be written to. */
#define NVOS_MEM_WRITE    0x2

/** Specifies the memory may be executed. */
#define NVOS_MEM_EXECUTE  0x4

/**
 * The memory must be visible by all processes, this is only valid for
 * WinCE 5.0.
 */
#define NVOS_MEM_GLOBAL_ADDR 0x8

/** The memory may be both read and writen. */
#define NVOS_MEM_READ_WRITE (NVOS_MEM_READ | NVOS_MEM_WRITE)

/** Specifies the memory must be mapped as inner- and outer-cached. */
#define NVOS_MEM_MAP_WRITEBACK 0x10

/** Specifies the memory must be mapped as inner-cached. */
#define NVOS_MEM_MAP_INNER_WRITEBACK 0x20

/** Maps computer resources into user space.
 *
 *  @param phys The physical address start.
 *  @param size The size of the aperture.
 *  @param attrib Memory attributes (caching).
 *  @param flags Bitwise OR of \c NVOS_MEM_*.
 *  @param [out] ptr A pointer to the result pointer.
 */
NvError
NvOsPhysicalMemMap(NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr);

/** Maps computer resources into user space.
 *
 * This function is intended to be called by device drivers only,
 * and will fail in user space. The virtual address can be allocated
 * by calling NvRmOsPhysicalMemMap() with flags set to ::NVOS_MEM_NONE, which
 * should be done by the calling process. That virtual region will be
 * passed to some device driver, and this function will set up the
 * PTEs to make the virtual space point to the supplied physical
 * address.
 *
 * This is used by NvRmMemMap() to map memory under WinCE6 where user
 * mode applications cannot map physical memory directly.
 *
 *  @param pCallerPtr A pointer to the virtual address from the calling process.
 *  @param phys The physical address start.
 *  @param size The size of the aperture.
 *  @param attrib Memory attributes (caching).
 *  @param flags Bitwise OR of \c NVOS_MEM_*.
 */
NvError
NvOsPhysicalMemMapIntoCaller(void *pCallerPtr, NvOsPhysAddr phys,
    size_t size, NvOsMemAttribute attrib, NvU32 flags);

/**
 * Releases resources previously allocated by NvOsPhysicalMemMap().
 *
 * @param ptr The virtual pointer returned by \c NvOsPhysicalMemMap. If this
 *     pointer is null, this function has no effect.
 * @param size The size of the mapped region.
 */
void NvOsPhysicalMemUnmap(void *ptr, size_t size);

/*@}*/
/** @name Page Allocator
 */
/*@{*/

/**
 *  Low-level memory allocation of the external system memory.
 */
typedef enum
{
    NvOsPageFlags_Contiguous    = 0,
    NvOsPageFlags_NonContiguous = 1,

    NvOsMemFlags_Forceword = 0x7ffffff,
} NvOsPageFlags;

typedef struct NvOsPageAllocRec *NvOsPageAllocHandle;

/** Allocates memory via the page allocator.
 *
 *  @param size The number of bytes to allocate.
 *  @param attrib Page caching attributes.
 *  @param flags Various memory allocation flags.
 *  @param protect Page protection attributes (\c NVOS_MEM_*).
 *  @param [out] descriptor A pointer to the result descriptor.
 *
 *  @return A descriptor (not a pointer to virtual memory),
 *  which may be passed into other functions.
 */
NvError
NvOsPageAlloc(size_t size, NvOsMemAttribute attrib,
    NvOsPageFlags flags, NvU32 protect, NvOsPageAllocHandle *descriptor);

/**
 * Locks down the pages in a region of memory and provides a descriptor that can
 * be used to query the PTEs. Locked pages are guaranteed to not be swapped
 * out or moved by the OS. To unlock the pages when done, call NvOsPageFree()
 * on the resulting descriptor.
 *
 * @note Some operating systems may not support this operation and will return
 *     \c NvError_NotImplemented to all requests.
 *
 * @param ptr A pointer to the buffer to lock down.
 * @param size Number of bytes in the buffer to lock down.
 * @param protect Page protection attributes (NVOS_MEM_*).
 * @param [out] descriptor A pointer to the output parameter to pass back
 *     the descriptor.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvOsPageLock(void *ptr, size_t size, NvU32 protect, NvOsPageAllocHandle *descriptor);

/** Frees pages from NvOsPageAlloc().
 *
 *  It is not valid to call NvOsPageFree() while there are outstanding
 *  mappings.
 *
 *  @param descriptor The descriptor from \c NvOsPageAlloc.
 */
void
NvOsPageFree(NvOsPageAllocHandle descriptor);

/** Maps pages into the virtual address space.
 *
 *  Upon successful completion, \a *ptr holds a virtual address
 *  that may be accessed.
 *
 *  @param descriptor Allocated pages from NvOsPageAlloc(), etc.
 *  @param offset Offset in bytes into the page range.
 *  @param size The size of the mapping.
 *  @param [out] ptr A pointer to the result pointer.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvOsPageMap(NvOsPageAllocHandle descriptor, size_t offset, size_t size,
    void **ptr);

/** Maps pages into the provided virtual address space.
 *
 *  Virtual address space can be obtained by calling
 *  NvOsPhysicalMemMap() and passing ::NVOS_MEM_NONE for the
 *  flags parameter.
 *
 * @note You should only use this function if you really, really
 *       know what you are doing(1).
 *
 *  Upon successful completion, \a *ptr holds a virtual address
 *  that may be accessed.
 *
 *  @param descriptor Allocated pages from NvOsPageAlloc(), etc.
 *  @param pCallerPtr Pointer to user supplied virtual address space.
 *  @param offset Offset in bytes into the page range
 *  @param size The size of the mapping
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvOsPageMapIntoPtr(NvOsPageAllocHandle descriptor, void *pCallerPtr,
    size_t offset, size_t size);

/** Unmaps the virtual address from NvOsPageMap().
 *
 *  @param descriptor Allocated pages from NvOsPageAlloc(), etc.
 *  @param ptr A pointer to the virtual address to unmap that was returned
 *      from \c NvOsPageMap.
 *  @param size The size of the mapping, which should match what
 *   was passed into \c NvOsPageMap.
 */
void
NvOsPageUnmap(NvOsPageAllocHandle descriptor, void *ptr, size_t size);

/** Returns the physical address given an offset.
 *
 *  This is useful for non-contiguous page allocations.
 *
 *  @param descriptor The descriptor from NvOsPageAlloc(), etc.
 *  @param offset The offset in bytes into the page range.
 */
NvOsPhysAddr
NvOsPageAddress(NvOsPageAllocHandle descriptor, size_t offset);

/*@}*/
/** @name Dynamic Library Handling
 */
/*@{*/

/** A handle to a dynamic library. */
typedef struct NvOsLibraryRec *NvOsLibraryHandle;

/** Load a dynamic library.
 *
 *  No operating system specific suffixes or paths should be used for the
 *  library name. So do not use:
 *
 *  <pre>
        /usr/lib/libnvos.so
        libnvos.dll
    </pre>
 *
 * Just use:
 *  <pre>
    libnvos
   </pre>
 *
 *  @param name A pointer to the library name.
 *  @param [out] library A pointer to the result library.
 *
 *  @retval NvError_LibraryNotFound If the library cannot be opened.
 */
NvError
NvOsLibraryLoad(const char *name, NvOsLibraryHandle *library);

/** Gets an address of a symbol in a dynamic library.
 *
 *  @param library The dynamic library.
 *  @param symbol A pointer to the symbol to lookup.
 *
 *  @return The address of the symbol, or NULL if the symbol cannot be found.
 */
void*
NvOsLibraryGetSymbol(NvOsLibraryHandle library, const char *symbol);

/** Unloads a dynamic library.
 *
 *  @param library The dynamic library to unload.
 *      It is okay to pass a null \a library value.
 */
void
NvOsLibraryUnload(NvOsLibraryHandle library);

/*@}*/
/** @name Syncronization Objects and Thread Management
 */
/*@{*/

typedef struct NvOsMutexRec *NvOsMutexHandle;
typedef struct NvOsIntrMutexRec *NvOsIntrMutexHandle;
typedef struct NvOsConditionRec *NvOsConditionHandle;
typedef struct NvOsSemaphoreRec *NvOsSemaphoreHandle;
typedef struct NvOsThreadRec *NvOsThreadHandle;

/** Unschedules the calling thread for at least the given
 *      number of milliseconds.
 *
 *  Other threads may run during the sleep time.
 *
 *  @param msec The number of milliseconds to sleep.
 */
void
NvOsSleepMS(NvU32 msec);

/** Stalls the calling thread for at least the given number of
 *  microseconds. The actual time waited might be longer; you cannot
 *  depend on this function for precise timing.
 *
 *  @note It is safe to use this function at ISR time.
 *
 *  @param usec The number of microseconds to wait.
 */
void
NvOsWaitUS(NvU32 usec);

/**
 * Allocates a new process-local mutex.
 *
 * @note Mutexes can be locked recursively; if a thread owns the lock,
 * it can lock it again as long as it unlocks it an equal number of times.
 *
 * @param mutex The mutex to initialize.
 *
 * @return \a NvError_MutexCreateFailed, or one of common error codes on
 * failure.
 */
NvError NvOsMutexCreate(NvOsMutexHandle *mutex);

/** Locks the given unlocked mutex.
 *
 *  If a process is holding a lock on a multi-process mutex when it terminates,
 *  this lock will be automatically released.
 *
 *  @param mutex The mutex to lock; note that this is a recursive lock.
 */
void NvOsMutexLock(NvOsMutexHandle mutex);

/** Unlocks a locked mutex.
 *
 *  A mutex must be unlocked exactly as many times as it has been locked.
 *
 *  @param mutex The mutex to unlock.
 */
void NvOsMutexUnlock(NvOsMutexHandle mutex);

/** Frees the resources held by a mutex.
 *
 *  Mutecies are reference counted and a given mutex will not be destroyed
 *  until the last reference has gone away.
 *
 *  @param mutex The mutex to destroy. Passing in a null mutex is okay.
 */
void NvOsMutexDestroy(NvOsMutexHandle mutex);

/**
 * Creates a mutex that is safe to aquire in an ISR.
 *
 * @param mutex A pointer to the mutex is stored here on success.
 */
NvError NvOsIntrMutexCreate(NvOsIntrMutexHandle *mutex);

/**
 * Aquire an ISR-safe mutex.
 *
 * @param mutex The mutex to lock. For kernel (OAL) implementations,
 *     NULL implies the system-wide lock will be used.
 */
void NvOsIntrMutexLock(NvOsIntrMutexHandle mutex);

/**
 * Releases an ISR-safe mutex.
 *
 * @param mutex The mutex to unlock. For kernel (OAL) implementations,
 *     NULL implies the system-wide lock will be used.
 */
void NvOsIntrMutexUnlock(NvOsIntrMutexHandle mutex);

/**
 * Destroys an ISR-safe mutex.
 *
 * @param mutex The mutex to destroy. If \a mutex is NULL, this API has no
 *     effect.
 */
void NvOsIntrMutexDestroy(NvOsIntrMutexHandle mutex);

/**
 * Creates a condition variable.
 *
 * @param cond A pointer to the condition variable to initialize.
 *
 * @retval NvSuccess if successful, or the appropriate error code.
 * @retval NvError_InsufficientMemory if insufficient memory exists.
 * @retval NvError_Busy if there are insufficient resources.
 */
NvError NvOsConditionCreate(NvOsConditionHandle *cond);

/**
 * Frees resources held by the condition variable.
 *
 * @param cond The condition variable to destroy.
 *
 * @retval NvSuccess if successful.
 * @retval NvError_BadParameter if condition variable is not valid.
 * @retval NvError_Busy if the condition variable is currently being used in
 *     another thread.
 */
NvError NvOsConditionDestroy(NvOsConditionHandle cond);

/**
 * Unblocks all threads currently blocked on the specified condition variable.
 *
 * @param cond The condition variable on which other threads may be blocked.
 *
 * @retval NvError_BadParameter if the condition variable is not valid.
 */
NvError NvOsConditionBroadcast(NvOsConditionHandle cond);

/**
 * Unblocks atleast one of the threads that are blocked on the specified
 *    condition variable.
 *
 * @param cond The condition variable on which other threads may be blocked.
 *
 * @retval NvError_BadParameter if condition variable is not valid.
 */
NvError NvOsConditionSignal(NvOsConditionHandle cond);

/**
 * Atomically releases the mutex and causes the calling thread to block on
 *    the condition variable. This function must be called with mutex locked
 *    by the calling thread or undefined behaviour results. Upon successful
 *    return, the mutex shall have been locked and be owned by the calling
 *    thread.
 *
 *  @param cond The condition variable to wait on.
 *  @param mutex The associated mutex that needs to be locked by the calling
 *      thread.
 *
 *  @retval NvSuccess if successful.
 *  @retval NVError_AccessDenied if the mutex was not owned by the calling
 *      thread.
 * @retval NvError_BadParameter if condition variable or mutex is not valid.
 */
NvError NvOsConditionWait(NvOsConditionHandle cond, NvOsMutexHandle mutex);

/**
 * Atomically releases the mutex and causes the calling thread to block on
 *     the condition variable with a timeout. Must be called with mutex locked
 *     by the calling thread or undefined behaviour results. Upon successful
 *     return, the mutex shall have been locked and be owned by the calling
 *     thread
 *
 *  @param cond The condition variable to wait on.
 *  @param mutex The associated mutex that needs to be locked by the calling
 *     thread.
 *  @param microsecs Time specified in microseconds for timeout to occur.
 *
 *  @retval NvSuccess if successful.
 *  @retval NVError_AccessDenied if the mutex was not owned by the calling
 *      thread.
 *  @retval NVError_Timeout if the time specified by microsecs has passed
 *      before the condition variable is signalled  or broadcast.
 * @retval NvError_BadParameter if condition variable or mutex is not valid.
 */
NvError NvOsConditionWaitTimeout(NvOsConditionHandle cond, NvOsMutexHandle mutex, NvU32 microsecs);

/**
 * Creates a counting semaphore.
 *
 * @param semaphore A pointer to the semaphore to initialize.
 * @param value The initial semaphore value.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvOsSemaphoreCreate(NvOsSemaphoreHandle *semaphore, NvU32 value);

/**
 * Creates a duplicate semaphore from the given semaphore.
 * Freeing the original semaphore has no effect on the new semaphore.
 *
 * @param orig The semaphore to duplicate.
 * @param semaphore A pointer to the new semaphore.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvOsSemaphoreClone( NvOsSemaphoreHandle orig,  NvOsSemaphoreHandle *semaphore);

/**
 * Obtains a safe, usable handle to a semaphore passed across an ioctl()
 * interface by a client to a device driver. Validates that the original
 * semaphore handle is legal, and creates a new handle (valid in the driver's
 * process/address space) that the client cannot asynchronously destroy.
 *
 * The new handle must be freed, just like any other semaphore handle, by
 * passing it to NvOsSemaphoreDestroy().
 *
 * @param hClientSema The client's semaphore handle.
 * @param phDriverSema If successful, returns a new handle to the semaphore
 *     that the driver can safely use.
 *
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvOsSemaphoreUnmarshal( NvOsSemaphoreHandle hClientSema,
    NvOsSemaphoreHandle *phDriverSema);

/** Waits until the semaphore value becomes non-zero, then
 *  decrements the value and returns.
 *
 *  @param semaphore The semaphore to wait for.
 */
void NvOsSemaphoreWait(NvOsSemaphoreHandle semaphore);

/**
 * Waits for the given semaphore value to become non-zero with timeout. If
 * the semaphore value becomes non-zero before the timeout, then the value is
 * decremented and \a NvSuccess is returned.
 *
 * @param semaphore The semaphore to wait for.
 * @param msec Timeout value in milliseconds.
 *     \c NV_WAIT_INFINITE can be used to wait forever.
 *
 * @retval NvError_Timeout If the wait expires.
 */
NvError
NvOsSemaphoreWaitTimeout(NvOsSemaphoreHandle semaphore, NvU32 msec);

/** Increments the semaphore value.
 *
 *  @param semaphore The semaphore to signal.
 */
void
NvOsSemaphoreSignal(NvOsSemaphoreHandle semaphore);

/** Frees resources held by the semaphore.
 *
 *  Semaphores are reference counted across the computer (multiproceses),
 *  and a given semaphore will not be destroyed until the last reference has
 *  gone away.
 *
 *  @param semaphore The semaphore to destroy.
 *      Passing in a null semaphore is okay (no op).
 */
void
NvOsSemaphoreDestroy(NvOsSemaphoreHandle semaphore);

/** Sets thread mode.
 *
 * @pre If this is called, it must be called before any other threading function.
 * All but the first call to this function do nothing and return
 * \c NvError_AlreadyAllocated.
 *
 * @param coop 0 to disable coop mode, and 1 to enable coop mode.
 *
 * @returns NvSuccess On success.
 * @retval NvError_AlreadyAllocated If called previously.
 */
NvError NvOsThreadMode(int coop);

/** Entry point for a thread.
 */
typedef void (*NvOsThreadFunction)(void *args);

/** Creates a thread.
 *
 *  @param function The thread entry point.
 *  @param args A pointer to the thread arguments.
 *  @param [out] thread A pointer to the result thread ID structure.
 */
NvError
NvOsThreadCreate( NvOsThreadFunction function, void *args,
    NvOsThreadHandle *thread);

/** Creates a near interrupt priority thread.
 *
 *  @param function The thread entry point.
 *  @param args A pointer to the thread arguments.
 *  @param [out] thread A pointer to the result thread ID structure.
 */
NvError
NvOsInterruptPriorityThreadCreate( NvOsThreadFunction function, void *args,
    NvOsThreadHandle *thread);

/**
 * Sets the thread's priority to low priority.
 *
 * @retval NvError_NotSupported May be returned.
 */
NvError NvOsThreadSetLowPriority(void);

/** Waits for the given thread to exit.
 *
 *  The joined thread will be destroyed automatically. All OS resources
 *  will be reclaimed. There is no method for terminating a thread
 *  before it exits naturally.
 *
 *  @param thread The thread to wait for.
 *  Passing in a null thread ID is okay (no op).
 */
void NvOsThreadJoin(NvOsThreadHandle thread);

/** Yields to another runnable thread.
 */
void NvOsThreadYield(void);

/**
 * Atomically compares the contents of a 32-bit memory location with a value,
 * and if they match, updates it to a new value. This function is the
 * equivalent of the following code, except that other threads or processors
 * are effectively prevented from reading or writing \a *pTarget while we are
 * inside the function.
 *
 * @code
 * NvS32 OldTarget = *pTarget;
 * if (OldTarget == OldValue)
 *     *pTarget = NewValue;
 * return OldTarget;
 * @endcode
 */
NvS32 NvOsAtomicCompareExchange32(NvS32 *pTarget, NvS32 OldValue, NvS32
    NewValue);

/**
 * Atomically swaps the contents of a 32-bit memory location with a value. This
 * function is the equivalent of the following code, except that other threads
 * or processors are effectively prevented from reading or writing \a *pTarget
 * while we are inside the function.
 *
 * @code
 * NvS32 OldTarget = *pTarget;
 * *pTarget = Value;
 * return OldTarget;
 * @endcode
 */
NvS32 NvOsAtomicExchange32(NvS32 *pTarget, NvS32 Value);

/**
 * Atomically increments the contents of a 32-bit memory location by a specified
 * amount. This function is the equivalent of the following code, except that
 * other threads or processors are effectively prevented from reading or
 * writing \a *pTarget while we are inside the function.
 *
 * @code
 * NvS32 OldTarget = *pTarget;
 * *pTarget = OldTarget + Value;
 * return OldTarget;
 * @endcode
 */
NvS32 NvOsAtomicExchangeAdd32(NvS32 *pTarget, NvS32 Value);

/** A TLS index that is guaranteed to be invalid. */
#define NVOS_INVALID_TLS_INDEX 0xFFFFFFFF
#define NVOS_TLS_CNT            4

/**
 * Allocates a thread-local storage variable. All TLS variables have initial
 * value NULL in all threads when first allocated.
 *
 * @returns The TLS index of the TLS variable if successful, or
 *     ::NVOS_INVALID_TLS_INDEX if not.
 */
NvU32 NvOsTlsAlloc(void);

/**
 * Frees a thread-local storage variable.
 *
 * @param TlsIndex The TLS index of the TLS variable. This function is a no-op
 *     if TlsIndex equals ::NVOS_INVALID_TLS_INDEX.
 */
void NvOsTlsFree(NvU32 TlsIndex);

/**
 * Gets the value of a thread-local storage variable.
 *
 * @param TlsIndex The TLS index of the TLS variable.
 *     The current value of the TLS variable is returned.
 */
void *NvOsTlsGet(NvU32 TlsIndex);

/**
 * Sets the value of a thread-local storage variable.
 *
 * @param TlsIndex The TLS index of the TLS variable.
 * @param Value A pointer to the new value of the TLS variable.
 */
void NvOsTlsSet(NvU32 TlsIndex, void *Value);

/**
 * Registers a function that should be called when the thread terminates
 * to clean up any structures stored in thread-local storage.
 *
 * @param func The callback function called when the thread terminates.
 * @param context A pointer to the data to pass to the callback function.
 */
NvError NvOsTlsAddTerminator(void (*func)(void *), void *context);

/**
 * Checks the list of existing terminator functions for one that matches
 * both the function and the data, and then removes that function from the
 * list.
 *
 * @param func The callback function called when the thread terminates.
 * @param context A pointer to the data to pass to the callback function.
 * @returns True if the terminator exists and was removed, otherwise false if not.
 */
NvBool NvOsTlsRemoveTerminator(void (*func)(void *), void *context);

/*@}*/
/** @name Time Functions
 */
/*@{*/

/**
 * @brief Defines the system time structure.
 */
typedef struct NvOsSystemTimeRec
{
    NvU32   Seconds;
    NvU32   Milliseconds;
} NvOsSystemTime;

/** Gets system realtime clock.
 *
 * @param hNvOsSystemtime A pointer to the system handle used to set the time.
 */
NvError
NvOsGetSystemTime(NvOsSystemTime *hNvOsSystemtime);

/** Sets system realtime clock.
 *
 * @param hNvOsSystemtime A pointer to the system handle used to get the time.
 */
NvError
NvOsSetSystemTime(NvOsSystemTime *hNvOsSystemtime);

/** @return The system time in milliseconds.
 *
 *  The returned values are guaranteed to be monotonically increasing,
 *  but may wrap back to zero (after about 50 days of runtime).
 *
 *  In some systems, this is the number of milliseconds since power-on,
 *  or may actually be an accurate date.
 */
NvU32
NvOsGetTimeMS(void);

/** @return The system time in microseconds.
 *
 *  The returned values are guaranteed to be monotonically increasing,
 *  but may wrap back to zero.
 *
 *  Some systems cannot gauantee a microsecond resolution timer.
 *  Even though the time returned is in microseconds, it is not gaurnateed
 *  to have micro-second resolution.
 *
 *  Please be advised that this API is mainly used for code profiling and
 *  meant to be used direclty in driver code.
 */
NvU64
NvOsGetTimeUS(void);

/*@}*/
/** @name CPU Cache
 *  Cache operations for both instruction and data cache, implemented
 *  per processor.
 */
/*@{*/

/** Writes back the entire data cache.
 */
void
NvOsDataCacheWriteback(void);

/** Writes back and invalidates the entire data cache.
 */
void
NvOsDataCacheWritebackInvalidate(void);

/** Writes back a range of the data cache.
 *
 *  @param start A pointer to the start address.
 *  @param length The number of bytes to write back.
 */
void
NvOsDataCacheWritebackRange(void *start, NvU32 length);

/** Writes back and invlidates a range of the data cache.
 *
 *  @param start A pointer to the start address.
 *  @param length The number of bytes to write back.
 */
void
NvOsDataCacheWritebackInvalidateRange(void *start, NvU32 length);

/** Invalidates the entire instruction cache.
 */
void
NvOsInstrCacheInvalidate(void);

/** Invalidates a range of the instruction cache.
 *
 *  @param start A pointer to the start address.
 *  @param length The number of bytes.
 */
void
NvOsInstrCacheInvalidateRange(void *start, NvU32 length);

/** Checks and returns whether the given memory address is within
 *  memory of given attribute.
 *
 *  @param Attrib Memory attribute to check.
 *  @param Addr Address of the memory, which is to be checked.
 *  @retval NV_TRUE if memory pointed by \a Addr is of given attribute,
 *          NV_FASLE otherwise.
 */
NvBool
NvOsIsMemoryOfGivenType(NvOsMemAttribute Attrib, NvU32 Addr);

/** Flushes the CPU's write combine buffer.
 */
void
NvOsFlushWriteCombineBuffer(void);

/** Interrupt handler function.
 */
typedef void (*NvOsInterruptHandler)(void *args);

/** Interrupt handler type.
 */
typedef struct NvOsInterruptRec *NvOsInterruptHandle;

/**
 * Registers the interrupt handler with the IRQ number.
 *
 * @note This function is intended to @b only be called
 *       from NvRmInterruptRegister().
 *
 * @param IrqListSize Size of the \a IrqList passed in for registering the IRQ
 *      handlers for each IRQ number.
 * @param pIrqList Array of IRQ numbers for which interupt handlers are to be
 *     registerd.
 * @param pIrqHandlerList A pointer to an array of interrupt routines to be
 *      called when an interrupt occurs.
 * @param context A pointer to the register's context handle.
 * @param handle A pointer to the interrupt handle.
 * @param InterruptEnable If true, immediately enable interrupt.  Otherwise
 *      enable interrupt only after calling NvOsInterruptEnable().
 *
 * @retval NvError_IrqRegistrationFailed If the interrupt is already registered.
 * @retval NvError_BadParameter If the IRQ number is not valid.
 */
NvError
NvOsInterruptRegister(NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable);

/**
 * Unregisters the interrupt handler from the associated IRQ number.
 *
 * @note This function is intended to @b only be called
 *       from NvRmInterruptUnregister().
 *
 * @param handle interrupt Handle returned when a successfull call is made to
 *     NvOsInterruptRegister().
 */
void
NvOsInterruptUnregister(NvOsInterruptHandle handle);

/**
 * Enables the interrupt handler with the IRQ number.
 *
 * @note This function is intended to @b only be called
 *       from NvOsInterruptRegister() and NvRmInterruptRegister().
 *
 * @param handle Interrupt handle returned when a successfull call is made to
 *     \c NvOsInterruptRegister.
 *
 * @retval NvError_BadParameter If the handle is not valid.
 * @retval NvError_InsufficientMemory If interrupt enable failed.
 * @retval NvSuccess If interrupt enable is successful.
 */
NvError
NvOsInterruptEnable(NvOsInterruptHandle handle);

/**
 *  Called when the ISR/IST is done handling the interrupt.
 *
 *  @note This API should be called only from NvRmInterruptDone().
 *
 * @param handle Interrupt handle returned when a successfull call is made to
 *     NvOsInterruptRegister().
 */
void
NvOsInterruptDone(NvOsInterruptHandle handle);

/**
 * Mask/unmask an interrupt.
 *
 * Drivers can use this API to fend off interrupts. Mask means no interrupts
 * are forwarded to the CPU. Unmask means, interrupts are forwarded to the
 * CPU. In case of SMP systems, this API masks the interrutps to all the CPUs,
 * not just the calling CPU.
 *
 * @param handle    Interrupt handle returned by NvOsInterruptRegister().
 * @param mask      NV_FALSE to forward the interrupt to CPU; NV_TRUE to
 *     mask the interrupts to CPU.
 */
void NvOsInterruptMask(NvOsInterruptHandle handle, NvBool mask);

#define NVOS_MAX_PROFILE_APERTURES  (4UL)

/**
 * Profile aperture sizes.
 *
 * Code may execute and be profiled from mutliple apertures. This will get the
 * size of each aperture. The caller is expected to allocate the number of
 * bytes for each aperture into a single void* array (void**), which will be
 * used in NvOsProfileStart() and NvOsProfileStop().
 *
 * This may be called twice, the first time to get the number of apertures
 * (sizes should be null), and the second time with the sizes parameter
 * non-null. Alternately, ::NVOS_MAX_PROFILE_APERTURES may be used as the
 * size of the sizes array.
 *
 * @param apertures A pointer to the number of apertures that will be profiled.
 * @param sizes A pointer to the size of each aperture.
 */
void
NvOsProfileApertureSizes( NvU32 *apertures, NvU32 *sizes );

/**
 * Enables statistical profiling.
 *
 * @param apertures A pointer to an array of storage for profile data.
 */
void
NvOsProfileStart( void **apertures );

/**
 * Stops profiling and prepares the profile samples for analysis.
 *
 * @param apertures A pointer to the storage for the profile samples.
 */
void
NvOsProfileStop( void **apertures );

/**
 * Writes profile data to the given file.
 *
 * @post This is expected to close the file after a successful write.
 *
 * @param file The file to write to.
 * @param index The aperture number.
 * @param aperture A pointer to the storage for the profile samples.
 */
NvError
NvOsProfileWrite( NvOsFileHandle file, NvU32 index, void *aperture );

/**
 * Sets the CPU affinity for current thread. \a CpuHint is the CPU number starting
 * from 0 (the primary boot CPU) to N-1, where N is the total number of CPUs in
 * the system. This hint is ignored if \a CpuHint is larger than the number
 * of CPUs in the system, or the system cannot put the thread on the given CPU
 * for any reason.
 *
 * @param CpuHint [in]CPU number.
 */
void
NvOsThreadSetAffinity(NvU32 CpuHint);

#define NVOS_INVALID_CPU_AFFINITY ((NvU32)(-1))

/**
 * Gets the caller thread affinity on which CPU it runs.
 *
 * @return CPU mask for where current thread affinity exists, or
 *         ::NVOS_INVALID_CPU_AFFINITY when any error occurs.
 */
NvU32
NvOsThreadGetAffinity(void);


/*
 * Tracing support. Enable with NVOS_TRACE in nvos_trace.h.
 */
#if NVOS_TRACE || NV_DEBUG

#if NV_DEBUG
void *NvOsAllocLeak( size_t size, const char *f, int l );
void *NvOsReallocLeak( void *ptr, size_t size, const char *f, int l );
void NvOsFreeLeak( void *ptr, const char *f, int l );
#endif

static NV_INLINE void *NvOsAllocTraced(size_t size, const char *f, int l)
{
    void *ptr;

#if NV_DEBUG
    ptr = (NvOsAllocLeak)(size, f, l);
#else
    ptr = (NvOsAlloc)(size);
#endif
#if NVOS_TRACE
    NVOS_TRACE_LOG_PRINTF(("NvOsAlloc, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)ptr));
#endif

    return ptr;
}

static NV_INLINE void *NvOsReallocTraced(void *ptr, size_t size, const char *f,
    int l )
{
    void* ret;

#if NV_DEBUG
    ret = (NvOsReallocLeak)(ptr, size, f, l);
#else
    ret = (NvOsRealloc)(ptr, size);
#endif
#if NVOS_TRACE
    NVOS_TRACE_LOG_PRINTF(("NvOsRealloc, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)ret));
#endif

    return ret;
}

static NV_INLINE void NvOsFreeTraced(void *ptr, const char *f, int l )
{

#if NV_DEBUG
    (NvOsFreeLeak)(ptr, f, l);
#else
    (NvOsFree)(ptr);
#endif
#if NVOS_TRACE
    NVOS_TRACE_LOG_PRINTF(("NvOsFree, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)ptr));
#endif
}


#define NvOsAlloc(size) NvOsAllocTraced(size, __FILE__, __LINE__)
#define NvOsRealloc(ptr, size) \
    NvOsReallocTraced(ptr, size, __FILE__, __LINE__)
#define NvOsFree(ptr) NvOsFreeTraced(ptr, __FILE__, __LINE__)

#endif /* NVOS_TRACE */


#if (NVOS_TRACE || NV_DEBUG)

/**
 * Sets the file and line corresponding to a resource allocation.
 * Call will fill file and line for the most recently stored
 * allocation location, if not already set.
 *
 * @param userptr A pointer to used by client to identify resource.
 *     Can be NULL, which leads to no-op.
 * @param file A pointer to the name of the file from which allocation
 *     originated. Value cannot be NULL; use "" for an empty string.
 * @param l The line.
 */
void NvOsSetResourceAllocFileLine(void* userptr, const char* file, int line);

static NV_INLINE void *
NvOsExecAllocTraced(size_t size, const char *f, int l )
{
    void* ret;
    ret = (NvOsExecAlloc)(size);
    NvOsSetResourceAllocFileLine(ret, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsExecAlloc, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)ret));
    return ret;
}

static NV_INLINE void
NvOsExecFreeTraced(void *ptr, size_t size, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsExecFree, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)ptr));
    (NvOsExecFree)(ptr, size);
}

static NV_INLINE NvError
NvOsSharedMemAllocTraced(const char *key, size_t size,
    NvOsSharedMemHandle *descriptor, const char *f, int l )
{
    NvError status;
    status = (NvOsSharedMemAlloc)(key, size, descriptor);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*descriptor, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsSharedMemAlloc, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*descriptor)));
    return status;
}

static NV_INLINE NvError
NvOsSharedMemHandleFromFdTraced(int fd,
    NvOsSharedMemHandle *descriptor, const char *f, int l )
{
    NvError status;
    status = (NvOsSharedMemHandleFromFd)(fd, descriptor);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*descriptor, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsSharedMemHandleFromFd, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*descriptor)));
    return status;
}

static NV_INLINE NvError
NvOsSharedMemGetFdTraced(NvOsSharedMemHandle descriptor,
     int *fd, const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsSharedMemGetFd)(descriptor, fd);
    NVOS_TRACE_LOG_PRINTF(("NvOsSharedMemGetFd, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*descriptor)));
    return status;
}

static NV_INLINE NvError
NvOsSharedMemMapTraced(NvOsSharedMemHandle descriptor, size_t offset,
    size_t size, void **ptr, const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsSharedMemMap)(descriptor, offset, size, ptr);
    NVOS_TRACE_LOG_PRINTF(("NvOsSharedMemMap, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*ptr)));
    return status;
}

static NV_INLINE void
NvOsSharedMemUnmapTraced(void *ptr, size_t size, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsSharedMemUnmap, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(ptr)));
    (NvOsSharedMemUnmap)(ptr, size);
}

static NV_INLINE void
NvOsSharedMemFreeTraced(NvOsSharedMemHandle descriptor, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsSharedMemFree, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(descriptor)));
    (NvOsSharedMemFree)(descriptor);
}

static NV_INLINE NvError
NvOsPhysicalMemMapTraced(NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr, const char *f, int l )
{
    NvError status;
    status = (NvOsPhysicalMemMap)(phys, size, attrib, flags, ptr);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*ptr, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsPhysicalMemMap, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*ptr)));
    return status;
}

static NV_INLINE NvError
NvOsPhysicalMemMapIntoCallerTraced( void *pCallerPtr, NvOsPhysAddr phys,
    size_t size, NvOsMemAttribute attrib, NvU32 flags, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsPhysicalMemMapIntoCaller, \
        %s, %d, %ums, 0x%x\n", f, l, NvOsGetTimeMS(), (NvU32)(pCallerPtr)));
    return (NvOsPhysicalMemMapIntoCaller)(pCallerPtr, phys, size, attrib,
        flags);
}

static NV_INLINE void
NvOsPhysicalMemUnmapTraced(void *ptr, size_t size, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
     NVOS_TRACE_LOG_PRINTF(("NvOsPhysicalMemUnmap, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(ptr)));
     (NvOsPhysicalMemUnmap)(ptr, size);
}

static NV_INLINE NvError
NvOsPageAllocTraced(size_t size, NvOsMemAttribute attrib,
    NvOsPageFlags flags, NvU32 protect, NvOsPageAllocHandle *descriptor,
    const char *f, int l )
{
    NvError status;
    status = (NvOsPageAlloc)(size, attrib, flags, protect, descriptor);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*descriptor, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsPageAlloc, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(descriptor)));
    return status;
}

static NV_INLINE NvError
NvOsPageLockTraced(void *ptr, size_t size, NvU32 protect, NvOsPageAllocHandle* descriptor,
    const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsPageLock)(ptr, size, protect, descriptor);
    NVOS_TRACE_LOG_PRINTF(("NvOsPageLock, %s, %d, %ums, 0x%x, %d, 0x%x, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(ptr), size, protect, (NvU32)*descriptor));
    return status;
}

static NV_INLINE void
NvOsPageFreeTraced(NvOsPageAllocHandle descriptor, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsPageFree, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(descriptor)));
    (NvOsPageFree)(descriptor);
}

static NV_INLINE NvError
NvOsPageMapTraced(NvOsPageAllocHandle descriptor, size_t offset, size_t size,
    void **ptr, const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsPageMap)(descriptor, offset, size, ptr);
    NVOS_TRACE_LOG_PRINTF(("NvOsPageMap, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*ptr)));
    return status;
}

static NV_INLINE NvError
NvOsPageMapIntoPtrTraced( NvOsPageAllocHandle descriptor, void *pCallerPtr,
    size_t offset, size_t size, const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsPageMapIntoPtr)(descriptor, pCallerPtr, offset, size);
    NVOS_TRACE_LOG_PRINTF(("NvOsPageMapIntoCaller, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(pCallerPtr)));
    return status;
}

static NV_INLINE void
NvOsPageUnmapTraced(NvOsPageAllocHandle descriptor, void *ptr, size_t size,
    const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsPageUnmap, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(ptr)));
    (NvOsPageUnmap)(descriptor, ptr, size);
}

static NV_INLINE NvOsPhysAddr
NvOsPageAddressTraced(NvOsPageAllocHandle descriptor, size_t offset,
    const char *f, int l )
{
    NvOsPhysAddr PhysAddr;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    PhysAddr = (NvOsPageAddress)(descriptor, offset);
    NVOS_TRACE_LOG_PRINTF(("NvOsPageAddress, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(PhysAddr)));
    return PhysAddr;
}

static NV_INLINE NvError
NvOsMutexCreateTraced(NvOsMutexHandle *mutex, const char *f, int l )
{
    NvError status;
    status = (NvOsMutexCreate)(mutex);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*mutex, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsMutexCreate, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*mutex)));
    return status;
}

static NV_INLINE void
NvOsMutexLockTraced(NvOsMutexHandle mutex, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsMutexLock, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)mutex));
    (NvOsMutexLock)(mutex);
}

static NV_INLINE void
NvOsMutexUnlockTraced(NvOsMutexHandle mutex, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsMutexUnlock, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)mutex));
    (NvOsMutexUnlock)(mutex);
}

static NV_INLINE void NvOsMutexDestroyTraced(
                            NvOsMutexHandle mutex,
                            const char *f,
                            int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsMutexDestroy, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)mutex));
    (NvOsMutexDestroy)(mutex);
}

static NV_INLINE NvError
NvOsIntrMutexCreateTraced(NvOsIntrMutexHandle *mutex, const char *f, int l )
{
    NvError status;
    status = (NvOsIntrMutexCreate)(mutex);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*mutex, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsIntrMutexCreate, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*mutex)));
    return status;
}

static NV_INLINE void
NvOsIntrMutexLockTraced(NvOsIntrMutexHandle mutex, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsIntrMutexLock, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)mutex));
    (NvOsIntrMutexLock)(mutex);
}

static NV_INLINE void
NvOsIntrMutexUnlockTraced(NvOsIntrMutexHandle mutex, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsIntrMutexUnlock, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)mutex));
    (NvOsIntrMutexUnlock)(mutex);
}

static NV_INLINE void
NvOsIntrMutexDestroyTraced(NvOsIntrMutexHandle mutex, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsIntrMutexDestroy, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)mutex));
    (NvOsIntrMutexDestroy)(mutex);
}

static NV_INLINE NvError
NvOsSemaphoreCreateTraced( NvOsSemaphoreHandle *semaphore,  NvU32 value,
    const char *f, int l )
{
    NvError status;
    status = (NvOsSemaphoreCreate)(semaphore, value);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*semaphore, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreCreate, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*semaphore)));
    return status;
}

static NV_INLINE NvError
NvOsSemaphoreCloneTraced( NvOsSemaphoreHandle orig, NvOsSemaphoreHandle *clone,
    const char *f, int l )
{
    NvError status;
    status = (NvOsSemaphoreClone)(orig, clone);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*clone, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreClone, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*clone)));
    return status;
}

static NV_INLINE NvError
NvOsSemaphoreUnmarshalTraced( NvOsSemaphoreHandle hClientSema,
    NvOsSemaphoreHandle *phDriverSema, const char *f, int l )
{
    NvError status;
    status = (NvOsSemaphoreUnmarshal)(hClientSema, phDriverSema);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*phDriverSema, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreUnmarshal, %s, %d, %ums, 0x%x\r\n",
        f, l, NvOsGetTimeMS(), (NvU32)(hClientSema)));
    return status;
}

static NV_INLINE void
NvOsSemaphoreWaitTraced( NvOsSemaphoreHandle semaphore, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreWait, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)semaphore));
    (NvOsSemaphoreWait)(semaphore);
}

static NV_INLINE NvError
NvOsSemaphoreWaitTimeoutTraced( NvOsSemaphoreHandle semaphore, NvU32 msec,
    const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreWaitTimeout, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)semaphore));
    status = (NvOsSemaphoreWaitTimeout)(semaphore, msec);
    return status;
}

static NV_INLINE void
NvOsSemaphoreSignalTraced( NvOsSemaphoreHandle semaphore, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreSignal, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)semaphore));
    (NvOsSemaphoreSignal)(semaphore);
}

static NV_INLINE void
NvOsSemaphoreDestroyTraced( NvOsSemaphoreHandle semaphore, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsSemaphoreDestory, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)semaphore));
    (NvOsSemaphoreDestroy)(semaphore);
}

static NV_INLINE NvError
NvOsThreadCreateTraced( NvOsThreadFunction function, void *args,
    NvOsThreadHandle *thread, const char *f, int l )
{
    NvError status;
    status = (NvOsThreadCreate)(function, args, thread);
    if (status == NvSuccess)
        NvOsSetResourceAllocFileLine(*thread, f, l);
    NVOS_TRACE_LOG_PRINTF(("NvOsThreadCreate, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(*thread)));
    return status;
}

static NV_INLINE void
NvOsThreadJoinTraced( NvOsThreadHandle thread, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsThreadJoin, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)thread));
    (NvOsThreadJoin)(thread);
}

static NV_INLINE void
NvOsThreadYieldTraced(const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsThreadYield, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)0));
    (NvOsThreadYield)();
}

static NV_INLINE NvError
NvOsInterruptRegisterTraced(NvU32 IrqListSize, const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList, void *context,
    NvOsInterruptHandle *handle, NvBool InterruptEnable, const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsInterruptRegister)(IrqListSize, pIrqList, pIrqHandlerList,
        context, handle, InterruptEnable);
    NVOS_TRACE_LOG_PRINTF(("NvOsInterruptRegister, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(handle)));
    return status;
}

static NV_INLINE void
NvOsInterruptUnregisterTraced(NvOsInterruptHandle handle, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsInterruptUnregister, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(handle)));
    (NvOsInterruptUnregister)(handle);
}

static NV_INLINE NvError
NvOsInterruptEnableTraced(NvOsInterruptHandle handle, const char *f, int l )
{
    NvError status;
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    status = (NvOsInterruptEnable)(handle);
    NVOS_TRACE_LOG_PRINTF(("NvOsInterruptRegister, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(handle)));
    return status;
}

static NV_INLINE void
NvOsInterruptDoneTraced(NvOsInterruptHandle handle, const char *f, int l )
{
    (void) f; /* potentially unused parameter */
    (void) l; /* potentially unused parameter */
    NVOS_TRACE_LOG_PRINTF(("NvOsInterruptDone, %s, %d, %ums, 0x%x\n",
        f, l, NvOsGetTimeMS(), (NvU32)(handle)));
    (NvOsInterruptDone)(handle);
}

#define NvOsExecAlloc(size) NvOsExecAllocTraced(size, __FILE__, __LINE__)
#define NvOsExecFree(ptr, size) \
    NvOsExecFreeTraced(ptr, size, __FILE__, __LINE__)
#define NvOsSharedMemAlloc(key, size, descriptor) \
    NvOsSharedMemAllocTraced(key, size, descriptor, __FILE__, __LINE__)
#define NvOsSharedMemHandleFromFd(fd, descriptor) \
    NvOsSharedMemHandleFromFdTraced(fd, descriptor, __FILE__, __LINE__)
#define NvOsSharedMemGetFd(descriptor, fd) \
    NvOsSharedMemGetFdTraced(descriptor, fd, __FILE__, __LINE__)
#define NvOsSharedMemMap(descriptor, offset, size, ptr) \
    NvOsSharedMemMapTraced(descriptor, offset, size, ptr, __FILE__, __LINE__)
#define NvOsSharedMemUnmap(ptr, size) \
    NvOsSharedMemUnmapTraced(ptr, size, __FILE__, __LINE__)
#define NvOsSharedMemFree(descriptor) \
    NvOsSharedMemFreeTraced(descriptor, __FILE__, __LINE__)
#define NvOsPhysicalMemMap(phys, size, attrib, flags, ptr)   \
    NvOsPhysicalMemMapTraced(phys, size, attrib, flags, ptr, \
            __FILE__, __LINE__)
#define NvOsPhysicalMemMapIntoCaller(pCallerPtr, phys, size, attrib, flags) \
    NvOsPhysicalMemMapIntoCallerTraced(pCallerPtr, phys, size, attrib, flags, \
            __FILE__, __LINE__)
#define NvOsPhysicalMemUnmap(ptr, size)   \
    NvOsPhysicalMemUnmapTraced(ptr, size, __FILE__, __LINE__)
#define NvOsPageAlloc(size, attrib, flags, protect, descriptor)   \
    NvOsPageAllocTraced(size, attrib, flags, protect, descriptor, \
            __FILE__, __LINE__)
#define NvOsPageFree(descriptor) \
    NvOsPageFreeTraced(descriptor, __FILE__, __LINE__)
#define NvOsPageMap(descriptor, offset, size, ptr)   \
    NvOsPageMapTraced(descriptor, offset, size, ptr, __FILE__, __LINE__)
#define NvOsPageMapIntoPtr(descriptor, pCallerPtr, offset, size)   \
    NvOsPageMapIntoPtrTraced(descriptor, pCallerPtr, offset, size, \
            __FILE__, __LINE__)
#define NvOsPageUnmap(descriptor, ptr, size) \
    NvOsPageUnmapTraced(descriptor, ptr, size, __FILE__, __LINE__)
#define NvOsPageAddress(descriptor, offset)   \
    NvOsPageAddressTraced(descriptor, offset, __FILE__, __LINE__)
#define NvOsMutexCreate(mutex) NvOsMutexCreateTraced(mutex, __FILE__, __LINE__)
#define NvOsMutexLock(mutex) NvOsMutexLockTraced(mutex, __FILE__, __LINE__)
#define NvOsMutexUnlock(mutex) NvOsMutexUnlockTraced(mutex, __FILE__, __LINE__)
#define NvOsMutexDestroy(mutex) \
    NvOsMutexDestroyTraced(mutex, __FILE__, __LINE__)
#define NvOsIntrMutexCreate(mutex) \
    NvOsIntrMutexCreateTraced(mutex, __FILE__, __LINE__)
#define NvOsIntrMutexLock(mutex) \
    NvOsIntrMutexLockTraced(mutex, __FILE__, __LINE__)
#define NvOsIntrMutexUnlock(mutex) \
    NvOsIntrMutexUnlockTraced(mutex, __FILE__, __LINE__)
#define NvOsIntrMutexDestroy(mutex) \
    NvOsIntrMutexDestroyTraced(mutex, __FILE__, __LINE__)
#define NvOsSemaphoreCreate(semaphore, value)   \
    NvOsSemaphoreCreateTraced(semaphore, value, __FILE__, __LINE__)
#define NvOsSemaphoreClone(orig, semaphore)   \
    NvOsSemaphoreCloneTraced(orig, semaphore, __FILE__, __LINE__)
#define NvOsSemaphoreUnmarshal(hClientSema, phDriverSema)   \
    NvOsSemaphoreUnmarshalTraced(hClientSema, phDriverSema, __FILE__, __LINE__)
/*
#define NvOsSemaphoreWait(semaphore)   \
    NvOsSemaphoreWaitTraced(semaphore, __FILE__, __LINE__)
#define NvOsSemaphoreWaitTimeout(semaphore, msec)   \
    NvOsSemaphoreWaitTimeoutTraced(semaphore, msec, __FILE__, __LINE__)
*/
#define NvOsSemaphoreSignal(semaphore)   \
    NvOsSemaphoreSignalTraced(semaphore, __FILE__, __LINE__)
#define NvOsSemaphoreDestroy(semaphore)   \
    NvOsSemaphoreDestroyTraced(semaphore, __FILE__, __LINE__)
#define NvOsThreadCreate(func, args, thread)    \
    NvOsThreadCreateTraced(func, args, thread, __FILE__, __LINE__)
#define NvOsThreadJoin(thread)  \
    NvOsThreadJoinTraced(thread, __FILE__, __LINE__)
#define NvOsThreadYield() NvOsThreadYieldTraced(__FILE__, __LINE__)
#define NvOsInterruptRegister(IrqListSize, pIrqList, pIrqHandlerList, \
        context, handle, InterruptEnable) \
    NvOsInterruptRegisterTraced(IrqListSize, pIrqList, pIrqHandlerList, \
        context, handle, InterruptEnable, __FILE__, __LINE__)
#define NvOsInterruptUnregister(handle) \
    NvOsInterruptUnregisterTraced(handle, __FILE__, __LINE__)
#define NvOsInterruptEnable(handle) \
    NvOsInterruptEnableTraced(handle, __FILE__, __LINE__)
#define NvOsInterruptDone(handle) \
    NvOsInterruptDoneTraced(handle, __FILE__, __LINE__)

#endif // NVOS_TRACE

// Forward declare resource tracking struct.
typedef struct NvCallstackRec     NvCallstack;

typedef enum
{
    NvOsCallstackType_NoStack = 1,
    NvOsCallstackType_HexStack,
    NvOsCallstackType_SymbolStack,

    NvOsCallstackType_Last,
    NvOsCallstackType_Force32 = 0x7FFFFFFF
} NvOsCallstackType;

typedef void (*NvOsDumpCallback)(void* context, const char* line);

void NvOsDumpToDebugPrintf(void* context, const char* line);

NvCallstack* NvOsCallstackCreate      (NvOsCallstackType stackType);
void         NvOsCallstackDestroy     (NvCallstack* callstack);
NvU32        NvOsCallstackGetHeight   (NvCallstack* stack);
void         NvOsCallstackGetFrame    (char* buf, NvU32 len, NvCallstack* stack, NvU32 level);
void         NvOsCallstackDump        (NvCallstack* stack, NvU32 skip, NvOsDumpCallback callBack, void* context);
NvU32        NvOsCallstackHash        (NvCallstack* stack);
NvBool       NvOsCallstackContainsPid (NvCallstack* stack, NvU32 pid);
void         NvOsGetProcessInfo       (char* buf, NvU32 len);

static NV_INLINE void NvOsDebugCallstack(NvU32 skip)
{
    NvCallstack* stack = NvOsCallstackCreate(NvOsCallstackType_SymbolStack);
    if (stack)
    {
        NvOsCallstackDump(stack, skip, NvOsDumpToDebugPrintf, NULL);
        NvOsCallstackDestroy(stack);
    }
}

/**
 * To be deprecated.
 *
 * Sets a frame rate target that matches the CPU frequency scaling to the
 * measured performance.
 *
 * @param target  The fps target.
 * @return A file descriptor that, on exiting, you pass to the NvOsCancelFpsTarget function.
 */
int NvOsSetFpsTarget(int target);

/**
 * To be deprecated.
 *
 * Sets the frame rate target with an existing file descriptor (fd).
 *
 * @param fd      An open fd returned by the NvOsSetFpsTarget() function.
 * @param target  The fps target.
 */
int NvOsModifyFpsTarget(int fd, int target);

/**
 * To be deprecated.
 *
 * Cancels the fps target previously supplied with the NvOsSetFpsTarget() function.
 *
 * @param fd The file descriptor generated by \c NvOsSetFpsTarget.
 */
void NvOsCancelFpsTarget(int fd);

/*@}*/
/** @name Unified scaling hint functions
 */
/*@{*/

/**
 * Defines throughput hint types. These are passed directly to unified
 * scaler as integers. Any integer will do (for private hints to be
 * recognized by some of the running governors) but only these listed here
 * are handled specially by the unified scaler.
 */
typedef enum
{
    /*
     * Reserved hints that are working:
     */

    /* Generic throughput hint: value is current performance by 1000. */
    NvOsTPHintType_ThroughputHint   = 0,

    /* Generic framerate target: APM will use the value as framerate target
     * and adjust the clocks accordingly. Only makes sense in applications
     * where framerate maps naturally to application performance. */
    NvOsTPHintType_FramerateTarget  = 1,

    /* Floor and cap for CPU/GPU performance: the value is a factor [0.0,
     * 1.0] of available performance, represented by unsigned integer values
     * between 0L and UINT_MAX. The value will indirectly affect the clocks
     * and/or cores. */
    NvOsTPHintType_MinCPU           = 2,
    NvOsTPHintType_MaxCPU           = 3,
    NvOsTPHintType_MinGPU           = 4,
    NvOsTPHintType_MaxGPU           = 5,

    /*
     * Hints reserved but not yet implemented or considered unofficial:
     */

    /* Similar to NvOsTPHintType_FramerateTarget but suggests that instead
     * of loosely tracking the framerate the unified scaler should try to
     * help ensure that this given framerate will always be reached if only
     * possible. */
    NvOsTPHintType_FramerateMinimum = 6,

    NvOsTPHintType_AppFramerate     = 7,
    NvOsTPHintType_AppIdletime      = 8,

    /*
     * Unreserved user hints are #64 and higher.
     */
    NvOsTPHintType_LastReserved     = 63,
    NvOsTPHintType_Force32          = 0x7FFFFFFF
} NvOsTPHintType;

/**
 * Function may use this as a symbol for "all hints" or "invalid hint",
 * depending on the context.
 */
#define NVOSTPHINTTYPE_NO_TYPE ((NvOsTPHintType)-1)

/**
 * For hint timeouts. A zero timeout makes a one-time hint that is processed
 * once in the next unified scaler cycle and then removed. Values greater
 * than zero are milliseconds.
 */
#define NVOSTPHINT_TIMEOUT_ONCE 0

/**
 *
 * */
#define NVOSTPHINT_DEFAULT_TAG 0x00000000U

/**
 * Send a throughput hint to unifier scaler.
 *
 * The usecase must not contain whitespace and should match a known governor
 * class in the unified scaler. The type and value of the hint are
 * application specific. The timeout is the hint's lifetime in milliseconds,
 * or NVOSTPHINT_TIMEOUT_ONCE.
 *
 * For hints of type NvOsTPHintType_ThroughputHint, the hint shall be in the
 * range of [1..INT_MAX]. If the hint is 1000 it means we're exactly at the
 * target throughput rate, 500 means we're twice too slow, and 2000 means
 * we're twice too fast. Thus, hints below 1000 cause unified scaler to
 * scale clocks up and hints above 1000 cause unified scaler to scale clocks
 * down.
 *
 * For hints of type NvOsTPHintType_FramerateTarget, the hint is the target
 * framerate in the range of [0..vsync]. If the target is greater than zero
 * will ussrd will use its value as the target framerate and scale up/down
 * accordingly, depending on the actual system framerate. However, if the
 * target is 0 (zero) then ussrd will still do framerate targetting but uses
 * a default target framerate as the effective target framerate. The default
 * target framerate is specific to the system and/or application, and may
 * originate from an application profile.
 *
 * For hints of any other type, the hint value is passed directly to unified
 * scaler and maybe picked up uninterpreted by any of the unified scaler
 * governors that recognize the hint type within the effective governor
 * class.
 *
 * The timeout defines the lifetime of the hint. A framerate target hint
 * could live long periods of time since it doesn't change. A throuhgput
 * hint probably only applies to the moment it was sent, so it should have a
 * relatively short timeout (100-200ms) and an updated hint should be
 * repeatedly sent before the timeout of the previous hint expires.
 *
 * The unified scaler might impose a maximum value for the timeout and
 * exceeding that will fail this call: see the return value.
 *
 * @param usecase       Usecase string
 * @param type          Type of the throughput hint as described in
 *                      NvOsTPHintType
 * @param value         Value of the throughput hint
 * @param timeout_ms    Timeout for the hint, or NVOSTPHINT_TIMEOUT_ONCE
 * @return Returns 0 on success, nonzero on error as follows: -1 if usecase
 *         could not be registered, another negative value on other error
 *         such as connection failure, and a positive value if timeout_ms
 *         was longer than what is the platform-dependent value supported by
 *         the unified scaler in which case the value returned is the
 *         maximum allowed timeout in milliseconds.
 */
int NvOsSendThroughputHint (const char *usecase, NvOsTPHintType type, NvU32 value, NvU32 timeout_ms);

/**
 * Send throughput hints to unifier scaler with unique client tag.
 *
 * Similar to NvOsSendThroughputHint() but can send several hints at the
 * same time with a single write. The varargs take NvOsSendThroughputHint()
 * arguments directly and the argument list is terminated by NULL as the
 * usecase pointer.
 *
 * Use NVOSTPHINT_DEFAULT_TAG as client_tag, or optionally a client specific
 * tag which can be an arbitrary non-NULL NvU32 but must remain the same
 * through the lifespan of the client so that unified scaler can distinguish
 * different clients from each other.
 *
 * A typical client tag could be a pointer to some client specific struct.
 * In case of EGL internally, a client tag could be per surface in which
 * case it might as well be a pointer to the EGLSurface struct.
 *
 * Example:
 *
 *   NvOsSendThroughputHint(0xbadbeef,
 *                          "camera", NvOsTPHintType_ThroughputHint, 900, 1000,
 *                          "camera", NvOsTPHintType_FramerateMinimum, 30, 2000,
 *                          NULL);
 *
 * This would send two camera hints, both identified as client BADBEEF.
 * Further, NvOsSendThroughputHint() is equivalent to:
 *
 *  NvOsVaSendThroughputHints(NVOSTPHINT_DEFAULT_TAG,
 *                            usecase, type, value, timeout_ms,
 *                            NULL);
 *
 * @param client_tag    Client tag
 * @return Returns the same as NvOsSendThroughputHint()
 */
int NvOsVaSendThroughputHints (NvU32 client_tag, ...);

/**
 * Unset all hints for a particular usecase, or if usecase is NULL then
 * close all open socket connections and terminate all hints currently in
 * effect.
 *
 * Should be called when the application knows it will turn inactive.
 *
 * The usecase must not contain whitespace and should match a known governor
 * class in the unified scaler.
 *
 * @param usecase Usecase string
 */
void NvOsCancelThroughputHint (const char *usecase);

/*@}*/
/** @} */

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVOS_H
