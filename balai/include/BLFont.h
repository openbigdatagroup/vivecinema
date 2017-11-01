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
 * @file	BLFont.h
 * @author	andre chen
 * @history	2012/01/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_FONT_H
#define BL_FONT_H

#include "BLObject.h"
#include "BLVector3.h"

namespace mlabs { namespace balai { namespace graphics {
	
// forward declaration
struct Color;
class ITexture;
class Primitives;

// font alignment
enum {
    FONT_ALIGN_LEFT_TOP   = 0x00000000,
    FONT_ALIGN_RIGHT      = 0x00000001,
    FONT_ALIGN_BOTTOM     = 0x00000002,
    FONT_ALIGN_CENTERED_X = 0x00000004,
    FONT_ALIGN_CENTERED_Y = 0x00000008,
	FONT_ALIGN_CENTER     = FONT_ALIGN_CENTERED_X|FONT_ALIGN_CENTERED_Y,
	FONT_ALIGN_RIGHT_BOTTOM = FONT_ALIGN_RIGHT|FONT_ALIGN_BOTTOM,
};

//-----------------------------------------------------------------------------
// font
class IFont : public IResource
{
	BL_DECLARE_RTTI;
	BL_NO_DEFAULT_CTOR(IFont);

protected:
	IFont(uint32 id, uint32 group):IResource(id, group) {}
	virtual ~IFont() {}

public:
	// (x, y) : (0,0) --- (1,0)
	//            |         |
	//          (1,0) --- (1,1)
	// return height(screen space) of the text region
	virtual float DrawText(float x, float y, Color const& color, char const* text, uint32 align=FONT_ALIGN_LEFT_TOP) = 0;
	virtual float DrawText(float x, float y, uint32 size, Color const& color, char const* text, uint32 align=FONT_ALIGN_LEFT_TOP) = 0;
};

//-----------------------------------------------------------------------------
// texture based font for ASCII only
class IAsciiFont : public IFont
{
	BL_DECLARE_RTTI;
	BL_NO_COPY_ALLOW(IAsciiFont);
	BL_NO_DEFAULT_CTOR(IAsciiFont);

	virtual void BeginDraw_(Primitives&, bool screenspace) const;
	virtual void EndDraw_(Primitives&) const;

protected:
	float		texCoords_[128-32][4];
	ITexture*	texture_;
	uint32		fontHeight_;
    uint32		texSize_;     // Texture dimensions(width and height)

	IAsciiFont(uint32 id, uint32 group);
	virtual ~IAsciiFont() = 0;

public:
	virtual float DrawText(float x, float y, uint32 size, Color const& color, char const* text, uint32 align=FONT_ALIGN_LEFT_TOP);
	float DrawText(float x, float y, Color const& color, char const* text, uint32 align=FONT_ALIGN_LEFT_TOP) {
		return DrawText(x, y, fontHeight_, color, text, align);
	}

	// draw in 3D world
	//
	//      up
	//     /|
	//      | height = up.Norm()
	//      |
	//    center ---> right (reference direction)
	//
	bool BeginDraw3D(Primitives&, bool depthTest=true, bool depthMask=false) const;
	bool DrawCharacter(Primitives&,
					  char character,
					  math::Vector3 const& center,
					  math::Vector3 const& right,
					  math::Vector3 const& up) const;
	void EndDraw(Primitives&) const;

	// get texture coordinate
	bool GetTexcoord(char c, float& s0, float& t0, float& s1, float& t1) const;

    // factory
	static IAsciiFont* CreateFontFromFile(char const* filename);
};

}}}

#endif