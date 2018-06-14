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
 * @file    BLQuaternion.h
 * @desc    quaternion for rotation
 * @author	andre chen
 * @history	2011/12/29 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_QUATERNION_H
#define BL_QUATERNION_H

#include "BLVector3.h"

// Use quaternions to fix gimbal lock :
//	1. Use a quaternion to represent the rotation. 
//	2. Generate a temporary quaternion for the change from the current 
//	   orientation to the new orientation. 
//	3. PostMultiply the temp quaternion with the original quaternion. 
//	   This results in a new orientation that combines both rotations. 
//	4. Convert the quaternion to a matrix and use matrix multiplication as normal. 

namespace mlabs { namespace balai { namespace math {

// forward declarations
class Matrix3;

class Quaternion
{
	// A quaternion is q = w + x*i + y*j + z*k. for a normalized quaternion 
	// q has form of cos(A/2) + sin(A/2)*(Rx*i + Ry*j + Rz*k), 
	// where A is the rotation angle with respect to unit rotation axis (Rx,Ry,Rz).
	// (w_=cos(A/2), x_=sin(A/2)*Rx, y_=sin(A/2)*Ry, z_=sin(A/2)*Rz)
	//
	// Quaternion rotate v(x,y,z) in this way:
	//   v' = Rotate_q(v) = q(0+v)q*   where q* is the inverse/conjugate of q.
	//
	// Consider the compose rotation : Rotate_q1(Rotate_q2(v)) = q1(q2 (0+v) q2*)q1* 
	//   = q1q2 (0+v) q2*q1* = (q1q2)(0+v)(q1q2)* = Rotate_q1q2(v)
	//	i.e.
	//		q = q1q2 => Rotate_q : first rotate via q2 and then rotate via q1.
	//		(just like matrix mat = ma*mb (first apply mb then ma)
    //
	float w_, x_, y_, z_;

public:
	// identity - no rotate
	Quaternion():w_(1.0f),x_(0.0f),y_(0.0),z_(0.0f) {}
	
	// un-normalized, use at your own risk!
	Quaternion(float w, float x, float y, float z):w_(w),x_(x),y_(y),z_(z) {}

	// from rotate angle and axis
	Quaternion(Vector3 const& axis, float angle):w_(1.0f),x_(0.0f),y_(0.0),z_(0.0f) { 
		SetAxisAngle(axis, angle);
	}

	// from Euler angle
	Quaternion(float ax, float ay, float az):w_(1.0f),x_(0.0f),y_(0.0),z_(0.0f) {
		SetEulerAngles(ax, ay, az);
	}

	// conjugate(inverse)
	Quaternion const Conjugate() const { return Quaternion(w_, -x_, -y_, -z_); }
	Quaternion const GetInverse() const { return Quaternion(w_, -x_, -y_, -z_); }

	// identity
	void MakeIdentity() { w_=1.0f; x_=y_=z_=0.0f; }
	bool IsIdentity() const { return EqualsOne(w_)||EqualsOne(-w_); }

    // comparison
	bool Equals(Quaternion const& rhs) const;
	bool operator==(Quaternion const& rhs) const { return Equals(rhs); }
	bool operator!=(Quaternion const& rhs) const { return !Equals(rhs); }

	// compound
    Quaternion& operator*=(Quaternion const& b) {
        // temp vxt, yt, zt in case of a *= a;
		float const xt = w_*b.x_ + x_*b.w_ + y_*b.z_ - z_*b.y_;
		float const yt = w_*b.y_ - x_*b.z_ + y_*b.w_ + z_*b.x_;
		float const zt = w_*b.z_ + x_*b.y_ - y_*b.x_ + z_*b.w_;
		w_ = w_*b.w_ - x_*b.x_ - y_*b.y_ - z_*b.z_;
		x_ = xt;
		y_ = yt;
		z_ = zt;
		return *this;
	}

//	float Norm() const                   { return Sqrt(w_*w_ + x_*x_ + y_*y_ + z_*z_); }
	float NormSq() const				 { return (w_*w_ + x_*x_ + y_*y_ + z_*z_); }
    float Dot(Quaternion const& q) const { return (w_*q.w_+x_*q.x_+y_*q.y_+z_*q.z_); }
	Quaternion const& QuickNormalize() { // at your own risk..
		float const s = FastInvSqrt(w_*w_ + x_*x_ + y_*y_ + z_*z_);
	    w_*=s; x_*=s; y_*=s; z_*=s;
	    return *this;
	}
	// normalize(reset as identity if zero)
	Quaternion const& Normalize(); 

