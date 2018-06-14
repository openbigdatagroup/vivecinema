/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2011 HTC Corporation
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
 * @file    BLTransform.h
 * @desc    uni-scale, rotate and translate transformation
 * @author	andre chen
 * @history	2011/12/29 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_TRANSFORM_H
#define BL_TRANSFORM_H

#include "BLQuaternion.h"

namespace mlabs { namespace balai { namespace math {

//
// Transform(v) = Translate(Rotate(UniScale(v)))
class Transform
{
    Quaternion	rotate_;
    Vector3		origin_; // translate
    float		scale_;  // uni-scale

public:
    // The transformation is Y = Translate(Rotate(Scale(X))).
    // Scale first, then Rotate and Translate at last.
	Transform():rotate_(),origin_(0.0f,0.0f,0.0f),scale_(1.0f) {}
	Transform(Quaternion const& q, Vector3 const& o, float s=1.0f):
	    rotate_(q),origin_(o),scale_(s) {}
	~Transform() {}
	
	// reset to the identity
	void MakeIdentity() {
	    rotate_.MakeIdentity();
		origin_.Zero();
		scale_ = 1.0f;
	}

	// comparison
	bool Equals(Transform const& rhs) const;

	// Hints about the structure of the transform
    bool IsIdentity() const;

	// accessors
    Quaternion const& GetRotate() const { return rotate_; }
	float GetScale() const				{ return scale_;  }
	void SetScale(float s)				{ scale_ = s; }	
	void SetRotate(Quaternion const& q) { rotate_ = q; }
	void SetOrigin(Vector3 const& o)    { origin_ = o; }
	void SetOrigin(float x, float y, float z) {
		origin_.x = x; origin_.y = y; origin_.z = z;
	}

	// orientation & position
	Vector3 const XAxis() const { return rotate_.XAxis(); }
	Vector3 const YAxis() const { return rotate_.YAxis(); }
	Vector3 const ZAxis() const { return rotate_.ZAxis(); }
	Vector3 const& Origin() const { return origin_; }
	
	// rotation control
	void GetRotateAxisAngle(Vector3& axis, float& angle) const {
		rotate_.GetAxisAngle(axis, angle);
	}
	void SetRotateAxisAngle(Vector3 const& axis, float angle) {
		rotate_.SetAxisAngle(axis, angle);
	}
	void SetYawPitchRoll(float yaw, float pitch, float roll) {
		rotate_.SetYawPitchRoll(yaw, pitch, roll);
	}

	// get inverse
	Transform const& GetInverse(Transform& inv) const; // no const due quaternion's GetInverse()

	// Apply Transform
	Vector3 const& PointTransform(Vector3& out, Vector3 const& p) const {
		rotate_.Rotate(out, scale_ * p);
	    return out += origin_;
	}
	Vector3 const PointTransform(Vector3 const& p) const {
		return rotate_.Rotate(scale_ * p) + origin_;
	}
	Vector3 const& VectorTransform(Vector3& out, Vector3 const& v) const {
		return rotate_.Rotate(out, v);
	}
	Vector3 const VectorTransform(Vector3 const& v) const {
		return rotate_.Rotate(v);
	}

	// set look at
	// return false if lookDir=0
	bool SetHeading(Vector3 const& from, Vector3 lookDir, float roll=0.0f);
	bool SetLookAt(Vector3 const& from, Vector3 const& lookAt=Vector3(0.0f,0.0f,0.0f), float roll=0.0f) {
		return SetHeading(from, lookAt-from, roll);
	}

	// convert to matrix3
	Matrix3 const& GetMatrix3(Matrix3& mat3) const;

	// normally, don't do this!
	// Except you're extract matrix data and pack into Transform from max/maya
	// return false if conversion fail(nothing changed in that case!)
	bool SetMatrix3(Matrix3 const& mtx);

	// multiplication assignment
	Transform& operator*=(Transform const&);

	// this = a * b;
	Transform const& SetMul(Transform const& parent, Transform const& child);

	// this = (1.0f-lerp)*a + lerp*b;
	Transform const& SetLerp(Transform const& a, Transform const& b, float lerp);

	// this = base * (alpha * mulitiplier); that is, base((alpha * mulitiplier))
	Transform const& SetAddBlend(Transform const& base, Transform const& mulitiplier, float alpha);

	// vip
	friend class Matrix3;
};

BL_COMPILE_ASSERT(32==sizeof(Transform), Error_Transform_size_not_32);

//- parent * child, that is, (parent * child)(v) = parent(child(v)) ----------------------------------------
inline Transform const operator*(Transform parent, Transform const& child) { return parent*=child; }

}}}

#endif // BL_TRANSFORM_H