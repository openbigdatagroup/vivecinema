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
 * @file	BLShader.h
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_SHADER_H
#define BL_SHADER_H

#include "BLObject.h"
#include "BLResourceManager.h"
#include "BLStringPool.h"

namespace mlabs { namespace balai { namespace graphics {

// forward declarations
class ITexture;

namespace shader {
	class  Semantic;
	struct Constant;
	struct Sampler;
	
	//
	// semantics are the "name" of used shader paramters, when you write a
	// new shader code, you must use following semantics as paramters only!
	//
	namespace semantics {
		// weights
		uint32 const weight0123			= CalcCRC("weight0123");
		uint32 const weight4567			= CalcCRC("weight4567");

		// transforms
		uint32 const matBones			= CalcCRC("matBones");
		uint32 const matWorld			= CalcCRC("matWorld");
		uint32 const matInvWorld		= CalcCRC("matInvWorld");
		uint32 const matView			= CalcCRC("matView");
		uint32 const matInvView			= CalcCRC("matInvView");
		uint32 const matProj			= CalcCRC("matProj");
		uint32 const matWorldView		= CalcCRC("matWorldView");
		uint32 const matInvWorldView	= CalcCRC("matWorldView");
		uint32 const matViewProj		= CalcCRC("matViewProj");
		uint32 const matWorldViewProj	= CalcCRC("matWorldViewProj");

		// materials
		uint32 const ambient			= CalcCRC("ambient");
		uint32 const diffuse			= CalcCRC("diffuse");
		uint32 const specular			= CalcCRC("specular"); // .w = shininesss
		uint32 const emissive			= CalcCRC("emissive");
		uint32 const uvOffset			= CalcCRC("uvOffset");
		uint32 const bumpy_fresnel_reflection = CalcCRC("bumpy_fresnel_reflection");

		// texture maps
		uint32 const diffuseMap			= CalcCRC("diffuseMap");
		uint32 const normalMap			= CalcCRC("normalMap");
		uint32 const reflectionMap		= CalcCRC("reflectionMap"); // static environment cubemap
		uint32 const envMap				= CalcCRC("envMap");
		uint32 const randomMap			= CalcCRC("randomMap"); // image processing
		uint32 const lookupMap			= CalcCRC("lookupMap"); // image processing
		uint32 const sourceMap			= CalcCRC("sourceMap"); // image processing, the source image
		uint32 const proceduralMap		= CalcCRC("proceduralMap"); // image processing, to render from one to another
		
		// multi-tap offset, likely, +/- { 0.5f/w, 0.5f/h, 1.0f/w, 1.0/h }, free to be interpreted.
		uint32 const tapCoeffs          = CalcCRC("tapCoeffs");
		uint32 const tapCoeffs2         = CalcCRC("tapCoeffs2");

        // vectors...
        uint32 const eyePosition        = CalcCRC("eyePosition"); // eye position in world space

		// lights...
		uint32 const lightObjectDir		= CalcCRC("lightObjectDir");
		uint32 const lightWorldDir		= CalcCRC("lightWorldDir");
	} // namespace shader::semantics
} // namespace shader


//- shader --------------------------------------------------------------------
class IShader : public IResource
{
	BL_DECLARE_RTTI;

	// disable default/copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(IShader);
	BL_NO_COPY_ALLOW(IShader);

protected:
	//
	//			VS constants	PS constants	VS Texture		PS Texture
	//  PS3			468*			**			   4			   16
	//
	//  D3D9	vs_2_0 : 256	ps_2_0 : 32		vs_2_0 : 0			8***
	//			vs_3_0 : 256	ps_3_0 : 224	vs_3_0 : 4
	//
	//  GL ES
	//
	//
	//  *   by default, register c256 through c467 are available for allocation by compiler.
	//		Registers c0 through c255 are designated as shared, and the compiler will only use
	//      them if they are specifiec with Cg semantics. this default boundary between shared
	//		and compiler-allocated registers can be modified with the -firstallocreg option.
	//  **	the fragment shader does not have a register for retaining constants. To input a
	//		constant, the field for specifying constants in the instruction is used.
	//	***	depend on D3DCAPS9::MaxSimultaneousTextures and D3DCAPS9::MaxTextureBlendStages
	//
	shader::Constant* constants_;
	shader::Sampler*  samplers_;
	uint32            totalConstants_;
	uint32            totalSamplers_;

