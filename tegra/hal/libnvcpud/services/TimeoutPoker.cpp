/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#include "TimeoutPoker.h"
#include <fcntl.h>
#include "NvParseConfig.h"

#undef LOG_TAG
#define LOG_TAG "nvcpu::TimeoutPoker"

//#define DEBUG_ATTRIBUTE_SETTING 1

#ifdef DEBUG_ATTRIBUTE_SETTING
#define DBG_ATTR( X ) X
#else
#define DBG_ATTR( X )
#endif

namespace android {
namespace nvcpu {

/* this is not the best way to translate booststrength to frequency.
   however, this is resonable as of 2011. This table might need to
   be revisited in the future.  Some board-dependent configuration may
   also be necessary */

int TimeoutPoker::FreqStepTableKhz[NVCPU_BOOST_STRENGTH_COUNT] = {
     300000, /* NVCPU_BOOST_STRENGTH_LOWEST      */
     450000, /* NVCPU_BOOST_STRENGTH_LOW         */
     600000, /* NVCPU_BOOST_STRENGTH_MEDIUM_LOW  */
     750000, /* NVCPU_BOOST_STRENGTH_MEDIUM      */
     900000, /* NVCPU_BOOST_STRENGTH_MEDIUM_HIGH */
    1050000, /* NVCPU_BOOST_STRENGTH_HIGH        */
    1200000, /* NVCPU_BOOST_STRENGTH_HIGHEST     */
};

int TimeoutPoker::CameraFreqLevels[NVCPU_CAMERA_LEVELS_COUNT] = {
    NVCPU_BOOST_STRENGTH_MEDIUM_LOW, /* NVCPU_CAMERA_PREVIEW         */
    NVCPU_BOOST_STRENGTH_MEDIUM,     /* NVCPU_CAMERA_PREVIEW_FD      */
    NVCPU_BOOST_STRENGTH_MEDIUM,     /* NVCPU_CAMERA_VIDEO_RECORDING */
};

enum {
    CLIENT_FD,
    SERVER_FD
};

TimeoutPoker::TimeoutPoker(Barrier* readyToRun)
{
    mPokeHandler = new PokeHandler(this, readyToRun);
    int err = NvCpud_parseCpuConf(TimeoutPoker::FreqStepTableKhz, TimeoutPoker::CameraFreqLevels);
}

//Called usually from IPC thread
void TimeoutPoker::pushEvent(QueuedEvent* event)
{
    mPokeHandler->sendEventDelayed(0, event);
}

int TimeoutPoker::PokeHandler::createHandleForFd(int fd)
{
    int pipefd[2];
    int res = pipe(pipefd);
    if (res) {
        DBG_ATTR(LOGE("unabled to create handle for fd"));
        close(fd);
        return -1;
    }

    listenForHandleToCloseFd(pipefd[SERVER_FD], fd);
    if (res) {
        close(fd);
        close(pipefd[SERVER_FD]);
        close(pipefd[CLIENT_FD]);
        return -1;
    }

    return pipefd[CLIENT_FD];
}

int TimeoutPoker::PokeHandler::openPmQosNode(const char* filename, int val)
{
    int pm_qos_fd = open(filename, O_RDWR);;
    if (pm_qos_fd < 0) {
        DBG_ATTR(LOGE("unable to open pm_qos file for %s", filename));
        return -1;
    }
    write(pm_qos_fd, &val, sizeof(val));
    DBG_ATTR(LOGI("opened %s fd (%d) with val %d", filename, pm_qos_fd, val));
    return pm_qos_fd;
}

int TimeoutPoker::PokeHandler::createHandleForPmQosRequest(const char* filename, int val, bool force)
{
    if (!force && !mConfig.enabled)
        return -1;

    int fd = openPmQosNode(filename, val);
    if (fd < 0) {
        return -1;
    }

    return createHandleForFd(fd);
}

int TimeoutPoker::createPmQosHandle(const char* filename,
        int val, bool force)
{
    Barrier done;
    int ret;
    pushEvent(new PmQosOpenHandleEvent(
                filename, val, &ret, &done, force));

    done.wait();
    return ret;
}

void TimeoutPoker::requestPmQosTimed(const char* filename,
        int val, nsecs_t timeout, bool force)
{
    pushEvent(new PmQosOpenTimedEvent(
                filename, val, timeout, force));
}

/*
 * PokeHandler
 */
void TimeoutPoker::PokeHandler::sendEventDelayed(
        nsecs_t delay, TimeoutPoker::QueuedEvent* ev) {
    Mutex::Autolock _l(mEvLock);

    int key = generateNewKey();
    mQueuedEvents.add(key, ev);
    mWorker->mLooper->sendMessageDelayed(delay, this, key);
}

TimeoutPoker::QueuedEvent*
TimeoutPoker::PokeHandler::removeEventByKey(
int what) {
    Mutex::Autolock _l(mEvLock);

    // msg.what contains a key to retrieve the message parameters
    TimeoutPoker::QueuedEvent* e =
        mQueuedEvents.valueFor(what);
    mQueuedEvents.removeItem(what);
    return e;
}

int TimeoutPoker::PokeHandler::generateNewKey(void)
{
    return mKey++;
}

TimeoutPoker::PokeHandler::PokeHandler(TimeoutPoker* poker, Barrier* readyToRun) :
    mPoker(poker),
    mKey(0),
    mSpamRefresh(false)
{
    mWorker = new LooperThread(readyToRun);
    mWorker->run("nvcpu::TimeoutPoker::PokeHandler::LooperThread", PRIORITY_FOREGROUND);
    readyToRun->wait();
    refreshConfig();
}

int TimeoutPoker::boostStrengthToFreq(NvCpuBoostStrength strength)
{
    /* Special Camera use cases */
    if ((strength >= NVCPU_CAMERA_PREVIEW) &&
        (strength <= NVCPU_CAMERA_VIDEO_RECORDING)) {
        int strengthLevel = TimeoutPoker::CameraFreqLevels[strength - NVCPU_CAMERA_PREVIEW];
        return FreqStepTableKhz[strengthLevel];
    }

    if ((strength < NVCPU_BOOST_STRENGTH_LOWEST) ||
        (strength >= NVCPU_BOOST_STRENGTH_COUNT)) {
        LOGE("requested NvCpuBoostStrength is invalid!");
        return 0;
    }

    /* treat NVCPU_BOOST_STRENGTH_HIGHEST specially to really max out
     * the cpu frequency of this devices. */
    if (strength == NVCPU_BOOST_STRENGTH_HIGHEST) {
        // requesting whatever maximum that system supports
        return INT_MAX;
    }
    return FreqStepTableKhz[strength];
}

void TimeoutPoker::PokeHandler::handleMessage(const Message& msg)
{
    assert(!mQueuedEvents->isEmpty());

    // msg.what contains a key to retrieve the message parameters
    TimeoutPoker::QueuedEvent* e =
        removeEventByKey(msg.what);

    if (mSpamRefresh)
        refreshConfig();

    if (!e)
        return;

    e->run(this);

    delete e;
}

void TimeoutPoker::PokeHandler::openPmQosTimed(const String8& filename,
        int val, nsecs_t timeout, bool force)
{
    if (!force && !mConfig.enabled)
        return;

    int fd = openPmQosNode(filename.string(), val);
    if (fd < 0) {
        return;
    }

    sendEventDelayed(timeout, new TimeoutEvent(fd));
}

void TimeoutPoker::PokeHandler::timeoutRequest(int fd)
{
    close(fd);
    DBG_ATTR(LOGI("closed pm_qos node(%d)", fd));
}

void TimeoutPoker::PokeHandler::refreshConfig()
{

   if (!mConfig.bootCompleted && !mConfig.checkBootComplete()) {
       // if the config is not ready, keep trying checking this
       // for every 500 ms
        sendEventDelayed(ms2ns(500), new RefreshConfigEvent());
        return;
   }

   mConfig.refresh();
   mSpamRefresh = (mConfig.configRefreshNs == 0);
   if (mConfig.configRefreshNs <= 0)
       return;

    sendEventDelayed(mConfig.configRefreshNs,
            new RefreshConfigEvent());
}

status_t TimeoutPoker::PokeHandler::LooperThread::readyToRun()
{
    mLooper = Looper::prepare(0);
    if (mLooper == 0)
        return NO_MEMORY;
    mReadyToRun->open();
    return NO_ERROR;
}

bool TimeoutPoker::PokeHandler::LooperThread::threadLoop()
{
   int res = mLooper->pollAll(99999);
   if (res == ALOOPER_POLL_ERROR)
       LOGE("Poll returned an error!");
    return true;
}

class CallbackContext {
public:
    CallbackContext(sp<Looper> looper, int fd) : looper(looper), fd(fd) {}

    sp<Looper> looper;
    int fd;
};

static int pipeCloseCb(int handle, int events, void* data)
{
    CallbackContext* ctx = (CallbackContext*)data;

    if (events & (ALOOPER_EVENT_ERROR | ALOOPER_EVENT_HANGUP)) {
        DBG_ATTR(LOGI("closing fd (%d) for handle (%d)",  ctx->fd, handle));

        ctx->looper->removeFd(handle);
        close(handle);
        close(ctx->fd);
        delete ctx;
        return 0;
    }
    return 1;
}

//Reverse arity of result to match call-site usage
int TimeoutPoker::PokeHandler::listenForHandleToCloseFd(int handle, int fd)
{
    //This func is threadsafe
    return !mWorker->mLooper->addFd(handle, ALOOPER_POLL_CALLBACK,
            ALOOPER_EVENT_ERROR | ALOOPER_EVENT_HANGUP,
            pipeCloseCb, new CallbackContext(mWorker->mLooper, fd));
}

};
};
