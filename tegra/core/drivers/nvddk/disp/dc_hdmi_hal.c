/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

//#undef NV_ENABLE_DEBUG_PRINTS
//#define NV_ENABLE_DEBUG_PRINTS 1

#include "nvassert.h"
#include "dc_hdmi_hal.h"
#include "nvddk_disp_hw.h"
#include "nvrm_drf.h"
#include "nvrm_pinmux.h"
#include "nvrm_channel.h"
#include "nvrm_memmgr.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "t30/class_ids.h"
#include "t30/arapb_misc.h"
#include "nvfxmath.h"

/* HDMI driver: see the hdmi spec at
 * \\netapp-hq\archive\specs\HDMI\HDMI_Specification_1.1.pdf
 */

/* for running on simulation (rtl), set
 *      NV_CFG_CHIPLIB_ARGS="-c sockchip_top_axitrans.cfg -- +v top_axitrans"
 * or
 *      NV_CFG_CHIPLIB_ARGS="-c sockfsim.cfg -- +v display +v hdmi"
 */

#define BUG_409279_HDCP_GLITCH 1

static DcHdmi s_Hdmi;

static NvOdmDispDeviceMode default_mode[] =
{
    // 1920x1080p 59.94/60hz (Format 16)
    { 1920, // width
      1080, // height
      24, // bpp
      0, // refresh
      148500, // frequency
      NVDDK_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        44, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        88, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    }
};

/* Commenting it for now to avoid tmake build errors.
 * we might need this later hence not deleting it
 */
#if 0
static NvOdmDispDeviceMode hdmi_modes[] =
{
    // 1280x720p 60hz: EIA/CEA-861-B Format 4
    { 1280, // width
      720, // height
      24, // bpp
      0, // refresh
      74250, // frequency in khz
      NVDDK_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        40, // h_sync_width
        5, // v_sync_width
        220, // h_back_porch
        20, // v_back_porch
        1280, // h_disp_active
        720, // v_disp_active
        110, // h_front_porch
        5, // v_front_porch
      },
      NV_FALSE // partial
    },

    // 720x480p 59.94hz: EIA/CEA-861-B Formats 2 & 3
    { 720, // width
      480, // height
      24, // bpp
      0, // refresh
      27000, // frequency
      NVDDK_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        62, // h_sync_width
        6, // v_sync_width
        60, // h_back_porch
        30, // v_back_porch
        720, // h_disp_active
        480, // v_disp_active
        16, // h_front_porch
        9, // v_front_porch
      },
      NV_FALSE // partial
    },

    // 640x480p 60hz: EIA/CEA-861-B Format 1
    { 640, // width
      480, // height
      24, // bpp
      0, // refresh
      25200, // frequency
      NVDDK_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        96, // h_sync_width
        2, // v_sync_width
        48, // h_back_porch
        33, // v_back_porch
        640, // h_disp_active
        480, // v_disp_active
        16, // h_front_porch
        10, // v_front_porch
      },
      NV_FALSE // partial
    },

    // 720x576p 50hz EIA/CEA-861-B Formats 17 & 18
    { 720, // width
      576, // height
      24, // bpp
      (50 << 16), // refresh
      27000, // frequency
      NVDDK_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        64, // h_sync_width
        5, // v_sync_width
        68, // h_back_porch
        39, // v_back_porch
        720, // h_disp_active
        576, // v_disp_active
        12, // h_front_porch
        5, // v_front_porch
      },
      NV_FALSE // partial
    },

    // 1920x1080p 59.94/60hz (Format 16)
    { 1920, // width
      1080, // height
      24, // bpp
      0, // refresh
      148500, // frequency
      NVDDK_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        44, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        88, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    },
};
#endif
/* get the modes from the display device itself */
static NvOdmDispDeviceMode edid_modes[NVDDK_DISP_MAX_MODES];
static NvU32 edid_modes_num;

/* this script will generate the N and CTS for a given audio frequency, Fs,
 * and pixel clock, Fpix. This is useful since the actual pixel clock may not
 * be exactly what the hdmi spec asks for.
 *
#!/usr/bin/env perl

my $best_n = 0;
my $best_cts = 0;

my $Fs = 44100;
#my $Fs = 32000;
#my $Fs = 48000;

my $Fpix = 25250000;  # actual for 25.2 640x480@60

my $min_n = int(128*$Fs / 1500);
my $max_n = int(128*$Fs / 300);

my $ideal_n = int(128*$Fs / 1000);

my $min_err = 100.0;
for (my $n = $min_n; $n <= $max_n; $n += 1) {
    my $cts_f = $Fpix * $n / (128.0*$Fs);
    my $cts = int($cts_f + 0.5);
    my $err = abs($cts_f - $cts);
    if ( $err < $min_err ||
       (($err == $min_err) &&
            (abs($n - $ideal_n) < abs($best_n - $ideal_n)))) {
        $min_err = $err;
        $best_n = $n;
        $best_cts = $cts;
    }
}

print "best CTS $best_cts, N $best_n  (err $min_err)\n";

$best_cts == ($Fpix * $best_n) / (128 * $Fs) or die "inexact result - beware";

exit 0;
 *
 */

/* hdmi audio frequency table. see section 7.2.3 in the hdmi 1.1 spec. */
typedef struct HdmiAudioFreqRec
{
    NvU32 PixelClk; // pixel clock freq in kHz
    /* see section 7.2 for discussion on N and CTS */
    NvU32 N;
    NvU32 CTS;
} HdmiAudioFreq;

static HdmiAudioFreq s_AudioFreqTable_32[] =
{
    { 25200, 4096, 25250 }, // actual pixel clock is 25.25, adjust cts
    { 27000, 4096, 27000 },
    { 54000, 4096, 54000 },
    // The following is for 1366x768 @ 60Hz, VESA mode resolution. It is not
    // HDMI resolution. These clocks are calculated by hand.
    { 72000, 4096, 72000 },
    { 74250, 4096, 74250 },
    { 148500, 4096, 148500 },
    { 0 },
};

static HdmiAudioFreq s_AudioFreqTable_441[] =
{
    { 25200, 14112, 63125 }, // actual pixel clock is 25.25, adjust cts and n
    { 27000, 6272, 30000 },
    { 54000, 6272, 60000 },
    // The following is for 1366x768 @ 60Hz, VESA mode resolution. It is not
    // HDMI resolution. These clocks are calculated by hand.
    { 72000, 6272, 80000 },
    { 74250, 6272, 82500 },
    { 148500, 6272, 165000 },
    { 0 },
};

static HdmiAudioFreq s_AudioFreqTable_48[] =
{
    { 25200, 6144, 25250 }, // actual pixel clock is 25.25, adjust cts
    { 27000, 6144, 27000 },
    { 54000, 6144, 54000 },
    // The following is for 1366x768 @ 60Hz, VESA mode resolution. It is not
    // HDMI resolution. These clocks are calculated by hand.
    { 72000, 6144, 72000 },
    { 74250, 6144, 74250 },
    { 148500, 6144, 148500 },
    { 0 },
};

static void
hdmi_decoder_write( DcHdmi *hdmi, NvU32 addr, NvU32 data )
{
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_EMU1_0, data );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_EMU0_0, addr | HD_WRITE_BIT );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_EMU0_0, HD_SAFE_ADDR );
}

static HdmiAudioFreq *
DcHdmiGetAudioFreq( NvU32 sample, NvU32 pixel )
{
    HdmiAudioFreq *f;

    switch( sample ) {
    case NvDdkDispAudioFrequency_32000hz:
        f = &s_AudioFreqTable_32[0];
        break;
    case NvDdkDispAudioFrequency_44100hz:
        f = &s_AudioFreqTable_441[0];
        break;
    case NvDdkDispAudioFrequency_48000hz:
        f = &s_AudioFreqTable_48[0];
        break;
    default:
        // default to a slow frequency
        return &s_AudioFreqTable_441[0];
    }

    while( f->PixelClk )
    {
        if( f->PixelClk == pixel )
        {
            return f;
        }

        f++;
    }

    /* fallback to the default frequency */
    return &s_AudioFreqTable_441[0];
}

static void
DcHdmiAudioConfig( DcHdmi *hdmi, NvU32 PixelClk,
    NvDdkDispAudioFrequency AudioClk )
{
    NvU32 val;
    NvU32 audio_n;
    HdmiAudioFreq *freq;
    NvU32 delta;
    NvU32 f, i;
    NvU32 freqs[] =
        {
            32000,
            44100,
            48000,
            88200,
            96000,
            176400,
            192000,
        };

    val = HDMI_READ( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0);
    if(AudioClk == NvDdkDispAudioFrequency_00000hz ||
        hdmi->bDvi == NV_TRUE)
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_GENERIC_CTRL, AUDIO, DIS,val );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0, val );
        return;
    }

    val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_GENERIC_CTRL, AUDIO, EN,val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0, val );

    val = NV_DRF_NUM( HDMI, NV_PDISP_AUDIO_CNTRL0, ERROR_TOLERANCE, 6 )
        | NV_DRF_NUM( HDMI, NV_PDISP_AUDIO_CNTRL0, FRAMES_PER_BLOCK, 0xc0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_AUDIO_CNTRL0_0, val );

    /* use eco procedure from gpu bug 179142 */

    freq = DcHdmiGetAudioFreq( AudioClk, PixelClk );
    NV_ASSERT( freq );

    /* clear acr control */
    val = 0;
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_ACR_CTRL_0, val );

    /* set N counter */
    audio_n =
          NV_DRF_DEF( HDMI, NV_PDISP_AUDIO_N, LOOKUP, DISABLE )
        | NV_DRF_DEF( HDMI, NV_PDISP_AUDIO_N, RESETF, ASSERT )
        | NV_DRF_DEF( HDMI, NV_PDISP_AUDIO_N, GENERATE, ALTERNATE )
        | NV_DRF_NUM( HDMI, NV_PDISP_AUDIO_N, VALUE, freq->N - 1 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_AUDIO_N_0, audio_n );

    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH,
            SB6, freq->N & 0xff )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH,
            SB5, ( freq->N >> 8 ) & 0xff )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH,
            SB4, ( freq->N >> 16 ) & 0xf )
        | NV_DRF_DEF( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH, ENABLE, YES );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_ACR_0441_SUBPACK_HIGH_0, val );

    /* ap16 fixed the cts stuff, need to program the cts into subpack low. this
     * has no effect on ap15.
     */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_LOW,
            SB1, (freq->CTS >> 16) & 0xff )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_LOW,
            SB2, (freq->CTS >> 8) & 0xff )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_ACR_0441_SUBPACK_LOW,
            SB3, freq->CTS & 0xff );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_ACR_0441_SUBPACK_LOW_0, val );

    /* eco stuff, hw_cts = 1, cts_reset_val = 1, force cts mode (bit 1) */
    // FIXME: bit 31 might need to be set for audio priority
    val = 1 | (1 << 1) | (1 << 16);
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_SPARE_0, val );

    /* deassert reset */
    audio_n = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_AUDIO_N, RESETF,
        DEASSERT, audio_n );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_AUDIO_N_0, audio_n );

    /* setup audio sample stuff -- see hdmi_programming_guide.doc */
    for( i = 0; i < NV_ARRAY_SIZE(freqs); i++ )
    {
        #define HDMI_AUDIOCLK_FREQ 216000000

        NvU32 eight_half;
        NvU32 low;
        NvU32 high;

        f = freqs[i];
        if( f > 96000 )
        {
            delta = 2;
        }
        else if( f > 48000 )
        {
            delta = 6;
        }
        else
        {
            delta = 9;
        }

        eight_half = (8 * HDMI_AUDIOCLK_FREQ) / (f * 128);
        low = eight_half - delta;
        high = eight_half + delta;

        /* use fs1 for layout, use fs1 as an array base */
        val = NV_DRF_NUM( HDMI, NV_PDISP_AUDIO_FS1, LOW, low )
            | NV_DRF_NUM( HDMI, NV_PDISP_AUDIO_FS1, HIGH, high );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_AUDIO_FS1_0 + i, val );
    }
}

static NvOdmDispHdmiDriveTerm*
DcHdmiFindOdmTmds( NvOdmDispHdmiDriveTermTbl* tmds_drv_trm,
    NvRmFreqKHz cur_panel_freq, NvS32 cur_height, NvBool cur_pre_emp_on )
{
    NvOdmDispHdmiDriveTerm* odm_tmds_data;
    NvU32                   i;

    odm_tmds_data = NULL;

    if ( NULL == tmds_drv_trm )
        goto done;

    for ( i = 0; i < tmds_drv_trm->numberOfEntry; i++ )
    {
        NvOdmDispHdmiDriveTermEntry* cur_entry;
        cur_entry = &(tmds_drv_trm->pEntry[i]);

        if ( ( cur_entry->condition.maxPanelFreqKhz > cur_panel_freq ) &&
             ( cur_panel_freq >= cur_entry->condition.minPanelFreqKhz ) &&
             ( cur_entry->condition.maxHeight > cur_height ) &&
             ( cur_height >= cur_entry->condition.minHeight ) &&
             ( ( cur_entry->condition.checkPreEmp == NV_FALSE ) ||
               ( cur_entry->condition.preEmpesisOn == cur_pre_emp_on ) ) )
        {
            odm_tmds_data = &(cur_entry->data);
            break;
        }
    }

done:
    return odm_tmds_data;
}

static void
DcHdmiSorEnable( DcController *cont, DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 dc0 = 0xa;
    NvU32 dc1 = 0xa;
    NvU32 dc2 = 0xa;
    NvU32 dc3 = 0xa;
    NvBool pre_emp_on;
    NvDdkDispCapabilities *caps = cont->caps;
    NvOdmDispHdmiDriveTerm *drv_trm_data;

    pre_emp_on = NVDDK_DISP_CAP_IS_SET( caps,
        NvDdkDispCapBit_Hdmi_PreEmphasis );

    drv_trm_data = DcHdmiFindOdmTmds( hdmi->adaptation->tmdsDriveTerm,
        cont->panel_freq, cont->mode.height, pre_emp_on );

    if( drv_trm_data )
    {
        dc0 = drv_trm_data->tmdsCurrentLane0;
        dc1 = drv_trm_data->tmdsCurrentLane1;
        dc2 = drv_trm_data->tmdsCurrentLane2;
        dc3 = drv_trm_data->tmdsCurrentLane3;
    }
    else if( cont->mode.height == 1080 )
    {
        if( !pre_emp_on )
        {
            dc0 = 0x20;
            dc1 = 0x20;
            dc2 = 0x20;
            dc3 = 0x20;
        }
        else
        {
            dc0 = 0xc;
            dc1 = 0xc;
            dc2 = 0xc;
            dc3 = 0xc;
        }
    }

    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_LANE_DRIVE_CURRENT, LANE0, dc0 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_LANE_DRIVE_CURRENT, LANE1, dc1 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_LANE_DRIVE_CURRENT, LANE2, dc2 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_LANE_DRIVE_CURRENT, LANE3, dc3 );
