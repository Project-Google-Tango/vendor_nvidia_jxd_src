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

#ifdef ANDROID
#include "media/AudioEffect.h"
#include "hardware/audio_effect.h"
#endif

/* Interface structures */

typedef struct Object_interface {
    const struct SLObjectItf_ *mItf;    // const
    // field mThis would be redundant within an IObject, so we substitute mEngine
    CEngine *mEngine;               // const
    const ClassTable *mClass;       // const
    SLuint32 mInstanceID;           // const for debugger and for RPC, 0 means unpublished
    slObjectCallback mCallback;
    void *mContext;
    unsigned mGottenMask;           ///< bit-mask of interfaces exposed or added, then gotten
    unsigned mLossOfControlMask;    // interfaces with loss of control enabled
    unsigned mAttributesMask;       // attributes which have changed since last sync
#if USE_PROFILES & USE_PROFILES_BASE
    SLint32 mPriority;
#endif
    pthread_mutex_t mMutex;
#ifdef USE_DEBUG
    // Only keep the pthread_t, not the kernel tid, because pthread_self() is very fast
    // (typically just arithmetic on the stack pointer). But a gettid() is a kernel call
    // and so too slow to do every time a mutex is acquired. However, we can determine
    // the kernel tid from the pthread_t.
    pthread_t mOwner;
    const char *mFile;
    int mLine;
    volatile int32_t mGeneration;   // read without a lock, incremented with a lock
#endif
    pthread_cond_t mCond;
    SLuint8 mState;                 // really SLuint32, but SLuint8 to save space
#if USE_PROFILES & USE_PROFILES_BASE
    SLuint8 mPreemptable;           // really SLboolean, but SLuint8 to save space
#else
    SLuint8 mPadding;
#endif
    SLuint8 mStrongRefCount;        // number of strong references to this object
    // (object cannot be destroyed as long as > 0, and referrers _prefer_ it stay in Realized state)
    // for best alignment, do not add any fields here
#define INTERFACES_Default 1
    SLuint8 mInterfaceStates[INTERFACES_Default];    // state of each of interface
    // do not add any fields here
} IObject;

#include "locks.h"

typedef struct {
    const struct SL3DCommitItf_ *mItf;
    IObject *mThis;
    SLboolean mDeferred;
    SLuint32 mGeneration;   // incremented each master clock cycle
    SLuint32 mWaiting;      // number of threads waiting in Commit
} I3DCommit;

enum CartesianSphericalActive {
    CARTESIAN_COMPUTED_SPHERICAL_SET,
    CARTESIAN_REQUESTED_SPHERICAL_SET,
    CARTESIAN_UNKNOWN_SPHERICAL_SET,
    CARTESIAN_SET_SPHERICAL_COMPUTED,   // not in 1.0.1
    CARTESIAN_SET_SPHERICAL_REQUESTED,  // not in 1.0.1
    CARTESIAN_SET_SPHERICAL_UNKNOWN
};

typedef struct {
    const struct SL3DDopplerItf_ *mItf;
    IObject *mThis;
    // The API allows client to specify either Cartesian and spherical velocities.
    // But an implementation will likely prefer one or the other. So for
    // maximum portablity, we maintain both units and an indication of which
    // unit was set most recently. In addition, we keep a flag saying whether
    // the other unit has been derived yet. It can take significant time
    // to compute the other unit, so this may be deferred to another thread.
    // For this reason we also keep an indication of whether the secondary
    // has been computed yet, and its accuracy.
    // Though only one unit is primary at a time, a union is inappropriate:
    // the application might read in both units (not in 1.0.1),
    // and due to multi-threading concerns.
    SLVec3D mVelocityCartesian;
    struct {
        SLmillidegree mAzimuth;
        SLmillidegree mElevation;
        SLmillidegree mSpeed;
    } mVelocitySpherical;
    enum CartesianSphericalActive mVelocityActive;
    SLpermille mDopplerFactor;
} I3DDoppler;

typedef struct {
    const struct SL3DGroupingItf_ *mItf;
    IObject *mThis;
    C3DGroup *mGroup;   // strong reference to associated group or NULL
} I3DGrouping;

