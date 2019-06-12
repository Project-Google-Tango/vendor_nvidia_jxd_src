/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "tridentexample.h"
#include "trident.hpp"
#include "nvos.h"

//---------------------------------------------------------------------------
// SAMPLE APPLICATION CODE
//---------------------------------------------------------------------------

class MyCPUStage : public CPUStage
{
public:
    MyCPUStage() {}
    ~MyCPUStage() {}

    void setup (const MetaState& metaState) {
        m_mySwapBuffer = getBuffer (m_mySwapHandle);
        m_myRGBColor[0] = m_myShade * metaState.exposure;
        m_myRGBColor[1] = m_myRGBColor[0];
        m_myRGBColor[2] = m_myRGBColor[0];
    }

    void execute()  {
        // do stuff
//         Buffer* pOutBuffer = getModule()->getBuffer(IO_BUFFER_HANDLE);
//         void*   pOutData = pOutBuffer->pData;
//         for each pixel, for each component, pData = m_myRGBColor[i];
    }

    TridentBufferHandle m_mySwapHandle;
    TridentBuffer* m_mySwapBuffer;
    int m_myShade;
    int m_myRGBColor[3];
};

class MyOGLStage : public OGLStage
{
public:
    MyOGLStage() {}
    ~MyOGLStage() {}

    void setup (const MetaState& metaState) {}
};

class MyCPUModule : public Module
{
public:
    MyCPUModule() {
        TridentBufferHandle mySwapHandle[1];
        registerSwapBuffers (mySwapHandle, 1);
        m_myCPUStage.m_mySwapHandle = mySwapHandle[0];
        m_myCPUStage.m_myShade = 0.5;
        setStage (0, &m_myCPUStage);
        setOutputBuffer (IO_BUFFER_HANDLE);
    }
    ~MyCPUModule() {}

    virtual void errorHandler (int errorCode) {
        NvOsDebugPrintf ("oh no we got an error!");
    }

    MyCPUStage m_myCPUStage;
};

class MyOGLModule : public Module
{
public:
    MyOGLModule() {
        setStage (0, &myOGLStage);
        setOutputBuffer (IO_BUFFER_HANDLE);
    }
    ~MyOGLModule() {}

    virtual void errorHandler(int errorCode) {
        NvOsDebugPrintf ("oh no we got an error!");
    }

    MyOGLStage myOGLStage;
};

class MyPreISPDomain : public Domain
{
public:
    MyPreISPDomain() {
        setModule (0, &m_myCPUModule);
        setModule (1, &m_myOGLModule);
    }
    ~MyPreISPDomain() {}

    MyCPUModule m_myCPUModule;
    MyOGLModule m_myOGLModule;
};


class MyPipeline : public Pipeline
{
public:
    MyPipeline(NvMMBlockHandle blockHandle) : Pipeline(blockHandle) {
        setDomainPreISP (&m_myPreISPDomain);
//        setDomainPostISP (&myPostISPDomain);
    }
    ~MyPipeline() {}

    virtual void errorHandler (int errorCode) {
        NvOsDebugPrintf ("oh no more errors!");
    }

    MyPreISPDomain m_myPreISPDomain;
};


// call via a backdoor in nvcamerasettings.cpp (scenemode == sunset)

void TridentDemo(NvMMBlockHandle blockHandle)
{
    NvOsDebugPrintf ("hello from TridentDemo");

    MyPipeline myPipeline(blockHandle);
    Launch (&myPipeline);
    // vamp
    Launch (NULL);  // stop the pipe
}

