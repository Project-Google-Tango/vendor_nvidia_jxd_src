/*
 * Copyright (c) 2007-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvutil.h"
#include "nvrm_hardware_access.h"
#include "nvrm_message.h"
#include "nvrm_rpc.h"
#include "nvrm_moduleloader.h"
#include "nvrm_moduleloader_private.h"
#include "nvrm_avp_private.h"
#include "nvrm_structure.h"

#ifndef NV_USE_AOS
#define NV_USE_AOS  0
#endif
#if !NV_USE_AOS
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/nvfw_ioctl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#endif

NvRmRPCHandle s_RPCHandle = NULL;
#define AVP_EXECUTABLE_NAME "nvrm_avp.axf"

#if NV_USE_AOS
static NvError SendMsgDetachModule(NvRmLibraryHandle  hLibHandle);
static NvError SendMsgAttachModule(NvRmLibraryHandle *hLibHandle,
                                   void* pArgs,
                                   NvU32 sizeOfArgs);
#endif

NvError NvRmPrivInitModuleLoaderRPC(NvRmDeviceHandle hDevice);
void NvRmPrivDeInitModuleLoaderRPC(void);

#define ADD_MASK                   0x00000001
// For the elf to be relocatable, we need atleast 2 program segments
// Although even elfs with more than 1 program segment may not be relocatable.
#define MIN_SEGMENTS_FOR_DYNAMIC_LOADING    2

#if !NV_USE_AOS
static NvError NvRmIoctl(int req, void *op)
{
    NvError error = NvSuccess;
    int nvfwfd = open("/dev/nvfw", O_RDWR);
    if (nvfwfd < 0)
    {
        NvOsDebugPrintf("Open /dev/nvfw failed: %s\n", strerror(errno));
        return NvError_KernelDriverNotFound;
    }

    if (ioctl(nvfwfd, req, op) < 0)
    {
        NvOsDebugPrintf("/dev/nvfw ioctl failed (%d): %s\n", req, strerror(errno));
        error = NvError_IoctlFailed;
    }
    close(nvfwfd);
    return error;
}
#endif

NvError
NvRmPrivLoadKernelLibrary(NvRmDeviceHandle hDevice,
                      const char *pLibName,
                      NvRmLibraryHandle *hLibHandle)
{
    NvError Error = NvSuccess;

    if ((Error = NvRmPrivLoadLibrary(hDevice, pLibName, 0, NV_FALSE,
        hLibHandle)) != NvSuccess)
    {
        return Error;
    }
    return Error;
}

#if NV_USE_AOS

NvError
NvRmLoadLibrary(NvRmDeviceHandle hDevice,
                const char *pLibName,
                void* pArgs,
                NvU32 sizeOfArgs,
                NvRmLibraryHandle *hLibHandle)
{
    NvError Error = NvSuccess;
    NV_ASSERT(sizeOfArgs <= MAX_ARGS_SIZE);

    Error = NvRmLoadLibraryEx(hDevice, pLibName, pArgs, sizeOfArgs, NV_FALSE,
        hLibHandle);
    return Error;
}

NvError
NvRmLoadLibraryEx(NvRmDeviceHandle hDevice,
                const char *pLibName,
                void* pArgs,
                NvU32 sizeOfArgs,
                NvBool IsApproachGreedy,
                NvRmLibraryHandle *hLibHandle)
{
    NvError Error = NvSuccess;
    NV_ASSERT(sizeOfArgs <= MAX_ARGS_SIZE);

    Error = NvRmPrivInitAvp(hDevice);
    if (Error)
    {
        NvOsDebugPrintf("\r\nAvpInitFailed\r\n");
        return Error;
    }
    // Check if this is a special case to just load avp kernel.
    else if (!NvOsStrcmp(pLibName, AVP_EXECUTABLE_NAME))
    {
        *hLibHandle = NULL;
        Error = NvRmPrivRPCConnect(s_RPCHandle);
        return Error;
    }

    if ((Error = NvRmPrivLoadLibrary(hDevice, pLibName, 0, IsApproachGreedy,
        hLibHandle)) != NvSuccess)
    {
        return Error;
    }
    if((Error = NvRmPrivRPCConnect(s_RPCHandle)) == NvSuccess)
    {
        Error = SendMsgAttachModule(hLibHandle, pArgs, sizeOfArgs);
    }
    else
    {
        NvOsDebugPrintf("\r\nRPCConnect timedout during NvRmLoadLibraryEx\r\n");
    }
    if (Error)
    {
        NvRmPrivFreeLibrary(*hLibHandle);
    }
    return Error;
}

NvError
NvRmGetProcAddress(NvRmLibraryHandle Handle,
               const char *pSymbol,
               void **pSymAddress)
{
    NvError Error = NvSuccess;
    NV_ASSERT(Handle);
    Error = NvRmPrivGetProcAddress(Handle, pSymbol, pSymAddress);
    return Error;
}

NvError NvRmFreeLibrary(NvRmLibraryHandle hLibHandle)
{
    NvError Error = NvSuccess;
    NV_ASSERT(hLibHandle);
    if((Error = NvRmPrivRPCConnect(s_RPCHandle)) == NvSuccess)
    {
        Error = SendMsgDetachModule(hLibHandle);
    }
    if (Error == NvSuccess)
    {
        Error = NvRmPrivFreeLibrary(hLibHandle);
    }

    return Error;
}

#else

NvError NvRmFreeLibrary( NvRmLibraryHandle hLibHandle )
{
    struct nvfw_load_handle op;
    op.handle = hLibHandle;
    return NvRmIoctl(NVFW_IOC_FREE_LIBRARY, (void *)&op);
}

NvError NvRmGetProcAddress( NvRmLibraryHandle hLibHandle, const char * pSymbolName, void* * pSymAddress )
{
    struct nvfw_get_proc_address_handle op;
    NvError error;

    NvOsDebugPrintf("%s: symbolname=%s\n", __func__, pSymbolName);

    op.symbolname = pSymbolName;
    op.length = NvOsStrlen(pSymbolName);
    op.handle = hLibHandle;

    error = NvRmIoctl(NVFW_IOC_GET_PROC_ADDRESS, (void *)&op);
    if (error == NvSuccess)
    {
        *pSymAddress = op.address;
    }
    return error;
}

NvError NvRmLoadLibraryEx( NvRmDeviceHandle hDevice, const char * pLibName, void* pArgs, NvU32 sizeOfArgs, NvBool IsApproachGreedy, NvRmLibraryHandle * hLibHandle )
{
    struct nvfw_load_handle op;
    NvError error;

    // FIXME: Still calling NvRmPrivInitAvp in userland stub to kick some initialization.
    NvOsDebugPrintf("%s <userland impl->stub>: calling NvRmPrivInitAvp\n", __func__);
    {
        NvError e = NvRmPrivInitAvp(hDevice);
        if (e)
        {
            NvOsDebugPrintf("\r\nAvpInitFailed\r\n");
            return e;
        }
    }

    op.filename = pLibName;
    op.length = NvOsStrlen(pLibName);
    op.args = pArgs;
    op.argssize = sizeOfArgs;
    op.greedy = IsApproachGreedy;

    error = NvRmIoctl(NVFW_IOC_LOAD_LIBRARY_EX, (void *)&op);
    if (error == NvSuccess) {
        *hLibHandle = (NvRmLibraryHandle)op.handle;
    }
    return error;
}

NvError NvRmLoadLibrary( NvRmDeviceHandle hDevice, const char * pLibName, void* pArgs, NvU32 sizeOfArgs, NvRmLibraryHandle * hLibHandle )
{
    struct nvfw_load_handle op;
    NvError error;

    // FIXME: Still calling NvRmPrivInitAvp in userland stub to kick some initialization.
    NvOsDebugPrintf("%s <userland impl->stub>: calling NvRmPrivInitAvp\n", __func__);
    {
        NvError e = NvRmPrivInitAvp(hDevice);
        if (e)
        {
            NvOsDebugPrintf("\r\nAvpInitFailed\r\n");
            return e;
        }
    }

    op.filename = pLibName;
    op.length = NvOsStrlen(pLibName);
    op.args = pArgs;
    op.argssize = sizeOfArgs;

    error = NvRmIoctl(NVFW_IOC_LOAD_LIBRARY, (void *)&op);
    if (error == NvSuccess) {
        *hLibHandle = (NvRmLibraryHandle)op.handle;
    }
    return error;
}
#endif

#if NV_USE_AOS

//before unloading loading send message to avp with args and entry point via transport
static NvError SendMsgDetachModule(NvRmLibraryHandle hLibHandle)
{
    NvError Error = NvSuccess;
    NvU32 RecvMsgSize;
    NvRmMessage_DetachModule Msg;
    NvRmMessage_DetachModuleResponse MsgR;
    void *address = NULL;

    Msg.msg = NvRmMsg_DetachModule;

    if ((Error = NvRmGetProcAddress(hLibHandle, "main", &address)) != NvSuccess)
    {
        goto exit_gracefully;
    }
    Msg.msg = NvRmMsg_DetachModule;
    Msg.reason = NvRmModuleLoaderReason_Detach;
#if NV_USE_AOS == 1
    // FIXME: Do not compile this file for Linux userspace, and remove this #if.
    Msg.entryAddress = (NvU32)address;
#endif
    RecvMsgSize = sizeof(NvRmMessage_DetachModuleResponse);
    NvRmPrivRPCSendMsgWithResponse(s_RPCHandle,
                                           &MsgR,
                                           RecvMsgSize,
                                           &RecvMsgSize,
                                           &Msg,
                                           sizeof(Msg));

    Error = MsgR.error;
    if (Error)
    {
        goto exit_gracefully;
    }
exit_gracefully:
    return Error;
}

//after successful loading send message to avp with args and entry point via transport
static NvError SendMsgAttachModule(NvRmLibraryHandle *hLibHandle,
                                   void* pArgs,
                                   NvU32 sizeOfArgs)
{
    NvError Error = NvSuccess;
    NvU32 RecvMsgSize;
    NvRmMessage_AttachModule *MsgPtr=NULL;
    NvRmMessage_AttachModuleResponse MsgR;
    void *address = NULL;

    MsgPtr = NvOsAlloc(sizeof(*MsgPtr));
    if(MsgPtr==NULL)
    {
        Error = NvError_InsufficientMemory;
        goto exit_gracefully;
    }
    MsgPtr->msg = NvRmMsg_AttachModule;

    if(pArgs)
    {
        NvOsMemcpy(MsgPtr->args, pArgs, sizeOfArgs);
    }

    MsgPtr->size = sizeOfArgs;
    if ((Error = NvRmGetProcAddress(*hLibHandle, "main", &address)) != NvSuccess)
    {
        goto exit_gracefully;
    }
#if NV_USE_AOS == 1
    // FIXME: Do not compile this file for Linux userspace, and remove this #if.
    MsgPtr->entryAddress = (NvU32)address;
#endif
    MsgPtr->reason = NvRmModuleLoaderReason_Attach;
    RecvMsgSize = sizeof(NvRmMessage_AttachModuleResponse);
    NvRmPrivRPCSendMsgWithResponse(s_RPCHandle,
                                           &MsgR,
                                           RecvMsgSize,
                                           &RecvMsgSize,
                                           MsgPtr,
                                           sizeof(*MsgPtr));

    Error = MsgR.error;
    if (Error)
    {
        goto exit_gracefully;
    }
exit_gracefully:
    NvOsFree(MsgPtr);
    return Error;
}

#endif

NvError NvRmPrivInitModuleLoaderRPC(NvRmDeviceHandle hDevice)
{
    return NvRmPrivRPCInit(hDevice, "RPC_AVP_PORT", &s_RPCHandle);
}

void NvRmPrivDeInitModuleLoaderRPC()
{
    NvRmPrivRPCDeInit(s_RPCHandle);
}

SegmentNode* AddToSegmentList(SegmentNode *pList,
                              NvRmMemHandle pRegion,
                              Elf32_Phdr Phdr,
                              NvU32 Idx,
                              NvU32 PhysAddr,
                              void* MapAddress)
{
    SegmentNode *TempRec = NULL;
    SegmentNode *CurrentRec = NULL;

    TempRec = NvOsAlloc(sizeof(SegmentNode));
    if (TempRec != NULL)
    {
        TempRec->pLoadRegion = pRegion;
        TempRec->Index = Idx;
        TempRec->VirtualAddr = Phdr.p_vaddr;
        TempRec->MemorySize = Phdr.p_memsz;
        TempRec->FileOffset = Phdr.p_offset;
        TempRec->FileSize = Phdr.p_filesz;
        TempRec->LoadAddress = PhysAddr;
        TempRec->MapAddr = MapAddress;
        TempRec->Next = NULL;

        CurrentRec = pList;
        if (CurrentRec == NULL)
        {
            pList = TempRec;
        }
        else
        {
            while (CurrentRec->Next != NULL)
            {
                CurrentRec = CurrentRec->Next;
            }
            CurrentRec->Next = TempRec;
        }
    }
    return pList;
}
void RemoveRegion(SegmentNode *pList)
{
    if (pList != NULL)
    {
        SegmentNode *pCurrentRec;
        SegmentNode *pTmpRec;
        pCurrentRec = pList;
        while (pCurrentRec != NULL)
        {
            NvRmMemUnpin(pCurrentRec->pLoadRegion);
            NvRmMemHandleFree(pCurrentRec->pLoadRegion);
            pCurrentRec->pLoadRegion = NULL;
            pTmpRec = pCurrentRec;
            pCurrentRec = pCurrentRec->Next;
            NvOsFree( pTmpRec );
        }
        pList = NULL;
    }
}

void UnMapRegion(SegmentNode *pList)
{
    if (pList != NULL)
    {
        SegmentNode *pCurrentRec;
        pCurrentRec = pList;
        while (pCurrentRec != NULL && pCurrentRec->MapAddr )
        {
            NvRmMemUnmap(pCurrentRec->pLoadRegion, pCurrentRec->MapAddr,
                pCurrentRec->MemorySize);
            pCurrentRec = pCurrentRec->Next;
        }
    }
}

NvError
ApplyRelocation(SegmentNode *pList,
                NvU32 FileOffset,
                NvU32 SegmentOffset,
                NvRmMemHandle pRegion,
                const Elf32_Rel *pRel)
{
    NvError Error = NvSuccess;
    NvU8 Type = 0;
    NvU32 SymIndex = 0;
    Elf32_Word Word = 0;
    SegmentNode *pCur;
    NvU32 TargetVirtualAddr = 0;
    NvU32 LoadAddress = 0;
    NV_ASSERT(NULL != pRel);

    NvRmMemRead(pRegion, SegmentOffset,&Word, sizeof(Word));
    NV_DEBUG_PRINTF(("NvRmMemRead: SegmentOffset 0x%04x, word %p\r\n",
        SegmentOffset, Word));
    Type = ELF32_R_TYPE(pRel->r_info);

    switch (Type)
    {
    case R_ARM_NONE:
        break;
    case R_ARM_CALL:
        break;
    case R_ARM_RABS32:
        SymIndex = ELF32_R_SYM(pRel->r_info);
        if (pList != NULL)
        {
            pCur = pList;
            while (pCur != NULL)
            {
                if (pCur->Index == (SymIndex - 1))
                {
                    TargetVirtualAddr = pCur->VirtualAddr;
                    LoadAddress = pCur->LoadAddress;
                }
                pCur = pCur->Next;
            }
            if (LoadAddress > TargetVirtualAddr)
            {
                Word = Word + (LoadAddress - TargetVirtualAddr);
            }
            else //handle negative displacement
            {
                Word = Word - (TargetVirtualAddr - LoadAddress);
            }
            NV_DEBUG_PRINTF(("NvRmMemWrite: SegmentOffset 0x%04x, word %p\r\n",
                SegmentOffset, Word));
            NvRmMemWrite(pRegion, SegmentOffset, &Word, sizeof(Word));
        }
        break;
    default:
        Error = NvError_NotSupported;
        NV_DEBUG_PRINTF(("This relocation type is not handled = %d\r\n", Type));
        break;
    }
    return Error;
}

NvError
GetSpecialSectionName(Elf32_Word SectionType,
                      Elf32_Word SectionFlags,
                      const char** SpecialSectionName)
{
    const char *unknownSection = "Unknown\r\n";
    *SpecialSectionName = unknownSection;
    /// Mask off the high 16 bits for now
    switch (SectionFlags & 0xffff)
    {
    case SHF_ALLOC|SHF_WRITE:
        if (SectionType == SHT_NOBITS)
            *SpecialSectionName = ".bss\r\n";
        else if (SectionType == SHT_PROGBITS)
            *SpecialSectionName = ".data\r\n";
        else if (SectionType == SHT_FINI_ARRAY)
            *SpecialSectionName = ".fini_array\r\n";
        else if (SectionType == SHT_INIT_ARRAY)
            *SpecialSectionName = ".init_array\r\n";
        break;
    case SHF_ALLOC|SHF_EXECINSTR:
        if (SectionType == SHT_PROGBITS)
            *SpecialSectionName = ".init or fini \r\n";

        break;
    case SHF_ALLOC:
        if (SectionType == SHT_STRTAB)
            *SpecialSectionName = ".dynstr\r\n";
        else if (SectionType == SHT_DYNSYM)
            *SpecialSectionName = ".dynsym\r\n";
        else if (SectionType == SHT_HASH)
            *SpecialSectionName = ".hash\r\n";
        else if (SectionType == SHT_PROGBITS)
            *SpecialSectionName = ".rodata\r\n";
        else
            *SpecialSectionName = unknownSection;
        break;
    default:
        if (SectionType == SHT_PROGBITS)
            *SpecialSectionName = ".comment\r\n";
        else
            *SpecialSectionName = unknownSection;
        break;
    }
    return NvSuccess;
}

NvError
ParseDynamicSegment(SegmentNode *pList,
                    const char* pSegmentData,
                    size_t SegmentSize,
                    NvU32 DynamicSegmentOffset)
{
    NvError Error = NvSuccess;
    Elf32_Dyn* pDynSeg = NULL;
    NvU32  Counter = 0;
    NvU32 RelocationTableAddressOffset = 0;
    NvU32 RelocationTableSize = 0;
    NvU32 RelocationEntrySize = 0;
    const Elf32_Rel* RelocationTablePtr = NULL;
    NvU32 SymbolTableAddressOffset = 0;
    NvU32 SymbolTableEntrySize = 0;
    NvU32 SymbolTableSize = 0;
    NvU32 SegmentOffset = 0;
    NvU32 FileOffset = 0;
    SegmentNode *node;
#if NV_ENABLE_DEBUG_PRINTS
    // Strings for interpreting ELF header e_type field.
    static const char * s_DynSecTypeText[] =
    {
        "DT_NULL",
        "DT_NEEDED",
        "DT_PLTRELSZ",
        "DT_PLTGOT",
        "DT_HASH",
//        "DT_STRTAB",
        "String Table Address",
//        "DT_SYMTAB",
        "Symbol Table Address",
//        "DT_RELA",
        "Relocation Table Address",
//        "DT_RELASZ",
        "Relocation Table Size",
//        "DT_RELAENT",
        "Relocation Entry Size",
//        "DT_STRSZ",
        "String Table Size",
//        "DT_SYMENT",
        "Symbol Table Entry Size",
        "DT_INIT",
        "DT_FINI",
        "DT_SONAME",
        "DT_RPATH",
        "DT_SYMBOLIC",
//        "DT_REL",
        "Relocation Table Address",
//        "DT_RELSZ",
        "Relocation Table Size",
//        "DT_RELENT",
        "Relocation Entry Size",
        "DT_PLTREL",
        "DT_DEBUG",
        "DT_TEXTREL",
        "DT_JMPREL",
        "DT_BIND_NOW",
        "DT_INIT_ARRAY",
        "DT_FINI_ARRAY",
        "DT_INIT_ARRAYSZ",
        "DT_FINI_ARRAYSZ",
        "DT_RUNPATH",
        "DT_FLAGS",
        "DT_ENCODING",
        "DT_PREINIT_ARRAY",
        "DT_PREINIT_ARRAYSZ",
        "DT_NUM",
        "DT_OS-specific",
        "DT_PROC-specific",
        ""
    };
#else
    #define s_DynSecTypeText ((char**)0)
#endif

    pDynSeg = (Elf32_Dyn*)pSegmentData;
    do
    {
        if (pDynSeg->d_tag < DT_NUM)
        {
            NV_DEBUG_PRINTF(("Entry %d with Tag %s %d\r\n",
                Counter++, s_DynSecTypeText[pDynSeg->d_tag], pDynSeg->d_val));
        }
        else
        {
            NV_DEBUG_PRINTF(("Entry %d Special Compatibility Range %x %d\r\n",
                Counter++, pDynSeg->d_tag, pDynSeg->d_val));
        }
        if (pDynSeg->d_tag == DT_NULL)
            break;
        if ((pDynSeg->d_tag == DT_REL) || (pDynSeg->d_tag == DT_RELA))
            RelocationTableAddressOffset = pDynSeg->d_val;
        if ((pDynSeg->d_tag == DT_RELENT) || (pDynSeg->d_tag == DT_RELAENT))
            RelocationEntrySize = pDynSeg->d_val;
        if ((pDynSeg->d_tag == DT_RELSZ) || (pDynSeg->d_tag == DT_RELASZ))
            RelocationTableSize = pDynSeg->d_val;
        if (pDynSeg->d_tag == DT_SYMTAB)
            SymbolTableAddressOffset = pDynSeg->d_val;
        if (pDynSeg->d_tag == DT_SYMENT)
            SymbolTableEntrySize = pDynSeg->d_val;
        if (pDynSeg->d_tag == DT_ARM_RESERVED1)
            SymbolTableSize = pDynSeg->d_val;
        pDynSeg++;

    }while ((Counter*sizeof(Elf32_Dyn)) < SegmentSize);

    if (RelocationTableAddressOffset && RelocationTableSize && RelocationEntrySize)
    {
        RelocationTablePtr = (const Elf32_Rel*)&pSegmentData[RelocationTableAddressOffset];

        for (Counter = 0; Counter < (RelocationTableSize/RelocationEntrySize); Counter++)
        {
            //calculate the actual offset of the reloc entry
            NV_DEBUG_PRINTF(("Reloc %d offset is %x RType %d SymIdx %d \r\n",
                Counter, RelocationTablePtr->r_offset,
                ELF32_R_TYPE(RelocationTablePtr->r_info),
                ELF32_R_SYM(RelocationTablePtr->r_info)));

            node = pList;
            while (node != NULL)
            {
                if ( (RelocationTablePtr->r_offset > node->VirtualAddr) &&
                     (RelocationTablePtr->r_offset <=
                      (node->VirtualAddr + node->MemorySize)))
                {
                    FileOffset = node->FileOffset +
                                 (RelocationTablePtr->r_offset - node->VirtualAddr);

                    NV_DEBUG_PRINTF(("File offset to be relocated %d \r\n", FileOffset));

                    SegmentOffset = (RelocationTablePtr->r_offset - node->VirtualAddr);

                    NV_DEBUG_PRINTF(("Segment offset to be relocated %d \r\n", SegmentOffset));

                    Error = ApplyRelocation(pList, FileOffset, SegmentOffset,
                                            node->pLoadRegion, RelocationTablePtr);

                }
                node = node->Next;
            }
            RelocationTablePtr++;
        }

    }
    if (SymbolTableAddressOffset && SymbolTableSize && SymbolTableEntrySize)
    {
#if 0
        const Elf32_Sym* SymbolTablePtr = NULL;
        SymbolTablePtr = (const Elf32_Sym*)&pSegmentData[SymbolTableAddressOffset];
        for (Counter = 0; Counter <SymbolTableSize/SymbolTableEntrySize; Counter++)
        {

            NV_DEBUG_PRINTF(("Symbol name %x, value %x, size %x, info %x, other %x, shndx %x\r\n",
                SymbolTablePtr->st_name, SymbolTablePtr->st_value,
                SymbolTablePtr->st_size, SymbolTablePtr->st_info,
                SymbolTablePtr->st_other, SymbolTablePtr->st_shndx));

            SymbolTablePtr++;
        }
#endif
    }
    return Error;
}

NvError
LoadLoadableProgramSegment(NvOsFileHandle elfSourceHandle,
            NvRmDeviceHandle hDevice,
            NvRmLibraryHandle hLibHandle,
            Elf32_Phdr Phdr,
            Elf32_Ehdr Ehdr,
            const NvRmHeap * Heaps,
            NvU32 NumHeaps,
            NvU32 loop,
            const char *Filename,
            SegmentNode **segmentList)
{
    NvError Error = NvSuccess;
    NvRmMemHandle pLoadRegion = NULL;
    void* pLoadAddress = NULL;
    NvU32 offset = 0;
    NvU32 addr;  // address of pLoadRegion
    size_t bytesRead = 0;

    Error = NvRmMemHandleAlloc(hDevice, Heaps, NumHeaps,
                NV_MAX(16, Phdr.p_align), NvOsMemAttribute_Uncached,
                Phdr.p_memsz, NVRM_MEM_TAG_RM_MISC, 1, &pLoadRegion);

    if (Error != NvSuccess)
    {
        NV_DEBUG_PRINTF(("Memory Allocation %d Failed\r\n",Error));
        pLoadRegion = NULL;
        goto CleanUpExit;
    }
    addr = NvRmMemPin(pLoadRegion);

    Error = NvRmMemMap(pLoadRegion, 0, Phdr.p_memsz,
        NVOS_MEM_READ_WRITE, &pLoadAddress);
    if (Error != NvSuccess)
    {
        pLoadAddress = NULL;
    }

    // This will initialize ZI to zero
    if( pLoadAddress )
    {
        NvOsMemset(pLoadAddress, 0, Phdr.p_memsz);
    }
    else
    {
        NvU8 *tmp = NvOsAlloc( Phdr.p_memsz );
        if( !tmp )
        {
            goto CleanUpExit;
        }
        NvOsMemset( tmp, 0, Phdr.p_memsz );
        NvRmMemWrite( pLoadRegion, 0, tmp, Phdr.p_memsz );
        NvOsFree( tmp );
    }

    if(Phdr.p_filesz)
    {
        if( pLoadAddress )
        {
            if ((Error = NvOsFread(elfSourceHandle, pLoadAddress,
                               Phdr.p_filesz, &bytesRead)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
                goto CleanUpExit;
            }
        }
        else
        {
            NvU8 *tmp = NvOsAlloc( Phdr.p_filesz );
            if( !tmp )
            {
                goto CleanUpExit;
            }

            Error = NvOsFread( elfSourceHandle, tmp, Phdr.p_filesz,
                &bytesRead );
            if( Error != NvSuccess )
            {
                NvOsFree( tmp );
                goto CleanUpExit;
            }

            NvRmMemWrite( pLoadRegion, 0, tmp, Phdr.p_filesz );

            NvOsFree( tmp );
        }
    }
    if ((Ehdr.e_entry >= Phdr.p_vaddr)
        && (Ehdr.e_entry < (Phdr.p_vaddr + Phdr.p_memsz)))
    {
        // Odd address indicates entry point is Thumb code.
        // The address needs to be masked with LSB before being invoked.
        offset = (addr - Phdr.p_vaddr) | ADD_MASK;
        hLibHandle->EntryAddress = (void *)(Ehdr.e_entry + offset);
        NvOsDebugPrintf("%s segment %d: load at %x, entry %x\r\n",
                        Filename, loop, addr, hLibHandle->EntryAddress);
    }

    *segmentList = AddToSegmentList((*segmentList), pLoadRegion, Phdr, loop,
        addr, pLoadAddress);

CleanUpExit:
    if (Error != NvSuccess)
    {
        if(pLoadRegion != NULL)
        {
            if( pLoadAddress )
            {
                NvRmMemUnmap(pLoadRegion, pLoadAddress, Phdr.p_memsz);
            }

            NvRmMemUnpin(pLoadRegion);
            NvRmMemHandleFree(pLoadRegion);
        }
    }
    return Error;
}

static const char **NvRmPrivGetLibrarySearchPaths(void)
{
    static const char *AndroidPaths[] = { "/system/bin/", "/sbin/", "/etc/firmware", NULL };
    static const char *LinuxPaths[] = { "/etc/avp/", "./", NULL };
    static const char *WindowsPaths[] = { "", "\\", NULL };
    NvOsOsInfo OsInfo;

    if (NvOsGetOsInformation(&OsInfo)==NvSuccess &&
        OsInfo.OsType == NvOsOs_Linux)
    {
        if (OsInfo.Sku == NvOsSku_Android)
            return AndroidPaths;
        else
            return LinuxPaths;
    }

    return WindowsPaths;    // Actually assume AOS under RVDS
}

static NvError NvRmPrivLibraryFopen(
    const char *Filename,
    NvU32 Flags,
    NvOsFileHandle *File)
{
    NvError e = NvError_BadParameter;
    char *Path = NULL;
    const char **SearchPaths = NvRmPrivGetLibrarySearchPaths();

    while (e!=NvSuccess && *SearchPaths)
    {
        NvU32 PathSize = NvOsStrlen(*SearchPaths);
        NvU32 FileSize = NvOsStrlen(Filename);
        Path = NvOsAlloc(PathSize + FileSize + 1);

        if (!Path)
            return NvError_InsufficientMemory;

        Path[PathSize + FileSize] = 0;
        NvOsMemcpy(Path, *SearchPaths, PathSize);
        NvOsMemcpy(Path+PathSize, Filename, FileSize);
        e = NvOsFopen(Path, Flags, File);
        NvOsFree(Path);
        SearchPaths++;
    }

    return e;
}

static NvRmHeap NvRmModuleGetDefaultHeap(NvRmDeviceHandle hDevice)
{
    NvError Error = NvSuccess;
    static NvU32 fakeCap;
    NvU32 *pFakeCap;
    NvRmModuleCapability caps[] =
    {
        {1, 1, 0, NvRmModulePlatform_Silicon, (void *)&fakeCap},
    };

    Error = NvRmModuleGetCapabilities(hDevice, NvRmPrivModuleID_Gart,
                caps, 1, (void **)&pFakeCap);

    if (Error == NvSuccess)
    {
        /* for AP20 and earlier, use carveout */
        return NvRmHeap_ExternalCarveOut;
    } else {
        /* use SMMU with GART designator on t30+ */
        return NvRmHeap_GART;
    }
}

