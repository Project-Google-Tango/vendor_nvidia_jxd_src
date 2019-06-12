/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvml_usbf.h"
#include "nvml_usbf_common.h"
#include "nvboot_misc_t30.h"
#include "t30/include/nvboot_util_int.h"
#include "t30/include/nvboot_hardware_access_int.h"
#include "t30/arusb.h"
#include "t30/arapbpm.h"
#include "t30/arapb_misc.h"
#include "t30/arclk_rst.h"

#define UTMIP_PLL_RD(base, reg) \
    NV_READ32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0)

#define UTMIP_PLL_WR(base, reg, data) \
    NV_WRITE32(base + CLK_RST_CONTROLLER_UTMIP_##reg##_0, data)

#define UTMIP_PLL_SET_DRF_DEF(base, reg, field, def) \
    NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER_UTMIP, reg, field, def, UTMIP_PLL_RD(base, reg))

#define UTMIP_PLL_UPDATE_DEF(base, reg, field, define) \
    UTMIP_PLL_WR(base, reg, UTMIP_PLL_SET_DRF_DEF(base, reg, field, define))

#define NV_ADDRESS_MAP_USB_BASE             0x7D000000
#define NV_ADDRESS_MAP_CLK_RST_BASE         0x60006000
#define NV_ADDRESS_MAP_APB_PMC_BASE         0x7000E400
#define NV_ADDRESS_MAP_DATAMEM_BASE         0x40000000

//USB transfer buffer offset in IRAM, these macros are used in nvml_usbf.c
#define NV3P_TRANSFER_EP0_BUFFER_OFFSET NV3P_TRANSFER_EP0_BUFFER_OFFSET_T30
#define NV3P_TRANSFER_EP_BULK_BUFFER_OFFSET NV3P_TRANSFER_EP_BULK_BUFFER_OFFSET_T30
#define NV3P_USB_BUFFER_OFFSET NV3P_USB_BUFFER_OFFSET_T30


/* Golbal variables*/
/**
 * USB data store consists of Queue head and device transfer descriptors.
 * This structure must be aligned to 2048 bytes and must be uncached.
 */
static NvBootUsbDescriptorData *s_pUsbDescriptorBuf = NULL;
/**
 * Usbf context record and its pointer used locally by the driver
 */
static NvBootUsbfContext s_UsbfContext;
static NvBootUsbfContext *s_pUsbfCtxt = NULL;


// Dummy fields added to maintain NvBootClocksOscFreq enum values in a sequence
///////////////////////////////////////////////////////////////////////////////
//  PLLU configuration information (reference clock is osc/clk_m and PLLU-FOs are fixed at
//  12MHz/60MHz/480MHz).
//
//  reference frequency
//               13.0MHz       19.2MHz      12.0MHz      26.0MHz      16.8MHz....  38.4MHz....  48MHz
//  -----------------------------------------------------------------------------
// DIVN      960 (3c0h)    200 (0c8h)  960 (3c0h)   960 (3c0h)...400 (190h)..200 (0c8h)..960 (1E0h)
// DIVM      13 ( 0dh)      4 ( 04h)      12 ( 0ch)     26 ( 1ah)      7 (07h)       4 (04h)       12 (0ch)
// Filter frequency MHz
//               4.8              6                 2
// CPCON   1100b          0011b          1100b         1100b          0101b         0011b         1100b
// LFCON0  1                 0                 1                1                 0                0                1
///////////////////////////////////////////////////////////////////////////////
static const UsbPllClockParams s_UsbPllBaseInfo[NvBootClocksOscFreq_MaxVal] =
{
    //DivN, DivM, DivP, CPCON,  LFCON
    {0x3C0, 0x0D, 0x00, 0xC, 1}, // For NvBootClocksOscFreq_13,
    {0x190, 0x07, 0x00, 0x5, 0}, // For NvBootClocksOscFreq_16_8
    {       0,      0,      0,     0, 0}, // dummy field
    {       0,      0,      0,     0, 0}, // dummy field
    {0x0C8, 0x04, 0x00, 0x3, 0}, // For NvBootClocksOscFreq_19_2
    {0x0C8, 0x04, 0x00, 0x3, 0}, // For NvBootClocksOscFreq_38_4,
    {       0,      0,      0,     0, 0}, // dummy field
    {       0,      0,      0,     0, 0}, // dummy field
    {0x3C0, 0x0C, 0x00, 0xC, 1}, // For NvBootClocksOscFreq_12
    {0x3C0, 0x0C, 0x00, 0xC, 1}, // For NvBootClocksOscFreq_48,
    {       0,      0,      0,     0, 0}, // dummy field
    {       0,      0,      0,     0, 0}, // dummy field
    {0x3C0, 0x1A, 0x00, 0xC, 1}  // For NvBootClocksOscFreq_26
};

