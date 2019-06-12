#!/bin/bash

# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

NfsPort=
RootDevice=
RootFsPath=
FsType=
ExtraKernArgs=
NvFlashExtraOpts=
CompressKernel=0
FlashQNX=0
SkuId=0
Quickboot=1
RcmMode=0
FAMode=0
EfsDir=
EfsDevice=
BootDevice=
MkEfsPath=
UsePflash=0
Cygwin=0
CreateFSOnTarget=0

# Console option to enable/disable serial and framebuffer console
KernelConsoleArgs="console=ttyS0,115200n8"

DirBeingRunFrom=`dirname ${0}`
pushd $DirBeingRunFrom &>/dev/null

################## Utilities/Image Path ########################
## Default kernel hex address for qnx and linux.
Kern_Addr=A00800
NvFlashDir=.
ConfigFileDir=${NvFlashDir}
NvFlashPath=../../nvflash
Quickboot1=../../../bootloader/quickboot1.bin
Quickboot2=../../../bootloader/cpu_stage2.bin
CompressUtil=${NvFlashPath}/compress
Kernel=../../../kernel/zImage
DtbPath=../../../kernel/
UncompressedKernel=../../../kernel/Image
QNX=../../../bsp/images/
MKBOOTIMG_BIN=${NvFlashPath}
MKBOOTIMG=loadimg.img
FlashUpdateTool=update_sample
TegraFlashTool=tegraflash
PFlashTool=pflash
fileSystemImage=rootfs.Image
fileSystemTarball=rootfs.tgz
efsImage=efs.img

if [ "`uname -o`" == "Cygwin" ]; then
     echo "Running from Cygwin environment"
     Cygwin=1
fi

if [ $Cygwin -eq 1 ];then
     PING_CMD="ping -n"
     SUDO_CMD=""
     SUDO_S_CMD=""
else
     PING_CMD="ping -c"
     SUDO_CMD="sudo"
     SUDO_S_CMD="sudo -S"
fi

GenericFunction()
{
    :
}

GenericOptions="E:P:A:M:V:f:z:R:p:shyFon:S:r:c:Z:B"

Usage()
{
     echo ""
     echo "-h : Prints this message"
     echo "-n : Selects nfs port (linux targets only)"
     echo "-r : Selects root device to mount (linux targets only). Example: mtdblock1 for mtd, sda1 for USB harddrive."
     echo "-c : Additional kernel args to pass to kernel during bootup (linux targets only). "
     echo "-Z : Specify compression algorithm to be used for compressing kernel Image on host."
     echo "   : Supported options are: lzf, zlib and none."
     echo "     \"none\" is to use kernel zImage instead of Image i.e. no compression on host."
     echo "     Z option is mandatory but mutually exclusive with -k option."
     echo "-R : Root file system path"
     echo "-f : Root file system Type ext2, squashfs for orca2 platform"
     echo "     ext2, ext3, ext4 for P1859"
     echo "-E : EFS file system Directory Path"
     echo "-s : NOR Parallel flashing (pflash)"
     eval $zHelpOptions
     TransportHelpOptions
     BoardHelpOptions
}

if [ -z zHelpOptions ];then
    zHelpOptions=GenericFunction
fi

if [ -z zHandler ];then
    zHandler=GenericFunction
fi

GetEfsOffset()
{
    #Fix Me !! - Assuming 3 Mtd partitions exist.
    #System-Rootfs-EFS- System and Rootfs partiion start offset and size remains fixed.
    #EFS occupies remaining space available on NOR.
    #Calculate Root Partition Size
    RootPartSize=`echo $BoardKernArgs | cut -d ' ' -f2 | awk -F '[:,]' '{print $3}'| cut -d 'K' -f1`
    #Calculate Root Partition Offset
    RootPartOff=`echo $BoardKernArgs | cut -d ' ' -f2 | awk -F '[:,]' '{print $3}'| cut -d 'K' -f2 | cut -d '@' -f2`

    #Calculate EFS Partition Offset
    EfsPartOff=$(($RootPartSize + $RootPartOff))
    EfsOffset=$(($EfsPartOff*1024))

    #update Extra Kernel Adrgs, add efsoffset in commandline
    ExtraKernArgs="${ExtraKernArgs} efsoffset=${EfsOffset}"

    #Overwrite RootDevice to mtdblock1 and EFSDevice to mtdblock2 if EFS is present
    RootDevice=mtdblock1
    EfsDevice=mtdblock2

}

