# DO NOT add conditionals to this makefile of the form
#
#    ifeq ($(TARGET_TEGRA_VERSION),<latest SOC>)
#        <stuff for latest SOC>
#    endif
#
# Such conditionals break forward compatibility with future SOCs.
# If you must add conditionals to this makefile, use the form
#
#    ifneq ($(filter <list of older SOCs>,$(TARGET_TEGRA_VERSION)),)
#       <stuff for old SOCs>
#    else
#       <stuff for new SOCs>
#    endif

LOCAL_OEMCRYPTO_LEVEL := 3
nv_modules := \
    _collections \
    _fileio \
    _functools \
    _multibytecodec \
    _nvcameratools \
    _nvcamera_v1 \
    _random \
    _socket \
    _struct \
    _weakref \
    alcameradatataptest \
    aleglstream \
    alplaybacktest \
    alsnapshottest \
    alvcplayer \
    alvcrecorder \
    alvideorecordtest \
    alvepreview \
    array \
    audio_policy.$(TARGET_BOARD_PLATFORM) \
    audio.primary.$(TARGET_BOARD_PLATFORM) \
    binascii \
    bmp2h \
    bootloader \
    btmacwriter\
    camera.$(TARGET_BOARD_PLATFORM) \
    camera2_tests \
    capture \
    cmath \
    com.google.widevine.software.drm \
    com.google.widevine.software.drm.xml \
    buildbct \
    com.nvidia.display \
    com.nvidia.graphics \
    com.nvidia.nvstereoutils \
    com.nvidia.camera_debuginfo \
    datetime \
    dfs_cfg \
    dfs_log \
    dfs_monitor \
    dfs_stress \
    DidimCalibration \
    fcntl \
    fuse_bypass.txt \
    get_fs_stat \
    gps.$(TARGET_BOARD_PLATFORM) \
    gps.mtk \
    gralloc.$(TARGET_BOARD_PLATFORM) \
    hdcp_test \
    hdcp1x.srm \
    hdcp2x.srm \
    hdcp2xtest.srm \
    hosts \
    hwcomposer.$(TARGET_BOARD_PLATFORM) \
    init.hdcp \
    init.tf \
    init.tlk \
    inv_self_test \
    libaacdec \
    libaudioavp \
    libaudioservice \
    libaudioutils \
    libexpat \
    libardrv_dynamic \
    libtinyalsa \
    libcg \
    libcgdrv \
    libDEVMGR \
    libdrmwvmcommon \
    libEGL_perfhud \
    libEGL_tegra \
    libEGL_tegra_impl \
    libGLESv1_CM_perfhud \
    libGLESv1_CM_tegra \
    libGLESv1_CM_tegra_impl \
    libGLESv2_perfhud \
    libGLESv2_tegra \
    libGLESv2_tegra_impl \
    libnvidia_APP5Marker_jni \
    libh264enc \
    libhybrid \
    libinvensense_hal \
    libmd5 \
    libmllite \
    libmnlp_mt3332 \
    libmpeg4enc \
    libmplmpu \
    libnv_parser \
    libnv3gpwriter \
    libnv3p \
    libnv3pserver \
    libnvaacplusenc \
    libnvaboot \
    libnvaes_ref \
    libnvamrnbcommon \
    libnvamrnbdec \
    libnvamrnbenc \
    libnvamrwbdec \
    libnvamrwbenc \
    libnvappmain \
    libnvappmain_aos \
    libnvapputil \
    libnvaudio_memutils \
    libnvaudio_power \
    libnvaudio_ratecontrol \
    libnvaudioutils \
    libnvavp \
    libnvbasewriter \
    libnvbct \
    libnvblit \
    libnvboothost \
    libnvbootupdate \
    libnvbsacdec \
    libnvcam_imageencoder \
    libnvcamera \
    libnvcamerahdr \
    libnvcamerahdr_v3 \
    libnvcamerahdr_expfusion \
    libnvobjecttracking \
    libnvcameratools \
    libnvcamerautil \
    libnvchip \
    libnvcpl \
    libnvcms \
    libpowerservice \
    libnvcrypto \
    libnvddk_2d \
    libnvddk_2d_ap15 \
    libnvddk_2d_ar3d \
    libnvddk_2d_combined \
    libnvddk_2d_fastc \
    libnvddk_2d_swref \
    libnvddk_2d_v2 \
    libnvddk_aes \
    libnvddk_disp \
    libnvddk_fuse_read_avp \
    libnvddk_fuse_read_host \
    libnvddk_i2s \
    libnvddk_misc \
    libnvdigitalzoom \
    libnvdioconverter \
    libnvdispatch_helper \
    libnvdrawpath \
    libnvdtvsrc \
    libnvfs \
    libnvfsmgr \
    libnvfusebypass \
    libnvfxmath \
    libnvglsi \
    libnvhdmi3dplay_jni \
    libnvidia_display_jni \
    libnvilbccommon \
    libnvilbcdec \
    libnvilbcenc \
    libnvimageio \
    libnvintr \
    libnvme_msenc \
    libnvmm \
    libnvmm_camera \
    libnvmm_camera_v3 \
    libnvmmcommon \
    libnvmm_ac3audio \
    libnvmm_asfparser \
    libnvmm_audio \
    libnvmm_aviparser \
    libnvmm_contentpipe \
    libnvmm_msaudio \
    libnvmm_parser \
    libnvmm_utils \
    libnvmm_writer \
    libnvmmlite \
    libnvmmlite_audio \
    libnvmmlite_image \
    libnvmmlite_msaudio \
    libnvmmlite_utils \
    libnvmmlite_video \
    libnvmmtransport \
    libnvnative_env_core \
    libnvodm_audiocodec \
    libnvodm_dtvtuner \
    libnvodm_hdmi \
    libnvodm_imager \
    libnvodm_misc \
    libnvodm_query \
    libnvodm_services \
    libnvoggdec \
    libnvos \
    libnvos_aos \
    libnvos_aos_libgcc_avp \
    libnvparser \
    libnvpartmgr \
    libnvrm \
    libnvrm_channel_impl \
    libnvrm_graphics \
    libnvrm_impl \
    libnvrm_limits \
    libnvrm_secure \
    libnvrsa \
    libnvsm \
    libnvstitching \
    libnvstormgr \
    libnvsystemuiext_jni \
    libnvsystem_utils \
    libnvtestio \
    libnvtestmain \
    libnvtestresults \
    libnvtestrun \
    libnvtnr \
    libnvtsdemux \
    libnvtvmr \
    libnvusbhost \
    libnvwavdec \
    libnvwavenc \
    libnvwinsys \
    libnvwma \
    libnvwmalsl \
    libnvwsi \
    libnvwsi_core \
    libnvomx \
    libnvomxadaptor \
    libnvomxilclient \
    libopengles1_detect \
    libopengles2_detect \
    libopenmaxil_detect \
    libopenvg_detect \
    libpython2.6 \
    librs_jni \
    libsense_fu \
    libsensors.base \
    libsensors.isl29018 \
    libsensors.isl29028 \
    libsensors.mpl \
    libstagefrighthw \
    librm31080 \
    ts.default \
    librm_ts_service \
    raydium_selftest \
    rm_test \
    rm_ts_server \
    para_10_02_00_10 \
    para_10_03_00_10 \
    para_10_04_00_10 \
    para_10_04_00_c0 \
    para_10_07_00_b0 \
    para_10_08_00_b0 \
    para_10_02_00_20 \
    para_10_03_00_20 \
    para_10_04_00_20 \
    para_10_05_00_c0 \
    para_10_08_00_10 \
    para_10_09_00_c0 \
    para_10_02_00_b0 \
    para_10_03_00_b0 \
    para_10_04_00_b0 \
    para_10_06_00_b0 \
    para_10_08_00_20 \
    para_10_09_01_c0 \
    synaptics_direct_touch_daemon \
    libwfd_common \
    libwfd_sink \
    libwfd_source \
    libwlbwjni \
    libwlbwservice \
    libwvdrm_L$(LOCAL_OEMCRYPTO_LEVEL) \
    libwvmcommon \
    libwvocs_L$(LOCAL_OEMCRYPTO_LEVEL) \
    libWVStreamControlAPI_L$(LOCAL_OEMCRYPTO_LEVEL) \
    math \
    mnld \
    mnl.prop \
    MockNVCP \
    nfc.$(TARGET_BOARD_PLATFORM) \
    nvavp_os_0ff00000.bin \
    nvavp_os_8ff00000.bin \
    nvavp_os_eff00000.bin \
    nvavp_os_f7e00000.bin \
    nvavp_smmu.bin \
    nvavp_vid_ucode_alt.bin \
    nvavp_vid_ucode.bin \
    nvavp_aud_ucode.bin \
    nvbdktestbl \
    nvblob \
    nvcgcserver \
    NvCPLSvc \
    NvCPLUpdater \
    powerservice \
    nvdumppublickey \
    nvflash \
    nvgetdtb \
    nv_hciattach \
    nvhost \
    nvidia_display \
    nvsbktool \
    nvsecuretool \
    nvsignblob \
    nvtest \
    operator \
    parser \
    powersig \
    pbc \
    pbc2 \
    lbh_images \
    python \
    QuadDSecurityService \
    sensors.tegra \
    shaderfix \
    select \
    strop \
    tegrastats \
    test-libwvm \
    test-wvdrmplugin \
    test-wvplayer_L$(LOCAL_OEMCRYPTO_LEVEL) \
    time \
    tokenize \
    ttytestapp \
    unicodedata \
    libwvdrmengine \
    xaplay \
    libdrmdecrypt \
    tinyplay \
    tinycap \
    tinymix \
    tsechdcp_test \
    ussrd \
    cvc \
    wlbwservice \
    libnvasfparserhal \
    libnvaviparserhal \
    trace-cmd \
    powercap \
    PowerShark \
    libnvopt_dvm
