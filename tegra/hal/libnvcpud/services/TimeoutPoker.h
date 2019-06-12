/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef ANDROID_NV_CPU_TIMEOUT_POKER_H
#define ANDROID_NV_CPU_TIMEOUT_POKER_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/threads.h>
#include <utils/Errors.h>
#include <utils/List.h>
#include <utils/Looper.h>

#include <binder/IMemory.h>
#include <binder/PermissionCache.h>
#include <binder/BinderService.h>

#include <nvcpud/INvCpuService.h>

#include "Barrier.h"
#include "Config.h"

namespace android {
namespace nvcpu {

//It seems redundant to need both this message queue
//And the IPC threads message queue
//But I didn't see an easy way to
//run an event after a timeout on the IPC threads

class TimeoutPoker {
private:
    class PokeHandler;

public:
    TimeoutPoker(Barrier* readyToRun);

    int boostStrengthToFreq(NvCpuBoostStrength strength);

    int createPmQosHandle(const char* filename, int val, bool force);
    void requestPmQosTimed(const char* filename, int val, nsecs_t timeoutNs, bool force);

private:
    class QueuedEvent {
    public:
        virtual ~QueuedEvent() {}
        QueuedEvent() { }

        virtual void run(PokeHandler * const thiz) = 0;
    };

    class PmQosOpenTimedEvent : public QueuedEvent {
    public:
        virtual ~PmQosOpenTimedEvent() {}
        PmQosOpenTimedEvent(const char* node,
                int val,
                nsecs_t timeout,
                bool force) :
            node(node),
            val(val),
            timeout(timeout),
            force(force) { }

        virtual void run(PokeHandler * const thiz) {
            thiz->openPmQosTimed(node, val, timeout, force);
        }

    private:
        String8 node;
        int val;
        nsecs_t timeout;
        bool force;
    };

    class PmQosOpenHandleEvent : public QueuedEvent {
    public:
        virtual ~PmQosOpenHandleEvent() {}
        PmQosOpenHandleEvent(const char* node,
                int val,
                int* outFd,
                Barrier* done,
                bool force) :
            node(node),
            val(val),
            outFd(outFd),
            done(done),
            force(force) { }

        virtual void run(PokeHandler * const thiz) {
            *outFd = thiz->createHandleForPmQosRequest(node, val, force);
            done->open();
        }

    private:
        String8 node;
        int val;
        int* outFd;
        Barrier* done;
        bool force;
    };

    class RefreshConfigEvent : public QueuedEvent {
    public:
        virtual ~RefreshConfigEvent() {}
        RefreshConfigEvent() { }

        virtual void run(PokeHandler * const thiz) {
            thiz->refreshConfig();
        }

    private:
    };

    class TimeoutEvent : public QueuedEvent {
    public:
        virtual ~TimeoutEvent() {}
        TimeoutEvent(int pmQosFd) : pmQosFd(pmQosFd) {}

        virtual void run(PokeHandler * const thiz) {
            thiz->timeoutRequest(pmQosFd);
        }

    private:
        int pmQosFd;
    };

    void pushEvent(QueuedEvent* event);

    static int FreqStepTableKhz[NVCPU_BOOST_STRENGTH_COUNT];
    static int CameraFreqLevels[NVCPU_CAMERA_LEVELS_COUNT];
    class PokeHandler : public MessageHandler {
        class LooperThread : public Thread {
            private:
                Barrier* mReadyToRun;
            public:
                sp<Looper> mLooper;
                virtual bool threadLoop();
                LooperThread(Barrier* readyToRun) :
                    mReadyToRun(readyToRun) {}
                virtual status_t readyToRun();
        };
    public:

        sp<LooperThread> mWorker;

        KeyedVector<unsigned int, QueuedEvent*> mQueuedEvents;

        virtual void handleMessage(const Message& msg);
        PokeHandler(TimeoutPoker* poker, Barrier* readyToRun);
        int generateNewKey(void);
        void sendEventDelayed(nsecs_t delay, QueuedEvent* ev);
        int listenForHandleToCloseFd(int handle, int fd);
        QueuedEvent* removeEventByKey(int key);
        void openPmQosTimed(const String8& fileName, int val, nsecs_t timeout, bool force);
        int createHandleForFd(int fd);
        int createHandleForPmQosRequest(const char* filename, int val, bool force);
        int openPmQosNode(const char* filename, int val);
        void timeoutRequest(int fd);
        void refreshConfig();
    private:
        TimeoutPoker* mPoker;
        Config mConfig;
        int mKey;

        bool mSpamRefresh;
        mutable Mutex mEvLock;
    };

    sp<PokeHandler> mPokeHandler;
};

};
};

#endif
