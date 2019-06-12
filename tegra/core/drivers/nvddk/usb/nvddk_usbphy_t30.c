/*
 * Copyright (c) 2009-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK USB PHY functions</b>
 *
 * @b Description: Defines USB PHY private functions
 *
 */

#include "nvrm_module.h"
#include "nvrm_drf.h"
#include "t30/arusb.h"
#include "t30/arclk_rst.h"
#include "nvrm_hardware_access.h"
#include "nvddk_usbphy_priv.h"
#include "nvodm_query.h"
#include "t30/arapbpm.h"

/* Defines for USB register read and writes */
#define USB_REG_RD(reg)\
    NV_READ32(pUsbPhy->UsbVirAdr + ((USB2_CONTROLLER_2_USB2D_##reg##_0)/4))

#define USB_REG_WR(reg, data)\
    NV_WRITE32(pUsbPhy->UsbVirAdr + ((USB2_CONTROLLER_2_USB2D_##reg##_0)/4), (data))

// Read perticular field value from reg mentioned
#define USB_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_2_USB2D, reg, field, value)

#define USB_FLD_SET_DRF_DEF(reg, field, define, value) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, define, value)

#define USB_REG_SET_DRF_DEF(reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, define, USB_REG_RD(reg))

#define USB_REG_UPDATE_DEF(reg, field, define) \
    USB_REG_WR(reg, USB_REG_SET_DRF_DEF(reg, field, define))

#define USB_DRF_DEF(reg, field, define) \
    NV_DRF_DEF(USB2_CONTROLLER_2_USB2D, reg, field, define)

#define USB_DRF_DEF_VAL(reg, field, define) \
    (USB2_CONTROLLER_USB2D##_##reg##_0_##field##_##define)

#define USB_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_2_USB2D, reg, field, value)

#define USB_REG_READ_VAL(reg, field) \
    USB_DRF_VAL(reg, field, USB_REG_RD(reg))

#define USB_REG_SET_DRF_NUM(reg, field, num, value) \
    NV_FLD_SET_DRF_NUM(USB2_CONTROLLER_2_USB2D, reg, field, num, value);

/* Defines for USB IF register read and writes */

#define USB1_IF_FLD_SET_DRF_DEF(reg, field, define, value) \
    NV_FLD_SET_DRF_DEF(USB1_IF, reg, field, define, value)

#define USB1_IF_REG_RD(reg)\
    NV_READ32(pUsbPhy->UsbVirAdr + ((USB1_IF_##reg##_0)/4))

#define USB1_IF_REG_WR(reg, data)\
    NV_WRITE32(pUsbPhy->UsbVirAdr + ((USB1_IF_##reg##_0)/4), (data))

#define USB1_IF_REG_SET_DRF_DEF(reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB1_IF, reg, field, define, USB1_IF_REG_RD(reg))

#define USB1_IF_REG_UPDATE_DEF(reg, field, define) \
    USB1_IF_REG_WR(reg, USB1_IF_REG_SET_DRF_DEF(reg, field, define))

#define USB1_IF_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB1_IF, reg, field, value)

#define USB1_IF_REG_READ_VAL(reg, field) \
    USB1_IF_DRF_VAL(reg, field, USB1_IF_REG_RD(reg))

#define USB3_IF_FLD_SET_DRF_DEF(reg, field, define, value) \
    NV_FLD_SET_DRF_DEF(USB3_IF_USB, reg, field, define, value)

#define USB_IF_DRF_DEF(reg, field, define) \
    NV_DRF_DEF(USB2_IF_USB, reg, field, define)

#define USB_IF_DRF_NUM(reg, field, value) \
    NV_DRF_NUM(USB2_IF_USB, reg, field, value)

#define USB_IF_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_IF_USB, reg, field, value)

#define USB_IF_REG_RD(reg)\
    NV_READ32(pUsbPhy->UsbVirAdr + ((USB2_IF_USB_##reg##_0)/4))

#define USB_IF_REG_WR(reg, data)\
    NV_WRITE32(pUsbPhy->UsbVirAdr + ((USB2_IF_USB_##reg##_0)/4), (data))

#define USB_IF_REG_READ_VAL(reg, field) \
    USB_IF_DRF_VAL(reg, field, USB_IF_REG_RD(reg))

#define USB_IF_REG_SET_DRF_NUM(reg, field, define) \
    NV_FLD_SET_DRF_NUM(USB2_IF_USB, reg, field, define, USB_IF_REG_RD(reg))

#define USB_IF_REG_SET_DRF_DEF(reg, field, define) \
    NV_FLD_SET_DRF_DEF(USB2_IF_USB, reg, field, define, USB_IF_REG_RD(reg))

#define USB_IF_REG_UPDATE_DEF(reg, field, define) \
    USB_IF_REG_WR(reg, USB_IF_REG_SET_DRF_DEF(reg, field, define))

#define USB_IF_REG_UPDATE_NUM(reg, field, define) \
    USB_IF_REG_WR(reg, USB_IF_REG_SET_DRF_NUM(reg, field, define))


// defines for ULPI register access
#define ULPI_IF_REG_RD(reg)\
    NV_READ32(pUsbPhy->UsbVirAdr + ((USB2_IF_ULPI_##reg##_0)/4))

#define ULPI_IF_REG_WR(reg, data)\
    NV_WRITE32(pUsbPhy->UsbVirAdr + ((USB2_IF_ULPI_##reg##_0)/4), (data))

#define ULPI_IF_DRF_DEF(reg, field, define) \
    NV_DRF_DEF(USB2_IF_ULPI, reg, field, define)

#define ULPI_IF_DRF_NUM(reg, field, define) \
    NV_DRF_NUM(USB2_IF_ULPI, reg, field, define)


/* Defines for USB IF register read and writes */

#define USB_UTMIP_FLD_SET_DRF_DEF(reg, field, define, value) \
    NV_FLD_SET_DRF_NUM(USB1_UTMIP, reg, field, define, value)

#define USB_UTMIP_REG_RD(reg)\
    NV_READ32(pUsbPhy->UsbVirAdr + ((USB1_UTMIP_##reg##_0)/4))

#define USB_UTMIP_REG_WR(reg, data)\
    NV_WRITE32(pUsbPhy->UsbVirAdr + ((USB1_UTMIP_##reg##_0)/4), (data))

#define USB_UTMIP_REG_SET_DRF_NUM(reg, field, num) \
    NV_FLD_SET_DRF_NUM(USB1_UTMIP, reg, field, num, USB_UTMIP_REG_RD(reg))

#define USB_UTMIP_REG_UPDATE_NUM(reg, field, num) \
    USB_UTMIP_REG_WR(reg, USB_UTMIP_REG_SET_DRF_NUM(reg, field, num))

#define UTMIP_PLL_RD(base, reg) \
    NV_READ32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0)

#define UTMIP_PLL_WR(base, reg, data) \
    NV_WRITE32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0, data)

#define UTMIP_PLL_SET_DRF_DEF(base, reg, field, def) \
    NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER_UTMIP, reg, field, def, UTMIP_PLL_RD(base, reg))

#define UTMIP_PLL_UPDATE_DEF(base, reg, field, define) \
    UTMIP_PLL_WR(base, reg, UTMIP_PLL_SET_DRF_DEF(base, reg, field, define))


/**
 * Structure defining the fields for USB UTMI clocks delay Parameters.
 */
typedef struct UsbPllDelayParamsRec
{
    // Pll-U Enable Delay Count
    NvU16 EnableDelayCount;
    //PLL-U Stable count
    NvU16 StableCount;
    //Pll-U Active delay count
    NvU16 ActiveDelayCount;
    //PLL-U Xtal frequency count
    NvU16 XtalFreqCount;
} UsbPllDelayParams;

/*
 * Set of oscillator frequencies supported
 */
typedef enum
{
    NvRmClocksOscFreq_13_MHz = 0x0,
    NvRmClocksOscFreq_16_8_MHz,
    NvRmClocksOscFreq_dummy_1,
    NvRmClocksOscFreq_dummy_2,
    NvRmClocksOscFreq_19_2_MHz,
    NvRmClocksOscFreq_38_4_MHz,
    NvRmClocksOscFreq_dummy_3,
    NvRmClocksOscFreq_dummy_4,
    NvRmClocksOscFreq_12_MHz,
    NvRmClocksOscFreq_48_MHz,
    NvRmClocksOscFreq_dummy_5,
    NvRmClocksOscFreq_dummy_6,
    NvRmClocksOscFreq_26_MHz,
    NvRmClocksOscFreq_dummy_7,
    NvRmClocksOscFreq_dummy_8,
    NvRmClocksOscFreq_dummy_9,
    NvRmClocksOscFreq_Num, // dummy to get number of frequencies
    NvRmClocksOscFreq_Force32 = 0x7fffffff
} NvRmClocksOscFreq;


static const NvU32 s_UsbBiasTrkLengthTime[NvRmClocksOscFreq_Num] =
{
    /* 20 micro seconds delay after bias cell operation */
    6,  // For NvBootClocksOscFreq_13,
    7,  //  For NvBootClocksOscFreq_16_8
    0,  // dummy field
    0,  // dummy field
    8,  // For NvBootClocksOscFreq_19_2
    0xF,  //For NvBootClocksOscFreq_38_4,
    0,  // dummy field
    0,  // dummy field
    5,  // For NvBootClocksOscFreq_12
    0x13,  // For NvBootClocksOscFreq_48_9,
    0,  // dummy field
    0,  // dummy field
    0XB  // For NvBootClocksOscFreq_26
};


// Possible Oscillator Frequecies in KHz for mapping the index
static NvRmFreqKHz s_RmOscFrequecy[NvRmClocksOscFreq_Num] =
{
    13000, // 13 Mega Hertz
    16800,
    0,
    0,
    19200, // 19.2 Mega Hertz
    38400,
    0,
    0,
    12000, // 12 Mega Hertz
    48000,
    0,
    0,
    26000 // 26 Mega Hertz
};

///////////////////////////////////////////////////////////////////////////////
// USB PLL CONFIGURATION & PARAMETERS: refer to the arapb_misc_utmip.spec file.
///////////////////////////////////////////////////////////////////////////////
// PLL CONFIGURATION & PARAMETERS for different clock generators:
//-----------------------------------------------------------------------------
// Reference frequency     13.0MHz    19.2MHz    12.0MHz    26.0MHz    16.8MHz
// ----------------------------------------------------------------------------
// PLLU_ENABLE_DLY_COUNT   02 (02h)   03 (03h)   02 (02h)   04 (04h)   03 (03h)
// PLLU_STABLE_COUNT       51 (33h)   75 (4Bh)   47 (2Fh)  102 (66h)   65 (41h)
// PLL_ACTIVE_DLY_COUNT    05 (05h)   06 (06h)   04 (04h)   09 (09h)   10 (0Ah)
// XTAL_FREQ_COUNT        127 (7Fh)  187 (BBh)  118 (76h)  254 (FEh)  164 (A4h)
///////////////////////////////////////////////////////////////////////////////
static const UsbPllDelayParams s_UsbPllDelayParams[NvRmClocksOscFreq_Num] =
{
    //ENABLE_DLY,  STABLE_CNT,  ACTIVE_DLY,  XTAL_FREQ_CNT
    {0x02,         0x33,        0x09,        0x7F}, // For NvRmClocksOscFreq_13_MHz,
    {0x03,         0x42,        0x0B,        0xA5},  // For NvRmClocksOscFreq_16_8_MHz
    {   0,            0,          0,          0}, //dummy field
    {   0,            0,          0,          0}, //dummy field
    {0x03,         0x4B,        0x0C,        0xBC}, // For NvRmClocksOscFreq_19_2_MHz
    {0x05,         0x96,        0x18,        0x177}, // For NvRmClocksOscFreq_38_4_Mhz
    {   0,            0,          0,          0}, //dummy field
    {   0,            0,          0,          0}, //dummy field
    {0x02,         0x2F,        0x08,        0x76}, // For NvRmClocksOscFreq_12_MHz
    {0x06,         0xBC,        0x1F,        0x1D5}, // For NvRmClocksOscFreq_48_MHz
    {   0,            0,          0,          0}, //dummy field
    {   0,            0,          0,          0}, //dummy field
    {0x04,         0x66,        0x11,        0xFE} // For NvRmClocksOscFreq_26_MHz
};

///////////////////////////////////////////////////////////////////////////////
// USB Debounce values IdDig, Avalid, Bvalid, VbusValid, VbusWakeUp, and SessEnd.
// Each of these signals have their own debouncer and for each of those one out
// of 2 debouncing times can be chosen (BIAS_DEBOUNCE_A or BIAS_DEBOUNCE_B.)
//
// The values of DEBOUNCE_A and DEBOUNCE_B are calculated as follows:
// 0xffff -> No debouncing at all
// <n> ms = <n> *1000 / (1/19.2MHz) / 4
// So to program a 1 ms debounce for BIAS_DEBOUNCE_A, we have:
// BIAS_DEBOUNCE_A[15:0] = 1000 * 19.2 / 4  = 4800 = 0x12c0
// We need to use only DebounceA, We dont need the DebounceB
// values, so we can keep those to default.
///////////////////////////////////////////////////////////////////////////////
static const NvU32 s_UsbBiasDebounceATime[NvRmClocksOscFreq_Num] =
{
    /* Ten milli second delay for BIAS_DEBOUNCE_A */
    0x7EF4,  // For NvRmClocksOscFreq_13_MHz,
    0xA410,   // For NvRmClocksOscFreq_16_8_MHz
    0,
    0,
    0xBB80,  // For NvRmClocksOscFreq_19_2_MHz
    0XBB80,  // fOR NvRmClocksOscFreq_38_4_Mhz
    0,
    0,
    0x7530,  // For NvRmClocksOscFreq_12_MHz
    0xEA60,  // For NvRmClocksOscFreq_48_MHz
    0,
    0,
    0xFDE8   // For NvRmClocksOscFreq_26_MHz
};

///////////////////////////////////////////////////////////////////////////////
// The following arapb_misc_utmip.spec fields need to be programmed to ensure
// correct operation of the UTMIP block:
// Production settings :
//        'HS_SYNC_START_DLY' : 9,
//        'IDLE_WAIT'         : 17,
//        'ELASTIC_LIMIT'     : 16,
// All other fields can use the default reset values.
// Setting the fields above, together with default values of the other fields,
// results in programming the registers below as follows:
//         UTMIP_HSRX_CFG0 = 0x9168c000
//         UTMIP_HSRX_CFG1 = 0x13
///////////////////////////////////////////////////////////////////////////////
//UTMIP Idle Wait Delay
static const NvU8 s_UtmipIdleWaitDelay    = 17;
//UTMIP Elastic limit
static const NvU8 s_UtmipElasticLimit     = 16;
//UTMIP High Speed Sync Start Delay
static const NvU8 s_UtmipHsSyncStartDelay = 9;


static NvError
T30UsbPhyUtmiConfigure(
        NvDdkUsbPhy *pUsbPhy)
{
    NvU32 RegVal = 0;
    NvRmFreqKHz OscFreqKz = 0;
    NvU32 FreqIndex;
    NvRmPhysAddr PmcBaseAddr = 0; // PMC Base Address
    NvRmPhysAddr CarBaseAddr = 0; // Clock and Reset Base Address

    // Get Base address for APB PMC
    NvRmModuleGetBaseAddress(
                    pUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Misc, 0),
                    &PmcBaseAddr,
                    NULL);

    // Get Base Address for Clock & Reset module
    NvRmModuleGetBaseAddress(
                    pUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
                    &CarBaseAddr,
                    NULL);

    // Get the Oscillator Frequency
    OscFreqKz = NvRmPowerGetPrimaryFrequency(pUsbPhy->hRmDevice);

    // Get the Oscillator Frequency Index
    for (FreqIndex = 0; FreqIndex < NvRmClocksOscFreq_Num; FreqIndex++)
    {
        if (OscFreqKz == s_RmOscFrequecy[FreqIndex])
        {
            // Bail Out if frequecy matches with the supported frequency
            break;
        }
    }
    // If Index is equal to the maximum supported frequency count
    // There is a mismatch of the frequecy, so returning since the
    // frequency is not supported.
    if (FreqIndex >= NvRmClocksOscFreq_Num)
    {
        return NvError_NotSupported;
    }

    // Hold UTMIP in reset
    RegVal =USB_IF_REG_RD(SUSP_CTRL);
    RegVal = USB3_IF_FLD_SET_DRF_DEF(SUSP_CTRL, UTMIP_RESET, ENABLE, RegVal);
    USB_IF_REG_WR(SUSP_CTRL, RegVal);

    // Stop crystal clock by setting UTMIP_PHY_XTAL_CLOCKEN low
    RegVal = USB_UTMIP_REG_RD(MISC_CFG1);
    RegVal = USB_UTMIP_FLD_SET_DRF_DEF(MISC_CFG1,UTMIP_PHY_XTAL_CLOCKEN,
            0, RegVal);
    USB_UTMIP_REG_WR(MISC_CFG1, RegVal);


    UTMIP_PLL_UPDATE_DEF(CarBaseAddr,
            PLL_CFG2,
            UTMIP_PHY_XTAL_CLOCKEN,
            SW_DEFAULT);

    // Follow the crystal clock disable by >100ns delay.
    NvOsWaitUS(1);

    RegVal = UTMIP_PLL_RD(CarBaseAddr, PLL_CFG2);

    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
            PLL_CFG2,
            UTMIP_PLLU_STABLE_COUNT,
            s_UsbPllDelayParams[FreqIndex].StableCount,
            RegVal);

    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG2,
            UTMIP_PLL_ACTIVE_DLY_COUNT,
            s_UsbPllDelayParams[FreqIndex].ActiveDelayCount,
            RegVal);

    UTMIP_PLL_WR(CarBaseAddr, PLL_CFG2, RegVal);

    RegVal = UTMIP_PLL_RD(CarBaseAddr, PLL_CFG1);
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
            UTMIP_PLLU_ENABLE_DLY_COUNT, s_UsbPllDelayParams[FreqIndex].EnableDelayCount, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
            UTMIP_XTAL_FREQ_COUNT, s_UsbPllDelayParams[FreqIndex].XtalFreqCount, RegVal);

    // Remove power down for PLLU_ENABLE
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
            UTMIP_FORCE_PLL_ENABLE_POWERDOWN, 0x0, RegVal);
    // Remove power down for PLL_ACTIVE
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
            UTMIP_FORCE_PLL_ACTIVE_POWERDOWN, 0x0, RegVal);
    // Remove power down for PLLU
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
            UTMIP_FORCE_PLLU_POWERDOWN, 0x0, RegVal);
    UTMIP_PLL_WR(CarBaseAddr, PLL_CFG1, RegVal);

    /* Power-up pads for UTMIP PHY */
    if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_Utmi)
    {
        RegVal = NV_READ32(PmcBaseAddr + APBDEV_PMC_USB_AO_0);

        switch (pUsbPhy->Instance)
        {
            case 0:
                // Remove power down on VBUS WAKEUP PD
                RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, USB_AO,
                        VBUS_WAKEUP_PD_P0, 0x0, RegVal);

                // Remove power down on ID PD PD
                RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, USB_AO,
                        ID_PD_P0, 0x0, RegVal);
                break;
            case 1:
                // Remove power down on VBUS WAKEUP PD
                RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, USB_AO,
                        VBUS_WAKEUP_PD_P1, 0x0, RegVal);

                // Remove power down on ID PD PD
                RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, USB_AO,
                        ID_PD_P1, 0x0, RegVal);
                break;
            case 2:
                // Remove power down on VBUS WAKEUP PD
                RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, USB_AO,
                        VBUS_WAKEUP_PD_P2, 0x0, RegVal);

                // Remove power down on ID PD PD
                RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, USB_AO,
                        ID_PD_P2, 0x0, RegVal);
                break;
            default:
                break;
        }

        NV_WRITE32(PmcBaseAddr + APBDEV_PMC_USB_AO_0, RegVal);
        RegVal = NV_READ32(PmcBaseAddr + APBDEV_PMC_USB_AO_0);
    }

    USB_UTMIP_REG_UPDATE_NUM(BIAS_CFG1, UTMIP_BIAS_PDTRK_COUNT, s_UsbBiasTrkLengthTime[FreqIndex]);

    NvOsWaitUS(25);

    // Program 1ms Debounce time for VBUS to become valid.
    USB_UTMIP_REG_UPDATE_NUM(
            DEBOUNCE_CFG0, UTMIP_BIAS_DEBOUNCE_A, s_UsbBiasDebounceATime[FreqIndex]);

    USB_UTMIP_REG_UPDATE_NUM(TX_CFG0, UTMIP_FS_PREAMBLE_J, 0x1);

    // Configure the UTMIP_IDLE_WAIT and UTMIP_ELASTIC_LIMIT
    // Setting these fields, together with default values of the other
    // fields, results in programming the registers below as follows:
    //         UTMIP_HSRX_CFG0 = 0x9168c000
    //         UTMIP_HSRX_CFG1 = 0x13
    RegVal = USB_UTMIP_REG_RD(HSRX_CFG0);
    RegVal = USB_UTMIP_FLD_SET_DRF_DEF(HSRX_CFG0,
            UTMIP_IDLE_WAIT,s_UtmipIdleWaitDelay, RegVal);
    RegVal = USB_UTMIP_FLD_SET_DRF_DEF(HSRX_CFG0,
            UTMIP_ELASTIC_LIMIT,s_UtmipElasticLimit, RegVal);
    USB_UTMIP_REG_WR(HSRX_CFG0, RegVal);

    // Configure the UTMIP_HS_SYNC_START_DLY
    USB_UTMIP_REG_UPDATE_NUM(
            HSRX_CFG1, UTMIP_HS_SYNC_START_DLY, s_UtmipHsSyncStartDelay);

    NvOsWaitUS(1);

    RegVal = USB_UTMIP_REG_RD(MISC_CFG1);
    RegVal = USB_UTMIP_FLD_SET_DRF_DEF(MISC_CFG1,UTMIP_PHY_XTAL_CLOCKEN,
            1, RegVal);
    USB_UTMIP_REG_WR(MISC_CFG1, RegVal);


    UTMIP_PLL_UPDATE_DEF(CarBaseAddr,
            PLL_CFG2,
            UTMIP_PHY_XTAL_CLOCKEN,
            DEFAULT);

    RegVal = UTMIP_PLL_RD(CarBaseAddr, PLL_CFG2);

    switch (pUsbPhy->Instance)
    {
        case 0:
            RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                    UTMIP_PLL_CFG2,
                    UTMIP_FORCE_PD_SAMP_A_POWERDOWN,
                    0,
                    RegVal);
            break;
        case 1:
            RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                    UTMIP_PLL_CFG2,
                    UTMIP_FORCE_PD_SAMP_B_POWERDOWN,
                    0,
                    RegVal);
            break;
        case 2:
            RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
                    UTMIP_PLL_CFG2,
                    UTMIP_FORCE_PD_SAMP_C_POWERDOWN,
                    0,
                    RegVal);
            break;
        default:
            break;
    }

    UTMIP_PLL_WR(CarBaseAddr, PLL_CFG2, RegVal);

    // Follow the crystal clock disable by >100ns delay.
    NvOsWaitUS(1);

    // PLL Delay CONFIGURATION settings
    // The following parameters control the bring up of the plls:
    USB_UTMIP_REG_UPDATE_NUM(MISC_CFG0, UTMIP_SUSPEND_EXIT_ON_EDGE, 0);

    return NvSuccess;
}

static void
T30UsbPhyUtmiPowerControl(
        NvDdkUsbPhy *pUsbPhy,
        NvBool Enable)
{
    NvU32 RegVal = 0;
    NvU32 XcvrSetupValue = 0x5;
    NvRmPhysAddr PhyAddr = 0; // USB3 OTG Module Base Address
    NvRmPhysAddr CarBaseAddr = 0; // Clock & Reset Base address

    // Get Base Address for Clock & Reset module
    NvRmModuleGetBaseAddress(
                    pUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
                    &CarBaseAddr,
                    NULL);

    if (Enable)
    {
        if (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Device) {
            // Disable the automatic phy enable on wakeup event
            USB1_IF_REG_UPDATE_DEF(USB_SUSP_CTRL, USB_WAKE_ON_DISCON_EN_DEV, DISABLE);
            USB1_IF_REG_UPDATE_DEF(USB_SUSP_CTRL, USB_WAKE_ON_CNNT_EN_DEV, DISABLE);
        }

        if (pUsbPhy->Instance != 0)
        {

            NvRmModuleGetBaseAddress(
                    pUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, 0),
                    &PhyAddr,
                    NULL);


            RegVal =     NV_READ32(CarBaseAddr + CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);

            RegVal = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_USBD, ENABLE, RegVal);

            NV_WRITE32(CarBaseAddr + CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0, RegVal);

            NvOsWaitUS(2);

            RegVal =     NV_READ32(CarBaseAddr + CLK_RST_CONTROLLER_RST_DEVICES_L_0);

            RegVal = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_USBD_RST, DISABLE, RegVal);

            NV_WRITE32(CarBaseAddr + CLK_RST_CONTROLLER_RST_DEVICES_L_0, RegVal);

            NvOsWaitUS(2);

            RegVal = NV_READ32(PhyAddr + USB1_UTMIP_BIAS_CFG0_0);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_BIASPD, 0, RegVal);

            NV_WRITE32((PhyAddr + USB1_UTMIP_BIAS_CFG0_0), RegVal);
        }

        // USB Power Up sequence
        if (!pUsbPhy->pUtmiPadConfig->PadOnRefCount)
        {
            /* UTMI PAD control logic is common to all UTMIP phys and
               they are controlled from USB1 controller */
            // Power Up OTG and Bias config pad circuitry
            RegVal = NV_READ32(pUsbPhy->pUtmiPadConfig->pVirAdr + ((USB1_UTMIP_BIAS_CFG0_0)/4));
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_OTGPD, 0, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_BIASPD, 0, RegVal);

            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_IDPD_SEL, 0, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_IDPD_VAL, 0, RegVal);

            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_HSSQUELCH_LEVEL, 2, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_HSDISCON_LEVEL, 1, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_HSDISCON_LEVEL_MSB, 1, RegVal);
            NV_WRITE32(pUsbPhy->pUtmiPadConfig->pVirAdr + ((USB1_UTMIP_BIAS_CFG0_0)/4),
                    RegVal);
            NvOsWaitUS(1);

            /* [TODO]Need to remove power down from ID (IDPD_SEL and
               IDPD_VAL). These bits have moved to PMC register space and need
               to be programmed since in T30 by default power down is ON
               (on AP20 it was OFF and hence was not required).
               [TODO] Check if same programming is required for
               VBUS_WAKEUP_POWERDOWN as it has also moved to PMC reg space.
               w/o above it might not work on silicon.
               Refer arapbpmc.h
             */
        }
        /* Increment the reference count to turn off the UTMIP pads */
        pUsbPhy->pUtmiPadConfig->PadOnRefCount++;

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG0, UTMIP_FORCE_PD_POWERDOWN, 0);
        NvOsWaitUS(1);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG0, UTMIP_FORCE_PD2_POWERDOWN, 0);
        NvOsWaitUS(1);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG0, UTMIP_FORCE_PDZI_POWERDOWN, 0);
        NvOsWaitUS(1);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG0, UTMIP_XCVR_LSBIAS_SEL, 0);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG0, UTMIP_XCVR_SETUP, XcvrSetupValue);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG0, UTMIP_XCVR_SETUP_MSB, 3);

        if (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)
        {
            RegVal = USB_UTMIP_REG_RD(XCVR_CFG0);
            // To slow rise/ fall times in low-speed eye diagrams in host mode
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                    XCVR_CFG0, UTMIP_XCVR_LSFSLEW, 2, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                    XCVR_CFG0, UTMIP_XCVR_LSRSLEW, 2, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                    XCVR_CFG0, UTMIP_XCVR_HSSLEW, 2, RegVal);

            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                    XCVR_CFG0, UTMIP_XCVR_HSSLEW_MSB, 7, RegVal);

            USB_UTMIP_REG_WR(XCVR_CFG0, RegVal);
        }

        // Enables the PHY calibration values to read from the fuses.
        //RegVal = USB_UTMIP_REG_RD(SPARE_CFG0);
        //RegVal |= 0x18;
        //USB_UTMIP_REG_WR(SPARE_CFG0, RegVal);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG1, UTMIP_FORCE_PDCHRP_POWERDOWN, 0x0);
        NvOsWaitUS(1);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG1, UTMIP_FORCE_PDDR_POWERDOWN, 0x0);
        NvOsWaitUS(1);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG1, UTMIP_FORCE_PDDISC_POWERDOWN, 0x0);

        USB_UTMIP_REG_UPDATE_NUM(XCVR_CFG1, UTMIP_XCVR_TERM_RANGE_ADJ, 7);

        // Program UTMIP_FORCE_PDTRK_POWERDOWN
        RegVal = USB_UTMIP_REG_RD(BIAS_CFG1);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                BIAS_CFG1, UTMIP_FORCE_PDTRK_POWERDOWN, 0, RegVal);

        USB_UTMIP_REG_WR(BIAS_CFG1, RegVal);

        // Enable Batery charge enabling bit, set to '0' for enable
        USB_UTMIP_REG_UPDATE_NUM(BAT_CHRG_CFG0, UTMIP_PD_CHRG, 0);

        if (pUsbPhy->Instance == 1)
        {
            // Hold UHSIC in reset
            USB_IF_REG_UPDATE_DEF(SUSP_CTRL, UHSIC_RESET, ENABLE);
        }

        // Enable UTMIP PHY
        RegVal = USB_IF_REG_RD(SUSP_CTRL);
        RegVal = USB3_IF_FLD_SET_DRF_DEF(
                SUSP_CTRL, UTMIP_PHY_ENB, ENABLE, RegVal);
        USB_IF_REG_WR(SUSP_CTRL, RegVal);

        // Release reset to UTMIP
        RegVal =USB_IF_REG_RD(SUSP_CTRL);
        RegVal = USB3_IF_FLD_SET_DRF_DEF(SUSP_CTRL, UTMIP_RESET, DISABLE, RegVal);
        USB_IF_REG_WR(SUSP_CTRL, RegVal);
    }
    else
    {
        // Put the Phy in the suspend mode
        // Suspend port before setting PHCD bit
        RegVal = USB_REG_RD(HOSTPC1_DEVLC);
        RegVal = USB_FLD_SET_DRF_DEF(HOSTPC1_DEVLC, PHCD, ENABLE, RegVal);
        USB_REG_WR(HOSTPC1_DEVLC, RegVal);

        // Setup debounce time for wakeup event 5 HCLK cycles
        USB_IF_REG_UPDATE_NUM(SUSP_CTRL, USB_WAKEUP_DEBOUNCE_COUNT, 5);
        // ENABLE the automatic phy enable on wakeup event
        USB1_IF_REG_UPDATE_DEF(USB_SUSP_CTRL, USB_WAKE_ON_CNNT_EN_DEV, ENABLE);
        USB1_IF_REG_UPDATE_DEF(USB_SUSP_CTRL, USB_WAKE_ON_DISCON_EN_DEV, ENABLE);

        // USB Power down sequence
        /* decrement the pad control refernce count */
        pUsbPhy->pUtmiPadConfig->PadOnRefCount--;
        if (!pUsbPhy->pUtmiPadConfig->PadOnRefCount)
        {
            /* since there is no reference to the pads turn off */
            RegVal = NV_READ32(pUsbPhy->pUtmiPadConfig->pVirAdr + ((USB1_UTMIP_BIAS_CFG0_0)/4));
            // Power down OTG and Bias circuitry
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_OTGPD, 1, RegVal);
            RegVal = USB_UTMIP_FLD_SET_DRF_DEF(BIAS_CFG0, UTMIP_BIASPD, 1, RegVal);
            NV_WRITE32(pUsbPhy->pUtmiPadConfig->pVirAdr + ((USB1_UTMIP_BIAS_CFG0_0)/4),
                    RegVal);
        }
        // Disable Batery charge enabling bit set to '1' for disable
        USB_UTMIP_REG_UPDATE_NUM(BAT_CHRG_CFG0, UTMIP_PD_CHRG, 1);

        // Turn off power in the tranciver
        RegVal = USB_UTMIP_REG_RD(XCVR_CFG0);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                XCVR_CFG0, UTMIP_FORCE_PDZI_POWERDOWN, 1, RegVal);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                XCVR_CFG0, UTMIP_FORCE_PD2_POWERDOWN, 1, RegVal);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                XCVR_CFG0, UTMIP_FORCE_PD_POWERDOWN, 1, RegVal);
        USB_UTMIP_REG_WR(XCVR_CFG0, RegVal);

        RegVal = USB_UTMIP_REG_RD(XCVR_CFG1);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                XCVR_CFG1, UTMIP_FORCE_PDDISC_POWERDOWN, 1, RegVal);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                XCVR_CFG1, UTMIP_FORCE_PDCHRP_POWERDOWN, 1, RegVal);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                XCVR_CFG1, UTMIP_FORCE_PDDR_POWERDOWN, 1, RegVal);
        USB_UTMIP_REG_WR(XCVR_CFG1, RegVal);

        // Hold UTMIP in reset
        RegVal =USB_IF_REG_RD(SUSP_CTRL);
        RegVal = USB3_IF_FLD_SET_DRF_DEF(SUSP_CTRL, UTMIP_RESET, ENABLE, RegVal);
        USB_IF_REG_WR(SUSP_CTRL, RegVal);

        // Program UTMIP_FORCE_PDTRK_POWERDOWN
        RegVal = USB_UTMIP_REG_RD(BIAS_CFG1);
        RegVal = USB_UTMIP_FLD_SET_DRF_DEF(
                BIAS_CFG1, UTMIP_FORCE_PDTRK_POWERDOWN, 1, RegVal);
        USB_UTMIP_REG_WR(BIAS_CFG1, RegVal);
    }
}

