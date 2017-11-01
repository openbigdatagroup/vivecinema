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
 * @file	BLSpline.h
 * @desc    all kinds of B-spline
 * @author	andre chen
 * @history	2014/03/13 rewriten for htc studio engineering balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_SPLINE_H
#define BL_SPLINE_H

#include "BLObject.h"
#include "BLVector3.h"

namespace mlabs { namespace balai { namespace math {

enum SPLINE_TYPE {
	SPLINE_LINEAR1D,
	SPLINE_LINEAR3D,
	SPLINE_BEZIER1D,
	SPLINE_BEZIER3D,
	SPLINE_TCB1D,
	SPLINE_TCB3D,
};

//---------------------------------------------------------------------------
// *** spline interface
//---------------------------------------------------------------------------
class ISpline : public IShared
{
	// declare but not define
	BL_NO_COPY_ALLOW(ISpline);

	BL_DECLARE_RTTI;

//	static SPLINE_TYPE const sType_;

protected:
	virtual ~ISpline() {}

public:
	ISpline() {}
	virtual SPLINE_TYPE Type() const = 0; //  { return sType_; } virtual is a overkill.
	
	// with various dimemsion of splines...
	virtual bool Eval(float* value, float t) const = 0;
};

//---------------------------------------------------------------------------
// *** base spline class template
//---------------------------------------------------------------------------
template <typename KeyType>
class SplineBase : public ISpline
{
	static SPLINE_TYPE const sType_;

	KeyType* waypoints_;	// buffer allocation
	float*	 timeStamps_;	// time stample, could be 0 if linear
	uint32	 numWaypoints_;	// current size
	uint32	 allocSize_;	// capacity

	// declare but not define
	BL_NO_COPY_ALLOW(SplineBase);

	void Clear_() {
		if (waypoints_) {
			blFree(waypoints_);
			waypoints_ = NULL;
		}
		timeStamps_ = NULL;
		allocSize_ = numWaypoints_ = 0;
	}

	uint32 FindStartWaypoint_(float& alpha, float t) const {
		BL_ASSERT(2<=numWaypoints_);
		if (timeStamps_) {
			if (t<=timeStamps_[0]) {
				alpha = 0.0f;
				return 0;
			}
			else if (t>=timeStamps_[numWaypoints_-1]) {
				alpha = 1.0f;
				return (uint32) (numWaypoints_ - 2);
			}
			
			// binary search
			uint32 key = 0;
			uint32 hi  = numWaypoints_;
			uint32 mid = 0;
			while (hi>(key+1)) {
				mid = (key+hi) >> 1;
				if (timeStamps_[mid]>t)
					hi = mid;
				else
					key = mid;
			}

			alpha = (t - timeStamps_[key])/(timeStamps_[key+1] - timeStamps_[key]);
			return key;
		}
		else {
			if (t<=0.0f) {
				alpha = 0.0f;
				return 0;
			}
			else if (t>=1.0f) {
				alpha = 1.0f;
				return (uint32) (numWaypoints_ - 2);
			}

			t *= ((float)numWaypoints_ - 1.0f);

			uint32 const key = (uint32) t;
			alpha = t - (float)key;
			return key;
		}
	}
	
	virtual bool Interpolate_(float* position, KeyType const& k0, KeyType const& k1, float t) const = 0;

protected:
	virtual ~SplineBase() { Clear_(); }

public:
	SplineBase():ISpline(),
		waypoints_(NULL),timeStamps_(NULL),numWaypoints_(0),allocSize_(0) {}

	// type
	SPLINE_TYPE Type() const { return sType_; }

	bool Init(KeyType const* points, uint32 nWaypoints, float const* time=NULL) {
		// Data errors
		if (NULL==points|| nWaypoints<2)
			return false;

		// to prevent realloc too often...
		uint32 const keys_size = nWaypoints*sizeof(KeyType);
		uint32 const required_size =  keys_size + ((NULL==time) ? 0:(nWaypoints*sizeof(float)));
		if (required_size>allocSize_) {
			Clear_();

			waypoints_ = (KeyType*) blMalloc(required_size);
			if (NULL==waypoints_)
				return false;

			allocSize_ = required_size;
		}

		//
		// parsing and loading data...
		//
		BL_ASSERT(waypoints_);

		// time
		if (time) {
			timeStamps_ = (float*) (waypoints_+nWaypoints);

			// time must be monotonic increasing...
			timeStamps_[0] = time[0];
			for (uint32 i=1; i<nWaypoints; ++i) {
				timeStamps_[i] = time[i];
				if (timeStamps_[i]<timeStamps_[i-1]) {
					Clear_();
					return false;
				}
			}
		}
		else {
			timeStamps_ = NULL;
		}

		// load keys
		numWaypoints_ = nWaypoints;
		std::memcpy(waypoints_, points, keys_size);

		return true;
	}