// Dummy fields added to maintain NvBootClocksOscFreq enum values in a sequence
///////////////////////////////////////////////////////////////////////////////
// PLL CONFIGURATION & PARAMETERS: refer to the arapb_misc_utmip.spec file.
///////////////////////////////////////////////////////////////////////////////
// PLL CONFIGURATION & PARAMETERS for different clock generators:
//-----------------------------------------------------------------------------
// Reference frequency            13.0MHz       19.2MHz       12.0MHz      26.0MHz
// ----------------------------------------------------------------------------
// PLLU_ENABLE_DLY_COUNT   02 (02h)       03 (03h)       02 (02h)     04 (04h)
// PLLU_STABLE_COUNT          51 (33h)       75 (4Bh)       47 (2Fh)     102 (66h)
// PLL_ACTIVE_DLY_COUNT     09 (09h)       12 (0C)        08 (08h)     17 (11h)
// XTAL_FREQ_COUNT             118 (76h)     188 (BCh)     118 (76h)   254 (FEh)
//-----------------------------------------------------------------------------
// Reference frequency            16.8MHz        38.4MHz         48MHz
// ----------------------------------------------------------------------------
// PLLU_ENABLE_DLY_COUNT   03 (03h)        05 (05h)         06 (06h)
// PLLU_STABLE_COUNT          66 (42h)        150 (96h)       188 (BCh)
// PLL_ACTIVE_DLY_COUNT     11 (0Bh)       24 (18h)         30 (1Eh)
// XTAL_FREQ_COUNT             165 (A5h)      375 (177h)     469 (1D5h)
///////////////////////////////////////////////////////////////////////////////
static const UsbPllDelayParams s_UsbPllDelayParams[NvBootClocksOscFreq_MaxVal] =
{
    //ENABLE_DLY,  STABLE_CNT,  ACTIVE_DLY,  XTAL_FREQ_CNT
    {0x02, 0x33, 0x09,  0x7F}, // For NvBootClocksOscFreq_13,
    {0x03, 0x42, 0x0B,  0xA5}, // For NvBootClocksOscFreq_16_8
    {     0,      0,       0,       0}, // dummy field
    {     0,      0,       0,       0}, // dummy field
    {0x03, 0x4B, 0x0C,  0xBC}, // For NvBootClocksOscFreq_19_2
    {0x05, 0x96, 0x18, 0x177}, //For NvBootClocksOscFreq_38_4
    {     0,      0,       0,        0}, // dummy field
    {     0,      0,       0,       0}, // dummy field
    {0x02, 0x2F, 0x08,  0x76}, // For NvBootClocksOscFreq_12
    {0x06, 0xBC, 0X1F, 0x1D5}, // For NvBootClocksOscFreq_48
    {     0,      0,      0,         0}, // dummy field
    {     0,      0,      0,         0}, // dummy field
    {0x04, 0x66, 0x11,   0xFE}  // For NvBootClocksOscFreq_26
};

// Dummy fields added to maintain NvBootClocksOscFreq enum values in a sequence
///////////////////////////////////////////////////////////////////////////////
// Tracking Length Time: The tracking circuit of the bias cell consumes a
// measurable portion of the USB idle power  To curtail this power consumption
// the bias pad has added a PD_TDK signal to power down the bias cell. It is
// estimated that after 20microsec of bias cell operation the PD_TRK signal can
// be turned high to sve power. This can be automated by programming a timing
// interval as given in the below structure.
static const NvU32 s_UsbBiasTrkLengthTime[NvBootClocksOscFreq_MaxVal] =
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