static void
T30UsbPhySelectUsbMode(
        NvDdkUsbPhy *pUsbPhy)
{
    NvU32 RegVal = 0;
    NvU32 TimeOut = USB_PHY_HW_TIMEOUT_US;

    if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_Utmi)
    {
        if (pUsbPhy->Instance == 2)
        {
            // Disable ICUSB interface
            USB_REG_UPDATE_DEF(ICUSB_CTRL, IC_ENB1, DISABLE);
        }
        // set the controller to Host controller mode
        USB_REG_UPDATE_DEF(USBMODE, CM, HOST_MODE);
        do
        {
            // wait till mode change is finished.
            RegVal = USB_REG_READ_VAL(USBMODE, CM);
            if (!TimeOut)
            {
                return;
            }
            NvOsWaitUS(1);
            TimeOut--;
        } while (RegVal != USB_DRF_DEF_VAL(USBMODE, CM, HOST_MODE));
        RegVal = USB_REG_RD(HOSTPC1_DEVLC);
        RegVal = USB_FLD_SET_DRF_DEF(HOSTPC1_DEVLC, PTS, UTMI, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(HOSTPC1_DEVLC, STS, PARALLEL_IF, RegVal);
        USB_REG_WR(HOSTPC1_DEVLC, RegVal);
        RegVal= USB_REG_READ_VAL(USBMODE, CM);
        NV_DEBUG_PRINTF(("USB Mode set to %x\n",RegVal));
    }
}

