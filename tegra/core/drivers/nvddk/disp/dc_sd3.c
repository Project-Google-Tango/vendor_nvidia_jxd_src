/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvfxmath.h"
#include "nvrm_analog.h"
#include "nvrm_module.h"
#include "nvrm_memmgr.h"
#include "nvrm_interrupt.h"
#include "nvrm_pinmux.h"
#include "nvrm_pmu.h"
#include "nvddk_disp_hw.h"
#include "dc_sd3.h"
#include "dc_hal.h"
#include "nvrm_hardware_access.h"

#define SD_LOG_ARRAY_SIZE   10

static SmartDimmerLogEntry  SDLogArray[SD_LOG_ARRAY_SIZE];
static NvU32                SDLogEntryCount              = 0;

void
showSmartDimmerLog( NvU32 controller );

/* Note on hw spec: this always uses the Window B spec (ardispay_b.h)
 * since it is the most fully-featured (all the defines are there),
 * and every window has identical register layouts.
 */

void DcHal_HwSmartDimmerConfig( NvU32 controller,
    NvDdkSmartDimmerSettings *sdSettings, NvBool *manual )
{
    DcController *cont;
    NvU32 i;
    NvU32 val, reg;

    DC_CONTROLLER( controller, cont, return );
    DC_SAFE( cont, return );

    if( !NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_SmartDimmer ) )
    {
        NV_ASSERT( !"smart dimmer not supported" );
        cont->bFailed = NV_TRUE;
        return;
    }

    if( !sdSettings->Enable )
    {
        val = NV_DRF_DEF( DC_DISP, SD_CONTROL, SD_ENABLE, DISABLE );
        DC_WRITE( cont, DC_DISP_SD_CONTROL_0, val );
        return;
    }

    /* write the LUT and the transfer function for linear response */
    for( i = 0; i < 9; i++ )
    {
        // from the smart dimmer IAS
        NvU32 t = (4096 / (8 + i)) - 256;
        if( t > 255 ) t = 255;

        val = NV_DRF_NUM( DC_DISP, SD_LUT, R_LUT, t )
            | NV_DRF_NUM( DC_DISP, SD_LUT, G_LUT, t )
            | NV_DRF_NUM( DC_DISP, SD_LUT, B_LUT, t );
        DC_WRITE( cont, DC_DISP_SD_LUT_0 + i, val );
    }

    for( i = 0; i < 4; i++ )
    {
        // only program the top half of the curve since the values of K
        // can only be 128-255.
        NvU32 base = 128 + (i * 32);
        val = NV_DRF_NUM( DC_DISP, SD_BL_TF, POINT_0, base )
            | NV_DRF_NUM( DC_DISP, SD_BL_TF, POINT_1, base + 8 )
            | NV_DRF_NUM( DC_DISP, SD_BL_TF, POINT_2, base + 16 )
            | NV_DRF_NUM( DC_DISP, SD_BL_TF, POINT_3, base + 24 );

        DC_WRITE( cont, DC_DISP_SD_BL_TF_0 + i, val );
    }

    val = NV_DRF_NUM( DC_DISP, SD_CSC_COEFF, R_COEFF, sdSettings->Coeff_R )
        | NV_DRF_NUM( DC_DISP, SD_CSC_COEFF, G_COEFF, sdSettings->Coeff_G )
        | NV_DRF_NUM( DC_DISP, SD_CSC_COEFF, B_COEFF, sdSettings->Coeff_B );
    DC_WRITE( cont, DC_DISP_SD_CSC_COEFF_0, val );

    val = NV_DRF_NUM( DC_DISP, SD_BL_PARAMETERS, TIME_CONSTANT,
            sdSettings->BlTimeConstant )
        | NV_DRF_NUM( DC_DISP, SD_BL_PARAMETERS, STEP,
            sdSettings->BlStep );
    DC_WRITE( cont, DC_DISP_SD_BL_PARAMETERS_0, val );

    if( NVDDK_DISP_CAP_IS_SET( cont->caps,
            NvDdkDispCapBit_Bug_SmartDimmer_AutomaticMode ) ||
        cont->PwmOutputPin == DcPwmOutputPin_Unspecified )
    {
        val = NV_DRF_DEF( DC_DISP, SD_BL_CONTROL, BL_MODE, MANUAL );
        *manual = NV_TRUE;
    }
    else
    {
        val = NV_DRF_DEF( DC_DISP, SD_BL_CONTROL, BL_MODE, PWM_AUTO );
        *manual = NV_FALSE;
    }
    DC_WRITE( cont, DC_DISP_SD_BL_CONTROL_0, val );

    val = NV_DRF_NUM( DC_DISP, SD_FLICKER_CONTROL, TIME_LIMIT,
            sdSettings->FlickerTimeLimit )
        | NV_DRF_NUM( DC_DISP, SD_FLICKER_CONTROL, THRESHOLD,
            sdSettings->FlickerThreshold );
    DC_WRITE( cont, DC_DISP_SD_FLICKER_CONTROL_0, val );

    //
    // Start writing vals for SD_CONTROL
    //
    if ( sdSettings->Enable == 2 ) // OneShot
    {
        val = NV_DRF_DEF( DC_DISP, SD_CONTROL, SD_ENABLE, ONE_SHOT );
        val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, SD_ONE_SHOT, ENABLE, val );
    }
    else // Default Enable
    {
        val = NV_DRF_DEF( DC_DISP, SD_CONTROL, SD_ENABLE, ENABLE );
    }

    val |= NV_DRF_NUM( DC_DISP, SD_CONTROL, AGGRESSIVENESS,
                sdSettings->Aggressiveness ) |
           NV_DRF_NUM( DC_DISP, SD_CONTROL, HW_UPDATE_DLY,
                sdSettings->HwUpdateDelay );

    if( sdSettings->bEnableVidLum )
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, USE_VID_LUMA,
                ENABLE, val );
    }
    else
    {
        val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, USE_VID_LUMA,
                DISABLE, val );
    }

    // Automatic bin width setting.
    if( sdSettings->BinWidth == 0 )
    {
        switch( sdSettings->Aggressiveness )
        {
            case 5:
                val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, FOUR, val );
                break;
            case 2: case 3: case 4:
                val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, TWO, val );
                break;
            case 0: case 1:
            default:
                val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, ONE, val );
                break;
        }
    }
    else
    {
        switch( sdSettings->BinWidth )
        {
            case 2:
                 val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, TWO, val );
                 break;
            case 3:
                 val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, FOUR, val );
                 break;
            case 4:
                 val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, EIGHT, val );
                 break;
            case 1:   // Case 1 IS the default.
            default:
                 val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, BIN_WIDTH, ONE, val );
                 break;
        }
    }

    // If we're using manual K, we don't care about the skip-first workaround.
    if( sdSettings->bEnableManK )
    {
        NvU32 tval = 0;
        tval = NV_DRF_NUM( DC_DISP, SD_MAN_K_VALUES,
                           MAN_K_RED, (sdSettings->ManK_R & 0x3FF) ) |
               NV_DRF_NUM( DC_DISP, SD_MAN_K_VALUES,
                           MAN_K_GREEN, (sdSettings->ManK_G & 0x3FF) ) |
               NV_DRF_NUM( DC_DISP, SD_MAN_K_VALUES,
                           MAN_K_BLUE, (sdSettings->ManK_B & 0x3FF) );
        DC_WRITE( cont, DC_DISP_SD_MAN_K_VALUES_0, tval );

        val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, SD_CORRECTION_MODE, MANUAL, val );
    }
    else if( NVDDK_DISP_CAP_IS_SET( cont->caps,
             NvDdkDispCapBit_Bug_SmartDimmer_Histogram_Reset ))
    {
        NvU32 tval = 0;

        // Lock around the register read/write.
        NvOsMutexLock( cont->sdMutex );
        // Read the register to check for disable->enable transition.
        reg = DC_DISP_SD_CONTROL_0 * 4;
        DcRegRd( cont, 1, &reg, &tval );

        if (!NV_DRF_VAL(DC_DISP, SD_CONTROL, SD_ENABLE, tval))
        {
            // First, make sure next SD event will be skipped.
            cont->bSkipNext = NV_TRUE;
            // Set all three "manual K" values to 1.0
            DC_WRITE( cont, DC_DISP_SD_MAN_K_VALUES_0, 0 );
            // Set control to use manual K values.
            val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL, SD_CORRECTION_MODE, MANUAL, val );
        }
        NvOsMutexUnlock( cont->sdMutex );
    }

    // Lock around writes/reads to this register.
    NvOsMutexLock( cont->sdMutex);
    DC_WRITE( cont, DC_DISP_SD_CONTROL_0, val );
    NvOsMutexUnlock( cont->sdMutex );
}