// Dummy fields added to maintain NvBootClocksOscFreq enum values in a sequence
///////////////////////////////////////////////////////////////////////////////
// Debounce values IdDig, Avalid, Bvalid, VbusValid, VbusWakeUp, and SessEnd.
// Each of these signals have their own debouncer and for each of those one out
// of 2 debouncing times can be chosen (BIAS_DEBOUNCE_A or BIAS_DEBOUNCE_B)
//
// The values of DEBOUNCE_A and DEBOUNCE_B are calculated as follows:
// 0xffff -> No debouncing at all
// <n> ms = <n> *1000 / (1/19.2MHz) / 4
// So to program a 1 ms debounce for BIAS_DEBOUNCE_A, we have:
// BIAS_DEBOUNCE_A[15:0] = 1000 * 19.2 / 4  = 4800 = 0x12c0
// We need to use only DebounceA for BOOTROM. We don't need the DebounceB
// values, so we can keep those to default.
///////////////////////////////////////////////////////////////////////////////
static const NvU32 s_UsbBiasDebounceATime[NvBootClocksOscFreq_MaxVal] =
{
    /* Ten milli second delay for BIAS_DEBOUNCE_A */
    0x7EF4,  // For NvBootClocksOscFreq_13,
    0XA410,  //  For NvBootClocksOscFreq_16_8
             0,  // dummy field
             0,  // dummy field
    0xBB80,  // For NvBootClocksOscFreq_19_2
    0xBB80,  //For NvBootClocksOscFreq_38_4,
             0,  // dummy field
             0,  // dummy field
    0x7530,  // For NvBootClocksOscFreq_12
    0xEA60,  // For NvBootClocksOscFreq_48,
             0,  // dummy field
             0,  // dummy field
    0xFDE8   // For NvBootClocksOscFreq_26
};

////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////
//UTMIP Idle Wait Delay
static const NvU8 s_UtmipIdleWaitDelay    = 17;
//UTMIP Elastic limit
static const NvU8 s_UtmipElasticLimit     = 16;
//UTMIP High Speed Sync Start Delay
static const NvU8 s_UtmipHsSyncStartDelay = 9;