static void
T30UsbPhyUlpiNullModeConfigure(
        NvDdkUsbPhy *pUsbPhy)
{
    // default trimmer values
    NvOdmUsbTrimmerCtrl trimmerCtrl = {0, 0, 4, 4};

    if (pUsbPhy->pProperty->TrimmerCtrl.UlpiShadowClkDelay ||
            pUsbPhy->pProperty->TrimmerCtrl.UlpiClockOutDelay ||
            pUsbPhy->pProperty->TrimmerCtrl.UlpiDataTrimmerSel ||
            pUsbPhy->pProperty->TrimmerCtrl.UlpiStpDirNxtTrimmerSel)
    {
        // update the trimmer values if they are specified in nvodm_query
        NvOsMemcpy(&trimmerCtrl, &pUsbPhy->pProperty->TrimmerCtrl, sizeof(NvOdmUsbTrimmerCtrl));
    }

    // Put the UHSIC in the reset
    USB_IF_REG_UPDATE_DEF(SUSP_CTRL, UHSIC_RESET, ENABLE);

    // Put the UTMIP in reset
    USB_IF_REG_UPDATE_DEF(SUSP_CTRL, UTMIP_RESET, ENABLE);

    if (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)
    {
        USB_REG_UPDATE_DEF(USBMODE, CM, HOST_MODE);
    }
    else
    {
        USB_REG_UPDATE_DEF(USBMODE, CM, DEVICE_MODE);
    }

    // Change controller to use ULPI PHY
    USB_REG_UPDATE_DEF(HOSTPC1_DEVLC, PTS, ULPI);

    // Bypass the Pin Mux on the ULPI outputs and ULPI clock output enable
    ULPI_IF_REG_WR(TIMING_CTRL_0,
            ULPI_IF_REG_RD(TIMING_CTRL_0) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_OUTPUT_PINMUX_BYP, ENABLE) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_CLKOUT_PINMUX_BYP, ENABLE));

    // Enable ULPI clock from clk_gen
    USB_IF_REG_UPDATE_DEF(SUSP_CTRL, ULPI_PHY_ENB, ENABLE);

    // Set the timming perameters
    ULPI_IF_REG_WR(TIMING_CTRL_0,
            ULPI_IF_REG_RD(TIMING_CTRL_0) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_SHADOW_CLK_LOOPBACK_EN, ENABLE) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_SHADOW_CLK_SEL, POST_PAD) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_OUTPUT_PINMUX_BYP, ENABLE) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_CLKOUT_PINMUX_BYP, ENABLE) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_LBK_PAD_EN, OUTPUT) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_0, ULPI_SHADOW_CLK_DELAY, trimmerCtrl.UlpiShadowClkDelay) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_0, ULPI_CLOCK_OUT_DELAY, trimmerCtrl.UlpiClockOutDelay) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_0, ULPI_LBK_PAD_E_INPUT_OR, 0));

    // Set all the trimmers to 0 at the start
    ULPI_IF_REG_WR(TIMING_CTRL_1, 0);

    // wait for 10 micro seconds
    NvOsWaitUS(10);

    // Set USB2 controller to null ulpi mode (ULPIS2S_ENA = 1)
    // Enable PLLU (ULPIS2S_PLLU_MASTER_BLASTER60 = 1)
    //  ULPIS2S_SPARE[0] = VBUS active
    USB_IF_REG_WR(ULPIS2S_CTRL,
            USB_IF_DRF_DEF(ULPIS2S_CTRL, ULPIS2S_ENA, ENABLE) |
            USB_IF_DRF_DEF(ULPIS2S_CTRL, ULPIS2S_PLLU_MASTER_BLASTER60, ENABLE) |
            USB_IF_DRF_NUM(ULPIS2S_CTRL, ULPIS2S_SPARE,
                (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)?3:1));

    // Select ULPI_CORE_CLK_SEL to SHADOW_CLK
    ULPI_IF_REG_WR(TIMING_CTRL_0,
            ULPI_IF_REG_RD(TIMING_CTRL_0) |
            ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_CORE_CLK_SEL, SHADOW_CLK));

    // wait for 10 micro seconds
    NvOsWaitUS(10);

    // Set ULPI_CLK_OUT_ENA to 1 to enable ULPI null clocks
    // Can't set the trimmers before this
    ULPI_IF_REG_WR(TIMING_CTRL_0,
            ULPI_IF_REG_RD(TIMING_CTRL_0) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_0, ULPI_CLK_OUT_ENA, 1));

    // wait for 10 micro seconds
    NvOsWaitUS(10);

    // Set the trimmer values
    ULPI_IF_REG_WR(TIMING_CTRL_1,
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_LOAD, 0) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_SEL, trimmerCtrl.UlpiDataTrimmerSel) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_LOAD, 0) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_SEL, trimmerCtrl.UlpiStpDirNxtTrimmerSel) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_LOAD, 0) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_SEL, 4));

    // wait for 10 micro seconds
    NvOsWaitUS(10);

    //Load the trimmers by toggling the load bits
    ULPI_IF_REG_WR(TIMING_CTRL_1,
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_LOAD, 1) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_SEL, trimmerCtrl.UlpiDataTrimmerSel) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_LOAD, 1) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_SEL, trimmerCtrl.UlpiStpDirNxtTrimmerSel) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_LOAD, 1) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_SEL, 4));

    // wait for 10 micro seconds
    NvOsWaitUS(10);

    // Set ULPI_CLK_PADOUT_ENA  to 1 to enable ULPI null clock going out to
    // ulpi clk pad (ulpi_clk)
    ULPI_IF_REG_WR(TIMING_CTRL_0,
            ULPI_IF_REG_RD(TIMING_CTRL_0) |
            ULPI_IF_DRF_NUM(TIMING_CTRL_0, ULPI_CLK_PADOUT_ENA, 1));
}


