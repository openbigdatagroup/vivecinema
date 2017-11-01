/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2015 - present HTC Corporation
 * All Rights Reserved.
 *
 * The information contained in this work is the exclusive property of HTC Corporation
 * ("HTC").  Only the user who is legally authorized by HTC ("Authorized User") has
 * right to employ this work within the scope of this statement.  Nevertheless, the
 * Authorized User shall not use this work for any purpose other than the purpose
 * agreed by HTC.  Any and all addition or modification to this work shall be
 * unconditionally granted back to HTC and such addition or modification shall be
 * solely owned by HTC.  No right is granted under this statement, including but not
 * limited to, distribution, reproduction, and transmission, except as otherwise
 * provided in this statement.  Any other usage of this work shall be subject to the
 * further written consent of HTC.
 *
 * @file	BLVR.h
 * @author	andre chen
 * @history	2015/10/30 created
 *
 */
#ifndef BL_VR_BASE_H
#define BL_VR_BASE_H

#include "BLMatrix4.h"
#include "BLRenderSurface.h"
#include <functional>

namespace mlabs { namespace balai { namespace VR {

enum HMD_EYE {
    HMD_EYE_NEUTRAL = 0, // use this to retrieve hmd tracking pose
    HMD_EYE_LEFT    = 1,
    HMD_EYE_RIGHT   = 2
};

// tracked device type, most likely controller
enum TRACKED_DEVICE_TYPE {
    TRACKED_DEVICE_TYPE_CONTROLLER = 0,

    TRACKED_DEVICE_TYPE_TOTALS,
};

enum TRACKED_DEVICE_COMMAND {
    TRACKED_DEVICE_COMMAND_RUMBLE = 0, // HapticPulse

    TRACKED_DEVICE_COMMAND_TOTALS,
};

// max tracked devices support (note : OpenVR vr::k_unMaxTrackedDeviceCount = 16)
enum { MAX_TRACKED_DEVICES = 16 };

// controller button status
struct ControllerState {
    // x, y range from [-1.0, +1.0]
    struct Axis {
        float x, y;
        Axis():x(0.0f),y(0.0f) {}
        Axis& operator-=(Axis const& from) {
            x -= from.x;
            y -= from.y;
            return *this;
        }
    };

    Axis  Touchpad; // valid only if OnTouchpad = true
    float Trigger;
    uint8 Menu;
    uint8 Grip;
    uint8 OnTouchpad; // 0:release 1:touch 2:pressed
    uint8 Reserved;
    ControllerState():
        Touchpad(),
        Trigger(0.0f),
        Menu(0),
        Grip(0),
        OnTouchpad(0),
        Reserved(0) {}
    operator bool() const {
        return Trigger>0.0f || Menu || Grip || OnTouchpad;
    }
    void Reset() {
        Touchpad.x = Touchpad.y = 0.0f;
        Trigger = 0.0f;
        Menu = Grip = OnTouchpad = 0;
    }
};

// tracking device, positioning device or controller.
class TrackedDevice
{
    BL_NO_COPY_ALLOW(TrackedDevice);

protected:
    math::Matrix3 ltm_;
    TRACKED_DEVICE_TYPE type_;

    // controller status
    ControllerState cur_state_, prev_state_;

    // touchpad swipe control
    ControllerState::Axis touchpad_start_;
    mutable ControllerState::Axis touchpad_pinpt_;
    mutable int touchpad_frames_; // instead of call system::GetTime(), use simple counting.

    uint8 touchpad_release_;  // touchpad release count. when touchpad is constantly
                              // touching, a release (followed by a touch) is not a real release.
                              // this is openVR idiosyncrasy, move to openVR device subclass
                              // is reasonable.

    bool isActive_;     // is connected
    bool isTracked_;    // not lost tracking
    bool stateChanged_; // key botton changed

    TrackedDevice():ltm_(math::Matrix3::Identity),type_(TRACKED_DEVICE_TYPE_CONTROLLER),
        cur_state_(),prev_state_(),touchpad_start_(),touchpad_pinpt_(),touchpad_frames_(0),
        touchpad_release_(0),isActive_(false),isTracked_(false),stateChanged_(false) {
    }

public:
    virtual ~TrackedDevice() {}

    // ltm
    math::Matrix3 const& GetPose() const { return ltm_; }
    void SetPose(math::Matrix3 const& m) { ltm_ = m; }

    // pointer to
    bool GetPointer(math::Vector3& from, math::Vector3& dir) const {
        from = ltm_.Origin(); dir = ltm_.YAxis();
        return isTracked_;
    }

    // type
    void SetType(TRACKED_DEVICE_TYPE t) { type_ = t; }
    TRACKED_DEVICE_TYPE Type() const { return type_; }

    // key/button
    bool GetControllerState(ControllerState& current, ControllerState* prev=NULL) const {
        current = cur_state_;
        if (prev) *prev = prev_state_;
        return stateChanged_;
    }

    // is connected by system(power is on)
    void SetActive(bool c) {
        isActive_ = c;
        if (!isActive_) {
            isTracked_ = false;
            cur_state_.Reset();
            prev_state_.Reset();
            stateChanged_ = false;
            touchpad_release_ = 0;
        }
    }
    bool IsActive() const { return isActive_; }

    // is tracked or lost tracking
    void SetTracked(bool t) { isTracked_ = t; }
    bool IsTracked() const { return isTracked_; }
    bool IsKeyOn() const { return cur_state_; }
    bool GetAttention() const { return isTracked_ && cur_state_; }

    // is button status just change?
    bool IsStateChange() const { return stateChanged_; }

