#!/bin/bash

# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

GenericOptions="hyoz:S:b:Z:A:M:V:P:qRf:"
LinuxBoot=0
TargetType=
CompressKernel=0
BlUncompress=
ExtraOpts=
NvFlashExtraOpts=
FABCT=
FAMode=0
SkuId=0
FlashQNX=1
Quickboot=1
BootMedium="nor"
RcmMode=0
FAMode=0

DirBeingRunFrom=`dirname ${0}`
pushd $DirBeingRunFrom &> /dev/null

LINUX_SUDO_LDPATH="sudo LD_LIBRARY_PATH=${NvFlashPath}"

################## Utilities/Image Path ########################
## Default kernel hex address for qnx and linux.
Kern_Addr=A00800
NvFlashDir=.
ConfigFileDir=${NvFlashDir}
NvFlashPath=../../nvflash
BurnFlashBin=./burnflash.bin
Quickboot1=../../../bootloader/quickboot1.bin
Quickboot2=../../../bootloader/cpu_stage2.bin
Rcmboot=../../../bootloader/rcmboot.bin
CompressUtil=${NvFlashPath}/compress
Kernel=../../../kernel/zImage
QNX=../../../bsp/images/
MKBOOTIMG_BIN=${NvFlashPath}
MKBOOTIMG=loadimg.img
RCMBOOTIMG=rcmboot.img
FlashUpdateTool=update_sample
TegraFlashTool=tegraflash
SplashScreenBin=splash_screen.bin
StagePath=
FilesTempDir=`pwd`/.nvidia_tmp/
QNXUtils=`pwd`/.nvidia_qnx_tmp/
QNXTegraflashBuildScript=v7.build_tegraflash
QNXTegraflashIfsBin=ifs-nvidia-tegraflash.bin
QNXProcNToSMPInstr=procnto-smp-instr.sym

Usage()
{
    echo ""
    BoardHelpHeader
    echo ""
    echo "-h : Prints this message"
    echo "-B : Specify baud rate for kernel in bps (default is 115200 bps)"
    echo "-Z : Specify compression algorithm to be used for compressing kernel Image on host."
    echo "   : Supported options are: lzf, zlib and none."
    echo "     \"none\" is to use kernel zImage instead of Image i.e. no compression on host."
    echo "     Z option is mandatory but mutually exclusive with -k option."
    echo "-R : RCM Boot Support. Boot without Flashing Software(BCT is used from Media)"
    echo "-f <BCT> : FA mode Support. Don't use anything from Media(BCT needs to be provided, Use default keyword to use default BCT)"
    eval $zHelpOptions
    BoardHelpOptions
}

AbnormalTermination()
{
    #remove temp directories
    Cleanup
    popd &>/dev/null
    exit 10
}

GenericFunction()
{
    :
}

