LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# some external components look in $(TARGET_OUT_HEADERS) for these headers
LOCAL_COPY_HEADERS :=
LOCAL_COPY_HEADERS += nvrm_channel.h
LOCAL_COPY_HEADERS += nvrm_diag.h
LOCAL_COPY_HEADERS += nvrm_init.h
LOCAL_COPY_HEADERS += nvrm_interrupt.h
LOCAL_COPY_HEADERS += nvrm_memmgr.h
LOCAL_COPY_HEADERS += nvrm_module.h
LOCAL_COPY_HEADERS += nvrm_moduleloader.h
LOCAL_COPY_HEADERS += nvrm_transport.h

include $(BUILD_COPY_HEADERS)
