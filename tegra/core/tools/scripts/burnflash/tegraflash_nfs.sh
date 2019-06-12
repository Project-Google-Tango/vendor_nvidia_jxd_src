#!/bin/bash

# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.


IpAddr=
NfsRootPath=
DirPath=
UserName=
Password=
TargetTempDir=/root/.nvidia_tmp
TransportOptions="i:U:W:N:"

TransportHelpOptions()
{
     echo "-i : Target Ip Address"
     echo "-U : Target UserName"
     echo "-W : Target Password"
     echo "-N : NFS root directory path"
}
TransportParseOptions()
{
       case $Option in
          i) IpAddr=$OPTARG
             ;;
          U) UserName=$OPTARG
             ;;
          W) Password=$OPTARG
             ;;
          N) NfsRootPath=$OPTARG
             ;;
          *) BoardParseOptions
             ;;
        esac
}

TransportValidateParams()
{
   #Validate required packages exist
   if [ $Cygwin -eq 0 ];then
        sudo dpkg-query -Wf'${db:Status-abbrev}' putty-tools > /dev/null
        if [ $? -ne 0 ]; then
            echo "## Missing putty-tools  package ##"
            sudo apt-get install  putty-tools
        fi
        sudo dpkg -s  nmap > /dev/null
        if [ $? -ne 0 ]; then
            echo "## Missing nmap  package ##"
            sudo apt-get install  nmap
        fi
    fi

   #mandatory options
   if [ -z $IpAddr ]; then
       echo "## IP Address not provided. Trying to auto-probe the same ##"
       count=0
       duration=0
       while [ $count -le 1 ] && [ $duration -le 60 ];do
          sleep 1
          duration=$(($duration + 1 ))
          count=`nmap -sn 10.0.0.1/24 | grep "Nmap scan report" | wc -l`
       done

       if [ $duration -eq 60 ];then
          echo "Error in getting network IP. Exiting..."
          Usage
          AbnormalTermination
          exit
       fi

       IpAddr=`nmap -sn 10.0.0.1/24 | grep "Nmap scan report" | tail -n 1 | cut -d ' ' -f 5`
       echo Probe IP to $IpAddr
   fi

   #check network interface to target.
   ${PING_CMD} 1 $IpAddr &> /dev/null

   if [ $? -ne 0 ]; then
       echo "## Cannot reach target system at $IpAddr ##"
       AbnormalTermination
   fi

   if [ -z $UserName ];then
        echo "## UserName not specified. Using default login name \`nvidia\` ##"
        UserName="nvidia"
   fi

   if [ -z $Password ];then
        echo "## Password not specified. Using default password \`nvidia\` ##"
        Password="nvidia"
   fi

   if [ -z $NfsRootPath ]; then
        echo "## NFS root directory not provided. AutoProbing the same ##"
        mount_point=` echo "y" | plink -t $UserName@$IpAddr -pw $Password mount | grep -w nfs | head -n 1 | cut -d ':' -f 2 | cut -d ' ' -f1`
        echo Probed mount point to: $mount_point
        NfsRootPath=$mount_point
    fi
}

CopyFiles()
{
    #cp the required files
    ${SUDO_CMD} cp $Quickboot1 $DirPath >/dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to copy Quickboot1 binary ##"
         AbnormalTermination
    fi

    ${SUDO_CMD} cp $Quickboot2 $DirPath >/dev/null
    if [ $? -ne 0 ]; then
        echo "## Failed to copy Quickboot2 binary ##"
        AbnormalTermination
    fi

    ${SUDO_CMD} cp $MKBOOTIMG $DirPath >/dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to copy MKBOOTIMG ##"
         AbnormalTermination
    fi

    ${SUDO_CMD} cp $BCT $DirPath >/dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to copy BCT file $BCT ##"
         AbnormalTermination
    fi

    ${SUDO_CMD} cp $ConfigFile $DirPath >/dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to copy $ConfigFile ##"
         AbnormalTermination
    fi

    ${SUDO_CMD} cp $TegraFlashTool $DirPath >/dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to copy $TegraFlashTool ##"
         AbnormalTermination
    fi

    ${SUDO_CMD} cp $FlashUpdateTool $DirPath >/dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to copy $FlashUpdateTool ##"
         AbnormalTermination
    fi

    if [ $UsePflash -eq 1 ] ; then
         ${SUDO_CMD} cp $PFlashTool $DirPath >/dev/null
         if [ $? -ne 0 ]; then
                echo "## Failed to copy $PFlashTool ##"
                AbnormalTermination
         fi
    fi

    if [ ! -z "${NvFlashExtraOpts}" ] && [ -f "${BurnFlashDir}/blob" ] ; then
         ${SUDO_CMD} cp $BurnFlashDir/blob $DirPath >/dev/null
         NvFlashExtraOpts=" --priv-blob $TargetTempDir/blob "
         if [ $? -ne 0 ]; then
                echo "## Failed to copy $BurnFlashDir/blob ##"
                AbnormalTermination
         fi
    fi
    if [ -f "${DtbPath}/${DtbFileName}" ] ; then
        ${SUDO_CMD} cp ${DtbPath}/${DtbFileName} $DirPath/tegra.dtb > /dev/null
    fi
    echo "## Files copied to staging directory $DirPath ##"

}

