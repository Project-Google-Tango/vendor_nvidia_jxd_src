/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "imager_util.h"
#include "nvodm_imager_guid.h"

#if (BUILD_FOR_AOS == 0)
#include <linux/ioctl.h>
#endif

#define DEBUG_PRINTS 0

/**
 * LoadOverridesFile searches for and loads the contents of a
 * camera overrides file. If not found or empty, NULL is returned
 * and the error is logged. If found, the file is read into an
 * allocated buffer, and the buffer is returned. The caller owns
 * any memory returned.
 */
char*
LoadOverridesFile(const char *pFiles[], NvU32 len)
{
    NvOdmOsStatType Stat;
    NvOdmOsFileHandle hFile = NULL;
    char *pTempString = NULL;
    NvU32 i;

    for (i = 0; i < len; i++)
    {
#if DEBUG_PRINTS
        NvOsDebugPrintf("Imager: looking for override file [%s] %d/%d\n",
            pFiles[i], i+1, len);
#endif
        if (NvOdmOsStat(pFiles[i], &Stat) && Stat.size > 0) // found one
        {
            pTempString = NvOdmOsAlloc((size_t)Stat.size + 1);
            if (pTempString == NULL)
            {
                NV_ASSERT(pTempString != NULL &&
                          "Couldn't alloc memory to read config file");
                break;
            }
            if (!NvOdmOsFopen(pFiles[i], NVODMOS_OPEN_READ, &hFile))
            {
                NV_ASSERT(!"Failed to open a file that fstatted just fine");
                NvOdmOsFree(pTempString);
                pTempString = NULL;
                break;
            }
            NvOdmOsFread(hFile, pTempString, (size_t)Stat.size, NULL);
            pTempString[Stat.size] = '\0';
            NvOdmOsFclose(hFile);

            break;    // only handle the first file
        }
    }

#if DEBUG_PRINTS
    if(pTempString == NULL)
    {
        NvOsDebugPrintf("Imager: No override file found.\n");
    }
    else
    {
        NvOsDebugPrintf("Imager: Found override file data [%s]\n",
            pFiles[i]);
    }
#endif

    return pTempString;
}

/**
 * LoadBlobFile searches factory calibration blob file.
 * If not found or empty, NV_FALSE is returned.
 * If found, the blob is read into an pre-allocated buffer.
 */
NvBool
LoadBlobFile(const char *pFiles[], NvU32 len, NvU8 *pBlob, NvU32 blobSize)
{
    NvOdmOsStatType Stat;
    NvOdmOsFileHandle hFile = NULL;
    NvU32 i;

    for (i = 0; i < len; i++)
    {
#if DEBUG_PRINTS
        NvOsDebugPrintf("Imager: looking for override file [%s] %d/%d\n",
            pFiles[i], i+1, len);
#endif
        if (NvOdmOsStat(pFiles[i], &Stat) && Stat.size > 0) // found one
        {
            if ((NvU64) blobSize < Stat.size) // not enough space to store the blob
            {
                NvOsDebugPrintf("Insufficient memory for reading blob\n");
                continue;   // keep searching for blob files
            }

            if (!NvOdmOsFopen(pFiles[i], NVODMOS_OPEN_READ, &hFile))
            {
                NV_ASSERT(!"Failed to open a file that fstatted just fine");
                break;
            }
            NvOdmOsFread(hFile, pBlob, (size_t)Stat.size, NULL);
            NvOdmOsFclose(hFile);
            return NV_TRUE;    // only handle the first valid blob file
        }
    }

    return NV_FALSE;
}

/**
 * Read factory from EEPROM
 *
 * Typically, we expect this to read the whole 1K data from EEPROM,
 * but if the implementation returns an EEPROM factory data which is
 * less than 1K bytes, it would be alright.
 */
