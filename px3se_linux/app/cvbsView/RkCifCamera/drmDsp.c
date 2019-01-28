#include <stdio.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <drm_fourcc.h>
#include <string.h>
#include <errno.h>

#include "xf86drm.h"
#include "xf86drmMode.h"
#include "drmDsp.h"

#define FALSE (0)
#define TRUE (1)

struct sp_bo {
  int drm_fd;

  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t bpp;
  uint32_t format;
  uint32_t flags;

  uint32_t fb_id;
  uint32_t handle;
  void* map_addr;
  uint32_t pitch;
  uint32_t size;

  uint32_t fd;
  struct sp_bo *next;
};

struct drmDsp {
  struct fb_var_screeninfo vinfo;
  unsigned long screensize;
  struct sp_bo* initbo;
  struct sp_bo* nextbo;

  int fd;

  int conn_id;
  int crtc_id;
  int plane_id;
  uint32_t pipe;

  /* crtc data */
  uint16_t hdisplay;
  uint16_t vdisplay;
  uint32_t buffer_id;

  /* capabilities */
  int has_prime_import;
  int has_async_page_flip;
  int can_scale;

  uint32_t mm_width;
  uint32_t mm_height;
} gDrmDsp;


static drmModePlane *
find_plane_for_crtc (int fd, drmModeRes * res, drmModePlaneRes * pres,
    int crtc_id)
{
  drmModePlane *plane;
  int i, pipe;

  plane = NULL;
  pipe = -1;
  for (i = 0; i < res->count_crtcs; i++) {
    if (crtc_id == res->crtcs[i]) {
      pipe = i;
      break;
    }
  }

  if (pipe == -1)
    return NULL;

  for (i = 0; i < pres->count_planes; i++) {
    plane = drmModeGetPlane (fd, pres->planes[i]);
    if (plane->possible_crtcs & (1 << pipe))
      return plane;
    drmModeFreePlane (plane);
  }

  return NULL;
}

static drmModeCrtc *
find_crtc_for_connector (int fd, drmModeRes * res, drmModeConnector * conn,
    uint32_t * pipe)
{
  int i;
  int crtc_id;
  drmModeEncoder *enc;
  drmModeCrtc *crtc;
  uint32_t crtcs_for_connector = 0;

  crtc_id = -1;
  for (i = 0; i < res->count_encoders; i++) {
    enc = drmModeGetEncoder (fd, res->encoders[i]);
    if (enc) {
      if (enc->encoder_id == conn->encoder_id) {
        crtc_id = enc->crtc_id;
        drmModeFreeEncoder (enc);
        break;
      }
      drmModeFreeEncoder (enc);
    }
  }

  /* If no active crtc was found, pick the first possible crtc */
  if (crtc_id == -1) {
    for (i = 0; i < conn->count_encoders; i++) {
      enc = drmModeGetEncoder (fd, conn->encoders[i]);
      crtcs_for_connector |= enc->possible_crtcs;
      drmModeFreeEncoder (enc);
    }

    if (crtcs_for_connector != 0)
      crtc_id = res->crtcs[ffs (crtcs_for_connector) - 1];
  }

  if (crtc_id == -1)
    return NULL;

  for (i = 0; i < res->count_crtcs; i++) {
    crtc = drmModeGetCrtc (fd, res->crtcs[i]);
    if (crtc) {
      if (crtc_id == crtc->crtc_id) {
        if (pipe)
          *pipe = i;
        return crtc;
      }
      drmModeFreeCrtc (crtc);
    }
  }

  return NULL;
}
static int
connector_is_used (int fd, drmModeRes * res, drmModeConnector * conn)
{
  int result;
  drmModeCrtc *crtc;

  result = FALSE;
  crtc = find_crtc_for_connector (fd, res, conn, NULL);
  if (crtc) {
    result = crtc->buffer_id != 0;
    drmModeFreeCrtc (crtc);
  }

  return result;
}

static drmModeConnector *
find_used_connector_by_type (int fd, drmModeRes * res, int type)
{
  int i;
  drmModeConnector *conn;

  conn = NULL;
  for (i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector (fd, res->connectors[i]);
    if (conn) {
      if ((conn->connector_type == type) && connector_is_used (fd, res, conn))
        return conn;
      drmModeFreeConnector (conn);
    }
  }

  return NULL;
}
static drmModeConnector *
find_first_used_connector (int fd, drmModeRes * res)
{
  int i;
  drmModeConnector *conn;

  conn = NULL;
  for (i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector (fd, res->connectors[i]);
    if (conn) {
      if (connector_is_used (fd, res, conn))
        return conn;
      drmModeFreeConnector (conn);
    }
  }

  return NULL;
}