void
DcHal_HwSmartDimmerBrightness( NvU32 controller, NvU8 *brightness )
{
    DcController *cont;
    NvU32 val;
    NvU32 reg;

    DC_CONTROLLER( controller, cont, return );

    if( !NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_SmartDimmer ) )
    {
        NV_ASSERT( !"smart dimmer not supported" );
        cont->bFailed = NV_TRUE;
        return;
    }

    // Check for skip-next WAR.
    if( cont->bSkipNext == NV_TRUE )
    {
        // Lock around access to the register/variable.
        NvOsMutexLock( cont->sdMutex );
        cont->bSkipNext = NV_FALSE;

        // If we're turning off the skip WAR, set our correction mode to auto.
        reg = DC_DISP_SD_CONTROL_0 * 4;
        DcRegRd( cont, 1, &reg, &val );
        val = NV_FLD_SET_DRF_DEF( DC_DISP, SD_CONTROL,
                                  SD_CORRECTION_MODE, AUTO_CORRECT, val );
        DC_WRITE( cont, DC_DISP_SD_CONTROL_0, val );

        NvOsMutexUnlock( cont->sdMutex );

        // Brightness should not be adjusted.
        *brightness = 0;
        return;
    }

    reg = DC_DISP_SD_BL_CONTROL_0 * 4;
    DcRegRd( cont, 1, &reg, &val );

    *brightness = (NvU8)NV_DRF_VAL( DC_DISP, SD_BL_CONTROL, BRIGHTNESS, val );
}

