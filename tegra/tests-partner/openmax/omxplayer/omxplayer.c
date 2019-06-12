/*
 * Copyright (c) 2007 - 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifdef _WIN32
#pragma warning( disable : 4996 )
#endif

#include "testassert.h"
#include "samplebase.h"
#include "samplewindow.h"
#include "main_omxplayer.h"
#include "nvodm_imager.h"
#include "nvtest.h"
#include "nvfxmath.h"
#include "nvrm_power.h"
#include "nvutil.h"
#include "nvmetafile_parser.h"
#include "nvmetadata.h"

#if USE_ANDROID_WINSYS
#include <nvwinsys.h>
#endif

#include "../nvmm/include/nvmm_sock.h"

#include <ctype.h>
#include "NVOMX_ComponentRegister.h"
#include "NVOMX_CustomProtocol.h"
#include "omxtestlib.h"
#include "nvhttppost.h"

// Component names
static char *g_Reader       = "OMX.Nvidia.reader";
static char *g_AudioReader  = "OMX.Nvidia.audio.read";
static char *g_Clock        = "OMX.Nvidia.clock.component";
static char *g_AudioRender  = "OMX.Nvidia.audio.render";
static char *g_OvRenderYUV  = "OMX.Nvidia.std.iv_renderer.overlay.yuv420";
static char *g_OvRenderRgb  = "OMX.Nvidia.render.overlay.argb8888";
static char *g_HdmiRender   = "OMX.Nvidia.render.hdmi.overlay.yuv420";
static char *g_TvoutRender  = "OMX.Nvidia.render.tvout.overlay.yuv420";
static char *g_SecondaryRender = "OMX.Nvidia.render.secondary.overlay.yuv420";
static char *g_NvMMRenderer = "OMX.Nvidia.video.renderer";
static char *g_Mp4Decoder   = "OMX.Nvidia.mp4.decode";
static char *g_Mp4ExtDecoder   = "OMX.Nvidia.mp4ext.decode";
static char *g_Mpeg2Decoder = "OMX.Nvidia.mpeg2v.decode";
static char *g_H264Decoder  = "OMX.Nvidia.h264.decode";
static char *g_H264ExtDecoder  = "OMX.Nvidia.h264ext.decode";
static char *g_Vc1Decoder   = "OMX.Nvidia.vc1.decode";
static char *g_AacDecoder   = "OMX.Nvidia.aac.decoder";
static char *g_AdtsDecoder  = "OMX.Nvidia.adts.decoder";
static char *g_eAacPDecoder = "OMX.Nvidia.eaacp.decoder";
static char *g_BsacDecoder  = "OMX.Nvidia.bsac.decoder";
static char *g_Mp3Decoder   = "OMX.Nvidia.mp3.decoder";
static char *g_WavDecoder   = "OMX.Nvidia.wav.decoder";
static char *g_WmaDecoder   = "OMX.Nvidia.wma.decoder";
static char *g_WmaProDecoder = "OMX.Nvidia.wmapro.decoder";
static char *g_WmaLSLDecoder = "OMX.Nvidia.wmalossless.decoder";
static char *g_VorbisDecoder= "OMX.Nvidia.vorbis.decoder";
static char *g_AmrDecoder   = "OMX.Nvidia.amr.decoder";
static char *g_AmrWBDecoder = "OMX.Nvidia.amrwb.decoder";
static char *g_AudioWriter  = "OMX.Nvidia.audio.write";
static char *g_VideoWriter  = "OMX.Nvidia.video.write";
static char *g_ImageWriter  = "OMX.Nvidia.image.write";
static char *g_ImageSequenceWriter  = "OMX.Nvidia.imagesequence.write";
static char *g_Camera       = "OMX.Nvidia.camera";
static char *g_JpegDecoder  = "OMX.Nvidia.jpeg.decoder";
static char *g_MJpegDecoder  = "OMX.Nvidia.mjpeg.decoder";
static char *g_ImageReader  = "OMX.Nvidia.image.read";
static char *g_JpegEncoder  = "OMX.Nvidia.jpeg.encoder";
static char *g_H264Encoder  = "OMX.Nvidia.h264.encoder";
static char *g_Mpeg4Encoder = "OMX.Nvidia.mp4.encoder";
static char *g_H263Encoder  = "OMX.Nvidia.h263.encoder";
//static char *g_VidHdrWriter = "OMX.Nvidia.vidhdr.write";
//static char *g_VidHdrReader = "OMX.Nvidia.vidhdr.read";
static char *g_3gpWriter    = "OMX.Nvidia.mp4.write";
static char *g_AudioCapturer = "OMX.Nvidia.audio.capturer";
static char *g_AacEncoder   = "OMX.Nvidia.aac.encoder";
static char *g_AmrEncoder   = "OMX.Nvidia.amr.encoder";
static char *g_AmrWBEncoder = "OMX.Nvidia.amrwb.encoder";
static char *g_wavEncoder = "OMX.Nvidia.wav.encoder";
static char *g_WavWriter = "OMX.Nvidia.wav.write";
static char *g_SampleComponent = "OMX.Nvidia.audio.samplecomponent";
static char *g_Mp2Decoder   = "OMX.Nvidia.mp2.decoder";
static char *g_H263Decoder    = "OMX.Nvidia.h263.decode";

// Various globals

// magic numbers for youttube formats
#define FMT_18  18  // H.264 MP in .mp4
#define FMT_22  22  // H.264 HP in .mp4 for HD
#define FMT_35  35  // H.264 MP in .flv for HQ  (not supported)
#define FMT_34  34  // H.264 MP in .flv for LQ  (not supported)


static OMX_CALLBACKTYPE g_oCallbacks;
static TestContext g_oAppData;
static OMX_BOOL bKeepDisplayOn = OMX_FALSE;

static NvxProfile g_AudDecProf;
static NvxProfile g_VidDecProf;
static NvxProfile g_DemuxProf;
static NvxProfile g_VidRendProf;
static NvxProfile g_CameraProf;
static NvxProfile g_AudioRecProf;
static NvxProfile g_AudioEncProf;
static NvxProfile g_JpegEncoderProf;
static NvxProfile g_H264EncoderProf;
static NvU32 HDMI_width;
static NvU32 HDMI_height;

static const NvU32 s_HDMI1080Width = 1920;
static const NvU32 s_HDMI1080height = 1080;

#define VIDEO_ENCODER_TYPE_H264    0
#define VIDEO_ENCODER_TYPE_MPEG4   1
#define VIDEO_ENCODER_TYPE_H263    2

#define NVX_MAX_OMXPLAYER_GRAPHS 5
static NvxGraph g_Graphs[NVX_MAX_OMXPLAYER_GRAPHS];
static OMX_BOOL g_drmlicenseAcquired =OMX_FALSE;
static NvU32 g_drmlicenseiterations =0;

typedef enum
{
    eDeleteJPEGFilesUnset = 0,
    eDeleteJPEGFilesFalse,
    eDeleteJPEGFilesTrue,
} EDeleteJPEGFiles;

// custom quantization tables
static const NvU8 Std_QuantTable_Luma[] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    7, 9, 9, 9, 1, 1, 1, 9  //This row is modified from quantization tables
};

static const NvU8 Std_QuantTable_Chroma[] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    9, 9, 9, 9, 9, 9, 9, 9  //This row is modified from quantization tables
};

typedef struct NvxCameraParam_ {
    int nWidthPreview;
    int nHeightPreview;
    int nWidthCapture;
    int nHeightCapture;
    OMX_BOOL enablePreview;
    OMX_BOOL enableCameraCapture;
    OMX_BOOL videoCapture;
    int captureTime;
    int previewTime;
    int stillCountBurst;
    int nDelayCapture;
    int nDelayLoopStillCapture;
    int nDelayHalfPress;
    int nImageQuality;
    int nThumbnailQuality;
    int focusPosition;
    OMX_IMAGE_FOCUSCONTROLTYPE focusAuto;
    int exposureTimeMS;
    int iso;
    OMX_METERINGTYPE exposureMeter;
    OMX_S32 xEVCompensation;
    OMX_S32 nContrast;
    OMX_BOOL bAutoFrameRate;
    OMX_S32 nAutoFrameRateLow;
    OMX_S32 nAutoFrameRateHigh;
    OMX_IMAGE_FLASHCONTROLTYPE   eFlashControl;
    ENvxFlickerType              eFlicker;

    OMX_IMAGEFILTERTYPE          eImageFilter;
    OMX_S32 xScaleFactor;
    OMX_S32 xScaleFactorMultiplier;
    OMX_BOOL EnableTestPattern;
    OMX_BOOL bPrecaptureConverge;
    OMX_S32 nRotationPreview;
    OMX_S32 nRotationCapture;
    OMX_BOOL bStab;
    ENvxCameraStereoCameraMode StCamMode;
    OMX_S32  nBrightness;
    OMX_BOOL bUserSetBrightness;
    OMX_S32  nSaturation;
    OMX_S32  nHue;
    OMX_WHITEBALCONTROLTYPE eWhiteBalControl;
    OMX_BOOL bPausePreviewAfterCapture;

    NVX_F32 EdgeEnhancementStrengthBias;
    NVX_RectF32 oCropPreview;
    NVX_RectF32 oCropCapture;

    OMX_U32 SensorId;
    OMX_U32 nBitRate;
    OMX_U32 nFrameRate;
    int useVidHdrRW;
    OMX_U32 nAppType;
    OMX_U32 nQualityLevel;
    OMX_U32 nErrorResilLevel;
    OMX_BOOL enableSvcVideoEncoding;
    int videoEncoderType;
    OMX_U32 videoEncodeLevel;
    OMX_U32 TemporalTradeOffLevel;

    OMX_BOOL enableAudio;
    OMX_AUDIO_CODINGTYPE eAudioCodingType;
    OMX_AUDIO_AACPROFILETYPE eAacProfileType;
    int audioTime;
    OMX_U32 nAudioSampleRate;
    OMX_U32 nAudioBitRate;
    OMX_U32 nChannels;
    OMX_BOOL bDrcEnabled;
    OMX_S32   ClippingTh;
    OMX_S32   LowerCompTh;
    OMX_S32   UpperCompTh;
    OMX_S32   NoiseGateTh;

    OMX_U32  nLoopsStillCapture;        // loops of single shot
    OMX_U64  nTSAppStart;               // timestamp right before init_graph
    OMX_U64  nTSAppCameraComponentStart;
    OMX_U64  nTSAppCaptureStart;
    OMX_U64  nTSAppCaptureEnd;
    OMX_U64  nTSAppEnd;
    OMX_U64  nTotalShutterLag;
    OMX_U64  nTSAutoFocusAchieved;
    OMX_U64  nTSAutoExposureAchieved;
    OMX_U64  nTSAutoWhiteBalanceAchieved;
    OMX_U64  nTSAppReceivesEndOfFile;
    OMX_BOOL bHalfPress;
    OMX_U32  nHalfPressFlag;
    OMX_U32  nHalfPressTimeOutMS;
    OMX_U32  nRawDumpCount;
    OMX_STRING FilenameRawDump;
    OMX_U8   eDeleteJPEGFiles;
    OMX_BOOL bAbortOnHalfPressFailure;
    OMX_U32  n3gpMaxFramesAudio;
    OMX_U32  n3gpMaxFramesVideo;
    OMX_U32  nSmoothZoomTimeMS;
    OMX_U32  nZoomAbortMS;
    OMX_S32  nVideoRotation;
    OMX_MIRRORTYPE eVideoMirror;
    OMX_AUDIO_PCMMODETYPE pcmMode;
    enum {anr_default, anr_off, anr_on} anrMode;
    enum {afm_default, afm_slow, afm_fast} afmMode;

    char ExifFile[100];
    OMX_BOOL   bExifEnable;
    char GPSFile[100];
    OMX_BOOL   bGPSEnable;
    OMX_BOOL bInterOpInfo;
    char InterOpInfoIndex[MAX_INTEROP_STRING_IN_BYTES];
    int nImageQTablesSet_Luma; //CustomTables_Luma need to set or not
    int nImageQTablesSet_Chroma; //CustomTables_Chroma need to set or not
    OMX_U32 nIntraFrameInterval;
    NVX_VIDEO_RATECONTROL_MODE eRCMode;
    OMX_BOOL bFlagThumbnailQF;
    NVX_STEREOCAPTUREINFO StCapInfo;
} NvxCameraParam;

#define MAX_EXIF_PARMS 50
#define CHARACHTER_CODE_LENGTH 8

typedef struct
{
    OMX_U32  Argc;
    char  *Argv[MAX_EXIF_PARMS];
}NvXExifArgs;

OMX_HANDLETYPE g_FileReader;
static OMX_HANDLETYPE g_hCamera;
// This static initializer needs to die.
static NvxCameraParam g_cameraParam =
    {176, 144, 176, 144, OMX_FALSE, OMX_FALSE, OMX_FALSE, 0, 30, 1, 0, 0, 0, 75,75,
     0, OMX_IMAGE_FocusControlOn, -1, -1, OMX_MeteringModeMatrix, 0, 50,
     OMX_FALSE, NV_SFX_ONE, NV_SFX_HIGH, OMX_IMAGE_FlashControlOff,
     NvxFlicker_Off, OMX_ImageFilterNone, NV_SFX_ONE, NV_SFX_ONE,
     OMX_FALSE, OMX_FALSE, 0, 0, OMX_FALSE, NvxLeftOnly, 0, OMX_FALSE, 0, 0,
     OMX_WhiteBalControlAuto,OMX_FALSE, 0.0, {0,0,0,0}, {0,0,0,0},
     0, 512000, 30, 0, 0, 0, 0, OMX_FALSE, VIDEO_ENCODER_TYPE_H264,
     255, 0, OMX_FALSE, OMX_AUDIO_CodingAAC, OMX_AUDIO_AACObjectLC,
     0, 44100, 128000, 2, OMX_FALSE, 15000, 6000, 12000, 500,
     1,0,0,0,0,0,0,0,0,0,0,OMX_FALSE, 0,0,0,0,0,OMX_FALSE,0,0,0,0,0,OMX_MirrorNone,
     OMX_AUDIO_PCMModeLinear, anr_default, afm_default,
     "\0", OMX_FALSE, "\0", OMX_FALSE, OMX_FALSE, "\0",0,0, 60,
     NVX_VIDEO_RateControlMode_CBR,OMX_FALSE, {NvxCameraCaptureType_Video,
     Nvx_BufferFlag_Stereo_LeftRight}};

static OMX_HANDLETYPE g_hAudioCapturer;
static OMX_HANDLETYPE g_hVIEncoder = NULL;

static OMX_VIDEO_MPEG4LEVELTYPE g_eLevelMpeg4[] = {
    OMX_VIDEO_MPEG4Level0,
    OMX_VIDEO_MPEG4Level1,
    OMX_VIDEO_MPEG4Level2,
    OMX_VIDEO_MPEG4Level3,
    OMX_VIDEO_MPEG4LevelMax,
};
#define g_nLevelsMpeg4             (sizeof(g_eLevelMpeg4) / sizeof(g_eLevelMpeg4[0]))

static OMX_VIDEO_AVCLEVELTYPE g_eLevelH264[] = {
    OMX_VIDEO_AVCLevel1,
    OMX_VIDEO_AVCLevel1b,
    OMX_VIDEO_AVCLevel11,
    OMX_VIDEO_AVCLevel12,
    OMX_VIDEO_AVCLevel13,
    OMX_VIDEO_AVCLevel2,
    OMX_VIDEO_AVCLevel21,
    OMX_VIDEO_AVCLevel22,
    OMX_VIDEO_AVCLevel3,
    OMX_VIDEO_AVCLevel31,
    OMX_VIDEO_AVCLevel32,
    OMX_VIDEO_AVCLevel4,
    OMX_VIDEO_AVCLevel41,
    OMX_VIDEO_AVCLevel42,
    OMX_VIDEO_AVCLevel5,
    OMX_VIDEO_AVCLevel51,
    OMX_VIDEO_AVCLevelMax,
};
#define g_nLevelsH264              (sizeof(g_eLevelH264) / sizeof(g_eLevelH264[0]))

#if NVOS_IS_LINUX || NVOS_IS_UNIX
#define GET_PATH_STORAGE_CARD(_filename)  "/sdcard/"  _filename
#define GET_PATH_STORAGE_CARD2(_filename)  "/sdcard2/"  _filename
#else
#define GET_PATH_STORAGE_CARD(_filename)  "\\Storage Card\\"  _filename
#define GET_PATH_STORAGE_CARD2(_filename)  "\\Storage Card2\\" _filename
#endif

NvError OMX_Fill_ExifIfd(NvXExifArgs *pExifArgs, OMX_STRING ExifFile);
void Parse_APP1_Args(NvU32 argc, char** argv, NVX_CONFIG_ENCODEEXIFINFO *pExifInfo,
                     NVX_CONFIG_ENCODEGPSINFO *);


static void s_printProfileNumbers(void)
{
    OMX_U32 previewFrameCount;
    OMX_U32 captureFrameCount;
    NvOsFileHandle oOut = NULL;
    NvError status;

    status = NvOsFopen(g_CameraProf.ProfileFileName,
                           NVOS_OPEN_CREATE|NVOS_OPEN_WRITE,
                           &oOut);
    if(status != NvSuccess)
    {
        oOut = NULL;
        NvOsDebugPrintf("Fileopen FAILED for %s\n", g_CameraProf.ProfileFileName);
    }

    if (oOut)
    {
        // print to file all profile numbers
        NvOsFprintf(oOut, "\n");
        NvOsFprintf(oOut, "AppStart             at %f \n", g_cameraParam.nTSAppStart/10000000.0);
        NvOsFprintf(oOut, "AppCameraStart      at %f \n", g_cameraParam.nTSAppCameraComponentStart/10000000.0);
        NvOsFprintf(oOut, "AppCaptureStart      at %f \n", g_cameraParam.nTSAppCaptureStart/10000000.0);

        NvOsFprintf(oOut, "PreviewStart         at %f \n", g_CameraProf.nTSPreviewStart/10000000.0);
        NvOsFprintf(oOut, "CaptureStart         at %f \n", g_CameraProf.nTSCaptureStart/10000000.0);
        NvOsFprintf(oOut, "CaptureEnd           at %f \n", g_CameraProf.nTSCaptureEnd/10000000.0);

        NvOsFprintf(oOut, "StillConfirmationFrame           at %f \n", g_CameraProf.nTSStillConfirmationFrame/10000000.0);
        NvOsFprintf(oOut, "FirstPreviewFrameAfterStill           at %f \n", g_CameraProf.nTSFirstPreviewFrameAfterStill/10000000.0);
        NvOsFprintf(oOut, "JpegEndOfFile           at %f \n", g_cameraParam.nTSAppReceivesEndOfFile/10000000.0);
        NvOsFprintf(oOut, "PreviewEnd           at %f \n", g_CameraProf.nTSPreviewEnd/10000000.0);

        NvOsFprintf(oOut, "AppCaptureEnd        at %f \n", g_cameraParam.nTSAppCaptureEnd/10000000.0);
        NvOsFprintf(oOut, "AppEnd               at %f \n", g_cameraParam.nTSAppEnd/10000000.0);

        previewFrameCount = g_CameraProf.nPreviewEndFrameCount - g_CameraProf.nPreviewStartFrameCount + 1;
        captureFrameCount = g_CameraProf.nCaptureEndFrameCount - g_CameraProf.nCaptureStartFrameCount + 1;

        NvOsFprintf(oOut, "Preview Frame Count: %3d frame\n", previewFrameCount);
        NvOsFprintf(oOut, "Capture Frame Count: %3d frame\n", captureFrameCount);

        if (g_CameraProf.nTSPreviewEnd - g_CameraProf.nTSPreviewStart)
        {
            NvOsFprintf(oOut, "Preview Frame Rate:    %2.2f fps\n", (10000000.0 * (previewFrameCount - 1)) / (g_CameraProf.nTSPreviewEnd - g_CameraProf.nTSPreviewStart));
        }

        if (g_CameraProf.nTSCaptureEnd - g_CameraProf.nTSCaptureStart)
        {
            NvOsFprintf(oOut, "Burst Frame Rate:      %2.2f fps\n", (10000000.0 * (captureFrameCount - 1)) / (g_CameraProf.nTSCaptureEnd - g_CameraProf.nTSCaptureStart));
        }

        if (g_cameraParam.nTSAppCaptureEnd - g_cameraParam.nTSAppCaptureStart)
        {
            // App always waits for camera capture ready signal for next frame, that means one more frame than camera block counts
            NvOsFprintf(oOut, "Click to Click period: %2.4f second\n", (g_cameraParam.nTSAppCaptureEnd - g_cameraParam.nTSAppCaptureStart) / (10000000.0 * captureFrameCount));
        }
        if (g_CameraProf.nTSCaptureStart - g_cameraParam.nTSAppCaptureStart)
        {
            NvOsFprintf(oOut, "Click time: %2.4f second\n", (g_CameraProf.nTSCaptureStart - g_cameraParam.nTSAppCaptureStart) / (10000000.0));
        }

        if (!g_cameraParam.videoCapture && g_cameraParam.nLoopsStillCapture)
        {
            NvOsFprintf(oOut, "Average shutter lag: %2.4f second\n", g_cameraParam.nTotalShutterLag / (1000000.0 * g_cameraParam.nLoopsStillCapture));
        }
        if (!g_cameraParam.videoCapture)
        {
            NvOsFprintf(oOut, "Capture To Jpeg: %2.4f second\n", (g_cameraParam.nTSAppReceivesEndOfFile -g_cameraParam.nTSAppCaptureStart) / 10000000.0 );
        }

        if (g_cameraParam.enablePreview)
        {
            NvOsFprintf(oOut, "Camera Startup lag: %2.4f second\n", (g_CameraProf.nTSPreviewStart - g_cameraParam.nTSAppCameraComponentStart) / 10000000.0);

            NvOsFprintf(oOut, "Preview lag: %2.4f second\n", (g_CameraProf.nTSPreviewStart - g_cameraParam.nTSAppStart) / 10000000.0);
            NvOsFprintf(oOut, "Preview shutdown: %2.4f second\n", (g_cameraParam.nTSAppEnd - g_CameraProf.nTSPreviewEnd) / 10000000.0);

            if(g_cameraParam.enableCameraCapture && g_cameraParam.bPausePreviewAfterCapture)
            {
              NvOsFprintf(oOut, "Capture To Post-View: %2.4f second\n", (g_CameraProf.nTSStillConfirmationFrame - g_cameraParam.nTSAppCaptureStart) / 10000000.0);
              NvOsFprintf(oOut, "Capture To ViewFinder: %2.4f second\n", (g_CameraProf.nTSFirstPreviewFrameAfterStill - g_cameraParam.nTSAppCaptureStart) / 10000000.0);
            }

            if (g_cameraParam.bHalfPress)
            {
                if (g_cameraParam.nHalfPressFlag & 1)
                {
                    if ((OMX_U64)(OMX_S64)(-1) != g_cameraParam.nTSAutoFocusAchieved)
                        NvOsFprintf(oOut, "AutoFocus ConvergeAndLock achieved in %2.4f second\n", g_cameraParam.nTSAutoFocusAchieved / 1000000.0);
                    else
                        NvOsFprintf(oOut, "AutoFocus ConvergeAndLock timed out\n");
                }
                if (g_cameraParam.nHalfPressFlag & 2)
                {
                    if ((OMX_U64)(OMX_S64)(-1) != g_cameraParam.nTSAutoExposureAchieved)
                        NvOsFprintf(oOut, "AutoExposure ConvergeAndLock achieved in %2.4f second\n", g_cameraParam.nTSAutoExposureAchieved / 1000000.0);
                    else
                        NvOsFprintf(oOut, "AutoExposure ConvergeAndLock timed out\n");
                }
                if (g_cameraParam.nHalfPressFlag & 4)
                {
                    if ((OMX_U64)(OMX_S64)(-1) != g_cameraParam.nTSAutoWhiteBalanceAchieved)
                        NvOsFprintf(oOut, "AutoWhiteBalance ConvergeAndLock achieved in %2.4f second\n", g_cameraParam.nTSAutoWhiteBalanceAchieved / 1000000.0);
                    else
                        NvOsFprintf(oOut, "AutoWhiteBalance ConvergeAndLock timed out\n");
                }
            }
        }

        if (g_CameraProf.xExposureTime)
        {
            float ExpTimeF;
            ExpTimeF = NV_SFX_TO_FP(g_CameraProf.xExposureTime);
            NvOsFprintf(oOut, "Exposure: 1/%.1f seconds (%0.3f s)\n", 1.0f/ExpTimeF, ExpTimeF);
        }
        else
        {
            NvOsFprintf(oOut, "Exposure: no info about exposure time\n");
        }
        NvOsFprintf(oOut, "Exposure: ISO%d\n", g_CameraProf.nExposureISO);

        if (g_CameraProf.nBadFrameCount)
        {
            NvOsFprintf(oOut, "Bad Frames: %d\n", g_CameraProf.nBadFrameCount);
        }

        NvOsFclose(oOut);
    }

    // print all profile numbers
    NvOsDebugPrintf("AppStart             at %f \n", g_cameraParam.nTSAppStart/10000000.0);
    NvOsDebugPrintf("AppCameraStart       at %f \n", g_cameraParam.nTSAppCameraComponentStart/10000000.0);
    NvOsDebugPrintf("AppCaptureStart      at %f \n", g_cameraParam.nTSAppCaptureStart/10000000.0);

    NvOsDebugPrintf("PreviewStart         at %f \n", g_CameraProf.nTSPreviewStart/10000000.0);
    NvOsDebugPrintf("CaptureStart         at %f \n", g_CameraProf.nTSCaptureStart/10000000.0);
    NvOsDebugPrintf("CaptureEnd           at %f \n", g_CameraProf.nTSCaptureEnd/10000000.0);

    NvOsDebugPrintf("StillConfirmationFrame                at %f \n", g_CameraProf.nTSStillConfirmationFrame/10000000.0);
    NvOsDebugPrintf("FirstPreviewFrameAfterStill           at %f \n", g_CameraProf.nTSFirstPreviewFrameAfterStill/10000000.0);

    NvOsDebugPrintf("JpegEndOfFile        at %f \n", g_cameraParam.nTSAppReceivesEndOfFile/10000000.0);
    NvOsDebugPrintf("PreviewEnd           at %f \n", g_CameraProf.nTSPreviewEnd/10000000.0);


    NvOsDebugPrintf("AppCaptureEnd        at %f \n", g_cameraParam.nTSAppCaptureEnd/10000000.0);
    NvOsDebugPrintf("AppEnd               at %f \n", g_cameraParam.nTSAppEnd/10000000.0);

    previewFrameCount = g_CameraProf.nPreviewEndFrameCount - g_CameraProf.nPreviewStartFrameCount + 1;
    captureFrameCount = g_CameraProf.nCaptureEndFrameCount - g_CameraProf.nCaptureStartFrameCount + 1;

    NvOsDebugPrintf("Preview Frame Count: %3d frame\n", previewFrameCount);
    NvOsDebugPrintf("Capture Frame Count: %3d frame\n", captureFrameCount);

    if (g_CameraProf.nTSPreviewEnd - g_CameraProf.nTSPreviewStart)
    {
        NvOsDebugPrintf("Preview Frame Rate:    %2.2f fps\n", (10000000.0 * (previewFrameCount - 1)) / (g_CameraProf.nTSPreviewEnd - g_CameraProf.nTSPreviewStart));
    }

    if (g_CameraProf.nTSCaptureEnd - g_CameraProf.nTSCaptureStart)
    {
        NvOsDebugPrintf("Burst Frame Rate:      %2.2f fps\n", (10000000.0 * (captureFrameCount - 1)) / (g_CameraProf.nTSCaptureEnd - g_CameraProf.nTSCaptureStart));
    }

    if (g_cameraParam.nTSAppCaptureEnd - g_cameraParam.nTSAppCaptureStart)
    {
        // App always waits for camera capture ready signal for next frame, that means one more frame than camera block counts
        NvOsDebugPrintf("Click to Click period: %2.4f second\n", (g_cameraParam.nTSAppCaptureEnd - g_cameraParam.nTSAppCaptureStart) / (10000000.0 * captureFrameCount));
    }
    if (g_CameraProf.nTSCaptureStart - g_cameraParam.nTSAppCaptureStart)
    {
        NvOsDebugPrintf("Click time: %2.4f second\n", (g_CameraProf.nTSCaptureStart - g_cameraParam.nTSAppCaptureStart) / (10000000.0));
    }

    if (!g_cameraParam.videoCapture && g_cameraParam.nLoopsStillCapture)
    {
        NvOsDebugPrintf("Average shutter lag: %2.4f second\n", g_cameraParam.nTotalShutterLag / (1000000.0 * g_cameraParam.nLoopsStillCapture));
    }

    if (!g_cameraParam.videoCapture)
    {
        NvOsDebugPrintf("Capture To Jpeg: %2.4f second\n", (g_cameraParam.nTSAppReceivesEndOfFile - g_cameraParam.nTSAppCaptureStart)/ 10000000.0 );
    }

    if (g_cameraParam.enablePreview)
    {
        NvOsDebugPrintf("Camera Startup lag: %2.4f second\n", (g_CameraProf.nTSPreviewStart - g_cameraParam.nTSAppCameraComponentStart) / 10000000.0);
        NvOsDebugPrintf("Preview lag: %2.4f second\n", (g_CameraProf.nTSPreviewStart - g_cameraParam.nTSAppStart) / 10000000.0);
        NvOsDebugPrintf("Preview shutdown: %2.4f second\n", (g_cameraParam.nTSAppEnd - g_CameraProf.nTSPreviewEnd) / 10000000.0);

        if(g_cameraParam.enableCameraCapture && g_cameraParam.bPausePreviewAfterCapture)
        {
            NvOsDebugPrintf("Capture To Post-View: %2.4f second\n", (g_CameraProf.nTSStillConfirmationFrame - g_cameraParam.nTSAppCaptureStart) / 10000000.0);
            NvOsDebugPrintf("Capture To ViewFinder: %2.4f second\n", (g_CameraProf.nTSFirstPreviewFrameAfterStill - g_cameraParam.nTSAppCaptureStart) / 10000000.0);
        }

        if (g_cameraParam.bHalfPress)
        {
            if (g_cameraParam.nHalfPressFlag & 1)
            {
                if ((OMX_U64)(OMX_S64)(-1) != g_cameraParam.nTSAutoFocusAchieved)
                    NvOsDebugPrintf("AutoFocus ConvergeAndLock achieved in %2.4f second\n", g_cameraParam.nTSAutoFocusAchieved / 1000000.0);
                else
                    NvOsDebugPrintf("AutoFocus ConvergeAndLock timed out\n");
            }
            if (g_cameraParam.nHalfPressFlag & 2)
            {
                if ((OMX_U64)(OMX_S64)(-1) != g_cameraParam.nTSAutoExposureAchieved)
                    NvOsDebugPrintf("AutoExposure ConvergeAndLock achieved in %2.4f second\n", g_cameraParam.nTSAutoExposureAchieved / 1000000.0);
                else
                    NvOsDebugPrintf("AutoExposure ConvergeAndLock timed out\n");
            }
            if (g_cameraParam.nHalfPressFlag & 4)
            {
                if ((OMX_U64)(OMX_S64)(-1) != g_cameraParam.nTSAutoWhiteBalanceAchieved)
                    NvOsDebugPrintf("AutoWhiteBalance ConvergeAndLock achieved in %2.4f second\n", g_cameraParam.nTSAutoWhiteBalanceAchieved / 1000000.0);
                else
                    NvOsDebugPrintf("AutoWhiteBalance ConvergeAndLock timed out\n");
            }
        }
    }

    if (g_CameraProf.xExposureTime)
    {
        float ExpTimeF;
        ExpTimeF = NV_SFX_TO_FP(g_CameraProf.xExposureTime);
        NvOsDebugPrintf("Exposure: 1/%.1f seconds (%0.3f s)\n", 1.0f/ExpTimeF, ExpTimeF);
    }
    else
    {
        NvOsDebugPrintf("Exposure: no info about exposure time\n");
    }
    NvOsDebugPrintf("Exposure: ISO%d\n", g_CameraProf.nExposureISO);

    if (g_CameraProf.nBadFrameCount)
    {
        NvOsDebugPrintf("Bad Frames: %d\n", g_CameraProf.nBadFrameCount);
    }

    return;
}

static void NvxWindowMoveFunc(int xpos, int ypos, void *pClientData)
{
    NvxGraph *pGraph = (NvxGraph *)pClientData;
    OMX_HANDLETYPE hRenderer = pGraph->hRenderer;
    OMX_CONFIG_POINTTYPE oPointType;

    if (!hRenderer)
       return;

    INIT_PARAM(oPointType);
    oPointType.nX = xpos;
    oPointType.nY = ypos;

    OMX_SetConfig( hRenderer, OMX_IndexConfigCommonOutputPosition, &oPointType);
}

static void NvxWindowResizeFunc(int xsize, int ysize, void *pClientData)
{
    NvxGraph *pGraph = (NvxGraph *)pClientData;
    OMX_HANDLETYPE hRenderer = pGraph->hRenderer;
    OMX_FRAMESIZETYPE oSize;

    if (!hRenderer)
       return;

    INIT_PARAM(oSize);
    oSize.nPortIndex = 0;
    oSize.nWidth = xsize;
    oSize.nHeight = ysize;

    OMX_SetConfig(hRenderer, OMX_IndexConfigCommonOutputSize, &oSize);
}

static void NvxWindowCloseFunc(void *pClientData)
{
    NvxGraph *pGraph = (NvxGraph *)pClientData;

    if (!pGraph->oCompGraph.eos)
    {
        pGraph->oCompGraph.eos = 1;
        NvOsSemaphoreSignal(pGraph->oCompGraph.hTimeEvent);
        NvOsSemaphoreSignal(g_oAppData.hMainEndEvent);
    }
}

static OMX_ERRORTYPE InitSourceGraph(int testcase, OMX_HANDLETYPE *pDemux,
                                     char *infile, char *reader,
                                     NvxGraph *pGraph)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    switch (testcase)
    {
        default:
        {
            // Create a normal reader
            eError = InitReaderWriter(reader, infile, pDemux, pGraph,
                                      &g_oAppData, &g_oCallbacks);
            g_FileReader = *pDemux;
            break;
        }
    }
    return eError;
}

static OMX_ERRORTYPE setANR(OMX_HANDLETYPE cam, NvBool ison)
{
    NVX_CONFIG_ADVANCED_NOISE_REDUCTION anr;
    OMX_ERRORTYPE err;
    OMX_INDEXTYPE anr_idx;

    err = OMX_GetExtensionIndex(cam,
                                NVX_INDEX_CONFIG_ADVANCED_NOISE_REDUCTION,
                                &anr_idx);
    if (err != OMX_ErrorNone) return err;

    INIT_PARAM(anr);
    anr.nPortIndex = 0;
    err = OMX_GetConfig(cam, anr_idx, &anr);
    anr.enable = ison;
    err = OMX_SetConfig(cam, anr_idx, &anr);

    return err;
}

static OMX_ERRORTYPE setAFM(OMX_HANDLETYPE cam, NvBool isslow)
{
    NVX_CONFIG_AUTOFOCUS_SPEED afm;
    OMX_ERRORTYPE err;
    OMX_INDEXTYPE afm_idx;

    err = OMX_GetExtensionIndex(cam,
                                NVX_INDEX_CONFIG_AUTOFOCUS_SPEED,
                                &afm_idx);
    if (err != OMX_ErrorNone) return err;

    INIT_PARAM(afm);
    afm.nPortIndex = 0;
    afm.bFast = !isslow;
    err = OMX_SetConfig(cam, afm_idx, &afm);

    return err;
}

static OMX_ERRORTYPE s_runAVRecorder(NvxGraph *pCurGraph)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_BOOLEANTYPE     cc;
    OMX_INDEXTYPE eIndexPreviewEnable;
    OMX_U64 nTSAppHalfPress = 0;

    // config camera
    if (g_cameraParam.enableCameraCapture || g_cameraParam.enablePreview)
    {
        OMX_CONFIG_EXPOSUREVALUETYPE ExposureValue; // OMX_IndexConfigCommonExposureValue
        OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE FocusControl; // OMX_IndexConfigFocusControl
        OMX_INDEXTYPE eIndex;
        OMX_INDEXTYPE eIndexAutoFrameRate;
        OMX_CONFIG_CONTRASTTYPE      oContrast;
        NVX_CONFIG_AUTOFRAMERATE     oAutoFrameRate;
        NVX_CONFIG_FLICKER           oFlicker;
        OMX_CONFIG_IMAGEFILTERTYPE   oImageFilter;
        OMX_IMAGE_PARAM_FLASHCONTROLTYPE    oFlashControl;

        INIT_PARAM(cc);

        INIT_PARAM(ExposureValue);
        ExposureValue.nPortIndex = 0;
        OMX_GetConfig(g_hCamera, OMX_IndexConfigCommonExposureValue, &ExposureValue);
        if (g_cameraParam.exposureTimeMS == -1)
        {
            ExposureValue.bAutoShutterSpeed = OMX_TRUE;
        }
        else
        {
            ExposureValue.bAutoShutterSpeed = OMX_FALSE;
            ExposureValue.nShutterSpeedMsec = g_cameraParam.exposureTimeMS;
        }

        if (g_cameraParam.iso == -1)
        {
            ExposureValue.bAutoSensitivity = OMX_TRUE;
        }
        else
        {
            ExposureValue.bAutoSensitivity = OMX_FALSE;
            ExposureValue.nSensitivity = g_cameraParam.iso;
        }

        // Flash setting
        INIT_PARAM(oFlashControl);
        oFlashControl.nPortIndex = 1;
        oFlashControl.eFlashControl = g_cameraParam.eFlashControl;
        OMX_SetConfig(g_hCamera, OMX_IndexConfigFlashControl, &oFlashControl);


        ExposureValue.eMetering = g_cameraParam.exposureMeter;
        ExposureValue.xEVCompensation = g_cameraParam.xEVCompensation;
        OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonExposureValue, &ExposureValue);

        INIT_PARAM(FocusControl);
        FocusControl.nPortIndex = 0;
        OMX_GetConfig(g_hCamera, OMX_IndexConfigFocusControl, &FocusControl);
        FocusControl.eFocusControl = g_cameraParam.focusAuto;
        FocusControl.nFocusStepIndex = g_cameraParam.focusPosition;
        OMX_SetConfig(g_hCamera, OMX_IndexConfigFocusControl, &FocusControl);

        eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_AUTOFRAMERATE,
                                       &eIndexAutoFrameRate);
        TEST_ASSERT_NOTERROR(eError);
        INIT_PARAM(oAutoFrameRate);
        oAutoFrameRate.bEnabled = g_cameraParam.bAutoFrameRate;
        oAutoFrameRate.low = g_cameraParam.nAutoFrameRateLow;
        oAutoFrameRate.high = g_cameraParam.nAutoFrameRateHigh;
        OMX_SetConfig(g_hCamera, eIndexAutoFrameRate, &oAutoFrameRate);

        INIT_PARAM(oContrast);
        oContrast.nPortIndex = 0;
        oContrast.nContrast = g_cameraParam.nContrast;
        OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonContrast, &oContrast);

        {
            OMX_CONFIG_BRIGHTNESSTYPE    oBrightness;
            OMX_CONFIG_SATURATIONTYPE    oSaturation;
            NVX_CONFIG_HUETYPE           oHue;

            if (g_cameraParam.bUserSetBrightness)
            {
                INIT_PARAM(oBrightness);
                oBrightness.nPortIndex = 0;
                oBrightness.nBrightness = g_cameraParam.nBrightness;
                OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonBrightness, &oBrightness);
            }

            INIT_PARAM(oSaturation);
            oSaturation.nPortIndex = 0;
            oSaturation.nSaturation = g_cameraParam.nSaturation;
            OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonSaturation, &oSaturation);

            eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_HUE, &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oHue);
            oHue.nPortIndex = 0;
            oHue.nHue = g_cameraParam.nHue;
            OMX_SetConfig(g_hCamera, eIndex, &oHue);
        }

        {
            OMX_CONFIG_WHITEBALCONTROLTYPE           oWB;

            INIT_PARAM(oWB);
            oWB.nPortIndex = 0;
            oWB.eWhiteBalControl = g_cameraParam.eWhiteBalControl;
            OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonWhiteBalance, &oWB);
        }

        {
            OMX_PARAM_U32TYPE           oSmoothZoomTime;

            eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_SMOOTHZOOMTIME,
                                           &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oSmoothZoomTime);
            oSmoothZoomTime.nPortIndex = 0;
            oSmoothZoomTime.nU32 = g_cameraParam.nSmoothZoomTimeMS;
            OMX_SetConfig(g_hCamera, eIndex, &oSmoothZoomTime);
        }

        eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_FLICKER,
                                       &eIndex);
        TEST_ASSERT_NOTERROR(eError);
        INIT_PARAM(oFlicker);
        oFlicker.nPortIndex = 0;
        //OMX_GetConfig(g_hCamera, eIndex, &oFlicker);
        oFlicker.eFlicker = g_cameraParam.eFlicker;
        OMX_SetConfig(g_hCamera, eIndex, &oFlicker);

        INIT_PARAM(oImageFilter);
        oImageFilter.nPortIndex = 0;
        oImageFilter.eImageFilter = g_cameraParam.eImageFilter;
        OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonImageFilter, &oImageFilter);

        {
            OMX_CONFIG_SCALEFACTORTYPE oScaleFactor;
            INIT_PARAM(oScaleFactor);
            oScaleFactor.nPortIndex = 5;
            oScaleFactor.xWidth = oScaleFactor.xHeight = g_cameraParam.xScaleFactor;
            eError = OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonDigitalZoom, &oScaleFactor);
            TEST_ASSERT_NOTERROR(eError);
        }

        {
            NVX_CONFIG_PRECAPTURECONVERGE oPrecaptureConverge;
            eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_PRECAPTURECONVERGE,
                                           &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oPrecaptureConverge);
            oPrecaptureConverge.nPortIndex = 5;
            OMX_GetConfig(g_hCamera, eIndex, &oPrecaptureConverge);
            if (oPrecaptureConverge.bPrecaptureConverge != g_cameraParam.bPrecaptureConverge)
            {
                oPrecaptureConverge.bPrecaptureConverge = g_cameraParam.bPrecaptureConverge;
                eError = OMX_SetConfig(g_hCamera, eIndex, &oPrecaptureConverge);
                TEST_ASSERT_NOTERROR(eError);
            }
        }

        {
            OMX_CONFIG_FRAMESTABTYPE oFrameStab;
            INIT_PARAM(oFrameStab);
            oFrameStab.nPortIndex = 5;
            oFrameStab.bStab = g_cameraParam.bStab;  //OMX_TRUE or OMX_FALSE
            eError = OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonFrameStabilisation, &oFrameStab);
            TEST_ASSERT_NOTERROR(eError);
        }

        {
            OMX_CONFIG_RECTTYPE oCropRect;
            INIT_PARAM(oCropRect);

            if (g_cameraParam.oCropPreview.right - g_cameraParam.oCropPreview.left > 0 &&
                g_cameraParam.oCropPreview.bottom - g_cameraParam.oCropPreview.top > 0)
            {
                oCropRect.nPortIndex = 0;
                oCropRect.nLeft   = (OMX_S32)g_cameraParam.oCropPreview.left;
                oCropRect.nTop    = (OMX_S32)g_cameraParam.oCropPreview.top;
                oCropRect.nWidth  = (OMX_U32)(g_cameraParam.oCropPreview.right - g_cameraParam.oCropPreview.left);
                oCropRect.nHeight = (OMX_U32)(g_cameraParam.oCropPreview.bottom - g_cameraParam.oCropPreview.top);
                eError = OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonOutputCrop, &oCropRect);
                TEST_ASSERT_NOTERROR(eError);
            }
            if (g_cameraParam.oCropCapture.right - g_cameraParam.oCropCapture.left > 0 &&
                g_cameraParam.oCropCapture.bottom - g_cameraParam.oCropCapture.top > 0)
            {
                oCropRect.nPortIndex = 5;
                oCropRect.nLeft   = (OMX_S32)g_cameraParam.oCropCapture.left;
                oCropRect.nTop    = (OMX_S32)g_cameraParam.oCropCapture.top;
                oCropRect.nWidth  = (OMX_U32)(g_cameraParam.oCropCapture.right - g_cameraParam.oCropCapture.left);
                oCropRect.nHeight = (OMX_U32)(g_cameraParam.oCropCapture.bottom - g_cameraParam.oCropCapture.top);
                eError = OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonOutputCrop, &oCropRect);
                TEST_ASSERT_NOTERROR(eError);
            }
        }

        {
            NVX_CONFIG_EDGEENHANCEMENT oEdgeEnhancement;
            eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_EDGEENHANCEMENT,
                                           &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oEdgeEnhancement);
            oEdgeEnhancement.nPortIndex = 1;
            if (g_cameraParam.EdgeEnhancementStrengthBias < -1.0 || g_cameraParam.EdgeEnhancementStrengthBias > 1.0)
            {
                oEdgeEnhancement.bEnable = OMX_FALSE;
            }
            else
            {
                oEdgeEnhancement.bEnable = OMX_TRUE;
                oEdgeEnhancement.strengthBias = g_cameraParam.EdgeEnhancementStrengthBias;
            }

            eError = OMX_SetConfig(g_hCamera, eIndex, &oEdgeEnhancement);
            TEST_ASSERT_NOTERROR(eError);
        }

        {
            NVX_CONFIG_CAMERATESTPATTERN TestPatternValue;

            //Set Test Pattern for Camera
            if(g_cameraParam.EnableTestPattern)
            {
                INIT_PARAM(TestPatternValue);

                eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_CAMERATESTPATTERN,
                                   &eIndex);
                TEST_ASSERT_NOTERROR(eError);
                eError = OMX_SetConfig(g_hCamera, eIndex, &TestPatternValue);
                TEST_ASSERT_NOTERROR(eError);
            }
        }
        if (g_cameraParam.anrMode != anr_default)
            setANR(g_hCamera, g_cameraParam.anrMode == anr_on);
        if (g_cameraParam.afmMode != afm_default)
            setAFM(g_hCamera, g_cameraParam.afmMode == afm_slow);
    }

    // start preview
    if (g_cameraParam.enablePreview)
    {
        eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_PREVIEWENABLE,
                                       &eIndexPreviewEnable);
        TEST_ASSERT_NOTERROR(eError);
        cc.bEnabled = OMX_TRUE;
        OMX_SetConfig(g_hCamera, eIndexPreviewEnable, &cc);

        if (g_cameraParam.nZoomAbortMS)
        {
            OMX_INDEXTYPE                    eIndex;
            OMX_CONFIG_BOOLEANTYPE           oZoomAbort;

            WaitForAnyErrorOrTime(&g_oAppData, g_cameraParam.nZoomAbortMS);
            eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_ZOOMABORT, &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oZoomAbort);
            oZoomAbort.bEnabled = OMX_TRUE;
            OMX_SetConfig(g_hCamera, eIndex, &oZoomAbort);
        }

        g_cameraParam.nDelayCapture = g_cameraParam.nDelayCapture > g_cameraParam.nDelayHalfPress ?
            g_cameraParam.nDelayCapture : g_cameraParam.nDelayHalfPress;

        g_cameraParam.previewTime = g_cameraParam.nDelayCapture > g_cameraParam.previewTime ?
            0 : g_cameraParam.previewTime - g_cameraParam.nDelayCapture;

        g_cameraParam.nDelayCapture = g_cameraParam.nDelayCapture - g_cameraParam.nDelayHalfPress;

        WaitForAnyErrorOrTime(&g_oAppData, 1000 * g_cameraParam.nDelayHalfPress);

        if(g_cameraParam.bPausePreviewAfterCapture)
        {
            OMX_CONFIG_BOOLEANTYPE  oPausePreviewAfterSTillCapture;
            INIT_PARAM(oPausePreviewAfterSTillCapture);
            oPausePreviewAfterSTillCapture.bEnabled = g_cameraParam.bPausePreviewAfterCapture;
            OMX_SetConfig(g_hCamera, OMX_IndexAutoPauseAfterCapture, &oPausePreviewAfterSTillCapture);
        }

        if (g_cameraParam.bHalfPress)
        {
            NVX_CONFIG_CONVERGEANDLOCK oConvergeAndLock;
            OMX_INDEXTYPE eIndexConvergeAndLock;

            eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_CONVERGEANDLOCK,
                                           &eIndexConvergeAndLock);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oConvergeAndLock);
            oConvergeAndLock.bUnlock = OMX_FALSE;
            oConvergeAndLock.bAutoFocus = (g_cameraParam.nHalfPressFlag & 1)? OMX_TRUE : OMX_FALSE;
            oConvergeAndLock.bAutoExposure = (g_cameraParam.nHalfPressFlag & 2)? OMX_TRUE : OMX_FALSE;
            oConvergeAndLock.bAutoWhiteBalance = (g_cameraParam.nHalfPressFlag & 4)? OMX_TRUE : OMX_FALSE;
            oConvergeAndLock.nTimeOutMS = g_cameraParam.nHalfPressTimeOutMS;
            nTSAppHalfPress = NvOsGetTimeUS();
            OMX_SetConfig(g_hCamera, eIndexConvergeAndLock, &oConvergeAndLock);

            if (g_cameraParam.bAbortOnHalfPressFailure)
            {
                if (g_cameraParam.nHalfPressFlag & 1)
                    NvOsSemaphoreWait(pCurGraph->oCompGraph.hCameraHalfPress3A);
                if (g_cameraParam.nHalfPressFlag & 2)
                    NvOsSemaphoreWait(pCurGraph->oCompGraph.hCameraHalfPress3A);
                if (g_cameraParam.nHalfPressFlag & 4)
                    NvOsSemaphoreWait(pCurGraph->oCompGraph.hCameraHalfPress3A);

                if ( ( (g_cameraParam.nHalfPressFlag & 1) && (0 == pCurGraph->oCompGraph.nTSAutoFocusAchieved)        ) ||
                     ( (g_cameraParam.nHalfPressFlag & 2) && (0 == pCurGraph->oCompGraph.nTSAutoExposureAchieved)     ) ||
                     ( (g_cameraParam.nHalfPressFlag & 4) && (0 == pCurGraph->oCompGraph.nTSAutoWhiteBalanceAchieved) ) )
                {
                    g_cameraParam.enableCameraCapture = OMX_FALSE;
                    g_cameraParam.enableAudio = OMX_FALSE;
                    pCurGraph->oCompGraph.hEndComponent = g_FileReader;
                    g_oCallbacks.EventHandler(g_FileReader, &g_oAppData, OMX_EventBufferFlag, 0, OMX_BUFFERFLAG_EOS, 0);
                    eError = OMX_ErrorTimeout;
                    NvOsDebugPrintf("!!!Halfpress can't converge!!!\n");
                }
            }
        }
    }

    WaitForAnyErrorOrTime(&g_oAppData, 1000 * g_cameraParam.nDelayCapture);
    if (OMX_ErrorNone != pCurGraph->oCompGraph.eErrorEvent)
    {
        NvOsDebugPrintf("Some error happened during preview\n");
        goto L_EndPreview;
    }

    // run capture
    if (g_cameraParam.enableCameraCapture)
    {
        cc.bEnabled = OMX_TRUE;

        g_cameraParam.nTSAppCaptureStart = NvOsGetTimeUS() * 10;

        if (!g_cameraParam.videoCapture)
        {
            OMX_U32 i;
            for (i=0; i<g_cameraParam.nLoopsStillCapture; i++)
            {
                OMX_U64 ts = 0;

                if (i > 0)
                    NvOsSleepMS(g_cameraParam.nDelayLoopStillCapture * 1000);

                ts = NvOsGetTimeUS();

                OMX_SetConfig(g_hCamera, OMX_IndexConfigCapturing, &cc);

                // wait for camera ready event
                NvOsSemaphoreWait(pCurGraph->oCompGraph.hCameraStillCaptureReady);
                if (OMX_ErrorNone != pCurGraph->oCompGraph.eErrorEvent)
                    break;

                if(g_cameraParam.enablePreview && g_cameraParam.bPausePreviewAfterCapture)
                {
                      NvOsSemaphoreWait(pCurGraph->oCompGraph.hCameraPreviewPausedAfterStillCapture);
                      cc.bEnabled = OMX_TRUE;
                      OMX_SetConfig(g_hCamera, eIndexPreviewEnable, &cc);
                }

                g_cameraParam.nTotalShutterLag += pCurGraph->oCompGraph.TimeStampCameraReady - ts;
            }
        }
        else
        {
            OMX_U32 t1,t2,TotalTime,CapturedTime;
            t1 = t2 = CapturedTime = 0;
            TotalTime = g_cameraParam.captureTime*1000*1000;

            {
                OMX_INDEXTYPE eIndexCombineCapture;
                NVX_CONFIG_COMBINEDCAPTURE  oCombineCapture;

                eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_COMBINEDCAPTURE,
                                                   &eIndexCombineCapture);
                INIT_PARAM(oCombineCapture);
                oCombineCapture.nPortIndex = 5; //video capture
                oCombineCapture.nImages = 1;

                OMX_SetConfig(g_hCamera, eIndexCombineCapture, &oCombineCapture);
            }

            if (g_cameraParam.enableAudio)
            {
                OMX_SetConfig(g_hAudioCapturer, OMX_IndexConfigCapturing, &cc);
            }

            NvOsDebugPrintf("For AVRecorder, take %s for %d seconds\n",
                         g_cameraParam.enableAudio ? "audio_and_video" : "video",
                         g_cameraParam.captureTime);
            while(CapturedTime < TotalTime)
            {
                t1 = (NvU32)NvOsGetTimeUS();
                WaitForAnyErrorOrTime(&g_oAppData, 1000 * g_cameraParam.captureTime);
                t2 = (NvU32)NvOsGetTimeUS();
                CapturedTime += (t2 - t1);
                if( ((NvxError)pCurGraph->oCompGraph.eErrorEvent == NvxError_WriterFileSizeLimitExceeded) && (pCurGraph->SplitMode == 1))
                {
                    NvOsDebugPrintf("Start splitmode\n");
                    {
                        OMX_INDEXTYPE eIndexConfigFilename;
                        NVX_CONFIG_SPLITFILENAME oFilenameParam;
                        INIT_PARAM(oFilenameParam);
                        eError = OMX_GetExtensionIndex(g_FileReader, NVX_INDEX_CONFIG_SPLITFILENAME, &eIndexConfigFilename);
                        oFilenameParam.pFilename = pCurGraph->splitfile;
                        OMX_SetConfig(g_FileReader, eIndexConfigFilename, &oFilenameParam);
                    }
                    pCurGraph->oCompGraph.eErrorEvent = OMX_ErrorNone;
                }
                else if((NvxError)pCurGraph->oCompGraph.eErrorEvent == NvxError_WriterFileSizeLimitExceeded)
                {
                    g_oCallbacks.EventHandler(g_FileReader, &g_oAppData, OMX_EventError, OMX_ErrorUndefined, 0, 0);
                    break;
                }
                else
                    break;

                pCurGraph->SplitMode = 0;
            }


#if 0
            // sample code for pausing
            {
                OMX_INDEXTYPE eIndex;
                eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_CAPTUREPAUSE,
                                               &eIndex);
                TEST_ASSERT_NOTERROR(eError);
                cc.bEnabled = OMX_TRUE;
                OMX_SetConfig(g_hCamera, eIndex, &cc);
                NvOsSleepMS(1000 * g_cameraParam.capturePauseTime);
                cc.bEnabled = OMX_FALSE;
                OMX_SetConfig(g_hCamera, eIndex, &cc);
            }
#endif
            {
                //Reset NVX_INDEX_CONFIG_COMBINEDCAPTURE
                OMX_INDEXTYPE eIndexCombineCapture;
                NVX_CONFIG_COMBINEDCAPTURE  oCombineCapture;

                eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_COMBINEDCAPTURE,
                                                   &eIndexCombineCapture);
                INIT_PARAM(oCombineCapture);
                oCombineCapture.nPortIndex = 5; //video capture
                oCombineCapture.nImages = 0;
                OMX_SetConfig(g_hCamera, eIndexCombineCapture, &oCombineCapture);
            }

            if (g_cameraParam.enableAudio)
            {
                OMX_SetConfig(g_hAudioCapturer, OMX_IndexConfigCapturing, &cc);
            }
        }

        g_cameraParam.nTSAppCaptureEnd = NvOsGetTimeUS() * 10;
    }
    else if (g_cameraParam.enableAudio)
    {
        cc.bEnabled = OMX_TRUE;
        OMX_SetConfig(g_hAudioCapturer, OMX_IndexConfigCapturing, &cc);
        NvOsDebugPrintf("For AVRecorder, take %s for %d seconds\n",
                         "audio", g_cameraParam.audioTime);
        WaitForAnyErrorOrTime(&g_oAppData, 1000 * g_cameraParam.audioTime);

        cc.bEnabled = OMX_FALSE;
        OMX_SetConfig(g_hAudioCapturer, OMX_IndexConfigCapturing, &cc);
    }

    // Do raw dump after still capture
    if (g_cameraParam.nRawDumpCount)
    {
        OMX_INDEXTYPE                eIndexRawCapture;
        NVX_CONFIG_CAMERARAWCAPTURE  oRawCapture;
        OMX_ERRORTYPE eErrorTmp = OMX_ErrorNone;
        eErrorTmp = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_CAMERARAWCAPTURE,
                                       &eIndexRawCapture);
        TEST_ASSERT_NOTERROR(eErrorTmp);
        INIT_PARAM(oRawCapture);
        oRawCapture.nCaptureCount = g_cameraParam.nRawDumpCount;
        oRawCapture.Filename = g_cameraParam.FilenameRawDump;
        eErrorTmp = OMX_SetConfig(g_hCamera, eIndexRawCapture, &oRawCapture);
        TEST_ASSERT_NOTERROR(eErrorTmp);
    }

L_EndPreview:
    // end preview
    if (g_cameraParam.enablePreview)
    {
        int captureTime = g_cameraParam.audioTime > g_cameraParam.captureTime ?
                          g_cameraParam.audioTime : g_cameraParam.captureTime;
        int moreTime = g_cameraParam.previewTime - captureTime;
        if (moreTime > 0)
        {
            NvOsDebugPrintf("preview %d sec more\n", moreTime);
            WaitForAnyErrorOrTime(&g_oAppData, 1000 * moreTime);
        }
        cc.bEnabled = OMX_FALSE;
        OMX_SetConfig(g_hCamera, eIndexPreviewEnable, &cc);

        if (g_cameraParam.bHalfPress)
        {
            WaitForAnyErrorOrTime(&g_oAppData, g_cameraParam.nHalfPressTimeOutMS);
            g_cameraParam.nTSAutoFocusAchieved = pCurGraph->oCompGraph.nTSAutoFocusAchieved ?
                pCurGraph->oCompGraph.nTSAutoFocusAchieved - nTSAppHalfPress : (OMX_U64)(OMX_S64)(-1);
            g_cameraParam.nTSAutoExposureAchieved = pCurGraph->oCompGraph.nTSAutoExposureAchieved ?
                pCurGraph->oCompGraph.nTSAutoExposureAchieved - nTSAppHalfPress : (OMX_U64)(OMX_S64)(-1);
            g_cameraParam.nTSAutoWhiteBalanceAchieved = pCurGraph->oCompGraph.nTSAutoWhiteBalanceAchieved ?
                pCurGraph->oCompGraph.nTSAutoWhiteBalanceAchieved - nTSAppHalfPress : (OMX_U64)(OMX_S64)(-1);
        }
    }

    return eError;
}

static OMX_ERRORTYPE s_SetupVideoEncoder(OMX_HANDLETYPE hVideoEncoder)
{
    OMX_ERRORTYPE eError;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    OMX_VIDEO_PARAM_BITRATETYPE oBitRate;
    OMX_CONFIG_FRAMERATETYPE oFrameRate;
    OMX_IMAGE_PARAM_QFACTORTYPE oQFactor;
    NVX_PARAM_VIDENCPROPERTY oEncodeProp;
    OMX_INDEXTYPE eIndex;

    INIT_PARAM(oPortDef);
    oPortDef.nPortIndex = 0;
    OMX_GetParameter(hVideoEncoder, OMX_IndexParamPortDefinition, &oPortDef);
    oPortDef.format.video.nFrameWidth = g_cameraParam.nWidthCapture;
    oPortDef.format.video.nFrameHeight = g_cameraParam.nHeightCapture;
    OMX_SetParameter(hVideoEncoder, OMX_IndexParamPortDefinition, &oPortDef);

    if (VIDEO_ENCODER_TYPE_MPEG4 == g_cameraParam.videoEncoderType)
    {
        OMX_VIDEO_PARAM_MPEG4TYPE oMpeg4Type;
        INIT_PARAM(oMpeg4Type);
        oMpeg4Type.nPortIndex = 0;
        OMX_GetParameter(hVideoEncoder, OMX_IndexParamVideoMpeg4, &oMpeg4Type);
        if (g_cameraParam.videoEncodeLevel >= g_nLevelsMpeg4)
            g_cameraParam.videoEncodeLevel = g_nLevelsMpeg4 - 1;
        oMpeg4Type.eLevel = g_eLevelMpeg4[g_cameraParam.videoEncodeLevel];
        OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoMpeg4, &oMpeg4Type);
    }
    else  if (VIDEO_ENCODER_TYPE_H264 == g_cameraParam.videoEncoderType)
    {
        OMX_VIDEO_PARAM_AVCTYPE oH264Type;
        INIT_PARAM(oH264Type);
        oH264Type.nPortIndex = 0;
        OMX_GetParameter(hVideoEncoder, OMX_IndexParamVideoAvc, &oH264Type);
        if (g_cameraParam.videoEncodeLevel >= g_nLevelsH264)
            g_cameraParam.videoEncodeLevel = g_nLevelsH264 - 1;
        oH264Type.eLevel = g_eLevelH264[g_cameraParam.videoEncodeLevel];
        OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoAvc, &oH264Type);

        if (g_cameraParam.eRCMode == NVX_VIDEO_RateControlMode_VBR)
        {
            NVX_PARAM_RATECONTROLMODE oRCMode;
            INIT_PARAM(oRCMode);
            oRCMode.nPortIndex = 0;
            oRCMode.eRateCtrlMode = NVX_VIDEO_RateControlMode_VBR;
            eError = OMX_GetExtensionIndex(hVideoEncoder, NVX_INDEX_PARAM_RATECONTROLMODE,
                                       &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            OMX_SetParameter(hVideoEncoder, eIndex, &oRCMode);
        }
    }

    {
        NVX_CONFIG_TEMPORALTRADEOFF oTemporalTradeOff;
        INIT_PARAM(oTemporalTradeOff);
        oTemporalTradeOff.nPortIndex = 0;
        oTemporalTradeOff.TemporalTradeOffLevel = g_cameraParam.TemporalTradeOffLevel;
        eError = OMX_GetExtensionIndex(hVideoEncoder, NVX_INDEX_CONFIG_VIDEO_ENCODE_TEMPORALTRADEOFF,
                                       &eIndex);
        TEST_ASSERT_NOTERROR(eError);
        OMX_SetConfig(hVideoEncoder, eIndex, &oTemporalTradeOff);
    }

    {
        OMX_CONFIG_ROTATIONTYPE oRotation;
        INIT_PARAM(oRotation);
        oRotation.nPortIndex = 0;
        oRotation.nRotation = g_cameraParam.nVideoRotation;
        eError = OMX_SetConfig(hVideoEncoder, OMX_IndexConfigCommonRotate, &oRotation);
        TEST_ASSERT_NOTERROR(eError);
    }

    {
        OMX_CONFIG_MIRRORTYPE oMirror;
        INIT_PARAM(oMirror);
        oMirror.nPortIndex = 0;
        oMirror.eMirror = g_cameraParam.eVideoMirror;
        eError = OMX_SetConfig(hVideoEncoder, OMX_IndexConfigCommonMirror, &oMirror);
        TEST_ASSERT_NOTERROR(eError);
    }

    {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD oIntraPeriod;
        INIT_PARAM(oIntraPeriod);
        oIntraPeriod.nPortIndex = 0;
        oIntraPeriod.nIDRPeriod = g_cameraParam.nIntraFrameInterval;
        oIntraPeriod.nPFrames = g_cameraParam.nIntraFrameInterval -1;
        eError = OMX_SetConfig(hVideoEncoder, OMX_IndexConfigVideoAVCIntraPeriod, &oIntraPeriod);
        TEST_ASSERT_NOTERROR(eError);
    }

    INIT_PARAM(oBitRate);
    oBitRate.nTargetBitrate = g_cameraParam.nBitRate;
    oBitRate.eControlRate = OMX_Video_ControlRateConstantSkipFrames;
    OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoBitrate, &oBitRate);

    INIT_PARAM(oFrameRate);
    oFrameRate.xEncodeFramerate = ((g_cameraParam.nFrameRate)<<16);
    OMX_SetConfig(hVideoEncoder, OMX_IndexConfigVideoFramerate, &oFrameRate);

    INIT_PARAM(oEncodeProp);
    oEncodeProp.nPortIndex = 1;
    oEncodeProp.eApplicationType = (g_cameraParam.nAppType == 0) ?
        NVX_VIDEO_Application_Camcorder : NVX_VIDEO_Application_VideoTelephony;
    switch (g_cameraParam.nErrorResilLevel)
    {
        case 0: oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_None; break;
        case 1: oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_Low; break;
        case 2: oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_High; break;
        default:oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_None; break;
    }
    oEncodeProp.bSvcEncodeEnable = g_cameraParam.enableSvcVideoEncoding;
    // Reset bInsertVUI & bInsertAUD to avoid RTP mode getting disabled at encoder.
    oEncodeProp.bInsertVUI = 0;
    oEncodeProp.bInsertAUD = 0;
    oEncodeProp.nVirtualBufferSize = 0; // Set it to zero as not specified

    eError = OMX_GetExtensionIndex(hVideoEncoder, NVX_INDEX_PARAM_VIDEO_ENCODE_PROPERTY, &eIndex);
    if (eError == OMX_ErrorNone)
    {
        OMX_SetParameter( hVideoEncoder, eIndex, &oEncodeProp);
    }

    INIT_PARAM(oQFactor);
    switch (g_cameraParam.nQualityLevel)
    {
        case 0: oQFactor.nQFactor = 33; break;
        case 1: oQFactor.nQFactor = 66; break;
        case 2: oQFactor.nQFactor = 100; break;
        default: oQFactor.nQFactor = 66; break;
    }
    OMX_SetParameter( hVideoEncoder, OMX_IndexParamQFactor, &oQFactor);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE InitAVRecorderGraph(char *Camera, char *PreviewRender,
                                         char *AudioCapturer, OMX_AUDIO_CODINGTYPE eAudioCodingType,
                                         NvxGraph *pGraph)
{
    OMX_ERRORTYPE eError;
    OMX_HANDLETYPE hAVWriter;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;

    char *videoEncoder = VIDEO_ENCODER_TYPE_MPEG4 == g_cameraParam.videoEncoderType ?
                         g_Mpeg4Encoder :
                         (VIDEO_ENCODER_TYPE_H263 == g_cameraParam.videoEncoderType ? g_H263Encoder : g_H264Encoder);
    char *VIEncoder = g_cameraParam.videoCapture ? videoEncoder : g_JpegEncoder;
    char *AVWriter = (g_cameraParam.enableCameraCapture && !g_cameraParam.videoCapture) ?
                      g_ImageSequenceWriter : g_3gpWriter;
    char *AudioEncoder = NULL;
    NvU32 WriterAudioPort = 1;

    if(OMX_AUDIO_CodingAMR == eAudioCodingType)
    {
        if(g_cameraParam.nAudioSampleRate == 16000)
            AudioEncoder = g_AmrWBEncoder;
        else
            AudioEncoder = g_AmrEncoder;
    }
    else if(OMX_AUDIO_CodingPCM == eAudioCodingType)
    {
        AudioEncoder = g_wavEncoder;
        AVWriter = g_WavWriter;
        WriterAudioPort = 0;
    }
    else
        AudioEncoder = g_AacEncoder;

    // safety check
    if ((g_cameraParam.enableAudio && g_cameraParam.enableCameraCapture && !g_cameraParam.videoCapture) ||
        (!g_cameraParam.enableAudio && !g_cameraParam.enableCameraCapture && !g_cameraParam.enablePreview) ||
        (g_cameraParam.enableAudio && g_cameraParam.enableCameraCapture && g_cameraParam.audioTime != g_cameraParam.captureTime) ||
        (g_cameraParam.enableAudio && OMX_AUDIO_CodingPCM == eAudioCodingType && (g_cameraParam.enableCameraCapture || g_cameraParam.videoCapture ||
        g_cameraParam.enablePreview)))
    {
        NvOsDebugPrintf("invalid parameters\n");
        return OMX_ErrorBadParameter;
    }

    bKeepDisplayOn = OMX_TRUE;

    // check existence of directory
    if (g_cameraParam.enableAudio || g_cameraParam.enableCameraCapture)
    {
        int size = NvOsStrlen(pGraph->outfile) + 1;
        char *dirpath;
        char *str;
        NvOsStatType stat = { 0, NvOsFileType_Directory };
        NvError err;

        dirpath = NvOsAlloc(size);
        if (!dirpath)
            return OMX_ErrorInsufficientResources;

        NvOsStrncpy(dirpath, pGraph->outfile, size);
        str = strrchr(dirpath, '\\');
        if (!str)
            str = strrchr(dirpath, '/');

        if (str)
        {
            *str = 0;
            err = NvOsStat(dirpath, &stat);

            if (err != NvSuccess)
            {
                NvOsDebugPrintf("Directory '%s' not found\n", dirpath);
                NvOsFree(dirpath);
                return OMX_ErrorInsufficientResources;
            }
        }

        NvOsFree(dirpath);
    }

    // clock component
    eError = InitClock(g_Clock, pGraph, &g_oAppData, &g_oCallbacks);
    TEST_ASSERT_NOTERROR(eError);
    AddComponentToGraph(&pGraph->oCompGraph, pGraph->hClock);

    if (g_cameraParam.enableAudio || g_cameraParam.enableCameraCapture)
    {
        // file writer component
        eError = InitReaderWriter(AVWriter, pGraph->outfile, &hAVWriter, pGraph, &g_oAppData, &g_oCallbacks);
        g_FileReader = hAVWriter;
        TEST_ASSERT_NOTERROR(eError);
        AddComponentToGraph(&pGraph->oCompGraph, hAVWriter);
        OMX_SendCommand(hAVWriter, OMX_CommandPortDisable, OMX_ALL, 0);

        if (g_cameraParam.n3gpMaxFramesAudio || g_cameraParam.n3gpMaxFramesVideo)
        {
            OMX_INDEXTYPE  eIndex;
            OMX_PARAM_U32TYPE            oMaxFramesParam;

            INIT_PARAM(oMaxFramesParam);
            eError = OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_MAXFRAMES, &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            if (g_cameraParam.n3gpMaxFramesAudio)
            {
                oMaxFramesParam.nPortIndex = 1;
                oMaxFramesParam.nU32 = g_cameraParam.n3gpMaxFramesAudio;
                eError = OMX_SetParameter(hAVWriter, eIndex, &oMaxFramesParam);
                TEST_ASSERT_NOTERROR(eError);
            }
            if (g_cameraParam.n3gpMaxFramesVideo)
            {
                oMaxFramesParam.nPortIndex = 0;
                oMaxFramesParam.nU32 = g_cameraParam.n3gpMaxFramesVideo;
                eError = OMX_SetParameter(hAVWriter, eIndex, &oMaxFramesParam);
                TEST_ASSERT_NOTERROR(eError);
            }
        }

        if (!g_cameraParam.enableCameraCapture || g_cameraParam.videoCapture)
        {
            if (OMX_AUDIO_CodingAAC == eAudioCodingType)
            {
                OMX_AUDIO_PARAM_AACPROFILETYPE oAacProfile;
                INIT_PARAM(oAacProfile);
                oAacProfile.nPortIndex = 1;
                OMX_GetParameter( hAVWriter, OMX_IndexParamAudioAac, &oAacProfile);
                oAacProfile.eAACProfile = g_cameraParam.eAacProfileType;
                if((g_cameraParam.eAacProfileType == OMX_AUDIO_AACObjectHE_PS) &&
                   (g_cameraParam.nAudioBitRate == 128000))
                {
                    g_cameraParam.nAudioBitRate = 64000;
                }
                else if((g_cameraParam.eAacProfileType == OMX_AUDIO_AACObjectHE) &&
                        (g_cameraParam.nAudioBitRate == 128000 && g_cameraParam.nChannels == 1))
                {
                        g_cameraParam.nAudioBitRate = 64000;
                }
                oAacProfile.nSampleRate = g_cameraParam.nAudioSampleRate;
                oAacProfile.nBitRate = g_cameraParam.nAudioBitRate;
                oAacProfile.nChannels = g_cameraParam.nChannels;
                OMX_SetParameter( hAVWriter, OMX_IndexParamAudioAac, &oAacProfile);
            }
        }

        pGraph->oCompGraph.hEndComponent = hAVWriter;
        g_oAppData.hEndComponent2 = hAVWriter;
    }

    if (g_cameraParam.enableCameraCapture || g_cameraParam.enablePreview)
    {
        OMX_HANDLETYPE hCamera;
        OMX_INDEXTYPE  eIndexSensorId, eIndex;
        OMX_PARAM_SENSORMODETYPE     oSensorMode;
        OMX_PARAM_U32TYPE            oSensorIdParam;
        NVX_CONFIG_STEREOCAPABLE     oStCapable;
        NVX_PARAM_STEREOCAMERAMODE   oStMode;
        NVX_PARAM_STEREOCAPTUREINFO  oStCapInfo;


        // camera is needed
        OMX_SendCommand(pGraph->hClock, OMX_CommandPortEnable, 0, 0);   // camera

        // camera component
        eError = OMX_GetHandle(&hCamera, Camera, &g_oAppData, &g_oCallbacks);
        TEST_ASSERT_NOTERROR(eError);
        AddComponentToGraph(&pGraph->oCompGraph, hCamera);
        SetProfileMode(hCamera, &g_CameraProf);
        g_hCamera = hCamera;
        pGraph->oCompGraph.hCameraComponent = hCamera;
        OMX_SendCommand(hCamera, OMX_CommandPortEnable, 2, 0);          // clock in

        g_cameraParam.nTSAppCameraComponentStart = NvOsGetTimeUS() * 10;

        INIT_PARAM(oSensorIdParam);
        oSensorIdParam.nU32 = g_cameraParam.SensorId;
        eError = OMX_GetExtensionIndex(hCamera, NVX_INDEX_PARAM_SENSORID,
                                       &eIndexSensorId);
        TEST_ASSERT_NOTERROR(eError);
        eError = OMX_SetParameter(hCamera, eIndexSensorId, &oSensorIdParam);
        TEST_ASSERT_NOTERROR(eError);

        eError = OMX_SetupTunnel(pGraph->hClock, 0, hCamera, 2);
        TEST_ASSERT_NOTERROR(eError);


#if 0
        {
            OMX_INDEXTYPE  eIndex;
            NVX_CONFIG_SENSORMODELIST            oSensorModeList;

            INIT_PARAM(oSensorModeList);
            eError = OMX_GetExtensionIndex(hCamera, NVX_INDEX_CONFIG_SENSORMODELIST, &eIndex);
            TEST_ASSERT_NOTERROR(eError);
            oSensorModeList.nSensorModes = 0;
            eError = OMX_GetConfig(hCamera, eIndex, &oSensorModeList);
            TEST_ASSERT_NOTERROR(eError);
        }
#endif
        if ((g_cameraParam.StCamMode < NvxLeftOnly) ||
            (g_cameraParam.StCamMode > NvxStereo))
        {
            g_cameraParam.StCamMode = NvxLeftOnly;
        }
        eError = OMX_GetExtensionIndex(hCamera, NVX_INDEX_CONFIG_STEREOCAPABLE,
                                           &eIndex);
        TEST_ASSERT_NOTERROR(eError);
        INIT_PARAM(oStCapable);
        eError = OMX_GetConfig(hCamera, eIndex, &oStCapable);

        if((OMX_ErrorNone == eError) && oStCapable.StereoCapable)
        {
            INIT_PARAM(oStMode);
            oStMode.StereoCameraMode = g_cameraParam.StCamMode;
            eError = OMX_GetExtensionIndex(hCamera, NVX_INDEX_PARAM_STEREOCAMERAMODE, &eIndex);
            if (eError == OMX_ErrorNone)
            {
                OMX_SetParameter(hCamera, eIndex, &oStMode);
            }

            /* Set StereoCaptureInfo only if we are in stereo mode*/
            if (oStMode.StereoCameraMode == NvxStereo)
            {
                INIT_PARAM(oStCapInfo);
                oStCapInfo.StCaptureInfo.CameraCaptureType =
                    g_cameraParam.StCapInfo.CameraCaptureType;
                oStCapInfo.StCaptureInfo.CameraStereoType =
                    g_cameraParam.StCapInfo.CameraStereoType;
                eError = OMX_GetExtensionIndex(hCamera,
                            NVX_INDEX_PARAM_STEREOCAPTUREINFO, &eIndex);
                if (eError == OMX_ErrorNone)
                {
                    OMX_SetParameter(hCamera, eIndex, &oStCapInfo);
                }
            }
        }

        // For video capture
        if(g_cameraParam.videoCapture)
        {
            INIT_PARAM(oSensorMode);
            oSensorMode.nPortIndex = 5;
            OMX_GetParameter(hCamera, OMX_IndexParamCommonSensorMode, &oSensorMode);
            oSensorMode.bOneShot = !g_cameraParam.videoCapture;
            OMX_SetParameter(hCamera, OMX_IndexParamCommonSensorMode, &oSensorMode);
        }
        else if(g_cameraParam.enableCameraCapture)
        {
            INIT_PARAM(oSensorMode);
            oSensorMode.nPortIndex = 1;
            OMX_GetParameter(hCamera, OMX_IndexParamCommonSensorMode, &oSensorMode);
            oSensorMode.bOneShot = !g_cameraParam.videoCapture;
            OMX_SetParameter(hCamera, OMX_IndexParamCommonSensorMode, &oSensorMode);
        }


        if (1 < g_cameraParam.stillCountBurst)
        {
            OMX_PARAM_U32TYPE            oNumAvailableStreams;
            INIT_PARAM(oNumAvailableStreams);
            oNumAvailableStreams.nPortIndex = 1;
            OMX_GetParameter(hCamera, OMX_IndexParamNumAvailableStreams, &oNumAvailableStreams);
            oNumAvailableStreams.nU32 = g_cameraParam.stillCountBurst;
            OMX_SetParameter(hCamera, OMX_IndexParamNumAvailableStreams, &oNumAvailableStreams);
        }

        INIT_PARAM(oPortDef);
        if (!g_cameraParam.enablePreview)
        {
            OMX_SendCommand(hCamera, OMX_CommandPortDisable, 0, 0);
        }
        else
        {
            OMX_HANDLETYPE hPreviewRender;

            OMX_SendCommand(pGraph->hClock, OMX_CommandPortEnable, 1, 0);   // preview_render
            OMX_SendCommand(hCamera, OMX_CommandPortEnable, 0, 0);          // preview out

            // camera preview port
            oPortDef.nPortIndex = 0;
            OMX_GetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);
            oPortDef.format.video.nFrameWidth = g_cameraParam.nWidthPreview;
            oPortDef.format.video.nFrameHeight = g_cameraParam.nHeightPreview;
            OMX_SetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);

            // preview render component
            eError = OMX_GetHandle(&hPreviewRender, PreviewRender, &g_oAppData, &g_oCallbacks);
            TEST_ASSERT_NOTERROR(eError);
            AddComponentToGraph(&pGraph->oCompGraph, hPreviewRender);
            SetProfileMode(hPreviewRender, &g_VidRendProf);
            OMX_SendCommand(hPreviewRender, OMX_CommandPortEnable, 0, 0);   // video in
            OMX_SendCommand(hPreviewRender, OMX_CommandPortEnable, 1, 0);   // clock in
            SetSize(hPreviewRender, pGraph);
            SetCrop(hPreviewRender, pGraph);
            SetKeepAspect(hPreviewRender, pGraph);

