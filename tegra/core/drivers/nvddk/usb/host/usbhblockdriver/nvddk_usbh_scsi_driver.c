/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_usbh_scsi_driver.h"

#define NvError_Failure (NvError)2;

#if 0
#define SCSI_DEBUG_PRINTF(x) NvOsDebugPrintf x
#else
#define SCSI_DEBUG_PRINTF(x) {}
#endif

#define SINGLE_SECTOR 1
#define SECTOR_RANGE 128
#define DEV_DESC_LEN 8
#define CNF_DESC_LEN 9
#define INF_DESC_LEN 9
#define END_DESC_LEN 7
#define STR_SHORT_LEN 2
#define STR_DESC_LEN 4
#define MAX_DESC_LEN 0xFF
#define EP_ADDR_MASK 0x0F
#define CBW_SIGNATURE 0x43425355

void
SetCommandBlockWrapper(
        NvU32 transferLen,
        DeviceInfo *pdevInfo,
        NvU32 flags,
        NvU8 lun,
        NvU8 commandLen,
        NvU8 *commandBlock)
{
    Usb_Msd_Cbw gcbw = {0};

    if ((commandLen < 1) || (commandLen > 16))
    {
        SCSI_DEBUG_PRINTF(("setCBW: invalid length\r\n"));
        return;
    }

    //update cbw signature tag..
    pdevInfo->CBW_tag++;
    gcbw.Signature = CBW_SIGNATURE;
    gcbw.Tag = pdevInfo->CBW_tag;
    gcbw.DataTransferLength = transferLen;
    gcbw.Flags = flags;
    gcbw.Lun = lun;
    gcbw.Length = commandLen;
    NvOsMemcpy((void *)gcbw.Cmd, commandBlock, commandLen);
    NvOsMemcpy(
        (void *)&(pdevInfo->Cbw), (const void *)(&gcbw), sizeof(Usb_Msd_Cbw));
}

NvError BulkOnlyResetCommand(NvDdkUsbhHandle hUsbh, DeviceInfo *pdevInfo)
{
    DeviceRequestTypes descType;
    NvU32 transferLen = 0;
    NvU16 Value, Index;
    NvU8 * pbuffer = NULL;

    // Class specific request, max luns supported request for BOT , len 0
    descType    = (DeviceRequestTypes)ClassSpecificRequest_Bulk_Reset;
    transferLen     = 0;
    Value       = 0;
    Index       = 0;
    return GetDescriptor(hUsbh,
            pdevInfo->CntrlEpHndle,
            descType,
            pbuffer,
            Value,
            Index,
            &transferLen);
}

NvError
GetDescriptor(
        NvDdkUsbhHandle hUsbh,
        NvU32 ephandle,
        DeviceRequestTypes descType,
        NvU8* pBuffer,
        NvU16 Value,
        NvU16 Index,
        NvU32* requestLen)
{
    NvError errStatus = NvSuccess;
    SetupPacket cntrlPacket;
    NvDdkUsbhTransfer ddkTransfer;

    if (descType == DeviceRequestTypes_Get_Descriptor)
    {
        cntrlPacket.ReqType  = BmRequestCodeDir_Dev2Host << 7 |
            BmRequestCodeType_Standard << 5 |BmRequestCodeTarget_Device;
        cntrlPacket.Request  = descType;
        cntrlPacket.Value    = Value;
        cntrlPacket.Index    = Index;
        cntrlPacket.Length   = *requestLen;
        ddkTransfer.TransferBufferLength = *requestLen;
    }else if (descType == DeviceRequestTypes_Set_Address ||
            descType == DeviceRequestTypes_Set_Configuration ||
            descType == DeviceRequestTypes_Set_Feature)
    {
        cntrlPacket.ReqType  = BmRequestCodeDir_Host2Dev << 7 |
            BmRequestCodeType_Standard << 5 |BmRequestCodeTarget_Device;
        cntrlPacket.Request  = descType;
        cntrlPacket.Value    = Value;
        cntrlPacket.Index    = Index;
        cntrlPacket.Length   = *requestLen;
        ddkTransfer.TransferBufferLength   = 0x0;
    }else if(descType == DeviceRequestTypes_Clear_Feature)
    {
        cntrlPacket.ReqType  = BmRequestCodeDir_Host2Dev << 7 |
            BmRequestCodeType_Standard << 5 |BmRequestCodeTarget_Endpoint;
        cntrlPacket.Request  = descType;
        cntrlPacket.Value    = Value;
        cntrlPacket.Index    = Index;
        cntrlPacket.Length   = *requestLen;
        ddkTransfer.TransferBufferLength   = *requestLen;
    }else if (descType == ClassSpecificRequest_Get_Max_Luns)
    {
        cntrlPacket.ReqType  = BmRequestCodeDir_Dev2Host << 7 |
            BmRequestCodeType_Class << 5 |BmRequestCodeTarget_Interface;
        cntrlPacket.Request  = descType;
        cntrlPacket.Value    = Value;
        cntrlPacket.Index    = Index;
        cntrlPacket.Length   = *requestLen;
        ddkTransfer.TransferBufferLength   = *requestLen;
    }else if (descType == ClassSpecificRequest_Bulk_Reset)
    {
        cntrlPacket.ReqType  = BmRequestCodeDir_Host2Dev << 7 |
            BmRequestCodeType_Class << 5 |BmRequestCodeTarget_Interface;
        cntrlPacket.Request  = descType;
        cntrlPacket.Value    = Value;
        cntrlPacket.Index    = Index;
        cntrlPacket.Length   = *requestLen;
        ddkTransfer.TransferBufferLength  = *requestLen;
    }

    ddkTransfer.EpPipeHandler        = ephandle;
    ddkTransfer.pTransferBuffer       = pBuffer;
    NvOsMemcpy((void *)ddkTransfer.SetupPacket, (const void *)(&cntrlPacket), 8); // sizeof(SetUpPacket)
    ddkTransfer.ActualLength = 0;

    errStatus = NvDdkUsbhSubmitTransfer(hUsbh, &ddkTransfer);


    // actual transmitted length
    *requestLen = ddkTransfer.ActualLength;
    return errStatus;
}

