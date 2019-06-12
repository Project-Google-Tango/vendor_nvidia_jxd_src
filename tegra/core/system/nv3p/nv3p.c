/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nv3p.h"
#include "nv3p_bytestream.h"
#include "nv3p_transport.h"


/* double the currently-largest command size, just to have some wiggle-room
 * (updates to commands without fixing this on accident, etc.)
 */
#define NV3P_MAX_COMMAND_SIZE \
    ((NV3P_PACKET_SIZE_BASIC + \
    NV3P_PACKET_SIZE_COMMAND + \
    (sizeof(Nv3pCmdCreatePartition)) + \
    NV3P_PACKET_SIZE_FOOTER) * 2)

#define NV3P_MAX_ACK_SIZE \
    NV3P_PACKET_SIZE_BASIC + \
    NV3P_PACKET_SIZE_NACK + \
    NV3P_PACKET_SIZE_FOOTER

typedef struct Nv3pSocketRec
{
    Nv3pTransportHandle hTrans;
    NvU32 sequence;
    NvBool bCrypt;

    NvU32 recv_sequence;

    /* for partial reads */
    NvU32 bytes_remaining;
    NvU32 recv_checksum;

    /* the last nack code */
    Nv3pNackCode last_nack;

    unsigned char s_buffer[NV3P_MAX_COMMAND_SIZE];
    unsigned char s_args[NV3P_MAX_COMMAND_SIZE]; // handed out to clients
    unsigned char s_retbuf[NV3P_MAX_COMMAND_SIZE];
    unsigned char s_ack[NV3P_MAX_ACK_SIZE];
} Nv3pSocket;

#define WRITE64( packet, data ) \
    do { \
        (packet)[0] = (NvU8)((NvU64)(data)) & 0xff; \
        (packet)[1] = (NvU8)(((NvU64)(data)) >> 8) & 0xff; \
        (packet)[2] = (NvU8)(((NvU64)(data)) >> 16) & 0xff; \
        (packet)[3] = (NvU8)(((NvU64)(data)) >> 24) & 0xff; \
        (packet)[4] = (NvU8)(((NvU64)(data)) >> 32) & 0xff; \
        (packet)[5] = (NvU8)(((NvU64)(data)) >> 40) & 0xff; \
        (packet)[6] = (NvU8)(((NvU64)(data)) >> 48) & 0xff; \
        (packet)[7] = (NvU8)(((NvU64)(data)) >> 56) & 0xff; \
        (packet) += 8; \
    } while( 0 )

#define WRITE32( packet, data ) \
    do { \
        (packet)[0] = (data) & 0xff; \
        (packet)[1] = ((data) >> 8) & 0xff; \
        (packet)[2] = ((data) >> 16) & 0xff; \
        (packet)[3] = ((data) >> 24) & 0xff; \
        (packet) += 4; \
    } while( 0 )

#define WRITE8( packet, data ) \
    do { \
        (packet)[0] = (data) & 0xff; \
        (packet) += 1; \
    } while( 0 )

#define READ64( packet, data ) \
    do { \
        (data) = (NvU64)((packet)[0] \
               | ((NvU64)((packet)[1]) << 8) \
               | ((NvU64)((packet)[2]) << 16) \
               | ((NvU64)((packet)[3]) << 24) \
               | ((NvU64)((packet)[4]) << 32) \
               | ((NvU64)((packet)[5]) << 40) \
               | ((NvU64)((packet)[6]) << 48) \
               | ((NvU64)((packet)[7]) << 56)); \
        (packet) += 8; \
    } while( 0 )

#define READ32( packet, data ) \
    do { \
        (data) = (NvU32)((packet)[0] \
               | (((packet)[1]) << 8) \
               | (((packet)[2]) << 16) \
               | (((packet)[3]) << 24)); \
        (packet) += 4; \
    } while( 0 )

#define READ32_PUNNED( packet, DestType, data ) \
    do {                                        \
        union {                                 \
            DestType PunOut;                    \
            NvU32    PunIn;                     \
        } PunUnion;                             \
        NvU32 Temp;                             \
        READ32 ( (packet), Temp );              \
        PunUnion.PunIn = Temp;                  \
        (data) = PunUnion.PunOut;               \
    } while (0);

#define READ8( packet, data ) \
    do { \
        (data) = (packet)[0]; \
        (packet) += 1; \
    } while( 0 )

/* header structures for fun */
typedef struct Nv3pHdrBasicRec
{
    NvU32 Version;
    NvU32 PacketType;
    NvU32 Sequence;
} Nv3pHdrBasic;

NvError
Nv3pOpen( Nv3pSocketHandle *h3p, Nv3pTransportMode mode, NvU32 instance )
{
    NvError e;
    Nv3pSocket *sock;

    sock = NvOsAlloc(sizeof(Nv3pSocket));
    NvOsMemset( sock, 0, sizeof(Nv3pSocket) );
    sock->last_nack = Nv3pNackCode_Success;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReopen( &sock->hTrans, mode, instance )
    );

    *h3p = sock;
    return NvSuccess;

fail:
    // FIXME: erorr codes
    return e;
}


NvError
Nv3pReOpen(Nv3pSocketHandle *h3p, Nv3pTransportMode mode, NvU32 instance)
{
    NvError e;
    Nv3pSocket *pSock = NvOsAlloc(sizeof(Nv3pSocket));
    if (!pSock)
        return NvError_InsufficientMemory;
    NvOsMemset(pSock, 0, sizeof(Nv3pSocket));
    pSock->last_nack = Nv3pNackCode_Success;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReopen(&pSock->hTrans, mode, instance)
    );
    *h3p = pSock;
    return NvSuccess;

fail:
    if (pSock)
        NvOsFree(pSock);
    return e;
}

void
Nv3pClose( Nv3pSocketHandle h3p )
{
    if( !h3p )
    {
        return;
    }

    Nv3pTransportClose( h3p->hTrans );
    NvOsFree(h3p);
}

/**
 * Just sum the bits. Don't get two's compliment
 */
static NvU32
Nv3pPrivChecksum( NvU8 *packet, NvU32 length )
{
    NvU32 i;
    NvU32 sum;
    NvU8 *tmp;

    sum = 0;
    tmp = packet;
    for( i = 0; i < length; i++ )
    {
        sum += *tmp;
        tmp++;
    }

    return sum;
}

static NvBool
Nv3pPrivReceiveBasicHeader( Nv3pSocketHandle h3p, Nv3pHdrBasic *hdr,
    NvU32 *checksum )
{
    NvError e;
    NvU32 length;
    NvU8 *tmp;

    tmp = &h3p->s_buffer[0];
    length = NV3P_PACKET_SIZE_BASIC;
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReceive( h3p->hTrans, tmp, length, 0, 0 )
    );

    READ32( tmp, hdr->Version );
    READ32( tmp, hdr->PacketType );
    READ32( tmp, hdr->Sequence );

    if( hdr->Version != 0x1 )
    {
        goto fail;
    }

    h3p->recv_sequence = hdr->Sequence;

    *checksum = Nv3pPrivChecksum(&h3p->s_buffer[0], length);
    return NV_TRUE;

fail:
    return NV_FALSE;
}

static NvError
Nv3pPrivWaitAck( Nv3pSocketHandle h3p )
{
    NvError e;
    Nv3pHdrBasic hdr = {0,0,0};
    NvU32 recv_checksum = 0, checksum;
    NvU32 length = 0;
    NvBool b;

    h3p->last_nack = Nv3pNackCode_Success;

    b = Nv3pPrivReceiveBasicHeader( h3p, &hdr, &recv_checksum );
    if( !b )
    {
        e = NvError_Nv3pUnrecoverableProtocol;
        goto fail;
    }

    length = NV3P_PACKET_SIZE_BASIC;
    switch( hdr.PacketType ) {
    case Nv3pByteStream_PacketType_Ack:
        length += NV3P_PACKET_SIZE_ACK;
        break;
    case Nv3pByteStream_PacketType_Nack:
        length += NV3P_PACKET_SIZE_NACK;
        break;
    default:
        e = NvError_Nv3pUnrecoverableProtocol;
        goto fail;
    }

    if( hdr.PacketType == Nv3pByteStream_PacketType_Nack )
    {
        /* read 4 more bytes to get the error code */
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans,
                (NvU8 *)&h3p->last_nack, 4, 0, 0 )
        );

        recv_checksum += Nv3pPrivChecksum( (NvU8 *)&h3p->last_nack, 4 );
    }

    /* get/verify the checksum */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReceive( h3p->hTrans, (NvU8 *)&checksum, 4, 0, 0 )
    );

    if( recv_checksum + checksum != 0 )
    {
        e = NvError_Nv3pUnrecoverableProtocol;
        goto fail;
    }

    if( hdr.Sequence != h3p->sequence - 1 )
    {
        e = NvError_Nv3pUnrecoverableProtocol;
        goto fail;
    }

    if( hdr.PacketType == Nv3pByteStream_PacketType_Nack )
    {
        return NvError_Nv3pPacketNacked;
    }

    return NvSuccess;

