/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef NV_OMX_PLUGIN_H_

#define NV_OMX_PLUGIN_H_

#ifdef BUILD_GOOGLETV
#include <hardware/OMXPluginBaseExt.h>
#else
#include <media/hardware/OMXPluginBase.h>
#endif

namespace android {

#ifdef BUILD_GOOGLETV
struct NVOMXPlugin : public OMXPluginBaseExt {
#else
struct NVOMXPlugin : public OMXPluginBase {
#endif
    NVOMXPlugin();
    virtual ~NVOMXPlugin();

    virtual OMX_ERRORTYPE makeComponentInstance(
            const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

    virtual OMX_ERRORTYPE destroyComponentInstance(
            OMX_COMPONENTTYPE *component);

    virtual OMX_ERRORTYPE enumerateComponents(
            OMX_STRING name,
            size_t size,
            OMX_U32 index);

    virtual OMX_ERRORTYPE getRolesOfComponent(
            const char *name,
            Vector<String8> *roles);

#ifdef BUILD_GOOGLETV
    virtual OMX_ERRORTYPE setupTunnel(
            OMX_COMPONENTTYPE *outputComponent,
            OMX_U32 outputPortIndex,
            OMX_COMPONENTTYPE *inputComponent,
            OMX_U32 inputPortIndex);
#endif

private:
    void *mLibHandle;

    typedef OMX_ERRORTYPE (*InitFunc)();
    typedef OMX_ERRORTYPE (*DeinitFunc)();
    typedef OMX_ERRORTYPE (*ComponentNameEnumFunc)(
            OMX_STRING, OMX_U32, OMX_U32);

    typedef OMX_ERRORTYPE (*GetHandleFunc)(
            OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);

    typedef OMX_ERRORTYPE (*FreeHandleFunc)(OMX_HANDLETYPE *);

    typedef OMX_ERRORTYPE (*GetRolesOfComponentFunc)(
            OMX_STRING, OMX_U32 *, OMX_U8 **);

#ifdef BUILD_GOOGLETV
    typedef OMX_ERRORTYPE (*SetupTunnelFunc)(
            OMX_HANDLETYPE, OMX_U32, OMX_HANDLETYPE, OMX_U32);
#endif

    InitFunc mInit;
    DeinitFunc mDeinit;
    ComponentNameEnumFunc mComponentNameEnum;
    GetHandleFunc mGetHandle;
    FreeHandleFunc mFreeHandle;
    GetRolesOfComponentFunc mGetRolesOfComponentHandle;
#ifdef BUILD_GOOGLETV
    SetupTunnelFunc mSetupTunnel;
#endif

    NVOMXPlugin(const NVOMXPlugin &);
    NVOMXPlugin &operator=(const NVOMXPlugin &);
};

}  // namespace android

#endif  // NV_OMX_PLUGIN_H_