void
showSmartDimmerLog( NvU32 controller )
{
    DcController           *cont;
    SmartDimmerLogEntry    *thisEntry;
    NvU32                  val;
    NvU32                  i;
    NvU32                  start, current, end, loopIndex = 0;

    DC_CONTROLLER( controller, cont, return );

    if( !NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_SmartDimmer ) )
    {
         NV_ASSERT( !"smart dimmer not supported" );
         cont->bFailed = NV_TRUE;
         return;
    }

    if( !SDLogEntryCount )
    {
        NvOsDebugPrintf( "No SmartDimmer activity has been recorded.\n" );
        return;
    }


    // Get the first entry.
    if( SDLogEntryCount <= SD_LOG_ARRAY_SIZE )
    {
        start = 0;
        end = SDLogEntryCount - 1;
    }
    else
    {
        start = SDLogEntryCount % SD_LOG_ARRAY_SIZE;
        end = (SDLogEntryCount - 1) % SD_LOG_ARRAY_SIZE;
    }

    current = start;
    thisEntry = &(SDLogArray[current]);

    NvOsDebugPrintf( "\nConstant Values:\n" );
    // Print the LUT/TF only once.
    NvOsDebugPrintf( "SD_LUT = \tR_LUT\tG_LUT\tB_LUT\n" );
    for( i = 0; i < SD_LUT_SIZE; i++ )
    {
        val = thisEntry->SD_LUT[i];

        NvOsDebugPrintf( "%d:\t\t 0x%02x\t 0x%02x\t 0x%02x\n", i,
                 NV_DRF_VAL( DC_DISP, SD_LUT, R_LUT, val ),
                 NV_DRF_VAL( DC_DISP, SD_LUT, G_LUT, val ),
                 NV_DRF_VAL( DC_DISP, SD_LUT, B_LUT, val ));
    }

    NvOsDebugPrintf( "\nSD_BL_TF = \t PT_0\t PT_1\t PT_2\t PT_3\n" );
    for( i = 0; i < SD_BL_TF_SIZE; i++ )
    {
        val = thisEntry->SD_BL_TF[i];
        NvOsDebugPrintf( "%d:\t\t 0x%02x\t 0x%02x\t 0x%02x\t 0x%02x\n", i,
                 NV_DRF_VAL( DC_DISP, SD_BL_TF, POINT_0, val ),
                 NV_DRF_VAL( DC_DISP, SD_BL_TF, POINT_1, val ),
                 NV_DRF_VAL( DC_DISP, SD_BL_TF, POINT_2, val ),
                 NV_DRF_VAL( DC_DISP, SD_BL_TF, POINT_3, val ));
    }

    NvOsDebugPrintf( "\nTotal SD3 activities count: %d\n", SDLogEntryCount );
    while( 1 )
    {
        NvOsDebugPrintf( "Entry(%d) Info:\n", loopIndex++ );

        NvOsDebugPrintf( "SD_CONTROL = 0x%08X\n", thisEntry->SD_CONTROL );
        NvOsDebugPrintf( "SD_BL_CONTROL = 0x%08X\n", thisEntry->SD_BL_CONTROL );

        NvOsDebugPrintf( "SD_CSC_COEFF = 0x%08X\n", thisEntry->SD_CSC_COEFF );
        NvOsDebugPrintf( "SD_FLICKER_CONTROL = 0x%08X\n", thisEntry->SD_FLICKER_CONTROL );
        NvOsDebugPrintf( "SD_PIXEL_COUNT = 0x%08X\n", thisEntry->SD_PIXEL_COUNT );
        NvOsDebugPrintf( "SD_BL_PARAMETERS = 0x%08X\n", thisEntry->SD_BL_PARAMETERS );

        NvOsDebugPrintf( "SD_HW_K_VALUES = 0x%08x\n", thisEntry->SD_HW_K_VALUES );
        NvOsDebugPrintf( "\nSD_HISTOGRAM = \tBIN_0\tBIN_1\tBIN_2\tBIN_3\n" );
        for( i = 0; i < SD_HISTO_SIZE; i++ )
        {
            val = thisEntry->SD_HISTOGRAM[i];
            NvOsDebugPrintf( "%d:\t\t 0x%02x\t 0x%02x\t 0x%02x\t 0x%02x\n", i,
                     NV_DRF_VAL( DC_DISP, SD_HISTOGRAM, BIN_0, val ),
                     NV_DRF_VAL( DC_DISP, SD_HISTOGRAM, BIN_1, val ),
                     NV_DRF_VAL( DC_DISP, SD_HISTOGRAM, BIN_2, val ),
                     NV_DRF_VAL( DC_DISP, SD_HISTOGRAM, BIN_3, val ));
        }

        NvOsDebugPrintf( "\nInput Backlight Intensity = %d\n", thisEntry->InBacklightIntensity);
        NvOsDebugPrintf( "Output Backlight Intensity = %d\n", thisEntry->OutBacklightIntensity);
        NvOsDebugPrintf( "PWM frequence = %4.2f, SD percentage = %4.2f\n\n",
                          (100 * (float)thisEntry->OutBacklightIntensity/255),
                          (100 * (float)thisEntry->OutBacklightIntensity/(float)thisEntry->InBacklightIntensity));

        if (current == end)
            break;

        current = (current + 1) % SD_LOG_ARRAY_SIZE;
        thisEntry = &(SDLogArray[current]);
    }
}

