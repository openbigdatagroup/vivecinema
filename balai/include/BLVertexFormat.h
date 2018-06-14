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
 * @file	BLVertexFormat.cpp
 * @author	andre chen
 * @history	2012/01/05 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_VERTEX_FORMAT_H
#define BL_VERTEX_FORMAT_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace graphics {

#ifdef BL_OS_PS3
/********************************************************************************
 *			SCE_VP_RSX Varying Input Semantics									*
 *	Semantics							Corresponding Data(in Cg)				*
 *	POSITION, ATTR0						vertex Position (float4)				*
 *	BLENDWEIGHT, ATTR1					vertex weight (float4)					*
 *	NORMAL, ATTR2						normal (float3)							*
 *	COLOR, COLOR0, DIFFUSE, ATTR3		color (float4)							*
 *	COLOR1, SPECULAR, ATTR4				secondary color (float4)				*
 *	FOGCOORD, TESSFACTOR, ATTR5			fog coordinate (float)					*
 *	PSIZE, ATTR6						point size (float)						*
 *	BLENDINDICES, ATTR7					palette indices for skinning (float4)	*
 *	TEXCOORD0, ATTR8					texture coordinates#0 (float4)			*
 *  TEXCOORD1, ATTR9					texture coordinates#1 (float4)			*
 *  TEXCOORD2, ATTR10					texture coordinates#2 (float4)			*
 *  TEXCOORD3, ATTR11					texture coordinates#3 (float4)			*
 *  TEXCOORD4, ATTR12					texture coordinates#4 (float4)			*
 *  TEXCOORD5, ATTR13					texture coordinates#5 (float4)			*
 *  TEXCOORD6, ATTR14					texture coordinates#6 (float4)			*
 *  TEXCOORD7, ATTR15					texture coordinates#7 (float4)			*
 *	TANGENT, ATTR14						tangent vector (float4)					*
 *	BINORMAL, ATTR15					binomial vector (float4)				*
 ********************************************************************************/

//
// the problem is - how can i fix the attribute-collision beautifully?
//
enum GCM_CG_VERTEX_ATTRIBUTE_SLOT {
	GCM_CG_VERTEX_ATTRIBUTE_XYZ		  = 0,
	GCM_CG_VERTEX_ATTRIBUTE_XYZW	  = GCM_CG_VERTEX_ATTRIBUTE_XYZ, // collision!
	
	// normal/tangent/binormal
	GCM_CG_VERTEX_ATTRIBUTE_NORMAL	  = 2,
	GCM_CG_VERTEX_ATTRIBUTE_TANGENT	  = 14,
	GCM_CG_VERTEX_ATTRIBUTE_BINORMAL  = 15,
	
	// 2 colors
	GCM_CG_VERTEX_ATTRIBUTE_DIFFUSE	  = 3,
	GCM_CG_VERTEX_ATTRIBUTE_COLOR	  = GCM_CG_VERTEX_ATTRIBUTE_DIFFUSE,
	GCM_CG_VERTEX_ATTRIBUTE_COLOR0	  = GCM_CG_VERTEX_ATTRIBUTE_DIFFUSE,
//	GCM_CG_VERTEX_ATTRIBUTE_COLOR1	  = 4, // model does not have specular color, encoded in texture map
	
	// skin info
	GCM_CG_VERTEX_ATTRIBUTE_WEIGHTS	  = 1, // type = CELL_GCM_VERTEX_UB, 8-bit unsigned integer normalized to [0, 1]
	GCM_CG_VERTEX_ATTRIBUTE_INDICES	  = 7, // type = CELL_GCM_VERTEX_UB256, 8-bit unsigned integer without normalization

	// point size
	GCM_CG_VERTEX_ATTRIBUTE_POINTSIZE = 6,

	// fog coord
	GCM_CG_VERTEX_ATTRIBUTE_FOGCOORD  = 5,

	// texcoords
	GCM_CG_VERTEX_ATTRIBUTE_TEXCOORD0 = 8,
	GCM_CG_VERTEX_ATTRIBUTE_TEXCOORD1 = 9,
	GCM_CG_VERTEX_ATTRIBUTE_TEXCOORD2 = 10,
	GCM_CG_VERTEX_ATTRIBUTE_TEXCOORD3 = 11,
};
#elif defined(BL_RENDERER_OPENGL)


