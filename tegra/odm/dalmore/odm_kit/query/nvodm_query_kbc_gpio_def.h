/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         The KBC GPIO pin definitions</b>
 *
 * @b Description: Define the KBC GPIO pins in row and column numbers.
 */

#ifndef NVODM_QUERY_KBC_GPIO_DEF_H
#define NVODM_QUERY_KBC_GPIO_DEF_H

typedef enum
{
    NvOdmKbcGpioPin_KBRow0 = 0,
    NvOdmKbcGpioPin_KBRow1,
    NvOdmKbcGpioPin_KBRow2,
    NvOdmKbcGpioPin_KBRow3,
    NvOdmKbcGpioPin_KBRow4,
    NvOdmKbcGpioPin_KBRow5,
    NvOdmKbcGpioPin_KBRow6,
    NvOdmKbcGpioPin_KBRow7,
    NvOdmKbcGpioPin_KBRow8,
    NvOdmKbcGpioPin_KBRow9,
    NvOdmKbcGpioPin_KBRow10,
    NvOdmKbcGpioPin_KBCol0,
    NvOdmKbcGpioPin_KBCol1,
    NvOdmKbcGpioPin_KBCol2,
    NvOdmKbcGpioPin_KBCol3,
    NvOdmKbcGpioPin_KBCol4,
    NvOdmKbcGpioPin_KBCol5,
    NvOdmKbcGpioPin_KBCol6,
    NvOdmKbcGpioPin_KBCol7,
    NvOdmKbcGpioPin_Num,
    NvOdmKbcGpioPin_Force32 = 0x7FFFFFFF
}NvOdmKbcGpioPin;

#endif // NVODM_QUERY_KBC_GPIO_DEF_H
