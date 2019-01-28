
INCLUDEPATH +=$$PWD
INCLUDEPATH +=$$PKG_CONFIG_SYSROOT_DIR/usr/include/libdrm 
INCLUDEPATH +=$$PWD/out/include
HEADERS += \
    $$PWD/drmDsp.h \
    $$PWD/fb.h

SOURCES += \
    $$PWD/fb.c	\
	$$PWD/drmDsp.c  \
    $$PWD/cameraApi.cpp  \

DEFINES += \
	USE_RK_V4L2_HEAD_FILES	\
	SUPPORT_ION	\
	USE_DRM_DISPLAY	\
	__gnu_linux__ \
	LINUX	\
	_FILE_OFFSET_BITS=64	\
	HAS_STDINT_H	\
	ENABLE_ASSERT	\
