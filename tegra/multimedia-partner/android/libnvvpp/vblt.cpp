/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>

#include <cutils/properties.h>
#include <utils/Log.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nv.h>
#include <EGL/eglext_nvinternal.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2ext_nv.h>
#include <media/stagefright/foundation/hexdump.h>

#include "NvxHelpers.h"
#include "vblt.h"
#include "nvutil.h"

using namespace android;

#define NUM_PLANES 2

#define Y_PLANE 0
#define UV_PLANE 1

//#define DEBUG

#ifdef DEBUG
#define PRINT_GL_ERROR() \
         do{ \
            GLuint gErr = glGetError(); \
            if(GL_NO_ERROR != gErr) { \
                ALOGE("VBLT: GL Error: %x [%d]", gErr, __LINE__); \
            } \
         }while(0)

#define PRINT_EGL_ERROR() \
         do{  \
            GLuint egErr = eglGetError(); \
            if(0x3000 != egErr) { \
                ALOGE("VBLT: EGL Error: %x [%d]", egErr, __LINE__); \
            } \
         }while(0)
#else
#define PRINT_GL_ERROR()
#define PRINT_EGL_ERROR()
#endif

static const char FILENAME_VSY[] ="/system/etc/firmware/InvertY.vs";
static const char FILENAME_FSY[] ="/system/etc/firmware/InvertY.fs";
static const char FILENAME_VSUV[] ="/system/etc/firmware/InvertUV.vs";
static const char FILENAME_FSUV[] ="/system/etc/firmware/InvertUV.fs";

static FILE* vsyfd = NULL;
static FILE* fsyfd = NULL;
static FILE* vsuvfd = NULL;
static FILE* fsuvfd = NULL;

static char* vsysrc = NULL;
static char* fsysrc = NULL;
static char* vsuvsrc = NULL;
static char* fsuvsrc = NULL;


static EGLConfig chooseConfig(EGLDisplay dpy) {
    EGLConfig cfg;
    EGLint num_cfgs;

    const EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };


    if (!eglChooseConfig(dpy, attrs, &cfg, 1, &num_cfgs) || (num_cfgs != 1)) {
        ALOGE("VBLT: Failed to choose EGL config: error 0x%x", eglGetError());
        PRINT_EGL_ERROR();
        return NULL;
    }

    return cfg;
}

