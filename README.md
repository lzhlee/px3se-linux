# px3se-linux
px3se linux source code
make fs:
1.build uboot:
cd u-boot;make px3se_linux_defconfig;make -j8;cd -;

2.build kernel:
cd kernel;make ARCH=arm  px3se_linux_defconfig -j8;make ARCH=arm px3se-sdk.img -j24;cd -

3.build rootfs:
cd buildroot;make rockchip_px3se_defconfig;cd ..; ./build_all.sh;./mkfirmware.sh 

4.pack image:
cp px3se_linux/rockimg/* px3se_linux/tools/linux/Linux_Pack_Firmware/rockdev/rockimg
cd px3se_linux/tools/linux/Linux_Pack_Firmware/rockdev;./mkupdate.sh

image name: update.img
