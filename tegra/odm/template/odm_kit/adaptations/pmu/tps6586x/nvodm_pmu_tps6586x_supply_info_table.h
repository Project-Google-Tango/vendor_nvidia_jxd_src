/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TPS6586x_SUPPLY_INFO_TABLE_H_
#define TPS6586x_SUPPLY_INFO_TABLE_H_

#if defined(__cplusplus)
extern "C"
{
#endif

#define TPS_EXT_GUID(supply) \
    (NV_ODM_GUID('t','p','s','e','x','t',0,0) | ((supply) & 0xff))

typedef enum
{
    TPS6586xPmuSupply_Invalid = 0x0,

    //DCD0
    TPS6586xPmuSupply_DCD0,

    //DCD1
    TPS6586xPmuSupply_DCD1,

    //DCD2
    TPS6586xPmuSupply_DCD2,


    //LDO0
    TPS6586xPmuSupply_LDO0,

    //LDO1
    TPS6586xPmuSupply_LDO1,

    //LDO2
    TPS6586xPmuSupply_LDO2,

    //LDO3
    TPS6586xPmuSupply_LDO3,
    
    //LDO4
    TPS6586xPmuSupply_LDO4,
     
    //LDO5
    TPS6586xPmuSupply_LDO5,

    //LDO6
    TPS6586xPmuSupply_LDO6,

    //LDO7
    TPS6586xPmuSupply_LDO7,

    //LDO8
    TPS6586xPmuSupply_LDO8,

    //LDO9
    TPS6586xPmuSupply_LDO9,

    //RTC_OUT
    TPS6586xPmuSupply_RTC_OUT,

    //RED1
    TPS6586xPmuSupply_RED1,

    //GREEN1
    TPS6586xPmuSupply_GREEN1,

    //BLUE1
    TPS6586xPmuSupply_BLUE1,

    //RED2
    TPS6586xPmuSupply_RED2,

    //GREEN2
    TPS6586xPmuSupply_GREEN2,

    //BLUE2
    TPS6586xPmuSupply_BLUE2,

    //LED_PWM
    TPS6586xPmuSupply_LED_PWM,

    //PWM
    TPS6586xPmuSupply_PWM,

    //White LED(SW3)
    TPS6586xPmuSupply_WHITE_LED,

    //SOC
    TPS6586xPmuSupply_SoC,

    /*--- Secondary/External PMU Rails ---*/

    // PMU GPIO-3: VDD_1V05
    Ext_TPS62290PmuSupply_BUCK,

    // PMU GPIO-2: VDD_1V2
    Ext_TPS72012PmuSupply_LDO,

    // PMU GPIO-1: VDD_1V5
    Ext_TPS74201PmuSupply_LDO,

    // AP GPIO(T,2): VDDIO_HDMI, VDDIO_VGA (5V @ 500ma)
    Ext_TPS2051BPmuSupply_VDDIO_VID,

    // AP GPIO(T,3): VDDIO_SD
    Ext_SWITCHPmuSupply_VDDIO_SD,

    // AP GPIO(I,6): VDDIO_SDMMC
    Ext_SWITCHPmuSupply_VDDIO_SDMMC,

    // AP GPIO(W,0): VDD_BL
    // FIXME: This is already supplied by nvodm_query_gpio in the display GPIO settings.
    Ext_SWITCHPmuSupply_VDD_BL,

    // AP GPIO(C,6): VDD_PNL
    // FIXME: This is already supplied by nvodm_query_gpio in the display GPIO settings.
    Ext_SWITCHPmuSupply_VDD_PNL,

    TPS6586xPmuSupply_Num,

    TPS6586xPmuSupply_Force32 = 0x7FFFFFFF
} TPS6586xPmuSupply;

/// The total rail count
#define VRAILCOUNT  TPS6586xPmuSupply_Num

/// The total number of external supplies (which use both AP and PMU GPIOs)
#define TPS6586x_EXTERNAL_SUPPLY_NUM \
    (NvU32)(TPS6586xPmuSupply_Num - Ext_TPS62290PmuSupply_BUCK)

/// Macro for converting a vddRail to AP GPIO pin index.
#define NVODM_EXT_AP_GPIO_RAIL(x) ((x) - Ext_TPS2051BPmuSupply_VDDIO_VID)

/// The total number of external supplies which use AP GPIO pins for enable
#define TPS6586x_EXTERNAL_SUPPLY_AP_GPIO_NUM \
    (NvU32)NVODM_EXT_AP_GPIO_RAIL(TPS6586xPmuSupply_Num)

#if defined(__cplusplus)
}
#endif

#endif /* TPS6586x_SUPPLY_INFO_TABLE_H_ */
