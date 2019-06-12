LOCAL_PATH:= $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES:= \
	main_nvcpud.cpp

LOCAL_SHARED_LIBRARIES := \
	libnvcpud \
	liblog \
	libbinder \
	libutils

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../services

LOCAL_MODULE:= nvcpud
LOCAL_MODULE_TAGS:= optional

include $(NVIDIA_EXECUTABLE)