enum AnglesVectorsActive {
    ANGLES_COMPUTED_VECTORS_SET,    // not in 1.0.1
    ANGLES_REQUESTED_VECTORS_SET,   // not in 1.0.1
    ANGLES_UNKNOWN_VECTORS_SET,
    ANGLES_SET_VECTORS_COMPUTED,
    ANGLES_SET_VECTORS_REQUESTED,
    ANGLES_SET_VECTORS_UNKNOWN
};

typedef struct {
    const struct SL3DLocationItf_ *mItf;
    IObject *mThis;
    SLVec3D mLocationCartesian;
    struct {
        SLmillidegree mAzimuth;
        SLmillidegree mElevation;
        SLmillimeter mDistance;
    } mLocationSpherical;
    enum CartesianSphericalActive mLocationActive;
    struct {
        SLmillidegree mHeading;
        SLmillidegree mPitch;
        SLmillidegree mRoll;
    } mOrientationAngles;
    struct {
        SLVec3D mFront;
        SLVec3D mAbove;
        SLVec3D mUp;
    } mOrientationVectors;
    enum AnglesVectorsActive mOrientationActive;
    // Rotations can be slow, so are deferred.
    SLmillidegree mTheta;
    SLVec3D mAxis;
    SLboolean mRotatePending;
} I3DLocation;

typedef struct {
    const struct SL3DMacroscopicItf_ *mItf;
    IObject *mThis;
    struct {
        SLmillimeter mWidth;
        SLmillimeter mHeight;
        SLmillimeter mDepth;
    } mSize;
    struct {
        SLmillimeter mHeading;
        SLmillimeter mPitch;
        SLmillimeter mRoll;
    } mOrientationAngles;
    struct {
        SLVec3D mFront;
        SLVec3D mAbove;
        SLVec3D mUp;
    } mOrientationVectors;
    enum AnglesVectorsActive mOrientationActive;
    // Rotations can be slow, so are deferred.
    SLmillidegree mTheta;
    SLVec3D mAxis;
    SLboolean mRotatePending;
} I3DMacroscopic;

typedef struct {
    const struct SL3DSourceItf_ *mItf;
    IObject *mThis;
    SLboolean mHeadRelative;
    SLboolean mRolloffMaxDistanceMute;
    SLmillimeter mMaxDistance;
    SLmillimeter mMinDistance;
    SLmillidegree mConeInnerAngle;
    SLmillidegree mConeOuterAngle;
    SLmillibel mConeOuterLevel;
    SLpermille mRolloffFactor;
    SLpermille mRoomRolloffFactor;
    SLuint8 mDistanceModel;
} I3DSource;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLAudioDecoderCapabilitiesItf_ *mItf;
#else
    const struct XAAudioDecoderCapabilitiesItf_ *mItf;
#endif
    IObject *mThis;
} IAudioDecoderCapabilities;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLAudioEncoderItf_ *mItf;
#else
    const struct XAAudioEncoderItf_ *mItf;
#endif
    IObject *mThis;
    SLAudioEncoderSettings mSettings;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IAudioEncoder;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLAudioEncoderCapabilitiesItf_ *mItf;
#else
    const struct XAAudioEncoderCapabilitiesItf_ *mItf;
#endif
    IObject *mThis;
} IAudioEncoderCapabilities;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLAudioIODeviceCapabilitiesItf_ *mItf;
#else
    const struct XAAudioIODeviceCapabilitiesItf_ *mItf;
#endif
    IObject *mThis;
    slAvailableAudioInputsChangedCallback mAvailableAudioInputsChangedCallback;
    void *mAvailableAudioInputsChangedContext;
    slAvailableAudioOutputsChangedCallback mAvailableAudioOutputsChangedCallback;
    void *mAvailableAudioOutputsChangedContext;
    slDefaultDeviceIDMapChangedCallback mDefaultDeviceIDMapChangedCallback;
    void *mDefaultDeviceIDMapChangedContext;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IAudioIODeviceCapabilities;

