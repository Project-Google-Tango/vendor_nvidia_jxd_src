LOCAL_PATH := $(call my-dir)
include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
	aes_ref \
	hybrid \
	md5 \
	nvappmain \
	nvapputil \
	nvblob \
	nvdumper \
	nvfxmath \
	nvintr \
	nvos \
	nvosutils \
	nvos/aos/avp \
	nvusbhost/libnvusbhost \
	nvtestmain \
	nvtestresults \
	tegrastats \
	mknod \
	chipid \
))