NvError EnumerateDevice(NvDdkUsbhHandle hUsbh, DeviceInfo *pdevInfo)
{
    NvError errStatus = NvSuccess;
    DeviceRequestTypes descType;
    UsbStringDescriptor strDesc;
    NvU32 transferLen = 0;
    NvU16 Value, Index, cfgLen, langId;
    NvU8 *mbuffer = NULL;
    NvU8 *pbuffer = NULL;
    UsbEndPointDescriptor  endDesc;
    NvDdkUsbhPipeInfo PipeInfo;
    NvU8 endpoints = 0;

#ifdef MULTI_LUN
    NvU32 i = 0;
#endif
    mbuffer = NvOsAlloc(MAX_DESC_LEN);
    pbuffer = mbuffer;

    PipeInfo.DeviceAddr = 0;
    PipeInfo.EndpointType = NvDdkUsbhPipeType_Control;
    PipeInfo.EpNumber = 0;
    PipeInfo.PipeDirection = NvDdkUsbhDir_In;
    PipeInfo.MaxPktLength = Usb_Speed_Pkt_Size_Full;
    SCSI_DEBUG_PRINTF((" LINe %d func %s", __LINE__, __func__));

    NvDdkUsbhGetPortSpeed(hUsbh, &PipeInfo.SpeedOfDevice);

    NvDdkUsbhAddPipe(hUsbh, &PipeInfo);

    pdevInfo->CntrlEp0Hndle = PipeInfo.EpPipeHandler;
    // get Device Descriptor on default address (0), len 64 bytes
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = DEV_DESC_LEN; // min control packet size
    Value = DescriptorType_Usb_Dt_Device << BmRequestCodeDir_Shift ;
    Index = 0;
    GetDescriptor(hUsbh,
        pdevInfo->CntrlEp0Hndle, descType, pbuffer, Value, Index, &transferLen);

    SCSI_DEBUG_PRINTF(
        ("EnumDev::GET_DESC for 0,0 is Completed, descriptor len %d\n",
                 transferLen));
    NvOsMemcpy((void *)(&pdevInfo->DevDesc), pbuffer,transferLen);

    // set Address on default address (0), specify new address (1), len 0
    descType = DeviceRequestTypes_Set_Address;
    transferLen = 0;
    Value       = pdevInfo->DevAddr;
    Index = 0;
    GetDescriptor(hUsbh,
        pdevInfo->CntrlEp0Hndle, descType, pbuffer, Value, Index, &transferLen);
    SCSI_DEBUG_PRINTF(
        ("EnumDev::SET_ADDRESS for 0,0 is Completed, descriptor len %d \n",
                 transferLen));

    PipeInfo.DeviceAddr = pdevInfo->DevAddr;
    PipeInfo.EndpointType = NvDdkUsbhPipeType_Control;
    PipeInfo.EpNumber = 0;
    PipeInfo.PipeDirection = NvDdkUsbhDir_In;

    PipeInfo.MaxPktLength = pdevInfo->DevDesc.BMaxPacketSize0;//0x40;
    NvDdkUsbhGetPortSpeed(hUsbh, &PipeInfo.SpeedOfDevice);

    NvDdkUsbhAddPipe(hUsbh, &PipeInfo);
    pdevInfo->CntrlEpHndle = PipeInfo.EpPipeHandler;

    // get Devcie descriptor with new address (1), len(18 bytes) as obtained with first device descriptor
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen     = pdevInfo->DevDesc.BLength;//0x12
    Value = DescriptorType_Usb_Dt_Device << BmRequestCodeDir_Shift;
    Index = 0;

    SCSI_DEBUG_PRINTF(
        ("EnumDev::GET_DEV_DESCRIPTOR for SET  ADDR(1) is Completed, descriptor len %d \n",
                 transferLen));
    NvOsMemcpy((void *)(&pdevInfo->DevDesc), pbuffer,transferLen);

    // get Config descriptor (basic ), len 9 bytes
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = CNF_DESC_LEN;
    Value = DescriptorType_Usb_Dt_Config << BmRequestCodeDir_Shift;
    Index = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer, Value,
                                     Index, &transferLen);

    NvOsMemcpy((void *)(&pdevInfo->CfgDesc), pbuffer, transferLen);
    cfgLen = pdevInfo->CfgDesc.WTotalLength;

    SCSI_DEBUG_PRINTF((
        "EnumDev:: GET_CONFIG_ DESC for is Completed::config Length= %d transfer len %d\n",
         cfgLen, transferLen));
    // get String Desc, LANGID , len max
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = STR_DESC_LEN;
    Value = DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer,
         Value, Index, &transferLen);
    SCSI_DEBUG_PRINTF(
        ("EnumDev::GET_STRING_LANGID_ DESC Completed::and transferlen %d \n",
                 transferLen));

    NvOsMemcpy((void *)(&pdevInfo->StrDesc), pbuffer, transferLen);
    strDesc = pdevInfo->StrDesc;
    langId = strDesc.BString[0];

    // get String Desc, Index 3, len 255 (lang id from previous one)
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = MAX_DESC_LEN;
    Value = StringDescriptorIndex_Usb_Serial_Id |
        (DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift);
    Index = langId;
    SCSI_DEBUG_PRINTF(("EnumDev::GET_STRING_LANGID22_ DESC START\n"));
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer, Value,
        Index, &transferLen);
    SCSI_DEBUG_PRINTF(("EnumDev::len %d \n", transferLen));

    // get Config Desc complete, obtain the length from first config descriptor..
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = cfgLen;
    Value = DescriptorType_Usb_Dt_Config << BmRequestCodeDir_Shift;
    Index       = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer, Value,
         Index, &transferLen);
    SCSI_DEBUG_PRINTF(
        ("EnumDev:: GET_CONFIG_ DESC for is Completed::config Length= %d transfer len %d\n",
                 cfgLen, transferLen));
    NvOsMemcpy(( void *)( &pdevInfo->CfgDesc ), pbuffer , transferLen );

    SCSI_DEBUG_PRINTF(("EnumDev:: GET_CONFIG_ DESC InterfaceDesc %d\n",
         pdevInfo->CfgDesc.BNumInterfaces));
    pbuffer += CNF_DESC_LEN; //point to interface descriptor..

    // retrieve interface descriptor from complete conf desc
    NvOsMemcpy((void *)(&pdevInfo->IntfDesc), pbuffer,
                     sizeof(UsbInterfaceDescriptor));

    SCSI_DEBUG_PRINTF(
        ("EnumDev:: Number of endpoint desc %d class %d subClas %d and Protocol %d\n",
            pdevInfo->IntfDesc.BNumEndpoints,
            pdevInfo->IntfDesc.BInterfaceClass,
            pdevInfo->IntfDesc.BInterfaceSubClass,
            pdevInfo->IntfDesc.BInterfaceProtocol));

    if ((pdevInfo->IntfDesc.BInterfaceClass != DeviceClass_MassStorage) &&
            (pdevInfo->IntfDesc.BInterfaceSubClass != DeviceSubClass_Trans) &&
            (pdevInfo->IntfDesc.BInterfaceProtocol !=
                         DeviceProtocolClass_Bulk_Only_Trnspt))
    {
        SCSI_DEBUG_PRINTF(("EnumDev:: class %d subClas %d and Protocol %d\n",
                pdevInfo->IntfDesc.BInterfaceClass,
                pdevInfo->IntfDesc.BInterfaceSubClass,
                pdevInfo->IntfDesc.BInterfaceProtocol));
        // return here.. we do not support this device
        return -1;
    }

    // get EpPipeHandler descriptor informations
    pbuffer += INF_DESC_LEN;//point to EpPipeHandler descriptor..

    for (endpoints = 0; endpoints < pdevInfo->IntfDesc.BNumEndpoints ; endpoints++)
    {
        NvOsMemcpy((void *)(&endDesc) , pbuffer ,
             sizeof(UsbEndPointDescriptor));
        if (endDesc.BEndpointAddress & 0x80 )  // if it is IN ep
        {
            SCSI_DEBUG_PRINTF(("EnumDev:: In EpPipeHandler Present\n"));
            NvOsMemcpy((void *)(&pdevInfo->InEndpoint), pbuffer,
                 sizeof(UsbEndPointDescriptor));
        }else
        {
            SCSI_DEBUG_PRINTF(("EnumDev:: Out EpPipeHandler Present\n" ));
            NvOsMemcpy((void *)(&pdevInfo->OutEndpoint), pbuffer,
                 sizeof(UsbEndPointDescriptor));
        }
        pbuffer += END_DESC_LEN;// point to next EpPipeHandler descriptor..
    }

    // get String Desc, LANGID , len 255 , request lang id codes
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = STR_DESC_LEN;
    Value = DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer, Value,
        Index, &transferLen);
    NvOsMemcpy((void *)(&pdevInfo->StrDesc), pbuffer, transferLen);
    strDesc = pdevInfo->StrDesc;
    langId = strDesc.BString[0];

    // get String Desc, Index 2, len 255 (Windex/lang id from previous one)
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = MAX_DESC_LEN;
    Value = StringDescriptorIndex_Usb_Prod_Id |
            DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index       = langId;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType,
            pbuffer, Value, Index, &transferLen);

    // get String Desc, LANGID , len 255
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = STR_DESC_LEN;
    Value = DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer,
                 Value, Index, &transferLen);
    NvOsMemcpy((void *)(&pdevInfo->StrDesc), pbuffer, transferLen);
    strDesc = pdevInfo->StrDesc;
    langId =  strDesc.BString[0];

    // get String Desc, Index 2, len 255 (Windex/lang id from previous one)
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = MAX_DESC_LEN;
    Value = StringDescriptorIndex_Usb_Prod_Id |
            DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index = langId;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer,
             Value, Index, &transferLen);
    // get String Desc, LANGID , len 2
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = STR_SHORT_LEN;
    Value = DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer,
             Value, Index, &transferLen);

    // get String Desc, LANGID , len 4
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = STR_DESC_LEN;
    Value = DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift;
    Index = 0;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer, Value,
             Index, &transferLen);
    NvOsMemcpy((void *)(&pdevInfo->StrDesc), pbuffer,transferLen);
    strDesc = pdevInfo->StrDesc;
    langId = strDesc.BString[0];

    // get String Desc, Index 3 , len 2
    descType = DeviceRequestTypes_Get_Descriptor;
    transferLen = STR_SHORT_LEN;
    Value = StringDescriptorIndex_Usb_Serial_Id |
         (DescriptorType_Usb_Dt_String << BmRequestCodeDir_Shift);
    Index = langId;
    GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer, Value,
                 Index, &transferLen);
    SCSI_DEBUG_PRINTF(("EnumDev::GET_STRING_LANGID33_ DESC Completed::len %d\n",
             transferLen));

    // get String Desc, Index 3 , len from the last descriptor lenght

    // set configuration, configuration value returned by
    // configuration descriptor, len 0
    descType = DeviceRequestTypes_Set_Configuration;
    transferLen = 0;
    Value       = pdevInfo->CfgDesc.BConfigurationValue;// new configuration
    Index = 0;
    GetDescriptor(hUsbh,pdevInfo->CntrlEpHndle, descType, pbuffer, Value, Index,
             &transferLen);
    SCSI_DEBUG_PRINTF(("EnumDev::SET_CONFIG Completed::len %d\n", transferLen));
