#ifndef _DRM_CAMERA_BUFFER_H
#define _DRM_CAMERA_BUFFER_H

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>

#include "CameraBuffer.h"

typedef unsigned int            RK_U32;
typedef signed int              RK_S32;

typedef struct {
    RK_U32  alignment;
    RK_S32  drm_device;
} allocator_ctx_drm;

class DrmCameraBuffer : public CameraBuffer {
  friend class DrmCameraBufferAllocator;
 public:
  virtual void* getHandle(void) const;
  virtual void* getPhyAddr(void) const;

  virtual void* getVirtAddr(void) const;

  virtual const char* getFormat(void);

  virtual unsigned int getWidth(void);

  virtual unsigned int getHeight(void);

  virtual size_t getDataSize(void) const;

  virtual void setDataSize(size_t size);

  virtual size_t getCapacity(void) const;

  virtual unsigned int getStride(void) const;

  virtual bool lock(unsigned int usage = CameraBuffer::READ);

  virtual bool unlock(unsigned int usage = CameraBuffer::READ);

  virtual ~DrmCameraBuffer();

  virtual int getFd() { return mShareFd;}

 protected:
  DrmCameraBuffer(
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
      weak_ptr<ICameraBufferOwener> bufOwener);

  void *mHandle;
  int mShareFd;
  void* mVaddr;
  unsigned int mWidth;
  unsigned int mHeight;
  size_t mBufferSize;
  const char* mCamPixFmt;
  unsigned int mStride;
  size_t mDataSize;
  unsigned long mPhy;
};

class DrmCameraBufferAllocator : public CameraBufferAllocator {
  friend class DrmCameraBuffer;
 public:
  DrmCameraBufferAllocator(void);

  ~DrmCameraBufferAllocator(void);

  virtual shared_ptr<CameraBuffer> alloc(
      const char* camPixFmt,
      unsigned int width,
      unsigned int height,
      unsigned int usage,
      weak_ptr<ICameraBufferOwener> bufOwener);

 private:
  virtual void free(CameraBuffer* buffer);

  allocator_ctx_drm *allocator_ctx;
};

#endif
