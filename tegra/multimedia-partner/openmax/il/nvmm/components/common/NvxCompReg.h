/* Copyright (c) 2006-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxCompReg.h
 *
 *  NVIDIA's lists of components for OMX Component Registration.
 */

#ifndef _NVX_COMP_REG_H_
#define _NVX_COMP_REG_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_Core.h>
#include <stdlib.h>

/* Extern declarations for component initialization functions */
/* Add a declaration for each component to be declared        */
OMX_ERRORTYPE NvxRawFileReaderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAudioFileReaderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileReaderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxImageFileReaderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLargeVideoFileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoSchedulerInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxClockComponentInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxRawFileWriterInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAudioFileWriterInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileWriterInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxImageFileWriterInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxImageSequenceFileWriterInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp4FileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp3FileReaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp3FileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVorbisDecoderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMpeg2DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp4DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp4ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH263DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAacDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAdtsDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxEAacPlusDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxBsacDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp3DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWavDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWmaDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWmaProDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWmaLosslessDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrWBDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAudioRendererInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH264DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH264ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVc1DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVP8DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoOverlayRGB565Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoOverlayARGB8888Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoOverlayYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoHdmiYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoLvdsYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoCrtYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoHdmiARGBInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoLvdsARGBInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoCrtARGBInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoTvoutYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoSecondaryYUV420Init(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxCameraInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAudioCapturerInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxJpegEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMJpegVideoDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAacEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrnbEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrwbEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWavEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxilbcEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileReaderPacketHeaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoFileWriterPacketHeaderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAsfParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMkvParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Nvx3gpParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAviParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxOggParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWavParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAacParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAwbParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMpsParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxSuperParserInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp2DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxFlvParserInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVidExtractInit(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWavFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAmrFileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteJpegEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp3LiteDecoderInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAacLiteEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH264DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH264ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH264DecoderSecureInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH264DecoderLowLatencyInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH265DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteVc1DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteVC1DecoderSecureInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteVP8DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteMpeg2DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteMp4DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteMp4ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH263DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteMJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteEAacPlusDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteWmaDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLoopbackRendererInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp3WrapperInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH264WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH264WrapperInitSecure(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH264WrapperInitLowLatency(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH264ExtWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVc1WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVP8WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMpeg2WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp4WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxMp4ExtWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxH263WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxEAacPlusDecoderWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWrapperAacEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH264EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteH263EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteMpeg4EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteVP8EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxJpegEncoderWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWmaDecoderWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWrapperMJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxWrapperJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxLiteJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxBypassDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent);
#ifdef BUILD_GOOGLETV
OMX_ERRORTYPE NvxHdmiInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVideoPostProcessorInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxVidSchedulerInit(OMX_IN  OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxAc3AudioDecoderInit(OMX_IN OMX_HANDLETYPE hComponent);
#endif

/* List of name/init function pairs as a #define.  This can be used to define your own static registration list
   that includes all NVIDIA supplied components like this:
    OMX_COMPONENTREGISTERTYPE OMX_ComponentRegistered[] =
    {
       { "OMX.someothervender.component", &SomeOtherVenderComponentInit },
       { "OMX.anothervender.component", &AnotherVenderComponentInit },
       NVX_ALL_NVIDIA_COMPONENTS,
       { NULL, NULL }
    };

    The default static registration list in the library only contains NVIDIA components.
 */
#define NVX_COMMON_NVIDIA_COMPONENTS \
  { "OMX.Nvidia.raw.read", &NvxRawFileReaderInit }, \
  { "OMX.Nvidia.audio.read", &NvxAudioFileReaderInit }, /* variation on NvxRawFileReader with audio port */ \
  { "OMX.Nvidia.video.read", &NvxVideoFileReaderInit }, /* variation on NvxRawFileReader with video port */ \
  { "OMX.Nvidia.image.read", &NvxImageFileReaderInit }, /* variation on NvxRawFileReader with image port */ \
  { "OMX.Nvidia.video.read.large", &NvxLargeVideoFileReaderInit }, /* variation on NvxRawFileReader with larger data buffers */ \
  { "OMX.Nvidia.video.scheduler", &NvxVideoSchedulerInit }, \
  { "OMX.Nvidia.clock.component", &NvxClockComponentInit }, \
  { "OMX.Nvidia.raw.write", &NvxRawFileWriterInit }, \
  { "OMX.Nvidia.audio.write", &NvxAudioFileWriterInit }, \
  { "OMX.Nvidia.video.write", &NvxVideoFileWriterInit }, \
  { "OMX.Nvidia.image.write", &NvxImageFileWriterInit }, \
  { "OMX.Nvidia.imagesequence.write", &NvxImageSequenceFileWriterInit }, \
  { "OMX.Nvidia.vidhdr.write", &NvxVideoFileWriterPacketHeaderInit }, \
  { "OMX.Nvidia.vidhdr.read", &NvxVideoFileReaderPacketHeaderInit }, \
  { "OMX.Nvidia.mp4.write", &NvxMp4FileWriterInit}, \
  { "OMX.Nvidia.wav.write", &NvxWavFileWriterInit}, \
  { "OMX.Nvidia.amr.write", &NvxAmrFileWriterInit}

#define NVX_NVMM_COMPONENTS \
  { "OMX.Nvidia.h264.encoder", &NvxLiteH264EncoderInit }, \
  { "OMX.Nvidia.aac.decoder", &NvxEAacPlusDecoderWrapperInit }, \
  { "OMX.Nvidia.adts.decoder", &NvxAdtsDecoderInit }, \
  { "OMX.Nvidia.bsac.decoder", &NvxBsacDecoderInit }, \
  { "OMX.Nvidia.wav.decoder", &NvxWavDecoderInit }, \
  { "OMX.Nvidia.wma.decoder", &NvxWmaDecoderWrapperInit }, \
  { "OMX.Nvidia.wmapro.decoder", &NvxWmaProDecoderInit }, \
  { "OMX.Nvidia.wmalossless.decoder", &NvxWmaLosslessDecoderInit }, \
  { "OMX.Nvidia.vorbis.decoder", &NvxVorbisDecoderInit }, \
  { "OMX.Nvidia.amr.decoder", &NvxAmrDecoderInit }, \
  { "OMX.Nvidia.amrwb.decoder", &NvxAmrWBDecoderInit }, \
  { "OMX.Nvidia.jpeg.encoder", &NvxJpegEncoderWrapperInit }, \
  { "OMX.Nvidia.jpeg.decoder", &NvxWrapperJpegDecoderInit }, \
  { "OMX.Nvidia.mjpeg.decoder", &NvxWrapperMJpegDecoderInit }, \
  { "OMX.Nvidia.std.iv_renderer.overlay.rgb565", &NvxVideoOverlayRGB565Init }, \
  { "OMX.Nvidia.render.overlay.argb8888", &NvxVideoOverlayARGB8888Init }, \
  { "OMX.Nvidia.std.iv_renderer.overlay.yuv420", &NvxVideoOverlayYUV420Init }, \
  { "OMX.Nvidia.render.hdmi.overlay.yuv420", NvxVideoHdmiYUV420Init }, \
  { "OMX.Nvidia.render.lvds.overlay.yuv420", NvxVideoLvdsYUV420Init }, \
  { "OMX.Nvidia.render.crt.overlay.yuv420", NvxVideoCrtYUV420Init }, \
  { "OMX.Nvidia.render.hdmi.overlay.argb8888", NvxVideoHdmiARGBInit }, \
  { "OMX.Nvidia.render.lvds.overlay.argb8888", NvxVideoLvdsARGBInit }, \
  { "OMX.Nvidia.render.crt.overlay.argb8888", NvxVideoCrtARGBInit }, \
  { "OMX.Nvidia.render.tvout.overlay.yuv420", NvxVideoTvoutYUV420Init }, \
  { "OMX.Nvidia.render.secondary.overlay.yuv420", NvxVideoSecondaryYUV420Init }, \
  { "OMX.Nvidia.audio.render", &NvxAudioRendererInit }, \
  { "OMX.Nvidia.amr.encoder", &NvxAmrnbEncoderInit }, \
  { "OMX.Nvidia.amrwb.encoder", &NvxAmrwbEncoderInit }, \
  { "OMX.Nvidia.aac.encoder", &NvxWrapperAacEncoderInit }, \
  { "OMX.Nvidia.mp4.encoder", &NvxLiteMpeg4EncoderInit }, \
  { "OMX.Nvidia.h263.encoder", &NvxLiteH263EncoderInit }, \
  { "OMX.Nvidia.ilbc.encoder", &NvxilbcEncoderInit }, \
  { "OMX.Nvidia.camera", &NvxCameraInit }, \
  { "OMX.Nvidia.asf.read", &NvxAsfParserInit }, \
  { "OMX.Nvidia.mp4.read", &Nvx3gpParserInit },\
  { "OMX.Nvidia.mkv.read", &NvxMkvParserInit }, \
  { "OMX.Nvidia.avi.read", &NvxAviParserInit }, \
  { "OMX.Nvidia.mps.read", &NvxMpsParserInit }, \
  { "OMX.Nvidia.mp3.read", &NvxMp3FileReaderInit }, \
  { "OMX.Nvidia.ogg.read", &NvxOggParserInit }, \
  { "OMX.Nvidia.wav.read", &NvxWavParserInit }, \
  { "OMX.Nvidia.aac.read", &NvxAacParserInit }, \
  { "OMX.Nvidia.amr.read", &NvxAmrParserInit }, \
  { "OMX.Nvidia.awb.read", &NvxAwbParserInit }, \
  { "OMX.Nvidia.reader", &NvxSuperParserInit }, \
  { "OMX.Nvidia.mp2.decoder", &NvxMp2DecoderInit }, \
  { "OMX.Nvidia.video.extractor", &NvxVidExtractInit }, \
  { "OMX.Nvidia.wav.encoder", &NvxWavEncoderInit }, \
  { "OMX.Nvidia.render.loopback", &NvxLoopbackRendererInit }, \
  { "OMX.Nvidia.mp3.decoder", &NvxMp3WrapperInit }, \
  { "OMX.Nvidia.mp4ext.decode", &NvxMp4ExtWrapperInit }, \
  { "OMX.Nvidia.mp4.decode", &NvxMp4WrapperInit }, \
  { "OMX.Nvidia.h263.decode", &NvxH263WrapperInit }, \
  { "OMX.Nvidia.mpeg2v.decode",&NvxMpeg2WrapperInit }, \
  { "OMX.Nvidia.h264ext.decode", &NvxH264ExtWrapperInit }, \
  { "OMX.Nvidia.h264.decode", &NvxH264WrapperInit }, \
  { "OMX.Nvidia.h264.decode.secure", &NvxH264WrapperInitSecure }, \
  { "OMX.Nvidia.h264.decode.low.latency", &NvxH264WrapperInitLowLatency }, \
  { "OMX.Nvidia.h265.decode", &NvxLiteH265DecoderInit }, \
  { "OMX.Nvidia.vc1.decode", &NvxVc1WrapperInit }, \
  { "OMX.Nvidia.vc1.decode.secure", &NvxLiteVC1DecoderSecureInit }, \
  { "OMX.Nvidia.eaacp.decoder", &NvxEAacPlusDecoderWrapperInit }, \
  { "OMX.Nvidia.bypass.decoder", &NvxBypassDecoderInit }

#define NVX_NEW_NVMM_COMPONENTS \
  { "OMX.Nvidia.vp8.encoder", &NvxLiteVP8EncoderInit }, \
  { "OMX.Nvidia.vp8.decode", &NvxVP8WrapperInit }

#ifdef BUILD_GOOGLETV
#define NVX_GOOGLETV_COMPONENTS \
  { "OMX.Nvidia.video.PostProcessor", &NvxVideoPostProcessorInit }, \
  { "OMX.Nvidia.vid.Scheduler", &NvxVidSchedulerInit }, \
  { "OMX.Nvidia.hdmi", &NvxHdmiInit }, \
  { "OMX.Nvidia.audio.capturer", &NvxAudioCapturerInit }, \
  { "OMX.Nvidia.ac3.decode", &NvxAc3AudioDecoderInit }, \
  { "OMX.Nvidia.eac3.decode", &NvxAc3AudioDecoderInit }

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