#if USE_ANDROID_WINSYS
            if ((pGraph->renderer != g_HdmiRender) &&
                (pGraph->renderer != g_TvoutRender) &&
                (pGraph->renderer != g_SecondaryRender) &&
                (!pGraph->bNoWindow))
            {
                NvBool succeed = NV_FALSE;
                NvU32 nXOffset = 0;
                NvU32 nYOffset = 0;
                NvU32 nWinWidth = g_cameraParam.nWidthPreview;
                NvU32 nWinHeight = g_cameraParam.nHeightPreview;
                NvxWinCallbacks oWinCallbacks;

                NvOsMemset(&oWinCallbacks, 0, sizeof(NvxWinCallbacks));
                oWinCallbacks.pWinMoveCallback = NvxWindowMoveFunc;
                oWinCallbacks.pWinResizeCallback = NvxWindowResizeFunc;
                oWinCallbacks.pWinCloseCallback = NvxWindowCloseFunc;
                oWinCallbacks.pClientData = pGraph;

                succeed = NvxCreateWindow(nXOffset, nYOffset, nWinWidth, nWinHeight, &pGraph->hWnd, &oWinCallbacks);
                if (succeed && pGraph->hWnd)
                {
                    OMX_INDEXTYPE eIndex;
                    NVX_CONFIG_ANDROIDWINDOW oExt;

                    eError = OMX_GetExtensionIndex(hPreviewRender,
                                 (char *)NVX_INDEX_CONFIG_ANDROIDWINDOW,
                                 &eIndex);
                    if (OMX_ErrorNone == eError)
                    {
                        INIT_PARAM(oExt);
                        oExt.oAndroidWindowPtr = (NvU64)NvWinSysWindowGetNativeHandle(pGraph->hWnd);
                        OMX_SetConfig(hPreviewRender, eIndex, &oExt);
                    } else {
                        NvOsDebugPrintf("OMX_GetExtensionIndex NVX_INDEX_CONFIG_ANDROIDWINDOW failed\n");
                        return eError;
                    }
                } else {
                    NvOsDebugPrintf("NvxCreateWindow failed in InitAVRecorderGraph\n");
                    return succeed;
                }
            }

