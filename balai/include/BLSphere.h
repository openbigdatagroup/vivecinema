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
 * @file	BLSphere.cpp
 * @desc    3D (bounding) sphere
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_SPHERE_H
#define BL_SPHERE_H

#include "BLPlane.h"

namespace mlabs { namespace balai { namespace math {

class Transform;

class Sphere
{
	Vector3 center_;
	float	radius_;

	Sphere(float cx, float cy, float cz, float r):center_(cx,cy,cz),radius_(r) {}

public:
    // The sphere is represented as |X-C| <= R where C is the center and R is
    // the radius. if _R<0.0f, sphere is not valid(not take any space).
	Sphere():center_(0.0f,0.0f,0.0f),radius_(-1.0f) {}
	Sphere(Vector3 const& center, float radius):center_(center),radius_(radius) {}
	~Sphere() {}
    
	// default copy constructor and default assignment operator work fine.

	// addition assignment
	Sphere& operator+=(Sphere const& r);

	void Init(float cx, float cy, float cz, float r) {
		center_.x = cx; 
		center_.y = cy;
		center_.z = cz;
		radius_ = r;
	}
	bool IsEmpty() const { return (radius_<0.0f); }
	Vector3 const& GetCenter() const { return center_; }
	float GetRadius() const	{ return radius_; }
	float Volume() const { return (radius_>0) ? (4.188790f*radius_*radius_*radius_):0.0f; } // (4/3)(Pi)r^3
	void Reset() { center_.Zero(); radius_ = -1.0f; }
	bool Contains(Vector3 const& pt) const {
		return (radius_>0.0f) && (pt-center_).NormSq()<(radius_*radius_);
	}
	bool Overlaps(Sphere const& that) const {
		if (radius_>=0.0f && that.radius_>=0.0f) {
			float t = radius_ + that.radius_;
			return (center_-that.center_).NormSq() < (t*t);
		}
		return false;
	}

	// transform
	Sphere const& SetTransform(Matrix3 const& xform, Sphere const& localSphere);
	Sphere const& SetTransform(Transform const& xform, Sphere const& localSphere);
	Sphere const& SelfTransform(Transform const& xform); // transform itself

	// generate by points
	Sphere const& GenerateByPoints(Vector3 const* pts, uint32 nCnt);

	// collide with a plane
	PLANE_SIDE WhichSide(Plane const& plane) const;
};

//----------------------------------------------------------------------------
inline Sphere const operator+(Sphere a, Sphere const& b) { return (a+=b); }

}}}

#endif // BL_SPHERE3_H