	// conver from rotation/angle
	Quaternion const& SetAxisAngle(Vector3 axis, float angle);
	bool GetAxisAngle(Vector3& axis, float& angle) const;

    // convert from yaw(heading)/pitch(attitude)/roll(bank) : R = [yaw][roll][pitch]
	Quaternion const& SetEulerAngles(float ax, float ay, float az);
	Quaternion const& SetYawPitchRoll(float yaw, float pitch, float roll) {
		return SetEulerAngles(pitch, roll, yaw);
	}

	// get angles(note : this involved slow atan, call with care, also no error reports)
	void GetEulerAngles(float& ax, float& ay, float& az) const;
	void GetYawPitchRoll(float& yaw, float& pitch, float& roll) const {
		GetEulerAngles(pitch, roll, yaw);
	}

	// polar coordinate system
	// phi : polar angle [0, pi], [north pole, south pole]
	// theta : Azimuthal angle [-pi, +pi]
	// roll:(also called back) [-pi, +pi]
	// return true if azimuthal angle is valid, or false if not(north/south pole)
	bool GetSphericalCoordinates(float& phi, float& theta, float& roll) const;

	// make self a quaternion that will rotate v1 to v2(both unit vectors)
	Quaternion const& SetConstraint(Vector3 const& v1, Vector3 const& v2);

    // build a quaternion with respect to axis x, y, z. all 3 vectors are normalized.
	// SetAxes brainless solves the equation, so it could come out the result that never exists.
	// return true if it looks like a real solve. false otherwise.
	bool SetAxes(Vector3 const& x/*right*/, Vector3 const& y/*front*/, Vector3 const& z/*up*/);
	bool SetAxes(Vector3 const& x, Vector3 const& y) {
		return SetAxes(x, y, x.Cross(y));
	}

	// Quaternion const& SetRotationMatrix(Matrix3& mtx) const; // NOT always possible!
	Matrix3 const& BuildRotationMatrix(Matrix3& mtx, bool resetTranslate=true) const;

    // rotate a vector
	Vector3 const Rotate(Vector3 const& v) const;
	Vector3 const& Rotate(Vector3& out, Vector3 const& v) const; // out is returned

	// local x-axis(1,0,0) after rotated(i.e. "right" vector, the 1st column of rotation matrix)
	Vector3 const XAxis() const {
		return Vector3(1.0f-2.0f*(y_*y_+z_*z_), 2.0f*(x_*y_+w_*z_), 2.0f*(x_*z_-w_*y_));
	}

	// local y-axis(0,1,0) after rotated(i.e. "front" vector, the 2nd column of rotation matrix)
	Vector3 const YAxis() const {
		return Vector3(2.0f*(x_*y_-w_*z_), 1.0f-2.0f*(x_*x_+z_*z_), 2.0f*(y_*z_+w_*x_));
	}

	// local z-axis(0,0,1) after rotated(i.e. "up" vector, the 3rd column of rotation matrix)
	Vector3 const ZAxis() const {
		return Vector3(2.0f*(x_*z_+w_*y_), 2.0f*(y_*z_-w_*x_), 1.0f-2.0f*(x_*x_+y_*y_));
	}

	// multiple rotation angle
	Quaternion const MultiplyAngle(float scale) const;

