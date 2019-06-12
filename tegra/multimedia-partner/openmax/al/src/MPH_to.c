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

// Map minimal perfect hash of an interface ID to its class index.

#include "MPH.h"
#ifdef LINUX_OMXAL
#include "MPH_to.h"
#endif

// If defined, then compile with C99 such as GNU C, not GNU C++ or non-GNU C.
//#define USE_DESIGNATED_INITIALIZERS

// It is critical that all entries are populated with either a specific index
// or -1. Do not let the compiler use a default initializer of zero, because
// that actually maps to the IObject index. For non-USE_DESIGNATED_INITIALIZERS
// builds, we use the automagically-generated MPH_to_*.h files for this reason.

// A good test is to use the GNU C compiler with -S option (for assembler output),
// and compile both with and without USE_DESIGNATED_INITIALIZERS.  The resulting .s
// files should be identical for both compilations.

// Important note: if you add any interfaces here, be sure to also
// update the #define for the corresponding INTERFACES_<Class>.

// IObject is the first interface in a class, so the index for MPH_OBJECT must be zero.
// Don't cross streams, otherwise bad things happen.


const signed char MPH_to_3DGroup[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_3DLOCATION] = 2,
    [MPH_3DDOPPLER] = 3,
    [MPH_3DSOURCE] = 4,
    [MPH_3DMACROSCOPIC] = 5
#else
#include "MPH_to_3DGroup.h"
#endif
};

const signed char MPH_to_AudioPlayer[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_PLAY] = 2,
    [MPH_3DDOPPLER] = 3,
    [MPH_3DGROUPING] = 4,
    [MPH_3DLOCATION] = 5,
    [MPH_3DSOURCE] = 6,
    [MPH_BUFFERQUEUE] = 7,
    [MPH_EFFECTSEND] = 8,
    [MPH_MUTESOLO] = 9,
    [MPH_METADATAEXTRACTION] = 10,
    [MPH_METADATATRAVERSAL] = 11,
    [MPH_PREFETCHSTATUS] = 12,
    [MPH_RATEPITCH] = 13,
    [MPH_SEEK] = 14,
    [MPH_VOLUME] = 15,
    [MPH_3DMACROSCOPIC] = 16,
    [MPH_BASSBOOST] = 17,
    [MPH_DYNAMICSOURCE] = 18,
    [MPH_ENVIRONMENTALREVERB] = 19,
    [MPH_EQUALIZER] = 20,
    [MPH_PITCH] = 21,
    [MPH_PRESETREVERB] = 22,
    [MPH_PLAYBACKRATE] = 23,
    [MPH_VIRTUALIZER] = 24,
    [MPH_VISUALIZATION] = 25,
#ifdef ANDROID
    [MPH_ANDROIDEFFECT] = 26,
    [MPH_ANDROIDEFFECTSEND] = 27,
    [MPH_ANDROIDCONFIGURATION] = 28,
    [MPH_ANDROIDSIMPLEBUFFERQUEUE] = 7,  // alias for [MPH_BUFFERQUEUE]
    [MPH_ANDROIDBUFFERQUEUESOURCE] = 29
#endif
#else
#include "MPH_to_AudioPlayer.h"
#endif
};

const signed char MPH_to_AudioRecorder[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_RECORD] = 2,
    [MPH_AUDIOENCODER] = 3,
    [MPH_BASSBOOST] = 4,
    [MPH_DYNAMICSOURCE] = 5,
    [MPH_EQUALIZER] = 6,
    [MPH_VISUALIZATION] = 7,
    [MPH_VOLUME] = 8,
#ifdef ANDROID
    [MPH_ANDROIDSIMPLEBUFFERQUEUE] = 9, // this is not an alias
    [MPH_ANDROIDCONFIGURATION] = 10
#endif
#else
#include "MPH_to_AudioRecorder.h"
#endif
};