static void
T30UsbPhyUlpiLinkModeConfigure(
        NvDdkUsbPhy *pUsbPhy, NvBool Enable)
{
    NvU32 RegVal;
    if (Enable)
    {
        // Put the UHSIC in the reset
        USB_IF_REG_UPDATE_DEF(SUSP_CTRL, UHSIC_RESET, ENABLE);

        // Put the UTMIP in reset
        USB_IF_REG_UPDATE_DEF(SUSP_CTRL, UTMIP_RESET, ENABLE);

        if (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)
        {
            USB_REG_UPDATE_DEF(USBMODE, CM, HOST_MODE);
        }
        else
        {
            USB_REG_UPDATE_DEF(USBMODE, CM, DEVICE_MODE);
        }

        // Change controller to use ULPI PHY
        USB_REG_UPDATE_DEF(HOSTPC1_DEVLC, PTS, ULPI);

        // Bypass the Pin Mux on the ULPI outputs and ULPI clock output enable
        ULPI_IF_REG_WR(TIMING_CTRL_0,
                ULPI_IF_REG_RD(TIMING_CTRL_0) |
                ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_OUTPUT_PINMUX_BYP, ENABLE) |
                ULPI_IF_DRF_DEF(TIMING_CTRL_0, ULPI_CLKOUT_PINMUX_BYP, ENABLE));

        // Enable ULPI clock from clk_gen
        USB_IF_REG_UPDATE_DEF(SUSP_CTRL, ULPI_PHY_ENB, ENABLE);

        // Set all the trimmers to 0 at the start
        ULPI_IF_REG_WR(TIMING_CTRL_1, 0);

        // Set the trimmer values
        ULPI_IF_REG_WR(TIMING_CTRL_1,
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_LOAD, 0) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_SEL, 4) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_LOAD, 0) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_SEL, 4) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_LOAD, 0) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_SEL, 4));

        // wait for 10 micro seconds
        NvOsWaitUS(10);

        //Load the trimmers by toggling the load bits
        ULPI_IF_REG_WR(TIMING_CTRL_1,
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_LOAD, 1) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DATA_TRIMMER_SEL, 4) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_LOAD, 1) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_STPDIRNXT_TRIMMER_SEL, 4) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_LOAD, 1) |
                ULPI_IF_DRF_NUM(TIMING_CTRL_1, ULPI_DIR_TRIMMER_SEL, 4));


        // fix VbusValid for Harmony due to floating VBUS
        RegVal = USB_REG_RD(ULPI_VIEWPORT);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_WAKEUP, CLEAR, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RUN, SET, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RD_WR, WRITE, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_PORT, SW_DEFAULT, RegVal);

        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_REG_ADDR, 0x8, RegVal);
        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_DATA_WR, 0x40, RegVal);

        USB_REG_WR(ULPI_VIEWPORT, RegVal);


        // wait for run bit to be cleared
        do
        {
            RegVal = USB_REG_RD(ULPI_VIEWPORT);
        } while (USB_DRF_VAL(ULPI_VIEWPORT, ULPI_RUN, RegVal));



        // set UseExternalVbusIndicator to 1
        RegVal = USB_REG_RD(ULPI_VIEWPORT);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_WAKEUP, CLEAR, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RUN, SET, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RD_WR, WRITE, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_PORT, SW_DEFAULT, RegVal);

        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_REG_ADDR, 0xB, RegVal);
        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_DATA_WR, 0x80, RegVal);

        USB_REG_WR(ULPI_VIEWPORT, RegVal);


        // wait for run bit to be cleared
        do
        {
            RegVal = USB_REG_RD(ULPI_VIEWPORT);
        } while (USB_DRF_VAL(ULPI_VIEWPORT, ULPI_RUN, RegVal));
    }
    else
    {
        // Resetting  the ULPI register IndicatorPassThru
        RegVal = USB_REG_RD(ULPI_VIEWPORT);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_WAKEUP, CLEAR, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RUN, SET, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RD_WR, WRITE, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_PORT, SW_DEFAULT, RegVal);

        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_REG_ADDR, 0x9, RegVal);
        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_DATA_WR, 0x40, RegVal);
        USB_REG_WR(ULPI_VIEWPORT, RegVal);

        // wait for run bit to be cleared
        do
        {
            RegVal = USB_REG_RD(ULPI_VIEWPORT);
        } while (USB_DRF_VAL(ULPI_VIEWPORT, ULPI_RUN, RegVal));

        // Resetting ULPI register UseExternalVbusIndicator
        RegVal = USB_REG_RD(ULPI_VIEWPORT);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_WAKEUP, CLEAR, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RUN, SET, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_RD_WR, WRITE, RegVal);
        RegVal = USB_FLD_SET_DRF_DEF(ULPI_VIEWPORT, ULPI_PORT, SW_DEFAULT, RegVal);
        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_REG_ADDR, 0xC, RegVal);
        RegVal = USB_REG_SET_DRF_NUM(ULPI_VIEWPORT, ULPI_DATA_WR, 0x80, RegVal);

        USB_REG_WR(ULPI_VIEWPORT, RegVal);

        // wait for run bit to be cleared
        do
        {
            RegVal = USB_REG_RD(ULPI_VIEWPORT);
        } while (USB_DRF_VAL(ULPI_VIEWPORT, ULPI_RUN, RegVal));
    }
}