typedef struct {
    const struct SLBassBoostItf_ *mItf;
    IObject *mThis;
    SLboolean mEnabled;
    SLpermille mStrength;
#if defined(ANDROID)
    effect_descriptor_t mBassBoostDescriptor;
    android::sp<android::AudioEffect> mBassBoostEffect;
#endif
} IBassBoost;

typedef struct BufferQueue_interface {
    const struct SLBufferQueueItf_ *mItf;
    IObject *mThis;
    SLBufferQueueState mState;
    slBufferQueueCallback mCallback;
    void *mContext;
    // originally SLuint32, but range-checked down to SLuint16
    SLuint16 mNumBuffers;
    /*SLboolean*/ SLuint16 mClearRequested;
    BufferHeader *mArray;
    BufferHeader *mFront, *mRear;
#ifdef ANDROID
    SLuint32 mSizeConsumed;
#endif
    // saves a malloc in the typical case
#define BUFFER_HEADER_TYPICAL 4
    BufferHeader mTypical[BUFFER_HEADER_TYPICAL+1];
} IBufferQueue;

#define MAX_DEVICE 2    // hard-coded array size for default in/out

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLDeviceVolumeItf_ *mItf;
#else
    const struct XADeviceVolumeItf_ *mItf;
#endif
    IObject *mThis;
    SLint32 mVolume[MAX_DEVICE];
} IDeviceVolume;

typedef struct {
    const struct SLDynamicInterfaceManagementItf_ *mItf;
    IObject *mThis;
    slDynamicInterfaceManagementCallback mCallback;
    void *mContext;
} IDynamicInterfaceManagement;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLDynamicSourceItf_ *mItf;
#else
    const struct XADynamicSourceItf_ *mItf;
#endif
    IObject *mThis;
    SLDataSource *mDataSource;
} IDynamicSource;

// private

struct EnableLevel {
    SLboolean mEnable;
    SLmillibel mSendLevel;
};

// indexes into IEffectSend.mEnableLevels

#define AUX_ENVIRONMENTALREVERB 0
#define AUX_PRESETREVERB        1
#define AUX_MAX                 2

typedef struct {
    const struct SLEffectSendItf_ *mItf;
    IObject *mThis;
    struct EnableLevel mEnableLevels[AUX_MAX];  // wet enable and volume per effect type
} IEffectSend;

typedef struct Engine_interface {
    const struct SLEngineItf_ *mItf;
    IObject *mThis;
    SLboolean mLossOfControlGlobal;
#ifdef USE_SDL
    COutputMix *mOutputMix; // SDL pulls PCM from an arbitrary IOutputMixExt
#endif
    // Each engine is its own universe.
    SLuint32 mInstanceCount;
    unsigned mInstanceMask; // 1 bit per active object
    unsigned mChangedMask;  // objects which have changed since last sync
#define MAX_INSTANCE 32     // maximum active objects per engine, see mInstanceMask
    IObject *mInstances[MAX_INSTANCE];
    SLboolean mShutdown;
    SLboolean mShutdownAck;
    // SLuint32 mVersion;      // 0xXXYYZZ where XX=major, YY=minor, ZZ=step
} IEngine;

typedef struct {
    const struct SLEngineCapabilitiesItf_ *mItf;
    IObject *mThis;
    SLboolean mThreadSafe;
    // const
    SLuint32 mMaxIndexLED;
    SLuint32 mMaxIndexVibra;
} IEngineCapabilities;

typedef struct {
    const struct SLEnvironmentalReverbItf_ *mItf;
    IObject *mThis;
    SLEnvironmentalReverbSettings mProperties;
#if defined(ANDROID)
    effect_descriptor_t mEnvironmentalReverbDescriptor;
    android::sp<android::AudioEffect> mEnvironmentalReverbEffect;
#endif
} IEnvironmentalReverb;

struct EqualizerBand {
    SLmilliHertz mMin;
    SLmilliHertz mCenter;
    SLmilliHertz mMax;
};

#if defined(ANDROID)
#define MAX_EQ_BANDS 0
#else
#define MAX_EQ_BANDS 4  // compile-time limit, runtime limit may be smaller
#endif

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLEqualizerItf_ *mItf;
#else
    const struct XAEqualizerItf_ *mItf;
