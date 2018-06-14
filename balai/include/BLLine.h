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
 * @file	BLLine.h
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_LINE_H
#define BL_LINE_H

#include "BLVector3.h"

namespace mlabs { namespace balai { namespace math {

class Line3
{
	Vector3	start_;
	Vector3	direction_;
	float	length_;
	
	Line3() {} // uninitialized, private!

public:
    // The line is represented as P+t*D where P is the line origin and D is
    // a unit-length direction vector.  The user must ensure that the
    // direction vector satisfies this condition.

    // construction
    Line3(Vector3 const& start, Vector3 const& end):start_(start),direction_(end-start) {
        direction_.Normalize(&length_);
    }
	void SetEndPoints(Vector3 const& start, Vector3 const& end) {
		start_     = start;
	    direction_ = end - start;
	    direction_.Normalize(&length_);
	}
	
	// accessors
	Vector3 const& Start() const { return start_; }
	Vector3 const& Direction() const { return direction_; }
	float Length() const { return length_; }
	Vector3 const End() const {
		return start_ + (length_*direction_);
	}

	// s [0, 1]
	Vector3 const& Eval(float s, Vector3& v) const {
		v = start_ + (s*length_*direction_);
		return v;
	}
	Vector3 const Eval(float s) const {
		Vector3 v(start_);
		return v+=(s*length_*direction_);
	}
};
// alias
typedef Line3 Line;

}}}

#endif