PFlashRawImage()
{
    rawImage=pflashImage.img
    #Fix Me !! - Assuming 3 Mtd partitions exist.
    #System-Rootfs-EFS- System and Rootfs partiion start offset and size remains fixed.
    #EFS occupies remaining space available on NOR.
    #Calculate Root Partition Size
    RootPartSize=`echo $BoardKernArgs | cut -d ' ' -f2 | awk -F '[:,]' '{print $3}'| cut -d 'K' -f1`
    #Calculate Root Partition Offset
    RootPartOff=`echo $BoardKernArgs | cut -d ' ' -f2 | awk -F '[:,]' '{print $3}'| cut -d 'K' -f2 | cut -d '@' -f2`

    if [ -z "$RootPartOff" ] ; then
       echo "Incorrect partitions"
       echo $BoardKernArgs
       exit
    fi

    echo "## Creating concatenated image ##"
    #Create a concatinated single image
    #Step 1 - Read back the system partition - /dev/mtd0
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} "dd if=/dev/mtd0 of=$TargetTempDir/$rawImage bs=1K count=$RootPartOff status=noxfer" >& /dev/null

    if [ $? -ne 0 ] && [ ! -f $DirPath/$rawImage  ] ; then
         echo "## Failed to read back system partition image ##"
         AbnormalTermination
    fi
    #Step 2 - Append Root file system image at the end of system image
    if [ ! -f $fileSystemImage ] ; then
         echo "## Missing Root file system image $fileSystemImage ##"
         AbnormalTermination
    fi
    if [ -z  "$EfsDir" ]; then
          dd if=$fileSystemImage of=$DirPath/$rawImage bs=1K seek=$RootPartOff status=noxfer >& /dev/null
    else
          dd if=$fileSystemImage of=$DirPath/$rawImage bs=1K seek=$RootPartOff count=$RootPartSize  status=noxfer >& /dev/null
    fi

    #step 3 - Append Efs File system Image
     if [ ! -z  "$EfsDir" ]; then
            if [ ! -f  "$efsImage" ]; then
                 echo "## Missing Efs file system image ##"
                 AbnormalTermination
            fi

            #Calculate EFS Partition Offset
            EfsPartOff=$(($RootPartSize + $RootPartOff))

            fileSize=$(stat -c '%s' $DirPath/$rawImage)

            dd if=$efsImage of=$DirPath/$rawImage bs=1K seek=$EfsPartOff  status=noxfer >& /dev/null

      fi

     #step 4 - Check the size of rawImage
     fileSize=$(stat -c '%s' $DirPath/$rawImage)
     echo "Pflash Image Size - $fileSize"

     #step 5 - Split Image in 2G parts
     GB2inbytes=$((2048 * 1024 * 1024 ))
     GB2inMB=2048
     count=0
     NoOfFiles=$(($fileSize/$GB2inbytes))
     fileList=""

     if [ $fileSize -gt $GB2inbytes ] ; then
          echo "Splitting Files"
          split --bytes=$GB2inbytes --numeric-suffixes --suffix-length=1 --verbose $DirPath/$rawImage $DirPath/$rawImage.part.
     else
          mv $DirPath/$rawImage $DirPath/$rawImage.part.0
     fi

     if [ $(($fileSize % $GB2inbytes)) -eq 0 ] ; then
             NoOfFiles=$NoOfFiles
     else
             NoOfFiles=$(($NoOfFiles + 1 ))
     fi

     #step 6 - Pflash the files

     while [ $count -lt $NoOfFiles ] ; do
       fileList="$fileList $TargetTempDir/$rawImage.part.$count"
       count=$(($count + 1))
     done

     #step 7
     echo "## Executing Pflash on target ##"
     echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/pflash $NoOfFiles $fileList

     if [ $? -ne 0 ]; then
        echo "## Failed to pflash the target ##"
        AbnormalTermination
     else
        echo "## Pflash Success. ##"
     fi

}


