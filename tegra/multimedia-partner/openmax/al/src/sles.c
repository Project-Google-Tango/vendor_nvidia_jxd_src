/*
 * Copyright (C) 2010 The Android Open Source Project
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

/* OpenSL ES private and global functions not associated with an interface or class */

#include "sles_allinclusive.h"


/** \brief Return true if the specified interface exists and has been initialized for this object.
 *  Returns false if the class does not support this kind of interface, or the class supports the
 *  interface but this particular object has not had the interface exposed at object creation time
 *  or by DynamicInterface::AddInterface. Note that the return value is not affected by whether
 *  the application has requested access to the interface with Object::GetInterface. Assumes on
 *  entry that the object is locked for either shared or exclusive access.
 */

bool IsInterfaceInitialized(IObject *thiz, unsigned MPH)
{
    assert(NULL != thiz);
    assert( /* (MPH_MIN <= MPH) && */ (MPH < (unsigned) MPH_MAX));
    const ClassTable *clazz = thiz->mClass;
    assert(NULL != clazz);
    int index;
    if (0 > (index = clazz->mMPH_to_index[MPH])) {
        return false;
    }
    assert(MAX_INDEX >= clazz->mInterfaceCount);
    assert(clazz->mInterfaceCount > (unsigned) index);
    switch (thiz->mInterfaceStates[index]) {
    case INTERFACE_EXPOSED:
    case INTERFACE_ADDED:
        return true;
    default:
        return false;
    }
}


/** \brief Map an IObject to it's "object ID" (which is really a class ID) */

SLuint32 IObjectToObjectID(IObject *thiz)
{
    assert(NULL != thiz);
    // Note this returns the OpenSL ES object ID in preference to the OpenMAX AL if both available
    const ClassTable *clazz = thiz->mClass;
    assert(NULL != clazz);
    SLuint32 id = clazz->mSLObjectID;
    if (!id)
        id = clazz->mXAObjectID;
    return id;
}


/** \brief Acquire a strong reference to an object.
 *  Check that object has the specified "object ID" (which is really a class ID) and is in the
 *  realized state.  If so, then acquire a strong reference to it and return true.
 *  Otherwise return false.
 */

SLresult AcquireStrongRef(IObject *object, SLuint32 expectedObjectID)
{
    if (NULL == object) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    // NTH additional validity checks on address here
    SLresult result;
    object_lock_exclusive(object);
    SLuint32 actualObjectID = IObjectToObjectID(object);
    if (expectedObjectID != actualObjectID) {
        SL_LOGE("object %p has object ID %u but expected %u", object, actualObjectID,
            expectedObjectID);
        result = SL_RESULT_PARAMETER_INVALID;
    } else if (SL_OBJECT_STATE_REALIZED != object->mState) {
        SL_LOGE("object %p with object ID %u is not realized", object, actualObjectID);
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        ++object->mStrongRefCount;
        result = SL_RESULT_SUCCESS;
    }
    object_unlock_exclusive(object);
    return result;
}


/** \brief Release a strong reference to an object.
 *  Entry condition: the object is locked.
 *  Exit condition: the object is unlocked.
 *  Finishes the destroy if needed.
 */

void ReleaseStrongRefAndUnlockExclusive(IObject *object)
{
#ifdef USE_DEBUG
    assert(pthread_equal(pthread_self(), object->mOwner));
#endif
    assert(0 < object->mStrongRefCount);
    if ((0 == --object->mStrongRefCount) && (SL_OBJECT_STATE_DESTROYING == object->mState)) {
        // FIXME do the destroy here - merge with IDestroy
        // but can't do this until we move Destroy to the sync thread
        // as Destroy is now a blocking operation, and to avoid a race
    } else {
        object_unlock_exclusive(object);
    }
}


/** \brief Release a strong reference to an object.
 *  Entry condition: the object is unlocked.
 *  Exit condition: the object is unlocked.
 *  Finishes the destroy if needed.
 */

void ReleaseStrongRef(IObject *object)
{
    assert(NULL != object);
    object_lock_exclusive(object);
    ReleaseStrongRefAndUnlockExclusive(object);
}


/** \brief Convert POSIX pthread error code to OpenSL ES result code */

