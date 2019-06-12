#!/bin/bash

# Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

Usage()
{
    echo ""
    BoardHelpHeader
    echo ""
    echo "-h : Prints this message"
    echo "-F : Uses framebuffer as default console (need mods linux build)"
    echo "-B : Specify baud rate for kernel in bps (default is 115200 bps)"
    echo "-n : Selects nfs port (linux targets only)"
    echo "-r : Selects root device to mount (linux targets only). Example: sda1 for USB harddrive."
    echo "     Note that for ease of scripting, both -n and -r are allowed, but last instance of one of them must be 'none'"
    echo "-c : Additional kernel args to pass to kernel during bootup (linux targets only). "
    echo "     Example: -c \"quiet init=/bin/dash\" disables the boot messages and "
    echo "              sets /bin/dash as the initial command executed by the kernel."
    echo "-Z : Specify compression algorithm to be used for compressing kernel Image on host."
    echo "   : Supported options are: lzf, zlib and none."
    echo "     \"none\" is to use kernel zImage instead of Image i.e. no compression on host."
    echo "     Z option is mandatory but mutually exclusive with -k option."
    echo "-q : Flash QNX image file system"
    echo "-u : Flash u-boot bootloader"
    echo "-R : RCM Boot Support. Boot without Flashing Software(BCT is used from Media)"
    echo "-f <BCT> : FA mode Support. Don't use anything from Media(BCT needs to be provided, Use default keyword to use default BCT)"
    echo "-H : Build with Hypervisor"
    eval $zHelpOptions
    BoardHelpOptions
}

AbnormalTermination()
{
    popd &>/dev/null
    exit 10
}

GenericFunction()
{
    :
}

GenericOptions="d:hyFon:r:c:z:S:b:Z:B:A:M:V:P:quRf:H"
NfsPort=
RootDevice=
LinuxBoot=0
TargetType=
ExtraKernArgs=
UseUboot=0
CompressKernel=0
BlUncompress=
Disp="vga"
ExtraOpts=
NvFlashExtraOpts=
is_present_loadimg_img=
FABCT=
FAMode=0
SkuId=0
BoardId=0
FlashQNX=0
Quickboot=1
BootMedium="nand"
BaudRate=0
RcmMode=0
FAMode=0
Cygwin=0
UseHypervisor=0

# Console option to enable/disable serial and framebuffer console
KernelConsoleArgs="console=ttyS0,115200n8"
CompressionAlgo=
ExtraNvflashArgs=

DirBeingRunFrom=`dirname ${0}`
pushd $DirBeingRunFrom &>/dev/null

if [ -z zHelpOptions ];then
    zHelpOptions=GenericFunction
fi

if [ -z zHandler ];then
    zHandler=GenericFunction
fi

if [ "`uname -o`" == "Cygwin" ]; then
     echo "Running from Cygwin environment"
     Cygwin=1
fi

################## Utilities/Image Path ########################
## Default kernel hex address for qnx and linux.
Kern_Addr=A00800
NvFlashDir=.
ConfigFileDir=${NvFlashDir}
NvFlashPath=../../nvflash
BurnFlashBin=./burnflash.bin
Uboot=../../../bootloader/u-boot.bin
Quickboot1=../../../bootloader/quickboot1.bin
Quickboot2=../../../bootloader/cpu_stage2.bin
Rcmboot=../../../bootloader/rcmboot.bin
CompressUtil=${NvFlashPath}/compress
Kernel=../../../kernel/zImage
DtbPath=../../../kernel/
UncompressedKernel=../../../kernel/Image
QNX=../../../bsp/images/
MKBOOTIMG_BIN=${NvFlashPath}
MKBOOTIMG=loadimg.img
RCMBOOTIMG=rcmboot.img

if [ $Cygwin -eq 1 ]; then
    LINUX_SUDO_LDPATH=""
else
    LINUX_SUDO_LDPATH="sudo LD_LIBRARY_PATH=${NvFlashPath}"
fi

################## Internal Development ########################

if [ -z $BurnFlashDir ]; then
    BurnFlashDir=.
fi

if [ -f "${BurnFlashDir}/internal_setup.sh" ] ; then
    . ${BurnFlashDir}/internal_setup.sh
fi

