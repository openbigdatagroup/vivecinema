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
 * @file	BLVector4.h
 * @desc    x, y, z, w
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef ML_VECTOR4_H
#define ML_VECTOR4_H

#include "BLMath.h"

namespace mlabs { namespace balai { namespace math {

class Vector4
{
public:
	float x, y, z, w;

	// construction
	Vector4() {}  // uninitialized
	Vector4(float _x, float _y, float _z, float _w):x(_x),y(_y),z(_z),w(_w) {}

	// default copy constructor/assignment operator work fine

    // comparison
	bool IsZero() const {
        return EqualsZero(x) && EqualsZero(y) &&
		       EqualsZero(z) && EqualsZero(w);
    }
	bool Equals(Vector4 const& rhs) const {
		return math::Equals(x, rhs.x) && math::Equals(y, rhs.y) &&
			   math::Equals(z, rhs.z) && math::Equals(w, rhs.w);
	}
	bool operator==(Vector4 const& rhs) const {
		return (x==rhs.x && y==rhs.y && z==rhs.z && w==rhs.w);
	}
	bool operator!=(Vector4 const& rhs) const {
		 return (x!=rhs.x || y!=rhs.y || z!=rhs.z || w!=rhs.w);
	}

	// arithmetic updates
	Vector4& operator+=(Vector4 const& rhs) {
		x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w;
        return *this;
    }
	Vector4& operator-=(Vector4 const& rhs) {
        x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w;
        return *this;
	}
	Vector4& operator*=(Vector4 const& rhs) {
        x *= rhs.x; y *= rhs.y; z *= rhs.z; w *= rhs.w;
        return *this;
    }
    Vector4& operator*=(float s) {
	    x *= s; y *= s; z *= s; w *= s;
	    return *this;
    }
//	Vector4& operator/=(float s) {
//		BL_ASSERT2(!EqualsZero(s), "[Vector4] divided by zero");
//      s = 1.0f/s;
//      x *= s; y *= s; z *= s; w *= s;
//      return *this;
//	}

    // vector operations
	float Norm() const { return Sqrt(x*x + y*y + z*z + w*w); }
	float NormSq() const { return (x*x + y*y + z*z + w*w); }
	float CabNorm() const { return (FAbs(x) + FAbs(y) + FAbs(z) + FAbs(w)); }
	Vector4 const& Normalize(float* length = 0) {
		float s = (x*x + y*y + z*z + w*w);
        if (EqualsOne(s)) {
		    if (length)
			    *length = 1.0f;
		    return *this;
	    }
	    else if (EqualsZero(s)) {
		    if (length)
			    *length = 0.0f;
		    x = y = z = w = 0.0f;
		    return *this;
	    }

	    s = Sqrt(s);
	    if (length)
		    *length = s;

        s = 1.0f/s;
        x *= s; y *= s; z *= s; w *= s;
	    return *this;
	}

    // dot product
	float Dot(Vector4 const& rhs) const {
		return (x*rhs.x + y*rhs.y + z*rhs.z + w*rhs.w);
	}

	void Clamp(float inf, float sup) {
		if (inf>sup) {
		    float const temp(sup);
		    sup = inf;
		    inf = temp;
	    }

	    if (x<inf)
			x = inf;
	    else if (x>sup)
			x = sup;
	
	    if (y<inf)
			y = inf;
	    else if (y>sup)
			y = sup;
	
        if (z<inf)
			z = inf;
	    else if (z>sup)
			z = sup;

        if (w<inf)
			w = inf;
        else if (w>sup)
			w = sup;
	}
};

//----------------------------------------------------------------------------
inline Vector4 const operator+(Vector4 const& a, Vector4 const& b) {
	return Vector4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
}
//----------------------------------------------------------------------------
inline Vector4 const operator-(Vector4 const& a, Vector4 const& b) {
	return Vector4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w);
}
//----------------------------------------------------------------------------
inline Vector4 const operator*(Vector4 const& a, Vector4 const& b) {
	return Vector4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);
}
//----------------------------------------------------------------------------
inline Vector4 const operator*(Vector4 const& a, float s) {
	return Vector4(a.x*s, a.y*s, a.z*s, a.w*s);
}
//----------------------------------------------------------------------------
inline Vector4 const operator*(float s, Vector4 const& a) {
	return Vector4(a.x*s, a.y*s, a.z*s, a.w*s);
}
//--- Negate -----------------------------------------------------------------
inline Vector4 const operator-(Vector4 const& a) {
	return Vector4(-a.x, -a.y, -a.z, -a.w);
}

// alias
typedef Vector4 Vector4f;

}}}

#endif