fail:
    return e;
}

static NvError
Nv3pPrivDrainPacket( Nv3pSocketHandle h3p, Nv3pHdrBasic *hdr )
{
    NvError e;
    e = NvError_Nv3pUnrecoverableProtocol;

    /* consume an ack or nack packet. the other packet types are not
     * recoverable.
     */
    if( hdr->PacketType == Nv3pByteStream_PacketType_Ack ||
        hdr->PacketType == Nv3pByteStream_PacketType_Nack )
    {
        NvU32 checksum;

        if( hdr->PacketType == Nv3pByteStream_PacketType_Nack )
        {
            Nv3pNackCode code;

            /* read 4 more bytes to get the error code */
            NV_CHECK_ERROR_CLEANUP(
                Nv3pTransportReceive( h3p->hTrans, (NvU8 *)&code, 4, 0, 0 )
            );

            h3p->last_nack = code;
        }

        /* drain the checksum */
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans, (NvU8 *)&checksum, 4, 0, 0 )
        );

        e = NvError_Nv3pBadPacketType;
    }

fail:
    return e;
}

static void
Nv3pPrivWriteBasicHeader( Nv3pSocketHandle h3p, Nv3pByteStream_PacketType type,
    NvU32 sequence, NvU8 *packet )
{
    NvU8 *tmp;
    tmp = packet;

    WRITE32( tmp, 0x1 );
    WRITE32( tmp, type );
    WRITE32( tmp, sequence );
}

static void
Nv3pPrivWriteFooter( Nv3pSocketHandle h3p, NvU32 checksum, NvU8 *packet )
{
    WRITE32( packet, checksum );
}