FlashSystemPartition()
{
    echo y | plink -t $UserName@$IpAddr -pw $Password exit

    echo "#######"
    echo "Programming Nvidia Partitions"
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/tegraflash --create-pt --bct $TargetTempDir/$(basename "$BCT" ".bct").bct --cfg $TargetTempDir/$(basename "$ConfigFile" ".cfg").cfg --odmdata `printf "0x%0x" $ODMData` ${NvFlashExtraOpts} --dev $BootDevice >/dev/null

    if [ $? -ne 0 ]; then
        echo "Failed"
        AbnormalTermination
    else
        echo "Success."
    fi

    echo "#######"
    echo "Flashing Primary Bootloader partition"
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/update_sample --BlPrimary --Qb1Path  $TargetTempDir/$(basename "$Quickboot1" ".bin").bin --Qb2Path $TargetTempDir/$(basename "$Quickboot2" ".bin").bin --dev $BootDevice >/dev/null

    if [ $? -ne 0 ]; then
         echo "Failed"
         AbnormalTermination
    else
         echo "Success."
    fi

    echo "#######"
    echo "Flashing Recovery Bootloader partition"
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/update_sample --BlRecovery --Qb1Path  $TargetTempDir/$(basename "$Quickboot1" ".bin").bin --Qb2Path $TargetTempDir/$(basename "$Quickboot2" ".bin").bin --dev $BootDevice > /dev/null

    if [ $? -ne 0 ]; then
         echo "Failed"
         AbnormalTermination
    else
         echo "Success."
    fi

    echo "#######"
    echo "Flashing Primary Kernel partition"
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/update_sample --KernPrimary --KernPath  $TargetTempDir/$MKBOOTIMG --dev $BootDevice >/dev/null

    if [ $? -ne 0 ]; then
         echo "Failed"
         AbnormalTermination
    else
         echo "Success."
    fi

    echo "#######"
    echo "Flashing Recovery Kernel partition"
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/update_sample --KernRecovery --KernPath  $TargetTempDir/$MKBOOTIMG --dev $BootDevice > /dev/null

    if [ $? -ne 0 ]; then
         echo "Failed"
         AbnormalTermination
    else
         echo "Success."
    fi

    is_dtb_present=`grep -w "DTB_PRIMARY" ${ConfigFile}`
    if [ -n "${is_dtb_present}" ]; then
        echo "Flashing DTB Primary partition"
        echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/update_sample --Partition DTB_PRIMARY --PartImgPath $TargetTempDir/tegra.dtb --dev $BootDevice > /dev/null

        if [ $? -ne 0 ]; then
            echo "Failed"
            AbnormalTermination
        else
            echo "Success."
        fi
    fi
    is_dtb_present=`grep -w "DTB_RECOVERY" ${ConfigFile}`
    if [ -n "${is_dtb_present}" ]; then
        echo "Flashing DTB Recovery partition"
        echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_S_CMD} $TargetTempDir/update_sample --Partition DTB_RECOVERY --PartImgPath $TargetTempDir/tegra.dtb --dev $BootDevice > /dev/null

        if [ $? -ne 0 ]; then
            echo "Failed"
            AbnormalTermination
        else
            echo "Success."
        fi
    fi
}

