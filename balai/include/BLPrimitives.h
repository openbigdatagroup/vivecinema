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
 * @file	BLPrimitiveRenderer.h
 * @desc    primitive renderer
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_PRIMITIVE_RENDERER_H
#define BL_PRIMITIVE_RENDERER_H

#include "BLGraphics.h"

namespace mlabs { namespace balai { namespace graphics {

enum GFXPRIMITIVE_TYPE {
	GFXPT_NONE                 = 0x00,
	GFXPT_POINTSPRITE   	   = 0x01,
	GFXPT_LINELIST			   = 0x02,
	GFXPT_LINESTRIP			   = 0x03,
	GFXPT_TRIANGLELIST		   = 0x04,
	GFXPT_TRIANGLESTRIP		   = 0x05,
	GFXPT_TRIANGLEFAN		   = 0x06,
	GFXPT_QUADLIST			   = 0x07,
	GFXPT_SPRITE			   = 0x08,
	GFXPT_TOTALS               = 0x09,
	GFXPT_3D_SPACE_MASK		   = 0x0f,	// not a valid type
	GFXPT_SCREEN_SPACE  	   = 0x10,	// not a valid type
	GFXPT_SCREEN_LINELIST	   = 0x12,
	GFXPT_SCREEN_LINESTRIP	   = 0x13,
	GFXPT_SCREEN_TRIANGLELIST  = 0x14,
	GFXPT_SCREEN_TRIANGLESTRIP = 0x15,
	GFXPT_SCREEN_TRIANGLEFAN   = 0x16,
	GFXPT_SCREEN_QUADLIST	   = 0x17,
	GFXPT_SCREEN_SPRITE		   = 0x18,
};

// for performance, Primitives is a concrete class
class Primitives
{
	BL_NO_COPY_ALLOW(Primitives);

	// inner structure
	struct SPRITE { // 20B
        float x, y, z;
        Color c;
        float s;
    };
    struct VERTEX { // 24B
        float x, y, z;
        Color c;
        float s, t;
    };

	union {
		VERTEX*	vertexPtr_;
		SPRITE*	spritePtr_;
    };

	GFXPRIMITIVE_TYPE type_;
	float losX_, losY_, losZ_, losW_; // line of sight - vector use to do camera front testing
	Color color_;         // current color
	union {
		float s_;
		float size_;
	};
	float t_;         // current texcoord(if sprite, s_ is the size)
    uint32 vcount_;       // vertex count
    uint32 const vlimit_; // vertex limit
	uint32 const slimit_; // sprite limit

	// private ctor/dtor
    Primitives();
	~Primitives() {}

    void Flush_();

public:
    void SetColor(uint8 red, uint8 green, uint8 blue, uint8 alpha=255) {
        color_.r = red; color_.g = green; color_.b = blue; color_.a = alpha;
    }
	void SetColor(Color const& color) { color_=color; }
	void SetTexcoord(float s, float t) { s_=s; t_=t; }
	
	// use default ShaderEffect to draw textured primitives(lighting off)
	bool BeginDraw(ITexture const* tex, GFXPRIMITIVE_TYPE type);

	// you need to setup Shader/Texture/Parameters after making this call
	bool BeginDraw(GFXPRIMITIVE_TYPE type);
	
	// remember to call EndDraw() for every BeginDraw() succeed call
	void EndDraw();

	// 2d vertex :
	//	(0,0)----(1,0)
	//	  |        |
	//	(0,1)----(1,1)
	uint32 AddVertex2D(float x, float y);
	uint32 AddVertex2D(float x, float y, float s, float t) {
		s_=s; t_=t;
		return AddVertex2D(x, y);
	}
	
	// 3d vertex, x->right, y->front, z->up
	uint32 AddVertex(float x, float y, float z);
	uint32 AddVertex(float x, float y, float z, float s, float t) {
		s_=s; t_=t;
		return AddVertex(x, y, z);
	}
	uint32 AddVertex(math::Vector3 const& v) {
		return AddVertex(v.x, v.y, v.z);
	}
	uint32 AddVertex(math::Vector3 const& v, float s, float t) {
		s_=s; t_=t;
		return AddVertex(v.x, v.y, v.z);
	}

	// a point sprite
	uint32 AddPointSprite(float x, float y, float z, float size);
	uint32 AddPointSprite(math::Vector3 const& v, Color const& color, float size) {
		color_ = color;
		return AddPointSprite(v.x, v.y, v.z, size);
	}

    // helper - draw a fullscreen quad
	// TBD : make it quicker....
	bool DrawFullScreenQuad(ITexture const* tex, Color const& color=Color::White) {
		if (BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
            color_ = color;
			s_ = 0.0f; t_ = 0.0f;
			AddVertex2D(0.0f, 0.0f); // TBD : make it faster!....

			s_ = 0.0f; t_ = 1.0f;
			AddVertex2D(0.0f, 1.0f); // TBD : make it faster!....

			s_ = 1.0f; t_ = 1.0f;
			AddVertex2D(1.0f, 1.0f); // TBD : make it faster!....
			
			s_ = 1.0f; t_ = 0.0f;
			AddVertex2D(1.0f, 0.0f); // TBD : make it faster!....

			EndDraw();
			return true;
		}
		return false;	
    }

	// reset every frame
	void Reset();

	// initialize/finalize
	bool Initialize();
	void Finalize();

	// singleton
	static Primitives& GetInstance() {
		static Primitives inst_;
		return inst_;
	}
};

//
// Draw bounding box
template<typename BOX>
inline void gfxDebugDrawBox(BOX const& abox, Color const& color) {
	uint16 const indices[16] = { 0, 1, 3, 2, 0, 4, 5, 7, 6, 4, 5, 1, 3, 7, 6, 2 };
	math::Vector3 verts[8]; 
	uint32 nVerts = abox.EvalVertices(verts);
	if (8==nVerts)
		nVerts = 16;
	else if (4==nVerts)
		nVerts = 5;
	else if (2!=nVerts)
		return;

	Renderer& renderer_ = Renderer::GetInstance();
	renderer_.SetWorldMatrix(math::Matrix3::Identity);

	Primitives& prim = Primitives::GetInstance();
	prim.BeginDraw(NULL, GFXPT_LINESTRIP);
	prim.SetColor(color);
	for (uint32 i=0; i<nVerts; ++i) {
		prim.AddVertex(verts[indices[i]]);
	}
	prim.EndDraw();
}

}}} // namespace mlabs::balai::graphics

#endif // BL_PRIMITIVE_RENDERER_H