#ifdef MULTI_LUN
    // Class specific request, max luns supported request for BOT , len 1
    // gET_MAX_LUNS
    // A1 FE 00 00 00 00 01 00
    for (i =0; i < 3; i++)
    {
        descType = (DeviceRequestTypes)ClassSpecificRequest_Get_Max_Luns;
        transferLen = 1;
        Value = 0;
        Index = 0;
        errStatus = GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType,
                 pbuffer, Value, Index, &transferLen);
        if (errStatus == NvSuccess)
            break;
        else if (errStatus == NvError_UsbfEpStalled)
        {
            // call clear feature  to clear the stalled state
            // clear feature command
            descType = DeviceRequestTypes_Clear_Feature;
            transferLen = 0;// zero transfer len
            Value = 0;
            Index       = 0; // it is control endpoint
            GetDescriptor(hUsbh, pdevInfo->CntrlEpHndle, descType, pbuffer,
                 Value, Index, &transferLen);
        }
    }
    if (errStatus == NvSuccess)
    {
        pdevInfo->MaxLuns = pbuffer[0];
    }
    else
    {
        // default it is 1
        pdevInfo->MaxLuns  = 0;
    }
#else
    pdevInfo->MaxLuns  = 0;
#endif

    SCSI_DEBUG_PRINTF(("EnumDev::GET_MAX_LUN Completed::Maxluns %d\n",
             pdevInfo->MaxLuns));
    // free allocated buffer memory
    NvOsFree((void*)mbuffer);
    return errStatus;
}

