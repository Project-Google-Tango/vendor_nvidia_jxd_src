ifneq ($(BOARD_VENDOR_USE_NV_CAMERA), false)

TEGRA_CAMERA_TYPE := nvmm

ifeq ($(TEGRA_CAMERA_TYPE),nvmm)

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

# enable/disable nVidia face detection
LOCAL_CFLAGS += -DUSE_NV_FD=1

# enable/disable the nVidia HDR processing
ENABLE_NVIDIA_HDR := true

# flag to enable Hal V3
ifeq ($(TARGET_TEGRA_VERSION), t124)
NV_CAMERA_V3 := true
LOCAL_CFLAGS += -DNV_CAMERA_V3=1
LOCAL_CFLAGS += -DCAMERA_T124
else
ifeq ($(TARGET_TEGRA_VERSION), t132)
NV_CAMERA_V3 := true
LOCAL_CFLAGS += -DNV_CAMERA_V3=1
LOCAL_CFLAGS += -DCAMERA_T124
else
ifeq ($(TARGET_TEGRA_VERSION), t114)
LOCAL_CFLAGS += -DCAMERA_T114
else
ifeq ($(TARGET_TEGRA_VERSION), t148)
LOCAL_CFLAGS += -DCAMERA_T148
else
NV_CAMERA_V3 := false
LOCAL_CFLAGS += -DNV_CAMERA_V3=0
LOCAL_CFLAGS += -DCAMERA_WESTON
endif
endif
endif
endif

# HDR is not supported on ap20
ifeq ($(TARGET_TEGRA_VERSION), ap20)
ENABLE_NVIDIA_HDR := false
endif

ifeq ($(ENABLE_NVIDIA_HDR), true)
    LOCAL_CFLAGS += -DENABLE_NVIDIA_HDR=1
else
    LOCAL_CFLAGS += -DENABLE_NVIDIA_HDR=0
endif

ifeq ($(ENABLE_TRIDENT), true)
    $(info TRIDENT enabled in libnvcamerategra)
    LOCAL_CFLAGS += -DENABLE_TRIDENT=1
else
    LOCAL_CFLAGS += -DENABLE_TRIDENT=0
endif

ifeq ($(BOARD_CAMERA_PREVIEW_HDMI_ONLY), true)
  ifneq ($(BOARD_HAVE_VID_ROUTING_TO_HDMI), true)
    $(error BOARD_HAVE_VID_ROUTING_TO_HDMI must be set to true in order for \
            BOARD_CAMERA_PREVIEW_HDMI_ONLY to take effect)
  endif
  LOCAL_CFLAGS += -DNV_GRALLOC_USAGE_FLAGS_CAMERA=GRALLOC_USAGE_EXTERNAL_DISP
endif

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
    LOCAL_CFLAGS += -DNV_SUPPORT_HIGH_FPS_RECORDING=1
    LOCAL_CFLAGS += -DNV_HALV3_METADATA_ENHANCEMENT_WAR=1
    LOCAL_CFLAGS += -DNV_POWERSERVICE_ENABLE=1
else
    LOCAL_CFLAGS += -DNV_SUPPORT_HIGH_FPS_RECORDING=0
    LOCAL_CFLAGS += -DNV_HALV3_METADATA_ENHANCEMENT_WAR=0
    LOCAL_CFLAGS += -DNV_POWERSERVICE_ENABLE=0
endif

# We really want to get things clean enough that we can check
# this in with -Werror on.
ifeq ($(NV_CAMERA_V3), false)
    LOCAL_CFLAGS += -Werror
else
    LOCAL_CFLAGS += -Werror -Wno-error=sign-compare
    LOCAL_CFLAGS += -DENABLE_BLOCKLINEAR_SUPPORT=1
endif