static void NvMlPrivUsbInitT30(void)
{
    NvU32 RegVal = 0;
    NvU32 PlluStableTime = 0;
    NvBootClocksOscFreq OscFreq;
    NvU32 UsbBase = NV_ADDRESS_MAP_USB_BASE;

    // Get the Oscillator frequency
    OscFreq = NvBootClocksGetOscFreqT30();

    // Enable PLL U for USB
    NvBootClocksStartPllT30(NvBootClocksPllId_PllU,
                         s_UsbPllBaseInfo[OscFreq].M,
                         s_UsbPllBaseInfo[OscFreq].N,
                         s_UsbPllBaseInfo[OscFreq].P,
                         s_UsbPllBaseInfo[OscFreq].CPCON,
                         s_UsbPllBaseInfo[OscFreq].LFCON,
                         &PlluStableTime);


    NvBootClocksSetEnableT30(NvBootClocksClockId_UsbId, NV_TRUE);
    // Reset the USB controller
    // NvBootResetSetEnableT30(NvBootResetDeviceId_UsbId, NV_TRUE);
    // NvBootResetSetEnableT30(NvBootResetDeviceId_UsbId, NV_FALSE);

    /*
     * Assert UTMIP_RESET in USB1_IF_USB_SUSP_CTRL register to put
     * UTMIP1 in reset.
     */
    USBIF_REG_UPDATE_DEF(UsbBase, USB_SUSP_CTRL, UTMIP_RESET, ENABLE);

    /*
    * Set USB1 to use UTMIP PHY by setting
    * USB1_IF_USB_SUSP_CTRL.UTMIP_PHY_ENB  register to 1.
    */
    RegVal = NV_READ32(UsbBase + USB1_IF_USB_SUSP_CTRL_0);

    RegVal = NV_FLD_SET_DRF_DEF(USB1_IF, USB_SUSP_CTRL, UTMIP_PHY_ENB, ENABLE, RegVal);

    NV_WRITE32(UsbBase + USB1_IF_USB_SUSP_CTRL_0, RegVal);


    // Stop crystal clock by setting UTMIP_PHY_XTAL_CLOCKEN low
    UTMIP_REG_UPDATE_DEF(UsbBase,
                         MISC_CFG1,
                         UTMIP_PHY_XTAL_CLOCKEN,
                         SW_DEFAULT);

    UTMIP_PLL_UPDATE_DEF(NV_ADDRESS_MAP_CLK_RST_BASE,
                         PLL_CFG2,
                         UTMIP_PHY_XTAL_CLOCKEN,
                         SW_DEFAULT);

    // Follow the crystal clock disable by >100ns delay.
    NvOsWaitUS(1);

    /*
     * To Use the A Session Valid for cable detection logic,
     * VBUS_WAKEUP mux must be switched to actually use a_sess_vld
     * threshold.  This is done by setting VBUS_SENSE_CTL bit in
     * USB_LEGACY_CTRL register.
     */
    USBIF_REG_UPDATE_DEF(UsbBase,
                         USB1_LEGACY_CTRL,
                         USB1_VBUS_SENSE_CTL,
                         A_SESS_VLD);

    // PLL Delay CONFIGURATION settings
    // The following parameters control the bring up of the plls:
    RegVal = UTMIP_PLL_RD(NV_ADDRESS_MAP_CLK_RST_BASE, PLL_CFG2);
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
                        PLL_CFG2,
                        UTMIP_PLLU_STABLE_COUNT,
                        s_UsbPllDelayParams[OscFreq].StableCount,
                        RegVal);

    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG2,
                        UTMIP_PLL_ACTIVE_DLY_COUNT,
                        s_UsbPllDelayParams[OscFreq].ActiveDelayCount,
                        RegVal);

    UTMIP_PLL_WR(NV_ADDRESS_MAP_CLK_RST_BASE, PLL_CFG2, RegVal);
    // Set PLL enable delay count and Crystal frequency count ( clock reset domain)
    RegVal = UTMIP_PLL_RD(NV_ADDRESS_MAP_CLK_RST_BASE, PLL_CFG1);
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
                    UTMIP_PLLU_ENABLE_DLY_COUNT,
                    s_UsbPllDelayParams[OscFreq].EnableDelayCount,
                    RegVal);

    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
                    UTMIP_XTAL_FREQ_COUNT,
                    s_UsbPllDelayParams[OscFreq].XtalFreqCount,
                    RegVal);
    // Remove power down for PLLU_ENABLE
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
                    UTMIP_FORCE_PLL_ENABLE_POWERDOWN,
                    0x0,
                    RegVal);
    // Remove power down for PLL_ACTIVE
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
                    UTMIP_FORCE_PLL_ACTIVE_POWERDOWN,
                    0x0,
                    RegVal);
    // Remove power down for PLLU
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
                    UTMIP_FORCE_PLLU_POWERDOWN,
                    0x0,
                    RegVal);

    UTMIP_PLL_WR(NV_ADDRESS_MAP_CLK_RST_BASE, PLL_CFG1, RegVal);

    // Read BIAS cfg0 reg
    RegVal = UTMIP_REG_RD(UsbBase, BIAS_CFG0);
    // Remove power down on BIAS PD
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG0,
                    UTMIP_BIASPD,
                    0x0,
                    RegVal);
    UTMIP_REG_WR(UsbBase, BIAS_CFG0, RegVal);

    // Read BIAS cfg1 reg
    RegVal = UTMIP_REG_RD(UsbBase, BIAS_CFG1);
    // Setting the tracking length time.
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG1,
                    UTMIP_BIAS_PDTRK_COUNT,
                    s_UsbBiasTrkLengthTime[OscFreq],
                    RegVal);

    UTMIP_REG_WR(UsbBase, BIAS_CFG1, RegVal);

    NvOsWaitUS(1);

    // Read BIAS cfg1 reg
    RegVal = UTMIP_REG_RD(UsbBase, BIAS_CFG1);
    // Remove power down on PDTRK
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG1,
                    UTMIP_FORCE_PDTRK_POWERDOWN,
                    0,
                    RegVal);
    // Power up PD TRK
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG1,
                    UTMIP_FORCE_PDTRK_POWERUP,
                    1,
                    RegVal);
    UTMIP_REG_WR(UsbBase, BIAS_CFG1, RegVal);

    // After setting the trk lenth time, need to wait 25us before trk powerdown.
    NvOsWaitUS(25);

    // Enabling power down  for PDTRK.
    RegVal = UTMIP_REG_RD(UsbBase, BIAS_CFG1);

    // Power down on PDTRK ckt again.
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG1,
                    UTMIP_FORCE_PDTRK_POWERDOWN,
                    1,
                    RegVal);

    UTMIP_REG_WR(UsbBase, BIAS_CFG1, RegVal);

    // Set UTMIP_XCVR_LSBIAS_SEL to 0
    // Read XCVR_CFG0 reg
    RegVal = UTMIP_REG_RD(UsbBase, XCVR_CFG0);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    XCVR_CFG0,
                    UTMIP_XCVR_LSBIAS_SEL,
                    0x0,
                    RegVal);
        UTMIP_REG_WR(UsbBase, XCVR_CFG0, RegVal);

    NvOsWaitUS(1);

    // Program 1ms Debounce time for VBUS to become valid.
    UTMIP_REG_UPDATE_NUM(UsbBase, DEBOUNCE_CFG0,
                UTMIP_BIAS_DEBOUNCE_A,
                s_UsbBiasDebounceATime[OscFreq]);

    /* Actual value required for Bias Debounce A for clock 38.4 is 0x17700
    and 48 Mhz is 0x1D4C0. But we have 16 bits for this, as per above calculations we need a 17th bit.
    For this, we need to use below config and set the timescale on bias debounce.
    */
    if ((OscFreq == NvBootClocksOscFreq_38_4) || (OscFreq == NvBootClocksOscFreq_48))
    {
        // Read BIAS cfg1 reg
        RegVal = UTMIP_REG_RD(UsbBase, BIAS_CFG1);
        // Remove power down on PDTRK
        RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                        BIAS_CFG1,
                        UTMIP_BIAS_DEBOUNCE_TIMESCALE,
                        1,
                        RegVal);
        UTMIP_REG_WR(UsbBase, BIAS_CFG1, RegVal);
    }

    // Set bit 3 & 4 of UTMIP_SPARE_CFG0 to 1
    RegVal = UTMIP_REG_RD(UsbBase, SPARE_CFG0) | 0x18;
    UTMIP_REG_WR(UsbBase, SPARE_CFG0, RegVal);

    // Set UTMIP_FS_PREAMBLE_J to 1
    UTMIP_REG_UPDATE_NUM(UsbBase, TX_CFG0, UTMIP_FS_PREAMBLE_J, 0x1);

    /* Configure the UTMIP_IDLE_WAIT and UTMIP_ELASTIC_LIMIT
     * Setting these fields, together with default values of the other
     * fields, results in programming the registers below as follows:
     *         UTMIP_HSRX_CFG0 = 0x9168c000
     *         UTMIP_HSRX_CFG1 = 0x13
     */

    // Set PLL enable delay count and Crystal frequency count
    RegVal = UTMIP_REG_RD(UsbBase, HSRX_CFG0);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    HSRX_CFG0,
                    UTMIP_IDLE_WAIT,
                    s_UtmipIdleWaitDelay,
                    RegVal);

    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    HSRX_CFG0,
                    UTMIP_ELASTIC_LIMIT,
                    s_UtmipElasticLimit,
                    RegVal);

    UTMIP_REG_WR(UsbBase, HSRX_CFG0, RegVal);

    // Configure the UTMIP_HS_SYNC_START_DLY
    UTMIP_REG_UPDATE_NUM(UsbBase, HSRX_CFG1,
                UTMIP_HS_SYNC_START_DLY,
                s_UtmipHsSyncStartDelay);

    // Proceed  the crystal clock enable  by >100ns delay.
    NvOsWaitUS(1);

    // Resuscitate  crystal clock by setting UTMIP_PHY_XTAL_CLOCKEN
    UTMIP_REG_UPDATE_DEF(UsbBase,
                         MISC_CFG1,
                         UTMIP_PHY_XTAL_CLOCKEN,
                         DEFAULT);
    UTMIP_PLL_UPDATE_DEF(NV_ADDRESS_MAP_CLK_RST_BASE,
                         PLL_CFG2,
                         UTMIP_PHY_XTAL_CLOCKEN,
                         DEFAULT);

    // Remove various power downs one by one

    // Set PLL enable delay count and Crystal frequency count ( clock reset domain)
    RegVal = UTMIP_PLL_RD(NV_ADDRESS_MAP_CLK_RST_BASE, PLL_CFG2);
    // Remove power down for PLLU_ENABLE
    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG2,
                    UTMIP_FORCE_PD_SAMP_A_POWERDOWN,
                    0x0,
                    RegVal);

    RegVal = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG2,
                    UTMIP_FORCE_PD_SAMP_A_POWERUP,
                    0x1,
                    RegVal);

    UTMIP_PLL_WR(NV_ADDRESS_MAP_CLK_RST_BASE, PLL_CFG2, RegVal);

    NvOsWaitUS(1);

    // Read BIAS cfg0 reg and remove OTG power down
    RegVal = UTMIP_REG_RD(UsbBase, BIAS_CFG0);
    // Remove power down on OTGPD
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG0,
                    UTMIP_OTGPD,
                    0x0,
                    RegVal);
    // Set 0 to UTMIP_IDPD_SEL
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG0,
                    UTMIP_IDPD_SEL,
                    0x0,
                    RegVal);
    // Set 0 to UTMIP_IDPD_VAL
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    BIAS_CFG0,
                    UTMIP_IDPD_VAL,
                    0x0,
                    RegVal);
    UTMIP_REG_WR(UsbBase, BIAS_CFG0, RegVal);

    NvOsWaitUS(1);

    // Remove Power Down for VBUS detectors from PMC
    // registers.
    RegVal = NV_READ32(NV_ADDRESS_MAP_APB_PMC_BASE + APBDEV_PMC_USB_AO_0);

    // Remove power down on VBUS WAKEUP PD
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC,
                    USB_AO,
                    VBUS_WAKEUP_PD_P0,
                    0x0,
                    RegVal);

    NV_WRITE32(NV_ADDRESS_MAP_APB_PMC_BASE + APBDEV_PMC_USB_AO_0, RegVal);

    NvOsWaitUS(1);

    // Remove power down on PD
    RegVal = UTMIP_REG_RD(UsbBase, XCVR_CFG0);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    XCVR_CFG0,
                    UTMIP_FORCE_PD_POWERDOWN,
                    0x0,
                    RegVal);

    UTMIP_REG_WR(UsbBase, XCVR_CFG0, RegVal);

    NvOsWaitUS(1);

    RegVal = UTMIP_REG_RD(UsbBase, XCVR_CFG0);

    // Remove power down on PD2
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    XCVR_CFG0,
                    UTMIP_FORCE_PD2_POWERDOWN,
                    0x0,
                    RegVal);

    UTMIP_REG_WR(UsbBase, XCVR_CFG0, RegVal);

    NvOsWaitUS(1);

    RegVal = UTMIP_REG_RD(UsbBase, XCVR_CFG0);

    // Remove power down on PDZI
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    XCVR_CFG0,
                    UTMIP_FORCE_PDZI_POWERDOWN,
                    0x0,
                    RegVal);

    UTMIP_REG_WR(UsbBase, XCVR_CFG0, RegVal);

    NvOsWaitUS(1);

    // Remove power down on PDCHRP

    RegVal = UTMIP_REG_RD(UsbBase, XCVR_CFG1);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    XCVR_CFG1,
                    UTMIP_FORCE_PDCHRP_POWERDOWN,
                    0x0,
                    RegVal);
    UTMIP_REG_WR(UsbBase, XCVR_CFG1, RegVal);

    NvOsWaitUS(1);

    // Remove power down on PDDR
    RegVal = UTMIP_REG_RD(UsbBase, XCVR_CFG1);
    RegVal = NV_FLD_SET_DRF_NUM(USB1_UTMIP,
                    XCVR_CFG1,
                    UTMIP_FORCE_PDDR_POWERDOWN,
                    0x0,
                    RegVal);
    UTMIP_REG_WR(UsbBase, XCVR_CFG1, RegVal);

    NvOsWaitUS(1);
}

