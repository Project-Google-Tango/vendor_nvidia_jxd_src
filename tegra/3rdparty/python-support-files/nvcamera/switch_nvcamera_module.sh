#!/bin/sh
# Copyright (c) 2010-2014, NVIDIA Corporation. All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto. Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

Usage () {
    # print help message
    echo "Usage: sh $0 [v1|v3]"
    exit
}

if [ "$#" -ne 1 ]
then
    echo "ERROR: Invalid number of arguments!!"
    Usage
fi

if [ "$1" = "-h" ] || [ "$1" = "--help" ] || [ "$1" = "-help" ]
then
    Usage
elif [ "$1" != "v1" ] && [ "$1" != "v3" ]
then
    echo "ERROR: Invalid argument!"
    Usage
fi

if [ -d "/system/lib/python2.6/nvcamera" ]
then
    if [ -d "/system/lib/python2.6/nvcamera_v1" ]
    then
        echo "Current version of nvcamera module/NVCS is using V3 API..."
        if [ "$1" = "v1" ]
        then
            # change the names of python module directories and file names
            mv /system/lib/python2.6/nvcamera /system/lib/python2.6/nvcamera_v3 &
            sleep 2
            mv /system/lib/python2.6/nvcamera_v1 /system/lib/python2.6/nvcamera &
            sleep 2
            mv /system/lib/python2.6/nvcamera/__init__v1.py /system/lib/python2.6/nvcamera/__init__.py

            status=$?
            if [ $status -ne 0 ]
            then
                echo "Please try to run \"adb remount\" from the host system and try again!!"
                exit $status
            else
                echo "Changed to old version"
            fi
        else
            exit
        fi
    elif [ -d "/system/lib/python2.6/nvcamera_v3" ]
    then
        echo "Current version of nvcamera module/NVCS is using old (V1) API..."
        if [ "$1" = "v3" ]
        then
            # change the names of python module directories
            mv /system/lib/python2.6/nvcamera/__init__.py /system/lib/python2.6/nvcamera/__init__v1.py &
            sleep 2
            mv /system/lib/python2.6/nvcamera /system/lib/python2.6/nvcamera_v1 &
            sleep 2
            mv /system/lib/python2.6/nvcamera_v3 /system/lib/python2.6/nvcamera

            status=$?
            if [ $status -ne 0 ]
            then
                echo "Please try to run \"adb remount\" from the host system and try again!!"
                exit $status
            else
                echo "Changed to new version"
            fi
        else
            exit
        fi
    else
        echo "Current version of NVCS seems to be old (v1) version..."
    fi
else
    echo "NVCS is not installed on the system..."
fi

