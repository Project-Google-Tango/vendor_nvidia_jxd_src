/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database Peripheral
 *                 entries for the peripherals on m2601 module.
 */

//  AVdd_PEXB/A
{
    NV_VDD_PEX_ODM_ID,
    s_AVddPexBAddresses,
    NV_ARRAY_SIZE(s_AVddPexBAddresses),
    NvOdmPeripheralClass_Other
},

//  AVdd_PEXPLL
{
    NV_VDD_PLL_PEX_ODM_ID,
    s_AVddPexPllAddresses,
    NV_ARRAY_SIZE(s_AVddPexPllAddresses),
    NvOdmPeripheralClass_Other
},

// PLLD (NV reserved)  It is removed from Ap20, so moving to PLL_A_P_C_S
{
    NV_VDD_PLLD_ODM_ID,
    s_AVddPllAPCSAddresses,
    NV_ARRAY_SIZE(s_AVddPllAPCSAddresses),
    NvOdmPeripheralClass_Other
},

//SOC (NV reserved) It is removed on T30, so setting to VddIoSys
{
    NV_VDD_SoC_ODM_ID,
    s_VddIoSysAddresses,
    NV_ARRAY_SIZE(s_VddIoSysAddresses),
    NvOdmPeripheralClass_Other
},

// CORE (NV reserved)
{
    NV_VDD_CORE_ODM_ID,
    s_VddCoreAddresses,
    NV_ARRAY_SIZE(s_VddCoreAddresses),
    NvOdmPeripheralClass_Other
},

// SYS (NV reserved)
{
    NV_VDD_SYS_ODM_ID,
    s_VddIoSysAddresses,
    NV_ARRAY_SIZE(s_VddIoSysAddresses),
    NvOdmPeripheralClass_Other
},

// DDR (NV reserved)
{
    NV_VDD_DDR_ODM_ID,
    s_VddIoDdrAddresses,
    NV_ARRAY_SIZE(s_VddIoDdrAddresses),
    NvOdmPeripheralClass_Other
},

// DDR_RX (NV reserved)
{
    NV_VDD_DDR_RX_ODM_ID,
    s_VddDdrRxAddresses,
    NV_ARRAY_SIZE(s_VddDdrRxAddresses),
    NvOdmPeripheralClass_Other
},


// RTC (NV reserved) VDD_RTC is derived from VDD_CORE - Review Why from core
{
    NV_VDD_RTC_ODM_ID,
    s_VddCoreAddresses,
    NV_ARRAY_SIZE(s_VddCoreAddresses),
    NvOdmPeripheralClass_Other
},

// PLLA (NV reserved)
{
    NV_VDD_PLLA_ODM_ID,
    s_AVddPllAPCSAddresses,
    NV_ARRAY_SIZE(s_AVddPllAPCSAddresses),
    NvOdmPeripheralClass_Other
},

// PLLM (NV reserved)
{
    NV_VDD_PLLM_ODM_ID,
    s_AVddPllMAddresses,
    NV_ARRAY_SIZE(s_AVddPllMAddresses),
    NvOdmPeripheralClass_Other
},

// PLLP (NV reserved)
{
    NV_VDD_PLLP_ODM_ID,
    s_AVddPllAPCSAddresses,
    NV_ARRAY_SIZE(s_AVddPllAPCSAddresses),
    NvOdmPeripheralClass_Other
},

// PLLC (NV reserved)
{
    NV_VDD_PLLC_ODM_ID,
    s_AVddPllAPCSAddresses,
    NV_ARRAY_SIZE(s_AVddPllAPCSAddresses),
    NvOdmPeripheralClass_Other
},

// PLLE (NV reserved)
{
    NV_VDD_PLLE_ODM_ID,
    s_AVddPllEAddresses,
    NV_ARRAY_SIZE(s_AVddPllEAddresses),
    NvOdmPeripheralClass_Other
},

// OSC VDD (NV reserved)
{
    NV_VDD_OSC_ODM_ID,
    s_AVddOscAddresses,
    NV_ARRAY_SIZE(s_AVddOscAddresses),
    NvOdmPeripheralClass_Other
},

// PLLX (NV reserved)
{
    NV_VDD_PLLX_ODM_ID,
    s_AVddPllXAddresses,
    NV_ARRAY_SIZE(s_AVddPllXAddresses),
    NvOdmPeripheralClass_Other
},


//UART (NV reserved)
{
    NV_VDD_UART_ODM_ID,
    s_VddIoUartAddresses,
    NV_ARRAY_SIZE(s_VddIoUartAddresses),
    NvOdmPeripheralClass_Other
},

//  PMU0
{
    NV_ODM_GUID('t','p','s','6','5','9','1','a'),
    s_Pmu0Addresses,
    NV_ARRAY_SIZE(s_Pmu0Addresses),
    NvOdmPeripheralClass_Other
},

// test rail - update Nitin
{
    NV_ODM_GUID('T','E','S','T','R','A','I','L'),
    s_AllRailAddresses,
    NV_ARRAY_SIZE(s_AllRailAddresses),
    NvOdmPeripheralClass_Other,
},

// LCD Display
{
    NV_ODM_GUID('L','C','D',' ','W','V','G','A'),
    s_LcdDisplayAddresses,
    NV_ARRAY_SIZE(s_LcdDisplayAddresses),
    NvOdmPeripheralClass_Display
},

// HDMI
{
    NV_ODM_GUID('F','F','A',' ','H','D','M','I'),
    s_HdmiAddresses,
    NV_ARRAY_SIZE(s_HdmiAddresses),
    NvOdmPeripheralClass_Display
},

// NOTE: This list *must* end with a trailing comma.