CreateBootImage()
{
    echo "Building Tegraflash Version Build Profiler..."

    cp -R ${StagePath}/* ${QNXUtils}/install/armle-v7/sbin/
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy utilities(required to be packaged) to qnx directory ##"
         AbnormalTermination
    fi

    mkifs -v -r ${QNXUtils}/install -v ${QNXUtils}/images/${QNXTegraflashBuildScript} ${QNXTegraflashIfsBin}
    if [ $? -ne 0 ] ; then
         echo "## Failed to create QNX IFS ##"
         AbnormalTermination
    fi

    echo "Creating Boot Image..."
    ${MKBOOTIMG_BIN}/mkbootimg --kernel ${QNXTegraflashIfsBin} --ramdisk NONE --kern_addr $Kern_Addr --cmdline "root=/dev/${RootDevice} rw rootwait ${BoardKernArgs} ${ExtraKernArgs} " ${BlUncompress} -o $MKBOOTIMG

    if [ "$?" -eq 0 ] ; then
        echo "Boot Image Created successfully"
    else
        echo "Error while creating Boot Image, exiting..."
        AbnormalTermination
    fi
}

CopyFlashFileSet1()
{
    # cp the required files

    cp $Quickboot1 $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy Quickboot1 binary ##"
         AbnormalTermination
    fi

    cp $Quickboot2 $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
        echo "## Failed to copy Quickboot2 binary ##"
        AbnormalTermination
    fi

    echo "Creating Boot Image ..."
    ${MKBOOTIMG_BIN}/mkbootimg --kernel ${Kernel} --ramdisk NONE --kern_addr $Kern_Addr --cmdline "root=/dev/${RootDevice} rw rootwait ${BoardKernArgs} ${ExtraKernArgs} " ${BlUncompress} -o $MKBOOTIMG
    if [ $? -ne 0 ] ; then
        echo "Error while creating Boot Image, exiting..."
        AbnormalTermination
    fi
}

CopyFlashFileSet2()
{
    # cp the required files for packaging into IFS

    cp $MKBOOTIMG $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy MKBOOTIMG ##"
         AbnormalTermination
    fi

    # copy BCT as tegraflash.bct
    cp $BCT $StagePath/flash.bct > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy BCT file $BCT ##"
         AbnormalTermination
    fi

    # copy BLOB as blob
    cp $BLOB $StagePath/blob > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy BLOB file $BLOB ##"
         AbnormalTermination
    fi

    cp $ConfigFile $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy $ConfigFile ##"
         AbnormalTermination
    fi

    # Store odm data in a file and copy on to target
    echo ${ODMData} > $StagePath/odmdata
    if [ ! -f $StagePath/odmdata ] ; then
        echo "## Failed to create $StagePath/odmdata with odm data info ##"
        AbnormalTermination
    fi

    cp $TegraFlashTool $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy $TegraFlashTool ##"
         AbnormalTermination
    fi

    cp $FlashUpdateTool $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy $FlashUpdateTool ##"
         AbnormalTermination
    fi

    cp $SplashScreenBin $StagePath > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy $SplashScreenBin ##"
         AbnormalTermination
    fi

    if [ ! -z "${NvFlashExtraOpts}" ] && [ -f "${BurnFlashDir}/blob" ] ; then
         cp $BurnFlashDir/blob $StagePath > /dev/null
         NvFlashExtraOpts=" --priv-blob $FilesTempDir/blob "
         if [ $? -ne 0 ] ; then
                echo "## Failed to copy $BurnFlashDir/blob ##"
                AbnormalTermination
         fi
    fi

    echo "## Files copied to staging directory $StagePath ##"
}

CopyQNXUtils()
{
    # cp the required files

    cp -R $QNX/../* $QNXUtils > /dev/null
    if [ $? -ne 0 ] ; then
         echo "## Failed to copy QNX Utils ##"
         AbnormalTermination
    fi
    echo "## QNX Utils copied to staging directory $QNXUtils ##"
}

CreateStagingDir()
{
    # Create Temp flashing directory
    StagePath=$FilesTempDir

    mkdir -p $StagePath
    if [ $? -ne 0 ]; then
        echo "## Failed to create staging directory ##"
        AbnormalTermination
    fi

    mkdir -p $QNXUtils
    if [ $? -ne 0 ] ; then
        echo "## Failed to create QNX utils directory ##"
        AbnormalTermination
    fi

}

CopyStaging()
{
    CreateStagingDir
    CopyFlashFileSet1
    CopyFlashFileSet2
    CopyQNXUtils
}

Cleanup()
{
    rm -rf $StagePath > /dev/null
    rm -rf $QNXUtils > /dev/null
}

# Console option to enable/disable serial and framebuffer console
CompressionAlgo=
ExtraNvflashArgs=

if [ -z zHelpOptions ] ; then
    zHelpOptions=GenericFunction
fi

if [ -z zHandler ] ; then
    zHandler=GenericFunction
fi

################## Internal Development ########################

if [ -z $BurnFlashDir ] ; then
    BurnFlashDir=.
fi

if [ -f "${BurnFlashDir}/internal_setup.sh" ] ; then
    . ${BurnFlashDir}/internal_setup.sh
    # In case internal_setup.sh exists set temporary directory paths differently
    FilesTempDir=${NV_OUTDIR}/.nvidia_tmp/
    QNXUtils=${NV_OUTDIR}/.nvidia_qnx_tmp/
fi

if [ -f "${NvFlashDir}/burnflash_helper.sh" ] ; then
    . "${NvFlashDir}/burnflash_helper.sh"
else
    echo "burnflash_helper.sh not found"
    exit -1
fi

echo "burnflash.sh for $TargetType board"

while getopts "${GenericOptions}${BoardOptions}" Option ; do
    case $Option in
        h)  Usage
            # Exit if only usage (-h) was specfied.
            if [[ $# -eq 1 ]] ; then
                AbnormalTermination
            fi
            exit 0
            ;;
        z)  ZFile=$OPTARG
            if [ -e $ZFile ] ; then
                eval $zHandler $ZFile
            else
                echo "File $ZFile does not exist"
                exit -1
            fi
            ;;
        A)  BSer=$OPTARG
            eval $AHandler $BSer
            ;;
        M)  MInterface=$OPTARG
            MId=\${$OPTIND}
            eval $MHandler $MInterface $MId
            shift 1
            ;;
        V)  SVer=$OPTARG
            eval $VHandler $SVer
            ;;
        P)  PInfo=$OPTARG
            PVer=\${$OPTIND}
            eval $pHandler $PInfo $PVer
            shift 1
            ;;
        S)  SkuId=$OPTARG
            ;;
        o)  Quickboot=0
            ;;
        Z)  CompressionAlgo=$OPTARG
            if [ $OPTARG = "lzf" ] || [ $OPTARG = "zlib" ] ; then
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
        q)  FlashQNX=1
            ;;
        R)  RcmMode=1
            ;;
        f)  FAMode=1
            FABCT=$OPTARG
            ;;
        b)  BootMedium=$OPTARG
            ;;
        *)  BoardParseOptions
            ;;
    esac
done
shift $(($OPTIND-1))

################## All options are needed ####################
# Variables do not assume default values to avoid confusion
# We could expect strict ordering and do away with -f/-b/-n options
if [ -z "$TargetType" ] ; then
    echo "Missing Parameter"
    Usage
    AbnormalTermination
fi
if [ ${RcmMode} -eq 1 ] && [ ${FAMode} -eq 1 ] ; then
    echo "FA Mode and RCM mode can't be specified at same time"
    Usage
    AbnormalTermination
fi
if [ ${RcmMode} -eq 1 ] || [ ${FAMode} -eq 1 ] ; then
    if [ ${CompressKernel} -eq 1 ] ; then
        echo "Compression support not avaliable with Rcm Boot"
        Usage
        AbnormalTermination
    fi
    if [ $Quickboot -eq 0 ] ; then
        echo "Quickboot is required for the RCM boot"
        Usage
        AbnormalTermination
    fi
fi

if [ -n "$ExtraOpts" ] ; then
    eval $iHandler
fi

if [ ${Quickboot} -ne 1 ] && [ ${CompressKernel} -eq 1 ] ; then
    echo "-Z option is supported only with Quickboot bootloader"
    Usage
    AbnormalTermination
fi

#### Empirical Check for Target availability ####
lsusb |grep -i nvidia
if [ $? != 0 ] ; then
    echo "No recovery-target found; Make sure the target device is connected to the"
    echo "host and is in recovery mode. Exiting"
    exit -1
fi

# call function for board specific validation of parameters
BoardValidateParams

################## Internal Development ########################
Kernel=${QNX}/${QNXFileName}

# execute any board specific steps required before flashing
BoardPreflashSteps

#################### Flash the Kernel ############################
if [ ! -f ${Kernel} ] ; then
    echo "Error: Kernel image not present"
    AbnormalTermination
fi

if [ ${CompressKernel} -eq 1 ] ; then

    CompressUtil=${CompressUtil}_${CompressionAlgo}
    echo "Compressing kernel image..."
    if [ ${CompressionAlgo} = "zlib" ] ; then
        echo "Using ZLIB compression algorithm to compress the image"
        ${CompressUtil} ${Kernel} > ${Kernel}.def
        if [ $? -ne 0 ] ; then
            echo "Failed to compress kernel image"
            AbnormalTermination
        fi
        BlUncompress="--bluncompress zlib"
    else
        echo "Using LZF compression algorithm to compress the image"
        if [ ${BootMedium} = "nor" ] ; then    # Use block size of (16K - 1) for nor and (2K - 1) for others.
            ${CompressUtil} -b 16383 -c ${Kernel} -r > ${Kernel}.def
            if [ $? -ne 0 ]; then
                echo "Failed to compress kernel image"
                AbnormalTermination
            fi
        else
            ${CompressUtil} -b 2047 -c ${Kernel} -r > ${Kernel}.def
            if [ $? -ne 0 ] ; then
                echo "Failed to compress kernel image"
                AbnormalTermination
            fi
        fi
        BlUncompress="--bluncompress lzf"
    fi

    echo "Kernel Compression successful"

    Kernel=${Kernel}.def
fi

#remove temp directories
Cleanup

if [ ! $RcmMode -eq 1 ] ; then
    CopyStaging
    # create image which in turn contains binaries required for flashing boot medium on target
    CreateBootImage
else
    CreateStagingDir
    CopyFlashFileSet1
fi

# Flash the target
if [ $FAMode -eq 1 ] ; then
    if [ "$FABCT" == "default" ] ; then
        FABCT=$BCT
    fi
    if [ -f ${FABCT} ] ; then
        cat $Rcmboot $MKBOOTIMG > $RCMBOOTIMG
        ${LINUX_SUDO_LDPATH} ${NvFlashPath}/nvflash --bct ${FABCT} --setbct --configfile ${ConfigFile} --create --bl $RCMBOOTIMG --setentry 0x84008000 0x84008000 --odmdata `printf "0x%0x" ${ODMData}` --go
    else
        echo "File ${FABCT} not found. Terminating"
        AbnormalTermination
    fi
else
    cat $Rcmboot $MKBOOTIMG > $RCMBOOTIMG
    sync
    ${LINUX_SUDO_LDPATH} ${NvFlashPath}/nvflash --bl ${RCMBOOTIMG} --setentry 0x84008000 0x84008000 ${ExtraNvflashArgs} --go
fi
cmd_output=$?

###############################################################################################3

# Clean up
if [ -n "$ExtraOpts" ] ; then
    eval $CleanUpHandler
fi

if [ ${Quickboot} -eq 1 ] ; then
    rm $MKBOOTIMG
fi

if [ ! $RcmMode -eq 1 ] ; then
    if [ -f ${QNXTegraflashIfsBin} ] ; then
        rm ${QNXTegraflashIfsBin}
    fi

    if [ -f "${PWD}/${QNXProcNToSMPInstr}" ] ; then
        rm ${PWD}/${QNXProcNToSMPInstr}
    fi
fi

# board specific steps required after nvflash is done

#clean up - remove flashing directory
Cleanup

if [ "$cmd_output" -eq 0 ] ; then
    rm $RCMBOOTIMG
    echo "## Data transfer completed. ##"
    echo "## Please check console output for more details ##"
else
    echo "## Data transfer failed. Exiing ##"
    AbnormalTermination
fi

popd &>/dev/null
exit 0
