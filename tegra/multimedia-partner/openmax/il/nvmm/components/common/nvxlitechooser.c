#include "NvxCompReg.h"
#include "nvmm_util.h"

static int UsingNvMMLite(int bNeedAVP)
{
    return NvMMIsUsingNewAVP();
}

OMX_ERRORTYPE NvxMp3WrapperInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(0))
        return NvxMp3LiteDecoderInit(hComponent);
    return NvxMp3DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxWrapperAacEncoderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(0))
        return NvxAacLiteEncoderInit(hComponent);
    return NvxAacEncoderInit(hComponent);
}

OMX_ERRORTYPE NvxH264WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteH264DecoderInit(hComponent);
    return NvxH264DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxH264WrapperInitSecure(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteH264DecoderSecureInit(hComponent);
    return NvxH264DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxH264WrapperInitLowLatency(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteH264DecoderLowLatencyInit(hComponent);
    return NvxH264DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxH264ExtWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteH264ExtDecoderInit(hComponent);
    return NvxH264ExtDecoderInit(hComponent);
}

OMX_ERRORTYPE NvxVc1WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteVc1DecoderInit(hComponent);
    return NvxVc1DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxVP8WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteVP8DecoderInit(hComponent);
    return NvxVP8DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxMpeg2WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteMpeg2DecoderInit(hComponent);
    return NvxMpeg2DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxMp4WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteMp4DecoderInit(hComponent);
    return NvxMp4DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxMp4ExtWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteMp4ExtDecoderInit(hComponent);
    return NvxMp4ExtDecoderInit(hComponent);
}

OMX_ERRORTYPE NvxH263WrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteH263DecoderInit(hComponent);
    return NvxH263DecoderInit(hComponent);
}

OMX_ERRORTYPE NvxEAacPlusDecoderWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(0))
        return NvxLiteEAacPlusDecoderInit(hComponent);
    return NvxEAacPlusDecoderInit(hComponent);
}

OMX_ERRORTYPE NvxJpegEncoderWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteJpegEncoderInit(hComponent);
    return NvxJpegEncoderInit(hComponent);
}

OMX_ERRORTYPE NvxWmaDecoderWrapperInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(0))
        return NvxLiteWmaDecoderInit(hComponent);
    return NvxWmaDecoderInit(hComponent);
}

OMX_ERRORTYPE NvxWrapperMJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
        return NvxLiteMJpegDecoderInit(hComponent);
    return NvxMJpegVideoDecoderInit(hComponent);
}

OMX_ERRORTYPE NvxWrapperJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    if (UsingNvMMLite(1))
       return NvxLiteJpegDecoderInit(hComponent);
    return NvxJpegDecoderInit(hComponent);
}