SLresult err_to_result(int err)
{
    if (EAGAIN == err || ENOMEM == err) {
        return SL_RESULT_RESOURCE_ERROR;
    }
    if (0 != err) {
        return SL_RESULT_INTERNAL_ERROR;
    }
    return SL_RESULT_SUCCESS;
}


/** \brief Check the interface IDs passed into a Create operation */

SLresult checkInterfaces(const ClassTable *clazz, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired,
    unsigned *pExposedMask, unsigned *pRequiredMask)
{
    assert(NULL != clazz && NULL != pExposedMask);
    // Initially no interfaces are exposed
    unsigned exposedMask = 0;
    unsigned requiredMask = 0;
    const struct iid_vtable *interfaces = clazz->mInterfaces;
    SLuint32 interfaceCount = clazz->mInterfaceCount;
    SLuint32 i;
    // Expose all implicit interfaces
    for (i = 0; i < interfaceCount; ++i) {
        switch (interfaces[i].mInterface) {
        case INTERFACE_IMPLICIT:
        case INTERFACE_IMPLICIT_PREREALIZE:
            // there must be an initialization hook present
            if (NULL != MPH_init_table[interfaces[i].mMPH].mInit) {
                exposedMask |= 1 << i;
            }
            break;
        case INTERFACE_EXPLICIT:
        case INTERFACE_DYNAMIC:
        case INTERFACE_UNAVAILABLE:
        case INTERFACE_EXPLICIT_PREREALIZE:
            break;
        default:
            assert(false);
            break;
        }
    }
    if (0 < numInterfaces) {
        if (NULL == pInterfaceIds || NULL == pInterfaceRequired) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        bool anyRequiredButUnsupported = false;
        // Loop for each requested interface
        for (i = 0; i < numInterfaces; ++i) {
            SLInterfaceID iid = pInterfaceIds[i];
            if (NULL == iid) {
                return SL_RESULT_PARAMETER_INVALID;
            }
            SLboolean isRequired = pInterfaceRequired[i];
            int MPH, index;
            if ((0 > (MPH = IID_to_MPH(iid))) ||
                    // there must be an initialization hook present
                    (NULL == MPH_init_table[MPH].mInit) ||
                    (0 > (index = clazz->mMPH_to_index[MPH])) ||
                    (INTERFACE_UNAVAILABLE == interfaces[index].mInterface)) {
                // Here if interface was not found, or is not available for this object type
                if (isRequired) {
                    // Application said it required the interface, so give up
                    SL_LOGE("class %s interface %u required but unavailable MPH=%d",
                            clazz->mName, i, MPH);
                    anyRequiredButUnsupported = true;
                }
                // Application said it didn't really need the interface, so ignore with warning
                SL_LOGW("class %s interface %u requested but unavailable MPH=%d",
                        clazz->mName, i, MPH);

                continue;
            }
            if (isRequired) {
                requiredMask |= (1 << index);
            }
            // The requested interface was both found and available, so expose it
            exposedMask |= (1 << index);
            // Note that we ignore duplicate requests, including equal and aliased IDs
        }
        if (anyRequiredButUnsupported) {
            return SL_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    *pExposedMask = exposedMask;
    if (NULL != pRequiredMask) {
        *pRequiredMask = requiredMask;
    }
    return SL_RESULT_SUCCESS;
}


/* Interface initialization hooks */

extern void
    I3DCommit_init(void *),
    I3DDoppler_init(void *),
    I3DGrouping_init(void *),
    I3DLocation_init(void *),
    I3DMacroscopic_init(void *),
    I3DSource_init(void *),
    IAndroidConfiguration_init(void *),
    IAndroidEffect_init(void *),
    IAndroidEffectCapabilities_init(void *),
    IAndroidEffectSend_init(void *),
    IAndroidBufferQueue_init(void *),
    IAudioDecoderCapabilities_init(void *),
    IAudioEncoder_init(void *),
    IAudioEncoderCapabilities_init(void *),
    IAudioIODeviceCapabilities_init(void *),
    IBassBoost_init(void *),
    IBufferQueue_init(void *),
    IDeviceVolume_init(void *),
    IDynamicInterfaceManagement_init(void *),
    IDynamicSource_init(void *),
    IEffectSend_init(void *),
    IEngine_init(void *),
    IEngineCapabilities_init(void *),
    IEnvironmentalReverb_init(void *),
    IEqualizer_init(void *),
    ILEDArray_init(void *),
    IMIDIMessage_init(void *),
    IMIDIMuteSolo_init(void *),
    IMIDITempo_init(void *),
    IMIDITime_init(void *),
    IMetadataExtraction_init(void *),
    IMetadataTraversal_init(void *),
    IMuteSolo_init(void *),
    IObject_init(void *),
    IOutputMix_init(void *),
    IOutputMixExt_init(void *),
    IPitch_init(void *),
    IPlay_init(void *),
    IPlaybackRate_init(void *),
    IPrefetchStatus_init(void *),
    IPresetReverb_init(void *),
    IRatePitch_init(void *),
    IRecord_init(void *),
    ISeek_init(void *),
    IThreadSync_init(void *),
    IVibra_init(void *),
    IVirtualizer_init(void *),
    IVisualization_init(void *),
    IVolume_init(void *),
    ICameraDevice_init(void *),
    ICameraCapabilities_init(void *),
    IImageControls_init(void *),
    IImageDecoderCapabilities_init(void *),
    IImageEffects_init(void *),
    IImageEncoder_init(void *),
    IImageEncoderCapabilities_init(void *),
    IMetadataInsertion_init(void *),
    INvCameraExtCapabilities_init(void *),
    INvVideoDecoder_init(void *),
    INvVideoDecoderExtCapabilities_init(void *),
    INvVideoEncoderExt_init(void *),
    INvVideoEncoderExtCapabilities_init(void *),
    ISnapshot_init(void *),
    IVideoEncoder_init(void *),
    IVideoEncoderCapabilities_init(void *),
    IVideoPostProcessing_init(void *),
    IXARecord_init(void *),
    IGetCapabilitiesOfInterface_init(void *),
    INvPushApp_init(void *),
    INvBurstMode_init(void *);

#ifdef LINUX_OMXAL
extern void
    IAudioEncoder_deinit(void *),
    IAudioIODeviceCapabilities_deinit(void *),
    IImageDecoderCapabilities_deinit(void *),
    IImageEffects_deinit(void *),
    IImageEncoder_deinit(void *),
    IPlay_deinit(void *),
    IPlaybackRate_deinit(void *),
    IRecord_deinit(void *),
    ISeek_deinit(void *),
    ISnapshot_deinit(void *),
    IVideoEncoder_deinit(void *),
    IVolume_deinit(void *),
    IOutputMix_deinit(void *);
#endif

extern void
    I3DGrouping_deinit(void *),
    IAndroidEffect_deinit(void *),
    IAndroidEffectCapabilities_deinit(void *),
    IAndroidBufferQueue_deinit(void *),
    IBassBoost_deinit(void *),
    IBufferQueue_deinit(void *),
    IEngine_deinit(void *),
    IEnvironmentalReverb_deinit(void *),
    IEqualizer_deinit(void *),
    IObject_deinit(void *),
    IPresetReverb_deinit(void *),
    IThreadSync_deinit(void *),
    IVirtualizer_deinit(void *),
    ICameraDevice_deinit(void *),
    ICameraCapabilities_deinit(void *),
    IImageEncoderCapabilities_deinit(void *),
    INvCameraExtCapabilities_deinit(void *),
    INvVideoDecoder_deinit(void *),
    INvVideoDecoderExtCapabilities_deinit(void *),
    INvVideoEncoderExt_deinit(void *),
    INvVideoEncoderExtCapabilities_deinit(void *),
    IGetCapabilitiesOfInterface_deinit(void *),
    INvPushApp_deinit(void *),
    INvBurstMode_deinit(void *);
extern bool
    IAndroidEffectCapabilities_Expose(void *),
    IBassBoost_Expose(void *),
    IEnvironmentalReverb_Expose(void *),
    IEqualizer_Expose(void *),
    IPresetReverb_Expose(void *),
    IVirtualizer_Expose(void *);

extern void
    IXAEngine_init(void *),
    IStreamInformation_init(void*),
    IVideoDecoderCapabilities_init(void *),
    IVideoEncoderCapabilities_init(void *);

extern void
    IXAEngine_deinit(void *),
    IStreamInformation_deinit(void *),
    IVideoDecoderCapabilities_deinit(void *),
    IVideoEncoderCapabilities_deinit(void *);

extern bool
    IVideoDecoderCapabilities_expose(void *),
    IVideoEncoderCapabilities_expose(void *),
    INvVideoEncoderExtCapabilities_expose(void *),
    INvVideoDecoderExtCapabilities_expose(void *);

#if !(USE_PROFILES & USE_PROFILES_MUSIC)
#ifdef ANDROID
#define IDynamicSource_init         NULL
#define IMetadataTraversal_init     NULL
#endif
#define IVisualization_init         NULL
#endif

#if !(USE_PROFILES & USE_PROFILES_GAME)
#define I3DCommit_init      NULL
#define I3DDoppler_init     NULL
#define I3DGrouping_init    NULL
#define I3DLocation_init    NULL
#define I3DMacroscopic_init NULL
#define I3DSource_init      NULL
#define IMIDIMessage_init   NULL
#define IMIDIMuteSolo_init  NULL
#define IMIDITempo_init     NULL
#define IMIDITime_init      NULL
#define IPitch_init         NULL
#define IRatePitch_init     NULL
#define I3DGrouping_deinit  NULL
#endif

#if !(USE_PROFILES & USE_PROFILES_BASE)
#define IAudioDecoderCapabilities_init   NULL
#define IAudioIODeviceCapabilities_init  NULL
#define IDeviceVolume_init               NULL
#define IEngineCapabilities_init         NULL
#define IThreadSync_init                 NULL
#define IThreadSync_deinit               NULL
#endif

#if !(USE_PROFILES & USE_PROFILES_OPTIONAL)
#define ILEDArray_init  NULL
#define IVibra_init     NULL
#endif

#ifdef LINUX_OMXAL
#define ILEDArray_init  NULL
#define IVibra_init     NULL
#endif

#ifndef ANDROID
#define IAndroidConfiguration_init        NULL
#define IAndroidEffect_init               NULL
#define IAndroidEffectCapabilities_init   NULL
#define IAndroidEffectSend_init           NULL
#define IAndroidEffect_deinit             NULL
#define IAndroidEffectCapabilities_deinit NULL
#define IAndroidEffectCapabilities_Expose NULL
#define IAndroidBufferQueue_init          NULL
#define IAndroidBufferQueue_deinit        NULL
#ifdef ANDROID
#define IStreamInformation_init           NULL
#define IStreamInformation_deinit         NULL
#define IOutputMix_init                   NULL
#endif
#define IEnvironmentalReverb_init         NULL
#define IEnvironmentalReverb_deinit       NULL
#define IEnvironmentalReverb_Expose       NULL
#define IEqualizer_Expose                 NULL
#define IMuteSolo_init                    NULL
#define IPresetReverb_init                NULL
#define IPresetReverb_deinit              NULL
#define IPresetReverb_Expose              NULL
#define IVirtualizer_init                 NULL
#define IVirtualizer_deinit               NULL
#define IVirtualizer_Expose               NULL
#define IXARecord_init                    NULL
#define IVideoDecoderCapabilities_deinit  NULL
#define IVideoDecoderCapabilities_expose  NULL
#define IVideoEncoderCapabilities_deinit  NULL
#define IVideoEncoderCapabilities_expose  NULL
#define INvCameraExtCapabilities_init     NULL
#define INvCameraExtCapabilities_deinit   NULL
#define INvVideoEncoderExtCapabilities_init NULL
#define INvVideoEncoderExtCapabilities_deinit NULL
#define INvVideoEncoderExtCapabilities_expose NULL
#define INvVideoDecoderExtCapabilities_init NULL
#define INvVideoDecoderExtCapabilities_deinit NULL
#define INvVideoDecoderExtCapabilities_expose NULL
#define INvVideoDecoder_init              NULL
#define INvVideoDecoder_deinit            NULL
#define INvVideoEncoderExt_init           NULL
#define INvVideoEncoderExt_deinit         NULL
#define IGetCapabilitiesOfInterface_init  NULL
#define IGetCapabilitiesOfInterface_deinit NULL
#define INvPushApp_init                   NULL
#define INvPushApp_deinit                 NULL
#define INvBurstMode_init                 NULL
#define INvBurstMode_deinit               NULL
#define IEffectSend_init                  NULL
#define IBassBoost_init                   NULL
#define IBassBoost_deinit                 NULL
#define IBassBoost_Expose                 NULL
#define IBufferQueue_init                 NULL
#define IBufferQueue_deinit               NULL
#endif

#ifndef USE_OUTPUTMIXEXT
#define IOutputMixExt_init  NULL
#endif


/*static*/ const struct MPH_init MPH_init_table[MPH_MAX] = {
    { /* MPH_3DCOMMIT, */ I3DCommit_init, NULL, NULL, NULL, NULL },
    { /* MPH_3DDOPPLER, */ I3DDoppler_init, NULL, NULL, NULL, NULL },
    { /* MPH_3DGROUPING, */ I3DGrouping_init, NULL, I3DGrouping_deinit, NULL, NULL },
    { /* MPH_3DLOCATION, */ I3DLocation_init, NULL, NULL, NULL, NULL },
    { /* MPH_3DMACROSCOPIC, */ I3DMacroscopic_init, NULL, NULL, NULL, NULL },
    { /* MPH_3DSOURCE, */ I3DSource_init, NULL, NULL, NULL, NULL },
    { /* MPH_AUDIODECODERCAPABILITIES, */ IAudioDecoderCapabilities_init, NULL, NULL, NULL, NULL },
    { /* MPH_AUDIOENCODER, */ IAudioEncoder_init, NULL, NULL, NULL, NULL },
    { /* MPH_AUDIOENCODERCAPABILITIES, */ IAudioEncoderCapabilities_init, NULL, NULL, NULL, NULL },
    { /* MPH_AUDIOIODEVICECAPABILITIES, */ IAudioIODeviceCapabilities_init, NULL, NULL, NULL,
        NULL },
    { /* MPH_BASSBOOST, */ IBassBoost_init, NULL, IBassBoost_deinit, IBassBoost_Expose, NULL },
    { /* MPH_BUFFERQUEUE, */ IBufferQueue_init, NULL, IBufferQueue_deinit, NULL, NULL },
    { /* MPH_DEVICEVOLUME, */ IDeviceVolume_init, NULL, NULL, NULL, NULL },
    { /* MPH_DYNAMICINTERFACEMANAGEMENT, */ IDynamicInterfaceManagement_init, NULL, NULL, NULL,
        NULL },
    { /* MPH_DYNAMICSOURCE, */ IDynamicSource_init, NULL, NULL, NULL, NULL },
    { /* MPH_EFFECTSEND, */ IEffectSend_init, NULL, NULL, NULL, NULL },
    { /* MPH_ENGINE, */ IEngine_init, NULL, IEngine_deinit, NULL, NULL },
    { /* MPH_ENGINECAPABILITIES, */ IEngineCapabilities_init, NULL, NULL, NULL, NULL },
    { /* MPH_ENVIRONMENTALREVERB, */ IEnvironmentalReverb_init, NULL, IEnvironmentalReverb_deinit,
        IEnvironmentalReverb_Expose, NULL },
    { /* MPH_EQUALIZER, */ IEqualizer_init, NULL, IEqualizer_deinit, IEqualizer_Expose, NULL },
    { /* MPH_LED, */ ILEDArray_init, NULL, NULL, NULL, NULL },
    { /* MPH_METADATAEXTRACTION, */ IMetadataExtraction_init, NULL, NULL, NULL, NULL },
    { /* MPH_METADATATRAVERSAL, */ IMetadataTraversal_init, NULL, NULL, NULL, NULL },
    { /* MPH_MIDIMESSAGE, */ IMIDIMessage_init, NULL, NULL, NULL, NULL },
    { /* MPH_MIDITIME, */ IMIDITime_init, NULL, NULL, NULL, NULL },
    { /* MPH_MIDITEMPO, */ IMIDITempo_init, NULL, NULL, NULL, NULL },
    { /* MPH_MIDIMUTESOLO, */ IMIDIMuteSolo_init, NULL, NULL, NULL, NULL },
    { /* MPH_MUTESOLO, */ IMuteSolo_init, NULL, NULL, NULL, NULL },
    { /* MPH_NULL, */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_OBJECT, */ IObject_init, NULL, IObject_deinit, NULL, NULL },
    { /* MPH_OUTPUTMIX, */ IOutputMix_init, NULL, IOutputMix_deinit, NULL, NULL },
    { /* MPH_PITCH, */ IPitch_init, NULL, NULL, NULL, NULL },
    { /* MPH_PLAY, */ IPlay_init, NULL, NULL, NULL, NULL },
    { /* MPH_PLAYBACKRATE, */ IPlaybackRate_init, NULL, NULL, NULL, NULL },
    { /* MPH_PREFETCHSTATUS, */ IPrefetchStatus_init, NULL, NULL, NULL, NULL },
    { /* MPH_PRESETREVERB, */ IPresetReverb_init, NULL, IPresetReverb_deinit,
        IPresetReverb_Expose, NULL },
    { /* MPH_RATEPITCH, */ IRatePitch_init, NULL, NULL, NULL, NULL },
    { /* MPH_RECORD, */ IRecord_init, NULL, NULL, NULL, NULL },
    { /* MPH_SEEK, */ ISeek_init, NULL, NULL, NULL, NULL },
    { /* MPH_THREADSYNC, */ IThreadSync_init, NULL, IThreadSync_deinit, NULL, NULL },
    { /* MPH_VIBRA, */ IVibra_init, NULL, NULL, NULL, NULL },
    { /* MPH_VIRTUALIZER, */ IVirtualizer_init, NULL, IVirtualizer_deinit, IVirtualizer_Expose,
        NULL },
    { /* MPH_VISUALIZATION, */ IVisualization_init, NULL, NULL, NULL, NULL },
    { /* MPH_VOLUME, */ IVolume_init, NULL, NULL, NULL, NULL },
// Wilhelm desktop extended interfaces
    { /* MPH_OUTPUTMIXEXT, */ IOutputMixExt_init, NULL, NULL, NULL, NULL },
// Android API level 9 extended interfaces
    { /* MPH_ANDROIDEFFECT */ IAndroidEffect_init, NULL, IAndroidEffect_deinit, NULL, NULL },
    { /* MPH_ANDROIDEFFECTCAPABILITIES */ IAndroidEffectCapabilities_init, NULL,
        IAndroidEffectCapabilities_deinit, IAndroidEffectCapabilities_Expose, NULL },
    { /* MPH_ANDROIDEFFECTSEND */ IAndroidEffectSend_init, NULL, NULL, NULL, NULL },
    { /* MPH_ANDROIDCONFIGURATION */ IAndroidConfiguration_init, NULL, NULL, NULL, NULL },
    { /* MPH_ANDROIDSIMPLEBUFFERQUEUE */ IBufferQueue_init /* alias */, NULL, NULL, NULL, NULL },
// Android API level 10 extended interfaces
    { /* MPH_ANDROIDBUFFERQUEUESOURCE */ IAndroidBufferQueue_init, NULL, IAndroidBufferQueue_deinit,
        NULL, NULL },
// OpenMAX AL 1.0.1 interfaces
    { /* MPH_XAAUDIODECODERCAPABILITIES */ IAudioDecoderCapabilities_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAAUDIOENCODER */ IAudioEncoder_init, NULL, IAudioEncoder_deinit, NULL, NULL },
    { /* MPH_XAAUDIOENCODERCAPABILITIES */ IAudioEncoderCapabilities_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAAUDIOIODEVICECAPABILITIES */ IAudioIODeviceCapabilities_init, NULL, IAudioIODeviceCapabilities_deinit, NULL, NULL },
    { /* MPH_XACAMERA */ ICameraDevice_init, NULL, NULL, NULL, NULL },
    { /* MPH_XACAMERACAPABILITIES */ ICameraCapabilities_init, NULL, ICameraCapabilities_deinit, NULL, NULL },
    { /* MPH_XACONFIGEXTENSION */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XADEVICEVOLUME */ IDeviceVolume_init, NULL, NULL, NULL, NULL },
    { /* MPH_XADYNAMICINTERFACEMANAGEMENT 59 */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XADYNAMICSOURCE */ IDynamicSource_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAENGINE */ IXAEngine_init, NULL, IXAEngine_deinit, NULL, NULL },
    { /* MPH_XAEQUALIZER */ IEqualizer_init, NULL, IEqualizer_deinit, NULL, NULL },
    { /* MPH_XAIMAGECONTROLS */ IImageControls_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAIMAGEDECODERCAPABILITIES */ IImageDecoderCapabilities_init, NULL, IImageDecoderCapabilities_deinit, NULL, NULL },
    { /* MPH_XAIMAGEEFFECTS */ IImageEffects_init, NULL, IImageEffects_deinit, NULL, NULL },
    { /* MPH_XAIMAGEENCODER */ IImageEncoder_init, NULL, IImageEncoder_deinit, NULL, NULL },
    { /* MPH_XAIMAGEENCODERCAPABILITIES */ IImageEncoderCapabilities_init, NULL, IImageEncoderCapabilities_deinit, NULL, NULL },
    { /* MPH_XALED */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XAMETADATAEXTRACTION */ IMetadataExtraction_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAMETADATAINSERTION */ IMetadataInsertion_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAMETADATATRAVERSAL */ IMetadataTraversal_init, NULL, NULL, NULL, NULL },
//  { /* MPH_XANULL */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XAOBJECT */ IObject_init, NULL, IObject_deinit, NULL, NULL },
    { /* MPH_XAOUTPUTMIX */ IOutputMix_init, NULL, IOutputMix_deinit, NULL, NULL },
    { /* MPH_XAPLAY */ IPlay_init, NULL, IPlay_deinit, NULL, NULL },
    { /* MPH_XAPLAYBACKRATE */ IPlaybackRate_init, NULL, IPlaybackRate_deinit, NULL, NULL },
    { /* MPH_XAPREFETCHSTATUS, */ IPrefetchStatus_init, NULL, NULL, NULL, NULL },
    { /* MPH_XARADIO */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XARDS */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XARECORD */ IRecord_init, NULL, IRecord_deinit, NULL, NULL },
    { /* MPH_XASEEK */ ISeek_init, NULL, ISeek_deinit, NULL, NULL },
    { /* MPH_XASNAPSHOT */ ISnapshot_init, NULL, ISnapshot_deinit, NULL, NULL },
    { /* MPH_XASTREAMINFORMATION */ IStreamInformation_init, NULL, IStreamInformation_deinit,
        NULL, NULL },
    { /* MPH_XATHREADSYNC */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XAVIBRA */ NULL, NULL, NULL, NULL, NULL },
    { /* MPH_XAVIDEODECODERCAPABILITIES */ IVideoDecoderCapabilities_init, NULL,
            IVideoDecoderCapabilities_deinit, IVideoDecoderCapabilities_expose, NULL },
    { /* MPH_XAVIDEOENCODER */ IVideoEncoder_init, NULL, IVideoEncoder_deinit, NULL, NULL },
    { /* MPH_XAVIDEOENCODERCAPABILITIES */ IVideoEncoderCapabilities_init, NULL,
            IVideoEncoderCapabilities_deinit, IVideoEncoderCapabilities_expose, NULL },
    { /* MPH_XAVIDEOPOSTPROCESSING */ IVideoPostProcessing_init, NULL, NULL, NULL, NULL },
    { /* MPH_XAVOLUME, */ IVolume_init, NULL, IVolume_deinit, NULL, NULL },
    { /* MPH_NVCAMERAEXTCAPABILITIES, */ INvCameraExtCapabilities_init, NULL,
        INvCameraExtCapabilities_deinit, NULL, NULL},
    { /* MPH_NVVIDEOENCODEREXTCAPABILITIES, */ INvVideoEncoderExtCapabilities_init, NULL,
        INvVideoEncoderExtCapabilities_deinit, INvVideoEncoderExtCapabilities_expose, NULL},
    { /* MPH_NVVIDEODECODEREXTCAPABILITIES, */ INvVideoDecoderExtCapabilities_init, NULL,
        INvVideoDecoderExtCapabilities_deinit, INvVideoDecoderExtCapabilities_expose, NULL},
    { /* MPH_NVVIDEODECODER, */ INvVideoDecoder_init, NULL,
        INvVideoDecoder_deinit, NULL, NULL},
    { /* MPH_NVVIDEOENCODEREXT, */ INvVideoEncoderExt_init, NULL,
        INvVideoEncoderExt_deinit, NULL, NULL},
    { /* MPH_GETCAPABILITIESOFINTERFACE, */ IGetCapabilitiesOfInterface_init, NULL,
        IGetCapabilitiesOfInterface_deinit, NULL, NULL},
    { /* MPH_NVPUSHAPP, */ INvPushApp_init, NULL, INvPushApp_deinit, NULL, NULL},
    { /* MPH_NVBURSTMODE, */ INvBurstMode_init, NULL, INvBurstMode_deinit, NULL, NULL},
};


/** \brief Construct a new instance of the specified class, exposing selected interfaces */

IObject *construct(const ClassTable *clazz, unsigned exposedMask, SLEngineItf engine,
    SLuint32 numInterfaces,const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    IObject *thiz;
    // Do not change this to malloc; we depend on the object being memset to zero
    thiz = (IObject *) calloc(1, clazz->mSize);
    if (NULL != thiz) {
        SL_LOGV("construct %s at %p", clazz->mName, thiz);
        unsigned lossOfControlMask = 0;
        // a NULL engine means we are constructing the engine
        IEngine *thisEngine = (IEngine *) engine;
        if (NULL == thisEngine) {
            // thisEngine = &((CEngine *) thiz)->mEngine;
            thiz->mEngine = (CEngine *) thiz;
        } else {
            thiz->mEngine = (CEngine *) thisEngine->mThis;
            interface_lock_exclusive(thisEngine);
            if (MAX_INSTANCE <= thisEngine->mInstanceCount) {
                SL_LOGE("Too many objects");
                interface_unlock_exclusive(thisEngine);
                free(thiz);
                return NULL;
            }
            // pre-allocate a pending slot, but don't assign bit from mInstanceMask yet
            ++thisEngine->mInstanceCount;
            assert(((unsigned) ~0) != thisEngine->mInstanceMask);
            interface_unlock_exclusive(thisEngine);
            // const, no lock needed
            if (thisEngine->mLossOfControlGlobal) {
                lossOfControlMask = ~0;
            }
        }
        thiz->mLossOfControlMask = lossOfControlMask;
        thiz->mClass = clazz;
        const struct iid_vtable *x = clazz->mInterfaces;
        SLuint8 *interfaceStateP = thiz->mInterfaceStates;
        SLuint32 index;
        unsigned char i, MPH;
        int required_mphvalues[numInterfaces];
        for (i = 0; i < numInterfaces; ++i) {
            SLInterfaceID iid = pInterfaceIds[i];
            MPH = IID_to_MPH(iid);
            SLuint32 index = clazz->mMPH_to_index[MPH];
            if ((x+index)->mMPH != MPH)
            {
                SL_LOGV("construct: [%d] Aliased MPH %d maps to %d, index:%d for %s",
                    i, MPH, (x + index)->mMPH, index, clazz->mName);
                MPH = (x+index)->mMPH;
            }
            required_mphvalues[i] = MPH;
        }

        for (index = 0; index < clazz->mInterfaceCount; ++index, ++x, exposedMask >>= 1) {
            SLuint8 state;
            // initialize all interfaces which are implicit and are explicitly requested
            const struct MPH_init *mi = &MPH_init_table[x->mMPH];
            VoidHook init = NULL;
            if ((x->mInterface == INTERFACE_IMPLICIT) ||
                (x->mInterface == INTERFACE_IMPLICIT_PREREALIZE))
            {
                 init = mi->mInit;
            }
            else
            {
                for (i=0;i<numInterfaces;++i) {
                     if (x->mMPH == required_mphvalues[i] && pInterfaceRequired[i]) {
                             SL_LOGV("Explicit Interface Required, MPH %d ",x->mMPH);
                             init = mi->mInit;
                             break;
                     }
                }
            }

            if (NULL != init) {
                void *self = (char *) thiz + x->mOffset;
                // IObject does not have an mThis, so [1] is not always defined
                if (index) {
                    ((IObject **) self)[1] = thiz;
                }
                // call the initialization hook
                (*init)(self);
                // IObject does not require a call to GetInterface
                if (index) {
                    // This trickery invalidates the v-table until GetInterface
                    ((size_t *) self)[0] ^= ~0;
                }
                // if interface is exposed, also call the optional expose hook
                BoolHook expose;
                state = (exposedMask & 1) && ((NULL == (expose = mi->mExpose)) || (*expose)(self)) ?
                        INTERFACE_EXPOSED : INTERFACE_INITIALIZED;
                // FIXME log or report to application if an expose hook on a
                // required explicit interface fails at creation time
            } else {
                state = INTERFACE_UNINITIALIZED;
            }
            *interfaceStateP++ = state;
        }
        // note that the new object is not yet published; creator must call IObject_Publish
    }
    return thiz;
}
