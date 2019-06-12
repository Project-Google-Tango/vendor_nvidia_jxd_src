/*
 * Copyright (c) 2006-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software, related documentation and any
 * modification thereto.  Any use, reproduction, disclosure or distribution
 * of this software and related documentation without an express license
 * agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Physical Display Interface</b>
 *
 * @b Description: Defines the ODM interface for physical displays.
 *
 */

#ifndef INCLUDED_NVODM_DISP_H
#define INCLUDED_NVODM_DISP_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"

/**
 * @defgroup nvodm_disp Display Adaptation Interface
 *
 * This is the display ODM adaptation interface, for abstraction of physical
 * displays. This API includes initialization, power states, and pin mappings.
 *
 * Each display device has one or more @em modes, each with a its own set of
 * timing values, resolution, and color depth, as specified in its
 * ::NvOdmDispDeviceMode structure instance.
 *
 * The display adaptation HAL calls the Setup() function on
 * uninitialized ("off") panels. Setup defines additional specifications
 * for the panel, such as:
 * - display type (TFT panel/CLI smart panel)
 * - maximum display resolution
 * - color format
 * - data alignment (MSB/LSB)
 * - pin map setting
 *
 * The following list shows the calls to make in the order shown.
 * - NvOdmDispListDevices--returns a list of physical devices.
 * - NvOdmDispListModes--returns the list of modes supported by this panel;
 *     \c NvDdkDisp can then pick the best mode.
 * - NvOdmDispSetMode--sends the instruction sequence to the panel to
 *     turn the display on to the given mode; may be called
 *     again at any time. A NULL mode is returned to release resources.
 * - NvOdmDispSetBacklight--sets the backlight intensity for display devices
 *     that have their own backlight controller.
 * - NvOdmDispSetPowerLevel--cycles through power states, including the initial
 *     power on. The power state is set to off before the mode is cleared via
 *     \c NvOdmDispSetMode.
 * - NvOdmDispGetParameter--returns the device-specfic configuration;
 *     is typically called several times.
 * - NvOdmDispGetPinPolarities--passes back by reference explicitly defined
 *     polarities for signals such as DE, H/V SYNC, and PCLK.
 * - NvOdmDispSetSpecialFunction--sets the given special function on the
 *     display device.
 * - NvOdmDispReleaseDevices--is called at the <em>end of time</em>.
 *
 * All display APIs that can fail will return NV_TRUE if successful, NV_FALSE
 * otherwise.
 *
 * See also <a class="el" href="group__nvodm__example__disp.html">Examples:
 *   Display</a>
 *
 * @ingroup nvodm_adaptation
 * @{
 */

/**
 * Defines an opaque handle that exists for each display device in the
 * system, each of which is defined by the customer implementation.
 */
typedef struct NvOdmDispDeviceRec *NvOdmDispDeviceHandle;

/**
 * Defines the display device timings.
 *
 * Below is an example for horizontal timing (vertical is analogous, replace
 * "active data" with "line timing").
 *
 * <pre>
 *                      blank time
 *    +-------------+               +-------------+
 * ___| active data |_______________| active data |___
 * bp                 fp | sync | bp
 *
 * bp: back porch (Horiz_BackPorch)
 * fp: front porch (Horiz_FrontPorch)
 * sync: Horiz_SyncWidth
 * active data: pixel clock & pixel data
 * </pre>
 *
 * The ref-to-sync (Horiz_RefToSync and Vert_RefToSync) values are for the
 * controller internal timing; they do not show up on the wire.
 * They should be set such that:
 * - Horiz_RefToSync + Horiz_SyncWidth + Horiz_BackPorc > 11
 * - Vert_RefToSync + Vert_SyncWidth + Vert_BackPorch > 1
 * - Vert_RefToSync >= 1
 */
typedef struct NvOdmDispDeviceTimingRec
{
    NvU32 Horiz_RefToSync;
    NvU32 Vert_RefToSync;
    NvU32 Horiz_SyncWidth;
    NvU32 Vert_SyncWidth;
    NvS32 Horiz_BackPorch;
    NvS32 Vert_BackPorch;
    NvU32 Horiz_DispActive;
    NvU32 Vert_DispActive;
    NvU32 Horiz_FrontPorch;
    NvU32 Vert_FrontPorch;
} NvOdmDispDeviceTiming;

/*
 *  Defines the mode in which panel is working
 *  Normal, if a single panel is drived by a DSI instance
 *  Dual, if both DSI instance drives a panel
 */
typedef enum
{
    NvOdmDispDsiMode_Normal = 0x1,
    NvOdmDispDsiMode_Dual = 0x2,
    NvOdmDispDsiMode_Ganged = 0x3,
    NvOdmDispDsiMode_Force32 = 0x7fffffff,
} NvOdmDispDsiMode;

typedef enum
{
    NvOdmDisp_GangedType_LeftRight = 0x1,
    NvOdmDisp_GangedType_OddEven = 0x2,
    NvOdmDisp_GangedType_Force32   = 0x7fffffff,
}NvOdmDispGangedType;

/**
 * Defines the display device types.
 */
