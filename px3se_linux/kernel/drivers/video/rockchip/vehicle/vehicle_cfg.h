#ifndef __VEHICLE_CFG
#define __VEHICLE_CFG
#include "media/soc_camera.h"
/* Driver information */
#define VEHICLE_DRIVER_NAME		"Vehicle"

#define VEHICLE_VERSION   "1.000"

#define VEHICLE_DEBUG 1

#if VEHICLE_DEBUG
#define DBG(format, args...) \
	pr_info("%s %s(%d): " format, VEHICLE_DRIVER_NAME, \
		__func__, __LINE__, ## args)
#else
#define DBG(format, args...)
#endif

#define MAX_BUF_NUM (6)

#define CVBS_DOUBLE_FPS_MODE	/*PAL 50fps; NTSC 60fps*/

enum {
	CIF_INPUT_FORMAT_YUV = 0,
	CIF_INPUT_FORMAT_PAL = 2,
	CIF_INPUT_FORMAT_NTSC = 3,
	CIF_INPUT_FORMAT_RAW = 4,
	CIF_INPUT_FORMAT_JPEG = 5,
	CIF_INPUT_FORMAT_MIPI = 6,
	CIF_INPUT_FORMAT_PAL_SW_COMPOSITE = 0xff000000,
	CIF_INPUT_FORMAT_NTSC_SW_COMPOSITE = 0xfe000000,
};

enum {
	CIF_OUTPUT_FORMAT_422 = 0,
	CIF_OUTPUT_FORMAT_420 = 1,
};

struct vehicle_cfg {
	int width;
	int height;
	/*
action:	source video data input format.
000 - YUV
010 - PAL
011 - NTSC
100 - RAW
101 - JPEG
110 - MIPI
*/
	int input_format;
	/*
	   0 - output is 422
	   1 - output is 420
	   */
	int output_format;
	/*
	   YUV input order
	   00 - UYVY
	   01 - YVYU
	   10 - VYUY
	   11 - YUYV
	   */
	int yuv_order;
	/*
	   ccir input order
0 : odd field first
1 : even field first
*/
	int field_order;
	/*
	   BT.656 not use
	   BT.601 hsync polarity
val:
0-low active
1-high active
*/
	int href;
	/*
	   BT.656 not use
	   BT.601 hsync polarity
val :
0-low active
1-high active
*/
	int vsync;
	int start_x;
	int start_y;
	int frame_rate;

	unsigned int buf_phy_addr[MAX_BUF_NUM];
	unsigned int buf_num;
	bool ad_ready;
	/*0:no, 1:90; 2:180; 4:270; 0x10:mirror-y; 0x20:mirror-x*/
	int rotate_mirror;
	unsigned int bus_param;
};

#endif
