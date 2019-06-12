/*
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NATIVECALLBACKINTERFACE_HPP_
#define NATIVECALLBACKINTERFACE_HPP_

#include <jni.h>

#define LOG_TAG "NativeCallbackInterface"
#include "logging.hpp"

// An implementation of the callback interface will map calls on the native
// side to their counterparts in a java class called the "NativeCallbackHandler".
// Some trickery is involved to gain access to the JVM and JNIEnv, but static
// helper functions make it a bit less painful.
//FIXME: this is really a very specialized thing, could it be made a bit more
// abstract?  For example, get rid of the specific callback names and make it a
// generic mapping function from native to java where the names are parameters...?
class NativeCallbackInterface
{
public:
    // sets the callback handling mechanism
    virtual void setCallbackHandler(JNIEnv* pEnv, jclass& NativeCallbackHandler) = 0;

    // Callback when objected selection is complete
    virtual void callbackObjectRectangeUpdated(JNIEnv* pEnv, int status, float tlx, float tly, float brx, float bry) = 0;

    // helper functions for connecting to java:

    // Helper function to clear caught exceptions from the java environment
    static void checkExceptions(JNIEnv* pEnv)
    {
        if (pEnv->ExceptionCheck())
        {
            jthrowable e = pEnv->ExceptionOccurred();
            pEnv->ExceptionClear(); // have to clear the exception before JNI will work again

            // convert exception to error message
            jclass eClass = pEnv->GetObjectClass(e);
            jmethodID methodID = pEnv->GetMethodID(eClass, "toString", "()Ljava/lang/String;");
            jstring jError = (jstring) pEnv->CallObjectMethod(e, methodID);
            const char* cErrorMsg = pEnv->GetStringUTFChars(jError, NULL);

            LOGE("JNI Exception %s", cErrorMsg);
            pEnv->ReleaseStringUTFChars(jError, cErrorMsg);
        }
    }

    // Helper function to detach the java environment using the virtual machine
    static void detachJavaEnv(JavaVM* jvm, JNIEnv* env)
    {
        //LOGD("deattachJavaEnv: begin");
        checkExceptions(env);
        jvm->DetachCurrentThread();
        //LOGD("deattachJavaEnv: end");
    }

    // Helper function to attach the java environment using the virtual machine
    static JNIEnv* attachJavaEnv(JavaVM* jvm)
    {
        //LOGD("attachJavaEnv: begin");
        if (jvm == NULL)
        {
            LOGI("Java VM not set, callback dropped");
            return NULL;
        }

        // find JNI environment from the java virtual machine
        JNIEnv* pEnv;
        int res = jvm->AttachCurrentThread(&pEnv, NULL);
        if (res)
        {
            LOGI("AttachCurrentThread FAILED, callback dropped", res);
            return NULL;
        }
        //LOGD("attachJavaEnv: Java environment is attached");
        return pEnv;
    }

};

#undef LOG_TAG
#endif //NATIVECALLBACKINTERFACE_HPP_