const signed char MPH_to_Engine[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_ENGINE] = 2,
    [MPH_ENGINECAPABILITIES] = 3,
    [MPH_THREADSYNC] = 4,
    [MPH_AUDIOIODEVICECAPABILITIES] = 5,
    [MPH_AUDIODECODERCAPABILITIES] = 6,
    [MPH_AUDIOENCODERCAPABILITIES] = 7,
    [MPH_3DCOMMIT] = 8,
    [MPH_DEVICEVOLUME] = 9,
    [MPH_XAENGINE] = 10,
#ifdef ANDROID
    [MPH_ANDROIDEFFECTCAPABILITIES] = 11,
    [MPH_XAVIDEODECODERCAPABILITIES] = 12,
    [MPH_XACAMERACAPABILITIES] = 13,
    [MPH_XAIMAGEDECODERCAPABILITIES] = 14,
    [MPH_XAIMAGEENCODERCAPABILITIES] = 15,
    [MPH_XAVIDEOENCODERCAPABILITIES] = 16,
    [MPH_NVCAMERAEXTCAPABILITIES] = 17,
    [MPH_NVVIDEOENCODEREXTCAPABILITIES] = 18,
    [MPH_NVVIDEODECODEREXTCAPABILITIES] = 19,
    [MPH_GETCAPABILITIESOFINTERFACE] = 20
#endif
#ifdef LINUX_OMXAL
    [MPH_XAAUDIODECODERCAPABILITIES] = 11,
    [MPH_XAAUDIOENCODERCAPABILITIES] = 12,
    [MPH_XAAUDIOIODEVICECAPABILITIES] = 13,
    [MPH_XACONFIGEXTENSION] = 14,
    [MPH_XADEVICEVOLUME] = 15,
    [MPH_XADYNAMICINTERFACEMANAGEMENT] = 16,
    [MPH_XATHREADSYNC] = 17,
    [MPH_XAVIDEODECODERCAPABILITIES] = 18,
    [MPH_XACAMERACAPABILITIES] = 19,
    [MPH_XAIMAGEDECODERCAPABILITIES] = 20,
    [MPH_XAIMAGEENCODERCAPABILITIES] = 21,
    [MPH_XAVIDEOENCODERCAPABILITIES] = 22,
    [MPH_NVCAMERAEXTCAPABILITIES] = 23,
    [MPH_NVVIDEOENCODEREXTCAPABILITIES] = 24,
    [MPH_NVVIDEODECODEREXTCAPABILITIES] = 25,
    [MPH_GETCAPABILITIESOFINTERFACE] = 26
#endif
#else
#include "MPH_to_Engine.h"
#endif
};

const signed char MPH_to_LEDDevice[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_LED] = 2
#else
#include "MPH_to_LEDDevice.h"
#endif
};

const signed char MPH_to_Listener[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_3DDOPPLER] = 2,
    [MPH_3DLOCATION] = 3
#else
#include "MPH_to_Listener.h"
#endif
};

const signed char MPH_to_MetadataExtractor[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_DYNAMICSOURCE] = 2,
    [MPH_METADATAEXTRACTION] = 3,
    [MPH_METADATATRAVERSAL] = 4
#else
#include "MPH_to_MetadataExtractor.h"
#endif
};

const signed char MPH_to_MidiPlayer[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_PLAY] = 2,
    [MPH_3DDOPPLER] = 3,
    [MPH_3DGROUPING] = 4,
    [MPH_3DLOCATION] = 5,
    [MPH_3DSOURCE] = 6,
    [MPH_BUFFERQUEUE] = 7,
    [MPH_EFFECTSEND] = 8,
    [MPH_MUTESOLO] = 9,
    [MPH_METADATAEXTRACTION] = 10,
    [MPH_METADATATRAVERSAL] = 11,
    [MPH_MIDIMESSAGE] = 12,
    [MPH_MIDITIME] = 13,
    [MPH_MIDITEMPO] = 14,
    [MPH_MIDIMUTESOLO] = 15,
    [MPH_PREFETCHSTATUS] = 16,
    [MPH_SEEK] = 17,
    [MPH_VOLUME] = 18,
    [MPH_3DMACROSCOPIC] = 19,
    [MPH_BASSBOOST] = 20,
    [MPH_DYNAMICSOURCE] = 21,
    [MPH_ENVIRONMENTALREVERB] = 22,
    [MPH_EQUALIZER] = 23,
    [MPH_PITCH] = 24,
    [MPH_PRESETREVERB] = 25,
    [MPH_PLAYBACKRATE] = 26,
    [MPH_VIRTUALIZER] = 27,
    [MPH_VISUALIZATION] = 28,
