
#include "DrmCameraBuffer.h"
#include "camHalTrace.h"

using namespace std;

#define drm_debug (1)

#define DRM_FUNCTION                (0x00000001)
#define DRM_DEVICE                  (0x00000002)
#define DRM_CLIENT                  (0x00000004)
#define DRM_IOCTL                   (0x00000008)

#if drm_debug
	#define drm_dbg(flag, fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
	#define drm_dbg(flag, fmt, ...)
#endif

#define drm_err printf

typedef enum DRM_ERR_T{
	DRM_ERR_NULL_PTR = -1,
	DRM_ERR_UNKNOW = -2,
	DRM_ERR_MALLOC = -3,
} DRM_ERR;

typedef struct DrmBufferInfo_t {
    size_t          size;
    void            *ptr;
    void            *hnd;
    int             fd;
    int             index;
} DrmBufferInfo;

static const char *dev_drm = "/dev/dri/card0";

static int drm_ioctl(int fd, int req, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, req, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    drm_dbg(DRM_FUNCTION, "drm_ioctl %x with code %d: %s\n", req,
            ret, strerror(errno));

    return ret;
}

static void* drm_mmap(int fd, size_t len, int prot, int flags, loff_t offset)
{
    static unsigned long pagesize_mask = 0;
#if !defined(__gnu_linux__)
    func_mmap64 fp_mmap64 = mpp_rt_get_mmap64();
#endif

    if (fd < 0)
        return NULL;

    if (!pagesize_mask)
        pagesize_mask = sysconf(_SC_PAGESIZE) - 1;

    offset = (offset + pagesize_mask) & ~pagesize_mask;

#if !defined(__gnu_linux__)
    if (fp_mmap64)
        return fp_mmap64(NULL, len, prot, flags, fd, offset);

    return NULL;
#else
    return mmap(NULL, len, prot, flags, fd, offset);
#endif
}

static int drm_handle_to_fd(int fd, RK_U32 handle, int *map_fd, RK_U32 flags)
{
    int ret;
    struct drm_prime_handle dph;
    memset(&dph, 0, sizeof(struct drm_prime_handle));
    dph.handle = handle;
    dph.fd = -1;
    dph.flags = flags;

    if (map_fd == NULL)
        return -EINVAL;

    ret = drm_ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &dph);
    if (ret < 0) {
        return ret;
    }

    *map_fd = dph.fd;

    drm_dbg(DRM_FUNCTION, "get fd %d", *map_fd);

    if (*map_fd < 0) {
        drm_err("map ioctl returned negative fd\n");
        return -EINVAL;
    }

    return ret;
}

static int drm_fd_to_handle(int fd, int map_fd, RK_U32 *handle, RK_U32 flags)
{
    int ret;
    struct drm_prime_handle dph;

    dph.fd = map_fd;
    dph.flags = flags;

    ret = drm_ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &dph);
    if (ret < 0) {
        return ret;
    }

    *handle = dph.handle;
    drm_dbg(DRM_FUNCTION, "get handle %d", *handle);

    return ret;
}

static int drm_map(int fd, RK_U32 handle, size_t length, int prot,
                   int flags, unsigned char **ptr, int *map_fd)
{
    int ret;
    struct drm_mode_map_dumb dmmd;
    memset(&dmmd, 0, sizeof(dmmd));
    dmmd.handle = handle;

    if (map_fd == NULL)
        return -EINVAL;
    if (ptr == NULL)
        return -EINVAL;

    ret = drm_handle_to_fd(fd, handle, map_fd, 0);
    drm_dbg(DRM_FUNCTION, "drm_map fd %d\n", *map_fd);
    if (ret < 0)
        return ret;

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dmmd);
    if (ret < 0) {
        close(*map_fd);
        drm_dbg(DRM_FUNCTION, "dev fd %d length %d failed\n", fd, length);
        return ret;
    }

    drm_dbg(DRM_FUNCTION, "dev fd %d length %d\n", fd, length);

    *ptr = (unsigned char*)drm_mmap(fd, length, prot, flags, dmmd.offset);
    if (*ptr == MAP_FAILED) {
        close(*map_fd);
        *map_fd = -1;
        drm_err("mmap failed: %s\n", strerror(errno));
        return -errno;
    }

    return ret;
}

