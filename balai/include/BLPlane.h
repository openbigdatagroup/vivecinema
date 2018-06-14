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
 * @file	BLPlane.h
 * @desc    3D plane math
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_PLANE_H
#define BL_PLANE_H

#include "BLVector3.h"

namespace mlabs { namespace balai { namespace math {

class Matrix3;
class Matrix4;
class Line3;

enum PLANE_SIDE {
	PLANE_RIGHT_ON		= 0,
	PLANE_FRONT_SIDE	= 1,
	PLANE_BACK_SIDE		= 2,
	PLANE_POSITIVE_SIDE = PLANE_FRONT_SIDE,
	PLANE_NEGATIVE_SIDE = PLANE_BACK_SIDE,
	PLANE_NO_RESULT		= 3
};

// frustum planes
enum {
	FRUSTUM_LEFT_PLANE   = 0,
	FRUSTUM_RIGHT_PLANE  = 1,
	FRUSTUM_BOTTOM_PLANE = 2,
	FRUSTUM_TOP_PLANE	 = 3,
	FRUSTUM_NEAR_PLANE   = 4,
	FRUSTUM_FAR_PLANE	 = 5,
	FRUSTUM_TOTAL_PLANES = 6,

	INIT_FRUSTUM_PLANE_STATE = 0x0000003f,
};

class Plane
{
	float a_, b_, c_, d_;
	Plane(float a, float b, float c, float d):a_(a),b_(b),c_(c),d_(d) {}

public:	
	Plane():a_(0.0f),b_(0.0f),c_(1.0f),d_(0.0f) {}

	// construct plane from a normal and a point
	Plane(Vector3 normal, Vector3 const& p0) { 
		normal.Normalize();
		a_ =  normal.x;
		b_ =  normal.y;
		c_ =  normal.z;
		d_ = -normal.Dot(p0);
	}

	Plane(Vector3 const& normal, float dist) { 
		BL_ASSERT(EqualsOne(normal.NormSq()));
		a_ = normal.x;
		b_ = normal.y;
		c_ = normal.z;
		d_ = dist;
	}

	// Normalize - if a plane fail to normalize, it is invalid
	bool Normalize() {
		float s = a_*a_ + b_*b_ + c_*c_;
		if (s<constants::float_epsilon)
			return false;
	
		if (!EqualsOne(s)) {
			s = 1.0f/Sqrt(s);
			a_ *= s; b_ *= s; c_ *= s; d_ *= s;
		}
		return true;
	}

	// flip - the other side
	Plane const& Flip() { a_=-a_; b_=-b_; c_=-c_; d_=-d_; return *this; }
	
	// Compute d = Dot(N,Q)+c where N is the plane normal and c is the plane
    // constant.  This is a signed distance.  The sign of the return value is
    // positive if the point is on the positive side of the plane, negative if
    // the point is on the negative side, and zero if the point is on the
    // plane.
	float Distance(Vector3 const& p) const { return (a_*p.x + b_*p.y + c_*p.z + d_); }
	float Distance(float x, float y, float z) const { return (a_*x+b_*y+c_*z+d_); }

	// which side? - this favor right-on case!
	PLANE_SIDE WhichSide(Vector3 const& p) const {
		float const dist = a_*p.x + b_*p.y + c_*p.z + d_;
		if (dist>constants::float_zero_sup)
			return PLANE_FRONT_SIDE;
		if (dist<constants::float_zero_inf)
			return PLANE_BACK_SIDE;
		return PLANE_RIGHT_ON;
	}

	// build reflection matrix
	Matrix3 const& BuildReflectionMatrix(Matrix3& mat) const;
	Matrix3 const BuildReflectionMatrix() const;
	
	// intersetion test, return distance of the closest point
	float Intersect(Vector3& closestPt, Line3 const& line) const;

	// friend
	friend void gfxBuildFrustumPlanes(Plane plns[FRUSTUM_TOTAL_PLANES], Matrix4 const& mtx4ViewProj);
};
BL_COMPILE_ASSERT(16==sizeof(Plane), ML_plane_size_not_correct);

// build frustum planes, platform dependent, sizeof(plns)/sizeof(Plane) >= 6
void gfxBuildFrustumPlanes(Plane plns[], Matrix4 const& mtx4ViewProj);

}}} // mlabs::balai::math

#endif // BL_PLANE_H