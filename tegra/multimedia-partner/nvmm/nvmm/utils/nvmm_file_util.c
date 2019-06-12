/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <ctype.h>
#include <string.h>

#include "nvos.h"
#include "nvcommon.h"

#include "nvmm_file_util.h"

NvError NvMMUtilFilenameToParserType(NvU8 *filename,
                                     NvMMParserCoreType *eParserType,
                                     NvMMSuperParserMediaType *eMediaType)
{
    char *str = strrchr((char *)filename, '.');
    char FileExt[8] = { 0 };

    if (str)
    {
        str++;
        if (*str != 0 && strlen(str) != 0 && strlen(str) <= 7)
        {
            NvOsMemcpy(FileExt, str, strlen(str) + 1);
            str = FileExt;
            while (*str)
            {
                *str = tolower(*str);
                str++;
            }
        }
    }

    *eParserType = NvMMParserCoreTypeForce32;
    *eMediaType = NvMMSuperParserMediaType_Force32;

    if (!strcmp(FileExt, "mp3") || !strcmp(FileExt, "mpa"))
    {
        *eParserType = NvMMParserCoreType_Mp3Parser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
    }
    else if (!strcmp(FileExt, "mp4") || !strcmp(FileExt, "3gp") || 
             !strcmp(FileExt, "3gpp") ||
             !strcmp(FileExt, "m4v") || !strcmp(FileExt, "mov") ||
             !strcmp(FileExt, "3g2") || !strcmp(FileExt, "qt"))
    {
        *eParserType = NvMMParserCoreType_Mp4Parser;
    }
    else if (!strcmp(FileExt, "m4a") || !strcmp(FileExt, "m4b"))
    {
        *eMediaType = NvMMSuperParserMediaType_Audio;
        *eParserType = NvMMParserCoreType_Mp4Parser;
    }
    else if (!strcmp(FileExt, "asf") || !strcmp(FileExt, "wmv") || !strcmp(FileExt, "pyv"))
    {
        *eParserType = NvMMParserCoreType_AsfParser;
    }
    else if (!strcmp(FileExt, "wma"))
    {
        *eMediaType = NvMMSuperParserMediaType_Audio;
        *eParserType = NvMMParserCoreType_AsfParser;
    }
    else if (!strcmp(FileExt, "amr") || !strcmp(FileExt, "awb"))
    {
        *eParserType = NvMMParserCoreType_AmrParser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
    }
    else if (!strcmp(FileExt, "avi") || !strcmp(FileExt, "divx"))
    {
        *eParserType = NvMMParserCoreType_AviParser;
    }
    else if (!strcmp(FileExt, "mpg") || !strcmp(FileExt, "mpeg"))
    {
        *eParserType = NvMMParserCoreType_MpsParser;
    }
    else if (!strcmp(FileExt, "aac"))
    {
        *eMediaType = NvMMSuperParserMediaType_Audio;
        *eParserType = NvMMParserCoreType_AacParser;
    }
    else if (!strcmp(FileExt, "ogg") || !strcmp(FileExt, "oga"))
    {
        *eMediaType = NvMMSuperParserMediaType_Audio;
        *eParserType = NvMMParserCoreType_OggParser;
    }
    else if (!strcmp(FileExt, "wav"))
    {
        *eParserType = NvMMParserCoreType_WavParser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
    }
    else if (!strcmp(FileExt, "mp2") || !strcmp(FileExt, "mpg") ||
             !strcmp(FileExt, "mp1"))
    {
        *eParserType = NvMMParserCoreType_Mp3Parser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
    }
    else if (!strcmp(FileExt, "m2v"))
    {
        *eParserType = NvMMParserCoreType_M2vParser;
    }

    return NvSuccess;
}

NvError NvMMUtilConvertNvMMParserCoreType(NvParserCoreType eParserType,
                                          NvMMParserCoreType *eNvMMParserType)
{
    switch (eParserType)
    {
    case NvParserCoreType_AsfParser:
        *eNvMMParserType = NvMMParserCoreType_AsfParser;
        break;
    case NvParserCoreType_3gppParser:
        *eNvMMParserType = NvMMParserCoreType_3gppParser;
        break;
    case NvParserCoreType_AviParser:
        *eNvMMParserType = NvMMParserCoreType_AviParser;
        break;
    case NvParserCoreType_Mp3Parser:
        *eNvMMParserType = NvMMParserCoreType_Mp3Parser;
        break;
    case NvParserCoreType_Mp4Parser:
        *eNvMMParserType = NvMMParserCoreType_Mp4Parser;
        break;
    case NvParserCoreType_OggParser:
        *eNvMMParserType = NvMMParserCoreType_OggParser;
        break;
    case NvParserCoreType_AacParser:
        *eNvMMParserType = NvMMParserCoreType_AacParser;
        break;
    case NvParserCoreType_AmrParser:
        *eNvMMParserType = NvMMParserCoreType_AmrParser;
        break;
    case NvParserCoreType_RtspParser:
        *eNvMMParserType = NvMMParserCoreType_RtspParser;
        break;
    case NvParserCoreType_M2vParser:
        *eNvMMParserType = NvMMParserCoreType_M2vParser;
        break;
    case NvParserCoreType_MtsParser:
        *eNvMMParserType = NvMMParserCoreType_MtsParser;
        break;
    case NvParserCoreType_NemParser:
        *eNvMMParserType = NvMMParserCoreType_NemParser;
        break;
    case NvParserCoreType_WavParser:
        *eNvMMParserType = NvMMParserCoreType_WavParser;
        break;
    case NvParserCoreType_FlvParser:
        *eNvMMParserType = NvMMParserCoreType_FlvParser;
        break;
    case NvParserCoreType_MkvParser:
        *eNvMMParserType = NvMMParserCoreType_MkvParser;
        break;
    default:
        *eNvMMParserType = NvMMParserCoreType_UnKnown;
    }
    return NvSuccess;
}