static int drm_alloc(int fd, size_t len, size_t align, RK_U32 *handle)
{
    int ret;
    struct drm_mode_create_dumb dmcb;

    memset(&dmcb, 0, sizeof(struct drm_mode_create_dumb));
    dmcb.bpp = 8;
    dmcb.width = (len + align - 1) & (~(align - 1));
    dmcb.height = 1;
    dmcb.size = dmcb.width * dmcb.bpp;

    drm_dbg(DRM_FUNCTION, "fd %d aligned %d size %lld\n", fd, align, dmcb.size);

    if (handle == NULL)
        return -EINVAL;

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dmcb);
    if (ret < 0)
        return ret;
    *handle = dmcb.handle;

    drm_dbg(DRM_FUNCTION, "get handle %d size %d\n", *handle, dmcb.size);

    return ret;
}

static int drm_free(int fd, RK_U32 handle)
{
    struct drm_mode_destroy_dumb data = {
        .handle = handle,
    };
    return drm_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &data);
}

static int os_allocator_drm_open(void **ctx, size_t alignment)
{
    RK_S32 fd;
    allocator_ctx_drm *p;

    drm_dbg(DRM_FUNCTION, "enter\n");

    if (NULL == ctx) {
        drm_err("os_allocator_open Android do not accept NULL input\n");
        return DRM_ERR_NULL_PTR;
    }

    *ctx = NULL;

    fd = open(dev_drm, O_RDWR);
    if (fd < 0) {
        drm_err("open %s failed!\n", dev_drm);
        return DRM_ERR_UNKNOW;
    }

    drm_dbg(DRM_DEVICE, "open drm dev fd %d\n", fd);

    p = (allocator_ctx_drm *)malloc(sizeof(allocator_ctx_drm));
    if (NULL == p) {
        close(fd);
        drm_err("os_allocator_open Android failed to allocate context\n");
        return DRM_ERR_MALLOC;
    } else {
        /*
         * default drm use cma, do nothing here
         */
        p->alignment    = alignment;
        p->drm_device   = fd;
        *ctx = p;
    }

    drm_dbg(DRM_FUNCTION, "leave\n");

    return 0;
}

static int os_allocator_drm_alloc(void *ctx, DrmBufferInfo *info)
{
    int ret = 0;
    allocator_ctx_drm *p = NULL;

    if (NULL == ctx) {
        drm_err("os_allocator_close Android do not accept NULL input\n");
        return DRM_ERR_NULL_PTR;
    }

    p = (allocator_ctx_drm *)ctx;
    drm_dbg(DRM_FUNCTION, "alignment %d size %d\n", p->alignment, info->size);
    ret = drm_alloc(p->drm_device, info->size, p->alignment,
                    (RK_U32 *)&info->hnd);
    if (ret) {
        drm_err("os_allocator_drm_alloc drm_alloc failed ret %d\n", ret);
        return ret;
    }
    drm_dbg(DRM_FUNCTION, "handle %d\n", (RK_U32)((intptr_t)info->hnd));
    ret = drm_map(p->drm_device, (RK_U32)((intptr_t)info->hnd), info->size,
                  PROT_READ | PROT_WRITE, MAP_SHARED,
                  (unsigned char **)&info->ptr, &info->fd);
    if (ret) {
        drm_err("os_allocator_drm_alloc drm_map failed ret %d\n", ret);
        return ret;
    }
    return ret;
}