enum GL_VERTEX_ATTRIBUTE_SLOT {
    // position, either XYZ or XYZW
    GL_VERTEX_ATTRIBUTE_POSITION  = 0,  // attribute vec4 position;
        
    // normal/tangent/binormal
    GL_VERTEX_ATTRIBUTE_NORMAL	  = 1,  // attribute vec3 normal;
    GL_VERTEX_ATTRIBUTE_TANGENT	  = 2,  // attribute vec4 tangent; .w = flip
        
    // 1 color
    GL_VERTEX_ATTRIBUTE_COLOR	  = 3,  // attribute mediump vec4 color;
        
    // 1 texcoord
    GL_VERTEX_ATTRIBUTE_TEXCOORD  = 4,  // attribute vec2 texcoord;
    
    // skin info
    GL_VERTEX_ATTRIBUTE_INDICES	  = 5,  // attribute vec4 indices; skin indices, 8-bit unsigned integer without normalization
    GL_VERTEX_ATTRIBUTE_WEIGHTS	  = 6,  // attribute vec4 weights; skin weights, 8-bit unsigned integer normalized to [0, 1]

    // point size
    GL_VERTEX_ATTRIBUTE_POINTSIZE = 7,  // attribute float psize; point size
        
    // fog coord - i don't see any fog in the OpenGL ES 2.0.25 spec
//  GL_VERTEX_ATTRIBUTE_FOGCOORD  = 7,
    
    // note the minimum requirement : const mediump int gl_MaxVertexAttribs = 8;
    GL_VERTEX_ATTRIBUTE_MAXIMUM   = 8
};
#endif

// vertex attribute
enum VERTEX_ATTRIBUTE_FORMAT {
	// xyz(3 floats)/xyzw(4 floats)
	VERTEX_ATTRIBUTE_XYZ			= 0,
	VERTEX_ATTRIBUTE_XYZW			= 0x80000000L,
	
	// normal/tangent/bitangent
	VERTEX_ATTRIBUTE_NORMAL			= (1<<0),
	VERTEX_ATTRIBUTE_TANGENT		= (1<<1),
	VERTEX_ATTRIBUTE_BINORMAL		= (1<<2),   // bitangent vector may be calculated in vertex shader(D3D9)
												// or be stored in vertex(PS3)
	// diffuse color
	VERTEX_ATTRIBUTE_COLOR			= (1<<3),	// if (!COLOR) R=G=B = 255
	VERTEX_ATTRIBUTE_ALPHA			= (1<<4),	// if (!ALPHA) A = 255
	VERTEX_ATTRIBUTE_DIFFUSE		= VERTEX_ATTRIBUTE_COLOR|VERTEX_ATTRIBUTE_ALPHA,

	// skin info(4 indices + 4 weights)
	VERTEX_ATTRIBUTE_SKININFO		= (1<<5),

	// point size(1 float)
	VERTEX_ATTRIBUTE_POINTSIZE		= (1<<6),

	// fog coord(1 float)
	VERTEX_ATTRIBUTE_FOGCOORD		= (1<<7),

	// texcoords sets
	VERTEX_ATTRIBUTE_TEXCOORD       = (1<<8),
	VERTEX_ATTRIBUTE_TEXCOORD0		= VERTEX_ATTRIBUTE_TEXCOORD,
	VERTEX_ATTRIBUTE_TEXCOORD1      = (1<<9),
	VERTEX_ATTRIBUTE_TEXCOORD_LIMIT = 2,
	
	// morph coordinate(position offset)
	VERTEX_ATTRIBUTE_BLENDSHAPE_SHIFT = 16,
	VERTEX_ATTRIBUTE_BLENDSHAPE_MASK  = (0x0f)<<VERTEX_ATTRIBUTE_BLENDSHAPE_SHIFT,
	VERTEX_ATTRIBUTE_BLENDSHAPE0	  = (1<<VERTEX_ATTRIBUTE_BLENDSHAPE_SHIFT),
	VERTEX_ATTRIBUTE_BLENDSHAPE_LIMIT = 8,
};
#define VERTEX_ATTRIBUTE_BLENDSHAPES(n)	(n<<VERTEX_ATTRIBUTE_BLENDSHAPE_SHIFT)

