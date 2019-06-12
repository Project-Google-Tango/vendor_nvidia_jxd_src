/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_DC_DSI_HAL_H
#define INCLUDED_DC_DSI_HAL_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvrm_channel.h"
#include "nvrm_gpio.h"
#include "nvodm_disp.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "dc_hal.h"
#include "ardsi.h"
#include "arapb_misc.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* dsi registers are word-adddressed in the spec */
#define DSI_WRITE( h, reg, val ) \
    NV_WRITE32( (NvU32 *)((h)->VirAddr) + (reg), (val) )

#define DSI_READ( h, reg ) \
    NV_READ32( (NvU32 *)((h)->VirAddr) + (reg) )

#define DSI_ENABLE_DEBUG_PRINTS 0

#define DSI_MAX_VSYNC_TIMEOUT_MSEC  2

// Macros for calculating the phy timings
// Number of bits per byte
#define DSI_NO_OF_BITS_PER_BYTE 8
// Period of one bit time in nano seconds
#define DSI_TBIT_Factorized(Freq)    (((1000) * (1000) * (1000))/(Freq * 2))
#define DSI_TBIT(Freq)       (DSI_TBIT_Factorized(Freq)/(1000))

// Period of one byte time in nano seconds
#define DSI_TBYTE(Freq)    ((DSI_TBIT_Factorized(Freq)) * (DSI_NO_OF_BITS_PER_BYTE))
#define DSI_PHY_TIMING_DIV(X, Freq) ((X * 1000) / (DSI_TBYTE(Freq)))
// As per Mipi spec (3 + NV_MAX(8 * DSI_TBIT, 60 + 4 * DSI_TBIT) /
//    DSI_TBYTE) min
#define DSI_THSTRAIL_VAL(Freq) \
     (NV_MAX(((8) * (DSI_TBIT(Freq))), ((60) + ((4) * (DSI_TBIT(Freq))))))

// Minimum ULPM wakeup time as per the spec is 1msec
#define DSI_ULPM_WAKEUP_TIME_MSEC   1
#define DSI_CYCLE_COUNTER_VALUE     512
#define DSI_TIMEOUT_VALUE(Delay, Freq) \
    ((Delay) *  \
     (DSI_PHY_TIMING_DIV(((1000) * (1000)), (Freq)))) / \
    (DSI_CYCLE_COUNTER_VALUE)

// LP host RX timeout terminal count.
// FIXME: This value needs to be calculated based on Max Rx data from the
// panel.
#define DSI_LRXH_TO_VALUE   0x2000
// Default Peripheral reset timeout
#define DSI_PR_TO_VALUE     0x2000
// Turn around timeout terminal count
#define DSI_TA_TO_VALUE     0x2000
// Additional Hs TX timeout margin
#define DSI_HTX_TO_MARGIN   720

// Turn around timeout tally
#define DSI_TA_TALLY_VALUE      0x0
// LP Rx timeout tally
#define DSI_LRXH_TALLY_VALUE    0x0
// HS Tx Timeout tally
#define DSI_HTX_TALLY_VALUE     0x0

// DSI Power control settle time 10 micro seconds
#define DSI_POWER_CONTROL_SETTLE_TIME_US    10

/* Horizontal Sync Blank Packet Over head
 * DSI_overhead = size_of(HS packet header)
 *             + size_of(BLANK packet header) + size_of(checksum)
 * DSI_overhead = 4 + 4 + 2 = 10
 */
#define DSI_HSYNC_BLNK_PKT_OVERHEAD  10

/* Horizontal Front Porch Packet Overhead
 * DSI_overhead = size_of(checksum)
 *            + size_of(BLANK packet header) + size_of(checksum)
 * DSI_overhead = 2 + 4 + 2 = 8
 */
#define DSI_HFRONT_PORCH_PKT_OVERHEAD 8