static void
T30UsbPhyUlpiPowerControl(
        NvDdkUsbPhy *pUsbPhy,
        NvBool Enable)
{
    NvU32 RegVal = 0;

    if (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)
    {
        if (Enable)
        {
            // Bring the Phy out of suspend mode
            USB_IF_REG_UPDATE_DEF(SUSP_CTRL, USB_SUSP_CLR, SET);
            NvOsWaitUS(100);
            USB_IF_REG_UPDATE_DEF(SUSP_CTRL, USB_SUSP_CLR, UNSET);
        }
        else
        {
            // Put the Phy in the suspend mode
            RegVal = USB_REG_RD(HOSTPC1_DEVLC);
            RegVal = USB_FLD_SET_DRF_DEF(HOSTPC1_DEVLC, PHCD, ENABLE, RegVal);
            USB_REG_WR(HOSTPC1_DEVLC, RegVal);
        }
    }
}


static NvError
T30UsbPhyIoctlVbusInterrupt(
        NvDdkUsbPhy *pUsbPhy,
        const void *pInputArgs)
{
    NvDdkUsbPhyIoctl_VBusInterruptInputArgs *pVbusIntr = NULL;
    NvU32 RegVal = 0;

    if (!pInputArgs)
        return NvError_BadParameter;

    pVbusIntr = (NvDdkUsbPhyIoctl_VBusInterruptInputArgs *)pInputArgs;

    if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_Utmi)
    {
#ifdef NVODM_BOARD_IS_FPGA
        RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_SENSORS);
        if (pVbusIntr->EnableVBusInterrupt)
        {
            // VBUS_SENSORS -- A_SESS_VLD_INT_EN
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_SENSORS,
                    A_SESS_VLD_INT_EN, ENABLE, RegVal);
        }
        else
        {
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_SENSORS,
                    A_SESS_VLD_INT_EN, ENABLE, RegVal);

            // VBUS_SENSORS -- A_SESS_VLD_CHG_DET
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_SENSORS,
                    A_SESS_VLD_CHG_DET, SET, RegVal);
        }

        USB1_IF_REG_WR(USB_PHY_VBUS_SENSORS, RegVal);