#else
            // original way of rendering that get the "2s elapsed in InitFifo; is daemon started" error in
            // dev-hc branch honeycomb release - 4/26/11
            oPortDef.nPortIndex = 0;
            OMX_GetParameter(hPreviewRender, OMX_IndexParamPortDefinition, &oPortDef);
            oPortDef.format.video.nFrameWidth = g_cameraParam.nWidthPreview;
            oPortDef.format.video.nFrameHeight = g_cameraParam.nHeightPreview;
            OMX_SetParameter(hPreviewRender, OMX_IndexParamPortDefinition, &oPortDef);

#endif
            eError = OMX_SetupTunnel(pGraph->hClock, 1, hPreviewRender, 1);
            TEST_ASSERT_NOTERROR(eError);

            eError = OMX_SetupTunnel(hCamera, 0, hPreviewRender, 0);
            TEST_ASSERT_NOTERROR(eError);

            pGraph->oCompGraph.hEndComponent = hPreviewRender;
        }

        if (!g_cameraParam.enableCameraCapture)
        {
            OMX_SendCommand(hCamera, OMX_CommandPortDisable, 1, 0);
        }
        else
        {
            OMX_HANDLETYPE hVIEncoder;

            if(g_cameraParam.videoCapture)
                OMX_SendCommand(hCamera, OMX_CommandPortEnable, 5, 0);          // video out
            else if(g_cameraParam.enableCameraCapture)
                OMX_SendCommand(hCamera, OMX_CommandPortEnable, 1, 0);          // capture out

            OMX_SendCommand(hAVWriter, OMX_CommandPortEnable, 0, 0);        // video in

            // Special case where app sets 0 resolution for still port to pick maximum
            // supported resolution of device camera sensor.
            // Update resolution before OMX_SetParameter call on all graph components.
            if ((g_cameraParam.nWidthCapture == 0) && (g_cameraParam.nHeightCapture == 0))
            {
                OMX_ERRORTYPE omxErr = OMX_ErrorNone;
                OMX_INDEXTYPE eInd = OMX_IndexMax;
                NVX_CONFIG_SENSORMODELIST oModes;
                OMX_U32 maxWidth = 0;
                OMX_U32 maxHeight = 0;

                omxErr = OMX_GetExtensionIndex(hCamera,
                                   (char *)NVX_INDEX_CONFIG_SENSORMODELIST,
                                   &eInd);
                if (omxErr != OMX_ErrorNone)
                {
                    NvOsDebugPrintf("OMX_GetExtensionIndex failed [0x%0x]\n", omxErr);
                    return omxErr;
                }

                INIT_PARAM(oModes);
                oModes.nPortIndex = 1;

                omxErr = OMX_GetParameter(hCamera,eInd,&oModes);
                if (omxErr != OMX_ErrorNone)
                {
                    NvOsDebugPrintf("GetSensorModesList fails [0x%0x]\n", omxErr);
                    return omxErr;
                }

                OMX_U32 i;
                for (i = 0; i < oModes.nSensorModes; i++)
                {
                    if ((oModes.SensorModeList[i].width > maxWidth) &&
                    (oModes.SensorModeList[i].height > maxHeight))
                    {
                        maxWidth = oModes.SensorModeList[i].width;
                        maxHeight = oModes.SensorModeList[i].height;
                    }
                }

                NvOsDebugPrintf("maxWidth %d maxHeight %d\n", maxWidth, maxHeight);
                g_cameraParam.nWidthCapture = maxWidth;
                g_cameraParam.nHeightCapture = maxHeight;
            }

            if(g_cameraParam.videoCapture)
            {
                //  Video Port
                NVX_CONFIG_AUTOFRAMERATE     oAutoFrameRate;
                OMX_INDEXTYPE eIndexAutoFrameRate;

                // Disable Capture Port
                OMX_SendCommand(hCamera, OMX_CommandPortDisable, 1, 0);

                oPortDef.nPortIndex = 5;
                OMX_GetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);
                oPortDef.format.video.nFrameWidth = g_cameraParam.nWidthCapture;
                oPortDef.format.video.nFrameHeight = g_cameraParam.nHeightCapture;
                OMX_SetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);

                eError = OMX_GetExtensionIndex(g_hCamera, NVX_INDEX_CONFIG_AUTOFRAMERATE,
                                           &eIndexAutoFrameRate);
                INIT_PARAM(oAutoFrameRate);
                oAutoFrameRate.bEnabled = g_cameraParam.bAutoFrameRate;
                oAutoFrameRate.low = g_cameraParam.nAutoFrameRateLow;
                oAutoFrameRate.high = g_cameraParam.nAutoFrameRateHigh;
                OMX_SetConfig(g_hCamera, eIndexAutoFrameRate, &oAutoFrameRate);
            }
            else if(g_cameraParam.enableCameraCapture)
            {
                // camera capture port
                oPortDef.nPortIndex = 1;
                OMX_GetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);
                oPortDef.format.video.nFrameWidth = g_cameraParam.nWidthCapture;
                oPortDef.format.video.nFrameHeight = g_cameraParam.nHeightCapture;
                OMX_SetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);
            }


            // video/image encoder component
            eError = OMX_GetHandle(&hVIEncoder, VIEncoder, &g_oAppData, &g_oCallbacks);
            TEST_ASSERT_NOTERROR(eError);

            g_hVIEncoder = hVIEncoder;

            AddComponentToGraph(&pGraph->oCompGraph, hVIEncoder);
            OMX_SendCommand(hVIEncoder, OMX_CommandPortEnable, 0, 0);    // video in
            OMX_SendCommand(hVIEncoder, OMX_CommandPortEnable, 1, 0);    // video out

            if (g_cameraParam.videoCapture)
            {
                // video encoder component
                SetProfileMode(hVIEncoder, &g_H264EncoderProf);

                s_SetupVideoEncoder(hVIEncoder);
                eError = OMX_SetupTunnel(hCamera, 5, hVIEncoder, 0);
                TEST_ASSERT_NOTERROR(eError);

                eError = OMX_SetupTunnel(hVIEncoder, 1, hAVWriter, 0);
                TEST_ASSERT_NOTERROR(eError);
            }
            else
            {
                // image encoder component
                SetProfileMode(hVIEncoder, &g_JpegEncoderProf);

                oPortDef.nPortIndex = 0;
                OMX_GetParameter(hVIEncoder, OMX_IndexParamPortDefinition, &oPortDef);
                oPortDef.format.image.nFrameWidth = g_cameraParam.nWidthCapture;
                oPortDef.format.image.nFrameHeight = g_cameraParam.nHeightCapture;
                OMX_SetParameter(hVIEncoder, OMX_IndexParamPortDefinition, &oPortDef);

                if (pGraph->bThumbnailPreferred)
                {
                    OMX_SendCommand(hCamera, OMX_CommandPortEnable, 3, 0);      // thumbnail out
                    OMX_SendCommand(hVIEncoder, OMX_CommandPortEnable, 2, 0);   // thumbnail in

                    // camera thumbnail port
                    oPortDef.nPortIndex = 3;
                    OMX_GetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);
                    oPortDef.format.video.nFrameWidth = pGraph->nThumbnailWidth;
                    oPortDef.format.video.nFrameHeight = pGraph->nThumbnailHeight;
                    OMX_SetParameter(hCamera, OMX_IndexParamPortDefinition, &oPortDef);

                    // image encoder thumbnail port
                    oPortDef.nPortIndex = 2;
                    OMX_GetParameter(hVIEncoder, OMX_IndexParamPortDefinition, &oPortDef);
                    oPortDef.format.image.nFrameWidth = pGraph->nThumbnailWidth;
                    oPortDef.format.image.nFrameHeight = pGraph->nThumbnailHeight;
                    OMX_SetParameter(hVIEncoder, OMX_IndexParamPortDefinition, &oPortDef);

                    eError = OMX_SetupTunnel(hCamera, 3, hVIEncoder, 2);
                    TEST_ASSERT_NOTERROR(eError);
                }

                if (pGraph->nEncodedSize)
                {
                    OMX_VIDEO_CONFIG_NALSIZE oNalSize;
                    INIT_PARAM(oNalSize);
                    oNalSize.nPortIndex = 1;
                    oNalSize.nNaluBytes = pGraph->nEncodedSize;
                    OMX_SetConfig( hVIEncoder, OMX_IndexConfigVideoNalSize, &oNalSize);
                }

                // Image encoder quality setup
                {
                    OMX_IMAGE_PARAM_QFACTORTYPE oQFactor;
                    INIT_PARAM(oQFactor);
                    oQFactor.nPortIndex = 0;
                    oQFactor.nQFactor = g_cameraParam.nImageQuality;
                    OMX_SetParameter( hVIEncoder, OMX_IndexParamQFactor, &oQFactor);
                }
                if(g_cameraParam.bFlagThumbnailQF == OMX_TRUE)
                {
                    OMX_IMAGE_PARAM_QFACTORTYPE oQFactor;
                    OMX_INDEXTYPE  eIndex;
                    eError = OMX_GetExtensionIndex(hVIEncoder, NVX_INDEX_CONFIG_THUMBNAILQF,&eIndex);
                    TEST_ASSERT_NOTERROR(eError);
                    INIT_PARAM(oQFactor);
                    oQFactor.nPortIndex = 0;
                    oQFactor.nQFactor = g_cameraParam.nThumbnailQuality;
                    OMX_SetConfig( hVIEncoder, eIndex, &oQFactor);
                }
                // Custom Qunat Tables set up
                if(g_cameraParam.nImageQTablesSet_Luma == NV_TRUE)
                {
                    OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE oQTable;
                    INIT_PARAM(oQTable);
                    oQTable.nPortIndex = 0;

                    oQTable.eQuantizationTable = OMX_IMAGE_QuantizationTableLuma;
                    NvOsMemcpy(oQTable.nQuantizationMatrix, Std_QuantTable_Luma, 64);
                    OMX_SetParameter( hVIEncoder, OMX_IndexParamQuantizationTable, &oQTable);
                }
                if(g_cameraParam.nImageQTablesSet_Chroma == NV_TRUE)
                {
                    OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE oQTable;
                    INIT_PARAM(oQTable);
                    oQTable.nPortIndex = 0;

                    oQTable.eQuantizationTable = OMX_IMAGE_QuantizationTableChroma;
                    NvOsMemcpy(oQTable.nQuantizationMatrix, Std_QuantTable_Chroma, 64);
                    OMX_SetParameter( hVIEncoder, OMX_IndexParamQuantizationTable, &oQTable);
                }
                eError = OMX_SetupTunnel(hCamera, 1, hVIEncoder, 0);
                TEST_ASSERT_NOTERROR(eError);

                eError = OMX_SetupTunnel(hVIEncoder, 1, hAVWriter, 0);
                TEST_ASSERT_NOTERROR(eError);
            }
        }
    }

    if (g_cameraParam.enableAudio)
    {
        OMX_HANDLETYPE hAudioCapturer, hAudioEncoder;

        OMX_SendCommand(pGraph->hClock, OMX_CommandPortEnable, 2, 0);   // audio source
        OMX_SendCommand(hAVWriter, OMX_CommandPortEnable, WriterAudioPort, 0);        // audio in

        // audio capturer component
        eError = OMX_GetHandle(&hAudioCapturer, AudioCapturer, &g_oAppData, &g_oCallbacks);
        TEST_ASSERT_NOTERROR(eError);
        AddComponentToGraph(&pGraph->oCompGraph, hAudioCapturer);
        SetProfileMode(hAudioCapturer, &g_AudioRecProf);
        g_hAudioCapturer = hAudioCapturer;
        OMX_SendCommand(hAudioCapturer, OMX_CommandPortEnable, 0, 0);   // audio out
        OMX_SendCommand(hAudioCapturer, OMX_CommandPortEnable, 1, 0);   // clock in

        {
            OMX_AUDIO_PARAM_PCMMODETYPE oCapturerPcm;
            INIT_PARAM(oCapturerPcm);
            oCapturerPcm.nPortIndex = 0;
            OMX_GetParameter( hAudioCapturer, OMX_IndexParamAudioPcm, &oCapturerPcm);
            oCapturerPcm.nSamplingRate = g_cameraParam.nAudioSampleRate;
            oCapturerPcm.nChannels = g_cameraParam.nChannels;
            OMX_SetParameter( hAudioCapturer, OMX_IndexParamAudioPcm, &oCapturerPcm);
        }

        {
            NVX_CONFIG_AUDIO_DRCPROPERTY oAgcProperty;
            OMX_INDEXTYPE  eIndex;

            eError = OMX_GetExtensionIndex(hAudioCapturer, NVX_INDEX_CONFIG_AUDIO_DRCPROPERTY,
                                           &eIndex);
            INIT_PARAM(oAgcProperty);
            oAgcProperty.nPortIndex = 0;
            oAgcProperty.EnableDrc = (OMX_S32)g_cameraParam.bDrcEnabled;
            oAgcProperty.ClippingTh  = (OMX_S32)g_cameraParam.ClippingTh;
            oAgcProperty.LowerCompTh = (OMX_S32)g_cameraParam.LowerCompTh;
            oAgcProperty.UpperCompTh = (OMX_S32)g_cameraParam.UpperCompTh;
            oAgcProperty.NoiseGateTh = (OMX_S32)g_cameraParam.NoiseGateTh;
            OMX_SetConfig(hAudioCapturer, eIndex, &oAgcProperty);
        }

        // audio encoder component
        eError = OMX_GetHandle(&hAudioEncoder, AudioEncoder, &g_oAppData, &g_oCallbacks);
        TEST_ASSERT_NOTERROR(eError);
        AddComponentToGraph(&pGraph->oCompGraph, hAudioEncoder);
        SetProfileMode(hAudioCapturer, &g_AudioEncProf);
        OMX_SendCommand(hAudioEncoder, OMX_CommandPortEnable, 0, 0);    // audio in
        OMX_SendCommand(hAudioEncoder, OMX_CommandPortEnable, 1, 0);    // audio out

        if (OMX_AUDIO_CodingAAC == eAudioCodingType)
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE oAacProfile;
            INIT_PARAM(oAacProfile);
            oAacProfile.nPortIndex = 1;
            OMX_GetParameter( hAudioEncoder, OMX_IndexParamAudioAac, &oAacProfile);
            oAacProfile.eAACProfile = g_cameraParam.eAacProfileType;
            if((g_cameraParam.eAacProfileType == OMX_AUDIO_AACObjectHE_PS) && (g_cameraParam.nAudioBitRate == 128000))
            {
                g_cameraParam.nAudioBitRate = 64000;
            }
            oAacProfile.nSampleRate = g_cameraParam.nAudioSampleRate;
            oAacProfile.nBitRate = g_cameraParam.nAudioBitRate;
            oAacProfile.nChannels = g_cameraParam.nChannels;
            OMX_SetParameter( hAudioEncoder, OMX_IndexParamAudioAac, &oAacProfile);
        }
        if (OMX_AUDIO_CodingPCM == eAudioCodingType)
        {
            OMX_AUDIO_PARAM_PCMMODETYPE oPcmProfile;
            INIT_PARAM(oPcmProfile);
            oPcmProfile.nPortIndex = 1;
            OMX_GetParameter( hAudioEncoder, OMX_IndexParamAudioPcm, &oPcmProfile);
            oPcmProfile.nChannels = g_cameraParam.nChannels;
            oPcmProfile.nBitPerSample = 16;
            oPcmProfile.nSamplingRate = g_cameraParam.nAudioSampleRate;
            oPcmProfile.ePCMMode = g_cameraParam.pcmMode;
            OMX_SetParameter( hAudioEncoder, OMX_IndexParamAudioPcm, &oPcmProfile);
        }
        if (OMX_AUDIO_CodingAMR == eAudioCodingType)
        {
            OMX_U32 BitRates_AMR[18] = {0, 4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200,
                                       6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850};
            OMX_U32 i;

            OMX_AUDIO_PARAM_AMRTYPE oAmrProfile;
            INIT_PARAM(oAmrProfile);
            oAmrProfile.nPortIndex = 1;
            OMX_GetParameter( hAudioEncoder, OMX_IndexParamAudioAmr, &oAmrProfile);
            for (i = 0; i < 18; i++)
            {
                if (g_cameraParam.nAudioBitRate == BitRates_AMR[i])
                {
                    oAmrProfile.eAMRBandMode = i;
                    oAmrProfile.nBitRate = g_cameraParam.nAudioBitRate;
                    break;
                }
            }
            g_cameraParam.nAudioBitRate = oAmrProfile.nBitRate;
            g_cameraParam.nChannels = oAmrProfile.nChannels;
            oAmrProfile.eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
            OMX_SetParameter( hAudioEncoder, OMX_IndexParamAudioAmr, &oAmrProfile);
            OMX_SetParameter( hAVWriter, OMX_IndexParamAudioAmr, &oAmrProfile);
        }

        eError = OMX_SetupTunnel(pGraph->hClock, 2, hAudioCapturer, 1);
        TEST_ASSERT_NOTERROR(eError);

        eError = OMX_SetupTunnel(hAudioCapturer, 0, hAudioEncoder, 0);
        TEST_ASSERT_NOTERROR(eError);

        eError = OMX_SetupTunnel(hAudioEncoder, 1, hAVWriter, WriterAudioPort);
        TEST_ASSERT_NOTERROR(eError);
    }

    return eError;
}

