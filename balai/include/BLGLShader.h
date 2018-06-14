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
 * @file	BLGLShader.h
 * @author	andre chen
 * @history	2012/01/09 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_OPENGL_SHADER_H
#define BL_OPENGL_SHADER_H

#include "BLShader.h"

//
// design thoughts :
//
// Unlike other graphics api. OpenGL Shader Language combine vertex and fragment
// shaders into a single execution unit - a glProgram. In nu engine, A ShaderEffect
// object play the same role as glProgram, but it's not intended to be inherited
// by design. I solve this design problem by use GLVertexShader as a proxy or
// a surrogate object for ShaderEffect object. GLVertexShader will bind the
// glProgram. Meanwhile, since it can not leave pixel shader as null. we will
// implement GLFragmentShader class to bind all samplers. and all uniform variables
// will be taken care by GLVertexShader.
//
// in short, all uniforms are taken care by GLVertexShader(plus glProgram),
// all samplers are taken care by GLFragmentShader.
//
//
// available 8 attributes(float, vec2, vec3, vec4, mat2, mat3, and mat4 only) :
//      attribute vec4  position;
//      attribute vec3  normal;
//      attribute vec4  tangent;
//      attribute mediump vec4 color;
//      attribute vec2  texcoord;
//      attribute vec4  weights; // skin weights
//      attribute vec4  indices; // skin indices
//      attribute float psize;   // point size, collide with "indices"
//

namespace mlabs { namespace balai { namespace graphics {

//-----------------------------------------------------------------------------
class GLProgram : public ShaderEffect
{
	BL_DECLARE_RTTI;
	BL_NO_COPY_ALLOW(GLProgram);

	int*   samplerLoc_;
    uint32 program_;

protected:
	~GLProgram();

public:
	explicit GLProgram(uint32 id):ShaderEffect(id),samplerLoc_(NULL),program_(0) {}

	// IShader
	bool BindConstant(shader::Constant const* sc, void const* data) const;

	// ShaderEffect
	bool OnSelected(ShaderEffect const* previousFX);

	// render comes shortly(implemention must call default implementation in the end)
	bool OnBegin();

	// init
    bool Init(uint32 program);
};

}}}

#endif