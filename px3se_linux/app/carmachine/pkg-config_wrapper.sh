#!/bin/sh
PKG_CONFIG_SYSROOT_DIR=/home/flystar/work/px3se/px3se_linux/buildroot/output/host/usr/arm-rockchip-linux-gnueabihf/sysroot
export PKG_CONFIG_SYSROOT_DIR
PKG_CONFIG_LIBDIR=/home/flystar/work/px3se/px3se_linux/buildroot/output/host/usr/arm-rockchip-linux-gnueabihf/sysroot/usr/lib/pkgconfig:/home/flystar/work/px3se/px3se_linux/buildroot/output/host/usr/arm-rockchip-linux-gnueabihf/sysroot/usr/share/pkgconfig:/home/flystar/work/px3se/px3se_linux/buildroot/output/host/usr/arm-rockchip-linux-gnueabihf/sysroot/usr/lib/arm-rockchip-linux-gnueabihf/pkgconfig
export PKG_CONFIG_LIBDIR
exec pkg-config "$@"