typedef enum
{
    /** Specifies parallel pixel data and sync signals panel (dumb panel). */
    NvOdmDispDeviceType_TFT = 0x1,

    /** Specifies serial/narrow bus panel with on-board memory
     * (smart panel).
     */
    NvOdmDispDeviceType_CLI,

    /** Specifies display Serial Interface: MIPI standard. */
    NvOdmDispDeviceType_DSI,

    /** Specifies computer monitor. */
    NvOdmDispDeviceType_CRT,

    /** Specifies standard-definition television. */
    NvOdmDispDeviceType_TV,

    /** Specifies high-definition television. */
    NvOdmDispDeviceType_HDMI,

    NvOdmDispDeviceType_Num,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvOdmDispDeviceType_Force32 = 0x7FFFFFFF,
} NvOdmDispDeviceType;

/**
 *  Defines the display device usage types.
 */
typedef enum
{
    /** Specifies there can only be one Primary device in the system. */
    NvOdmDispDeviceUsage_Primary = 0x1,

    /** Specifies there can only be one Secondary device in the system. */
    NvOdmDispDeviceUsage_Secondary,

    /**
     * Specifies the display device may be plugged or unplugged to/from
     * the system at any time.
     */
    NvOdmDispDeviceUsage_Removable,

    /**
     * Sepcifies an unassigned display usage--This may be seen when there
     * are multiple primary display devices supported by the ODM kit which
     * are runtime-detected. The device that is present in the system will
     * be Primary, all others are Unassigned.
     */
    NvOdmDispDeviceUsage_Unassigned,

    /** Specifies an unknown display device. */
    NvOdmDispDeviceUsage_Unknown,

    NvDdkDispDeviceUsage_Num,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkDispDeviceUsage_Force32 = 0x7FFFFFFF
} NvOdmDispDeviceUsageType;

/**
 * Holds the display device mode.
 */
typedef struct NvOdmDispDeviceModeRec
{
    /** Holds the height of the mode in pixels. */
    NvS32 width;

    /** Holds the width of the mode in pixels. */
    NvS32 height;

    /** Holds the bits per pixel. */
    NvU32 bpp;

    /**
     * Holds the refresh frequency.
     * This is a Q16 (signed 15.16 fixed point value) containing
     * the frequency of the mode in Hertz (Hz). This value is in fixed point
     * to accomodate frequencies that are not integer multiples. For
     * example, 60 Hz would be represented as 0x003C0000.
     */
    NvS32 refresh;

    /**
     * Pixel clock in kilohertz (kHz). Use this to override the automatic
     * pixel clock calculation. Should be left 0, in most cases.
     */
    NvS32 frequency;

    /** One of NVODM_DISP_MODE_FLAG_* or zero. */
    NvU32 flags;
#define NVODM_DISP_MODE_FLAG_NONE           (0x0)
    /**
     * Some panels have a partial mode where a smaller surface may be
     * displayed to save power. When the panel is in partial mode, the
     * window that is diplaying the partial mode surface may be placed
     * anywhere within the max resolution for the normal (full) mode. Only
     * one window may be enabled when a partial mode is set. The color depth
     * of a partial mode will most likely be lower than a full screen mode.
     */
#define NVODM_DISP_MODE_FLAG_PARTIAL        (1UL << 1)
    /** Interlaced rather than progressive scan. */
#define NVODM_DISP_MODE_FLAG_INTERLACED     (1UL << 2)
    /** Hsync is negative polarity. */
#define NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE (1UL << 3)
    /** Vsync is negative polarity. */
#define NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE (1UL << 4)
    /** Native mode for the display device */
#define NVODM_DISP_MODE_FLAG_NATIVE         (1UL << 5)
    /** This mode requires a tearing-effect signal to trigger the display
     * controller to send pixel data.
     */
#define NVODM_DISP_MODE_FLAG_USE_TEARING_EFFECT (1UL << 6)
    /** Not a standardized mode type. */
#define NVODM_DISP_MODE_FLAG_TYPE_CUSTOM    (1UL << 16)
    /**
     * Standard and Established VESA Timings -- see the EDID 1.4
     * specification for more information.
     */
#define NVODM_DISP_MODE_FLAG_TYPE_VESA      (1UL << 17)
    /** Detailed Timing Descriptor (from the EDID spec). */
#define NVODM_DISP_MODE_FLAG_TYPE_DTD       (1UL << 18)
    /** Defined by the CEA-861-D Specification. */
#define NVODM_DISP_MODE_FLAG_TYPE_HDMI      (1UL << 19)
    /** 3D (stereo) display mode, most likely an HDMI 1.4 display. */
#define NVODM_DISP_MODE_FLAG_TYPE_3D        (1UL << 20)

    /** Timings for this mode. */
    NvOdmDispDeviceTiming timing;

    /** Full/partial panel mode.
     * @deprecated This field is deprecated. Use ::flags instead.
     */
    NvBool bPartialMode;

} NvOdmDispDeviceMode;

/**
 * Defines the TFT configuration flags.
 */
typedef enum
{
    /**
     * Specifies to use data enable instead of horizontal/vertical sync.
     */
    NvOdmDispTftFlag_DataEnableMode = 1,

    NvOdmDispTftFlag_Force32= 0x7FFFFFFF,
} NvOdmDispTftFlags;

/**
 * Defines extended configuration TFT devices.
 */
typedef struct NvOdmDispTftConfigRec
{
    /** Specifies zero or more of NvOdmDispTftFlags. */
    NvU32 flags;
} NvOdmDispTftConfig;