inline uint32 HasWCoordinate(uint32 flags)	{ return (VERTEX_ATTRIBUTE_XYZW&flags); }
inline uint32 HasNormal(uint32 flags)		{ return (VERTEX_ATTRIBUTE_NORMAL&flags); }
inline uint32 HasTangent(uint32 flags)		{ return (VERTEX_ATTRIBUTE_TANGENT&flags); }
inline uint32 HasBinormal(uint32 flags)		{ return (VERTEX_ATTRIBUTE_BINORMAL&flags); }
inline uint32 HasVertexColor(uint32 flags)	{ return (VERTEX_ATTRIBUTE_DIFFUSE&flags); }
inline uint32 HasVertexAlpha(uint32 flags)	{ return (VERTEX_ATTRIBUTE_ALPHA&flags); }
inline uint32 HasSkinInfo(uint32 flags)		{ return (VERTEX_ATTRIBUTE_SKININFO&flags); }
inline uint32 TotalTexCoords(uint32 flags)  {
	if (flags&VERTEX_ATTRIBUTE_TEXCOORD) {
		return (0==(flags&VERTEX_ATTRIBUTE_TEXCOORD1)) ? 1:2;
	}
	return 0;
}
inline uint32 TotalBlendShapes(uint32 flags) {
	uint32 const targets = (VERTEX_ATTRIBUTE_BLENDSHAPE_MASK&flags)>>VERTEX_ATTRIBUTE_BLENDSHAPE_SHIFT;
	if (targets>VERTEX_ATTRIBUTE_BLENDSHAPE_LIMIT)
		return VERTEX_ATTRIBUTE_BLENDSHAPE_LIMIT;
	else
		return targets;
}
inline uint32 TotalAttributes(uint32 flags) {
	uint32 attr = 1; // must have position
	if (VERTEX_ATTRIBUTE_NORMAL&flags)	++attr;
	if (VERTEX_ATTRIBUTE_TANGENT&flags)	++attr;
	if (VERTEX_ATTRIBUTE_BINORMAL&flags) ++attr;
	if (VERTEX_ATTRIBUTE_DIFFUSE&flags) ++attr;
	if (VERTEX_ATTRIBUTE_SKININFO&flags) attr+=2;
	if (VERTEX_ATTRIBUTE_POINTSIZE&flags) ++attr;
	return attr+TotalBlendShapes(flags)+TotalTexCoords(flags);
}
inline uint32 TranslateFVF(char* buf, uint32 flags) {
	buf[0] = '_'; buf[1] = 'v';	// v for vertex position
	uint32 c = 2;
	if (VERTEX_ATTRIBUTE_NORMAL&flags) {
		buf[c++] = '_'; buf[c++] = 'n'; // n for normal
	}
	if (VERTEX_ATTRIBUTE_TANGENT&flags) {
		buf[c++] = '_'; buf[c++] = 't';	// t for tangent
	}
	if (VERTEX_ATTRIBUTE_BINORMAL&flags) {
		buf[c++] = '_'; buf[c++] = 'b';	// b for bitangent
	}
	if (VERTEX_ATTRIBUTE_DIFFUSE&flags) {
		buf[c++] = '_'; buf[c++] = 'c'; // c for vertex color
	}
	if (VERTEX_ATTRIBUTE_SKININFO&flags) {
		buf[c++] = '_'; buf[c++] = 's'; // s for skin info
	}
	if (VERTEX_ATTRIBUTE_POINTSIZE&flags) {
		buf[c++] = '_'; buf[c++] = 'p'; // p for point size
	}
	uint8 const mt = (uint8) TotalBlendShapes(flags); 
	if (mt) {
		buf[c++] = '_'; buf[c++] = 'm'; buf[c++] = 't'; buf[c++] = '0'+mt; // mt1 - mt8 for # of morph targets
	}	
	uint8 const tc = (uint8) TotalTexCoords(flags);
	if (tc) {
		buf[c++] = '_'; buf[c++] = 't'; buf[c++] = 'c'; buf[c++] = '0'+tc; // tc1 - tc8 for # of texcoord sets
	}
	buf[c] = '\0';
	return c;
}

}}}

#endif // BL_VERTEX_FORMAT_H