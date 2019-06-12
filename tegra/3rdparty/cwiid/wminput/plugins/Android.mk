LOCAL_PATH := $(my-dir)
subdir_makefiles := \
        $(LOCAL_PATH)/acc/Android.mk \
        $(LOCAL_PATH)/ir_ptr/Android.mk \
        $(LOCAL_PATH)/led/Android.mk \
        $(LOCAL_PATH)/nunchuk_acc/Android.mk \
        $(LOCAL_PATH)/nunchuk_stick2btn/Android.mk
include $(subdir_makefiles)
