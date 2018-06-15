#----------------------------------------------------------------------------------------
# android make file to build png (static) library                        -andre '14.08.21
# to use me :
# 1) build static library(depend on your project directory)
# include $(LOCAL_PATH)/../../balai/middlewares/libpng/png.mk
#
# 2) link(this makes LOCAL_EXPORT_xxx work)
# LOCAL_STATIC_LIBRARIES += png
#
# note that zlib is in ndk(ndk/platforms/android-n/arch-xxxx/usr/include/linux/zlib.h)
# just need to link dynamic zlib
#----------------------------------------------------------------------------------------
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# module name
LOCAL_MODULE := png

# options
LOCAL_CFLAGS := -Wall -fno-rtti -fno-exceptions

#LOCAL_CPP_EXTENSION := .c .cpp

# export include path
LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)

# includes path
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/../../build/include

# compilation units
LOCAL_SRC_FILES := BLPNG.cpp \
    src/png.c \
    src/pngerror.c \
    src/pngget.c \
    src/pngmem.c \
    src/pngpread.c \
    src/pngread.c \
    src/pngrio.c \
    src/pngrtran.c \
    src/pngrutil.c \
    src/pngset.c \
    src/pngtrans.c \
    src/pngwio.c \
    src/pngwrite.c \
    src/pngwtran.c \
    src/pngwutil.c

# link the dynamic zlib
LOCAL_EXPORT_LDLIBS := -lz
    
# kick off build
include $(BUILD_STATIC_LIBRARY)