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
 * @file	BLVector3.h
 * @desc    x, y & z in 3D space
 * @author	andre chen
 * @history	2011/12/29 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_VECTOR3_H
#define BL_VECTOR3_H

#include "BLMath.h"

namespace mlabs { namespace balai { namespace math {

class Vector3
{
public:
	float x, y, z;

	Vector3() {}  // uninitialized
	Vector3(float xx, float yy, float zz):x(xx),y(yy),z(zz) {}

	// you may need this... andre '10.11.08
	float operator[](int id) const { return (1==id) ? y:(2==id ? z:x); }
	float& operator[](int id) { return (1==id) ? y:(2==id ? z:x); }

	// zero
	void Zero() { x = y = z = 0.0f; }

	// comparison
	bool IsZero() const {
        return EqualsZero(x) && EqualsZero(y) && EqualsZero(z);
    }
	bool operator==(Vector3 const& rhs) const {
        return x==rhs.x && y==rhs.y && z==rhs.z;
	}
	bool operator!=(Vector3 const& rhs) const {
        return x!=rhs.x || y!=rhs.y || z!=rhs.z;
    }
	bool Equals(Vector3 const& rhs) const {
		return math::Equals(x, rhs.x) && math::Equals(y, rhs.y) && math::Equals(z, rhs.z);
    }
	bool Equals(Vector3 const& rhs, float range) const {
        return math::Equals(x, rhs.x, range) && math::Equals(y, rhs.y, range) && math::Equals(z, rhs.z, range);
	}
    
	// arithmetic updates
    Vector3& operator+=(Vector3 const& rhs) {
        x += rhs.x; y += rhs.y; z += rhs.z;
        return *this;
    }
    Vector3& operator-=(Vector3 const& rhs) {
        x -= rhs.x; y -= rhs.y; z -= rhs.z;
        return *this;
    }
	Vector3& operator*=(Vector3 const& rhs) {
        x *= rhs.x; y *= rhs.y; z *= rhs.z;
        return *this;
    }
    Vector3& operator*=(float s) {
	    x *= s; y *= s; z *= s;
	    return *this;
    }
//	Vector3& operator/=(float s) {
//		BL_ASSERT2(!EqualsZero(s), "[Vector3] divided by zero");
//      s = 1.0f/s;
//      x *= s; y *= s; z *= s;
//	    return *this;
//	}

	Vector3 const Abs() const {
		return Vector3((x>0.0f ? x:-x), (y>0.0f ? y:-y), (z>0.0f ? z:-z));
	}

    // vector operations
	float Norm() const    { return Sqrt(x*x + y*y + z*z); }
	float NormSq() const  { return (x*x + y*y + z*z); }
	float CabNorm() const { return FAbs(x) + FAbs(y) + FAbs(z); }
	Vector3 const& Normalize(float* length = 0) {
        float s = x*x + y*y + z*z;
	    if (s<constants::float_minimum) {
		    x = y = 0.0f; z = 1.0f; // watch this!
		    if (length)
				*length = 0.0f;
	    }
	    else if (EqualsOne(s)) {
		    if (length)
				*length = 1.0f;
	    }
		else {
	        s = Sqrt(s);
	        if (length)
		        *length = s;

            s = 1.0f/s;
	        x *= s; y *= s; z *= s;
		}
        return *this;
    }

    // dot & cross product
	float Dot(Vector3 const& rhs) const {
        return (x*rhs.x + y*rhs.y + z*rhs.z);
	}
	Vector3 const Cross(Vector3 const& rhs) const {
	    return Vector3(y*rhs.z-z*rhs.y, z*rhs.x-x*rhs.z, x*rhs.y-y*rhs.x);
	}

