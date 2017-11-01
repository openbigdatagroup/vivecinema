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
 * @file	BLMatrix4.h
 * @desc    float 4x4 matrix, for math operation details, please refer to Matrix3.
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_MATRIX4_H
#define BL_MATRIX4_H

#include "BLMatrix3.h"

namespace mlabs { namespace balai { namespace math {

class Matrix4
{
public:
	static Matrix4 const Identity;
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;
	float _41, _42, _43, _44;

	Matrix4() {} // un-initialized
	~Matrix4() {}
	explicit Matrix4(float const m[16]); // quick conversion, data in row-major.
	explicit Matrix4(Matrix3 const& m3); // convert from Matrix3
    Matrix4(float m11, float m12, float m13, float m14,
            float m21, float m22, float m23, float m24,
            float m31, float m32, float m33, float m34,
            float m41, float m42, float m43, float m44);

	// default copy constructor and assignment operator work fine

	// compare
	bool Equals(Matrix4 const&) const;

	// Reset
	Matrix4 const& MakeIdentity();
	bool IsIdentity() const { return Equals(Identity); }

	// matrix concatenate
	Matrix4& operator*=(Matrix3 const& mat3);
	Matrix4& operator*=(Matrix4 const& mat4);

	// inverse/transpose
	Matrix4 const GetInverse(float* pfDet=0) const;
	Matrix4 const GetTranspose() const {
		return Matrix4(_11, _21, _31, _41,
					   _12, _22, _32, _42,
					   _13, _23, _33, _43,
					   _14, _24, _34, _44);
	}

	// "Norm" of a matrix
	float Determinant() const;
	
	// homogeneous transform(divided by w)
	Vector3 const& PointTransform(Vector3& out, Vector3 const& v) const;
	Vector3 const  PointTransform(Vector3 const& v) const;
};

//
// TO-DO : we need vec(SIMD) version. anyone?
//

//-----------------------------------------------------------------------------
// return [a]*[b] in mathematics 
inline Matrix4 const Matrix4Multiply(Matrix4 const& a, Matrix4 const& b)
{
	return Matrix4(a._11*b._11 + a._12*b._21 + a._13*b._31 + a._14*b._41,
				   a._11*b._12 + a._12*b._22 + a._13*b._32 + a._14*b._42,
				   a._11*b._13 + a._12*b._23 + a._13*b._33 + a._14*b._43,
				   a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14*b._44,

				   a._21*b._11 + a._22*b._21 + a._23*b._31 + a._24*b._41,
				   a._21*b._12 + a._22*b._22 + a._23*b._32 + a._24*b._42,
				   a._21*b._13 + a._22*b._23 + a._23*b._33 + a._24*b._43,
				   a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24*b._44,

				   a._31*b._11 + a._32*b._21 + a._33*b._31 + a._34*b._41,
				   a._31*b._12 + a._32*b._22 + a._33*b._32 + a._34*b._42,
				   a._31*b._13 + a._32*b._23 + a._33*b._33 + a._34*b._43,
				   a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34*b._44,

				   a._41*b._11 + a._42*b._21 + a._43*b._31 + a._44*b._41,
				   a._41*b._12 + a._42*b._22 + a._43*b._32 + a._44*b._42,
				   a._41*b._13 + a._42*b._23 + a._43*b._33 + a._44*b._43,
				   a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44*b._44);
}
// mix with matrix3
inline Matrix4 const Matrix4Multiply(Matrix4 const& a, Matrix3 const& b)
{
	return Matrix4(a._11*b._11 + a._12*b._21 + a._13*b._31,
				   a._11*b._12 + a._12*b._22 + a._13*b._32,
				   a._11*b._13 + a._12*b._23 + a._13*b._33,
				   a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14,

				   a._21*b._11 + a._22*b._21 + a._23*b._31,
				   a._21*b._12 + a._22*b._22 + a._23*b._32,
				   a._21*b._13 + a._22*b._23 + a._23*b._33,
				   a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24,

				   a._31*b._11 + a._32*b._21 + a._33*b._31,
				   a._31*b._12 + a._32*b._22 + a._33*b._32,
				   a._31*b._13 + a._32*b._23 + a._33*b._33,
				   a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34,

				   a._41*b._11 + a._42*b._21 + a._43*b._31,
				   a._41*b._12 + a._42*b._22 + a._43*b._32,
				   a._41*b._13 + a._42*b._23 + a._43*b._33,
				   a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44);
}
//-----------------------------------------------------------------------------
// result = Parent * Child; 
inline Matrix4 const operator*(Matrix4 const& Parent, Matrix4 const& Child) {
	return Matrix4Multiply(Parent, Child);
}

// mix with matrix3
inline Matrix4 const operator*(Matrix4 const& Parent, Matrix3 const& Child) {
	return Matrix4Multiply(Parent, Child);
}

/* projection matrix is identical to platform specific projection matrix(expect row/column major) */
Matrix4 const& gfxBuildPerspectiveProjMatrix(Matrix4& out, float fovY, float aspect_ratio/* width/height */, float near_clip=0.1f, float far_clip=100.0f);
Matrix4 const& gfxBuildParallelProjMatrix(Matrix4& out, float left, float right, float bottom, float top, float near_clip=0.1f, float far_clip=100.0f);

/* build stereo matrices andre 2014/07/14
 * parameters:
 *  fovY					vertical field of view angle in radian
 *  aspect_ratio			aspectio ratio for both view width/right, (typically width = half width of renderbuffer)
 *  interaxial_separation	interaxial separation (in meters) is the distance between the centers of 2 camera lenses
 *	screen_plane_distance   screen plane distance (in meters) is where no parallax occur, objects behind screen plane have positive parallax
 *  near_clip, far_clip		near and far clipping plane distances (in meters)
 *
 * quick fact:(http://www.dashwood3d.com/blog/beginners-guide-to-shooting-stereoscopic-3d/)
 *	1. The interocular separation (or interpupulary distance) technically refers to the distance between the centers of the human eyes.
 *	   This distance is typically accepted to be an average of 65mm (roughly 2.5 inches) for a male adult.
 *	2. Humen eyes naturally angle in towards the object of interest to make it a single image. This is called convergence.
 *	3. What NEVER happens to your eyes in the natural world is divergence, which would mean that your eyes would angle outward.
 *  4. The 1/30th Rule : The 1/30 rule refers to a commonly accepted rule that has been used for decades by hobbyist stereographers around the world. 
 *	   It basically states that the interaxial separation should only be 1/30th of the distance from your camera to the closest subject.
 *	   In the case of ortho-stereoscopic shooting that would mean your cameras should only be 2.5" apart and your closest subject
 *     should never be any closer than 75 inches (about 6 feet) away.
 *
 */
void gfxBuildStereoPerspectiveProjMatrices(Matrix4& matProjL, Matrix4& matProjR,
								   float fovY,
								   float aspect_ratio,
								   float interaxial_separation, // 0.065m or 2.5" see above
								   float screen_plane_distance, // > 75 inches (about 6 feet)
								   float near_clip=0.1f, float far_clip=100.0f);
}}}

#endif // BL_MATRIX4_H