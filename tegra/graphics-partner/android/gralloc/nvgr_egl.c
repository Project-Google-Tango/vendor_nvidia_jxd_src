/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nvinternal.h>
#include <cutils/log.h>
#include <dlfcn.h>
#include <unistd.h>

#include "nvgralloc.h"

typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETDISPLAYPROC) (EGLNativeDisplayType display_id);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLINITIALIZEPROC) (EGLDisplay dpy, EGLint *major, EGLint *minor);
typedef __eglMustCastToProperFunctionPointerType (EGLAPIENTRYP PFNEGLGETPROCADDRESSPROC) (const char *procname);

/* Make part of NvGrModule? */
struct NvGrEglStateRec {
    void *dso;
    EGLDisplay dpy;
    /* Core functions */
    PFNEGLGETDISPLAYPROC GetDisplay;
    PFNEGLINITIALIZEPROC Initialize;
    PFNEGLGETPROCADDRESSPROC GetProcAddress;
    /* Extension functions */
    PFNEGLCREATEIMAGEKHRPROC CreateImage;
    PFNEGLDESTROYIMAGEKHRPROC DestroyImage;
    PFNEGLDECOMPRESSIMAGENVXPROC DecompressImage;
};

NvError NvGrEglInit(NvGrModule *m)
{
    int i;
    static const char *eglPaths[] = {
        "/vendor/lib/egl/libEGL_tegra.so",
        "/system/lib/egl/libEGL_tegra.so",
    };
    NvError err = NvError_NotInitialized;
    NvGrEglState *egl = (NvGrEglState *) malloc(sizeof(NvGrEglState));

    if (!egl) {
        return NvError_InsufficientMemory;
    }
    memset(egl, 0, sizeof(NvGrEglState));

    for (i = 0; i < (int)NV_ARRAY_SIZE(eglPaths); i++) {
        const char *path = eglPaths[i];
        if (access(path, R_OK)) {
            continue;
        }
        egl->dso = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
        if (!egl->dso) {
            ALOGE("%s: Failed to load EGL from %s\n", __func__, path);
            goto fail;
        }
        egl->GetDisplay = (PFNEGLGETDISPLAYPROC) dlsym(egl->dso, "eglGetDisplay");
        egl->Initialize = (PFNEGLINITIALIZEPROC) dlsym(egl->dso, "eglInitialize");
        egl->GetProcAddress = (PFNEGLGETPROCADDRESSPROC) dlsym(egl->dso, "eglGetProcAddress");
        if (!egl->GetDisplay ||
            !egl->Initialize ||
            !egl->GetProcAddress) {
            ALOGE("%s: Failed to resolve EGL symbols\n", __func__);
            goto fail;
        }
        break;
    }
    egl->dpy = egl->GetDisplay(EGL_DEFAULT_DISPLAY);

    egl->CreateImage = (PFNEGLCREATEIMAGEKHRPROC)
        egl->GetProcAddress("eglCreateImageKHR");
    egl->DestroyImage = (PFNEGLDESTROYIMAGEKHRPROC)
        egl->GetProcAddress("eglDestroyImageKHR");
    egl->DecompressImage = (PFNEGLDECOMPRESSIMAGENVXPROC)
        egl->GetProcAddress("eglDecompressImageNVX");

    if (egl->dpy == EGL_NO_DISPLAY ||
        !egl->CreateImage ||
        !egl->DestroyImage ||
        !egl->DecompressImage) {
        ALOGE("%s: Failed to initialize EGL", __func__);
        goto fail;
    }

    m->egl = egl;
    return NvSuccess;
fail:
    if (egl->dso) {
        dlclose(egl->dso);
    }
    free(egl);
    return err;
}

void NvGrEglDeInit(NvGrModule *m)
{
    if (m->egl) {
        dlclose(m->egl->dso);
        free(m->egl);
        m->egl = NULL;
    }
}

static EGLImageKHR NvGrEglCreateImage(NvGrModule *m, NvNativeHandle *h)
{
    EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NVRM_SURFACE_COUNT_NVX, h->SurfCount,
        EGL_NONE,
    };
    EGLImageKHR image = m->egl->CreateImage(
        m->egl->dpy, EGL_NO_CONTEXT, EGL_NVRM_SURFACE_NVX, h->Surf, attrs);

    if (image == EGL_NO_IMAGE_KHR) {
        ALOGE("%s: Failed to create EGL Image (0x%x)", __func__, eglGetError());
    }

    return image;
}

static EGLBoolean NvGrEglDecompressImage(NvGrModule *m, EGLImageKHR image,
                                         int fenceIn, int *fenceOut)
{
    EGLint attrs[5];
    int attrIdx = 0;
    int fenceOutAttrIdx = -1;

    if (fenceIn >= 0) {
        attrs[attrIdx++] = EGL_WAIT_FENCE_FD_NVX;
        attrs[attrIdx++] = fenceIn;
    }
    if (fenceOut) {
        attrs[attrIdx++] = EGL_ISSUE_FENCE_FD_NVX;
        attrs[attrIdx++] = -1;
        fenceOutAttrIdx = attrIdx - 1;
    }
    attrs[attrIdx++] = EGL_NONE;

    if (!m->egl->DecompressImage(m->egl->dpy, image, attrs)) {
        ALOGE("%s: eglDecompressImageNVX failed (0x%x)", __func__, eglGetError());
        return EGL_FALSE;
    }

    if (fenceOutAttrIdx != -1) {
        // Extract the output fence (written by eglDecompressImageNVX
        // from the attrib array). Passing output arguments in the attrib
        // array may seem a bit dodgy but this is an internal extension.
        NV_ASSERT(fenceOut);
        *fenceOut = attrs[fenceOutAttrIdx];
    }

    return EGL_TRUE;
}

NvError NvGrEglDecompressBuffer(NvGrModule *m, NvNativeHandle *h,
                                int fenceIn, int *fenceOut)
{
    EGLImageKHR image;
    EGLBoolean result = EGL_FALSE;

    if (!m->egl) {
        NvError err = NvGrEglInit(m);
        if (err) {
            return err;
        }
    }
    NV_ASSERT(m->egl);

    // We need to make sure every time that EGL is initialized, because the app
    // might call eglTerminate at any time. Most of the time this is a no-op, as
    // EGL will detect that it has already been initialized. If we actually
    // do trigger initialization here, we will leak the resources until process
    // exit (unless the app happens to call eglTerminate).
    // Since we bypass meta-EGL, this call will not affect meta-EGL refcounting.
    // Bug 1386997.
    if (!m->egl->Initialize(m->egl->dpy, NULL, NULL)) {
        ALOGE("%s: eglInitialize failed (0x%x)", __func__, eglGetError());
        return NvError_InsufficientMemory;
    }

    // Create image
    // TODO: Store in the handle to avoid recreating the image every time?
    //       Try to share the EGLImage between GLComposer and decompression?
    //       Bug 1330072.
    image = NvGrEglCreateImage(m, h);

    // Decompress
    if (image != EGL_NO_IMAGE_KHR) {
        result = NvGrEglDecompressImage(m, image, fenceIn, fenceOut);

        if (result) {
            NvGrEventCounterInc(m, NvGrEventCounterId_LazyDecompression);
        }

        m->egl->DestroyImage(m->egl->dpy, image);
    }

    return result ? NvSuccess : NvError_InsufficientMemory;
}