/**
 * Defines CLI configuration flags.
 */
typedef enum
{
    /**
     * Specifies that the pixel clock will be disabled during the blanking
     * period.
     */
    NvOdmDispCliFlag_DisableClockInBlank = 0x1,

    /** Ignore. Forces compilers to make 32-bit enums. */
    NvOdmDispCliFlag_Force32 = 0x7FFFFFFF,
} NvOdmDispCliFlags;

#define NVODM_DISP_CLI_MAX_INSTRUCTIONS 6

/**
 * Defines per-frame instructions for CLI panels.
 *
 * Some CLI panels require per-frame register writes. Up to
 * NVODM_DISP_CLI_MAX_INSTRUCTIONS may be sent automatically by the display
 * controller over the parallel data interface.
 *
 * Each instruction may be a command or a parameter, indicated by the Command
 * field.
 */
typedef struct NvOdmDispCliInstructionRec
{
    NvU16 Data;
    NvBool Command;
} NvOdmDispCliInstruction;

/**
 * Defines extended configuration CLI devices.
 */
typedef struct NvOdmDispCliConfigRec
{
    /** Specifies the per-frame instructions sent to the CLI panel. */
    NvOdmDispCliInstruction Instructions[NVODM_DISP_CLI_MAX_INSTRUCTIONS];

    /** Specifies the number of instructions per frame. */
    NvU32 nInstructions;

    /** Specifies zero or more NvOdmDispCliFlags. */
    NvU32 flags;
} NvOdmDispCliConfig;

/**
 * DSI Configuration Types.
 */

/** Defines DSI video mode operation types. */
typedef enum
{
    NvOdmDsiVideoModeType_Burst = 1,
    NvOdmDsiVideoModeType_NonBurst,
    NvOdmDsiVideoModeType_NonBurstSyncEnd,
    NvOdmDsiVideoModeType_DcDrivenCommandMode,

    NvOdmDsiVideoModeType_Force32 = 0x7FFFFFFF
} NvOdmDsiVideoModeType;

/** Defines the DSI data formats. */
typedef enum
{
    NvOdmDsiDataFormat_16BitP = 1,
    NvOdmDsiDataFormat_18BitNP,
    NvOdmDsiDataFormat_18BitP,
    NvOdmDsiDataFormat_24BitP,

    NvOdmDsiDataFormat_Force32 = 0x7FFFFFFF
} NvOdmDsiDataFormat;

/** Defines the DSI read response. */
typedef enum
{
    NvOdmDsiReadResponse_Success = 1,
    NvOdmDsiReadResponse_ReadAckError,
    NvOdmDsiReadResponse_InvalidResponse,
    NvOdmDsiReadResponse_NoReadResponse,

    NvOdmDsiReadResponse_Force32 = 0x7FFFFFFF
} NvOdmDsiReadResponse;


/** Defines the DSI virtual channel numbers. */
typedef enum
{
    NvOdmDsiVirtualChannel_0 = 0,
    NvOdmDsiVirtualChannel_1 = 1,
    NvOdmDsiVirtualChannel_2 = 2,
    NvOdmDsiVirtualChannel_3 = 3,

    NvOdmDsiVirtualChannel_Force32 = 0x7FFFFFFF
} NvOdmDsiVirtualChannel;

/** Defines high speed (HS) clock control. */
typedef enum
{
    /** Specifies HS clock lane is always enabled. */
    NvOdmDsiHsClock_Continuous = 0,
    /** Specifies HS clock lane is enabled only during HS transmissions. */
    NvOdmDsiHsClock_TxOnly = 1,
    /**
     * Specifies HS clock lane is always enabled + workaround (WAR)
     * for \a tHSPREPR: before the DSI clock is enabled, \a tHSPREPR is set to
     * a big value (max(WarMinThsprepr, 300nS)) on the first cycle so that
     * the receiver won't get any clock transitions on its \c clk_settle cycle.
     */
    NvOdmDsiHsClock_Continuous_Thsprepr_WAR = 2,
    /**
     * Specifies HS clock lane is always enabled + WAR (work around)
     * for \a tHSPREPR using PLLD: before the DSI clock is enable, set 
     * \a tHSPREPR to 0 and PLLD to (min(WarMaxInitDsiClkKHz, 1.2MHz)) so that
     * the receiver won't get any clock transitions on 
     * (min(WarMaxInitDsiClkKHz, 1.2MHz))/2
     */
    NvOdmDsiHsClock_Continuous_Thsprepr_PLL_WAR = 3,

    NvOdmDsiHsClock_Force32 = 0x7FFFFFFF
} NvOdmDsiHsClock;

/**
 * Defines the DSI phy timing perameters.
 * - Tbyte = Period of one byte clock in nanoseconds.
 * - Tbit = Period of one bit time in nanoseconds. ( Tbit = Tbyte / 8 ).
 */
typedef struct NvOdmDispDsiPhyTimingRec
{
    /** 120 / Tbyte */
    NvU32 ThsExit;
    /** 3  + max(8 Tbit, 60 + 4 Tbit) / Tbyte */
    NvU32 ThsTrail;
    /** (145 + (5 Tbit)) / Tbyte */
    NvU32 TdatZero;
    /** (65 + (5 Tbit)) / Tbyte */
    NvU32 ThsPrepr;

    /** 80 / Tbyte */
    NvU32 TclkTrail;
    /** (70 + (52 Tbit)) / Tbyte */
    NvU32 TclkPost;
    /** 260 / Tbyte */
    NvU32 TclkZero;
    /** 60 / Tbyte */
    NvU32 Ttlpx;
    /** Not Programmed in default */
    NvU32 TclkPrepr;
    /** Not Programmed in default */
    NvU32 TclkPre;
} NvOdmDispDsiPhyTiming;

