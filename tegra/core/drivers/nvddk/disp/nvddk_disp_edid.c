/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvrm_i2c.h"
#include "nvodm_query_discovery.h"
#include "nvddk_disp_structure.h"
#include "nvddk_disp_edid.h"
#include "dc_hdmi_hal.h"
#include "nvddk_disp_test.h"

/**
 * See \\netapp-hq\Archive\specs\vesa\edid.pdf for details on the EDID
 * structure, etc.
 */

#define COMMON_I2C_TRANS_SETUP( t, flags, numbytes, address ) \
    (t)->Flags = (flags); \
    (t)->NumBytes = (numbytes); \
    (t)->Address = (address);

#define ODM_I2C_TRANS_SETUP( t, flags, numbytes, address, buf ) \
    do { \
        COMMON_I2C_TRANS_SETUP( t, flags, numbytes, address ) \
        (t)->Buf = (buf); \
        (t)++; \
    } while (0 )

#define RM_I2C_TRANS_SETUP( t, flags, numbytes, address, is10bit ) \
    do { \
        COMMON_I2C_TRANS_SETUP( t, flags, numbytes, address ) \
        (t)->Is10BitAddress = is10bit; \
        (t)++; \
    } while (0 )

#define NVDDK_DISP_EDID_PINMUX  (0)

static NvBool
NvDdkDispPrivEdidExportSVD( NvOdmDispDeviceMode *mode,
    NvEdidShortVideoDescriptor *desc, NvBool check3d );

static void
NvDdkDispEdidCrtDdcMuxerConfig(NvRmI2cHandle i2c)
{
    const NvOdmPeripheralConnectivity *pConn= NULL;
    const NvOdmIoAddress *addrList = NULL;
    NvU32 Index;
    NvRmI2cTransactionInfo I2cTransactionInfo;
    NvU8 WriteBuffer[1];

    pConn = NvOdmPeripheralGetGuid(NV_ODM_GUID('c','r','t','d','d','c','m','u'));
    if( pConn == NULL )
        return;

    for( Index = 0; Index < pConn->NumAddress; Index++ )
    {
        if( pConn->AddressList[Index].Interface == NvOdmIoModule_I2c )
        {
            addrList = &pConn->AddressList[Index];
            break;
        }
    }

    if( !addrList )
        return;

    I2cTransactionInfo.Flags = NVRM_I2C_WRITE;
    I2cTransactionInfo.NumBytes = 1;
    I2cTransactionInfo.Address = addrList->Address;
    I2cTransactionInfo.Is10BitAddress = NV_FALSE;
    WriteBuffer[0] = (addrList->Purpose) & 0xFF;
    NvRmI2cTransaction(i2c, 0, NV_WAIT_INFINITE, 100, WriteBuffer, 1,
        &I2cTransactionInfo, 1);
}

static NvBool
NvEdidI2cRead( NvRmI2cHandle i2c, DcHdmiOdm *adpt,
    NvU32 slave, NvU8 segment, NvU32 addr, NvU8 *out )
{
    NvU8 buffer[130]; // 2 extra bytes for segment, etc.
    NvU8 *b;
    NvU32 idx;

    /* There are two major bugs in the i2c controller:
     * 1) it only allows 8 bytes at a time
     * 2) repeat start has seriously stupid limitations
     *
     * This will use the sw i2c implementation to get around these issues.
     */

    /* for extension blocks, vesa added a "segment" pointer that will point
     * to up to 256 blocks, each segment points to two 128 byte blocks.
     * the segment pointer is a single register at slave address 0x60 and
     * is set back to zero automatically after each i2c stop bit (have to
     * use sw i2c to get around repeat start issues).
     */

    if( adpt->i2cTransaction )
    {
        NvOdmI2cStatus err = NvOdmI2cStatus_Success;
        NvOdmI2cTransactionInfo odmTrans[3];
        NvOdmI2cTransactionInfo *t;

        t = &odmTrans[0];
        b = &buffer[0];

        if( segment )
        {
            /* don't generate a write for segment pointer for segment 0 */
            ODM_I2C_TRANS_SETUP( t,
                NVODM_I2C_IS_WRITE|NVODM_I2C_USE_REPEATED_START,
                1, 0x60, b );
            *b = segment;
            b++;
        }

        ODM_I2C_TRANS_SETUP( t,
            NVODM_I2C_IS_WRITE|NVODM_I2C_USE_REPEATED_START,
                1, slave, b );
        *b = addr;
        b++;

        ODM_I2C_TRANS_SETUP( t, 0, 128, slave, b );

        idx = b - buffer;

        err = (*(adpt->i2cTransaction))( NULL, odmTrans,
            (t - odmTrans), 40, NV_WAIT_INFINITE);
        if( err != NvOdmI2cStatus_Success )
        {
            goto fail;
        }
    }
    else
    {
        NvError err = NvSuccess;
        NvRmI2cTransactionInfo trans[3];
        NvRmI2cTransactionInfo *t;

        t = &trans[0];
        b = &buffer[0];

        if( segment )
        {
            /* don't generate a write for segment pointer for segment 0 */
            RM_I2C_TRANS_SETUP( t,
                NVRM_I2C_WRITE | NVRM_I2C_NOSTOP,
                1, 0x60, NV_FALSE );
            *b = segment;
            b++;
        }

        RM_I2C_TRANS_SETUP( t,
            NVRM_I2C_WRITE | NVRM_I2C_NOSTOP,
            1, slave, NV_FALSE );
        *b = addr;
        b++;

        RM_I2C_TRANS_SETUP( t, NVRM_I2C_READ,
                128, slave, NV_FALSE );
        idx = b - buffer;

        err = NvRmI2cTransaction( i2c, NVDDK_DISP_EDID_PINMUX,
            NV_WAIT_INFINITE, 40, buffer, 128 + idx, trans, t - trans );
        if( err != NvSuccess )
        {
            goto fail;
        }
    }

    goto clean;

fail:
    return NV_FALSE;

clean:
    NvOsMemcpy( out, &buffer[idx], 128 );
    return NV_TRUE;
}

static void
NvEdidParseSTI( NvU8 *ptr, NvEdidStandardTimingIdentification *sti,
    NvBool IsPre14 )
{
    NvU32 tmp;

    tmp = *ptr;

    sti->HorizActive = ((NvU32)tmp * 8) + 248;

    ptr++;
    tmp = *ptr;

    sti->RefreshRate = (tmp & 0x3F) + 60;

    /* aspect ratio */
    switch( tmp >> 6 ) {
    case 0: /* 1:1 or 16:10 depending on EDID revision */
        if (IsPre14)
            sti->VertActive = sti->HorizActive;
        else
            sti->VertActive = (sti->HorizActive / 16) * 10;
        break;
    case 1: /* 4:3 */
        sti->VertActive = (sti->HorizActive / 4) * 3;
        break;
    case 2: /* 5:4 */
        sti->VertActive = (sti->HorizActive / 5) * 4;
        break;
    case 3: /* 16:9 */
    default:
        sti->VertActive = (sti->HorizActive / 16) * 9;
        break;
    }
}

static void
NvEdidParseDTD( NvU8 *ptr, NvEdidDetailedTimingDescription *dtd )
{
    NvU8 tmp;

    dtd->PixelClock = (NvU32)( (ptr[0] & 0xff) |
        ((NvU16)(ptr[1] & 0xff) << 8));
    /* convert to KHz */
    dtd->PixelClock *= 10;
    ptr += 2;

    tmp = ptr[2];
    dtd->HorizActive = (NvU16)(*ptr) | (((NvU16)(tmp >> 4)) << 8);
    ptr++;

    dtd->HorizBlank = (NvU16)(*ptr) | ((NvU16)(tmp & 0xf) << 8);
    ptr += 2;

    tmp = ptr[2];
    dtd->VertActive = (NvU16)(*ptr) | (((NvU16)(tmp >> 4)) << 8);
    ptr++;

    dtd->VertBlank = (NvU16)(*ptr) | ((NvU16)(tmp & 0xf) << 8);
    ptr += 2;

    tmp = ptr[3];
    dtd->HorizOffset = (NvU16)(*ptr) | (NvU16)(((tmp & 0xC0) >> 6) << 8);
    ptr++;

    dtd->HorizPulse = (NvU16)(*ptr) | (NvU16)(((tmp & 0x30) >> 4) << 8);
    ptr++;

    dtd->VertOffset = (NvU16)( (*ptr & 0xf0) >> 4) | ((tmp & 0xC) >> 2) << 4;

    dtd->VertPulse = (NvU16)( (*ptr & 0xf)) | (tmp & 0x3) << 4;
    ptr += 2;

    dtd->HorizImageSize = ptr[0] | (((ptr[2] & 0xf0) >> 4) << 8);
    dtd->VertImageSize = ptr[1] | ((ptr[2] & 0xf) << 8);
    ptr += 3;

    dtd->HorizBoarder = *ptr;
    ptr++;

    dtd->VertBoarder = *ptr;
    ptr++;

    dtd->Flags = *ptr;
}