void
DcHal_HwSmartDimmerLog( NvU32 controller, NvU32 InIntensity, NvU32 OutIntensity, NvBool show )
{
    DcController           *cont;
    SmartDimmerLogEntry    *thisEntry;
    NvU32                  val;
    NvU32                  reg;
    NvU32                  i;

    DC_CONTROLLER( controller, cont, return );

    if( !NVDDK_DISP_CAP_IS_SET( cont->caps, NvDdkDispCapBit_SmartDimmer ) )
    {
         NV_ASSERT( !"smart dimmer not supported" );
         cont->bFailed = NV_TRUE;
         return;
    }

    if( show )
    {
        showSmartDimmerLog( controller );
        return;
    }

    thisEntry = &(SDLogArray[SDLogEntryCount%SD_LOG_ARRAY_SIZE]);

    thisEntry->InBacklightIntensity = InIntensity;
    thisEntry->OutBacklightIntensity = OutIntensity;

    reg = DC_DISP_SD_CONTROL_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_CONTROL = val;

    reg = DC_DISP_SD_BL_CONTROL_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_BL_CONTROL = val;

    reg = DC_DISP_SD_CSC_COEFF_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_CSC_COEFF = val;

    reg = DC_DISP_SD_FLICKER_CONTROL_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_FLICKER_CONTROL = val;

    reg = DC_DISP_SD_PIXEL_COUNT_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_PIXEL_COUNT = val;

    reg = DC_DISP_SD_BL_PARAMETERS_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_BL_PARAMETERS = val;

    reg = DC_DISP_SD_HW_K_VALUES_0 * 4;
    DcRegRd( cont, 1, &reg, &val );
    thisEntry->SD_HW_K_VALUES = val;

    for( i = 0; i < SD_LUT_SIZE; i++ )
    {
        reg = (DC_DISP_SD_LUT_0 + i) * 4;
        DcRegRd( cont, 1, &reg, &val );
        thisEntry->SD_LUT[i] = val;
    }

    for( i = 0; i < SD_HISTO_SIZE; i++ )
    {
        reg = (DC_DISP_SD_HISTOGRAM_0 + i) * 4;
        DcRegRd( cont, 1, &reg, &val );
        thisEntry->SD_HISTOGRAM[i] = val;
    }

    for( i = 0; i < SD_BL_TF_SIZE; i++ )
    {
        reg = (DC_DISP_SD_BL_TF_0 + i) * 4;
        DcRegRd( cont, 1, &reg, &val );
        thisEntry->SD_BL_TF[i] = val;
    }

    SDLogEntryCount++;

}