/**
 * Defines extended configuration DSI devices.
 */
typedef struct NvOdmDispDsiConfigRec
{
    /** Holds the DSI pixel data format. */
    NvOdmDsiDataFormat DataFormat;

    /** Holds the video mode for data transfers. */
    NvOdmDsiVideoModeType VideoMode;

    /** Holds the virtual number to use. */
    NvOdmDsiVirtualChannel VirChannel;

    /** Holds the number of data lines supported by the panel. */
    NvU32 NumOfDataLines;

    /** Holds the panel refresh rate. */
    NvU32 RefreshRate;

    /**
     * Holds the D-PHY frequency in kHz.
     * If \a Freq is set to 0:
     * - For video mode, \a Freq will be recalculated using DC freqency.
     * - For command mode, \a Freq will be assigned to
     *      ::NVODM_DISP_DSI_COMMAND_MODE_FREQ
     * */
    NvU32 Freq;
#define NVODM_DISP_DSI_COMMAND_MODE_FREQ    (50000)

    /** Holds the panel reset duration in milliseconds. */
    NvU32 PanelResetTimeoutMSec;

    /**
     * Holds the D-PHY frequency in kHz.
     * Frequency configured in command mode.
     * If frequency is 0, frequency is assigned to
     *      ::NVODM_DISP_DSI_COMMAND_MODE_FREQ
     */
    NvU32 LpCommandModeFreqKHz;

    /** Holds the high-speed command mode frequency. */
    NvU32 HsCommandModeFreqKHz;

    /**
     * Holds a flag for high-speed command mode support
     * for frame buffer data transfer.
     */
    NvBool HsSupportForFrameBuffer;

    /** Holds the control value of the high-speed clock lane. */
    NvOdmDsiHsClock HsClockControl;

    /**
     * Minimum tHSPREPR (in nS) for its WAR. Valid only if \a HsClockControl
     * is set to ::NvOdmDsiHsClock_Continuous_Thsprepr_WAR.
     */
    NvU32 WarMinThsprepr;

    /**
     * Max DSI clock (in kHz) when transitioning from LP to HS. Valid only if
     * ::HsClockControl is set to
     * ::NvOdmDsiHsClock_Continuous_Thsprepr_PLLD_WAR.
     */
    NvU32 WarMaxInitDsiClkKHz;

    /**
     * Flag to indicate if HS clock needs to be ON in LP mode.
     * If NV_TRUE, enables HS clock in LP mode i.e., while sending commands
     * to the panel in LP mode.
     */
    NvBool EnableHsClockInLpMode;

    /**
     * Flag to indicate whether to use the phy timings supplied by the ODM
     * or use the default phy timings calculated by the driver.
     * If NV_TRUE, phy timing values supplied by the panel specific ODM are
     * programmed. If NV_FALSE, default phy timings calculated by the driver
     * are programmed.
     */
    NvBool bIsPhytimingsValid;

    /** Holds DSI phy timing perameters. */
    NvOdmDispDsiPhyTiming PhyTiming;

   /**
    * Display mode in which the panel is operating
    */
    NvOdmDispDsiMode DsiDisplayMode;
    NvOdmDispGangedType GangedType;

    /** Reserved. */
    NvU32 flags;
} NvOdmDispDsiConfig;

/**
 * DSI Transport function pointer table. DSI adapatations should use this
 * to read/write to the DSI panel. This table will be given to the adaptation
 * at initialization time via ::NvOdmDispSetSpecialFunction.
 */