    // touchpad swiping
    int GetTouchpadMove(ControllerState::Axis& from, ControllerState::Axis& to) const {
        if (cur_state_.OnTouchpad && touchpad_frames_>0) {
            from = touchpad_pinpt_;
            to = cur_state_.Touchpad; 
            return touchpad_frames_;
        }
        return 0;
    }
    bool ResetTouchpadMove() const {
        if (cur_state_.OnTouchpad && touchpad_frames_>1) {
            touchpad_pinpt_ = prev_state_.Touchpad;
            touchpad_frames_ = 1;
            return true;
        }
        return false;
    }

    // event - default = rumble 1 millisecond
    virtual bool SetCommand(TRACKED_DEVICE_COMMAND cmd=TRACKED_DEVICE_COMMAND_RUMBLE, uint32 param1=0, uint32 param2=1000) const = 0;

    // show self
    virtual bool DrawSelf() const = 0;
};

enum VR_INTERRUPT {
    VR_INTERRUPT_PAUSE,
    VR_INTERRUPT_RESUME,
    VR_QUIT
};

// the VR manager(singlon)
class Manager
{
protected:
    mutable system::Mutex mutex_;
    math::Matrix4 matProjLeft_;
    math::Matrix4 matProjRight_;
    math::Matrix3 matHMDTrackingPose_;
    math::Matrix3 matLeftEyePoseOffset_;
    math::Matrix3 matRightEyePoseOffset_;

    // interrupt handler
    std::function<void(VR_INTERRUPT, void*)> interrupt_handler_;

    // RenderSurface size(for single left or right eye)
    int   renderWidth_;
    int   renderHeight_;
    float nearClipPlane_;
    float farClipPlane_;

    bool  hmdConnected_;
    bool  hmdTracked_; // matHMDTrackingPose_ is good

    Manager():
        mutex_(),
        matProjLeft_(math::Matrix4::Identity),
        matProjRight_(math::Matrix4::Identity),
        matHMDTrackingPose_(math::Matrix3::Identity),
        matLeftEyePoseOffset_(math::Matrix3::Identity),
        matRightEyePoseOffset_(math::Matrix3::Identity),
        interrupt_handler_(nullptr),
        renderWidth_(0),renderHeight_(0),
        nearClipPlane_(0.1f),farClipPlane_(100.0f),
        hmdConnected_(false),hmdTracked_(false) { // near and far clipping distance
    }
    ~Manager() {} // since not public, virtual is not necessary

public:
    virtual TrackedDevice const* GetTrackedDevice(int i,
                      TRACKED_DEVICE_TYPE type=TRACKED_DEVICE_TYPE_CONTROLLER) const = 0;

    bool GetRenderSurfaceSize(int& width, int& height) const {
        width = renderWidth_;
        height = renderHeight_;
        return width>0 && height>0;
    }

    std::function<void(VR_INTERRUPT, void*)>& InterruptHandler() {
        return interrupt_handler_;
    }

    // get HMD's pose. local transformation matrix
    bool GetHMDPose(math::Matrix3& pose, HMD_EYE eye=HMD_EYE_NEUTRAL) const {
        BL_MUTEX_LOCK(mutex_);
        if (HMD_EYE_LEFT==eye) {
            pose = matHMDTrackingPose_*matLeftEyePoseOffset_;
        }
        else if (HMD_EYE_RIGHT==eye) {
            pose = matHMDTrackingPose_*matRightEyePoseOffset_;
        }
        else {
            pose = matHMDTrackingPose_;
        }
        return hmdConnected_ && hmdTracked_;
    }

    // are we good?
    bool IsHMDLostTracking() const { return !hmdConnected_ || !hmdTracked_; }

    // view matrix. can be directly used by renderer
    math::Matrix3 const GetViewMatrix(HMD_EYE eye) const {
        math::Matrix3 pose;
        GetHMDPose(pose, eye);
        return gfxBuildViewMatrixFromLTM(pose, pose);
    }

    // projection matrix. can be directly used by renderer
    math::Matrix4 const& GetProjectionMatrix(HMD_EYE eye) const {
        return (HMD_EYE_RIGHT==eye) ? matProjRight_:matProjLeft_;
    }

    // horizontal fov in radian. (very roughly)
    virtual float GetHorizontalFOV(HMD_EYE eye=HMD_EYE_NEUTRAL) const {
        float _11; // 2*nearClipPlane_ / (right-left)
        float _13; // (right+left) / (right-left)
        if (HMD_EYE_RIGHT==eye) {
           _11 = matProjRight_._11; 
           _13 = matProjRight_._13; 
        }
        else { // 105.6 degrees
           _11 = matProjLeft_._11;
           _13 = matProjLeft_._13;
        }
        _11 = nearClipPlane_*_13/_11; // 0.5 * (right + left)
        _13 = _11/_13; // 0.5 * (right - left)

        return atan((_13-_11)/nearClipPlane_) + atan((_13+_11)/nearClipPlane_);
    }

    // returns the number of elapsed seconds since the last recorded vsync event. This 
    virtual float GetTimeSinceLastVsync(uint64& frameCounter) const = 0;
    virtual float GetFrameTimeRemaining() const = 0;

    // set new clipping planes, only call it when needed.
    virtual bool SetClippingPlanes(float zNear, float zFar) = 0;

    // initialize graphics objects
    virtual bool Initialize() = 0;

    // this may stall, call in Application::Render()
    virtual bool UpdatePoses() = 0;

    // present as early as it can, this may stall.
    virtual bool Present(HMD_EYE eye, // must be HMD_EYE_LEFT or HMD_EYE_RIGHT
                         graphics::RenderSurface* surface) = 0;

    // clean up
    virtual void Finalize() = 0;

    // singlton to be implemented
    static Manager& GetInstance();
};

}}}

#endif