	//
	// add some special constants, for examples -
	//   matInvWorld, matView, matInvView, matProj, matWorldView, matInvWorldView, matViewProj, matWorldViewProj or eyePosition?
	//
	explicit IShader(uint32 id):IResource(id, 0),constants_(NULL),samplers_(NULL),totalConstants_(0),totalSamplers_(0) {}
	virtual ~IShader() { Clear(); }
	void Clear() {
		if (constants_) {
			blFree(constants_);
			constants_ = NULL;
		}
		else if (samplers_) {
			blFree(samplers_);
		}
		samplers_ = NULL;
		totalConstants_ = totalSamplers_ = 0;
	}

#ifdef BL_DEBUG_BUILD
    void DebugCheck_() const;
#endif

public:
	// vertex shader constants and samplers
	uint32 GetTotalConstants() const { return totalConstants_; }
	uint32 GetTotalSamplers() const  { return totalSamplers_; }

	// find constants
	shader::Constant const* FindConstant(uint32 semantic) const;
	shader::Constant const* FindConstant(char const* semantic) const {
		return FindConstant(CalcCRC(semantic));
	}
	
	// find samplers
	shader::Sampler const* FindSampler(uint32 semantic) const;
	shader::Sampler const* FindSampler(char const* semantic) const {
		return FindSampler(CalcCRC(semantic));
	}

	// bind shader constant/texture
	virtual bool BindConstant(shader::Constant const* sc, void const* data) const = 0;
	bool BindSampler(shader::Sampler const* ss, ITexture const* tex) const;

	// slow way...
	uint32 SetConstant(uint32 semantic, void const* data, uint32 count=0) const;
	bool SetSampler(uint32 semantic, ITexture const* tex) const {
        return BindSampler(FindSampler(semantic), tex);
	}
};

//- shader effect -------------------------------------------------------------
class ShaderEffect : public IShader
{
	BL_DECLARE_RTTI;
	BL_NO_DEFAULT_CTOR(ShaderEffect);
	BL_NO_COPY_ALLOW(ShaderEffect);

protected:
	virtual ~ShaderEffect() {} // use Release()

public:
	explicit ShaderEffect(uint32 id):IShader(id) {}

	// being selected(implemention must call default implementation in the end)
	virtual bool OnSelected(ShaderEffect const* /*previousFX*/) = 0;

	// render comes shortly(implemention must call default implementation in the end)
	virtual bool OnBegin() = 0;

	// to cleanup
	virtual void OnEnd() {}
};

//-- Shader manager -----------------------------------------------------------
class ShaderManager : public ResourceManager<ShaderEffect, 128>
{
protected:
#ifdef BL_DEBUG_NAME
	StringPool stringPool_;
	ShaderManager():stringPool_(1024, 4096) {}
	~ShaderManager() {}
public:
    char const* GetEffectName(ShaderEffect const* fx) {
		return (NULL==fx) ? NULL:stringPool_.Find(fx->Name());
	}
	friend class shader::Semantic;
#else
	ShaderManager() {}
	~ShaderManager() {}
#endif

public:
	static ShaderManager& GetInstance();

	//
	// create/delete core(common) shaders 
	//
	virtual uint32 Initialize(void* mem=NULL) = 0;
	virtual void Finalize() { ReleaseAll(); }

	// resolve effect
	virtual ShaderEffect* ResolveEffect(char const* effect, char const* technique, uint32 fvf) const;

	// create/delete game(application) shader from media
	// load a shader package file, a file contains many compiled shaders
//	virtual bool CreateShaders(char const* shaderFile) = 0; 

	// delete a shader by its name?
//	bool DeleteShader(char const* name) { return UnregisterByName(StringPool::CalcCRC(name)); }

