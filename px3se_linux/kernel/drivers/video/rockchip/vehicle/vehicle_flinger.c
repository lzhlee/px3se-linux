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
#include <linux/atomic.h>
#include <linux/rk_fb.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <asm/div64.h>
#include <linux/uaccess.h>
#include <linux/rk_fb.h>
#include <linux/linux_logo.h>
#include <linux/dma-mapping.h>
#include <linux/regulator/consumer.h>
#include <linux/of_address.h>
#include <linux/memblock.h>
#include <linux/kthread.h>
#include <linux/fdtable.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <video/of_display_timing.h>
#include <video/display_timing.h>
#endif

#if defined(CONFIG_ION_ROCKCHIP)
#include <linux/rockchip_ion.h>
#include <linux/rockchip-iovmm.h>
#include <linux/dma-buf.h>
#include <linux/highmem.h>
#endif

#include "../rga/rga.h"
#include "../iep/iep.h"
#include "vehicle_flinger.h"
#include <linux/rk_fb.h>
//#include "clogo1.h"

#define USE_COTINUE_BUFFER 0

enum force_value {
	FORCE_WIDTH = 1280,
	FORCE_HEIGHT = 720,
	FORCE_STRIDE = 1280,
	FORCE_XOFFSET = 0,
	FORCE_YOFFSET = 0,
	FORCE_FORMAT = HAL_PIXEL_FORMAT_YCrCb_NV12,
	FORCE_ROTATION = RGA_TRANSFORM_ROT_270,
};

#define FIX_SOURCE_BUFFER /*cif pingpong mode, fix 2 src buffer*/
#define RK312X_RGA /*do not support yuv2yuv rotate, support yuv2rga rotate*/

#ifdef FIX_SOURCE_BUFFER
enum {
	NUM_SOURCE_BUFFERS = 2, /*2 src buffer for cif*/
	NUM_TARGET_BUFFERS = 3, /*3 dst buffer , 1 ipp*/
};
#else
enum {
	NUM_SOURCE_BUFFERS = 4, /*4 src buffer for cif*/
	NUM_TARGET_BUFFERS = 5, /*4 dst buffer rga, 1 eip*/
};
#endif

enum buffer_state {
	UNKNOW = 0,
	FREE,
	DEQUEUE,
	QUEUE,
	ACQUIRE,
	DISPLAY,
};

struct rect {
	size_t x;
	size_t y;
	size_t w;
	size_t h;
	size_t s;
	size_t f;
};

struct graphic_buffer {
	struct list_head list;
	struct ion_handle *handle;
	struct sync_fence *rel_fence;
	struct rect src;
	struct rect dst;
	enum buffer_state state;
	unsigned long phy_addr;
	void *vir_addr;
	int ion_hnd_fd;
	int rotation;
	int offset;
	int len;
	int width;
	int height;
	int stride;
	int format;
	struct work_struct render_work;
};

struct queue_buffer {
	struct list_head list;
	struct graphic_buffer *buffer;
};

struct flinger {
	struct device *dev;
	struct ion_client *ion_client;
	struct work_struct init_work;
	struct work_struct render_work;
	struct workqueue_struct *render_workqueue;
	struct mutex source_buffer_lock;/*src buffer lock*/
	struct mutex target_buffer_lock;/*dst buffer lock*/
	struct graphic_buffer source_buffer[NUM_SOURCE_BUFFERS];
	struct graphic_buffer target_buffer[NUM_TARGET_BUFFERS];
	struct graphic_buffer cvbs_buffer;
	struct mutex queue_buffer_lock;/*buffr queue lock*/
	struct list_head queue_buffer_list;
	wait_queue_head_t worker_wait;
	atomic_t worker_cond_atomic;
	atomic_t worker_running_atomic;
	atomic_t souce_frame_atomic;
	int source_index;
	int target_index;
	struct vehicle_cfg v_cfg;
	int cvbs_field_count;
	/*debug*/
	int debug_cif_count;
	int debug_vop_count;
};

atomic_t flinger_running;
struct flinger *flinger = NULL;

#ifdef CONFIG_DRM_ROCKCHIP
struct graphic_buffer *logo_buffer;
extern int rk_drm_fb_show_kernel_logo(ion_phys_addr_t phy_addr, char __iomem *screen_base);
#endif
static int rk_flinger_queue_work(struct flinger *flinger,
				 struct graphic_buffer *src_buffer);

static int rk_flinger_create_ion_client(struct flinger *flinger)
{
	int ret = 0;

	if (!flinger)
		return -ENODEV;

#if defined(CONFIG_ION_ROCKCHIP)
	flinger->ion_client = rockchip_ion_client_create("flinger");
	if (IS_ERR(flinger->ion_client)) {
		DBG("wzqtest failed to create ion client for flinger");
		return PTR_ERR(flinger->ion_client);
	}
#else
	flinger->ion_client = NULL;
	ret = -ENODEV;
#endif
	DBG("wzqtest %s for ret = %d\n", __func__, ret);
	return ret;
}

static int rk_flinger_desrtoy_ion_client(struct flinger *flinger)
{
	int ret = 0;

	DBG("wzqtest%s,%d\n", __func__, __LINE__);
	if (!flinger)
		return -ENODEV;

#if defined(CONFIG_ION_ROCKCHIP)
	ion_client_destroy(flinger->ion_client);
#else
	flinger->ion_client = NULL;
	ret = -ENODEV;
#endif
	return ret;
}

static int rk_flinger_alloc_bpp(int format)
{
	int width = 4;

	switch (format) {
	case HAL_PIXEL_FORMAT_RGB_565:
		width = 2;
		break;
	case HAL_PIXEL_FORMAT_RGB_888:
		width =  3;
		break;
	case HAL_PIXEL_FORMAT_RGBA_8888:
		width =  4;
		break;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		width =  4;
		break;
	case HAL_PIXEL_FORMAT_BGRA_8888:
		width =  4;
		break;
	case HAL_PIXEL_FORMAT_YCrCb_NV12:
		width = 2;
       break;
	case HAL_PIXEL_FORMAT_YCrCb_444:
		width = 3;
		break;
	default:
		DBG("%s: unsupported format: 0x%x\n", __func__, format);
		break;
	}
	return width;
}

static int rk_flinger_init_buffer_dma_fd(struct flinger *flinger,
					 struct graphic_buffer *buffer)
{
	struct flinger *flg = flinger;
	int ion_hnd_fd = -1;

	if (!flg)
		return -ENODEV;

	if (!buffer)
		return -EINVAL;