static void
NvEdidParseExtBlockCollection( NvU8 *raw, NvU32 idx, NvDdkDispEdid *edid )
{
    NvU8 *ptr;
    NvU8 tmp;
    NvU32 code;
    NvU32 len;

    ptr = &raw[4];

    while( ptr < &raw[idx] )
    {
        tmp = *ptr;
        len = tmp & 0x1f;

        /* data blocks have tags in top 3 bits:
         * tag code 2: video data block
         * tag code 3: vendor specific data block
         */

        code = (tmp >> 5) & 0x3;
        switch( code ) {
        case 2:
        {
            /* video data block */

            NvU32 i, j, n;
            NvU8 *vdb;
            NvEdidShortVideoDescriptor svd;

            n = len; /* length of video descriptors in bytes */

            if( n > NVDDK_DISP_EDID_MAX_SVD )
            {
                n = NVDDK_DISP_EDID_MAX_SVD;
            }
            
            /* short video descriptor:
             * bits 0-6: index of video format (same index for the avi info
             *           frame)
             * bit 7: native/not-native
             */
            ptr++;
            vdb = ptr;
            j = 0;
            for( i = 0; i < n; i++ )
            {
                NvU8 id;
                tmp = *vdb;

                id = tmp & 0x7f;

                switch( (NvEdidVideoId)id ) {
                case NvEdidVideoId_640_480_1:
                case NvEdidVideoId_720_480_2:
                case NvEdidVideoId_720_480_3:
                case NvEdidVideoId_1280_720_4:
                case NvEdidVideoId_1920_1080_16:
                case NvEdidVideoId_720_576_17:
                case NvEdidVideoId_720_576_18:
                case NvEdidVideoId_1280_720_19:
                case NvEdidVideoId_1920_1080_31:
                case NvEdidVideoId_1920_1080_32:
                case NvEdidVideoId_1920_1080_33:
                case NvEdidVideoId_1920_1080_34:
                    svd.Id = (NvEdidVideoId)id;
                    svd.Native = ( tmp >> 7 ) ? NV_TRUE : NV_FALSE;
                    edid->SVD[j] = svd;
                    j++;
                    break;
                default:
                    break;
                }

                vdb++;
            }

            edid->nSVDs += j;
            ptr += len;
            break;
        }
        case 3:
        {
            NvU32 j = 0;
            NvU32 n = len; // len does not include the header

            if ( (n >= 8 )&&
                 ( ptr[1] == 0x03)&&
                 ( ptr[2] == 0x0c)&&
                 ( ptr[3] == 0) )
            {
                // vsdb for HDMI devices
                // see table 8-16 for HDMI VSDB format
                j = 8;
                tmp = ptr[j++];
                if ( tmp & 0x20 ) // HDMI_Video_present
                {
                    if ( tmp & 0x80 ) // Latency_Fields_present
                        j += 2;
                    if ( tmp & 0x40 ) // I_Latency_Fields_present
                        j += 2;
                    if ( j <= n )
                    {
                        tmp = ptr[j];
                        if ( tmp  & 0x80 ) // 3D_present
                            edid->bHas3dSupport = NV_TRUE;
                    }
                }
            }

            // copy out for future use
            len++;
            len = NV_MIN( len, NVDDK_DISP_EDID_VSDB_MAX );
            NvOsMemcpy( edid->VendorSpecificDataBlock, ptr, len );

            ptr += n+1; // adding the header
            break;
        }
        default:
            len++; // len does not include header
            ptr += len;
            break;
        }
    }
}

static void
NvEdidParseExt( NvU8 *raw, NvDdkDispEdid *edid, NvU32 revision )
{
    NvU32 idx;
    NvU32 i;
    NvU8 *ptr;

    idx = raw[2];
    if( idx == 0 )
    {
        /* No data in Reserved Data Block and no DTDs */
        return;
    }

    /* parse the Data Block Collection */
    /* idx value 0x4 implies no data in reserved data block */
    if( revision == 3 && idx != 0x4)
    {
        NvEdidParseExtBlockCollection( raw, idx, edid );
    }

    /* parse the Detailed Timing Descriptors, if any */
    ptr = &raw[idx];
    for( i = 0; edid->nDTDs < NVDDK_DISP_EDID_MAX_DTD; i++ )
    {
        NvEdidDetailedTimingDescription dtd;

        NvOsMemset( &dtd, 0, sizeof(NvEdidDetailedTimingDescription));

        /* Check for alternate descriptors/end of DTDs */
        if ( ptr[0] == 0 && ptr[1] == 0 )
        {
            break; 
        }

        NvEdidParseDTD( ptr, &dtd );
        edid->DTD[ edid->nDTDs ] = dtd;
        edid->nDTDs++;
        
        // Last two bytes: end of padding and checksum 
        if ((idx + 18) >= (NVDDK_DISP_EDID_BLOCK_SIZE - 2)) 
        {
            break; 
        }
        idx += 18; 
        ptr += 18;
    }
}

