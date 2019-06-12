/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvsecuretool.h"
#include "nvutil.h"

#define MAX_VERSION_STRING_LENGTH 11

/* Defines the structure for storing changes */
typedef struct NvSecureToolChangesRec
{
    NvU8 Major;
    NvU8 Minor;
    NvU16 Branch;
    NvU8 Desc[256];
}NvSecureToolChanges;

/* Variable which holds all nvsecuretool changes */
static NvSecureToolChanges s_ChangeDesc[] =
{
    {04, 00, 0000, "Separate version for nvsecuretool"},
    {04, 01, 0000, "--keyfile option added: 1047405"},
    {04, 02, 0000, "--oemfile option added: 1047405"},
    {04, 03, 0000, "public key hash stored in pub.sha: 1275318"},
    {04, 04, 0000, "Support bct config file: 213448"},
    {04, 05, 0000, "allocate sufficient memory to store priv key: 240903"},
    {04, 06, 0000, "Encrypt/Sign TOS image: 245518"},
    {04, 07, 0000, "Encrypt/Sign WB0 image: 268358"}
};

static char s_NvSecureToolVersionString[MAX_VERSION_STRING_LENGTH];
static char s_NvSecureBlobVersionString[] = NVFLASH_VERSION;

const char* NvSecureGetToolVersion()
{
    NvU32 LastChangeIndex;

    LastChangeIndex = sizeof(s_ChangeDesc) / sizeof(NvSecureToolChanges);
    LastChangeIndex--;

    NvOsSnprintf(s_NvSecureToolVersionString, MAX_VERSION_STRING_LENGTH,
                 "%d.%02d.%04d",
                 s_ChangeDesc[LastChangeIndex].Major,
                 s_ChangeDesc[LastChangeIndex].Minor,
                 s_ChangeDesc[LastChangeIndex].Branch);

    return s_NvSecureToolVersionString;
}

const char* NvSecureGetBlobVersion()
{
    return s_NvSecureBlobVersionString;
}

