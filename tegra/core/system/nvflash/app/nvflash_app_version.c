/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvflash_version.h"
#include "nvutil.h"
#include "nvapputil.h"
#include "nvflash_util.h"
#include "nvflash_commands.h"
#include "nvflash_versioninfo.h"

#define VERIFY( exp, code ) \
    if( !(exp) ) {          \
        code;               \
    }

extern NvBool nvflash_wait_status(void);

void
nvflash_parse_binary_version(char* VersionStr, NvFlashBinaryVersionInfo *VersionInfo)
{
    char *end;

    end  = VersionStr;

    if(*end == 'v')
        end++;

    VersionInfo->MajorNum = NvUStrtoul(end, &end, 10);
    end++;
    VersionInfo->MinorNum = NvUStrtoul(end, &end, 10);
    end++;
    VersionInfo->Branch = NvUStrtoul(end, &end, 10);
}

NvBool
nvflash_validate_versionstr(char* VersionStr)
{
    NvU32 len = 0;
    NvU32 dotcount = 0;
    NvBool b = NV_TRUE;

    len = NvOsStrlen(VersionStr) - 1;
    do
    {
        if (VersionStr[len] == '.')
        {
            dotcount++;
        }
        else if (VersionStr[len] < '0' || VersionStr[len] > '9')
        {
            b = NV_FALSE;
            break;
        }

        len--;
    } while(len);

    if (dotcount != 2)
        b = NV_FALSE;

    return b;
}

NvBool
nvflash_validate_nv3pserver_version(Nv3pSocketHandle hSock)
{
    const char *err_str = 0;
    NvBool b = NV_TRUE;
    NvError e = NvSuccess;
    NvFlashCommand cmd;
    NvFlashBinaryVersionInfo NvFlashVersion, Nv3pVersion;
    Nv3pCmdGetNv3pServerVersion Nv3pVersionStruc;

    /* Get Version info and check compatibility between Nvflash & Nv3pServer */
    cmd = Nv3pCommand_GetNv3pServerVersion;
    e = Nv3pCommandSend(hSock, cmd, (NvU8 *)&Nv3pVersionStruc, 0);
    if (e != NvSuccess )
    {
        // Just continue execution , avoid warning print as of now.
    }
    else
    {
        b = nvflash_wait_status();
        VERIFY(b, err_str = "unable to do retrieve version info"; goto fail);

        // Validate version strings
        b = nvflash_validate_versionstr(Nv3pVersionStruc.Version);
        VERIFY(b, err_str = "invalid Nv3pServer version string"; goto fail);

        // Parse version strings
        nvflash_parse_binary_version(Nv3pVersionStruc.Version, &Nv3pVersion);
        nvflash_parse_binary_version(NVFLASH_VERSION, &NvFlashVersion);

        // Compare compatibility between Nvflash and Bootloader
        if (Nv3pVersion.MajorNum != NvFlashVersion.MajorNum)
        {
            NvAuPrintf("ALERT: Nvflash %s and Nv3pServer %s are not compatible\n",
                        NVFLASH_VERSION, Nv3pVersionStruc.Version);
            NvAuPrintf("Use \"./nvflash --cmdhelp --versioninfo\" for more "
                       "information \n\n");
            err_str = "nvflash and 3pserver are not compatible";
            goto fail;
        }

        // Compare compatibility between Nvflash and Bootloader minor version
        if (Nv3pVersion.MinorNum != NvFlashVersion.MinorNum)
        {
            NvAuPrintf("Nv3pServer version %s\n",Nv3pVersionStruc.Version);
            NvAuPrintf("DebugInfo: Minor version of Nvflash and 3pserver "
                       "are not same\n");
            NvAuPrintf("Use \"./nvflash --cmdhelp --versioninfo\" for more "
                       "information \n\n");
        }

        // Compare compatibility between Nvflash and Bootloader branches
        if (Nv3pVersion.Branch != NvFlashVersion.Branch)
        {
            NvAuPrintf("Nv3pServer version %s \n", Nv3pVersionStruc.Version);
            NvAuPrintf("warning:Branch version of Nvflash and 3pserver "
                       "are not same\n");
            NvAuPrintf("Use \"./nvflash --cmdhelp --versioninfo\" for more "
                       "information \n\n");
        }
    }

    goto clean;

fail:
    b = NV_FALSE;
    if (err_str)
        NvAuPrintf("%s\n", err_str);
clean:
    return b;
}