NvError GetMediaInfo(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun)
{
    // NvDdkUsbhHandle ghUsbh;
    NvError errStatus = NvSuccess;
    NvU8 iterations = 3;
    NvU8 deviceready = 0;

    // inquiry
    errStatus = scsiInquiryCommand(ghUsbh, pdevInfo,bIn, bOut, lun);
    if (errStatus != NvSuccess)
    {
        SCSI_DEBUG_PRINTF(
            ("EnumDev::GetMediaInfo Inquiry command failed for lun %d \n", lun));
        return errStatus;
    }

    if (pdevInfo->Csw.Status != BULK_STAT_OK)
    {
        // do request sense command for the failed operation
        errStatus = scsiRequestSenseCommand(ghUsbh, pdevInfo, bIn, bOut, lun);
        if ((pdevInfo->CommandSenseData.SenseKey == RequestSenseKey_Not_Ready)
                 && (pdevInfo->CommandSenseData.AdditionalSenseCode ==
                 RequestSenseKey_No_Medium))
        {
            // medium not present..
            SCSI_DEBUG_PRINTF(
              ("ScsiRequestSenseCommand Medium NOT present on lun %d \n", lun));
            return (NvError)-1;//device media not present..
        }else if ((pdevInfo->CommandSenseData.SenseKey ==
                                     RequestSenseKey_Unit_Attention))
        {
            // medium not ready..
            SCSI_DEBUG_PRINTF((
                 "ScsiRequestSenseCommand Medium NOT ready on lun %d \n", lun));
            if (pdevInfo->CommandSenseData.AdditionalSenseCode ==
                                    RequestSenseKey_Not_Ready_To_Ready)
            {

                for (deviceready = 0; deviceready < 10; deviceready++)
                {
                    // test unit ready command
                    errStatus = scsiTestUnitReady(ghUsbh, pdevInfo,
                                         bIn, bOut, lun);
                }

                if (pdevInfo->Csw.Status != BULK_STAT_OK)
                {
                    SCSI_DEBUG_PRINTF(("ScsiRequestSenseCommand Medium NOT ready"
                            " & present on lun %d \n",
                            lun));
                    return (NvError)-1;// device media not ready..
                }
            }
        }
    }

    // test unit ready command
    errStatus = scsiTestUnitReady(ghUsbh, pdevInfo, bIn, bOut, lun);

    if (pdevInfo->Csw.Status != BULK_STAT_OK)
    {
        // do request sense command for the failed operation
        errStatus = scsiRequestSenseCommand(ghUsbh, pdevInfo, bIn, bOut, lun);
        if ((pdevInfo->CommandSenseData.SenseKey == RequestSenseKey_Not_Ready)
                && (pdevInfo->CommandSenseData.AdditionalSenseCode
                             == RequestSenseKey_No_Medium))
        {
            // medium not present..
            SCSI_DEBUG_PRINTF(
               ("scsiRequestSenseCommand Medium NOT present on lun %d\n", lun));
            return (NvError)-1;//device media not present..
        }else if ((pdevInfo->CommandSenseData.SenseKey
                            == RequestSenseKey_Unit_Attention))
        {
            // medium not ready..
            SCSI_DEBUG_PRINTF(
                ("scsiRequestSenseCommand Medium NOT ready on lun %d\n", lun));

            // return (NvError)-1;//device media not present..

            if (pdevInfo->CommandSenseData.AdditionalSenseCode
                            == RequestSenseKey_Not_Ready_To_Ready)
            {
                for (deviceready = 0;deviceready < 10; deviceready++)
                {
                    // test unit ready command
                    errStatus = scsiTestUnitReady(ghUsbh,pdevInfo, bIn,
                                                     bOut, lun);
                }

                if (pdevInfo->Csw.Status != BULK_STAT_OK)
                {
                    SCSI_DEBUG_PRINTF(("scsiRequestSenseCommand Medium NOT ready"
                            " & present on lun %d \n", lun));
                    return (NvError)-1;// device media not ready..
                }
            }
        }
    }
    // read capacity
    // sector size and total no., of sectors addressable
    do
    {
        errStatus = scsiReadCapacityCommand(ghUsbh, pdevInfo, bIn, bOut, lun);
        if (errStatus != NvSuccess)
            SCSI_DEBUG_PRINTF(("EnumDev::GetMediaInfo scsiReadCapacityCommand "
                    "failed for lun %d trialno., %d\n", lun, iterations));
        iterations--;
    }while ((errStatus != NvSuccess) && iterations);

    if ((errStatus != NvSuccess) && !iterations)
    {
        SCSI_DEBUG_PRINTF(("UsbTestApp ::scsiReadCapacityCommand receive failed attempting BOT\n"));
        BulkOnlyResetCommand(ghUsbh, pdevInfo);
    }

    if (pdevInfo->Csw.Status != BULK_STAT_OK)
    {
        return (NvError)-1;// device media not ready..
    }else
    {
        errStatus = NvSuccess;
    }
    return errStatus;
}