NvBool
LoadFactoryFromEEPROM(
    NvOdmImagerHandle hImager,
    int camera_fd,
    NvU32 ioctl_eeprom,
    void *pValue)
{
    int ret;
    NvBool Status;
    NvOdmImagerPowerLevel PreviousPowerLevel;

    if(!hImager || !hImager->pSensor)
        return NV_FALSE;

    if(camera_fd < 0)
        return NV_FALSE;

    hImager->pSensor->pfnGetPowerLevel(hImager, &PreviousPowerLevel);
    if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
    {
        ret = hImager->pSensor->pfnSetPowerLevel(hImager, NvOdmImagerPowerLevel_On);
        if (!ret)
        {
            NV_DEBUG_PRINTF(("%s: failed to set power to ON.  Aborting EEPROM read.\n", __FUNCTION__));
            return NV_FALSE;
        }
    }

    ret = ioctl(camera_fd, ioctl_eeprom, (NvU8 *) pValue);

    if(ret < 0)
    {
        Status = NV_FALSE;
        NV_DEBUG_PRINTF(("%s: ioctl to get FactoryCalibration data failed %s\n", __FUNCTION_, _strerror(errno)));
    }
    else
    {
        Status = NV_TRUE;
        NV_DEBUG_PRINTF(("%s: ioctl to get FactoryCalibration data success.\n", __FUNCTION__));
    }

    if (PreviousPowerLevel != NvOdmImagerPowerLevel_On)
    {
        ret = hImager->pSensor->pfnSetPowerLevel(hImager, PreviousPowerLevel);
        if (!ret)
            NV_DEBUG_PRINTF(("%s: warning: failed to set power to previous state after EEPROM read\n", __FUNCTION__));
    }
    return Status;
}

static void
GetPixelAspectRatioCorrectedModeDimension(
    const NvOdmImagerSensorMode *pMode,
    NvSize *pDimension)
{
    NV_ASSERT(pMode->PixelAspectRatio >= 0.0);

    if (pMode->PixelAspectRatio > 1.0)
    {
        pDimension->width = pMode->ActiveDimensions.width;
        pDimension->height = (NvS32)(pMode->ActiveDimensions.height /
                                      pMode->PixelAspectRatio);
    }
    else if ((pMode->PixelAspectRatio > 0.0) && (pMode->PixelAspectRatio < 1.0))
    {
        pDimension->width = (NvS32)(pMode->ActiveDimensions.width *
                                    pMode->PixelAspectRatio);
        pDimension->height = pMode->ActiveDimensions.height;
    }
    else
    {
        *pDimension = pMode->ActiveDimensions;
    }

    // round to 2-pixel
    pDimension->width = (pDimension->width + 1) & ~1;
    pDimension->height = (pDimension->height + 1) & ~1;
}

static NvBool
IsAspectRatioSame(
    const NvSize ASize,
    const NvSize BSize)
{
    // 16/9 = 1.777 or 1.11000111 in binary
    // 4/3 = 1.333 or 1.01010101 in binary
    // So to check one digit accuracy after
    // decimal i.e. 0.1, floating point operation
    // is used and difference between the two ratio
    // is compared with 0.1.
    // This code is used couple of times only so
    // using floating point does effect performance.

    NvF32 ARatio, BRatio, diff;
    ARatio = ((NvF32)ASize.width/ASize.height);
    BRatio = ((NvF32)BSize.width/BSize.height);
    diff = (ARatio > BRatio) ? ARatio - BRatio : BRatio - ARatio;
    if (diff<0.1)
        return NV_TRUE;
    else
        return NV_FALSE;
}