#else
        RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);
        if (pVbusIntr->EnableVBusInterrupt)
        {
            // VBUS_WAKEUP_ID -- VBUS_WAKEUP_INT_EN
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VBUS_WAKEUP_WAKEUP_EN, ENABLE, RegVal);
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VBUS_WAKEUP_INT_EN, ENABLE, RegVal);
        }
        else
        {
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VBUS_WAKEUP_WAKEUP_EN, DISABLE, RegVal);
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VBUS_WAKEUP_INT_EN, DISABLE, RegVal);
            // VBUS_WAKEUP_ID -- VBUS_WAKEUP_CHG_DET
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VBUS_WAKEUP_CHG_DET, SET, RegVal);
        }

        USB1_IF_REG_WR(USB_PHY_VBUS_WAKEUP_ID, RegVal);
#endif
    }
    else if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_UlpiExternalPhy)
    {
        if (pVbusIntr->EnableVBusInterrupt)
        {
            USB_REG_WR(USBINTR, USB_DRF_DEF(USBINTR, UE, ENABLE));
            RegVal = USB_REG_SET_DRF_DEF(OTGSC, BSVIE, ENABLE);
            USB_REG_WR(OTGSC, RegVal);
        }
        else
        {
            USB_REG_WR(USBSTS, USB_REG_RD(USBSTS));
            RegVal = USB_REG_SET_DRF_DEF(OTGSC, BSVIE, DISABLE);
            USB_REG_WR(OTGSC, RegVal);
        }
    }
    else
    {
        // In NULL phy mode this is not required as VBUS is always present
    }

    return NvSuccess;
}