static NvBool
NvEdidParse( NvRmI2cHandle i2c, DcHdmiOdm *adpt, NvU32 addr,
    NvU8 *raw, NvDdkDispEdid *edid )
{
    NvU8 tmp;
    NvU32 i, n;
    NvU8 *ptr;
    NvU8 segment;
    NvU8 features;
    NvBool isPre14;

    /* verify the header */
    if( !( raw[0] == 0 && raw[1] == 0xff && raw[2] == 0xff && raw[3] == 0xff &&
           raw[4] == 0xff && raw[5] == 0xff && raw[6] == 0xff && raw[7] == 0 ))
    {
        return NvError_BadParameter;
    }

    NvOsMemset( edid, 0, sizeof(NvDdkDispEdid) );

    ptr = &raw[18];
    edid->Version = *ptr;
    ptr++;
    edid->Revision = *ptr;
    if( edid->Version != 1 )
    {
        return NV_FALSE;
    }

    /* interpretation of some aspect ratio codes depend on EDID revision */
    isPre14 = ( edid->Version == 1 && edid->Revision < 4 );

    /* Video Input Definition -- analog/digial bit */
    if( edid->Revision >= 0x4 )
    {
        ptr = &raw[20];
        edid->VideoInputDefinition = *ptr;
    }

    /* grab the features byte */
    features = raw[24];

    /* established timings */
    ptr = &raw[35];
    tmp = *ptr;
    edid->EstTiming1 = (NvEdidEstablishedTiming1)tmp;

    ptr++;
    tmp = *ptr;
    edid->EstTiming2 = (NvEdidEstablishedTiming2)tmp;

    /* standard timings */
    n = 0;
    ptr = &raw[38];
    for( i = 0; i < 8; i++ )
    {
        NvEdidStandardTimingIdentification sti;

        tmp = *ptr;
        if( tmp == 1 )
        {
            /* unused field */
        }
        else
        {
            NvEdidParseSTI( ptr, &sti, isPre14 );
            edid->STI[n] = sti;
            n++;
        }

        ptr += 2;
    }

    edid->nSTIs = n;

    /* detailed timings */
    n = 0;
    ptr = &raw[54];
    for( i = 0; i < 4; i++ )
    {
        NvEdidDetailedTimingDescription dtd;

        NvOsMemset( &dtd, 0, sizeof(NvEdidDetailedTimingDescription));

        if( ptr[0] == 0 && ptr[1] == 0 )
        {
            /* check for Display Descriptors */
            if( edid->Revision >= 4 )
            {
                tmp = ptr[3];

                if( (tmp == NVDDK_DISP_EDID_STI_TAG ) &&
                    (ptr[2] == 0) && (ptr[4] == 0))
                {
                    /* more standard timing identifiers */
                    NvU8 *p; 

                    n = edid->nSTIs;
                    p = &ptr[5];
                    for ( i = 0; i < 6; i++ )
                    {
                        NvEdidStandardTimingIdentification sti;

                        if( (p[0] == 1 ) && (p[1] == 1))
                        {
                            /* unused field */
                        }
                        else
                        {
                            NvEdidParseSTI( p, &sti, isPre14 );
                            edid->STI[n] = sti;
                            n++;
                        }
                        p += 2;
                    }
                    edid->nSTIs = n;
                }
                else if( ( tmp == NVDDK_DISP_EDID_EST_TIMING_3_TAG ) &&
                    (ptr[2] == 0) && (ptr[4] == 0) )
                {
                    if (ptr[5] == 0xA)
                    {
                        /* Established Timings III */
                        edid->EstTiming3.Res0 = (NvEdidEst3Res0)ptr[6];
                        edid->EstTiming3.Res1 = (NvEdidEst3Res1)ptr[7];
                        edid->EstTiming3.Res2 = (NvEdidEst3Res2)ptr[8];
                        edid->EstTiming3.Res3 = (NvEdidEst3Res3)ptr[9];
                        edid->EstTiming3.Res4 = (NvEdidEst3Res4)ptr[10];
                        edid->EstTiming3.Res5 = (NvEdidEst3Res5)ptr[11];
                    }
                    else
                    {
                        return NV_FALSE;
                    }
                }
            }
            if ( (ptr[2] == 0) && (ptr[3] == 0xFC) && (ptr[4] == 0) )
            {
                /* Monitor descriptor */
                if (!NvOsStrncmp((const char*)&ptr[5], "NTSC/PAL", 8))
                {
                    /* CRT/HDMI can not be type TV NTSC/PAL */
                    return NV_FALSE;
                }
            }
        }
        else if ( ptr[0] == 1 && ptr[1] == 1)
        {
            /* unused field */
        }
        else
        {
            NvEdidParseDTD( ptr, &dtd );

            /* if this is >= 1.4 edid and the features bit 1 is set, then
             * the first DTD is the native mode.
             */
            if( i == 0 && !isPre14 && features & (1 << 1) )
            {
                dtd.bNative = NV_TRUE;
            }

            edid->DTD[n] = dtd;
            n++;
        }

        ptr += 18;
    }

    edid->nDTDs = n;

    /* extension flag */
    tmp = *ptr;
    n = 1;
    segment = 0;
    while( tmp ) /* more 128 byte edids follow this one */
    {
        /* read the raw edid bytes */
        if( !NvEdidI2cRead( i2c, adpt, addr, segment, (128 * n), &raw[0] ) )
        {
            return NV_FALSE;
        }

        /* segment pointer accesses 2 blocks at a time. */
        n ^= 1;

        if( !n )
        {
            segment++;
        }

        if( raw[0] == 2 )
        {
            /* CEA-861 extension */

            switch( raw[1] ) {
            case 1:
            case 2:
            case 3:
                NvEdidParseExt( raw, edid, raw[1] );
                edid->bHasCeaExtension = NV_TRUE;
                break;
            default:
                /* ingore formats that we don't understand (per spec) */
                break;
            }
        }
        else if( raw[0] == 0x10 )
        {
            /* video timing */
        }
        else if( raw[0] == 0x40 )
        {
            /* display info */
        }
        else if( raw[0] == 0x50 )
        {
            /* localized string */
        }
        else if( raw[0] == 0x60 )
        {
            /* digital packet video link */
        }
        else if( raw[0] == 0xf0 )
        {
            /* block map */
        }
        else if( raw[0] == 0xff )
        {
            /* manufacturer extension */
        }

        tmp--;
    }

    return NV_TRUE;
}

static const NvOdmPeripheralConnectivity *
NvEdidDiscover( NvDdkDispDisplayHandle hDisplay )
{
    NvU64 guid;
    const NvOdmPeripheralConnectivity *conn;
    NvU32 vals[] =
        {
            NvOdmPeripheralClass_Display,
            NvOdmIoModule_I2c,
            0,
        };
    const NvOdmPeripheralSearch search[]=
        {
            NvOdmPeripheralSearch_PeripheralClass,
            NvOdmPeripheralSearch_IoModule,
            NvOdmPeripheralSearch_IoModule,
        };

    if( hDisplay->attr.Type == NvDdkDispDisplayType_CRT )
    {
        vals[2] = NvOdmIoModule_Crt;
    }
    else if( hDisplay->attr.Type == NvDdkDispDisplayType_HDMI )
    {
        vals[2] = NvOdmIoModule_Hdmi;
    }

    if( !NvOdmPeripheralEnumerate( search, vals, 3, &guid, 1 ) )
    {
        return NULL;
    }

    conn = NvOdmPeripheralGetGuid( guid );
    return conn;
}

static void NvDdkDispEdidPowerConfig(
    const NvOdmPeripheralConnectivity *conn,
    NvOdmServicesPmuHandle hPmu,
    NvU32 *rail_mask,
    NvBool IsPowerEnable)
{
    NvU32 i;

    if (IsPowerEnable)
    {
        for( i = 0; i < conn->NumAddress; i++ )
        {
            if( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
            {
                NvOdmServicesPmuVddRailCapabilities cap;
                NvU32 settle_us;
                NvU32 mv;

                /* address is the vdd rail id */
                NvOdmServicesPmuGetCapabilities( hPmu,
                    conn->AddressList[i].Address, &cap );

                /* only enable a rail that's off -- save the rail state and
                * restore it at the end of this function. this will prevent
                * issues of accidentally disabling rails that were enabled
                * by the bootloader.
                */
                mv = 0;
                NvOdmServicesPmuGetVoltage( hPmu,
                conn->AddressList[i].Address, &mv );
                if( mv )
                {
                    continue;
                }

                *rail_mask |= (1UL << i);

                /* set the rail volatage to the recommended */
                NvOdmServicesPmuSetVoltage( hPmu,
                    conn->AddressList[i].Address, cap.requestMilliVolts,
                    &settle_us );

                /* wait for the rail to settle */
                NvOdmOsWaitUS( settle_us );
            }
        }
    }
    else
    {
        for( i = 0; i < conn->NumAddress; i++ )
        {
            if( (*rail_mask & (1UL << i)) &&
                (conn->AddressList[i].Interface == NvOdmIoModule_Vdd) )
            {
                /* set the rail volatage to the recommended */
                NvOdmServicesPmuSetVoltage( hPmu,
                    conn->AddressList[i].Address, NVODM_VOLTAGE_OFF, 0 );
            }
        }
    }
}

NvError
NvDdkDispEdidRead( NvRmDeviceHandle hRm, NvDdkDispDisplayHandle hDisplay,
    NvDdkDispEdid *edid )
{
    const NvOdmPeripheralConnectivity *conn = NULL;
    const NvOdmIoAddress *addrlist = NULL;
    NvRmI2cHandle i2c = NULL;
    NvOdmServicesPmuHandle hPmu = NULL;
    NvError e;
    NvU32 addr;
    NvU32 inst;
    NvU8 raw[NVDDK_DISP_EDID_BLOCK_SIZE];
    NvU32 i;
    DcHdmiOdm *adpt;
    NvU32 rail_mask = 0;

    conn = NvEdidDiscover( hDisplay );
    if( !conn )
    {
        return NvError_NotSupported;
    }

    /* the DDC i2c address is always first in the address list */
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_I2c )
        {
            addrlist = &conn->AddressList[i];
            break;
        }
    }

    if( !addrlist )
    {
        return NvError_NotSupported;
    }

    addr = addrlist->Address;
    inst = addrlist->Instance;

    /* enable the power rails (may already be enabled, which is fine) */
    hPmu = NvOdmServicesPmuOpen();
    NvDdkDispEdidPowerConfig(conn, hPmu, &rail_mask, NV_TRUE);

    adpt = DcHdmiOdmOpen();
    if( hDisplay->attr.Type != NvDdkDispDisplayType_HDMI ||
        !adpt->i2cTransaction )
    {
        // fall back to RmI2c (not HDMI or no NvOdmDispHdmiI2cTransaction)
        NV_CHECK_ERROR_CLEANUP(
            NvRmI2cOpen( hRm, NvOdmIoModule_I2c, inst, &i2c )
        );

        if( hDisplay->attr.Type == NvDdkDispDisplayType_CRT )
            NvDdkDispEdidCrtDdcMuxerConfig(i2c);
    }

    /* read the raw edid bytes */
    if( !NvEdidI2cRead( i2c, adpt, addr, 0, 0, &raw[0] ) )
    {
        e = NvError_BadValue;
        goto fail;
    }

    /* parse the edid structure */
    if( raw[0] == 0 )
    {
        if( !NvEdidParse( i2c, adpt, addr, raw, edid ) )
        {
            e = NvError_BadValue;
            goto fail;
        }
    }
    else
    {
        // FIXME: handle version 2 of the vesa edid
        e = NvError_NotSupported;
        goto fail;
    }

    e = NvSuccess;
    goto clean;

