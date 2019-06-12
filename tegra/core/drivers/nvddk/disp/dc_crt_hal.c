/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "dc_crt_hal.h"
#include "nvrm_drf.h"
#include "nvrm_pinmux.h"
#include "nvrm_analog.h"
#include "nvodm_services.h"
#include "nvodm_disp.h"

static NvOdmDispDeviceMode default_mode[] =
{
    // VESA Est1 640x480@60Hz
    { 640, // width
      480, // height
      24, // bpp
      0, // refresh
      25175, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE ),
      // timing
      { 8, // h_ref_to_sync
        2, // v_ref_to_sync
        96, // h_sync_width
        2, // v_sync_width
        40 + 8, // h_back_porch
        25 + 8, // v_back_porch
        640, // h_disp_active
        480, // v_disp_active
        8 + 8, // h_front_porch
        8 + 2, // v_front_porch
      },
      NV_FALSE // partial
    }
};

static NvOdmDispDeviceMode crt_modes[] =
{
    // VESA Est1 640x480@60Hz
    { 640, // width
      480, // height
      24, // bpp
      0, // refresh
      25175, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE ),
      // timing
      { 8, // h_ref_to_sync
        2, // v_ref_to_sync
        96, // h_sync_width
        2, // v_sync_width
        40 + 8, // h_back_porch
        25 + 8, // v_back_porch
        640, // h_disp_active
        480, // v_disp_active
        8 + 8, // h_front_porch
        8 + 2, // v_front_porch
      },
      NV_FALSE // partial
    },

    // VESA #900602
    { 800, // width
      600, // height
      24, // bpp
      0, // refresh
      40000, // frequency in khz
      0, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        128, // h_sync_width
        4, // v_sync_width
        88, // h_back_porch
        23, // v_back_porch
        800, // h_disp_active
        600, // v_disp_active
        40, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    },

    // BIOS 0x119 1280x1024@60Hz
    { 1280, // width
      1024, // height
      24, // bpp
      0, // refresh
      108000, // frequency in khz
      0, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        112, // h_sync_width
        3, // v_sync_width
        248, // h_back_porch
        38, // v_back_porch
        1280, // h_disp_active
        1024, // v_disp_active
        48, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    },

    // D1h, 00h -- 1920x1200
    { 1920, // width
      1200, // height
      24, // bpp
      0, // refresh
      193250, // frequency in khz
      0, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        200, // h_sync_width
        6, // v_sync_width
        336, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1200, // v_disp_active
        136, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static DcCrt s_Crt;

static NvBool DcCrtDiscover( DcCrt *crt );
static void DcCrtInit( void );
static void DcCrtDeinit( void );

/* get the modes from the display device itself */
static NvOdmDispDeviceMode edid_modes[NVDDK_DISP_MAX_MODES];
static NvU32 edid_modes_num;

static void DcCrtSync( DcController *cont )
{
    NvU32 val;

    /* the hardware is a little broken. there is no polarity control for
     * the h and v sync singals to the tvdac, so have to use the lvp and lhp
     * signals to generate the h and v sync since they have polarity controls.
     */

    val = cont->signal_options0;
    val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_SIGNAL_OPTIONS0,
        H_PULSE0_ENABLE, ENABLE, val );
    val = NV_FLD_SET_DRF_DEF( DC_DISP, DISP_SIGNAL_OPTIONS0,
        V_PULSE0_ENABLE, ENABLE, val );
    cont->signal_options0 = val;
    DC_WRITE( cont, DC_DISP_DISP_SIGNAL_OPTIONS0_0, val );

    val = NV_DRF_DEF( DC_DISP, H_PULSE0_CONTROL, H_PULSE0_MODE, NORMAL )
        | NV_DRF_DEF( DC_DISP, H_PULSE0_CONTROL, H_PULSE0_POLARITY, HIGH )
        | NV_DRF_DEF( DC_DISP, H_PULSE0_CONTROL, H_PULSE0_V_QUAL, ALWAYS )
        | NV_DRF_DEF( DC_DISP, H_PULSE0_CONTROL, H_PULSE0_LAST, END_A );

    if( (cont->mode.flags & NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, H_PULSE0_CONTROL,
            H_PULSE0_POLARITY, LOW, val );
    }

    DC_WRITE( cont, DC_DISP_H_PULSE0_CONTROL_0, val );

    val = NV_DRF_NUM( DC_DISP, H_PULSE0_POSITION_A,
            H_PULSE0_START_A, cont->mode.timing.Horiz_RefToSync )
        | NV_DRF_NUM( DC_DISP, H_PULSE0_POSITION_A,
            H_PULSE0_END_A, cont->mode.timing.Horiz_RefToSync +
                cont->mode.timing.Horiz_SyncWidth );
    DC_WRITE( cont, DC_DISP_H_PULSE0_POSITION_A_0, val );

    val = NV_DRF_DEF( DC_DISP, V_PULSE0_CONTROL, V_PULSE0_POLARITY, HIGH )
        | NV_DRF_DEF( DC_DISP, V_PULSE0_CONTROL, V_PULSE0_DELAY, NODELAY )
        | NV_DRF_DEF( DC_DISP, V_PULSE0_CONTROL, V_PULSE0_LAST, END_A );
    if( (cont->mode.flags & NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE) )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, V_PULSE0_CONTROL,
            V_PULSE0_POLARITY, LOW, val );
    }
    DC_WRITE( cont, DC_DISP_V_PULSE0_CONTROL_0, val );

    val = NV_DRF_NUM( DC_DISP, V_PULSE0_POSITION_A,
            V_PULSE0_START_A, cont->mode.timing.Vert_RefToSync )
        | NV_DRF_NUM( DC_DISP, V_PULSE0_POSITION_A,
            V_PULSE0_END_A, cont->mode.timing.Vert_RefToSync +
                cont->mode.timing.Vert_SyncWidth );
    DC_WRITE( cont, DC_DISP_V_PULSE0_POSITION_A_0, val );
}

