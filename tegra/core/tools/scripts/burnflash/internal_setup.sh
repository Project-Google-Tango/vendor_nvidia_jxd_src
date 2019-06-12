#
# Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
#
# This script should be sourced from burnflash.sh

if [ -z "${NV_OUTDIR}" ]; then
    echo "Legacy build"
else
    #tmake
    if [ -z ${EMBEDDED_TARGET_PLATFORM} ] ; then
        # Default to $NV_TARGET_BOARD-linux
        EMBEDDED_TARGET_PLATFORM=$NV_TARGET_BOARD-linux
    fi
    TARGET_BOARD=${NV_TARGET_BOARD}
    if [ "${NV_BUILD_CONFIGURATION_IS_DEBUG}" -eq 0 ]; then
        BUILD_FLAVOR=release
    else
        BUILD_FLAVOR=debug
    fi
fi

if [ -z ${TEGRA_TOP} ] || [ -z ${EMBEDDED_TARGET_PLATFORM} ]; then
    echo "Error: Please make sure TEGRA_TOP and EMBEDDED_TARGET_PLATFORM shell variables are set properly"
    AbnormalTermination
fi

TARGET_OS_SUBTYPE=gnu_linux
TARGET_BOOT_MEDIUM=nand

QNX_BSP_REPO=${TEGRA_TOP}/qnx/src
QNX_BSP_SOURCE_PATH=src

if [ ${EMBEDDED_TARGET_PLATFORM} = "p852m-linux" ]; then
    TARGET_BOARD=p852
elif [ ${EMBEDDED_TARGET_PLATFORM} = "p852-linux" ]; then
    TARGET_BOARD=p852
    TARGET_BOOT_MEDIUM=nor
