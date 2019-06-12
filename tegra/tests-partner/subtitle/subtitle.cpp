/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <cutils/memory.h>

#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <gui/Surface.h>
#include <gui/ISurface.h>
#include <gui/SurfaceComposerClient.h>

using namespace android;

int main(int argc, char** argv)
{
    printf("------ Subtitle App ------\n");

    // set up the thread-pool
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    // create a client to surfaceflinger
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();

    sp<SurfaceControl> surfaceControl = client->createSurface(
            String8("Subtitle"), 360, 80, PIXEL_FORMAT_RGBA_8888, 0);
    if( surfaceControl == NULL) {
        printf("Subtitle App: Failed to create a Surface\n");
        return -1;
    }
    sp<Surface> surface = surfaceControl->getSurface();

    ANativeWindow* window = (ANativeWindow*)surface.get();
    status_t err = native_window_set_usage(window,
                   GRALLOC_USAGE_PRIVATE_2 | GRALLOC_USAGE_SW_WRITE_OFTEN);

    Surface::SurfaceInfo info;

    // lock it
    surface->lock(&info);

    // red box
    ssize_t bpr = info.s * bytesPerPixel(info.format);
    android_memset32((uint32_t*)info.bits, 0x800000F0, bpr*info.h);

    // unlock it
    surface->unlockAndPost();

    SurfaceComposerClient::openGlobalTransaction();
    surfaceControl->setLayer(100000);
    SurfaceComposerClient::closeGlobalTransaction();

    printf("Subtitle layer is posted. CTRL+C to finish.\n");

    IPCThreadState::self()->joinThreadPool();

    return 0;
}