if [ -f "${NvFlashDir}/burnflash_helper.sh" ]; then
    . "${NvFlashDir}/burnflash_helper.sh"
else
    echo "burnflash_helper.sh not found"
    exit -1
fi

echo "burnflash.sh for $TargetType board"

############### Parsing Commandline ######################
if [[ $# -lt 1 ]] ; then
    Usage
    AbnormalTermination
fi

while getopts "${GenericOptions}${BoardOptions}" Option ; do
    case $Option in
        h) Usage
            # Exit if only usage (-h) was specfied.
            if [[ $# -eq 1 ]] ; then
                AbnormalTermination
            fi
            exit 0
            ;;
        r) RootDevice=$OPTARG
           echo "Root device set to ${RootDevice}."
           ;;

        F) KernelConsoleArgs=""
           if [ $BaudRate -ne 0 ]; then
               echo "Option -F is incompatible with option -B"
               AbnormalTermination
           fi
       # framebuffer console does not need console option
           ;;

        B) BaudRate=${OPTARG}
           if [ ! ${KernelConsoleArgs} ]; then
               echo "Option -B is incompatible with option -F"
               AbnormalTermination
           fi
           KernelConsoleArgs="console=ttyS0,"$BaudRate"n8"
           ;;
        n) NfsPort=$OPTARG
           ;;
        c) ExtraKernArgs=$OPTARG
           ;;
        z) ZFile=$OPTARG
           if [ -e $ZFile ]; then
               eval $zHandler $ZFile
           else
               echo "File $ZFile does not exist"
               exit -1
           fi
           ;;
        A) BSer=$OPTARG
           eval $AHandler $BSer
           ;;
        M) MInterface=$OPTARG
           MId=\${$OPTIND}
           eval $MHandler $MInterface $MId
           shift 1
           ;;
        V) SVer=$OPTARG
           eval $VHandler $SVer
           ;;
        P) PInfo=$OPTARG
           PVer=\${$OPTIND}
           eval $pHandler $PInfo $PVer
           shift 1
           ;;
        d) if [ $OPTARG = "vga" ] || [ $OPTARG = "hdmi" ] || [ $OPTARG = "tftlcd" ]; then
                Disp=$OPTARG
           else
               echo "Invalid option for -d. Supported values are vga, hdmi and tftlcd."
               exit -1
           fi
           ;;
        b) BoardId=$OPTARG
           ;;
        S) SkuId=$OPTARG
           ;;
        o) Quickboot=0
           ;;
        Z) CompressionAlgo=$OPTARG
           if [ $OPTARG = "lzf" ] || [ $OPTARG = "zlib" ]; then
               CompressKernel=1
           else
               if [ $OPTARG = "none" ] ; then
                    CompressKernel=0
               else
                    echo "Invalid option for -Z. Supported values are lzf, zlib or none."
                    exit -1
               fi
           fi
           ;;
       q) FlashQNX=1
           ;;
       H) UseHypervisor=1
           ;;
       R) RcmMode=1
           ;;
       u) UseUboot=1
          Quickboot=0
          FlashQNX=0
          CompressKernel=0
          ;;
       f) FAMode=1
           FABCT=$OPTARG
           ;;
        *) BoardParseOptions
           ;;
    esac
done
shift $(($OPTIND-1))

################## Virtualization Parameters ##################
if [ "${UseHypervisor}" -eq 1 ]; then
       UncompressedKernel=../../../../hypervisor/hypervisor-meta/current/codezero/Image
       Kernel=../../../../hypervisor/hypervisor-meta/current/codezero/Image
       Kern_Addr=01000000
fi

################## Display default paths ####################
if [ -z ${UserConfigFile} ] ; then
    echo ; echo;
    echo "################# Using the default paths ###################"
    if [ ! ${FlashQNX} -eq 1 ] && [ ! ${UseUboot} -eq 1 ]; then
        echo "Kernel: ${Kernel}"
    fi
    if [ ${UseUboot} -eq 1 ]; then
        echo "Uboot: ${Uboot}"
    fi
    if [ ${FlashQNX} -eq 1 ]; then
        echo "QNX: ${QNX}"
    fi
    if [ ${Quickboot} -eq 1 ]; then
        echo "Quickboot1: ${Quickboot1}"
        echo "Quickboot2: ${Quickboot2}"
        if [ ! -r ${Quickboot1} -o ! -r ${Quickboot2} ]; then
            ls -l ${Quickboot1} ${Quickboot2}
            AbnormalTermination
        fi
    fi
    echo "#############################################################"
    echo ; echo;