/* Horizontal Back Porch Packet
 * DSI_overhead = size_of(HE packet header)
 *            + size_of(BLANK packet header) + size_of(checksum)
 *            + size_of(RGB packet header)
 * DSI_overhead = 4 + 4 + 2 + 4 = 14
 */
#define DSI_HBACK_PORCH_PKT_OVERHEAD  14


#define SOL_TO_VALID_PIX_CLK_DELAY 4
#define VALID_TO_FIFO_PIX_CLK_DELAY 4
#define FIFO_WR_PIX_CLK_DELAY 2
#define FIFO_RD_BYTE_CLK_DELAY 6


// Maximum polling time for reading the dsi status register
#define DSI_STATUS_POLLING_DURATION_USEC    100000
#define DSI_STATUS_POLLING_DELAY_USEC       100
#define DSI_VIDEO_FIFO_DEPTH    480
#define DSI_HOST_FIFO_DEPTH     64
#define DSI_PACKET_HEADER_SIZE_BYTES  4
#define DSI_COMMAND_SIZE_BYTES    1
// Fifo depthe available for writing data.
#define DSI_HOST_FIFO_DEPTH_BYTES   \
    ((DSI_HOST_FIFO_DEPTH * (4)) -  \
            (DSI_PACKET_HEADER_SIZE_BYTES + DSI_COMMAND_SIZE_BYTES))
#define DSI_VIDEO_FIFO_DEPTH_BYTES   \
    ((DSI_VIDEO_FIFO_DEPTH * (4)) -  \
            (DSI_PACKET_HEADER_SIZE_BYTES + DSI_COMMAND_SIZE_BYTES))
// Recomended SOL delay
#define DSI_DEFAULT_SOL_DELAY   0x18

// Mask to get the MASB byte of the column and row address
#define DSI_MSB_ADDRESS_MASK    8

// DCS commands for command mode
#define DSI_ENTER_PARTIAL_MODE  0x12
#define DSI_SET_PIXEL_FORMAT    0x3A
#define DSI_AREA_COLOR_MODE 0x4C
#define DSI_SET_PARTIAL_AREA    0x30
#define DSI_SET_PAGE_ADDRESS    0x2B
#define DSI_SET_ADDRESS_MODE    0x36
#define DSI_SET_COLUMN_ADDRESS  0x2A
#define DSI_WRITE_MEMORY_START  0x2C
#define DSI_WRITE_MEMORY_CONTINUE  0x3C
#define DSI_MAX_COMMAND_DELAY_USEC  250000
#define DSI_COMMAND_DELAY_STEPS_USEC  10

// Nop command. It is for running sanity check
#define DSI_CMD_NOP_COMMAND_TYPE        0x05
#define DSI_CMD_NOP_COMMAND_REG         0x00
#define DSI_CMD_NOP_COMMAND_SIZE        0x01
#define DSI_CMD_NOP_COMMAND_DATA        0x00
#define DSI_CMD_NOP_COMMAND_ISLONG      NV_FALSE

// Command for turning display on/off. This is a MIPI standard command and is
// the same for all panels
#define DSI_CMD_DISPLAY_ONOFF_TYPE      0x05
#define DSI_CMD_DISPLAY_ON_REG          0x29
#define DSI_CMD_DISPLAY_OFF_REG         0x28
#define DSI_CMD_DISPLAY_ONOFF_SIZE      0x01
#define DSI_CMD_DISPLAY_ONOFF_DATA      0x00
#define DSI_CMD_DISPLAY_ONOFF_ISLONG    NV_FALSE

// End of Transmit command for HS mode
#define DSI_CMD_HS_EOT_PACKAGE          0x000F0F08

// Type of error messages on BTA read
#define DSI_BTA_ACK_FALSE_CTRL_ERR      0x00004002
#define DSI_BTA_ACK_SINGLE_ECC_ERR      0x00000102

// Error message header
#define DSI_ERROR_HEADER_SIG   0x02

// Max number of attempt for clock enable WAR
#define DSI_MAX_CLOCK_ENABLE_TIMEOUT    5

