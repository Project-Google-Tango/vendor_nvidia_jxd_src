/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 
#include "nvos.h"
#include "nvassert.h"
#include "nv3p_transport.h"
#include <stdio.h>
#include <string.h>



#ifdef ENABLE_DEBUG_PRINTS
#define DEBUG_PRINT(x) NvAuPrintf x
#else
#define DEBUG_PRINT(x)
#endif

#define MAX_BUFF_SIZE 50

typedef struct Nv3pTransportRec
{
    FILE* ReqFileHandle;
    FILE* RespFileHandle;
    NvOsStatType     RespStat;
    char ReqDone[MAX_BUFF_SIZE];
    char ReqReady[MAX_BUFF_SIZE];
    char ResDone[MAX_BUFF_SIZE];
    char ResReady[MAX_BUFF_SIZE];
    NvBool ReqSent;
} Nv3pTransport;

static Nv3pTransport s_JtagTransport;

static NvBool IsJtagAlive(void)
{
    char TempData[20];
    FILE* ReqFileHandle = 0;
    memset(TempData,0,20);
    strcpy(TempData,"Debugger Connected");
    ReqFileHandle = fopen(NV3P_REQUEST_FILE, "wb+");
    fwrite( TempData, 1, 20, ReqFileHandle);
    fclose(ReqFileHandle);
    memset(TempData,0,20);
    ReqFileHandle = fopen(NV3P_REQUEST_FILE, "rb");
    fread(TempData, 1, 20, ReqFileHandle);
    fclose(ReqFileHandle);
    remove(NV3P_REQUEST_FILE);
    if(strcmp(TempData,"Debugger Connected") == 0)
        return NV_TRUE;
    return NV_FALSE;
}

NvError
Nv3pTransportCloseJtag( Nv3pTransportHandle hTrans )
{
    remove(NV3P_REQUEST_FILE);
    remove(NV3P_RESPONSE_FILE);
    remove(hTrans->ReqReady);
    remove(hTrans->ReqDone);
    remove(hTrans->ResReady);
    remove(hTrans->ResDone);
    return NvSuccess;
}

NvError
Nv3pTransportReopenJtag( Nv3pTransportHandle *hTrans )
{
    Nv3pTransport *trans;
    trans = &s_JtagTransport;
    trans->ReqFileHandle = 0;
    trans->RespFileHandle = 0;
    trans->ReqSent = NV_FALSE;

    memset(trans->ReqReady,0,MAX_BUFF_SIZE);
    strcpy(trans->ReqReady,NV3P_REQUEST_FILE);
    strcat(trans->ReqReady,".ready");
    memset(trans->ReqDone,0,MAX_BUFF_SIZE);
    strcpy(trans->ReqDone,NV3P_REQUEST_FILE);
    strcat(trans->ReqDone,".done");

    memset(trans->ResReady,0,MAX_BUFF_SIZE);
    strcpy(trans->ResReady,NV3P_RESPONSE_FILE);
    strcat(trans->ResReady,".ready");
    memset(trans->ResDone,0,MAX_BUFF_SIZE);
    strcpy(trans->ResDone,NV3P_RESPONSE_FILE);
    strcat(trans->ResDone,".done");

    remove(NV3P_REQUEST_FILE);
    remove(NV3P_RESPONSE_FILE);
    remove(trans->ReqReady);
    remove(trans->ReqDone);
    remove(trans->ResReady);
    remove(trans->ResDone);

    while( !IsJtagAlive() );

    *hTrans = trans;
    return NvSuccess;

}

NvError
Nv3pTransportOpenJtag( Nv3pTransportHandle *hTrans )
{
    return Nv3pTransportReopenJtag(hTrans);
}

NvError
Nv3pTransportSendJtag( 
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags )
{

    FILE* HandleReqDone = 0;
    FILE* HandleReqReady = 0;
    hTrans->ReqFileHandle = 0;
    DEBUG_PRINT(("JtagSend: waiting for send \r\n"));
    if(hTrans->ReqSent)
    {
        //check if processing of previous request is done or not
        do{
            HandleReqDone = fopen(hTrans->ReqDone, "rb");
        }while( !HandleReqDone );
        fclose(HandleReqDone);
        while(remove(hTrans->ReqDone) != 0);
    }

    //create new request
    do{
            hTrans->ReqFileHandle = fopen(NV3P_REQUEST_FILE, "wb+");
    }while( !hTrans->ReqFileHandle );
    fwrite(data , 1, length, hTrans->ReqFileHandle);
    DEBUG_PRINT(("JtagSend: send %d bytes length\r\n",length));
    fclose(hTrans->ReqFileHandle);

    //Create request ready notification
    HandleReqReady = fopen(hTrans->ReqReady, "wb");
    fclose(HandleReqReady);
    hTrans->ReqSent = NV_TRUE;
    return NvSuccess;

}

NvError
Nv3pTransportReceiveJtag(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags )
{
    NvError e;
    FILE* HandleRespDone = 0;
    FILE* HandleRespReady = 0;
    DEBUG_PRINT(("JtagReceive: wait for response\r\n"));

    if(!hTrans->RespFileHandle)
    {
        do{
            HandleRespReady = fopen(hTrans->ResReady, "rb");
        }while( !HandleRespReady );
        fclose(HandleRespReady);
        while(remove(hTrans->ResReady) != 0);

        do{
                hTrans->RespFileHandle = fopen(NV3P_RESPONSE_FILE, "rb");
                if(hTrans->RespFileHandle)
                {
                    fseek (hTrans->RespFileHandle, 0, SEEK_END);
                    hTrans->RespStat.size = ftell (hTrans->RespFileHandle);
                    fseek (hTrans->RespFileHandle, 0, SEEK_SET);
                    if(hTrans->RespStat.size >= length)
                        break;
                    fclose(hTrans->RespFileHandle);
                }
        }while(1);
        DEBUG_PRINT(("JtagReceive: FoundFile\r\n"));
    }

    if(length > hTrans->RespStat.size)
        NV_CHECK_ERROR_CLEANUP( NvError_InvalidSize );

        DEBUG_PRINT(("JtagReceive: reading %d bytes length\r\n",length));
        fread(data , 1, length, hTrans->RespFileHandle);
    hTrans->RespStat.size -= length;

    if(hTrans->RespStat.size == 0)
    {
        fclose(hTrans->RespFileHandle);
        while(remove(NV3P_RESPONSE_FILE) != 0);
        hTrans->RespFileHandle = 0;
        //notify that response has been read and we are ready for next request
        HandleRespDone = fopen(hTrans->ResDone, "wb");
        fclose(HandleRespDone);
        DEBUG_PRINT(("JtagReceive: File deleted\r\n"));
    }

    if(received)
        *received = length;

    return NvSuccess;

fail:
    return e;
}

