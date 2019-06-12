/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3.h"
#include "nvcommon.h"
#include <stdio.h>
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

#define MAX_LINE_LENGTH 256
#define NVCAMERA_FILE_LOCATION "/etc/nvcamera.conf"
#define DEFAULT_NUM_CAMERAS 3

namespace android {

static void NvCameraHal_removeSpaces(char *buf);
static NvError NvCameraHal_parseVersion(char *buf, NvU16 *version);
static NvError
NvCameraHal_parseLine(
        char *buf,
        NvU16 *cameraID,
        char *deviceLocation,
        int *facing,
        int *orientation,
        NvCameraHal_CameraType *cameraType);
static int NvCameraHal_generateDefaultCameras(NvCameraHal_CameraInfo **sCameraInfo);
static NvError NvCameraHal_parseNvU16(char **string, NvU16 *num);
static NvError NvCameraHal_parseNvS16(char **string, NvS16 *num);
static inline NvBool NvCameraHal_ValidEntry(NvCameraHal_CameraType cameraType);

int NvCameraHal_parseCameraConf(
        NvCameraHal_CameraInfo **sCameraInfo)
{
    FILE *hConfFile = NULL;
    char buf[MAX_LINE_LENGTH]={'\0'};
    char tmp[16];
    NvU16 version = 0;
    NvU16 line = 1;
    int numCameras = 0;
    int length;
    NvBool foundVersion = NV_FALSE;
    *sCameraInfo = NULL;

    hConfFile = fopen(NVCAMERA_FILE_LOCATION, "r");

    if (!hConfFile) {
        NV_LOGV(HAL3_PARSE_CONFIG_TAG, "No file /etc/nvcamera.conf");
        // No conf file means no camera on board, but need to check usb camera
        goto checkusb;
    }

    if (!fgets(buf, MAX_LINE_LENGTH, hConfFile))
    {
        NV_LOGE("Can't read /etc/nvcamera.conf");
        goto cleanup;
    }
    while (!feof(hConfFile))
    {
        NvU16 cameraID;
        NvCameraHal_CameraType cameraType = NVCAMERAHAL_CAMERATYPE_MONO;
        int facing = CAMERA_FACING_FRONT;
        int orientation = 0;
        char deviceLocation[MAX_LINE_LENGTH];

        NvCameraHal_removeSpaces(buf);           // remove white spaces
        line++;

        // ignore comments and empty lines but parse rest
        if ((buf[0]!='#')&&(strncmp(buf,"//",2))&&(buf[0]!='\0'))
        {
            NvError err;

            if (!foundVersion)
            {
                err = NvCameraHal_parseVersion(buf, &version);
                if (err != NvSuccess)
                {
                    NV_LOGE("in /etc/nvcamera.conf: missing version (%d)", line);
                    goto cleanup;
                }
                else
                    foundVersion = NV_TRUE;
            }
            else
            {
                err = NvCameraHal_parseLine(buf, &cameraID, deviceLocation, &facing,
                        &orientation, &cameraType);
                if (err == NvSuccess)
                {
                    if (NvCameraHal_ValidEntry(cameraType))
                    {
                        numCameras++;
                        length = NvOsStrlen(deviceLocation)+1;
                        NvCameraHal_CameraInfo *ret;
                        ret = (NvCameraHal_CameraInfo *)NvOsRealloc(*sCameraInfo, numCameras*sizeof(NvCameraHal_CameraInfo));
                        if (ret == NULL)    // realloc failed
                        {
                            numCameras--;
                            goto cleanup;
                        }
                        *sCameraInfo = ret;
                        (*sCameraInfo)[numCameras-1].deviceLocation = (char *)NvOsAlloc(length*sizeof(char));
                        if ((*sCameraInfo)[numCameras-1].deviceLocation == NULL)
                            goto cleanup;
                        NvOsStrncpy((*sCameraInfo)[numCameras-1].deviceLocation, deviceLocation, length);
                        NvOsMemset(&((*sCameraInfo)[numCameras-1].camInfo), 0, sizeof(struct camera_info));
                        (*sCameraInfo)[numCameras-1].camInfo.facing = facing;
                        (*sCameraInfo)[numCameras-1].camInfo.orientation = orientation;
                        (*sCameraInfo)[numCameras-1].camType = cameraType;
                    }
                    else
                    {
                        NV_LOGE("in /etc/nvcamera.conf(%d): invalid entry, cameraType %d", line, cameraType);
                    }
                }
                else
                {
                    NV_LOGW(HAL3_PARSE_CONFIG_TAG, "in /etc/nvcamera.conf(%d): malformed line.", line);
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

checkusb:
    // check if usb camera is present
    NvCameraHal_UpdateUsbExistence(sCameraInfo,&numCameras);

    if (hConfFile)
        fclose(hConfFile);
    return numCameras;

cleanup:
    if (numCameras > 0)    // free up memory used so far
    {
        if ((*sCameraInfo))
        {
            for (int i = 0; i < numCameras; i++)
            {
                NvOsFree((*sCameraInfo)[i].deviceLocation);
                (*sCameraInfo)[i].deviceLocation = NULL;
            }
            NvOsFree(*sCameraInfo);
            *sCameraInfo = NULL;
        }
    }
    if (hConfFile)
        fclose(hConfFile);
    return 0; //  return 0 camera directly
}

void NvCameraHal_UpdateUsbExistence(
    NvCameraHal_CameraInfo **sCameraInfo,
    int *numCameras)
{
    int fd;
    NvCameraHal_CameraInfo *ret;
    int tempnum = *numCameras;

    /* open first device */
    fd = open ("/dev/video0", O_RDWR | O_NONBLOCK, 0);

    if (fd < 0)
    {
        /* First device opening failed so  trying with the next device */
        fd = open ("/dev/video1", O_RDWR | O_NONBLOCK, 0);
        if (fd < 0)
        {
            return; // usb device not found
        }
    }

    if (fd >= 0)
    {
        close(fd);
    }

    tempnum++;
    ret = (NvCameraHal_CameraInfo *)NvOsRealloc(*sCameraInfo,
                        tempnum*sizeof(NvCameraHal_CameraInfo));
    if (ret == NULL)    // realloc failed
    {
        return; // return without incrementing number of cameras
    }

    *sCameraInfo = ret;
    (*sCameraInfo)[tempnum-1].deviceLocation = (char *)NvOsAlloc((strlen("Default")+1)*sizeof(char));
    if ((*sCameraInfo)[tempnum-1].deviceLocation == NULL)
        goto cleanup;

    strncpy((*sCameraInfo)[tempnum-1].deviceLocation, "Default", strlen("Default")+1);

    (*sCameraInfo)[tempnum-1].camInfo.orientation = 0;
    (*sCameraInfo)[tempnum-1].camType = NVCAMERAHAL_CAMERATYPE_USB;

    *numCameras = tempnum;
    return;

cleanup:
    if ((*sCameraInfo))
    {
        int i = tempnum;
        NvOsFree((*sCameraInfo)[i].deviceLocation);
        (*sCameraInfo)[i].deviceLocation = NULL;
    }
    return;
}

typedef struct {
    const char *key;
    int value;
} E_int;

typedef struct {
    const char *key;
    NvCameraHal_CameraType value;
} E_NvCameraHal_CameraType;

E_int NvCameraHal_FacingTable[] = {
    {"front,", CAMERA_FACING_FRONT},
    {"back,", CAMERA_FACING_BACK},
    {NULL, 0}
};

E_NvCameraHal_CameraType NvCameraHal_TypeTable[] = {
    {"mono", NVCAMERAHAL_CAMERATYPE_MONO},
    {"stereo", NVCAMERAHAL_CAMERATYPE_STEREO},
    {"usb", NVCAMERAHAL_CAMERATYPE_USB},
    {NULL, NVCAMERAHAL_CAMERATYPE_UNDEFINED}
};


static NvError NvCameraHal_parseFacing(char **string, int *facing)
{
    int i = 0;
    int length, res;

    while (NvCameraHal_FacingTable[i].key != NULL)
    {
        length = NvOsStrlen(NvCameraHal_FacingTable[i].key);
        res = strncmp(*string, NvCameraHal_FacingTable[i].key, length);
        if (res == 0)
        {
            *facing = NvCameraHal_FacingTable[i].value;
            (*string) += NvOsStrlen(NvCameraHal_FacingTable[i].key);
            return NvSuccess;
        }
        i++;
    }

    return NvError_BadParameter;
}

static NvError NvCameraHal_parseType(char **string, NvCameraHal_CameraType *type)
{
    int i = 0;
    int length, res;

    while (NvCameraHal_TypeTable[i].key != NULL)
    {
        length = NvOsStrlen(NvCameraHal_TypeTable[i].key);
        res = strncmp(*string, NvCameraHal_TypeTable[i].key, length);
        if (res == 0)
        {
            *type = NvCameraHal_TypeTable[i].value;
            (*string) += NvOsStrlen(NvCameraHal_TypeTable[i].key);
            return NvSuccess;
        }
        i++;
    }

    return NvError_BadParameter;
}

// convert a number string pointed to by **string to an unsigned
// integer. Returns NV_FALSE if no digits found.  Sets **string
// to location immediately following all digits
static NvError NvCameraHal_parseNvU16(char **string, NvU16 *num)
{
    NvU32 number = 0;
    if (((*string)[0] > '9')||((*string)[0] < '0'))    // no number
        return NvError_BadParameter;
    while (((*string)[0] <= '9') && ((*string)[0] >= '0') &&
            ((*string)[0] != '\0'))
    {
        number *= 10;
        number += (*string)[0] - '0';
        if (number > (1<<16))
            number = 0xffffffff;
        (*string)++;
    }
    if (number > (1<<16))    // number bigger then 16 bit
        return NvError_BadParameter;

    *num = number;
    return NvSuccess;
}

static NvError NvCameraHal_parseNvS16(char **string, NvS16 *num)
{
    NvU16 number;
    NvS16 neg = 1;
    NvError ret;

    if ((*string)[0] == '-')
    {
        neg = -1;
        (*string)++;
    }
    ret = NvCameraHal_parseNvU16(string, &number);
    if (ret != NvSuccess)
        return ret;

    if (number > (1<<15))    // number bigger then 16 bit w/o sig
        return NvError_BadParameter;

    *num = number;
    *num *= neg;

    return NvSuccess;
}

static void NvCameraHal_removeSpaces(char *buf)
{
    char *p1;
    int i;

    // get rid of spaces in line up to MAX_LINE_LENGTH
    p1 = buf;
    i = 0;
    while ((i <  MAX_LINE_LENGTH) && (buf[i] != '\0'))
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

// takes a string and tries to parse the version token
// if successful returns with the version number in
// the parameter
static NvError NvCameraHal_parseVersion(char *buf, NvU16 *version)
{
    NvU8 i;
    char *pbuf;
    char tmp[5];
    NvError ret;

    // make sure string starts with "version="
    if (strncmp(buf, "version=", 8))
        return NvError_BadParameter;

    pbuf = buf + 8;
    *version = 0;
    ret = NvCameraHal_parseNvU16(&pbuf, version);
    if (ret != NvSuccess)
        return NvError_BadParameter;

    return NvSuccess;
}

// takes a string in buf and tries to parse camera information
// out of it.  All parameters will be returned with the parsed
// values if function returns NvSuccess otherwise function
// returns NvError_BadParameter.  buf and deviceLocation must
// be MAX_LINE_LENGTH buffers.
static NvError
NvCameraHal_parseLine(
    char *buf,
    NvU16 *cameraID,
    char *deviceLocation,
    int *facing,
    int *netOrientation,
    NvCameraHal_CameraType *cameraType)
{
    NvBool neg;
    char *pbuf;
    char tmp[3];
    NvS16 sensorOrientation = 0;
    NvU16 i = 0;
    NvError ret;
    NvS32 dispOrientation = 0;

    *cameraID = 0;

    pbuf = buf;

    // parse the camera#= entry where # is a number
    if (strncmp(pbuf, "camera", 6))
        return NvError_BadParameter;
    pbuf += 6;
    ret = NvCameraHal_parseNvU16(&pbuf, cameraID);
    if (ret != NvSuccess)
        return NvError_BadParameter;
    if (pbuf[0] != '=')
        return NvError_BadParameter;
    pbuf++;

    // get the device location as a string
    // accept alphanumeric '_' and '-' in device names
    while ((i < MAX_LINE_LENGTH) && (((pbuf[0] <= '9') && (pbuf[0] >= '/')) ||
                ((pbuf[0] <= 'Z')&&(pbuf[0] >= 'A')) ||
                ((pbuf[0] <= 'z')&&(pbuf[0] >= 'a')) ||
                (pbuf[0] == '-') || (pbuf[0] == '_')))
    {
        deviceLocation[i++] = pbuf[0];
        pbuf++;
    }
    deviceLocation[i] = '\0';
    // check for ,
    if (pbuf[0] != ',')
        return NvError_BadParameter;
    pbuf++;

    // parse whether the camera is front or back facing
    ret = NvCameraHal_parseFacing(&pbuf, facing);
    if (ret != NvSuccess)
        return NvError_BadParameter;

    // parse the orientation of the camera
    // save it as a signed integer though only the values
    // 0, 90, 180, and 270 are supported so far
    ret = NvCameraHal_parseNvS16(&pbuf, &sensorOrientation);
    if (ret != NvSuccess)
        return NvError_BadParameter;

    // validate currently supported orientations
    if ((sensorOrientation != 0) && (sensorOrientation != 90) &&
            (sensorOrientation != 180) && (sensorOrientation != 270))
    {
        return NvError_BadParameter;
    }

    // Get orientation info from display device
    // NvCameraHal_getDisplayOrientation() returns 0 or 90.
    // Orientation value in nvcamera.conf can override if it
    // is 270 or 180.
    dispOrientation = NvCameraHal_getDisplayOrientation();

    if((dispOrientation == 0) && (sensorOrientation == 180))
    {
        *netOrientation = 180;
    }
    else if((dispOrientation == 90) && (sensorOrientation == 270))
    {
        *netOrientation = 270;
    }
    else
    {
        *netOrientation = dispOrientation;
    }

    // check for ,
    if (pbuf[0] != ',')
        return NvError_BadParameter;
    pbuf++;

    // parse the type of camera
    ret = NvCameraHal_parseType(&pbuf, cameraType);
    if (ret != NvSuccess)
    {
        *cameraType = NVCAMERAHAL_CAMERATYPE_UNDEFINED;
        return NvError_BadParameter;
    }
    return NvSuccess;
}

// set default configuration (3 cameras, front, back and usb)
int NvCameraHal_generateDefaultCameras(NvCameraHal_CameraInfo **sCameraInfo)
{
    char defaultString[] = "Default";

    *sCameraInfo = (NvCameraHal_CameraInfo *)NvOsAlloc(DEFAULT_NUM_CAMERAS*
            sizeof(NvCameraHal_CameraInfo));
    if (*sCameraInfo == NULL)
        return 0;
    for (int i = 0; i < DEFAULT_NUM_CAMERAS; i++)
    {
        // empty string
        (*sCameraInfo)[i].deviceLocation = (char *)NvOsAlloc(sizeof(defaultString));
        if ((*sCameraInfo)[i].deviceLocation == NULL)
        {
            // clean up on failed alloc
            for (int j = 0; j < i; j++)
            {
                NvOsFree((*sCameraInfo)[j].deviceLocation);
                (*sCameraInfo)[j].deviceLocation = NULL;
            }
            NvOsFree(*sCameraInfo);
            *sCameraInfo = NULL;
            return 0;
        }
        NvOsStrncpy((*sCameraInfo)[i].deviceLocation, defaultString, sizeof(defaultString));
        (*sCameraInfo)[i].camType = NVCAMERAHAL_CAMERATYPE_MONO;
        (*sCameraInfo)[i].camInfo.orientation = 0;
    }
    (*sCameraInfo)[0].camInfo.facing = CAMERA_FACING_BACK;
    (*sCameraInfo)[1].camInfo.facing = CAMERA_FACING_FRONT;
#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
    (*sCameraInfo)[2].camInfo.facing = CAMERA_FACING_UNKNOWN;
#endif
    (*sCameraInfo)[2].camType = NVCAMERAHAL_CAMERATYPE_USB;
    return DEFAULT_NUM_CAMERAS;
}

static inline NvBool
NvCameraHal_ValidEntry(
    NvCameraHal_CameraType cameraType)
{
    // USB entry only valid when USB detected
    bool isValidEntry =
        (cameraType == NVCAMERAHAL_CAMERATYPE_STEREO) ||        // stereo camera is always valid
        (cameraType == NVCAMERAHAL_CAMERATYPE_MONO);            // mono camera is always valid
    return isValidEntry ? NV_TRUE : NV_FALSE;
}

// get display orientation by reading out /sys/devices/tegradc.0/mode
#define TEGRA_DC0_DEVICE_LOCATION "/sys/class/graphics/fb0/device/mode"
#define TEGRA_DC0_DEVICE_ROTATION "/sys/class/graphics/fb0/device/panel_rotation"
#define TEGRA_DC0_XRES "h_active"
#define TEGRA_DC0_YRES "v_active"
int NvCameraHal_getDisplayOrientation()
{
    FILE* file = NULL;
    char mode[80];
    NvU32 value;
    NvU32 xres = 0;
    NvU32 yres = 0;
    NvS32 rotation = 0;

    file = fopen(TEGRA_DC0_DEVICE_LOCATION, "r");
    if (!file)
    {
        NV_LOGE("%s: Failed to read tegra display device mode.", __FUNCTION__);
        // return zero degree if there is no compatible display device
        return 0;
    }

    do {
        if (fscanf(file, "%s %d\n", mode, &value) == 2)
        {
            if (!strncmp(TEGRA_DC0_XRES, mode, strlen(TEGRA_DC0_XRES)))
                xres = value;
            if (!strncmp(TEGRA_DC0_YRES, mode, strlen(TEGRA_DC0_YRES)))
                yres = value;
        }
   } while (!feof(file));

    fclose(file);
    file = NULL;
    // fetch rotation value
    file = fopen(TEGRA_DC0_DEVICE_ROTATION, "r");
    if (!file)
    {
        NV_LOGE("%s: Failed to open tegra display orientation err:%s",
                __FUNCTION__, strerror(errno));
        // ignore rotation
    }
    else
    {
        do {
            if (fscanf(file, "%d\n", &rotation) != 1)
            {
                NV_LOGE("%s: Failed to read tegra display orientation err:%s",
                        __FUNCTION__, strerror(errno));
            }
        } while (!feof(file));
        fclose(file);
        NV_ASSERT(rotation == 0 || rotation == 90 || rotation == 180 ||
                rotation == 270);
    }

    if (xres < yres)
    {
        return abs(rotation + 90) % 180;
    }
    else
    {
        return rotation % 180;
    }
}

}