#ifdef T30_CHIP
    // FIXME: fuses aren't burned yet in silicon, override some stuff
    val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_LANE_DRIVE_CURRENT,
        FUSE_OVERRIDE, TRUE, val );
#endif
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_LANE_DRIVE_CURRENT_0, val );

#ifndef T30_CHIP
    val = HDMI_READ(hdmi, HDMI_NV_PDISP_SOR_PAD_CTLS0_0);
    val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PAD_CTLS0,
        FUSE_OVERRIDE, TRUE, val );
    HDMI_WRITE(hdmi, HDMI_NV_PDISP_SOR_PAD_CTLS0_0, val);
#endif

    /* sor power up sequence */
    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_CTL, PU_PC, 0 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_CTL, PU_PC_ALT, 0 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_CTL, PD_PC, 8 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_CTL, PD_PC_ALT, 8 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_CTL, SWITCH, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_SEQ_CTL_0, val );

    #define SOR_SEQ_INST( num ) \
          NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_INST ## num, WAIT_TIME, 1 ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, WAIT_UNITS, VSYNC ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, HALT, TRUE ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, PIN_A, LOW ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, PIN_B, LOW ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, DRIVE_PWM_OUT_LO, \
            TRUE ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, TRISTATE_IOS,\
            ENABLE_PINS ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLACK_DATA, \
            NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLANK_DE, NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLANK_H, NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLANK_V, NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, ASSERT_PLL_RESETV,\
            NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, POWERDOWN_MACRO, \
            NORMAL )

    val = SOR_SEQ_INST( 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_SEQ_INST0_0, val );

    val = SOR_SEQ_INST( 8 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_SEQ_INST8_0, val );

    /* bug 350258, set rotclk to 2 */
    val = NV_RESETVAL( HDMI, NV_PDISP_SOR_CSTM );
    val = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_CSTM, ROTCLK, 2, val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_CSTM_0, val );

    #undef SOR_SEQ_INST
}

static void
DcHdmiSorStart( DcController *cont, DcHdmi *hdmi )
{
    NvU32 val;

    val = NV_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, NORMAL_STATE, PU )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_PWR, NORMAL_START, 0 )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, SAFE_STATE, PD )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, SETTING_NEW, TRIGGER );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_PWR_0, val );

    /* note: setting_new doesn't really work. just set it to DONE.
     * hw bug 399594
     */
    val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, SETTING_NEW, DONE,
        val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_PWR_0, val );

    do
    {
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_SOR_PWR_0 );
    } while( NV_DRF_VAL( HDMI, NV_PDISP_SOR_PWR, SETTING_NEW, val ) !=
        HDMI_NV_PDISP_SOR_PWR_0_SETTING_NEW_DONE );

    val = NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_CRCMODE,
            COMPLETE_RASTER )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_OWNER, HEAD0 )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_SUBOWNER, BOTH )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_PROTOCOL, SINGLE_TMDS_A )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_DEPOL, POSITIVE_TRUE );
    if( cont->mode.height == 720 || cont->mode.height == 1080 )
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_HSYNCPOL,
                POSITIVE_TRUE, val );
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_VSYNCPOL,
                POSITIVE_TRUE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_HSYNCPOL,
                NEGATIVE_TRUE, val );
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_STATE2, ASY_VSYNCPOL,
                NEGATIVE_TRUE, val );
    }
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE2_0, val );

    val = NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE1, ASY_HEAD_OPMODE,
            AWAKE )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE1, ASY_ORMODE, NORMAL )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE1, ATTACHED, 0 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE1, ARM_SHOW_VGA, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE1_0, val );

    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE0, UPDATE, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE0_0, val );

    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE0, UPDATE, 1 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE0_0, val );

    val = NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE1, ASY_HEAD_OPMODE,
            AWAKE )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE1, ASY_ORMODE, NORMAL )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE1, ATTACHED, 1 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE1, ARM_SHOW_VGA, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE1_0, val );

    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE0, UPDATE, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE0_0, val );
}

static void
DcHdmiSorStop( DcController *cont, DcHdmi *hdmi )
{
    NvU32 val;

    val = NV_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, NORMAL_STATE, PD )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_PWR, NORMAL_START, 0 )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, SAFE_STATE, PD )
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, SETTING_NEW, TRIGGER );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_PWR_0, val );

    /* note: hw bug 399594 */
    val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PWR, SETTING_NEW, DONE,
        val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_PWR_0, val );

    do
    {
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_SOR_PWR_0 );
    } while( NV_DRF_VAL( HDMI, NV_PDISP_SOR_PWR, SETTING_NEW, val ) !=
        HDMI_NV_PDISP_SOR_PWR_0_SETTING_NEW_DONE );

    #define SOR_SEQ_INST( num ) \
          NV_DRF_NUM( HDMI, NV_PDISP_SOR_SEQ_INST ## num, WAIT_TIME, 1 ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, WAIT_UNITS, VSYNC ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, HALT, TRUE ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, PIN_A, LOW ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, PIN_B, LOW ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, DRIVE_PWM_OUT_LO, \
            TRUE ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, TRISTATE_IOS,\
            TRISTATE ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLACK_DATA, \
            NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLANK_DE, NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLANK_H, NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, BLANK_V, NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, ASSERT_PLL_RESETV,\
            NORMAL ) \
        | NV_DRF_DEF( HDMI, NV_PDISP_SOR_SEQ_INST ## num, POWERDOWN_MACRO, \
            POWERDOWN )

    val = NV_DRF_DEF( HDMI, NV_PDISP_SOR_STATE1, ASY_ORMODE, SAFE )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE1, ATTACHED, 0 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE1, ARM_SHOW_VGA, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE1_0, val );

    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE0, UPDATE, 1 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE0_0, val );

    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_STATE0, UPDATE, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_STATE0_0, val );

    val = SOR_SEQ_INST( 8 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_SEQ_INST8_0, val );
}

static void
DcHdmiTmdsConfig( DcController *cont, DcHdmi *hdmi )
{
    NvU32 pll0;
    NvU32 pll1;
    NvU32 pe;
    NvU8 tmds_term_adj;
    NvBool tmds_term_en;
    NvBool pre_emp_on;
    NvDdkDispCapabilities *caps = cont->caps;
    NvOdmDispHdmiDriveTerm *drv_trm_data;

    pre_emp_on = NVDDK_DISP_CAP_IS_SET( caps,
        NvDdkDispCapBit_Hdmi_PreEmphasis );

    pll0 = NV_RESETVAL( HDMI, NV_PDISP_SOR_PLL0 );
    pll1 = NV_RESETVAL( HDMI, NV_PDISP_SOR_PLL1 );

    pll0 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL0, PWR, ON, pll0 );
    pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, VCOPD, 0, pll0 );
    pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, RESISTORSEL, 0x1,
        pll0 );
    pll0 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL0, PDBG, ON, pll0 );
    pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, PDPORT, 0, pll0 );

    if( pre_emp_on && ( cont->mode.height == 1080 ) )
    {
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, ICHPMP, 0x1,
            pll0 );
    }
    else
    {
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, ICHPMP, 0x2,
            pll0 );
    }

    pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, PULLDOWN, 0, pll0 );

    if( pre_emp_on )
    {
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, BG_V17_S, 0x3,
                pll0 );
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, TX_REG_LOAD, 0x3,
                pll0 );
    }

    if( cont->panel_freq <= 27000 )
    {
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, VCOCAP, 0x0,
            pll0 );
    }
    else if( cont->panel_freq <= 74250 )
    {
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, VCOCAP, 0x1,
            pll0 );
    }
    else
    {
        pll0 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL0, VCOCAP, 0x3,
            pll0 );
    }
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_PLL0_0, pll0 );

    drv_trm_data  = NULL;
    tmds_term_en  = NV_FALSE;
    tmds_term_adj = 0x00;

    drv_trm_data = DcHdmiFindOdmTmds( hdmi->adaptation->tmdsDriveTerm,
        cont->panel_freq, cont->mode.height, pre_emp_on );

    if( drv_trm_data )
    {
        tmds_term_en = drv_trm_data->tmdsTermEn;
        tmds_term_adj = drv_trm_data->tmdsTermAdj;
        pll1 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL1, LOADADJ,
            drv_trm_data->tmdsLoadAdj, pll1 );
    }
    else if( cont->mode.height == 1080 )
    {
        tmds_term_en = NV_TRUE;
    }

    if( tmds_term_en )
    {
        pll1 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL1, TMDS_TERM,
            ENABLE, pll1 );
    }
    else
    {
        pll1 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL1, TMDS_TERM,
            DISABLE, pll1 );
    }

    pll1 = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_SOR_PLL1, TMDS_TERMADJ,
        tmds_term_adj, pll1 );

    if( pre_emp_on )
    {
        /* enable preemphasis for 1080p */
        if( cont->mode.height == 1080 )
        {
            pll1 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL1, PE_EN, ENABLE,
                pll1 );
        }
        else
        {
            pll1 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL1, PE_EN, DISABLE,
                pll1 );
        }
        pll1 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL1, HALF_FULL_PE,
                HALF, pll1 );
        pll1 = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_SOR_PLL1, S_D_PIN_PE,
                SINGLE, pll1 );
    }
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_PLL1_0, pll1 );

    if( pre_emp_on && ( cont->mode.height == 1080 ) )
    {
        pe = NV_RESETVAL( HDMI, NV_PDISP_PE_CURRENT );
        pe = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_PE_CURRENT, PE_CURRENT0,
                0xf, pe );
        pe = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_PE_CURRENT, PE_CURRENT1,
                0xf, pe );
        pe = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_PE_CURRENT, PE_CURRENT2,
                0xf, pe );
        pe = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_PE_CURRENT, PE_CURRENT3,
                0xf, pe );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_PE_CURRENT_0, pe );
    }
}

static void
DcHdmiAviInfo( DcController *cont, DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 pb1, pb2, pb3, pb4, pb5, checksum;
    NvSFx ar;

    #define ASPECT_16_9 NV_SFX_FP_TO_FX_CT( 16.0f / 9.0f )
    #define ASPECT_4_3 NV_SFX_FP_TO_FX_CT( 4.0f / 3.0f )

    if( hdmi->bDvi )
    {
        val = NV_DRF_DEF( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_CTRL, ENABLE, NO );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_CTRL_0, val );
        return;
    }

    /* HB0: type
     * HB1: version
     * HB2: length
     */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_HEADER, HB0, 0x82 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_HEADER, HB1, 0x2 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_HEADER, HB2, 0xd );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_HEADER_0, val );

    /*
     * Setup the Packet Data Bytes
     * PB1: S:0, B:0, A:0, Y:0
     * PB2: R M C:0
     * PB3: SC=0
     * PB4: VIC=<format number from CEA-861-D spec>
     * PB5: PR=0
     *
     * S is pb1[1:0] as defined below:
     *   0,0 - no data
     *   0,1 - overscan
     *   1,0 - underscan
     *   1,1 - reserved
     *
     * M is aspect ratio in pb2[5:4] as defined:
     *   0,0 - no data
     *   0,1 - 4:3
     *   1,0 - 16:9
     *   1,1 - reserved
     *
     * R is active format in pb2[3:0] as defined:
     *   1,0,0,0 - same as picture
     *   1,0,0,1 - 4:3 (center)
     *   1,0,1,0 - 16:9 (center)
     *   1,0,1,1 - 14:9 (center)
     *   ignore rest
     */
    pb1 = 0; // overscan: 0x1, underscan 0x2
    pb2 = (1 << 3); // "same as picture"
    pb3 = 0;
    pb5 = 0;

    /* default to 16:9 aspect ratio */
    ar = ASPECT_16_9;

    switch( cont->mode.height ) {
    case 480:
        if( cont->mode.width == 640 )
        {
            ar = ASPECT_4_3;
            pb4 = 1;
        }
        else
        {
            pb4 = 3;
        }
        break;
    case 576:
    {
        /* need to look at the edid to get the aspect ratio. look at the
         * physical image size in a DTD (any should be do), figure out the
         * aspect ratio. default is 16:9
         */

        if( hdmi->edid && hdmi->edid->nDTDs )
        {
            NvSFx h, v, tmp;

            h = NV_SFX_WHOLE_TO_FX( hdmi->edid->DTD[0].HorizImageSize );
            v = NV_SFX_WHOLE_TO_FX( hdmi->edid->DTD[0].VertImageSize );

            tmp = NV_SFX_DIV( h, v );

            /* 16:9 is 1.7, 4:3 is 1.3. see if tmp is > 1.4 */
            if( NV_SFX_GT( tmp, NV_SFX_FP_TO_FX_CT( 1.4f ) ) )
            {
                ar = ASPECT_16_9;
            }
            else
            {
                ar = ASPECT_4_3;
            }
        }

        if( ar == ASPECT_16_9 )
        {
            pb4 = 18; // 16:9, 17 is 4:3
        }
        else
        {
            pb4 = 17;
        }

        break;
    }
    case 720:
        if( cont->mode.timing.Horiz_FrontPorch == 110 )
        {
            /* 60hz */
            pb4 = 4;
        }
        else
        {
            /* 50hz */
            pb4 = 19;
        }
        break;
    case 1080:
        if( cont->mode.timing.Horiz_FrontPorch == 88)
        {
            /* 60hz */
            pb4 = 16;
        }
        else if( cont->mode.timing.Horiz_FrontPorch == 528)
        {
            /* 50hz */
            pb4 = 31;
        }
        else
        {
            /* 24hz */
            pb4 = 32;
        }

        break;
    default:
        pb4 = 0;
        break;
    }

    if( ar == ASPECT_16_9 )
    {
        pb2 |= 2 << 4;
    }
    else
    {
        pb2 |= 1 << 4;
    }

    #undef ASPECT_16_9
    #undef ASPECT_4_3

    checksum = ((256 - (0xd + 0x2 + 0x82 + pb1 + pb2 + pb3 + pb4 + pb5))
        & 0xff) & 0xff;

    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_LOW, PB0,
            checksum )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_LOW, PB1,
            pb1 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_LOW, PB2,
            pb2 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_LOW, PB3,
            pb3 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_LOW_0, val );

    /* PB4-6 */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_HIGH, PB4,
            pb4 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_HIGH, PB5,
            pb5 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_HIGH, PB6,
            0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK0_HIGH_0, val );

    /* PB7-10 */
    val = 0;
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK1_LOW_0, val );

    /* PB11-13 */
    val = 0;
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_SUBPACK1_HIGH_0, val );

    val = NV_DRF_DEF( HDMI, NV_PDISP_HDMI_AVI_INFOFRAME_CTRL, ENABLE, EN );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AVI_INFOFRAME_CTRL_0, val );
}