static drmModeConnector *
find_main_monitor (int fd, drmModeRes * res)
{
  /* Find the LVDS and eDP connectors: those are the main screens. */
  static const int priority[] = { DRM_MODE_CONNECTOR_LVDS,
    DRM_MODE_CONNECTOR_eDP
  };
  int i;
  drmModeConnector *conn;

  conn = NULL;
  for (i = 0; !conn && i < sizeof(priority); i++)
    conn = find_used_connector_by_type (fd, res, priority[i]);

  /* if we didn't find a connector, grab the first one in use */
  if (!conn)
    conn = find_first_used_connector (fd, res);

  /* if no connector is used, grab the first one */
  if (!conn)
    conn = drmModeGetConnector (fd, res->connectors[0]);

  return conn;
}

int add_fb_sp_bo(struct sp_bo* bo, uint32_t format) {
  int ret;
  uint32_t handles[4], pitches[4], offsets[4];

  handles[0] = bo->handle;
  pitches[0] = bo->pitch;
  offsets[0] = 0;

  ret = drmModeAddFB2(bo->drm_fd, bo->width, bo->height,
                      format, handles, pitches, offsets,
                      &bo->fb_id, bo->flags);
  if (ret) {
    printf("failed to create fb ret=%d\n", ret);
    return ret;
  }
  return 0;
}

static int map_sp_bo(struct sp_bo* bo) {
  int ret;
  struct drm_mode_map_dumb md;

  if (bo->map_addr)
    return 0;

  md.handle = bo->handle;
  ret = drmIoctl(bo->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &md);
  if (ret) {
    printf("failed to map sp_bo ret=%d\n", ret);
    return ret;
  }

  bo->map_addr = mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                      bo->drm_fd, md.offset);
  if (bo->map_addr == MAP_FAILED) {
    printf("failed to map bo ret=%d\n", -errno);
    return -errno;
  }
  return 0;
}
struct sp_bo* create_sp_bo(struct drmDsp * dev, uint32_t width, uint32_t height,
                           uint32_t depth, uint32_t bpp, uint32_t format, uint32_t flags) {
  int ret;
  struct drm_mode_create_dumb cd;
  struct sp_bo* bo;

  memset(&cd, 0, sizeof(cd));

  bo = calloc(1, sizeof(*bo));
  if (!bo)
    return NULL;

  cd.height = height;
  cd.width = width;
  cd.bpp = bpp;
  cd.flags = flags;

  ret = drmIoctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd);
  if (ret) {
    printf("failed to create sp_bo %d\n", ret);
    return NULL;
  }

  bo->drm_fd = dev->fd;
  bo->width = width;
  bo->height = height;
  bo->depth = depth;
  bo->bpp = bpp;
  bo->format = format;
  bo->flags = flags;

  bo->handle = cd.handle;
  bo->pitch = cd.pitch;
  bo->size = cd.size;

  ret = map_sp_bo(bo);
  if (ret) {
    printf("failed to map bo ret=%d\n", ret);
    return NULL;
  }

  return bo;
}


