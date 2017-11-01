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
 * @file	BLMatrix3.h
 * @desc    Matrix3, is a 3x4 row-major matrix. Represent a "Frame", or
 *			local transformation to transform from local(object) space to
 *			world space.
 *
 *			The mathematical view of a 3x4 Matrix3 :
 *              | _11, _12, _13, _14 |
 *              | _21, _22, _23, _24 |
 *              | _31, _32, _33, _34 |
 *              |   0,   0,   0, 1.0 | <<-- implicit 4th row of (0,0,0,1)
 *
 *          To Transform a (column) vector3 :
 *              |x'|   | _11, _12, _13, _14 | 	| x |
 *              |y'| = | _21, _22, _23, _24 | * | y |
 *              |z'|   | _31, _32, _33, _34 |	| z |
 *                                              |1.0| <<-- implicit vector3.w
 *          in math :
 *              x' = _11*x + _12*y + _13*z + _14;
 *              y' = _21*x + _22*y + _23*z + _24;
 *              z' = _31*x + _32*y + _33*z + _34;
 *
 *          concatnation :
 *             Matrix3 scale, rotate, translate;
 *             Vector3 pos, result;
 *
 *             result = translate * rotate * scale * pos;
 *
 *           implies, result = (translate * (rotate * (scale * pos)));
 *
 * @author	andre chen
 * @history	2011/12/29 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_MATRIX3_H
#define BL_MATRIX3_H

#include "BLVector3.h"

namespace mlabs { namespace balai { namespace math {

class Transform;

class Matrix3
{
public:
	static Matrix3 const Identity;
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;

	Matrix3() {} // uninitialized
	~Matrix3() {}
	Matrix3(float x, float y, float z) :
		_11(1.0f),_12(0.0f),_13(0.0f),_14(x),
		_21(0.0f),_22(1.0f),_23(0.0f),_24(y),
		_31(0.0f),_32(0.0f),_33(1.0f),_34(z) {}
	Matrix3(float m11, float m12, float m13,
			float m21, float m22, float m23,
			float m31, float m32, float m33):
		_11(m11),_12(m12),_13(m13),_14(0.0f),
		_21(m21),_22(m22),_23(m23),_24(0.0f),
		_31(m31),_32(m32),_33(m33),_34(0.0f) {}
	Matrix3(float m11, float m12, float m13, float m14,
			float m21, float m22, float m23, float m24,
			float m31, float m32, float m33, float m34):
		_11(m11),_12(m12),_13(m13),_14(m14),
		_21(m21),_22(m22),_23(m23),_24(m24),
		_31(m31),_32(m32),_33(m33),_34(m34) {}
	
	// quick conversion, data in row-major.
	explicit Matrix3(float const m[12]): 
		_11(m[0]),_12(m[1]),_13(m[2]),_14(m[3]),
		_21(m[4]),_22(m[5]),_23(m[6]),_24(m[7]),
		_31(m[8]),_32(m[9]),_33(m[10]),_34(m[11]) {}
	
	// convert from Transform
	explicit Matrix3(Transform const& t) { SetTransform(t); }
	
	//
	// no need to make copy constructor and assignment operator, default ones work fine!
	//

	// Reset
	Matrix3 const& MakeIdentity() {
		_11 = 1.0f; _12 = 0.0f; _13 = 0.0f; _14 = 0.0f;
		_21 = 0.0f; _22 = 1.0f; _23 = 0.0f; _24 = 0.0f;
		_31 = 0.0f; _32 = 0.0f; _33 = 1.0f; _34 = 0.0f;
		return *this;
	}

	// transpose
	Matrix3 const GetTranspose() const {
		return Matrix3(_11, _21, _31, _14,
				       _12, _22, _32, _24,
				       _13, _23, _33, _34);
	}

	// local x-axis(1,0,0) in transformed space, i.e. "Right"
	Vector3 const XAxis() const { return Vector3(_11, _21, _31); }

	// local y-axis(0,1,0) in transformed space, i.e. "Front"
	Vector3 const YAxis() const { return Vector3(_12, _22, _32); }

	// local z-axis(0,0,1) in transformed space, i.e. "Up"
	Vector3 const ZAxis() const { return Vector3(_13, _23, _33); }

	// Get origin(position)
	Vector3 const Origin() const { return Vector3(_14, _24, _34); }

    // origin
	void SetOrigin(Vector3 const& o) { _14 = o.x; _24 = o.y; _34 = o.z; }
	void SetOrigin(float x, float y, float z) { _14 = x; _24 = y; _34 = z; }

	// comparison
	bool Equals(Matrix3 const& rhs) const;
	bool IsIdentity() const { return Equals(Identity); }

	// determinant = X.Dot(Y.Cross(Z)) = sx * sy * sz;
	float Determinant() const {
		return (_11*(_22*_33 - _32*_23) + _12*(_31*_23 - _21*_33) + _13*(_21*_32 - _31*_22));
	}
 
