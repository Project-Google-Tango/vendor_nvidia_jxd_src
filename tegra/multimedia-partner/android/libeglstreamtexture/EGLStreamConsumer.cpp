/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "EGLStreamConsumer"
#include <utils/Log.h>

#include "EGLStreamConsumer.h"
#include <dlfcn.h>
#include <ui/GraphicBuffer.h>

#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/IGraphicBufferAlloc.h>
#include <utils/String8.h>
#include <nvmm_buffertype.h>
#include <media/stagefright/foundation/ADebug.h>
#include <NVOMX_IndexExtensions.h>

#define DEBUG_DUMP 0
#if DEBUG_DUMP
FILE *fp = NULL;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif
#include "nvgralloc.h"
#include "nvrm_memmgr.h"

static PFNEGLQUERYSTREAMKHRPROC eglQueryStreamKHR = NULL;

static NvEglApiExports eglExports;

#if defined(__cplusplus)
}
#endif

namespace android {

EGLStreamConsumer::EGLStreamConsumer(EGLStreamKHR stream, EGLDisplay dpy,
                                int nWidth, /* Consumer width */
                                int nHeight /* Consumer height */)
    : mDefaultWidth(nWidth),
      mDefaultHeight(nHeight),
      mPixelFormat(0),
      mSynchronousMode(false),
      mStopped(false),
      mStartTimeNs(0),
      mLatency(0),
      mEglStream(stream),
      mDisplay(dpy),
      mStarted(false),
      m_hRmDev(NULL),
      mWaitSem(NULL),
      mFramesGiven(0),
      mFramesReceivedBack(0)
{
    ALOGV("EGLStreamConsumer::EGLStreamConsumer dpy %d \n", (int)dpy);

    if (EglStreamWrapperInitializeEgl() != OK)
    {
        ALOGV("Failed to initialize EGLStream");
    }
    else
    {

        eglExports = g_EglStreamWrapperExports;
        eglQueryStreamKHR = g_EglStreamWrapperEglQueryStreamKHR;

        pStreamFrame = (NvEglApiStreamFrame*) malloc( sizeof(NvEglApiStreamFrame));
        init(); // memory dealer init..

        if (eglExports.streamConsumerALDataLocatorConnect)
        {
            mWrapper.mType = AL_AS_EGL_CONSUMER;
            mWrapper.mpDataObject = this;
            eglExports.streamConsumerALDataLocatorConnect((NvEglStreamHandle)mEglStream,
                (NvEglApiStreamInfo *)&mWrapper);
            ALOGV("streamConsumerALDataLocatorConnect is done \n");
            start();
        }
        else
        {
            ALOGV("%s[%d] ERROR OUT NOT INIT \n", __FUNCTION__, __LINE__);
        }
    }
}

EGLStreamConsumer::~EGLStreamConsumer() {
    ALOGV("EGLStreamConsumer::~EGLStreamConsumer ++");
#if DEBUG_DUMP
    String8 str;
    ALOGV("dumping now \n");
    dump(str);
    ALOGV("DUMP: \n\n\n %s \n\n\n", str.string());
#endif

    Aframe *p = NULL;
    CHECK(mFramesList.size() == kNumBuffers);
    for( int i=0 ;i < kNumBuffers; i++) {
        ALOGV("Calling deallocate %d ",i);
        p = *mFramesList.begin();
        mFramesList.erase(mFramesList.begin());
        DeAllocateFrameFully(p);
    }

    if (m_hRmDev)
    {
       NvRmClose(m_hRmDev);
       m_hRmDev = NULL;
    }
    if (mWaitSem)
    {
        NvOsSemaphoreDestroy(mWaitSem);
        mWaitSem = NULL;
    }
#if DEBUG_DUMP
    fclose(fp);
#endif
    close();
    ALOGV("EGLStreamConsumer::~EGLStreamConsumer --");
}

void EGLStreamConsumer::close()
{
    if (!mStopped) {
        Mutex::Autolock lock(mMutex);
        mStopped = true;
    }
    EglStreamWrapperClose();
}

void EGLStreamConsumer::dump(String8& result) const
{
    char buffer[1024];
    dump(result, "", buffer, 1024);
}

void EGLStreamConsumer::dump(String8& result, const char* prefix,
        char* buffer, size_t SIZE) const
{
    Mutex::Autolock _l(mMutex);
    return;
}

int32_t EGLStreamConsumer::setEglStreamAttrib(u_int32_t attr, int32_t val)
{
    ALOGI("setEglStreamAttrib attr = %x val =%x", attr, val);
    switch ( attr )
    {
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            mLatency = val;
            break;
        default:
            ALOGI("TO DO Handle %x attrib", attr);
            break;
    }
    ALOGV("EGLStreamConsumer::setEglStreamAttrib--");

    return NvSuccess;
}

int32_t EGLStreamConsumer::eglStreamDisconnect()
{
    ALOGV("eglStreamDisconnect++");

    ALOGV("eglStreamDisconnect--");
    return NvSuccess;
}


// static
void *EGLStreamConsumer::threadWrapper(void *pthis) {
    ALOGV("ThreadWrapper: %p", pthis);
    EGLStreamConsumer *consumer = static_cast<EGLStreamConsumer *>(pthis);
    consumer->readFromSource();
    return NULL;
}


status_t EGLStreamConsumer::start() {
    ALOGV("EGLStreamConsumer Start");
    mStarted = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    int err = pthread_create(&mThread, &attr, threadWrapper, this);
    pthread_attr_destroy(&attr);

    if (err) {
        ALOGV("Error creating thread!");
        return -ENODEV;
    }
    return OK;
}


status_t EGLStreamConsumer::stop() {
    ALOGV("EGLStreamConsumer Stop");
    mStarted = false;

    {
        Mutex::Autolock autoLock(mFilledListMutex);
        mFilledListCondition.signal();
    }
    void *dummy;
    pthread_join(mThread, &dummy);
    status_t err = (status_t) dummy;

    ALOGV("Ending the reading thread");
    return err;
}

void EGLStreamConsumer::readFromSource() {
    ALOGV("EGLStreamConsumer ReadFromSource++");
    if (!mStarted) {
        return;
    }

    if (NvSuccess != NvRmOpen(&m_hRmDev, 0)) {
        ALOGV("EGLStreamConsumer - NvRmOpen FAILED!");
    }
    if (NvSuccess != NvOsSemaphoreCreate(&mWaitSem, 0)) {
        ALOGV("EGLStreamConsumer - NvOsSemaphoreCreate FAILED!");
    }

    NvU8 i = 0;
    NvU32 nFrameCnt = 0;
    status_t err = OK;
    NvError aNvError;

    NvEglApiStreamFrame *newFrame = NULL;
    NvU64  timeout = 0;
    ALOGV("EGLStreamConsumer accessing the frames");
    NvU64 prevTime = 0xFFFF;
    NvMMSurfaceDescriptor SurfaceDesc;
    while (mStarted && err == OK) {
        ALOGV("Get the Frame");
        newFrame = this->pStreamFrame;
        newFrame->time = 0xFFFF;
        // get Frame
        aNvError = eglExports.streamConsumerAcquireFrame((NvEglStreamHandle)mEglStream, newFrame, (1000* 1000/40));

        if(0xFFFF == newFrame->time || prevTime == newFrame->time)
        {
            ALOGV("No Frame present sleeping for 40ms");
            ALOGV("No Frame present sleeping for 40ms prevTime:%llu nf time: %llu", prevTime, newFrame->time);
            usleep(1000 * 1000/25);
            //Query.
            {
                int value = 0;
                bool result = true;
                if (eglQueryStreamKHR)
                result = eglQueryStreamKHR(mDisplay, mEglStream, EGL_STREAM_STATE_KHR, &value);
                if ( !result ) {
                    value = 0;
                    ALOGE("Failed to query stream state");
                }
                else {
                    ALOGV("query stream state: %d", value);
                    //check value
                    if(EGL_STREAM_STATE_DISCONNECTED_KHR == value) {
                        eglExports.streamConsumerReleaseFrame((NvEglStreamHandle)mEglStream, newFrame);
                        mStarted = false;
                        break;
                    }
                }
            }

            continue;
        }
        else{
            prevTime = newFrame->time;
        }
        if(0 == nFrameCnt){
            prevTime = newFrame->time;
        }
        ALOGV("got frame: %d [ newFrame->time: %lld] \n", nFrameCnt,  newFrame->time);

        //wait for fence on Frame
        if (newFrame->fence.SyncPointID != NVRM_INVALID_SYNCPOINT_ID)
        {
            NvRmFence* fence = &newFrame->fence;
            ALOGV("### %s[%d] wait fence id %d value %d \n", __FUNCTION__, __LINE__,
                newFrame->fence.SyncPointID,
                newFrame->fence.Value);
            if(NvSuccess != NvRmChannelSyncPointWaitTimeout(m_hRmDev,
                            fence->SyncPointID,
                            fence->Value,
                            mWaitSem,
                            350)) {
                ALOGI("EGLStreamConsumer:wait fence timedout\n");
            }

            newFrame->fence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
        }
        nFrameCnt++;
        ALOGV("[nFrameCnt:%d] bufCount: %d memId:%d alphaformat:%d colorspace:%d allowedquality:%d \n",
            nFrameCnt, newFrame->info.bufCount, newFrame->info.memId,
            newFrame->info.alphaformat, newFrame->info.colorspace, newFrame->info.allowedquality);

        NvRmMemHandle phMem;
        NvError aErr = NvRmMemHandleFromFd(newFrame->info.memFd, &phMem);
        if(NvSuccess != aErr)
        {
            ALOGV("EGLStreamConsumer - NvRmMemHandleFromFd failed: %d  [ BIG ERROR] \n", aErr);
            //return the Frame..
            aNvError = eglExports.streamConsumerReleaseFrame((NvEglStreamHandle)mEglStream, newFrame);
            continue;
        }

        NvRmSurface *pSurface;
        SurfaceDesc.SurfaceCount = 0;
        SurfaceDesc .Empty = false;
        int width = 0; int height = 0;
        for( i= 0; i < newFrame->info.bufCount; i++)
        {
            pSurface = &newFrame->info.buf[i];
            ALOGV(" Width:%d  Height:%d ColorFormat:%d Layout:%d Pitch:%d  newFrame->info.bufCount:%d \n",
                pSurface->Width, pSurface->Height, pSurface->ColorFormat, pSurface->Layout, pSurface->Pitch, newFrame->info.bufCount);
            width = pSurface->Width;
            height = pSurface->Height;
            pSurface->hMem = phMem;
            SurfaceDesc.SurfaceCount++;
            SurfaceDesc.Surfaces[i] = *pSurface;
        }

        Aframe *pFramePtr  = NULL;
        Mutex::Autolock autoLock(mFreeListMutex);
        if(mFreeFramesList.empty())
        {
            int32_t reltime =  (1000 * 1000/25) * 1000;
            ALOGV(" mFreeListCondition.waitRelative(mFreeListMutex, reltime);  ++\n");
            mFreeListCondition.waitRelative(mFreeListMutex, reltime); //
            ALOGV(" mFreeListCondition.waitRelative(mFreeListMutex, reltime);  -- \n");
        }
        if( !mFreeFramesList.empty()) {

            pFramePtr = *mFreeFramesList.begin();
            // remove from freelist
            mFreeFramesList.erase(mFreeFramesList.begin());
        }

        if(NULL != pFramePtr)
        {
            NvMMBuffer *pMMBuffer = NULL;
            sp<IMemory> frame = pFramePtr->mPtr ;
            void * frameptr = (void *)(frame->pointer());
            pMMBuffer = (NvMMBuffer*)(void*)((OMX_U8 *)(frameptr) + 4 + sizeof(OMX_BUFFERHEADERTYPE) );

            // copy timestampe
            pFramePtr->timestamp = newFrame->time;
            // convert
            m_Converter.Scale(&SurfaceDesc, &pMMBuffer->Payload.Surfaces, NvDdk2dTransform_FlipVertical);
#if DEBUG_DUMP
            //DumpSurfaces(pMMBuffer, NULL);
            DumpSurfaces(NULL, &SurfaceDesc);
#endif
            aNvError = eglExports.streamConsumerReleaseFrame((NvEglStreamHandle)mEglStream, newFrame);

            pFramePtr->status = EGLStreamConsumer::kFILLED;
            {
                ALOGV("EGLStreamConsumer to get mFilledListMutex ++ \n");
                Mutex::Autolock autoLock(mFilledListMutex);
                mFilledFramesList.push_back(pFramePtr);
                ALOGV("EGLStreamConsumer to get mFilledListMutex -- \n");
            }
            mFilledListCondition.signal();
        }
        else {
            ALOGV("EGLStreamConsumer ReadFromSource: Frame(%d) dropped \n", nFrameCnt);
            //return the Frame.
            aNvError = eglExports.streamConsumerReleaseFrame((NvEglStreamHandle)mEglStream, newFrame);
        }
    }

    // Disconnect
    if (eglExports.streamConsumerDisconnect)
    {
        eglExports.streamConsumerDisconnect((NvEglStreamHandle)mEglStream);
    }

    //means .. we have no more frames OR stop is called.
    mStarted = false;
    {
        Mutex::Autolock autoLock(mFilledListMutex);
        mFilledListCondition.signal();
    }
    ALOGV("EGLStreamConsumer ReadFromSource--\n");
}



int EGLStreamConsumer::returnRecordedFrame(sp<IMemory>& frame)
{
    ALOGV("Release recorded frame+++++++ :%d", mFramesReceivedBack++);
    Aframe *aCurFrame = NULL;
    //Find Aframe corresponding this frame
    for (List<Aframe* >::iterator it = mFramesList.begin();
         it != mFramesList.end(); ++it) {
        if ((*it)->mPtr->pointer() ==  frame->pointer()) {
            //we found!
            aCurFrame = (*it);
            break;
        }
    }

    if(NULL != aCurFrame) {
        //move Aframe* to FreeFrames..
       Mutex::Autolock autoLock(mFreeListMutex);
       mFreeFramesList.push_back(aCurFrame);
       mFreeListCondition.signal();
    }
    else {
        ALOGE("returnRecordedFrame - Frame not found in List \n");
        return NvError_BadParameter;
    }
    ALOGV("Release recorded frame-- :%d", mFramesReceivedBack);
    return NvSuccess;
 }



int EGLStreamConsumer::init() {
    ALOGV("EGLStreamConsumer::Init start ++");
    mMemoryDealer = new MemoryDealer(kNumBuffers * kBufferSize);

    // mFramesList
    for (int i=0 ;i < kNumBuffers; i++) {
        Aframe *p = new Aframe();
        p->mPtr =  mMemoryDealer->allocate(kBufferSize);
        memset(p->mPtr ->pointer(),0,kBufferSize);
        p->mFrameId = i;
        p->status = EGLStreamConsumer::kFREE;

        mFramesList.push_back(p);

        // copy all to FreeList as well!
        mFreeFramesList.push_back(p);
        AllocateFrameFully(p);
    }

    ALOGV("EGLStreamConsumer::Init start --");
    return NvSuccess;
}


// Fill a YV12 buffer with a multi-colored checkerboard pattern
#if DEBUG_DUMP
static void fillYV12Buffer(uint8_t* buf, int w, int h, int stride)
{
    const int blockWidth = w > 16 ? w / 16 : 1;
    const int blockHeight = h > 16 ? h / 16 : 1;
    const int yuvTexOffsetY = 0;
    int yuvTexStrideY = stride;
    int yuvTexOffsetV = yuvTexStrideY * h;
    int yuvTexStrideV = (yuvTexStrideY/2 + 0xf) & ~0xf;
    int yuvTexOffsetU = yuvTexOffsetV + yuvTexStrideV * h/2;
    int yuvTexStrideU = yuvTexStrideV;
    ALOGV(" fillYV12Buffer    w: %d h:%d  ++\n",w,h);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            int parityX = (x / blockWidth) & 1;
            int parityY = (y / blockHeight) & 1;
            unsigned char intensity = (parityX ^ parityY) ? 63 : 191;
            buf[yuvTexOffsetY + (y * yuvTexStrideY) + x] = intensity;
            if (x < w / 2 && y < h / 2) {
                buf[yuvTexOffsetU + (y * yuvTexStrideU) + x] = intensity;
                if (x * 2 < w / 2 && y * 2 < h / 2) {
                    buf[yuvTexOffsetV + (y*2 * yuvTexStrideV) + x*2 + 0] =
                    buf[yuvTexOffsetV + (y*2 * yuvTexStrideV) + x*2 + 1] =
                    buf[yuvTexOffsetV + ((y*2+1) * yuvTexStrideV) + x*2 + 0] =
                    buf[yuvTexOffsetV + ((y*2+1) * yuvTexStrideV) + x*2 + 1] =
                    intensity;
                }
            }
        }
    }
    ALOGV(" fillYV12Buffer  w: %d h:%d --\n",w,h);
}
#endif