static void
DcHdmiStereoInfo( DcController *cont, DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 pb1, pb2, pb3, pb4, pb5, checksum;

    if( !hdmi->edid || !hdmi->edid->bHas3dSupport ||
        !(cont->mode.flags & NVODM_DISP_MODE_FLAG_TYPE_3D) )
    {
        // disabling sending header to HDMI
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0 );
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_GENERIC_CTRL, ENABLE,
            DIS, val );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0, val );
        return;
    }

    /* HB0: type
     * HB1: version
     * HB2: length
     */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_HEADER, HB0, 0x81 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_HEADER, HB1, 0x01 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_HEADER, HB2, 0x05 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_HEADER_0, val );

    pb1 = 0x03;
    pb2 = 0x0c;
    pb3 = 0;
    pb4 = 0x02 << 5; // hdmi_video_format
    pb5 = 0; // 3d_structure

    checksum = ((256 - (pb1 + pb2 + pb3 + pb4 + pb5)) & 0xff) & 0xff;

    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_SUBPACK0_LOW, PB0,
                checksum )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_SUBPACK0_LOW, PB1,
                pb1 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_SUBPACK0_LOW, PB2,
                pb2 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_SUBPACK0_LOW, PB3,
                pb3 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK0_LOW_0, val );

    /* PB4-6 */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_SUBPACK0_HIGH, PB4, pb4 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_GENERIC_SUBPACK0_HIGH, PB5, pb5 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_SUBPACK0_HIGH_0, val );

    val = HDMI_READ( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0 );
    val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_GENERIC_CTRL, ENABLE, EN,
            val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0, val );
}

static void
DcHdmiAudioInfo( DcController *cont, DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 pb1, checksum;

    if( hdmi->bDvi )
    {
        val = NV_DRF_DEF( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL,
            ENABLE, NO );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL_0, val );
        return;
    }

    /* HB0: type
     * HB1: version
     * HB2: length
     */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER, HB0, 0x84 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER, HB1, 0x1 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER, HB2, 0xa );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_HEADER_0, val );

    /*
     * Setup the Packet Data Bytes
     * PB1: CC=1
     */
    pb1 = 0x1;

    checksum = ((256 - (0xa + 0x1 + 0x84 + pb1)) & 0xff) & 0xff;

    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_SUBPACK0_LOW, PB0,
            checksum )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_SUBPACK0_LOW, PB1,
            pb1 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_SUBPACK0_LOW_0, val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_SUBPACK0_HIGH_0, 0 );

    val = NV_DRF_DEF( HDMI, NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL, ENABLE, EN );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_AUDIO_INFOFRAME_CTRL_0, val );
}

static void
DcHdmiCommit( DcController *cont, DcHdmi *hdmi )
{
    NvError e;
    NvU32 val;
    NvU32 tmp;
    NvOdmDispDeviceTiming *timing;
    NvRmClockConfigFlags clock_flags = (NvRmClockConfigFlags)0;
    NvDdkDispCapabilities *caps = cont->caps;

    /* the controller mode must be set */
    NV_ASSERT( cont->mode.width && cont->mode.height && cont->mode.bpp );

    /* map the registers */
    if( !hdmi->hdmi_regs )
    {
        NvRmPhysAddr phys;
        NvU32 size;
        NvU32 freq;
        NvU32 tolerance;

        NvRmModuleGetBaseAddress( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_Hdmi, 0 ),
            &phys, &size );

        NV_CHECK_ERROR_CLEANUP(
            NvRmPhysicalMemMap( phys, size, NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached, &hdmi->hdmi_regs )
        );

        hdmi->hdmi_regs_size = size;

        if( NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_Hdmi_Kfuse ) )
        {
            /* map the kfuse registers too */
            NvRmModuleGetBaseAddress( cont->hRm,
                NVRM_MODULE_ID( NvRmModuleID_KFuse, 0 ),
                &phys, &size);

            NV_CHECK_ERROR_CLEANUP(
                NvRmPhysicalMemMap( phys, size, NVOS_MEM_READ_WRITE,
                    NvOsMemAttribute_Uncached, &hdmi->kfuse_regs )
            );

            hdmi->kfuse_regs_size = size;
        }

        hdmi->PowerClientId = NVRM_POWER_CLIENT_TAG('H','D','M','I');
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerRegister( cont->hRm, 0, &hdmi->PowerClientId )
        );

        if( cont->bMipi )
        {
            clock_flags = NvRmClockConfig_MipiSync;
        }

        // BUG 518736 -- clocks must be enabled before configuring them
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerModuleClockControl( cont->hRm,
                NVRM_MODULE_ID( NvRmModuleID_Hdmi, 0 ),
                hdmi->PowerClientId,
                NV_TRUE )
        );

        /* must set hdmi to exactly the same frequency as display */
        if( cont->mode.flags & NVDDK_DISP_MODE_FLAG_TYPE_DTD )
        {
            tolerance = 1015;    // -0.5% to +1.5%
        }
        else
        {
            // NVDDK_DISP_MODE_FLAG_TYPE_HDMI NVDDK_DISP_MODE_FLAG_TYPE_VESA
            tolerance = 1005;    //  -0.5% to +0.5%
        }

        freq = cont->panel_freq;

        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerModuleClockConfig( cont->hRm,
                NVRM_MODULE_ID( NvRmModuleID_Hdmi, 0 ),
                cont->PowerClientId,
                (freq * 995) / 1000,
                (freq * tolerance) / 1000 ,
                &freq, 1, &freq, clock_flags )
        );

        /* send a reset pulse to clear out any left-over state */
        NvRmModuleReset( cont->hRm, NVRM_MODULE_ID( NvRmModuleID_Hdmi, 0 ) );
    }

    timing = &cont->mode.timing;

    /* enable video window for simulation: magic 0x2600 value sets the
     * decoder mode to hdmi, not hdmi/dvi.
     */
    hdmi_decoder_write( hdmi, HD_HDMI_REG_ADDR, 0x2600 );

    /* set vsync_h_position to 1 in disp_timing_options */
    val = NV_DRF_NUM( DC_DISP, DISP_TIMING_OPTIONS,
        VSYNC_H_POSITION, 1 );
    DC_WRITE( cont, DC_DISP_DISP_TIMING_OPTIONS_0, val );

    /* set the color base */
    DcSetColorBase( cont, NvOdmDispBaseColorSize_888 );

    /* video preamble -- uses the h_pulse2 signal.
     * 8 clocks wide, ending 2 clocks before end of hblank (in v active)
     */

    /* shadow the signal options just to be sure nothing gets smashed */
    val = cont->signal_options0;
    val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_SIGNAL_OPTIONS0, H_PULSE2_ENABLE,
        ENABLE, val );
    cont->signal_options0 = val;
    DC_WRITE( cont, DC_DISP_DISP_SIGNAL_OPTIONS0_0, val );

    val = NV_DRF_DEF( DC_DISP, H_PULSE2_CONTROL, H_PULSE2_MODE, NORMAL )
        | NV_DRF_DEF( DC_DISP, H_PULSE2_CONTROL, H_PULSE2_POLARITY, HIGH )
        | NV_DRF_DEF( DC_DISP, H_PULSE2_CONTROL, H_PULSE2_V_QUAL, VACTIVE )
        | NV_DRF_DEF( DC_DISP, H_PULSE2_CONTROL, H_PULSE2_LAST, END_A );
    DC_WRITE( cont, DC_DISP_H_PULSE2_CONTROL_0, val );

    tmp = timing->Horiz_RefToSync + timing->Horiz_SyncWidth +
        timing->Horiz_BackPorch - 10;
    val = NV_DRF_NUM( DC_DISP, H_PULSE2_POSITION_A, H_PULSE2_START_A, tmp )
        | NV_DRF_NUM( DC_DISP, H_PULSE2_POSITION_A, H_PULSE2_END_A, tmp + 8 );
    DC_WRITE( cont, DC_DISP_H_PULSE2_POSITION_A_0, val );

    /* vsync window adjustment */
    val = NV_DRF_NUM( HDMI, NV_PDISP_HDMI_VSYNC_WINDOW, END, 0x210 )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDMI_VSYNC_WINDOW, START, 0x200 )
        | NV_DRF_DEF( HDMI, NV_PDISP_HDMI_VSYNC_WINDOW, ENABLE, YES );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_VSYNC_WINDOW_0, val );

    /* select source of display or displayb */
    if( cont->index == 0 )
    {
        val = NV_DRF_DEF( HDMI, NV_PDISP_INPUT_CONTROL, HDMI_SRC_SELECT,
            DISPLAY );
    }
    else
    {
        val = NV_DRF_DEF( HDMI, NV_PDISP_INPUT_CONTROL, HDMI_SRC_SELECT,
            DISPLAYB );
    }

    /* limited color range, in most cases */
    if( cont->mode.height > 480 )
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_INPUT_CONTROL,
            ARM_VIDEO_RANGE, LIMITED, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_INPUT_CONTROL,
            ARM_VIDEO_RANGE, FULL, val );
    }
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_INPUT_CONTROL_0, val );

    /* define crc initialization */
#if NV_DEBUG
    val = NV_DRF_DEF( HDMI, NV_PDISP_CRC_CONTROL, ARM_CRC_ENABLE, YES );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_CRC_CONTROL_0, val );
#endif

    /* hdmi clock is in usec, 8.2 format */
    tmp = cont->panel_freq; /* this is in khz */
    tmp /= 1000; /* mhz */
    tmp <<= 2; /* 8.2 */
    val = NV_DRF_NUM( HDMI, NV_PDISP_SOR_REFCLK, DIV_INT, tmp >> 2 )
        | NV_DRF_NUM( HDMI, NV_PDISP_SOR_REFCLK, DIV_FRAC, tmp & 0x3 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_SOR_REFCLK_0, val );

    /* setup audio */
    DcHdmiAudioConfig( hdmi, cont->panel_freq, hdmi->audio_freq );

    /* enable hdmi + audio */

    /* setup rekey and  max ac packet. */
    val = NV_RESETVAL( HDMI, NV_PDISP_HDMI_CTRL );
    if( hdmi->bDvi == NV_TRUE )
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_CTRL, ENABLE, NO, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_CTRL, ENABLE, EN, val );
    }
#define DC_HDMI_REKEY (56)
    val = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_HDMI_CTRL, REKEY, DC_HDMI_REKEY,
        val );
    tmp = timing->Horiz_SyncWidth + timing->Horiz_BackPorch +
        timing->Horiz_FrontPorch - DC_HDMI_REKEY;
    tmp = (tmp - 18) / 32;
    val = NV_FLD_SET_DRF_NUM( HDMI, NV_PDISP_HDMI_CTRL, MAX_AC_PACKET,
        tmp, val );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_CTRL_0, val );
#undef DC_HDMI_REKEY

    val = NV_RESETVAL( HDMI, NV_PDISP_HDMI_GENERIC_CTRL );
    if( hdmi->bDvi == NV_FALSE || (hdmi->audio_freq != NvDdkDispAudioFrequency_00000hz) )
    {
        val = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_HDMI_GENERIC_CTRL, AUDIO, EN,
            val );
    }
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_GENERIC_CTRL_0, val );

    /* configure avi info and audio info packets */
    DcHdmiAviInfo( cont, hdmi );
    DcHdmiAudioInfo( cont, hdmi );

    /* configure the stereo info */
    DcHdmiStereoInfo( cont, hdmi );

    /* enable tmds */
    DcHdmiTmdsConfig( cont, hdmi );

    /* stop the display  */
    DcStopDisplay( cont, NV_FALSE );

    /* power up the sor */
    DcHdmiSorEnable( cont, hdmi );

    /* start the sor */
    DcHdmiSorStart( cont, hdmi );

    /* set the hdmi enable bit */
    DcEnable( cont, DC_HAL_HDMI, NV_TRUE );

    return;

fail:
    DcEnable( cont, DC_HAL_HDMI, NV_FALSE );
    NvOsPhysicalMemUnmap( hdmi->hdmi_regs, hdmi->hdmi_regs_size );
    hdmi->hdmi_regs = 0;

    if( hdmi->kfuse_regs )
    {
        NvOsPhysicalMemUnmap( hdmi->kfuse_regs, hdmi->kfuse_regs_size );
        hdmi->kfuse_regs = 0;
    }

    cont->bFailed = NV_TRUE;
}

