/*
 * Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "panels.h"
#include "nvodm_services.h"
#include "nvodm_backlight.h"
#include "panel_sharp_dsi.h"
#include "panel_panasonic.h"
#include "panel_lgd.h"
#include "panel_hdmi.h"
#include "panel_auo_dsi.h"

NvOdmDispDevice g_PrimaryDisp;

static NvBool s_init = NV_FALSE;
extern void
NvOsDebugPrintf( const char *format, ... );

void
NvOdmDispListDevices( NvU32 *nDevices, NvOdmDispDeviceHandle *phDevices )
{
    NvBool b;
    NvU32 count;
    NvOdmDisplayBoardInfo DisplayBoardInfo;

    if( !s_init )
    {
        if (NvOdmQueryIsHdmiPrimaryDisplay())
            hdmi_GetHal( &g_PrimaryDisp );
        else if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
            &DisplayBoardInfo, sizeof(DisplayBoardInfo)))
        {
            switch(DisplayBoardInfo.BoardInfo.BoardID) {
            case BOARD_E1639:
            case BOARD_E1813:
                sharpDsi_GetHal(&g_PrimaryDisp);
                break;

            case BOARD_E1797:
            case BOARD_E1627:
                panasonic_GetHal(&g_PrimaryDisp);
                break;

            case BOARD_E1549:
                lgd_GetHal(&g_PrimaryDisp);
                break;

            case BOARD_P1761:
            case BOARD_E1807:
            case BOARD_E1937:
                auoDsi_GetHal(&g_PrimaryDisp);
                break;

            default:
                sharpDsi_GetHal(&g_PrimaryDisp);
                break;
            }
        }
        b = g_PrimaryDisp.Setup( &g_PrimaryDisp );
        NV_ASSERT( b );
        b = b; // Otherwise release builds get 'unused variable'.

        s_init = NV_TRUE;
    }

    if( !(*nDevices) )
    {
        *nDevices = 1;
    }
    else
    {
        NV_ASSERT( phDevices );
        count = *nDevices;

        *phDevices = &g_PrimaryDisp;
        phDevices++;
        count--;
    }
}

void
NvOdmDispReleaseDevices( NvU32 nDevices, NvOdmDispDeviceHandle *hDevices )
{
    while( nDevices-- )
    {
        (*hDevices)->Release( *hDevices );
        hDevices++;
    }

    s_init = NV_FALSE;
}

NvBool NvOdmGetPmuBoardId(NvOdmBoardInfo *PmuBoard)
{
    NvBool status;

   status = NvOdmPeripheralGetBoardInfo(PMU_E1733, PmuBoard);
   if (!status)
       status = NvOdmPeripheralGetBoardInfo(PMU_E1734, PmuBoard);
   if (!status)
       status = NvOdmPeripheralGetBoardInfo(PMU_E1735, PmuBoard);
   if (!status)
       status = NvOdmPeripheralGetBoardInfo(PMU_E1736, PmuBoard);
   if (!status)
       status = NvOdmPeripheralGetBoardInfo(PMU_E1769, PmuBoard);
   if (!status)
       status = NvOdmPeripheralGetBoardInfo(PMU_E1936, PmuBoard);
   if (!status)
   {
       return NV_FALSE;
   }
   return NV_TRUE;
}

void
NvOdmDispGetDefaultGuid( NvU64 *guid )
{
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;
    NvOdmBoardInfo PmuBoard;

    if (!NvOdmGetPmuBoardId(&PmuBoard))
    {
        NvOdmOsPrintf("This PmuBoardId is not supported.\n");
    }

    if (NvOdmQueryIsHdmiPrimaryDisplay())
        *guid = HDMI_GUID;
    else if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
        &DisplayBoardInfo, sizeof(DisplayBoardInfo)))
    {
        switch(DisplayBoardInfo.BoardInfo.BoardID) {
        case BOARD_E1639:
        case BOARD_E1813:
            if (PmuBoard.BoardID == PMU_E1735)
                *guid = SHARP_TPS65914_2516_PANEL_GUID;
            else
                *guid = SHARP_2516_PANEL_GUID;
            break;

        case BOARD_E1797:
        case BOARD_E1627:
            if (PmuBoard.BoardID == PMU_E1735)
                *guid = PANASONIC_TPS65914_PANEL_GUID;
            else if (PmuBoard.BoardID == PMU_E1736)
                *guid = PANASONIC_E1736TPS65914_PANEL_GUID;
            else if (PmuBoard.BoardID == PMU_E1936)
                *guid = PANASONIC_E1736TPS65914_PANEL_GUID;
            else
                *guid = PANASONIC_PANEL_GUID;
            break;

        case BOARD_E1549:
            if (PmuBoard.BoardID == PMU_E1735)
                *guid = LGD_TPS65914_PANEL_GUID;
            else if (PmuBoard.BoardID == PMU_E1736)
                *guid = LGD_E1736TPS65914_PANEL_GUID;
            else if (PmuBoard.BoardID == PMU_E1936)
                *guid = LGD_E1736TPS65914_PANEL_GUID;
            else if (PmuBoard.BoardID == PMU_E1769)
                *guid = LGD_E1769TPS65914_PANEL_GUID;
            else
                *guid = LGD_PANEL_GUID;
            break;

        case BOARD_P1761:
            if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
                &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
            {
                if (DisplayPanelInfo.DisplayPanelId)
                    *guid = AUO_E1937TPS65914_PANEL_GUID;
                else
                    *guid = AUO_E1807TPS65914_PANEL_GUID;
            }
            break;

        case BOARD_E1807:
            *guid = AUO_E1807TPS65914_PANEL_GUID;
            break;

        case BOARD_E1937:
            *guid = AUO_E1937TPS65914_PANEL_GUID;
            break;

       default:
            if (PmuBoard.BoardID == PMU_E1735)
                *guid = SHARP_TPS65914_2516_PANEL_GUID;
            else
                *guid = SHARP_2516_PANEL_GUID;
            break;
        }
    }

    return;
}

NvBool
NvOdmDispGetDeviceByGuid( NvU64 guid, NvOdmDispDeviceHandle *phDevice )
{
    NvU64 pDispGuid;

    NvOdmDispGetDefaultGuid(&pDispGuid);

    if (guid == pDispGuid)
    {
        *phDevice = &g_PrimaryDisp;
        return NV_TRUE;
    }
    else
    {
        *phDevice = 0;
        return NV_FALSE;
    }
}

void
NvOdmDispListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes )
{
    if (!hDevice)
        return ;

    hDevice->ListModes( hDevice, nModes, modes );
}

NvBool
NvOdmDispSetMode( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode,
    NvU32 flags )
{
    if (!hDevice)
        return NV_FALSE;

    return hDevice->SetMode( hDevice, mode, flags );
}

void
NvOdmDispSetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if (!hDevice)
        return ;

    hDevice->SetBacklight( hDevice, intensity );
}

void
NvOdmDispSetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    if (!hDevice)
        return ;

    hDevice->SetPowerLevel( hDevice, level );
}

NvBool
NvOdmDispGetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val )
{
    if (!hDevice)
        return NV_FALSE;

    return hDevice->GetParameter( hDevice, param, val );
}

void *
NvOdmDispGetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return NULL;

    return hDevice->GetConfiguration( hDevice );
}

void
NvOdmDispGetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    if (!hDevice)
        return ;

    hDevice->GetPinPolarities( hDevice, nPins, Pins, Values );
}

NvBool
NvOdmDispSetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    if (!hDevice)
        return NV_FALSE;

    return hDevice->SetSpecialFunction( hDevice, function, config );
}
NvBool
NvOdmBacklightInit(NvOdmDispDeviceHandle hDevice, NvBool bReopen)
{
    if (!hDevice)
        return NV_FALSE;

    if (hDevice->BacklightSetup != NULL)
        return hDevice->BacklightSetup(hDevice, bReopen);
    else
        return NV_FALSE;
}

void
NvOdmBacklightDeinit(NvOdmDispDeviceHandle hDevice)
{

    if (hDevice->BacklightRelease != NULL)
        hDevice->BacklightRelease(hDevice);
}

void
NvOdmBacklightIntensity(NvOdmDispDeviceHandle hDevice,
                      NvU8 intensity)
{
    if (hDevice->BacklightIntensity != NULL)
        hDevice->BacklightIntensity(hDevice, intensity);
}