	if (buffer->handle && buffer->ion_hnd_fd < 0) {
		ion_hnd_fd = ion_share_dma_buf_fd(flg->ion_client,
						  buffer->handle);
		if (ion_hnd_fd < 0)
			DBG(":%s,ion_fd=%d\n", __func__, ion_hnd_fd);
		if (ion_hnd_fd == 0) {
			put_unused_fd(ion_hnd_fd);
			ion_hnd_fd = get_unused_fd();
			ion_hnd_fd = ion_share_dma_buf_fd(flinger->ion_client,
							  buffer->handle);
		}
		buffer->ion_hnd_fd = ion_hnd_fd;
	} else if (buffer->ion_hnd_fd == 0) {
		ion_hnd_fd = buffer->ion_hnd_fd;
		put_unused_fd(ion_hnd_fd);
		ion_hnd_fd = get_unused_fd();
		ion_hnd_fd = ion_share_dma_buf_fd(flinger->ion_client,
						  buffer->handle);
	} else {
		ion_hnd_fd = buffer->ion_hnd_fd;
	}

	return ion_hnd_fd;
}

static int rk_flinger_get_buffer_dma_fd(struct flinger *flinger,
					struct graphic_buffer *buffer)
{
	return  rk_flinger_init_buffer_dma_fd(flinger, buffer);
}

static int rk_flinger_alloc_buffer(struct flinger *flinger,
				   struct graphic_buffer *buffer,
				   int w, int h,
				   int s, int f)
{
	ion_phys_addr_t ion_phy_addr;
	struct ion_handle *handle;
	unsigned long phy_addr;
	size_t len;
	int fd = -1, bpp;
	int ret = 0;

	if (!flinger)
		return -ENODEV;

	if (!buffer)
		return -EINVAL;

	bpp = rk_flinger_alloc_bpp(f);
	len = s * h * bpp;
	handle = ion_alloc(flinger->ion_client,
			   len, 0, ION_HEAP(ION_CMA_HEAP_ID), 0);
	if (IS_ERR(handle)) {
		DBG("failed to ion_alloc:%ld\n", PTR_ERR(handle));
		return -ENOMEM;
	}

	ret = ion_phys(flinger->ion_client, handle, &ion_phy_addr, &len);
	phy_addr = ion_phy_addr;
	buffer->vir_addr = ion_map_kernel(flinger->ion_client, handle);
	buffer->handle = handle;
	buffer->rel_fence = NULL;
	buffer->phy_addr = phy_addr;
	buffer->rotation = 0;
	buffer->width = w;
	buffer->height = h;
	buffer->stride = s;
	buffer->format = f;
	buffer->len = len;
	buffer->ion_hnd_fd = fd;

	return 0;
}

int rk_flinger_free_buffer(struct flinger *flinger,
			   struct graphic_buffer *buffer)
{
	if (!flinger)
		return -ENODEV;

	if (!buffer)
		return -EINVAL;

	if (buffer && buffer->ion_hnd_fd > -1)
		put_unused_fd(buffer->ion_hnd_fd);

	if (buffer && buffer->handle)
		ion_free(flinger->ion_client, buffer->handle);

	if (buffer && buffer->ion_hnd_fd > -1)
		__close_fd(current->files, buffer->ion_hnd_fd);

	return 0;
}

static int rk_flinger_create_worker(struct flinger *flinger)
{
	struct workqueue_struct *wq = NULL;

	wq = create_singlethread_workqueue("flinger-render");
	if (!wq) {
		DBG("wzqtest Failed to create flinger workqueue\n");
		return -ENODEV;
	}
	flinger->render_workqueue = wq;

	return 0;
}

static int rk_flinger_destory_worker(struct flinger *flinger)
{
	if (!flinger)
		return -ENODEV;

	if (flinger->render_workqueue)
		destroy_workqueue(flinger->render_workqueue);

	return 0;
}

int vehicle_flinger_init(struct device *dev)
{
	struct graphic_buffer *buffer, *tmp;
	struct flinger *flg = NULL;
	int i, ret, fd, w, h, s, f;

	w = FORCE_WIDTH;
	h = FORCE_HEIGHT;
	s = FORCE_STRIDE;
	f = FORCE_FORMAT;

	flg = kzalloc(sizeof(*flg), GFP_KERNEL);
	if (!flg) {
		DBG("wzqtest: %s: %d", __func__, __LINE__);
		return -ENOMEM;
	}

	mutex_init(&flg->queue_buffer_lock);
	mutex_init(&flg->source_buffer_lock);
	mutex_init(&flg->target_buffer_lock);
	INIT_LIST_HEAD(&flg->queue_buffer_list);
	init_waitqueue_head(&flg->worker_wait);
	atomic_set(&flg->worker_cond_atomic, 0);
	atomic_set(&flg->worker_running_atomic, 1);

	fd = get_unused_fd();
	DBG("wzqtest %s,%d\n", __func__, __LINE__);

	ret = rk_flinger_create_ion_client(flg);
	if (ret) {
		DBG("wzqtest:%s: %d", __func__, __LINE__);
		goto out;
	}

	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		flg->source_buffer[i].handle = NULL;
		flg->source_buffer[i].phy_addr = 0;
		flg->source_buffer[i].ion_hnd_fd = -1;
	}
	for (i = 0; i < NUM_TARGET_BUFFERS; i++) {
		flg->target_buffer[i].handle = NULL;
		flg->target_buffer[i].phy_addr = 0;
		flg->target_buffer[i].ion_hnd_fd = -1;
	}

	tmp = NULL;
	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		buffer = &(flg->source_buffer[i]);
		ret = rk_flinger_alloc_buffer(flg, buffer, w, h, s, f);
		if (ret) {
			DBG("wzqtest:%s: %d", __func__, __LINE__);
			goto free_dst_alloc;
		}
		buffer->state = FREE;
	}
	for (i = 0; i < NUM_TARGET_BUFFERS; i++) {
		buffer = &(flg->target_buffer[i]);
		ret = rk_flinger_alloc_buffer(flg, buffer, w, h, s, HAL_PIXEL_FORMAT_RGBX_8888);
		if (ret) {
			DBG("wzqtest:%s: %d", __func__, __LINE__);
			goto free_src_alloc;
		}
		buffer->state = FREE;
	}

	/*alloc buffer for cvbs composite*/
	buffer = &(flg->cvbs_buffer);
	ret = rk_flinger_alloc_buffer(flg, buffer, w, h, s, f);
	if (ret) {
		DBG("wzqtest:%s: %d", __func__, __LINE__);
		goto free_dst_alloc;
	}
	buffer->state = FREE;

	ret = rk_flinger_create_worker(flg);
	if (ret) {
		DBG("wzqtest:%s: %d", __func__, __LINE__);
		goto free_dst_alloc;
	}
	flinger = flg;

	rk_flinger_queue_work(flg, NULL);

	flg->dev = dev;
	DBG("wzqtest init rk flinger ok rga\n");
	return 0;
free_dst_alloc:
	for (i = 0; i < NUM_TARGET_BUFFERS; i++) {
		ret = rk_flinger_free_buffer(flg, &(flg->target_buffer[i]));
		if (ret)
			DBG("wzqtest:%s: %d", __func__, __LINE__);
	}
free_src_alloc:
	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		ret = rk_flinger_free_buffer(flg, &(flg->source_buffer[i]));
		if (ret)
			DBG("wzqtest:%s: %d", __func__, __LINE__);
	}

	DBG("wzqtest:%s: %d", __func__, __LINE__);
	rk_flinger_desrtoy_ion_client(flg);