static NvBool NvMlPrivUsbfIsCableConnectedT30(NvBootUsbfContext *pUsbContext)
{

    /* Return the A Session valid status for cable detection
     * A session valid is "1" = NV_TRUE means cable is present.
     * A session valid is "0" = NV_FALSE means cable is not present.
     */
        return (USBIF_DRF_VAL(USB_PHY_VBUS_SENSORS,
                          A_SESS_VLD_STS,
                          USBIF_REG_RD(NV_ADDRESS_MAP_USB_BASE,
                                       USB_PHY_VBUS_SENSORS)));

}

static NvBootError NvMlPrivUsbfHwEnableControllerT30(void)
{
    NvBootError ErrorValue = NvBootError_Success;
    NvU32 PhyClkValid = 0;
    NvU32 TimeOut = 0;

    //Bring respective USB and PHY out of reset by writing 0 to UTMIP_RESET
    USBIF_REG_UPDATE_DEF(NV_ADDRESS_MAP_USB_BASE,
                         USB_SUSP_CTRL,
                         UTMIP_RESET,
                         DISABLE);

    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    // Wait for the phy clock to become valid or hardware timeout

    do {
        PhyClkValid = USBIF_REG_READ_VAL(NV_ADDRESS_MAP_USB_BASE,
                                         USB_SUSP_CTRL,
                                         USB_PHY_CLK_VALID);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (!PhyClkValid);

    return ErrorValue;
}


static NvBootError NvMlPrivUsbWaitClockT30(void)
{
    NvU32 TimeOut = CONTROLLER_HW_TIMEOUT_US;
    NvU32 PhyClkValid = 0;

    TimeOut = CONTROLLER_HW_TIMEOUT_US;
    do {
        //wait for the phy clock to become valid
    //    regValue = APB_MISC_REG_RD(PP_MISC_USB_OTG);
       // PhyClkValid = NV_DRF_VAL(APB_MISC_PP, MISC_USB_OTG, PCLKVLD, regValue);
        PhyClkValid = USBIF_REG_READ_VAL(NV_ADDRESS_MAP_USB_BASE,
                                         USB_SUSP_CTRL,
                                         USB_PHY_CLK_VALID);
        if (!TimeOut)
        {
            return NvBootError_HwTimeOut;
        }
        NvOsWaitUS(1);
        TimeOut--;
    } while (!PhyClkValid);

    return NvBootError_Success;
}

static void NvMlPrivInitBaseAddressT30(NvBootUsbfContext * pUsbContext)
{
    // Return the base address to USB_BASE, as only USB1 can be used for
    // recovery mode.
    pUsbContext->UsbBaseAddr = NV_ADDRESS_MAP_USB_BASE;
}

static void NvMlPrivGetPortSpeedT30(NvBootUsbfContext * pUsbContext)
{
    pUsbContext->UsbPortSpeed = (NvBootUsbfPortSpeed)
                                 USB_REG_READ_VAL(HOSTPC1_DEVLC, PSPD);
}


#define NVML_FUN_SOC(FN,ARG) FN##T30 ARG
#define NVBOOT_FUN_SOC(FN,ARG) FN##T30 ARG
#include "nvml_usbf.c"
#undef NVML_FUN_SOC
#undef NVBOOT_FUN_SOC