typedef struct NvOdmDispDsiTransportRec
{
    /**
     * Initializes the DSI transport.
     */
    NvBool (*NvOdmDispDsiInit)( NvOdmDispDeviceHandle hDevice, NvU32 DsiInstance, NvU32 flags );

    /**
     * Deinitializes the DSI transport.
     */
    void (*NvOdmDispDsiDeinit)( NvOdmDispDeviceHandle hDevice, NvU32 DsiInstance );

    /**
     * Writes data to the panel via DSI controller.
     *
     * @param hDevice The handle to the display device.
     * @param DataType The DSI packet data type.
     * @param RegAddr Register address in the panel.
     * @param DataSize Data size that needs to be transmitted.
     * @param pData A pointer to the panel data.
     * @param bLongPacket If NV_TRUE, data is in long packet format.
     *      If NV_FALSE, data is in short packet format.
     *
     * @return NV_TRUE if successful, or NV_FALSE otherwise.
     */
    NvBool (*NvOdmDispDsiWrite)( NvOdmDispDeviceHandle hDevice,
        NvU32 DataType, NvU32 RegAddr, NvU32 DataSize, NvU8 *pData,
        NvU32 DsiInstance, NvBool bLongPacket );

    /**
     * Reads data from the panel via DSI controller.
     *
     * @param hDevice The handle to the display device.
     * @param RegAddr Register address in the panel.
     * @param DataSize Data size that needs to be recieved.
     * @param pData A pointer to the panel data to be stored.
     * @param bLongPacket If NV_TRUE, data is in long packet format.
     *      If NV_FALSE, data is in short packet format.
     *
     * @return NV_TRUE if successful, or NV_FALSE otherwise.
     */
    NvBool (*NvOdmDispDsiRead)( NvOdmDispDeviceHandle hDevice,
        NvU8 RegAddr, NvU32 DataSize, NvU8 *pData,
        NvU32 DsiInstance, NvBool bLongPacket );

    /**
     * Configures the DSI controller for command mode. This function
     * also sends the required commands to the panel for entering into
     * the partial mode based on the requested mode, i.e., video mode or
     * partial update mode.
     *
     * This API must be called before writing/reading data from the
     * panel, so that the controller is configured in command mode.
     *
     * @param hDevice The handle to the display device.
     * @param bPartialMode  If NV_TRUE, configures the DSI controller in
     * command mode and sends the required commands to the panel for entering
     * the partial mode. If NV_FALSE, configures the DSI controller in command
     * mode and quits.
     *
     * @return NV_TRUE if successful, or NV_FALSE otherwise.
     */
    NvBool (*NvOdmDispDsiEnableCommandMode)( NvOdmDispDeviceHandle hDevice, NvU32 DsiInstance );

    /**
     * Writes the given surface into the display device's internal memory.
     * The device must be in command or partial mode.
     *
     * @param hDevice The handle to the display device.
     * @param pSurface A pointer to the source surface pixel data.
     * @param Width Surface width in bytes.
     * @param Height Surface height in lines.
     * @param pSrc A pointer into the source surface.
     * @param pUpdate Rectangular region on the display to update from the
     *      source surface.
     * @param bpp Bits per pixel.
     */
    NvBool (*NvOdmDispDsiUpdate)( NvOdmDispDeviceHandle hDevice,
        NvU8 *pSurface, NvU32 Width, NvU32 Height, NvU32 bpp, NvPoint *pSrc,
        NvU32 DsiInstance, NvRect *pUpdate );

    /**
     * Gets the current DSI phy frequency in kHz.
     *
     * @param hDevice The handle to the display device.
     * @param pDsiPhyFreqKHz A pointer to the current DSI phy frequency in kHz.
     */
    NvBool (*NvOdmDispDsiGetPhyFreq)( NvOdmDispDeviceHandle hDevice,
        NvU32 DsiInstance, NvU32 *pDsiPhyFreqKHz );

    /**
     * Reads data from the panel register and restores the mode of operation
     * of the DSI controller.
     *
     * This function reads the specified panel register and restores the
     * mode (i.e., command or video mode) of the controller.
     * For example, if this function is called while display is on in the video
     * mode, it reads the panel register and restores back the video mode
     * display.
     *
     * This function reads and parses the read response from the panel.
     * If the read response is Long Read, Short Read, or Error Acknowledgement,
     * it copies the payload of the response to the read buffer.
     * If the read response is in unspecified format, it copies the whole read
     * response to the read buffer.
     * The response from the panel is stored in the read response pointer \a
     * pReadResp. @sa NvOdmDsiReadResponse
     *
     * @param hDevice The handle to the display device.
     * @param RegAddr Register address in the panel.
     * @param DataSize Data size that needs to be received.
     * @param pData A pointer to the panel data to be stored.
     * @param pReadResp A pointer to the read response from the panel.
     * @param flags Reserved, must be 0x0.
     *
     * @return NV_TRUE if successful, or NV_FALSE otherwise.
     */
    NvBool (*NvOdmDispDsiPing)( NvOdmDispDeviceHandle hDevice,
        NvU8 RegAddr, NvU32 DataSize, NvU8 *pData, NvU32 *pReadResp,
        NvU32 DsiInstance, NvU32 flags );

} NvOdmDispDsiTransport;

/**
 * Holds the customize termination and drive-strength for HDMI TMDS.
 */
typedef struct NvOdmDispHdmiDriveTermRec
{
    /// Enables/disables TMDS termination.
    NvBool  tmdsTermEn;

    /// Termination resistance control. @note \a tmdsTermEn must be enabled
    /// for this field to take affect.
    NvU8    tmdsTermAdj;

    /// Load pulse position adjustment.
    NvU8    tmdsLoadAdj;

    /// Per lane I/O current. Start from 1.500mA, and each increment is 0.375 mA
    /// For example:
    /// <pre>
    ///     000000 --> 1.500 mA
    ///     000001 --> 1.875 mA
    ///     000010 --> 2.250 mA
    ///     ...
    ///     111111 --> 25.125mA
    /// </pre>
    NvU8    tmdsCurrentLane0;
    NvU8    tmdsCurrentLane1;
    NvU8    tmdsCurrentLane2;
    NvU8    tmdsCurrentLane3;
} NvOdmDispHdmiDriveTerm;

typedef struct NvOdmDispHdmiDrvTrmCdtRec
{
    NvS32       maxHeight;         /**< Excluded. */
    NvS32       minHeight;         /**< Included. */
    NvU32       maxPanelFreqKhz;   /**< Excluded. */
    NvU32       minPanelFreqKhz;   /**< Included. */
    NvBool      checkPreEmp;       /**< Set to TRUE to check \a preEmpesisOn. */
    NvBool      preEmpesisOn;
} NvOdmDispHdmiDrvTrmCdt;

