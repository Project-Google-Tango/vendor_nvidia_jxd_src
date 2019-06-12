/* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto.  Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an 
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** This file  defines various structures used by 
*   meta file parser interfaces.
*/


#ifndef INCLUDED_NVMETADATA_H
#define INCLUDED_NVMETADATA_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif


    /** @brief NvMetaDataInfoTypes Enum that represents different types of attributes
    * that can be present in a meta file. Any newly added attributes, should be
    * added to the end of list, before 'NvFileInfo_Max' to maintain backward
    * compatibility.
    *
    * @ingroup mmblock
    */
    typedef enum 
    {
        NvMetaDataInfo_URL = 0x0,
        NvMetaDataInfo_Title,
        NvMetaDataInfo_Album,
        NvMetaDataInfo_Artist,
        NvMetaDataInfo_Author,
        NvMetaDataInfo_Copyright,
        NvMetaDataInfo_Genre,
        NvMetaDataInfo_Composer,
        NvMetaDataInfo_Year,
        NvMetaDataInfo_AlbumArt,
        NvMetaDataInfo_TrackNumber,
        NvMetaDataInfo_Encoded,
        NvMetaDataInfo_Comment,
        NvMetaDataInfo_Publisher,
        NvMetaDataInfo_OrgArtist,
        NvMetaDataInfo_Bpm,
        NvMetaDataInfo_AlbumArtist,
        NvMetaDataInfo_CoverArt,
        NvMetaDataInfo_CoverArtURL,
        NvMetaDataInfo_Max,/// Signifies Maximum attributes
        NvMetaDataInfo_Force32 = 0x7FFFFFFF
    } NvMetaDataInfoTypes;


    /** @brief NvMetaDataCharSet Enum that represents different types of metadatainfo encoding types
    * that can be present in a meta file. 
    *
    * @ingroup mmblock
    */
    typedef enum {
        NvMetaDataEncode_Utf8 = 1,
        NvMetaDataEncode_Utf16,
        NvMetaDataEncode_Ascii,
        NvMetaDataEncode_NvU32, 
        NvMetaDataEncode_Binary, 
        NvMetaDataEncode_JPEG = 0x100,
        NvMetaDataEncode_PNG,
        NvMetaDataEncode_BMP,
        NvMetaDataEncode_GIF,
        NvMetaDataEncode_Other,
        NvMetaDataEncode_Force32 = 0x7FFFFFFF
    } NvMetaDataCharSet;


    /** @brief NvFileMetaData stores the meta data of a single file entry.
    * info[n] and length[n] are relative. info[n] represents the metadatainfo attribute
    * (say URL or title) and length[n] represents the length for nth attribute. 
    * For example, n = 2, which represents 'NvFileInfo_Album' attribute. info[2] contains
    * album name and length[2] represents the size of the data given in info[2].
    *
    * @ingroup mmblock
    */
    typedef struct NvFileMetaData
    {
        /// File metadatatype Info
        NvU8* info[NvMetaDataInfo_Max];
        /// Length of the file metadata Info
        NvU32 length[NvMetaDataInfo_Max];
        /// Char type of metadata Info
        NvMetaDataCharSet encodechartype[NvMetaDataInfo_Max];
        /// File size
        NvU64 fileSize;
        /// Total duration of the file
        NvU64 totalDuration;
    } NvFileMetaData;

   

#if defined(__cplusplus)
}
#endif

#endif