#    TegraOTA \
# disabled mjolnir components
#    libgrid \
#    libgrid_jrtp \
#    nvpgcservice \
#    libremoteinput \
#    libnvthreads \
#    libnvrtpaudio \
#    libadaptordecoder \
#    libadaptordecoderjni \
# nvcpud
#    libnvcpud \
#    nvcpud \

ifneq ($(filter t30 t114 t148 t124,$(TARGET_TEGRA_VERSION)),)
    nv_modules += microboot
endif

ifeq ($(BOARD_WIDEVINE_OEMCRYPTO_LEVEL),1)
    nv_modules += liboemcrypto
endif

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
    nv_modules += nvtboot
    nv_modules += nvtbootwb0
endif

nv_modules += libtsechdcp

nv_modules += libh264msenc
nv_modules += libvp8msenc
nv_modules += libtsec_otf_keygen
nv_modules += xusb_sil_rel_fw
nv_modules += tegra_xusb_firmware
nv_modules += libnvtsecmpeg2ts
ifneq ($(filter t114,$(TARGET_TEGRA_VERSION)),)
    nv_modules += nvhost_msenc02.fw
    nv_modules += nvhost_tsec.fw
endif

ifneq ($(filter t148,$(TARGET_TEGRA_VERSION)),)
    nv_modules += nvhost_msenc030.fw
    nv_modules += nvhost_tsec.fw
