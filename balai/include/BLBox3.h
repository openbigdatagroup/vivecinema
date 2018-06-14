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
 * @file	BLBox3.cpp
 * @desc    3D (bounding) box
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_BOX3_H
#define BL_BOX3_H

#include "BLQuaternion.h"
#include "BLPlane.h"

namespace mlabs { namespace balai { namespace math {

class Transform;

// axis-aligned bounding Box
class AABox3
{
	Vector3 center_;	// position center
	Vector3 extent_;	// extent(half the extent actually)
	float	radius_;	// radius < 0 if empty
	float	invMass_;	// inverse of mass, for physics, a = f/mass = f*invMass_, also pad to 32B

public:
	AABox3():center_(0.0f,0.0f,0.0f),extent_(0.0f,0.0f,0.0f),radius_(-1.0f),invMass_(0.0f) {}
	AABox3(Vector3 const& inf, Vector3 const& sup):
	  center_(0.0f,0.0f,0.0f),extent_(0.0f,0.0f,0.0f),radius_(-1.0f),invMass_(0.0f) { Init(inf, sup); }
	~AABox3() {}

	// accessors
	Vector3 const& GetCenter() const { return center_; }
	Vector3 const& GetHalfExtent() const { return extent_; }
	float GetRadius() const { return radius_; }
	float GetInvMass() const { return invMass_; }
	bool IsEmpty() const { return radius_<0.0f; }

	// volume
	float Volume() const { return (radius_>0.0) ? (8.0f*extent_.x*extent_.y*extent_.z):0.0f; }

    void Reset() {
		center_.Zero();
		extent_.Zero();
		radius_ = -1.0f;
		invMass_ = 0.0f;
	}

	bool Init(Vector3 const& inf, Vector3 const& sup, float mass=-1.0f) {
		if (inf.x<=sup.x && inf.y<=sup.y && inf.y<=sup.y) {
			center_  = 0.5f*(inf+sup);
			extent_  = sup - center_;
			radius_  = extent_.Norm();
			if (mass>0.0f)
			    invMass_ = 1.0f/mass;
		    else if (0.0f==mass)
			    invMass_ = 0.0f;
			return true;
		}
		return false;
	}

    // set mass, value less equal to zero will set as infinity
	void SetMass(float mass) {
		if (mass>0.0f)
			invMass_ = 1.0f/mass;
		else
			invMass_ = 0.0f;
	}

	// min max
	bool GetMinMax(Vector3& inf, Vector3& sup) const {
		inf = sup = center_;
		if (radius_>0.0f) {	
			inf -= extent_;
			sup += extent_;
			return true;
		}
		return false;
	}

	bool GetCenter(Vector3& c) const {
		if (radius_>=0.0f) {
			c = center_;
			return true;
		}
		return false;
	}

	// check a point
	bool Contains(Vector3 const& pt) const {
		if (radius_>0.0f) {
			Vector3 const diff = pt - center_;
			return (diff.NormSq()<(radius_*radius_)) &&
					(-extent_.x<diff.x && diff.x<extent_.x) &&
						(-extent_.y<diff.y && diff.y<extent_.y) &&
							(-extent_.z<diff.z && diff.z<extent_.z);
		}
		return false;
	}

	// merge that
	AABox3& operator+=(AABox3 const& that);

	// box3
	AABox3 const& GenerateByPoints(Vector3 const* pts, uint32 nCnt);

	// eval vertices
	uint32 EvalVertices(Vector3 vert[8]) const;

	// aabb-plane intersects 
	PLANE_SIDE WhichSide(Plane const& plane) const;

	// aabb-aabb intersects
	bool Overlaps(AABox3 const& that) const;
};
BL_COMPILE_ASSERT(32==sizeof(AABox3), aabox3_size_not_correct);

// orient box
class OBox3
{
	Quaternion	orientation_;
	Vector3		center_;	// position center
	Vector3		extent_;	// extent
	float		radius_;	// radius < 0 if empty
	float		invMass_;	// inverse of mass, for physics, a = f/mass = f*invMass_, also pad to 48 bytes

public:
	OBox3():orientation_(),center_(0.0f,0.0f,0.0f),extent_(0.0f,0.0f,0.0f),radius_(-1.0f),invMass_(0.0f) {}
	OBox3(Vector3 const& inf, Vector3 const& sup):orientation_(),
	  center_(0.0f,0.0f,0.0f),extent_(0.0f,0.0f,0.0f),radius_(-1.0f),invMass_(0.0f) { Init(inf, sup); }
	OBox3(Quaternion const& rot, Vector3 const& center, Vector3 const& ext, float radius, float weight=0.0f):
		orientation_(rot),center_(center),extent_(ext),radius_(radius),invMass_(weight) {}
	~OBox3() {}