AbnormalTermination()
{
    #remove temp directories
    Cleanup
    popd &>/dev/null
    exit 10
}

CreateBootImage()
{
     echo "Creating Boot Image..."
     if [ ! -z  "$EfsDir" ]; then
        GetEfsOffset
     fi

     if [ ! -z  "$NfsPort" ]; then
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
}

#Main body

if [ -z $BurnFlashDir ]; then
    BurnFlashDir=.
    MkEfsPath=${BurnFlashDir}
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

if [ -f "${BurnFlashDir}/tegraflash_nfs.sh" ]; then
     . ${BurnFlashDir}/tegraflash_nfs.sh
else
     echo "tegraflash_nfs.sh not found"
     exit -1
fi

while getopts "${GenericOptions}${BoardOptions}${TransportOptions}" Option ; do
  case $Option in
     h) Usage
        # Exit if only usage (-h) was specfied.
        if [[ $# -eq 1 ]] ; then
           AbnormalTermination
        fi
        exit 0
        ;;
     f) FsType=$OPTARG
        ;;
     E) EfsDir=$OPTARG
	;;
     s) UsePflash=1
	;;
     R) RootFsPath=$OPTARG
        ;;
     r) RootDevice=$OPTARG
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
     S) SkuId=$OPTARG
        ;;
     o) Quickboot=0
        ;;
     Z) CompressionAlgo=$OPTARG
           if [ $OPTARG = "lzf" ] || [ $OPTARG = "zlib" ]; then
              CompressKernel=1
           else
              if [ $OPTARG != "none" ] ; then
                 echo "Invalid option for -Z. Supported values are lzf, zlib or none."
                 exit -1
              fi
           fi
        ;;
     q) FlashQNX=1
        ;;
     b) BootDevice=$OPTARG
        ;;
     *) TransportParseOptions
   esac
done
shift $(($OPTIND-1))

if [ -n "$ExtraOpts" ]; then
   eval $iHandler
fi

if [ ! ${FlashQNX} -eq 1 ]; then
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


if [ ! -z "$NfsPort" ] && [ ! -z "$RootDevice" ]; then
    echo "Exclusive options used together"
    Usage
    AbnormalTermination
fi
if [ ${Quickboot} -ne 1 ] && [ ${CompressKernel} -eq 1 ]; then
    echo "-Z option is supported only with Quickboot bootloader"
    Usage
    AbnormalTermination
fi


#################### Flash the Kernel ############################
if [ ! -f ${Kernel} ]; then
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
        if [ ${BootDevice} = "nor" ] || [ ${BootDevice} = "ide" ]; then    # Use block size of (16K - 1) for nor and (2K - 1) for others.
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

#
############################################# mkbootimg creation #######################################

#Main body

trap AbnormalTermination SIGHUP SIGINT SIGTERM

#make sure all fs options are given together.
if [ -n "$RootFsPath" -a -n "$FsType" -a  -z "$RootDevice" ]  ||
   [ -z "$RootFsPath" -a -n "$FsType" ] ||
   [ -n "$RootFsPath" -a -z "$FsType" ]; then 
          echo "Incorrect options (-R -r & -f options should be used together for rootfs flashing)"
          Usage
          AbnormalTermination
fi

# call function for board specific validation of parameters
BoardValidateParams

#transport Specific validation parameters.
TransportValidateParams

#check NfsPort and Root device options are mutually exclusive
if [ ! -z  "$NfsPort" ] &&  [ ! -z  "$RootDevice" ]  ; then
     echo "Incorrect options (-n & -R can not be used together)"
     Usage
     AbnormalTermination
