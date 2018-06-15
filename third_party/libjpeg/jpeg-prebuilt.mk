#------------------------------------------------------------------------------
# to use prebuilt static jpeg library                         -andre '14.06.16
# 1) include $(LOCAL_PATH)/../../balai/middlewares/libjpeg/jpeg-prebuilt.mk
# 2) LOCAL_STATIC_LIBRARIES += jpeg
#------------------------------------------------------------------------------
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# module name
LOCAL_MODULE := jpeg

# export include path
LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)

# the lib
LOCAL_SRC_FILES := lib/$(TARGET_ARCH_ABI)/libjpeg.a

include $(PREBUILT_STATIC_LIBRARY)