NvError
scsiInquiryCommand(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun)
{
    NvError errStatus = NvSuccess;
    Inquiry inqCmd = {0};
    InquiryData inqData = {0};
    NvDdkUsbhTransfer ddkTransfer ={0};

    // set the Various values to form the commmand and sent out.
    inqCmd.OpCode  = ScsiCommands_Inquiry;
    inqCmd.LogicalUnitNum = lun;
    inqCmd.AllocationLength = sizeof(InquiryData);//36 bytes

    SetCommandBlockWrapper(inqCmd.AllocationLength, pdevInfo, 0x80,
            lun, ScsiCommandLen_Inq_TestUnit, (NvU8*)&inqCmd);

    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Cbw);
    ddkTransfer.ActualLength = 0;
    ddkTransfer.Status = 0;
    // transmit command and retrieve the data via inqData
    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {
        /// 2 data phase, receive data over bulkIn EpPipeHandler
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.pTransferBuffer = (NvU8 *)(&inqData);
        ddkTransfer.TransferBufferLength = sizeof(InquiryData);
        ddkTransfer.ActualLength  = 0;
        ddkTransfer.Status = 0;
        // transmit command and retrieve the data via inqData
        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    if (errStatus == NvSuccess)
    {
        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength  = 0;
        ddkTransfer.Status  = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    // validate csw
    return errStatus;
}

NvError
scsiReadCapacityCommand(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun)
{
    NvError errStatus = NvSuccess;
    ReadCapacity rdCapacity = {0};
    NvDdkUsbhTransfer ddkTransfer = {0};
    DeviceRequestTypes descType;
    NvU16 Value, Index;
    NvU32 transferLen = 0;
    NvU8 pbuffer[8];

    // Read Capacity command
    rdCapacity.OpCode = ScsiCommands_Read_Capacity_10;
    rdCapacity.Lun = lun;

    SetCommandBlockWrapper(sizeof(ReadCapacityData), pdevInfo, 0x80,
            lun, sizeof(ReadCapacity), (NvU8*)&rdCapacity);

    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Cbw);

    // transmit command and retrieve the data via inqData
    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {
        /// 2 data phase, receive data over bulkIn endpoint capacity data via pdevInfo->RdCapacityData[lun]
        ddkTransfer.EpPipeHandler            = bIn;
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&pbuffer);
        ddkTransfer.TransferBufferLength   = sizeof(ReadCapacityData);
        ddkTransfer.Status               = 0;
        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }

    SCSI_DEBUG_PRINTF(
        ("UsbTestApp ::scsiReadCapacityCommand Status = %d::\n",
                 ddkTransfer.Status));
    // update the capacity and sector size globlly for usage for Read and Write operations
    // set the transfer direction by using ephandler..
    if ((errStatus == NvSuccess) && !ddkTransfer.Status)
    {
        ///3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength = 0;
        ddkTransfer.Status = 0;

        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

        pdevInfo->RdCapacityData[lun].RLogicalBlockAddress    =
             pbuffer[0]<<24 |pbuffer[1] << 16 |pbuffer[2]<<8 | pbuffer[3];
        pdevInfo->RdCapacityData[lun].BlockLengthInBytes      =
             pbuffer[4]<<24 |pbuffer[5] << 16 |pbuffer[6]<< 8 | pbuffer[7];
    }else
    {
        errStatus = -1;
        SCSI_DEBUG_PRINTF(("UsbTestApp ::scsiReadCapacityCommand failed with = %d::\n",
                     ddkTransfer.Status));
        // clear feature command
        descType = DeviceRequestTypes_Clear_Feature;
        transferLen = 0;//zero transfer len
        Value = 0;
        Index       = pdevInfo->InEndpoint.BEndpointAddress;// endpoint number
        GetDescriptor(ghUsbh, pdevInfo->CntrlEpHndle, descType, &pbuffer[0],
                 Value, Index, &transferLen);

        /// receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength = 0;
        ddkTransfer.Status = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    return errStatus;
}