	//////////////////////////////////////////////////////////////////////////
    // interpolation : Lerp, SLerp, quickSLerp
    // reference : 
    // "Hacking Quaternions" - 
    //   http://number-none.com/product/Hacking%20Quaternions/
    // "Understanding Slerp, Then Not Using It" -
    //   http://number-none.com/product/Understanding%20Slerp,%20Then%20Not%20Using%20It/
    // Game Programming Gems I Sec.2.9 "Interpolating Quaternions"
    // Game Programming Gems II Sec.2.6 "Smooth C2 Quaternion-based Flythrough Paths"
    // Game Programming Gems V sec.2.4 "Faster Quaternion Interpolation Using Approximations"
    // Mathematics for 3D Game Programming & Computer Graphics, Sec.3.6 "Quaternion"
	//
	// Performance compare:
	//                      constant velocity | Execute Time | Shortest path
	// QuaternionLerp              NO         |    100%      |     Yes
	// QuaternionQuickSlerp    YES(Almost)    |    125%      |     Yes
	// QuaternionSlerp             YES        |    225%      |     Yes
	//
	// Always prefer SetQuickSlerp, moreover, also note SetSphericalLerp support "Extra spins".
	//
	Quaternion const& SetLinearLerp(Quaternion const& a, Quaternion const& b, float lerp);
	Quaternion const& SetSphericalLerp(Quaternion const& a, Quaternion const& b, float lerp, int iExtraSpins=0);
	Quaternion const& SetQuickSlerp(Quaternion const& a, Quaternion const& b, float lerp, bool faster=true);

	// negate(represent another identical rotation)
	friend Quaternion const operator-(Quaternion const&);
	
	// multiply, produce a combined rotation : result(v) = a(b(v))
	friend Quaternion const operator*(Quaternion const& a, Quaternion const& b);

    // friend
    friend class DualQuaternion;
};

//- friends -------------------------------------------------------------------
inline Quaternion const operator-(Quaternion const& q) {
	return Quaternion(-q.w_, -q.x_, -q.y_, -q.z_);
}
//-----------------------------------------------------------------------------
inline Quaternion const operator*(Quaternion const& a, Quaternion const& b) {
	return Quaternion(a.w_*b.w_ - a.x_*b.x_ - a.y_*b.y_ - a.z_*b.z_,  // w = aw*bw - <va, vb>
					  a.w_*b.x_ + a.x_*b.w_ + a.y_*b.z_ - a.z_*b.y_,  // xyz = aw * vb + bw * va + va.cross(vb)
					  a.w_*b.y_ - a.x_*b.z_ + a.y_*b.w_ + a.z_*b.x_,
					  a.w_*b.z_ + a.x_*b.y_ - a.y_*b.x_ + a.z_*b.w_);
}

/* Alternative form of quaternion *****************************************
// Note : S3 -> R4
// quat(S3) : (cos(theta), sin(theta)*v) = (s, (a, b, c))
// R4(x, y, z, w) -
// x = a/sqrt(2(1-s)),
// y = b/sqrt(2(1-s)),
// z = c/sqrt(2(1-s)),
// w = (1-s)/sqrt(2(1-s)). s = 1.0f, null rotation cause singularity!!!
// in that case, "Selective Negation", (x,y,z,w) = (0,0,0,1)
//
// R4 -> S3
// s = (x^x+y^y+z^z-w^w)/(x^x+y^y+z^z+w^w);
// a = 2xw/(x^x+y^y+z^z+w^w);
// b = 2yw/(x^x+y^y+z^z+w^w);
// c = 2zw/(x^x+y^y+z^z+w^w);
//
// SO(3)?
****************************************************************************/

//
// TO-DO : verify all methods!
//
// dual quaternion : http://www.cis.upenn.edu/~ladislav/dq/index.html
class DualQuaternion
{
    Quaternion real_, dual_;

public:
    DualQuaternion():real_(),dual_(0.0f,0.0f,0.0f,0.0f) {}
    DualQuaternion(Quaternion const& rotate, Vector3 const& translate):
        real_(rotate),dual_(0.0f, translate.x, translate.y, translate.z) {
        //real_.Normalize();
        dual_ *= real_;
        dual_.x_ *= 0.5f; dual_.y_ *= 0.5f; dual_.z_ *= 0.5f; dual_.w_ *= 0.5f;
/*
        // dual part: w:0 x:1 y:2 z:3
        dual_.w = -0.5*( translate.x*real_.x + translate.y*real_.y + translate.z*real_.z);
        dual_.x =  0.5*( translate.x*real_.w + translate.y*real_.z - translate.z*real_.y);
        dual_.y =  0.5*(-translate.x*real_.z + translate.y*real_.w + translate.z*real_.x);
        dual_.z =  0.5*( translate.x*real_.y - translate.y*real_.x + translate.z*real_.w);
*/
    }