static NvError
T30UsbPhyIoctlVBusStatus(
        NvDdkUsbPhy *pUsbPhy,
        void *pOutputArgs)
{
    NvDdkUsbPhyIoctl_VBusStatusOutputArgs *pVBusStatus = NULL;
    NvU32 RegVal = 0;

    if (!pOutputArgs)
        return NvError_BadParameter;

    pVBusStatus = (NvDdkUsbPhyIoctl_VBusStatusOutputArgs *)pOutputArgs;

    pVBusStatus = (NvDdkUsbPhyIoctl_VBusStatusOutputArgs *)pOutputArgs;

    if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_Utmi)
    {
#ifdef NVODM_BOARD_IS_FPGA
        RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_SENSORS);
        if (NV_DRF_VAL(USB1_IF, USB_PHY_VBUS_SENSORS, A_SESS_VLD_STS, RegVal))
#else
            RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);
        if (NV_DRF_VAL(USB1_IF, USB_PHY_VBUS_WAKEUP_ID, VBUS_WAKEUP_STS, RegVal))
#endif
        {
            pVBusStatus->VBusDetected = NV_TRUE;
        }
        else
        {
            pVBusStatus->VBusDetected = NV_FALSE;
        }
    }
    else if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_UlpiExternalPhy)
    {
        if (USB_REG_READ_VAL(OTGSC, BSV))
        {
            pVBusStatus->VBusDetected = NV_TRUE;
        }
        else
        {
            pVBusStatus->VBusDetected = NV_FALSE;
        }
    }
    else
    {
        // In NULL phy mode VBUS is always present
        pVBusStatus->VBusDetected = NV_TRUE;
    }

    return NvSuccess;
}

static NvError
T30UsbPhyIoctlIdPinInterrupt(
        NvDdkUsbPhy *pUsbPhy,
        const void *pInputArgs)
{
    NvDdkUsbPhyIoctl_IdPinInterruptInputArgs *pIdPinIntr = NULL;
    NvU32 RegVal = 0;

    if (!pInputArgs)
        return NvError_BadParameter;

    pIdPinIntr = (NvDdkUsbPhyIoctl_IdPinInterruptInputArgs *)pInputArgs;

    if (pUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_Utmi)
    {
        RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);

        if (pIdPinIntr->EnableIdPinInterrupt)
        {
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    ID_PU, ENABLE, RegVal);
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    ID_INT_EN, ENABLE, RegVal);
        }
        else
        {
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    ID_PU, DISABLE, RegVal);
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    ID_INT_EN, DISABLE, RegVal);
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    ID_CHG_DET, SET, RegVal);
        }
        USB1_IF_REG_WR(USB_PHY_VBUS_WAKEUP_ID, RegVal);
    }
    else
    {
        //TODO:ENable the ID pin interrupt for the ULPI
    }

    return NvSuccess;
}

static NvError
T30UsbPhyIoctlIdPinStatus(
        NvDdkUsbPhy *pUsbPhy,
        void *pOutputArgs)
{
    NvDdkUsbPhyIoctl_IdPinStatusOutputArgs *pIdPinStatus = NULL;
    NvU32 RegVal = 0;

    if (!pOutputArgs)
        return NvError_BadParameter;

    pIdPinStatus = (NvDdkUsbPhyIoctl_IdPinStatusOutputArgs *)pOutputArgs;

    pIdPinStatus = (NvDdkUsbPhyIoctl_IdPinStatusOutputArgs *)pOutputArgs;

    RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);
    if (!NV_DRF_VAL(USB1_IF, USB_PHY_VBUS_WAKEUP_ID, ID_STS, RegVal))
    {
        pIdPinStatus->IdPinSetToLow = NV_TRUE;
    }
    else
    {
        pIdPinStatus->IdPinSetToLow = NV_FALSE;
    }

    return NvSuccess;
}

static NvError
T30UsbPhyIoctlDedicatedChargerDetection(
        NvDdkUsbPhy *pUsbPhy,
        const void *pInputArgs)
{
    // These values (in milli second) are taken from the battery charging spec.
#define TDP_SRC_ON_MS    100
#define TDPSRC_CON_MS    40
    NvDdkUsbPhyIoctl_DedicatedChargerDetectionInputArgs *pChargerDetection = NULL;
    NvU32 RegVal = 0;

    if (!pInputArgs)
        return NvError_BadParameter;

    pChargerDetection = (NvDdkUsbPhyIoctl_DedicatedChargerDetectionInputArgs *)pInputArgs;

    if (pUsbPhy->pProperty->UsbInterfaceType != NvOdmUsbInterfaceType_Utmi)
    {
        // Charger detection is not there for ULPI
        return NvSuccess;
    }

    if (pChargerDetection->EnableChargerDetection)
    {
        // Enable charger detection logic
        RegVal = USB_UTMIP_REG_RD(BAT_CHRG_CFG0);
        RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
                UTMIP_OP_SRC_EN, 1, RegVal);
        RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
                UTMIP_ON_SINK_EN, 1, RegVal);
        USB_UTMIP_REG_WR(BAT_CHRG_CFG0, RegVal);

        // Source should be on for 100 ms as per USB charging spec
        NvOsSleepMS(TDP_SRC_ON_MS);

        RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);
        if (pChargerDetection->EnableChargerInterrupt)
        {
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VDAT_DET_INT_EN, ENABLE, RegVal);
        }
        else
        {
            // If charger is not connected disable the interrupt
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VDAT_DET_INT_EN, DISABLE, RegVal);
            RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                    VDAT_DET_CHG_DET, SET, RegVal);
        }
        USB1_IF_REG_WR(USB_PHY_VBUS_WAKEUP_ID, RegVal);
    }
    else
    {
        // disable the interrupt
        RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);
        RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                VDAT_DET_INT_EN, DISABLE, RegVal);
        RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_PHY_VBUS_WAKEUP_ID,
                VDAT_DET_CHG_DET, SET, RegVal);
        USB1_IF_REG_WR(USB_PHY_VBUS_WAKEUP_ID, RegVal);

        // Disable charger detection logic
        RegVal = USB_UTMIP_REG_RD(BAT_CHRG_CFG0);
        RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
                UTMIP_OP_SRC_EN, 0, RegVal);
        RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP, BAT_CHRG_CFG0,
                UTMIP_ON_SINK_EN, 0, RegVal);
        USB_UTMIP_REG_WR(BAT_CHRG_CFG0, RegVal);

        // Delay of 40 ms before we pull the D+ as per battery charger spec.
        NvOsSleepMS(TDPSRC_CON_MS);
    }

    return NvSuccess;
}