out:
	kfree(flg);
	return ret;
}
__maybe_unused int vehicle_flinger_deinit(void)
{
	struct flinger *flg = flinger;
	int i, ret;

	ret = 0;
	i = 0;
	DBG("yuyz test %s:%d\n", __func__, __LINE__);
	if (!flg)
		return -ENODEV;

#if 1
	atomic_set(&flg->worker_running_atomic, 0);
	atomic_inc(&flg->worker_cond_atomic);
	wake_up(&flg->worker_wait);
	flush_work(&flg->render_work);
	flush_workqueue(flg->render_workqueue);
	rk_flinger_destory_worker(flg);
	flinger = NULL;
	msleep(1);
	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		ret = rk_flinger_free_buffer(flg, &flg->source_buffer[i]);
		if (ret)
			DBG("wzqtest:%s: %d", __func__, __LINE__);
	}
	for (i = 0; i < NUM_TARGET_BUFFERS; i++) {
		ret = rk_flinger_free_buffer(flg, &flg->target_buffer[i]);
		if (ret)
			DBG("wzqtest:%s: %d", __func__, __LINE__);
	}
	rk_flinger_desrtoy_ion_client(flg);
	kfree(flg);
#endif
	DBG("yuyz test %s finish\n", __func__);
	return 0;
}

static int rk_flinger_format_hal_to_rga(int format)
{
	int rga_format = -1;

	switch (format) {
	case HAL_PIXEL_FORMAT_RGB_565:
		rga_format = RK_FORMAT_RGB_565;
		break;
	case HAL_PIXEL_FORMAT_RGB_888:
		rga_format =  RK_FORMAT_RGB_888;
		break;
	case HAL_PIXEL_FORMAT_RGBA_8888:
		rga_format =  RK_FORMAT_RGBA_8888;
		break;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		rga_format =  RK_FORMAT_RGBX_8888;
		break;
	case HAL_PIXEL_FORMAT_BGRA_8888:
		rga_format =  RK_FORMAT_BGRA_8888;
		break;
	case HAL_PIXEL_FORMAT_YCrCb_NV12:
		rga_format =  RK_FORMAT_YCbCr_420_SP;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
		rga_format =  RK_FORMAT_YCbCr_422_SP;
		break;
	default:
		break;
	}

	return rga_format;
}

static int rk_flinger_set_rect(struct rect *rect, int x, size_t y,
			       int w, int h, int s, int f)
{
	if (!rect)
		return -EINVAL;

	rect->x = x;
	rect->y = y;
	rect->w = w;
	rect->h = h;
	rect->s = s;
	rect->f = f;

	return 0;
}

static int
rk_flinger_set_buffer_rotation(struct graphic_buffer *buffer, int r)
{
	if (!buffer)
		return -EINVAL;

	buffer->rotation = r;
	return buffer->rotation;
}

static int
rk_flinger_cacultae_dst_rect_by_rotation(struct graphic_buffer *buffer)
{
	struct rect *src_rect, *dst_rect;

	if (!buffer)
		return -EINVAL;

	src_rect = &buffer->src;
	dst_rect = &buffer->dst;

	switch (buffer->rotation) {
	case RGA_TRANSFORM_ROT_90:
	case RGA_TRANSFORM_ROT_270:
		dst_rect->x = src_rect->x;
		dst_rect->y = src_rect->y;
		dst_rect->h = src_rect->w;
		dst_rect->w = src_rect->h;
		dst_rect->s = src_rect->h;
		break;
	case RGA_TRANSFORM_ROT_0:
	case RGA_TRANSFORM_ROT_180:
	case RGA_TRANSFORM_FLIP_H:
	case RGA_TRANSFORM_FLIP_V:
	default:
		dst_rect->x = src_rect->x;
		dst_rect->y = src_rect->y;
		dst_rect->w = src_rect->w;
		dst_rect->h = src_rect->h;
		dst_rect->s = src_rect->s;
		break;
	}
	return 0;
}

static int rk_flinger_fill_buffer_rects(struct graphic_buffer *buffer,
					struct rect *src_rect,
					struct rect *dst_rect)
{
	if (!buffer)
		return -EINVAL;

	if (src_rect)
		memcpy(&buffer->src, src_rect, sizeof(struct rect));
	if (dst_rect)
		memcpy(&buffer->dst, dst_rect, sizeof(struct rect));
	return 0;
}

static void field_copy(char *dst_buffer, char *src_buffer, int even_field,
		       int src_width, int src_height, int src_yuv_stride)
{
	char *dst_tmp;
	char *src_tmp;
	int vir_ystride = src_width * 2;
	int vir_uvstride = src_yuv_stride * 2;
	int h;

	even_field = even_field % 2;

	/*copy y*/
	dst_tmp = dst_buffer + even_field * src_width;
	src_tmp = src_buffer;
	for (h = 0; h < src_height; h++) {
		memcpy(dst_tmp, src_tmp, src_width);
		dst_tmp += vir_ystride;
		src_tmp += src_width;
	}
	/*copy uv*/
	dst_tmp = dst_buffer + src_width * src_height * 2 +
		even_field * src_yuv_stride;
	src_tmp = src_buffer + src_width * src_height;
	for (h = 0; h < src_height; h++) {
		memcpy(dst_tmp, src_tmp, src_width);
		dst_tmp += vir_uvstride;
		src_tmp += src_yuv_stride;
	}
}

static int rk_flinger_dump_rga_req(struct rga_req rga_request)
{
	return 0;
}