typedef struct NvOdmDispHdmiDriveTermEntryRec
{
    NvOdmDispHdmiDrvTrmCdt  condition;
    NvOdmDispHdmiDriveTerm  data;
} NvOdmDispHdmiDriveTermEntry;

typedef struct NvOdmDispHdmiDriveTermTblRec
{
    NvU32                           numberOfEntry;
    NvOdmDispHdmiDriveTermEntry*    pEntry;
} NvOdmDispHdmiDriveTermTbl;

/**
 * Defines display device special features, such as positioning the
 * partial mode active area, etc. Special features are controlled/configured
 * by NvOdmDispSetSpecialFunction().
 */
typedef enum
{
    /**
     * Configures the partial mode runtime parameters. Configuration is
     * defined by \c NvOdmDispPartialModeConfig.
     */
    NvOdmDispSpecialFunction_PartialMode = 0x1,

    /**
     * The DSI Transport function table. See ::NvOdmDispDsiTransport.
     */
    NvOdmDispSpecialFunction_DSI_Transport,

    /**
     * Enables automatically adjusting display brightness based on ambient
     * sensor data, etc.
     */
    NvOdmDispSpecialFunction_AutoBacklight,

    NvOdmDispSpecialFunction_Force32 = 0x7FFFFFFF,
} NvOdmDispSpecialFunction;

typedef struct NvOdmDispPartialModeConfigRec
{
    /** Vertical offset from the top of the panel glass (assumes full width) */
    NvU32 LineOffset;
} NvOdmDispPartialModeConfig;

/**
 * Defines pins that may require special configuration.
 */
typedef enum
{
    NvOdmDispPin_DataEnable = 0x1,
    NvOdmDispPin_HorizontalSync,
    NvOdmDispPin_VerticalSync,
    NvOdmDispPin_PixelClock,

    NvOdmDispPin_Num,
    NvOdmDispPin_Force32 = 0x7FFFFFFF,
} NvOdmDispPin;

/**
 * Defines pin polarity. All pins are active high by default, with the
 * exception of the pixel clock, which is active low.
 */
typedef enum
{
    NvOdmDispPinPolarity_High = 0x1,
    NvOdmDispPinPolarity_Low,
    NvOdmDispPinPolarity_Force32 = 0x7FFFFFFF,
} NvOdmDispPinPolarity;

/**
 *  Defines the display device power states.
 */
typedef enum
{
    /**
     * The display device has no power.
     */
    NvOdmDispDevicePowerLevel_Off = 0x1,

    /**
     * The display device is about to be powered on -- this is called before
     * the pixel clock is enabled.
     */
    NvOdmDispDevicePowerLevel_PrePower,

    /**
     * The display device is fully powered -- this is called after the pixel
     * clock is enabled.
     */
    NvOdmDispDevicePowerLevel_On,

    /**
     * Some panels support a low-power sleep mode.
     */
    NvOdmDispDevicePowerLevel_Suspend,

    /**
     * Smart panels may support a self-refresh mode where the display
     * controller is no longer sending pixel data.
     */
    NvOdmDispDevicePowerLevel_SelfRefresh,

    NvOdmDispDevicePowerLevel_Num,
    NvOdmDispDevicePowerLevel_Force32 = 0x7FFFFFFF,
} NvOdmDispDevicePowerLevel;

/**
 * See ::NvOdmDispParameter_BaseColorSize for details.
 */
typedef enum
{
    NvOdmDispBaseColorSize_111 = 1,
    NvOdmDispBaseColorSize_222,
    NvOdmDispBaseColorSize_333,
    NvOdmDispBaseColorSize_444,
    NvOdmDispBaseColorSize_555,
    NvOdmDispBaseColorSize_565,
    NvOdmDispBaseColorSize_666,
    NvOdmDispBaseColorSize_332,
    NvOdmDispBaseColorSize_888,

    NvOdmDispBaseColorSize_Force32 = 0x7FFFFFFF,
} NvOdmDispBaseColorSize;

typedef enum
{
    NvOdmDispDataAlignment_MSB = 1,
    NvOdmDispDataAlignment_LSB,

    NvOdmDispDataAlignment_Force32 = 0x7FFFFFFF,
} NvOdmDispDataAlignment;

/**
 * See ::NvOdmDispParameter_PinMap for details.
 */
typedef enum
{
    NvOdmDispPinMap_Dual_Rgb18_x_Cpu8 = 1,
    NvOdmDispPinMap_Dual_Rgb18_Spi4_x_Cpu8,
    NvOdmDispPinMap_Dual_Rgb18_x_Cpu9,
    NvOdmDispPinMap_Dual_Rgb18_Spi4_x_Cpu9_Shared,
    NvOdmDispPinMap_Dual_Rgb18_Spi4_x_Cpu9,
    NvOdmDispPinMap_Dual_Rgb24_x_Spi5,
    NvOdmDispPinMap_Dual_Rgb24_Spi5_x_Spi5_Shared,
    NvOdmDispPinMap_Dual_Rgb24_Spi5_x_Spi5,
    NvOdmDispPinMap_Dual_Cpu9_x_Cpu9,
    NvOdmDispPinMap_Dual_Cpu18_x_Cpu9,
    NvOdmDispPinMap_Single_Cpu9,
    NvOdmDispPinMap_Single_Rgb6_Spi4,
    NvOdmDispPinMap_Single_Rgb18_Spi4,
    NvOdmDispPinMap_Single_Rgb18,
    NvOdmDispPinMap_Single_Cpu18,
    NvOdmDispPinMap_Single_Cpu24,
    NvOdmDispPinMap_Single_Rgb24_Spi5,
    NvOdmDispPinMap_CRT,

    /* Reserved for engineering testing */
    NvOdmDispPinMap_Reserved0 = 0x70000000,
    NvOdmDispPinMap_Reserved1 = 0x70000001,

    NvOdmDispPinMap_Unspecified = 0x7FFFFFFE,
    NvOdmDispPinMap_Force32 = 0x7FFFFFFF,
} NvOdmDispPinMap;