sp <IMemory> EGLStreamConsumer::getNextFrame( void *pTimestamp /* Timestamp of retframe */,
                                              void *pRetStatusArg /* eFrameReqStatus */,
                                              void* pTimeOut   /* NvU64 */)
{
    ALOGV(" EGLStreamConsumer::getNextFrame ++:%d", mFramesGiven++);
    NvU64 timeoutVal = 0;
    eFrameReqStatus *pRetStatus = (eFrameReqStatus*)pRetStatusArg;
    if(pTimeOut) {
        timeoutVal = *(NvU64*)pTimeOut;
    }

    Mutex::Autolock autoLock(mFilledListMutex);
    if(!mStarted)
    {
        ALOGV(" EGLStreamConsumer::getNextFrame -----%d", mFramesGiven);
       if(pRetStatus) {
           *pRetStatus = eFrameReqStatus_EOS;
       }
        return NULL;
    }

    ALOGV("mFilledListCondition.wait ++  \n");
    while(mStarted && mFilledFramesList.empty())
    {
        if(timeoutVal){
            mFilledListCondition.waitRelative(mFilledListMutex, timeoutVal);
            // check if its timeout
            if(mFilledFramesList.empty())
            {
                if(pRetStatus) {
                    *pRetStatus = eFrameReqStatus_TIMEOUT;
                }
                return NULL;
            }
        }
        else
            mFilledListCondition.wait(mFilledListMutex);
    }
    ALOGV("mFilledListCondition.wait -----  \n");

    Aframe *pFramePtr = NULL;
    if(!mStarted)
    {
       ALOGV(" EGLStreamConsumer::frames given %d", mFramesGiven);
       if(pRetStatus) {
           *pRetStatus = eFrameReqStatus_EOS;
       }
       return NULL;  //This means EOS !!! we dont have frames any more.
    }
    else
    {
        pFramePtr = *mFilledFramesList.begin();
        // check if we need to free this as well
        mFilledFramesList.erase(mFilledFramesList.begin());
        pFramePtr->status = EGLStreamConsumer::kGIVEN_TO_CLIENT;

        if(pTimestamp) {
            NvU64 *tmp = (NvU64*)pTimestamp;
            *tmp = pFramePtr->timestamp;
        }
        if(pRetStatus) {
            *pRetStatus = eFrameReqStatus_ACTUALFRAME;
        }
    }
    ALOGV(" EGLStreamConsumer::getNextFrame --%d", mFramesGiven);
    return pFramePtr->mPtr;
 }