static int rk_flinger_rga_composite(struct flinger *flinger,
				    struct graphic_buffer *src_buffer,
				    struct graphic_buffer *dst_buffer,
				    int even_field)
{
	struct rga_req rga_request;
	unsigned long src_phy, dst_phy;
	int sx, sy, sw, sh, ss, sf;
	int dx, dy, dw, dh, ds, df;
	int orientation;
	int ret;
	int src_fd, dst_fd;

	src_fd = rk_flinger_get_buffer_dma_fd(flinger, src_buffer);
	dst_fd = rk_flinger_get_buffer_dma_fd(flinger, dst_buffer);

	memset(&rga_request, 0, sizeof(rga_request));

	if (!src_buffer || !dst_buffer)
		return -EINVAL;

	orientation = RGA_TRANSFORM_ROT_0;/*src_buffer->rotation*/

	sx = src_buffer->src.x;
	sy = src_buffer->src.y;
	sw = src_buffer->src.w;
	ss = src_buffer->src.s;
	sh = src_buffer->src.h;
	sf = rk_flinger_format_hal_to_rga(src_buffer->src.f);

	dx = src_buffer->src.x;
	dy = src_buffer->src.y;
	dw = src_buffer->src.w;
	ds = src_buffer->src.s * 2;
	dh = src_buffer->src.h;
	df = rk_flinger_format_hal_to_rga(src_buffer->src.f);

	src_phy = src_buffer->phy_addr + src_buffer->offset;
	dst_phy = dst_buffer->phy_addr + dst_buffer->offset;

	rga_request.rotate_mode = 0;
	rga_request.sina = 0;
	rga_request.cosa = 0;
	rga_request.dst.vir_w = ds;
	rga_request.dst.vir_h = dh;
	rga_request.dst.act_w = dw;
	rga_request.dst.act_h = dh;
	rga_request.dst.x_offset = even_field ? dw : 0;
	rga_request.dst.y_offset = 0;

	rga_request.src.yrgb_addr = src_fd;
#if defined(__arm64__) || defined(__aarch64__)
	rga_request.src.uv_addr = (unsigned long)src_phy;
	rga_request.src.v_addr = (unsigned long)src_phy + ss * sh;
#else
	rga_request.src.uv_addr = (unsigned int)0;
	rga_request.src.v_addr = (unsigned int)0;
#endif

	rga_request.dst.yrgb_addr = -1;/*dst_fd;*/
#if defined(__arm64__) || defined(__aarch64__)
	rga_request.dst.uv_addr = (unsigned long)dst_phy;
	rga_request.dst.v_addr = (unsigned long)dst_phy + ds * sh;
#else
	rga_request.dst.uv_addr = (unsigned int)0;
	rga_request.dst.v_addr = (unsigned int)0;
#endif

	rga_request.src.vir_w = ss;
	rga_request.src.vir_h = sh;
	rga_request.src.format = sf;
	rga_request.src.act_w = sw;
	rga_request.src.act_h = sh;
	rga_request.src.x_offset = 0;
	rga_request.src.y_offset = 0;

	rga_request.dst.format = df;

	rga_request.clip.xmin = 0;
	rga_request.clip.xmax = dw - 1;
	rga_request.clip.ymin = 0;
	rga_request.clip.ymax = dh - 1;
	rga_request.scale_mode = 0;

	/* rga_request.mmu_info.mmu_en = 1; */
	/* rga_request.mmu_info.mmu_flag = ((2 & 0x3) << 4) |
		 1 | (1 <<31 | 1 << 8 | 1<<10); */

	rk_flinger_dump_rga_req(rga_request);

	ret = rga_ioctl_kernel(&rga_request);
	if (ret)
		DBG("RGA_BLIT_SYNC faile(%d)\n", ret);

	return 0;
}

/*composite 2 field to 1 frame*/
static int rk_flinger_soft_composite(struct flinger *flinger,
				     struct graphic_buffer *src_buffer,
				     struct graphic_buffer *dst_buffer)
{
	#define USE_RGA_COMPOSITE (1)
	int even_field;
	struct flinger *flg = flinger;
	int src_width;
	int src_height;
	int src_yuv_stride;

	src_width = src_buffer->src.w;
	src_height = src_buffer->src.h;
	if (CIF_OUTPUT_FORMAT_422 == flinger->v_cfg.output_format)
		src_yuv_stride = src_width;
	else
		src_yuv_stride = src_width / 2;
	even_field = flg->cvbs_field_count % 2;

	if (USE_RGA_COMPOSITE) {
		rk_flinger_rga_composite(flg, src_buffer, dst_buffer,
					 even_field);
	} else {
		field_copy(dst_buffer->vir_addr, src_buffer->vir_addr,
			   even_field, src_width, src_height,
			   src_yuv_stride);
		if (flg->cvbs_field_count == 0)
			field_copy(dst_buffer->vir_addr, src_buffer->vir_addr,
				   !even_field, src_width, src_height,
				   src_yuv_stride);
	}
	src_buffer->state = FREE;
	flg->cvbs_field_count++;

	/*  copy param to dst buffer */
	dst_buffer->format = src_buffer->format;
	dst_buffer->rotation = src_buffer->rotation;
	memcpy(&dst_buffer->src, &src_buffer->src, sizeof(struct rect));
	memcpy(&dst_buffer->dst, &src_buffer->dst, sizeof(struct rect));
	dst_buffer->src.h *= 2;
	rk_flinger_cacultae_dst_rect_by_rotation(dst_buffer);
	return 0;
}

static int rk_flinger_iep_deinterlace(struct flinger *flinger,
				      struct graphic_buffer *src_buffer,
				      struct graphic_buffer *dst_buffer)
{
	struct IEP_MSG *msg;
	int w;
	int h;
	int src_fd, dst_fd;
	int iep_format;
	struct graphic_buffer *com_buffer = src_buffer;

	if (flinger->v_cfg.input_format == CIF_INPUT_FORMAT_PAL_SW_COMPOSITE ||
	    flinger->v_cfg.input_format == CIF_INPUT_FORMAT_NTSC_SW_COMPOSITE) {
		com_buffer = &flinger->cvbs_buffer;
		rk_flinger_soft_composite(flinger, src_buffer, com_buffer);
		src_buffer->state = FREE;
		src_buffer = com_buffer;
	}

	if (CIF_OUTPUT_FORMAT_422 == flinger->v_cfg.output_format)
		iep_format = IEP_FORMAT_YCbCr_422_SP;
	else
		iep_format = IEP_FORMAT_YCbCr_420_SP;

    msg = (struct IEP_MSG *)kzalloc(sizeof(struct IEP_MSG),
      GFP_KERNEL);
    if (!msg)
        return -ENOMEM;

	memset(msg, 0, sizeof(struct IEP_MSG));

	w = src_buffer->src.w;
	h = src_buffer->src.h;

	src_fd = rk_flinger_get_buffer_dma_fd(flinger, src_buffer);
	dst_fd = rk_flinger_get_buffer_dma_fd(flinger, dst_buffer);

	msg->src.act_w = w;
	msg->src.act_h = h;
	msg->src.x_off = 0;
	msg->src.y_off = 0;
	msg->src.vir_w = w;
	msg->src.vir_h = h;
	msg->src.format = iep_format;
	msg->src.mem_addr = src_fd;
	msg->src.uv_addr  = src_fd | (w * h << 10);
	msg->src.v_addr = 0;

	msg->dst.act_w = w;
	msg->dst.act_h = h;
	msg->dst.x_off = 0;
	msg->dst.y_off = 0;
	msg->dst.vir_w = w;
	msg->dst.vir_h = h;
	msg->dst.format = iep_format;
	msg->dst.mem_addr = dst_fd;
	msg->dst.uv_addr = dst_fd | (w * h << 10);
	msg->dst.v_addr = 0;

	msg->dein_high_fre_en = 1;
	msg->dein_mode = IEP_DEINTERLACE_MODE_I2O1;
	msg->field_order = FIELD_ORDER_BOTTOM_FIRST;
	msg->dein_high_fre_fct = 50;
	msg->dein_ei_mode = 1;
	msg->dein_ei_smooth = 1;
	msg->dein_ei_sel = 0;
	msg->dein_ei_radius = 2;

	iep_process_sync(msg);
	kfree(msg);