static int os_allocator_drm_import(void *ctx, DrmBufferInfo *data)
{
    int ret = 0;
    allocator_ctx_drm *p = (allocator_ctx_drm *)ctx;
    struct drm_mode_map_dumb dmmd;
    memset(&dmmd, 0, sizeof(dmmd));

    drm_dbg(DRM_FUNCTION, "enter");
    // NOTE: do not use the original buffer fd,
    //       use dup fd to avoid unexpected external fd close
    data->fd = dup(data->fd);

    ret = drm_fd_to_handle(p->drm_device, data->fd, (RK_U32 *)&data->hnd, 0);

    drm_dbg(DRM_FUNCTION, "get handle %d", (RK_U32)(data->hnd));

    dmmd.handle = (RK_U32)(data->hnd);

    ret = drm_ioctl(p->drm_device, DRM_IOCTL_MODE_MAP_DUMB, &dmmd);
    if (ret < 0)
        return ret;

    drm_dbg(DRM_FUNCTION, "dev fd %d length %d", p->drm_device, data->size);

    data->ptr = drm_mmap(p->drm_device, data->size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, dmmd.offset);
    if (data->ptr == MAP_FAILED) {
        drm_err("mmap failed: %s\n", strerror(errno));
        return -errno;
    }

    drm_dbg(DRM_FUNCTION, "leave");

    return ret;
}

static int os_allocator_drm_release(void *ctx, DrmBufferInfo *data)
{
    allocator_ctx_drm *p = NULL;

    if (NULL == ctx) {
        drm_dbg(DRM_FUNCTION, "invalid ctx");
        return DRM_ERR_NULL_PTR;
    }
    p = (allocator_ctx_drm *)ctx;

    if (data->ptr) {
        munmap(data->ptr, data->size);
        data->ptr = NULL;
    }

    drm_free(p->drm_device, (RK_U32)((intptr_t)data->hnd));

    return 0;
}

static int os_allocator_drm_free(void *ctx, DrmBufferInfo *data)
{
    allocator_ctx_drm *p = NULL;

    if (NULL == ctx) {
        drm_dbg(DRM_FUNCTION, "invalid ctx");
        return DRM_ERR_NULL_PTR;
    }
    p = (allocator_ctx_drm *)ctx;

    if (data->ptr) {
        munmap(data->ptr, data->size);
        data->ptr = NULL;
    }

    drm_free(p->drm_device, (RK_U32)((intptr_t)data->hnd));
    /* Not necessary here */
    close(data->fd);

    return 0;
}

static int os_allocator_drm_close(void *ctx)
{
    int ret;
    allocator_ctx_drm *p;

    if (NULL == ctx) {
        drm_err("os_allocator_close Android do not accept NULL input\n");
        return DRM_ERR_NULL_PTR;
    }

    p = (allocator_ctx_drm *)ctx;
    drm_dbg(DRM_FUNCTION, "close fd %d", p->drm_device);
    ret = close(p->drm_device);
    free(p);
    if (ret < 0)
        return (int) - errno;
    return 0;
}

DrmCameraBuffer::DrmCameraBuffer(
    void *handle,
    int sharefd,
    unsigned long phy,
    void* vaddr,
    const char* camPixFmt,
    unsigned int width,
    unsigned int height,
    int stride,
    size_t size,
    weak_ptr<CameraBufferAllocator> allocator,
    weak_ptr<ICameraBufferOwener> bufOwener):
  CameraBuffer(allocator, bufOwener), mHandle(handle), mShareFd(sharefd),
  mPhy(phy), mVaddr(vaddr), mWidth(width), mHeight(height), mBufferSize(size),
  mCamPixFmt(camPixFmt), mStride(stride) {
  //ALOGV("%s: sharefd %d, vaddr %p, camPixFmt %s, stride %d", __func__, sharefd, vaddr, camPixFmt, stride);

}

DrmCameraBuffer::~DrmCameraBuffer(void) {}

void* DrmCameraBuffer::getHandle(void) const {
  return (void*)&mHandle;
}
void* DrmCameraBuffer::getPhyAddr(void) const {
  return (void*)mPhy;
}

void* DrmCameraBuffer::getVirtAddr(void) const {
  return mVaddr;
}