fail:
clean:
    /* disable rails that were enabled here */
    NvDdkDispEdidPowerConfig(conn, hPmu, &rail_mask, NV_FALSE);

    NvOdmServicesPmuClose( hPmu );
    NvRmI2cClose( i2c );
    DcHdmiOdmClose( adpt );

    return e;
}

// this could be faster, but it's not worth using magic bit-twiddling tables
// for 8 bit counts.
static NvU32 count_bits( NvU8 bits )
{
    NvU32 i;
    NvU32 count = 0;
    for( i = 0; i < 8; i++ )
    {
        if( bits & 1 ) count++;
        bits >>= 1;
    }

    return count;
}

static NvU32
NvDdkDispPrivEdidCountModes( NvDdkDispEdid *edid )
{
    NvU32 i;
    NvBool b;
    NvU32 modes = 0;

    modes += count_bits( edid->EstTiming1 );
    modes += count_bits( edid->EstTiming2 );
    modes += edid->nSTIs;
    modes += edid->nDTDs;
    modes += edid->nSVDs;

    if (edid->bHas3dSupport)
    {
        // need to check SVD to determin how may 3d modes are 
        // supported
        for( i = 0; i < edid->nSVDs; i++ )
        {
            b = NvDdkDispPrivEdidExportSVD( NULL, &edid->SVD[i], NV_TRUE );
            if( b )
            {
                modes++;
            }
        }
    }

    
    if (edid->Revision >=4)
    {
        if (edid->EstTiming3.Res0)
            modes += count_bits(edid->EstTiming3.Res0);
        if (edid->EstTiming3.Res1)
            modes += count_bits(edid->EstTiming3.Res1);
        if (edid->EstTiming3.Res2)
            modes += count_bits(edid->EstTiming3.Res2);
        if (edid->EstTiming3.Res3)
            modes += count_bits(edid->EstTiming3.Res3);
        if (edid->EstTiming3.Res4)
            modes += count_bits(edid->EstTiming3.Res4);
        if (edid->EstTiming3.Res5)
            modes += count_bits(edid->EstTiming3.Res5);
    }

    return modes;
}

/* all supported modes */