/**
 * Defines for Display PWM output pin.
 *
 */
#define NVODM_DISP_GPIO_MAKE_PIN(port, pin) \
    (0x80000000 | (((NvU32)(port) & 0xff) << 8) | (((NvU32)(pin) & 0xff)))
#define NVODM_DISP_GPIO_GET_PORT(x) (((x) >> 8) & 0xff)
#define NVODM_DISP_GPIO_GET_PIN(x) ((x) & 0xff)
#define NvOdmDispPwmOutputPin_Unspecified 0
typedef NvU32 NvOdmDispPwmOutputPin;

/**
 * Defines the display device configuration, which is read-only from the
 * display controller software. All parameters are READ ONLY. Zero may be
 * returned for "don't care" values.
 */
typedef enum
{
    /**
     * Specifies the display device type; the parameter type is
     * ::NvOdmDispDeviceType.
     */
    NvOdmDispParameter_Type = 0x1,

    /**
     * Specifies the display device usage type -- one of
     * ::NvOdmDispDeviceUsageType.
     */
    NvOdmDispParameter_Usage,

    /**
     * Specifies the maximum horizontal resolution.
     */
    NvOdmDispParameter_MaxHorizontalResolution,

    /**
     * Specifies the maximum vertical resolution.
     */
    NvOdmDispParameter_MaxVerticalResolution,

    /**
     * The size of the color, in RGB bits, after dither.  Specified with
     * NvOdmDispBaseColorSize.
     */
    NvOdmDispParameter_BaseColorSize,

    /**
     * Pixel data alignment to the pins from the display controller.
     * Specified with NvOdmDispDataAlignment.
     */
    NvOdmDispParameter_DataAlignment,

    /**
     * Basic pin mapping configurations that fit most panels.
     * See the below list for an explanation of notation:
     *
     * - Some configurations will use dual panels, while others will use
     *   only a single panel. These are noted as either dual or single.
     * - Dual panel configurations are separated by "_x_" which means that
     *   the Main LCD is listed first, followed by "_x_" and then the
     *   sub-panel.
     * - "Smart" panels are denoted with "CPU" and a bus width. For example,
     *   a smart panel with a 9-bit data bus is denoted as @em CPU9.
     * - Framebuffer-less panels with VSYNC/HSYNC are denoted by "Rgb" and
     *   include the color depth. For example, an 18-bit RGB panel is shown as
     *   "Rgb18."
     * - Some panels use SPI instead of the control signals from the
     *   display controller. The number of SPI wires varies, but is specified
     *   in the list below. For example, a panel that uses 4-wire SPI
     *   is identified as Spi4.
     * - If any SPI signals are shared between main and sub panels, this is
     *   noted with shared.
     * - Some manufacturers also implement connections in a unique way, even
     *   if other configuration parameters (as noted above) are the same. For
     *   these panels, the panel manufacturer's name is included to
     *   differentiate it from the others.
     *
     * The PinMap parameter is specified with NvOdmDispPinMap.
     */
    NvOdmDispParameter_PinMap,

    /**
     * Color calibration panels can vary in the strength of the pixels
     * per channel, resulting in slightly off colors.
     * A panel can be calibrated by measuring the light spectrum of a
     * white screen displayed on the device under set lighting conditions,
     * for instance D65 light, and comparing against a blank piece of white
     * paper under the same light.
     * The color calibration parameters for red, green, and blue are each
     * a single Q16 (Signed 15.16 fixed point) value.
     * The macro NVODM_DISP_FP_TO_FX() is provided for your convenience.
     */
    NvOdmDispParameter_ColorCalibrationRed,
    NvOdmDispParameter_ColorCalibrationGreen,
    NvOdmDispParameter_ColorCalibrationBlue,
#define NVODM_DISP_FP_TO_FX(c) \
    ((NvU32)(c * 65536.0f + ((c<0.0f) ? -0.5f : 0.5f)))

    /**
     * Holds the display PWM output pin, specified with
     * ::NvOdmDispPwmOutputPin.
     */
    NvOdmDispParameter_PwmOutputPin,

    /**
     * Backlight brightness response curve. Most backlights have a non-linear
     * response for a given control input. Backlight response parameters
     * contain 4 points on the curve from 128 to 255. By default, the curve is
     * linear, the programming of which would look like:
     * <pre>
     *
     * for( i = 0; i < 4; i++ )
     * {
     *     base = 128 + (i * 32);
     *     // the 'p' array are the backlight response parameters
     *     p[i] = base
     *          | ((base + 8) << 8)
     *          | ((base + 16) << 16)
     *          | ((base + 24) << 24);
     * }
     *
     * </pre>
     */
    NvOdmDispParameter_BacklightResponse_0,
    NvOdmDispParameter_BacklightResponse_1,
    NvOdmDispParameter_BacklightResponse_2,
    NvOdmDispParameter_BacklightResponse_3,

    /**
     * Holds the dsi controller instance to
     * which the dsi display device is attached.
     */
    NvOdmDispParameter_DsiInstance,

    /* Display mode in which panel is operating */
    NvOdmDispParameter_DsiMode,

    /**
     * Specifies the initial backlight intensity to turn on.
     * The range is from 1 to 255.
     */
    NvOdmDispParameter_BacklightInitialIntensity,

    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvOdmDispParameter_Force32 = 0x7FFFFFFF
} NvOdmDispParameter;