static void
Nv3pPrivWriteCmd( Nv3pSocketHandle h3p, Nv3pCommand command, void *args,
    NvU32 *length, NvU8 *packet )
{
    NvU8 *tmp;

    tmp = packet;

    switch( command ) {
    case Nv3pCommand_GetBit:
    case Nv3pCommand_GetBct:
    case Nv3pCommand_DeleteAll:
    case Nv3pCommand_FormatAll:
    case Nv3pCommand_Obliterate:
    case Nv3pCommand_Go:
    case Nv3pCommand_Sync:
    case Nv3pCommand_EndPartitionConfiguration:
    case Nv3pCommand_EndVerifyPartition:
    case Nv3pCommand_GetDevInfo:
    case Nv3pCommand_GetNv3pServerVersion:
    case Nv3pCommand_BDKGetSuiteInfo:
    case Nv3pCommand_SkipSync:
    case Nv3pCommand_GetBoardDetails:
    case Nv3pCommand_Recovery:
    case Nv3pCommand_ReadBoardInfo:
        // no args or output only
        *length = 0;
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        break;
    case Nv3pCommand_Status:
    {
        NvU32 len;
        Nv3pCmdStatus *a = (Nv3pCmdStatus *)args;
        *length = (2 * 4) + NV3P_STRING_MAX;
        WRITE32( tmp, *length );
        WRITE32( tmp, command );

        NvOsMemset( tmp, 0, NV3P_STRING_MAX );
        len = NV_MIN( NvOsStrlen( a->Message ), NV3P_STRING_MAX - 1 );
        NvOsMemcpy( tmp, a->Message, len );
        tmp += NV3P_STRING_MAX;

        WRITE32( tmp, a->Code );
        WRITE32( tmp, a->Flags );
        break;
    }
    case Nv3pCommand_DownloadBct:
    {
        Nv3pCmdDownloadBct *a = (Nv3pCmdDownloadBct *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_SetBlHash:
    {
        Nv3pCmdBlHash *a = (Nv3pCmdBlHash *)args;
        *length = (2 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->BlIndex);
        WRITE32(tmp, a->Length);
        break;
    }
    case Nv3pCommand_UpdateBct:
    {
        Nv3pCmdUpdateBct *a = (Nv3pCmdUpdateBct *)args;
        *length = (3 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->Length);
        WRITE32(tmp, a->BctSection);
        WRITE32(tmp, a->PartitionId);
        break;
    }
    case Nv3pCommand_FuseWrite:
    {
        Nv3pCmdFuseWrite *a = (Nv3pCmdFuseWrite *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_FormatPartition:
    {
        Nv3pCmdFormatPartition *a = (Nv3pCmdFormatPartition *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->PartitionId );
        break;
    }
    case Nv3pCommand_DownloadBootloader:
    {
        Nv3pCmdDownloadBootloader *a = (Nv3pCmdDownloadBootloader *)args;
        *length = (2 * 4) + (1 * 8);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE64( tmp, a->Length );
        WRITE32( tmp, a->Address );
        WRITE32( tmp, a->EntryPoint );
        break;
    }
    case Nv3pCommand_SetDevice:
    {
        Nv3pCmdSetDevice *a = (Nv3pCmdSetDevice *)args;
        *length = (2 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Type );
        WRITE32( tmp, a->Instance );
        break;
    }
    case Nv3pCommand_DownloadPartition:
    {
        Nv3pCmdDownloadPartition *a = (Nv3pCmdDownloadPartition *)args;
        *length = (1 * 4) + (1 * 8);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Id );
        WRITE64( tmp, a->Length );
        break;
    }
    case Nv3pCommand_QueryPartition:
    {
        Nv3pCmdQueryPartition *a = (Nv3pCmdQueryPartition *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Id );
        break;
    }
    case Nv3pCommand_StartPartitionConfiguration:
    {
        Nv3pCmdStartPartitionConfiguration *a =
            (Nv3pCmdStartPartitionConfiguration *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->nPartitions );
        break;
    }
    case Nv3pCommand_CreatePartition:
    {
        NvU32 len;
        Nv3pCmdCreatePartition *a = (Nv3pCmdCreatePartition *)args;
#ifndef NV_EMBEDDED_BUILD
        *length = (2 * 8) + (8 * 4) + NV3P_STRING_MAX;
#else
        *length = (2 * 8) + (9 * 4) + NV3P_STRING_MAX;
#endif
        WRITE32( tmp, *length );
        WRITE32( tmp, command );

        NvOsMemset( tmp, 0, NV3P_STRING_MAX );
        len = NV_MIN( NvOsStrlen( a->Name ), NV3P_STRING_MAX - 1 );
        NvOsMemcpy( tmp, a->Name, len );
        tmp += NV3P_STRING_MAX;

        WRITE64( tmp, a->Size );
        WRITE64( tmp, a->Address );
        WRITE32( tmp, a->Id );
        WRITE32( tmp, a->Type );
        WRITE32( tmp, a->FileSystem );
        WRITE32( tmp, a->AllocationPolicy );
        WRITE32( tmp, a->FileSystemAttribute );
        WRITE32( tmp, a->PartitionAttribute );
        WRITE32( tmp, a->AllocationAttribute );
        WRITE32( tmp, a->PercentReserved );
#ifdef NV_EMBEDDED_BUILD
        WRITE32( tmp, a->IsWriteProtected );
#endif
        break;
    }
    case Nv3pCommand_VerifyPartitionEnable:
    case Nv3pCommand_VerifyPartition:
    {
        Nv3pCmdVerifyPartition *a = (Nv3pCmdVerifyPartition *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Id );
        break;
    }
    case Nv3pCommand_ReadPartition:
    {
        Nv3pCmdReadPartition *a = (Nv3pCmdReadPartition *)args;
        *length = (1 * 4) + (2 * 8);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Id );
        WRITE64( tmp, a->Offset );
        WRITE64( tmp, a->Length );
        break;
    }
    case Nv3pCommand_ReadPartitionTable:
    {
        Nv3pCmdReadPartitionTable *a = (Nv3pCmdReadPartitionTable *)args;
        *length = (1 * 8) + (2 * 4) + 1;
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE64(tmp, a->Length);
        WRITE32(tmp, a->NumLogicalSectors);
        WRITE32(tmp, a->StartLogicalSector);
        WRITE8(tmp,  a->ReadBctFromSD);
        break;
    }
    case Nv3pCommand_RawDeviceRead:
    case Nv3pCommand_RawDeviceWrite:
    {
        Nv3pCmdRawDeviceAccess *a = (Nv3pCmdRawDeviceAccess *)args;
        *length = (2 * 4) ;
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->StartSector);
        WRITE32( tmp, a->NoOfSectors);
        break;
    }
    case Nv3pCommand_SetBootPartition:
    {
        Nv3pCmdSetBootPartition *a = (Nv3pCmdSetBootPartition *)args;
        *length = (5 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Id );
        WRITE32( tmp, a->LoadAddress );
        WRITE32( tmp, a->EntryPoint );
        WRITE32( tmp, a->Version );
        WRITE32( tmp, a->Slot );
        break;
    }
    case Nv3pCommand_OdmOptions:
    {
        Nv3pCmdOdmOptions *a = (Nv3pCmdOdmOptions *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Options );
        break;
    }
    case Nv3pCommand_SetBootDevType:
    {
        Nv3pCmdSetBootDevType *a = (Nv3pCmdSetBootDevType *)args;
        *length = (1 * 4);

        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->DevType);
        break;
    }
    case Nv3pCommand_SetBootDevConfig:
    {
        Nv3pCmdSetBootDevConfig *a = (Nv3pCmdSetBootDevConfig *)args;
        *length = (1 * 4);

        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->DevConfig);
        break;
    }
    case Nv3pCommand_OdmCommand:
    {
        Nv3pCmdOdmCommand *a = (Nv3pCmdOdmCommand *)args;
        switch ( a->odmExtCmd ) {
        case Nv3pOdmExtCmd_FuelGaugeFwUpgrade:
        {
            *length = (1 * 4) + (2 * 8);
            WRITE32(tmp, *length);
            WRITE32(tmp, command);
            WRITE32(tmp, a->odmExtCmd);
            WRITE64(tmp, a->odmExtCmdParam.fuelGaugeFwUpgrade.FileLength1);
            WRITE64(tmp, a->odmExtCmdParam.fuelGaugeFwUpgrade.FileLength2);
            break;
        }
        case Nv3pOdmExtCmd_UpdateuCFw:
        {
            *length = (2 * 4);
            WRITE32(tmp, *length);
            WRITE32(tmp, command);
            WRITE32(tmp, a->odmExtCmd);
            WRITE64(tmp, a->odmExtCmdParam.updateuCFw.FileSize);
            break;
        }
        case Nv3pOdmExtCmd_VerifySdram:
        {
            *length = (1 * 4) + (1 * 4);
            WRITE32(tmp, *length);
            WRITE32(tmp, command);
            WRITE32(tmp, a->odmExtCmd);
            WRITE32(tmp, a->odmExtCmdParam.verifySdram.Value);
            break;
        }
        case Nv3pOdmExtCmd_Get_Tnid:
        {
            *length = (1 * 4);
            WRITE32( tmp, *length );
            WRITE32( tmp, command );
            WRITE32(tmp, a->odmExtCmd);
            break;
        }
        case Nv3pOdmExtCmd_Verify_Tid:
        {
            *length = (2 * 4);
            WRITE32( tmp, *length );
            WRITE32( tmp, command );
            WRITE32(tmp, a->odmExtCmd);
            WRITE32(tmp, a->odmExtCmdParam.tnid_info.length);
            break;
        }
        case Nv3pOdmExtCmd_LimitedPowerMode:
        {
            *length = (1 * 4);
            WRITE32( tmp, *length );
            WRITE32( tmp, command );
            WRITE32(tmp, a->odmExtCmd);
            break;
        }
        case Nv3pOdmExtCmd_TegraTabFuseKeys:
        case Nv3pOdmExtCmd_TegraTabVerifyFuse:
        {
            *length = (1 * 4) + (2*4);
            WRITE32(tmp, *length);
            WRITE32(tmp, command);
            WRITE32(tmp, a->odmExtCmd);
            WRITE64(tmp, a->odmExtCmdParam.tegraTabFuseKeys.FileLength);
            break;
        }
        case Nv3pOdmExtCmd_UpgradeDeviceFirmware:
        {
            *length = (1 * 4) + (2 * 4);
            WRITE32(tmp, *length);
            WRITE32(tmp, command);
            WRITE32(tmp, a->odmExtCmd);
            WRITE64(tmp, a->odmExtCmdParam.upgradeDeviceFirmware.FileLength);
            break;
        }
        default:
            NV_ASSERT( !"bad command" );
        }
        break;
    }
    case Nv3pCommand_SetTime:
    {
        Nv3pCmdSetTime *a = (Nv3pCmdSetTime *)args;
        *length = (2 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );

        WRITE32( tmp, a->Seconds);
        WRITE32( tmp, a->Milliseconds);
        break;
    }
    case Nv3pCommand_NvPrivData:
    {
        Nv3pCmdNvPrivData *a = (Nv3pCmdNvPrivData *)args;
        *length = (1 * 4);
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_RunBDKTest:
    {
        NvU32 len;
        Nv3pCmdRunBDKTest *a = (Nv3pCmdRunBDKTest *)args;
        *length = (2 * NV3P_STRING_MAX) + (4 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);

        NvOsMemset(tmp, 0, NV3P_STRING_MAX);
        len = NV_MIN(NvOsStrlen(a->Suite), NV3P_STRING_MAX - 1);
        NvOsMemcpy(tmp, a->Suite, len);
        tmp += NV3P_STRING_MAX;

        NvOsMemset(tmp, 0, NV3P_STRING_MAX);
        len = NV_MIN(NvOsStrlen(a->Argument), NV3P_STRING_MAX - 1);
        NvOsMemcpy(tmp, a->Argument, len);
        tmp += NV3P_STRING_MAX;

        WRITE32(tmp, a->PrivData.Instance);
        WRITE32(tmp, a->PrivData.Mem_address);
        WRITE32(tmp, a->PrivData.Reserved);
        WRITE32(tmp, a->PrivData.File_size);
        break;
    }
    case Nv3pCommand_SetBDKTest:
    {
        Nv3pCmdSetBDKTest *a = (Nv3pCmdSetBDKTest *)args;
        *length = (1 * 4) + 1;
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE8(tmp, a->StopOnFail);
        WRITE32(tmp, a->Timeout);
        break;
    }
    case Nv3pCommand_symkeygen:
    {
        Nv3pCmdSymKeyGen*a = (Nv3pCmdSymKeyGen *)args;
        *length = (1 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->Length);
        break;
    }
    case Nv3pCommand_DFuseBurn:
    {
        Nv3pCmdDFuseBurn *a = (Nv3pCmdDFuseBurn *)args;
        *length = (2 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->bct_length);
        WRITE32(tmp, a->fuse_length);
        break;
    }
#if ENABLE_NVDUMPER
    case Nv3pCommand_NvtbootRAMDump:
    {
        Nv3pCmdNvtbootRAMDump *a = (Nv3pCmdNvtbootRAMDump *)args;
        *length = (2 * 4) ;
        WRITE32( tmp, *length );
        WRITE32( tmp, command );
        WRITE32( tmp, a->Offset);
        WRITE32( tmp, a->Length);
        break;
    }
#endif
    case Nv3pCommand_GetPlatformInfo:
    {
        Nv3pCmdGetPlatformInfo *a = (Nv3pCmdGetPlatformInfo *)args;
        *length = 1;
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE8(tmp, a->SkipAutoDetect);
        break;
    }
    case Nv3pCommand_Reset:
    {
        Nv3pCmdReset *a = (Nv3pCmdReset *)args;
        *length = (2 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->Reset);
        WRITE32(tmp, a->DelayMs);
        break;
    }
    case Nv3pCommand_ReadNctItem:
    {
        Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)args;

        *length = (1 * 4);
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->EntryIdx);
        break;
    }
    case Nv3pCommand_WriteNctItem:
    {
        Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)args;
        NvU32 len = sizeof(a->Data);

        *length = (1 * 4) + len;
        WRITE32(tmp, *length);
        WRITE32(tmp, command);
        WRITE32(tmp, a->EntryIdx);

        NvOsMemset(tmp, 0, len);
        NvOsMemcpy(tmp, &a->Data, len);
        tmp += len;
        break;
    }
    default:
        NV_ASSERT( !"bad command" );
    }
}