static const char *StreamTypeToChar(ENvxStreamType eType)
{
    switch (eType)
    {
        case NvxStreamType_MPEG4:
        case NvxStreamType_MPEG4Ext: return "mpeg4";
        case NvxStreamType_H264:
        case NvxStreamType_H264Ext: return "h264";
        case NvxStreamType_H263: return "h263";
        case NvxStreamType_WMV: return "wmv";
        case NvxStreamType_MPEG2V: return "mpeg-2v";
        case NvxStreamType_MJPEG: return "mjpeg";
        case NvxStreamType_JPG: return "jpg";
        case NvxStreamType_MP3: return "mp3";
        case NvxStreamType_MP2: return "mp2";
        case NvxStreamType_WAV: return "wav";
        case NvxStreamType_AAC: return "aac";
        case NvxStreamType_AACSBR: return "aac-sbr";
        case NvxStreamType_BSAC: return "bsac";
        case NvxStreamType_WMA: return "wma";
        case NvxStreamType_WMAPro: return "wma-pro";
        case NvxStreamType_WMALossless: return "wma-lossless";
        case NvxStreamType_AMRWB: return "amr-wb";
        case NvxStreamType_AMRNB: return "amr-nb";
        case NvxStreamType_VORBIS: return "vorbis";
        case NvxStreamType_ADPCM: return "adpcm";
        case NvxStreamType_AC3: return "ac3";
        case NvxStreamType_MS_MPG4: return "ms-mpg4";
        case NvxStreamType_QCELP: return "qcelp";
        case NvxStreamType_EVRC: return "evrc";
        default: break;
    }

    return "unknown";
}


static void ProbeInput(NvxGraph *pGraph, OMX_HANDLETYPE hParser, int vid, int aud)
{
    NVX_PARAM_DURATION oDuration;
    NVX_PARAM_STREAMTYPE oStreamType;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    NVX_PARAM_AUDIOPARAMS oAudParams;
    OMX_ERRORTYPE eError;
    OMX_INDEXTYPE eParam, eAudioIndex;

    eError = OMX_GetExtensionIndex(hParser, NVX_INDEX_PARAM_DURATION, &eParam);
    TEST_ASSERT_NOTERROR(eError);

    INIT_PARAM(oDuration);
    INIT_PARAM(oStreamType);
    INIT_PARAM(oPortDef);
    INIT_PARAM(oAudParams);

    eError = OMX_GetParameter(hParser, eParam, &oDuration);
    if (eError == OMX_ErrorNone)
    {
        NvOsDebugPrintf("File duration is: %d ms\n", (NvU32)(oDuration.nDuration / 1000));
        pGraph->nFileDuration = (NvU32)(oDuration.nDuration / 1000000); //In sec
    }

    eError = OMX_GetExtensionIndex(hParser, NVX_INDEX_PARAM_STREAMTYPE, &eParam);
    TEST_ASSERT_NOTERROR(eError);

    eError = OMX_GetExtensionIndex(hParser, NVX_INDEX_PARAM_AUDIOPARAMS, &eAudioIndex);
    TEST_ASSERT_NOTERROR(eError);

    if (vid >= 0)
    {
        oStreamType.nPort = vid;
        eError = OMX_GetParameter(hParser, eParam, &oStreamType);
        if (eError == OMX_ErrorNone)
        {
            NvOsDebugPrintf("Stream: %d is %s\n", vid, StreamTypeToChar(oStreamType.eStreamType));
            if (!pGraph->video && !pGraph->novid)
            {
                switch (oStreamType.eStreamType)
                {
                    case NvxStreamType_H263:
                        pGraph->video = g_H263Decoder; break;
                    case NvxStreamType_MPEG4:
                        pGraph->video = g_Mp4Decoder; break;
                    case NvxStreamType_MPEG4Ext:
                        pGraph->video = g_Mp4ExtDecoder; break;
                    case NvxStreamType_WMV:
                        pGraph->video = g_Vc1Decoder; break;
                    case NvxStreamType_H264:
                        pGraph->video = g_H264Decoder; break;
                    case NvxStreamType_H264Ext:
                        pGraph->video = g_H264ExtDecoder; break;
                    case NvxStreamType_MPEG2V:
                        pGraph->video = g_Mpeg2Decoder; break;
                    case NvxStreamType_MJPEG:
                        pGraph->video = g_MJpegDecoder; break;
                    default:
                        break;
                }
            }

            oPortDef.nPortIndex = vid;
            eError = OMX_GetParameter(hParser, OMX_IndexParamPortDefinition, &oPortDef);
            if (eError == OMX_ErrorNone)
                NvOsDebugPrintf("  size: %dx%d\n", oPortDef.format.video.nFrameWidth, oPortDef.format.video.nFrameHeight);
                NvOsDebugPrintf("  Video bit rate: %d Hz\n", oPortDef.format.video.nBitrate);
        }
    }

    if (aud >= 0)
    {
        oStreamType.nPort = aud;
        eError = OMX_GetParameter(hParser, eParam, &oStreamType);
        if (eError == OMX_ErrorNone)
        {
            NvOsDebugPrintf("Stream: %d is %s\n", aud, StreamTypeToChar(oStreamType.eStreamType));
            if (!pGraph->audio && !pGraph->noaud)
            {
                switch (oStreamType.eStreamType)
                {
                    case NvxStreamType_MP2:
                        pGraph->audio = g_Mp2Decoder; break;
                    case NvxStreamType_MP3:
                        pGraph->audio = g_Mp3Decoder; break;
                    case NvxStreamType_WAV:
                        pGraph->audio = g_WavDecoder; break;
                    case NvxStreamType_AAC:
                        pGraph->audio = g_AacDecoder; break;
                    case NvxStreamType_AACSBR:
                        pGraph->audio = g_eAacPDecoder; break;
                    case NvxStreamType_BSAC:
                        pGraph->audio = g_BsacDecoder; break;
                    case NvxStreamType_WMA:
                        pGraph->audio = g_WmaDecoder; break;
                    case NvxStreamType_WMAPro:
                        pGraph->audio = g_WmaProDecoder; break;
                    case NvxStreamType_WMALossless:
                        pGraph->audio = g_WmaLSLDecoder; break;
                    case NvxStreamType_AMRWB:
                        pGraph->audio = g_AmrWBDecoder; break;
                    case NvxStreamType_AMRNB:
                        pGraph->audio = g_AmrDecoder; break;
                    case NvxStreamType_VORBIS:
                        pGraph->audio = g_VorbisDecoder; break;
                    default: break;
                }
            }

            oAudParams.nPort = aud;
            eError = OMX_GetParameter(hParser, eAudioIndex, &oAudParams);
            if (eError == OMX_ErrorNone)
            {
                NvOsDebugPrintf("  rate: %d Hz\n", oAudParams.nSampleRate);
                NvOsDebugPrintf(" chans: %d\n", oAudParams.nChannels);
                NvOsDebugPrintf(" bit/s: %d bps\n", oAudParams.nBitRate);
                NvOsDebugPrintf("  size: %d bits\n", oAudParams.nBitsPerSample);
            }
        }
    }
}

static void ProbeMetadata(NvxGraph *pGraph, OMX_HANDLETYPE hParser)
{
    NVX_CONFIG_QUERYMETADATA md;
    OMX_ERRORTYPE eError;
    OMX_INDEXTYPE eIndex;
    OMX_U8 *pBuffer;
    OMX_U32 len;
    ENvxMetadataType types[3] = { NvxMetadata_Title, NvxMetadata_Artist, NvxMetadata_Album };
    char *typenames[3] = { " Title: ", "Artist: ", " Album: " };
    int i;

    eError = OMX_GetExtensionIndex(hParser, NVX_INDEX_CONFIG_QUERYMETADATA, &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    INIT_PARAM(md);
    len = 0;
    pBuffer = NULL;

    for (i = 0; i < 3; i++)
    {
        md.sValueStr = (char *)pBuffer;
        md.nValueLen = len;
        if (md.sValueStr)
            NvOsMemset(md.sValueStr, 0, len);
        md.eType = types[i];

        eError = OMX_GetConfig(hParser, eIndex, &md);

        // arbitrary limit on the size
        if (OMX_ErrorInsufficientResources == eError && md.nValueLen < 16384)
        {
            NvOsFree(pBuffer);
            len = md.nValueLen + 2;
            pBuffer = NvOsAlloc(len);
            if (!pBuffer)
                return;

            NvOsMemset(pBuffer, 0, len);

            md.sValueStr = (char *)pBuffer;
            md.nValueLen = len;
            eError = OMX_GetConfig(hParser, eIndex, &md);
        }

        if (OMX_ErrorNone == eError)
        {
            if (md.eCharSet == OMX_MetadataCharsetASCII ||
                md.eCharSet == OMX_MetadataCharsetUTF8)
            {
                NvOsDebugPrintf("%s %s\n", typenames[i], md.sValueStr);
            }
            else if (md.eCharSet == NVOMX_MetadataCharsetU32)
            {
                NvOsDebugPrintf("%s %d\n", typenames[i], *((NvU32*)md.sValueStr));
            }
            else if (md.eCharSet == OMX_MetadataCharsetUTF16LE)
            {
                OMX_U32 nSizeConv = 0;
                OMX_U8 *pTmpBuffer = NULL;

                nSizeConv = NvUStrlConvertCodePage(NULL, 0, NvOsCodePage_Utf8,
                                                   md.sValueStr, md.nValueLen,
                                                   NvOsCodePage_Utf16);

                if (nSizeConv <= 0)
                    continue;

                pTmpBuffer = NvOsAlloc(nSizeConv);
                if (!pTmpBuffer)
                    return;

                NvUStrlConvertCodePage(pTmpBuffer, nSizeConv, NvOsCodePage_Utf8,
                                       md.sValueStr, md.nValueLen,
                                       NvOsCodePage_Utf16);
                NvOsDebugPrintf("%s%s\n", typenames[i], pTmpBuffer);

                NvOsFree(pTmpBuffer);
            }
            else if (md.eCharSet == OMX_MetadataCharsetBinary)
            {
                NvOsDebugPrintf("%sUnsupported binary encoding\n", typenames[i]);
            }
        }
    }

    NvOsFree(pBuffer);
}

static void EndOfTrackCallback(NvxCompGraph *pCompGraph)
{
    int i;
    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        NvxGraph *pGraph = &(g_Graphs[i]);
        if (&(pGraph->oCompGraph) == pCompGraph && pGraph->hFileParser)
        {
            ProbeMetadata(pGraph, pGraph->hFileParser);
        }
    }
}

static void DrmDirectLicenseAcquisitionCallback(NvU16 *licenseUrl,NvU32 licenseurlSize,NvU8  *licenseChallenge,NvU32 licenseChallengeSize)
{

    NvError   Status = NvSuccess;
    NvOsDebugPrintf("Attempting to acquire License........");
    Status = NvHttpClient(licenseUrl,licenseurlSize,licenseChallenge,licenseChallengeSize);
    if(Status != NvSuccess)
       NvOsDebugPrintf("License could not be acquired.....");
    else
    {
       NvOsDebugPrintf("License acquired........");
       g_drmlicenseAcquired = OMX_TRUE;
     }

}

static void SaveCoverArt(NvxGraph *pGraph, OMX_HANDLETYPE hParser)
{
    NVX_CONFIG_QUERYMETADATA md;
    OMX_ERRORTYPE eError;
    OMX_INDEXTYPE eIndex;
    OMX_U8 *pBuffer = NULL;
    OMX_U32 len = 0;
    const char *pName = NULL;
    NvOsFileHandle hOut;

    eError = OMX_GetExtensionIndex(hParser, NVX_INDEX_CONFIG_QUERYMETADATA, &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    INIT_PARAM(md);
    md.sValueStr = NULL;
    md.nValueLen = 0;
    md.eType = NvxMetadata_CoverArt;

    eError = OMX_GetConfig(hParser, eIndex, &md);

    if (OMX_ErrorInsufficientResources == eError)
    {
        len = md.nValueLen;
        if (len == 0)
        {
            NvOsDebugPrintf("No coverart\n");
            return;
        }

        pBuffer = NvOsAlloc(len);
        if (!pBuffer)
            return;

        NvOsMemset(pBuffer, 0, len);

        md.sValueStr = (char *)pBuffer;
        md.nValueLen = len;
        eError = OMX_GetConfig(hParser, eIndex, &md);
    }

    if (OMX_ErrorNone != eError ||  md.nValueLen == 0)
    {
        NvOsDebugPrintf("No cover art\n");
        NvOsFree(pBuffer);
        return;
    }

    switch (md.eCharSet)
    {
        case NVOMX_MetadataFormatJPEG: pName = "coverart.jpg"; break;
        case NVOMX_MetadataFormatPNG: pName = "coverart.png"; break;
        case NVOMX_MetadataFormatBMP: pName = "coverart.bmp"; break;
        case NVOMX_MetadataFormatGIF: pName = "coverart.gif"; break;
        default: pName = "coverart.ukn"; break;
    }

    if (NvSuccess != NvOsFopen(pName, NVOS_OPEN_CREATE | NVOS_OPEN_WRITE,
                               &hOut))
    {
        NvOsDebugPrintf("unable to open: %s\n", pName);
        NvOsFree(pBuffer);
        return;
    }

    NvOsFwrite(hOut, pBuffer, md.nValueLen);
    NvOsFclose(hOut);

    NvOsDebugPrintf("Saved %d bytes of cover art to %s\n", md.nValueLen, pName);

    NvOsFree(pBuffer);
}

// Create a playback graph
static OMX_ERRORTYPE InitVideoPlayer(NvxGraph *pGraph)
{
    OMX_ERRORTYPE eError;
    OMX_HANDLETYPE hFileReader = NULL, hAudDec = NULL, hVidRender = NULL;
    OMX_HANDLETYPE hAudRender = NULL, hVidDec = NULL, hCustComp = NULL;
    int testcase = pGraph->testtype;
    char *infile = pGraph->infile;
    char *reader = pGraph->parser;
    char *video = pGraph->video;
    char *audio = pGraph->audio;
    char *renderer = pGraph->renderer;
    int stubrender = pGraph->stubrender;

    eError = InitSourceGraph(testcase, &hFileReader, infile, reader, pGraph);
    TEST_ASSERT_NOTERROR(eError);
    if (eError != OMX_ErrorNone)
    {
        OMX_FreeHandle(hFileReader);
        return eError;
    }

    SetProfileMode(hFileReader, &g_DemuxProf);
    if (pGraph->bEnableFileCache)
        SetFileCacheSize(hFileReader, pGraph->nFileCacheSize);

    if (JPEG_DECODER != testcase)
    {
        ProbeInput(pGraph, hFileReader, 0, 1);
        ProbeMetadata(pGraph, hFileReader);
        if (pGraph->bSaveCoverArt)
            SaveCoverArt(pGraph, hFileReader);
        video = pGraph->video;
        audio = pGraph->audio;
    }

    // Disable any unused ports
    if (!video)
        OMX_SendCommand(hFileReader, OMX_CommandPortDisable, 0, 0);
    if (!audio)
        OMX_SendCommand(hFileReader, OMX_CommandPortDisable, 1, 0);

    if ((!video && !audio) || (pGraph->noaud && pGraph->novid) ||
        (!video && pGraph->noaud) || (!audio && pGraph->novid))
    {
        OMX_FreeHandle(hFileReader);
        return OMX_ErrorBadParameter;
    }

    AddComponentToGraph(&pGraph->oCompGraph, hFileReader);

    // Create a clock component
    eError = InitClock(g_Clock, pGraph, &g_oAppData, &g_oCallbacks);
    TEST_ASSERT_NOTERROR(eError);

    AddComponentToGraph(&pGraph->oCompGraph, pGraph->hClock);

    if (video)
    {
        bKeepDisplayOn = OMX_TRUE;

        // Create the video decoder
        eError = OMX_GetHandle(&hVidDec, video, &g_oAppData, &g_oCallbacks);
        TEST_ASSERT_NOTERROR(eError);
        SetProfileMode(hVidDec, &g_VidDecProf);
        SetLowMem(hVidDec, pGraph->bLowMem);

        if (pGraph->bThumbnailPreferred)
        {
            OMX_INDEXTYPE  eIndexThumbnail;
            NVX_CONFIG_THUMBNAIL oConfigThumbnail;

            eError = OMX_GetExtensionIndex(hVidDec, NVX_INDEX_CONFIG_THUMBNAIL,
                                           &eIndexThumbnail);
            TEST_ASSERT_NOTERROR(eError);
            INIT_PARAM(oConfigThumbnail);
            oConfigThumbnail.bEnabled = OMX_TRUE;
            oConfigThumbnail.nWidth = pGraph->nThumbnailWidth;
            oConfigThumbnail.nHeight = pGraph->nThumbnailHeight;
            eError = OMX_SetConfig(hVidDec, eIndexThumbnail, &oConfigThumbnail);
            TEST_ASSERT_NOTERROR(eError);
        }

        if (NV_SFX_WHOLE_TO_FX(1) != pGraph->xScaleFactor)
        {
            OMX_CONFIG_SCALEFACTORTYPE oScaleFactor;
            INIT_PARAM(oScaleFactor);
            oScaleFactor.nPortIndex = 1;
            oScaleFactor.xWidth = oScaleFactor.xHeight = pGraph->xScaleFactor;
            eError = OMX_SetConfig(hVidDec, OMX_IndexConfigCommonScale, &oScaleFactor);
            TEST_ASSERT_NOTERROR(eError);
        }

        if (pGraph->bIsM2vPlayback)
        {
            // right now deinterlacing is available for m2v file (mpeg2) only.
            OMX_INDEXTYPE eIndex;
            NVX_PARAM_DEINTERLACE oDeinterlace;
            INIT_PARAM(oDeinterlace);
            oDeinterlace.DeinterlaceMethod = pGraph->DeinterlaceMethod;

            eError = OMX_GetExtensionIndex(hVidDec, NVX_INDEX_PARAM_DEINTERLACING,
                                       &eIndex);
            if (OMX_ErrorNone == eError)
            {
                eError = OMX_SetParameter(hVidDec, eIndex, &oDeinterlace);
                TEST_ASSERT_NOTERROR(eError);
            }
        }

        if (stubrender)
        {
            // Use a file to write video to if rendering is disabled
            eError = InitReaderWriter(g_VideoWriter, "output.yuv", &hVidRender, pGraph, &g_oAppData, &g_oCallbacks);
            g_FileReader = hVidRender;
            TEST_ASSERT_NOTERROR(eError);
        }
        else
        {
            // Create a video renderer
            eError = OMX_GetHandle(&hVidRender, renderer, &g_oAppData,
                                   &g_oCallbacks);
            TEST_ASSERT_NOTERROR(eError);

            SetProfileMode(hVidRender, &g_VidRendProf);

            SetSize(hVidRender, pGraph);
            SetCrop(hVidRender, pGraph);
            SetKeepAspect(hVidRender, pGraph);
            if (pGraph->bSmartDimmerEnable)
                SetSmartDimmer(hVidRender, pGraph);

            OMX_SendCommand(pGraph->hClock, OMX_CommandPortEnable, 0, 0);
            OMX_SendCommand(hVidRender, OMX_CommandPortEnable, 1, 0);

            // Set rotation, if necessary
            if (pGraph->nRotate != 0)
            {
                OMX_CONFIG_ROTATIONTYPE oRotation;
                INIT_PARAM(oRotation);

                oRotation.nRotation = pGraph->nRotate;
                OMX_SetConfig(hVidRender, OMX_IndexConfigCommonRotate,
                              &oRotation);
            }

            //Todo: is it possible to avoid these setconfigs?
            if(pGraph->renderer == g_NvMMRenderer)
            {
                if (pGraph->bThumbnailPreferred)
                {
                    OMX_INDEXTYPE  eIndexThumbnail;
                    NVX_CONFIG_THUMBNAIL oConfigThumbnail;

                    eError = OMX_GetExtensionIndex(hVidDec, NVX_INDEX_CONFIG_THUMBNAIL,
                                                   &eIndexThumbnail);
                    TEST_ASSERT_NOTERROR(eError);
                    INIT_PARAM(oConfigThumbnail);
                    oConfigThumbnail.bEnabled = OMX_TRUE;
                    oConfigThumbnail.nWidth = pGraph->nThumbnailWidth;
                    oConfigThumbnail.nHeight = pGraph->nThumbnailHeight;
                    eError = OMX_SetConfig(hVidRender, eIndexThumbnail, &oConfigThumbnail);
                    TEST_ASSERT_NOTERROR(eError);
                }

                if (NV_SFX_WHOLE_TO_FX(1) != pGraph->xScaleFactor)
                {
                    OMX_CONFIG_SCALEFACTORTYPE oScaleFactor;
                    INIT_PARAM(oScaleFactor);
                    oScaleFactor.nPortIndex = 1;
                    oScaleFactor.xWidth = oScaleFactor.xHeight = pGraph->xScaleFactor;
                    eError = OMX_SetConfig(hVidRender, OMX_IndexConfigCommonScale, &oScaleFactor);
                    TEST_ASSERT_NOTERROR(eError);
                }
            }

            if (pGraph->bHideDesktop)
            {
                OMX_INDEXTYPE eIndex;
                NVX_CONFIG_WINDOWOVERRIDE oWinOverride;
                INIT_PARAM(oWinOverride);

                oWinOverride.eWindow = NvxWindow_A;
                oWinOverride.eAction = NvxWindowAction_TurnOff;

                if (OMX_ErrorNone == OMX_GetExtensionIndex(hVidRender,
                          NVX_INDEX_CONFIG_WINDOW_DISP_OVERRIDE, &eIndex))
                {
                    OMX_SetConfig(hVidRender, eIndex,
                              &oWinOverride);
                }
            }
            if (pGraph->StereoRenderMode == OMX_STEREORENDERING_HOR_STITCHED)
            {
                OMX_INDEXTYPE stereorendindex;
                OMX_CONFIG_STEREORENDMODETYPE oStereoSet;
                eError = OMX_GetExtensionIndex(hVidRender,
                NVX_INDEX_CONFIG_STEREORENDMODE, &stereorendindex);
                TEST_ASSERT_NOTERROR(eError);

                INIT_PARAM(oStereoSet);

                oStereoSet.eType = OMX_STEREORENDERING_HOR_STITCHED;
                OMX_SetConfig(hVidRender, stereorendindex,
                              &oStereoSet);
                NvOsDebugPrintf("Stereo rendering enabled \n");

            }

            // If we're not in a special display mode, use native windowing system
            if ((pGraph->renderer != g_HdmiRender) &&
                (pGraph->renderer != g_TvoutRender) &&
                (pGraph->renderer != g_SecondaryRender) &&
                (!pGraph->bNoWindow))
            {
                NvBool succeed = NV_FALSE;
                NvU32 nXOffset = (pGraph->nX > 0) ? pGraph->nX : 0;
                NvU32 nYOffset = (pGraph->nY > 0) ? pGraph->nY : 0;
                NvU32 nWinWidth = (pGraph->nWidth > 0) ? pGraph->nWidth : 0;
                NvU32 nWinHeight = (pGraph->nHeight > 0) ? pGraph->nHeight : 0;
                NvxWinCallbacks oWinCallbacks;

                NvOsMemset(&oWinCallbacks, 0, sizeof(NvxWinCallbacks));
                oWinCallbacks.pWinMoveCallback = NvxWindowMoveFunc;
                oWinCallbacks.pWinResizeCallback = NvxWindowResizeFunc;
                oWinCallbacks.pWinCloseCallback = NvxWindowCloseFunc;
                oWinCallbacks.pClientData = pGraph;

                succeed = NvxCreateWindow(nXOffset, nYOffset, nWinWidth, nWinHeight, &pGraph->hWnd, &oWinCallbacks);
                if (succeed && pGraph->hWnd)
                {
#if USE_ANDROID_WINSYS
                    OMX_INDEXTYPE eIndex;
                    NVX_CONFIG_ANDROIDWINDOW oExt;

                    eError = OMX_GetExtensionIndex(hVidRender,
                                 (char *)NVX_INDEX_CONFIG_ANDROIDWINDOW,
                                 &eIndex);
                    if (OMX_ErrorNone == eError)
                    {
                        INIT_PARAM(oExt);
                        oExt.oAndroidWindowPtr = (NvU64)NvWinSysWindowGetNativeHandle(pGraph->hWnd);
                        OMX_SetConfig(hVidRender, eIndex, &oExt);
                    }
#else
                    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
                    INIT_PARAM(oPortDef);
                    oPortDef.nPortIndex = 0;
                    eError = OMX_GetParameter(hVidRender, OMX_IndexParamPortDefinition, &oPortDef);

                    if ( OMX_ErrorNone == eError)
                    {
                        NvxWindowStruct *win = (NvxWindowStruct *)pGraph->hWnd;
                        oPortDef.format.video.pNativeRender = win->hWnd;
                        eError = OMX_SetParameter( hVidRender, OMX_IndexParamPortDefinition, &oPortDef);

                        if (!pGraph->bAlphaBlend)
                        {
                            OMX_CONFIG_COLORKEYTYPE oColorKey;

                            INIT_PARAM(oColorKey);
                            oColorKey.nARGBColor = 0x00000002;
                            oColorKey.nARGBMask  = 0xFFFFFFFF;
                            OMX_SetConfig( hVidRender, OMX_IndexConfigCommonColorKey, &oColorKey);
                        }
                    }
#endif
                }
            }

            if (pGraph->bAlphaBlend)
            {
                OMX_CONFIG_COLORBLENDTYPE oColorBlend;

                INIT_PARAM(oColorBlend);
                oColorBlend.nRGBAlphaConstant = (OMX_U32)(pGraph->nAlphaValue << 24);
                oColorBlend.eColorBlend = OMX_ColorBlendAlphaConstant;
                OMX_SetConfig( hVidRender, OMX_IndexConfigCommonColorBlend, &oColorBlend);
            }

            // Connect the renderer to the clock component.  This allows the
            // 'video scheduler' component to be unnecessary
            eError = OMX_SetupTunnel(pGraph->hClock, 0, hVidRender, 1);
            TEST_ASSERT_NOTERROR(eError);
        }

        // Tunnel the file reader and video decoder
        eError = OMX_SetupTunnel(hFileReader, 0, hVidDec, 0);

        TEST_ASSERT_NOTERROR(eError);

        // Tunnel the video decoder and renderer
        eError = OMX_SetupTunnel(hVidDec, 1, hVidRender, 0);
        TEST_ASSERT_NOTERROR(eError);

        AddComponentToGraph(&pGraph->oCompGraph, hVidDec);
        AddComponentToGraph(&pGraph->oCompGraph, hVidRender);
    }

    if (audio)
    {
        // Create an audio decoder component
        eError = OMX_GetHandle(&hAudDec, audio, &g_oAppData, &g_oCallbacks);
        TEST_ASSERT_NOTERROR(eError);
        if (pGraph->custcomp)
        {
            eError = OMX_GetHandle(&hCustComp, pGraph->custcomp, &g_oAppData, &g_oCallbacks);
            TEST_ASSERT_NOTERROR(eError);
        }

        SetProfileMode(hAudDec, &g_AudDecProf);
        if (!video)
            SetAudioOnlyHint(hAudDec, OMX_TRUE);

        if (stubrender)
        {
            // Create a file to write PCM data to if rendering is disabled
            eError = InitReaderWriter(g_AudioWriter, "output.pcm", &hAudRender, pGraph, &g_oAppData, &g_oCallbacks);
            g_FileReader = hAudRender;
            TEST_ASSERT_NOTERROR(eError);
        }
        else
        {
            // Create an audio renderer
            eError = OMX_GetHandle(&hAudRender, g_AudioRender, &g_oAppData,
                                   &g_oCallbacks);
            TEST_ASSERT_NOTERROR(eError);

            if (!video)
                SetDisableTimestampUpdates(hAudRender, OMX_TRUE);

            // Hook the audio renderer to the clock
            OMX_SendCommand(pGraph->hClock, OMX_CommandPortEnable, 1, 0);

            SetVolume(hAudRender, pGraph);
            SetAudioOutput(hAudRender, pGraph);
            if (pGraph->bSetDB)
                SetEQ(hAudRender, pGraph);

            eError = OMX_SetupTunnel(pGraph->hClock, 1, hAudRender, 1);
            TEST_ASSERT_NOTERROR(eError);

            {
                NVX_CONFIG_AUDIO_DRCPROPERTY oDrcProperty;
                OMX_INDEXTYPE  eIndex;

                eError = OMX_GetExtensionIndex(hAudRender, NVX_INDEX_CONFIG_AUDIO_DRCPROPERTY,
                                               &eIndex);
                INIT_PARAM(oDrcProperty);
                oDrcProperty.nPortIndex = 0;
                oDrcProperty.EnableDrc = (OMX_S32)g_cameraParam.bDrcEnabled; // borrow from camera
                oDrcProperty.ClippingTh  = (OMX_S32)g_cameraParam.ClippingTh;
                oDrcProperty.LowerCompTh = (OMX_S32)g_cameraParam.LowerCompTh;
                oDrcProperty.UpperCompTh = (OMX_S32)g_cameraParam.UpperCompTh;
                oDrcProperty.NoiseGateTh = (OMX_S32)g_cameraParam.NoiseGateTh;
                OMX_SetConfig(hAudRender, eIndex, &oDrcProperty);
            }
        }

        // Tunnel the file parser to the audio decoder
        eError = OMX_SetupTunnel(hFileReader, 1, hAudDec, 0);
        TEST_ASSERT_NOTERROR(eError);

        if (pGraph->custcomp)
        {
            // Tunnel the audio decoder to the custom component
            eError = OMX_SetupTunnel(hAudDec, 1, hCustComp, 0);
            TEST_ASSERT_NOTERROR(eError);
            // Tunnel the custom component to the audio renderer
            eError = OMX_SetupTunnel(hCustComp, 1, hAudRender, 0);
            TEST_ASSERT_NOTERROR(eError);
            AddComponentToGraph(&pGraph->oCompGraph, hCustComp);
        }
        else
        {
            // Tunnel the audio decoder to the audio renderer
            eError = OMX_SetupTunnel(hAudDec, 1, hAudRender, 0);
            TEST_ASSERT_NOTERROR(eError);
        }

        AddComponentToGraph(&pGraph->oCompGraph, hAudDec);
        AddComponentToGraph(&pGraph->oCompGraph, hAudRender);
    }

    // Set which component will wait for End Of Stream
    if (video)
    {
        pGraph->oCompGraph.hEndComponent = hVidRender;
        pGraph->hRenderer = hVidRender;
        pGraph->hDecoder = hVidDec;

        if (!g_VidRendProf.bNoAVSync && audio)
        {
            pGraph->oCompGraph.hEndComponent2 = hAudRender;
            pGraph->oCompGraph.needEOS = 2;
        }
    }
    else
    {
        pGraph->bAudioOnly = OMX_TRUE;
        pGraph->oCompGraph.hEndComponent = hAudRender;
    }

    pGraph->hFileParser = hFileReader;

    return eError;
}

// Create a raw audio player graph
static OMX_ERRORTYPE InitAudioPlayer(char *infile, int stubrender,
                                     char *decoder, char* reader, NvxGraph *pGraph)
{
    OMX_ERRORTYPE eError;
    OMX_HANDLETYPE hFileReader, hDecoder, hRender;

    eError = InitReaderWriter(reader, infile, &hFileReader, pGraph, &g_oAppData, &g_oCallbacks);
    g_FileReader = hFileReader;
    if (eError != OMX_ErrorNone)
    {
        OMX_FreeHandle(hFileReader);
        return eError;
    }

    ProbeMetadata(pGraph, hFileReader);
    if (pGraph->bSaveCoverArt)
        SaveCoverArt(pGraph, hFileReader);

    if (pGraph->bEnableFileCache)
        SetFileCacheSize(hFileReader, pGraph->nFileCacheSize);

    // Allocate a clock
    eError = InitClock(g_Clock, pGraph, &g_oAppData, &g_oCallbacks);
    TEST_ASSERT_NOTERROR(eError);

    // Allocate the audio decoder
    eError = OMX_GetHandle(&hDecoder, decoder, &g_oAppData, &g_oCallbacks);
    TEST_ASSERT_NOTERROR(eError);

    SetProfileMode(hDecoder, &g_AudDecProf);
    SetAudioOnlyHint(hDecoder, OMX_TRUE);

    if (stubrender)
    {
        // Use a file writer if playback is disabled
        eError = InitReaderWriter(g_AudioWriter, "output.pcm", &hRender, pGraph, &g_oAppData, &g_oCallbacks);
        g_FileReader = hRender;
        TEST_ASSERT_NOTERROR(eError);
    }
    else
    {
        // Allocate a audio playback component, and hook it up to the clock
        eError = OMX_GetHandle(&hRender, g_AudioRender, &g_oAppData,
                               &g_oCallbacks);
        TEST_ASSERT_NOTERROR(eError);

        SetDisableTimestampUpdates(hRender, OMX_TRUE);

        OMX_SendCommand(pGraph->hClock, OMX_CommandPortEnable, 0, 0);

        SetVolume(hRender, pGraph);
        SetAudioOutput(hRender, pGraph);
        if (pGraph->bSetDB)
            SetEQ(hRender, pGraph);

        eError = OMX_SetupTunnel(pGraph->hClock, 0, hRender, 1);
        TEST_ASSERT_NOTERROR(eError);
    }

    pGraph->oCompGraph.hEndComponent = hRender;

    // Build remainder of component graph
    eError = OMX_SetupTunnel(hFileReader, 0, hDecoder, 0);
    TEST_ASSERT_NOTERROR(eError);

    eError = OMX_SetupTunnel(hDecoder, 1, hRender, 0);
    TEST_ASSERT_NOTERROR(eError);

    // Add all components to the SampleBase list
    AddComponentToGraph(&pGraph->oCompGraph, hFileReader);
    AddComponentToGraph(&pGraph->oCompGraph, hDecoder);
    AddComponentToGraph(&pGraph->oCompGraph, hRender);
    AddComponentToGraph(&pGraph->oCompGraph, pGraph->hClock);

    pGraph->hFileParser = hFileReader;
    pGraph->bAudioOnly = OMX_TRUE;

    return eError;
}

static NvError InitGraph(NvxGraph *pCurGraph)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    // Check for 'infile's existance
    if (pCurGraph->infile && !pCurGraph->streaming &&
        !((AV_RECORDER == pCurGraph->testtype) ||
          g_cameraParam.useVidHdrRW == 1))
    {
        NvOsStatType stat = { 0, NvOsFileType_File };
        NvError err;

        err = NvOsStat(pCurGraph->infile, &stat);

        if (err != NvSuccess || stat.size == 0)
        {
            NvOsDebugPrintf("File '%s' not found\n", pCurGraph->infile);
            return NvError_TestApplicationFailed;
        }
    }

    switch (pCurGraph->testtype)
    {
        case RAWPLAYER:
        case JPEG_DECODER:
            if (!pCurGraph->video && !pCurGraph->audio && !pCurGraph->parser)
            {
                NvOsDebugPrintf("No audio or video decoder set\n");
                return NvError_TestApplicationFailed;
            }
            break;
        default:
            break;
    }

    // Create the desired graph based on the command line params
    switch (pCurGraph->testtype)
    {
        case PLAYER:
            pCurGraph->parser = g_Reader;
            eError = InitVideoPlayer(pCurGraph);
            break;
        case RAWPLAYER:
            eError = InitAudioPlayer(pCurGraph->infile, pCurGraph->stubrender,
                                     pCurGraph->audio, g_AudioReader,
                                     pCurGraph);
            break;
        case AV_RECORDER:
            eError = InitAVRecorderGraph(g_Camera, pCurGraph->renderer,
                                         g_AudioCapturer, g_cameraParam.eAudioCodingType,
                                         pCurGraph);
            break;
        case JPEG_DECODER:
            pCurGraph->parser = g_ImageReader;
            eError = InitVideoPlayer(pCurGraph);
            break;
        default:
            NvOsDebugPrintf("Unknown test mode\n");
            return NvError_TestApplicationFailed;
    }

    if (OMX_ErrorNone == eError)
        return NvSuccess;
    return NvError_TestApplicationFailed;
}