	/*  copy param to dst buffer */
	dst_buffer->format = src_buffer->format;
	dst_buffer->rotation = src_buffer->rotation;
	memcpy(&dst_buffer->src, &src_buffer->src, sizeof(struct rect));
	memcpy(&dst_buffer->dst, &src_buffer->dst, sizeof(struct rect));

	src_buffer->state = FREE;
	return 0;
}

static int rk_flinger_rga_blit(struct flinger *flinger,
			       struct graphic_buffer *src_buffer,
			       struct graphic_buffer *dst_buffer)
{
	struct rga_req rga_request;
	unsigned long src_phy;
	int sx, sy, sw, sh, ss, sf;
	int dx, dy, dw, dh, ds, df;
	int orientation;
	int ret;
	int src_fd, dst_fd;

	src_fd = rk_flinger_get_buffer_dma_fd(flinger, src_buffer);
	dst_fd = rk_flinger_get_buffer_dma_fd(flinger, dst_buffer);

	memset(&rga_request, 0, sizeof(rga_request));

	if (!src_buffer || !dst_buffer)
		return -EINVAL;

	orientation = src_buffer->rotation;

	sx = src_buffer->src.x;
	sy = src_buffer->src.y;
	sw = src_buffer->src.w;
	ss = src_buffer->src.s;
	sh = src_buffer->src.h;
	sf = rk_flinger_format_hal_to_rga(src_buffer->src.f);

	dx = src_buffer->dst.x;
	dy = src_buffer->dst.y;
	dw = src_buffer->dst.w;
	ds = src_buffer->dst.s;
	dh = src_buffer->dst.h;
	df = rk_flinger_format_hal_to_rga(src_buffer->dst.f);

	src_phy = src_buffer->phy_addr + src_buffer->offset;

	if (src_buffer->offset) {
		sh += src_buffer->offset / src_buffer->len * sh;
		sx = src_buffer->offset / src_buffer->len * sh;
		src_fd = 0;
	}

	switch (orientation) {
	case RGA_TRANSFORM_ROT_0:
		rga_request.rotate_mode = 0;
		rga_request.sina = 0;
		rga_request.cosa = 0;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = 0;
		break;
	case RGA_TRANSFORM_FLIP_H:/*x mirror*/
		rga_request.rotate_mode = 2;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = 0;
		break;
	case RGA_TRANSFORM_FLIP_V:/*y mirror*/
		rga_request.rotate_mode = 3;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = 0;
		break;
	case RGA_TRANSFORM_ROT_90:
		rga_request.rotate_mode = 1;
		rga_request.sina = 65536;
		rga_request.cosa = 0;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dh;
		rga_request.dst.act_h = dw;
		rga_request.dst.x_offset = dw - 1;
		rga_request.dst.y_offset = 0;
		break;
	case RGA_TRANSFORM_ROT_180:
		rga_request.rotate_mode = 1;
		rga_request.sina = 0;
		rga_request.cosa = -65536;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = dw - 1;
		rga_request.dst.y_offset = dh - 1;
		break;
	case RGA_TRANSFORM_ROT_270:
		rga_request.rotate_mode = 1;
		rga_request.sina = -65536;
		rga_request.cosa = 0;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dh;
		rga_request.dst.act_h = dw;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = dh - 1;
		break;
	default:
		rga_request.rotate_mode = 0;
		rga_request.sina = 0;
		rga_request.cosa = 0;
		rga_request.dst.vir_w = ds;
		rga_request.dst.vir_h = dh;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = 0;
		break;
	}

#ifdef RK312X_RGA
    if (orientation == RGA_TRANSFORM_ROT_90 ||
        orientation == RGA_TRANSFORM_ROT_180 ||
        orientation == RGA_TRANSFORM_ROT_270) {
        df = RK_FORMAT_RGBX_8888;
    }
#endif

	rga_request.src.yrgb_addr = src_fd;
#if defined(__arm64__) || defined(__aarch64__)
	rga_request.src.uv_addr = (unsigned long)src_phy;
	rga_request.src.v_addr = (unsigned long)src_phy + ss * sh;
#else
	rga_request.src.uv_addr = (unsigned int)0;
	rga_request.src.v_addr = (unsigned int)0;
#endif

	rga_request.dst.yrgb_addr = dst_fd;
#if defined(__arm64__) || defined(__aarch64__)
	rga_request.dst.uv_addr = (unsigned long)0;
	rga_request.dst.v_addr = (unsigned long)0;
#else
	rga_request.dst.uv_addr = (unsigned int)0;
	rga_request.dst.v_addr = (unsigned int)0;
#endif

	rga_request.src.vir_w = ss;
	rga_request.src.vir_h = sh;
	rga_request.src.format = sf;
	rga_request.src.act_w = sw;
	rga_request.src.act_h = sh;
	rga_request.src.x_offset = 0;
	rga_request.src.y_offset = 0;

	rga_request.dst.format = df;

	rga_request.clip.xmin = 0;
	rga_request.clip.xmax = dw - 1;
	rga_request.clip.ymin = 0;
	rga_request.clip.ymax = dh - 1;
	rga_request.scale_mode = 0;

	/* rga_request.mmu_info.mmu_en = 1; */
	/* rga_request.mmu_info.mmu_flag = ((2 & 0x3) << 4) |
		 1 | (1 <<31 | 1 << 8 | 1<<10); */

	rk_flinger_dump_rga_req(rga_request);

	ret = rga_ioctl_kernel(&rga_request);
	if (ret)
		DBG("RGA_BLIT_SYNC faile(%d)\n", ret);

	rk_flinger_dump_rga_req(rga_request);

	return 0;
}

static int rk_flinger_rga_render(struct flinger *flinger,
				 struct graphic_buffer *src_buffer,
				 struct graphic_buffer *dst_buffer)
{
	int timeout = 508;

	if (!flinger || !src_buffer || !dst_buffer)
		return -EINVAL;

	if (dst_buffer && dst_buffer->rel_fence) {
		if (0)
			sync_fence_wait(dst_buffer->rel_fence, timeout);
		sync_fence_put(dst_buffer->rel_fence);
		dst_buffer->rel_fence = NULL;
	}
	rk_flinger_rga_blit(flinger, src_buffer, dst_buffer);
	rk_flinger_fill_buffer_rects(dst_buffer, &src_buffer->dst,
				     &src_buffer->dst);

#ifdef RK312X_RGA
/*YCrCb422 after rotate 90 / 270 degree, become YCrCb444*/
    if (src_buffer->rotation == RGA_TRANSFORM_ROT_90 ||
        src_buffer->rotation == RGA_TRANSFORM_ROT_180 ||
        src_buffer->rotation == RGA_TRANSFORM_ROT_270)
        dst_buffer->src.f = HAL_PIXEL_FORMAT_RGBX_8888;
#endif

	return 0;
}