fi

################## All options are needed ####################
# Variables do not assume default values to avoid confusion
# We could expect strict ordering and do away with -f/-b/-n options
if [ -z "$TargetType" ]; then
    echo "Missing Parameter"
    Usage
    AbnormalTermination
fi
if [ ${RcmMode} -eq 1 ] && [ ${FAMode} -eq 1 ];then
    echo "FA Mode and RCM mode can't be specified at same time"
    Usage
    AbnormalTermination
fi
if [ ${RcmMode} -eq 1 ] || [ ${FAMode} -eq 1 ];then
    if [ ${CompressKernel} -eq 1 ]; then
        echo "Compression support not avaliable with Rcm Boot"
        Usage
        AbnormalTermination
    fi
    if [ $Quickboot -eq 0 ];then
        echo "Quickboot is required for the RCM boot"
        Usage
        AbnormalTermination
    fi
fi

if [ -n "$ExtraOpts" ]; then
   eval $iHandler
fi

if [ ! ${FlashQNX} -eq 1 ] && [ ! ${UseUboot} -eq 1 ]; then
    if [ -z ${UserConfigFile} ];then
        if [ -z "$NfsPort" ] && [ -z "$RootDevice" ]; then
            echo "Missing Parameter"
            Usage
            AbnormalTermination
        fi
    fi
    if [ ${CompressKernel} -eq 1 ]; then
        # Use Image instead of zImage for Linux
        Kernel=${UncompressedKernel}
    fi
else
    if [ ! -z "$NfsPort" ] || [ ! -z "$RootDevice" ]; then
        echo "Invalid parameter combination for -q option"
        Usage
        AbnormalTermination
    fi
fi


if [ ! -z "$NfsPort" ] && [ ! -z "$RootDevice" ] && [ "$NfsPort" != "none" ] && [ "$RootDevice" != "none" ]; then
    echo "Both -n and -r specified, without anyone of them being 'none'"
    Usage
    AbnormalTermination
fi

if [ ${Quickboot} -ne 1 ] && [ ${CompressKernel} -eq 1 ]; then
    echo "-Z option is supported only with Quickboot bootloader"
    Usage
    AbnormalTermination
fi
# call function for board specific validation of parameters
BoardValidateParams

if [ ${FlashQNX} -eq 1 ]; then
    Kernel=${QNX}/${QNXFileName}
fi

# execute any board specific steps required before flashing
BoardPreflashSteps

# 22-20 bits of ODM data is used to define display type
case $Disp in
    "vga")  let "ODMData &= 0xFF8FFFFF"
            let "ODMData |= 0x00300000"
            ;;
    "hdmi") let "ODMData &= 0xFF8FFFFF"
            let "ODMData |= 0x00200000"
            ;;
    "tftlcd") let "ODMData &= 0xFF8FFFFF"
            ;;
    *)      echo "Invalid Disp optios"
            ;;
esac

#### Empirical Check for Target availability ####
lsusb |grep -i nvidia
if [ $? != 0 ]; then
    echo "No recovery-target found; Make sure the target device is connected to the"
    echo "host and is in recovery mode. Exiting"
    exit -1
fi

is_present_loadimg_img=`grep -w ${MKBOOTIMG} ${ConfigFile}`
if [ -n "${is_present_loadimg_img}" ]; then
    if [ -z "${CompressionAlgo}" ] && [ $RcmMode -eq 0 ]; then
        echo "Error: Missing option -Z"
        AbnormalTermination
    fi
fi
################### Flash the bootloader #########################
echo "Flashing the bootloader..."
# Copy bootloader(s) and config file and other required binaries

if [ -f "${BurnFlashDir}/internal_setup.sh" ] ; then
    if [ -f ${BurnFlashBin} ]; then
        cp -f ${BurnFlashBin} $PWD/burnflash.bin
    fi

    if [ ! ${FlashQNX} -eq 1 ]; then
        cp -f ${PREBUILT_BIN}/mkyaffs2image ${PREBUILT_BIN}/mkfs.ext3 ${PREBUILT_BIN}/mkfs.ext2 ${MKBOOTIMG_BIN}/mkbootimg ${NvFlashPath}
    fi