void
DcHdmiEnable( NvU32 controller, NvBool enable )
{
    DcHdmi *hdmi;
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    hdmi = &s_Hdmi;

    if( enable )
    {
        hdmi->adaptation = DcHdmiOdmOpen();

        HOST_CLOCK_ENABLE( cont );

        DcHdmiCommit( cont, hdmi );

        /* make sure memory is running fast enough */
        NV_ASSERT_SUCCESS(
            NvRmPowerBusyHint( cont->hRm, NvRmDfsClockId_Emc,
                hdmi->PowerClientId, NV_WAIT_INFINITE, NvRmFreqMaximum )
        );
    }
    else
    {
        NvU32 val;

        if( !hdmi->hdmi_regs )
        {
            return;
        }

        /* quickly toggling the hardware on and off seems to mess up stuff. */
        // FIMXE: figure out why this happens, how to fix it reliably
        NvOsSleepMS( 100 );

        /* send shutdown sequence */
        DcHdmiSorStop( cont, hdmi );

        /* shutdown hdmi */
        val = NV_DRF_DEF( HDMI, NV_PDISP_HDMI_CTRL, ENABLE, DIS );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDMI_CTRL_0, val );

        /* unset the hdmi enable bit */
        DcEnable( cont, DC_HAL_HDMI, NV_FALSE );

        /* cancel busy hint */
        NV_ASSERT_SUCCESS(
            NvRmPowerBusyHint( cont->hRm, NvRmDfsClockId_Emc,
                hdmi->PowerClientId, 0, 0 )
        );

        /* disable clocks */
        NvRmPowerModuleClockControl( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_Hdmi, 0 ),
            hdmi->PowerClientId,
            NV_FALSE );

        NvOsPhysicalMemUnmap( hdmi->hdmi_regs, hdmi->hdmi_regs_size );
        hdmi->hdmi_regs = 0;

        if( hdmi->kfuse_regs )
        {
            NvOsPhysicalMemUnmap( hdmi->kfuse_regs, hdmi->kfuse_regs_size );
            hdmi->kfuse_regs = 0;
        }

        NvRmPowerUnRegister( cont->hRm, hdmi->PowerClientId );
        hdmi->PowerClientId = 0;

        edid_modes_num = 0;

        HOST_CLOCK_DISABLE( cont );

        DcHdmiOdmClose( hdmi->adaptation );
    }

    return;
}

void DcHdmiListModes( NvU32 *nModes, NvOdmDispDeviceMode *modes )
{
    NvOdmDispDeviceMode *m;
    NvU32 len;

    NV_ASSERT( nModes );

    m = default_mode;
    len = NV_ARRAY_SIZE(default_mode);

    if( !(*nModes ) )
    {
        *nModes = len;
    }
    else
    {
        NvU32 i;

        len = NV_MIN( *nModes, len );

        for( i = 0; i < len; i++ )
        {
            modes[i] = m[i];
        }
    }
}

static NvBool
DcHdmiDiscover( DcHdmi *hdmi )
{
    NvU64 guid;
    const NvU32 vals[] =
    {
        NvOdmPeripheralClass_Display,
        NvOdmIoModule_Hdmi,
        NvOdmIoModule_I2c,
    };
    const NvOdmPeripheralSearch search[] =
    {
        NvOdmPeripheralSearch_PeripheralClass,
        NvOdmPeripheralSearch_IoModule,
    };
    NvOdmPeripheralConnectivity const *conn;

    if( hdmi->conn )
    {
        return NV_TRUE;
    }

    /* find a display guid */
    if( !NvOdmPeripheralEnumerate( search, vals, 2, &guid, 1 ) )
    {
        return NV_FALSE;
    }

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid( guid );
    if( !conn )
    {
        return NV_FALSE;
    }

    hdmi->guid = guid;
    hdmi->conn = conn;
    return NV_TRUE;
}

static NvBool DcHdmiInit( void )
{
    NvU32 i;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvU32 settle_us;
    DcHdmi *hdmi;

    hdmi = &s_Hdmi;

    hdmi->adaptation = DcHdmiOdmOpen();

    /* get the peripheral config */
    if( !DcHdmiDiscover( hdmi ) )
    {
        return NV_FALSE;
    }

    /* enable the power rail */
    hPmu = NvOdmServicesPmuOpen();
    conn = hdmi->conn;

    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            NvOdmServicesPmuVddRailCapabilities cap;

            /* address is the vdd rail id */
            NvOdmServicesPmuGetCapabilities( hPmu,
                conn->AddressList[i].Address, &cap );

            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage( hPmu,
                conn->AddressList[i].Address, cap.requestMilliVolts,
                &settle_us );

            /* wait for the rail to settle */
            NvOdmOsWaitUS( settle_us );
        }
    }

    NvOdmServicesPmuClose( hPmu );

    return NV_TRUE;
}

static void DcHdmiDeinit( void )
{
    NvU32 i;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    DcHdmi *hdmi;

    hdmi = &s_Hdmi;

    /* disable the power rails */
    hPmu = NvOdmServicesPmuOpen();
    if( !hdmi->conn )
    {
        if( !DcHdmiDiscover( hdmi ) )
        {
            return;
        }
    }
    conn = hdmi->conn;

    NV_ASSERT( conn );

    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage( hPmu,
                conn->AddressList[i].Address, NVODM_VOLTAGE_OFF, 0 );
        }
    }

    NvOdmServicesPmuClose( hPmu );
    DcHdmiOdmClose( hdmi->adaptation );
}

void DcHdmiSetPowerLevel( NvOdmDispDevicePowerLevel level )
{
    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        DcHdmiDeinit();
        break;
    case NvOdmDispDevicePowerLevel_On:
        DcHdmiInit();
        break;
    default:
        break;
    }
}

void DcHdmiGetParameter( NvOdmDispParameter param, NvU32 *val )
{
    switch( param ) {
    case NvOdmDispParameter_Type:
        *val = NvOdmDispDeviceType_HDMI;
        break;
    case NvOdmDispParameter_Usage:
        *val = NvOdmDispDeviceUsage_Removable;
        break;
    case NvOdmDispParameter_MaxHorizontalResolution:
        *val = 1920;
        break;
    case NvOdmDispParameter_MaxVerticalResolution:
        *val = 1200;
        break;
    case NvOdmDispParameter_BaseColorSize:
        *val = 0;
        break;
    case NvOdmDispParameter_DataAlignment:
        *val = 0;
        break;
    case NvOdmDispParameter_PinMap:
        *val = 0;
        break;
    case NvOdmDispParameter_PwmOutputPin:
        *val = 0;
        break;
    default:
        break;
    }
}

NvU64 DcHdmiGetGuid( void )
{
    DcHdmi *hdmi;

    hdmi = &s_Hdmi;
    if( !hdmi->conn )
    {
        if( !DcHdmiDiscover( hdmi ) )
        {
            return (NvU64)-1;
        }
    }

    return hdmi->guid;
}

void DcHdmiSetEdid( NvDdkDispEdid *edid, NvU32 flags )
{
    DcHdmi *hdmi;
    NvU32 len;
    NvU32 ieee_code;
    NvU8 *tmp;

    hdmi = &s_Hdmi;
    hdmi->edid = edid;

    if (flags & NVDDK_DISP_SET_EDID_FLAG_USE_DEFAULT)
    {
        hdmi->bDvi = NV_FALSE;
        edid_modes_num = 1;
        NvOsMemcpy( &edid_modes[0], &default_mode[0], sizeof (NvOdmDispDeviceMode));
        goto clean;
    }

    if( !edid && !flags )
    {
        flags = NVDDK_DISP_SET_EDID_FLAG_USE_BUILTIN;
    }

    if( !edid && (flags & NVDDK_DISP_SET_EDID_FLAG_USE_BUILTIN) )
    {
        hdmi->bDvi = NV_FALSE;
        edid_modes_num = 0;
        goto clean;
    }
    else if( !edid && (flags & NVDDK_DISP_SET_EDID_FLAG_USE_VESA) )
    {
        // reuse 'flags' -- too lazy to do otherwise
        // list vesa modes if there's an edid failure
        flags = NVDDK_DISP_EDID_EXPORT_FLAG_VESA;
    }
    else if( !edid && (flags & NVDDK_DISP_SET_EDID_FLAG_USE_ALL) )
    {
        flags = NVDDK_DISP_EDID_EXPORT_FLAG_ALL;
    }

    /* get the modes from the edid */
    edid_modes_num = 0;
    NvDdkDispEdidExport( edid, &edid_modes_num, 0, flags );
    edid_modes_num = NV_MIN( NVDDK_DISP_MAX_MODES, edid_modes_num );
    NvDdkDispEdidExport( edid, &edid_modes_num, edid_modes, flags );

    if( edid )
    {
        /* need to check the vendor specific data block to determine hdmi vs
         * dvi
         */
        len = edid->VendorSpecificDataBlock[0] & 0x1f;
        if( !len )
        {
            goto dvi;
        }

        tmp = &edid->VendorSpecificDataBlock[1];
        ieee_code = (*tmp) | (*(tmp + 1) << 8) | (*(tmp + 2) << 16);
        if( ieee_code != NVDDK_DISP_EDID_IEEE_REG_ID )
        {
            /* some sinks (the hdcp analyzer, for instance) do not seem to set
             * the IEEE flag, so check to see if there are CEA edid extensions
             * instead.
             */
            if( edid->bHasCeaExtension == NV_FALSE )
            {
                goto dvi;
            }
        }
    }

    hdmi->bDvi = NV_FALSE;
    goto clean;

dvi:
    hdmi->bDvi = NV_TRUE;

clean:
    NV_DEBUG_PRINTF(( "connector type: %s\n", (hdmi->bDvi) ? "dvi" : "hdmi" ));
    return;
}

void DcHdmiAudioFrequency( NvU32 controller, NvDdkDispAudioFrequency freq )
{
    DcHdmi *hdmi;
    DcController *cont;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    hdmi = &s_Hdmi;

    hdmi->audio_freq = freq;

    if( hdmi->hdmi_regs )
    {
        DcHdmiAudioConfig( hdmi, cont->panel_freq, freq );
    }
}

/* write the control register with synchronization for frame-end, if required
 */
static void
DcHdcpWriteControl( DcController *cont, DcHdmi *hdmi, NvU32 ctrl )
{
     // FIXME: add a bug cap bit for this
#if BUG_409279_HDCP_GLITCH
    NvU32 val;

    cont->stream.LastEngineUsed = NvRmModuleID_Display;
    cont->stream.SyncPointID = cont->SyncPointId;
    cont->stream.SyncPointsUsed = cont->SyncPointShadow;
    cont->stream.ClientManaged = NV_TRUE;

    #define DC_HDCP_WRITE_CONTROL_WORDS (64)

    NVRM_STREAM_BEGIN( &cont->stream, cont->pb, DC_HDCP_WRITE_CONTROL_WORDS );

    DcPushSetcl( cont );

    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_ACQUIRE_MUTEX( cont->hw_mutex ) );

    DcPushWaitFrame( cont );
    DcPushSetcl( cont );

    DcLatch( cont, NV_FALSE );

    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_SET_CLASS( NV_HDMI_CLASS_ID, 0, 0 ) );

    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_NONINCR( HDMI_NV_PDISP_RG_HDCP_CTRL_0, 1 ) );
    NVRM_STREAM_PUSH_U( cont->pb, ctrl );

    /* poke the sync point to ensure the pushbuffer made it through */
    DcPushSetcl( cont );
    cont->SyncPointShadow++;
    val = NV_DRF_DEF( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_COND, IMMEDIATE )
        | NV_DRF_NUM( DC_CMD, GENERAL_INCR_SYNCPT, GENERAL_INDX,
            cont->SyncPointId );
    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_NONINCR( DC_CMD_GENERAL_INCR_SYNCPT_0, 1 ) );
    NVRM_STREAM_PUSH_U( cont->pb, val );

    NVRM_STREAM_PUSH_U( cont->pb,
        NVRM_CH_OPCODE_RELEASE_MUTEX( cont->hw_mutex ) );

    NVRM_STREAM_END( &cont->stream, cont->pb );
    cont->pb = 0;

    cont->stream.SyncPointsUsed = cont->SyncPointShadow;
    NvRmStreamFlush( &cont->stream, 0 );

    #undef DC_HDCP_WRITE_CONTROL_WORDS

#else
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0, ctrl );
#endif
}

/** HDCP implementation */

/* Note for hdcp i2c:
 *
 * The primary link address is 0x74 (secondary is 0x76)
 *
 * Offsets:
 * 0x0-0x4: Bksv
 * 0x8-0x9: Ri
 * 0x10-0x14: Aksv - writing 0x14 will trigger authentication sequence
 * 0x15: Ainfo - bit 1 enables hdcp 1.1 features
 * 0x18-0x1f: An - must be written before Aksv
 *
 * The i2c write format is:
 *
 * SA: Slave Address, 7 bits
 *
 * +---------------------------------------------------+
 * |S| SA |W|A| Offset Addr (8) |A| Write Data (8) |A|P|
 * +---------------------------------------------------+
 *
 * Read format:
 *
 * +----------------------------------------------------------+
 * |S| SA |W|A| Offset Addr (8) |A|SR| SA | Read Data (8) |A|P|
 * +----------------------------------------------------------+
 *
 * Shortcut for reading Ri (implicit offset of 0x8):
 *
 * +------------------------------------------------+
 * |S| SA |R|A| Read Data (8) |A| Read Data (8) |A|P|
 * +------------------------------------------------+
 *
 * NOTE: The hdcp spec has 3 read data bytes, but Ri is 16 bits.
 */

#define DC_HDCP_I2C_CLOCK       (40)
#define DC_HDCP_I2C_PINMUX      (0)

static NvBool
DcHdcpI2cWrite( DcHdmi *hdmi, NvU8 *buffer, NvU32 len )
{
    DcHdmiOdm *adpt;

    adpt = hdmi->adaptation;
    if( adpt->i2cTransaction )
    {
        NvOdmI2cStatus odmErr;
        NvOdmI2cTransactionInfo odmTrans;

        odmTrans.Flags = NVODM_I2C_IS_WRITE;
        odmTrans.NumBytes = len;
        odmTrans.Address = hdmi->hdcp_addr;
        odmTrans.Buf = buffer;

        // FIXME: clock freq?
        odmErr = (*(adpt->i2cTransaction))( NULL, &odmTrans, 1,
            DC_HDCP_I2C_CLOCK, NV_WAIT_INFINITE );
        return ( odmErr == NvOdmI2cStatus_Success ) ? NV_TRUE : NV_FALSE;
    }
    else
    {
        NvError err;
        NvRmI2cTransactionInfo trans;

        trans.Flags = NVRM_I2C_WRITE;
        trans.NumBytes = len;
        trans.Address = hdmi->hdcp_addr;
        trans.Is10BitAddress = NV_FALSE;

        err = NvRmI2cTransaction( hdmi->i2c, DC_HDCP_I2C_PINMUX,
            NV_WAIT_INFINITE, DC_HDCP_I2C_CLOCK, buffer, len, &trans, 1 );

        return ( err == NvSuccess ) ? NV_TRUE : NV_FALSE;
    }
}

