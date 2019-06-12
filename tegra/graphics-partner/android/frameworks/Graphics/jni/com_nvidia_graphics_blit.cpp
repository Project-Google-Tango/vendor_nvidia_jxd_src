/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define LOG_TAG "NvBlit-JNI"

#include <list>
#include "jni.h"
#include "gui/GLConsumer.h"
#include "android_runtime/android_graphics_SurfaceTexture.h"
#include "nvassert.h"
#include "nvblit.h"
#include "nvgr.h"

#define NVBLIT_CLASS         "com/nvidia/graphics/NvBlit"
#define NVBLITSTATE_CLASS    "com/nvidia/graphics/NvBlit$State"
#define SURFACETEXTURE_CLASS "android/graphics/SurfaceTexture"

#define NVBLITSTATE_JNI_ID   "mState"

#define GET_STATE(e, s) \
    reinterpret_cast<NvBlitState *>(e->GetIntField(s, NvBlitState_FieldID));

#define FORCE_SYNC 0

static jfieldID NvBlitState_FieldID = NULL;
static NvBlitContext *ctx = NULL;
static android::sp<ANativeWindow> anw;
static ANativeWindowBuffer *anwb = NULL;
static int acquireFence = -1;
static std::list< android::sp<android::GLConsumer> > srcSurfaces;

static void
JNI_NvBlit_ClassInit(JNIEnv *env,
                     jclass clazz)
{
    jclass nvblitStateClazz = env->FindClass(NVBLITSTATE_CLASS);

    if (nvblitStateClazz == NULL)
    {
        ALOGE("Can't find class %s", NVBLITSTATE_CLASS);
        return;
    }

    NvBlitState_FieldID = env->GetFieldID(nvblitStateClazz,
                                          NVBLITSTATE_JNI_ID,
                                          "I");
}

static jint
JNI_NvBlit_Open(JNIEnv *env,
                jclass clazz)
{
    if (ctx != NULL)
        return NvSuccess;

    anw = NULL;
    anwb = NULL;
    acquireFence = -1;
    srcSurfaces.clear();

    return NvBlitOpen(&ctx);
}

static void
JNI_NvBlit_Close(JNIEnv *env,
               jclass clazz)
{
    if (ctx == NULL)
        return;

    NvBlitClose(ctx);

    ctx = NULL;
    anw = NULL;
    anwb = NULL;
    acquireFence = -1;
    srcSurfaces.clear();
}

static jint
JNI_NvBlit_FrameBegin(JNIEnv *env,
                      jclass clazz,
                      jobject jDstSurface)
{
    using namespace android;

    int result;

    if (ctx == NULL)
        goto fail;

    anw = android_SurfaceTexture_getNativeWindow(env, jDstSurface);
    if (anw.get() == NULL)
        goto fail;

    native_window_set_buffer_count(anw.get(), 3);
    native_window_set_usage(anw.get(), GRALLOC_USAGE_HW_2D);
    native_window_set_buffers_timestamp(anw.get(), NATIVE_WINDOW_TIMESTAMP_AUTO);

    result = anw->dequeueBuffer(anw.get(), &anwb, &acquireFence);
    if (result != 0)
        goto fail;

    #if FORCE_SYNC
    if (acquireFence >= 0)
    {
        sync_wait(acquireFence, -1);
        close(acquireFence);
        acquireFence = -1;
    }
    #endif

    return NvSuccess;

fail:
    anw = NULL;
    anwb = NULL;
    acquireFence = -1;
    srcSurfaces.clear();
    return -1;
}

