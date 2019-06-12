/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NVMM_EXIF_H
#define INCLUDED_NVMM_EXIF_H

#include "nvcommon.h"
#include "nvrm_memmgr.h"

#if defined(__cplusplus)
extern "C"
{
#endif
#define MAX_EXIF_STRING_IN_BYTES                   (200)
#define MAX_EXIF_MAKER_NOTE_SIZE_IN_BYTES          (2048)
#define MAX_EXIF_IMAGEUNIQUEID_IN_SIZE_IN_BYTES    (33)
#define MAX_GPS_STRING_IN_BYTES                    (32)
#define GPS_DATESTAMP_LENGTH                       (11)
#define MAX_INTEROP_STRING_IN_BYTES                (32)
#define MAX_GPS_PROCESSING_METHOD_IN_BYTES         (40)

    typedef struct
    {
        NvF32 Lux;
        NvU32 Cct; //correlated color temperature
        NvF32 WB_Red; //white balance gain for Red channel
        NvF32 WB_Gr; //white balance gain for Gr channel
        NvF32 WB_Gb; //white balance gain for Gb channel
        NvF32 WB_Blue; //white balance gain for Blue channel
    }NvMMTnrExtensions;

    typedef struct
    {
        NvMMTnrExtensions TnrExtensions;
    }NvMMExifEncodeExtensions;

    typedef struct {
        /* The current sensor frame rate.  Passed down to encoders in order
        to handle frame rate changes. The distinction from true frame
        rate is that due to system performance, frames could be dropped.
        The sensor frame rate is the only reliable metric camera can pass.
        More info in NvBug 309930.  Float because the value will range anywhere
        between 1/2 a frame per second, up to 60.0 frames per second, and
        be influenced by the clock rate. A typical frame rate might be 14.7 fps.
        */
        /* For all fields, treat 0 as 'unknown', (YUV sensors) */
        NvF32 SensorFrameRate; // frames per second
        NvMMExifEncodeExtensions EncodeExtensions;
        NvU32 ExposureTimeNumerator;    // seconds
        NvU32 ExposureTimeDenominator;
        NvS32 ExposureBiasNumerator;    // f-stops (+ more exposure, - less exposure)
        NvS32 ExposureBiasDenominator;
        NvU32 FocalLengthNumerator;     // centimeters
        NvU32 FocalLengthDenominator;
        NvU16 ExposureProgram;
        NvU16 GainISO;         // ISO value, 50, 100, etc.
        NvU16 Orientation_ThumbNail;
        NvU16 MeteringMode;    // Spot, Center, Matrix
        NvU16 FlashUsed;        // 0 = Flash didn't fire, 1=Flash fired, 5=Strobe return light not detected, 7=Strobe return light detected,other=reserved.
        NvU32 MaxApertureNumerator;
        NvU32 MaxApertureDenominator;
        NvU32 FNumberNumerator;
        NvU32 FNumberDenominator;
        NvU32 ZoomFactorNumerator;
        NvU32 ZoomFactorDenominator;
        NvF32 ColorCorrectionMatrix[16];
        NvU16 WhiteBalance;         // 0 = Auto WB, 1 = Manual WB
        NvU16 LightSource;          // 0 = Unknown, 1 = Daylight, 2 = Fluorescent, etc, 255 = Other light source
        NvU16 SceneCaptureType;     // 0 = Standard, 1 = Landscape, 2 = Portrait, 3 = Night scene.
        // NOTE: SceneCaptureType is different than SceneType
        NvU16 ExposureMode;         // 0=Auto, 1=Manual Exposure, 2=Bracket Exposure
        NvU16 Sharpness;
        NvU16 SensingMethod;
        NvU16 SubjectArea[4];
        NvU32 SubjectDistanceNumerator;
        NvU32 SubjectDistanceDenominator;   // 
        /* unit for next three fields is APEX
         * (Additive System of Photographic Exposure)
         */
        NvS32 ShutterSpeedValueNumerator;
        NvS32 ShutterSpeedValueDenominator;
        NvU32 ApertureValueNumerator;
        NvU32 ApertureValueDenominator;
        NvS32 BrightnessValueNumerator;
        NvS32 BrightnessValueDenominator;

        char MakerNote[MAX_EXIF_MAKER_NOTE_SIZE_IN_BYTES];
        NvU32           SizeofMakerNoteInBytes; // 

        /*
        *These are moved to the NvMM0thIFDTags structure
        *These are redundant. Need to be removed.
        *Don't use these.
        *
        */
        char  ImageDescription[MAX_EXIF_STRING_IN_BYTES];    // These 6 fields to be filled by Application --> Dshow/Dmo/Omx --> Jpegenc
        char  Make[MAX_EXIF_STRING_IN_BYTES];

        char  Model[MAX_EXIF_STRING_IN_BYTES];

        char  Copyright[MAX_EXIF_STRING_IN_BYTES];

        char  Artist[MAX_EXIF_STRING_IN_BYTES];

        char  Software[MAX_EXIF_STRING_IN_BYTES];

        char  DateTime[MAX_EXIF_STRING_IN_BYTES];             // Is this needed here?

        // NOTE: DateTime is a string in format "YYYY:MM:DD HH:MM:SS". We may not be able to use buffer timestamp. Buffer timestamp is relative(?).
        // Q: Can we get the date/time from OS? Similar to what we get using "date" and "time" commands in dos/windows?
        char  DateTimeOriginal[MAX_EXIF_STRING_IN_BYTES];         // Same as DateTime

        char  DateTimeDigitized[MAX_EXIF_STRING_IN_BYTES];        // Same as DateTime

        // Unique Image ID string, equivalent to hexadecimal notation in 128 bits
        char  ImageUniqueID[MAX_EXIF_IMAGEUNIQUEID_IN_SIZE_IN_BYTES];

        // add more as needed
    } NvMMEXIFInfo;

    typedef enum
    {
        NvMMExif_Orientation_0_Degrees          = 1,    // Row_0 - visual top,      Col_0 - visual left
        NvMMExif_Orientation_0_Degrees_Flip_H   = 2,    // Row_0 - visual top,      Col_0 - visual right
        NvMMExif_Orientation_180_Degrees        = 3,    // Row_0 - visual bottom,   Col_0 - visual right
        NvMMExif_Orientation_180_Degrees_Flip_H = 4,    // Row_0 - visual bottom,   Col_0 - visual left
        NvMMExif_Orientation_270_Degrees_Flip_H = 5,    // Row_0 - visual left,     Col_0 - visual top
        NvMMExif_Orientation_270_Degrees        = 6,    // Row_0 - visual right,    Col_0 - visual top
        NvMMExif_Orientation_90_Degrees_Flip_H  = 7,    // Row_0 - visual right,    Col_0 - visual bottom
        NvMMExif_Orientation_90_Degrees         = 8,    // Row_0 - visual left,     Col_0 - visual bottom
        NvMMExif_Orientation_Force32            = 0x7FFFFFFF
    } NvMMExif_Orientation;

    // These fields to be filled by Application --> Dshow/Dmo/Omx --> Jpegenc
    typedef struct
    {
        char  ImageDescription[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofImageDescriptionInBytes;

        char  Make[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofMakeInBytes;

        char  Model[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofModelInBytes;

        char  Copyright[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofCopyrightInBytes;

        char  Artist[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofArtistInBytes;

        char  Software[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofSoftwareInBytes;

        char  DateTime[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofDateTimeInBytes;

        char  DateTimeOriginal[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofDateTimeOriginalInBytes;

        char  DateTimeDigitized[MAX_EXIF_STRING_IN_BYTES];
        NvU32 SizeofDateTimeDigitizedInBytes;

        NvU16 filesource; //This one is Exif Tag
        /*@UserComment: 
        First 8 bytes charcter ID code + data

        CharacterIDCode   CodeDesignation(8 Bytes)                                 References
        ASCII       41.H, 53.H, 43.H, 49.H, 49.H, 00.H, 00.H, 00.H           ITU-T T.50 IA5
        JIS         4A.H, 49.H, 53.H, 00.H, 00.H, 00.H, 00.H, 00.H           JIS X208-1990
        Unicode     55.H, 4E.H, 49.H, 43.H, 4F.H, 44.H, 45.H, 00.H           Unicode Standard
        Undefined   00.H, 00.H, 00.H, 00.H, 00.H, 00.H, 00.H, 00.H           Undefined

        */
        char  UserComment[MAX_EXIF_STRING_IN_BYTES]; //This one is Exif Tag
        NvU32 SizeofUserCommentInBytes;

        NvMMExif_Orientation Orientation;
    }NvMM0thIFDTags;

    //Geo tagging support 
    // These fields to be filled by Application --> Dshow/Dmo/Omx --> Jpegenc

    typedef struct{

        NvU32 GPSBitMapInfo; //This has the info of every GPSTag is enabled or not
        NvU32 GPSVersionID;
        NvU32 GPSLatitudeNumerator[3];
        NvU32 GPSLatitudeDenominator[3];
        NvU32 GPSLongitudeNumerator[3];
        NvU32 GPSLongitudeDenominator[3];
        NvU32 GPSTimeStampNumerator[3];
        NvU32 GPSTimeStampDenominator[3];
        NvU32 GPSAltitudeNumerator;
        NvU32 GPSAltitudeDenominator;
        NvU32 GPSDOPNumerator;
        NvU32 GPSDOPDenominator;
        NvU32 GPSImgDirectionNumerator;
        NvU32 GPSImgDirectionDenominator;
        char  GPSLatitudeRef[2];
        char  GPSLongitudeRef[2];
        char  GPSStatus[2];
        char  GPSMeasureMode[2];
        char  GPSImgDirectionRef[2];
        char  GPSSatellites[MAX_GPS_STRING_IN_BYTES];
        char  GPSMapDatum[MAX_GPS_STRING_IN_BYTES];
        char  GPSProcessingMethod[MAX_GPS_PROCESSING_METHOD_IN_BYTES];
        char  GPSDateStamp[GPS_DATESTAMP_LENGTH];
        NvU8  GPSAltitudeRef;

    } NvMMGPSInfo;

//This Structure holds GPS Processing Method tag info.
typedef struct{
    char  GPSProcessingMethod[MAX_GPS_PROCESSING_METHOD_IN_BYTES];

}NvMMGPSProcMethod;

    typedef enum
    {
        NvMMGPSBitMap_LatitudeRef =      0x01,
        NvMMGPSBitMap_LongitudeRef =     0x02,
        NvMMGPSBitMap_AltitudeRef =      0x04,
        NvMMGPSBitMap_TimeStamp =        0x08,
        NvMMGPSBitMap_Satellites =       0x10,
        NvMMGPSBitMap_Status =           0x20,
        NvMMGPSBitMap_MeasureMode =      0x40,
        NvMMGPSBitMap_DOP =              0x80,
        NvMMGPSBitMap_ImgDirectionRef =  0x100,
        NvMMGPSBitMap_MapDatum =         0x200,
        NvMMGPSBitMap_ProcessingMethod = 0x400,
        NvMMGPSBitMap_DateStamp =        0x800,
    }NvMMGPSBitMap;

    //This Structure holds InterOperability IFD tags.
    typedef struct
    {
        char Index[MAX_INTEROP_STRING_IN_BYTES];
    }NvMMInterOperabilityInfo;


#define EXIF_MAKE_LENGTH   (16)
#define EXIF_MODEL_LENGTH  (32)

    // This Structure holds Decoded Exif Tag entries
    typedef struct
    {
        NvU8  Make[EXIF_MAKE_LENGTH]; // Holding the Name of Manufracturer
        NvU8  Model[EXIF_MODEL_LENGTH]; // Holding the Name of Model number of digiCam
        NvU32 ThumbnailCompression;
        NvU32 ThumbnailOffset;
        NvU32 ThumbnailLength;
        NvU32 ThumbnailImageWidth;
        NvU32 ThumbnailImageHeight;
        NvU32 PrimaryImageWidth;
        NvU32 PrimaryImageHeight;
        NvU8  ResolutionUnit;
        NvU64 XResolution;
        NvU64 YResolution;
        NvU8  bpp;
        NvU8 ImageDescription[32];
#if 0
        NvU16 ISOSpeedRatings;
        NvU8  Software[APP1_SOFTWARE_LENGTH];
        NvU8  DateTime[APP1_DATE_TIME_LENGTH];
        NvU8  YCbCrPositioning[APP1_YCBCR_POSITIONING_LENGTH];
        NvU8  ExifVersion[APP1_EXIF_VERSION_LENGTH];
        NvU8  DateTimeOriginal[APP1_DATE_TIME_ORIGINAL_LENGTH];
        NvU8  DateTimeDigitized[APP1_DATE_TIME_DIGITIZED_LENGTH];
        NvU8  ComponentsConfiguration[APP1_COMPONENTS_CONFIGURATION_LENGTH];
        NvU8  MeteringMode[APP1_METERING_MODE_LENGTH];
        NvU8  FlashPixVersion[APP1_FLASH_PIX_VERSION_LENGTH];
        NvU8  ColorSpace[APP1_COLOR_SPACE_LENGTH];
        NvU8  Orientation[APP1_ORIENTATION_LENGTH];
        NvS64 ExposureBiasValue;//[APP1_EXPOSURE_BIAS_VALUE_LENGTH];
        NvU8  FNumber[APP1_FNUMBER_LENGTH];
        NvU8  FlashFiring[APP1_FLASH_FIRING_LENGTH];
        NvU8  FlashReturn[APP1_FLASH_RETURN_LENGTH];
        NvU8  SceneCaptureType[APP1_SCENE_CAPTURE_TYPE_LENGTH];
        NvU8  LightSource[APP1_LIGHT_SOURCE_LENGTH];
        NvU64 SubjectDistanceRange;
        NvU8  WhiteBalance[APP1_LIGHT_WHITE_BALANCE_LENGTH];
        NvU8  CustomRendered[APP1_CUSTOM_RENDERED_LENGTH];
        NvU64 ExposureTime;
        NvU8  ExposureMode[APP1_EXPOSURE_MODE_LENGTH];
        NvU64 DigitalZoomRatio;
        NvU16 InteroperabilityIFDPointer;
#endif
    }NvMM_DECODED_EXIFInfo;


    typedef enum
    {
        NvMMExif_ExposureProgram_NotDef           = 0,
        NvMMExif_ExposureProgram_Manual,
        NvMMExif_ExposureProgram_Normal,
        NvMMExif_ExposureProgram_AperturePriority,
        NvMMExif_ExposureProgram_ShutterPriority,
        NvMMExif_ExposureProgram_Creative,
        NvMMExif_ExposureProgram_action,
        NvMMExif_ExposureProgram_Portrait,
        NvMMExif_ExposureProgram_Landscape,
        NvMMExif_ExposureProgram_Force16 = 0x7FFF
    }NvMMExif_ExposureProgram;

    typedef enum
    {
        NvMMExif_MeteringMode_Unknown = 0,
        NvMMExif_MeteringMode_Average,
        NvMMExif_MeteringMode_CenterWeightedAverage,
        NvMMExif_MeteringMode_Spot,
        NvMMExif_MeteringMode_MultiSpot,
        NvMMExif_MeteringMode_Pattern,
        NvMMExif_MeteringMode_Partial,
        NvMMExif_MeteringMode_Other = 255,
        NvMMExif_Force16 = 0x7FFF
    } NvMMExif_MeteringMode;

    typedef enum
    {
        NvMMExif_ColorSpace_sRGB = 1,
        NvMMExif_ColorSpace_Uncalibrated = 0xffff
    } NvMMExif_ColorSpace;

    typedef enum
    {
        NvMMExif_WhiteBalance_Auto = 0,
        NvMMExif_WhiteBalance_Manual = 1,
        NvMMExif_WhiteBalance_Force16 = 0x7FFFF
    } NvMMExif_WhiteBalance;

    typedef enum
    {
        NvMMExif_LightSource_Unknown = 0,
        NvMMExif_LightSource_Daylight = 1,
        NvMMExif_LightSource_Fluorescent = 2,
        NvMMExif_LightSource_Tungsten = 3,              // Incandescent light
        NvMMExif_LightSource_Flash = 4,
        NvMMExif_LightSource_FineWeather = 9,
        NvMMExif_LightSource_Cloudy = 10,
        NvMMExif_LightSource_Shade = 11,
        NvMMExif_LightSource_DaylightFluorescent = 12,  // (D 5700 - 7100K)
        NvMMExif_LightSource_DayWhiteFluorescent = 13,  // (N 4600 - 5400K)
        NvMMExif_LightSource_CoolWhiteFluorescent = 14, // (W 3900 - 4500K)
        NvMMExif_LightSource_WhiteFluorescent = 15,     // (WW 3200 - 3700K)
        NvMMExif_LightSource_StandardLightA = 17,
        NvMMExif_LightSource_StandardLightB = 18,
        NvMMExif_LightSource_StandardLightC = 19,
        NvMMExif_LightSource_D55 = 20,
        NvMMExif_LightSource_D65 = 21,
        NvMMExif_LightSource_D75 = 22,
        NvMMExif_LightSource_D50 = 23,
        NvMMExif_LightSource_ISOStudioTungsten = 24,
        NvMMExif_LightSource_Other = 255,
        NvMMExif_LightSource_Force16 = 0x7FFFF
    } NvMMExif_LightSource;

    typedef enum
    {
        NvMMExif_SceneCaptureType_Standard = 0,
        NvMMExif_SceneCaptureType_Landscape,
        NvMMExif_SceneCaptureType_Portrait,
        NvMMExif_SceneCaptureType_NightScene,
        NvMMExif_SceneCaptureType_Force16 = 0x7FFF
    } NvMMExif_SceneCaptureType;

    typedef enum
    {
        NvMMExif_ExposureMode_Auto = 0,
        NvMMExif_ExposureMode_ManualExposure,
        NvMMExif_ExposureMode_BracketExposure,
        NvMMExif_ExposureMode_Force16 = 0x7FFF
    } NvMMExif_ExposureMode;

    typedef enum
    {
        NvMMExif_SubjectDistance_Unknown = 0
    } NvMMExif_SubjectDistance;

    typedef enum
    {
        // No units, aspect ratio only specified
        NvMMExif_ThumbnailCompression_None = 0,
        // Pixels per inch 
        NvMMExif_ThumbnailCompression_Uncompressed = 1,
        // Pixels per centimetre 
        NvMMExif_ThumbnailCompression_JPEG = 6,
        // unspecified
        NvMMExif_ThumbnailCompression_Uncalibrated = 0x7FFF
    } NvMMExif_ThumbnailCompression;


    typedef enum
    {
        NvMMExif_Flash_NotFired = 0,
        NvMMExif_Flash_Fired = 0x01,
        NvMMExif_Flash_StrobeRetLightNotDetect = 0x04,
        NvMMExif_Flash_StrobeRetLightDetect = 0x06,
        NvMMExif_Flash_CompulsoryFiring = 0x08,
        NvMMExif_Flash_CompulsorySuppression = 0x10,
        NvMMExif_Flash_AutoMode = 0x18,
        NvMMExif_Flash_NoFlashFunction = 0x20,
        NvMMExif_Flash_RedEyeSupported = 0x40,
        NvMMExif_Flash_Force16 = 0x7FFFF
    } NvMMExif_Flash;

    typedef enum
    {
        NvMMExif_FileSource_DSC = 3,
        NvMMExif_FileSource_Force16 = 0x7FFFF
    } NvMMExif_FileSource;



#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_EXIF_H


