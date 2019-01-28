/*
 * drivers/video/rockchip/flinger/flinger.c
 *
 * Copyright (C) 2016 Rockchip Electronics Co.Ltd
 * Authors:
 *      Zhiqin Wei <wzq@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "vehicle_cfg.h"

#ifndef _drivers_video_rockchip_flinger_h_
#define _drivers_video_rockchip_flinger_h_

int rk_flinger_init(struct device *dev);
int rk_flinger_deinit(void);
int rk_flinger_reverse_open(struct vehicle_cfg *cfg);
int rk_flinger_reverse_exit(void);
void rk_flinger_message_handler(int size, const void *mssg);

enum {
	RGA_TRANSFORM_ROT_MASK   =   0x0000000F,
	RGA_TRANSFORM_ROT_0      =   0x00000000,
	RGA_TRANSFORM_ROT_90     =   0x00000001,
	RGA_TRANSFORM_ROT_180    =   0x00000002,
	RGA_TRANSFORM_ROT_270    =   0x00000004,

	RGA_TRANSFORM_FLIP_MASK  =   0x000000F0,
	RGA_TRANSFORM_FLIP_H     =   0x00000020,
	RGA_TRANSFORM_FLIP_V     =   0x00000010,
};
#endif