NvError
NvRmPrivLoadLibrary(NvRmDeviceHandle hDevice,
            const char *Filename,
            NvU32 Address,
            NvBool IsApproachGreedy,
            NvRmLibraryHandle *hLibHandle)
{
    NvError Error = NvSuccess;
    NvOsFileHandle elfSourceHandle = 0;
    size_t bytesRead = 0;
    Elf32_Ehdr elf;
    Elf32_Phdr progHeader;
    NvU32 loop = 0;
    char *dynamicSegementBuffer = NULL;
    int dynamicSegmentOffset = 0;
    int lastFileOffset = 0;
    SegmentNode *segmentList = NULL;
    NvRmHeap HeapProperty[2];
    NvRmHeap defaultHeapProperty;
    NvU32 HeapSize = 0;

    NV_ASSERT(NULL != Filename);
    *hLibHandle = NULL;

    defaultHeapProperty = NvRmModuleGetDefaultHeap(hDevice);

    if ((Error = NvRmPrivLibraryFopen(Filename, NVOS_OPEN_READ,
                           &elfSourceHandle)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("Elf source file Not found Error = %d\r\n", Error));
        NvOsDebugPrintf("\r\nFailed to load library %s, NvError=%d."
            " Make sure it is present on the device\r\n", Filename, Error);
        return Error;
    }
    if ((Error = NvOsFread(elfSourceHandle, &elf,
                           sizeof(elf), &bytesRead)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("File Read size mismatch %d\r\n", bytesRead));
        goto CleanUpExit;
    }
    // Parse the elf headers and display information
    parseElfHeader(&elf);
    /// Parse the Program Segment Headers and display information
    if ((Error = parseProgramSegmentHeaders(elfSourceHandle, elf.e_phoff, elf.e_phnum)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("parseProgramSegmentHeaders failed %d\r\n", Error));
        goto CleanUpExit;
    }
    /// Parse the section Headers and display information
    if ((Error = parseSectionHeaders(elfSourceHandle, &elf)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("parseSectionHeaders failed %d\r\n", Error));
        goto CleanUpExit;
    }
    // allocate memory for handle....
    *hLibHandle = NvOsAlloc(sizeof(NvRmLibHandle));
    if (!*hLibHandle)
    {
        Error = NvError_InsufficientMemory;
        goto CleanUpExit;
    }

    if (elf.e_phnum && elf.e_phnum < MIN_SEGMENTS_FOR_DYNAMIC_LOADING)
    {
        if ((Error = loadSegmentsInFixedMemory(elfSourceHandle,
                                               &elf, 0, &(*hLibHandle)->pLibBaseAddress)) != NvSuccess)
        {
            NV_DEBUG_PRINTF(("LoadSegmentsInFixedMemory Failed %d\r\n", Error));
            goto CleanUpExit;
        }
        (*hLibHandle)->EntryAddress = (*hLibHandle)->pLibBaseAddress;
        return Error;
    }
    else if (elf.e_phnum)
    {
        if ((Error = NvOsFseek(elfSourceHandle,
                               elf.e_phoff, NvOsSeek_Set)) != NvSuccess)
        {
            NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
            goto CleanUpExit;
        }
        lastFileOffset = elf.e_phoff;
        // load the IRAM mandatory  and DRAM mandatory sections first...
        for (loop = 0; loop < elf.e_phnum; loop++)
        {
            if ((Error = NvOsFread(elfSourceHandle, &progHeader,
                                   sizeof(Elf32_Phdr), &bytesRead)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
                goto CleanUpExit;
            }
            lastFileOffset += bytesRead;
            if (progHeader.p_type == PT_LOAD)
            {
                NV_DEBUG_PRINTF(("Found load segment %d\r\n",loop));
                if ((Error = NvOsFseek(elfSourceHandle,
                                       progHeader.p_offset, NvOsSeek_Set)) != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
                    goto CleanUpExit;
                }
                if (progHeader.p_vaddr >= DRAM_MAND_ADDRESS && progHeader.p_vaddr < IRAM_PREF_EXT_ADDRESS)
                {
                    if (progHeader.p_vaddr >= DRAM_MAND_ADDRESS && progHeader.p_vaddr < IRAM_MAND_ADDRESS)
                    {
                        HeapProperty[0] = defaultHeapProperty;
                    }
                    else if (progHeader.p_vaddr >= IRAM_MAND_ADDRESS)
                    {
                        HeapProperty[0] = NvRmHeap_IRam;
                    }
                    Error = LoadLoadableProgramSegment(elfSourceHandle, hDevice, (*hLibHandle),
                                                                        progHeader, elf, HeapProperty, 1, loop,
                                                                        Filename, &segmentList);
                    if (Error != NvSuccess)
                    {
                        NV_DEBUG_PRINTF(("Unable to load segment %d \r\n", loop));
                        goto CleanUpExit;
                    }
                }
                if ((Error = NvOsFseek(elfSourceHandle,
                                       lastFileOffset, NvOsSeek_Set)) != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
                    goto CleanUpExit;
                }
            }
        }

        // now load the preferred and dynamic sections
        if ((Error = NvOsFseek(elfSourceHandle,
                               elf.e_phoff, NvOsSeek_Set)) != NvSuccess)
        {
            NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
            goto CleanUpExit;
        }
        lastFileOffset = elf.e_phoff;
        for (loop = 0; loop < elf.e_phnum; loop++)
        {
            if ((Error = NvOsFread(elfSourceHandle, &progHeader,
                                   sizeof(Elf32_Phdr), &bytesRead)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
                goto CleanUpExit;
            }
            lastFileOffset += bytesRead;
            if (progHeader.p_type == PT_LOAD)
            {
                NV_DEBUG_PRINTF(("Found load segment %d\r\n",loop));
                if ((Error = NvOsFseek(elfSourceHandle,
                                       progHeader.p_offset, NvOsSeek_Set)) != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
                    goto CleanUpExit;
                }
                 if (progHeader.p_vaddr < DRAM_MAND_ADDRESS)
                 {
                    if (IsApproachGreedy == NV_FALSE)
                    {
                        HeapSize = 1;
                        //conservative allocation - IRAM_PREF sections in DRAM.
                        HeapProperty[0] = defaultHeapProperty;
                    }
                    else
                    {
                        // greedy allocation - IRAM_PREF sections in IRAM, otherwise fallback to DRAM
                        HeapSize = 2;
                        HeapProperty[0] = NvRmHeap_IRam;
                        HeapProperty[1] = defaultHeapProperty;
                    }
                    Error = LoadLoadableProgramSegment(elfSourceHandle, hDevice,
                                                                        (*hLibHandle), progHeader, elf,
                                                                        HeapProperty, HeapSize, loop,
                                                                        Filename, &segmentList);
                    if (Error != NvSuccess)
                    {
                        NV_DEBUG_PRINTF(("Unable to load segment %d \r\n", loop));
                        goto CleanUpExit;
                    }
                }
                else if (progHeader.p_vaddr >= IRAM_PREF_EXT_ADDRESS)
                {
                    if (IsApproachGreedy == NV_FALSE)
                    {
                        HeapSize = 1;
                        //conservative allocation - IRAM_PREF sections in DRAM.
                        HeapProperty[0] = defaultHeapProperty;
                    }
                    else
                    {
                        // greedy allocation - IRAM_PREF sections in IRAM, otherwise fallback to DRAM
                        HeapSize = 2;
                        HeapProperty[0] = NvRmHeap_IRam;
                        HeapProperty[1] = defaultHeapProperty;
                    }
                    Error = LoadLoadableProgramSegment(elfSourceHandle, hDevice,
                                                                        (*hLibHandle), progHeader, elf,
                                                                        HeapProperty, HeapSize, loop,
                                                                        Filename, &segmentList);
                    if (Error != NvSuccess)
                    {
                        NV_DEBUG_PRINTF(("Unable to load segment %d \r\n", loop));
                        goto CleanUpExit;
                    }
                }
                if ((Error = NvOsFseek(elfSourceHandle,
                                    lastFileOffset, NvOsSeek_Set)) != NvSuccess)
                {
                 NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
                 goto CleanUpExit;
                }
            }
            if (progHeader.p_type != PT_DYNAMIC)
                continue;
            dynamicSegmentOffset = progHeader.p_offset;
            if ((Error = NvOsFseek(elfSourceHandle,
                                   dynamicSegmentOffset, NvOsSeek_Set)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
                goto CleanUpExit;
            }
            dynamicSegementBuffer = NvOsAlloc(progHeader.p_filesz);
            if (dynamicSegementBuffer == NULL)
            {
                NV_DEBUG_PRINTF(("Memory Allocation %d Failed\r\n", progHeader.p_filesz));
                goto CleanUpExit;
            }
            if ((Error = NvOsFread(elfSourceHandle, dynamicSegementBuffer,
                                   progHeader.p_filesz, &bytesRead)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
                goto CleanUpExit;
            }
            if ((Error = ParseDynamicSegment(
                                            segmentList,
                                            dynamicSegementBuffer,
                                            progHeader.p_filesz,
                                            dynamicSegmentOffset)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Parsing and relocation of segment failed \r\n"));
                goto CleanUpExit;
            }
            (*hLibHandle)->pList = segmentList;
            if ((Error = NvOsFseek(elfSourceHandle,
                                lastFileOffset, NvOsSeek_Set)) != NvSuccess)
            {
             NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
             goto CleanUpExit;
            }
        }
    }

    CleanUpExit:
    {
        if (Error == NvSuccess)
        {
            UnMapRegion(segmentList);
            NvOsFree(dynamicSegementBuffer);
            NvOsFclose(elfSourceHandle);
        }
        else
        {
            RemoveRegion(segmentList);
            NvOsFree(dynamicSegementBuffer);
            NvOsFclose(elfSourceHandle);
            NvOsFree(*hLibHandle);
        }
    }
    return Error;
}

NvError NvRmPrivFreeLibrary(NvRmLibHandle *hLibHandle)
{
    NvError Error = NvSuccess;
    RemoveRegion(hLibHandle->pList);
    NvOsFree(hLibHandle);
    return Error;
}

 void parseElfHeader(Elf32_Ehdr *elf)
{
    if (elf->e_ident[0] == ELF_MAG0)
    {
        NV_DEBUG_PRINTF(("File is elf Object File with Identification %c%c%c\r\n",
            elf->e_ident[1], elf->e_ident[2], elf->e_ident[3]));
        NV_DEBUG_PRINTF(("Type of ELF is %x\r\n", elf->e_type));
        //An object file conforming to this specification must have
        //the value EM_ARM (40, 0x28).
        NV_DEBUG_PRINTF(("Machine type of the file is %x\r\n", elf->e_machine));
        //Address of entry point for this file. bit 1:0
        //indicates if entry point is ARM or thum mode
        NV_DEBUG_PRINTF(("Entry point for this axf is %x\r\n", elf->e_entry));
        NV_DEBUG_PRINTF(("Version of the ELF is %d\r\n", elf->e_version));
        NV_DEBUG_PRINTF(("Program Table Header Offset %d\r\n", elf->e_phoff));
        NV_DEBUG_PRINTF(("Section Table Header Offset %d\r\n", elf->e_shoff));
        NV_DEBUG_PRINTF(("Elf Header size %d\r\n", elf->e_ehsize));
        NV_DEBUG_PRINTF(("Section Header's Size %d\r\n", elf->e_shentsize));
        NV_DEBUG_PRINTF(("Number of Section Headers %d\r\n", elf->e_shnum));
        NV_DEBUG_PRINTF(("String Table Section Header Index %d\r\n", elf->e_shstrndx));
        NV_DEBUG_PRINTF(("\r\n"));
    }
}

NvError parseProgramSegmentHeaders(NvOsFileHandle elfSourceHandle,
                                   NvU32 segmentHeaderOffset,
                                   NvU32 segmentCount)
{
    Elf32_Phdr progHeader;
    size_t bytesRead = 0;
    NvU32 loop = 0;
    NvError Error = NvSuccess;
    if (segmentCount)
    {
        NV_DEBUG_PRINTF(("Program Headers Found %d\r\n", segmentCount));
        if ((Error = NvOsFseek(elfSourceHandle,
                               segmentHeaderOffset, NvOsSeek_Set)) != NvSuccess)
        {
            NV_DEBUG_PRINTF(("File Seek failed %d\r\n", Error));
            return Error;
        }

        for (loop = 0; loop < segmentCount; loop++)
        {
            if ((Error = NvOsFread(elfSourceHandle, &progHeader,
                                   sizeof(progHeader), &bytesRead)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
                return Error;
            }

            NV_DEBUG_PRINTF(("Program %d Header type %d\r\n",
                loop, progHeader.p_type));
            NV_DEBUG_PRINTF(("Program %d Header offset %d\r\n",
                loop, progHeader.p_offset));
            NV_DEBUG_PRINTF(("Program %d Header Virtual Address %x\r\n",
                loop, progHeader.p_vaddr));
            NV_DEBUG_PRINTF(("Program %d Header Physical Address %x\r\n",
                loop, progHeader.p_paddr));
            NV_DEBUG_PRINTF(("Program %d Header Filesize %d\r\n",
                loop, progHeader.p_filesz));
            NV_DEBUG_PRINTF(("Program %d Header Memory Size %d\r\n",
                loop, progHeader.p_memsz));
            NV_DEBUG_PRINTF(("Program %d Header Flags %x\r\n",
                loop, progHeader.p_flags));
            NV_DEBUG_PRINTF(("Program %d Header alignment %d\r\n",
                loop, progHeader.p_align));
            NV_DEBUG_PRINTF(("\r\n"));
        }
    }
    return NvSuccess;
}

NvError
parseSectionHeaders(NvOsFileHandle elfSourceHandle, Elf32_Ehdr *elf)
{
    NvError Error = NvSuccess;
    NvU32 stringTableOffset = 0;
    Elf32_Shdr sectionHeader;
    size_t bytesRead = 0;
    NvU32 loop = 0;
    char* stringTable = NULL;
    const char *specialNamePtr = NULL;

    // Try to get to the string table so that we can get section names
    stringTableOffset = elf->e_shoff + (elf->e_shentsize * elf->e_shstrndx);

    NV_DEBUG_PRINTF(("String Table File Offset is %d\r\n", stringTableOffset));

    if ((Error = NvOsFseek(elfSourceHandle,
                           stringTableOffset, NvOsSeek_Set)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
        return Error;
    }
    if ((Error = NvOsFread(elfSourceHandle, &sectionHeader,
                           sizeof(sectionHeader), &bytesRead)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
        return Error;
    }
    if (sectionHeader.sh_type == SHT_STRTAB)
    {
        NV_DEBUG_PRINTF(("Found Section is string Table\r\n"));
        if (sectionHeader.sh_size)
        {
            stringTable = NvOsAlloc(sectionHeader.sh_size);
            if (stringTable == NULL)
            {
                NV_DEBUG_PRINTF(("String table mem allocation failed for %d\r\n",
                    sectionHeader.sh_size));
                return  NvError_InsufficientMemory;
            }
            if ((Error = NvOsFseek(elfSourceHandle,
                                   sectionHeader.sh_offset, NvOsSeek_Set)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
                goto CleanUpExit_parseSectionHeaders;
            }
            if ((Error = NvOsFread(elfSourceHandle, stringTable,
                                   sectionHeader.sh_size, &bytesRead)) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
                goto CleanUpExit_parseSectionHeaders;
            }
        }
    }
    if ((Error = NvOsFseek(elfSourceHandle,
                           elf->e_shoff, NvOsSeek_Set)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("File Seek failed %d\r\n", bytesRead));
        goto CleanUpExit_parseSectionHeaders;
    }
    for (loop = 0; loop < elf->e_shnum; loop++)
    {
        if ((Error = NvOsFread(elfSourceHandle, &sectionHeader,
                               sizeof(sectionHeader), &bytesRead)) != NvSuccess)
        {
            NV_DEBUG_PRINTF(("File Read failed %d\r\n", bytesRead));
            goto CleanUpExit_parseSectionHeaders;
        }

        NV_DEBUG_PRINTF(("Section %d is named %s\r\n",
            loop, &stringTable[sectionHeader.sh_name]));
        NV_DEBUG_PRINTF(("Section %d Type %d\r\n",
            loop, sectionHeader.sh_type));
        NV_DEBUG_PRINTF(("Section %d Flags %x\r\n",
            loop, sectionHeader.sh_flags));

        GetSpecialSectionName(sectionHeader.sh_type,
                              sectionHeader.sh_flags, &specialNamePtr);

        NV_DEBUG_PRINTF(("Section %d Special Name is %s",
            loop, specialNamePtr));
        NV_DEBUG_PRINTF(("Section %d Address %x\r\n",
            loop, sectionHeader.sh_addr));
        NV_DEBUG_PRINTF(("Section %d File Offset %d\r\n",
            loop, sectionHeader.sh_offset));
        NV_DEBUG_PRINTF(("Section %d Size %d \r\n",
            loop, sectionHeader.sh_size));
        NV_DEBUG_PRINTF(("Section %d Link %d \r\n",
            loop, sectionHeader.sh_link));
        NV_DEBUG_PRINTF(("Section %d Info %d\r\n",
            loop, sectionHeader.sh_info));
        NV_DEBUG_PRINTF(("Section %d alignment %d\r\n",
            loop, sectionHeader.sh_addralign));
        NV_DEBUG_PRINTF(("Section %d Fixed Entry Size %d\r\n",
            loop, sectionHeader.sh_entsize));
        NV_DEBUG_PRINTF(("\r\n"));

    }
    CleanUpExit_parseSectionHeaders:
    if (stringTable)
        NvOsFree(stringTable);
    return Error;
}


NvError
loadSegmentsInFixedMemory(NvOsFileHandle elfSourceHandle,
                          Elf32_Ehdr *elf, NvU32 segmentIndex, void **loadaddress)
{
    NvError Error = NvSuccess;
    size_t bytesRead = 0;
    Elf32_Phdr progHeader;

    if ((Error = NvOsFseek(elfSourceHandle,
                           elf->e_phoff + (segmentIndex * sizeof(progHeader)), NvOsSeek_Set)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("loadSegmentsInFixedMemory File Seek failed %d\r\n", bytesRead));
        return Error;
    }

    if ((Error = NvOsFread(elfSourceHandle, &progHeader,
                           sizeof(Elf32_Phdr), &bytesRead)) != NvSuccess)
    {
        NV_DEBUG_PRINTF((" loadSegmentsInFixedMemory File Read failed %d\r\n", bytesRead));
        return Error;
    }
    NV_ASSERT(progHeader.p_type == PT_LOAD);
    if ((Error = NvOsFseek(elfSourceHandle,
                           progHeader.p_offset, NvOsSeek_Set)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("loadSegmentsInFixedMemory File Seek failed %d\r\n", Error));
        return Error;
    }
    if((Error = NvRmPhysicalMemMap(progHeader.p_vaddr,
            progHeader.p_memsz, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached, loadaddress)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("loadSegmentsInFixedMemory Failed trying to Mem Map %x\r\n", progHeader.p_vaddr));
        return Error;
    }
    // This will initialize ZI to zero
    NvOsMemset(*loadaddress, 0, progHeader.p_memsz);
    if ((Error = NvOsFread(elfSourceHandle, *loadaddress,
                           progHeader.p_filesz, &bytesRead)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("loadSegmentsInFixedMemory File Read failed %d\r\n", bytesRead));
        return Error;
    }
    // Load address need to be reset to the physical address as this is passed to the AVP as the entry point.
    *loadaddress = (void *)progHeader.p_vaddr;

    return Error;
}

NvError NvRmPrivGetProcAddress(NvRmLibraryHandle Handle,
                    const char *pSymbol,
                    void **pSymAddress)
{
    NvError Error = NvSuccess;
    // In phase 1, this API will just return the load address as entry address
    NvRmLibHandle *hHandle = Handle;

    //NOTE: The EntryAddress is pointing to a THUMB function
    //(LSB is set). The Entry Function must be in THUMB mode.
    if (hHandle->EntryAddress != NULL)
    {
        *pSymAddress = hHandle->EntryAddress;
    }
    else
    {
        Error = NvError_SymbolNotFound;
    }
    return Error;
}
