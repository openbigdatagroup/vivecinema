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
 * @file	BLVector2.h
 * @desc    x, y in 2D space
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_VECTOR2_H
#define BL_VECTOR2_H

#include "BLMath.h"

namespace mlabs { namespace balai { namespace math {

class Vector2
{
public:
	float x, y;

	Vector2() {}  // uninitialized
	Vector2(float x_, float y_):x(x_),y(y_) {}

	// zero
	void Zero() { x = y = 0.0f; }

	// comparison
	bool IsZero() const {
        return EqualsZero(x) && EqualsZero(y);
    }

    // comparison
	bool Equals(Vector2 const& rhs) const {
		return math::Equals(x, rhs.x) && math::Equals(y, rhs.y);
	}
	bool operator==(Vector2 const& rhs) const {
		return (x==rhs.x) && (y==rhs.y);
	}
	bool operator!=(Vector2 const& rhs) const {
		return (x!=rhs.x) || (y!=rhs.y);
	}

    // arithmetic updates
	Vector2& operator+=(Vector2 const& rhs) {
		x+=rhs.x; y+=rhs.y;
		return *this;
	}
	Vector2& operator-=(Vector2 const& rhs) {
		x-=rhs.x; y-=rhs.y;
		return *this;
	}
	Vector2& operator*=(Vector2 const& rhs) {
		x*=rhs.x; y*=rhs.y;
		return *this;
	}
	Vector2& operator*=(float s) {
		x*=s; y*=s;
		return *this;
	}
//	Vector2& operator/=(float s) {
//		BL_ASSERT2(!EqualsZero(s), "[Vector2] divided by zero");
//		s = 1.0f/s; // NAN?
//		x*=s; y*=s;
//		return *this;
//	}

	Vector2 const Abs() const {
		return Vector2((x>0.0f ? x:-x), (y>0.0f ? y:-y));
	}

    // vector operations
	float Norm() const { return Sqrt(x*x + y*y); }
	float NormSq() const { return (x*x + y*y); }
	float Dot(Vector2 const& a) const { return (x*a.x + y*a.y); }
	Vector2 const& Normalize(float* length = 0) {
		float s = (x*x + y*y);
        if (EqualsOne(s)) {
			if (length)
			    *length = 1.0f;
	    }
	    else if (s<constants::float_minimum) {
		    if (length)
			    *length = 0.0f;
		    x = y = 0.0f;
	    }
		else {
	        s = Sqrt(s);
	        if (length)
		        *length = s;

            s = 1.0f/s;
            x *= s; y *= s;
		}
        return *this;
	}

    // returns (-y,x)
	Vector2 const Perp() const { return Vector2(-y, x); }

    // Compute the barycentric coordinates of the point with respect to the
    // triangle <V0,V1,V2>, P = b0*V0 + b1*V1 + b2*V2, where b0 + b1 + b2 = 1.
    bool GetBarycentrics(Vector2 const& a, Vector2 const& b, Vector2 const& c, 
						 float& rfBaryV0, float& rfBaryV1) const;

    // Gram-Schmidt orthonormalization.  Take linearly independent vectors U
    // and V and compute an orthonormal set (unit length, mutually
    // perpendicular).
    void Orthonormalize(Vector2& U, Vector2& V);
};

//----------------------------------------------------------------------------
inline Vector2 const operator+(Vector2 const& a, Vector2 const& b) { 
	return Vector2(a.x+b.x, a.y+b.y); 
}
inline Vector2 const operator-(Vector2 const& a, Vector2 const& b) {
	return Vector2(a.x-b.x, a.y-b.y); 
}
inline Vector2 const operator*(Vector2 const& a, float s) {
	return Vector2(s*a.x, s*a.y);
}
inline Vector2 const operator*(float s, Vector2 const& a) { 
	return Vector2(s*a.x, s*a.y);
}
inline Vector2 const operator-(Vector2 const& a) { // negate
	return Vector2(-a.x, -a.y);
}
//----------------------------------------------------------------------------
inline bool Vector2::GetBarycentrics(Vector2 const& P0, Vector2 const& P1, Vector2 const& P2, 
							         float& s, float& t) const
{
    // compute the area of tri
    Vector2 a = P1 - P0;
	Vector2 b = P2 - P0;
	float fInvTotalArea2X = FAbs(a.x*b.y - a.y*b.x);
	if (EqualsZero(fInvTotalArea2X))
		return false;
	
	fInvTotalArea2X = 1.0f/fInvTotalArea2X;
	
	a.x = P1.x - x; a.y = P1.y - y;
	b.x = P2.x - x; b.y = P2.y - y;
	s = (FAbs(a.x*b.y - a.y*b.x))*fInvTotalArea2X;

	a.x = P0.x - x; a.y = P0.y - y;
//  b.x = P2.x - x; b.y = P2.y - y;
	t = (FAbs(a.x*b.y - a.y*b.x))*fInvTotalArea2X;
    return true;
}
//----------------------------------------------------------------------------
inline void Vector2::Orthonormalize(Vector2& U, Vector2& V)
{
    // If the input vectors are v0 and v1, then the Gram-Schmidt
    // orthonormalization produces vectors u0 and u1 as follows,
    //
    //   u0 = v0/|v0|
    //   u1 = (v1-(u0*v1)u0)/|v1-(u0*v1)u0|
    //
    // where |A| indicates length of vector A and A*B indicates dot
    // product of vectors A and B.

    // compute u0
    U.Normalize();

    // compute u1
    V -= U*(U.Dot(V));
    V.Normalize();
}

//--- alias ------------------------------------------------------------------
typedef Vector2 Vector2f;

}}}

#endif