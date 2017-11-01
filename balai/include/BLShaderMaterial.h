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
 * @file	BLShaderMaterial.h
 * @author	andre chen
 * @history	2012/02/23 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_SHADER_MATERIAL_H
#define BL_SHADER_MATERIAL_H

#include "BLMaterial.h"
#include "BLGraphics.h" // RenderState
#include "BLStreamFWD.h"

namespace mlabs { namespace balai { namespace graphics {

// forward declarations
class ITexture;
class ShaderEffect;

// material - in what cases, the material is shared? i mean,
// 2 models share the same material(including textures)?
// material is not a resource, hold by Mesh, which is a resource
class ShaderMaterial : public IMaterial
{
	// glues...
	struct ShaderConstant {
		shader::Constant const* shaderConst;
		shader::Constant4f*		values;
		ShaderConstant():shaderConst(NULL),values(NULL) {}
	};
	struct ShaderSampler {
		shader::Sampler const* sampler;
		ITexture*			   texture;
		ShaderSampler():sampler(NULL),texture(NULL) {}
	};
	struct TexCoordAnimReg {
		shader::Constant4f* value;
		float				du;
		float				dv;
		uint16				row;
		uint16				col;
	};

	ShaderEffect*		effect_;	// technique/shader
	shader::Constant4f*	allocBuf_;	// alloc buffer
	TexCoordAnimReg*	texCoordAnimConstans_;	// size = 1 or 0
	ShaderConstant*		shaderConstants_;
	ShaderSampler*		samplerStages_;
	RenderState			rstate_;	// render state
	uint32				renderPass_; // render pass
	uint16				totalShaderConsts_;
	uint16				totalSamplerStages_;

public:
	ShaderMaterial():effect_(NULL),
		allocBuf_(NULL),texCoordAnimConstans_(NULL),shaderConstants_(NULL),samplerStages_(NULL),
		rstate_(),renderPass_(0),
		totalShaderConsts_(0),totalSamplerStages_(0) {}
	~ShaderMaterial();

	// render pass
	uint32 RenderPass() const { return renderPass_; }
	bool RenderPass(uint32 pass) const { return (uint32)(1<<pass)==renderPass_; }

	// set shader constants, the slower way.
	int SetShaderConstant(uint32 semantic, void const* src, uint32 count) const {
		return (NULL!=effect_) ? effect_->SetConstant(semantic, src, count) : -1;
	}

	// create from tream
	bool CreateFromStream(fileio::ifstream& stream,
						  fileio::MaterialAttribute const& attr,
						  char const* stringPool);
	
	// release all resources
	void Release();

	// commit
	void Commit(DrawConfig const*) const;

	// is transparent pass?
	bool IsTransparent() const { return rstate_.IsAlphaBlendEnable(); }
};
//BL_COMPILE_ASSERT(36==sizeof(ShaderMaterial), __ShaderMaterial_size_not_OK);

}}} // namespace mlabs::balai::graphics

#endif // BL_SHADER_MATERIAL_H