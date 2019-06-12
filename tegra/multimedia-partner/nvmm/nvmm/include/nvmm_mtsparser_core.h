/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_MTS_PARSER_CORE_H
#define INCLUDED_NVMM_MTS_PARSER_CORE_H

#include "nvmm_parser_core.h"

NvError 
NvMMCreateMtsCoreParser(
    NvMMParserCoreHandle *hParserCore, 
    NvMMParserCoreCreationParameters *pParams);

void NvMMDestroyMtsCoreParser(NvMMParserCoreHandle hParserCore);

#endif // INCLUDED_NVMM_MTS_PARSER_CORE_H



























































