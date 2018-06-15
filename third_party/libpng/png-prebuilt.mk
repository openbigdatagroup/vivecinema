#------------------------------------------------------------------------------
# to use prebuilt static png library                          -andre '14.08.21
# 1) include $(LOCAL_PATH)/../../balai/middlewares/png/png-prebuilt.mk
# 2) LOCAL_STATIC_LIBRARIES += png
#------------------------------------------------------------------------------
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# module name
LOCAL_MODULE := png

# export include path
LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)

# the lib
LOCAL_SRC_FILES := lib/$(TARGET_ARCH_ABI)/libpng.a

# link the dynamic zlib
LOCAL_EXPORT_LDLIBS := -lz

include $(PREBUILT_STATIC_LIBRARY)