static int rk_flinger_vop_show_logo(struct flinger *flinger,
				    struct rk_fb_car_par *par)
{
#ifdef CONFIG_DRM_ROCKCHIP
	unsigned int x_pos , y_pos ,vir_addr;
	int i, j, fence_fd, ion_hnd_fd = -1;
	struct rk_fb_win_cfg_data fb_info;
	struct sync_fence *rel_fence = NULL;
	u32 phy_addr = logo_buffer->phy_addr;
	u32 len;

	if (!flinger || !logo_buffer)
        return -1;

	// if vop iommu enabled
	if (par->fb_inited && par->iommu_enabled) {
        ion_map_iommu(par->dev,
			    flinger->ion_client,
			    logo_buffer->handle,
			    (unsigned long *)&phy_addr,
			    (unsigned long *)&len);
        //DBG("iommu map phy = %x, raw phy = %lx\n", phy_addr, buffer->phy_addr);
	}

	par = rk_fb_get_car_par();
	/*	1. reinit buffer format */
	logo_buffer->state = ACQUIRE;
	printk("par format = %d, par xact = %d----\n", par->direct_show.format, par->direct_show.xact);

	/*2. fill buffer info*/
	x_pos = (par->screen_width-par->direct_show.xact)/2;
	y_pos = (par->screen_height-par->direct_show.yact)/2;

	ion_hnd_fd = rk_flinger_get_buffer_dma_fd(flinger, logo_buffer);
	if (ion_hnd_fd < 0) {
        DBG("get fd failed\n");
        return -1;
	}

	memset(&fb_info, 0, sizeof(struct rk_fb_win_cfg_data));
	fb_info.ret_fence_fd = -1;
	for (i = 0; i < RK_MAX_BUF_NUM; i++)
        fb_info.rel_fence_fd[i] = -1;

	fb_info.wait_fs = 0;
	fb_info.win_par[0].win_id = 0;
	fb_info.win_par[0].z_order = 0;
	fb_info.win_par[0].area_par[0].ion_fd = logo_buffer->ion_hnd_fd;
	fb_info.win_par[0].area_par[0].phy_addr = phy_addr;
	fb_info.win_par[0].area_par[0].acq_fence_fd = -1;
	fb_info.win_par[0].area_par[0].x_offset = 0;
	fb_info.win_par[0].area_par[0].y_offset = 0;
	fb_info.win_par[0].area_par[0].xact = par->direct_show.xact;
	fb_info.win_par[0].area_par[0].yact = par->direct_show.yact;
	fb_info.win_par[0].area_par[0].xvir = par->direct_show.xact;
	fb_info.win_par[0].area_par[0].yvir = par->direct_show.yact;
	fb_info.win_par[0].area_par[0].xpos = x_pos;
	fb_info.win_par[0].area_par[0].ypos = y_pos;
	fb_info.win_par[0].area_par[0].xsize = par->direct_show.xact;
	fb_info.win_par[0].area_par[0].ysize = par->direct_show.yact;
	fb_info.win_par[0].area_par[0].data_format = HAL_PIXEL_FORMAT_RGB_565;
	fb_info.win_par[0].reserved0 = 1;

	// commit buffer to display
	vir_addr = (unsigned long)(&fb_info);
	do {
        i = 0;
        j = 0;

        rk_set_dsp(RK_FBIOSET_CAR_CONFIG_DONE, vir_addr);
        /* DBG("wzqtest:%s,%d\n", __func__, __LINE__); */
        if (fb_info.ret_fence_fd > -1) {
            fence_fd = fb_info.ret_fence_fd;
            if (fence_fd > -1)
			    rel_fence = sync_fence_fdget(fence_fd);
            if (rel_fence) {
			    sync_fence_put(rel_fence);
			    rel_fence = NULL;
			    /*__close_fd(current->files, fence_fd);*/
			    fence_fd = -1;
            }
        }
        for (i = 0; i < RK_MAX_BUF_NUM; i++) {
            fence_fd = fb_info.rel_fence_fd[i];
            if (fence_fd > -1)
			    rel_fence = sync_fence_fdget(fence_fd);
            if (rel_fence) {
			    sync_fence_put(rel_fence);
			    rel_fence = NULL;
			    /*__close_fd(current->files, fence_fd);*/
			    fence_fd = -1;
            }
        }
	} while (0);
#endif

	return 0;
}

static int rk_flinger_vop_show(struct flinger *flinger,
			       struct graphic_buffer *buffer)
{
	struct rk_fb_win_cfg_data fb_info;
	unsigned long vir_addr;
	int i, j, fence_fd, ion_hnd_fd = -1;
	struct sync_fence *rel_fence = NULL;
	struct rk_fb_car_par *par = rk_fb_get_car_par();
	u32 phy_addr = buffer->phy_addr;
	u32 len;

	if (!flinger || !buffer)
		return -EINVAL;

	if (buffer->state != ACQUIRE)
		DBG("buf[%p] not acquired[%d]", buffer, buffer->state);

#ifdef CONFIG_DRM_ROCKCHIP
	// if vop iommu enabled
	if (par->fb_inited && par->iommu_enabled) {
    	  ion_map_iommu(par->dev,
			    flinger->ion_client,
			    buffer->handle,
			    (unsigned long *)&phy_addr,
			    (unsigned long *)&len);
        //DBG("iommu map phy = %x, raw phy = %lx\n", phy_addr, buffer->phy_addr);
	}
#endif
	ion_hnd_fd = rk_flinger_get_buffer_dma_fd(flinger, buffer);
	if (ion_hnd_fd < 0)
		return -EINVAL;

	if (0 == par->car_reversing)
		return 0;

	memset(&fb_info, 0, sizeof(struct rk_fb_win_cfg_data));
	fb_info.ret_fence_fd = -1;
	for (i = 0; i < RK_MAX_BUF_NUM; i++)
		fb_info.rel_fence_fd[i] = -1;

	fb_info.wait_fs = 0;

	fb_info.win_par[0].area_par[0].data_format = buffer->src.f;
	fb_info.win_par[0].win_id = 0;
	fb_info.win_par[0].z_order = 0;
	fb_info.win_par[0].area_par[0].ion_fd = buffer->ion_hnd_fd;
#ifdef CONFIG_DRM_ROCKCHIP    
	fb_info.win_par[0].area_par[0].phy_addr = phy_addr;
#else
	fb_info.win_par[0].area_par[0].phy_addr = 0;
#endif
	fb_info.win_par[0].area_par[0].acq_fence_fd = -1;

	fb_info.win_par[0].area_par[0].x_offset = buffer->src.x;
	fb_info.win_par[0].area_par[0].y_offset = buffer->src.y;
	fb_info.win_par[0].area_par[0].xact = buffer->src.w;
	fb_info.win_par[0].area_par[0].yact = buffer->src.h;
	fb_info.win_par[0].area_par[0].xvir = buffer->src.s;
	fb_info.win_par[0].area_par[0].yvir = buffer->src.h;

	fb_info.win_par[0].area_par[0].xpos = buffer->dst.x;
	fb_info.win_par[0].area_par[0].ypos = buffer->dst.y;
	fb_info.win_par[0].area_par[0].xsize = par->screen_width;
	fb_info.win_par[0].area_par[0].ysize = par->screen_height;
	fb_info.win_par[0].area_par[0].data_format = buffer->src.f;