#define DSI_USE_SYNC_POINTS 1

#define DSI_MAX_SYNC_POINT_WAIT_MSEC    1000
// Delay required after issuing the trigger
#define DSI_COMMAND_COMPLETION_DELAY_USEC   5

#define DSI_TE_WAIT_TIMEOUT_MSEC    100
#define DSI_TE_WAIT_TIMEOUT_USEC    ((DSI_TE_WAIT_TIMEOUT_MSEC) * (1000))
#define DSI_TE_POLL_TIME_USEC   10
// FIXME: Calculate the timeout based on panel clock
#define DSI_DELAY_FOR_FIFO_FLUSH_MSEC   20

// min tHSPREPR (in nS) for tHSPREPR WAR
#define DSI_MIN_THSPREPR_FOR_THS_WAR    300

// max DSI_clock (in kHz) for tHSPREPR WAR
#define DSI_MAX_DSICLK_FOR_THS_WAR      1200

enum{
    TEGRA_DSI_PAD_ENABLE,
    TEGRA_DSI_PAD_DISABLE
};

#define MIPI_DSI_AUTOCAL_TIMEOUT_USEC 2000
#define DSI_PAD_CONTROL 0x4b
#define DSI_PAD_CONTROL_0_VS1 0x4b
#define DSI_PAD_CONTROL_2_VS1 0x50
#define MIPI_CAL_STARTCAL(x)       (((x) & 0x1) << 0)
#define MIPI_CAL_ACTIVE(x)         (((x) & 0x1) << 0)
#define MIPI_AUTO_CAL_DONE(x)      (((x) & 0x1) << 16)
#define DSI_PAD_SLEWUPADJ(x)       (((x) & 0x7) << 16)
#define DSI_PAD_SLEWDNADJ(x)       (((x) & 0x7) << 12)
#define DSI_PAD_LPUPADJ(x)         (((x) & 0x7) << 8)
#define DSI_PAD_LPDNADJ(x)         (((x) & 0x7) << 4)
#define DSI_PAD_OUTADJCLK(x)       (((x) & 0x7) << 0)
#define PAD_PDVREG(x)              (((x) & 0x1) << 1)
#define PAD_VCLAMP_LEVEL(x)        (((x) & 0x7) << 16)
#define MIPI_CAL_OVERIDEDSIA(x)    (((x) & 0x1) << 30)
#define MIPI_CAL_SELDSIA(x)        (((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSDSIA(x)     (((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSDSIA(x)     (((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSDSIA(x)     (((x) & 0x1f) << 0)
#define MIPI_CAL_SELDSID(x)        (((x) & 0x1) << 21)
#define MIPI_CAL_SELDSIC(x)        (((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSDSIC(x)     (((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSDSIC(x)     (((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSDSIC(x)     (((x) & 0x1f) << 0)
#define MIPI_CAL_OVERIDEC(x)       (((x) & 0x1) << 30)
#define MIPI_CAL_SELC(x)           (((x) & 0x1) << 21)
#define MIPI_CAL_HSPDOSC(x)        (((x) & 0x1f) << 16)
#define MIPI_CAL_HSPUOSC(x)        (((x) & 0x1f) << 8)
#define MIPI_CAL_TERMOSC(x)        (((x) & 0x1f) << 0)
#define MIPI_CAL_SELDSIA(x)        (((x) & 0x1) << 21)
#define MIPI_CAL_SELDSIB(x)        (((x) & 0x1) << 21)
#define MIPI_CAL_NOISE_FLT(x)      (((x) & 0xf) << 26)
#define MIPI_CAL_PRESCALE(x)       (((x) & 0x3) << 24)
#define MIPI_CAL_CLKEN_OVR(x)      (((x) & 0x1) << 4)
#define MIPI_CAL_AUTOCAL_EN(x)     (((x) & 0x1) << 1)
#define MIPI_CAL_OVERIDEDSIC(x)    (((x) & 0x1) << 30)
#define MIPI_AUTO_CAL_DONE_DSID(x) (((x) & 0x1) << 31)
#define MIPI_AUTO_CAL_DONE_DSIC(x) (((x) & 0x1) << 30)
#define MIPI_AUTO_CAL_DONE_DSIB(x) (((x) & 0x1) << 29)
#define MIPI_AUTO_CAL_DONE_DSIA(x) (((x) & 0x1) << 28)
#define MIPI_AUTO_CAL_DONE_CSIE(x) (((x) & 0x1) << 24)
#define MIPI_AUTO_CAL_DONE_CSID(x) (((x) & 0x1) << 23)
#define MIPI_AUTO_CAL_DONE_CSIC(x) (((x) & 0x1) << 22)
#define MIPI_AUTO_CAL_DONE_CSIB(x) (((x) & 0x1) << 21)
#define MIPI_AUTO_CAL_DONE_CSIA(x) (((x) & 0x1) << 20)
#define MIPI_AUTO_CAL_DONE(x)      (((x) & 0x1) << 16)
#define MIPI_CAL_DRIV_DN_ADJ(x)    (((x) & 0xf) << 12)
#define MIPI_CAL_DRIV_UP_ADJ(x)    (((x) & 0xf) << 8)
#define MIPI_CAL_TERMADJ(x)        (((x) & 0xf) << 4)
#define MIPI_CAL_ACTIVE(x)         (((x) & 0x1) << 0)
#define DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB(x)   (((x) & 0x1) << 24)
#define DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_CLK_ENAB(x)   (((x) & 0x1) << 24)
#define DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(x)       (((x) & 0xf) << 16)
#define DSI_PAD_CONTROL_0_VS1_PAD_PULLDN_ENAB(x)       (((x) & 0xf) << 16)
#define DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK(x)          (((x) & 0x1) << 8)
#define DSI_PAD_CONTROL_0_VS1_PAD_PDIO_CLK(x)          (((x) & 0x1) << 8)
#define DSI_PAD_CONTROL_0_VS1_PAD_PDIO(x)              (((x) & 0xf) << 0)
#define DSI_PAD_CONTROL_0_VS1_PAD_PDIO(x)              (((x) & 0xf) << 0)
#define DSI_PAD_CONTROL_PAD_PDIO_CLK(x)                (((x) & 0x1) << 18)
#define DSI_PAD_CONTROL_PAD_PDIO(x)                    (((x) & 0x3) << 16)
#define DSI_PAD_CONTROL_PAD_PULLDN_ENAB(x)             (((x) & 0x1) << 28)
#define MIPI_CAL_CLKOVERIDEDSIA(x)                     (((x) & 0x1) << 30)
#define MIPI_CAL_CLKSELDSIA(x)                         (((x) & 0x1) << 21)
#define MIPI_CAL_HSCLKPDOSDSIA(x)                      (((x) & 0x1f) << 8)
#define MIPI_CAL_HSCLKPUOSDSIA(x)                      (((x) & 0x1f) << 0)
#define PAD_DRIV_UP_REF(x)                             (((x) & 0x7) << 8)