/**
 * Gets a list of all display devices in the system.
 *
 * This may be called twice, once to get the number of displays, then again
 * to fill in the display handles.
 *
 * @param nDevices A pointer to the number of display handles (\c num) in the
 *      given array.
 * @param phDevices An array of display device handles of \c num length. If
 *      \em *nDevices is 0, then \em *nDevices is populated with the number
 *      of display devices.
 */
void
NvOdmDispListDevices( NvU32 *nDevices, NvOdmDispDeviceHandle *phDevices );

/**
 * Gets the list of modes for the specified display device.
 *
 * @param hDevice The handle to the display device.
 * @param nModes A pointer to the number of modes.
 * @param modes A pointer to the returned modes array.
 */
void
NvOdmDispListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

/**
 * Releases the display devices.
 *
 * @param nDevices The number of display devices to release.
 * @param hDevices A pointer to the array of display device handles.
 */
void
NvOdmDispReleaseDevices( NvU32 nDevices, NvOdmDispDeviceHandle *hDevices );

/**
 * Gets the default device GUID (globally unique identifier).
 *
 * @param guid [out] A pointer to the default guid
 *
 * If a display device is looked up via GUID 0, then this function will
 * be used to determine the actual GUID.
 */
void
NvOdmDispGetDefaultGuid( NvU64 *guid );

/**
 * Gets a display device based on its globally unique identifier (GUID)
 * from the peripheral datatbase.
 *
 * @param guid The device's GUID.
 * @param phDevice [out] A pointer to the device handle.
 */
NvBool
NvOdmDispGetDeviceByGuid( NvU64 guid, NvOdmDispDeviceHandle *phDevice );

/**
 * Sets the mode for the display device.
 *
 * Device initialization is expected to occur the first time this is called
 * with a non-null mode. See the flags below.
 *
 * @param hDevice The handle of the display device.
 * @param mode A pointer to the mode to set.
 * @param flags Zero or more flags
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 *
 * Flags:
 * NVODM_DISP_MODE_FLAGS_REOPEN -- this will be set if a bootloader has already
 *      initialized the device. The mode should still be set, but any device
 *      initialization should be skipped to avoid visual artifacts.
 *      NvOdmDispSetMode will be called before NvOdmDispSetPowerLevel.
 *      NvOdmDispSetPowerLevel should not reinit the display device if this
 *      flag has been set. Subsequent suspend and resume calls should work
 *      normally.
 */
NvBool
NvOdmDispSetMode( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode,
    NvU32 flags );

#define NVODM_DISP_MODE_FLAGS_NONE      (0x0)
#define NVODM_DISP_MODE_FLAGS_REOPEN    (0x1)

/**
 * Sets the backlight intensity, for display devices that have their
 * own backlight controller.
 *
 * This may be ignored if the backlight is driven off of an external PWM
 * signal, etc.
 *
 * @param hDevice The handle of the display device.
 * @param intensity 0 for off, 255 for full brightness.
 */
void
NvOdmDispSetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

/**
 * @brief Sets the desired power level for the display device.
 *
 * @param hDevice The handle of the display device.
 * @param level The target power level to set.
 */
void
NvOdmDispSetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

/**
 * Gets the specified display device parameter.
 *
 * @param hDevice The handle of the display device.
 * @param param  Specifies which parameter value to get.
 * @param val  A pointer to the returned parameter.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmDispGetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

/**
 * Gets the extended configuration structure.
 *
 * @param hDevice The handle of the display device.
 * @return A pointer to the returned device extended configuration
 *  structure.
 */
void *
NvOdmDispGetConfiguration( NvOdmDispDeviceHandle hDevice );

/**
 * Gets the pin polarity configuration.
 *
 * This may set \em nPins to 0 to use the default settings.
 *
 * Each pin and value should be copied out. \em nPins should be set to the
 * number of pins written and shall not exceed ::NvOdmDispPin_Num.
 *
 * @param hDevice The handle of the display device.
 * @param nPins The number of pins to configure.
 * @param pins An array of pin IDs.
 * @param vals The polarity values.
 */
void
NvOdmDispGetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *pins, NvOdmDispPinPolarity *vals );

/**
 * Sets the given special function on the display device.
 *
 * @param hDevice The handle of the display device.
 * @param function The function type.
 * @param config A pointer to the function configuration.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmDispSetSpecialFunction(
    NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function,
    void *config );

//Initialise the Bridge to convert DSI to
//Lvds signals
void InitDsi2LvdsBridge(NvOdmDispGangedType GangedType);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_DISP_H
