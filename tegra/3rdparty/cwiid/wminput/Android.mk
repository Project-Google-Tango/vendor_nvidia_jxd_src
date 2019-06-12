LOCAL_PATH:= $(call my-dir)

include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

LOCAL_SRC_FILES:= main.c conf.c c_plugin.c uinput.c action_enum.c util.c parser.c lexer.c

LOCAL_MODULE:= wminput
LOCAL_CFLAGS:= \
        -DWMINPUT_CONFIG_DIR=\"/system/etc\" \
        -DCWIID_PLUGINS_DIR=\"/system/lib\"

LOCAL_C_INCLUDES:=\
        $(LOCAL_PATH)/../libcwiid \
        $(LOCAL_PATH)/../common/include/ \
        external/bluetooth/bluez/lib \
        external/bluetooth/bluez/src

LOCAL_SHARED_LIBRARIES := \
        libdl \
        libbluetooth \
        libdbus \
        libcutils \
        libglib \
        libcwiid

LOCAL_MODULE_TAGS := optional

include $(NVIDIA_EXECUTABLE)