#else
#include "MPH_to_MidiPlayer.h"
#endif
};

const signed char MPH_to_OutputMix[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_OUTPUTMIX] = 2,
#ifdef USE_OUTPUTMIXEXT
    [MPH_OUTPUTMIXEXT] = 3,
#endif
    [MPH_ENVIRONMENTALREVERB] = 4,
    [MPH_EQUALIZER] = 5,
    [MPH_PRESETREVERB] = 6,
    [MPH_VIRTUALIZER] = 7,
    [MPH_VOLUME] = 8,
    [MPH_BASSBOOST] = 9,
    [MPH_VISUALIZATION] = 10,
#ifdef ANDROID
    [MPH_ANDROIDEFFECT] = 11
#endif
#ifdef LINUX_OMXAL
    [MPH_XACONFIGEXTENSION] = 11,
    [MPH_XADYNAMICINTERFACEMANAGEMENT] = 12,
    [MPH_XAEQUALIZER] = 13,
    [MPH_XAOUTPUTMIX] = 14,
    [MPH_XAVOLUME] = 15,
#endif
#else
#include "MPH_to_OutputMix.h"
#endif
};

const signed char MPH_to_Vibra[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_VIBRA] = 2
#else
#include "MPH_to_Vibra.h"
#endif
};

const signed char MPH_to_MediaPlayer[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_XAOBJECT] = 0,
    [MPH_XADYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_XAPLAY] = 2,
    [MPH_XASTREAMINFORMATION] = 3,
    [MPH_XAVOLUME] = 4,
    [MPH_XASEEK] = 5,
    [MPH_XAPREFETCHSTATUS] = 6,
#ifdef ANDROID
    [MPH_ANDROIDBUFFERQUEUESOURCE] = 7,
    [MPH_NVVIDEODECODER] = 8,
    [MPH_XAVIDEOPOSTPROCESSING] = 9,
#endif
#ifdef LINUX_OMXAL
    [MPH_NVVIDEODECODER] = 7,
    [MPH_XAVIDEOPOSTPROCESSING] = 8,
    [MPH_XACONFIGEXTENSION] = 9,
    [MPH_XADYNAMICSOURCE] = 10,
    [MPH_XAEQUALIZER] = 11,
    [MPH_XAIMAGECONTROLS] = 12,
    [MPH_XAIMAGEEFFECTS] = 13,
    [MPH_XAMETADATAEXTRACTION] = 14,
    [MPH_XAMETADATATRAVERSAL] = 15,
    [MPH_XAPLAYBACKRATE] = 16,
#endif
#else
#include "MPH_to_MediaPlayer.h"
#endif
};

const signed char MPH_to_CameraDevice[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_XAOBJECT] = 0,
    [MPH_XACAMERA] = 1,
    [MPH_XADYNAMICINTERFACEMANAGEMENT] = 2,
    [MPH_XACONFIGEXTENSION] = 3,
    [MPH_XAIMAGECONTROLS] = 4,
    [MPH_XAIMAGEEFFECTS] = 5,
    [MPH_XAVIDEOPOSTPROCESSING] = 6
#else
#include "MPH_to_CameraDevice.h"
#endif
};