	vir_addr = (unsigned long)(&fb_info);
	do {
		i = 0;
		j = 0;

		flinger->debug_vop_count++;

        if (0) {
            static struct file *filep;
            static loff_t pos;
            static const int record_frame_count = 200;
            static int frame_count;
            if (++frame_count < record_frame_count) {
                int frame_len = buffer->src.w * buffer->src.h * 4 / 2;
                int ret;
                char path[32] = {0};
                mm_segment_t fs;

                if (!filep) {
                    sprintf(path, "/data/test.dat");
                    filep = filp_open(path, O_CREAT | O_RDWR, 0644);
                    if (IS_ERR(filep))
                        return 0;
                }
                fs = get_fs();
                set_fs(KERNEL_DS);
                ret = vfs_write(filep, buffer->vir_addr, frame_len, &pos);
                set_fs(fs);
            } else if (filep) {
                filp_close(filep, 0);
                filep = NULL;
            }
        }

		rk_set_dsp(RK_FBIOSET_CAR_CONFIG_DONE, vir_addr);

		/* DBG("wzqtest:%s,%d\n", __func__, __LINE__); */
		if (fb_info.ret_fence_fd > -1) {
			fence_fd = fb_info.ret_fence_fd;
			if (fence_fd > -1)
				rel_fence = sync_fence_fdget(fence_fd);
			if (rel_fence) {
				sync_fence_put(rel_fence);
				rel_fence = NULL;
				__close_fd(current->files, fence_fd);
				fence_fd = -1;
			}
		}

		for (i = 0; i < RK_MAX_BUF_NUM; i++) {
			fence_fd = fb_info.rel_fence_fd[i];
			if (fence_fd > -1)
				rel_fence = sync_fence_fdget(fence_fd);
			if (rel_fence) {
				sync_fence_put(rel_fence);
				rel_fence = NULL;
				__close_fd(current->files, fence_fd);
				fence_fd = -1;
			}
		}
	} while (0);

	return 0;
}

static void rk_flinger_first_done(struct work_struct *work)
{
	struct graphic_buffer *buffer;
	struct flinger *flg = flinger;
	int i, fd;

	if (!flg)
		return;

	fd = get_unused_fd();

	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		if (flg->source_buffer[i].state == FREE) {
			buffer = &(flg->source_buffer[i]);
			rk_flinger_set_rect(&buffer->src,
					    FORCE_XOFFSET, FORCE_YOFFSET,
					    FORCE_WIDTH, FORCE_HEIGHT,
					    FORCE_STRIDE, FORCE_FORMAT);
			rk_flinger_set_buffer_rotation(buffer, FORCE_ROTATION);
			rk_flinger_cacultae_dst_rect_by_rotation(buffer);
			buffer->dst.f = buffer->src.f;
		}
	}
}

static void rk_flinger_render_show(struct work_struct *w)
{
	struct graphic_buffer *src_buffer, *dst_buffer, *iep_buffer, *buffer;
	/* struct queue_buffer *cur = NULL, *next = NULL; */
	struct flinger *flg = flinger;
	int i, found = 0;
	static int count = -1;
	static int last_src_index = -1;

	src_buffer = NULL;
	dst_buffer = NULL;
	flg->source_index = 0;

	do {
try_again:
		wait_event_interruptible_timeout(flg->worker_wait,
						 atomic_read(&flg->worker_cond_atomic),
						 msecs_to_jiffies(1000000));
		if (0 == atomic_read(&flg->worker_running_atomic)) {
			DBG("%s loop exit\n", __func__);
			break;
		}
		if (0 >= atomic_read(&flg->worker_cond_atomic)) {
			/*printk("waiting 'worker_cond_atomic' timed out.");*/
			goto try_again;
		}
		atomic_dec(&flg->worker_cond_atomic);

#ifdef FIX_SOURCE_BUFFER
		found = atomic_read(&flg->souce_frame_atomic);
		if (found < 0 || found > 1)
			goto try_again;
		src_buffer = &flg->source_buffer[found];
		last_src_index = found;
#else
		/*  1. find src buffer */
		src_buffer = NULL;
		found = last_src_index + 1;
		for (i = 0; i < NUM_SOURCE_BUFFERS; i++, found++) {
			found = found % NUM_SOURCE_BUFFERS;
			if (flg->source_buffer[found].state == QUEUE) {
				src_buffer = &flg->source_buffer[found];
				last_src_index = found;
				break;
			}
		}
#endif

		if (!src_buffer) {
			msleep(3);
			DBG("[%s:%d] error, no buffer\n", __func__, __LINE__);
			goto try_again;
		}

		//DBG("src buffer %lx\n",src_buffer->phy_addr);
		count++;
		src_buffer->state = ACQUIRE;

		if (!iep_device_ready()) {
			src_buffer->state = FREE;
			DBG("%s iep not ready, continue\n", __func__);
			continue;
		}
		/*  2. find dst buffer */
		dst_buffer = NULL;
		iep_buffer = NULL;

		/*get iep, rga, vop buffer*/
		if (src_buffer->rotation != 0) {
			if (flg->v_cfg.input_format == CIF_INPUT_FORMAT_PAL ||
			    flg->v_cfg.input_format == CIF_INPUT_FORMAT_NTSC ||
			    flg->v_cfg.input_format ==
			    CIF_INPUT_FORMAT_PAL_SW_COMPOSITE ||
				flg->v_cfg.input_format ==
				CIF_INPUT_FORMAT_NTSC_SW_COMPOSITE) {
				iep_buffer = &(flg->target_buffer
					       [NUM_TARGET_BUFFERS - 1]);
			}
			dst_buffer = &(flg->target_buffer
				       [count % (NUM_TARGET_BUFFERS - 1)]);
			dst_buffer->state = ACQUIRE;
		} else if (flg->v_cfg.input_format == CIF_INPUT_FORMAT_PAL ||
			   flg->v_cfg.input_format == CIF_INPUT_FORMAT_NTSC ||
			   flg->v_cfg.input_format ==
			   CIF_INPUT_FORMAT_PAL_SW_COMPOSITE ||
			   flg->v_cfg.input_format ==
			   CIF_INPUT_FORMAT_NTSC_SW_COMPOSITE) {
			iep_buffer = &(flg->target_buffer
				       [count % NUM_TARGET_BUFFERS]);
			iep_buffer->state = ACQUIRE;
		}

		//eip_buffer = NULL;

		/*do deinterlace & rotation & display*/
		if (src_buffer->rotation != 0) {
			if (iep_buffer) {
				rk_flinger_iep_deinterlace(flg, src_buffer,
							   iep_buffer);
				rk_flinger_rga_render(flg,
						      iep_buffer, dst_buffer);
			} else {
				rk_flinger_rga_render(flg,
						      src_buffer, dst_buffer);
				src_buffer->state = FREE;
			}
			rk_flinger_vop_show(flg, dst_buffer);
			for (i = 0; i < NUM_TARGET_BUFFERS; i++) {
				buffer = &(flinger->target_buffer[i]);
				if (buffer->state == DISPLAY)
					buffer->state = FREE;
			}
			dst_buffer->state = DISPLAY;
		} else {
			if (iep_buffer) {
				rk_flinger_iep_deinterlace(flg, src_buffer,
							   iep_buffer);
				rk_flinger_vop_show(flg, iep_buffer);
			} else {
				rk_flinger_vop_show(flg, src_buffer);
			}
			src_buffer->state = FREE;
		}
	} while (1);
}

