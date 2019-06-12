LOCAL_PATH := $(call my-dir)

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
    common \
    omxplayer \
    omxplayer2 \
))