	// if true, transform is inside-out and you are in trouble!
	bool Parity() const { return Determinant()<0.0f; }

/*
	// modify orientation
	Matrix3 const& Scale(float sx, float sy, float sz, bool PRE_CONCATENATE=true){
		if (PRE_CONCATENATE) { // (DEFAULT case)
			// |_11 _12 _13|   |_11 _12 _13|   |sx  0  0  0|
			// |_21 _22 _23| = |_21 _22 _23| * | 0 sy  0  0|
			// |_31 _32 _33|   |_31 _32 _33|   | 0  0 sz  0|
			_11 *= sx;	_12 *= sy;	_13 *= sz;
			_21 *= sx;	_22 *= sy;	_23 *= sz;
			_31 *= sx;	_32 *= sy;	_33 *= sz;
		}
		else {
			// |_11 _12 _13 _14|   |sx  0  0  0|   |_11 _12 _13 _14|   
			// |_21 _22 _23 _24| = | 0 sy  0  0| * |_21 _22 _23 _24| 
			// |_31 _32 _33 _34|   | 0  0 sz  0|   |_31 _32 _33 _34|   
			_11 *= sx;	_12 *= sx;	_13 *= sx;	_14 *= sx;
			_21 *= sy;	_22 *= sy;	_23 *= sy;	_24 *= sy;
			_31 *= sz;	_32 *= sz;	_33 *= sz;	_34 *= sz;
		}
		return *this;
	}
	Matrix3 const& Translate(float tx, float ty, float tz, bool PRE_CONCATENATE=false) {
		if (PRE_CONCATENATE) { 
			// |_11 _12 _13 _14|  |_11 _12 _13 _14| | 1  0  0  tx|
			// |_21 _22 _23 _24| =|_21 _22 _23 _24|*| 0  1  0  ty|
			// |_31 _32 _33 _34|  |_31 _32 _33 _34| | 0  0  1  tz|
			_14 += (_11*tx + _12*ty +_13*tz);
			_24 += (_21*tx + _22*ty +_23*tz);
			_34 += (_31*tx + _32*ty +_33*tz);
		}
		else { // (DEFAULT case)
			// |_11 _12 _13 _14|   | 1  0  0  tx|   |_11 _12 _13 _14|   
			// |_21 _22 _23 _24| = | 0  1  0  ty| * |_21 _22 _23 _24| 
			// |_31 _32 _33 _34|   | 0  0  1  tz|   |_31 _32 _33 _34|   
			_14 += tx;	_24 += ty;	_34 += tz;
		}

		return *this;
	}
*/	
	// matrix concatenate : v' = (a*=b)(v) = a*(b*(v))
	Matrix3& operator*=(Matrix3 const& pre);

	// Tranform to Matrix3
	Matrix3 const& SetTransform(Transform const& t);
	
	// Euler angles to Matrix3(origin not touch)
	Matrix3 const& SetEulerAngles(float x/*pitch*/, float y/*roll*/, float z/*yaw*/);
	
	// axis angles to Matrix3(origin not touch)
	Matrix3 const& SetRotateAxisAngle(Vector3 const& axis, float angle);
	
	// note : this build local transform matrix, not "VIEW" matrix.
	// World coordinates:(like 3ds max coordinate system)
	//		* X-axis => Right
	//		* Y-axis => Front
	//		* Z-axis => Up
	Matrix3 const& SetLookAt(Vector3 const& o, Vector3 const& at,
							 Vector3 const& up=Vector3(0.0f, 0.0f, 1.0f));

	// get inverse, get determinant if you interested.
    Matrix3 const GetInverse(float* pDeterminant=0) const;

	// *this = a * b
	Matrix3 const& SetMul(Matrix3 const& a, Matrix3 const& b);

	// Transform everything in World space to this Coordinate System.
	// out = [Matrix3][v]
	Vector3 const& PointTransform(Vector3& out, Vector3 const& p) const;
	Vector3 const  PointTransform(Vector3 const& p) const {
		return Vector3(_11*p.x + _12*p.y + _13*p.z + _14,
				       _21*p.x + _22*p.y + _23*p.z + _24,
				       _31*p.x + _32*p.y + _33*p.z + _34);
	}
	Vector3 const& VectorTransform(Vector3& out, Vector3 const& v) const;
	Vector3 const  VectorTransform(Vector3 const& v) const {
		return Vector3(_11*v.x + _12*v.y + _13*v.z,
				       _21*v.x + _22*v.y + _23*v.z,
				       _31*v.x + _32*v.y + _33*v.z);
	}