int  EGLStreamConsumer::DeAllocateFrameFully(Aframe *pFrame)
{
    ALOGV("Deallocate Frame ++ ");
    sp<IMemory> frame = pFrame->mPtr;
    void * frameptr = (void *)(frame->pointer());

    NvMMBuffer* pBuffer = (NvMMBuffer*)(void*)((OMX_U8 *)(frameptr) + 84);
    NvMMSurfaceDescriptor *pSurfaceDesc = &pBuffer->Payload.Surfaces;
    for (int i=0; i < pSurfaceDesc->SurfaceCount; i++)
    {
        m_Converter.NvMemoryFree(&pSurfaceDesc->Surfaces[i].hMem);
    }
    mMemoryDealer->deallocate(frame->offset());
    ALOGV("Deallocate Frame -- ");
    return NvSuccess;
}

int EGLStreamConsumer::AllocateFrameFully(Aframe *pFrame)
{
    ALOGV("EGLStreamConsumer::AllocateFrameFully frameId: %d ++++++ \n ", pFrame->mFrameId);

    NvMMBuffer *pBuffer;
    Aframe *pFramePtr = pFrame;
    sp<IMemory> frame = pFramePtr->mPtr ;
    void * frameptr = (void *)(frame->pointer());
    memset(frame->pointer(),0,kBufferSize);

    // allocate YUV surface
    NvU32 ImageSize = 0;
    NvBool UseAlignment = 1;
    pBuffer = (NvMMBuffer*)(void*)((OMX_U8 *)(frameptr) + 84);
    m_Converter.NvOmxAllocateYuv420ContinuousNvmmBuffer(mDefaultWidth,mDefaultHeight, pBuffer);

    // use the surfacedescr of it
    m_Converter.NvxSurfaceAllocContiguousYuv(
                &pBuffer->Payload.Surfaces,
                mDefaultWidth, mDefaultHeight,
                NvMMVideoDecColorFormat_YUV420,
                NvRmSurfaceLayout_Pitch,
                &ImageSize,
                UseAlignment, NvOsMemAttribute_WriteCombined);

#if DEBUG_DUMP
    void *BaseAddr;
    NvRmMemMap(pBuffer->Payload.Surfaces.Surfaces[0].hMem, 0, 3 * mDefaultWidth* (mDefaultHeight)/2 ,NVOS_MEM_READ_WRITE, &BaseAddr );
    fillYV12Buffer((uint8_t*)BaseAddr, mDefaultWidth, mDefaultHeight,mDefaultWidth);
    NvRmMemUnmap(pBuffer->Payload.Surfaces.Surfaces[0].hMem,BaseAddr,3 * mDefaultWidth * (mDefaultHeight)/2);
#endif

    pBuffer->BufferID  =  NvRmMemGetFd(pBuffer->Payload.Surfaces.Surfaces[0].hMem);

    //allocate OMXHDR and Copy it to Aframe..
    OMX_BUFFERHEADERTYPE pOMXBuf;
    pOMXBuf.nSize = sizeof(OMX_BUFFERHEADERTYPE);
    pOMXBuf.nVersion.s.nVersionMajor = 1;
    pOMXBuf.nVersion.s.nVersionMinor = 0;

    pOMXBuf.pPlatformPrivate = (void*)pBuffer->BufferID  ;
    pOMXBuf.nFlags = OMX_BUFFERFLAG_NV_BUFFER;   // indicates NV Buffer
    pOMXBuf.nOutputPortIndex = 1;
    pOMXBuf.nInputPortIndex = -1;

    int length = sizeof(NvMMBuffer);
    pOMXBuf.nFilledLen = length;
    pOMXBuf.pBuffer =   ((OMX_U8 *)(frameptr) + 4 + sizeof(OMX_BUFFERHEADERTYPE)) ;
    pOMXBuf.nTimeStamp = 0;
    OMX_U32 type = kMetadataBufferTypeEglStreamSource;
    memcpy(frameptr, &type, 4);
    memcpy((OMX_U8 *)(frameptr) + 4, &pOMXBuf, sizeof(OMX_BUFFERHEADERTYPE)); //copy value inside...
    ALOGV("EGLStreamConsumer::AllocateFrameFully frameId: %d -------\n ", pFrame->mFrameId);
    return NvSuccess;
 }