static NvBool
DcHdcpI2cRead( DcHdmi *hdmi, NvU8 *buffer, NvU32 len )
{
    DcHdmiOdm *adpt;

    adpt = hdmi->adaptation;
    if( adpt->i2cTransaction )
    {
        NvOdmI2cStatus odmErr = NvOdmI2cStatus_Success;
        NvOdmI2cTransactionInfo odmTrans[2];
        NvU8 attempts;

        odmTrans[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
        odmTrans[0].NumBytes = 1;
        odmTrans[0].Address = hdmi->hdcp_addr;
        odmTrans[0].Buf = buffer;
        odmTrans[1].Flags = 0;      // read
        odmTrans[1].NumBytes = len - 1;
        odmTrans[1].Address = hdmi->hdcp_addr;
        odmTrans[1].Buf = buffer+1;

        /* the hdcp compliance tester wants multiple attempts to read stuff */
        attempts = 0;
        while( attempts < 3 )
        {
            odmErr = (*(adpt->i2cTransaction))( NULL, odmTrans, 2,
                DC_HDCP_I2C_CLOCK, NV_WAIT_INFINITE );
            if( odmErr != NvOdmI2cStatus_Success )
            {
                attempts++;
                NvOsSleepMS( 100 );
                continue;
            }

            break;
        }

        return ( odmErr == NvOdmI2cStatus_Success ) ? NV_TRUE : NV_FALSE;
    }
    else
    {
        NvError err = NvSuccess;
        NvRmI2cTransactionInfo trans[2];
        NvU8 attempts;

        trans[0].Flags = NVRM_I2C_WRITE | NVRM_I2C_NOSTOP;
        trans[0].NumBytes = 1;
        trans[0].Address = hdmi->hdcp_addr;
        trans[0].Is10BitAddress = NV_FALSE;
        trans[1].Flags = NVRM_I2C_READ;
        trans[1].NumBytes = len - 1;
        trans[1].Address = hdmi->hdcp_addr;
        trans[1].Is10BitAddress = NV_FALSE;

        /* the hdcp compliance tester wants multiple attempts to read stuff */
        attempts = 0;
        while( attempts < 3 )
        {
            err = NvRmI2cTransaction( hdmi->i2c, DC_HDCP_I2C_PINMUX,
                NV_WAIT_INFINITE, DC_HDCP_I2C_CLOCK, buffer, len, trans, 2 );
            if( err != NvSuccess )
            {
                attempts++;
                NvOsSleepMS( 100 );
                continue;
            }

            break;
        }

        return ( err == NvSuccess ) ? NV_TRUE : NV_FALSE;
    }
}

static NvBool
DcHdcpI2cReadRi( DcHdmi *hdmi, NvU16 *Ri )
{
    DcHdmiOdm *adpt;

    adpt = hdmi->adaptation;
    if( adpt->i2cTransaction )
    {
        NvOdmI2cStatus odmErr = NvOdmI2cStatus_Success;
        NvOdmI2cTransactionInfo odmTrans;

        odmTrans.Flags = 0;// indicate read
        odmTrans.NumBytes = 2;
        odmTrans.Address = hdmi->hdcp_addr;
        odmTrans.Buf = (NvU8 *)Ri;

        odmErr = (*(adpt->i2cTransaction))( NULL, &odmTrans, 1,
            DC_HDCP_I2C_CLOCK, NV_WAIT_INFINITE );

        return ( odmErr == NvOdmI2cStatus_Success ) ? NV_TRUE : NV_FALSE;
    }
    else
    {
        NvError err = NvSuccess;
        NvRmI2cTransactionInfo trans;

        trans.Flags = NVRM_I2C_READ;
        trans.NumBytes = 2;
        trans.Address = hdmi->hdcp_addr;
        trans.Is10BitAddress = NV_FALSE;

        err = NvRmI2cTransaction( hdmi->i2c, DC_HDCP_I2C_PINMUX,
            NV_WAIT_INFINITE, DC_HDCP_I2C_CLOCK, (NvU8 *)Ri, 2, &trans, 1 );

        return ( err == NvSuccess ) ? NV_TRUE : NV_FALSE;
    }
}

#define LINK_DEBUG 0

#if LINK_DEBUG
static void
ri_debug( DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 i;
    struct tuple
    {
        NvU32 reg;
        char *name;
    };

    struct tuple tuples[] =
    {
        { HDMI_NV_PDISP_RG_HDCP_CTRL_0, "HDMI_NV_PDISP_RG_HDCP_CTRL" },
        { HDMI_NV_PDISP_HDMI_CTRL_0, "HDMI_NV_PDISP_HDMI_CTRL" },
        { HDMI_NV_PDISP_HDMI_VSYNC_KEEPOUT_0,
            "HDMI_NV_PDISP_HDMI_VSYNC_KEEPOUT" },
        { HDMI_NV_PDISP_HDMI_VSYNC_WINDOW_0,
            "HDMI_NV_PDISP_HDMI_VSYNC_WINDOW" },
        { HDMI_NV_PDISP_RG_HDCP_CKSV_MSB_0, "HDMI_NV_PDISP_RG_HDCP_CKSV_MSB" },
        { HDMI_NV_PDISP_RG_HDCP_CKSV_LSB_0, "HDMI_NV_PDISP_RG_HDCP_CKSV_LSB" },
        { HDMI_NV_PDISP_RG_HDCP_SPRIME_MSB_0,
            "HDMI_NV_PDISP_RG_HDCP_SPRIME_MSB" },
        { HDMI_NV_PDISP_RG_HDCP_CS_MSB_0, "HDMI_NV_PDISP_RG_HDCP_CS_MSB" },
        { HDMI_NV_PDISP_RG_HDCP_CS_LSB_0, "HDMI_NV_PDISP_RG_HDCP_CS_LSB" },
    };

    NvOsDebugPrintf( "Bcaps: 0x%x\n", hdmi->hdcp.Bcaps );

    for( i = 0; i < NV_ARRAY_SIZE(tuples); i++ )
    {
        struct tuple *t = &tuples[i];
        val = HDMI_READ( hdmi, t->reg );
        NvOsDebugPrintf( "%s: 0x%x\n", t->name, val );
    }
}
#endif

static NvBool
DcHdcpVerifyLink( DcHdmi *hdmi, NvBool bWait )
{
    NvU16 receiver = 0;
    NvU16 transmitter = 0;
    NvU32 reg = 0, old = 0;
    NvU32 i;
    NvU8 buffer[3];

#if LINK_DEBUG
    static NvU16 s_trans[32];
    static NvU16 s_recv[32];
    static NvU32 s_idx = 0;
#endif

    /* hdcp spec suggests trying 3 attempts to be sure there aren't i2c link
     * issues.
     */
    for( i = 0; i < 3; i++ )
    {
        do
        {
            if( bWait )
            {
                /* wait for the next Ri (not R0).
                 * make sure that Ri is not in the middle of an update by
                 * reading from the transmitter, then receiver, then
                 * transmitter again.
                 */
                old = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_RI_0 );
            }

            if( !hdmi->hdcp.bUseLongRi && !DcHdcpI2cReadRi( hdmi, &receiver ) )
            {
                receiver = 0;
            }

            if( !receiver )
            {
                /* try long reads if the short read failed */
                hdmi->hdcp.bUseLongRi = NV_TRUE;
            }

            if( hdmi->hdcp.bUseLongRi )
            {
                buffer[0] = 0x8; // Ri
                if ( !DcHdcpI2cRead( hdmi, buffer, 3 ) )
                {
                    return NV_FALSE;
                }
                receiver = (NvU16)(buffer[1] | (buffer[2] << 8));
            }
            if( !receiver )
            {
                return NV_FALSE;
            }

            reg = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_RI_0 );
            transmitter = (NvU16)NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_RI,
                 VALUE, reg );
        } while( bWait && old != reg );

        /* make sure hotplug hasn't been deasserted */
        if( hdmi->hdcp.context->bHotplugDeasserted )
        {
            return NV_FALSE;
        }

#if LINK_DEBUG
        {
            s_trans[s_idx] = transmitter;
            s_recv[s_idx] = receiver;
            s_idx = (s_idx + 1) & 31;
            if( s_idx == 0 )
            {
                for( i = 0; i < 32; i++ )
                {
                    NvOsDebugPrintf( "trans: %x recv: %x\n", s_trans[i],
                        s_recv[i] );
                }
            }
        }
#endif

        if( receiver == transmitter )
        {
            return NV_TRUE;
        }
    }

#if LINK_DEBUG
    {
        for( i = 0; i < s_idx; i++ )
        {
            NvOsDebugPrintf( "trans: %x recv: %x\n", s_trans[i], s_recv[i] );
        }

        ri_debug( hdmi );
    }
#endif

    return NV_FALSE;
}

static NvBool
DcHdcpVerifyKsvs( DcHdmi *hdmi, NvU64 *ksvs, NvU32 nKsvs )
{
    NvU32 count;
    NvU64 *list;
    NvU32 i, j;
    DcHdmiOdm *adpt;

    /* make sure the ksv isn't in the revocation list
     * always use the ODM adaptation if it exists
     */
    adpt = hdmi->adaptation;
    if( adpt->hdcpIsRevokedKsv )
    {
        if( (*(adpt->hdcpIsRevokedKsv))( ksvs, nKsvs ) )
        {
            goto fail;
        }
    }
    else
    {
        list = hdmi->hdcp.context->RevokedKsvs;
        for( i = 0; i < nKsvs; i++ )
        {
            for( j = 0; j < hdmi->hdcp.context->nRevokedKsvs; j++ )
            {
                if( ksvs[i] == list[j] )
                {
                    goto fail;
                }
            }
        }
    }

    /* ksvs must have 20 bits set */
    for( i = 0; i < nKsvs; i++ )
    {
        NvU64 ksv = ksvs[i];
        count = 0;
        while( ksv )
        {
            if( ksv & 0x1 )
            {
                count++;
            }

            ksv >>= 1;
        }

        if( count != 20 )
        {
            goto fail;
        }
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

static NvBool
DcHdcpSecureRom( DcHdmi *hdmi, NvBool bRepeater )
{
    NvU32 ctrl;
    NvU32 i;

    if( bRepeater )
    {
        NvU32 val;
        val = NV_DRF_NUM( HDMI, NV_PDISP_RG_HDCP_BKSV_MSB, REPEATER,
            bRepeater );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB_0, 0 );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_MSB_0, val );
    }

    #define SECURE_ROM_RETRIES 32

    hdmi->hdcp.Aksv = 0;
    hdmi->hdcp.Bksv = 0;
    hdmi->hdcp.An = 0;

    ctrl = NV_RESETVAL( HDMI, NV_PDISP_RG_HDCP_CTRL );
    /* access secure rom to get An, etc */
    ctrl = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CTRL, RUN, YES, ctrl );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0, ctrl );

    /* wait for secure rom to do stuff */
    for( i = 0; i < SECURE_ROM_RETRIES; i++ )
    {
        ctrl = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0 );
        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, AN, ctrl ) != 0 )
        {
            break;
        }

        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, SROM_ERR, ctrl ) )
        {
            NV_ASSERT( !"secure rom error" );
            goto fail;
        }

        NvOsSleepMS( 1 );
    }
    if( i == SECURE_ROM_RETRIES )
    {
        goto fail;
    }

    #undef SECURE_ROM_RETRIES

    hdmi->hdcp.Aksv =
        (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AKSV_MSB_0)) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AKSV_LSB_0 );

    hdmi->hdcp.An =
        (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AN_MSB_0)) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AN_LSB_0 );

    if( !DcHdcpVerifyKsvs( hdmi, &hdmi->hdcp.Aksv, 1 ) )
    {
        goto fail;
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

#define HDMI_KFUSE_DEBUG 0

void
DcHdcpKfuseDebug( DcHdmi *hdmi, NvU32 index );
void
DcHdcpKfuseDebug( DcHdmi *hdmi, NvU32 index )
{
    /* magic values from hw -- this is chip specific (actual die, not ap20
     * in general).
     */
    static const NvU32 exp_crc[] = // low, high
    {
        0x521f,0x16c4,
        0x4891,0x2653,
        0x06b2,0x5c90,
        0x8600,0xe7b3,
        0x8513,0x9f6f,
        0xfeec,0x49b3,
        0x1625,0xc065,
        0x0f2a,0x2b2f,
        0x51be,0x6b66,
        0xe953,0xdee9,
        0x1388,0xf87b,
        0xa54f,0x7d76,
        0x11d0,0xb36a,
        0xe6c5,0x1102,
        0x3de2,0xe185,
        0x929a,0x7754,
        0x24a3,0xaf9a,
        0x7041,0x45e4,
        0x7041,0xdce9,
        0x7041,0xe6fa,
        0x7041,0xc91c,
        0x7041,0xa882,
        0x7041,0x2ad8,
        0x7041,0x1903,
        0x7041,0x9e4e,
        0x7041,0xd0e5,
        0x7041,0x2354,
        0x7041,0x0e84,
        0x7041,0x436d,
        0x7041,0x97d5,
        0x7041,0x1f3f,
        0x7041,0x3a81,
        0x7041,0x4890,
        0x7041,0x5e7c,
        0x7041,0x9f4e,
        0x7041,0xa02f
    };

    NvU32 val;
    NvU32 sum_lo, sum_hi;

    sum_lo = exp_crc[index * 2];
    sum_hi = exp_crc[(index * 2) + 1];

    /* trigger a hardware crc check for the keys */
    val = NV_DRF_NUM( HDMI, NV_PDISP_KEY_DEBUG1, CHECKSUMVAL_LOW, sum_lo )
        | NV_DRF_NUM( HDMI, NV_PDISP_KEY_DEBUG1, CHECKSUMVAL_HIGH, sum_hi );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_DEBUG1_0, val );

    val = NV_DRF_DEF( HDMI, NV_PDISP_KEY_DEBUG0, CHECKSUM, TRIGGER );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_DEBUG0_0, val );

    do
    {
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_KEY_DEBUG0_0 );
    } while( NV_DRF_VAL( HDMI, NV_PDISP_KEY_DEBUG0, CHECKSUM, val ) !=
            HDMI_NV_PDISP_KEY_DEBUG0_0_CHECKSUM_DONE );

    /* get the result */
    if( NV_DRF_VAL( HDMI, NV_PDISP_KEY_DEBUG0, CHECKSUMCMP_LOW, val ) !=
        HDMI_NV_PDISP_KEY_DEBUG0_0_CHECKSUMCMP_LOW_MATCH )
    {
        NV_ASSERT( !"checksum low mismatch" );
    }
    if( NV_DRF_VAL( HDMI, NV_PDISP_KEY_DEBUG0, CHECKSUMCMP_HIGH, val ) !=
        HDMI_NV_PDISP_KEY_DEBUG0_0_CHECKSUMCMP_HIGH_MATCH )
    {
        NV_ASSERT( !"checksum high mismatch" );
    }
}