void free_sp_bo(struct sp_bo* bo) {
  int ret;
  struct drm_mode_destroy_dumb dd;

  if (!bo)
    return;

  if (bo->map_addr)
    munmap(bo->map_addr, bo->size);

  if (bo->fb_id) {
    ret = drmModeRmFB(bo->drm_fd, bo->fb_id);
    bo->fb_id = 0;
    if (ret)
      printf("Failed to rmfb ret=%d!\n", ret);
  }

  if (bo->handle) {
    dd.handle = bo->handle;
    ret = drmIoctl(bo->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
    if (ret)
      printf("Failed to destroy buffer ret=%d\n", ret);
  }

  free(bo);
}

void draw_rect(struct sp_bo* bo, uint32_t x, uint32_t y, uint32_t width,
               uint32_t height, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t i, j, xmax = x + width, ymax = y + height;

  if (xmax > bo->width)
    xmax = bo->width;
  if (ymax > bo->height)
    ymax = bo->height;

  for (i = y; i < ymax; i++) {
    uint8_t* row = bo->map_addr + i * bo->pitch;

    for (j = x; j < xmax; j++) {
      uint8_t* pixel = row + j * 4;

      if (bo->format == DRM_FORMAT_ARGB8888 ||
          bo->format == DRM_FORMAT_XRGB8888) {
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
        pixel[3] = a;
      } else if (bo->format == DRM_FORMAT_RGBA8888) {
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
        pixel[3] = a;
      }
    }
  }
}

void fill_bo(struct sp_bo* bo, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
  draw_rect(bo, 0, 0, bo->width, bo->height, a, r, g, b);
}

int initDrmDsp() {
  struct drmDsp *dev = &gDrmDsp;
  int fd;
  int ret;
  drmModeRes* res = NULL;
  drmModePlaneRes* pres = NULL;
  drmModeConnector *conn;
  drmModeCrtc *crtc;
  drmModePlane *plane;

  memset(dev, 0, sizeof(struct drmDsp));

  fd = open("/dev/dri/card0", 0x0002);//O_RDWR);
  if (fd < 0) {
    printf("failed to open card0\n");
    return NULL;
  }

  dev->fd = fd;

  res = drmModeGetResources(dev->fd);
  if (!res) {
    printf("failed to get res\n");
    goto err;
  }

  conn = find_main_monitor (dev->fd, res);
  if (!conn) {
    printf("failed to get conn\n");
    goto err;
  }

  crtc = find_crtc_for_connector (dev->fd, res, conn, &dev->pipe);
  if (!crtc) {
    printf("failed to get crtc\n");
    goto err;
  }

  pres = drmModeGetPlaneResources (dev->fd);
  if (!pres) {
    printf("failed to get plane pres\n");
    goto err;
  }

  plane = find_plane_for_crtc (dev->fd, res, pres, crtc->crtc_id);
  if (!crtc) {
    printf("failed to get plane\n");
    goto err;
  }

  dev->conn_id = conn->connector_id;
  dev->crtc_id = crtc->crtc_id;
  dev->plane_id = plane->plane_id;

  printf("connector id = %d / crtc id = %d / plane id = %d\n",
      dev->conn_id, dev->crtc_id, dev->plane_id);

  dev->hdisplay = crtc->mode.hdisplay;
  dev->vdisplay = crtc->mode.vdisplay;
  dev->buffer_id = crtc->buffer_id;

  dev->mm_width = conn->mmWidth;
  dev->mm_height = conn->mmHeight;

  printf("h = %d / v = %d / buffer_id = %d, plane_id = %d\n",
      dev->hdisplay, dev->vdisplay, dev->buffer_id, dev->plane_id);  

  if (plane)
    drmModeFreePlane (plane);
  if (pres)
    drmModeFreePlaneResources (pres);
  if (crtc)
    drmModeFreeCrtc (crtc);
  if (conn)
    drmModeFreeConnector (conn);
  if (res)
    drmModeFreeResources(res);

  /*init black screen*/
  dev->initbo = create_sp_bo(dev, dev->hdisplay, dev->vdisplay,
                             24, 32, DRM_FORMAT_XRGB8888, 0);
  if (!dev->initbo) {
    printf("failed to create new init bo\n");
    goto err;
  }

  ret = add_fb_sp_bo(dev->initbo, DRM_FORMAT_XRGB8888);
  if (ret) {
    printf("failed to add fb ret=%d\n", ret);
    goto err;
  }

  fill_bo(dev->initbo, 0xFF, 0x11, 0x11, 0x11);

  ret = drmModeSetPlane(dev->fd, dev->plane_id,
                  dev->crtc_id, dev->initbo->fb_id, 0, 0, 0,
                  dev->hdisplay,
                  dev->vdisplay,
                  0, 0,
                  dev->initbo->width << 16,
                  dev->initbo->height << 16);
  return 0;

err:
  if (plane)
    drmModeFreePlane (plane);
  if (pres)
    drmModeFreePlaneResources (pres);
  if (crtc)
    drmModeFreeCrtc (crtc);
  if (conn)
    drmModeFreeConnector (conn);
  if (res)
    drmModeFreeResources(res);

  return -1;
}
struct sp_bo* bo_list_search(struct drmDsp *pDrmDsp, int dmaFd)
{
  struct sp_bo *bo = pDrmDsp->nextbo;

  while (bo) {
    if (bo->fd == dmaFd)
      return bo;
    bo = bo->next;
  }
  return NULL;
}

void bo_list_add_head(struct drmDsp *pDrmDsp, struct sp_bo *bo)
{
  struct sp_bo *tmp_bo;

  tmp_bo = pDrmDsp->nextbo;
  pDrmDsp->nextbo = bo;
  bo->next = tmp_bo;
}

void bo_list_free(struct drmDsp *pDrmDsp)
{
  struct sp_bo *bo = pDrmDsp->nextbo;
  struct sp_bo *tmp;
  struct drm_gem_close args;

  memset(&args, 0, sizeof(args));
  while (bo) {
    int ret;

    if (!bo)
      return;

    if (bo->map_addr)
      munmap(bo->map_addr, bo->size);

    if (bo->fb_id) {
      ret = drmModeRmFB(bo->drm_fd, bo->fb_id);
      if (ret)
        printf("Failed to rmfb drm_fd %d, fb_id %d, ret=%d!\n", bo->drm_fd, bo->fb_id, ret);
      bo->fb_id = 0;
      args.handle = bo->handle;
      ret = drmIoctl(bo->drm_fd, DRM_IOCTL_GEM_CLOSE, &args);
      if (ret) {
        printf("failed to close bo->drm_fd ret=%d\n", ret);
        return ret;
      }
    }
    tmp = bo;
    bo = bo->next;
    free(tmp);
  }
  pDrmDsp->nextbo = NULL;
}

void deInitDrmDsp() {
  struct drmDsp* pDrmDsp = &gDrmDsp;

  printf("deInitDrmDsp\n");
  fflush(stdout);
  bo_list_free(pDrmDsp);
  printf("after free bolist\n");
  fflush(stdout);

  if (pDrmDsp->initbo) {
    free_sp_bo(pDrmDsp->initbo);
    pDrmDsp->initbo = NULL;
  }
  printf("after free initbo\n");
  fflush(stdout);

  if (pDrmDsp->fd >= 0) {
    drmClose (pDrmDsp->fd);
    pDrmDsp->fd = -1;
  }
  memset(pDrmDsp, 0, sizeof(struct drmDsp));
}

int drmDspReleaseFrames() {
  struct drmDsp* pDrmDsp = &gDrmDsp;
  bo_list_free(pDrmDsp);
  return 0;
}

int drmDspFrame(int width, int height, int dmaFd, int fmt) {
  int ret;
  struct drm_mode_create_dumb cd;
  struct sp_bo* bo;
  struct drmDsp* pDrmDsp = &gDrmDsp;
  int wAlign16 = ((width + 15) & (~15));
  int hAlign16 = ((height + 15) & (~15));
  int frameSize = wAlign16 * hAlign16 * 3 / 2;
  int dmafd = dmaFd;
  uint32_t handles[4], pitches[4], offsets[4];

  if (DRM_FORMAT_NV12 != fmt)
    return -1;

  bo = bo_list_search(pDrmDsp, dmaFd);
  if (!bo) {
    bo = calloc(1, sizeof(*bo));
    if (!bo)
      return NULL;
    memset(bo, 0, sizeof(*bo));
	memset(handles, 0, sizeof(handles));
	memset(pitches, 0, sizeof(pitches));
	memset(offsets, 0, sizeof(offsets));

    ret = drmPrimeFDToHandle(pDrmDsp->fd, dmaFd, &bo->handle);
	if (ret) {
		printf("drmPrimeFDToHandle failed, dmaFd %d, ret %d\n", dmaFd, ret);
		exit(-1);
	}
	bo->drm_fd = pDrmDsp->fd;
    bo->width = wAlign16;
    bo->height = hAlign16;
    bo->depth = 16;
    bo->bpp = 32;
    bo->format = DRM_FORMAT_NV12;
    bo->flags = 0;
    bo->fd = dmaFd;

    handles[0] = bo->handle;
    pitches[0] = bo->width;
    offsets[0] = 0;
    handles[1] = bo->handle;
    pitches[1] = bo->width;
    offsets[1] = bo->width * bo->height;

    printf("addfb2: fd:%d ,wxh:%ux%u,format:%u,handles:%u,%u,pictches:%u,%u,offsets:%u,%u,fb_id:%u,flags:%u \n",
         pDrmDsp->fd, bo->width, bo->height, bo->format,
         handles[0], handles[1], pitches[0], pitches[1],
         offsets[0], offsets[1], bo->fb_id, bo->flags);

    ret = drmModeAddFB2(bo->drm_fd, bo->width, bo->height,
    				bo->format, handles, pitches, offsets,
    				&bo->fb_id, bo->flags);
    if (ret) {
      printf("failed to addfb2, dmafd %d, ret=%d\n",dmaFd, ret);
      exit(-1);
    }
	bo_list_add_head(pDrmDsp, bo);
  }
  /*printf ("drmModeSetPlane at (%i,%i) %ix%i sourcing at (%i,%i) %ix%i \n",
      0,0,pDrmDsp->hdisplay,pDrmDsp->vdisplay,
      0,0, width, height);*/
  ret = drmModeSetPlane(pDrmDsp->fd, pDrmDsp->plane_id,
  				  pDrmDsp->crtc_id, bo->fb_id, 0, 0, 0,
  				  pDrmDsp->hdisplay,
  				  pDrmDsp->vdisplay,
  				  0, 0, bo->width << 16, bo->height << 16);

}
