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
 * @file	BLMaterial.h
 * @author	andre chen
 * @history	2012/02/20 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_MATERIAL_H
#define BL_MATERIAL_H

#include "BLGraphics.h" // def RenderState

//
// material header
//
namespace mlabs { namespace balai { namespace fileio {
	struct MaterialAttribute {
		uint32	Name;			// name index to lookup name table or CRC if table N/A
		uint32  Technique;		// technique
		uint32	FVF;			// format of vertices to apply material
		uint32	DataSize;		// contain data size
		uint32	RenderPass;		// render pass
		uint32	TotalTextures;  // total textures used
		uint32	TotalParams;	// total shader parameters used(platform dependent)

		// render states
		uint8	AlphaTest;
		uint8	ZWiteDisable;
		uint8	TwoFaces;
		uint8	AlphaBlend;
	};
	BL_COMPILE_ASSERT(32==sizeof(MaterialAttribute), __material_attr_size_not_ok);
}}}

// what are materials? in what cases 2 different meshes share the same material?
namespace mlabs { namespace balai { namespace graphics {

// render pass predefined, pass 1<<4 - 1<<31 are determined by game code...
enum RENDER_PASS {
	RENDER_PASS_SETUP	   = 1,		// set up pass, eg. up-side-down world for reflection
	RENDER_PASS_OPAQUE	   = 1<<1,  // main opaque
	RENDER_PASS_ALPHABLEND = 1<<2,	// transparent pass(need distance sort)
	RENDER_PASS_ADDBLEND   = 1<<3,  // additive blending pass

	RENDER_PASS_PREDEFINED = 4
};

// material manipulate flags, i.e. global material overwrite
enum RENDER_OPTION {
	RENDER_OPTION_RENDER_STATE	= 1<<0,	// use render state
	RENDER_OPTION_NO_LIT		= 1<<1,
	RENDER_OPTION_TRANSPARENT	= 1<<2,	// transparent, degenerate alpha
	RENDER_OPTION_TEXCELL_FRAME	= 1<<3, // texture cell animation		


	// substitute textures
	RENDER_OPTION_TEXTURE		= 1<<29,
	RENDER_OPTION_TEXTURE2		= 1<<30,
	
	// skip drawing
	RENDER_OPTION_NO_DRAW		= 1<<31,	// no draw, how good is that?
};

// control materials...
struct DrawConfig {
	RenderState		state;		// render state
	uint32			flags;		// combination of MATERIAL_OPTIONs or 0
	uint32			texCell;	// texture cell id
	float			time;		// current time, for animated material, like uv...
	float			transparency_;
	union {
		uint32		uValue;
		float		fValue;
		Color		color;
		ITexture*	texture;
	} params;
	DrawConfig():state(),flags(0),texCell(0),time(0.0f),transparency_(0.0f) {}
};

//
// material interface
//
class IMaterial
{
	// an empty class

	// no copy allowed
	BL_NO_COPY_ALLOW(IMaterial);

protected:
	IMaterial() {}

public:
	virtual ~IMaterial() {}
	
	// release all resources if desired 
	virtual void Release() = 0;

	// commit to set as 'current material'
	virtual void Commit(DrawConfig const*) const = 0;

	// if alpha channel is enable, transparent pass must be rendered
	// after opaque pass, and in sequence from back to forth
	virtual bool IsTransparent() const = 0;

	// render pass
	virtual uint32 RenderPass() const = 0;
	virtual bool RenderPass(uint32 pass) const = 0;
};

}}}

#endif // BL_MATERIAL_H