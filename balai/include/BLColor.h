/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2012 HTC Corporation
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
 * @file	BLColor.h
 * @desc    color
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_COLOR_H
#define BL_COLOR_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace graphics {

struct Color
{
	static Color const Zero, Black, Gray, White, Red, Green, Blue, Yellow, Cyan, Purple, Pink;
	uint8 r, g, b, a;
	
	// platform dependent
	operator uint32() const;

	// pick up a random color
	Color const& Random(bool transparent=false);

	// linear interpolation between a(lerp=0.0f) and b(lerp=1.0f)
	Color const& SetLerp(Color const& clr0, Color const& clr1, float lerp);
};

//-----------------------------------------------------------------------------
inline Color const operator*(float s, Color color)
{
	color.r = (uint8)(s*color.r);
	color.g = (uint8)(s*color.g);
	color.b = (uint8)(s*color.b);
	color.a = (uint8)(s*color.a);
	return color;
}
//-----------------------------------------------------------------------------
inline Color const operator+(Color lhs, Color const& rhs) 
{
	lhs.r = (uint8)(lhs.r + rhs.r);
	lhs.g = (uint8)(lhs.g + rhs.g);
	lhs.b = (uint8)(lhs.b + rhs.b);
	lhs.a = (uint8)(lhs.a + rhs.a);
	return lhs;
}
//-----------------------------------------------------------------------------
inline Color const ColorLerp(Color clr1, Color const& clr2, float t)
{
	float t_ = 1.0f - t;
	clr1.r = (uint8)(t_*clr1.r + t*clr2.r);
	clr1.g = (uint8)(t_*clr1.g + t*clr2.g);
	clr1.b = (uint8)(t_*clr1.b + t*clr2.b);
	clr1.a = (uint8)(t_*clr1.a + t*clr2.a);
	return clr1;
}

}}}

#endif // BL_COLOR_H