fi

if [ ${Quickboot} -eq 1 ] || [ -n "${UserConfigFile}" ] ; then
    cp ${Quickboot1} ${PWD}
    cp ${Quickboot2} ${PWD}
fi

# FIXME: Remove this later on, only added to get kernel change pass
DtbT124FileName=tegra124-vcm30t124.dtb
if [ ${FlashQNX} -ne 1 ]; then
    if [ -f "${DtbPath}/${DtbFileName}" ] ; then
        cp ${DtbPath}/${DtbFileName} ${PWD}/tegra.dtb
    fi

    if [ -f "${DtbPath}/${DtbT124FileName}" ] ; then
        cp ${DtbPath}/${DtbT124FileName} ${PWD}/tegra.dtb
    fi
fi

if [ ${UseUboot} -eq 1 ]; then
    cp ${Uboot} ${PWD}
fi

#################### Copy Splash Screen ############################
if [ ! -z ${SplashScreenBin} ] ; then

    cp ${SplashScreenBin} ${PWD}/splash_screen.bin
fi

#################### Flash the Kernel ############################
if [ ! -f ${Kernel} ] && [ ! ${UseUboot} -eq 1 ]; then
    echo "Error: Kernel image not present"
    AbnormalTermination
fi

if [ ${CompressKernel} -eq 1 ]; then

    CompressUtil=${CompressUtil}_${CompressionAlgo}
    echo "Compressing kernel image..."
    if [ ${CompressionAlgo} = "zlib" ]; then
        echo "Using ZLIB compression algorithm to compress the image"
        ${CompressUtil} ${Kernel} > ${Kernel}.def
        if [ $? -ne 0 ]; then
            echo "Failed to compress kernel image"
            AbnormalTermination
        fi
        BlUncompress="--bluncompress zlib"
    else
        echo "Using LZF compression algorithm to compress the image"
        if [ ${BootMedium} = "nor" ]; then    # Use block size of (16K - 1) for nor and (2K - 1) for others.
            ${CompressUtil} -b 16383 -c ${Kernel} -r > ${Kernel}.def
            if [ $? -ne 0 ]; then
                echo "Failed to compress kernel image"
                AbnormalTermination
            fi
        else
            ${CompressUtil} -b 2047 -c ${Kernel} -r > ${Kernel}.def
            if [ $? -ne 0 ]; then
                echo "Failed to compress kernel image"
                AbnormalTermination
            fi
        fi
        BlUncompress="--bluncompress lzf"
    fi

    if [ "$?" -eq 0 ]; then
        echo "Kernel Compression successful"
    else
        echo "Error while Compressing Kernel Image, exiting..."
        AbnormalTermination
    fi

    Kernel=${Kernel}.def
    if [ ${FlashQNX} -ne 1 ]; then
###   Kernel address in hex ####
        Kern_Addr=8000
    fi
fi

############################################# mkbootimg creation and flashing #######################################

if [ -n "${is_present_loadimg_img}" ] || [ $RcmMode -eq 1 ]; then
    echo "Creating Boot Image..."
    if [ ! -z  "$NfsPort" ] && [ "$NfsPort" != "none" ]; then
        ${MKBOOTIMG_BIN}/mkbootimg --kernel $Kernel --ramdisk NONE --kern_addr $Kern_Addr --cmdline "root=/dev/nfs ip=:::::${NfsPort}:on rw netdevwait ${BoardKernArgs} ${ExtraKernArgs} ${KernelConsoleArgs} " ${BlUncompress}  -o $MKBOOTIMG
    else
        if [ -n "${RootDevice}" ] || [ ${FlashQNX} -eq 1 ]; then
            ${MKBOOTIMG_BIN}/mkbootimg --kernel $Kernel --ramdisk NONE --kern_addr $Kern_Addr --cmdline "root=/dev/${RootDevice} rw rootwait ${BoardKernArgs} ${ExtraKernArgs} ${KernelConsoleArgs} " ${BlUncompress} -o $MKBOOTIMG
        else
            echo "No Root-Device or nfsport is specified. Exiting ... "
            AbnormalTermination
        fi
    fi

    if [ "$?" -eq 0 ]; then
        echo "Boot Image Created successfully"
    else
        echo "Error while creating Boot Image, exiting..."
        AbnormalTermination
    fi