static void
DcHdmiObsBus( DcController *cont )
{
    NvError e;
    NvRmPhysAddr phys;
    NvU32 reg, data;
    NvU32 size;
    void *regs;

    NvRmModuleGetBaseAddress( cont->hRm,
        NVRM_MODULE_ID( NvRmModuleID_Misc, 0 ), &phys, &size );

    NV_CHECK_ERROR_CLEANUP(
        NvRmPhysicalMemMap( phys, size, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached, &regs )
    );

    /* kfuse debug */
    reg = NV_DRF_DEF( APB_MISC_GP, OBSCTRL, OBS_EN, ENABLE )
        | NV_DRF_DEF( APB_MISC_GP, OBSCTRL, OBS_MOD_SEL, HDMI )
        | NV_DRF_NUM( APB_MISC_GP, OBSCTRL, OBS_PART_SEL, 2 )
        | NV_DRF_NUM( APB_MISC_GP, OBSCTRL, OBS_SIG_SEL, 2 );
    NV_WRITE32( (((NvU8 *)regs) + APB_MISC_GP_OBSCTRL_0), reg );

    data = NV_READ32( (((NvU8 *)regs) + APB_MISC_GP_OBSDATA_0) );
    NvOsDebugPrintf( "hdmi obs data: 0x%x\n", data );

    NvOsPhysicalMemUnmap( regs, size );
fail:
    return; // silly compiler
}

#define KFUSE_TIMEOUT (100)

static NvBool
DcHdcpKfuseKeys( DcController *cont, DcHdmi *hdmi, NvBool bRepeater )
{
    NvError e;
    NvU32 i, j;
    NvU32 ctrl, val;
    NvU32 timeout;
    void *kfuse_regs = 0;
    NvBool ret;
    NvOsOsInfo OsInfo;
    NvOsFileHandle hFile = NULL;

    NvOsGetOsInformation(&OsInfo);

    if( bRepeater )
    {
        val = NV_DRF_NUM( HDMI, NV_PDISP_RG_HDCP_BKSV_MSB, REPEATER,
            bRepeater );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB_0, 0 );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_MSB_0, val );
    }

    hdmi->hdcp.Aksv = 0;
    hdmi->hdcp.Bksv = 0;
    hdmi->hdcp.An = 0;

    if( OsInfo.OsType == NvOsOs_Linux)
        NvOsFopen("/sys/firmware/fuse/kfuse_raw", NVOS_OPEN_READ, &hFile);

    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerModuleClockControl( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_KFuse, 0 ),
            hdmi->PowerClientId,
            NV_TRUE )
    );

    /* don't configure the clock, it'll be ok. */

    /* steps for getting the keys:
     * 1) enable local keys
     * 2) wait for kfuse hw to read keys from fuses to its sram
     * 3) setup the copy
     * 4) set the AES key select, use 0 for now
     * 5) copy the keys to hdcp hw
     * 6) enable hdcp control, need to poll for valid An, etc.
     */

    kfuse_regs = hdmi->kfuse_regs;
    NV_ASSERT( kfuse_regs );

    ctrl = NV_DRF_DEF( HDMI, NV_PDISP_KEY_CTRL, LOCAL_KEYS, ENABLED );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_CTRL_0, ctrl );

    /* start address at zero, auto incr */
    val = NV_DRF_NUM( KFUSE, KEYADDR, ADDR, 0 )
        | NV_DRF_NUM( KFUSE, KEYADDR, AUTOINC, 1 );
    NV_WRITE32( ((NvU8 *)kfuse_regs) + KFUSE_KEYADDR_0, val );

    /* wait for kfuse hw to get data from fuses */
    timeout = KFUSE_TIMEOUT;
    do
    {
        // note that kfuse registers are incr4. hdmi/hdcp's are incr1
        val = NV_READ32( ((NvU8 *)kfuse_regs) + KFUSE_STATE_0 );

        if( NV_DRF_VAL( KFUSE, STATE, DONE, val ) )
        {
            break;
        }

        NvOsSleepMS( 10 );
    } while( --timeout );
    if( timeout == 0 )
    {
        NV_ASSERT( !"kfuse timeout" );
        goto fail;
    }

    /* verify stuff is ok */
    if( NV_DRF_VAL( KFUSE, STATE, CRCPASS, val ) == 0 )
    {
        NV_ASSERT( !"kfuse crc failure" );
        goto fail;
    }

    /* issue a reload */
    ctrl = NV_DRF_NUM( HDMI, NV_PDISP_KEY_CTRL, PKEY_REQUEST_RELOAD, 1 )
        | NV_DRF_DEF( HDMI, NV_PDISP_KEY_CTRL, LOCAL_KEYS, ENABLED );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_CTRL_0, ctrl );

    /* wait for hdcp to load the keys */
    timeout = KFUSE_TIMEOUT;
    do
    {
        ctrl = HDMI_READ( hdmi, HDMI_NV_PDISP_KEY_CTRL_0 );

        if( NV_DRF_VAL( HDMI, NV_PDISP_KEY_CTRL, PKEY_LOADED, ctrl ) ==
            HDMI_NV_PDISP_KEY_CTRL_0_PKEY_LOADED_TRUE )
        {
            break;
        }

        // use smaller sleep than the original key-load
        NvOsSleepMS( 1 );
    } while( --timeout );
    if( timeout == 0 )
    {
        NV_ASSERT( !"pkey load timeout" );
        goto fail;
    }

    /* setup copy */
    val = NV_DRF_NUM( HDMI, NV_PDISP_KEY_SKEY_INDEX, IDX_VALUE, 0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_SKEY_INDEX_0, val );

    val = NV_DRF_NUM( KFUSE, KEYADDR, ADDR, 0 )
        | NV_DRF_NUM( KFUSE, KEYADDR, AUTOINC, 1 );
    NV_WRITE32( ((NvU8 *)kfuse_regs) + KFUSE_KEYADDR_0, val );

    /* make sure keyram is cleared */
    timeout = KFUSE_TIMEOUT;
    do
    {
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_KEY_DEBUG0_0 );

        if( NV_DRF_VAL( HDMI, NV_PDISP_KEY_DEBUG0, SRAMCLEAR, val ) ==
            HDMI_NV_PDISP_KEY_DEBUG0_0_SRAMCLEAR_DONE )
        {
            break;
        }

        NvOsSleepMS( 1 );
    } while( --timeout );
    if( timeout == 0 )
    {
        NV_ASSERT( !"key sram clear failure\n" );
        goto fail;
    }

    /* 576 bytes = 144 words - 4 words per line */
    #define KFUSE_LINES (576 / 4 / 4)
    for( i = 0; i < KFUSE_LINES; i++ )
    {
        NvU32 data[4];
        if( OsInfo.OsType == NvOsOs_Linux)
        {
            NvOsFread(hFile, data, sizeof data, NULL);
        }
        else
        {
            for( j = 0; j < 4; j++ )
            {
                // autoincr
                data[j] = NV_READ32( ((NvU8 *)kfuse_regs) + KFUSE_KEYS_0 );
            }
        }

        HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_HDCP_KEY_0_0, data[0] );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_HDCP_KEY_1_0, data[1] );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_HDCP_KEY_2_0, data[2] );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_HDCP_KEY_3_0, data[3] );

        val = NV_DRF_DEF( HDMI, NV_PDISP_KEY_HDCP_KEY_TRIG, LOAD_HDCP_KEY,
                TRIGGER );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_HDCP_KEY_TRIG_0, val );

        ctrl = NV_DRF_DEF( HDMI, NV_PDISP_KEY_CTRL, LOCAL_KEYS, ENABLED )
            | NV_DRF_DEF( HDMI, NV_PDISP_KEY_CTRL, WRITE16, TRIGGER )
            | ((i == 0) ?
                NV_DRF_DEF( HDMI, NV_PDISP_KEY_CTRL, AUTOINC, DISABLED ) :
                NV_DRF_DEF( HDMI, NV_PDISP_KEY_CTRL, AUTOINC, ENABLED ));

        HDMI_WRITE( hdmi, HDMI_NV_PDISP_KEY_CTRL_0, ctrl );

        timeout = KFUSE_TIMEOUT;
        do {
            ctrl = HDMI_READ( hdmi, HDMI_NV_PDISP_KEY_CTRL_0 );

            if( NV_DRF_VAL( HDMI, NV_PDISP_KEY_CTRL, WRITE16, ctrl ) ==
                HDMI_NV_PDISP_KEY_CTRL_0_WRITE16_DONE )
            {
                break;
            }

            NvOsSleepMS( 1 );
        } while( --timeout );
        if( timeout == 0 )
        {
            NV_ASSERT( !"key load failure" );
            goto fail;
        }

        if( HDMI_KFUSE_DEBUG && i == KFUSE_LINES - 1 )
        {
            DcHdcpKfuseDebug( hdmi, i );
        }
    }

    NV_DEBUG_CODE(
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_KEY_CTRL_0 );
        NV_ASSERT( NV_DRF_VAL( HDMI, NV_PDISP_KEY_CTRL, ADDRESS, val ) ==
            576 );
    );

    #undef KFUSE_TIMEOUT

    #define HDCP_KEY_RETRIES 32

    ctrl = NV_RESETVAL( HDMI, NV_PDISP_RG_HDCP_CTRL );
    ctrl = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CTRL, RUN, YES, ctrl );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0, ctrl );

    /* wait for hardware to generate hdcp values */
    for( i = 0; i < HDCP_KEY_RETRIES; i++ )
    {
        ctrl = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0 );
        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, AN, ctrl ) != 0 )
        {
            break;
        }

        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, SROM_ERR, ctrl ) )
        {
            NV_ASSERT( !"secure rom error" );
            goto fail;
        }

        NvOsSleepMS( 1 );
    }
    if( i == HDCP_KEY_RETRIES )
    {
        if( NV_DEBUG )
        {
            DcHdmiObsBus( cont );
        }
        NV_ASSERT( !"An invalid" );
        goto fail;
    }

    #undef HDCP_KEY_RETRIES

    hdmi->hdcp.Aksv =
        (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AKSV_MSB_0)) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AKSV_LSB_0 );

    hdmi->hdcp.An =
        (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AN_MSB_0)) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_AN_LSB_0 );

    if( !DcHdcpVerifyKsvs( hdmi, &hdmi->hdcp.Aksv, 1 ) )
    {
        NV_ASSERT( !"Aksv invalid" );
        goto fail;
    }

    ret = NV_TRUE;
    goto clean;

fail:
    ret = NV_FALSE;

clean:
    if (hFile) NvOsFclose(hFile);
    hFile = NULL;

    /* disable kfuse clock */
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerModuleClockControl( cont->hRm,
            NVRM_MODULE_ID( NvRmModuleID_KFuse, 0 ),
            hdmi->PowerClientId,
            NV_FALSE )
    );

    return ret;
}