elif [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-linux" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx" ] ||
     [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx-f" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx-g" ] ||
     [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx-m2" ] ; then
    TARGET_BOARD=p1852
    TARGET_BOOT_MEDIUM=nor
    QNX_BSP_DIR=bsp/mmx2
    if [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx-m2" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx-f" ] ; then
        SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/p1852/nvflash/splash_screen.bin
    fi
    if [ ${EMBEDDED_TARGET_PLATFORM} = "p1852-qnx-g" ] ; then
        SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/p1852/nvflash/splash_screen_g.bin
    fi
elif [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30-t30-qnx-g" ]; then
    TARGET_BOARD=vcm30-t30
    TARGET_BOOT_MEDIUM=nor
    QNX_BSP_DIR=bsp/vcm
    SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/vcm30-t30/nvflash/splash_screen_g.bin
elif [ ${EMBEDDED_TARGET_PLATFORM} = "cardhu-linux" ]; then
    TARGET_BOARD=cardhu
    TARGET_BOOT_MEDIUM=emmc
elif [ ${EMBEDDED_TARGET_PLATFORM} = "e1853-linux" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "e1853-qnx-g" ] ||
     [ ${EMBEDDED_TARGET_PLATFORM} = "e1853-qnx-c" ] ; then
    TARGET_BOARD=e1853
    TARGET_BOOT_MEDIUM=nor
    if [ ${EMBEDDED_TARGET_PLATFORM} = "e1853-qnx-g" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "e1853-qnx-c" ]; then
        SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/e1853/nvflash/splash_screen.bin
        QNX_BSP_DIR=bsp/vcm
    fi
elif [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t114-qnx-g" ]; then
    TARGET_BOARD=vcm30t114
    TARGET_BOOT_MEDIUM=nand
    SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/vcm30t114/nvflash/splash_screen_g.bin
    QNX_BSP_DIR=bsp/vcm
# remove
elif [ ${EMBEDDED_TARGET_PLATFORM} = "e1859_t114-qnx-g" ]; then
    TARGET_BOARD=e1859_t114
    TARGET_BOOT_MEDIUM=nand
    SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/e1859_t114/nvflash/splash_screen_g.bin
    QNX_BSP_DIR=bsp/vcm
elif [ ${EMBEDDED_TARGET_PLATFORM} = "ardbeg-qnx" ]; then
    TARGET_BOARD=ardbeg
    TARGET_BOOT_MEDIUM=nand
    SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/ardbeg/nvflash/splash_screen_g.bin
    QNX_BSP_DIR=bsp/vcm
elif [ ${EMBEDDED_TARGET_PLATFORM} = "m2601-linux" ]; then
    TARGET_BOARD=m2601
    TARGET_BOOT_MEDIUM=nor
elif [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-linux" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-qnx-m3" ] ||
     [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-qnx-g" ] ; then
    TARGET_BOARD=vcm30t124
    TARGET_BOOT_MEDIUM=nor
    QNX_BSP_SOC=t12x
    if [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-qnx-m3" ] || [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-qnx-g" ] ; then
        if [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-qnx-m3" ] ; then
            QNX_BSP_DIR=bsp/mmx3
            SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/vcm30t124/nvflash/splash_screen.bin
        fi
        if [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30t124-qnx-g" ] ; then
            QNX_BSP_DIR=bsp/vcm
            SplashScreenBin=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/vcm30t124/nvflash/splash_screen_g.bin
        fi
    fi
# TODO: remove
elif [ ${EMBEDDED_TARGET_PLATFORM} = "vcm30-t124-linux" ]; then
    TARGET_BOARD=vcm30-t124
    TARGET_BOOT_MEDIUM=nor
else
    echo "Error: Unknown **EMBEDDED_TARGET_PLATFORM"
    AbnormalTermination
fi

if [ -n "${USE_MODS_DEFCONFIG}" ]; then
    KERNEL_CONFIG=tegra_${TARGET_BOARD}_mods_defconfig
else
    KERNEL_CONFIG=tegra_${TARGET_BOARD}_${TARGET_OS_SUBTYPE}_defconfig
fi
UBOOT_CONFIG=tegra2_${TARGET_BOARD}_config


if [ ${TARGET_BOARD} = "p852" ]; then
    UBOOT_CONFIG=tegra2_${TARGET_BOARD}_${TARGET_BOOT_MEDIUM}_${TARGET_OS_SUBTYPE}_config
fi


if [ -z "${NV_OUTDIR}" ]; then
    # Legacy build
    OUTDIR_BOOTLOADER=_out/${BUILD_FLAVOR}_${EMBEDDED_TARGET_PLATFORM}/aos_armv6
    OUTDIR_HOST=_out/${BUILD_FLAVOR}_${EMBEDDED_TARGET_PLATFORM}/rh9_x86
    OUTDIR_KERNEL=${TEGRA_TOP}/core/_out/${BUILD_FLAVOR}_${KERNEL_CONFIG}
    OUTDIR_QUICKBOOT=${TEGRA_TOP}/core/_out/${BUILD_FLAVOR}_${EMBEDDED_TARGET_PLATFORM}/quickboot
    OUTDIR_UBOOT=${TEGRA_TOP}/core/_out/${BUILD_FLAVOR}_uboot_${UBOOT_CONFIG}

    BurnFlashBin=${TEGRA_TOP}/core/${OUTDIR_BOOTLOADER}/fastboot.bin
    QNX=${TEGRA_TOP}/core-private/_out/${BUILD_FLAVOR}_${EMBEDDED_TARGET_PLATFORM}/qnx650_armv6/

    CompressUtil=${TEGRA_TOP}/core/${OUTDIR_HOST}/compress
    NvFlashPath=${TEGRA_TOP}/core/${OUTDIR_HOST}
    SKUFlashPath=${TEGRA_TOP}/core-private/${OUTDIR_HOST}
    MKBOOTIMG_BIN=${TEGRA_TOP}/core/${OUTDIR_HOST}

    FlashUpdateTool=${OUTDIR_QUICKBOOT}/update_sample
    TegraFlashTool=${OUTDIR_QUICKBOOT}/tegraflash
    NvFlashDir=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/${TARGET_BOARD}/nvflash
    MkEfsPath=${TEGRA_TOP}/efs/efs-private/efstools/mkefs/${OUTDIR_HOST}
else
    # tmake build
    OUTDIR_KERNEL=${NV_OUTDIR}/nvidia/kernel
    OUTDIR_QUICKBOOT=${NV_OUTDIR}/nvidia/quickboot/_out/${BUILD_FLAVOR}_$(echo ${NV_TARGET_BOARD} | sed s/-/_/g)/images
    OUTDIR_UBOOT=${NV_OUTDIR}/nvidia/u-boot

    BurnFlashBin=${NV_OUTDIR}/nvidia/core/system/fastboot-*/bootloader.bin
    QNX=${NV_OUTDIR}/nvidia/qnx-bsp/${QNX_BSP_DIR}/images/

    CompressUtil=${NV_OUTDIR}/nvidia/quickboot/quickboot-private/utils/tools/host/compress
    NvFlashPath=${NV_OUTDIR}/nvidia/core/system/nvflash/app-hostcc
    SKUFlashPath=${NV_OUTDIR}/nvidia/core-private/system/nvskuflash-hostcc
    MKBOOTIMG_BIN=${NV_OUTDIR}/nvidia/3rdparty/mkbootimg/tmake-hostcc

    FlashUpdateTool=${NV_OUTDIR}/nvidia/quickboot/quickboot-partner/tools/libnvupdate/samples/_out/update_sample
    TegraFlashTool=${NV_OUTDIR}/nvidia/quickboot/quickboot-partner/tools/target/libtegraflash/app/_out/tegraflash
    PFlashTool=${NV_OUTDIR}/nvidia/pflash/pflash-app-l4t/pflash
    DtbPath=${OUTDIR_KERNEL}/arch/arm/boot/dts

    # burnflash doesn't look into sub-directory for compress tool
    ln -sf ${CompressUtil}/*/compress_* $(dirname ${CompressUtil})

    NvFlashDir=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/${NV_TARGET_BOARD}/nvflash
    MkEfsPath=${NV_OUTDIR}/nvidia/efs/efs-private/efstools/mkefs-hostcc

    if [[ ${EMBEDDED_TARGET_PLATFORM} == *qnx* ]] ; then
        export QNX_BASE=${TEGRA_TOP}/qnx/qnx650sp1/${EMBEDDED_TARGET_PLATFORM}
        export QNX_HOST=${QNX_BASE}/host/linux/x86
        export QNX_TARGET=${QNX_BASE}/target/qnx6
        export QNX_CONFIGURATION=${QNX_BASE}/config
        export PATH=$PATH:${QNX_HOST}/usr/bin/
        TEGRAFLASH_BUILD_SCRIPT=${QNX_BSP_REPO}/${QNX_BSP_DIR}/${QNX_BSP_SOURCE_PATH}/hardware/startup/boards/nvidia-${QNX_BSP_SOC}/${TARGET_BOARD}/v7.build_tegraflash
        cp ${TEGRAFLASH_BUILD_SCRIPT} ${QNX}
    fi
fi

Uboot=${OUTDIR_UBOOT}/u-boot.bin
Quickboot1=${OUTDIR_QUICKBOOT}/quickboot1.bin
Quickboot2=${OUTDIR_QUICKBOOT}/cpu_stage2.bin
Rcmboot=${OUTDIR_QUICKBOOT}/rcmboot.bin
Kernel=${OUTDIR_KERNEL}/arch/arm/boot/zImage
UncompressedKernel=${OUTDIR_KERNEL}/arch/arm/boot/Image
BootMedium=${TARGET_BOOT_MEDIUM}

ConfigFileDir=${TEGRA_TOP}/bootloader/nvbootloader/odm-partner/common
PREBUILT_BIN=${P4ROOT}/sw/embedded/external-prebuilt/tools