    Quaternion const& GetRotation() const { return real_; }
    Vector3 const GetTranslation() const {
        Quaternion t = dual_; // dual_ must be normalized
        t.x_ *= 2.0f; t.y_ *= 2.0f; t.z_ *= 2.0f; t.w_ *= 2.0f;
        t *= real_.Conjugate();
        return Vector3(t.x_, t.y_, t.z_);
    }
    
    DualQuaternion const& Normalize() {
        float s = real_.NormSq();
        s = 1.0f/sqrt(s);
        real_.x_ *= s; real_.y_ *= s; real_.z_ *= s; real_.w_ *= s;
        dual_.x_ *= s; dual_.y_ *= s; dual_.z_ *= s; dual_.w_ *= s;
        return *this;
    }

    Matrix3 const& BuildMatrix(Matrix3& mtx) const;
};

/*
// input: unit quaternion 'q0', translation vector 't' 
// output: unit dual quaternion 'dq'
void QuatTrans2UDQ(const float q0[4], const float t[3], 
                  float dq[2][4])
{
   // non-dual part (just copy q0):
   for (int i=0; i<4; i++) dq[0][i] = q0[i];
   // dual part:
   dq[1][0] = -0.5*(t[0]*q0[1] + t[1]*q0[2] + t[2]*q0[3]);
   dq[1][1] = 0.5*( t[0]*q0[0] + t[1]*q0[3] - t[2]*q0[2]);
   dq[1][2] = 0.5*(-t[0]*q0[3] + t[1]*q0[0] + t[2]*q0[1]);
   dq[1][3] = 0.5*( t[0]*q0[2] - t[1]*q0[1] + t[2]*q0[0]);
}

// input: unit dual quaternion 'dq'
// output: unit quaternion 'q0', translation vector 't'
void UDQ2QuatTrans(const float dq[2][4], 
                  float q0[4], float t[3])
{
   // regular quaternion (just copy the non-dual part):
   for (int i=0; i<4; i++) q0[i] = dq[0][i];
   // translation vector:
   t[0] = 2.0*(-dq[1][0]*dq[0][1] + dq[1][1]*dq[0][0] - dq[1][2]*dq[0][3] + dq[1][3]*dq[0][2]);
   t[1] = 2.0*(-dq[1][0]*dq[0][2] + dq[1][1]*dq[0][3] + dq[1][2]*dq[0][0] - dq[1][3]*dq[0][1]);
   t[2] = 2.0*(-dq[1][0]*dq[0][3] - dq[1][1]*dq[0][2] + dq[1][2]*dq[0][1] + dq[1][3]*dq[0][0]);
}

// input: dual quat. 'dq' with non-zero non-dual part
// output: unit quaternion 'q0', translation vector 't'
void DQ2QuatTrans(const float dq[2][4], 
                  float q0[4], float t[3])
{
   float len = 0.0;
   for (int i=0; i<4; i++) len += dq[0][i] * dq[0][i];
   len = sqrt(len); 
   for (int i=0; i<4; i++) q0[i] = dq[0][i] / len;
   t[0] = 2.0*(-dq[1][0]*dq[0][1] + dq[1][1]*dq[0][0] - dq[1][2]*dq[0][3] + dq[1][3]*dq[0][2]) / len;
   t[1] = 2.0*(-dq[1][0]*dq[0][2] + dq[1][1]*dq[0][3] + dq[1][2]*dq[0][0] - dq[1][3]*dq[0][1]) / len;
   t[2] = 2.0*(-dq[1][0]*dq[0][3] - dq[1][1]*dq[0][2] + dq[1][2]*dq[0][1] + dq[1][3]*dq[0][0]) / len;
}
*/
}}}

#endif	// BL_QUATERNION_H