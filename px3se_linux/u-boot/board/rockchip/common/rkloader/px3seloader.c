/*
 * (C) Copyright 2008-2015 Fuzhou Rockchip Electronics Co., Ltd
 * Peter, Software Engineering, <superpeter.cai@gmail.com>.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <malloc.h>
#include "../config.h"
#include "rkloader.h"

#define VENDOR_UPDATER_ID  14

typedef struct{
	//release date
	unsigned int update_version;

	//firmware.img path.
	unsigned char update_path[200];

	/* update mode
	 *	 0x0000 -> generate mode.
	 *	 0xF000 -> updater mode.
	 */
	unsigned short update_mode;
}UpdaterInfo;

unsigned short rk_px3se_check_boot_type(void)
{
	int ret = 0;
	UpdaterInfo fwinfo;
	ret = vendor_storage_read(VENDOR_UPDATER_ID, (char*)&fwinfo, sizeof(UpdaterInfo));
	if(-1 == ret) {
		printf("vendor_storage_read VENDOR_UPDATER_ID failed!\n");
		return ret;
	}
#if 0
	fwinfo.update_mode = 0;
	vendor_storage_write(VENDOR_UPDATER_ID, (char*)&fwinfo, sizeof(UpdaterInfo));
#endif
	return fwinfo.update_mode;
}

unsigned short rk_px3se_boot_mode_clear(void)
{
	int ret = 0;
	UpdaterInfo fwinfo;

	fwinfo.update_mode = 0;
	ret = vendor_storage_write(VENDOR_UPDATER_ID, (char*)&fwinfo, sizeof(UpdaterInfo));
	if(-1 == ret) {
		printf("vendor_storage_write VENDOR_UPDATER_ID failed!\n");
		return ret;
	}

	return fwinfo.update_mode;
}

void rkloader_px3se_boot(void)
{
	unsigned short boot_mode = rk_px3se_check_boot_type();

	if(0xF000 == boot_mode) {
		printf("boot recovery\n");
		char *const boot_cmd[] = {"bootrk", "recovery"};
		do_bootrk(NULL, 0, ARRAY_SIZE(boot_cmd), boot_cmd);
	}
	if(0xFF00 == boot_mode) {
		printf("boot rockusb\n");
		rk_px3se_boot_mode_clear();
		do_rockusb(NULL, 0, 0, NULL);
	}
}