#endif
    IObject *mThis;
    SLboolean mEnabled;
    SLuint16 mPreset;
#if 0 < MAX_EQ_BANDS
    SLmillibel mLevels[MAX_EQ_BANDS];
#endif
    // const to end of struct
    SLuint16 mNumPresets;
    SLuint16 mNumBands;
#if !defined(ANDROID)
    const struct EqualizerBand *mBands;
    const struct EqualizerPreset *mPresets;
#endif
    SLmillibel mBandLevelRangeMin;
    SLmillibel mBandLevelRangeMax;
#if defined(ANDROID)
    effect_descriptor_t mEqDescriptor;
    android::sp<android::AudioEffect> mEqEffect;
#endif
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IEqualizer;

#define MAX_LED_COUNT 32

typedef struct {
    const struct SLLEDArrayItf_ *mItf;
    IObject *mThis;
    SLuint32 mLightMask;
    SLHSL mColors[MAX_LED_COUNT];
    // const
    SLuint8 mCount;
} ILEDArray;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLMetadataExtractionItf_ *mItf;
#else
    const struct XAMetadataExtractionItf_ *mItf;
#endif
    IObject *mThis;
    SLuint32 mKeySize;
    const void *mKey;
    SLuint32 mKeyEncoding;
    const SLchar *mValueLangCountry;
    SLuint32 mValueEncoding;
    SLuint8 mFilterMask;
    int mKeyFilter;
} IMetadataExtraction;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLMetadataTraversalItf_ *mItf;
#else
    const struct XAMetadataTraversalItf_ *mItf;
#endif
    IObject *mThis;
    SLuint32 mIndex;
    SLuint32 mMode;
    SLuint32 mCount;
    SLuint32 mSize;
} IMetadataTraversal;

typedef struct {
    const struct SLMIDIMessageItf_ *mItf;
    IObject *mThis;
    slMetaEventCallback mMetaEventCallback;
    void *mMetaEventContext;
    slMIDIMessageCallback mMessageCallback;
    void *mMessageContext;
    SLuint8 mMessageTypes;
} IMIDIMessage;

typedef struct {
    const struct SLMIDIMuteSoloItf_ *mItf;
    IObject *mThis;
    SLuint16 mChannelMuteMask;
    SLuint16 mChannelSoloMask;
    SLuint32 mTrackMuteMask;
    SLuint32 mTrackSoloMask;
    // const
    SLuint16 mTrackCount;
} IMIDIMuteSolo;

typedef struct {
    const struct SLMIDITempoItf_ *mItf;
    IObject *mThis;
    SLuint32 mTicksPerQuarterNote;
    SLuint32 mMicrosecondsPerQuarterNote;
} IMIDITempo;

typedef struct {
    const struct SLMIDITimeItf_ *mItf;
    IObject *mThis;
    SLuint32 mDuration;
    SLuint32 mPosition;
    SLuint32 mStartTick;
    SLuint32 mNumTicks;
} IMIDITime;

typedef struct {
    const struct SLMuteSoloItf_ *mItf;
    IObject *mThis;
    // fields that were formerly here are now at CAudioPlayer
} IMuteSolo;

#define MAX_TRACK 32        // see mActiveMask

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLOutputMixItf_ *mItf;
#else
    const struct XAOutputMixItf_ *mItf;
#endif
    IObject *mThis;
#ifndef LINUX_OMXAL
    slMixDeviceChangeCallback mCallback;
#else
    xaMixDeviceChangeCallback mCallback;
#endif
    void *mContext;
} IOutputMix;

#ifdef USE_OUTPUTMIXEXT
typedef struct {
    const struct SLOutputMixExtItf_ *mItf;
    IObject *mThis;
    unsigned mActiveMask;   // 1 bit per active track
    Track mTracks[MAX_TRACK];
    SLboolean mDestroyRequested;    ///< Mixer to acknowledge application's call to Object::Destroy
} IOutputMixExt;
#endif

typedef struct {
    const struct SLPitchItf_ *mItf;
    IObject *mThis;
    SLpermille mPitch;
    // const
    SLpermille mMinPitch;
    SLpermille mMaxPitch;
} IPitch;