const char* DrmCameraBuffer::getFormat(void) {
  return mCamPixFmt;
}

unsigned int DrmCameraBuffer::getWidth(void) {
  return mWidth;
}

unsigned int DrmCameraBuffer::getHeight(void) {
  return mHeight;
}

size_t DrmCameraBuffer::getDataSize(void) const {
  return mDataSize;
}

void DrmCameraBuffer::setDataSize(size_t size) {
  mDataSize = size;
}

size_t DrmCameraBuffer::getCapacity(void) const {
  return mBufferSize;
}

unsigned int DrmCameraBuffer::getStride(void) const {
  return mStride;
}

bool DrmCameraBuffer::lock(unsigned int usage) {
  UNUSED_PARAM(usage);
  //ALOGV("%s", __func__);
  return true;
}

bool DrmCameraBuffer::unlock(unsigned int usage) {
  UNUSED_PARAM(usage);
  //ALOGV("%s", __func__);

  return true;
}

DrmCameraBufferAllocator::DrmCameraBufferAllocator(void) {
  //ALOGD("%s", __func__);

  os_allocator_drm_open((void **)&allocator_ctx, 64);
}

DrmCameraBufferAllocator::~DrmCameraBufferAllocator(void) {
  //ALOGD("%s", __func__);
  if (allocator_ctx)
	os_allocator_drm_close(allocator_ctx);
  if (mNumBuffersAllocated > 0) {
    ALOGE("%s: memory leak; %d camera buffers have not been freed", __func__, mNumBuffersAllocated);
  }
}

shared_ptr<CameraBuffer> DrmCameraBufferAllocator::alloc(
    const char* camPixFmt,
    unsigned int width,
    unsigned int height,
    unsigned int usage,
    weak_ptr<ICameraBufferOwener> bufOwener) {
  int ret;
  int stride;
  size_t buffer_size;
  shared_ptr<DrmCameraBuffer> camBuff;
  unsigned long phy = 0;
  DrmBufferInfo info;

  buffer_size = calcBufferSize(camPixFmt, (width + 0xf) & ~0xf, (height + 0xf) & ~0xf);
  stride = width;

  info.size = buffer_size;
  ret = os_allocator_drm_alloc(allocator_ctx, &info);
  if (ret != 0) {
    ALOGE("%s: drm buffer allocation failed (error %d)", __func__, ret);
    goto alloc_end;
  }

  //ion_get_phys(mIonClient, buffHandle, &phy);

  ALOGD("alloc: handle %d, shared_fd %d, vaddr %p, size %d\n",info.hnd, info.fd, info.ptr, buffer_size);
  camBuff = shared_ptr<DrmCameraBuffer> (new DrmCameraBuffer(info.hnd, info.fd, phy, info.ptr, camPixFmt, width, height, stride,
                                                             buffer_size, shared_from_this(), bufOwener));
  if (!camBuff.get()) {
    ALOGE("%s: Out of memory", __func__);
  } else {
    mNumBuffersAllocated++;
    if (camBuff->error()) {
    }
  }
alloc_end:
  return camBuff;
}

void DrmCameraBufferAllocator::free(CameraBuffer* buffer) {
  int ret;

  if (buffer) {
    DrmBufferInfo info;
    DrmCameraBuffer* camBuff = static_cast<DrmCameraBuffer*>(buffer);
    //ALOGD("free: index %d, handle %d, shared_fd %d, vaddr %p, size %d",
    //buffer->getIndex(), camBuff->mHandle, camBuff->mShareFd, camBuff->mVaddr, camBuff->mBufferSize);
	info.fd = camBuff->mShareFd;
	info.hnd = camBuff->mHandle;
	info.ptr = camBuff->mVaddr;
	info.size = camBuff->mBufferSize;
	ret = os_allocator_drm_free(allocator_ctx, &info);
	if (ret != 0) {
	  ALOGE("%s: drm free buffer failed (error %d)", __func__, ret);
    }
    mNumBuffersAllocated--;
  }
}