static void FindFile(char *filename, NvxGraph *pGraph)
{
    char *fullpath;
    int size;
    NvOsStatType stat = { 0, NvOsFileType_File };
    NvError err;
#define NUM_PATHS 5
    char *paths[NUM_PATHS] = { "/storage card/", "/storage card/openmax/",
                               "/storage card/isdbt/", "/NandFlash/", "/" };
    int i;

    size = NvOsStrlen(filename) + 25;
    fullpath = NvOsAlloc(size);

    for (i = 0; i < NUM_PATHS; i++)
    {
        NvOsMemset(fullpath, 0, size);
        NvOsSnprintf(fullpath, size, "%s%s", paths[i], filename);

        err = NvOsStat(fullpath, &stat);
        if (err == NvSuccess && stat.size > 0)
        {
            pGraph->infile = fullpath;
            pGraph->delinfile = OMX_TRUE;
            return;
        }
    }

    pGraph->infile = filename;
    pGraph->delinfile = OMX_FALSE;

    NvOsFree(fullpath);
}

static void PickParser(NvxGraph *pCurGraph)
{
    char *str = strrchr(pCurGraph->infile, '.');
    char ext[8] = { 0 };
    char start[8] = { 0 };
    int len;

    if (str)
    {
        str++;
        if (*str == 0 || strlen(str) == 0 || strlen(str) > 7)
        {
        }
        else
        {
            NvOsMemcpy(ext, str, strlen(str) + 1);
            str = ext;
            while (*str)
            {
                *str = tolower(*str);
                str++;
            }
        }
    }

    len = strlen(pCurGraph->infile) + 1;
    if (len > 7)
        len = 7;

    NvOsMemcpy(start, pCurGraph->infile, len);
    start[7] = '\0';

    str = start;
    while (*str)
    {
        *str = tolower(*str);
        str++;
    }

    if (strchr(start, ':') && strchr(start, '/'))
        pCurGraph->streaming = OMX_TRUE;

    if (!strcmp(start, "rtsp://") || !strcmp(start, "http://"))
    {
        pCurGraph->testtype = PLAYER;
        pCurGraph->streaming = OMX_TRUE;
    }
    else if (!strcmp(ext, "adts"))
    {
        pCurGraph->testtype = RAWPLAYER;
        pCurGraph->audio = g_AdtsDecoder;
    }
    else if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg"))
    {
        pCurGraph->testtype = JPEG_DECODER;
        g_VidRendProf.bFlip = OMX_FALSE;
        pCurGraph->video = g_JpegDecoder;
    }
    else if (!strcmp(ext, "mp4") || !strcmp(ext, "3gp") || !strcmp(ext, "m4a") || !strcmp(ext, "m4b") ||
        !strcmp(ext, "3gpp") || !strcmp(ext, "m4v") || !strcmp(ext, "mov")|| !strcmp(ext, "3g2") ||
        !strcmp(ext, "avi") || !strcmp(ext, "divx") || !strcmp(ext, "asf") ||
        !strcmp(ext, "wmv") || !strcmp(ext, "wma") || !strcmp(ext, "mp3") || !strcmp(ext, "mpa") ||
        !strcmp(ext, "ogg") || !strcmp(ext, "oga")|| !strcmp(ext, "aac") || !strcmp(ext, "amr") ||
        !strcmp(ext, "awb") || !strcmp(ext, "m2v") || !strcmp(ext, "qt")|| !strcmp(ext, "wav") ||
        !strcmp(ext, "mp2") || !strcmp(ext, "mp1") || !strcmp(ext, "flv") || !strcmp(ext, "mkv") ||
        !strcmp(ext, "pyv") || !strcmp(ext, "mpg"))
    {
        pCurGraph->testtype = PLAYER;
    }
    else
        goto bail;

    return;

bail:
    NvOsDebugPrintf("Unknown extension for file: %s\n", pCurGraph->infile);
}

static void CreateYTURL(NvxGraph *pCurGraph, char *id, NvU32 fmt_type)
{
    char url[1024];
    char *buf = NULL;
    NvS64 len = 0;
    int templen = 0;
    NvError e;
    char *finalurl = NULL;
    char vidid[50];
    char sessionid[256];
    char *p, *q;

    if (NvOsStrlen(id) > 50)
        return;

    finalurl = NvOsAlloc(1024);
    if (!finalurl)
        return;

    NvOsSnprintf(url, 128, "http://www.youtube.com/watch?v=%s", id);

    e = NvMMSockGetHTTPFile(url, &buf, &len);

    if (NvSuccess != e || len <= 0)
        goto fail;

    NvOsMemset(vidid, 0, 50);
    NvOsMemset(sessionid, 0, 256);

    // Search for "video_id":
    q = NvUStrstr(buf, "\"video_id\":");
    if (!q)
    {
        q = NvUStrstr(buf, "&video_id=");
        if (!q)
        {
            goto fail;
        }

        templen = NvOsStrlen("&video_id=");

        q += templen;

        p = q;
        q = vidid;
        while (*p && *p != '&' && (q - vidid) < 50)
        {
            *q = *p;
            q++; p++;
        }

        // Search for "t":
        q = NvUStrstr(buf, "&t=");
        if (!q)
        {
            goto fail;
        }

        templen = NvOsStrlen("&t=");

        q += templen;
        p = q;
        q = sessionid;
        while (*p && *p != '&' && (q - sessionid) < 256)
        {
            *q = *p;
            q++; p++;
        }

    }
    else
    {
        templen = NvOsStrlen("\"video_id\":");

        q += templen;

        p = NvUStrstr(q, "\"");
        if (!p)
            goto fail;

        // Skip the '"'
        p++;
        q = vidid;
        while (*p && *p != '\"' && (q - vidid) < 50)
        {
            *q = *p;
            q++; p++;
        }

        // Search for "t":
        q = NvUStrstr(buf, "\"t\":");
        if (!q)
            goto fail;

        templen = NvOsStrlen("\"t\":");

        q += templen;
        p = NvUStrstr(q, "\"");
        if (!p)
            goto fail;

        q = sessionid;
        // skip the '"'
        p++;

        while (*p && *p != '\"' && (q - sessionid) < 256)
        {
            *q = *p;
            q++; p++;
        }

    }

    NvOsSnprintf(finalurl, 1024,
                 "http://www.youtube.com/get_video?video_id=%s&t=%s&fmt=%d",
                 vidid, sessionid, fmt_type);

    NvOsDebugPrintf("getting url: %s\n", finalurl);

    pCurGraph->infile = finalurl;
    pCurGraph->delinfile = OMX_TRUE;
    NvOsFree(buf);
    return;

fail:
    NvOsDebugPrintf("Failed to find URL\n");
    NvOsFree(buf);
    NvOsFree(finalurl);
    pCurGraph->delinfile = OMX_FALSE;
}


static void omx_sanity(NvTestApplicationHandle h, int sanitystatus)
{
    NVTEST_VERIFY(h, sanitystatus == 0);
}

OMX_API OMX_ERRORTYPE SampleComponentInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_API NV_CUSTOM_PROTOCOL *GetSampleProtocol(void);

