/** Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto.  Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an 
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** This file is an interface to implement various meta file parsers 
* (for example: m3u, pls meta file parsers). This interface is used by 
* meta file extractor util to parse the meta files given by the client. 
* The file has interfaces defined to get different meta data information 
* present in meta file.
*/


#ifndef INCLUDED_NVMETAFILE_M3UPARSER_H
#define INCLUDED_NVMETAFILE_M3UPARSER_H

#include "nvmetadata.h"
#include "nvmetafile_parser.h"


NvError M3uParserOpen(
                      NvU8* filename, 
                      NvMetaFileParserHandle *handle, 
                      NvU32* nTotalFileCount);


NvError M3uParserClose(NvMetaFileParserHandle handle);


NvError M3uFileInfo(
                    NvMetaFileParserHandle handle, 
                    NvU32 nIndex, 
                    NvU32 nfilesToRead, 
                    NvFileMetaData **fileInfoBuffer);





#endif