static void InitTexture(GLuint texid, uint32_t w, uint32_t h)
{
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static EGLContext createContext(EGLDisplay dpy, EGLConfig curCfg, NvBool isProtected) {
    EGLint contextAttrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_SECURE_NV, isProtected ? EGL_TRUE : EGL_FALSE,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(dpy, curCfg, EGL_NO_CONTEXT, contextAttrs);
        PRINT_EGL_ERROR();

    return ctx;
}


static EGLImageKHR createImage(EGLDisplay dpy, NvMMBuffer* buf, uint8_t index) {
    EGLint attrs[] = {
        EGL_NVRM_SURFACE_COUNT_NVX, 1,
        EGL_NONE,
    };

    NvColorFormat colorformat = buf->Payload.Surfaces.Surfaces[index].ColorFormat;


    if (index == Y_PLANE) {
        buf->Payload.Surfaces.Surfaces[index].ColorFormat = NvColorFormat_L8;
    } else if (index == UV_PLANE) {
        buf->Payload.Surfaces.Surfaces[index].ColorFormat = NvColorFormat_RG8;
    }

    EGLImageKHR image = eglCreateImageKHR(dpy,
                                          EGL_NO_CONTEXT,
                                          EGL_NVRM_SURFACE_NVX,
                                          &(buf->Payload.Surfaces.Surfaces[index]),
                                          attrs);
        PRINT_EGL_ERROR();

    buf->Payload.Surfaces.Surfaces[index].ColorFormat = colorformat;

    if (image == EGL_NO_IMAGE_KHR)
        ALOGE("VBLT: Failed to create EGLImage: %#x", eglGetError());

    return image;
}

/* Video, bacon, lettuce and tomato */
struct VBLT {

    EGLDisplay        mEGLDpy;
    EGLConfig         mEGLCfg;
    EGLContext        mEGLCtx;
    EGLContext        mEGLCtxSecure;
    EGLSurface        mEGLNopSurf;
    GLuint            mProgs[NUM_PLANES];
    GLenum            mTexTarget;
    EGLContext*       eglCtxPtr;
    GLuint            frameBufferIds[NUM_PLANES];
    GLuint            dstTexs[NUM_PLANES];
    GLuint            srcTexs[NUM_PLANES];
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetFunc;
public:
    void print(void){
        ALOGE("mEGLDpy is %p", mEGLDpy);
        ALOGE("mmEGLCtx is %p", mEGLCtx);
        ALOGE("mEGLNopSurf is %p", mEGLNopSurf);
    }

    VBLT():
        mEGLDpy(EGL_NO_DISPLAY),
        mEGLCfg(NULL),
        mEGLCtx(EGL_NO_CONTEXT),
        mEGLCtxSecure(EGL_NO_CONTEXT),
        mEGLNopSurf(EGL_NO_SURFACE),
        mTexTarget(GL_TEXTURE_2D),
        eglCtxPtr(NULL)
    {
        mEGLDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        PRINT_EGL_ERROR();
        memset(mProgs, 0, sizeof(GLuint) * NUM_PLANES);
        eglInitialize(mEGLDpy, NULL, NULL);

        if(!mEGLCfg)
            mEGLCfg = chooseConfig(mEGLDpy);
        PRINT_EGL_ERROR();

        if(!mEGLCfg) {
            ALOGE("VBLT: Failed to choose EGL config: error 0x%x", eglGetError());
        }

        if(mEGLNopSurf == EGL_NO_SURFACE) {
            EGLint nopAttribs[] = {
                EGL_WIDTH,  1,
                EGL_HEIGHT, 1,
                EGL_NONE
            };

            mEGLNopSurf = eglCreatePbufferSurface(mEGLDpy, mEGLCfg, nopAttribs);
            PRINT_EGL_ERROR();
            if(mEGLNopSurf == EGL_NO_SURFACE) {
                ALOGE("VBLT: Failed to create EGL pixmap surface: error 0x%x", eglGetError());
            }
        }

//        if(isProtected)
//            eglCtxPtr = &mEGLCtxSecure;
//        else
//            eglCtxPtr = &mEGLCtx;

        PRINT_EGL_ERROR();
        mEGLCtx = createContext(mEGLDpy, mEGLCfg, NV_FALSE);
        PRINT_EGL_ERROR();

        if(mEGLCtx == EGL_NO_CONTEXT) {
            ALOGE("VBLT: Failed to create EGL context: error 0x%x", eglGetError());
        }

        PRINT_EGL_ERROR();
        if(!eglMakeCurrent(mEGLDpy, mEGLNopSurf, mEGLNopSurf, mEGLCtx)) {
            ALOGE("VBLT: Failed to make EGL context current");
        }
        PRINT_EGL_ERROR();

        glGenFramebuffers(NUM_PLANES, frameBufferIds);
        glGenTextures(NUM_PLANES, dstTexs);
        glGenTextures(NUM_PLANES, srcTexs);

        imageTargetFunc = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        if (!imageTargetFunc) {
            ALOGE("VBLT: Missing support for GL_NV_EGL_image_YUV\n");
        }
    }

    ~VBLT() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(NUM_PLANES, srcTexs);
        glDeleteTextures(NUM_PLANES, dstTexs);
        glDeleteFramebuffers(NUM_PLANES, frameBufferIds);
        //mEGLDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        glDeleteProgram(mProgs[0]);
        glDeleteProgram(mProgs[1]);
        eglMakeCurrent(mEGLDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        PRINT_EGL_ERROR();
        eglDestroyContext(mEGLDpy, mEGLCtx);
        PRINT_EGL_ERROR();
        eglDestroySurface(mEGLDpy, mEGLNopSurf);
        PRINT_EGL_ERROR();
        //eglDestroyContext(mEGLDpy, mEGLCtxSecure);
        eglTerminate(mEGLDpy);
        PRINT_EGL_ERROR();
        eglReleaseThread();
        PRINT_EGL_ERROR();
    }


    NvBool initShaders(GLuint* progY, GLuint* progUV) {
        GLint cStat;
        GLuint vsY, vsUV, fsY, fsUV;
        char log[4096];
        GLsizei loglen = 0;
        vsY = vsUV = fsY = fsUV = 0;

        vsY = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vsY, 1, (const char**)&vsysrc, 0);
        PRINT_GL_ERROR();
        glCompileShader(vsY);
        PRINT_GL_ERROR();
        glGetShaderiv(vsY, GL_COMPILE_STATUS, &cStat);
        PRINT_GL_ERROR();
        if (cStat == GL_FALSE)
        {
            ALOGE("VBLT: Failed to compile Y vertex shader");
            glGetShaderInfoLog(vsY, 1024, &loglen, log);
            ALOGE("Shader info log %s", log);
            ALOGE("The shader string is %s", vsysrc);
            return NV_FALSE;
        }


        if (progY) {
            // Y
            fsY = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fsY, 1, (const char**)&fsysrc, 0);
            glCompileShader(fsY);
            glGetShaderiv(fsY, GL_COMPILE_STATUS, &cStat);
            if (cStat == GL_FALSE)
            {
                ALOGE("VBLT: Failed to compile Y fragment shader");
                glGetShaderInfoLog(fsY, 1024, &loglen, log);
                ALOGE("Shader info log %s", log);
                ALOGE("The shader string is %s", fsysrc);
                return NV_FALSE;
            }

            GLuint pY = glCreateProgram();
            glAttachShader(pY, vsY);
            glAttachShader(pY, fsY);

            glBindAttribLocation(pY, 0, "a_pos");
            glBindAttribLocation(pY, 1, "a_tcoord");
            glLinkProgram(pY);

            *progY = pY;

            glDeleteShader(fsY);
        }
        glDeleteShader(vsY);

        vsUV = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vsUV, 1, (const char**)&vsuvsrc, 0);
        glCompileShader(vsUV);
        glGetShaderiv(vsUV, GL_COMPILE_STATUS, &cStat);
        if (cStat == GL_FALSE)
        {
            ALOGE("VBLT: Failed to compile UV vertex shader");
            glGetShaderInfoLog(vsUV, 1024, &loglen, log);
            ALOGE("Shader info log %s", log);
            ALOGE("The shader string is %s", vsuvsrc);
            return NV_FALSE;
        }

        if(progUV) {
            //UV
            fsUV = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fsUV, 1, (const char**)&fsuvsrc, 0);
            glCompileShader(fsUV);
            glGetShaderiv(fsUV, GL_COMPILE_STATUS, &cStat);
            if (cStat == GL_FALSE)
            {
                ALOGE("VBLT: Failed to compile UV fragment shader");
                glGetShaderInfoLog(fsUV, 1024, &loglen, log);
                ALOGE("Shader info log %s", log);
                ALOGE("The shader string is %s", fsuvsrc);
                return NV_FALSE;
            }

            GLuint pUV = glCreateProgram();

            glAttachShader(pUV, vsUV);
            glAttachShader(pUV, fsUV);

            glBindAttribLocation(pUV, 0, "a_pos");
            glBindAttribLocation(pUV, 1, "a_tcoord");

            glLinkProgram(pUV);
            *progUV = pUV;
            glDeleteShader(fsUV);
        }

        glDeleteShader(vsUV);

        return NV_TRUE;
    }

    int updateVideoFrame(NvMMBuffer* src,
                         NvMMBuffer* dst,
                         const hwc_rect_t *sourceCrop,
                         int transform,
                         NvBool isProtected) {
        status_t err = OK;
        GLenum glErr = GL_NO_ERROR;

        const GLfloat vtData[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,
            0.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 1.0f,
            1.0f, -1.0f, 0.0f, 1.0f,
            1.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 1.0f,
        };

        EGLImageKHR srcImgY = EGL_NO_IMAGE_KHR;
        EGLImageKHR srcImgUV = EGL_NO_IMAGE_KHR;
        EGLImageKHR dstImgY = EGL_NO_IMAGE_KHR;
        EGLImageKHR dstImgUV = EGL_NO_IMAGE_KHR;
        GLuint progs[NUM_PLANES];


        dst->Payload.Surfaces.CropRect = *(NvRect*)sourceCrop;
        ALOGV("Crop Rect Values are: top: %d, bottom: %d, left: %d, right: %d", dst->Payload.Surfaces.CropRect.top, dst->Payload.Surfaces.CropRect.bottom, dst->Payload.Surfaces.CropRect.left, dst->Payload.Surfaces.CropRect.right);

        srcImgY = createImage(mEGLDpy, src, Y_PLANE);
        if(srcImgY == EGL_NO_IMAGE_KHR) {
            ALOGE("VBLT: Failed to create src Y EGLImage");
            err = UNKNOWN_ERROR;
            goto cleanup;
        }
        srcImgUV = createImage(mEGLDpy, src, UV_PLANE);
        if(srcImgUV == EGL_NO_IMAGE_KHR) {
            ALOGE("VBLT: Failed to create src UV EGLImage");
            err = UNKNOWN_ERROR;
            goto cleanup;
        }
        dstImgY = createImage(mEGLDpy, dst, Y_PLANE);
        if(dstImgY == EGL_NO_IMAGE_KHR) {
            ALOGE("VBLT: Failed to create dst Y EGLImage");
            err = UNKNOWN_ERROR;
            goto cleanup;
        }
        dstImgUV = createImage(mEGLDpy, dst, UV_PLANE);
        if(dstImgUV == EGL_NO_IMAGE_KHR) {
            ALOGE("VBLT: Failed to create dst UV EGLImage");
            err = UNKNOWN_ERROR;
            goto cleanup;
        }

        glBindTexture(mTexTarget, dstTexs[Y_PLANE]);
        imageTargetFunc(mTexTarget, (GLeglImageOES)dstImgY);
        glBindTexture(mTexTarget, dstTexs[UV_PLANE]);
        imageTargetFunc(mTexTarget, (GLeglImageOES)dstImgUV);

        // Draw
        if(!mProgs[0] && !initShaders(&mProgs[0], &mProgs[1]))
            goto cleanup;

        glBindTexture(mTexTarget, srcTexs[Y_PLANE]);
        InitTexture(srcTexs[Y_PLANE], dst->Payload.Surfaces.Surfaces[Y_PLANE].Width, dst->Payload.Surfaces.Surfaces[Y_PLANE].Height);
        imageTargetFunc(mTexTarget, (GLeglImageOES)srcImgY);
        glBindTexture(mTexTarget, srcTexs[UV_PLANE]);
        InitTexture(srcTexs[UV_PLANE], dst->Payload.Surfaces.Surfaces[Y_PLANE].Width, dst->Payload.Surfaces.Surfaces[Y_PLANE].Height);
        imageTargetFunc(mTexTarget, (GLeglImageOES)srcImgUV);


        for(unsigned ii = 0; ii < NUM_PLANES; ++ii) {
            glBindTexture(mTexTarget, dstTexs[ii]);
            glBindFramebuffer(GL_FRAMEBUFFER, frameBufferIds[ii]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTexTarget, dstTexs[ii], 0);

/*
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            switch(status) {
                case GL_FRAMEBUFFER_COMPLETE:
                    ALOGE("VBLT: updateVideoFrame: ii=%d: GL_FRAMEBUFFER_COMPLETE\n", ii);
                    break;
                default:
                    ALOGE("VBLT: updateVideoFrame: ii=%d: framebuffer error: %x\n", ii, status);
                    break;
            }
*/

            glViewport(0, 0, dst->Payload.Surfaces.Surfaces[ii].Width, dst->Payload.Surfaces.Surfaces[ii].Height);

            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            GLuint prog = mProgs[ii];
            glUseProgram(prog);
            int positionLoc   = glGetAttribLocation(prog, "a_pos");
            int texCoordsLoc  = glGetAttribLocation(prog, "a_tcoord");
            glVertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 6 * 4, vtData);
            glVertexAttribPointer(texCoordsLoc, 2, GL_FLOAT, GL_FALSE, 6 * 4, &vtData[4]);
            glEnableVertexAttribArray(positionLoc);
            glEnableVertexAttribArray(texCoordsLoc);
            if(ii == Y_PLANE) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(mTexTarget, srcTexs[Y_PLANE]);
                GLuint texSamplerY = glGetUniformLocation(prog, "s_tex_y");
                glUniform1i(texSamplerY, 0);
            }

            if(ii == UV_PLANE) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(mTexTarget, srcTexs[UV_PLANE]);
                GLuint texSamplerUV = glGetUniformLocation(prog, "s_tex_uv");
                glUniform1i(texSamplerUV, 0);
            }

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glUseProgram(0);
        }
        glFinish();
