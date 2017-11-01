/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2012 HTC Corporation
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
 * @file	BLCamera.h
 * @author	andre chen
 * @history	2012/01/09 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_CAMERA_H
#define BL_CAMERA_H

#include "BLObject.h"
#include "BLMatrix4.h" // this includes Matrix3.h
#include "BLTransform.h"

namespace mlabs { namespace balai { 

namespace graphics {

class Camera
{
private:
	mutable math::Matrix4 proj_; // projection matrix(build on demand)
	mutable math::Matrix3 view_; // view matrix(be built by xform_)
	math::Transform       xform_;  // local(world) transformation
	float configs_[4]; // for orthogonal projection { left, right, bottom, top }
					   // for perspective projection { fov, aspect_ratio, 0.0f, 0.0f }
	float nearClip_;   // distance to near clipping plane(meters), must > 0.0
	float farClip_;    // distance to far clipping plane(meters), must > nearClip_

public:
    Camera();
	virtual ~Camera() {}

	// parallel or perspective?
	bool IsParallel() const    { return (configs_[2]!=configs_[3]); }
	bool IsPerspective() const { return (configs_[2]==configs_[3]); }

	float GetFovY() const {
		return IsPerspective() ? configs_[0]:0.0f;
	}

	// really need these?
	float GetNearPlane() const	{ return nearClip_; }
	float GetFarPlane() const	{ return farClip_; }
    bool SetNearFarPlanes(float zNear, float zFar) {
        if (0.0f<zNear && zNear<zFar) {
            nearClip_ = zNear;
            farClip_ = zFar;
            return true;
        }
        return false;
    }

	// Set Parallel projection parameters.
	void SetParallelParam(float left, float top, float right, float bottom, float nearClip, float farClip) {
		BL_ASSERT(0.0f<nearClip);
		BL_ASSERT(nearClip<farClip);
		BL_ASSERT(bottom!=top);
		configs_[0] = left; 
		configs_[1] = right;
		configs_[2] = bottom;
		configs_[3] = top;
		nearClip_   = nearClip;
		farClip_    = farClip;
	}

	// Set Perspective projection parameters.
	void SetPerspectiveParam(float fovY, float nearClip, float farClip) {
		BL_ASSERT(0.0f<nearClip);
		BL_ASSERT(nearClip<farClip);
		configs_[0] = fovY;
		configs_[1] = 0.0f; // reset aspec ratio as framebuffer
		configs_[2] = configs_[3] = 0.0f; // mark as perspective projection
		nearClip_   = nearClip;
		farClip_    = farClip;
	}

	// set aspect ratio(width/height), 0.0f if reset(so it will use framebuffer aspect ratio)
	// (sometime you will like to set aspect=1.0 for cubemap rendering)
	bool SetPerspectiveAspectRatio(float aspect) {
		if (aspect>=0.0f && 0.0f==configs_[2] && configs_[2]==configs_[3]) {
			configs_[1] = aspect;
			return true;
		}
		return false;
	}

	// set orientation
	void SetTransform(math::Transform const& t) {
		math::gfxBuildViewMatrixFromTransform(view_, xform_=t); // math:: isn't necessary thanks to Koenig lookup(ADL)
	}
    void SetLTM(math::Matrix3 const& ltm) {
        xform_.SetMatrix3(ltm);
		math::gfxBuildViewMatrixFromTransform(view_, xform_);
	}
	void SetLookAt(math::Vector3 const& eye, math::Vector3 const& lookAt=math::Vector3(0.0f,0.0f,0.0f), float roll=0.0f) {
		xform_.SetLookAt(eye, lookAt, roll);
		math::gfxBuildViewMatrixFromTransform(view_, xform_);
	}
	
	// view and projection matrix(called by Renderer)
    math::Transform const& GetTransform() const { return xform_; }
	math::Matrix3 const& ViewMatrix() const { return view_; }
	math::Matrix4 const& ProjMatrix(float aspect) const; // building on demand
	math::Matrix4 const ViewProjectMatrix(float aspect) const { return ProjMatrix(aspect)*view_; }

	// Line of sight - in world space
	math::Vector3 const LineOfSight() const { return xform_.YAxis(); }
};

class OrbitCamera : public Camera
{
public:
    OrbitCamera() {}
    virtual ~OrbitCamera() {}

    void Yaw(float angle) {
        math::Transform xform = GetTransform();
        math::Transform tt(math::Quaternion(xform.ZAxis(), angle), math::Vector3(0.0f, 0.0f, 0.0f));
        SetTransform(tt*=xform);
    }
    void Pitch(float angle) {
        math::Transform xform = GetTransform();
        math::Transform tt(math::Quaternion(xform.XAxis(), angle), math::Vector3(0.0f, 0.0f, 0.0f));
        SetTransform(tt*=xform);
    }
    void Roll(float angle) {
        math::Transform xform = GetTransform();
        math::Transform tt(math::Quaternion(xform.YAxis(), angle), math::Vector3(0.0f, 0.0f, 0.0f));
        SetTransform(tt*=xform);
    }
    void Move(float x, float y, float z) {
        math::Transform xform = GetTransform();
        xform.SetOrigin(xform.Origin() + x*xform.XAxis() + y*xform.YAxis() + z*xform.ZAxis());
        SetTransform(xform);
   }
};

// 
// Camera system class to manage camera, pending on game define
// Note : this class is for demostrating only. To show how you should use Camera class,
//        you don't have to implement your own CameraSystem if it sounds stupid :-)
//
class CameraSystem : public IObject, public Camera
{
	BL_DECLARE_RTTI;
	virtual IObject* DoClone_() const = 0;

public:
//	Matrix4 const& ProjMatrix() const { return Camera::proj_; }
	virtual bool FrameMove(float frameTime) = 0;
};


enum STEREO_VIEW {
	STEREO_VIEW_MONO  = 0,	// mono
	STEREO_VIEW_LEFT  = 1,  // stereo left view
	STEREO_VIEW_RIGHT = 2,  // stereo right view
};
class StereoCameraSystem : public CameraSystem
{
	BL_DECLARE_RTTI;
	virtual IObject* DoClone_() const = 0;
	
protected:
	math::Matrix3		view2_;		// another view
	math::Vector3		lookAt_;
	//......
	float		eyeDist_;
	STEREO_VIEW	stereoView_;

public:
//  virtual Matrix3	const& ViewMatrix() const;
//	Matrix4 const& ProjMatrix() const { return Camera::proj_; }
//	virtual bool FrameMove(float frameTime) = 0;
	void SetView(STEREO_VIEW v) { stereoView_=v; }
};

}}}

#endif