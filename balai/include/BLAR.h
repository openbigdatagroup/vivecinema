//
//  MAR.h(Augmented Reality)
//  mantis
//
//  Created by andre 'hl' chen on 12/5/22.
//  Copyright (c) 2012 A&R. All rights reserved.
//
#ifndef BL_AR_BASE_H
#define BL_AR_BASE_H

#include "BLMatrix4.h"
#include "BLString.h"

namespace mlabs { namespace balai { namespace AR {

struct Trackable
{
    math::Matrix3 pose;
    float  sizeX;
    float  sizeY;
    uint32 name;
    uint32 type;
    uint32 uniqueId; // if applicable
    uint32 lostCount; // lost frame count
};

class Manager
{
    enum {
        MAX_TRACKERDATAS = 4,
        MAX_TRACKABLES  = 8,
    };

    struct TrackerData {
        String path;
        void*  impl;
    } trackerData_[MAX_TRACKERDATAS];
    Trackable     trackables_[MAX_TRACKABLES];
    math::Matrix4 matProj_;

    //
    // clip rect(area) is the see-able rect of framebuffer.
    // this is because after QCAR::Renderer::begin() is called, 
    // QCAR manipulates the viewport, typical bigger than framebuffer size.
    // (for iPhone 3GS, framebuffer = 480x320, but video buffer = 480x360.
    // it SetViewport(0, -20, 480, 360). so top area and bottom area 
    // will probably not show. for the iPhone 3GS case clipRect_ = 
    //   { 0(left), (20/360.0)(top), 1.0(right), (360-20)/360.0(bottom) }
    // see QCARView.mm, -(void) configureVideoBackground() for more details
    // (clipRect_ helps to render 2D objects inside ARBeginDraw/AREndDraw block)
    float         clipRect_[4]; // the region { left, top, right, bottom }
    float         unitScale_;
    float         nearClipPlane_;
    float         farClipPlane_;
    uint32        trackerType_;
    uint32        totalTrackerDatas_;
    uint32        totalTrackables_;
    bool          isCameraRunning_;
    bool          cameraContiFocusOn_;
    bool          cameraTorchOn_;

    void CalcMarkerMatrix_(math::Matrix3& mat, float const pose[12]) const;

    Manager();
    ~Manager() {}

public:
    bool RegisterTrackerData(char const* path) {
        if (path && totalTrackerDatas_<MAX_TRACKERDATAS && 0==totalTrackables_) {
            trackerData_[totalTrackerDatas_++].path = path;
            return true;
        }
        return false;
    }
    bool ActiveTrackerData(char const* path);
    bool ActiveTrackerData(uint32 id);

    float NearPlane() const { return nearClipPlane_; }
    float FarPlane() const { return farClipPlane_; }

    uint32 GetTrackerType() const { return trackerType_; }
    void  SetTrackerType(uint32 t) { trackerType_=t; }

    void SetProjectionMatrix(float const matProj[16]);
    math::Matrix4 const& GetProjectionMatrix() const { return matProj_; }

    void SetClipArea(float const rect[4]) { 
        clipRect_[0] = rect[0];
        clipRect_[1] = rect[1];
        clipRect_[2] = rect[2];
        clipRect_[3] = rect[3];
    }
    float const* GetClipArea() const { return clipRect_; }

    bool IsCameraRunning() const { return isCameraRunning_; }
    bool StartCamera();
    void StopCamera();
    void TriggerCameraAutoFocus();
    void TriggerCameraContinuousAutoFocus();

    // Load/Unload
    uint32 LoadTrackerData(); // [may be performed on a background thread]
    void   DestroyTrackerData();

    Trackable const* GetTrackerable(uint32 i) const {
        return (i<totalTrackables_) ? (trackables_+i):NULL;
    }

	// 
	// BeginDraw draw background
    // return <0 if camera is not running(not initialized), trackerable items.
    int BeginDraw();

    // pair with BeginDraw(), always render screen based objects after this.
    void EndDraw();

    // singlton
    static Manager& GetInstance();
};

}}}

#endif