FlashFileSystem()
{
    #cp prepared rootfs image to NFS directory for flashing
    ${SUDO_CMD} cp $fileSystemImage $DirPath
    if [ $? -ne 0 ]; then
         echo "## Failed to copy file system image to $DirPath ##"
         AbnormalTermination
    fi

    #Remove the temp local directory and file.
    ${SUDO_CMD} rm -f $fileSystemImage  >& /dev/null
    ${SUDO_CMD} rm -rf .tmp_mnt >& /dev/null

    echo " "
    echo "## Flashing Root file system ##"
    echo "## This will take some time ##"

    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} "dd if=$TargetTempDir/$fileSystemImage of=/dev/${RootDevice} status=noxfer" >& /dev/null

    if [ $? -ne 0 ]; then
         echo "Failed to write filesystem image to device"
         AbnormalTermination
    fi

    echo "## Root filesystem flashing complete ##"
}

FlashEfsFileSystem()
{
    #cp prepared rootfs image to NFS directory for flashing
    ${SUDO_CMD} cp $efsImage $DirPath
    if [ $? -ne 0 ]; then
         echo "## Failed to copy efs file system image to $DirPath ##"
         AbnormalTermination
    fi

    #Remove the temp local directory and file.
    ${SUDO_CMD} rm -f $efsImage >& /dev/null
    ${SUDO_CMD} rm -rf .tmp_mnt >& /dev/null


    echo " "
    echo "## Flashing EFS file system ##"
    echo "## This will take some time ##"

    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} "dd if=$TargetTempDir/$efsImage of=/dev/${EfsDevice} status=noxfer" >& /dev/null

    if [ $? -ne 0 ]; then
         echo "## Failed to write efs image to device ##"
         AbnormalTermination
    fi

    echo "## Efs file system flashing complete ##"
}

CreateAndFlashFileSystem()
{
    echo "## Creating RootFS and Flashing it ##"
    ${SUDO_CMD} tar cfzpP /tmp/${fileSystemTarball} -C ${RootFsPath} .
    ${SUDO_CMD} mv /tmp/${fileSystemTarball} ${NfsRootPath}/${TargetTempDir}/${fileSystemTarball}

    if [ $? -ne 0 ]; then
         echo "## Failed to create RootFS tarball ##"
         AbnormalTermination
    fi

    # Unmount rootfs device on target, in case auto-mounted by system.
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} umount /dev/${RootDevice} >& /dev/null

    # Create the filesystem on the specified root device
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} mkfs.${FsType} /dev/${RootDevice} >& /dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to find RootDevice Specified ##"
         AbnormalTermination
    fi

    # Create Temp mount point
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} mkdir -p ${TargetTempDir}/.temp_mount >& /dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to create Temporary mount point ##"
         AbnormalTermination
    fi

    # Mount the partition
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} mount  /dev/${RootDevice} ${TargetTempDir}/.temp_mount >& /dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to mount the root-device ##"
         AbnormalTermination
    fi

    # Untar the data
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} tar xfp ${TargetTempDir}/${fileSystemTarball} -C /${TargetTempDir}/.temp_mount >& /dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to extract rootfs data ##"
         AbnormalTermination
    fi

    # Unmount the device
    echo "$Password" | plink -t $UserName@$IpAddr -pw $Password ${SUDO_CMD} umount ${TargetTempDir}/.temp_mount >& /dev/null
    if [ $? -ne 0 ]; then
         echo "## Failed to Un-mount the root-device ##"
         AbnormalTermination
    fi

    echo "## Root filesystem flashing complete ##"

    ${SUDO_CMD} rm -rf ${NfsRootPath}/${TargetTempDir}/.temp_mount

    ${SUDO_CMD} rm -rf ${NfsRootPath}/${TargetTempDir}/${fileSystemTarball}
}
CopyStaging()
{

    #create Temp flashing directory - NFS Mounted
    DirPath=$NfsRootPath$TargetTempDir
    ${SUDO_CMD} rm -rf $DirPath >& /dev/null

    ${SUDO_CMD} mkdir -p $DirPath
    if [ $? -ne 0 ]; then
            echo "## Failed to create staging directory ##"
            AbnormalTermination
    fi
    echo "## NFS transport ##"
    CopyFiles
}

Cleanup()
{
    ${SUDO_CMD} rm -rf $DirPath > /dev/null
    ${SUDO_CMD} rm -rf $fileSystemImage
    ${SUDO_CMD} rm -rf $efsImage
}

