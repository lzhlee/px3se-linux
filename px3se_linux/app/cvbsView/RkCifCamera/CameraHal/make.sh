#! /bin/sh
SYSTEM_DIR=$(pwd)/../../../../buildroot/output/host/usr/arm-rockchip-linux-gnueabihf/sysroot
BUILD_OUT_DIR=$(pwd)/../out
SYSTEM_LIB_DIR=$SYSTEM_DIR/usr/lib
TARGET_LIB_DIR=$(pwd)/../../../../buildroot/output/target/usr/lib
[ ! -d "$SYSTEM_LIB_DIR" ] && mkdir $SYSTEM_LIB_DIR
[ -d "$BUILD_OUT_DIR" ] && rm -rf $BUILD_OUT_DIR
mkdir -p $BUILD_OUT_DIR/include/CameraHal/

cp -r HAL/include/*          $BUILD_OUT_DIR/include/CameraHal/
cp -r include/CamHalVersion.h	$BUILD_OUT_DIR/include/CameraHal/
cp -r include/shared_ptr.h   $BUILD_OUT_DIR/include/
cp -r include/ebase          $BUILD_OUT_DIR/include/
cp -r include/oslayer        $BUILD_OUT_DIR/include/
cp -r include/linux          $BUILD_OUT_DIR/include/CameraHal/

make clean

make

cp -v ./build/lib/libcam_hal.so $SYSTEM_LIB_DIR
cp -v ./build/lib/libcam_hal.so $TARGET_LIB_DIR


echo "Camera HAL build success!"
