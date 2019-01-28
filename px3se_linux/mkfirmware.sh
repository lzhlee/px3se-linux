TOOL_PATH=$(pwd)/build
IMAGE_OUT_PATH=$(pwd)/rockimg/
IMAGE_RELEASE_PATH=$(pwd)/rockimg/Image-release
KERNEL_PATH=$(pwd)/kernel
UBOOT_PATH=$(pwd)/u-boot
PRODUCT_PATH=$(pwd)/device/rockchip/px3-se/

case "$1" in
	[sS][lL][cC])
	RECOVERY_NAME=recovery_slc.img
	;;
	*)
	RECOVERY_NAME=recovery_emmc.img
	;;
esac

#cd buildroot && make && cd -
rm -rf $IMAGE_OUT_PATH
mkdir -p $IMAGE_OUT_PATH
echo "Package rootfs.img now"
source $PRODUCT_PATH/mkrootfs.sh
cp $(pwd)/buildroot/output/images/rootfs.ext4 $IMAGE_OUT_PATH/rootfs.img

cp $PRODUCT_PATH/rockimg/parameter-emmc.txt $IMAGE_OUT_PATH/

if [ -f $UBOOT_PATH/uboot.img ]
then
	echo -n "Package uboot.img.Copy uboot.img form $UBOOT_PATH ..."
	cp -a $UBOOT_PATH/uboot.img $IMAGE_OUT_PATH/uboot.img
	echo "done."
else
	if [ -f $PRODUCT_PATH/rockimg/uboot.img ]
	then
		echo -n "Package uboot.img.Copy uboot.img form $PRODUCT_PATH/rockimg ..."
        	cp -a $PRODUCT_PATH/rockimg/uboot.img $IMAGE_OUT_PATH/uboot.img
        	echo "done."
	else
		echo -e "\e[31m error: $UBOOT_PATH/uboot.img not fount! Please build uboot.img in $UBOOT_PATH first! \e[0m"
		echo -e "\e[31m error: Package image failed \e[0m"
		exit 0
	fi
fi

if [ -f $UBOOT_PATH/*loader_*.bin ]
then
	echo -n "Package loader.Copy loader bin form $UBOOT_PATH ..."
	cp -a $UBOOT_PATH/*loader_*.bin $IMAGE_OUT_PATH/MiniLoaderAll.bin
	echo "done."
else
	if [ -f $PRODUCT_PATH/rockimg/*loader_*.bin ]
        then
		echo -n "Package uboot.img.Copy loader bin form $PRODUCT_PATH/rockimg ..."
		cp -a $PRODUCT_PATH/rockimg/*loader_*.bin $IMAGE_OUT_PATH/MiniLoaderAll.bin
		echo "done."
        else
		echo -e "\e[31m error: $UBOOT_PATH/*loader_*.bin not fount,or there are multiple loaders! Please build loader in $UBOOT_PATH first! \e[0m"
		echo -e "\e[31m error: Package image failed \e[0m"
		rm $UBOOT_PATH/*loader_*.bin
		exit 0
	fi
fi

if [ -f $KERNEL_PATH/resource.img ]
then
        echo -n "create resource.img..."
        cp -a $KERNEL_PATH/resource.img $IMAGE_OUT_PATH/resource.img
        echo "done."
else
        echo -e "\e[31m error: $KERNEL_PATH/resource.img not fount! \e[0m"
        exit 0
fi

if [ -f $KERNEL_PATH/kernel.img ]
then
        echo -n "create kernel.img..."
        cp -a $KERNEL_PATH/kernel.img $IMAGE_OUT_PATH/kernel.img
        echo "done."
else
        echo -e "\e[31m error: $KERNEL_PATH/kernel.img not fount! \e[0m"
        exit 0
fi

echo -n "create recovery.img..."
cp -f $PRODUCT_PATH/rockimg/$RECOVERY_NAME $IMAGE_OUT_PATH/recovery.img
echo "done."

echo -e "\e[36m Image: image in $IMAGE_OUT_PATH is ready \e[0m"