ifeq ($(NV_CAMERA_V3), false)
LOCAL_SRC_FILES += nvimagescaler.cpp
LOCAL_SRC_FILES += nvcameraparserinfo.cpp
LOCAL_SRC_FILES += nvcamerasettingsparser.cpp
LOCAL_SRC_FILES += libnvcamerabuffermanager/nvbuffer_hw_allocator_tegra.cpp
LOCAL_SRC_FILES += libnvcamerabuffermanager/nvbuffer_manager.cpp
LOCAL_SRC_FILES += libnvcamerabuffermanager/nvbuffer_stream.cpp
LOCAL_SRC_FILES += libnvcamerabuffermanager/nvbuffer_stream_factory.cpp
LOCAL_SRC_FILES += nvcamerahalapi.cpp
LOCAL_SRC_FILES += nvcamerahal.cpp
LOCAL_SRC_FILES += nvcamerahalblockhelpers.cpp
LOCAL_SRC_FILES += nvcamerahaleventhelpers.cpp
LOCAL_SRC_FILES += nvcamerahalbuffernegotiation.cpp
LOCAL_SRC_FILES += nvcamerasettings.cpp
LOCAL_SRC_FILES += nvcamerahalcore.cpp
LOCAL_SRC_FILES += nvcameracallbacks.cpp
LOCAL_SRC_FILES += nvsensorlistener.cpp
LOCAL_SRC_FILES += nvcamerahalpostprocess.cpp
LOCAL_SRC_FILES += nvcamerahalpostprocessHDR.cpp
LOCAL_SRC_FILES += nvcameraparseconfig.cpp
LOCAL_SRC_FILES += nvcamerahallogging.cpp
LOCAL_SRC_FILES += nvcameramemprofileconfigurator.cpp
else ifeq ($(NV_CAMERA_V3), true)
LOCAL_SRC_FILES += camera_v3/nvimagescaler.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahaleventhelpers.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3api.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3utils.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3core.cpp
LOCAL_SRC_FILES += camera_v3/jpegencoder.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3router.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3streamport.cpp
LOCAL_SRC_FILES += camera_v3/nvformatconverter.cpp
LOCAL_SRC_FILES += camera_v3/nvmemallocator.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3metadatahandler.cpp
LOCAL_SRC_FILES += camera_v3/nvmetadatatranslator.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3metadataprocessor.cpp
LOCAL_SRC_FILES += camera_v3/nvcameraparseconfig.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahallogging.cpp
LOCAL_SRC_FILES += camera_v3/nvdevorientation.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3jpegprocessor.cpp
LOCAL_SRC_FILES += camera_v3/nvcamerahal3tnr.cpp
endif

ifeq ($(ENABLE_TRIDENT), true)
LOCAL_SRC_FILES += tridentexample.cpp
endif

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libcamera_client   \
	libcutils \
	libgui \
	libhardware_legacy \
	libjpeg \
	libmedia \
	libnvddk_2d_v2 \
	libnvmm_utils \
	libnvos \
	libnvrm \
	libnvtvmr \
	libui \
	libutils \
	libnvrm_graphics \
	libhardware \
	libnvcam_imageencoder \
	libnvmm_camera_v3 \
	libnvmm \
	libnvgr \
	libfcamdng

ifeq ($(NV_CAMERA_V3), true)
LOCAL_SHARED_LIBRARIES += \
	libhardware \
	libcamera_metadata \
	libcameraservice \
	libGLESv2
endif

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_SHARED_LIBRARIES += \
        libpowerservice_client
endif

ifeq ($(ENABLE_NVIDIA_HDR), true)
LOCAL_SHARED_LIBRARIES += libnvcamerahdr
endif
ifeq ($(ENABLE_TRIDENT), true)
LOCAL_SHARED_LIBRARIES += libtrident
endif

ENABLE_NVIDIA_CAMTRACE := false
ifeq ($(ENABLE_NVIDIA_CAMTRACE), true)
LOCAL_CFLAGS += -DENABLE_NVIDIA_CAMTRACE=1
LOCAL_SHARED_LIBRARIES += libnvcamtracer
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/utils/nvcamtrace
else
LOCAL_CFLAGS += -DENABLE_NVIDIA_CAMTRACE=0
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/utils/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/base/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/core_v3/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics-partner/android/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/../../../external/jpeg
LOCAL_C_INCLUDES += frameworks/native/include/media/hardware
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/image_enc/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/android/libnvcamerategra/libnvcamerabuffermanager
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/android/libnvcamerategra/camera_v3
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
ifeq ($(ENABLE_TRIDENT), true)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/coisp/include
endif
LOCAL_C_INCLUDES += frameworks/av/services/camera/libcameraservice/camera2
LOCAL_C_INCLUDES += frameworks/av/include/camera
LOCAL_C_INCLUDES += system/media/camera/include
LOCAL_C_INCLUDES += system/core/include/system
LOCAL_C_INCLUDES += device/generic/goldfish/opengl/system/OpenglSystemCommon
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_C_INCLUDES += frameworks/native/include/powerservice
endif

# HAL module implemenation stored in
# hw/camera.tegra.so
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.$(TARGET_BOARD_PLATFORM)

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef -Wcast-align
include $(NVIDIA_SHARED_LIBRARY)

endif
endif