cleanup:
        eglDestroyImageKHR(mEGLDpy, srcImgY);
        eglDestroyImageKHR(mEGLDpy, srcImgUV);
        eglDestroyImageKHR(mEGLDpy, dstImgY);
        eglDestroyImageKHR(mEGLDpy, dstImgUV);
        return err;
    }
};

vblt_t* vblt_create() {
   //Open the shader source files
   long filelength = 0;
   off64_t readlen = 0;
   if ( vsyfd == NULL) {
       vsyfd = fopen(FILENAME_VSY, "r");
       if (vsyfd != NULL) {
          ALOGE("Opened the Y Vertex Shader source");
       } else {
          ALOGE("Failed to open the Y Vertex Shader source");
          return NULL;
       }
       fseek(vsyfd, 0, SEEK_END);
       filelength = ftell(vsyfd);
       fseek(vsyfd, 0L, SEEK_SET);
       vsysrc = new char[filelength + 1];
       readlen = fread(vsysrc, 1, filelength, vsyfd);
       vsysrc[filelength] = 0;
       ALOGE("read %lld bytes from file", readlen);
   }
   if ( fsyfd == NULL) {
       fsyfd = fopen(FILENAME_FSY, "r");
       if (fsyfd != NULL) {
          ALOGE("Opened the Y Fragment Shader source");
       } else {
          ALOGE("Failed to open the Y Fragment Shader source");
          return NULL;
       }
       fseek(fsyfd, 0, SEEK_END);
       filelength = ftell(fsyfd);
       fseek(fsyfd, 0L, SEEK_SET);
       fsysrc = new char[filelength + 1];
       readlen = fread(fsysrc, 1, filelength, fsyfd);
       fsysrc[filelength] = 0;
       ALOGE("read %lld bytes from file", readlen);
   }
   if ( vsuvfd == NULL) {
       vsuvfd = fopen(FILENAME_VSUV, "r");
       if (vsuvfd != NULL) {
          ALOGE("Opened the UV Vertex Shader source");
       } else {
          ALOGE("Failed to open the UV Vertex Shader source");
          return NULL;
       }
       fseek(vsuvfd, 0, SEEK_END);
       filelength = ftell(vsuvfd);
       fseek(vsuvfd, 0L, SEEK_SET);
       vsuvsrc = new char[filelength + 1];
       readlen = fread(vsuvsrc, 1, filelength, vsuvfd);
       vsuvsrc[filelength] = 0;
       ALOGE("read %lld bytes from file", readlen);
   }
   if ( fsuvfd == NULL) {
       fsuvfd = fopen(FILENAME_FSUV, "r");
       if (fsuvfd != NULL) {
          ALOGE("Opened the UV Fragment Shader source");
       } else {
          ALOGE("Failed to open the UV Fragment Shader source");
          return NULL;
       }
       fseek(fsuvfd, 0, SEEK_END);
       filelength = ftell(fsuvfd);
       fseek(fsuvfd, 0L, SEEK_SET);
       fsuvsrc = new char[filelength + 1];
       readlen = fread(fsuvsrc, 1, filelength, fsuvfd);
       fsuvsrc[filelength] = 0;
       ALOGE("read %lld bytes from file", readlen);
   }

   fclose(vsyfd);
   fclose(fsyfd);
   fclose(vsuvfd);
   fclose(fsuvfd);
   vsyfd = NULL;
   fsyfd = NULL;
   vsuvfd = NULL;
   fsuvfd = NULL;

   VBLT *obj = new VBLT;
   //obj->print();
   return obj;
}

void vblt_destroy(vblt_t* vblt) {
   ALOGE("Freeing the shader sources");
   delete[] vsysrc;
   delete[] fsysrc;
   delete[] vsuvsrc;
   delete[] fsuvsrc;
   //vblt->print();
   delete vblt;
   vblt = NULL;
}

void vblt_blit(vblt_t* vblt,
               NvMMBuffer* src,
               NvMMBuffer* dst,
               const hwc_rect_t *sourceCrop,
               int transform,
               NvBool isProtected) {
    vblt->updateVideoFrame(src, dst, sourceCrop, transform, isProtected);
}
