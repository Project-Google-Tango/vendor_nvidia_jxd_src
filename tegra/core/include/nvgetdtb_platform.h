/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVGETDTB_PLATFORM
#define NVGETDTB_PLATFORM

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Defines the board information stored on eeprom */
typedef struct NvGetDtbBoardInfoRec
{
    // Holds the version number.
    NvU16 Version;
    // Holds the size of 2 byte value of \a BoardId data that follows.
    NvU16 Size;
    // Holds the board number.
    NvU16 BoardId;
    // Holds the SKU number.
    NvU16 Sku;
    // Holds the FAB number.
    NvU8 Fab;
    // Holds the part revision.
    NvU8 Revision;
    // Holds the minor revision level.
    NvU8 MinorRevision;
    // Holds the memory type.
    NvU8 MemType;
    // Holds the power configuration values.
    NvU8 PowerConfig;
    // Holds the SPL reworks, mech. changes.
    NvU8 MiscConfig;
    // Holds the modem bands value.
    NvU8 ModemBands;
    // Holds the touch screen configs.
    NvU8 TouchScreenConfigs;
    // Holds the display configs.
    NvU8 DisplayConfigs;
}NvGetDtbBoardInfo;

/* Board Fab versions */
#define BOARD_FAB_A00 0x0
#define BOARD_FAB_A01 0x1
#define BOARD_FAB_A02 0x2
#define BOARD_FAB_A03 0x3

/* Defines the different types of board whose
 * information is stored in EEPROM
 */
typedef enum
{
    NvGetDtbBoardType_ProcessorBoard = 0,
    NvGetDtbBoardType_PmuBoard,
    NvGetDtbBoardType_DisplayBoard,
    NvGetDtbBoardType_AudioCodec,
    NvGetDtbBoardType_CameraBoard,
    NvGetDtbBoardType_Sensor,
    NvGetDtbBoardType_NFC,
    NvGetDtbBoardType_Debug,
    NvGetDtbBoardType_MaxNumBoards,
}NvGetDtbBoardType;

/* Defines the boardid for main processor board */
typedef enum
{
    NvGetDtbPlatformType_Pluto      = 1580,
    NvGetDtbPlatformType_Dalmore    = 1611,
    NvGetDtbPlatformType_Atlantis   = 1670,
    NvGetDtbPlatformType_Ceres      = 1680,
    NvGetDtbPlatformType_Macallan   = 1545,
    NvGetDtbPlatformType_Ardbeg     = 1780,
    NvGetDtbPlatformType_TN8FFD     = 1761,
    NvGetDtbPlatformType_TN8ERSPOP  = 1922,
    NvGetDtbPlatformType_LagunaFfd  =  363,
    NvGetDtbPlatformType_LagunaErsS =  359,
    NvGetDtbPlatformType_Laguna     =  358,
} NvGetDtbPlatformType;

typedef enum
{
    NvGetDtbSkuType_ShieldErs = 1000,
    NvGetDtbSkuType_TegraNote = 1100,
} NvGetDtbSkuType;

/* Defines the boardid for PMU module board */
typedef enum
{
    NvGetDtbPMUModuleType_E1769 = 1769,
} NvGetDtbPMUModuleType;

/* Defines the boardid for sensor module board */
typedef enum
{
    NvGetDtbSensorModuleType_E1845 = 1845,
} NvGetDtbSensorModuleType;

/**
 * Use to change the state of device to recovery mode
 *
 * @param no arguments
 *
 * @retval NvSuccess if the device is in recovery
 */
NvError nvgetdtb_forcerecovery(void);

/**
 * Use to read the board id from device and print in suitable
 * format.
 *
 * @param no arguments
 *
 * @retval NvSuccess if board id read is successful
 */
NvError nvgetdtb_read_boardid(void);

/**
 * Use to read the board id data transferred via device to host
 *
 * @param BoardInfo buffer in which data is to be read
 * @param Size size of the data to be read
 * @param ListBoardId Determine if the board id is to be printed or not
 *
 * @retval NvSuccess if the device is in recovery
 */
NvError nvgetdtb_getdata(NvU8 *BoardInfo, NvU32 Size, NvBool *ListBoardId);

NvError NvGetdtbReadBoardInfo(NvGetDtbBoardType BoardType, NvGetDtbBoardInfo *pBoardInfo);
void NvGetdtbInitI2cPinmux(void);
NvError NvGetdtbEepromPowerRail(void);
NvError NvGetdtbInitGPIOExpander(void);
void NvGetdtbEnablePowerRail(void);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // NVGETDTB_PLATFORM