static void
JNI_NvBlit_FrameEnd(JNIEnv *env,
                    jclass clazz)
{
    using namespace android;

    int releaseFence = -1;
    NvError err;
    NvBlitResult blitResult;

    if (ctx == NULL)
        goto end;

    if (anw.get() == NULL)
        goto end;

    err = NvBlitFlush(ctx, &blitResult);

    if (err != NvSuccess)
    {
        ALOGE("Blit dispatch failed: %s\n", NvBlitGetErrorString(&blitResult));
    }
    else
    {
        releaseFence = NvBlitGetSync(&blitResult);

        #if FORCE_SYNC
        if (releaseFence >= 0)
        {
            sync_wait(releaseFence, -1);
            close(releaseFence);
            releaseFence = -1;
        }
        #endif

        if (releaseFence >= 0)
        {
            std::list< android::sp<android::GLConsumer> >::iterator i;
            i = srcSurfaces.begin();
            while (i != srcSurfaces.end())
            {
                sp<GLConsumer> &glc = *i++;
                sp<Fence> fence(new Fence(dup(releaseFence)));
                glc->setReleaseFence(fence);
            }
        }
    }

    nvgr_inc_write_count(anwb->handle);

    anw->queueBuffer(anw.get(), anwb, releaseFence);

end:
    anw = NULL;
    anwb = NULL;
    acquireFence = -1;
    srcSurfaces.clear();
}

static jint
JNI_NvBlit_Blit(JNIEnv *env,
                jclass clazz,
                jobject jState)
{
    using namespace android;

    int result;
    NvError err;
    NvBlitState *state;
    NvBlitResult blitResult;
    NvBlitSurface dstSurface;

    NV_ASSERT(NvBlitState_FieldID != NULL);

    if (ctx == NULL)
        return -1;

    if (anw.get() == NULL)
        return -1;

    state = GET_STATE(env, jState);
    dstSurface = anwb->handle;

    NvBlitSetDstSurface(state, dstSurface, acquireFence);

    err = NvBlit(ctx, state, &blitResult);

    if (err != NvSuccess)
    {
        ALOGE("Blit failed: %s\n", NvBlitGetErrorString(&blitResult));
        return err;
    }

    return NvSuccess;
}

static void
JNI_NvBlitState_Init(JNIEnv *env,
                     jobject jState)
{
    NV_ASSERT(NvBlitState_FieldID != NULL);

    NvBlitState *state = new NvBlitState;
    NvBlitClearState(state);

    env->SetIntField(jState, NvBlitState_FieldID, reinterpret_cast<int>(state));
}

static void
JNI_NvBlitState_Finalize(JNIEnv *env,
                         jobject jState)
{
    NvBlitState *state = GET_STATE(env, jState);
    delete state;
    env->SetIntField(jState, NvBlitState_FieldID, 0);
}

static void
JNI_NvBlitState_Clear(JNIEnv *env,
                      jobject jState)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitClearState(state);
    NvBlitSetFlag(state, NvBlitFlag_DeferFlush);
}

static void
JNI_NvBlitState_SetSrcSurface(JNIEnv *env,
                              jobject jState,
                              jobject jSrcSurface)
{
    using namespace android;

    NvBlitState *state = GET_STATE(env, jState);
    int srcFence = -1;
    NvBlitSurface srcSurface;

    sp<GLConsumer> glc = SurfaceTexture_getSurfaceTexture(env, jSrcSurface);
    if (glc.get() == NULL)
        return;

    sp<GraphicBuffer> gb = glc->getCurrentBuffer();
    if (gb.get() == NULL)
        return;

    ANativeWindowBuffer *anwb = gb->getNativeBuffer();
    if (anwb == NULL)
        return;

    sp<Fence> fence = glc->getCurrentFence();
    if (fence.get() != NULL && fence->isValid())
        srcFence = fence->dup();

    srcSurface = anwb->handle;

    #if FORCE_SYNC
    if (srcFence >= 0)
    {
        sync_wait(srcFence, -1);
        close(srcFence);
        srcFence = -1;
    }
    #endif

    NvBlitSetSrcSurface(state, srcSurface, srcFence);

    srcSurfaces.push_back(glc);
}

static void
JNI_NvBlitState_SetSrcRect(JNIEnv *env,
                           jobject jState,
                           jfloat left,
                           jfloat top,
                           jfloat right,
                           jfloat bottom)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitRect rect;
    rect.left   = left;
    rect.top    = top;
    rect.right  = right;
    rect.bottom = bottom;
    NvBlitSetSrcRect(state, &rect);
}