	ShaderEffect* FindEffect(char const* name) const {
		return (NULL!=name) ? Find(HashName(name)):NULL;
	}
	static uint32 HashName(char const* name) { return StringPool::HashString(name); }
};

// shader parameter semantic, constant and sampler
// (these definitions need ShaderManager, so it appears such late)
namespace shader {
	class Semantic {
		uint32 name_;	// crc
	public:
		Semantic():name_(BL_BAD_UINT32_VALUE) {}
		explicit Semantic(char const* name):name_(BL_BAD_UINT32_VALUE) {
#ifdef BL_DEBUG_NAME
			ShaderManager::GetInstance().stringPool_.AddString(name, &name_);
#else
            name_ = ShaderManager::HashName(name);
#endif
		}
		Semantic& operator=(char const* name) {
#ifdef BL_DEBUG_NAME
			ShaderManager::GetInstance().stringPool_.AddString(name, &name_);
#else
            name_ = ShaderManager::HashName(name);
#endif
			return *this;
		}
#ifdef BL_DEBUG_NAME
		operator char const*() const { return ShaderManager::GetInstance().stringPool_.Find(name_); }
#endif
		bool operator<(Semantic const& s) const { return name_<s.name_; }
		bool operator==(Semantic const& s) const { return name_==s.name_; }
		bool operator<(uint32 name) const { return name<name_; }
		bool operator==(uint32 name) const { return name==name_; }
		bool operator==(char const* name) const { return name_==ShaderManager::HashName(name); }
	};
	BL_COMPILE_ASSERT(4==sizeof(Semantic), __shader_Semantic_size_not_OK);

	// shader constant type
	enum CONSTANT_TYPE {
		VERTEX_SHADER_CONSTANT_FLOAT4  = 0,
		VERTEX_SHADER_CONSTANT_MATRIX3 = 1,	// row major matrix(float3x4)
		VERTEX_SHADER_CONSTANT_MATRIX4 = 2,	// row major matrix(float4x4)
		VERTEX_SHADER_CONSTANT_INT4	   = 3,
		VERTEX_SHADER_CONSTANT_BOOL	   = 4,
        VERTEX_SHADER_CONSTANT_FLOAT3  = 5, // OpenGL Shader Language 
			
		PIXEL_SHADER_CONSTANT_FLOAT4   = 0x00010000,
		PIXEL_SHADER_CONSTANT_MATRIX3  = 0x00010001,	// row major matrix(float3x4)
		PIXEL_SHADER_CONSTANT_MATRIX4  = 0x00010002,	// row major matrix(float4x4)
		PIXEL_SHADER_CONSTANT_INT4	   = 0x00010003,
		PIXEL_SHADER_CONSTANT_BOOL	   = 0x00010004,
		CONSTANT_TYPE_UNKNOWN		   = 0x7ff00000		// force dword
	};

	// shader sampler type
	enum SAMPLER_TYPE {
		SHADER_SAMPLER_2D			 = 0,
		SHADER_SAMPLER_CUBE			 = 1,
		SHADER_SAMPLER_VOLUME		 = 2,
		VERTEX_SHADER_SAMPLER		 = 0x00010000,
		VERTEX_SHADER_SAMPLER_2D	 = VERTEX_SHADER_SAMPLER | SHADER_SAMPLER_2D,
		VERTEX_SHADER_SAMPLER_CUBE	 = VERTEX_SHADER_SAMPLER | SHADER_SAMPLER_CUBE,
		VERTEX_SHADER_SAMPLER_VOLUME = VERTEX_SHADER_SAMPLER | SHADER_SAMPLER_VOLUME,
		SAMPLER_TYPE_UNKNOWN		 = 0x7ff00000 // force dword
	};

	// shader constant
	struct Constant {
		Semantic		Name;
		CONSTANT_TYPE	Type;
		uint32			ResourceIndex;	// register index(D3D), CGparameter(PS3)...
		uint16			ResourceCount;  // limit resource count of shader constants
		mutable uint16	ResourceBound;	// resource bound, sometimes, ResourceBound<ResourceCount, for example, bone matrix palette
        bool operator<(Constant const& s) const { return Name<s.Name; }
#ifdef BL_DEBUG_NAME
		char const* GetName() const { return (char const*) Name; }
#endif
	};
	struct Constant4f { float x, y, z, w; };

	// shader sampler(use sampler instead of Texture for not causing any confusion)
	struct Sampler {
		Semantic				Name;
		SAMPLER_TYPE			Type;
		uint32					Resource;	// register index(D3D), CGparameter(PS3)...
		mutable ITexture const*	TexObj;		// has been setup or not(debug check)
		bool operator<(Sampler const& s) const { return Name<s.Name; }
#ifdef BL_DEBUG_NAME		
		char const* GetName() const { return (char const*) Name; }
#endif
	};
} // namespace shader

}}} // namespace mlabs::balai::graphics

#endif // BL_SHADER_H