#!/system/bin/sh

if [ ! -f /data/nvidia/dgpu/nvidia.ko ]
then
mkdir -p /data/nvidia/dgpu
cd /system/vendor/nvidia/dgpu
arm-android-eabi-ld -EL -r -T module-common.lds --build-id -o /data/nvidia/dgpu/nvidia.ko nv-kernel.o nv-linux.o nvidia.mod.o
fi

sh /system/bin/makedevices.sh
insmod /data/nvidia/dgpu/nvidia.ko NVreg_RegistryDwords="RMPowerFeature=64"