static NvError
T30UsbPhyIoctlDedicatedChargerStatus(
        NvDdkUsbPhy *pUsbPhy,
        void *pOutputArgs)
{
    NvDdkUsbPhyIoctl_DedicatedChargerStatusOutputArgs *pChargerStatus = NULL;
    NvU32 RegVal = 0;

    if (!pOutputArgs)
        return NvError_BadParameter;

    pChargerStatus = (NvDdkUsbPhyIoctl_DedicatedChargerStatusOutputArgs *)pOutputArgs;

    if (pUsbPhy->pProperty->UsbInterfaceType != NvOdmUsbInterfaceType_Utmi)
    {
        // Charger detection is not there for ULPI
        // return Charger not available
        pChargerStatus->ChargerDetected = NV_FALSE;
        return NvSuccess;
    }

    RegVal = USB1_IF_REG_RD(USB_PHY_VBUS_WAKEUP_ID);
    if (NV_DRF_VAL(USB1_IF, USB_PHY_VBUS_WAKEUP_ID, VDAT_DET_STS, RegVal))
    {
        pChargerStatus->ChargerDetected = NV_TRUE;
    }
    else
    {
        pChargerStatus->ChargerDetected = NV_FALSE;
    }

    return NvSuccess;
}


static NvError
T30UsbPhyIoctl(
        NvDdkUsbPhy *pUsbPhy,
        NvDdkUsbPhyIoctlType IoctlType,
        const void *pInputArgs,
        void *pOutputArgs)
{
    NvError ErrStatus = NvSuccess;

    switch (IoctlType)
    {
        case NvDdkUsbPhyIoctlType_VBusStatus:
            ErrStatus = T30UsbPhyIoctlVBusStatus(pUsbPhy, pOutputArgs);
            break;
        case NvDdkUsbPhyIoctlType_VBusInterrupt:
            ErrStatus = T30UsbPhyIoctlVbusInterrupt(pUsbPhy, pInputArgs);
            break;
        case NvDdkUsbPhyIoctlType_IdPinStatus:
            ErrStatus = T30UsbPhyIoctlIdPinStatus(pUsbPhy, pOutputArgs);
            break;
        case NvDdkUsbPhyIoctlType_IdPinInterrupt:
            ErrStatus = T30UsbPhyIoctlIdPinInterrupt(pUsbPhy, pInputArgs);
            break;
        case NvDdkUsbPhyIoctlType_DedicatedChargerStatus:
            ErrStatus = T30UsbPhyIoctlDedicatedChargerStatus(pUsbPhy, pOutputArgs);
            break;
        case NvDdkUsbPhyIoctlType_DedicatedChargerDetection:
            ErrStatus = T30UsbPhyIoctlDedicatedChargerDetection(pUsbPhy, pInputArgs);
            break;
        default:
            return NvError_NotSupported;
    }

    return ErrStatus;
}

static NvError
T30UsbPhyWaitForStableClock(
        NvDdkUsbPhy *pUsbPhy)
{
    NvU32 TimeOut = USB_PHY_HW_TIMEOUT_US;
    NvU32 PhyClkValid = 0;

    // Wait for the phy clock to become valid or hardware timeout
    do {
        PhyClkValid = USB_IF_REG_READ_VAL(SUSP_CTRL, USB_PHY_CLK_VALID);
        if (!TimeOut)
        {
            return NvError_Timeout;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (PhyClkValid != USB3_IF_USB_SUSP_CTRL_0_USB_PHY_CLK_VALID_SET);

    return NvSuccess;
}

static NvError
T30UsbPhyPowerUp(
        NvDdkUsbPhy *pUsbPhy)
{
    NvError ErrVal = NvSuccess;

    switch (pUsbPhy->pProperty->UsbInterfaceType)
    {
        case NvOdmUsbInterfaceType_UlpiNullPhy:
            T30UsbPhyUlpiNullModeConfigure(pUsbPhy);
            T30UsbPhyUlpiPowerControl(pUsbPhy, NV_TRUE);
            break;
        case NvOdmUsbInterfaceType_UlpiExternalPhy:
            pUsbPhy->hOdmUlpi = NvOdmUsbUlpiOpen(pUsbPhy->Instance);
            T30UsbPhyUlpiLinkModeConfigure(pUsbPhy, NV_TRUE);
            T30UsbPhyUlpiPowerControl(pUsbPhy, NV_TRUE);
            break;
        case NvOdmUsbInterfaceType_Utmi:
            ErrVal = T30UsbPhyUtmiConfigure(pUsbPhy);
            if (ErrVal != NvSuccess)
                return ErrVal;
            T30UsbPhyUtmiPowerControl(pUsbPhy, NV_TRUE);
        default:
            break;
    }

    ErrVal = T30UsbPhyWaitForStableClock(pUsbPhy);

    if (ErrVal == NvSuccess)
    {
        T30UsbPhySelectUsbMode(pUsbPhy);

        if (pUsbPhy->pProperty->UsbInterfaceType ==
                NvOdmUsbInterfaceType_UlpiNullPhy)
            pUsbPhy->hOdmUlpi = NvOdmUsbUlpiOpen(pUsbPhy->Instance);
    }

    return ErrVal;
}

static NvError
T30UsbPhyPowerDown(
        NvDdkUsbPhy *pUsbPhy)
{
    switch (pUsbPhy->pProperty->UsbInterfaceType)
    {
        case NvOdmUsbInterfaceType_UlpiNullPhy:
            break;
        case NvOdmUsbInterfaceType_UlpiExternalPhy:
            T30UsbPhyUlpiLinkModeConfigure(pUsbPhy, NV_FALSE);
            T30UsbPhyUlpiPowerControl(pUsbPhy, NV_FALSE);
            if (pUsbPhy->hOdmUlpi)
            {
                NvOdmUsbUlpiClose(pUsbPhy->hOdmUlpi);
                pUsbPhy->hOdmUlpi = NULL;
            }
            break;
        case NvOdmUsbInterfaceType_Utmi:
            T30UsbPhyUtmiPowerControl(pUsbPhy, NV_FALSE);
        default:
            break;
    }

    return NvSuccess;
}

static void
T30UsbPhyClose(
        NvDdkUsbPhy *pUsbPhy)
{
    // Power down the USB controller
    NV_ASSERT_SUCCESS(T30UsbPhyPowerDown(pUsbPhy));
}

void
T30UsbPhyOpenHwInterface(
        NvDdkUsbPhy *pUsbPhy)
{
    pUsbPhy->PowerUp = T30UsbPhyPowerUp;
    pUsbPhy->PowerDown = T30UsbPhyPowerDown;
    pUsbPhy->Ioctl = T30UsbPhyIoctl;
    pUsbPhy->WaitForStableClock = T30UsbPhyWaitForStableClock;
    pUsbPhy->CloseHwInterface = T30UsbPhyClose;
}
