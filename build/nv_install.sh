# Enable abort on errors
set -e 

UNAME=`uname`

# Parse any special options given
link_args=
if [ $1 = "-l" ]; then
    if [ $UNAME = "Darwin" ] ; then
        shift
    else
        shift
        link_args=-l;
    fi
fi

# Need to have at least 2 args left
if [ $# -lt 2 ]; then
    echo $0: too few args provided;
    exit 1;
fi

# Extract out directory name (last arg), while remembering the args we need to pass to cp
copy_args=$*
while [ $# -ge 2 ]; do shift; done
dir=$1

# Create the target directory, and copy or hard link the files
mkdir -p $dir
if [ $UNAME = "Darwin" ]; then
    cp -fr $copy_args
else
    cp -fu $copy_args
fi