static NvBool
Nv3pPrivGetCmdReturn( Nv3pSocketHandle h3p, Nv3pCommand command, void *args )
{
    NvError e;
    NvU32 length;
    NvU8 *tmp = &h3p->s_buffer[0];

    switch( command ) {
    case Nv3pCommand_GetPlatformInfo:
    {
        Nv3pCmdGetPlatformInfo *a = (Nv3pCmdGetPlatformInfo *)args;

        length = (18 * 4) + 1;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( h3p, tmp, length, 0, 0 )
        );

        READ32( tmp, a->ChipUid.ecid_0);
        READ32( tmp, a->ChipUid.ecid_1);
        READ32( tmp, a->ChipUid.ecid_2);
        READ32( tmp, a->ChipUid.ecid_3);
        READ32( tmp, *(NvU32 *)&a->ChipId );
        READ32( tmp, a->ChipSku );
        READ32( tmp, a->BootRomVersion );
        READ32_PUNNED( tmp, Nv3pDeviceType, a->SecondaryBootDevice);
        READ32( tmp, a->OperatingMode );
        READ32( tmp, a->DeviceConfigStrap );
        READ32( tmp, a->DeviceConfigFuse );
        READ32( tmp, a->SdramConfigStrap );
        READ32_PUNNED( tmp, Nv3pDkStatus, a->DkBurned );
        READ8( tmp, a->HdmiEnable );
        READ8( tmp, a->MacrovisionEnable );
        READ8( tmp, a->SbkBurned );
        READ8( tmp, a->JtagEnable );
        READ32(tmp, a->BoardId.BoardNo);
        READ32(tmp, a->BoardId.BoardFab);
        READ32(tmp, a->BoardId.MemType);
        READ32(tmp, a->WarrantyFuse);
        READ8(tmp, a->SkipAutoDetect);
        break;
    }
    case Nv3pCommand_GetBoardDetails:
    {
        NvU32 Val = 0;

        length = (1 * 4);
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );

        READ32(tmp, Val);
        *(NvU32 *)args = Val;

        break;
    }

    case Nv3pCommand_GetBct:
    {
        Nv3pCmdGetBct *a = (Nv3pCmdGetBct *)args;
        length = (1 * 4);
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( h3p, tmp, length, 0, 0 )
        );

        READ32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_GetBit:
    {
        Nv3pCmdGetBit *a = (Nv3pCmdGetBit *)args;
        length = (1 * 4);
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( h3p, tmp, length, 0, 0 )
        );

        READ32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_QueryPartition:
    {
        Nv3pCmdQueryPartition *a = (Nv3pCmdQueryPartition *)args;
        length = 3 * 8;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( h3p, tmp, length, 0, 0 )
        );

        READ64( tmp, a->Size );
        READ64( tmp, a->Address );
        READ64( tmp, a->PartType );
        break;
    }
    case Nv3pCommand_ReadPartition:
    {
        Nv3pCmdReadPartition *a = (Nv3pCmdReadPartition *)args;
        length = 2 * 8;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( h3p, tmp, length, 0, 0 )
        );

        READ64( tmp, a->Offset );
        READ64( tmp, a->Length );
        break;
    }
    case Nv3pCommand_ReadPartitionTable:
    {
        Nv3pCmdReadPartitionTable *a = (Nv3pCmdReadPartitionTable *)args;
        length = (1 * 8) + (2 * 4) + 1;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );

        READ64(tmp, a->Length);
        READ32(tmp, a->StartLogicalSector);
        READ32(tmp, a->NumLogicalSectors);
        READ8(tmp,  a->ReadBctFromSD);
        break;
    }
    case Nv3pCommand_RawDeviceRead:
    case Nv3pCommand_RawDeviceWrite:
    {
        Nv3pCmdRawDeviceAccess *a = (Nv3pCmdRawDeviceAccess *)args;
        length = (1 * 8);
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive( h3p, tmp, length, 0, 0 )
        );

        READ64( tmp, a->NoOfBytes);
        break;
    }
    case Nv3pCommand_OdmCommand:
    {
        Nv3pCmdOdmCommand *a = (Nv3pCmdOdmCommand *)args;
        switch(a->odmExtCmd)
        {
            case Nv3pOdmExtCmd_VerifySdram:
            case Nv3pOdmExtCmd_TegraTabFuseKeys:
            case Nv3pOdmExtCmd_TegraTabVerifyFuse:
            case Nv3pOdmExtCmd_UpgradeDeviceFirmware:
            {
                length = (1 * 4);
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pDataReceive(h3p, tmp, length, 0, 0)
                );
                READ32(tmp, a->Data);
                break;
            }
            case Nv3pOdmExtCmd_Get_Tnid:
            {
                length = (2 * 4);
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pDataReceive(h3p, tmp, length, 0, 0)
                );
                READ32(tmp, a->Data);
                READ32(tmp,a->odmExtCmdParam.tnid_info.length);
                break;
            }
            case Nv3pOdmExtCmd_Verify_Tid:
            {
                length = (2 * 4);
                NV_CHECK_ERROR_CLEANUP(
                    Nv3pDataReceive(h3p, tmp, length, 0, 0)
                );
                READ32(tmp, a->Data);
                READ32(tmp,a->odmExtCmdParam.tnid_info.length);
                break;
            }
            case Nv3pOdmExtCmd_LimitedPowerMode:
            default:
                break;
        }
        break;
    }
    case Nv3pCommand_GetDevInfo:
    {
        Nv3pCmdGetDevInfo *a = (Nv3pCmdGetDevInfo *)args;

        length = 3 * 4;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );

        READ32(tmp, a->BytesPerSector);
        READ32(tmp, a->SectorsPerBlock);
        READ32(tmp, a->TotalBlocks);
        break;
    }
    case Nv3pCommand_RunBDKTest:
    {
        Nv3pCmdRunBDKTest *a = (Nv3pCmdRunBDKTest *)args;

        length = 1 * 4;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );

        READ32(tmp, a->NumTests);
        break;
    }
    case Nv3pCommand_GetNv3pServerVersion:
    {
        Nv3pCmdGetNv3pServerVersion *a = (Nv3pCmdGetNv3pServerVersion *)args;

        length = NV3P_STRING_MAX;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );

        NvOsMemcpy(a->Version, tmp, NV3P_STRING_MAX);
        tmp += NV3P_STRING_MAX;
        break;
    }
    case Nv3pCommand_symkeygen:
    {
        Nv3pCmdSymKeyGen *a = (Nv3pCmdSymKeyGen *)args;

        length = 1 * 4;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );
        READ32(tmp, a->Length);
        break;
    }
    case Nv3pCommand_BDKGetSuiteInfo:
    {
        Nv3pCmdGetSuitesInfo *a = (Nv3pCmdGetSuitesInfo *)args;

        length = 1 * 4;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );

        READ32(tmp, a->size);
        break;
    }
    case Nv3pCommand_ReadNctItem:
    {
        Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)args;

        length = 1 * 4;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pDataReceive(h3p, tmp, length, 0, 0)
        );
        READ32(tmp, a->Type);
        break;
    }
    case Nv3pCommand_ReadBoardInfo:
    case Nv3pCommand_CreatePartition:
    case Nv3pCommand_Status:
    case Nv3pCommand_DownloadBct:
    case Nv3pCommand_SetBlHash:
    case Nv3pCommand_UpdateBct:
    case Nv3pCommand_DownloadBootloader:
    case Nv3pCommand_SetDevice:
    case Nv3pCommand_DownloadPartition:
    case Nv3pCommand_SetBootPartition:
    case Nv3pCommand_DeleteAll:
    case Nv3pCommand_FormatAll:
    case Nv3pCommand_Obliterate:
    case Nv3pCommand_OdmOptions:
    case Nv3pCommand_SetBootDevType:
    case Nv3pCommand_SetBootDevConfig:
    case Nv3pCommand_Go:
    case Nv3pCommand_Sync:
    case Nv3pCommand_StartPartitionConfiguration:
    case Nv3pCommand_EndPartitionConfiguration:
    case Nv3pCommand_FormatPartition:
    case Nv3pCommand_VerifyPartitionEnable:
    case Nv3pCommand_VerifyPartition:
    case Nv3pCommand_EndVerifyPartition:
    case Nv3pCommand_SetTime:
    case Nv3pCommand_NvPrivData:
    case Nv3pCommand_FuseWrite:
    case Nv3pCommand_DFuseBurn:
    case Nv3pCommand_SkipSync:
    case Nv3pCommand_Reset:
    case Nv3pCommand_Recovery:
    case Nv3pCommand_SetBDKTest:
    case Nv3pCommand_WriteNctItem:
#if ENABLE_NVDUMPER
    case Nv3pCommand_NvtbootRAMDump:
#endif
        break;
    default:
        NV_ASSERT( !"bad command" );
        return NV_FALSE;
    }

    return NV_TRUE;

fail:
    return NV_FALSE;
}

NvError
Nv3pCommandSend( Nv3pSocketHandle h3p, Nv3pCommand command, void *args,
    NvU32 flags )
{
    NvError e;
    NvU32 checksum;
    NvU32 length = 0;
    NvU8 *packet;
    NvU8 *tmp;
    NvBool b;
    packet = &h3p->s_buffer[0];

    Nv3pPrivWriteBasicHeader( h3p, Nv3pByteStream_PacketType_Command,
        h3p->sequence, packet );

    tmp = packet + NV3P_PACKET_SIZE_BASIC;
    Nv3pPrivWriteCmd( h3p, command, args, &length, tmp );

    length += NV3P_PACKET_SIZE_BASIC;
    length += NV3P_PACKET_SIZE_COMMAND;

    checksum = Nv3pPrivChecksum( packet, length );
    checksum = ~checksum + 1;
    tmp = packet + length;
    Nv3pPrivWriteFooter( h3p, checksum, tmp );

    length += NV3P_PACKET_SIZE_FOOTER;


    /* send the packet */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportSend( h3p->hTrans, packet, length, 0 )
    );

    h3p->sequence++;

    /* wait for ack/nack */
    e = Nv3pPrivWaitAck( h3p );
    if( e != NvSuccess )
    {
        goto fail;
    }

    /* some commands have return data */
    b = Nv3pPrivGetCmdReturn( h3p, command, args );
    if( !b )
    {
        e = NvError_Nv3pBadReturnData;
        goto fail;
    }

    return NvSuccess;

fail:
    return e;
}