NvU32
NvOdmImagerGetBestSensorMode(
    const NvOdmImagerSensorMode* pRequest,
    const SensorSetModeSequence* pModeList,
    NvU32 NumSensorModes)
{
    NvU32 i;
    NvU32 BestMode = 0;
    NvS32 CurrentMatchScore = -1;

    // Match Score bit for Preference Parameter is set in the following order:
    #define CropModeBit 0x1 // Bit 1: Crop Mode
    #define AspectRatioBit 0x2 // Bit 2: Aspect Ratio, only for video mode
    #define FrameRateBit 0x4 // Bit 3: Frame Rate
    #define ResolutionBit 0x8 // Bit 4: Resolution

    for (i = 0; i < (NvU32)NumSensorModes; i++)
    {
        const NvOdmImagerSensorMode* pMode = &pModeList[i].Mode;
        NvS32 MatchScore = 0;
        NvSize CorrectedModeResolution = {0, 0};

        GetPixelAspectRatioCorrectedModeDimension(pMode, &CorrectedModeResolution);

        //Give Ist Preference to resolution while selecting a mode
        if (pRequest->ActiveDimensions.width <= CorrectedModeResolution.width &&
        pRequest->ActiveDimensions.height <= CorrectedModeResolution.height)
        {
            MatchScore = (MatchScore) | (ResolutionBit);
        }

        //Give IInd Preference to frame rate
        if (pRequest->PeakFrameRate <= pMode->PeakFrameRate)
        {
            MatchScore = (MatchScore) | (FrameRateBit);
        }

        //In case of Video Capture IIIrd preference is given to aspect ratio
        // while in case of Still capture it is ignored.
        if (IsAspectRatioSame(pMode->ActiveDimensions, pRequest->ActiveDimensions) &&
        ((pRequest->Type == NvOdmImagerModeType_Movie) ||
        (pRequest->Type == NvOdmImagerModeType_Preview)))
        {
            MatchScore = (MatchScore) | (AspectRatioBit);
        }

        //Give Final Preference Crop Mode, currently
        //it could be partial or No Crop Mode
        if (pMode->CropMode == NvOdmImagerNoCropMode)
        {
            MatchScore = (MatchScore) | (CropModeBit);
        }

        if (MatchScore > CurrentMatchScore)
        {
            //This mode is selected over previous mode
            BestMode = i;
            CurrentMatchScore = MatchScore;
        }
        else if (MatchScore == CurrentMatchScore)
        {
            // There is a tie between two modes
            //further we have to choose between these two
            switch(MatchScore>>2)
            {
#define MODE_WIDTH_SCALING(_m) \
    (((_m).PixelAspectRatio == 0.0f || (_m).PixelAspectRatio >= 1.0f) ? \
    (1.0f) : ((_m).PixelAspectRatio))

#define MODE_HEIGHT_SCALING(_m) \
    (((_m).PixelAspectRatio <= 1.0f) ? \
    (1.0f) : (1.0f / (_m).PixelAspectRatio))

#define BIGGER_RESOLUTION_MODE(_a, _b) \
    ((NvF32)_a.ActiveDimensions.width * MODE_WIDTH_SCALING(_a) < \
    (NvF32)_b.ActiveDimensions.width * MODE_WIDTH_SCALING(_b) || \
    (NvF32)_a.ActiveDimensions.height * MODE_HEIGHT_SCALING(_a) < \
    (NvF32)_b.ActiveDimensions.height * MODE_HEIGHT_SCALING(_b))

#define SMALLER_RESOLUTION_MODE(_a, _b) \
    ((NvF32)_a.ActiveDimensions.width * MODE_WIDTH_SCALING(_a) > \
    (NvF32)_b.ActiveDimensions.width * MODE_WIDTH_SCALING(_b) || \
    (NvF32)_a.ActiveDimensions.height * MODE_HEIGHT_SCALING(_a) > \
    (NvF32)_b.ActiveDimensions.height * MODE_HEIGHT_SCALING(_b))

            //0: Resolution doesn't match, frame rate doesn't match
            case 0:
            //1: Resolution doesn't match, frame rate matches
            case 1:
                // choose the one with a bigger resolution
                if (BIGGER_RESOLUTION_MODE(pModeList[BestMode].Mode, pModeList[i].Mode))
                BestMode = i;
            break;

            // 2: Resolution matches, frame rate doesn't match
            case 2:
                // choose the one with a higher frame rate
                if (pModeList[BestMode].Mode.PeakFrameRate < pModeList[i].Mode.PeakFrameRate)
                {
                    BestMode = i;
                }
                // choose the one with a smaller resolution if frame rates are the same
                else if (pModeList[BestMode].Mode.PeakFrameRate == pModeList[i].Mode.PeakFrameRate)
                {
                    if (SMALLER_RESOLUTION_MODE(pModeList[BestMode].Mode, pModeList[i].Mode))
                        BestMode = i;
                }
                break;
            // 3: Resolution matches, frame rate matches.
            case 3:
            {
                // choose the one with a smaller resolution
                if (SMALLER_RESOLUTION_MODE(pModeList[BestMode].Mode, pModeList[i].Mode))
                BestMode = i;
                break;
            }
        }
        }
    }

#if DEBUG_PRINTS
    NvOsDebugPrintf("NvOdmImagerGetBestSensorMode: requested %dx%d, %3.1fFPS, type %u selected %dx%d, %3.1fFPS (%u)\n",
                    pRequest->ActiveDimensions.width,
                    pRequest->ActiveDimensions.height,
                    pRequest->PeakFrameRate,
                    pRequest->Type,
                    pModeList[BestMode].Mode.ActiveDimensions.width,
                    pModeList[BestMode].Mode.ActiveDimensions.height,
                    pModeList[BestMode].Mode.PeakFrameRate,
                    BestMode);
#endif
    return BestMode;
}