static int rk_flinger_queue_work(struct flinger *flinger,
				 struct graphic_buffer *src_buffer)
{
	DBG("wzqtest:%s,%d\n", __func__, __LINE__);
	if (!flinger)
		return -ENODEV;

	if (!src_buffer) {
		if (flinger->render_workqueue) {
			INIT_WORK(&flinger->init_work, rk_flinger_first_done);
			queue_work(flinger->render_workqueue,
				   &flinger->init_work);
		}
	}

	if (flinger->render_workqueue) {
		INIT_WORK(&flinger->render_work, rk_flinger_render_show);
		queue_work(flinger->render_workqueue, &flinger->render_work);
	}
	return 0;
}

static struct graphic_buffer *
rk_flinger_lookup_buffer_by_phy_addr(unsigned long phy_addr)
{
	struct graphic_buffer *buffer = NULL;
	struct flinger *flg = flinger;
	int i;

	/*DBG("%s:phy_addr=%lx\n", __func__, phy_addr);*/
	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		if (flg->source_buffer[i].state == DEQUEUE) {
			buffer = &(flg->source_buffer[i]);
			if (buffer && buffer->offset +
			    buffer->phy_addr == phy_addr) {
				buffer->state = QUEUE;
				break;
			}
		}
	}
	if (i < NUM_SOURCE_BUFFERS)
		return buffer;
	else
		return NULL;
}

static bool vehicle_rotation_param_check(struct vehicle_cfg *v_cfg)
{
	switch (v_cfg->rotate_mirror) {
	case RGA_TRANSFORM_ROT_90:
	case RGA_TRANSFORM_ROT_270:
	case RGA_TRANSFORM_ROT_0:
	case RGA_TRANSFORM_ROT_180:
	case RGA_TRANSFORM_FLIP_H:
	case RGA_TRANSFORM_FLIP_V:
		return true;
	default:
		DBG("invalid rotate-mirror param %d\n", v_cfg->rotate_mirror);
		v_cfg->rotate_mirror = 0;
		return false;
	}
}
int vehicle_flinger_reverse_open(struct vehicle_cfg *v_cfg)
{
	int i;
	int width;
	int height;
	struct flinger *flg = flinger;
	struct graphic_buffer *buffer;
	int hal_format;

	width = v_cfg->width;
	height = v_cfg->height;

	if (!flinger)
		return -1;

	DBG("%s\n", __func__);

	vehicle_rotation_param_check(v_cfg);

	if (CIF_OUTPUT_FORMAT_422 == v_cfg->output_format)
		hal_format = HAL_PIXEL_FORMAT_YCbCr_422_SP;
	else
		hal_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

	/*  1. reinit buffer format */
	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		buffer = &(flg->source_buffer[i]);
		rk_flinger_set_rect(&buffer->src,
				    0, 0, width,
				    height, width, hal_format);
		rk_flinger_set_buffer_rotation(buffer, v_cfg->rotate_mirror);
		rk_flinger_cacultae_dst_rect_by_rotation(buffer);
		buffer->dst.f = buffer->src.f;
		buffer->state = FREE;
	}

	/*2. fill buffer info*/
	for (i = 0; i < NUM_SOURCE_BUFFERS && i < MAX_BUF_NUM; i++)
		v_cfg->buf_phy_addr[i] = flinger->source_buffer[i].phy_addr;
	v_cfg->buf_num = NUM_SOURCE_BUFFERS;

	flg->cvbs_field_count = 0;
	memcpy(&flg->v_cfg, v_cfg, sizeof(struct vehicle_cfg));
	rk_set_car_reverse(true);

#ifdef CONFIG_DRM_ROCKCHIP
	logo_buffer = &(flg->cvbs_buffer);
	rk_drm_fb_show_kernel_logo(logo_buffer->phy_addr, logo_buffer->vir_addr);
#endif

	DBG("%s succeed\n", __func__);
	return 0;
}

int vehicle_flinger_reverse_close(void)
{
	struct flinger *flg = flinger;
	struct rk_fb_car_par *par;

	DBG("%s\n", __func__);

	rk_set_car_reverse(false);
	par = rk_fb_get_car_par();
	DBG("%s par->logo_show_logo = %d\n", __func__,par->logo_showing);
	if (par->logo_showing)
		rk_flinger_vop_show_logo(flg, par);
	return 0;
}
/*frame_ready: 0, 1;pingpong buffer of cif*/
unsigned long vehicle_flinger_request_cif_buffer(int frame_ready)
{
#ifdef FIX_SOURCE_BUFFER
	struct flinger *flg = flinger;

	if (frame_ready < 0 || frame_ready > 1)
		return 0;
	return flg->source_buffer[frame_ready].phy_addr;
#else
	struct graphic_buffer *src_buffer = NULL;
	struct flinger *flg = flinger;
	static int last_src_index = -1;
	int found;
	int i;

	src_buffer = NULL;
	found = last_src_index + 1;
	for (i = 0; i < NUM_SOURCE_BUFFERS; i++) {
		found = (found + i) % NUM_SOURCE_BUFFERS;
		if (flg->source_buffer[found].state == FREE) {
			src_buffer = &flg->source_buffer[found];
			last_src_index = found;
			src_buffer->state = DEQUEUE;
			break;
		}
	}

	if (i < NUM_SOURCE_BUFFERS)
		return src_buffer->phy_addr;
	else
		return 0;
#endif
}

void vehicle_flinger_commit_cif_buffer(u32 buf_phy_addr)
{
	struct graphic_buffer *buffer = NULL;
	struct flinger *flg = flinger;

	if (!flg)
		return;

#ifdef FIX_SOURCE_BUFFER
	if (buf_phy_addr == flg->source_buffer[0].phy_addr)
		atomic_set(&flg->souce_frame_atomic, 0);
	else
		atomic_set(&flg->souce_frame_atomic, 1);
	atomic_inc(&flg->worker_cond_atomic);
	flg->debug_cif_count++;
	wake_up(&flg->worker_wait);
	return;
#endif

	buffer = rk_flinger_lookup_buffer_by_phy_addr(buf_phy_addr);
	if (buffer) {
		atomic_inc(&flg->worker_cond_atomic);
		flg->debug_cif_count++;
		wake_up(&flg->worker_wait);
	} else {
		DBG("%s,%x, no free buffer\n", __func__, buf_phy_addr);
	}
}
