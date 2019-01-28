#include <iostream>
#include <time.h>
#include <fcntl.h>
#ifdef RK_ISP10
#include <linux/v4l2-controls.h>
#include <media/rk-isp10-config.h>
#endif
#ifdef RK_ISP11
#include <linux/v4l2-controls.h>
#include <media/rk-isp11-config.h>
#endif
#include <CameraHal/CameraBuffer.h>
#include <CameraHal/CamHwItf.h>
#ifdef SUPPORT_ION
#include <CameraHal/IonCameraBuffer.h>
#endif
#include <CameraHal/CamCifDevHwItf.h>
#include <CameraHal/camHalTrace.h>
#include <CameraHal/StrmPUBase.h>

#include <CameraHal/linux/v4l2-controls.h>

#include <CameraHal/CameraIspTunning.h>
#ifdef USE_RK_DISPLAY
#include "fb.h"
#endif

#ifdef USE_DRM_DISPLAY
#include "drmDsp.h"
#include <CameraHal/DrmCameraBuffer.h>
#endif

#include <iep/iep_api.h>

#define CIF_DROP_FRAMES (3) /*drop first * buffers to avoid acquisition err display*/
#define CIF_RESET_TIME_S (2) /*restart after * sencods no data*/

//#define TEST_WRITE_SP_TO_FILE

using namespace std;
#define DECLAREPERFORMANCETRACK(name) \
  static int mFrameCount##name = 0;\
  static int mLastFrameCount##name = 0;\
  static long long mLastFpsTime##name = 0;\
  static float mFps##name = 0;
