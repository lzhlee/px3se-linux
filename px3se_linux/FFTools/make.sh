#!/bin/bash

set -e

usage()
{
cat << EOF
usage:
    $(basename $0) [-u|k|a] [-d dts_file_name] [-j make_thread]
    -u|k|a: make uboot|kernel|buildroot alone, if this arg is not exist, make all images default
    -d: kernel dts name
    -j: make theard num, if have not this arg, default theard is 1

NOTE: Run in the path of SDKROOT
EOF

if [ ! -z $1 ] ; then
    exit $1
fi
}

MAKE_THEARD=4
KERNEL_DTS='px3se-sdk'
MAKE_MODULES=''
MAKE_ALL=true

while getopts "ukahj:d:" arg
do
	case $arg in
		 u|k|a)
            MAKE_MODULES=$arg
            MAKE_ALL=false
			;;
		 j)
			MAKE_THEARD=$OPTARG
			;;
		 d)
			KERNEL_DTS=$OPTARG
			;;
		 h)
			usage 0
			;;
		 ?) 
			usage 1
			;;
	esac
done

FFTOOLS_PATH=$(dirname $0)

if $MAKE_ALL || [ $MAKE_MODULES = 'u' ]; then
    pushd u-boot/
    rm -f *loader_*.bin
    make px3se_linux_defconfig 
    make  -j $MAKE_THEARD
    popd
fi

if  $MAKE_ALL || [ $MAKE_MODULES = 'k' ]; then
    pushd kernel/
    make ARCH=arm px3se_linux_defconfig
    make  ARCH=arm "${KERNEL_DTS}.img" -j $MAKE_THEARD
    popd
fi

if $MAKE_ALL || [ $MAKE_MODULES = 'a' ]; then
    pushd buildroot/
    make rockchip_px3se_defconfig
    popd
    ./build_all.sh && ./mkfirmware.sh
fi

echo "Firefly-PX3SE make images finish!"