	// default copy constructor and default assignment operator work fine.
	void Reset() {
		orientation_.MakeIdentity();
		center_.Zero();
		extent_.Zero();
		radius_ = -1.0f;
		invMass_ = 0.0f;
	}
	bool IsEmpty() const { return (radius_<0.0f); }
	float Volume() const { return (0.0f<radius_) ? (8.0f*extent_.x*extent_.y*extent_.z):0.0f; }

	// accessors
	Quaternion const& GetOrientation() const { return orientation_; }
	Vector3 const& GetCenter() const { return center_; }
	Vector3 const& GetHalfExtent() const { return extent_; }
	float GetRadius() const { return radius_; }
	float GetInvMass() const { return invMass_; }
	bool Init(Vector3 const& inf, Vector3 const& sup, float mass=-1.0f) {
		if (inf.x<=sup.x && inf.y<=sup.y && inf.y<=sup.y) {
			orientation_.MakeIdentity();
			center_  = 0.5f*(inf+sup);
			extent_  = sup - center_;
			radius_  = extent_.Norm();
			if (mass>0.0f)
			    invMass_ = 1.0f/mass;
		    else if (0.0f==mass)
			    invMass_ = 0.0f;
			return true;
		}
		return false;
	}

	// set mass, value less equal to zero will set as infinity
	void SetMass(float mass) {
		if (mass>0.0f)
			invMass_ = 1.0f/mass;
		else
			invMass_ = 0.0f;
	}

	// check if a point inside box or not
	bool Contains(Vector3 const& point) const {
		if (radius_>0.0) {
			Vector3 diff = point - center_;
			if (diff.NormSq()>=(radius_*radius_))
				return false;

			// rotate to box space
			orientation_.GetInverse().Rotate(diff, diff);

			// check!
			return (-extent_.x<diff.x && diff.x<extent_.x) &&
					(-extent_.y<diff.y && diff.y<extent_.y) &&
					 (-extent_.z<diff.z && diff.z<extent_.z);
		}
		return false;
	}

	// get a plane for a face(0,1,2,3,4,5), the plane will be push alittle bit
	// to make box 'behind' the plane.
	bool GetFacePlane(Plane& plane, uint32 face) const {
		if (radius_>=0.0f) {
			Vector3 normal;
			Vector3 pt = center_;
			switch (face%6)
			{
			case 0:
				normal = orientation_.XAxis();
				pt += (extent_.x+constants::float_epsilon)*normal;
				break;
			case 1:
				normal = -orientation_.XAxis();
				pt += (extent_.x+constants::float_epsilon)*normal;
				break;

			case 2:
				normal = orientation_.YAxis();
				pt += (extent_.y+constants::float_epsilon)*normal;
				break;
			case 3:
				normal = -orientation_.YAxis();
				pt += (extent_.y+constants::float_epsilon)*normal;
				break;

			case 4:
				normal = orientation_.ZAxis();
				pt += (extent_.z+constants::float_epsilon)*normal;
				break;
			case 5:
				normal = -orientation_.ZAxis();
				pt += (extent_.z+constants::float_epsilon)*normal;
				break;
			}
			plane = Plane(normal, pt);
			return true;
		}
		return false;
	}

	// merge operator
	OBox3& operator+=(OBox3 const& r);

	// transformed by local box
	OBox3 const& SetTransform(Transform const& xform, OBox3 const& localBox);
	OBox3 const& SelfTransform(Transform const& xform); // transform itself
	OBox3 const& MoveTo(Vector3 const& pos) {
         center_ = pos;
		 return *this;
	}

	// eval (max 8) corner vertices of the Box, return # of vertices evaluated
	uint32 EvalVertices(Vector3 vtx[8]) const;

	// generate by points
	OBox3 const& GenerateByPoints(Vector3 const* pts, uint32 nCnt, Vector3 const* mean=NULL);

	// which side
	PLANE_SIDE WhichSide(Plane const& plane) const;

	// box-box intersects
	bool Overlaps(OBox3 const& that) const;
};

//----------------------------------------------------------------------------
inline OBox3 const operator+(OBox3 a, OBox3 const& b) { return a+=b; }

}}}

#endif // BL_BOX3_H