NvBool
DcHdcpGetRepeaterInfo( DcHdmi *hdmi )
{
    NvU16 Bstatus;
    NvU8 Bcaps;
    NvU8 buffer[5];
    NvU8 *msg = 0;
    NvU8 *ptr = 0;
    NvU64 *KsvFifo = 0;
    NvU32 timeout;
    NvU32 nDevices;
    NvU32 size;
    NvU32 i;
    NvDdkDispHdcpContext *ctx;

    ctx = hdmi->hdcp.context;

    timeout = NvOsGetTimeMS() + 5000;

    /* poll Bcaps until the ready bit is set, 5 second poll maximum */
    for( ;; )
    {
        if( ctx->bHotplugDeasserted )
        {
            NV_DEBUG_PRINTF(( "HPD during repeater authentication\n" ));
            goto fail;
        }

        if( NvOsGetTimeMS() >= timeout )
        {
            NV_DEBUG_PRINTF(( "repeater timeout\n" ));
            return NV_FALSE;
        }

        buffer[0] = 0x40; // Bcaps
        if( !DcHdcpI2cRead( hdmi, buffer, 2 ) )
        {
            NV_DEBUG_PRINTF(( "Bcaps read failure (repeater)\n" ));
            goto fail;
        }

        /* check for ready bit */
        Bcaps = buffer[1];
        if( (Bcaps >> 5) & 0x1 )
        {
            break;
        }

        NvOsSleepMS( 100 );
    }

    if( ctx->bHotplugDeasserted )
    {
        NV_DEBUG_PRINTF(( "HPD during repeater authentication\n" ));
        goto fail;
    }

    hdmi->hdcp.Bcaps = Bcaps;

    /* get V' from repeater */
    NvOsMemset( ctx->pkt.vP, 0, sizeof(ctx->pkt.vP) );
    for( i = 0; i < 5; i++ )
    {
        NvU32 j;
        NvU32 *vP = (NvU32 *)ctx->pkt.vP;

        buffer[0] = 0x20 + (i*4); // base V' addr is 0x20
        if( !DcHdcpI2cRead( hdmi, buffer, 5 ) )
        {
            NV_DEBUG_PRINTF(( "V' read failure\n" ));
            goto fail;
        }

        for( j = 0; j < 4; j++ )
        {
            vP[i] = (((NvU32)buffer[j+1]) << (j * 8)) | vP[i];
        }
        NV_DEBUG_PRINTF(( "DcHdcpGetRepeaterInfo: vP[%d] = 0x%x\n", i, vP[i] ));
    }

    /* Bstatus, has repeater stuff */
    buffer[0] = 0x41;
    if( !DcHdcpI2cRead( hdmi, buffer, 3 ) )
    {
        NV_DEBUG_PRINTF(( "Bstatus read failure\n" ));
        goto fail;
    }

    Bstatus = buffer[1] | (buffer[2] << 8);

    /* check for errors */
    if( ((Bstatus >> 7) & 0x1) || ((Bstatus >> 11) & 0x1) )
    {
        /* max devices or max cascade */
        NV_DEBUG_PRINTF(( "max device or max cascade\n" ));
        goto fail;
    }

    /* DEVICE_COUNT is bits 6:0 */
    nDevices = Bstatus & 0x7F;
    if( nDevices == 0 )
    {
        /* don't read the KSV if DEVICE_COUNT is zero */
        // FIXME: need a delay here? hdcp tester will eventually stop the
        // test, usually in the middle of authentication, so the test will
        // fail.
        NV_DEBUG_PRINTF(( "nDevices is zero\n" ));
        goto fail;
    }

    /* read the KSV fifo -- each device is 5 bytes */
    size = (nDevices * 5) + 1;
    msg = NvOsAlloc( size );
    if( !msg )
    {
        NV_ASSERT( msg );
        goto fail;
    }

    KsvFifo = ctx->pkt.bKsvList;

    msg[0] = 0x43; // KSV fifo
    if( !DcHdcpI2cRead( hdmi, msg, size ) )
    {
        NV_DEBUG_PRINTF(( "KSV fifo read failure\n" ));
        goto fail;
    }

    /* check for revoked keys in the ksv fifo */
    ptr = &msg[1];
    for( i = 0; i < nDevices; i++ )
    {
        KsvFifo[i] = (NvU64)( (NvU64)(ptr[0]) | (NvU64)(ptr[1]) << 8
            | (NvU64)(ptr[2]) << 16 | (NvU64)(ptr[3]) << 24
            | (NvU64)(ptr[4]) << 32 );
        ptr += 5;
    }
    if( !DcHdcpVerifyKsvs( hdmi, KsvFifo, nDevices ) )
    {
        NV_DEBUG_PRINTF(( "KSV fifo verification failure\n" ));
        goto fail;
    }

    ctx->pkt.numBKSVs = nDevices;
    ctx->pkt.bStatus[0] = Bstatus;

    NV_DEBUG_PRINTF(( "DcHdcpGetRepeaterInfo: Bksv = %llx\n", hdmi->hdcp.Bksv ));
    for( i = 0; i < nDevices; i++ )
    {
        NV_DEBUG_PRINTF(( "DcHdcpGetRepeaterInfo: ksrv[%d] = %llx\n", i, \
                            ctx->pkt.bKsvList[i] ));
    }

    NvOsFree( msg );

    return NV_TRUE;

fail:
    NvOsFree( msg );
    return NV_FALSE;
}

void DcHdcpEncryptEnable( DcController *cont, DcHdmi *hdmi )
{
    NvU32 ctrl;

    ctrl = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0 );

    /* enable encryption and 1.1 features
     * for hdmi mode: always enable 1.1 regardless of receiver's Bcaps.
     * for dvi: look at receiver Bcaps.
     */
    if( !hdmi->bDvi || (hdmi->bDvi && hdmi->hdcp.bOneOne) )
    {
        ctrl = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CTRL, ONEONE,
            ENABLED, ctrl );
    }
    ctrl = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CTRL, CRYPT, ENABLED,
        ctrl );

    DcHdcpWriteControl( cont, hdmi, ctrl );
}

NvBool
DcHdcpDownstream( DcController *cont, DcHdmi *hdmi )
{
    NvU64 tmp;
    NvU32 tmp32;
    NvU32 ctrl;
    NvU32 i;
    NvU8 Bcaps;
    NvU8 buffer[16];
    NvBool bOneOne = NV_FALSE;
    NvBool bRepeater = NV_FALSE;
    NvDdkDispCapabilities *caps = cont->caps;

    /* read the Bcaps from receiver, only set 1.1 features in Ainfo if the
     * receiver supports it.
     */
    buffer[0] = 0x40; // Bcaps
    if( !DcHdcpI2cRead( hdmi, buffer, 2 ) )
    {
        NV_DEBUG_PRINTF(( "Bcaps read failed\n" ));
        goto fail;
    }

    Bcaps = buffer[1];
    if( (Bcaps >> 1) & 0x1 )
    {
        bOneOne = NV_TRUE;
    }
    if( (Bcaps >> 6) & 0x1 )
    {
        bRepeater = NV_TRUE;
    }
    hdmi->hdcp.bOneOne = bOneOne;
    hdmi->hdcp.bRepeater = bRepeater;
    hdmi->hdcp.Bcaps = Bcaps;

    if( NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_Hdmi_Kfuse ) )
    {
        if( !DcHdcpKfuseKeys( cont, hdmi, bRepeater ) )
        {
            NV_ASSERT( !"kfuse failure" );
            goto fail;
        }
    }
    else
    {
        /* hw bug 445548 -- need to write the repeater bit before enabling the
         * secure rom.
         */
        if( !DcHdcpSecureRom( hdmi, bRepeater ) )
        {
            NV_ASSERT( !"secure rom failure" );
            goto fail;
        }
    }

    /* write Ainfo, An, Aksv to receiver */

    buffer[0] = 0x15; // Ainfo
    if( bOneOne )
    {
        buffer[1] = (1UL << 1); // enable 1.1 features
    }
    else
    {
        buffer[1] = 0;
    }

    if( !DcHdcpI2cWrite( hdmi, buffer, 2 ) )
    {
        NV_DEBUG_PRINTF(( "Ainfo write failure\n" ));
        goto fail;
    }

    tmp = hdmi->hdcp.An;
    buffer[0] = 0x18; // An
    for( i = 0; i < 8; i++ )
    {
        buffer[i + 1] = (NvU8)(tmp & 0xff);
        tmp >>= 8;
    }

    if( !DcHdcpI2cWrite( hdmi, buffer, 9 ) )
    {
        NV_DEBUG_PRINTF(( "An write failure\n" ));
        goto fail;
    }

    tmp = hdmi->hdcp.Aksv;
    buffer[0] = 0x10; // Aksv
    for( i = 0; i < 5; i++ )
    {
        buffer[i + 1] = (NvU8)(tmp & 0xff);
        tmp >>= 8;
    }

    if( !DcHdcpI2cWrite( hdmi, buffer, 6 ) )
    {
        NV_DEBUG_PRINTF(( "Aksv write failure\n" ));
        goto fail;
    }

    /* make sure hotplug hasn't been deasserted */
    if( hdmi->hdcp.context->bHotplugDeasserted )
    {
        NV_DEBUG_PRINTF(( "HPD during downstream authentication\n" ));
        goto fail;
    }

    /* get Bksv from reciever */

    buffer[0] = 0x0; // Bksv
    if( !DcHdcpI2cRead( hdmi, buffer, 6 ) )
    {
        NV_DEBUG_PRINTF(( "Bksv read failure\n" ));
        goto fail;
    }

    tmp = 0;
    for( i = 0; i < 5; i++ )
    {
        /* buffer[0] is the address for Bksv */
        tmp = (((NvU64)buffer[i + 1]) << (i * 8)) | tmp;
    }

    hdmi->hdcp.Bksv = tmp;

    if( !DcHdcpVerifyKsvs( hdmi, &hdmi->hdcp.Bksv, 1 ) )
    {
        NV_ASSERT( !"Bksv verify failure" );
        goto fail;
    }

    /* LSB must be written first! */
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB_0,
        (NvU32)(hdmi->hdcp.Bksv & 0xFFFFFFFF) );
    tmp32 = NV_DRF_NUM( HDMI, NV_PDISP_RG_HDCP_BKSV_MSB,
                VALUE, (NvU32)(hdmi->hdcp.Bksv >> 32) )
          | NV_DRF_NUM( HDMI, NV_PDISP_RG_HDCP_BKSV_MSB,
                REPEATER, bRepeater );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_MSB_0, tmp32 );

    #define R0_RETRIES 10

    /* wait for R0 to be valid, wait N * R0_RETRIES */
    i = 0;
    do
    {
        ctrl = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0 );
        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, R0, ctrl ) != 0 )
        {
            break;
        }

        NvOsSleepMS( 10 );
    } while( i++ < R0_RETRIES );

    if( i >= R0_RETRIES )
    {
        NV_ASSERT( !"R0 failure" );
        goto fail;
    }

    #undef R0_RETRIES

    /* can't read R0' with in 100ms of writing Aksv */
    NvOsSleepMS( 100 );

    if( hdmi->hdcp.context->bHotplugDeasserted )
    {
        NV_DEBUG_PRINTF(( "HPD during downstream authentication\n" ));
        goto fail;
    }

    if( !DcHdcpVerifyLink( hdmi, NV_FALSE ) )
    {
        NV_DEBUG_PRINTF(( "R0' failure\n" ));
        goto fail;
    }

    DcHdcpEncryptEnable( cont, hdmi );

    /* check for repeaters */
    if( bRepeater )
    {
        if( !DcHdcpGetRepeaterInfo( hdmi ) )
        {
             goto fail;
        }

        hdmi->hdcp.context->State = NvDdkDispHdcpState_RepeaterVerif;
    }
    else
    {
        hdmi->hdcp.context->State = NvDdkDispHdcpState_LinkVerif;
        hdmi->hdcp.context->pkt.numBKSVs = 0;
        hdmi->hdcp.context->pkt.bStatus[0] = 0;
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

// FIXME: getting Sprime and Mprime has too much duplicated code.
NvBool
DcHdcpGetSprime( DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 i;
    NvU64 Dksv;
    NvU64 Cksv;
    NvU64 Cn;
    NvU32 sp_msb, sp_lsb1, sp_lsb2;

    Cksv = hdmi->hdcp.context->pkt.cKsv;
    Cn = hdmi->hdcp.context->pkt.cN;

    /* write Cn to hardware */
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CN_LSB_0,
        (NvU32)(Cn & 0xFFFFFFFF) );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CN_MSB_0,
        (NvU32)(Cn >> 32 ) );

    val = NV_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CMODE, MODE, READ_S )
        | NV_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CMODE, INDEX, TMDS0_LINK0 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CMODE_0, val );

    /* write cksv */
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CKSV_LSB_0,
        (NvU32)(Cksv & 0xFFFFFFFF) );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CKSV_MSB_0, (NvU32)(Cksv >> 32 ) );

    #define SPRIME_RETRIES 32

    /* wait for sprime to be valid */
    for( i = 0; i < SPRIME_RETRIES; i++ )
    {
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0 );
        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, SPRIME, val ) != 0 )
        {
            break;
        }

        NvOsSleepMS( 1 );
    }
    if( i == SPRIME_RETRIES )
    {
        goto fail;
    }

    #undef SPRIME_RETRIES

    /* Sprime is split into 3 registers - each has various and random bits
     * that could need inspection. Sprime itself really contains two
     * interesting values - status and Kp' - parse those out.
     */
    sp_msb = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_MSB_0 );
    sp_lsb1 = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB1_0 );
    sp_lsb2 = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB2_0 );

    /* status is 16 bits - 8 from sprime_msb, 8 from sprime_lsb2.
     * nvhdcp.h's status structure is 64 bits wide plus a display id for
     * future expansion.
     */
    val = NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_SPRIME_LSB2, VALUE, sp_lsb2 );
    hdmi->hdcp.context->pkt.hdcpStatus[0].status = (sp_msb << 8) |
        (sp_lsb2 >> 24);
    hdmi->hdcp.context->pkt.flFlags |= NV_HDCP_FLAGS_S;
    hdmi->hdcp.context->pkt.kP[0] = ((NvU64)val << 32) | sp_lsb1;
    hdmi->hdcp.context->pkt.flFlags |= NV_HDCP_FLAGS_KP;

    /* check for CS */
    if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_SPRIME_MSB, STATUS_CS, sp_msb ) )
    {
        NvU64 cs =
            (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CS_MSB_0)) << 32
            | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CS_LSB_0 );
        hdmi->hdcp.context->pkt.cS = cs;
        hdmi->hdcp.context->pkt.flFlags |= NV_HDCP_FLAGS_CS;
    }

    Dksv = (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_DKSV_MSB_0)) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_DKSV_LSB_0 );
    if( !DcHdcpVerifyKsvs( hdmi, &Dksv, 1 ) )
    {
        /* hardware probably doesn't have the upstream keys... */
        NV_ASSERT( !"invalid Dksv" );
        goto fail;
    }
    hdmi->hdcp.context->pkt.dKsv[0] = Dksv;
    hdmi->hdcp.context->pkt.flFlags |= NV_HDCP_FLAGS_DKSV;

    hdmi->hdcp.context->pkt.bKsv[0] = hdmi->hdcp.Bksv;
    hdmi->hdcp.context->pkt.flFlags |= NV_HDCP_FLAGS_BKSV;

    return NV_TRUE;

fail:
    return NV_FALSE;
}

NvBool
DcHdcpGetMprime( DcHdmi *hdmi )
{
    NvU32 val;
    NvU32 i;
    NvU64 Mprime;
    NvU64 Dksv;
    NvU64 Cksv;
    NvU64 Cn;

    Cksv = hdmi->hdcp.context->pkt.cKsv;
    Cn = hdmi->hdcp.context->pkt.cN;

    /* write Cn to hardware */
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CN_LSB_0,
        (NvU32)(Cn & 0xFFFFFFFF) );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CN_MSB_0,
        (NvU32)(Cn >> 32 ) );

    /* set Cmode for READ_M. use link1 (it's the default). not sure why
     * we don't use link0, but it seems to work.
     */
    val = NV_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CMODE, MODE, READ_M )
        | NV_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CMODE, INDEX, TMDS0_LINK1 );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CMODE_0, val );

    /* write cksv */
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CKSV_LSB_0,
        (NvU32)(Cksv & 0xFFFFFFFF) );
    /* note: this is the trigger for mprime calulation and must be written
     * after hdcp_cmode, Cn, and Cksv's LSB
     */
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_CKSV_MSB_0, (NvU32)(Cksv >> 32 ) );

    #define MPRIME_RETRIES 32

    /* wait for mprime to be valid */
    for( i = 0; i < MPRIME_RETRIES; i++ )
    {
        val = HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL_0 );
        if( NV_DRF_VAL( HDMI, NV_PDISP_RG_HDCP_CTRL, MPRIME, val ) != 0 )
        {
            break;
        }

        NvOsSleepMS( 1 );
    }
    if( i == MPRIME_RETRIES )
    {
        goto fail;
    }

    #undef MPRIME_RETRIES

    Mprime = (NvU64)HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_MPRIME_MSB_0) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_MPRIME_LSB_0 );
    hdmi->hdcp.context->pkt.mP = Mprime;

    Dksv = (NvU64)(HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_DKSV_MSB_0)) << 32
        | HDMI_READ( hdmi, HDMI_NV_PDISP_RG_HDCP_DKSV_LSB_0 );
    if( !DcHdcpVerifyKsvs( hdmi, &Dksv, 1 ) )
    {
        /* hardware probably doesn't have the upstream keys... */
        NV_ASSERT( !"invalid Dksv" );
        goto fail;
    }
    hdmi->hdcp.context->pkt.dKsv[0] = Dksv;
    hdmi->hdcp.context->pkt.flFlags |= NV_HDCP_FLAGS_DKSV;

    return NV_TRUE;