	// inverse transpose and apply vector transform
	Vector3 const& NormalTransform(Vector3& out, Vector3 const& n) const;
};

//-----------------------------------------------------------------------------
inline Matrix3 const Matrix3Multiply(Matrix3 const& Parent, Matrix3 const& Child)
{
	return Matrix3(
			Parent._11*Child._11 + Parent._12*Child._21 + Parent._13*Child._31,
			Parent._11*Child._12 + Parent._12*Child._22 + Parent._13*Child._32,
			Parent._11*Child._13 + Parent._12*Child._23 + Parent._13*Child._33,
			Parent._11*Child._14 + Parent._12*Child._24 + Parent._13*Child._34 + Parent._14,

			Parent._21*Child._11 + Parent._22*Child._21 + Parent._23*Child._31,
			Parent._21*Child._12 + Parent._22*Child._22 + Parent._23*Child._32,
			Parent._21*Child._13 + Parent._22*Child._23 + Parent._23*Child._33,
			Parent._21*Child._14 + Parent._22*Child._24 + Parent._23*Child._34 + Parent._24,

			Parent._31*Child._11 + Parent._32*Child._21 + Parent._33*Child._31,
			Parent._31*Child._12 + Parent._32*Child._22 + Parent._33*Child._32,
			Parent._31*Child._13 + Parent._32*Child._23 + Parent._33*Child._33,
			Parent._31*Child._14 + Parent._32*Child._24 + Parent._33*Child._34 + Parent._34);
}
//-----------------------------------------------------------------------------
inline void Matrix3Multiply(Matrix3& out, Matrix3 const& Parent, Matrix3 const& Child)
{
	out._11 = Parent._11*Child._11 + Parent._12*Child._21 + Parent._13*Child._31;
	out._12 = Parent._11*Child._12 + Parent._12*Child._22 + Parent._13*Child._32;
	out._13 = Parent._11*Child._13 + Parent._12*Child._23 + Parent._13*Child._33;
	out._14 = Parent._11*Child._14 + Parent._12*Child._24 + Parent._13*Child._34 + Parent._14;

	out._21 = Parent._21*Child._11 + Parent._22*Child._21 + Parent._23*Child._31;
	out._22 = Parent._21*Child._12 + Parent._22*Child._22 + Parent._23*Child._32;
	out._23 = Parent._21*Child._13 + Parent._22*Child._23 + Parent._23*Child._33;
	out._24 = Parent._21*Child._14 + Parent._22*Child._24 + Parent._23*Child._34 + Parent._24;

	out._31 = Parent._31*Child._11 + Parent._32*Child._21 + Parent._33*Child._31;
	out._32 = Parent._31*Child._12 + Parent._32*Child._22 + Parent._33*Child._32;
	out._33 = Parent._31*Child._13 + Parent._32*Child._23 + Parent._33*Child._33;
	out._34 = Parent._31*Child._14 + Parent._32*Child._24 + Parent._33*Child._34 + Parent._34;
}
//-----------------------------------------------------------------------------
inline Matrix3 const operator*(Matrix3 const& Parent, Matrix3 const& Child) {
	return Matrix3Multiply(Parent, Child);
}

// build Covariance Matrix
Matrix3 const BuildCovarianceMatrix(Vector3 const* vertices, int cnt, Vector3 const* pMean=0);

// symmetric matrix(eg. Covariance Matrix) pass in
bool EigenDecompositioin(Vector3 axes[3], Matrix3& matSymmetric);

// calculate major axes from a set of vertices
inline bool CalcMajorAxes(Vector3 axes[3], Vector3 const* vertices, int vtxCnt, Vector3 const* mean=NULL) {
	// Refer to "Mathematics for 3D Game Programming and Computer Graphics",
	// ch7-1 Bounding Volume Construction.
	// since matCov is symmetric, it always has 3 eigenvectors that form an orthonormal basis,
	// well, mathematically:-)
	if (vtxCnt>0 && NULL!=vertices) {
		if (1==vtxCnt) {
			axes[0] = Vector3(1.0f, 0.0f, 0.0f);
			axes[1] = Vector3(0.0f, 1.0f, 0.0f);
			axes[2] = Vector3(0.0f, 0.0f, 1.0f);
			return true;
		}
		Matrix3 matCov(BuildCovarianceMatrix(vertices, vtxCnt, mean));
		return EigenDecompositioin(axes, matCov);
	}
	return false;
}

// [transform utilities]
// view matrix transforms vertices from world space to view space and 
// map to platform specific coordinate system.
// ltm(local transformation matrix) should be orthgonal.
Matrix3 const& gfxBuildViewMatrixFromLTM(Matrix3& out, Matrix3 const& ltm);
// build view matrix from transform
Matrix3 const& gfxBuildViewMatrixFromTransform(Matrix3& out, Transform const&);

// refer gfxBuildStereoPerspectiveProjMatrix(), BLMatrix4.h, ln.163
// interaxial_separation : interpupulary distance is typically accepted to be an average of 65mm (roughly 2.5 inches) for a male adult.
void gfxBuildStereoViewMatricesFromLTM(Matrix3& matViewL, Matrix3& matViewR, Matrix3 const& ltm,
									   float interaxial_separation);	// 0.065f

void gfxBuildStereoViewMatricesFromTransform(Matrix3& matViewL, Matrix3& matViewR, Transform const& xform,
									   float interaxial_separation);	// 0.065f

}}} // namespace mlabs::balai::math

#endif // ML_MATRIX3_H