NvError
scsiReadFormatCapacities(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun)
{
    NvError errStatus = NvSuccess;
    ReadCapacity rdCapacity={0};
    NvDdkUsbhTransfer ddkTransfer= {0};
    DeviceRequestTypes descType;
    NvU16 Value, Index;
    NvU32 transferLen = 0;
    NvU8 pbuffer[256];

    // Read Capacity command
    rdCapacity.OpCode   = ScsiCommands_Read_Format_Capacities;
    rdCapacity.Lun          = lun;
    rdCapacity.Control  = ScsiCommandLen_RdFormatCap;

    SetCommandBlockWrapper(ScsiCommandLen_RdFormatCap, pdevInfo,
            0x80, lun, sizeof(ReadCapacity), (NvU8*)&rdCapacity);

    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler            = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Cbw);

    // transmit command and retrieve the data via inqData
    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {
        /// 2 data phase, receive data over bulkIn EpPipeHandler capacity data via pdevInfo->RdCapacityData[lun]
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.ActualLength = 0;
        ddkTransfer.pTransferBuffer = pbuffer;
        ddkTransfer.TransferBufferLength = ScsiCommandLen_RdFormatCap;
        ddkTransfer.Status = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }

    SCSI_DEBUG_PRINTF(("UsbTestApp ::scsiReadCapacityCommand Status = %d::\n",
                 ddkTransfer.Status));

    // update the capacity and sector size globlly for usage for Read and Write operations
    // set the transfer direction by using ephandler..
    if (errStatus == NvSuccess && !ddkTransfer.Status)
    {

        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength = 0;
        ddkTransfer.Status  = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }else
    {
        errStatus = -1;
        SCSI_DEBUG_PRINTF(("UsbTestApp ::scsiReadCapacityCommand failed with = %d::\n",
                 ddkTransfer.Status));

        // clear feature command
        descType = DeviceRequestTypes_Clear_Feature;
        transferLen = 0;    //zero transfer len
        Value = 0;
        Index       = pdevInfo->InEndpoint.BEndpointAddress;// endpoint number
        GetDescriptor(ghUsbh, pdevInfo->CntrlEpHndle, descType, &pbuffer[0],
                     Value, Index, &transferLen);

        /// receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength = 0;
        ddkTransfer.Status = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    return errStatus;
}