	// eval position
	bool Eval(float* value, float t) const {
		if (numWaypoints_<2 || NULL==waypoints_)
			return false;

		float alpha = 0.0f;
		uint32 const key = FindStartWaypoint_(alpha, t);
		
		return Interpolate_(value, waypoints_[key], waypoints_[key+1], alpha);
	}
};

//---------------------------------------------------------------------------
// *** 1D poly line(the special case) and the key
//---------------------------------------------------------------------------
struct PolyLine1DKey {
	float Value;
};

class PolyLine1D : public SplineBase<PolyLine1DKey>
{
	bool Interpolate_(float* position, 
					  PolyLine1DKey const& k0,
					  PolyLine1DKey const& k1, float t) const {
		position[0] = k0.Value*(1.0f-t) + k1.Value*t;
		return true;
	}

	BL_NO_COPY_ALLOW(PolyLine1D);

public:
	PolyLine1D():SplineBase<PolyLine1DKey>() {}
	~PolyLine1D() {}
};

//---------------------------------------------------------------------------
// *** 3D poly line(the special case) and the key
//---------------------------------------------------------------------------
struct PolyLine3DKey {
	Vector3 Value;
};

class PolyLine3D : public SplineBase<PolyLine3DKey>
{
	bool Interpolate_(float* value, PolyLine3DKey const& k0, PolyLine3DKey const& k1, float t) const {
		Vector3 const pos((1.0f-t)*k0.Value + t*k1.Value);
		value[0] = pos.x;
		value[1] = pos.y;
		value[2] = pos.z;
		return true;
	}

	// declare but not define
	BL_NO_COPY_ALLOW(PolyLine3D);

public:
	PolyLine3D():SplineBase<PolyLine3DKey>() {}
	~PolyLine3D() {}
};

//---------------------------------------------------------------------------
// *** 1D Bezier spline and the key(knot)
//---------------------------------------------------------------------------
struct BezierKnot1D {
	float In, Value, Out;
};

class BezierSpline1D : public SplineBase<BezierKnot1D>
{
	bool Interpolate_(float* value, BezierKnot1D const& k0, BezierKnot1D const& k1, float t) const;

	// declare but not define
	BL_NO_COPY_ALLOW(BezierSpline1D);

public:
	BezierSpline1D():SplineBase<BezierKnot1D>() {}
	~BezierSpline1D() {}
};


//---------------------------------------------------------------------------
// *** Bezier spline(3D) and the key(knot)
//---------------------------------------------------------------------------
struct BezierKnot3D {
	Vector3 In, Value, Out;
};

class BezierSpline3D : public SplineBase<BezierKnot3D>
{
	bool Interpolate_(float* value, BezierKnot3D const& k0, BezierKnot3D const& k1, float t) const;

	// declare but not define
	BL_NO_COPY_ALLOW(BezierSpline3D);

public:
	BezierSpline3D():SplineBase<BezierKnot3D>() {}
	~BezierSpline3D() {}
};

//---------------------------------------------------------------------------
// *** The Kochanek-Bartels Splines (a.k.a TCB(Tension/continous/Bias) Splines)
//  Tension:	How sharply does the curve bend? 
//	Continuity: The rate of speed change(Acceleration)
//	Bias:		Direction of the curve passing through
// see also 3ds max's TCB key
//---------------------------------------------------------------------------
struct TCBKey1D {
	float InTan, Value, OutTan;
	float EaseIn, EaseOut; // bias
};
class TCBSpline1D : public SplineBase<TCBKey1D>
{
	bool Interpolate_(float* value, TCBKey1D const& k0, TCBKey1D const& k1, float t) const;

	// declare but not define
	BL_NO_COPY_ALLOW(TCBSpline1D);

public:
	TCBSpline1D():SplineBase<TCBKey1D>() {}
	~TCBSpline1D() {}
};

struct TCBKey3D {
	Vector3 InTan, Value, OutTan;
	float EaseIn, EaseOut; // bias
};
class TCBSpline3D : public SplineBase<TCBKey3D>
{
	bool Interpolate_(float* value, TCBKey3D const& k0, TCBKey3D const& k1, float t) const;

	// declare but not define
	BL_NO_COPY_ALLOW(TCBSpline3D);

public:
	TCBSpline3D():SplineBase<TCBKey3D>() {}
	~TCBSpline3D() {}
};


// more splines to come...


}}} // end namespaces

#endif	// BL_SPLINE_H