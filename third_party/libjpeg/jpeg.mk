#------------------------------------------------------------------------------
# android make file to build jpeg (static) library            -andre '13.02.19
# to use me :
# 1) build static library(depend on your project directory)
# include $(LOCAL_PATH)/../../balai/middlewares/libjpeg/jpeg.mk
# 
# 2) link(this makes LOCAL_EXPORT_xxx work)
# LOCAL_STATIC_LIBRARIES += jpeg
#------------------------------------------------------------------------------
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# module name
LOCAL_MODULE := jpeg

# options
LOCAL_CFLAGS := -Wall -fno-rtti -fno-exceptions
LOCAL_CFLAGS += -DJPEG_MULTIPLE_SCANLINES

# export include path
LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)

# includes path
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/../../build/include

# compilation units
# jcxxx.cpp for compressing
# jdxxx.cpp for decompressing
LOCAL_SRC_FILES := BLJPEG.cpp \
    src/jaricom.cpp \
    src/jcapimin.cpp \
    src/jcapistd.cpp \
    src/jcarith.cpp \
    src/jccoefct.cpp \
    src/jccolor.cpp \
    src/jcdctmgr.cpp \
    src/jchuff.cpp \
    src/jcinit.cpp \
    src/jcmainct.cpp \
    src/jcmarker.cpp \
    src/jcmaster.cpp \
    src/jcomapi.cpp \
    src/jcparam.cpp \
    src/jcphuff.cpp \
    src/jcprepct.cpp \
    src/jcsample.cpp \
    src/jdapimin.cpp \
    src/jdapistd.cpp \
    src/jdarith.cpp \
    src/jdatadst.cpp \
    src/jdatasrc.cpp \
    src/jdcoefct.cpp \
    src/jdcolor.cpp \
    src/jddctmgr.cpp \
    src/jdhuff.cpp \
    src/jdinput.cpp \
    src/jdmainct.cpp \
    src/jdmarker.cpp \
    src/jdmaster.cpp \
    src/jdmerge.cpp \
    src/jdphuff.cpp \
    src/jdpostct.cpp \
    src/jdsample.cpp \
    src/jerror.cpp \
    src/jfdctflt.cpp \
    src/jfdctfst.cpp \
    src/jfdctint.cpp \
    src/jidctflt.cpp \
    src/jidctfst.cpp \
    src/jidctint.cpp \
    src/jidctred.cpp \
    src/jmemmgr.cpp \
    src/jmemnobs.cpp \
    src/jquant1.cpp \
    src/jquant2.cpp \
    src/jutils.cpp

# ARM Neon support
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
LOCAL_SRC_FILES += src/jsimd_arm_neon.S.arm.neon src/jsimd_arm.cpp
else
LOCAL_SRC_FILES += src/jsimd_none.cpp
endif

# kick off build
include $(BUILD_STATIC_LIBRARY)