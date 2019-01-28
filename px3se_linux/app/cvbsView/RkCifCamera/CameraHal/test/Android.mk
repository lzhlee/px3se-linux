LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES +=\
        test.cpp \

LOCAL_SHARED_LIBRARIES:=

ifeq ($(IS_NEED_SHARED_PTR),true)
LOCAL_CPPFLAGS += -D ANDROID_SHARED_PTR
endif

#should include ../inlclude after system/core/include
LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include\
        $(LOCAL_PATH)/../include\
	$(LOCAL_PATH)/../include/linux \

LOCAL_SHARED_LIBRARIES += libdrm

LOCAL_CFLAGS += -Wno-error=unused-function -Wno-array-bounds
LOCAL_CFLAGS += -DLINUX  -D_FILE_OFFSET_BITS=64 -DHAS_STDINT_H -DENABLE_ASSERT
LOCAL_CPPFLAGS += -std=c++11
LOCAL_CPPFLAGS +=  -DLINUX  -DENABLE_ASSERT 
LOCAL_SHARED_LIBRARIES += \
		libcam_hal \

ifeq ($(IS_NEED_LINK_STLPORT),true)
LOCAL_SHARED_LIBRARIES += \
                libstlport 
endif

LOCAL_STATIC_LIBRARIES := \
	libcamera\
	libiep \

LOCAL_MODULE:= camHalTest.bin

include $(BUILD_EXECUTABLE)