/* DSI packet commands from Host to peripherals */
typedef enum
{
    DsiCommand_VSyncStart = 0x01,
    DsiCommand_VSyncEnd = 0x11,
    DsiCommand_HSyncStart = 0x21,
    DsiCommand_HSyncEnd = 0x31,
    DsiCommand_EndOfTransaction = 0x08,
    DsiCommand_Blanking = 0x19,
    DsiCommand_NullPacket = 0x09,
    DsiCommand_HActiveLength16Bpp = 0x0E,
    DsiCommand_HActiveLength18Bpp = 0x1E,
    DsiCommand_HActiveLength18BppNp = 0x2E,
    DsiCommand_HActiveLength24Bpp = 0x3E,
    DsiCommand_HSyncActive = DsiCommand_Blanking ,
    DsiCommand_HBackPorch = DsiCommand_Blanking,
    DsiCommand_HFrontPorch = DsiCommand_Blanking,
    DsiCommand_WriteNoParam = 0x05,
    DsiCommand_LongWrite = 0x39,
    DsiCommand_MaxReturnPacketSize = 0x37,
    DsiCommand_GenericReadRequestWith2Param = 0x24,
    DsiCommand_DcsReadWithNoParams = 0x06,

    DsiCommand_Force32 = 0x7FFFFFFF
} DsiCommand;