NvError NvOmxPlayerMain(int argc, char **argv)
{
    NvxProfile *pProf = NULL;
    char *line = NULL;
    char **largv = NULL;
    NvxGraph *pCurGraph;
    int i;
    int graphNum = 0;
    int playTimeInSec = 10;
    float playSpeed1 = 0;
    float playSpeed2 = 0;
    float playSpeed3 = 0;
    NvBool fastPlaySupported = 0;
    int seekTimeOffset = 0;
    unsigned char atleastOneGraphActive=1;
    int videoloop = 0;
    int numPlaysDesired = 1;
    int sanitystatus = 0;
    OMX_U32 maxdrops = 6;
    OMX_U32 fpsrange = 2;
    NvTestApplication nvtestapp;
    NvBool HDMIdisplay = 0;
    NvBool TVdisplay = 0;
    OMX_COMPONENTREGISTERTYPE regcustcomp;
    int jpegDecDisplayTimeInSec = 15;
    OMX_BOOL bIsULP = OMX_TRUE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvBool SanityFileAssigned = NV_FALSE;
    NvBool BufferingPause = NV_FALSE;
    NVTEST_INIT(&nvtestapp);

    // Initialize the base sample code, init OMX, etc
    if (NvSuccess != NvOMXSampleInit(&g_oAppData, &g_oCallbacks))
        goto omx_test_end;

    g_oAppData.pEOTCallback = EndOfTrackCallback;
    g_oAppData.pDLACallback = DrmDirectLicenseAcquisitionCallback;
    line = NvOsAlloc(2048);
    largv = NvOsAlloc(sizeof(char *) * 1024);

    NVOMX_RegisterNVCustomProtocol("test://", GetSampleProtocol());

    regcustcomp.pName = g_SampleComponent;
    regcustcomp.pInitialize = &SampleComponentInit;
    NVOMX_RegisterComponent(&regcustcomp);

begin_video_loop:

    atleastOneGraphActive = 1;
    graphNum = 0;
    seekTimeOffset = 0;
    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        NvOsMemset(&g_Graphs[i], 0, sizeof(NvxGraph));
        g_Graphs[i].ChannelID = 18;
        g_Graphs[i].testtype = NONE;
        g_Graphs[i].testcmd = PLAY;
        g_Graphs[i].infile = NULL;
        g_Graphs[i].outfile = "output.dat";
        g_Graphs[i].recordfile = NULL;
        g_Graphs[i].volume = -1;
        g_Graphs[i].eAudOutputType = NVX_AUDIO_OutputMusic;
        g_Graphs[i].renderer = g_OvRenderYUV;
        g_Graphs[i].xScaleFactor = NV_SFX_WHOLE_TO_FX(1);
        g_Graphs[i].playSpeed = 1.0;
        g_Graphs[i].ffRewTimeInSec = 2;
        g_Graphs[i].RecordMode = NvxRecordingMode_Disable;

        g_Graphs[i].bLowMem = OMX_TRUE;
        g_Graphs[i].bKeepAspect = OMX_TRUE;
        if (NvSuccess != NvOsSemaphoreCreate(&(g_Graphs[i].oCompGraph.hTimeEvent), 0))
            goto omx_test_end;

        if (NvSuccess != NvOsSemaphoreCreate(&(g_Graphs[i].oCompGraph.hCameraStillCaptureReady), 0))
            goto omx_test_end;

        if (NvSuccess != NvOsSemaphoreCreate(&(g_Graphs[i].oCompGraph.hCameraHalfPress3A), 0))
            goto omx_test_end;

        g_Graphs[i].oCompGraph.eos = 0;
        g_Graphs[i].oCompGraph.needEOS = 1;
        g_Graphs[i].oCompGraph.EosEvent = OMX_FALSE;
        g_Graphs[i].oCompGraph.eErrorEvent = OMX_ErrorNone;
        g_Graphs[i].oCompGraph.bWaitOnError = OMX_FALSE;
        g_Graphs[i].oCompGraph.bNoTimeOut = OMX_FALSE;

        g_Graphs[i].bAlphaBlend = OMX_FALSE;
        g_Graphs[i].nAlphaValue = 0xBF;

        g_Graphs[i].bCaptureFrame = OMX_FALSE;
        g_Graphs[i].capturefile = "capture.yuv";

        g_Graphs[i].nFileCacheSize = 1;
        g_Graphs[i].bEnableFileCache = OMX_TRUE;
        g_Graphs[i].bPlayTime = OMX_FALSE;
        g_oAppData.pCompGraph[i] = &(g_Graphs[i].oCompGraph);
    }

    pCurGraph = &g_Graphs[graphNum];

    NvOsMemset(&g_AudDecProf, 0, sizeof(NvxProfile));
    NvOsMemset(&g_VidDecProf, 0, sizeof(NvxProfile));
    NvOsMemset(&g_DemuxProf, 0, sizeof(NvxProfile));
    NvOsMemset(&g_VidRendProf, 0, sizeof(NvxProfile));
    NvOsMemset(&g_CameraProf, 0, sizeof(NvxProfile));
    NvOsMemset(&g_AudioRecProf, 0, sizeof(NvxProfile));
    NvOsMemset(&g_AudioEncProf, 0, sizeof(NvxProfile));

    g_VidRendProf.bFlip = OMX_TRUE;

    if (OMX_ErrorNone != SampleInitPlatform())
    {
        sanitystatus++;
        goto omx_test_end;
    }

    // Possibly read a file that contains command line arguments
    {
        NvOsFileHandle fIn;
        if (NvSuccess == (NvOsFopen("args.txt", NVOS_OPEN_READ, &fIn)))
        {
            size_t size = 0;
            NvOsMemset(line, 0, 2048);

            if (NvSuccess == (NvOsFread(fIn, line, 2048, &size)))
            {
                char *res = NULL;
                int largc = 1;

                line[size] = '\0';

                largv[0] = NULL;
                largv[largc] = res = strtok(line, " ");
                if (res)
                    largc++;

                while (res)
                {
                    largv[largc] = res = strtok(NULL, " ");
                    if (res)
                        largc++;
                }

                if (largc > 1)
                {
                    argc = largc;
                    argv = largv;
                }
            }
            NvOsFclose(fIn);
        }

    }

    // Parse command line arguments
    if (argc > 1)
    {
        int arg;
        for (arg = 1; arg < argc; arg++)
        {
            if (!strcmp(argv[arg], "--stubrenderers"))
            {
                pProf = NULL;
                pCurGraph->stubrender = 1;
            }
            else if (!strcmp(argv[arg], "--next"))
            {
                graphNum++;
                if (graphNum >= NVX_MAX_OMXPLAYER_GRAPHS)
                {
                    NvOsDebugPrintf("omxplayer: Too many actions\n");
                    goto omx_test_end;
                }

                pCurGraph = &g_Graphs[graphNum];
            }
            else if (!strcmp(argv[arg], "--rotate") && argc - 1 > arg)
            {
                pCurGraph->nRotate = atoi(argv[arg+1]);
                arg++;

                if (argc - 1 > arg && strncmp(argv[arg+1], "--", 2))
                {
                    arg++;
                    if (!strcmp(argv[arg], "capture"))
                        g_cameraParam.nRotationCapture = pCurGraph->nRotate;
                    else
                        g_cameraParam.nRotationPreview = pCurGraph->nRotate;
                }
                else
                {
                    g_cameraParam.nRotationPreview = pCurGraph->nRotate;
                }
            }
            else if (!strcmp(argv[arg], "--video_rotate") && argc - 1 > arg)
            {
                g_cameraParam.nVideoRotation = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--video_mirror") && argc - 1 > arg)
            {
                g_cameraParam.eVideoMirror = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--x") && argc - 1 > arg)
            {
                pCurGraph->nX = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--y") && argc - 1 > arg)
            {
                pCurGraph->nY = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--width") && argc - 1 > arg)
            {
                pCurGraph->nWidth = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--height") && argc - 1 > arg)
            {
                pCurGraph->nHeight = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--crop") && argc - 4 > arg)
            {
                pCurGraph->nCropX = atoi(argv[arg+1]);
                arg++;
                pCurGraph->nCropY = atoi(argv[arg+1]);
                arg++;
                pCurGraph->nCropWidth = atoi(argv[arg+1]);
                arg++;
                pCurGraph->nCropHeight = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--ovyuv"))
            {
                pCurGraph->renderer = g_OvRenderYUV;
                pCurGraph->bNoWindow = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--dtvlistprogs"))
            {
                pCurGraph->bMobileTVListProgs = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--dtvsetprog") && argc - 1 > arg)
            {
                pCurGraph->nMobileTVProgNum = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--isdbtcmdfile"))
            {
                pCurGraph->bMobileTVUseCmdFromFile = OMX_TRUE;
                pCurGraph->pMobileTVCmdFile = argv[arg+1];
                arg++;
            }
            else if (!strcmp(argv[arg], "--ovrgb"))
            {
                pCurGraph->renderer = g_OvRenderRgb;
            }
            else if (!strcmp(argv[arg], "--jpegdec"))
            {
                pCurGraph->testtype = JPEG_DECODER;
                pProf = &g_VidDecProf;
                g_VidRendProf.bFlip = OMX_FALSE;
                pCurGraph->video = g_JpegDecoder;
                if (argc - 1 > arg && strncmp(argv[arg+1], "--", 2))
                {
                    arg++;
                    jpegDecDisplayTimeInSec = atoi(argv[arg]);
                }
            }
            else if (!strcmp(argv[arg], "--adts"))
            {
                if (pCurGraph->testtype == NONE)
                    pCurGraph->testtype = RAWPLAYER;
                pProf = &g_AudDecProf;
                pCurGraph->audio = g_AdtsDecoder;
            }
            else if (!strcmp(argv[arg], "--noaudio"))
            {
                pCurGraph->noaud = OMX_TRUE;
                pCurGraph->audio = NULL;
            }
            else if (!strcmp(argv[arg], "--novideo"))
            {
                pCurGraph->novid = OMX_TRUE;
                pCurGraph->video = NULL;
            }
            else if (!strcmp(argv[arg], "--noaspect"))
            {
                pCurGraph->bKeepAspect = OMX_FALSE;
            }
            else if (!strcmp(argv[arg], "--deinterlace"))
            {
                arg++;
                pCurGraph->DeinterlaceMethod = atoi(argv[arg]);
                pCurGraph->bIsM2vPlayback = NV_TRUE;
            }
            else if (!strcmp(argv[arg], "--camera"))
            {
                pCurGraph->testtype = AV_RECORDER;
                pProf = &g_CameraProf;
            }
            else if (!strcmp(argv[arg], "--audiorec") && argc - 1 > arg)
            {
                g_cameraParam.enableAudio = OMX_TRUE;
                pCurGraph->testtype = AV_RECORDER;
                pProf = &g_AudioRecProf;
                arg++;
                g_cameraParam.audioTime = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--audenctype") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "amr"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingAMR;
                    g_cameraParam.nAudioSampleRate = 8000;
                    g_cameraParam.nChannels = 1;
                }
                else if (!strcmp(argv[arg], "amrwb"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingAMR;
                    g_cameraParam.nAudioSampleRate = 16000;
                    g_cameraParam.nChannels = 1;
                }
                else if (!strcmp(argv[arg], "aacp"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingAAC;
                    g_cameraParam.eAacProfileType = OMX_AUDIO_AACObjectHE;
                }
                else if (!strcmp(argv[arg], "eaacp"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingAAC;
                    g_cameraParam.eAacProfileType = OMX_AUDIO_AACObjectHE_PS;
                }
                else if (!strcmp(argv[arg], "pcm"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingPCM;
                    g_cameraParam.pcmMode = OMX_AUDIO_PCMModeLinear;
                }
                else if (!strcmp(argv[arg], "alaw"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingPCM;
                    g_cameraParam.pcmMode = OMX_AUDIO_PCMModeALaw;
                }
                else if (!strcmp(argv[arg], "mulaw"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingPCM;
                    g_cameraParam.pcmMode = OMX_AUDIO_PCMModeMULaw;
                }
                else // if (!strcmp(argv[arg], "aac"))
                {
                    g_cameraParam.eAudioCodingType = OMX_AUDIO_CodingAAC;
                    g_cameraParam.eAacProfileType = OMX_AUDIO_AACObjectLC;
                }
            }
            else if (!strcmp(argv[arg], "--audiosamplerate") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nAudioSampleRate = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--audiobitrate") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nAudioBitRate = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--audiochannel") && argc - 1 > arg)
            {
                arg++;
                if(g_cameraParam.eAudioCodingType != OMX_AUDIO_CodingAMR)
                {
                    g_cameraParam.nChannels = atoi(argv[arg]);
                }
            }
            else if (!strcmp(argv[arg], "--drc") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "on"))
                {
                    g_cameraParam.bDrcEnabled = OMX_TRUE;
                    if (argc - 4 > arg)
                    {
                        g_cameraParam.NoiseGateTh = (OMX_S32)atoi(argv[arg+1]);
                        g_cameraParam.LowerCompTh = (OMX_S32)atoi(argv[arg+2]);
                        g_cameraParam.UpperCompTh = (OMX_S32)atoi(argv[arg+3]);
                        g_cameraParam.ClippingTh  = (OMX_S32)atoi(argv[arg+4]);
                        arg += 4;
                    }
                    else
                    {
                        NvOsDebugPrintf("too few args for --drc, need 4 in order of NoiseGateTh, LowerCompTh, UpperCompTh, ClippingTh\n");
                        sanitystatus++;
                        goto omx_test_end_cleanup;
                    }
                }
                else
                {
                    g_cameraParam.bDrcEnabled = OMX_FALSE;
                }
            }
            else if (!strcmp(argv[arg], "--ulptest"))
            {
                pCurGraph->testcmd = ULPTEST;
                pCurGraph->bEnableFileCache = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--playtest"))
            {
                pCurGraph->testcmd = PLAY;
            }
            else if (!strcmp(argv[arg], "--pausetest"))
            {
                pCurGraph->testcmd = PAUSE;
            }
            else if (!strcmp(argv[arg], "--pausestoptest"))
            {
                pCurGraph->testcmd = PAUSE_STOP;
            }
            else if (!strcmp(argv[arg], "--loadedidleloaded"))
            {
                pCurGraph->testcmd = LOADED_IDLE_LOADED;
            }
            else if (!strcmp(argv[arg], "--idlestateflushports"))
            {
                pCurGraph->testcmd = FLUSH_PORTS_IN_IDLE;
            }
            else if (!strcmp(argv[arg], "--seektest"))
            {
                pCurGraph->testcmd = SEEK;
            }
            else if(!strcmp(argv[arg], "--complexseektest"))
            {
                pCurGraph->testcmd = COMPLEX_SEEK;
            }
            else if(!strcmp(argv[arg], "--crazyseek") && argc - 3 > arg)
            {
                pCurGraph->testcmd = CRAZY_SEEK_TEST;
                pCurGraph->nCrazySeekCount = atoi(argv[arg+1]);
                pCurGraph->nCrazySeekDelay = atoi(argv[arg+2]);
                pCurGraph->nCrazySeekSeed  = atoi(argv[arg+3]);
                arg += 3;
            }
            else if (!strcmp(argv[arg], "--stoptest"))
            {
                pCurGraph->testcmd = STOP;
            }
            else if (!strcmp(argv[arg], "--playtimeupdate") && argc - 1 > arg)
            {
                arg++;
                playTimeInSec = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--volumetest"))
            {
                pCurGraph->testcmd = VOLUME;
            }
            else if (!strcmp(argv[arg], "--trickmodetest") && argc - 2 > arg)
            {
                pCurGraph->testcmd = TRICKMODE;
                arg++;
                pCurGraph->playSpeed = (float)atof(argv[arg]);
                /* Take only positive speeds, use -ve of that for rewind.*/
                if(pCurGraph->playSpeed < 0)
                    pCurGraph->playSpeed = -(pCurGraph->playSpeed);
                arg++;
                pCurGraph->ffRewTimeInSec = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--ratecontroltest") && argc - 1 > arg)
            {
                pCurGraph->testcmd = RATECONTROLTEST;
                arg++;
                pCurGraph->ffRewTimeInSec = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--trickmodesanity"))
            {
                pCurGraph->testcmd = TRICKMODESANITY;
                playSpeed1 = pCurGraph->playSpeed = 2.0;
                playSpeed2 = 3.0;
                playSpeed3 = 4.0;

                pCurGraph->ffRewTimeInSec = 2;

            }
            else if ((!strcmp(argv[arg], "--input")  ||
                     (!strcmp(argv[arg], "-i"))) && argc - 1 > arg)
            {
                pProf = NULL;
                if (pCurGraph->delinfile && pCurGraph->infile)
                    NvOsFree(pCurGraph->infile);
                pCurGraph->infile = argv[arg+1];
                pCurGraph->delinfile = OMX_FALSE;
                arg++;
            }
            else if (!strcmp(argv[arg],"--stereorender") && argc - 1 > arg)
            {
                pCurGraph->StereoRenderMode = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--profile") && pProf)
            {
                pProf->bProfile = OMX_TRUE;
                strcpy(pProf->ProfileFileName, "profile.txt");
                if (((arg+1) < argc) && (*argv[arg + 1] != '-'))
                {
                    if (strlen(argv[arg + 1]) < PROFILE_FILE_NAME_LENGTH)
                        strcpy(pProf->ProfileFileName, argv[arg + 1]);
                    arg++;
                }
            }
            else if (!strcmp(argv[arg], "--profiledemux"))
            {
                pProf = &g_DemuxProf;
                pProf->bProfile = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--profilevidrend"))
            {
                pProf = &g_VidRendProf;
                pProf->bProfile = OMX_TRUE;
                strcpy(pProf->ProfileFileName, "VidRender.txt");
                if (((arg+1) < argc) && (*argv[arg + 1] != '-'))
                {
                    if (strlen(argv[arg + 1]) < PROFILE_FILE_NAME_LENGTH)
                        strcpy(pProf->ProfileFileName, argv[arg + 1]);
                    arg++;
                }
            }
            else if (!strcmp(argv[arg], "--showjitter"))
            {
                pProf = &g_VidRendProf;
                pProf->nProfileOpts = 1;
            }
            else if (!strcmp(argv[arg], "--noavsync"))
            {
                g_DemuxProf.bNoAVSync = OMX_TRUE;
                g_VidRendProf.bNoAVSync = OMX_TRUE;
            }
            else if ((!strcmp(argv[arg], "--disablerender")) ||
                     (!strcmp(argv[arg], "--dr")))
            {
                pProf = &g_VidRendProf;
                pProf->bDisableRendering = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--nvmm") && argc - 1 > arg)
            {
                pProf = &g_VidDecProf;
                pProf->nProfileOpts = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--verbose") && pProf)
            {
                pProf->bVerbose = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--stuboutput") && pProf)
            {
                pProf->bStubOutput = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--hdmi"))
            {
                pCurGraph->eAudOutputType = NVX_AUDIO_OutputHdmi;
                pCurGraph->renderer = g_HdmiRender;
                HDMIdisplay = 1;
                HDMI_width = 1280;
                HDMI_height = 720;
            }
            else if (!strcmp(argv[arg], "--hdmi480p"))
            {
                pCurGraph->eAudOutputType = NVX_AUDIO_OutputHdmi;
                pCurGraph->renderer = g_HdmiRender;
                HDMIdisplay = 1;
                HDMI_width = 720;
                HDMI_height = 480;
            }
            else if (!strcmp(argv[arg], "--nvmmrend"))
            {
                pCurGraph->renderer = g_NvMMRenderer;
            }
            else if (!strcmp(argv[arg], "--hdmi1080p"))
            {
                pCurGraph->eAudOutputType = NVX_AUDIO_OutputHdmi;
                pCurGraph->renderer = g_HdmiRender;
                HDMIdisplay = 1;
                HDMI_width = s_HDMI1080Width;
                HDMI_height = s_HDMI1080height;
            }
            else if (!strcmp(argv[arg], "--ulp"))
            {
                NvOsDebugPrintf("--ulp is deprecated (now always on), and will be removed in a future revision\n");
            }
            else if (!strcmp(argv[arg], "--ulp_kpi"))
            {
                pCurGraph->ulpkpiMode = 1;
            }
            else if (!strcmp(argv[arg], "--ulp_kpi_tune"))
            {
                pCurGraph->ulpkpiMode = 2;
            }
            else if (!strcmp(argv[arg], "--avsync") && argc - 1 > arg)
            {
                pProf = &g_VidRendProf;
                pProf->nAVSyncOffset = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--flip"))
            {
                pProf = &g_VidRendProf;
                pProf->bFlip = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--flipoff"))
            {
                pProf = &g_VidRendProf;
                pProf->bFlip = OMX_FALSE;
            }
            else if (!strcmp(argv[arg], "--dropfreq") && argc - 1 > arg)
            {
                pProf = &g_VidRendProf;
                pProf->nFrameDrop = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--volume") && argc - 1 > arg)
            {
                pCurGraph->volume = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--eq") && argc - 3 > arg)
            {
                int band = atoi(argv[arg+1]);
                int gain[2];
                gain[0] = atoi(argv[arg+2]);
                gain[1] = atoi(argv[arg+3]);

                if(gain[0] <-1200)
                {
                    gain[0] =-1200;
                    NvOsDebugPrintf("Left channel Gain below -1200 valid resetting to -1200. Valid range for gain [-1200,1200]\n");
                }
                else if(gain[0]>1200)
                {
                    gain[0] =1200;
                    NvOsDebugPrintf("Left channel Gain above 1200 valid resetting to 1200. Valid range for gain [-1200,1200]\n");
                }

                if(gain[1] <-1200)
                {
                    gain[1] =-1200;
                    NvOsDebugPrintf("Right channel Gain below -1200 valid resetting to -1200. Valid range for gain [-1200,1200]\n");
                }
                else if(gain[1]>1200)
                {
                    gain[1] =1200;
                    NvOsDebugPrintf("Right channel Gain above 1200 valid resetting to 1200. Valid range for gain [-1200,1200]\n");
                }

                if (band >= 0 && band < 5)
                {
                    pCurGraph->dBGain[0][band] = gain[0];
                    pCurGraph->dBGain[1][band] = gain[1];
                    pCurGraph->bSetDB = OMX_TRUE;
                }
                else
                    NvOsDebugPrintf("Malformed --eq option, valid: --eq [0->5] [-1200->1200] [-1200->1200]\n");

                arg++;
                arg++;
                arg++;
            }
            else if (!strcmp(argv[arg], "--pseq") && argc - 1 > arg)
            {
                int m;
                static int PresetEq[10][5] ={{-200, 300, 400, 200,0}, // Pop
                {-200, 300, 300, 300,200}, // Live
                { 300,   0,-300,   0,100}, // Rock
                {   0, 100, 300, 300,100}, // Club
                { 800, 500,-100,-100,-300}, // Bass
                {-600,-600,-100, 100, 700}, // Treble
                {-300, 600, 150,-300,-300}, // vocal
                { 600, 200,   0,-100,-600}, // Dance
                {   0,   0,   0,   0,-300}, // Classic
                {   0,   0,   0,   0,   0}, // Flat
                };
                char* strPreSetEq[10] =
                { "Pop",
                  "Live",
                  "Rock",
                  "Club",
                  "Bass",
                  "Treble",
                  "Vocal",
                  "Dance",
                  "Classic",
                  "Flat"
                };

                int preset = atoi(argv[arg+1]);
                if(preset<10 && preset>=0)
                {
                    for(m=0;m<5;m++)
                    {
                        pCurGraph->dBGain[0][m] = pCurGraph->dBGain[1][m] = PresetEq[preset][m];
                    }
                    pCurGraph->bSetDB = OMX_TRUE;

                    NvOsDebugPrintf("Using the %s Preset EQ\n",strPreSetEq[preset]);
                }
                else
                {
                    NvOsDebugPrintf("Invalid Preset EQ --pseq option, valid: --pseq [0,1,..,9]\n");
                }
                arg++;
            } else if (!strcmp(argv[arg], "--loop") && argc - 1 > arg) {
                numPlaysDesired = atoi(argv[arg+1]);
                arg++;
            } else if (!strcmp(argv[arg], "--preview") && argc - 2 > arg) {
                g_cameraParam.enablePreview = OMX_TRUE;
                g_cameraParam.nWidthPreview = atoi(argv[arg+1]);
                g_cameraParam.nHeightPreview = atoi(argv[arg+2]);
                arg += 2;
                g_VidRendProf.bNoAVSync = OMX_TRUE;
            } else if (!strcmp(argv[arg], "--previewtime") && argc - 1 > arg) {
                g_cameraParam.previewTime = atoi(argv[arg+1]);
                arg += 1;
            } else if (!strcmp(argv[arg], "--anr") && argc - 1 > arg) {
                g_cameraParam.anrMode =
                    strcmp(argv[arg+1], "on") ? anr_off : anr_on;
                arg += 1;
            } else if (!strcmp(argv[arg], "--afmode") && argc - 1 > arg) {
                g_cameraParam.afmMode =
                    !strcmp(argv[arg+1], "slow") ? afm_slow :
                    !strcmp(argv[arg+1], "fast") ? afm_fast :
                    afm_default;
                arg += 1;
            } else if (!strcmp(argv[arg], "--still") && argc - 2 > arg) {
                pProf = &g_JpegEncoderProf;
                g_cameraParam.nWidthCapture = atoi(argv[arg+1]);
                g_cameraParam.nHeightCapture = atoi(argv[arg+2]);
                g_cameraParam.enableCameraCapture = OMX_TRUE;
                g_cameraParam.videoCapture = OMX_FALSE;
                if (eDeleteJPEGFilesUnset == g_cameraParam.eDeleteJPEGFiles)
                    g_cameraParam.eDeleteJPEGFiles = eDeleteJPEGFilesTrue;
                arg += 2;
            }
            else if (!strcmp(argv[arg], "--image_quality") && argc - 1 > arg)
            {
                g_cameraParam.nImageQuality = (OMX_U32)atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--thumbnail_quality") && argc - 1 > arg)
            {
                g_cameraParam.nThumbnailQuality = (OMX_U32)atoi(argv[arg+1]);
                g_cameraParam.bFlagThumbnailQF = OMX_TRUE;
                arg++;
            }
            else if (!strcmp(argv[arg], "--image_qtables") && argc - 1 > arg)
            {
                g_cameraParam.nImageQTablesSet_Luma = (OMX_U32)atoi(argv[arg+1]);
                g_cameraParam.nImageQTablesSet_Chroma = (OMX_U32)atoi(argv[arg+2]);
                arg += 2;
            }
            else if (!strcmp(argv[arg], "--deletejpegfiles") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "yes"))
                {
                    g_cameraParam.eDeleteJPEGFiles = eDeleteJPEGFilesTrue;
                }
                else
                {
                    g_cameraParam.eDeleteJPEGFiles = eDeleteJPEGFilesFalse;
                }
            }
            else if (!strcmp(argv[arg], "--stillcount") && argc - 1 > arg)
            {
                pProf = &g_JpegEncoderProf;
                arg ++;
                g_cameraParam.stillCountBurst = atoi(argv[arg]);
            }
            else if(!strcmp(argv[arg],"--exififd"))
            {
                NvOsStrncpy((char *)g_cameraParam.ExifFile, argv[arg+1], NvOsStrlen(argv[arg+1])+1);
                g_cameraParam.bExifEnable = NV_TRUE;
                arg++;
            }
            else if(!strcmp(argv[arg],"--gpsifd"))
            {
                NvOsStrncpy((char *)g_cameraParam.GPSFile, argv[arg+1], NvOsStrlen(argv[arg+1])+1);
                g_cameraParam.bGPSEnable = NV_TRUE;
                arg++;
            }
            else if(!strcmp(argv[arg],"--interopifd"))
            {
                NvOsStrncpy((char *)g_cameraParam.InterOpInfoIndex, argv[arg+1],
                    MAX_INTEROP_STRING_IN_BYTES);
                g_cameraParam.bInterOpInfo = NV_TRUE;
                arg++;
            }
            else if (!strcmp(argv[arg], "--video") && argc - 3 > arg)
            {
                pProf = &g_H264EncoderProf;
                g_cameraParam.captureTime = atoi(argv[arg+1]);
                g_cameraParam.nWidthCapture = atoi(argv[arg+2]);
                g_cameraParam.nHeightCapture = atoi(argv[arg+3]);
                g_cameraParam.enableCameraCapture = OMX_TRUE;
                g_cameraParam.videoCapture = OMX_TRUE;
                g_cameraParam.bAutoFrameRate = OMX_TRUE;
                g_cameraParam.nAutoFrameRateLow = NV_SFX_WHOLE_TO_FX(30);
                g_cameraParam.nAutoFrameRateHigh = NV_SFX_WHOLE_TO_FX(31);

                arg += 3;
            }
            else if (!strcmp(argv[arg], "--output") && argc - 1 > arg)
            {
                pProf = NULL;
                pCurGraph->outfile = argv[arg+1];
                arg++;
            }
            else if (!strcmp(argv[arg], "--splitmode") && argc - 1 > arg)
            {
                pCurGraph->SplitMode = 1;
                pCurGraph->splitfile = argv[arg+1];
                arg++;
            }
            else if (!strcmp(argv[arg], "--3gpmaxframes") && argc - 2 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "audio"))
                {
                    g_cameraParam.n3gpMaxFramesAudio = atoi(argv[arg+1]);
                }
                else if (!strcmp(argv[arg], "video"))
                {
                    g_cameraParam.n3gpMaxFramesVideo = atoi(argv[arg+1]);
                }
                arg++;
            }
            else if (!strcmp(argv[arg], "--record") && argc - 1 > arg)
            {
                pProf = NULL;
                pCurGraph->recordfile = argv[arg+1];
                pCurGraph->RecordMode = NvxRecordingMode_Enable;
                arg++;
            }
            else if (!strcmp(argv[arg], "--recordonly") && argc - 1 > arg)
            {
                pProf = NULL;
                pCurGraph->recordfile = argv[arg+1];
                pCurGraph->RecordMode = NvxRecordingMode_EnableExclusive;
                arg++;
            }
            else if (!strcmp(argv[arg], "--channel") && argc - 1 > arg)
            {
                pProf = NULL;
                pCurGraph->ChannelID = (OMX_U32)atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--sensor") && argc - 1 > arg)
            {
                OMX_U32 SensorId = (OMX_U32)atoi(argv[arg+1]);
                g_cameraParam.SensorId = SensorId;
                arg++;
            }
            else if (!strcmp(argv[arg], "--bitrate") && argc - 1 > arg)
            {
                g_cameraParam.nBitRate = (OMX_U32)atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--videncprop") && argc - 3 > arg)
            {
                g_cameraParam.nAppType = (OMX_U32)atoi(argv[arg+1]);
                g_cameraParam.nQualityLevel = (OMX_U32)atoi(argv[arg+2]);
                g_cameraParam.nErrorResilLevel = (OMX_U32)atoi(argv[arg+3]);
                arg += 3;
            }
            else if (!strcmp(argv[arg], "--svcencode"))
            {
                g_cameraParam.enableSvcVideoEncoding = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--videnctype") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "mpeg4"))
                {
                    g_cameraParam.videoEncoderType = VIDEO_ENCODER_TYPE_MPEG4;
                }
                else if (!strcmp(argv[arg], "h263"))
                {
                    g_cameraParam.videoEncoderType = VIDEO_ENCODER_TYPE_H263;
                }

                if (argc - 1 > arg && strncmp(argv[arg+1], "--", 2))
                {
                    arg++;
                    g_cameraParam.videoEncodeLevel = atoi(argv[arg]);
                }
            }
            else if (!strcmp(argv[arg], "--temporaltradeofflevel") && argc - 1 > arg)
            {
                arg ++;
                g_cameraParam.TemporalTradeOffLevel = (OMX_U32)atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--fps") && argc - 1 > arg)
            {
                g_cameraParam.nFrameRate = (OMX_U32)atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--focuspos") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.focusPosition = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--focusauto") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "on"))
                {
                    g_cameraParam.focusAuto = OMX_IMAGE_FocusControlAuto;
                }
                else
                {
                    g_cameraParam.focusAuto = OMX_IMAGE_FocusControlOn;
                }
            }
            else if (!strcmp(argv[arg], "--autoframerate") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "on"))
                {
                    g_cameraParam.bAutoFrameRate = OMX_TRUE;
                }
                else
                {
                    g_cameraParam.bAutoFrameRate = OMX_FALSE;
                }
                if (argc - 2 > arg && strncmp(argv[arg+1], "--", 2))
                {
                    g_cameraParam.nAutoFrameRateLow = NV_SFX_WHOLE_TO_FX(atoi(argv[arg+1]));
                    g_cameraParam.nAutoFrameRateHigh = NV_SFX_WHOLE_TO_FX(atoi(argv[arg+2]));
                    arg += 2;
                }
            }
            else if (!strcmp(argv[arg], "--iframeinterval") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nIntraFrameInterval = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--expcomp") && argc - 1 > arg)
            {
                double expcomp = atof(argv[arg+1]);
                arg++;
                g_cameraParam.xEVCompensation = NV_SFX_FP_TO_FX((float)expcomp);
            }
            else if (!strcmp(argv[arg], "--metermode") && argc - 1 > arg)
            {
                arg++;

                if (strcmp(argv[arg], "spot") == 0)
                    g_cameraParam.exposureMeter = OMX_MeteringModeSpot;
                else if (strcmp(argv[arg], "center") == 0)
                    g_cameraParam.exposureMeter = OMX_MeteringModeAverage;
                else if (strcmp(argv[arg], "matrix") == 0)
                    // TODO allow specifying the matrix, for now, use default
                    g_cameraParam.exposureMeter = OMX_MeteringModeMatrix;
                else if (strcmp(argv[arg], "eval") == 0)
                {
                    // TODO use new extension for this
                    NvOsDebugPrintf("--metermode eval not implemented yet.\n");
                    NVTEST_FAIL(&nvtestapp);
                    goto omx_test_end;
                }
                else
                {
                    NvOsDebugPrintf("--metermode parameter should be spot, center, matrix, or eval.\n");
                    NVTEST_FAIL(&nvtestapp);
                    goto omx_test_end;
                }

            }
            else if (!strcmp(argv[arg], "--rcmode") && argc - 1 > arg)
            {
                arg++;
                if (strcmp(argv[arg], "vbr") == 0)
                    g_cameraParam.eRCMode = NVX_VIDEO_RateControlMode_VBR;
                else if (strcmp(argv[arg], "cbr") == 0)
                    g_cameraParam.eRCMode = NVX_VIDEO_RateControlMode_CBR;
                else
                {
                    NvOsDebugPrintf("--rcmode parameter should be vbr or cbr.\n");
                    NVTEST_FAIL(&nvtestapp);
                    goto omx_test_end;
                }
            }
            else if (!strcmp(argv[arg], "--exposure") && argc - 1 > arg)
            {
                char *exposureString;
                arg++;
                // Check for people trying to pass in seconds instead of milliseconds
                // (eg, --exposure .033 when they really mean --exposure 33)
                // Simply doing a strchr for '.' would catch 99% of mistakes made,
                // but checking for all non-digits is more robust.
                for (exposureString = argv[arg]; *exposureString != '\0'; exposureString++)
                {
                    if (!isdigit(*exposureString))
                    {
                        NvOsDebugPrintf("--exposure parameter is invalid, must be integer milliseconds.\n");
                        NVTEST_FAIL(&nvtestapp);
                        goto omx_test_end;
                    }
                }
                g_cameraParam.exposureTimeMS = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--stab") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "on"))
                    g_cameraParam.bStab = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--stereocamera") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "left"))
                    g_cameraParam.StCamMode = NvxLeftOnly;
                else if (!strcmp(argv[arg], "right"))
                    g_cameraParam.StCamMode = NvxRightOnly;
                else if (!strcmp(argv[arg], "stereo"))
                    g_cameraParam.StCamMode = NvxStereo;
            }
            else if (!strcmp(argv[arg], "--stereocapturetype") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "video"))
                    g_cameraParam.StCapInfo.CameraCaptureType = NvxCameraCaptureType_Video;
                else if (!strcmp(argv[arg], "videosquashed"))
                    g_cameraParam.StCapInfo.CameraCaptureType = NvxCameraCaptureType_VideoSquashed;
                else if (!strcmp(argv[arg], "still"))
                    g_cameraParam.StCapInfo.CameraCaptureType = NvxCameraCaptureType_Still;
                else if (!strcmp(argv[arg], "stillburst"))
                    g_cameraParam.StCapInfo.CameraCaptureType = NvxCameraCaptureType_StillBurst;
                else
                    NvOsDebugPrintf("--stereocapturetype %s is UnSupported. Default "
                        "it to 'video'\n", argv[arg]);
            }
            else if (!strcmp(argv[arg], "--stereotype") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "lr"))
                    g_cameraParam.StCapInfo.CameraStereoType = Nvx_BufferFlag_Stereo_LeftRight;
                else if (!strcmp(argv[arg], "tb"))
                    g_cameraParam.StCapInfo.CameraStereoType = Nvx_BufferFlag_Stereo_TopBottom;
                /*else if (!strcmp(argv[arg], "rl"))
                    g_cameraParam.StCapInfo.CameraStereoType = Nvx_BufferFlag_Stereo_RightLeft;
                else if (!strcmp(argv[arg], "bt"))
                    g_cameraParam.StCapInfo.CameraStereoType = Nvx_BufferFlag_Stereo_BottomTop;
                else if (!strcmp(argv[arg], "srl"))
                    g_cameraParam.StCapInfo.CameraStereoType = Nvx_BufferFlag_Stereo_SeparateRL;
                else if (!strcmp(argv[arg], "slr"))
                    g_cameraParam.StCapInfo.CameraStereoType = Nvx_BufferFlag_Stereo_SeparateLR;*/
                else
                    NvOsDebugPrintf("--stereotype %s is UnSupported. Default "
                        "it to 'lr'\n", argv[arg]);
            }
            else if (!strcmp(argv[arg], "--iso") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.iso = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--contrast") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nContrast = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--brightness") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nBrightness = atoi(argv[arg]);
                if (100 < g_cameraParam.nBrightness)
                    g_cameraParam.nBrightness = 100;
                else if (0 > g_cameraParam.nBrightness)
                    g_cameraParam.nBrightness = 0;
                g_cameraParam.bUserSetBrightness = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--saturation") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nSaturation = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--hue") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nHue = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--loopstillcapture") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nLoopsStillCapture = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--delaycapture") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nDelayCapture = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--delayloopstillcapture") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nDelayLoopStillCapture = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--delayhalfpress") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nDelayHalfPress = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--halfpress") && argc - 2 > arg)
            {
                g_cameraParam.bHalfPress = OMX_TRUE;
                arg++;
                g_cameraParam.nHalfPressFlag = atoi(argv[arg]);
                arg++;
                g_cameraParam.nHalfPressTimeOutMS = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--abortonhpfailure"))
            {
                g_cameraParam.bAbortOnHalfPressFailure = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--flash") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "on"))
                {
                    g_cameraParam.eFlashControl = OMX_IMAGE_FlashControlOn;
                }
                else if (!strcmp(argv[arg], "auto"))
                {
                    g_cameraParam.eFlashControl = OMX_IMAGE_FlashControlAuto;
                }
                else // if (!strcmp(argv[arg], "off"))
                {
                    g_cameraParam.eFlashControl = OMX_IMAGE_FlashControlOff;
                }
            }
            else if (!strcmp(argv[arg], "--flicker") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "auto"))
                {
                    g_cameraParam.eFlicker = NvxFlicker_Auto;
                }
                else if (!strcmp(argv[arg], "50"))
                {
                    g_cameraParam.eFlicker = NvxFlicker_50HZ;
                }
                else if (!strcmp(argv[arg], "60"))
                {
                    g_cameraParam.eFlicker = NvxFlicker_60HZ;
                }
                else // if (!strcmp(argv[arg], "off"))
                {
                    g_cameraParam.eFlicker = NvxFlicker_Off;
                }
            }
            else if (!strcmp(argv[arg], "--zoom") && argc - 1 > arg)
            {
                double zoom = atof(argv[arg+1]);   // allowed values: 1.0-->8.0
                arg++;
                if (zoom >= 1.0 && zoom <= 8.0)
                {
                    g_cameraParam.xScaleFactor = NV_SFX_FP_TO_FX((float)zoom);
                }
                else
                {
                    NvOsDebugPrintf("--zoom parameter is out of range. should be in [1.0, 8.0]\n");
                    goto omx_test_end;
                }
            }
            else if (!strcmp(argv[arg], "--zoommult") && argc - 1 > arg)
            {
                double zoom = atof(argv[arg+1]);   // allowed values: 1.0-->8.0
                arg++;

                g_cameraParam.xScaleFactorMultiplier = NV_SFX_FP_TO_FX((float)zoom);
            }
            else if (!strcmp(argv[arg], "--smoothzoomtime") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nSmoothZoomTimeMS = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--zoomabort") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.nZoomAbortMS = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--edgeenhancement") && argc - 1 > arg)
            {
                arg++;
                g_cameraParam.EdgeEnhancementStrengthBias = (NVX_F32)atof(argv[arg]);   // Within [-1.0 , 1.0]
            }
            else if (!strcmp(argv[arg], "--cameracrop") && argc - 5 > arg)
            {
                NVX_RectF32 *pRect = NULL;
                arg++;
                if (!strcmp(argv[arg], "preview"))
                    pRect = &g_cameraParam.oCropPreview;
                else
                    pRect = &g_cameraParam.oCropCapture;

                arg++;
                pRect->left = atoi(argv[arg]);
                arg++;
                pRect->top = atoi(argv[arg]);
                arg++;
                pRect->right = atoi(argv[arg]);
                arg++;
                pRect->bottom = atoi(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--precaptureconverge") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "off"))
                {
                    g_cameraParam.bPrecaptureConverge = OMX_FALSE;
                }
                else // if (!strcmp(argv[arg], "on"))
                {
                    g_cameraParam.bPrecaptureConverge = OMX_TRUE;
                }
            }
            else if (!strcmp(argv[arg], "--imagefilter") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "bw"))
                {
                    g_cameraParam.eImageFilter = NVX_ImageFilterBW;
                }
                else if (!strcmp(argv[arg], "sepia"))
                {
                    g_cameraParam.eImageFilter = NVX_ImageFilterSepia;
                }
                else if (!strcmp(argv[arg], "emboss"))
                {
                    g_cameraParam.eImageFilter = OMX_ImageFilterEmboss;
                }
                else if (!strcmp(argv[arg], "negative"))
                {
                    g_cameraParam.eImageFilter = OMX_ImageFilterNegative;
                }
                else if (!strcmp(argv[arg], "solarize"))
                {
                    g_cameraParam.eImageFilter = OMX_ImageFilterSolarize;
                }
                else if (!strcmp(argv[arg], "posterize"))
                {
                    g_cameraParam.eImageFilter = NVX_ImageFilterPosterize;
                }
                else // if (!strcmp(argv[arg], "none"))
                {
                    g_cameraParam.eImageFilter = OMX_ImageFilterNone;
                }
            }
            else if (!strcmp(argv[arg], "--whitebalance") && argc - 1 > arg)
            {
                arg++;
                if (!strcmp(argv[arg], "off"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlOff;
                }
                else if (!strcmp(argv[arg], "sunlight"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlSunLight;
                }
                else if (!strcmp(argv[arg], "cloudy"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlCloudy;
                }
                else if (!strcmp(argv[arg], "shade"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlShade;
                }
                else if (!strcmp(argv[arg], "tungsten"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlTungsten;
                }
                else if (!strcmp(argv[arg], "fluorescent"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlFluorescent;
                }
                else if (!strcmp(argv[arg], "incandescent"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlIncandescent;
                }
                else if (!strcmp(argv[arg], "flash"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlFlash;
                }
                else if (!strcmp(argv[arg], "horizon"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlHorizon;
                }
                else // if (!strcmp(argv[arg], "auto"))
                {
                    g_cameraParam.eWhiteBalControl = OMX_WhiteBalControlAuto;
                }
            }
            else if (!strcmp(argv[arg], "--rawdump") && argc - 2 > arg)
            {
                arg++;
                g_cameraParam.nRawDumpCount = atoi(argv[arg]);
                arg++;
                g_cameraParam.FilenameRawDump = argv[arg];

                // need setup still capture
                pProf = &g_JpegEncoderProf;
                g_cameraParam.nWidthCapture = 0;
                g_cameraParam.nHeightCapture = 0;
                g_cameraParam.enableCameraCapture = OMX_TRUE;
                g_cameraParam.videoCapture = OMX_FALSE;
                pCurGraph->outfile = "out";
            }
            else if (!strcmp(argv[arg], "--pausepreviewaftercapture") && argc - 1 > arg)
            {
                arg++;
                if(!strcmp(argv[arg], "off"))
                {
                   g_cameraParam.bPausePreviewAfterCapture = OMX_FALSE;
                }
                else
                {
                   g_cameraParam.bPausePreviewAfterCapture = OMX_TRUE;
                }
            }
            else if (!strcmp(argv[arg], "--dvs_sanity"))
            {
                // extra logic needed, in case --dvs_sanity is last
                // in the arg list. If more 'sanities' are needed,
                // one should allocate the string, do a snprintf, and free
                // it at the end
                if (pCurGraph->sanity == 7)
                {
                    pCurGraph->outfile = GET_PATH_STORAGE_CARD2("sanity7_out.3gp");
                }
                else
                {
                    pCurGraph->outfile = GET_PATH_STORAGE_CARD2("sanity_encdec.3gp");
                    pCurGraph->infile = GET_PATH_STORAGE_CARD2("sanity_encdec.3gp");
                }
                SanityFileAssigned = NV_TRUE;
            }
            else if (!strcmp(argv[arg], "--sanity") && argc - 1 > arg)
            {
                int testnum;

                arg++;
                testnum = atoi(argv[arg]);

                if (testnum == 1)
                {
                    pCurGraph->desiredfps = 24;
                    FindFile("rat6sec_h264_720_14M.mp4", pCurGraph);
                    numPlaysDesired = 2;
                    pCurGraph->sanity = 1;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if (2 == testnum)
                {
                    pCurGraph->testtype = AV_RECORDER;
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_cameraParam.previewTime = 10;
                    g_cameraParam.captureTime = 5;
                    g_cameraParam.nWidthCapture = 720;
                    g_cameraParam.nHeightCapture = 480;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_TRUE;
                    pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity2_out.3gp");
                    g_cameraParam.enableAudio = OMX_TRUE;
                    g_cameraParam.audioTime = 5;
                    pCurGraph->sanity = 2;
                }
                else if (3 == testnum)
                {
                    pCurGraph->testtype = AV_RECORDER;
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_cameraParam.previewTime = 10;
                    g_cameraParam.nWidthCapture = 720;
                    g_cameraParam.nHeightCapture = 480;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_FALSE;
                    pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity3_out.jpg");
                    g_ImageSequenceWriter = g_ImageWriter;
                    pCurGraph->sanity = 3;
                }
                else if (4 == testnum)
                {
                    pCurGraph->desiredfps = 15;
                    FindFile("murata20sec.ts", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 4;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if (5 == testnum)
                {
                    //OMX_CONFIG_SCALEFACTORTYPE ScaleFactor;
                    pCurGraph->testtype = AV_RECORDER;

                    //preview properties
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_VidRendProf.bNoAVSync = OMX_TRUE;
                    g_cameraParam.previewTime = 10;
                    //videoenc properties
                    g_cameraParam.captureTime = 10;
                    g_cameraParam.nWidthCapture = 1280;
                    g_cameraParam.nHeightCapture = 720;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_TRUE;

                    pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity5_out.3gp");

                    g_cameraParam.nBitRate = (OMX_U32)10000000;
                    g_cameraParam.nAppType = (OMX_U32)0;
                    g_cameraParam.nQualityLevel = (OMX_U32)1;
                    g_cameraParam.nErrorResilLevel = (OMX_U32)0;
                    g_cameraParam.enableSvcVideoEncoding = OMX_FALSE;

                    //Audio Properties
                    g_cameraParam.enableAudio = OMX_TRUE;
                    g_cameraParam.audioTime = 10;

                    pCurGraph->sanity = 5;

                    //Enable Test Pattern
                    g_cameraParam.EnableTestPattern = NV_TRUE;

                    //Set ZoomFactor
                    g_cameraParam.xScaleFactor = NV_SFX_ONE; //NV_SFX_ONE = 0x00010000 //need to work it out as per requirement

                    //Set ZoomFactorMultiplier
                    g_cameraParam.xScaleFactorMultiplier = NV_SFX_FP_TO_FX_CT(1.000385227272); //this gives 35.8 fps of video.

                }
                else if (6 == testnum) //To check only video Perf
                {
                    //OMX_CONFIG_SCALEFACTORTYPE ScaleFactor;
                    pCurGraph->testtype = AV_RECORDER;

                    //preview properties
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_VidRendProf.bNoAVSync = OMX_TRUE;
                    g_cameraParam.previewTime = 10;
                    //videoenc properties
                    g_cameraParam.captureTime = 10;
                    g_cameraParam.nWidthCapture = 1280;
                    g_cameraParam.nHeightCapture = 720;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_TRUE;

                    pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity6_out.3gp");

                    g_cameraParam.nBitRate = (OMX_U32)10000000;
                    g_cameraParam.nAppType = (OMX_U32)0;
                    g_cameraParam.nQualityLevel = (OMX_U32)1;
                    g_cameraParam.nErrorResilLevel = (OMX_U32)0;
                    g_cameraParam.enableSvcVideoEncoding = OMX_FALSE;

                    //Audio Properties
                    g_cameraParam.enableAudio = OMX_FALSE;
                    g_cameraParam.audioTime = 0;
                    pCurGraph->noaud = OMX_TRUE;
                    pCurGraph->audio = NULL;

                    pCurGraph->sanity = 6;

                    //Enable Test Pattern
                    g_cameraParam.EnableTestPattern = NV_TRUE;

                    //Set ZoomFactor
                    g_cameraParam.xScaleFactor = NV_SFX_ONE; //NV_SFX_ONE = 0x00010000 //need to work it out as per requirement

                    //Set ZoomFactorMultiplier
                    g_cameraParam.xScaleFactorMultiplier = NV_SFX_FP_TO_FX_CT(1.000385227272); //this gives 35.8 fps of video.

                }
                else if (7 == testnum)
                {
                    //OMX_CONFIG_SCALEFACTORTYPE ScaleFactor;
                    pCurGraph->testtype = AV_RECORDER;

                    //preview properties
                    g_cameraParam.enablePreview = OMX_FALSE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_VidRendProf.bNoAVSync = OMX_TRUE;
                    g_cameraParam.previewTime = 10;
                    //videoenc properties
                    g_cameraParam.captureTime = 10;
                    g_cameraParam.nWidthCapture = 1280;
                    g_cameraParam.nHeightCapture = 720;
                    g_cameraParam.enableCameraCapture = OMX_FALSE;
                    g_cameraParam.videoCapture = OMX_FALSE;

                    // Reuse the dvs_sanity logic flag
                    // this will have the output go to a 'scratch' partition
                    if (!SanityFileAssigned)
                        pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity7_out.3gp");
                    else
                        pCurGraph->outfile = GET_PATH_STORAGE_CARD2("sanity7_out.3gp");

                    //Audio Properties
                    g_cameraParam.enableAudio = OMX_TRUE;
                    g_cameraParam.audioTime = 10;

                    pCurGraph->sanity = 7;
                }
                else if(9 == testnum) // Video playback sanity
                {
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 9;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if (10 == testnum)
                {
                    // H264 720p A/V recording 10 seconds
                    pCurGraph->testtype = AV_RECORDER;
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_VidRendProf.bNoAVSync = OMX_TRUE;
                    g_cameraParam.previewTime = 5;
                    g_cameraParam.captureTime = 5;
                    g_cameraParam.nWidthCapture = 1280;
                    g_cameraParam.nHeightCapture = 720;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_TRUE;
                    if (!SanityFileAssigned)
                        pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity_encdec.3gp");
                    g_cameraParam.videoEncoderType = VIDEO_ENCODER_TYPE_H264;
                    g_cameraParam.nBitRate = (OMX_U32)10000000;
                    g_cameraParam.nAppType = (OMX_U32)0;
                    g_cameraParam.nQualityLevel = (OMX_U32)1;
                    g_cameraParam.nErrorResilLevel = (OMX_U32)0;
                    g_cameraParam.enableSvcVideoEncoding = OMX_FALSE;
                    g_cameraParam.enableAudio = OMX_TRUE;
                    g_cameraParam.audioTime = 5;
                    g_cameraParam.bAutoFrameRate = OMX_TRUE;
                    g_cameraParam.nAutoFrameRateLow = NV_SFX_WHOLE_TO_FX(30);
                    g_cameraParam.nAutoFrameRateHigh = NV_SFX_WHOLE_TO_FX(31);
                    pCurGraph->sanity = 10;
                }
                if (11 == testnum)
                {
                    // playback result of #10
                    pCurGraph->desiredfps = 30;
                    if (!SanityFileAssigned)
                        pCurGraph->infile = GET_PATH_STORAGE_CARD("sanity_encdec.3gp");
                    pCurGraph->sanity = 11;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if (12 == testnum)
                {
                    // MPEG4 D1 A/V recording 10 seconds
                    pCurGraph->testtype = AV_RECORDER;
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_VidRendProf.bNoAVSync = OMX_TRUE;
                    g_cameraParam.previewTime = 10;
                    g_cameraParam.captureTime = 10;
                    g_cameraParam.nWidthCapture = 720;
                    g_cameraParam.nHeightCapture = 480;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_TRUE;
                    if (!SanityFileAssigned)
                        pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity_encdec.3gp");
                    g_cameraParam.videoEncoderType = VIDEO_ENCODER_TYPE_MPEG4;
                    g_cameraParam.nBitRate = (OMX_U32)4000000;
                    g_cameraParam.nAppType = (OMX_U32)0;
                    g_cameraParam.nQualityLevel = (OMX_U32)1;
                    g_cameraParam.nErrorResilLevel = (OMX_U32)0;
                    g_cameraParam.enableSvcVideoEncoding = OMX_FALSE;
                    g_cameraParam.enableAudio = OMX_TRUE;
                    g_cameraParam.audioTime = 10;
                    g_cameraParam.bAutoFrameRate = OMX_TRUE;
                    g_cameraParam.nAutoFrameRateLow = NV_SFX_WHOLE_TO_FX(30);
                    g_cameraParam.nAutoFrameRateHigh = NV_SFX_WHOLE_TO_FX(31);
                    pCurGraph->sanity = 12;
                }
                if (13 == testnum)
                {
                    // playback result of #12
                    pCurGraph->desiredfps = 30;
                    if (!SanityFileAssigned)
                        pCurGraph->infile = GET_PATH_STORAGE_CARD("sanity_encdec.3gp");
                    pCurGraph->sanity = 13;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                if (14 == testnum)
                {
                    // 1080p Test Pattern video encode

                    //OMX_CONFIG_SCALEFACTORTYPE ScaleFactor;
                    pCurGraph->testtype = AV_RECORDER;

                    //preview properties
                    g_cameraParam.enablePreview = OMX_TRUE;
                    g_cameraParam.nWidthPreview = 720;
                    g_cameraParam.nHeightPreview = 480;
                    g_VidRendProf.bNoAVSync = OMX_TRUE;
                    g_cameraParam.previewTime = 10;
                    //videoenc properties
                    g_cameraParam.captureTime = 10;
                    g_cameraParam.nWidthCapture = 1920;
                    g_cameraParam.nHeightCapture = 1088;
                    g_cameraParam.enableCameraCapture = OMX_TRUE;
                    g_cameraParam.videoCapture = OMX_TRUE;

                    pCurGraph->outfile = GET_PATH_STORAGE_CARD("sanity14_out.3gp");

                    g_cameraParam.nBitRate = (OMX_U32)14000000;
                    g_cameraParam.nAppType = (OMX_U32)0;
                    g_cameraParam.nQualityLevel = (OMX_U32)1;
                    g_cameraParam.nErrorResilLevel = (OMX_U32)0;
                    g_cameraParam.enableSvcVideoEncoding = OMX_FALSE;

                    //Audio Properties
                    g_cameraParam.enableAudio = OMX_TRUE;
                    g_cameraParam.audioTime = 10;

                    pCurGraph->sanity = 14;

                    //Enable Test Pattern
                    g_cameraParam.EnableTestPattern = NV_TRUE;

                    //Set ZoomFactor
                    g_cameraParam.xScaleFactor = NV_SFX_ONE; //NV_SFX_ONE = 0x00010000 //need to work it out as per requirement

                    //Set ZoomFactorMultiplier
                    g_cameraParam.xScaleFactorMultiplier = NV_SFX_FP_TO_FX_CT(1.000385227272);

                }
                else if(15 == testnum)
                {
                    // Sanity test for H264 Baseline Profile 1080P 20 Mbps 30fps playback
                    pCurGraph->desiredfps = 30;
                    FindFile("rat6sec_h264_bp_1080_30fps_20M.mp4", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 15;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(16 == testnum)
                {
                    // Sanity test for XviD/DivX HD Profile 1080P 8Mbps 30fps playback
                    pCurGraph->desiredfps = 30;
                    FindFile("rat6sec_xvid_1080_30fps_8M.avi", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 16;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(17 == testnum)
                {
                    // Sanity test for MPEG4 Simple Profile 1080P 20Mbps 30fps playback
                    pCurGraph->desiredfps = 30;
                    FindFile("rat6sec_mpeg4_1080_30fps_20M.mp4", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 17;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(18 == testnum)
                {
                    // Sanity test for H264 Main Profile CABAC 720P 4Mbps 30fps playback
                    pCurGraph->desiredfps = 30;
                    FindFile("rat6sec_h264_mp_b1_cabac_720_30_4M.mp4", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 18;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(19 == testnum)
                {
                    // Sanity test for VC-1 AP 1080P 20Mbps 30fps playback
                    pCurGraph->desiredfps = 30;
                    FindFile("rat6sec_vc1_ap_1080P_30_10M.wmv", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 19;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(20 == testnum)
                {
                    pCurGraph->desiredfps = 30;
                    FindFile("sony5sec_mjpeg_720P_30fps_Q75.avi", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 20;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                //Audio sanity tests start from 20
                else if(21 == testnum)
                {
                    FindFile("aac_7s.m4a", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 21;
                    fastPlaySupported = 1 ;
                }
                else if(22 == testnum)
                {
                    FindFile("mp3_5s.mp3", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 23;
                    fastPlaySupported = 1;
                }
                else if(23 == testnum)
                {
                    FindFile("wma_10s.wma", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 25;
                    fastPlaySupported = 1;
                }
                /*else if(24 == testnum)
                {
                    FindFile("eaacp_10s.3gp", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 26;
                    fastPlaySupported = 0 ;
                }
                else if(22 == testnum)
                {
                    FindFile("amrnb_10s.3gp", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 22;
                    fastPlaySupported = 0;
                }
               else if(24 == testnum)
                {
                    FindFile("ogg_5s.ogg", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 24;
                    fastPlaySupported = 0 ;
                }*/
                //Video sanity tests start from 20
                else if(51 == testnum)
                {
                    FindFile("golden_flower_wmv_wma_720_30p_1M.wmv", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 51;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(52 == testnum)
                {
                    FindFile("david_gilmour_h264_aac_720_30p_1M.mp4", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 52;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
                else if(53 == testnum)
                {
                    FindFile("ShortStream.avi", pCurGraph);
                    numPlaysDesired = 1;
                    pCurGraph->sanity = 53;
                    g_VidRendProf.bSanity = OMX_TRUE;
                }
            }
            else if (!strcmp(argv[arg], "--thumbnail"))
            {
                pCurGraph->bThumbnailPreferred = OMX_TRUE;
                if (argc - 2 > arg && strncmp(argv[arg+1], "--", 2))
                {
                    pCurGraph->nThumbnailWidth = atoi(argv[arg+1]);
                    pCurGraph->nThumbnailHeight = atoi(argv[arg+2]);
                    arg += 2;
                }
            }
            else if (!strcmp(argv[arg], "--downscalefactor") && argc - 1 > arg)
            {
                int dsf = atoi(argv[arg+1]);   // allowed values: 1/2/4/8
                arg++;
                if (dsf > 0)
                {
                    pCurGraph->xScaleFactor = NV_SFX_FP_TO_FX((float)1.0 / (float)dsf);
                }
            }
            else if (!strcmp(argv[arg], "--encodedsize") && argc - 1 > arg)
            {
                arg++;
                pCurGraph->nEncodedSize = (atoi(argv[arg]) + 1023) & (~1023);
            }
            else if (!strcmp(argv[arg], "--readbuffersize") && argc - 1 > arg)
            {
                arg++;
                pCurGraph->nReadBufferSize = (atoi(argv[arg]) + 1023) & (~1023);
            }
            else if (!strcmp(argv[arg], "--highmem"))
            {
                pCurGraph->bLowMem = OMX_FALSE;
            }
            else if (!strcmp(argv[arg], "--trickmode") && argc - 1 > arg)
            {
                arg++;
                //  Fast    *     Slow       *    Slow      *      Fast
                //  Rewind -1.0   Rewind     O     Forward  +1.0   Forward
                // <***********************************************************>
                //          *                *              *
                //      Reverse Playback   Paused       Normal Plyaback
                pCurGraph->playSpeed = (float)atof(argv[arg]);
            }
            else if (!strcmp(argv[arg], "--alphablend"))
            {
                pCurGraph->bAlphaBlend = OMX_TRUE;
                if (argc - 1 > arg && strncmp(argv[arg+1], "-", 1))
                {
                    arg++;
                    pCurGraph->nAlphaValue = atoi(argv[arg]);
                }
            }
            else if (!strcmp(argv[arg], "--coverart"))
            {
                pCurGraph->bSaveCoverArt = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--fullscreen"))
            {
                pCurGraph->bHideDesktop = OMX_TRUE;
                pCurGraph->bNoWindow = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "-y") && argc - 1 > arg)
            {
                NvU32 fmt_type = FMT_18;
                NvU32 arg_incr = 0;
                arg++;
                if ((argc - 1) > arg)
                {
                    if (!strcmp(argv[arg+1], "HP"))
                    {
                        fmt_type = FMT_22;
                        arg_incr = 1;
                    }
                    else if (!strcmp(argv[arg+1], "BP"))
                    {
                        fmt_type = FMT_18;
                        arg_incr = 1;
                    }
                    else
                        fmt_type = FMT_18;
                }
                CreateYTURL(pCurGraph, argv[arg], fmt_type);
                arg += arg_incr;
            }
            else if (!strcmp(argv[arg], "--capture"))
            {
                pCurGraph->bCaptureFrame = OMX_TRUE;
                if (argc - 1 > arg && strncmp(argv[arg+1], "-", 1))
                {
                    arg++;
                    pCurGraph->capturefile = argv[arg];
                }
            }
            else if (!strcmp(argv[arg], "--tvout"))
            {
                pCurGraph->renderer = g_TvoutRender;
                TVdisplay = 1;
            }
            else if (!strcmp(argv[arg], "--playtime"))
            {
                pCurGraph->bPlayTime = OMX_TRUE;
            }
            else if (!strcmp(argv[arg], "--custcomp"))
            {
                pCurGraph->custcomp = g_SampleComponent;
            }
            else if (!strcmp(argv[arg], "--startoffset") && argc - 1 > arg)
            {
                pCurGraph->startTimeInSec = atoi(argv[arg+1]);
                arg++;
            }
            else if (!strcmp(argv[arg], "--secondary"))
            {
                pCurGraph->renderer = g_SecondaryRender;
            }
            else if (!strcmp(argv[arg], "--mp3") ||
                     !strcmp(argv[arg], "--vorbis") ||
                     !strcmp(argv[arg], "--avi") ||
                     !strcmp(argv[arg], "--asf") ||
                     !strcmp(argv[arg], "--3gp") ||
                     !strcmp(argv[arg], "--3gpp") ||
                     !strcmp(argv[arg], "--ulp"))
            {
                NvOsDebugPrintf("Option '%s' is deprecated and will be removed completely in a future revision\n", argv[arg]);
            }
            else if (!strcmp(argv[arg], "--cacheinfo"))
            {
                pCurGraph->testcmd = CACHEINFO;
            }
            else if (!strcmp(argv[arg], "--smartdimmer"))
            {
                pCurGraph->bSmartDimmerEnable = NV_TRUE;
                // Enable smart dimmer only for full screen
                pCurGraph->bHideDesktop = OMX_TRUE;
                pCurGraph->bNoWindow = OMX_TRUE;
            }
            else
            {
                NvOsDebugPrintf("OMX Player unknown argument: \"%s\"\n", argv[arg]);
                sanitystatus++;
                goto omx_test_end_cleanup;
            }
        }
    }

    g_cameraParam.nTSAppStart = NvOsGetTimeUS() * 10;

    {
        OMX_BOOL bRunTest = OMX_FALSE;
        int g;

        for (g = 0; g < NVX_MAX_OMXPLAYER_GRAPHS; g++)
        {
            pCurGraph = &g_Graphs[g];

            if (pCurGraph->testtype == NONE && pCurGraph->infile != NULL)
            {
                PickParser(pCurGraph);
                // If still test type is NONE, return sanity error.
                if(pCurGraph->testtype == NONE)
                {
                    sanitystatus++;
                    NvOsDebugPrintf("OMX Error: Still test type is NONE sanitystatus %d\n", sanitystatus);
                }
            }

            if (eDeleteJPEGFilesTrue == g_cameraParam.eDeleteJPEGFiles && pCurGraph->outfile && AV_RECORDER == pCurGraph->testtype)
            {
                RemoveJPEGFiles(pCurGraph->outfile);
            }

            if (pCurGraph->testtype == NONE)
                continue;

            bRunTest = OMX_TRUE;
            if (NvSuccess != InitGraph(pCurGraph))
            {


                sanitystatus++;
                videoloop = numPlaysDesired;
                NvOsDebugPrintf("OMX Error: InitGraph failed sanitystatus %d\n", sanitystatus);
                goto omx_test_end_cleanup;
            }
        }

        if (!bRunTest)
        {
            videoloop = numPlaysDesired;
            goto omx_test_end_cleanup;
        }
    }

    NvOsDebugPrintf("OMX Player starting playback\n");

    // Start all components in the graph
    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        OMX_ERRORTYPE eErrorTmp = OMX_ErrorNone;

        pCurGraph = &g_Graphs[i];
        if (pCurGraph->testtype == NONE)
            continue;

        if (!(pCurGraph->testcmd == PLAY && pCurGraph->bAudioOnly))
            bIsULP = OMX_FALSE;

        if (pCurGraph->testcmd == PLAY)
        {
            NvOsDebugPrintf("OMX Player Play Test Started\n");
            NvOsDebugPrintf("- Start playback\n");
        }
        else if (pCurGraph->testcmd == ULPTEST)
        {
            NvOsDebugPrintf("OMX Player ULP Switch Test Started\n");
            NvOsDebugPrintf("- Starting playback in ULP mode for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == PAUSE)
        {
            NvOsDebugPrintf("OMX Player Pause Test Started\n");
            NvOsDebugPrintf("- Starting playback for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == PAUSE_STOP)
        {
            NvOsDebugPrintf("OMX Player Pause Stop Test Started\n");
            NvOsDebugPrintf("- Starting playback for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == LOADED_IDLE_LOADED)
        {
            NvOsDebugPrintf("OMX Player Loaded->Idle->Loaded Test Started\n");
        }
        else if (pCurGraph->testcmd == FLUSH_PORTS_IN_IDLE)
        {
            NvOsDebugPrintf("OMX Player IDLE_STATE->Flush Ports Test Started\n");
        }
        else if (pCurGraph->testcmd == SEEK)
        {
            NvOsDebugPrintf("OMX Player Seek Test Started\n");
            NvOsDebugPrintf("- Starting playback for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == COMPLEX_SEEK)
        {
            NvOsDebugPrintf("OMX Player Complex Seek Test Started\n");
            playTimeInSec = 1;
            NvOsDebugPrintf("- Starting playback for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == CRAZY_SEEK_TEST)
        {
            NvOsDebugPrintf("OMX Player CRAZY_SEEK_TEST Started\n");
            NvOsDebugPrintf("Crazy Seek Count = %d \n", pCurGraph->nCrazySeekCount);

            if (pCurGraph->nCrazySeekSeed == 0)
            {
                pCurGraph->nCrazySeekSeed = 1;
            }
            NvOsDebugPrintf("Crazy Seek Seed = %d \n", pCurGraph->nCrazySeekSeed);

            if (pCurGraph->nCrazySeekDelay == 0)
            {
                pCurGraph->nCrazySeekDelay = 1000; // 1 sec
            }
            NvOsDebugPrintf("Crazy Seek Delay in ms = %d \n", pCurGraph->nCrazySeekDelay);

            playTimeInSec = pCurGraph->nCrazySeekDelay;
            NvOsDebugPrintf("- Starting playback for %d msec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == STOP)
        {
            NvOsDebugPrintf("OMX Player Stop Test Started\n");
            NvOsDebugPrintf("- Starting playback for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == VOLUME)
        {
            NvOsDebugPrintf("OMX Player Volume Test Started\n");
            NvOsDebugPrintf("- Starting playback for %d sec\n", playTimeInSec);
        }
        else if (pCurGraph->testcmd == TRICKMODE)
        {
            NvOsDebugPrintf("OMX Player Trick Mode Test Started\n");
        }
        else if (pCurGraph->testcmd == TRICKMODESANITY)
        {
            NvOsDebugPrintf("OMX Player Trick Mode Sanity Test Started\n");
           playTimeInSec = 1;
        }

        if (pCurGraph->testtype == PLAYER) 
            pCurGraph->oCompGraph.bWaitOnError = OMX_TRUE;

        if (AV_RECORDER == pCurGraph->testtype)
        {
            pCurGraph->oCompGraph.bNoTimeOut = OMX_TRUE;
            if (g_cameraParam.enableCameraCapture || g_cameraParam.enablePreview)
            {
                OMX_CONFIG_ROTATIONTYPE oRotation;
                INIT_PARAM(oRotation);
                oRotation.nPortIndex = 0;
                oRotation.nRotation = g_cameraParam.nRotationPreview;
                eError = OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonRotate, &oRotation);
                TEST_ASSERT_NOTERROR(eError);
                oRotation.nPortIndex = 1;
                oRotation.nRotation = g_cameraParam.nRotationCapture;
                eError = OMX_SetConfig(g_hCamera, OMX_IndexConfigCommonRotate, &oRotation);
                TEST_ASSERT_NOTERROR(eError);
            }
        }

        // Start all components in the graph
        eErrorTmp = TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateIdle,
                                                OMX_TRUE, &g_oAppData);
        if (pCurGraph->testcmd == LOADED_IDLE_LOADED)
        {
            //We have to return from here
            eErrorTmp = OMX_ErrorBadParameter;
        }
        if (OMX_ErrorNone != eErrorTmp)
        {
            NvOsDebugPrintf("omxplayer: hit error when transitioning from loaded to idle\n");
            TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateLoaded,
                                           OMX_TRUE, &g_oAppData);
            NvxDestroyWindow(pCurGraph->hWnd);
            // Deallocate everything
            FreeAllComponentsFromGraph(&pCurGraph->oCompGraph);
            pCurGraph->testtype = NONE;
            continue;
        }

        if(pCurGraph->testcmd == FLUSH_PORTS_IN_IDLE)
        {
            NvOsDebugPrintf("omxplayer: Flushing All ports in IDLE STATE ++\n");
            FlushAllPorts(&pCurGraph->oCompGraph, &g_oAppData);
            NvOsDebugPrintf("omxplayer: Flushing All ports in IDLE STATE --\n");
        }

        //Set the Exif, and GPS, And Interoperability Info
        if ( (AV_RECORDER == pCurGraph->testtype) &&
            (g_cameraParam.enableCameraCapture && !g_cameraParam.videoCapture) )
        {
            OMX_HANDLETYPE hVIEncoder;

            hVIEncoder = g_hVIEncoder;

            //Exif IFD  Set Up
            if(g_cameraParam.bExifEnable == NV_TRUE)
            {
                NVX_CONFIG_ENCODEEXIFINFO oExifIfd;
                NVX_CONFIG_ENCODEGPSINFO *pGPSInfo = NULL;
                NvU32 i;
                NvXExifArgs ExifArgs;
                OMX_INDEXTYPE  eIndex;
                NvError err = NvSuccess;

                eError = OMX_GetExtensionIndex(hVIEncoder, NVX_INDEX_CONFIG_ENCODEEXIFINFO,
                    &eIndex);

                TEST_ASSERT_NOTERROR(eError);
                INIT_PARAM(oExifIfd);
                oExifIfd.nPortIndex = 0;
                for (i = 0; i < MAX_EXIF_PARMS; i++)
                {
                    ExifArgs.Argv[i] = NvOsAlloc(MAX_EXIF_STRING_IN_BYTES);
                    if (NULL == ExifArgs.Argv[i])
                    {
                        NvOsDebugPrintf("omxplayer: Failed to Allocate memory for Commands \n");
                        goto EXIF_CLEANUP;
                    }
                }
                err = OMX_Fill_ExifIfd(&ExifArgs, g_cameraParam.ExifFile);
                if(err != NvSuccess)
                {
                    NvOsDebugPrintf("omxplayer: Failed to Allocate memory for Commands \n");
                    goto EXIF_CLEANUP;
                }
                Parse_APP1_Args(ExifArgs.Argc,ExifArgs.Argv, &oExifIfd, pGPSInfo);

                OMX_SetConfig( hVIEncoder, eIndex, &oExifIfd);
EXIF_CLEANUP:
                for (i = 0; i < MAX_EXIF_PARMS; i++)
                {
                    NvOsFree(ExifArgs.Argv[i]);
                }
            }

            //GPS IFD Set Up
            if(g_cameraParam.bGPSEnable == NV_TRUE)
            {
                NVX_CONFIG_ENCODEGPSINFO oGPSIfd;
                NVX_CONFIG_ENCODEEXIFINFO *pExifInfo = NULL;
                NvXExifArgs ExifArgs;
                NvU32 i;
                OMX_INDEXTYPE  eIndex;
                NvError err = NvSuccess;

                eError = OMX_GetExtensionIndex(hVIEncoder, NVX_INDEX_CONFIG_ENCODEGPSINFO,
                    &eIndex);
                TEST_ASSERT_NOTERROR(eError);
                INIT_PARAM(oGPSIfd);
                oGPSIfd.nPortIndex = 0;

                for (i = 0; i < MAX_EXIF_PARMS; i++)
                {
                    ExifArgs.Argv[i] = NvOsAlloc(MAX_EXIF_STRING_IN_BYTES);
                    if (NULL == ExifArgs.Argv[i])
                    {
                        NvOsDebugPrintf("omxplayer: Failed to Allocate memory for Commands \n");
                        goto GPS_CLEANUP;
                    }
                }
                err = OMX_Fill_ExifIfd(&ExifArgs, g_cameraParam.GPSFile);
                if(err != NvSuccess)
                {
                    NvOsDebugPrintf("omxplayer: Failed to Allocate memory for Commands \n");
                    goto GPS_CLEANUP;
                }
                Parse_APP1_Args(ExifArgs.Argc,ExifArgs.Argv, pExifInfo, &oGPSIfd);

                OMX_SetConfig( hVIEncoder, eIndex, &oGPSIfd);
GPS_CLEANUP:
                for (i = 0; i < MAX_EXIF_PARMS; i++)
                {
                    NvOsFree(ExifArgs.Argv[i]);
                }
            }

            if(g_cameraParam.bInterOpInfo == NV_TRUE)
            {
                NVX_CONFIG_ENCODE_INTEROPINFO oInterOpInfo;
                OMX_INDEXTYPE  eIndex;

                eError = OMX_GetExtensionIndex(hVIEncoder, NVX_INDEX_CONFIG_ENCODE_INTEROPINFO,
                    &eIndex);

                TEST_ASSERT_NOTERROR(eError);
                INIT_PARAM(oInterOpInfo);
                oInterOpInfo.nPortIndex = 0;
                NvOsStrncpy((char *)oInterOpInfo.Index, (const char *)g_cameraParam.InterOpInfoIndex,MAX_INTEROP_STRING_IN_BYTES);
                OMX_SetConfig( hVIEncoder, eIndex, &oInterOpInfo);
            }

        }
        if (pCurGraph->startTimeInSec)
        {
            OMX_ERRORTYPE eErrorTmp2;
            OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
            int timeInMilliSec = pCurGraph->startTimeInSec*1000;

            INIT_PARAM(timestamp);
            timestamp.nPortIndex = 0;
            timestamp.nTimestamp = (OMX_TICKS)(timeInMilliSec)*1000;

            NvOsDebugPrintf("Starting playback at %d ms position\n", timeInMilliSec);

            // seek to the new position
            eErrorTmp2 = OMX_SetConfig(g_FileReader, OMX_IndexConfigTimePosition, &timestamp);
            TEST_ASSERT_NOTERROR(eErrorTmp2);

            if (pCurGraph->oCompGraph.eErrorEvent != OMX_ErrorNone)
            {
                NvOsDebugPrintf("omxplayer: hit error when seeking to initial position\n");
                TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateLoaded,
                                               OMX_TRUE, &g_oAppData);
                NvxDestroyWindow(pCurGraph->hWnd);
                // Deallocate everything
                FreeAllComponentsFromGraph(&pCurGraph->oCompGraph);
                pCurGraph->testtype = NONE;
                continue;
            }

            // Start the clock
            StartClock(pCurGraph->hClock, (NvU32)(timestamp.nTimestamp / 1000));
        }
        else
        {
            StartClock(pCurGraph->hClock, 0);
        }

        eErrorTmp = TransitionAllComponentsToState(&pCurGraph->oCompGraph,
                                       OMX_StateExecuting, OMX_TRUE,
                                       &g_oAppData);
        if (OMX_ErrorNone != eErrorTmp)
        {
            NvOsDebugPrintf("omxplayer: hit error when transitioning from idle to executing\n");
            StopClock(pCurGraph->hClock);
            TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateIdle,
                                           OMX_TRUE, &g_oAppData);
            pCurGraph->oCompGraph.bWaitOnError = OMX_TRUE;
            TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateLoaded,
                                           OMX_TRUE, &g_oAppData);
            NvxDestroyWindow(pCurGraph->hWnd);
            // Deallocate everything
            FreeAllComponentsFromGraph(&pCurGraph->oCompGraph);
            pCurGraph->testtype = NONE;
            continue;
        }

        if(pCurGraph->playSpeed != 1.0)
        {
            if (pCurGraph->startTimeInSec)
            {
                SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
            }
            else
            {
                SetRateAndReplay(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, pCurGraph->playSpeed, &g_oAppData);
            }
        }

        if (AV_RECORDER == pCurGraph->testtype)
        {
            OMX_ERRORTYPE e = s_runAVRecorder(pCurGraph);
            if (OMX_ErrorNone != e)
            {
                sanitystatus++;
                NvOsDebugPrintf("OMX error returned by s_runAVRecorder 0x%x sanitystatus %d\n", e, sanitystatus);
            }

            // Wait for completion
            if (g_cameraParam.enableCameraCapture)
            {
                if (g_cameraParam.videoCapture && g_cameraParam.enableAudio)
                {
                    WaitForEnd2(&g_oAppData, 2);
                }
                else
                {
                    WaitForEnd2(&g_oAppData, g_cameraParam.nLoopsStillCapture);
                    g_cameraParam.nTSAppReceivesEndOfFile = g_oAppData.nTSBufferEOSReceived;

                }
            }

            if (g_cameraParam.enableCameraCapture && !g_cameraParam.videoCapture)
            {
                OMX_ERRORTYPE eErrorTmp2 = OMX_ErrorNone;
                OMX_INDEXTYPE eIndexParamFilename;
                NVX_PARAM_FILENAME oFilenameParam;
                char filename[1024];
                NvOsStatType stat = { 0, NvOsFileType_File };
                NvError err;

                // get the last file name for still capture
                {
                    eErrorTmp2 = OMX_GetExtensionIndex(g_FileReader, NVX_INDEX_PARAM_FILENAME,
                                                   &eIndexParamFilename);
                    TEST_ASSERT_NOTERROR(eErrorTmp2);

                    INIT_PARAM(oFilenameParam);
                    oFilenameParam.pFilename = filename;
                    eErrorTmp2 = OMX_GetConfig(g_FileReader, eIndexParamFilename, &oFilenameParam);
                    TEST_ASSERT_NOTERROR(eErrorTmp2);

                    NvOsDebugPrintf("Last image filename is %s\n", filename);
                }

                err = NvOsStat(filename, &stat);

                if (err != NvSuccess || stat.size == 0)
                {
                    NvOsDebugPrintf("File '%s' not found or empty\n", filename);
                    sanitystatus++;
                }
            }
        }
        pCurGraph->playState = OMX_StateExecuting;
    }

    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        g_Graphs[i].oCompGraph.bWaitOnError = OMX_TRUE;
    }

    /* In case of ONLY ULP Audio playback */
    if (bIsULP)
    {
        /* Minimum Suspend timeout value in the wince control panel is 1 min */
        playTimeInSec = 50; //9999; /* Taking value less than 1 min. */
    }

    while (atleastOneGraphActive)
    {
        // Reinit because of the loop
        for( i =0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
        {
            pCurGraph = &g_Graphs[i];
            if (pCurGraph->testtype != NONE)
                break;
        }


        // Wait for playTime
        if (pCurGraph->MobileTVDelay)
        {
            WaitForAnyEndOrTime(&g_oAppData, pCurGraph->MobileTVDelay * 1000);
        }
        else if(pCurGraph->bPlayTime)
        {
            float current_time;

            WaitForAnyEndOrTime(&g_oAppData, 1 * 1000);
            current_time = GetCurrentMediaTime(pCurGraph->hClock);
            NvOsDebugPrintf("Current time %f sec\n", current_time);
        }
        else
        {
waitforevent:
            if(!pCurGraph->oCompGraph.eos)
            {
                if (pCurGraph->testcmd == CRAZY_SEEK_TEST)
                {
                    WaitForAnyEndOrTime(&g_oAppData, playTimeInSec);
                    NvOsDebugPrintf("Wait for %d msec \n", playTimeInSec);
                }
                else
                {
                    WaitForAnyEndOrTime(&g_oAppData, playTimeInSec * 1000);
                    NvOsDebugPrintf("Wait for %d sec \n", playTimeInSec);
                }
            }
        }
        atleastOneGraphActive = 0;
        for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
        {
            pCurGraph = &g_Graphs[i];
            if (pCurGraph->testtype == NONE)
                continue;

            if (pCurGraph->oCompGraph.eos || pCurGraph->testcmd == STOP)
            {
                if (JPEG_DECODER == pCurGraph->testtype)
                {
                    NvOsDebugPrintf("jpeg decoder display for %d seconds\n", jpegDecDisplayTimeInSec);
                    NvOsSleepMS(jpegDecDisplayTimeInSec * 1000);
                }

                pCurGraph->oCompGraph.eos = 1;
                if (AV_RECORDER == pCurGraph->testtype)
                {
                    if (g_CameraProf.bProfile)
                    {
                        GetProfile(g_hCamera, &g_CameraProf);
                    }
                }

                if (PLAYER == pCurGraph->testtype)
                {
                    if (pCurGraph->oCompGraph.eErrorEvent != OMX_ErrorNone)
                    {
                        sanitystatus++;
                        NvOsDebugPrintf("OMX Error 0x%x in TestType %d sanitystatus %d\n", pCurGraph->oCompGraph.eErrorEvent, pCurGraph->testtype, sanitystatus);
                    }
                    pCurGraph->oCompGraph.eErrorEvent = OMX_ErrorNone;
                }
                NvOsDebugPrintf("- Stop playback\n");

#if !(NV_DEBUG)
                if (5 == pCurGraph->sanity)
                {
                    NVX_CONFIG_3GPMUXGETAVRECFRAMES AVRecFrames;
                    OMX_ERRORTYPE eError = OMX_ErrorNone;
                    OMX_INDEXTYPE eIndex;
                   // NvU32 AudioFrameRate, VideoFrameRate;

                    INIT_PARAM(AVRecFrames);
                    eError = OMX_GetExtensionIndex(g_FileReader, NVX_INDEX_CONFIG_3GPMUXGETAVRECFRAMES,
                                                   &eIndex);
                    TEST_ASSERT_NOTERROR(eError);
                    eError = OMX_GetConfig(g_FileReader, eIndex, &AVRecFrames);
                    TEST_ASSERT_NOTERROR(eError);

                    //AudioFrameRate = AVRecFrames.nAudioFrames/g_cameraParam.audioTime;
                    //VideoFrameRate = AVRecFrames.nVideoFrames/g_cameraParam.captureTime;

                    if((AVRecFrames.nAudioFrames < 420) || (AVRecFrames.nVideoFrames < 350)
                        || (AVRecFrames.nAudioFrames > 440) || (AVRecFrames.nVideoFrames > 400))
                    {
                        NVTEST_FAIL(&nvtestapp);
                        NvOsDebugPrintf("Frames Encoded Audio: %d \t Video: %d", AVRecFrames.nAudioFrames, AVRecFrames.nVideoFrames);
                        NvOsDebugPrintf("\n OMXPlayer: Failing Sanity 5, As see Performance Drop \n  ");
                    }

                }
                else if(6 == pCurGraph->sanity)
                {
                    NVX_CONFIG_3GPMUXGETAVRECFRAMES AVRecFrames;
                    OMX_ERRORTYPE eError = OMX_ErrorNone;
                    OMX_INDEXTYPE eIndex;

                    INIT_PARAM(AVRecFrames);
                    eError = OMX_GetExtensionIndex(g_FileReader, NVX_INDEX_CONFIG_3GPMUXGETAVRECFRAMES,
                                                   &eIndex);
                    TEST_ASSERT_NOTERROR(eError);
                    eError = OMX_GetConfig(g_FileReader, eIndex, &AVRecFrames);
                    TEST_ASSERT_NOTERROR(eError);

                    if((AVRecFrames.nVideoFrames < 350) || (AVRecFrames.nVideoFrames > 400))
                    {
                        NVTEST_FAIL(&nvtestapp);
                        NvOsDebugPrintf("Frames Encoded Video: %d", AVRecFrames.nVideoFrames);
                        NvOsDebugPrintf("\n OMXPlayer: Failing Sanity 6, As see Performance Drop \n  ");
                    }
                }
                else if (10 == pCurGraph->sanity || 12 == pCurGraph->sanity)
                {
                    NVX_CONFIG_3GPMUXGETAVRECFRAMES AVRecFrames;
                    OMX_ERRORTYPE eError = OMX_ErrorNone;
                    OMX_INDEXTYPE eIndex;
                   // NvU32 AudioFrameRate, VideoFrameRate;

                    INIT_PARAM(AVRecFrames);
                    eError = OMX_GetExtensionIndex(g_FileReader, NVX_INDEX_CONFIG_3GPMUXGETAVRECFRAMES,
                                                   &eIndex);
                    TEST_ASSERT_NOTERROR(eError);
                    eError = OMX_GetConfig(g_FileReader, eIndex, &AVRecFrames);
                    TEST_ASSERT_NOTERROR(eError);

                    //AudioFrameRate = AVRecFrames.nAudioFrames/g_cameraParam.audioTime;
                    //VideoFrameRate = AVRecFrames.nVideoFrames/g_cameraParam.captureTime;

                    if(((int)AVRecFrames.nAudioFrames < 40 * g_cameraParam.audioTime) || ((int)AVRecFrames.nVideoFrames < 25 * g_cameraParam.audioTime)
                        || ((int)AVRecFrames.nAudioFrames > 45 * g_cameraParam.audioTime) || ((int)AVRecFrames.nVideoFrames > 35 * g_cameraParam.audioTime))
                    {
                        NVTEST_FAIL(&nvtestapp);
                        NvOsDebugPrintf("Frames Encoded Audio: %d \t Video: %d", AVRecFrames.nAudioFrames, AVRecFrames.nVideoFrames);
                        NvOsDebugPrintf("\n OMXPlayer: Failing Sanity 10 or 12, As see performance doesn't fit in the range \n  ");
                    }
                }


                if (7 == pCurGraph->sanity)
                {
                    NVX_CONFIG_3GPMUXGETAVRECFRAMES AVRecFrames;
                    OMX_ERRORTYPE eError = OMX_ErrorNone;
                    OMX_INDEXTYPE eIndex;
                   // NvU32 AudioFrameRate, VideoFrameRate;

                    INIT_PARAM(AVRecFrames);
                    eError = OMX_GetExtensionIndex(g_FileReader, NVX_INDEX_CONFIG_3GPMUXGETAVRECFRAMES,
                                                   &eIndex);
                    TEST_ASSERT_NOTERROR(eError);
                    eError = OMX_GetConfig(g_FileReader, eIndex, &AVRecFrames);
                    TEST_ASSERT_NOTERROR(eError);

                    //AudioFrameRate = AVRecFrames.nAudioFrames/g_cameraParam.audioTime;
                    //VideoFrameRate = AVRecFrames.nVideoFrames/g_cameraParam.captureTime;

                    if((AVRecFrames.nAudioFrames < 420) || (AVRecFrames.nAudioFrames > 440))
                    {
                        NVTEST_FAIL(&nvtestapp);
                        NvOsDebugPrintf("Frames Encoded Audio: %d", AVRecFrames.nAudioFrames);
                        NvOsDebugPrintf("\n OMXPlayer: Failing Sanity 7, As see Performance Drop \n  ");
                    }

                }
#endif
                // Stop all components in the graph
                StopClock(pCurGraph->hClock);
                NvOsDebugPrintf("OMX Player: Clock Stopped\n");
                TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateIdle,
                                               OMX_TRUE, &g_oAppData);
                NvOsDebugPrintf("OMX Player: State set to OMX_StateIdle\n");
                if ((1 == pCurGraph->sanity || 4 == pCurGraph->sanity || 54 == pCurGraph->sanity) && pCurGraph->hRenderer)
                {
                    GetProfile(pCurGraph->hRenderer, &g_VidRendProf);

                    if (g_VidRendProf.nTotFrameDrops > maxdrops)
                    {
                        NvOsDebugPrintf("Framedrops over limit: %d > %d\n",
                                     g_VidRendProf.nTotFrameDrops, maxdrops);
                        sanitystatus++;
                    }
                    if (g_VidRendProf.nAvgFPS > pCurGraph->desiredfps + fpsrange ||
                        g_VidRendProf.nAvgFPS < pCurGraph->desiredfps - fpsrange)
                    {
                        NvOsDebugPrintf("Framerate out of range: %d vs %d\n",
                                     g_VidRendProf.nAvgFPS, pCurGraph->desiredfps);
                        sanitystatus++;
                    }
                }
                else if (2 == pCurGraph->sanity || 3 == pCurGraph->sanity || 5 == pCurGraph->sanity || 6 == pCurGraph->sanity || 7 == pCurGraph->sanity)
                {
                    NvOsStatType stat = { 0, NvOsFileType_File };
                    NvError err;

                    err = NvOsStat(pCurGraph->outfile, &stat);

                    if (err != NvSuccess || stat.size == 0)
                    {
                        NvOsDebugPrintf("File '%s' not found or empty\n", pCurGraph->outfile);
                        sanitystatus++;
                    }
                }

                if (PLAYER == pCurGraph->testtype)
                {
                    if (pCurGraph->oCompGraph.eErrorEvent != OMX_ErrorNone)
                    {
                        sanitystatus++;
                        NvOsDebugPrintf("OMX Error 0x%x in TestType %d sanitystatus %d\n", pCurGraph->oCompGraph.eErrorEvent, pCurGraph->testtype, sanitystatus);
                    }
                    pCurGraph->oCompGraph.eErrorEvent = OMX_ErrorNone;
                }

                TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateLoaded,
                                               OMX_TRUE, &g_oAppData);
                NvOsDebugPrintf("OMX Player: State set to OMX_StateLoaded\n");

                if (10 == pCurGraph->sanity || 12 == pCurGraph->sanity)
                {
                    NvOsStatType stat = { 0, NvOsFileType_File };
                    NvError err;

                    err = NvOsStat(pCurGraph->outfile, &stat);

                    if (err != NvSuccess || stat.size == 0)
                    {
                        NvOsDebugPrintf("File '%s' not found or empty\n", pCurGraph->outfile);
                        sanitystatus++;
                    }
                }

                NvxDestroyWindow(pCurGraph->hWnd);
                FreeAllComponentsFromGraph(&pCurGraph->oCompGraph);

                if ( AV_RECORDER == pCurGraph->testtype &&
                     ((g_cameraParam.enableCameraCapture && g_cameraParam.videoCapture) || g_cameraParam.enableAudio) )
                {
                    NvOsStatType stat = { 0, NvOsFileType_File };
                    NvError err;

                    err = NvOsStat(pCurGraph->outfile, &stat);

                    if (err != NvSuccess || stat.size == 0)
                    {
                        NvOsDebugPrintf("File '%s' not found or empty\n", pCurGraph->outfile);
                        sanitystatus++;
                    }
                }

                pCurGraph->testtype = NONE;
                continue;
            }
            else
            {
                if(pCurGraph->oCompGraph.mProcessBufferingEvent == NV_TRUE)
                {
                    //NvOsDebugPrintf("Omxplayer ::  BufferingPercent : %d\n",pCurGraph->oCompGraph.BufferingPercent);

                    if(pCurGraph->oCompGraph.ChangeStateToPause == NV_TRUE)
                    {
                         eError = PlaybackPause(pCurGraph, &g_oAppData);
                         TEST_ASSERT_NOTERROR(eError);
                         if(pCurGraph->testcmd == SEEK)
                         {
                             BufferingPause=NV_TRUE;
                             goto waitforevent;
                         }
                    }
                    else
                    {
                         eError = PlaybackResumeFromPause(pCurGraph, &g_oAppData);
                         TEST_ASSERT_NOTERROR(eError);
                         if(pCurGraph->testcmd == SEEK  && BufferingPause == NV_TRUE)
                         {
                             BufferingPause = NV_FALSE;
                             WaitForAnyEndOrTime(&g_oAppData, playTimeInSec * 1000);
                         }
                    }

                   pCurGraph->oCompGraph.mProcessBufferingEvent = NV_FALSE;
                 }

                atleastOneGraphActive = 1;
            }

            if (pCurGraph->testcmd == ULPTEST)
            {
                OMX_U32 nSize = 0;
                pCurGraph->bEnableFileCache = !pCurGraph->bEnableFileCache;
                NvOsDebugPrintf("- Setting mode to %s for another %d sec\n",
                             (pCurGraph->bEnableFileCache) ? "ULP" : "non-ULP",
                             playTimeInSec);
                if (pCurGraph->bEnableFileCache)
                    nSize = pCurGraph->nFileCacheSize;
                SetFileCacheSize(g_FileReader, nSize);
            }
            else if (pCurGraph->testcmd == PAUSE)
            {
                if (pCurGraph->playState == OMX_StateExecuting)
                {
                    NvOsDebugPrintf("- Pausing playback for another %d sec\n", playTimeInSec);
                    eError = PlaybackPause(pCurGraph, &g_oAppData);
                    TEST_ASSERT_NOTERROR(eError);
                }
                else if (pCurGraph->playState == OMX_StatePause)
                {
                    NvOsDebugPrintf("- Resuming playback for another %d sec\n", playTimeInSec);
                    eError = PlaybackResumeFromPause(pCurGraph, &g_oAppData);
                    TEST_ASSERT_NOTERROR(eError);
                }
            }
            else if (pCurGraph->testcmd == PAUSE_STOP)
            {
                //First Pause the playback
                NvOsDebugPrintf("- Pausing playback", playTimeInSec);
                eError = PlaybackPause(pCurGraph, &g_oAppData);
                TEST_ASSERT_NOTERROR(eError);
                //Now call stop
                pCurGraph->testcmd = STOP;
                playTimeInSec = 0;
            }
            else if (pCurGraph->testcmd == SEEK)
            {
                if (seekTimeOffset == 0)
                    NvOsDebugPrintf("- Seeking playback to 0 sec\n");
                else
                    NvOsDebugPrintf("- Seeking playback forward by %d sec \n", playTimeInSec);

                SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, seekTimeOffset*1000, &g_oAppData);
                NvOsDebugPrintf("- Resuming playback at %d sec for another %d sec\n", seekTimeOffset, playTimeInSec);
                seekTimeOffset+=(2*playTimeInSec);
            }
            else if(pCurGraph->testcmd == COMPLEX_SEEK)
            {
                static int cnt = 0;

                cnt++;
                if(cnt == 7)
                {
                    NvOsDebugPrintf("- Seeking playback to 0 sec\n");
                    SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, 0, &g_oAppData);
                    NvOsDebugPrintf("- Resuming playback at 0 sec\n");
                    cnt = 10;
                }
                else if(cnt % 7 == 0)
                {
                    NvOsDebugPrintf("- Seeking playback forward by %d sec\n", 20);
                    seekTimeOffset+=20;
                    SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, seekTimeOffset*1000, &g_oAppData);
                    NvOsDebugPrintf("- Resuming playback at %d sec\n", seekTimeOffset);
                }
                else if(cnt % 10 == 0)
                {
                    NvOsDebugPrintf("- Seeking playback backward by %d sec\n", 5);
                    seekTimeOffset-=5;
                    SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, seekTimeOffset*1000, &g_oAppData);
                    NvOsDebugPrintf("- Resuming playback at %d sec \n", seekTimeOffset);
                }
            }
            else if(pCurGraph->testcmd == CRAZY_SEEK_TEST)
            {
                static int cnt = 0;

                NvOsDebugPrintf("- CRAZY_SEEK_TEST cnt : %d\n", cnt);
                // Set Seed Value
                if (cnt == 0)
                {
                    //Seeding Random Generator
                    srand(pCurGraph->nCrazySeekSeed);
                }

                if (pCurGraph->nCrazySeekCount == (OMX_U32)cnt)
                {
                    NvOsDebugPrintf("CRAZY_SEEK_TEST -\n", seekTimeOffset);
                    pCurGraph->testcmd = STOP;
                    playTimeInSec = 0;
                    continue;
                }

                cnt++;

                seekTimeOffset = (rand() % pCurGraph->nFileDuration); // in sec

                NvOsDebugPrintf("CRAZY_SEEK_TEST Seeking playback to position %d sec\n", seekTimeOffset);

                SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, seekTimeOffset*1000, &g_oAppData);
            }
            else if (pCurGraph->testcmd == TRICKMODE)
            {
                float OrgPlaySpeed = pCurGraph->playSpeed;
                int testsection;

                for (testsection=0;testsection<3;testsection++)
                {
                    if (pCurGraph->oCompGraph.eos)
                    {
                        if (!pCurGraph->video)
                            continue;

                        NvOsDebugPrintf("- EOS in trick mode test, waiting for 3 sec -\r\n");
                        NvOsSleepMS(3000);

                        if (testsection == 1)
                        {
                            NvOsDebugPrintf("- Restart Playing in Fast Rewind Mode - \n");
                            pCurGraph->playSpeed = -OrgPlaySpeed;
                            SetRateAndReplay(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, pCurGraph->playSpeed, &g_oAppData);
                            WaitForAnyEndOrTime(&g_oAppData, pCurGraph->ffRewTimeInSec*1000);
                        }
                        else if (testsection == 2)
                        {
                            NvOsDebugPrintf("- Restart Playing in Normal Mode - \n");
                            pCurGraph->playSpeed = 1.0;
                            SetRateAndReplay(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, pCurGraph->playSpeed, &g_oAppData);
                            WaitForAnyEndOrTime(&g_oAppData, 10*1000);
                        }

                        if (pCurGraph->oCompGraph.eos)
                            continue;
                    }

                    if (testsection == 0)
                    {
                        NvOsDebugPrintf("- Playing in Normal Mode - \n");
                        pCurGraph->playSpeed = 1.0;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                        WaitForAnyEndOrTime(&g_oAppData, 10*1000);
                        if (pCurGraph->oCompGraph.eos)
                            continue;

                        NvOsDebugPrintf("- Playing in Fast Forward Mode - \n");
                        pCurGraph->playSpeed = OrgPlaySpeed;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                        WaitForAnyEndOrTime(&g_oAppData, pCurGraph->ffRewTimeInSec*1000);
                        if (pCurGraph->oCompGraph.eos)
                            continue;

                        NvOsDebugPrintf("- Playing in Normal Mode - \n");
                        pCurGraph->playSpeed = 1.0;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                        WaitForAnyEndOrTime(&g_oAppData, 10*1000);
                        if (pCurGraph->oCompGraph.eos)
                            continue;

                        NvOsDebugPrintf("- Playing in Fast Forward Mode - \n");
                        pCurGraph->playSpeed = OrgPlaySpeed;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                    }
                    else if (testsection == 1)
                    {
                        NvOsDebugPrintf("- Playing in Normal Mode - \n");
                        pCurGraph->playSpeed = 1.0;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                        WaitForAnyEndOrTime(&g_oAppData, 10*1000);
                        if (pCurGraph->oCompGraph.eos)
                            continue;

                        NvOsDebugPrintf("- Playing in Fast Rewind Mode - \n");
                        pCurGraph->playSpeed = -OrgPlaySpeed;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                    }
                    else
                    {
                        NvOsDebugPrintf("- Playing in Fast Forward Mode - \n");
                        pCurGraph->playSpeed = OrgPlaySpeed;
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                    }

                    // Play until the stream reaches to the end/start.
                    WaitForAnyEndOrTime(&g_oAppData, 0xFFFFFFFF);
                }

                // To end playback without waiting additional 10 seconds.
                NvOsSemaphoreSignal(g_oAppData.hMainEndEvent);
            }
            else if (pCurGraph->testcmd == RATECONTROLTEST)
            {
                playSpeed1 = 0.5;
                while(!pCurGraph->oCompGraph.eos)
                {
                    NvOsDebugPrintf("Set rate %f\n", playSpeed1);
                    SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, playSpeed1, &g_oAppData);
                    NvOsDebugPrintf("- Resuming playback for another %d sec\n", pCurGraph->ffRewTimeInSec);
                    WaitForAnyEndOrTime(&g_oAppData, pCurGraph->ffRewTimeInSec*1000);
                    playSpeed1 = playSpeed1*2;
                }

                playSpeed1 = 1.0;
                NvOsDebugPrintf("- Restart Playing in Normal Mode - \n");
                SetRateAndReplay(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, playSpeed1, &g_oAppData);
                WaitForAnyEndOrTime(&g_oAppData, pCurGraph->ffRewTimeInSec*1000);

                playSpeed1 = -1.0;
                NvOsDebugPrintf("- Restart Playing in Rewind Mode - \n");
                SetRateAndReplay(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, playSpeed1, &g_oAppData);
                WaitForAnyEndOrTime(&g_oAppData, pCurGraph->ffRewTimeInSec*1000);

                while(!pCurGraph->oCompGraph.eos)
                {
                    playSpeed1 = playSpeed1*2;
                    NvOsDebugPrintf("Set rate %f\n", playSpeed1);
                    SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, playSpeed1, &g_oAppData);
                    NvOsDebugPrintf("- Resuming playback for another %d sec\n", pCurGraph->ffRewTimeInSec);
                    WaitForAnyEndOrTime(&g_oAppData, pCurGraph->ffRewTimeInSec*1000);
                }

            }
            else if (pCurGraph->testcmd == TRICKMODESANITY)
            {
                int n =0;

                for (n=0;n<2;n++)
                {
                    if(n==0)
                    {
                        // fast forward mode = seeking to I frame Rate > 2
                        // fast playback/fast rewind = 0 < Rate <= 2
                        // A + V =start- Normal (2s) -seek backward(0s) -fast play (2x)(3s) - normal (3s) - fast forward (3x)(3s) -seek eos -fast rewind(4x)(3s) -eos -end
                        // A that has rate support = start- Normal (2s) -seek backward (0s)-normal (2s) - seek forward(4s) - fast playback (4x)(3s) - eos - end
                        // A without rate support = start- Normal (2s) -seek backward (0s) - normal (3s) - seek forward(5s)-normal -eos -end
                        pCurGraph->playSpeed = 1.0;
                        NvOsDebugPrintf("- Playing in Normal Mode - \n");
                        SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                        WaitForAnyEndOrTime(&g_oAppData, 2*1000);
                        if (pCurGraph->oCompGraph.eos)
                            continue;

                        NvOsDebugPrintf(" Seeking playback to 0 sec \n");
                        seekTimeOffset = 0;
                        SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, seekTimeOffset*1000, &g_oAppData);
                        NvOsDebugPrintf("- Resuming playback at %d sec\n", seekTimeOffset);

                        if(pCurGraph->sanity > 50) // Check if video file
                        {
                            NvOsDebugPrintf("- Playing in Fast Playback Mode - \n");
                            SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, playSpeed1, &g_oAppData);
                            WaitForAnyEndOrTime(&g_oAppData, 3*1000);
                            if (pCurGraph->oCompGraph.eos)
                                continue;

                            NvOsDebugPrintf("- Playing in Normal Mode - \n");
                            pCurGraph->playSpeed = 1.0;
                            SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                            WaitForAnyEndOrTime(&g_oAppData, 3*1000);
                            if (pCurGraph->oCompGraph.eos)
                                continue;

                            NvOsDebugPrintf("- Playing in Fast Forward Mode - \n");
                            SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, playSpeed2, &g_oAppData);
                            WaitForAnyEndOrTime(&g_oAppData, 3*1000);
                            if (pCurGraph->oCompGraph.eos)
                                continue;

                            NvOsDebugPrintf(" Seeking to EOS\n");
                            NvOsDebugPrintf(" Playing in Fast Rewind Mode - \n\n");
                            pCurGraph->playSpeed = -(playSpeed3);
                            SetRateAndReplay(&pCurGraph->oCompGraph, pCurGraph->hClock, pCurGraph->hFileParser, pCurGraph->playSpeed, &g_oAppData);

                            WaitForAnyEndOrTime(&g_oAppData, 3*1000);

                            NvOsDebugPrintf("- Playing in Normal Mode - \n");
                            pCurGraph->playSpeed = 1.0;
                            SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, pCurGraph->playSpeed, &g_oAppData);
                        }
                        else if(fastPlaySupported == 1) // Audio files that support fast and slow playback
                        {
                            NvOsDebugPrintf("- Playing in Normal Mode - \n");
                            WaitForAnyEndOrTime(&g_oAppData, 2*1000);
                            if (pCurGraph->oCompGraph.eos)
                                continue;

                            seekTimeOffset = 4;
                            NvOsDebugPrintf("Seek forward to %d sec - \n", seekTimeOffset);
                            SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, seekTimeOffset*1000, &g_oAppData);
                            NvOsDebugPrintf("- Resuming playback at %d sec\n", seekTimeOffset);

                            NvOsDebugPrintf("- Playing in Fast Playback Mode - \n");
                            SetRateOnGraph(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, playSpeed1, &g_oAppData);
                            WaitForAnyEndOrTime(&g_oAppData, 3*1000);
                            if (pCurGraph->oCompGraph.eos)
                                continue;
                       }
                       else  // Audio files that does not support fast and slow playback
                        {
                            // Rate is already set to 1.0. Play in normal mode for 3 secs
                            NvOsDebugPrintf("- Playing in Normal Mode - \n");
                            WaitForAnyEndOrTime(&g_oAppData, 3*1000);
                            if (pCurGraph->oCompGraph.eos)
                                continue;

                            seekTimeOffset = 5;
                            NvOsDebugPrintf("Seek forward to % sec - \n", seekTimeOffset);
                            SeekToTime(&pCurGraph->oCompGraph, pCurGraph->hClock, g_FileReader, seekTimeOffset*1000, &g_oAppData);
                            NvOsDebugPrintf("- Resuming playback at %d sec\n", seekTimeOffset);
                       }
                   }
                    else
                    {
                        // Play until the stream reaches to the end/start.
                        if(!pCurGraph->oCompGraph.eos)
                        {
                            NvOsDebugPrintf(" Waiting for EOS - \n\n");
                            WaitForAnyEndOrTime(&g_oAppData, 0xFFFFFFFF);
                            NvOsDebugPrintf(" Exited Wait Loop - \n\n");
                        }
                    }
                }

                // To end playback without waiting additional 10 seconds.
                NvOsSemaphoreSignal(g_oAppData.hMainEndEvent);
            }
            else if (pCurGraph->testcmd == CACHEINFO)
            {
                GetFileCacheInfo(g_FileReader);
            }
            if (pCurGraph->bCaptureFrame && pCurGraph->hRenderer)
            {
                OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
                OMX_INDEXTYPE eIndex;
                NVX_CONFIG_CAPTURERAWFRAME oRawFrame;
                NvU32 nFrameWidth, nFrameHeight;
                NvOsFileHandle hFile;

                NvOsDebugPrintf("- Capture current video frame - \n");
                pCurGraph->bCaptureFrame = OMX_FALSE;

                INIT_PARAM(oPortDef);
                oPortDef.nPortIndex = 0;
                eError = OMX_GetParameter(pCurGraph->hRenderer, OMX_IndexParamPortDefinition, &oPortDef);
                TEST_ASSERT_NOTERROR(eError);

                nFrameWidth = oPortDef.format.video.nFrameWidth;
                nFrameHeight = oPortDef.format.video.nFrameHeight;

                eError = OMX_GetExtensionIndex(pCurGraph->hRenderer, NVX_INDEX_CONFIG_CAPTURERAWFRAME, &eIndex);
                TEST_ASSERT_NOTERROR(eError);

                INIT_PARAM(oRawFrame);
                oRawFrame.nBufferSize = nFrameWidth * nFrameHeight * 3 / 2;
                oRawFrame.pBuffer = (NvU8 *)NvOsAlloc(oRawFrame.nBufferSize);
                eError = OMX_GetConfig(pCurGraph->hRenderer, eIndex, &oRawFrame);
                TEST_ASSERT_NOTERROR(eError);

                if (NvSuccess == NvOsFopen(pCurGraph->capturefile, NVOS_OPEN_CREATE | NVOS_OPEN_WRITE, &hFile))
                {
                    NvOsFwrite(hFile, (void*)oRawFrame.pBuffer, oRawFrame.nBufferSize);
                    NvOsFclose(hFile);

                    NvOsDebugPrintf("Captured video frame is saved as %s\n", pCurGraph->capturefile);
                }
                else
                {
                    NvOsDebugPrintf("Error opening file %s for write\n", pCurGraph->capturefile);
                }

                if (oRawFrame.pBuffer != NULL)
                {
                    NvOsFree(oRawFrame.pBuffer);
                }
            }

        }
    }

    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        pCurGraph = &g_Graphs[i];

        if (OMX_ErrorNone != pCurGraph->oCompGraph.eErrorEvent)
        {
            NvOsDebugPrintf("omxplayer received error event\n");
            sanitystatus++;
        }

        if (pCurGraph->testtype == NONE)
            continue;

        TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateLoaded,
            OMX_TRUE, &g_oAppData);

        // Deallocate everything
        FreeAllComponentsFromGraph(&pCurGraph->oCompGraph);
    }

omx_test_end_cleanup:

    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        pCurGraph = &g_Graphs[i];
        NvOsSemaphoreDestroy(pCurGraph->oCompGraph.hTimeEvent);
        NvOsSemaphoreDestroy(pCurGraph->oCompGraph.hCameraStillCaptureReady);
        NvOsSemaphoreDestroy(pCurGraph->oCompGraph.hCameraHalfPress3A);
        if (pCurGraph->delinfile)
            NvOsFree(pCurGraph->infile);
        pCurGraph->infile = NULL;
        pCurGraph->delinfile = OMX_FALSE;
    }

    g_cameraParam.nTSAppEnd = NvOsGetTimeUS() * 10;

    if (g_CameraProf.bProfile)
    {
        s_printProfileNumbers();
    }

    videoloop++;
    if (videoloop < numPlaysDesired)
    {
        NvOsDebugPrintf("Loop %d of %d\n", videoloop, numPlaysDesired);
        goto begin_video_loop;
    }
    if(g_drmlicenseAcquired == OMX_TRUE && (g_drmlicenseiterations < 1))
    {
        g_drmlicenseAcquired = OMX_FALSE;
        sanitystatus--;
        g_drmlicenseiterations++;
        goto begin_video_loop;
     }

omx_test_end:

//  silence the compiler warning for the variable only used for windows build.
    if (HDMIdisplay)
        HDMIdisplay = 0;

    if (TVdisplay)
        TVdisplay = 0;

    NvOMXSampleDeinit(&g_oAppData);
    SampleDeinitPlatform();

    NvOsFree(line);
    NvOsFree(largv);

    NVTEST_RUN_ARG(&nvtestapp, omx_sanity, sanitystatus);
    NVTEST_RESULT(&nvtestapp);
}


NvError OMX_Fill_ExifIfd(NvXExifArgs *pExifArgs, OMX_STRING ExifFile)
{

    NvU64 LastPosition, CurrPosition = 0;
    NvU32 bytes_read;
    NvS8 *file_params;
    NvU32 i,j;
    NvU32 isTag=1;
    NvOsFileHandle hExifFileHandle;
    NvError err = NvSuccess;

    err = NvOsFopen(ExifFile, NVOS_OPEN_READ, &hExifFileHandle);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("Test: ExifFilename open failed\n %s", ExifFile);
        return err;
    }

    NvOsFseek(hExifFileHandle, 0, NvOsSeek_End);
    NvOsFtell(hExifFileHandle, &LastPosition);
    NvOsFseek(hExifFileHandle, 0, NvOsSeek_Set);

    file_params = NvOsAlloc((NvU32)(LastPosition + 1));
    if (NULL == file_params)
    {
        NvOsDebugPrintf("omxplayer: Failed to Allocate memory for Commands \n");
        goto CleanUp;
    }

    NvOsFread(hExifFileHandle, file_params, (NvU32)LastPosition, &bytes_read);
    file_params[LastPosition] = '\0';

    if (bytes_read == LastPosition)
    {
        i = 0;
        pExifArgs->Argv[0][0] = '\0';
        pExifArgs->Argc = 1;
        j = 0;
        isTag = (pExifArgs->Argc%2);
        while (CurrPosition <= LastPosition)
        {

            if ((!( (file_params[CurrPosition] == ' ') && (isTag != 0) )) &&
                (file_params[CurrPosition] != '\n') && (file_params[CurrPosition] != 0xD) )
            {
                pExifArgs->Argv[pExifArgs->Argc][j++] = file_params[CurrPosition];
            }
            else if( (file_params[CurrPosition] == '\n') || (file_params[CurrPosition] == ' ') )
            {
                if (j != 0)
                    pExifArgs->Argv[pExifArgs->Argc][j++] = '\0';
                j = 0;
                pExifArgs->Argc++;
                isTag = (pExifArgs->Argc%2);
            }
            CurrPosition++;
        }
    }
    else
    {
        NvOsDebugPrintf("omxplayer: Illegal option on command line.\n");
        goto CleanUp;
    }
CleanUp:
    NvOsFclose(hExifFileHandle);
    NvOsFree(file_params);
    return err;
}



void
Parse_APP1_Args(
                NvU32 argc,
                char** argv,
                NVX_CONFIG_ENCODEEXIFINFO *pExifInfo,
                NVX_CONFIG_ENCODEGPSINFO *pGPSInfo)
{
    NvU32 i, k;
    NvU8 CharCode[] = {0x41, 0x53, 0x43, 0x49, 0x49, 0x00, 0x00, 0x00};

    for ( i=1; i<argc; i++)
    {
        if(pExifInfo != NULL)
        {
            if(!strcmp(argv[i], "--ImageDescription"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->ImageDescription, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--Make"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->Make, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--Model"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->Model, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--Copyright"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->Copyright, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--Artist"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->Artist, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--Software"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->Software, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--DateTime"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->DateTime, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--DateTimeOriginal"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->DateTimeOriginal, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--DateTimeDigitized"))
            {
                i++;
                NvOsStrncpy((char *)pExifInfo->DateTimeDigitized, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--filesource"))
            {
                i++;
                pExifInfo->filesource = atoi(argv[i]);
            }
            else if (!strcmp(argv[i], "--UserComment"))
            {
                i++;
                NvOsMemcpy(pExifInfo->UserComment, CharCode, CHARACHTER_CODE_LENGTH);
                NvOsStrncpy((char *)&pExifInfo->UserComment[8], argv[i], NvOsStrlen(argv[i]) + 1);
            }
        }
        /*
        *GPS Info
        */
        if(pGPSInfo != NULL)
        {
            if(!strcmp(argv[i], "--BitMapInfo"))
            {
                i++;
                pGPSInfo->GPSBitMapInfo = atoi(argv[i]);
            }
            else if(!strcmp(argv[i], "--VersionID"))
            {
                i++;
                pGPSInfo->GPSVersionID = atoi(argv[i]);
            }
            else if (!strcmp(argv[i], "--LatitudeRef"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSLatitudeRef, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--LatitudeNr"))
            {
                NvU32 CurrPosition, j;
                char number[32];
                k = j = CurrPosition = 0;
                i++;
                while(CurrPosition <= strlen(argv[i]))
                {
                    if ( (argv[i][CurrPosition] != ' ') && (argv[i][CurrPosition] != '\0') &&
                        (argv[i][CurrPosition] != '\n') )
                    {
                        number[j++] = argv[i][CurrPosition];
                    }
                    else if( !((argv[i][CurrPosition] != ' ') && (j==0)) )
                    {
                        number[j] = '\0';
                        j = 0;
                        pGPSInfo->GPSLatitudeNumerator[k++] = atoi(number);
                        if (argv[i][CurrPosition] == '\n')
                        {
                            break;
                        }
                    }
                    CurrPosition++;
                }
            }
            else if(!strcmp(argv[i], "--LatitudeDr"))
            {
                NvU32 CurrPosition, j;
                char number[32];
                k = j = CurrPosition = 0;
                i++;

                while(CurrPosition <= strlen(argv[i]))
                {
                    if ( (argv[i][CurrPosition] != ' ') && (argv[i][CurrPosition] != '\0') &&
                        (argv[i][CurrPosition] != '\n'))
                    {
                        number[j++] = argv[i][CurrPosition];
                    }
                    else if( !((argv[i][CurrPosition] != ' ') && (j==0)) )
                    {
                        number[j] = '\0';
                        j = 0;
                        pGPSInfo->GPSLatitudeDenominator[k++] = atoi(number);
                        if (argv[i][CurrPosition] == '\n')
                        {
                            break;
                        }
                    }
                    CurrPosition++;
                }
            }
            else if (!strcmp(argv[i], "--LongitudeRef"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSLongitudeRef, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--LongitudeNr"))
            {
                NvU32 CurrPosition, j;
                char number[32];
                k = j = CurrPosition = 0;
                i++;

                while(CurrPosition <= strlen(argv[i]))
                {
                    if ( (argv[i][CurrPosition] != ' ') && (argv[i][CurrPosition] != '\0') &&
                        (argv[i][CurrPosition] != '\n'))
                    {
                        number[j++] = argv[i][CurrPosition];
                    }
                    else if( !((argv[i][CurrPosition] != ' ') && (j==0)) )
                    {
                        number[j] = '\0';
                        j = 0;
                        pGPSInfo->GPSLongitudeNumerator[k++] = atoi(number);
                        if(argv[i][CurrPosition] == '\n')
                        {
                            break;
                        }
                    }
                    CurrPosition++;
                }

            }
            else if(!strcmp(argv[i], "--LongitudeDr"))
            {
                NvU32 CurrPosition, j;
                char number[32];
                k = j = CurrPosition = 0;
                i++;

                while(CurrPosition <= strlen(argv[i]))
                {
                    if ( (argv[i][CurrPosition] != ' ') && (argv[i][CurrPosition] != '\0') &&
                        (argv[i][CurrPosition] != '\n'))
                    {
                        number[j++] = argv[i][CurrPosition];
                    }
                    else if( !((argv[i][CurrPosition] != ' ') && (j==0)) )
                    {
                        number[j] = '\0';
                        j = 0;
                        pGPSInfo->GPSLongitudeDenominator[k++] = atoi(number);
                        if(argv[i][CurrPosition] == '\n')
                        {
                            break;
                        }
                    }
                    CurrPosition++;
                }
            }
            else if(!strcmp(argv[i], "--AltitudeRef"))
            {
                i++;
                pGPSInfo->GPSAltitudeRef = atoi(argv[i]);
            }
            else if(!strcmp(argv[i], "--AltitudeNr"))
            {
                i++;
                pGPSInfo->GPSAltitudeNumerator = atoi(argv[i]);
            }
            else if(!strcmp(argv[i], "--AltitudeDr"))
            {
                i++;
                pGPSInfo->GPSAltitudeDenominator = atoi(argv[i]);
            }
            else if(!strcmp(argv[i], "--TimeStampNr"))
            {
                NvU32 CurrPosition, j;
                char number[32];
                k = j = CurrPosition = 0;
                i++;

                while(CurrPosition <= strlen(argv[i]))
                {
                    if ( (argv[i][CurrPosition] != ' ') && (argv[i][CurrPosition] != '\0') &&
                        (argv[i][CurrPosition] != '\n') )
                    {
                        number[j++] = argv[i][CurrPosition];
                    }
                    else if( !((argv[i][CurrPosition] != ' ') && (j==0)) )
                    {
                        number[j] = '\0';
                        j = 0;
                        pGPSInfo->GPSTimeStampNumerator[k++] = atoi(number);
                        if(argv[i][CurrPosition] == '\n')
                        {
                            break;
                        }
                    }
                    CurrPosition++;
                }
            }
            else if(!strcmp(argv[i], "--TimeStampDr"))
            {
                NvU32 CurrPosition, j;
                char number[32];
                k = j = CurrPosition = 0;
                i++;

                while(CurrPosition <= strlen(argv[i]))
                {
                    if ( (argv[i][CurrPosition] != ' ') && (argv[i][CurrPosition] != '\0') &&
                        (argv[i][CurrPosition] != '\n') )
                    {
                        number[j++] = argv[i][CurrPosition];
                    }
                    else if( !((argv[i][CurrPosition] != ' ') && (j==0)) )
                    {
                        number[j] = '\0';
                        j = 0;
                        pGPSInfo->GPSTimeStampDenominator[k++] = atoi(number);
                        if(argv[i][CurrPosition] == '\n')
                        {
                            break;
                        }
                    }
                    CurrPosition++;
                }
            }
            else if (!strcmp(argv[i], "--Satellites"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSSatellites, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if (!strcmp(argv[i], "--Status"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSStatus, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if (!strcmp(argv[i], "--MeasureMode"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSMeasureMode, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--DOPNr"))
            {
                i++;
                pGPSInfo->GPSDOPNumerator = atoi(argv[i]);
            }
            else if(!strcmp(argv[i], "--DOPDr"))
            {
                i++;
                pGPSInfo->GPSDOPDenominator = atoi(argv[i]);
            }
            else if (!strcmp(argv[i], "--ImgDirectionRef"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSImgDirectionRef, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if(!strcmp(argv[i], "--ImgDirectionNr"))
            {
                i++;
                pGPSInfo->GPSImgDirectionNumerator = atoi(argv[i]);
            }
            else if(!strcmp(argv[i], "--ImgDirectionDr"))
            {
                i++;
                pGPSInfo->GPSImgDirectionDenominator = atoi(argv[i]);
            }
            else if (!strcmp(argv[i], "--MapDatum"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSMapDatum, argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if (!strcmp(argv[i], "--ProcessingMethod"))
            {
                i++;
                NvOsMemcpy(pGPSInfo->GPSProcessingMethod, CharCode, CHARACHTER_CODE_LENGTH);
                NvOsStrncpy((char *)&pGPSInfo->GPSProcessingMethod[8], argv[i], NvOsStrlen(argv[i]) + 1);
            }
            else if (!strcmp(argv[i], "--DateStamp"))
            {
                i++;
                NvOsStrncpy((char *)pGPSInfo->GPSDateStamp, argv[i], NvOsStrlen(argv[i]) + 1);
            }
        }
    }
}
