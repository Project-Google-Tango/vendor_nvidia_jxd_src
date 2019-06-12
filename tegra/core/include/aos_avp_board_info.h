/*
 * Copyright (c) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_AVP_BOARD_INFO_H
#define INCLUDED_AOS_AVP_BOARD_INFO_H

#include "nvcommon.h"
#include "nverror.h"

/* Defines the different types of board whose
 * information is stored in EEPROM
 */
typedef enum
{
    NvBoardType_ProcessorBoard = 1,
    NvBoardType_PmuBoard,
    NvBoardType_DisplayBoard,
}NvBoardType;

/* Defines the board information stored on eeprom */
typedef struct NvBoardInfoRec
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
}NvBoardInfo;

/* Reads the information of specified board type from EEPROM
 *
 * @param BoardType Type of the board
 * @param pBoardInfo Structure which will be updated with info read from EEPROM
 *                   Should not be NULL
 *
 * @return NvSuccess if successful else NvError.
 */
NvError NvBlReadBoardInfo(NvBoardType BoardType, NvBoardInfo *pBoardInfo);

/* Initializes the pinmuxes needed for I2C1 and I2C5 */
void NvBlInitI2cPinmux(void);

/* Initializes the I2C IO Expander */
NvError NvBlInitIoExpander(void);

#endif