typedef struct Play_interface {
#ifndef LINUX_OMXAL
    const struct SLPlayItf_ *mItf;
#else
    const struct XAPlayItf_ *mItf;
#endif
    IObject *mThis;
    SLuint32 mState;
    // next 2 fields are read-only to application
    SLmillisecond mDuration;
    SLmillisecond mPosition;
    slPlayCallback mCallback;
    void *mContext;
    SLuint32 mEventFlags;
    // the ISeek trick of using a distinct value doesn't work here because it's readable by app
    SLmillisecond mMarkerPosition;
    SLmillisecond mPositionUpdatePeriod; // Zero means do not do position updates (FIXME ~0)
#ifdef USE_OUTPUTMIXEXT
    SLuint32 mFrameUpdatePeriod;         // mPositionUpdatePeriod in frame units
    SLmillisecond mLastSeekPosition;     // Last known accurate position, set at Seek
    SLuint32 mFramesSinceLastSeek;       // Frames mixed since last known accurate position
    SLuint32 mFramesSincePositionUpdate; // Frames mixed since last position update callback
#endif
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IPlay;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLPlaybackRateItf_ *mItf;
#else
    const struct XAPlaybackRateItf_ *mItf;
#endif
    IObject *mThis;
    SLpermille mRate;
    SLuint32 mProperties;
    // const after initialization
    SLpermille mMinRate;
    SLpermille mMaxRate;
    SLpermille mStepSize;
    SLuint32 mCapabilities;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IPlaybackRate;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLPrefetchStatusItf_ *mItf;
#else
    const struct XAPrefetchStatusItf_ *mItf;
#endif
    IObject *mThis;
    SLuint32 mStatus;
    SLpermille mLevel;
    slPrefetchCallback mCallback;
    void *mContext;
    SLuint32 mCallbackEventsMask;
    SLpermille mFillUpdatePeriod;
#ifdef ANDROID
    /** FIXME used to call PrefetchStatus callback with object unlocked prior to return from API */
    slPrefetchCallback mDeferredPrefetchCallback;
    void *mDeferredPrefetchContext;
    SLuint32 mDeferredPrefetchEvents;
#endif
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IPrefetchStatus;

typedef struct {
    const struct SLPresetReverbItf_ *mItf;
    IObject *mThis;
    SLuint16 mPreset;
#if defined(ANDROID)
    effect_descriptor_t mPresetReverbDescriptor;
    android::sp<android::AudioEffect> mPresetReverbEffect;
#endif
} IPresetReverb;

typedef struct {
    const struct SLRatePitchItf_ *mItf;
    IObject *mThis;
    SLpermille mRate;
    // const
    SLpermille mMinRate;
    SLpermille mMaxRate;
} IRatePitch;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLRecordItf_ *mItf;
#else
    const struct XARecordItf_ *mItf;
#endif
    IObject *mThis;
    SLuint32 mState;
    SLmillisecond mDurationLimit;
    SLmillisecond mPosition;
    slRecordCallback mCallback;
    void *mContext;
    SLuint32 mCallbackEventsMask;
    SLmillisecond mMarkerPosition;
    SLmillisecond mPositionUpdatePeriod;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IRecord;

typedef struct {
#ifndef LINUX_OMXAL
    const struct SLSeekItf_ *mItf;
#else
    const struct XASeekItf_ *mItf;
#endif
    IObject *mThis;
    SLmillisecond mPos;     // mPos != SL_TIME_UNKNOWN means pending seek request
    SLboolean mLoopEnabled;
    SLmillisecond mStartPos;
    SLmillisecond mEndPos;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} ISeek;

typedef struct {
    const struct SLThreadSyncItf_ *mItf;
    IObject *mThis;
    SLboolean mInCriticalSection;
    SLuint32 mWaiting;  // number of threads waiting
    pthread_t mOwner;
} IThreadSync;

typedef struct {
    const struct SLVibraItf_ *mItf;
    IObject *mThis;
    SLboolean mVibrate;
    SLmilliHertz mFrequency;
    SLpermille mIntensity;
} IVibra;

typedef struct {
    const struct SLVirtualizerItf_ *mItf;
    IObject *mThis;
    SLboolean mEnabled;
    SLpermille mStrength;
#if defined(ANDROID)
    effect_descriptor_t mVirtualizerDescriptor;
    android::sp<android::AudioEffect> mVirtualizerEffect;
#endif
} IVirtualizer;

typedef struct {
    const struct SLVisualizationItf_ *mItf;
    IObject *mThis;
    slVisualizationCallback mCallback;
    void *mContext;
    SLmilliHertz mRate;
} IVisualization;

typedef struct /*Volume_interface*/ {
#ifndef LINUX_OMXAL
    const struct SLVolumeItf_ *mItf;
#else
    const struct XAVolumeItf_ *mItf;
#endif
    IObject *mThis;
    // Values as specified by the application
    SLmillibel mLevel;
    SLpermille mStereoPosition;
    SLuint8 /*SLboolean*/ mMute;
    SLuint8 /*SLboolean*/ mEnableStereoPosition;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IVolume;

typedef struct {
    const struct XAEngineItf_ *mItf;
    IObject *mThis;
} IXAEngine;

#define NB_SUPPORTED_STREAMS 1 // only one (video) stream supported in this implementation
typedef struct {
    const struct XAStreamInformationItf_ *mItf;
    IObject *mThis;
    xaStreamEventChangeCallback mCallback;
    void *mContext;
    XAboolean mActiveStreams[NB_SUPPORTED_STREAMS];
#ifdef ANDROID
    android::Vector<StreamInfo> mStreamInfoTable;
#endif
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IStreamInformation;

typedef struct {
    const struct XAVideoDecoderCapabilitiesItf_ *mItf;
    IObject *mThis;
} IVideoDecoderCapabilities;

/* Class structures */

/*typedef*/ struct C3DGroup_struct {
    IObject mObject;
#define INTERFACES_3DGroup 6 // see MPH_to_3DGroup in MPH_to.c for list of interfaces
    SLuint8 mInterfaceStates2[INTERFACES_3DGroup - INTERFACES_Default];
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    I3DLocation m3DLocation;
    I3DDoppler m3DDoppler;
    I3DSource m3DSource;
    I3DMacroscopic m3DMacroscopic;
    // remaining are per-instance private fields not associated with an interface
    unsigned mMemberMask;   // set of member objects
} /*C3DGroup*/;

#ifdef ANDROID

// FIXME Move these into the I... section above

typedef struct {
    const struct SLAndroidEffectItf_ *mItf;
    IObject *mThis;
    android::KeyedVector<SLuint32, android::AudioEffect* > *mEffects;
} IAndroidEffect;

typedef struct {
    const struct SLAndroidEffectCapabilitiesItf_ *mItf;
    IObject *mThis;
    SLuint32 mNumFx;
    effect_descriptor_t* mFxDescriptors;
} IAndroidEffectCapabilities;

typedef struct {
    const struct SLAndroidEffectSendItf_ *mItf;
    IObject *mThis;
    // only one send per interface for now (1 bus)
    SLboolean mEnabled;
    SLmillibel mSendLevel; //android::KeyedVector<SLuint32, SLmillibel> mSendLevels;
} IAndroidEffectSend;

typedef struct {
    const struct SLAndroidConfigurationItf_ *mItf;
    IObject *mThis;
} IAndroidConfiguration;

typedef struct {
    const struct SLAndroidBufferQueueItf_ *mItf;
    IObject *mThis;
    SLAndroidBufferQueueState mState;
    slAndroidBufferQueueCallback mCallback;
    SLuint32 mCallbackEventsMask;
    void *mContext;
    SLuint16 mNumBuffers;
    AndroidBufferType_type mBufferType;
    AdvancedBufferHeader *mBufferArray;
    AdvancedBufferHeader *mFront, *mRear;
} IAndroidBufferQueue;

#endif

typedef struct {
    const struct XACameraItf_ *mItf;
    IObject *mThis;
    xaCameraCallback mCallback;
    void *mContext;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} ICameraDevice;

typedef struct {
    const struct XAImageControlsItf_ *mItf;
    IObject *mThis;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IImageControls;

typedef struct {
    const struct XAImageEffectsItf_ *mItf;
    IObject *mThis;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IImageEffects;

typedef struct {
    const struct XAVideoPostProcessingItf_ *mItf;
    IObject *mThis;

    XAuint32 mSetupFlag;
    XAmillidegree mRotation;
    XAuint32 mScaleOptions;
    XAuint32 mBackgroundColor;
    XAuint32 mRenderingHints;
    XARectangle mSourceRect;
    XARectangle mDestRect;
    XAuint32 mMirror;
    XAboolean mbValid;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IVideoPostProcessing;

typedef struct {
    const struct XACameraCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pCameraCaps;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} ICameraCapabilities;

typedef struct {
    const struct XAConfigExtensionsItf_ *mItf;
    IObject *mThis;
} IConfigExtensions;

typedef struct {
    const struct XARecordItf_ *mItf;
    IObject *mThis;
    XAuint32 mState;
    XAmillisecond mDurationLimit;
    XAmillisecond mPosition;
    xaRecordCallback mCallback;
    void *mContext;
    XAuint32 mCallbackEventsMask;
    XAmillisecond mMarkerPosition;
    XAmillisecond mPositionUpdatePeriod;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IXARecord;

typedef struct {
    const struct XAVideoEncoderItf_ *mItf;
    IObject *mThis;
    void *pData;
    XAVideoSettings mSettings;
} IVideoEncoder;

typedef struct {
    const struct XASnapshotItf_ *mItf;
    IObject *mThis;
    const XAchar * pURI;
    xaSnapshotInitiatedCallback initiatedCallback;
    xaSnapshotTakenCallback takenCallback;
    XAuint32 numberOfPictures;
    XAuint32 fps;
    XAboolean freezeViewFinder;
    void *mContext;
    XAboolean shutterFeedback;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} ISnapshot;

typedef struct {
    const struct XAImageEncoderItf_ *mItf;
    IObject *mThis;
    XAuint32 encoderId;
    XAuint32 width;
    XAuint32 height;
    XApermille compressionLevel;
    XAuint32 colorFormat;
#ifdef LINUX_OMXAL
    void *pData;
#endif
} IImageEncoder;

typedef struct {
    const struct XAMetadataInsertionItf_ *mItf;
    IObject *mThis;
} IMetadataInsertion;

typedef struct
{
    const struct NVXAVideoEncoderExtItf_ *mItf;
    IObject *mThis;
    void *pData;
} INvVideoEncoderExt;

typedef struct
{
    const struct XAImageDecoderCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pData;
} IImageDecoderCapabilities;

typedef struct
{
    const struct XAImageEncoderCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pData;
} IImageEncoderCapabilities;

typedef struct
{
    const struct XAVideoEncoderCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pData;
} IVideoEncoderCapabilities;

typedef struct
{
    const struct NVXACameraExtCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pData;
    XAuint32 mNumCam;
} INvCameraExtCapabilities;

typedef struct
{
    const struct NVXAVideoEncoderExtCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pData;
} INvVideoEncoderExtCapabilities;

typedef struct
{
    const struct NVXAVideoDecoderExtCapabilitiesItf_ *mItf;
    IObject *mThis;
    void *pData;
} INvVideoDecoderExtCapabilities;

typedef struct
{
    const struct NVXAVideoDecoderItf_ *mItf;
    IObject *mThis;
    void *pData;
} INvVideoDecoder;

typedef struct
{
    const struct XAGetCapabilitiesOfInterfaceItf_ *mItf;
    IObject *mThis;
    void *pData;
} IGetCapabilitiesOfInterface;

typedef struct
{
    const struct NVXAPushAppItf_ *mItf;
    IObject *mThis;
    void *pData;
    int64_t startTime;
    int frameCount;
} INvPushApp;

typedef struct
{
    const struct NVXANvBurstModeItf_ *mItf;
    IObject *mThis;
    void *pData;
} INvBurstMode;

typedef struct
{
    void *mContext;
    nvxaPushDataToAppCallback mCallback;
} INvPushAppData;