/* dsi virtual channel bit position, refer to the DSI specs */
enum { DSI_VIR_CHANNEL_BIT_POSITION = 6 };

/* DBI interface pixel formats */
typedef enum
{
    DsiDbiPixelFormats_1 = 0,
    DsiDbiPixelFormats_3,
    DsiDbiPixelFormats_8 ,
    DsiDbiPixelFormats_12,
    DsiDbiPixelFormats_Reserved,
    DsiDbiPixelFormats_16,
    DsiDbiPixelFormats_18,
    DsiDbiPixelFormats_24,

    DsiDbiPixelFormats_Force32 = 0x7FFFFFFF
} DsiDbiPixelFormats;

typedef struct DcDsiRec
{
    NvRmDeviceHandle hRm;
    void *VirAddr;
    NvRmPhysAddr PhyAddr;

    NvU32 BankSize;
    NvU32 DataLanes;
    NvOdmDsiDataFormat DataFormat;
    NvOdmDsiVideoModeType VideoMode;
    NvOdmDsiVirtualChannel VirChannel;
    NvU32 RefreshRate;
    NvU32 LpCmdModeFreqKHz;
    NvU32 HsCmdModeFreqKHz;
    NvBool HsSupportForFrameBuffer;
    NvU32 PanelResetTimeoutMSec;
    NvU32 PhyFreq;

    NvU32 ControllerIndex;
    NvRmFreqKHz freq;
    NvU32 PowerClientId;
    NvRmChannelHandle  ch;
    NvOsSemaphoreHandle sem;
    NvU32 SyncPointId;
    NvU32 SyncPointShadow;
    NvU32 ChannelMutex;
    NvU32 DsiInstance;

    NvRmGpioHandle DsiTeGpioHandle;
    NvRmGpioPinHandle  DsiTePinHandle;
    NvRmGpioInterruptHandle GpioIntrHandle;
    NvOsSemaphoreHandle TeSignalSema;

    NvBool HostCtrlEnable;
    NvBool bInit;
    NvBool bEnabled;
    NvBool bDsiIsInHsMode;
    /** Holds the control value of the HS clock lane. */
    NvOdmDsiHsClock HsClockControl;
    /**
     * min tHSPREPR for its WAR. Valid only if HsClockControl is set to
     * NvOdmDsiHsClock_Continuous_Thsprepr_WAR
     */
    NvU32 WarMinThsprepr;
    /**
     * Max DSI clock (in KHz) when transitioning from LP to HS. Valid only if
     * HsClockControl is set to NvOdmDsiHsClock_Continuous_Thsprepr_PLLD_WAR
     */
    NvU32 WarMaxInitDsiClkKHz;

    NvBool TeFrameUpdate;
    NvOsMutexHandle DsiTransportMutex;
    NvBool EnableHsClockInLpMode;

    /* physical display device */
    struct NvDdkDispPanelRec *panel;

    NvOdmDispDsiMode Dsimode;
    NvOdmDispGangedType GangedType;
} DcDsi;

// DSI command details
typedef struct DsiCommandInfoRec
{
    // DSI command
    NvU32 DsiCmdType;
    // DSI command
    NvU32 DsiCmd;
    // Packet format
    NvBool IsLongPacket;
} DsiCommandInfo;

