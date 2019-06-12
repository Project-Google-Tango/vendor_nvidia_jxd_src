/**
* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** This file is an interface to implement various meta file parsers 
* (for example: m3u, pls meta file parsers).This intefface provides methods to be used 
* by Client to get the meta data information present in a meta file.
* 3rd Party meta parser library plug-in supported with the help of following two steps 
* 1.3rd party meta parser developers should export three methods namely NvMetaFileParserOpen(),
* NvMetaFileParserClose(),NvGetMetaFileInfo() through their library.
* 2.Update the MetaFileFormatLibs.xml file present in BSP with the corresponding library name.
* for exaample :Tag syntax to update XML file to use a 3rd party library "asxparser.dll"  is as follows
* library name length should be limited to  32 characters only.
* <MetaFileType>
*   <Ext>asx</Ext>
*       <Lib>asxparser</Lib>
* </MetaFileType>
*/


#ifndef INCLUDED_NVMETAFILE_PARSER_H
#define INCLUDED_NVMETAFILE_PARSER_H

#include "nverror.h"
#include "nvmetadata.h"

#if defined(__cplusplus)
extern "C"
{
#endif


    /// This handle will store the handle for meta file parser
    typedef void* NvMetaFileParserHandle;

    /**
    * NvMetaFileParserOpen
    * @brief This method determines the type of meta parser required to parse
    * the given URL and opens the meta file parser for the same. It returns the
    * meta file handle to the client. Client has to specify this handle for all meta
    * file related operations.
    *
    * @param sURL IN Input meta File path
    * @param hMetaParser OUT handle to the meta file parser
    * @param nTotalFileCount OUT returns total file entries present in the given metafile
    * @retval NvSuccess or NvError_xxx Explains the failures
    */
    NvError NvMetaFileParserOpen(
        NvU8* sURL,
        NvMetaFileParserHandle *hMetaParser,
        NvU32 *nTotalFileCount);

    /**
    * NvMetaFileParserClose
    * @brief This method destroys the meta file handle specified.
    *
    * @param hMetaParser IN Meta file handle 
    * @retval NvSuccess or NvError_xxx Explains the failures
    */
    NvError NvMetaFileParserClose(NvMetaFileParserHandle hMetaParser);

    /**
    * NvGetMetaFileInfo
    * @brief This method extracts file information for various file entries
    * present in the meta file. This supports extracting total file entry information
    * or file information for requested entries. It is the client's responsibility 
    * to given enough memory for extracting the infromation. The memory
    * requirements to fill the file information can be requested by the client
    * to the extractor using this API. Please look into the multiple
    * usages of this method given below.
    * 
    * Case 1: Requesting the memory requirements from the meta file extractor
    *
    * Example: NvGetFileInfo(handle, 1, 12, fileInfo); 
    * In this call, client has to send NULL pointers in the fileInfo->info[n], where 
    * n starts from zero to total file count in the meta file or specific number of 
    * entries in the meta file. File Extractor via meta file parser detects the NULL
    * pointers and returns memory requirements for various attributes through
    * fileInfo->length[n]. Client can allocate memory as per the length returned 
    * by this method.Information related to particular attributes can be get by setting 
    * all other uninterested arrtibute info pointers to 0xFFFFFFFF 
    *
    * Case 2: Extracting the file information from the meta file
    *
    * Example: NvGetFileInfo(handle, 1, 20, fileInfo);
    * In this case, we get the information starting from first file till '20' files, if available.
    * 
    * Example: NvGetFileInfo(handle, 3, 10, fileInfo);
    * In this case, we get the information for the file entry starting from '3' and '10' files
    * from that entry. That is, we extract the file information from 3rd file till 12th file.
    * 
    * Case 3: Extracting the playlist information from the meta file
    *
    * Example:  NvGetFileInfo(handle, 0, 1, fileInfo);
    * In this case we get global playlist information ,observe that nFileIndex is '0'
    *
    * @param hMetaParser IN Meta file Parser handle 
    * @param nFileIndex IN Starting File Entry for which information to be extracted
    * @param nFileCount IN Specifies the number of file entries from the nFileIndex for 
    * which the file information has to be extracted.
    * @param fileInfo OUT Meta data for requested file entries.
    * @retval NvSuccess or NvError_xxx Explains the failures
    */
    NvError
        NvGetMetaFileInfo(
        NvMetaFileParserHandle hMetaParser, 
        NvU32 nFileIndex, 
        NvU32 nFileCount, 
        NvFileMetaData **fileInfo);


#if defined(__cplusplus)
}
#endif

#endif