else
    if [ ${CompressKernel} -eq 1 ]; then
        echo "Using -Z option is not supported in this case. Did you intend to enable quickboot decompression? Please set stream_decompression field in nvflash config file to appropriate value"
        AbnormalTermination
    fi
fi
if [ $RcmMode -eq 1 ];then
    if [ -f "${PWD}/tegra.dtb" ]; then
        echo "Using ${TargetType} Tegra.dtb file.."
        cat $Rcmboot $MKBOOTIMG tegra.dtb >$RCMBOOTIMG
    else
        echo " Not Using Tegra.dtb file.."
        cat $Rcmboot $MKBOOTIMG >$RCMBOOTIMG
    fi
    sync
    ${LINUX_SUDO_LDPATH} ${NvFlashPath}/nvflash --bl ${RCMBOOTIMG} --setentry 0x84008000 0x84008000 ${ExtraNvflashArgs} --go
elif [ $FAMode -eq 1 ];then
    if [ "$FABCT" == "default" ]; then
        FABCT=$BCT
    fi
    if [ -f ${FABCT} ];then
        cat $Rcmboot $MKBOOTIMG >$RCMBOOTIMG
        ${LINUX_SUDO_LDPATH} ${NvFlashPath}/nvflash --bct ${FABCT} --setbct --configfile ${ConfigFile} --create --bl $RCMBOOTIMG --setentry 0x84008000 0x84008000 --odmdata `printf "0x%0x" ${ODMData}` --go
    else
        echo "File ${FABCT} not found. Terminating"
        AbnormalTermination
    fi
else
    # Flash the target
    ${LINUX_SUDO_LDPATH} ${NvFlashPath}/nvflash --bct ${BCT} --setbct --configfile ${ConfigFile} --create --bl burnflash.bin --odmdata `printf "0x%0x" ${ODMData}` ${NvFlashExtraOpts} --go
fi
cmd_output=$?

###############################################################################################3

# Clean up
if [ -n "$ExtraOpts" ]; then
    eval $CleanUpHandler
fi

if [ -f "${BurnFlashDir}/internal_setup.sh" ] ; then
    rm -f ${PWD}/burnflash.bin
    rm -f ${NvFlashPath}/mkyaffs2image ${NvFlashPath}/mkfs.ext3 ${NvFlashPath}/mkfs.ext2
fi

if [ ${Quickboot} -eq 1 ] || [ -n "${UserConfigFile}" ]; then
    rm ${PWD}/`basename ${Quickboot1}`
    rm ${PWD}/`basename ${Quickboot2}`
    rm $MKBOOTIMG
fi

if [ ${FlashQNX} -ne 1 ]; then
    if [ -f "${DtbPath}/${DtbFileName}" ] ; then
        rm ${PWD}/tegra.dtb
    fi
fi

if [ ${UseUboot} -eq 1 ]; then
    rm ${PWD}/`basename ${Uboot}`
fi

if [ ! -z ${SplashScreenBin} ] ; then
    rm ${PWD}/splash_screen.bin
fi

if [ "$cmd_output" -eq 0 ]; then
    if [ $RcmMode -eq 1 ] || [ $FAMode -eq 1 ];then
        rm $RCMBOOTIMG
        echo "Boot successfull"
        exit 0
    else
        echo "Flashed the Bootloader successfully."
    fi
else
    echo "Error while flashing bootloader, exiting..."
    AbnormalTermination
fi


# board specific steps required after nvflash is done
BoardPostNvFlashSteps
BoardPostKernelFlashSteps


if [ "$cmd_output" -eq 0 ]; then
    if [ ${UseUboot} -eq 1 ]; then
        echo "Flashed u-boot successfully"
        echo ""
        BoardUseUboot
    else
        echo "Flashed the kernel successfully"
        echo ""
        # execute any board specific steps required after the kernel is flashed
        BoardPostKernelFlashSteps
    fi

else
    echo "Error while flashing kernel, exiting..."
    AbnormalTermination
fi

popd &>/dev/null
exit 0
