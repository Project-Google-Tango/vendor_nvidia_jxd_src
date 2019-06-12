#!/system/bin/sh

# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

mtk=2

if [ $(getprop ro.nct.gps_id) == $mtk ]; then
	/system/bin/log -t "gps_select" -p i "MTK chip"
	setprop ro.hardware.gps mtk
else
	echo 61 > /sys/class/gpio/export
	echo 0 > /sys/class/gpio/gpio61/value
	echo "out" > /sys/class/gpio/gpio61/direction
	chown root:system /sys/class/gpio/gpio61/value
	chmod 0664 /sys/class/gpio/gpio61/value
	echo "enabled" > /sys/devices/platform/reg-userspace-consumer.2/state
	/system/bin/log -t "gps_select" -p i "BRCM chip"
	setprop ro.hardware.gps brcm
	if [ $(getprop ro.boot.commchip_id) == 1 ]; then
		/system/bin/log -t "gps_select" -p i "Commchip_id set to 1, Do not start BRCM GPS"
	else
		start gps-daemon
	fi
fi