const signed char MPH_to_MediaRecorder[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_XAOBJECT] = 0,
    [MPH_XARECORD] = 1,
    [MPH_AUDIOENCODER] = 2,
    [MPH_XAAUDIOENCODERCAPABILITIES] = 3,
    [MPH_XAVIDEOENCODER] = 4,
    [MPH_XASNAPSHOT] = 5,
    [MPH_XAIMAGEENCODER] = 6,
    [MPH_XAMETADATAINSERTION] = 7,
    [MPH_XADYNAMICINTERFACEMANAGEMENT] = 8,
    [MPH_XACONFIGEXTENSION] = 9,
    [MPH_XAEQUALIZER] = 10,
    [MPH_XAIMAGECONTROLS] = 11,
    [MPH_XAIMAGEEFFECTS] = 12,
    [MPH_XAMETADATAEXTRACTION] = 13,
    [MPH_XAMETADATATRAVERSAL] = 14,
    [MPH_XAVIDEOPOSTPROCESSING] = 15,
    [MPH_XAVOLUME] = 16,
    [MPH_NVVIDEOENCODEREXT] = 17,
    [MPH_NVBURSTMODE] = 18,
#ifdef ANDROID
    [MPH_ANDROIDBUFFERQUEUESOURCE] = 19
#endif
#else
#include "MPH_to_MediaRecorder.h"
#endif
};

const signed char MPH_to_NvDataTap[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_NVPUSHAPP] = 1,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 2,
    [MPH_XACONFIGEXTENSION] = 3
#else
    -1,//MPH_AUDIODECODERCAPABILITIES
    -1,//MPH_AUDIOENCODER
    -1,//MPH_AUDIOENCODERCAPABILITIES
    -1,//MPH_AUDIOIODEVICECAPABILITIES
    -1,//MPH_CAMERA
    -1,//MPH_CAMERACAPABILITIES
     6,//MPH_CONFIGEXTENSION
    -1,//MPH_DEVICEVOLUME
     5,//MPH_DYNAMICINTERFACEMANAGEMENT
    -1,//MPH_DYNAMICSOURCESINKCHANGE
    -1,//MPH_ENGINE
    -1,//MPH_EQUALIZER
    -1,//MPH_IMAGECONTROLS
    -1,//MPH_IMAGEDECODERCAPABILITIES
    -1,//MPH_IMAGEEFFECTS
    -1,//MPH_IMAGEENCODER
    -1,//MPH_IMAGEENCODERCAPABILITIES
    -1,//MPH_LED
    -1,//MPH_METADATAEXTRACTION
    -1,//MPH_METADATAINSERTION
    -1,//MPH_METADATAMESSAGE
    -1,//MPH_METADATATRAVERSAL
    -1,//MPH_NULL
     0,//MPH_OBJECT
    -1,//MPH_OUTPUTMIX
    -1,//MPH_PLAY
    -1,//MPH_PLAYBACKRATE
    -1,//MPH_PREFETCHSTATUS
    -1,//MPH_RADIO
    -1,//MPH_RDS
    -1,//MPH_RECORD
    -1,//MPH_SEEK
    -1,//MPH_SNAPSHOT
    -1,//MPH_STREAMINFORMATION
    -1,//MPH_THREADSYNC
    -1,//MPH_VIBRA
    -1,//MPH_VIDEODECODERCAPABILITIES
    -1,//MPH_VIDEOENCODER
    -1,//MPH_VIDEOENCODERCAPABILITIES
    -1,//MPH_VIDEOPOSTPROCESSING
    -1,//MPH_VOLUME
    -1,//MPH_NVDATATAPCREATION
     1,//MPH_NVPULLAL
     2,//MPH_NVPUSHAL
     3,//MPH_NVPULLAPP
     4,//MPH_NVPUSHAPP
    -1,//MPH_NVBUFFERQUEUE
    -1,//MPH_NVCAMERAEXTCAPABILITIES
    -1,//MPH_NVVIDEOENCODEREXTCAPABILITIES
    -1,//MPH_NVVIDEODECODEREXTCAPABILITIES
    -1,//MPH_NVVIDEODECODER
    -1,//MPH_NVVIDEOENCODEREXT
    -1,//MPH_NVBURSTMODE
#endif
};