/* utility function */
#if DEBUG_DUMP
void EGLStreamConsumer::DumpSurfaces(NvMMBuffer *pMMBuffer, NvxSurface *pargSurface)
{
    static int count = 0;
    if (count == 0)
    {
       fp = (FILE *)fopen("/data/dump.yuv","wb+");
       if (fp != NULL)
        ALOGE("**************** fopen success ***********************\n");
       count++;
    }
    count++;

    if(count != 7)
        return;
    // Dumping code !
    NvU32 length = 0;
    NvU8 *tmp_buf=NULL;
    NvU32 luma_width, luma_height, chroma_width = 0, chroma_height = 0;
    NvxSurface *pSurface;
    if(pMMBuffer)
        pSurface = &(pMMBuffer->Payload.Surfaces);
    else
        pSurface = pargSurface;
    NvU32 NumBytes = 0;

    luma_width = pSurface->Surfaces[0].Width;
    luma_height = pSurface->Surfaces[0].Height;
    ALOGE("width = %d heigth = %d \n", luma_width, luma_height);

    chroma_width = (((luma_width + 0x1) & (~0x1)) >> 1);
    chroma_height = (((luma_height + 0x1) & (~0x1)) >> 1);
    ALOGE("chroma_width = %d chroma_heigth = %d \n", chroma_width, chroma_height);

    length = (luma_width * luma_height*3)/2;
    tmp_buf = (NvU8*)NvOsAlloc(length);
    if (tmp_buf == NULL)
    {
        ALOGE("Not allocating buffer count =  %d \n", count);
    }
    else
    {
        ALOGE("Allocated the buffer %p",tmp_buf);
        NvRmSurfaceRead(
           &(pSurface->Surfaces[0]),
           0,
           0,
           pSurface->Surfaces[0].Width,
           pSurface->Surfaces[0].Height,
           tmp_buf);

        if (count == 0)
       {
           fp = (FILE *)fopen("/data/dump.yuv","wb+");
           if (fp != NULL)
            ALOGE("**************** fopen success ***********************\n");
       }
       if (fp == NULL)
       {
          ALOGE("fopen failed ***********************\n");
       }
       else
       {
          // NumBytes = fwrite(tmp_buf, sizeof(NvU8), length, fp);
           ALOGE("ulbkc: Dumped Y component of surface ");
           ALOGE("Now dump U surf ");
           if(pMMBuffer) {
               NvRmSurfaceRead(
               &(pSurface->Surfaces[1]),
               0,
               0,
               pSurface->Surfaces[1].Width,
               pSurface->Surfaces[1].Height,
               tmp_buf + (pSurface->Surfaces[0].Width * pSurface->Surfaces[0].Height));
              // NumBytes = fwrite(tmp_buf, sizeof(NvU8), length, fp);
               ALOGE("Dumped U component of surface ");
               ALOGE("Now dump V surf ");
               NvRmSurfaceRead(
               &(pSurface->Surfaces[2]),
               0,
               0,
               pSurface->Surfaces[2].Width,
               pSurface->Surfaces[2].Height,
               tmp_buf + (pSurface->Surfaces[0].Width * pSurface->Surfaces[0].Height)
                             + (pSurface->Surfaces[1].Width * pSurface->Surfaces[1].Height));
            }
           NumBytes = fwrite(tmp_buf, sizeof(NvU8), length, fp);

           if(tmp_buf)
           NvOsFree(tmp_buf);

           count ++;
       }
    }
}
#endif
} // end of namespace android