NvError
scsiRead(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun,
        NvU32 lba,
        NvU32 numOfSectors,
        NvU8* pDataBuffer)
{
    NvError errStatus = NvSuccess;
    NvDdkUsbhTransfer ddkTransfer = {0};
    ReadWrite10 rdCmd = {0};

    // Fill the various values for the READ10 Command
    rdCmd.OpCode                = ScsiCommands_Read_10;
    rdCmd.LogicalUnitNum        = lun;   // Logical unit number

    /// shilft lba so that upper byte goes to logicalBlockAddress[0] and lower byte goes to logicalBlockAddress[3]
    rdCmd.LogicalBlockAddress[0] = (lba & 0xFF000000) >> 24;
    rdCmd.LogicalBlockAddress[1] = (lba & 0x00FF0000) >> 16;
    rdCmd.LogicalBlockAddress[2] = (lba & 0x0000FF00) >> 8;
    rdCmd.LogicalBlockAddress[3] = lba & 0x000000FF;
    rdCmd.TransferLen[0] = (numOfSectors & 0xFF00) >> 8;
    rdCmd.TransferLen[1] = numOfSectors & 0x00FF;

    // sectorSize obtained from scsiReadCapacityCommand
    SetCommandBlockWrapper
        (numOfSectors * pdevInfo->RdCapacityData[lun].BlockLengthInBytes,
            pdevInfo, 0x80, lun, ScsiCommandLen_WrRd10, (NvU8*)&rdCmd);

    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler            = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength = sizeof(Usb_Msd_Cbw);

    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {

        /// 2 data phase, receive data over bulkIn endpoint data to pDataBuffer
        ddkTransfer.EpPipeHandler            = bIn;
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.pTransferBuffer      = pDataBuffer;
        ddkTransfer.TransferBufferLength   =
             numOfSectors * (pdevInfo->RdCapacityData[lun].BlockLengthInBytes);
        ddkTransfer.Status               = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }

    if (errStatus == NvSuccess)
    {
        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler         = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.Status               = 0;
        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    return errStatus;
}

NvError
scsiWrite(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun,
        NvU32 lba,
        NvU32 numOfSectors,
        NvU8* pDataBuffer)
{

    NvError errStatus = NvSuccess;
    NvDdkUsbhTransfer ddkTransfer = {0};
    ReadWrite10 wrCmd = {0};

    // set the values for the command
    wrCmd.OpCode = ScsiCommands_Write_10;
    wrCmd.LogicalUnitNum = lun;

    /// shilft lba so that upper byte goes to logicalBlockAddress[0] and lower byte goes tologicalBlockAddress[3]
    wrCmd.LogicalBlockAddress[0]  = (lba & 0xFF000000) >> 24;

    wrCmd.LogicalBlockAddress[1]  = (lba & 0x00FF0000) >> 16;
    wrCmd.LogicalBlockAddress[2]  = (lba & 0x0000FF00) >> 8;
    wrCmd.LogicalBlockAddress[3]  = lba & 0x000000FF;
    wrCmd.TransferLen[0] = (numOfSectors & 0xFF00) >> 8;
    wrCmd.TransferLen[1] = numOfSectors & 0x00FF;

    // sectorSize obtained from scsiReadCapacityCommand
    SetCommandBlockWrapper(
            numOfSectors * pdevInfo->RdCapacityData[lun].BlockLengthInBytes,
            pdevInfo, 0, lun, ScsiCommandLen_WrRd10, (NvU8*)&wrCmd);

    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler            = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Cbw);

    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    if (errStatus == NvSuccess)
    {
        /// 2 data phase, transmit data over bulkOut endpoint data from pDataBuffer
        ddkTransfer.EpPipeHandler            = bOut;
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.pTransferBuffer      = pDataBuffer;
        ddkTransfer.TransferBufferLength   =
             numOfSectors * (pdevInfo->RdCapacityData[lun].BlockLengthInBytes);
        ddkTransfer.Status               = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }

    if (errStatus == NvSuccess)
    {
        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler         = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.Status               = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    return errStatus;
}

NvError
scsiTestUnitReady(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun)
{
    NvError errStatus= NvSuccess;
    TestUnitReady testready = {0};
    NvDdkUsbhTransfer ddkTransfer = {0};

    // Fill the Contents for the Command
    testready.OpCode            = ScsiCommands_Test_Unit_Ready;
    testready.LogicalUnitNum    = lun;

    SetCommandBlockWrapper(0,pdevInfo, 0x80,
             lun,ScsiCommandLen_Inq_TestUnit, (NvU8*)&testready);

    // transmit command NO data involved
    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler            = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Cbw);

    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {
        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler         = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.Status               = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    return errStatus;
}


NvError
scsiRequestSenseCommand(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo *pdevInfo,
        NvU32 bIn,
        NvU32 bOut,
        NvU8 lun)
{
    NvError errStatus = NvSuccess;
    RequestSense senseCmd = {0};
    RequestSenseData senseData;
    NvDdkUsbhTransfer ddkTransfer = {0};

    // Set the Various values to form the commmand.
    senseCmd.OpCode = ScsiCommands_Request_Sense;
    senseCmd.LogicalUnitNum = lun;
    senseCmd.AllocationLength = sizeof(RequestSenseData);

    SetCommandBlockWrapper(senseCmd.AllocationLength, pdevInfo, 0x80,
            lun, ScsiCommandLen_ReqSense, (NvU8*)&senseCmd);

    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler            = bOut;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Cbw);

    // transmit command and retrieve the data via inqData
    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {
        /// 2 data phase, receive data over bulkIn endpoint
        ddkTransfer.EpPipeHandler            = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&senseData);
        ddkTransfer.TransferBufferLength   = sizeof(RequestSenseData);
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.Status               = 0;

        // transmit command and retrieve the data via inqData
        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    if (errStatus == NvSuccess)
    {
        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler  = bIn;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength = 0;
        ddkTransfer.Status = 0;

        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }

    if (pdevInfo->Csw.Status == BULK_STAT_OK)
    {
        pdevInfo->CommandSenseData = senseData;
    }else
    {
        SCSI_DEBUG_PRINTF(
            ("scsRequestSenseCommand failed with csw Status  = %d::\n",
                pdevInfo->Csw.Status));
    }
    return errStatus;
}