fail:
    return NV_FALSE;
}

static void
DcHdcpSecureRomTiming( DcController *cont, DcHdmi *hdmi )
{
    NvU32 prescale;
    NvU32 bit_period;
    NvU32 data_dly;
    NvU32 start_dly;
    NvU32 val;

    /* set hdcp secure rom timing -- see the formula in ardhmi.spec
     *
     * register values are generated with the following script to avoid
     * floating point to fixed point issues, execution speed, etc.
     *
     * #!/usr/bin/env tclsh
     * set dispclk_freq 74250
     * set rom_freq 200
     * set prescale [expr int(ceil( $dispclk_freq / $rom_freq / 256.0 )) - 1]
     * set bit_period [expr {
     *     int(ceil( $dispclk_freq / ($prescale + 1.0) / $rom_freq )) - 1 } ]
     * set data_dly [expr int(floor( ($bit_period + 1.0) * 3.0 / 4.0 )) - 1]
     * set start_dly [expr ( $bit_period >> 1 ) - 2]
     *
     * puts "HDMI.NV_PDISP_HDCPRIF_ROM_TIMING.PRESCALE $prescale"
     * puts "HDMI.NV_PDISP_HDCPRIF_ROM_TIMING.START_DLY $start_dly"
     * puts "HDMI.NV_PDISP_HDCPRIF_ROM_TIMING.DATA_DLY  $data_dly"
     * puts "HDMI.NV_PDISP_HDCPRIF_ROM_TIMING.BIT_PERIOD $bit_period"
     */
    switch( cont->panel_freq ) {
    case 148500:
        prescale = 2;
        start_dly = 121;
        data_dly = 185;
        bit_period = 247;
        break;
    case 74250:
        prescale = 1;
        start_dly = 90;
        data_dly = 138;
        bit_period = 185;
        break;
    case 27000:
        prescale = 0;
        start_dly = 65;
        data_dly = 100;
        bit_period = 134;
        break;
    case 25200:
    default:
        prescale = 0;
        start_dly = 60;
        data_dly = 93;
        bit_period = 125;
        break;
    }

    val = NV_DRF_NUM( HDMI, NV_PDISP_HDCPRIF_ROM_TIMING, PRESCALE, prescale )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDCPRIF_ROM_TIMING, START_DLY, start_dly )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDCPRIF_ROM_TIMING, DATA_DLY, data_dly )
        | NV_DRF_NUM( HDMI, NV_PDISP_HDCPRIF_ROM_TIMING, BIT_PERIOD,
            bit_period );
    HDMI_WRITE( hdmi, HDMI_NV_PDISP_HDCPRIF_ROM_TIMING_0, val );

}

static NvBool
DcHdcpInitialize( DcController *cont, DcHdmi *hdmi )
{
    NvError e = NvSuccess;
    NvU32 i;
    NvU32 nI2C = 0;
    NvOdmPeripheralConnectivity const *conn;
    DcHdmiOdm *adpt;
    NvDdkDispCapabilities *caps = cont->caps;

    adpt = hdmi->adaptation;
    if( !DcHdmiDiscover( hdmi ) )
    {
        goto fail;
    }

    /* find the i2c controller for hdmi connector, hdcp will
     * be the second address.
     */
    conn = hdmi->conn;
    hdmi->i2c = 0;
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface != NvOdmIoModule_I2c )
        {
            continue;
        }

        nI2C++;
        if( nI2C != 2 )
        {
            continue;
        }

        // fall back to RmI2c if no NvOdmDispHdmiI2cTransaction
        if( !adpt->i2cTransaction )
        {
            // FIXME: the instance param is a bug (400716)
            NV_CHECK_ERROR_CLEANUP(
                NvRmI2cOpen( cont->hRm,
                    NvOdmIoModule_I2c,
                    conn->AddressList[i].Instance,
                    &hdmi->i2c )
            );
            if( !hdmi->i2c )
            {
                goto fail;
            }
        }

        hdmi->hdcp_addr = conn->AddressList[i].Address;
        break;
    }

    if( !NVDDK_DISP_CAP_IS_SET( caps, NvDdkDispCapBit_Hdmi_Kfuse ) )
    {
        DcHdcpSecureRomTiming( cont, hdmi );
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

NvBool
DcHdcpOp( NvU32 controller, NvDdkDispHdcpContext *context, NvBool enable )
{
    DcHdmi *hdmi;
    DcController *cont;

    DC_CONTROLLER( controller, cont, return NV_FALSE );

    hdmi = &s_Hdmi;
    hdmi->hdcp.context = context;

    NV_ASSERT( cont );

    /* make sure hdmi is actually enabled */
    if( !hdmi->hdmi_regs )
    {
        return NV_FALSE;
    }

    // FIXME: AVMUTE?

    if( !enable )
    {
        NvU32 ctrl;

        ctrl = NV_RESETVAL( HDMI, NV_PDISP_RG_HDCP_CTRL );
        ctrl = NV_FLD_SET_DRF_DEF( HDMI, NV_PDISP_RG_HDCP_CTRL, RUN, NO,
            ctrl );

        DcHdcpWriteControl( cont, hdmi, ctrl );

        if( context )
        {
            context->State = NvDdkDispHdcpState_Unauthenticated;
        }

        /* clear out the hdcp registers */
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB_0, 0 );
        HDMI_WRITE( hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_MSB_0, 0 );

        // WORK-AROUND: there's at least one repeater in the wild (a
        // Yamaha RX-V1800) that does not clear enough of its internal
        // state when encryption is disabled such that a subsequent encryption
        // re-enable will fail unless there is some time with no video
        // signal.
        if( hdmi->hdcp.bRepeater )
        {
            NvU32 regs[3];
            NvU32 vals[3];
            NvU32 *tmp;

            tmp = regs;
            *tmp = DC_CMD_DISPLAY_COMMAND_0 * 4; tmp++;
            *tmp = DC_CMD_STATE_CONTROL_0 * 4; tmp++;
            *tmp = DC_CMD_STATE_CONTROL_0 * 4;

            tmp = vals;
            *tmp = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE,
                STOP );
            tmp++;
            *tmp = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_ACT_REQ,
                ENABLE );
            tmp++;
            *tmp = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_UPDATE, ENABLE );

            HOST_CLOCK_ENABLE( cont );

            NvRmHostModuleRegWr( cont->hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
                NV_ARRAY_SIZE(regs), regs, vals );

            NvOsSleepMS( 500 );

            tmp = vals;
            *tmp = NV_DRF_DEF( DC_CMD, DISPLAY_COMMAND, DISPLAY_CTRL_MODE,
                C_DISPLAY  );
            tmp++;
            *tmp = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_ACT_REQ,
                ENABLE );
            tmp++;
            *tmp = NV_DRF_DEF( DC_CMD, STATE_CONTROL, GENERAL_UPDATE, ENABLE );

            NvRmHostModuleRegWr( cont->hRm,
                NVRM_MODULE_ID( NvRmModuleID_Display, cont->index ),
                NV_ARRAY_SIZE(regs), regs, vals );

            HOST_CLOCK_DISABLE( cont );
        }

        NvOsMemset( &hdmi->hdcp, 0, sizeof(hdmi->hdcp) );

        if( hdmi->i2c )
        {
            NvRmI2cClose( hdmi->i2c );
            hdmi->i2c = 0;
        }
    }
    else
    {
        NV_ASSERT( context );

        if( context->pkt.cmdCommand == NV_HDCP_CMD_READ_LINK_STATUS )
        {
            if( context->State == NvDdkDispHdcpState_Unauthenticated )
            {
                goto fail;
            }

            if( !DcHdcpGetSprime( hdmi ) )
            {
                goto fail;
            }

            context->pkt.flFlags |= NV_HDCP_FLAGS_BSTATUS
                | NV_HDCP_FLAGS_BKSVLIST;
        }
        else if( context->State == NvDdkDispHdcpState_Unauthenticated )
        {
            /* wait for a frame. need to be sending video before enabling
             * hdcp (see the hdcp spec).
             */
            DcWaitFrame( cont );
            if( !DcHdcpInitialize( cont, hdmi ) )
            {
                goto fail;
            }

            if( !DcHdcpDownstream( cont, hdmi ) )
            {
                goto fail;
            }

            context->pkt.aKsv[0] = hdmi->hdcp.Aksv;
            context->pkt.aN[0] = hdmi->hdcp.An;
            context->pkt.flFlags |= NV_HDCP_FLAGS_AN | NV_HDCP_FLAGS_AKSV;
        }
        else
        {

            if( !DcHdcpVerifyLink( hdmi, NV_TRUE ) )
            {
                context->pkt.aKsv[0] = hdmi->hdcp.Aksv;
                context->pkt.aN[0] = hdmi->hdcp.An;
                context->pkt.flFlags |= NV_HDCP_FLAGS_AN | NV_HDCP_FLAGS_AKSV;
                goto fail;
            }

            if( context->State == NvDdkDispHdcpState_RepeaterVerif )
            {
                if( context->pkt.cmdCommand == NV_HDCP_CMD_VALIDATE_LINK )
                {
                    if( !DcHdcpGetMprime( hdmi ) )
                    {
                        goto fail;
                    }

                    context->pkt.flFlags |= NV_HDCP_FLAGS_BSTATUS
                        | NV_HDCP_FLAGS_BKSVLIST | NV_HDCP_FLAGS_V
                        | NV_HDCP_FLAGS_MP;
                }
            }
            else if( context->State == NvDdkDispHdcpState_RepeaterOk )
            {
                context->State = NvDdkDispHdcpState_LinkVerif;
            }
        }
    }

    return NV_TRUE;

fail:
    /* recursive! */
    context->State = NvDdkDispHdcpState_Unauthenticated;
    DcHdcpOp( controller, 0, NV_FALSE );
    return NV_FALSE;
}

/* adaptation stuff */
static const char *NVODM_HDMI_I2C_FUNC = "NvOdmDispHdmiI2cTransaction";
static const char *NVODM_HDMI_I2C_OPEN_FUNC = "NvOdmDispHdmiI2cOpen";
static const char *NVODM_HDMI_I2C_CLOSE_FUNC = "NvOdmDispHdmiI2cClose";
static const char *NVODM_HDMI_KSV_FUNC = "NvOdmDispHdcpIsRevokedKsv";
static const char *NVODM_HDMI_LIB = "libnvodm_hdmi";

static DcHdmiOdm s_Adpt;

DcHdmiOdm *
DcHdmiOdmOpen( void )
{
    NvError e;
    NvU32 bypass;
    DcHdmiOdm *a = &s_Adpt;
    NvOdmDispDeviceHandle   h_hdmi_Odm;
    NvBool  ret_val;
    NvU64 guid;

    if( a->refcount )
    {
        a->refcount++;
        return a;
    }

    // FIXME: this is an ugly config key name
    e = NvOsGetConfigU32( "ByPassHdmiDll", &bypass );
    if( (e != NvSuccess) || (!bypass) )
    {
        e = NvOsLibraryLoad( NVODM_HDMI_LIB, &a->hLib );
        if( e != NvSuccess )
        {
            NvOsMemset( a, 0, sizeof(DcHdmiOdm) );
            goto clean;
        }

        a->i2cTransaction = (NvOdmDispHdmiI2cTransactionType)
            NvOsLibraryGetSymbol( a->hLib, NVODM_HDMI_I2C_FUNC );

        a->i2cOpen = (NvOdmDispHdmiI2cOpenType)
            NvOsLibraryGetSymbol( a->hLib, NVODM_HDMI_I2C_OPEN_FUNC );

        if( a->i2cOpen )
        {
            (*(a->i2cOpen))();
        }

        a->i2cClose = (NvOdmDispHdmiI2cCloseType)
            NvOsLibraryGetSymbol( a->hLib, NVODM_HDMI_I2C_CLOSE_FUNC );

        a->hdcpIsRevokedKsv = (NvOdmDispHdcpIsRevokedKsvType)
            NvOsLibraryGetSymbol( a->hLib, NVODM_HDMI_KSV_FUNC );
    }
clean:
    NvOdmDispGetDefaultGuid(&guid);
    ret_val = NvOdmDispGetDeviceByGuid(guid, &h_hdmi_Odm);
    if ( ( NV_TRUE == ret_val ) &&
         ( NULL != h_hdmi_Odm ) )
    {
        a->tmdsDriveTerm = (NvOdmDispHdmiDriveTermTbl*)
            NvOdmDispGetConfiguration( h_hdmi_Odm );
    }

    a->refcount = 1;
    return a;
}

void
DcHdmiOdmClose( DcHdmiOdm *adpt )
{
    if( !adpt || !adpt->refcount || !adpt->hLib )
    {
        return;
    }

    adpt->refcount--;

    if( adpt->refcount == 0 )
    {
        if( adpt->i2cClose )
        {
            (*(adpt->i2cClose))();
        }

        if( adpt->hLib )
        {
            NvOsLibraryUnload( adpt->hLib );
        }

        NvOsMemset( adpt, 0, sizeof(DcHdmiOdm) );
    }
}
