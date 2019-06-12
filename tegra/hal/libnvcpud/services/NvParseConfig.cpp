/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include "NvParseConfig.h"
#include "TimeoutPoker.h"

namespace android {
namespace nvcpu {

#define MHZ_to_KHZ(x) (x*1000)
#define CAMERA_BOOST_LEVELS 3
static void NvCpud_removeSpaces(char *buf);
static bool NvCpud_parseLevel(char *buf, int *level, char *levelName);
static bool NvCpud_parseLine(char *buf, int *freqValue);

int NvCpud_parseCpuConf(int *cpuInfo, int *cameraLevels)
{
    FILE *hConfFile = NULL;
    char buf[MAX_LINE_LENGTH]={'\0'};
    char tmp[16];
    int line = 0;
    int level = 0;
    int foundLevels = 0;
    int freq_index = 0;

    hConfFile = fopen(NVCPUD_FILE_LOCATION, "r");
    if (!hConfFile) {
        ALOGE("Can't open %s\n", NVCPUD_FILE_LOCATION);
        goto cleanup;
    }

    if (!fgets(buf, MAX_LINE_LENGTH, hConfFile))
    {
        ALOGE("Can't read %s\n", NVCPUD_FILE_LOCATION);
        goto cleanup;
    }

    while (!feof(hConfFile))
    {
        int freqValue;
        char levelName[MAX_LINE_LENGTH];

        NvCpud_removeSpaces(buf); // remove white spaces
        line++;

        // ignore comments and empty lines but parse rest
        if ((buf[0]!='#')&&(strncmp(buf,"//",2))&&(buf[0]!='\0'))
        {
            bool res;
            if (foundLevels < CAMERA_BOOST_LEVELS)
            {
                res = NvCpud_parseLevel(buf, &level, levelName);
                if (res)
                {
                    cameraLevels[foundLevels] = level;
                    foundLevels += 1;
                }
                else {
                    ALOGE("in %s: missing level (%d)", NVCPUD_FILE_LOCATION, line);
                    goto cleanup;
                }
            }
            else
            {
                res = NvCpud_parseLine(buf, &freqValue);
                if (res)
                {
                    cpuInfo[freq_index] = MHZ_to_KHZ(freqValue);
                    freq_index += 1;
                }
                else
                {
                    ALOGE("in %s(%d): malformed line.", NVCPUD_FILE_LOCATION, line);
                    goto cleanup;
                }
            }
        }
        if (!fgets(buf, MAX_LINE_LENGTH, hConfFile))
        {
            if (!feof(hConfFile))
                goto cleanup;
        }
    }

    fclose(hConfFile);
    return 1;

cleanup:
    if (hConfFile)
        fclose(hConfFile);

    ALOGE("Parsing of %s failed: Using default frequency table.\n", NVCPUD_FILE_LOCATION);
    return 0;
}

static void NvCpud_removeSpaces(char *buf)
{
    char *p1;
    int i;

    // get rid of spaces in line up to MAX_LINE_LENGTH
    p1 = buf;
    i = 0;
    while ((i < MAX_LINE_LENGTH) && (buf[i] != '\0'))
    {
        if (buf[i]!=' ')
        {
            *p1 = buf[i];
            p1++;
        }
        i++;
    }
    *p1 = '\0';
}

// takes a string and tries to parse the level token
// if successful returns with the level name and number in
// the parameter
static bool NvCpud_parseLevel(char *buf, int *level, char *levelName)
{
    char *pbuf, *token, *end, *ctxt;

    // make sure string starts with "level_cam_"
    if (strncmp(buf, "level_cam_", 10))
        return false;

    pbuf = buf + 10;
    *level = 0;

    // get the level name as a string
    token = strtok_r(pbuf, "=", &ctxt);
    levelName = token;

    token = strtok_r(NULL, "", &ctxt);

    long int lev = strtol(token, &end, 10);
    if (strcmp(end, token) == 0)
        return false;
    else
        *level = (int)lev;

    return true;
}

// takes a string in buf and tries to parse cpu information
// out of it. All parameters will be returned with the parsed
// values if function returns true otherwise function
// returns false.
static bool NvCpud_parseLine(char *buf, int *freqValue)
{
    char *pbuf, *end;

    pbuf = buf;
    *freqValue = 0;

    long int val = strtol(pbuf, &end, 10);
    if (strcmp(end, pbuf) == 0)
        return false;
    else
        *freqValue = (int)val;

    return true;
}

}; //namespace nvcpu
}; //namespace android