NvError
Nv3pCommandComplete(
    Nv3pSocketHandle h3p,
    Nv3pCommand command,
    void *args,
    NvU32 flags )
{
    NvU32 length = 0;
    NvU8 *packet;
    NvU8 *tmp;

    packet = &h3p->s_retbuf[0];
    tmp = packet;

    Nv3pAck( h3p );

    switch( command ) {
    case Nv3pCommand_GetPlatformInfo:
    {
        Nv3pCmdGetPlatformInfo *a = (Nv3pCmdGetPlatformInfo *)args;
        length = (18 * 4) + 1;
        WRITE32( tmp, a->ChipUid.ecid_0);
        WRITE32( tmp, a->ChipUid.ecid_1);
        WRITE32( tmp, a->ChipUid.ecid_2);
        WRITE32( tmp, a->ChipUid.ecid_3);
        WRITE32( tmp, *(NvU32 *)&a->ChipId );
        WRITE32( tmp, a->ChipSku );
        WRITE32( tmp, a->BootRomVersion );
        WRITE32( tmp, a->SecondaryBootDevice );
        WRITE32( tmp, a->OperatingMode );
        WRITE32( tmp, a->DeviceConfigStrap );
        WRITE32( tmp, a->DeviceConfigFuse );
        WRITE32( tmp, a->SdramConfigStrap );
        WRITE32( tmp, a->DkBurned );
        WRITE8( tmp, a->HdmiEnable );
        WRITE8( tmp, a->MacrovisionEnable );
        WRITE8( tmp, a->SbkBurned );
        WRITE8( tmp, a->JtagEnable );
        WRITE32(tmp, a->BoardId.BoardNo);
        WRITE32(tmp, a->BoardId.BoardFab);
        WRITE32(tmp, a->BoardId.MemType);
        WRITE32(tmp, a->WarrantyFuse);
        WRITE8(tmp, a->SkipAutoDetect);
        break;
    }
    case Nv3pCommand_GetBoardDetails:
    {
        NvU32 Val = *(NvU32 *)args;

        length = (1 * 4);
        WRITE32(tmp, Val);

        break;
    }
    case Nv3pCommand_GetBct:
    {
        Nv3pCmdGetBct *a = (Nv3pCmdGetBct *)args;
        length = 1 * 4;
        WRITE32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_GetBit:
    {
        Nv3pCmdGetBit *a = (Nv3pCmdGetBit *)args;
        length = 1 * 4;
        WRITE32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_ReadPartitionTable:
    {
        Nv3pCmdReadPartitionTable *a = (Nv3pCmdReadPartitionTable *)args;
        length = (1 * 8) + (2 * 4) + 1;
        WRITE64(tmp, a->Length);
        WRITE32(tmp, a->NumLogicalSectors);
        WRITE32(tmp, a->StartLogicalSector);
        WRITE8(tmp,  a->ReadBctFromSD);
        break;
    }
    case Nv3pCommand_QueryPartition:
    {
        Nv3pCmdQueryPartition *a = (Nv3pCmdQueryPartition *)args;
        length = 3 * 8;
        WRITE64( tmp, a->Size );
        WRITE64( tmp, a->Address );
        WRITE64( tmp, a->PartType );
        break;
    }
    case Nv3pCommand_ReadPartition:
    {
        Nv3pCmdReadPartition *a = (Nv3pCmdReadPartition *)args;
        length = 2 * 8;
        WRITE64( tmp, a->Offset );
        WRITE64( tmp, a->Length );
        break;
    }
    case Nv3pCommand_RawDeviceRead:
    case Nv3pCommand_RawDeviceWrite:
    {
        Nv3pCmdRawDeviceAccess *a = (Nv3pCmdRawDeviceAccess *)args;
        length = 1 * 8;
        WRITE64( tmp, a->NoOfBytes);
        break;
    }
    case Nv3pCommand_OdmCommand:
    {
        Nv3pCmdOdmCommand *a = (Nv3pCmdOdmCommand *)args;
        switch(a->odmExtCmd)
        {
            case Nv3pOdmExtCmd_VerifySdram:
            case Nv3pOdmExtCmd_TegraTabFuseKeys:
            case Nv3pOdmExtCmd_TegraTabVerifyFuse:
            case Nv3pOdmExtCmd_UpgradeDeviceFirmware:
            {
                length = 1 * 4;
                WRITE32(tmp, a->Data);
                break;
            }
            case Nv3pOdmExtCmd_Get_Tnid:
            {
                length = 2 * 4;
                WRITE32(tmp, a->Data);
                WRITE32(tmp, a->odmExtCmdParam.tnid_info.length);
                break;
            }
            case Nv3pOdmExtCmd_Verify_Tid:
            {
                length = 2 * 4;
                WRITE32(tmp, a->Data);
                WRITE32(tmp, a->odmExtCmdParam.tnid_info.length);
                break;
            }
            case Nv3pOdmExtCmd_UpdateuCFw:
            case Nv3pOdmExtCmd_LimitedPowerMode:
                return NvSuccess;
            default:
                break;
        }
        break;
    }
    case Nv3pCommand_GetDevInfo:
    {
        Nv3pCmdGetDevInfo *a = (Nv3pCmdGetDevInfo *)args;
        length = 3 * 4;
        WRITE32(tmp, a->BytesPerSector);
        WRITE32(tmp, a->SectorsPerBlock);
        WRITE32(tmp, a->TotalBlocks);
        break;
    }
    case Nv3pCommand_RunBDKTest:
    {
        Nv3pCmdRunBDKTest *a = (Nv3pCmdRunBDKTest *)args;
        length = 1 * 4;
        WRITE32(tmp, a->NumTests);
        break;
    }
    case Nv3pCommand_GetNv3pServerVersion:
    {
        Nv3pCmdGetNv3pServerVersion *a = (Nv3pCmdGetNv3pServerVersion *)args;
        NvU32 len = 0;

        length = NV3P_STRING_MAX;
        NvOsMemset(tmp, 0, NV3P_STRING_MAX);
        len = NV_MIN(NvOsStrlen(a->Version), NV3P_STRING_MAX - 1);
        NvOsMemcpy(tmp, a->Version, len);
        tmp += NV3P_STRING_MAX;
        break;
    }
    case Nv3pCommand_symkeygen:
    {
        Nv3pCmdSymKeyGen *a = (Nv3pCmdSymKeyGen *)args;
        length = 1 * 4;
        WRITE32(tmp, a->Length);
        break;
    }
    case Nv3pCommand_BDKGetSuiteInfo:
    {
        Nv3pCmdGetSuitesInfo *a = (Nv3pCmdGetSuitesInfo *)args;
        length = 1 * 4;
        WRITE32(tmp, a->size);
        break;
    }
    case Nv3pCommand_ReadNctItem:
    {
        Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)args;
        length = 1 * 4;
        WRITE32(tmp, a->Type);
        break;
    }
    case Nv3pCommand_ReadBoardInfo:
    case Nv3pCommand_CreatePartition:
    case Nv3pCommand_Status:
    case Nv3pCommand_DownloadBct:
    case Nv3pCommand_SetBlHash:
    case Nv3pCommand_UpdateBct:
    case Nv3pCommand_DownloadBootloader:
    case Nv3pCommand_SetDevice:
    case Nv3pCommand_DownloadPartition:
    case Nv3pCommand_SetBootPartition:
    case Nv3pCommand_DeleteAll:
    case Nv3pCommand_FormatAll:
    case Nv3pCommand_Obliterate:
    case Nv3pCommand_OdmOptions:
    case Nv3pCommand_SetBootDevType:
    case Nv3pCommand_SetBootDevConfig:
    case Nv3pCommand_Go:
    case Nv3pCommand_Sync:
    case Nv3pCommand_StartPartitionConfiguration:
    case Nv3pCommand_EndPartitionConfiguration:
    case Nv3pCommand_FormatPartition:
    case Nv3pCommand_VerifyPartitionEnable:
    case Nv3pCommand_VerifyPartition:
    case Nv3pCommand_EndVerifyPartition:
    case Nv3pCommand_SetTime:
    case Nv3pCommand_FuseWrite:
    case Nv3pCommand_DFuseBurn:
    case Nv3pCommand_SkipSync:
    case Nv3pCommand_Reset:
    case Nv3pCommand_Recovery:
    case Nv3pCommand_SetBDKTest:
    case Nv3pCommand_WriteNctItem:
#if ENABLE_NVDUMPER
    case Nv3pCommand_NvtbootRAMDump:
#endif
    default:
        return NvSuccess;
    }

    return Nv3pDataSend( h3p, packet, length, 0 );
}

static NvBool
Nv3pPrivGetCmdArgs( Nv3pSocketHandle h3p, Nv3pCommand command, void **args,
    NvU8 *packet )
{
    NvU8 *tmp = packet;
    NvU8 *buf = &h3p->s_args[0];

    switch( command ) {
    case Nv3pCommand_DeleteAll:
    case Nv3pCommand_FormatAll:
    case Nv3pCommand_Obliterate:
    case Nv3pCommand_Go:
    case Nv3pCommand_Sync:
    case Nv3pCommand_EndPartitionConfiguration:
    case Nv3pCommand_SkipSync:
    case Nv3pCommand_Recovery:
    case Nv3pCommand_ReadBoardInfo:
        return NV_FALSE;
    case Nv3pCommand_GetBct:
    case Nv3pCommand_GetBit:
    case Nv3pCommand_EndVerifyPartition:
    case Nv3pCommand_GetDevInfo:
    case Nv3pCommand_GetNv3pServerVersion:
    case Nv3pCommand_BDKGetSuiteInfo:
    case Nv3pCommand_GetBoardDetails:
        /* output only */
        break;
    case Nv3pCommand_Status:
    {
        Nv3pCmdStatus *a = (Nv3pCmdStatus *)buf;
        NvOsMemcpy( a->Message, tmp, NV3P_STRING_MAX );
        tmp += NV3P_STRING_MAX;

        READ32_PUNNED( tmp, Nv3pStatus, a->Code );
        READ32( tmp, a->Flags );
        break;
    }
    case Nv3pCommand_DownloadBct:
    {
        Nv3pCmdDownloadBct *a = (Nv3pCmdDownloadBct *)buf;
        READ32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_SetBlHash:
    {
        Nv3pCmdBlHash *a = (Nv3pCmdBlHash *)buf;
        READ32(tmp, a->BlIndex);
        READ32(tmp, a->Length);
        break;
    }
    case Nv3pCommand_UpdateBct:
    {
        Nv3pCmdUpdateBct *a = (Nv3pCmdUpdateBct *)buf;
        READ32(tmp, a->Length);
        READ32(tmp, a->BctSection);
        READ32(tmp, a->PartitionId);
        break;
    }
    case Nv3pCommand_FuseWrite:
    {
        Nv3pCmdFuseWrite*a = (Nv3pCmdFuseWrite *)buf;
        READ32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_FormatPartition:
    {
        Nv3pCmdFormatPartition *a = (Nv3pCmdFormatPartition *)buf;
        READ32( tmp, a->PartitionId );
        break;
    }
    case Nv3pCommand_DownloadBootloader:
    {
        Nv3pCmdDownloadBootloader *a = (Nv3pCmdDownloadBootloader *)buf;
        READ64( tmp, a->Length );
        READ32( tmp, a->Address );
        READ32( tmp, a->EntryPoint );
        break;
    }
    case Nv3pCommand_SetDevice:
    {
        Nv3pCmdSetDevice *a = (Nv3pCmdSetDevice *)buf;
        READ32_PUNNED( tmp, Nv3pDeviceType, a->Type );
        READ32( tmp, a->Instance );
        break;
    }
    case Nv3pCommand_DownloadPartition:
    {
        Nv3pCmdDownloadPartition *a = (Nv3pCmdDownloadPartition *)buf;
        READ32( tmp, a->Id );
        READ64( tmp, a->Length );
        break;
    }
    case Nv3pCommand_QueryPartition:
    {
        Nv3pCmdQueryPartition *a = (Nv3pCmdQueryPartition *)buf;
        READ32( tmp, a->Id );
        break;
    }
    case Nv3pCommand_StartPartitionConfiguration:
    {
        Nv3pCmdStartPartitionConfiguration *a =
            (Nv3pCmdStartPartitionConfiguration *)buf;
        READ32( tmp, a->nPartitions );
        break;
    }
    case Nv3pCommand_CreatePartition:
    {
        Nv3pCmdCreatePartition *a = (Nv3pCmdCreatePartition *)buf;

        NvOsMemcpy( a->Name, tmp, NV3P_STRING_MAX );
        tmp += NV3P_STRING_MAX;

        READ64( tmp, a->Size );
        READ64( tmp, a->Address );
        READ32( tmp, a->Id );
        READ32_PUNNED( tmp, Nv3pPartitionType, a->Type );
        READ32_PUNNED( tmp, Nv3pFileSystemType, a->FileSystem );
        READ32_PUNNED( tmp, Nv3pPartitionAllocationPolicy, a->AllocationPolicy );
        READ32( tmp, *(NvU32 *)&a->FileSystemAttribute );
        READ32( tmp, *(NvU32 *)&a->PartitionAttribute );
        READ32( tmp, *(NvU32 *)&a->AllocationAttribute );
        READ32( tmp, a->PercentReserved );
#ifdef NV_EMBEDDED_BUILD
        READ32( tmp, a->IsWriteProtected );
#endif
        break;
    }
    case Nv3pCommand_ReadPartition:
    {
        Nv3pCmdReadPartition *a = (Nv3pCmdReadPartition *)buf;
        READ32( tmp, a->Id );
        READ64( tmp, a->Offset );
        READ64( tmp, a->Length );
        break;
    }
    case Nv3pCommand_ReadPartitionTable:
    {
        Nv3pCmdReadPartitionTable *a = (Nv3pCmdReadPartitionTable *)buf;
        READ64(tmp, a->Length);
        READ32(tmp, a->NumLogicalSectors);
        READ32(tmp, a->StartLogicalSector);
        READ8(tmp,  a->ReadBctFromSD);
        break;
    }
    case Nv3pCommand_RawDeviceRead:
    case Nv3pCommand_RawDeviceWrite:
    {
        Nv3pCmdRawDeviceAccess *a = (Nv3pCmdRawDeviceAccess *)buf;
        READ32( tmp, a->StartSector);
        READ32( tmp, a->NoOfSectors);
        break;
    }
    case Nv3pCommand_SetBootPartition:
    {
        Nv3pCmdSetBootPartition *a = (Nv3pCmdSetBootPartition *)buf;
        READ32( tmp, a->Id );
        READ32( tmp, a->LoadAddress );
        READ32( tmp, a->EntryPoint );
        READ32( tmp, a->Version );
        READ32( tmp, a->Slot );
        break;
    }
    case Nv3pCommand_OdmOptions:
    {
        Nv3pCmdOdmOptions *a = (Nv3pCmdOdmOptions *)buf;
        READ32( tmp, a->Options );
        break;
    }
    case Nv3pCommand_SetBootDevType:
    {
        Nv3pCmdSetBootDevType *a = (Nv3pCmdSetBootDevType *)buf;
        READ32_PUNNED(tmp, Nv3pDeviceType, a->DevType);
        break;
    }
    case Nv3pCommand_SetBootDevConfig:
    {
        Nv3pCmdSetBootDevConfig *a = (Nv3pCmdSetBootDevConfig *)buf;
        READ32(tmp, a->DevConfig);
        break;
    }
    case Nv3pCommand_OdmCommand:
    {
        Nv3pCmdOdmCommand *a = (Nv3pCmdOdmCommand *)buf;
        READ32(tmp, a->odmExtCmd);
        switch ( a->odmExtCmd ) {
        case Nv3pOdmExtCmd_FuelGaugeFwUpgrade:
        {
            READ64(tmp, a->odmExtCmdParam.fuelGaugeFwUpgrade.FileLength1);
            READ64(tmp, a->odmExtCmdParam.fuelGaugeFwUpgrade.FileLength2);
            break;
        }
        case Nv3pOdmExtCmd_UpdateuCFw:
        {
            READ32(tmp, a->odmExtCmdParam.updateuCFw.FileSize);
            break;
        }
        case Nv3pOdmExtCmd_Get_Tnid:
            break;
        case Nv3pOdmExtCmd_VerifySdram:
        {
            READ32(tmp, a->odmExtCmdParam.verifySdram.Value);
            break;
        }
        case Nv3pOdmExtCmd_Verify_Tid:
        {
            READ32(tmp, a->odmExtCmdParam.tnid_info.length);
            break;
        }
        case Nv3pOdmExtCmd_LimitedPowerMode:
            break;
        case Nv3pOdmExtCmd_TegraTabFuseKeys:
        case Nv3pOdmExtCmd_TegraTabVerifyFuse:
        {
            NvU64 FileLength;
            READ64(tmp, FileLength);
            a->odmExtCmdParam.tegraTabFuseKeys.FileLength = (NvU32)FileLength;
            break;
        }
        case Nv3pOdmExtCmd_UpgradeDeviceFirmware:
        {
            READ64(tmp, a->odmExtCmdParam.upgradeDeviceFirmware.FileLength);
            break;
        }
        default:
            NV_ASSERT( !"bad command" );
        }
        break;
    }
    case Nv3pCommand_VerifyPartitionEnable:
    case Nv3pCommand_VerifyPartition:
    {
        Nv3pCmdVerifyPartition *a = (Nv3pCmdVerifyPartition *)buf;
        READ32( tmp, a->Id);
        break;
    }
    case Nv3pCommand_SetTime:
    {
        Nv3pCmdSetTime *a = (Nv3pCmdSetTime *)buf;
        READ32( tmp, a->Seconds);
        READ32( tmp, a->Milliseconds);
        break;
    }
    case Nv3pCommand_NvPrivData:
    {
        Nv3pCmdNvPrivData *a = (Nv3pCmdNvPrivData *)buf;
        READ32( tmp, a->Length );
        break;
    }
    case Nv3pCommand_RunBDKTest:
    {
        Nv3pCmdRunBDKTest *a = (Nv3pCmdRunBDKTest *)buf;
        NvOsMemcpy(a->Suite, tmp, NV3P_STRING_MAX);
        tmp += NV3P_STRING_MAX;
        NvOsMemcpy(a->Argument, tmp, NV3P_STRING_MAX);
        tmp += NV3P_STRING_MAX;
        READ32(tmp, a->PrivData.Instance);
        READ32(tmp, a->PrivData.Mem_address);
        READ32(tmp, a->PrivData.Reserved);
        READ32(tmp, a->PrivData.File_size);
        break;
    }
    case Nv3pCommand_SetBDKTest:
    {
        Nv3pCmdSetBDKTest *a = (Nv3pCmdSetBDKTest *)buf;
        READ8(tmp, a->StopOnFail);
        READ32(tmp, a->Timeout);
        break;
    }
    case Nv3pCommand_symkeygen:
    {
        Nv3pCmdSymKeyGen*a = (Nv3pCmdSymKeyGen *)buf;
        READ32(tmp, a->Length);
        break;
    }
    case Nv3pCommand_DFuseBurn:
    {
        Nv3pCmdDFuseBurn *a = (Nv3pCmdDFuseBurn *)buf;
        READ32(tmp, a->bct_length);
        READ32(tmp, a->fuse_length);
        break;
    }
#if ENABLE_NVDUMPER
    case Nv3pCommand_NvtbootRAMDump:
    {
        Nv3pCmdNvtbootRAMDump *a = (Nv3pCmdNvtbootRAMDump *)buf;
        READ32( tmp, a->Offset);
        READ32( tmp, a->Length);
        break;
    }
#endif
    case Nv3pCommand_GetPlatformInfo:
    {
        Nv3pCmdGetPlatformInfo *a = (Nv3pCmdGetPlatformInfo *)buf;
        READ8(tmp, a->SkipAutoDetect);
        break;
    }
    case Nv3pCommand_Reset:
    {
        Nv3pCmdReset *a = (Nv3pCmdReset *)buf;
        READ32(tmp, a->Reset);
        READ32(tmp, a->DelayMs);
        break;
    }
    case Nv3pCommand_ReadNctItem:
    {
        Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)buf;
        READ32(tmp, a->EntryIdx);
        break;
    }
    case Nv3pCommand_WriteNctItem:
    {
        Nv3pCmdReadWriteNctItem *a = (Nv3pCmdReadWriteNctItem *)buf;
        READ32(tmp, a->EntryIdx);
        NvOsMemcpy(&a->Data, tmp, sizeof(a->Data));
        break;
    }
    default:
        return NV_FALSE;
    }

    *args = buf;
    return NV_TRUE;
}

NvError
Nv3pCommandReceive(
    Nv3pSocketHandle h3p,
    Nv3pCommand *command,
    void **args,
    NvU32 flags )
{
    NvError e;
    NvU8 *tmp;
    Nv3pHdrBasic hdr = {0,0,0};
    NvU32 length;
    NvU32 checksum, recv_checksum = 0;
    NvBool b;

    /* get the basic header, verify it's a command */
    b = Nv3pPrivReceiveBasicHeader( h3p, &hdr, &recv_checksum );
    if( !b )
    {
        e = NvError_Nv3pUnrecoverableProtocol;
        goto fail;
    }

    if( hdr.PacketType != Nv3pByteStream_PacketType_Command )
    {
        return Nv3pPrivDrainPacket( h3p, &hdr );
    }

    tmp = &h3p->s_buffer[0];

    /* get length and command number */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReceive( h3p->hTrans, tmp, (2 * 4), 0, 0 )
    );

    READ32( tmp, length );
    READ32( tmp, *(NvU32 *)command );

    /* read the args */
    if( length )
    {
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans, tmp, length, 0, 0 )
        );

        b = Nv3pPrivGetCmdArgs( h3p, *command, args, tmp );
        if( !b )
        {
            e = NvError_Nv3pUnrecoverableProtocol;
            goto fail;
        }
    }
    else
    {
        /* command may be output only */
        b = Nv3pPrivGetCmdArgs( h3p, *command, args, 0 );
        if( !b )
        {
            *args = 0;
        }
    }

    length += NV3P_PACKET_SIZE_COMMAND;
    recv_checksum += Nv3pPrivChecksum(&h3p->s_buffer[0], length);

    /* get/verify the checksum */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReceive( h3p->hTrans, (NvU8 *)&checksum, 4, 0, 0 )
    );

    if( recv_checksum + checksum != 0 )
    {
        e = NvError_Nv3pUnrecoverableProtocol;
        goto fail;
    }

    return NvSuccess;