void DcCrtEnable( NvU32 controller, NvBool enable )
{
    NvError e;
    DcController *cont;
    NvRmAnalogTvDacConfig cfg;
    NvRmAnalogInterface interface;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    interface = NVRM_ANALOG_INTERFACE( NvRmAnalogInterface_Tv, 0 );
    if( enable )
    {
        NvU32 val;

        DcCrtSync( cont );

        /* send h/v sync to the tvdac */
        val = NV_DRF_DEF( DC_DISP, DAC_CRT_CTRL, NOTBLANK_SELECT, DE )
            | NV_DRF_DEF( DC_DISP, DAC_CRT_CTRL, SYNC_SELECT, LVP0_LHP0 );
        DC_WRITE( cont, DC_DISP_DAC_CRT_CTRL_0, val );

        /* enable the tvdac -- use display as the data source.
         * display's timing must exactly match the vesa spec for the given
         * resolution
         */
        cfg.Source = NVRM_MODULE_ID( NvRmModuleID_Display, cont->index );
        cfg.DacAmplitude = NvRmAnalogGetTvDacConfiguration(cont->hRm, 
            NvRmAnalogTvDacType_CRT);
        if (cfg.DacAmplitude == 0)
        {
            cfg.DacAmplitude = 0x4C;
        }
        NV_CHECK_ERROR_CLEANUP(
            NvRmAnalogInterfaceControl( cont->hRm, interface, NV_TRUE,
                &cfg, sizeof(cfg) )
        );

        /* make sure memory is running fast enough */
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerBusyHint( cont->hRm, NvRmDfsClockId_Emc,
                cont->PowerClientId, NV_WAIT_INFINITE, NvRmFreqMaximum )
        );
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            NvRmAnalogInterfaceControl( cont->hRm, interface, NV_FALSE,
                0, 0 )
        );

        /* cancel the busy hint */
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerBusyHint( cont->hRm, NvRmDfsClockId_Emc,
                cont->PowerClientId, 0, 0 )
        );

        edid_modes_num = 0;
    }

    return;

