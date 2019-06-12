
/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NVBUFFER_HW_ALLOCATOR_TEGRA_H
#define NVBUFFER_HW_ALLOCATOR_TEGRA_H

#include <cutils/properties.h>
#include "nvbuffer_buffer_info.h"
#include "nvbuffer_manager_common.h"
#include "nvbuffer_stream.h"
#include "nvbuffer_driver_interface.h"
#include "nvddk_2d_v2.h"

// To enable memory profile print, we need to:
// 1. adb shell setprop camera-memory-profile 1
//  to enable profile print output
// 2. adb shell setprop enableLogs 3
//  to enable ALOGD

#define MEM_PROFILE_TAG "Memory Allocation"

/**
* Class TegraBufferConfig:
* This class implements the NvBufferConfigurator interface, with
* calls to the appropriate NvMMBuffer blocks.  For details of the
* public methods see NvBufferConfigurator definition.
**/
class TegraBufferConfig : public NvBufferConfigurator
{
public:
    TegraBufferConfig(NvCameraDriverInfo const &driverInfo);

    NvError GetOutputRequirements(NvBufferManagerComponent component,
                                  NvComponentBufferConfig *pStreamBuffCfg);
    // this will receive buffer requirements and return buffer configuration
    NvError GetOutputConfiguration(NvBufferManagerComponent component,
                                   NvComponentBufferConfig *pStreamBuffCfg);
    // this will set input requirements on a component after
    // the output requirements have been set
    NvError GetInputRequirement(NvBufferManagerComponent component,
                                NvComponentBufferConfig *pStreamBuffCfg);
    NvError ConfigureDrivers();

private:
    NvError GetDZRequirements(NvComponentBufferConfig *pStreamBuffCfg);
    NvError GetDzCfgFromRequirement(NvComponentBufferConfig *pStreamBuffCfg);
    NvError DZPreviewBufferConfig(
                            NvMMNewBufferRequirementsInfo *pReq,
                            NvMMNewBufferConfigurationInfo *pCfg);
    NvError UpdateDzWithReqs(NvMMNewBufferRequirementsInfo *pReq,NvU32 port);
    NvError UpdateDzWithConfig(NvMMNewBufferConfigurationInfo *pCfg,NvU32 port);
    NvError ReplyToDZBufferConfig(NvU32 port);
    NvError GetCaptureCfgAndReq(NvComponentBufferConfig *pStreamBuffCfg);
    NvRmSurfaceLayout GetUserSetSurfaceLayout(void);
    NvError PopulateDZSurfaceFormatInfo(
                NvMMNewBufferRequirementsInfo *pReq,
                NvU32 port);

    TransferBufferFunction DZTransferBufferToBlockFunct;


};

/**
* Class TegraBufferAllocator:
* This class implements the NvBufferAllocator interface, with
* calls to the appropriate NvMMBuffer blocks.  For details of the
* public methods see NvBufferAllocator definition
**/
class TegraBufferAllocator : public NvBufferAllocator
{

public:
    TegraBufferAllocator(NvCameraDriverInfo const &driverInfo);
    ~TegraBufferAllocator();
    NvError AllocateBuffer(NvBufferOutputLocation location,
                            const NvMMNewBufferConfigurationInfo *bufferCfg,
                            NvMMBuffer **buffer);
    NvError SetBufferCfg(NvBufferOutputLocation location,
                            const NvMMNewBufferConfigurationInfo *bufferCfg,
                            NvMMBuffer *pBuffer);
    NvError FreeBuffer(NvBufferOutputLocation location, NvMMBuffer *buffer);
    NvError Initialize();
    NvBool  RepurposeBuffers(NvBufferOutputLocation location,
                             const NvMMNewBufferConfigurationInfo& originalCfg,
                             const NvMMNewBufferConfigurationInfo& newCfg,
                             NvOuputPortBuffers *buffers,
                             NvU32 bufferCount);
private:
    NvError InitializeANWBuffer(const NvMMNewBufferConfigurationInfo *bufferCfg,
                                NvMMBuffer *pBuffer,
                                NvBool Allocate);
    NvError InitializeDZOutputBuffer(const NvMMNewBufferConfigurationInfo *pBufCfg,
                                     NvU32 StreamIndex,
                                     NvMMBuffer *pBuffer,
                                     NvBool Allocate);
    NvError InitializeCamOutputBuffer(const NvMMNewBufferConfigurationInfo *pBufCfg,
                                      NvU32 StreamIndex,
                                      NvMMBuffer *pBuffer,
                                      NvBool Allocate);

    NvError CleanupANWBuffer(NvMMBuffer *pBuffer);
    NvError CleanupDZBuffer(NvMMBuffer *pBuffer);
    NvError CleanupCamBuffer(NvMMBuffer *pBuffer);
    NvError AllocateDZSurfaces(NvMMSurfaceDescriptor *pSurfaceDesc,
                               NvU16 byteAlignment);
    NvError MemAllocDZ(NvRmMemHandle* phMem,
                       NvU32 Size,
                       NvU32 Alignment,
                       NvU32 *pPhysicalAddress);
    NvU32 ComputeSurfaceSizePostSwapDZ(NvRmSurface *pSurface);
    NvError CameraAllocateMetadata(NvRmMemHandle *phMem, NvU32 size);
    NvError CameraBlockAllocSurfaces(NvMMSurfaceDescriptor *pSurfaceDesc,
                                     NvBool StillOutputAlignmentNeeded);
    NvError CameraBlockMemAlloc(NvRmMemHandle* phMem,
                                NvU32 Size,
                                NvU32 Alignment,
                                NvU32 *PhysicalAddress);
    NvRmDeviceHandle hRm;
    NvDdk2dHandle    h2d;

    NvBool  MemProfilePrintEnabled;
};

/**
* Class TegraBufferHandler:
* This class implements the NvBufferHandler interface, with
* calls to the appropriate NvMMBuffer blocks.  For details of the
* public methods see NvBufferHandler definition
**/
class TegraBufferHandler : public NvBufferHandler
{

public:
    TegraBufferHandler(NvCameraDriverInfo const &driverInfo);
    NvError GiveBufferToComponent(NvBufferOutputLocation location, NvMMBuffer *buffer);
    NvError ReturnBuffersToManager(NvBufferOutputLocation location);
    ~TegraBufferHandler()  { };

private:
    NvError SendBufferToDZ(NvMMBuffer *buffer,NvU32 port);
    NvError SendBufferToCam(NvMMBuffer *buffer,NvU32 port);
    NvError ReturnBuffersFromDZ(NvU32 port);
    NvError ReturnBuffersFromCam(NvU32 port);

    TransferBufferFunction DZTransferBufferToBlockFunct;
    TransferBufferFunction CamTransferBufferToBlockFunct;


};







#endif //NVBUFFER_HW_ALLOCATOR_NON_TEGRA
