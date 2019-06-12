/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVMM_FILE_UTIL_H_
#define NVMM_FILE_UTIL_H_

#include "nvos.h"
#include "nvcommon.h"

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_parser_types.h"
#include "nvmm_super_parserblock.h"
#include "nvcustomprotocol.h"

NvError NvMMUtilFilenameToParserType(NvU8 *filename,
                                     NvMMParserCoreType *eParserType,
                                     NvMMSuperParserMediaType *eMediaType);

NvError NvMMUtilConvertNvMMParserCoreType(NvParserCoreType eParserType,
                                          NvMMParserCoreType *eNvMMParserType);

NvError NvMMUtilGuessFormatFromFile(char *szURI, NV_CUSTOM_PROTOCOL *pProtocol,
                                    NvParserCoreType *eParserType,
                                    NvMMSuperParserMediaType *eMediaType);

#endif

