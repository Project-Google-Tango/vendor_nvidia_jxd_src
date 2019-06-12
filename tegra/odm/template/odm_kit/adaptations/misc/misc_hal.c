/*
 * Copyright (c) 2008-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_sdio.h"
#include "nvodm_uart.h"
#include "nvodm_usbulpi.h"
#include "misc_hal.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"

#define NUM_SDIO_INSTANCES 4
#define NUM_UART_INSTANCES 2

#define SDIO_HANDLE(x) ((NvOdmSdioHandle)(((x)&0x3fffffffUL) | 0xc0000000UL))
#define UART_HANDLE(x) ((NvOdmUartHandle)(((x)&0x3fffffffUL) | 0x80000000UL))

#define IS_SDIO_HANDLE(x) (((NvU32)(x)&0xc0000000UL)==0xc0000000UL)
#define IS_UART_HANDLE(x) (((NvU32)(x)&0xc0000000UL)==0x80000000UL)
#define INVALID_HANDLE ((NvOdmMiscHandle)0)

#define HANDLE_INDEX(x) ((NvU32)(x)&0x3fffffffUL)
typedef struct NvOdmUsbUlpiRec
{
    NvU64    CurrentGUID;
} NvOdmUsbUlpi;

static NvOdmMiscHandle
GetMiscInstance(NvOdmMiscHandle PseudoHandle)
{
    static NvOdmMisc SdioInstances[NUM_SDIO_INSTANCES];
    static NvOdmUart UartInstances[NUM_UART_INSTANCES];
    static NvBool first = NV_TRUE;

    if (first)
    {
        NvOdmOsMemset(SdioInstances, 0, sizeof(SdioInstances));
        NvOdmOsMemset(UartInstances, 0, sizeof(UartInstances));
        first = NV_FALSE;
    }

    if (IS_SDIO_HANDLE(PseudoHandle) && (HANDLE_INDEX(PseudoHandle)<NUM_SDIO_INSTANCES))
        return (NvOdmMiscHandle)&SdioInstances[HANDLE_INDEX(PseudoHandle)];

    else if (IS_UART_HANDLE(PseudoHandle) && (HANDLE_INDEX(PseudoHandle)<NUM_UART_INSTANCES))
        return (NvOdmMiscHandle)&UartInstances[HANDLE_INDEX(PseudoHandle)];

    return NULL;
}

static NvOdmMiscHandle
OdmMiscOpen(NvOdmIoModule Module,
            NvU32 Instance)
{
    NvU64 Guid;
    NvOdmMiscHandle misc;
    NvOdmPeripheralSearch SearchAttrs[] =
    {
        NvOdmPeripheralSearch_IoModule,
        NvOdmPeripheralSearch_Instance
    };
    NvU32 SearchVals[2];

    SearchVals[0] = Module;
    SearchVals[1] = Instance;

    if (!NvOdmPeripheralEnumerate(SearchAttrs,
        SearchVals, NV_ARRAY_SIZE(SearchAttrs), &Guid, 1))
        return INVALID_HANDLE;

    if (Module==NvOdmIoModule_Uart)
        misc = GetMiscInstance((NvOdmMiscHandle)UART_HANDLE(Instance));
    else
        misc = GetMiscInstance((NvOdmMiscHandle)SDIO_HANDLE(Instance));

    if (misc && !misc->pConn)
    {
        misc->pConn = NvOdmPeripheralGetGuid(Guid);
        //  fill in HAL based on GUID here
    }

    if (!misc || !misc->pConn)
        return INVALID_HANDLE;

    if (misc->pfnOpen && !misc->pfnOpen(misc))
        return INVALID_HANDLE;

    if (misc->pfnSetPowerState && !misc->PowerOnRefCnt)
    {
        if (!misc->pfnSetPowerState(misc, NV_TRUE))
            return INVALID_HANDLE;
        else
            misc->PowerOnRefCnt = 1;
    }

    if (Module==NvOdmIoModule_Uart)
        return (NvOdmMiscHandle)UART_HANDLE(Instance);
    else
        return (NvOdmMiscHandle)SDIO_HANDLE(Instance);
}

NvOdmSdioHandle
NvOdmSdioOpen(NvU32 Instance)
{
    return (NvOdmSdioHandle) OdmMiscOpen(NvOdmIoModule_Sdio, Instance);
}

NvOdmUartHandle
NvOdmUartOpen(NvU32 Instance)
{
    return (NvOdmUartHandle) OdmMiscOpen(NvOdmIoModule_Uart, Instance);
}

static void
OdmMiscClose(NvOdmMiscHandle hOdmMisc)
{
    NvOdmMisc *misc = GetMiscInstance(hOdmMisc);

    if (misc)
    {
        if (misc->pfnSetPowerState && misc->PowerOnRefCnt)
        {
            if (misc->pfnSetPowerState(misc, NV_FALSE))
                misc->PowerOnRefCnt = 0;
        }

        if (misc->pfnClose)
            misc->pfnClose(misc);

        misc->pfnOpen = NULL;
        misc->pfnSetPowerState = NULL;
        misc->pfnClose = NULL;
    }
}

void
NvOdmSdioClose(NvOdmSdioHandle hOdmSdio)
{
    OdmMiscClose((NvOdmMiscHandle)hOdmSdio);
}

void
NvOdmUartClose(NvOdmUartHandle hOdmUart)
{
    OdmMiscClose((NvOdmMiscHandle)hOdmUart);
}

static NvError
OdmMiscSuspend(NvOdmMiscHandle hOdmMisc)
{
    NvOdmMisc *misc = GetMiscInstance(hOdmMisc);

    if (misc && misc->PowerOnRefCnt)
    {
        if (misc->pfnSetPowerState && (misc->PowerOnRefCnt==1))
        {
            if (!misc->pfnSetPowerState(misc, NV_FALSE))
                return NvError_NotSupported;
        }
        misc->PowerOnRefCnt--;
    }

    return NvSuccess;
}

NvBool NvOdmSdioSuspend(NvOdmSdioHandle hOdmSdio)
{
	NvError e = OdmMiscSuspend((NvOdmMiscHandle)hOdmSdio);
    if (e==NvSuccess)
	{
	    return NV_TRUE;
	}
	else
	{
	    return NV_FALSE;
	}
}

NvBool
NvOdmUartSuspend(NvOdmUartHandle hOdmUart)
{
    unsigned int i;
    NvError e = NvSuccess;
    for (i=0; i<NUM_UART_INSTANCES; i++)
    {
        NvOdmMiscHandle misc = GetMiscInstance((NvOdmMiscHandle)UART_HANDLE(i));
        NvError Err = OdmMiscSuspend(misc);
        if (e==NvSuccess && (e!=Err))
            e = Err;
    }
    if (e==NvSuccess)
	{
	    return NV_TRUE;
	}
	else
	{
	    return NV_FALSE;
	}
}

static NvError
OdmMiscResume(NvOdmMiscHandle hOdmMisc)
{
    NvOdmMisc *misc = GetMiscInstance(hOdmMisc);

    if (misc && misc->PowerOnRefCnt)
    {
        if (misc->pfnSetPowerState && !misc->PowerOnRefCnt)
        {
            if (!misc->pfnSetPowerState(misc, NV_TRUE))
                return NvError_NotSupported;
        }
        misc->PowerOnRefCnt++;
    }

    return NvSuccess;
}



NvBool
NvOdmSdioResume(NvOdmSdioHandle hOdmSdio)
{
	NvError e = OdmMiscSuspend((NvOdmMiscHandle)hOdmSdio);
    if (e==NvSuccess)
	{
	    return NV_TRUE;
	}
	else
	{
	    return NV_FALSE;
	}
}

NvBool
NvOdmUartResume(NvOdmUartHandle hOdmUart)
{
    unsigned int i;
    NvError e = NvSuccess;
    for (i=0; i<NUM_UART_INSTANCES; i++)
    {
        NvOdmMiscHandle misc = GetMiscInstance((NvOdmMiscHandle)UART_HANDLE(i));
        NvError Err = OdmMiscResume(misc);
        if (e==NvSuccess && (e!=Err))
            e = Err;
    }
    if (e==NvSuccess)
	{
	    return NV_TRUE;
	}
	else
	{
	    return NV_FALSE;
	}
}

NvOdmUsbUlpiHandle NvOdmUsbUlpiOpen(NvU32 Instance)
{
    // dummy implementation
    return NULL;
}

void NvOdmUsbUlpiClose(NvOdmUsbUlpiHandle hOdmUlpi)
{
    return;
}
