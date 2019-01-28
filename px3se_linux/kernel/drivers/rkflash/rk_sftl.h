/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __RK_SFTL_H
#define __RK_SFTL_H

u32 ftl_low_format(void);
int sftl_init(void);
int sftl_deinit(void);
int sftl_read(u32 index, u32 count, u8 *buf);
int sftl_write(u32 index, u32 count, u8 *buf);
u32 sftl_get_density(void);
s32 sftl_gc(void);

#endif
