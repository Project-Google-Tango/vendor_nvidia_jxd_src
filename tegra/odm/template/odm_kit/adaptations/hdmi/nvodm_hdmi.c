/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"

#if NVOS_IS_WINDOWS
#include <windows.h>
#endif

/* FIXME: Move these prototypes to a TBD common header */
NvOdmI2cStatus
NvOdmDispHdmiI2cTransaction(
    NvOdmServicesI2cHandle hOdmI2c,
    NvOdmI2cTransactionInfo *pTransaction,
    NvU32 NumOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMs);
NvBool NvOdmDispHdmiI2cOpen(void);
void NvOdmDispHdmiI2cClose(void);

/*  All of the entry points in the HDMI adaptation are loaded dynamically 
 *  by the display DDK, and if not present a default implementation will be
 *  used.  The function declarations are defined here for documentation
 *  purposes, but commented out.  Implementers can uncomment and fill in
 *  as needed.  Stub implementations of main entry points are provided,
 *  so that the DLL can be compiled and included in OS images. */

/*  NvOdmDispHdmiI2cTransaction -- allows implementers to change the interface
 *  that the HDMI I2C (HDCP and DDC reads) calls use.  All parameters match
 *  the definition of the NvOdmI2cTransaction service except hOdmI2c and
 *  ClockSpeekKHz, which can be ignored */
NvOdmI2cStatus NvOdmDispHdmiI2cTransaction(
    NvOdmServicesI2cHandle hOdmI2c,
    NvOdmI2cTransactionInfo *pTransaction,
    NvU32 NumOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMs)
{
    return NvError_NotSupported;
}

/* NvOdmDispHdmiI2cOpen -- allows implementers to init i2c if need */
NvBool NvOdmDispHdmiI2cOpen( void )
{
    return NV_TRUE;
}

/* NvOdmDispHdmiI2cClose -- allows implementers to deinit i2c
 * implemented in NvOdmDispHdmiI2cOpen */
void NvOdmDispHdmiI2cClose( void )
{
}

#if 0
/*  NvOdmDispHdmiHdcpIsRevokedKsv -- called by the display DDK during HDCP
 *  downstream authentication.  ODMs should compare the provided Ksv array
 *  against the stored SRM data and return NV_TRUE if any Ksv has been revoked,
 *  NV_FALSE if not */
NvBool NvOdmDispHdcpIsRevokedKsv( const NvU64* Ksv, NvU32 Num )
{
    return NV_TRUE;
}
#endif

#if 0
/**
 * Returns a pointer to a decrypted glob for HDCP Upstream authentication. The
 * pointer must be de-allocated with NvOdmDispHdmiReleaseGlob.
 *
 * If this is called multiple times without first releasing the glob, then
 * this will fail (return NULL).
 *
 * @param size Pointer to the size of the glob
 */
typedef void * (*NvOdmDispHdcpGetGlobType)( NvU32 *size );

/**
 * De-allocates/clears glob memory.
 */
typedef void (*NvOdmDispHdcpReleaseGlobType)( void *glob );
#endif

#if NVOS_IS_WINDOWS
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD Reason, LPVOID pReserved)
{
    if (Reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls((HMODULE)hInstance);

    return NV_TRUE;
}

#elif NVOS_IS_LINUX || NVOS_IS_UNIX
void _init(void);
void _init(void)
{
}

void _fini(void);
void _fini(void)
{
}
#else
static void NoOp(void)
{
}
#endif