	// Compute the barycentric coordinates of the point with respect to the
	// Triangle<V0,V1,V2>, P = b0*V0 + b1*V1 + b2*V2, where b2 = 1 - (b0 + b1)
	bool CalcBarycentrics(Vector3 const& V0, Vector3 const& V1, Vector3 const& V2,
		                  float& b0, float& b1) const;
};

//--- operators ---------------------------------------------------------------
inline Vector3 const operator+(Vector3 const& a, Vector3 const& b) {
	return Vector3(a.x+b.x, a.y+b.y, a.z+b.z); 
}
inline Vector3 const operator-(Vector3 const& a, Vector3 const& b) {
	return Vector3(a.x-b.x, a.y-b.y, a.z-b.z); 
}
inline Vector3 const operator*(Vector3 const& a, Vector3 const& b) {
	return Vector3(a.x*b.x, a.y*b.y, a.z*b.z); 
}
inline Vector3 const operator*(float s, Vector3 const& a) {
	return Vector3(s*a.x, s*a.y, s*a.z); 
}
inline Vector3 const operator*(Vector3 const& a, float s) {
	return Vector3(s*a.x, s*a.y, s*a.z); 
}
inline Vector3 const operator-(Vector3 const& a) { // negate
	return Vector3(-a.x, -a.y, -a.z); 
}
//--- comparsion --------------------------------------------------------------
inline bool operator<(Vector3 const& a, Vector3 const& b) {
	return a.x<b.x && a.y<b.y && a.z<b.z; 
}

//--- Triangle area ----------------------------------------------------------
inline float TriArea(Vector3 const& a, Vector3 const& b, Vector3 const& c) {
	return 0.5f*((b-a).Cross(c-a).Norm());
}
//----------------------------------------------------------------------------
inline float TriAreaSq(Vector3 const& a, Vector3 const& b, Vector3 const& c) {
	return 0.25f*((b-a).Cross(c-a).NormSq());
}
//----------------------------------------------------------------------------
inline float TriAreaSq4x(Vector3 const& a, Vector3 const& b, Vector3 const& c) {
	return (b-a).Cross(c-a).NormSq();
}
//----------------------------------------------------------------------------
inline float TriArea(Vector3 const& ab, Vector3 const& ac) {
	return 0.5f*(ab.Cross(ac).Norm());
}
//----------------------------------------------------------------------------
inline float TriAreaSq(Vector3 const& ab, Vector3 const& ac) {
	return 0.25f*(ab.Cross(ac).NormSq());
}

// Gram-Schmidt orthonormalization.  Take linearly independent vectors
// X, Y, and Z and compute an orthonormal set (unit length, mutually perpendicular).
inline void Orthonormalize(Vector3& U, Vector3& V, Vector3& W) {
    // If the input vectors are v0, v1, and v2, then the Gram-Schmidt
    // orthonormalization produces vectors u0, u1, and u2 as follows,
    //
    //   u0 = v0/|v0|
    //   u1 = (v1-<u0,v1>u0)/|v1-<u0,v1>u0|
    //   u2 = (v2-<u0,v2>u0-<u1,v2>u1)/|v2 - <u0,v2>u0 - <u1,v2>u1|
    //
    // where |A| indicates length of vector A and <A,B> indicates dot
    // product of vectors A and B.

    // compute u0
    U.Normalize();

    // compute u1
    V -= U*(U.Dot(V));
    V.Normalize();

    // compute u2
    W -= (U*(U.Dot(W)) + V*(V.Dot(W)));
    W.Normalize();
}

//----------------------------------------------------------------------------
inline bool Vector3::CalcBarycentrics(Vector3 const& v0, Vector3 const& v1, Vector3 const& v2,
							          float& s, float& t) const
{
    // compute the area of tri
    Vector3 a(v1 - v0);
	Vector3 b(v2 - v0);
	float fInvTotalArea2X = (a.Cross(b)).Norm();
	if (EqualsZero(fInvTotalArea2X))
		return false;

	fInvTotalArea2X = 1.0f/fInvTotalArea2X;
	
	a.x = v1.x - x; a.y = v1.y - y; a.z = v1.z - z;
	b.x = v2.x - x; b.y = v2.y - y; b.z = v2.z - z;
	s = (a.Cross(b)).Norm() * fInvTotalArea2X;

	a.x = v0.x - x; a.y = v0.y - y; a.z = v0.z - z;
//	b.x = v2.x - x; b.y = v2.y - y; b.z = v2.z - z;
	t = ((a.Cross(b)).Norm()) * fInvTotalArea2X;
    return true;
}

//--- alias ------------------------------------------------------------------
typedef Vector3 Vector3f;
/*
// if positive, distance of vStart and the intersection point return.
// if negative, this ray does not intersect the triangle specified.
float RayTriangleIntersect(Vector& s, Vector vStart, Vector vDir,
						   Vector v1, Vector v2, Vector v3)
{
	s = v1; // temp
	v1 = v2 - v1;	// p
	v2 = v3 - s;    // q
	s  = vStart - s;
}
*/

}}}

#endif