static void
JNI_NvBlitState_SetDstRect(JNIEnv *env,
                           jobject jState,
                           jfloat left,
                           jfloat top,
                           jfloat right,
                           jfloat bottom)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitRect rect;
    rect.left   = left;
    rect.top    = top;
    rect.right  = right;
    rect.bottom = bottom;
    NvBlitSetDstRect(state, &rect);
}

static void
JNI_NvBlitState_SetTransform(JNIEnv *env,
                             jobject jState,
                             jint transform)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitSetTransform(state, static_cast<NvBlitTransform>(transform));
}

static void
JNI_NvBlitState_SetFilter(JNIEnv *env,
                          jobject jState,
                          jint filter)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitSetFilter(state, static_cast<NvBlitFilter>(filter));
}

static void
JNI_NvBlitState_SetSrcColor(JNIEnv *env,
                            jobject jState,
                            jint color)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitSetSrcColor(state, color, NvColorFormat_A8R8G8B8);
}

static void
JNI_NvBlitState_SetColorTransform(JNIEnv *env,
                                  jobject jState,
                                  jint srcColorSpace,
                                  jint dstColorSpace,
                                  jint linkProfileType)
{
    NvBlitState *state = GET_STATE(env, jState);
    NvBlitSetColorTransform(state, (NvCmsAbsColorSpace)srcColorSpace,
                            (NvCmsAbsColorSpace)dstColorSpace, (NvCmsProfileType)linkProfileType);
}

#define DEF(c, f, p) \
    {                                \
        "native" # f,                \
        p,                           \
        (void *) JNI_ ## c ## _ ## f \
    }

static const JNINativeMethod NvBlit_Methods[] =
{
    DEF(NvBlit, ClassInit,  "()V"),
    DEF(NvBlit, Open,       "()I"),
    DEF(NvBlit, Close,      "()V"),
    DEF(NvBlit, FrameBegin, "(L" SURFACETEXTURE_CLASS ";)I"),
    DEF(NvBlit, FrameEnd,   "()V"),
    DEF(NvBlit, Blit,       "(L" NVBLITSTATE_CLASS ";)I"),
};

static const JNINativeMethod NvBlitState_Methods[] =
{
    DEF(NvBlitState, Init,              "()V"),
    DEF(NvBlitState, Finalize,          "()V"),
    DEF(NvBlitState, Clear,             "()V"),
    DEF(NvBlitState, SetSrcSurface,     "(L" SURFACETEXTURE_CLASS ";)V"),
    DEF(NvBlitState, SetSrcRect,        "(FFFF)V"),
    DEF(NvBlitState, SetDstRect,        "(FFFF)V"),
    DEF(NvBlitState, SetTransform,      "(I)V"),
    DEF(NvBlitState, SetFilter,         "(I)V"),
    DEF(NvBlitState, SetSrcColor,       "(I)V"),
    DEF(NvBlitState, SetColorTransform, "(III)V"),
};

#undef DEF

static NvBool
RegisterMethods(JNIEnv *env,
                const char *className,
                const JNINativeMethod *methods,
                size_t numMethods)
{
    jclass clazz = env->FindClass(className);

    if (clazz == NULL)
    {
        ALOGE("Can't find class %s", className);
        return NV_FALSE;
    }

    if (env->RegisterNatives(clazz, methods, numMethods) != JNI_OK)
    {
        ALOGE("Failed registering methods for %s\n", className);
        return NV_FALSE;
    }

    return NV_TRUE;
}

NvBool register_com_nvidia_graphics_blit(JNIEnv *env);

NvBool
register_com_nvidia_graphics_blit(JNIEnv *env)
{
    NvBool ok;

    ok = RegisterMethods(env,
                         NVBLIT_CLASS,
                         NvBlit_Methods,
                         NV_ARRAY_SIZE(NvBlit_Methods));
    if (ok == NV_FALSE)
        return NV_FALSE;

    ok = RegisterMethods(env,
                         NVBLITSTATE_CLASS,
                         NvBlitState_Methods,
                         NV_ARRAY_SIZE(NvBlitState_Methods));

    return ok;
}
