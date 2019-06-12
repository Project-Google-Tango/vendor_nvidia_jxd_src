# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
	aes \
	blockdev \
	disp \
	fuses \
	i2s \
	keyboard \
	i2c \
	nand \
	sdio \
	snor \
	se \
	spi_flash \
	xusb \
	mipibif \
	spi \
	sata \
	usb \
))