NvBool
nvflash_check_compatibility(void)
{
    NvBool b = NV_TRUE;
    char *blob_filename = NULL;
    NvU8 *nvsbktool_version = NULL;
    const char *err_str = NULL;
    NvFlashBinaryVersionInfo NvFlashVersion;
    NvFlashBinaryVersionInfo NvSbkToolVersion;

    NvFlashCommandGetOption(NvFlashOption_Blob, (void*)&blob_filename);

    if (!blob_filename)
    {
        return b;
    }
    else if (!blob_filename)
    {
        NvAuPrintf("Not Recommended using nvflash with older secure tool\n");
        return b;
    }

    // parse blob to retrieve nvsbktool version string
    b = nvflash_parse_blob(blob_filename, &nvsbktool_version,
                           NvFlash_Version, NULL);
    VERIFY(b, err_str = "secure tool version extraction from blob failed";
              goto fail);
    NvAuPrintf("Using blob %s\n", nvsbktool_version);

    // Validate version strings
    b = nvflash_validate_versionstr((char*)nvsbktool_version);
    VERIFY(b, err_str = "invalid secure tool version string"; goto fail);

    b = nvflash_validate_versionstr(NVFLASH_VERSION);
    VERIFY(b, err_str = "invalid Nvflash version string"; goto fail);

    // Parse version strings
    nvflash_parse_binary_version((char*)nvsbktool_version, &NvSbkToolVersion);
    nvflash_parse_binary_version(NVFLASH_VERSION, &NvFlashVersion);

    // Compare compatibility between NvSbktool and Nvflash
    if (NvSbkToolVersion.MajorNum != NvFlashVersion.MajorNum)
    {
        NvAuPrintf("ALERT: Nvflash %s and secure tool %s are not compatible\n",
                    NVFLASH_VERSION, nvsbktool_version);
        NvAuPrintf("Use \"./nvflash --cmdhelp --versioninfo\" for more "
                   "information\n\n");
        return NV_FALSE;
    }

    // Compare compatibility between NvSbktool and Nvflash minor version
    if (NvSbkToolVersion.MinorNum != NvFlashVersion.MinorNum)
    {
        NvAuPrintf("secure tool version %s \n",nvsbktool_version);
        NvAuPrintf("DebugInfo: Minor version of nvflash and secure tool are "
                   "not same\n");
        NvAuPrintf("Use \"./nvflash --cmdhelp --versioninfo\" for more "
                   "information\n\n");
    }

    // Compare compatibility between NvSbktool and Nvflash branch
    if (NvSbkToolVersion.Branch != NvFlashVersion.Branch)
    {
        NvAuPrintf("secure tool version  %s \n",nvsbktool_version);
        NvAuPrintf("warning: branch version of nvflash and secure tool are "
                   "not same\n");
        NvAuPrintf("Use \"./nvflash --cmdhelp --versioninfo\" for more "
                   "information\n\n");
    }

    return NV_TRUE;

fail:
    if (err_str)
    {
        NvAuPrintf("%s\n", err_str);
    }
    if (nvsbktool_version)
        NvOsFree(nvsbktool_version);
    return b;
}

NvBool
nvflash_display_versioninfo(NvU32 Major1, NvU32 Major2)
{
    NvU32 i = 0;
    NvU32 count = 0;
    NvU32 temp = 0;

    NvAuPrintf("****Brief description ---Version has 3 fields****\n");
    NvAuPrintf("Major: If major number of nvflash is not same as 3pserver "
               "or nvsecuretool then they are not compatible\n");
    NvAuPrintf("Minor: If minor number of nvflash is not same as 3pserver "
               "or nvsecuretool flashing works but some features might not work \n");
    NvAuPrintf("Branch: This is used to track the branch information \n");
    NvAuPrintf("\n***Change log***\n");

    count = sizeof(ChangeDesc) / sizeof(ChangeDescription);

    if (Major1 >Major2)
    {
        temp = Major1;
        Major1 = Major2;
        Major2 = temp;
    }

    while(i < count)
    {
        if (ChangeDesc[i].Major >= Major1 && ChangeDesc[i].Major <= Major2)
        {
            NvAuPrintf("%02d.%02d.%04d: %s\n",
                        ChangeDesc[i].Major, ChangeDesc[i].Minor,
                        ChangeDesc[i].Branch, ChangeDesc[i].Desc);
        }
        i++;
    }
    NvAuPrintf("\n");

    return NV_TRUE;
}