fail:
    return e;
}

NvError
Nv3pDataSend(
    Nv3pSocketHandle h3p,
    NvU8 *data,
    NvU32 length,
    NvU32 flags )
{
    NvError e;
    NvU32 checksum;
    NvU8 *packet;
    NvU8 *tmp;
    NvU32 hdrlen;

    packet = &h3p->s_buffer[0];

    Nv3pPrivWriteBasicHeader( h3p, Nv3pByteStream_PacketType_Data,
        h3p->sequence, packet );
    tmp = packet + NV3P_PACKET_SIZE_BASIC;

    /* data header */
    WRITE32( tmp, length );

    hdrlen = NV3P_PACKET_SIZE_BASIC + NV3P_PACKET_SIZE_DATA;

    /* checksum */
    checksum = Nv3pPrivChecksum( packet, hdrlen );
    checksum += Nv3pPrivChecksum( data, length );
    checksum = ~checksum + 1;

    /* send the headers */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportSend( h3p->hTrans, packet, hdrlen, 0 )
    );

    /* send the data */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportSend( h3p->hTrans, data, length, 0 )
    );

    /* send checksum */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportSend( h3p->hTrans, (NvU8 *)&checksum, 4, 0 )
    );

    h3p->sequence++;

    /* wait for ack/nack */
    e = Nv3pPrivWaitAck( h3p );
    if( e != NvSuccess )
    {
        goto fail;
    }

    return NvSuccess;