fi

#check pflash only used with Nor
if [ "$UsePflash" -eq 1 ] && [ "$BootDevice" != "NOR" ] ; then
     echo "Incorrect options (pflash only supported for NOR flashing)"
     Usage
     AbnormalTermination
fi

#remove temp directories
Cleanup

#create image
CreateBootImage

#copy files to staging directory.
CopyStaging

#Flash nvinternal components, bootloader, kernel
FlashSystemPartition

#prepare file system
if [ ${CreateFSOnTarget} -eq 0 ] && [ -n "${RootFsPath}" ] && [ -n "${RootDevice}" ]; then
     if [ $FsType != "ext2" ] && [ $FsType != "squashfs" ]; then
          echo "## invalid file system type ##"
          Usage
          AbnormalTermination
     fi

    #Create File system Image
    echo "Creating Root FileSystem"

    #Update /etc/fstab
    ${SUDO_CMD} cp ${RootFsPath}/etc/fstab ${RootFsPath}/etc/fstab.orig
    echo "/dev/$RootDevice     /     $FsType    ro      0     0" | ${SUDO_CMD} tee -a ${RootFsPath}/etc/fstab >>/dev/null
    echo "tmpfs            /var/log      tmpfs      rw      0     0" | ${SUDO_CMD} tee -a ${RootFsPath}/etc/fstab >>/dev/null
    echo "tmpfs            /tmp      tmpfs      rw      0    0"  | ${SUDO_CMD} tee -a ${RootFsPath}/etc/fstab >>/dev/null

    if [ $FsType == "ext2" ]; then

        # Get the number of inodes required in the ext2 filesystem
        inodecount=`${SUDO_CMD} find ${RootFsPath} | wc -l`
        blocksize=4096

        if [ $Cygwin -eq 1 ]; then
             # In ext2 filesystem, first 12 inodes are reserved - account for it
             let "inodecount=inodecount+12"

             blockcount=`du -s --block-size ${blocksize} ${RootFsPath} | cut -f1`
             #inodes use up blocks too
             let "blockcount=blockcount+inodecount+100"

             # extra 2% overhead(for blockcount and inodecount) the ext2 filesystem needs.
             blockcount=$(($blockcount+(($blockcount+49)/50)))

             # there needs to be a minimum of 60 blocks to create a valid filesystem using mke2fs
             if [ "$blockcount" -lt 80 ]
             then
                 blockcount=80
             fi

             ${SUDO_CMD} rm $fileSystemImage >& /dev/null

             #Create zeroed file equal to size of rootfs
             dd if=/dev/zero of=$fileSystemImage count=$blockcount bs=$blocksize status=noxfer >& /dev/null
             chmod 666 $fileSystemImage

             #format ext2 filesystem
             echo "y" | mke2fs -m 2 -q -F -T largefile -b ${blocksize} -N ${inodecount} $fileSystemImage
             if [ $? -ne 0 ]; then
                 echo "## Creating ext2 filesystem failed ##"
                 AbnormalTermination
             fi
             # copy the files into the ext2 filesystem
             echo "y" | e2fsimage -f ${fileSystemImage}  -d ${RootFsPath} -n -i

        else
             blockcount=`sudo du -s --block-size ${blocksize} ${RootFsPath} | cut -f1`
             #inodes use up blocks too
             let "blockcount=blockcount+inodecount"

             # extra 2% overhead(for blockcount and inodecount) the ext2 filesystem needs.
             blockcount=$(($blockcount+(($blockcount+49)/50)))

             # In ext2 filesystem, first 10 inodes are reserved - account for it
             let "inodecount=inodecount+10"

             # there needs to be a minimum of 60 blocks to create a valid filesystem using mke2fs
             if [ "$blockcount" -lt 60 ]
             then
                blockcount=60
              fi

             ${SUDO_CMD} rm $fileSystemImage >& /dev/null

             #Create zeroed file equal to size of rootfs
             dd if=/dev/zero of=$fileSystemImage count=$blockcount bs=$blocksize status=noxfer >& /dev/null
             chmod 666 $fileSystemImage

             #format ext2 filesystem
             echo "y" | mkfs.ext2 $fileSystemImage >/dev/null
             if [ $? -ne 0 ]; then
                 echo "## Creating ext2 filesystem failed ##"
                 AbnormalTermination
             fi
             ${SUDO_CMD} umount -f .tmp_mnt >& /dev/null
             ${SUDO_CMD} rm -rf .tmp_mnt >& /dev/null
             mkdir .tmp_mnt
             if [ $? -ne 0 ]; then
                 echo "## Failed to create temporary mount directory ##"
                 AbnormalTermination
             fi

             #loop mount ext2 formatted image
             ${SUDO_CMD} mount -o loop ${fileSystemImage} .tmp_mnt
             if [ $? -ne 0 ]; then
                 echo "## Failed to mount ##"
                 AbnormalTermination
             fi

             #copy contents of root file system to mounted image
             ${SUDO_CMD} cp -R $RootFsPath/* .tmp_mnt/
             if [ $? -ne 0 ]; then
                 echo "## Failed to copy root files ##"
                 ${SUDO_CMD} umount -f .tmp_mnt/
                 AbnormalTermination
             fi

             # remove the unnecessary files
             ${SUDO_CMD} rm -rf .tmp_mnt/etc/mtab

             #unmounted the ext2 loop mounted image
             ${SUDO_CMD} umount -f .tmp_mnt
             if [ $? -ne 0 ]; then
                 echo "## Failed to unmount the created root file system ##"
                 AbnormalTermination
             fi
        fi
    elif [ $FsType = "squashfs" ]; then
             echo "## Preparing Squashfs File system ##"
             ${SUDO_CMD} mksquashfs $RootFsPath $fileSystemImage -e $RootFsPath/etc/mtab
    fi

    if [ "$BootDevice" == "NOR" ]; then
         #validate rootfs image size is less than rootfs partition
         fileSize=$(stat -c '%s' $fileSystemImage)
         PartSizeKB=`echo $BoardKernArgs | cut -d ' ' -f2 | awk -F '[:,]' '{print $3}'| cut -d 'K' -f1`
         PartSize=$(($PartSizeKB*1024))
         if [ $fileSize -gt $PartSize ] ; then
              echo "Rootfile system requires bigger partition. RootFs Size - $fileSize Partition size - $PartSize      $fileSystemImage "
            AbnormalTermination
         fi
    fi
    #restore the original /etc/fstab
    ${SUDO_CMD} mv ${RootFsPath}/etc/fstab.orig  ${RootFsPath}/etc/fstab

    if [ ! -z  "$EfsDir" ]; then
            ${SUDO_CMD} rm -rf $efsImage
            echo "## Adding efs filesystem... ##"
            ${MkEfsPath}/mkefs $efsImage $EfsDir
            fileSize=$(stat -c '%s' $efsImage)
    fi

    if [ $UsePflash -eq 0 ] ; then
         #flash file system image created
         FlashFileSystem

         if [ ! -z  "$EfsDir" ]; then
            #flash EFS file system image created
            FlashEfsFileSystem
         fi
    else
         if [ "$BootDevice" == "NOR" ]; then
               echo "## Using pflash for flashing. ##"
               PFlashRawImage
         else
               echo "## Pflash support only available for NOR device ##"
               AbnormalTermination
         fi
    fi
else
   if [ -n "${RootFsPath}" ] && [ -n "${FsType}" ]; then
       CreateAndFlashFileSystem
   fi
fi

${SUDO_CMD} rm -f $fileSystemImage >& /dev/null
${SUDO_CMD} rm -rf .tmp_mnt >& /dev/null

#clean up - remove flashing directory
Cleanup
#echo "$Password" | plink -t $UserName@$IpAddr -pw $Password sudo rm -rf $TargetTempDir  > /dev/null

echo "## Flashing completed Successfully ##"
