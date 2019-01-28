#! /bin/bash

TOP_PATH=$(pwd)
KERNEL_PATH=$(pwd)/../kernel
PRODUCT_PATH=$(pwd)/../device/rockchip/px3-se
TOOLS_PATH=$PRODUCT_PATH/mini_fs
ROOTFS_BASE=$(pwd)/rootfs
ROOTFS_PATH=$(pwd)/rootfs/target
FSOVERLAY_PATH=$(pwd)/rootfs/rockchip/px3se/fs-overlay
IMAGE_PATH=$(pwd)/recoveryimg/
ROCKCHIP_GCC=$(pwd)/../buildroot/output/host/usr/bin/arm-rockchip-linux-gnueabihf-gcc
UPDATER_PATH=$(pwd)/updater-1.0

case "$1" in
	[eE][mM][mM][cC])
		echo "make px3se-emmc-minifs-sdk"
		product=px3se-emmc-minifs-sdk
		kernel_defconfig=px3se_linux_emmc_minifs_defconfig
		recovery_kernel_defconfig=px3se_recovery_minifs_emmc_defconfig
		img_name=recovery_emmc.img
		;;
	[sS][fF][cC])
		echo "make px3se-sfc-sdk"
		product=px3se-sfc-sdk
    kernel_defconfig=px3se_linux_sfc_defconfig
		recovery_kernel_defconfig=px3se_recovery_minifs_sfc_defconfig
		img_name=recovery_sfc.img
		;;
	[sS][lL][cC])
		echo "make px3se-slc-sdk"
		product=px3se-slc-sdk
    kernel_defconfig=px3se_linux_slc_defconfig
		recovery_kernel_defconfig=px3se_recovery_minifs_slc_defconfig
		img_name=recovery_slc.img
        ;;
	*)
		echo "parameter need:"
		echo "eMMC or slc  or sfc"
		exit
		;;
esac

#初始化操作（清理旧文件）
rm -rf $IMAGE_PATH
mkdir -p $IMAGE_PATH
rm -rf $ROOTFS_BASE

echo "make recovery rootfs..."

echo -n "tar xf resource/rootfs.tar.gz..."
tar zxf resource/rootfs.tar.gz
echo "done."

#仅小容量emmc需要parameter提供分区信息。
if [ $img_name == "recovery_emmc.img" ]
then
	cp -f $FSOVERLAY_PATH/parameter $ROOTFS_PATH/etc/
fi

#编译Updater
if [ ! -f "$ROCKCHIP_GCC" ]; then
	echo "ERROR:Please make buildroot first!"
	exit 0
fi
echo "make updater programe..."
cd $UPDATER_PATH && \
$ROCKCHIP_GCC -g2 -o updater updater.c recovery_display.c
if [ ! -f "$UPDATER_PATH/updater" ]; then
	echo "ERROR:updater make failed! Please check updater code."
	exit 0
fi
cp -f $UPDATER_PATH/updater $ROOTFS_PATH/usr/bin/

#拷贝升级相关脚本。
cp -f $FSOVERLAY_PATH/S50_updater_init_mini $ROOTFS_PATH/etc/init.d/S50_updater_init
cp -f $FSOVERLAY_PATH/RkUpdater.sh $ROOTFS_PATH/etc/profile.d/
cp -f $FSOVERLAY_PATH/init $ROOTFS_PATH/init

echo "make recovery kernel..."

cd $KERNEL_PATH
make ARCH=arm clean -j4 && make ARCH=arm $recovery_kernel_defconfig -j8 && make ARCH=arm $product.img -j12

cp $TOOLS_PATH/kernelimage $IMAGE_PATH

echo "cp dtb"
cp $KERNEL_PATH/arch/arm/boot/dts/$product.dtb $IMAGE_PATH/

echo "cp zImage"
cp $KERNEL_PATH/arch/arm/boot/zImage $IMAGE_PATH/

echo "revert kernel defconfig"
make ARCH=arm clean -j4 && make ARCH=arm $kernel_defconfig && make ARCH=arm $product.img -j12

echo "cat zImage & dtb > zImage-dtb"
cd $IMAGE_PATH && cat zImage $product.dtb > zImage-dtb && cd $TOP_PATH

echo "kernelimage ..."
cd $IMAGE_PATH && ./kernelimage --pack --kernel zImage-dtb recovery.img 0x62000000

cp recovery.img $TOOLS_PATH/$img_name
cd $TOP_PATH

rm -rf $IMAGE_PATH

echo "make recovery image ok !"