fail:
    interface = NVRM_ANALOG_INTERFACE( NvRmAnalogInterface_Tv, 0 );
    NV_ASSERT_SUCCESS(
        NvRmAnalogInterfaceControl( cont->hRm, interface, NV_FALSE, 0, 0 )
    );

    cont->bFailed = NV_TRUE;
}

void DcCrtListModes( NvU32 *nModes, NvOdmDispDeviceMode *modes )
{
    NvOdmDispDeviceMode *m;
    NvU32 len;

    NV_ASSERT( nModes );

    if( edid_modes_num )
    {
        m = edid_modes;
        len = edid_modes_num;
    }
    else
    {
        m = crt_modes;
        len = NV_ARRAY_SIZE(crt_modes);
    }

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

void DcCrtSetPowerLevel( NvOdmDispDevicePowerLevel level )
{
    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        DcCrtDeinit();
        break;
    case NvOdmDispDevicePowerLevel_On:
        DcCrtInit();
        break;
    default:
        break;
    }
}

void DcCrtGetParameter( NvOdmDispParameter param, NvU32 *val )
{
    switch( param ) {
    case NvOdmDispParameter_Type:
        *val = NvOdmDispDeviceType_CRT;
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

NvU64 DcCrtGetGuid( void )
{
    DcCrt *crt;
    crt = &s_Crt;

    if( !crt->conn )
    {
        if( !DcCrtDiscover( crt ) )
        {
            return (NvU64)-1;
        }
    }

    return crt->guid;
}

void DcCrtSetEdid( NvDdkDispEdid *edid, NvU32 flags )
{
    DcCrt *crt;
    crt = &s_Crt;
    crt->edid = edid;

    if( flags & NVDDK_DISP_SET_EDID_FLAG_USE_DEFAULT)
    {
        edid_modes_num = 1;
        NvOsMemcpy( &edid_modes[0], &default_mode[0], sizeof (NvOdmDispDeviceMode));
        return;
    }

    if( !edid && !flags )
    {
        flags = NVDDK_DISP_SET_EDID_FLAG_USE_BUILTIN;
    }

    if( !edid && (flags & NVDDK_DISP_SET_EDID_FLAG_USE_BUILTIN) )
    {
        edid_modes_num = 0;
        return;
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
    if( edid_modes_num )
    {
        NvDdkDispEdidExport( edid, &edid_modes_num, edid_modes, flags );
    }
}

NvBool
DcCrtDiscover( DcCrt *crt )
{
    NvU64 guid;
    const NvU32 vals[] =
        {
            NvOdmPeripheralClass_Display,
            NvOdmIoModule_Crt,
        };
    const NvOdmPeripheralSearch search[] =
        {
            NvOdmPeripheralSearch_PeripheralClass,
            NvOdmPeripheralSearch_IoModule,
        };
    NvOdmPeripheralConnectivity const *conn;

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

    crt->conn = conn;
    crt->guid = guid;

    return NV_TRUE;
}

void DcCrtInit( void )
{
    NvU32 i;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvU32 settle_us;
    DcCrt *crt;

    crt = &s_Crt;
    if( !crt->conn )
    {
        if( !DcCrtDiscover( crt ) )
        {
            return;
        }
    }

    /* enable the power rail */
    conn = crt->conn;
    hPmu = NvOdmServicesPmuOpen();
    if( !conn || !hPmu )
    {
        NV_ASSERT( !"broken odm services" );
        return;
    }

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
}

void
DcCrtDeinit( void )
{
    NvU32 i;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    DcCrt *crt;

    crt = &s_Crt;

    /* enable the power rail */
    hPmu = NvOdmServicesPmuOpen();
    conn = crt->conn;
    if( !conn || !hPmu )
    {
        NV_ASSERT( !"broken odm services" );
        return;
    }

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
}