NvError scsiFormatUnit(
        NvDdkUsbhHandle ghUsbh,
        DeviceInfo * pdevInfo,
        NvU32 blkInhdl,
        NvU32 blkOuthdl,
        NvU8 lun,
        NvU8 FmtData,
        NvU8 CmpList,
        NvU8 DftListFmt,
        NvU16 Interleave)
{
    NvError errStatus= NvSuccess;
    FormatUnit formatUnit = {0};
    NvDdkUsbhTransfer ddkTransfer = {0};

    // Fill the Contents for the Command
    formatUnit.OpCode = ScsiCommands_FormatUnit;
    formatUnit.SpecialParams = (lun << 5) | (FmtData << 4);

    if(FmtData)
        formatUnit.SpecialParams |= (CmpList << 3) | DftListFmt;

    formatUnit.Interleave = Interleave;

    SetCommandBlockWrapper(0,pdevInfo, 0x80, lun,
                            ScsiCommandLen_FormatUnit, (NvU8*)&formatUnit);

    // transmit command NO data involved
    /// 1 cbw transfer over bulk Out endpoint
    ddkTransfer.EpPipeHandler            = blkOuthdl;
    ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Cbw));
    ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Cbw);

    // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
    errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);

    if (errStatus == NvSuccess)
    {
        /// 3 receive csw from device over bulkIn and validate for signature and tag
        ddkTransfer.EpPipeHandler         = blkInhdl;
        ddkTransfer.pTransferBuffer      = (NvU8 *)(&(pdevInfo->Csw));
        ddkTransfer.TransferBufferLength   = sizeof(Usb_Msd_Csw);
        ddkTransfer.ActualLength            = 0;
        ddkTransfer.Status               = 0;
        // ddkTransfer.TransferSema = pUsbTestCtxt->Semaphore;
        errStatus = NvDdkUsbhSubmitTransfer(ghUsbh, &ddkTransfer);
    }
    return errStatus;
}

