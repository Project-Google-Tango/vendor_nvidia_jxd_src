/*************************************************************************************************
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 *
 * @addtogroup ObexLayer
 * @{
 */

/**
 * @file obex_file.h Obex File API.
 *
 */

#ifndef OBEX_FILE_H
#define OBEX_FILE_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "obex.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
/**
 * Open callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileOpen)(const char *name,int flags,int mode);

/**
 * Close callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileClose)(int file);

/**
 * Read callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileRead)(int file,void *ptr,int len);

/**
 * Write callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileWrite)(int file,const void *ptr,int len);

/**
 * Seek callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileLseek)(int file,int offset,int dir);

/**
 * Truncate callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileTruncate)(const char *name, int offset);

/**
 * Getsize callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileLgetsize)(int file);

/**
 * Remove callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileRemove)(const char *file_name);

/**
 * Copy callback function
 *
 * @see drv_fs.h
 */
typedef int (*obex_FileCopy)(const char *src_file_name,const char *dst_file_name);

/**
 * Rename callback function
 *
 * @see drv_fs.h
 */

typedef int (*obex_FileRename)(const char *old_file_name,const char *new_file_name);

/**
 * Stat callback function
 */
typedef int (*obex_FileStat)(const char *path, obex_Stat *buf);

/**
 * Opendir callback function
 */
typedef obex_Dir* (*obex_DirOpen)(const char *name);

/**
 * Readdir callback function
 */
typedef obex_OsDirEnt* (*obex_DirRead)(obex_Dir *dir);

/**
 * Closedir callback function
 */
typedef int (*obex_DirClose)(obex_Dir *dir);

/**
 * Functional interface for the OBEX File Layer. The OBEX file
 * layer calls into these function when processing requests
 */
typedef struct
{
    obex_FileOpen open;                       /**< open function */
    obex_FileClose close;                     /**< close function */
    obex_FileRead read;                       /**< read function */
    obex_FileWrite write;                     /**< write function */
    obex_FileLseek lseek;                     /**< lseek function */
    obex_FileTruncate truncate;               /**< truncate function */
    obex_FileLgetsize lgetsize;               /**< lgetsize function */
    obex_FileRemove remove;                   /**< remove function */
    obex_FileCopy copy;                       /**< copy function */
    obex_FileRename rename;                   /**< rename function */
    obex_FileStat stat;                       /**< stat function */
} obex_FileFuncs;

/**
 * Functional interface for the OBEX directory part of File
 * Layer. The OBEX file layer calls into these function when
 * processing requests wher obj is identified as a directory
 */
typedef struct
{
    obex_DirOpen opendir;                     /**< open dir function */
    obex_DirClose closedir;                   /**< close dir function */
    obex_DirRead readdir;                     /**< read dir function */
} obex_DirFuncs;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
* Initialise the OBEX (File) Application Layer by providing the
* OBEX File layer functional interface
*
* @param funcs   file functional interface
* @param d_funcs dir functional interface
* @return Return code
* @see obex.h
* @see drv_fs.h
*/
obex_ResCode obex_FileRegister(const obex_FileFuncs *funcs, const obex_DirFuncs *d_funcs);

/**
 * Set OBEX server inbox path
 *
 * @param inbox
 */
void obex_SetInbox(char *inbox);

/**
 * Will align system dir entry format with client's one
 *
 * @param entry
 * @param buf
 *
 * @return int
 */
int obex_FormatDirEntry(obex_OsDirEnt *entry, void *buf);

#endif /* #ifndef OBEX_FILE_H */

/** @} END OF FILE */
