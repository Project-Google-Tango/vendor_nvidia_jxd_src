#!/bin/bash

if [ a$GERRIT_USER == a ]; then
  export GERRIT_USER=$USER
fi

export GENERATE=0

if [ $1 == generate ]; then
    export GENERATE=1
fi

#directory, patchname
function apply_patch {
    echo === Changing to $TOP/$1 to apply patch
    pushd $TOP/$1
    echo === git am $TOP/vendor/nvidia/build/$2
    git am $TOP/vendor/nvidia/build/$2
    if [ $? != 0 ]; then
        echo === error: Applying patch failed!
        echo === Aborting!
        echo === Restoring original directory
        popd
        exit 1
    fi
    echo === Restoring original directory
    popd
}

#directory, commitid
function cherry_pick {
    echo === Changing to $TOP/$1 to cherry pick
    pushd $TOP/$1
    if [ $GENERATE == 1 ]; then
        echo === git format-patch $2
        git fetch ssh://git-master.nvidia.com:12001/android/platform/$1 rel-17-partner
        if [ $? != 0 ]; then
            echo === error: Downloading patch failed!
            echo === Aborting!
            echo === Restoring original directory
            popd
            exit 1
        fi
        mkdir -p $TOP/vendor/nvidia/build/patches/$1/$2/
        git format-patch -N -1 -o $TOP/vendor/nvidia/build/patches/$1/$2/ $2
    else
        echo === git am $TOP/vendor/nvidia/build/patches/$1/$2
        git am $TOP/vendor/nvidia/build/patches/$1/$2/*.patch
        if [ $? != 0 ]; then
            echo === error: Applying patch failed!
            echo === Aborting!
            echo === Restoring original directory
            popd
            exit 1
        fi
    fi
    echo === Restoring original directory
    popd
}

if [ a$TOP == a ]; then
    echo \$TOP is not set. Please set \$TOP before running this script
    exit 1
else
    echo No patches to apply
fi