static NvOdmDispDeviceMode s_640_350_85 =
    { 640, // width
      350, // height
      24, // bpp
      (85 << 16), // refresh
      31500, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        64, // h_sync_width
        3, // v_sync_width
        96, // h_back_porch
        60, // v_back_porch
        640, // h_disp_active
        350, // v_disp_active
        32, // h_front_porch
        32, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_640_400_85 =
    { 640, // width
      400, // height
      24, // bpp
      (85 << 16), // refresh
      31500, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        64, // h_sync_width
        3, // v_sync_width
        96, // h_back_porch
        41, // v_back_porch
        640, // h_disp_active
        400, // v_disp_active
        32, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_640_480_60 =
    { 640, // width
      480, // height
      24, // bpp
      0, // refresh
      25175, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        96, // h_sync_width
        2, // v_sync_width
        40+8, // h_back_porch
        25+8, // v_back_porch
        640, // h_disp_active
        480, // v_disp_active
        8+8, // h_front_porch
        2+8, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_640_480_72 =
    { 640, // width
      480, // height
      24, // bpp
      (72 << 16), // refresh
      31500, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        40, // h_sync_width
        3, // v_sync_width
        120 + 8, // h_back_porch
        20 + 8, // v_back_porch
        640, // h_disp_active
        480, // v_disp_active
        8 + 16, // h_front_porch
        8 + 1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_640_480_75 =
    { 640, // width
      480, // height
      24, // bpp
      (75 << 16), // refresh
      31500, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        64, // h_sync_width
        3, // v_sync_width
        120, // h_back_porch
        16, // v_back_porch
        640, // h_disp_active
        480, // v_disp_active
        16, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_640_480_85 =
    { 640, // width
      480, // height
      24, // bpp
      (85 << 16), // refresh
      36000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        56, // h_sync_width
        3, // v_sync_width
        80, // h_back_porch
        25, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        56, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_720_400_70 =
    { 720, // width
      400, // height
      24, // bpp
      (70 << 16), // refresh
      28320, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        108, // h_sync_width
        2, // v_sync_width
        54, // h_back_porch
        35, // v_back_porch
        720, // h_disp_active
        400, // v_disp_active
        18, // h_front_porch
        12, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_720_400_85 =
    { 720, // width
      400, // height
      24, // bpp
      (85 << 16), // refresh
      35500, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
        NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        72, // h_sync_width
        3, // v_sync_width
        108, // h_back_porch
        42, // v_back_porch
        720, // h_disp_active
        400, // v_disp_active
        36, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_800_600_56 =
    { 800, // width
      600, // height
      24, // bpp
      (56 << 16), // refresh
      36000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        72, // h_sync_width
        2, // v_sync_width
        128, // h_back_porch
        22, // v_back_porch
        800, // h_disp_active
        600, // v_disp_active
        24, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_800_600_60 =
    { 800, // width
      600, // height
      24, // bpp
      0, // refresh
      40000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
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
    };

static NvOdmDispDeviceMode s_800_600_72 =
    { 800, // width
      600, // height
      24, // bpp
      (72 << 16), // refresh
      50000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        120, // h_sync_width
        6, // v_sync_width
        64, // h_back_porch
        23, // v_back_porch
        800, // h_disp_active
        600, // v_disp_active
        56, // h_front_porch
        37, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_800_600_75 =
    { 800, // width
      600, // height
      24, // bpp
      (75 << 16), // refresh
      49500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        80, // h_sync_width
        3, // v_sync_width
        160, // h_back_porch
        21, // v_back_porch
        800, // h_disp_active
        600, // v_disp_active
        16, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_800_600_85 =
    { 800, // width
      600, // height
      24, // bpp
      (85 << 16), // refresh
      56250, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        64, // h_sync_width
        3, // v_sync_width
        152, // h_back_porch
        27, // v_back_porch
        800, // h_disp_active
        600, // v_disp_active
        32, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_848_480_60 =
    { 848, // width
      480, // height
      24, // bpp
      0, // refresh
      31500, // frequency in khz
      // flags
      ( NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
      NVODM_DISP_MODE_FLAG_TYPE_VESA ), 
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        80, // h_sync_width
        5, // v_sync_width
        104, // h_back_porch
        12, // v_back_porch
        848, // h_disp_active
        480, // v_disp_active
        24, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1024_768_60 =
    { 1024, // width
      768, // height
      24, // bpp
      0, // refresh
      65000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        136, // h_sync_width
        6, // v_sync_width
        160, // h_back_porch
        29, // v_back_porch
        1024, // h_disp_active
        768, // v_disp_active
        24, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1024_768_70 =
    { 1024, // width
      768, // height
      24, // bpp
      (70 << 16), // refresh
      75000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        136, // h_sync_width
        6, // v_sync_width
        144, // h_back_porch
        29, // v_back_porch
        1024, // h_disp_active
        768, // v_disp_active
        24, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1024_768_75 =
    { 1024, // width
      768, // height
      24, // bpp
      (75 << 16), // refresh
      78750, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        96, // h_sync_width
        3, // v_sync_width
        176, // h_back_porch
        28, // v_back_porch
        1024, // h_disp_active
        768, // v_disp_active
        16, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1024_768_85 =
    { 1024, // width
      768, // height
      24, // bpp
      (85 << 16), // refresh
      94500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        96, // h_sync_width
        3, // v_sync_width
        208, // h_back_porch
        36, // v_back_porch
        1024, // h_disp_active
        768, // v_disp_active
        48, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1152_864_75 =
    { 1152, // width
      864, // height
      24, // bpp
      (75 << 16), // refresh
      108000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        128, // h_sync_width
        3, // v_sync_width
        256, // h_back_porch
        32, // v_back_porch
        1152, // h_disp_active
        864, // v_disp_active
        64, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_768_60_RB =
    { 1280, // width
      768, // height
      24, // bpp
      0, // refresh
      68250, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        32, // h_sync_width
        7, // v_sync_width
        80, // h_back_porch
        12, // v_back_porch
        1280, // h_disp_active
        768, // v_disp_active
        48, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_768_60 =
    { 1280, // width
      768, // height
      24, // bpp
      0, // refresh
      79500, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        128, // h_sync_width
        7, // v_sync_width
        192, // h_back_porch
        20, // v_back_porch
        1280, // h_disp_active
        768, // v_disp_active
        64, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_768_75 =
    { 1280, // width
      768, // height
      24, // bpp
      (75 << 16), // refresh
      102250, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        128, // h_sync_width
        7, // v_sync_width
        208, // h_back_porch
        27, // v_back_porch
        1280, // h_disp_active
        768, // v_disp_active
        80, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_768_85 =
    { 1280, // width
      768, // height
      24, // bpp
      (85 << 16), // refresh
      117500, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        136, // h_sync_width
        7, // v_sync_width
        216, // h_back_porch
        31, // v_back_porch
        1280, // h_disp_active
        768, // v_disp_active
        80, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };


static NvOdmDispDeviceMode s_1280_960_60 =
    { 1280, // width
      960, // height
      24, // bpp
      0, // refresh
      108000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        112, // h_sync_width
        3, // v_sync_width
        312, // h_back_porch
        36, // v_back_porch
        1280, // h_disp_active
        960, // v_disp_active
        96, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_960_85 =
    { 1280, // width
      960, // height
      24, // bpp
      (85 << 16), // refresh
      148500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        160, // h_sync_width
        3, // v_sync_width
        224, // h_back_porch
        47, // v_back_porch
        1280, // h_disp_active
        960, // v_disp_active
        64, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_1024_60 =
    { 1280, // width
      1024, // height
      24, // bpp
      0, // refresh
      108000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
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
    };

static NvOdmDispDeviceMode s_1280_1024_75 =
    { 1280, // width
      1024, // height
      24, // bpp
      (75 << 16), // refresh
      135000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        144, // h_sync_width
        3, // v_sync_width
        248, // h_back_porch
        38, // v_back_porch
        1280, // h_disp_active
        1024, // v_disp_active
        16, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_1024_85 =
    { 1280, // width
      1024, // height
      24, // bpp
      (85 << 16), // refresh
      157500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        160, // h_sync_width
        3, // v_sync_width
        224, // h_back_porch
        44, // v_back_porch
        1280, // h_disp_active
        1024, // v_disp_active
        64, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1360_768_60 =
    { 1360, // width
      768, // height
      24, // bpp
      0, // refresh
      85500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        112, // h_sync_width
        6, // v_sync_width
        256, // h_back_porch
        18, // v_back_porch
        1360, // h_disp_active
        768, // v_disp_active
        64, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1400_1050_60_RB =
    { 1400, // width
      1050, // height
      24, // bpp
      0, // refresh
      101000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        32, // h_sync_width
        4, // v_sync_width
        80, // h_back_porch
        23, // v_back_porch
        1400, // h_disp_active
        1050, // v_disp_active
        48, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1400_1050_60 =
    { 1400, // width
      1050, // height
      24, // bpp
      0, // refresh
      121750, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        144, // h_sync_width
        4, // v_sync_width
        232, // h_back_porch
        32, // v_back_porch
        1400, // h_disp_active
        1050, // v_disp_active
        88, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1400_1050_75 =
    { 1400, // width
      1050, // height
      24, // bpp
      (75 << 16), // refresh
      156000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        144, // h_sync_width
        4, // v_sync_width
        248, // h_back_porch
        42, // v_back_porch
        1400, // h_disp_active
        1050, // v_disp_active
        104, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1440_900_60_RB =
    { 1440, // width
      900, // height
      24, // bpp
      0, // refresh
      88750, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        32, // h_sync_width
        6, // v_sync_width
        80, // h_back_porch
        17, // v_back_porch
        1440, // h_disp_active
        900, // v_disp_active
        48, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1440_900_60 =
    { 1440, // width
      900, // height
      24, // bpp
      0, // refresh
      106500, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        152, // h_sync_width
        6, // v_sync_width
        232, // h_back_porch
        25, // v_back_porch
        1440, // h_disp_active
        900, // v_disp_active
        80, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1440_900_75 =
    { 1440, // width
      900, // height
      24, // bpp
      (75 << 16), // refresh
      136750, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        152, // h_sync_width
        6, // v_sync_width
        248, // h_back_porch
        33, // v_back_porch
        1440, // h_disp_active
        900, // v_disp_active
        96, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1440_900_85 =
    { 1440, // width
      900, // height
      24, // bpp
      (85 << 16), // refresh
      157000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        152, // h_sync_width
        6, // v_sync_width
        256, // h_back_porch
        39, // v_back_porch
        1440, // h_disp_active
        900, // v_disp_active
        104, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1680_1050_60_RB = 
    { 1680, // width
      1050, // height
      24, // bpp
      0, // refresh
      119000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        32, // h_sync_width
        6, // v_sync_width
        80, // h_back_porch
        21, // v_back_porch
        1680, // h_disp_active
        1050, // v_disp_active
        48, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1680_1050_60 = 
    { 1680, // width
      1050, // height
      24, // bpp
      0, // refresh
      146250, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_HSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        176, // h_sync_width
        6, // v_sync_width
        280, // h_back_porch
        30, // v_back_porch
        1680, // h_disp_active
        1050, // v_disp_active
        104, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1600_1200_60 =
    { 1600, // width
      1200, // height
      24, // bpp
      0, // refresh
      162000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_VESA, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        192, // h_sync_width
        3, // v_sync_width
        304, // h_back_porch
        46, // v_back_porch
        1600, // h_disp_active
        1200, // v_disp_active
        64, // h_front_porch
        1, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1920_1200_60_RB = 
    { 1920, // width
      1200, // height
      24, // bpp
      0, // refresh
      154000, // frequency in khz
      // flags
      (  NVODM_DISP_MODE_FLAG_VSYNC_NEGATIVE |
         NVODM_DISP_MODE_FLAG_TYPE_VESA ),
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        32, // h_sync_width
        6, // v_sync_width
        80, // h_back_porch
        25, // v_back_porch
        1920, // h_disp_active
        1200, // v_disp_active
        48, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_640_480_1 =
    { 640, // width
      480, // height
      24, // bpp
      0, // refresh
      25200, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
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
    };

static NvOdmDispDeviceMode s_720_480_2 =
    { 720, // width
      480, // height
      24, // bpp
      0, // refresh
      27000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
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
    };

static NvOdmDispDeviceMode s_1280_720_4 =
    { 1280, // width
      720, // height
      24, // bpp
      0, // refresh
      74250, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
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
    };

static NvOdmDispDeviceMode s_1280_720_4_3d =
    { 1280, // width
      720, // height
      24, // bpp
      0, // refresh
      74250*2, // frequency in khz
      (NVODM_DISP_MODE_FLAG_TYPE_HDMI|
      NVODM_DISP_MODE_FLAG_TYPE_3D), // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        40, // h_sync_width
        5, // v_sync_width
        220, // h_back_porch
        20, // v_back_porch
        1280, // h_disp_active
        720*2+30, // v_disp_active
        110, // h_front_porch
        5, // v_front_porch
      },
      NV_FALSE // partial
    };


static NvOdmDispDeviceMode s_1920_1080_16 =
    { 1920, // width
      1080, // height
      24, // bpp
      0, // refresh
      148500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
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
    };

static NvOdmDispDeviceMode s_720_576_17 =
    { 720, // width
      576, // height
      24, // bpp
      (50 << 16), // refresh
      27000, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
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
    };

static NvOdmDispDeviceMode s_1280_720_19 =
    { 1280, // width
      720, // height
      24, // bpp
      (50 << 16), // refresh
      74250, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        40, // h_sync_width
        5, // v_sync_width
        220, // h_back_porch
        20, // v_back_porch
        1280, // h_disp_active
        720, // v_disp_active
        440, // h_front_porch
        5, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1280_720_19_3d =
    { 1280, // width
      720, // height
      24, // bpp
      (50 << 16), // refresh
      74250*2, // frequency in khz
      (NVODM_DISP_MODE_FLAG_TYPE_HDMI|
      NVODM_DISP_MODE_FLAG_TYPE_3D), // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        40, // h_sync_width
        5, // v_sync_width
        220, // h_back_porch
        20, // v_back_porch
        1280, // h_disp_active
        720*2+30, // v_disp_active
        440, // h_front_porch
        5, // v_front_porch
      },
      NV_FALSE // partial
    };


static NvOdmDispDeviceMode s_1920_1080_31 =
    { 1920, // width
      1080, // height
      24, // bpp
      (50 << 16), // refresh
      148500, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        44, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        528, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1920_1080_32 =
    { 1920, // width
      1080, // height
      24, // bpp
      (24 << 16), // refresh
      74250, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        44, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        638, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1920_1080_32_3d =
    { 1920, // width
      1080, // height
      24, // bpp
      (24 << 16), // refresh
      74250*2, // frequency in khz
      (NVODM_DISP_MODE_FLAG_TYPE_HDMI|
      NVODM_DISP_MODE_FLAG_TYPE_3D), // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        44, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080*2+45, // v_disp_active
        638, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    };


static NvOdmDispDeviceMode s_1920_1080_33 =
    { 1920, // width
      1080, // height
      24, // bpp
      (25 << 16), // refresh
      74250, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        44, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        528, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    };

static NvOdmDispDeviceMode s_1920_1080_34 =
    { 1920, // width
      1080, // height
      24, // bpp
      (30 << 16), // refresh
      74250, // frequency in khz
      NVODM_DISP_MODE_FLAG_TYPE_HDMI, // flags
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
    };

static NvOdmDispDeviceMode *s_vesa_modes[] =
    {
        &s_640_480_60,
        &s_640_480_72,
        &s_640_480_75,
        &s_640_480_85,
        &s_720_400_70,
        &s_800_600_56,
        &s_800_600_60,
        &s_800_600_72,
        &s_800_600_75,
        &s_800_600_85,
        &s_1024_768_60,
        &s_1024_768_70,
        &s_1024_768_75,
        &s_1024_768_85,
        &s_1152_864_75,
        &s_1280_960_60,
        &s_1280_1024_60,
        &s_1280_1024_75,
        &s_1360_768_60,
        &s_1440_900_60,
        &s_1680_1050_60,
        &s_1600_1200_60,
    };

static NvOdmDispDeviceMode *s_hdmi_modes[] =
    {
        &s_640_480_1,
        &s_720_480_2,
        &s_1280_720_4,
        &s_1920_1080_16,
        &s_720_576_17,
        &s_1280_720_19,
        &s_1920_1080_31,
        &s_1920_1080_32,
        &s_1920_1080_33,
        &s_1920_1080_34,
    };

static NvBool
NvDdkDispPrivEdidExportEst3Res0( NvOdmDispDeviceMode *mode,
    NvEdidEst3Res0 t)
{
    switch( t ) {
    case NvEdidEst3Res0_1152_864_75:
        NvOsMemcpy( mode, &s_1152_864_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_1024_768_85:
        NvOsMemcpy( mode, &s_1024_768_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_800_600_85:
        NvOsMemcpy( mode, &s_800_600_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_848_480_60:
        NvOsMemcpy( mode, &s_848_480_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_640_480_85:
        NvOsMemcpy( mode, &s_640_480_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_720_400_85:
        NvOsMemcpy( mode, &s_720_400_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_640_400_85:
        NvOsMemcpy( mode, &s_640_400_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res0_640_350_85:
        NvOsMemcpy( mode, &s_640_350_85, sizeof(NvOdmDispDeviceMode) );
        break;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportEst3Res1( NvOdmDispDeviceMode *mode,
    NvEdidEst3Res1 t)
{
    switch( t ) {
    case NvEdidEst3Res1_1280_1024_85:
        NvOsMemcpy( mode, &s_1280_1024_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_1024_60:
        NvOsMemcpy( mode, &s_1280_1024_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_960_85:
        NvOsMemcpy( mode, &s_1280_960_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_960_60:
        NvOsMemcpy( mode, &s_1280_960_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_768_85:
        NvOsMemcpy( mode, &s_1280_768_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_768_75:
        NvOsMemcpy( mode, &s_1280_768_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_768_60:
        NvOsMemcpy( mode, &s_1280_768_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res1_1280_768_60_RB:
        NvOsMemcpy( mode, &s_1280_768_60_RB, sizeof(NvOdmDispDeviceMode) );
        break;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportEst3Res2( NvOdmDispDeviceMode *mode,
    NvEdidEst3Res2 t)
{
    switch( t ) {
    case NvEdidEst3Res2_1400_1050_75:
        NvOsMemcpy( mode, &s_1400_1050_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1400_1050_60:
        NvOsMemcpy( mode, &s_1400_1050_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1400_1050_60_RB:
        NvOsMemcpy( mode, &s_1400_1050_60_RB, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1440_900_85:
        NvOsMemcpy( mode, &s_1440_900_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1440_900_75:
        NvOsMemcpy( mode, &s_1440_900_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1440_900_60:
        NvOsMemcpy( mode, &s_1440_900_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1440_900_60_RB:
        NvOsMemcpy( mode, &s_1440_900_60_RB, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res2_1360_768_60:
        NvOsMemcpy( mode, &s_1360_768_60, sizeof(NvOdmDispDeviceMode) );
        break;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportEst3Res3( NvOdmDispDeviceMode *mode,
    NvEdidEst3Res3 t)
{
    switch( t ) {
    case NvEdidEst3Res3_1600_1200_70:
    case NvEdidEst3Res3_1600_1200_65:
    case NvEdidEst3Res3_1680_1050_85:
    case NvEdidEst3Res3_1680_1050_75:
    case NvEdidEst3Res3_1400_1050_85:
        return NV_FALSE;
    case NvEdidEst3Res3_1600_1200_60:
        NvOsMemcpy( mode, &s_1600_1200_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res3_1680_1050_60:
        NvOsMemcpy( mode, &s_1680_1050_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res3_1680_1050_60_RB:
        NvOsMemcpy( mode, &s_1680_1050_60_RB, sizeof(NvOdmDispDeviceMode) );
        break;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportEst3Res4( NvOdmDispDeviceMode *mode,
    NvEdidEst3Res4 t)
{
    switch( t ) {
    case NvEdidEst3Res4_1920_1200_60_RB:
        NvOsMemcpy( mode, &s_1920_1200_60_RB, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEst3Res4_1920_1200_60:
    case NvEdidEst3Res4_1856_1392_75:
    case NvEdidEst3Res4_1856_1392_60:
    case NvEdidEst3Res4_1792_1344_75:
    case NvEdidEst3Res4_1792_1344_60:
    case NvEdidEst3Res4_1600_1200_85:
    case NvEdidEst3Res4_1600_1200_75:
        return NV_FALSE;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportEst3Res5( NvOdmDispDeviceMode *mode,
    NvEdidEst3Res5 t)
{
    return NV_FALSE;
}

static NvBool
NvDdkDispPrivEdidExportEst1( NvOdmDispDeviceMode *mode,
    NvEdidEstablishedTiming1 t )
{
    switch( t ) {
    case NvEdidEstablishedTiming1_720_400_88:
        return NV_FALSE;
    case NvEdidEstablishedTiming1_720_400_70:
        NvOsMemcpy( mode, &s_720_400_70, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming1_640_480_75:
        NvOsMemcpy( mode, &s_640_480_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming1_640_480_72:
        NvOsMemcpy( mode, &s_640_480_72, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming1_640_480_67:
        // don't see a 640x480 @ 67
        return NV_FALSE;
    case NvEdidEstablishedTiming1_640_480_60:
        NvOsMemcpy( mode, &s_640_480_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming1_800_600_60:
        NvOsMemcpy( mode, &s_800_600_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming1_800_600_56:
        NvOsMemcpy( mode, &s_800_600_56, sizeof(NvOdmDispDeviceMode) );
        break;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportEst2( NvOdmDispDeviceMode *mode,
    NvEdidEstablishedTiming2 t )
{
    switch( t ) {
    case NvEdidEstablishedTiming2_1280_1024_75:
        NvOsMemcpy( mode, &s_1280_1024_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming2_1024_768_75:
        NvOsMemcpy( mode, &s_1024_768_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming2_1024_768_70:
        NvOsMemcpy( mode, &s_1024_768_70, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming2_1024_768_60:
        NvOsMemcpy( mode, &s_1024_768_60, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming2_1024_768_87:
        // treat this like as 1024x768 @ 85hz.
        NvOsMemcpy( mode, &s_1024_768_85, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming2_832_624_75:
        return NV_FALSE;
    case NvEdidEstablishedTiming2_800_600_75:
        NvOsMemcpy( mode, &s_800_600_75, sizeof(NvOdmDispDeviceMode) );
        break;
    case NvEdidEstablishedTiming2_800_600_72:
        NvOsMemcpy( mode, &s_800_600_72, sizeof(NvOdmDispDeviceMode) );
        break;
    default:
        NV_ASSERT( !"bad est timing" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static void
NvDdkDispPrivEdidExportEst3( NvOdmDispDeviceMode *mode, NvEdidEstablishedTiming3 Est3, 
    NvU32 *pCount, NvU32 nModes)
{
    NvU32 i;
    NvBool b;
    NvU32 count = *pCount;

    for( i = 0; i < 8 && count < nModes; i++ )
    {
        if( (Est3.Res0 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst3Res0( mode,
                (NvEdidEst3Res0)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    for( i = 0; i < 8 && count < nModes; i++ )
    {
        if( (Est3.Res1 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst3Res1( mode,
                (NvEdidEst3Res1)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    for( i = 0; i < 8 && count < nModes; i++ )
    {
        if( (Est3.Res2 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst3Res2( mode,
                (NvEdidEst3Res2)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    for( i = 0; i < 8 && count < nModes; i++ )
    {
        if( (Est3.Res3 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst3Res3( mode,
                (NvEdidEst3Res3)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    for( i = 0; i < 8 && count < nModes; i++ )
    {
        if( (Est3.Res4 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst3Res4( mode,
                (NvEdidEst3Res4)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    for( i = 0; i < 8 && count < nModes; i++ )
    {
        if( (Est3.Res5 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst3Res5( mode,
                (NvEdidEst3Res5)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }
    *pCount = count;
}

/* Note on VESA vs nvap timing:
 * nvap's origin is from from the front porch, VESA is from addressable video,
 * so need to translate a little bit:
 *
 * NVAP          VESA
 * ----          ----
 * REF_TO_SYNC = Front porch
 * SYNC_WIDTH  = Sync time
 * BACK_PORCH  = Back porch + Top/left border
 * DISP_ACTIVE = Addr time
 * FRONT_PORCH = Bot/right border + Front porch
 */
static void
NvDdkDispPrivEdidExportDTD( NvOdmDispDeviceMode *mode,
    NvEdidDetailedTimingDescription *desc )
{
    /* the DTD doesn't give the front and back porch. have to calculate
     * via "offset", "border", "pulse" (sync width), and total sync time.
     *
     * offset - boarder = front porch
     * total - front porch - pulse = back porch
     */

    mode->width = desc->HorizActive;
    mode->height = desc->VertActive;
    mode->bpp = 24;
    mode->refresh = 0;
    mode->frequency = desc->PixelClock;
    mode->flags = ((desc->Flags >> 7) & 1) ?
        NVODM_DISP_MODE_FLAG_INTERLACED : 0;
    mode->flags |= NVODM_DISP_MODE_FLAG_TYPE_DTD;
    mode->timing.Horiz_RefToSync = 1;
    mode->timing.Vert_RefToSync = 1;
    mode->timing.Horiz_SyncWidth = desc->HorizPulse;
    mode->timing.Vert_SyncWidth = desc->VertPulse;
    mode->timing.Horiz_DispActive = desc->HorizActive;
    mode->timing.Vert_DispActive = desc->VertActive;
    mode->timing.Horiz_FrontPorch = desc->HorizOffset - desc->HorizBoarder;
    mode->timing.Vert_FrontPorch = desc->VertOffset - desc->VertBoarder;
    mode->timing.Horiz_BackPorch =
        desc->HorizBlank - mode->timing.Horiz_FrontPorch - desc->HorizPulse;
    mode->timing.Vert_BackPorch =
        desc->VertBlank - mode->timing.Vert_FrontPorch - desc->VertPulse;

    if( desc->bNative )
    {
        mode->flags |= NVODM_DISP_MODE_FLAG_NATIVE;
    }
}

static NvBool
NvDdkDispPrivEdidExportSTI( NvOdmDispDeviceMode *mode,
    NvEdidStandardTimingIdentification *sti)
{
    if ((sti->HorizActive == 640) &&
        (sti->VertActive == 480) &&
        (sti->RefreshRate == 85))
    {
        NvOsMemcpy( mode, &s_640_480_85, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 800) &&
        (sti->VertActive == 600) &&
        (sti->RefreshRate == 85))
    {
        NvOsMemcpy( mode, &s_800_600_85, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 1152) &&
        (sti->VertActive == 864) &&
        (sti->RefreshRate == 75))
    {
        NvOsMemcpy( mode, &s_1152_864_75, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 1280) &&
        (sti->VertActive == 960) &&
        (sti->RefreshRate == 60))
    {
        NvOsMemcpy( mode, &s_1280_960_60, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 1280) &&
        (sti->VertActive == 1024) &&
        (sti->RefreshRate == 60))
    {
        NvOsMemcpy( mode, &s_1280_1024_60, sizeof(NvOdmDispDeviceMode));
    }
    // treat 1360x765@60Hz like as 1360x768@60Hz.
    else if ((sti->HorizActive == 1360) &&
        ((sti->VertActive == 768) || (sti->VertActive == 765)) &&
        (sti->RefreshRate == 60))
    {
        NvOsMemcpy( mode, &s_1360_768_60, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 1440) &&
        (sti->VertActive == 900) &&
        (sti->RefreshRate == 60))
    {
        NvOsMemcpy( mode, &s_1440_900_60, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 1600) &&
        (sti->VertActive == 1200) &&
        (sti->RefreshRate == 60))
    {
        NvOsMemcpy( mode, &s_1600_1200_60, sizeof(NvOdmDispDeviceMode));
    }
    else if ((sti->HorizActive == 1680) &&
        (sti->VertActive == 1050) &&
        (sti->RefreshRate == 60))
    {
        NvOsMemcpy( mode, &s_1680_1050_60, sizeof(NvOdmDispDeviceMode));
    }
    else
    {
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
NvDdkDispPrivEdidExportSVD( NvOdmDispDeviceMode *mode,
    NvEdidShortVideoDescriptor *desc, NvBool check3d )
{
    NvOdmDispDeviceMode *pSrcMode = NULL;

    switch( desc->Id ) {
    case NvEdidVideoId_640_480_1:
        if (!check3d)
        {
            pSrcMode = &s_640_480_1;
        }
        break;
    case NvEdidVideoId_720_480_2:
    case NvEdidVideoId_720_480_3:
        if (!check3d)
        {
            pSrcMode = &s_720_480_2;
        }
        break;
    case NvEdidVideoId_1280_720_4:
        if (!check3d)
        {
            pSrcMode = &s_1280_720_4;
        }
        else
        {
            pSrcMode = &s_1280_720_4_3d;
        }
        break;
    case NvEdidVideoId_1920_1080_16:
        if (!check3d)
        {
            pSrcMode = &s_1920_1080_16;
        }
        break;
    case NvEdidVideoId_720_576_17:
    case NvEdidVideoId_720_576_18:
        if (!check3d)
        {
            pSrcMode = &s_720_576_17;
        }
        break;
    case NvEdidVideoId_1280_720_19:
        if (!check3d)
        {
            pSrcMode = &s_1280_720_19;
        }
        else
        {
            pSrcMode = &s_1280_720_19_3d;
        }
        break;
    case NvEdidVideoId_1920_1080_31:
        if (!check3d)
        {
            pSrcMode = &s_1920_1080_31;
        }
        break;
    case NvEdidVideoId_1920_1080_32:
        if (!check3d)
        {
            pSrcMode = &s_1920_1080_32;
        }
        else
        {
            pSrcMode = &s_1920_1080_32_3d;
        }
        break;
    case NvEdidVideoId_1920_1080_33:
        if (!check3d)
        {
            pSrcMode = &s_1920_1080_33;
        }
        break;
    case NvEdidVideoId_1920_1080_34:
        if (!check3d)
        {
            pSrcMode = &s_1920_1080_34;
        }
        break;
    default:
        NV_ASSERT( !"bad svd" );
        break;
    }

    if (pSrcMode && mode)
        NvOsMemcpy( mode, pSrcMode, sizeof(NvOdmDispDeviceMode) );

    if (desc->Native && mode)
        mode->flags |= NVODM_DISP_MODE_FLAG_NATIVE; 

    if (pSrcMode)
        return NV_TRUE;
    else
        return NV_FALSE;
}

static void
NvDdkDispPrivExportModes( NvU32 *nModes, NvOdmDispDeviceMode *modes,
    NvBool bAll )
{
    NvU32 n;
    NvU32 i;

    n = NV_ARRAY_SIZE( s_vesa_modes );
    if( bAll )
    {
        n += NV_ARRAY_SIZE( s_hdmi_modes );
    }

    if( !(*nModes) )
    {
        NV_ASSERT( modes == 0 );
        *nModes = n;
        return;
    }

    n = NV_MIN( *nModes, n );

    for( i = 0; i < n; i++ )
    {
        NvOdmDispDeviceMode *m;
        if( bAll && i >= NV_ARRAY_SIZE( s_vesa_modes ) )
        {
            m = s_hdmi_modes[i - NV_ARRAY_SIZE( s_vesa_modes )];
        }
        else
        {
            m = s_vesa_modes[i];
        }

        NvOsMemcpy( modes, m, sizeof(NvOdmDispDeviceMode) );
        modes++;
    }
}

void
NvDdkDispEdidExport( NvDdkDispEdid *edid, NvU32 *nModes,
    NvOdmDispDeviceMode *modes, NvU32 flags )
{
    NvU32 n;
    NvU32 i;
    NvU32 count = 0;
    NvOdmDispDeviceMode *mode;
    NvBool b;

    NV_ASSERT( nModes );

    if( !edid )
    {
        if( flags & NVDDK_DISP_EDID_EXPORT_FLAG_VESA )
        {
            NvDdkDispPrivExportModes( nModes, modes, NV_FALSE );
        }
        else if( flags & NVDDK_DISP_EDID_EXPORT_FLAG_ALL )
        {
            NvDdkDispPrivExportModes( nModes, modes, NV_TRUE );
        }
        else
        {
            NV_ASSERT( !"no edid and no flags specified" );
        }

        return;
    }

    n = NvDdkDispPrivEdidCountModes( edid );

    if( !(*nModes) )
    {
        NV_ASSERT( modes == 0 );
        *nModes = n;
        return;
    }

    n = NV_MIN( *nModes, n );
    mode = modes;

    /* standard video descriptors */
    for( i = 0; i < edid->nSVDs && count < n; i++ )
    {
        b = NvDdkDispPrivEdidExportSVD( mode, &edid->SVD[i], NV_FALSE );
        if( b )
        {
            mode++;
            count++;
        }
    }

    if (edid->bHas3dSupport)
    {
        for( i = 0; i < edid->nSVDs && count < n; i++ )
        {
            b = NvDdkDispPrivEdidExportSVD( mode, &edid->SVD[i], NV_TRUE );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    /* established timings */
    for( i = 0; i < 8 && count < n; i++ )
    {
        if( (edid->EstTiming1 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst1( mode,
                    (NvEdidEstablishedTiming1)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    for( i = 0; i < 8 && count < n; i++ )
    {
        if( (edid->EstTiming2 >> i) & 1 )
        {
            b = NvDdkDispPrivEdidExportEst2( mode,
                    (NvEdidEstablishedTiming2)(1 << i) );
            if( b )
            {
                mode++;
                count++;
            }
        }
    }

    if( edid->Revision >=4 )
    {
        NvDdkDispPrivEdidExportEst3( mode, edid->EstTiming3, &count, n);
    }

    /* standard timing Identification */
    for (i = 0; i < edid->nSTIs && count < n; i++ )
    {
        b = NvDdkDispPrivEdidExportSTI( mode, &edid->STI[i] );
        if( b)
        {
            mode++;
            count++;
        }
    }

    /* detailed timing descriptors -- the preferred timing is in here */
    for( i = 0; i < edid->nDTDs && count < n; i++ )
    {
        NvDdkDispPrivEdidExportDTD( mode, &edid->DTD[i] );
        // FIXME: don't export interlaced yet
        if( (mode->flags & NVODM_DISP_MODE_FLAG_INTERLACED) == 0 )
        {
            mode++;
            count++;
        }
    }

    *nModes = count;
}

NvError NvDdkDispTestGetEdid( 
    NvDdkDispDisplayHandle hDisplay,
    NvU8 *edid, 
    NvU32 *size)
{
    const NvOdmPeripheralConnectivity *conn = NULL;
    const NvOdmIoAddress *addrlist = NULL;
    NvRmI2cHandle i2c = NULL;
    NvOdmServicesPmuHandle hPmu = NULL;
    NvError e;
    NvU32 i, j, addr, inst, n;
    NvU8 raw[NVDDK_DISP_EDID_BLOCK_SIZE];
    DcHdmiOdm *adpt;
    NvU32 rail_mask = 0, count = 0, left = 0;
    NvU8 segment, tmp;

    NV_DEBUG_CODE(
        if ( *size )
        {
            NV_ASSERT( edid );
        }
    );

    conn = NvEdidDiscover( hDisplay );
    if ( !conn )
    {
        return NvError_NotSupported;
    }
    
    /* the DDC i2c address is always first in the address list */
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_I2c )
        {
            addrlist = &conn->AddressList[i];
            break;
        }
    }

    if( !addrlist )
    {
        return NvError_NotSupported;
    }

    addr = addrlist->Address;
    inst = addrlist->Instance;

    /* enable the power rails (may already be enabled, which is fine) */
    hPmu = NvOdmServicesPmuOpen();
    NvDdkDispEdidPowerConfig(conn, hPmu, &rail_mask, NV_TRUE);

    adpt = DcHdmiOdmOpen();
    if( hDisplay->attr.Type != NvDdkDispDisplayType_HDMI ||
        !adpt->i2cTransaction )
    {
        // fall back to RmI2c (not HDMI or no NvOdmDispHdmiI2cTransaction)
        NV_CHECK_ERROR_CLEANUP(
            NvRmI2cOpen( hDisplay->hDisp->hDevice, NvOdmIoModule_I2c, inst, &i2c )
        );

        if( hDisplay->attr.Type == NvDdkDispDisplayType_CRT )
            NvDdkDispEdidCrtDdcMuxerConfig(i2c);
    }

    /* read the raw edid bytes */
    if( !NvEdidI2cRead( i2c, adpt, addr, 0, 0, &raw[0] ) )
    {
        e = NvError_BadValue;
        goto fail;
    }
    count += 128;
    if (*size)
    {
        if (*size > 128)
        {
            left = *size - 128;
            NvOsMemcpy( edid, raw, 128 );
        }
        else
        {
            NvOsMemcpy( edid, raw, *size );
            left = 0;
        }
    }

    tmp = raw[126];
    n = j = 1;
    segment = 0;
    while ( tmp ) /* more 128 byte edids follow this one */
    {
        /* read the raw edid bytes */
        if( !NvEdidI2cRead( i2c, adpt, addr, segment, (128 * n), &raw[0]) )
        {
            e = NvError_BadValue;
            goto fail;
        }  

        /* segment pointer accesses 2 blocks at a time. */
        n ^= 1;

        if( !n )
        {
            segment++;
        }
        tmp--;
        count += 128;
        if ((*size) && left)
        {
            if (left > 128)
            {
                left = left - 128;
                NvOsMemcpy( &edid[128 * j], raw, 128);
            }
            else
            {
                NvOsMemcpy( &edid[128 * j], raw, left);
                left = 0;
            }
            j++;
        }

    }
    
    if ( (*size == 0) || (*size > count) )
    {
        *size = count;
    }
    
    e = NvSuccess;
    goto clean;

fail:
clean:
    /* disable rails that were enabled here */
    NvDdkDispEdidPowerConfig(conn, hPmu, &rail_mask, NV_FALSE);
    
    NvOdmServicesPmuClose( hPmu );
    NvRmI2cClose( i2c );
    DcHdmiOdmClose( adpt );

    return e;
}