fail:
    return e;
}

NvError
Nv3pDataReceive(
    Nv3pSocketHandle h3p,
    NvU8 *data,
    NvU32 length,
    NvU32 *bytes,
    NvU32 flags )
{
    NvError e;
    NvU8 *tmp;
    Nv3pHdrBasic hdr = {0,0,0};
    NvU32 checksum;
    NvU32 recv_length;
    NvBool b;

    /* check for left over stuff from a previous read */
    if( h3p->bytes_remaining == 0 )
    {
        /* get the basic header, verify it's data */
        b = Nv3pPrivReceiveBasicHeader( h3p, &hdr, &h3p->recv_checksum );
        if( !b )
        {
            e = NvError_Nv3pUnrecoverableProtocol;
            goto fail;
        }

        if( hdr.PacketType != Nv3pByteStream_PacketType_Data )
        {
            return Nv3pPrivDrainPacket( h3p, &hdr );
        }

        tmp = &h3p->s_buffer[0];

        /* get length */
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans, tmp, (1 * 4), 0, 0 )
        );

        READ32( tmp, recv_length );

        if( !recv_length )
        {
            e = NvError_Nv3pBadReceiveLength;
            goto fail;
        }

        h3p->recv_checksum += Nv3pPrivChecksum( (NvU8 *)&recv_length, 4 );

        /* setup for partial reads */
        h3p->bytes_remaining = recv_length;
        length = NV_MIN( length, recv_length );
    }
    else
    {
        length = NV_MIN( h3p->bytes_remaining, length );
    }

    /* read the data */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportReceive( h3p->hTrans, data, length, 0, 0 )
    );

    if( bytes )
    {
        *bytes = length;
    }

    h3p->recv_checksum += Nv3pPrivChecksum( data, length );

    h3p->bytes_remaining -= length;
    if( h3p->bytes_remaining == 0 )
    {
        /* get/verify the checksum */
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans, (NvU8 *)&checksum, 4, 0, 0 )
        );

        if( h3p->recv_checksum + checksum != 0 )
        {
            e = NvError_Nv3pUnrecoverableProtocol;
            goto fail;
        }

        Nv3pAck( h3p );
    }

    return NvSuccess;

fail:
    Nv3pNack( h3p, Nv3pNackCode_BadData );
    return e;
}

void
Nv3pTransferFail( Nv3pSocketHandle h3p, Nv3pNackCode code )
{
    NvError e;
    NvU8 *tmp;
    NvU32 length;
    NvBool bReceive = NV_FALSE;

    tmp = &h3p->s_buffer[0];

    if( h3p->bytes_remaining )
    {
        bReceive = NV_TRUE;
    }

    /* drain the bytes, throw them away */
    while( h3p->bytes_remaining )
    {
        length = NV_MIN( h3p->bytes_remaining, NV3P_MAX_COMMAND_SIZE );
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans, tmp, length, 0, 0 )
        );

        h3p->bytes_remaining -= length;
    }

    if( bReceive )
    {
        /* drain the checksum */
        NV_CHECK_ERROR_CLEANUP(
            Nv3pTransportReceive( h3p->hTrans, tmp, 4, 0, 0 )
        );
    }

    /* nack the sender */
    Nv3pNack( h3p, code );

fail:
    NV_ASSERT( !"Nv3pTransferFail failed..." );
}

void
Nv3pAck( Nv3pSocketHandle h3p )
{
    NvError e;
    NvU32 checksum;
    NvU8 *packet;
    NvU8 *tmp;
    NvU32 length;

    packet = &h3p->s_ack[0];

    Nv3pPrivWriteBasicHeader( h3p, Nv3pByteStream_PacketType_Ack,
        h3p->recv_sequence, packet );

    length = NV3P_PACKET_SIZE_BASIC;
    length += NV3P_PACKET_SIZE_ACK;

    checksum = Nv3pPrivChecksum( packet, length );
    checksum = ~checksum + 1;
    tmp = packet + length;
    Nv3pPrivWriteFooter( h3p, checksum, tmp );

    length += NV3P_PACKET_SIZE_FOOTER;

    /* send the packet */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportSend( h3p->hTrans, packet, length, 0 )
    );

fail:
    return;
}

void
Nv3pNack(
    Nv3pSocketHandle h3p,
    Nv3pNackCode code )
{
    NvError e;
    NvU32 checksum;
    NvU8 *packet;
    NvU8 *tmp;
    NvU32 length;

    packet = &h3p->s_ack[0];

    Nv3pPrivWriteBasicHeader( h3p, Nv3pByteStream_PacketType_Nack,
        h3p->recv_sequence, packet );

    length = NV3P_PACKET_SIZE_BASIC;

    tmp = packet + length;

    /* write the nack code */
    WRITE32( tmp, code );

    length += NV3P_PACKET_SIZE_NACK;
    checksum = Nv3pPrivChecksum( packet, length );
    checksum = ~checksum + 1;
    Nv3pPrivWriteFooter( h3p, checksum, tmp );

    length += NV3P_PACKET_SIZE_FOOTER;

    /* send the packet */
    NV_CHECK_ERROR_CLEANUP(
        Nv3pTransportSend( h3p->hTrans, packet, length, 0 )
    );

fail:
    return;
}

Nv3pNackCode
Nv3pGetLastNackCode( Nv3pSocketHandle h3p )
{
    return h3p->last_nack;
}