endif

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
nv_modules += \
    libglcore \
    libcuda \
    libnvrmapi_tegra \
    NETB_img.bin \
    vic03_ucode.bin \
    gpmu_ucode.bin \
    fecs.bin \
    gpccs.bin \
    nvhost_msenc031.fw \
    nvhost_tsec.fw
endif

ifneq ($(filter t30 t148,$(TARGET_TEGRA_VERSION)),)
else
# gamecasting for thor and loki
nv_modules += \
    libtwitchsdk \
    libnvgamecaster \
    NvGamecast
endif

nv_modules += \
    libnvddk_vic \
    libnvviccrc

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
    nv_modules += libnvRSDriver
    nv_modules += libclcore_nvidia
endif

ifeq ($(BOARD_BUILD_BOOTLOADER),true)
nv_modules += \
    bootloader
endif

ifeq ($(TARGET_USE_NCT),true)
nv_modules += \
    libnvnct
endif

# Begin nvsi modules but only on t114+
ifeq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
nv_nvsi_modules := \
    com.nvidia.nvsi.xml \
    libtsec_wrapper
include $(CLEAR_VARS)
LOCAL_MODULE := nv_nvsi_modules
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(nv_nvsi_modules)
include $(BUILD_PHONY_PACKAGE)
endif
# End nvsi module

# Begin wfd modules needed by NV products
nv_wfd_modules := \
    nvcap_test \
    libnvcap \
    libnvcapclk \
    libnvcap_video \
    com.nvidia.nvwfd \
    libwfd_common \
    libwfd_sink \
    libwfd_source \
    NvwfdProtocolsPack \
    NvWfdSink \
    NvwfdService \
    NvwfdSigmaDutTest \
    libjni_nvremote \
    libjni_nvremoteprotopkg \
    libjni-nvwfd-sink \
    libnvremoteevtmgr \
    libnvremotell \
    libnvremoteprotocol \
    libwlbwjni \
    libwlbwservice \
    wlbwservice

include $(CLEAR_VARS)
LOCAL_MODULE := nv_wfd_modules
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(nv_wfd_modules)
include $(BUILD_PHONY_PACKAGE)
# End wfd modules needed by NV products

ifeq ($(filter t30 t114 t148,$(TARGET_TEGRA_VERSION)),)
# Begin nvidia compiler modules
nvidia_compiler_modules := \
    libnvidia-compiler

include $(CLEAR_VARS)
LOCAL_MODULE := nvidia_compiler_modules
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(nvidia_compiler_modules)
include $(BUILD_PHONY_PACKAGE)
# End nvidia compiler modules
endif

ifneq ($(filter tf y,$(SECURE_OS_BUILD)),)
nv_modules += \
    libsmapi \
    libtf_crypto_sst \
    tf_daemon
endif

ifeq ($(SECURE_OS_BUILD),tlk)
nv_modules += \
    tos \
    tlk \
    tlk_daemon \
    keystore.tegra
endif

ifeq ($(NV_MOBILE_DGPU),1)
nv_modules += \
    libGLESv1_CM_dgpu_impl \
    libGLESv2_dgpu_impl \
    libglcore \
    mknod \
    nvidia.ko
endif

include $(CLEAR_VARS)

LOCAL_MODULE := nvidia_tegra_proprietary_src_modules
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(nv_modules)
LOCAL_REQUIRED_MODULES += $(ALL_NVIDIA_TESTS)
include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)

LOCAL_MODULE := nvidia-google-tests
LOCAL_REQUIRED_MODULES := camera2_test
include $(BUILD_PHONY_PACKAGE)