typedef struct DsiPartialRegionInfoRec
{
    /// top bar of the partial area
    NvS32 top_bar;

    /// bottom bar of the partial area
    NvS32 bot_bar;

    /// left column of a rectangle
    NvS32 left;

    /// top row of a rectangle
    NvS32 top;

    /// right column of a rectangle
    NvS32 right;

    /// bottom row of a rectangle
    NvS32 bottom;

    /// Number of bits per pixel in the data to be displayed.
    NvU32 BitsPerPixel;
} DsiPartialRegionInfo;

/* setup the DSI phy timming */
void
DsiSetupPhyTiming(
    DcController *cont,
    DcDsi *dsi,
    NvOdmDispDeviceHandle hPanel );

/* setup the packet lengths required for the DSI packet sequence*/
void DsiSetupPacketLength( DcController *cont, DcDsi *dsi );

/* setup the packet sequence required for the DSI */
void DsiSetupPacketSequence( DcDsi *dsi);

/* setup SOL delay required for the DSI */
void DsiSetupSolDelay( DcController *cont, DcDsi *dsi );

#ifdef NV_SUPPORTS_GANGED_MODE
/* setup SOL delay required for the DSI */
NvU32 PrivDsiGetGangedModeSolDelay(DcController *cont, DcDsi *dsi);

/* setup the packet lengths required for the DSI packet sequence*/
void GangedModeDsiSetupPacketLength(DcController *cont, DcDsi *dsi);

/* setup the packet sequence required for the DSI */
void GangedModeDsiSetupPacketSequence(DcDsi *dsi);
#endif

/**
 * enables dsi
 */
NvError
DcDsiEnable(
    DcController *cont,
    NvU32 DsiInstance,
    NvOdmDispDeviceHandle hPanel );

/**
 * disables dsi
 */
void DcDsiDisable( DcDsi *dsi );

NvError
dsi_write_data( DcDsi *dsi, NvU32 CommandType,
    NvU8 RegAddr, NvU8 *pData, NvU32 DataSize, NvBool IsLongPacket );

NvError dsi_read_data( DcDsi *dsi, NvU8 RegAddr,
    NvU8 *pData, NvU32 DataSize, NvBool IsLongPacket );

NvError dsi_bta_check_error( DcDsi *dsi );

NvError
dsi_enable_command_mode( DcDsi *dsi, NvOdmDispDeviceHandle hPanel );

/** public exports */

NvBool
DcDsiTransInit(
    NvOdmDispDeviceHandle hDevice,
    NvU32 DsiInstance,
    NvU32 flags );



void DcDsiTransDeinit( NvOdmDispDeviceHandle hDevice, NvU32 DsiInstance );

NvBool
DcDsiTransWrite(
    NvOdmDispDeviceHandle hDevice,
    NvU32 DataType,
    NvU32 RegAddr,
    NvU32 DataSize,
    NvU8 *pData,
    NvU32 DsInstance,
    NvBool bLongPacket );

NvBool
DcDsiTransRead(
    NvOdmDispDeviceHandle hDevice,
    NvU8 RegAddr,
    NvU32 DataSize,
    NvU8 *pData,
    NvU32 DsInstance,
    NvBool bLongPacket );

NvBool
DcDsiTransEnableCommandMode(
    NvOdmDispDeviceHandle hDevice,
    NvU32 DsiInstance );

NvBool
DcDsiTransUpdate(
    NvOdmDispDeviceHandle hDevice,
    NvU8 *pSurface,
    NvU32 width,
    NvU32 height,
    NvU32 bpp,
    NvPoint *pSrc,
    NvU32 DsInstance,
    NvRect *pUpdate );

NvBool
DcDsiTransGetPhyFreq(
    NvOdmDispDeviceHandle hDevice,
    NvU32 DsInstance,
    NvU32 *pDsiPhyFreqKHz );

NvBool
DcDsiTransPing(
    NvOdmDispDeviceHandle hDevice,
    NvU8 RegAddr,
    NvU32 DataSize,
    NvU8 *pData,
    NvU32 *pReadResp,
    NvU32 DsInstance,
    NvU32 flags );

#if defined(__cplusplus)
}
#endif

#endif
