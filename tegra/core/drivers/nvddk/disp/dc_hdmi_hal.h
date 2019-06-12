/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_DC_HDMI_HAL_H
#define INCLUDED_DC_HDMI_HAL_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_i2c.h"
#include "nvrm_hardware_access.h"
#include "nvddk_disp.h"
#include "nvddk_disp_edid.h"
#include "nvodm_query_discovery.h"
#include "dc_hal.h"
#include "arhdmi.h"
#include "arkfuse.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* adaptation */

// See Bug 453996 and 451332 details (NvOdmDispHdmiI2cTransaction,
// and NvOdmDispHdmiHdcpIsRevokedKsv)

// See Bug 453996 for this NvOdmDispHdmiI2cTransaction.  This function
// signature has to match NvOdmI2cTransaction
typedef NvOdmI2cStatus (*NvOdmDispHdmiI2cTransactionType)(
    NvOdmServicesI2cHandle hOdmI2c,             // ingnored, will pass NULL
    NvOdmI2cTransactionInfo *TransactionInfo,
    NvU32 NumberOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMilliSeconds);
typedef NvBool (*NvOdmDispHdmiI2cOpenType)(void);
typedef void (*NvOdmDispHdmiI2cCloseType)(void);

/**
 * Checks the given KSV list with a list of revoked KSVs. If any of the given
 * KSVs are revoked, this returns NV_TRUE, NV_FALSE otherwise.
 */
typedef NvBool (*NvOdmDispHdcpIsRevokedKsvType)( NvU64 *Ksv, NvU32 nKsvs );

typedef struct DcHdmiOdmRec
{
    NvOsLibraryHandle hLib;
    NvU32 refcount;
    NvOdmDispHdmiI2cTransactionType i2cTransaction;
    NvOdmDispHdmiI2cOpenType i2cOpen;
    NvOdmDispHdmiI2cCloseType i2cClose;
    NvOdmDispHdcpIsRevokedKsvType hdcpIsRevokedKsv;
    NvOdmDispHdmiDriveTermTbl* tmdsDriveTerm;
} DcHdmiOdm;

DcHdmiOdm *
DcHdmiOdmOpen( void );

void
DcHdmiOdmClose( DcHdmiOdm *adpt );

/* hdmi registers are word-adddressed in the spec */
#define HDMI_WRITE( h, reg, val ) \
    if( DC_DUMP_REGS ) \
        NV_DEBUG_PRINTF(( "hdmi: %.8x %.8x\n", (reg), (val) )); \
    NV_WRITE32( (NvU32 *)((h)->hdmi_regs) + (reg), (val) )

#define HDMI_READ( h, reg ) \
    NV_READ32( (NvU32 *)((h)->hdmi_regs) + (reg) )

/* for hdmi simulation, should run chiplib with:
 *
 * NV_CFG_CHIPLIB_ARGS="-c sockfsim.cfg -- +v hdmi +v display"
 */
#define HD_HDMI_REG_ADDR 0x2000
#define HD_WRITE_BIT 0x80000000
#define HD_SAFE_ADDR 0xf000

typedef struct DcHdcpRec
{
    NvU64 Aksv;
    NvU64 Bksv;
    NvU64 An;
    NvU64 M0;
    NvDdkDispHdcpContext *context;
    NvU8 Bcaps;
    NvBool bRepeater;
    NvBool bOneOne;
    NvBool bUseLongRi;
} DcHdcp;

typedef struct DcHdmiRec
{
    /* register aperture */
    void *hdmi_regs;
    NvU32 hdmi_regs_size;
    void *kfuse_regs;
    NvU32 kfuse_regs_size;

    /* power client stuff */
    NvU32 PowerClientId;
    NvRmFreqKHz freq;

    NvDdkDispAudioFrequency audio_freq;

    /* i2c handle for hdcp and edids */
    NvOdmPeripheralConnectivity const *conn;
    NvU64 guid;
    NvRmI2cHandle i2c;
    NvU32 hdcp_addr;

    /* the device EDID */
    NvDdkDispEdid *edid;

    /* adaptation to allow hijacking of i2c pins, etc. */
    DcHdmiOdm *adaptation;

    /* hdcp */
    NvBool bHdcpEnabled;
    DcHdcp hdcp;

    /* dvi device */
    NvBool bDvi;
} DcHdmi;

/* hdmi stuff */
void DcHdmiEnable( NvU32 controller, NvBool enable );
void DcHdmiListModes( NvU32 *nModes, NvOdmDispDeviceMode *modes );
void DcHdmiSetPowerLevel( NvOdmDispDevicePowerLevel level );
void DcHdmiGetParameter( NvOdmDispParameter param, NvU32 *val );
NvU64 DcHdmiGetGuid( void );
void DcHdmiSetEdid( NvDdkDispEdid *edid, NvU32 flags );
void DcHdmiAudioFrequency( NvU32 controller, NvDdkDispAudioFrequency freq );

/* hdcp stuff - public */
NvBool DcHdcpOp( NvU32 controller, NvDdkDispHdcpContext *context,
    NvBool enable );

/* hdcp stuff - private */
NvBool DcHdcpDownstream( DcController *cont, DcHdmi *hdmi );
NvBool DcHdcpRepeaters( DcHdmi *hdmi );
NvBool DcHdcpGetSprime( DcHdmi *hdmi );
NvBool DcHdcpGetMprime( DcHdmi *hdmi );
NvBool DcHdcpGetRepeaterInfo( DcHdmi *hdmi );
void DcHdcpEncryptEnable( DcController *cont, DcHdmi *hdmi );

#if defined(__cplusplus)
}
#endif

#endif