#define SHOWPERFORMACEFPS(name) \
  { \
    mFrameCount##name++;\
    if (!(mFrameCount##name & 0x1F)) { \
      struct timeval now; \
      gettimeofday(&now,NULL); \
      long long diff = now.tv_sec*1000000 + now.tv_usec - mLastFpsTime##name; \
      mFps##name = ((mFrameCount##name - mLastFrameCount##name) * float(1000*1000)) / diff; \
      mLastFpsTime##name = now.tv_sec*1000000 + now.tv_usec; \
      mLastFrameCount##name = mFrameCount##name; \
      LOGD("%s:%d Frames, %2.3f FPS",#name, mFrameCount##name, mFps##name); \
    } \
  }

#ifdef USE_RK_DISPLAY
static struct fb gFb = {0};
#endif

void dump_time(char *label) {
    static struct timeval t0;
    struct timeval t1;

    gettimeofday(&t1, NULL);

    long long elapsed = (t1.tv_sec-t0.tv_sec)*1000000LL + t1.tv_usec-t0.tv_usec;
    t0 = t1;

    printf("%s:time elapse = %ld\n", label, elapsed / 1000);
}

class CameraBufferOwenerImp : public ICameraBufferOwener {
 public:
  virtual bool releaseBufToOwener(weak_ptr<BufferBase> camBuf) { UNUSED_PARAM(camBuf); return false;}
};

static void iep_process_deinterlace(uint16_t src_w,
                                    uint16_t src_h,
                                    uint32_t src_fd,
                                    uint16_t dst_w,
                                    uint16_t dst_h,
                                    uint32_t dst_fd)
{
    iep_interface* api = iep_interface::create_new();
    iep_img src1;
    iep_img dst1;

    src1.act_w = src_w;
    src1.act_h = src_h;
    src1.x_off = 0;
    src1.y_off = 0;
    src1.vir_w = src_w;
    src1.vir_h = src_h;
    src1.format = IEP_FORMAT_YCbCr_420_SP;
    src1.mem_addr = src_fd;
    src1.uv_addr = src_fd | (src_w * src_h) << 10;
    src1.v_addr = 0;

    dst1.act_w = dst_w;
    dst1.act_h = dst_h;
    dst1.x_off = 0;
    dst1.y_off = 0;
    dst1.vir_w = dst_w;
    dst1.vir_h = dst_h;
    dst1.format = IEP_FORMAT_YCbCr_420_SP;
    dst1.mem_addr = dst_fd;
    dst1.uv_addr = dst_fd | (dst_w * dst_h) << 10;
    dst1.v_addr = 0;

    api->init(&src1, &dst1);

    api->config_yuv_deinterlace();

    if (api->run_sync())
        printf("%d failure\n", getpid());

    iep_interface::reclaim(api);
}

static void video_record_signal(void);

class NV12_IEP : public StreamPUBase
{
public:
    NV12_IEP() : StreamPUBase("NV12_IEP", true, false) {
        mBufferTimeoutCount = 0;
    }
    ~NV12_IEP() {}
    bool processFrame(shared_ptr<BufferBase> inBuf,
                      shared_ptr<BufferBase> outBuf) {
        if (!inBuf.get()) {
            printf("NULL buffer count %d\n", ++mBufferTimeoutCount);
            if (mBufferTimeoutCount >= CIF_RESET_TIME_S) {
                printf("timeout time expire, restart cif\n");
                video_record_signal();
            }
        }
        else
            mBufferTimeoutCount = 0;

        if (inBuf.get() && outBuf.get()) {
            iep_process_deinterlace(inBuf->getWidth(), inBuf->getHeight(),
                                    inBuf->getFd(), outBuf->getWidth(),
                                    outBuf->getHeight(), outBuf->getFd());
            outBuf->setDataSize(inBuf->getWidth() * inBuf->getHeight() * 3 / 2);
        }

        return true;
    }
private:
    int mBufferTimeoutCount;
};

class NV12_Display : public StreamPUBase
{
public:
    NV12_Display() : StreamPUBase("NV12_Display", true, true) {
        mFrameCounts = 0;
    }
    ~NV12_Display() {
		drmDspReleaseFrames();
	}
    bool processFrame(shared_ptr<BufferBase> inBuf,
                      shared_ptr<BufferBase> outBuf) {
        if (inBuf.get() && mFrameCounts++ >= CIF_DROP_FRAMES) {
			DECLAREPERFORMANCETRACK(nv12);
			SHOWPERFORMACEFPS(nv12);
			drmDspFrame(inBuf->getWidth(),
                  inBuf->getHeight(),
                  //(int)(spCamBuf->getVirtAddr()),
                  (int)(inBuf->getFd()),
                  DRM_FORMAT_NV12);
        }
        return true;
    }
private:
    int mFrameCounts;
};

static struct rk_cams_dev_info g_test_cam_infos;
#define RUNNINGTIME_MINUTE 3
#define RUNNINGTIME_MS (RUNNINGTIME_MINUTE*1000)

void openCifDev(int videoDevNum, int inputid);

#if 1
osThread camera_thread;
osEvent camera_event;
bool camera_running = false;
frm_info_t gFrmFmt;
static int camera_ad_channel;/*ad channel select for camera*/
static bool inited = false;


static void video_record_wait(void) {
    osEventWait(&camera_event);
}

static void video_record_signal(void) {
    osEventSignal(&camera_event);
}

static int main_thread(void* arg) 
{
  while(camera_running) {
    openCifDev(0, 0);
    if (camera_running)
        osSleep(300);
  }
  return 0;
}

int rk_camera_init(void) {
  if (inited)
	return 0;
  
  inited = true;
#ifdef USE_RK_DISPLAY
  rk_fb_open(&gFb);
#endif

#ifdef USE_DRM_DISPLAY
  if (initDrmDsp() < 0)
    ALOGD("DRM disp init failed !");
#endif

  frm_info_t frmFmt = {
    .frmSize = {720, 576},
    .frmFmt = HAL_FRMAE_FMT_NV12,
    .colorSpace = HAL_COLORSPACE_SMPTE170M,
    .fps = 30,
  };

  gFrmFmt = frmFmt;
  return 0;
}

int rk_camera_deinit(void) {
  if (!inited)
    return 0;

  inited = false;
  ALOGD("rk_camera_deinit !\n");
#ifdef USE_RK_DISPLAY
  rk_fb_close(&gFb);
#endif
#ifdef USE_DRM_DISPLAY
  ALOGD("before deInitDrmDsp !\n");
  deInitDrmDsp();
#endif
  ALOGD("tests end !\n");

  return 0;
}

/*must set channel before camera start
* channel start from 0
*/
void rk_camera_set_ad_channel(int channel) {
	camera_ad_channel = channel;
}

int rk_camera_get_ad_channel(void) {
	return camera_ad_channel;
}

int rk_camera_start(void) {
    camera_running = true;
    osEventInit(&camera_event, true, 0);
    if (osThreadCreate(&camera_thread, main_thread, NULL)) {
      printf("%s pthread create err!\n", __func__);
      return -1;
    }
    return 0;
}

int rk_camera_stop(void) {
    camera_running = false;
    dump_time("signal enter");
    video_record_signal();
    dump_time("signal exit");
    osThreadClose(&camera_thread);
    dump_time("osThreadClose exit");
    osEventDestroy(&camera_event);
    dump_time("osEventDestroy exit");
    return 0;
}

int rk_camera_change(int width, int height, int fps) {

	if (width == gFrmFmt.frmSize.width && 
	    height == gFrmFmt.frmSize.height)
		return 0;

	frm_info_t frmFmt = {
		.frmSize = {width, height},
		.frmFmt = HAL_FRMAE_FMT_NV12,
		.colorSpace = HAL_COLORSPACE_SMPTE170M,
		.fps = fps,
	};

	gFrmFmt = frmFmt;
    if (camera_running)
		video_record_signal();
	return 0;
}
int _main(int argc, const char* argv[]) {
    rk_camera_init();
    rk_camera_start();
    getchar();
    printf("get char to stop\n");
    rk_camera_stop();
    printf("stop succeed\n");
    rk_camera_deinit();
    return 0;
}
#endif

void openCifDev(int videoDevNum, int inputid) {
  int i = 0;

  dump_time("openCifDev enter");
  memset(&g_test_cam_infos, 0, sizeof(g_test_cam_infos));
  CamHwItf::getCameraInfos(&g_test_cam_infos);

  if (g_test_cam_infos.num_camers <= 0) {
    ALOGE("%s:no camera connected!!");
    return;
  }
  //search cif connectd cameras
  for (i = 0; i < g_test_cam_infos.num_camers; i++) {
    if ((g_test_cam_infos.cam[i]->type == RK_CAM_ATTACHED_TO_CIF) &&
        (g_test_cam_infos.cam[i]->dev == (&(g_test_cam_infos.cif_devs.cif_devs[videoDevNum]))) &&
        (g_test_cam_infos.cam[i]->index == inputid)) {
      ALOGD("connected cif camera name %s,input id %d", g_test_cam_infos.cam[i]->name,
            g_test_cam_infos.cam[i]->index);
      break;
    }
  }

  if (i == g_test_cam_infos.num_camers)
    ALOGE("no input %d connected to cif  %d !!", inputid, videoDevNum);

  ALOGD("construct CIF dev........");
  shared_ptr<CamHwItf> openCifDev =  shared_ptr<CamHwItf>\
                                     (new CamCifDevHwItf(&(g_test_cam_infos.cif_devs.cif_devs[videoDevNum])));//getCamHwItf();
  ALOGD("init CIF dev......");
  if (openCifDev->initHw(g_test_cam_infos.cam[i]->index) == false)
    ALOGE("isp CIF init error !\n");

#ifdef SUPPORT_ION //alloc buffers
  shared_ptr<CamHwItf::PathBase> mpath = openCifDev->getPath(CamHwItf::MP);
  shared_ptr<IonCameraBufferAllocator> bufAlloc(new IonCameraBufferAllocator());
  frm_info_t frmFmt = gFrmFmt; 
  frm_info_t outFmt;

  mpath->setCtrl(V4L2_CID_CHANNEL, camera_ad_channel);
  ALOGD("set channel 0");

  openCifDev->tryFormat(frmFmt, outFmt);

  gFrmFmt = outFmt;

  dump_time("atfer tryFormat");
  ALOGE("after tryFormat, %d x %d\n", outFmt.frmSize.width, outFmt.frmSize.height);

  //NewCameraBufferReadyNotifier* mMpBufNotifer = new CambufNotifierImp();
  shared_ptr<NV12_IEP> nv12_iep = shared_ptr<NV12_IEP>(new NV12_IEP());
  mpath->addBufferNotifier(nv12_iep.get());
  nv12_iep->prepare(outFmt, 4, bufAlloc);

  shared_ptr<NV12_Display> nv12_disp = shared_ptr<NV12_Display>(new NV12_Display());
  nv12_disp->prepare(outFmt, 0, NULL);
  nv12_iep->addBufferNotifier(nv12_disp.get());

  if (mpath->prepare(outFmt, 4, *(bufAlloc.get()), false, 0)) {
    dump_time("prepare exit");
    if (mpath->start()) {
	  nv12_iep->start();
      nv12_disp->start();
      dump_time("start exit");

      LOGD("wait for camera stop request");
      video_record_wait();
	  LOGD("receive camera stop request, stopping camera");

      nv12_iep->removeBufferNotifer(nv12_disp.get());
      nv12_disp->stop();
      nv12_disp->releaseBuffers();

      mpath->removeBufferNotifer(nv12_iep.get());
      nv12_iep->stop();
      nv12_iep->releaseBuffers();

      mpath->stop();
      mpath->releaseBuffers();
    }
  } else {
    ALOGE("prepare failed!\n");
  }
#endif
  //deinit HW
  ALOGD("deinit cif dev......");
  openCifDev->deInitHw();
  //delete isp dev
  ALOGD("destruct cif dev......");
  openCifDev.reset();
}

