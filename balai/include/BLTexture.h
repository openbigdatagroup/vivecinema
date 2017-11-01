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
 * @file	BLTexture.h
 * @author	andre chen
 * @history	2012/01/09 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_TEXTURE_H
#define BL_TEXTURE_H

#include "BLObject.h"
#include "BLResourceManager.h"
#include "BLStringPool.h"

namespace mlabs { namespace balai { namespace fileio {
	// texture header - pack into 32B
	struct TextureAttribute {
		uint32	Name;		 // name index to lookup name table or CRC if table N/A
		uint32  Reserve;	 // platform extra info

		// following 2 fields are in platform HW boundary(PS3:128-byte, Wii:64, ...)
		uint32	OffsetToTex; // Offset from the beginning of the file to the corresponding texture (bytes)
		uint32	TextureSize; // Size of texture data

		uint32  Pitch;		 // Pitch size of texture, (1-65536 = 16(4xf32) * 4096)
		uint16	Format;		 // platform native format
		uint16	Width;		 // (1-4096)
		uint16	Height;		 // (1-4096)
		uint16	Depth;		 // (1-512)
		uint8	LODs;		 // mip levels (1-13)
		uint8	Dimension;	 // 1D(1), 2D(0/2), 3D(3), Cube(4)
		uint8	ColorBits;	 // this also contain alpha bits
		uint8	AlphaBits;	 // alpha bits
	};
	BL_COMPILE_ASSERT(32==sizeof(TextureAttribute), TextureAttribute_size_not_32);
}}}

namespace mlabs { namespace balai { namespace graphics {

// Texture format enums
enum TEXTURE_FORMAT {
    //
    // important note:
    // OpenGL ES 3 deprecated GL_LUMINANCE_ALPHA and GL_LUMINANCE formats.
    //
	FORMAT_UNKNOWN	= 0,
	FORMAT_A8		= 1,
	FORMAT_I8		= 2,
	FORMAT_IA8		= 3,
	FORMAT_GB8		= 4,	// ps3
	FORMAT_RGB8		= 5,
	FORMAT_RGBA8	= 6,
	FORMAT_I16		= 7,
	FORMAT_IA16		= 8,
	FORMAT_RGBA16	= 9,
	FORMAT_R16f		= 10,
	FORMAT_RG16f	= 11,
	FORMAT_RGBA16f	= 12,
	FORMAT_R32f		= 13,
	FORMAT_RG32f	= 14,
	FORMAT_RGBA32f	= 15,
	FORMAT_UV8		= 16,
	FORMAT_UVWQ8	= 17,
	FORMAT_UV16		= 18,
	FORMAT_UVWQ16	= 19,
	FORMAT_RGB332	= 20,	
	FORMAT_RGB565	= 21,
	FORMAT_RGB555	= 22,
	FORMAT_RGB5A1	= 23,
	FORMAT_ARGB1555	= 24,	// ps3
	FORMAT_RGBA4	= 25,
	FORMAT_RGB10A2	= 26,
	FORMAT_UV5L6	= 27,
	FORMAT_UVW10A2	= 28,
	FORMAT_DXT1		= 29,
	FORMAT_DXT3		= 30,
	FORMAT_DXT5		= 31,

	// Wii
	FORMAT_I4		= 32,
	FORMAT_IA4		= 33,
	FORMAT_RGB5A3	= 34,

	// PSP use DXT1, DXT3 and DXT5 texture formats only
	
	// iPhone(PowerVR OpenGL ES implementation)
	FORMAT_PVRTC4	= 40,	// 4bpp, 1:8 compress ratio
	FORMAT_PVRTC2	= 41,	// 2bpp, 1:16 compress ratio, poor quality!

	// depth format - max value = 0x3f (i.e. FORMAT_MASK>>16)
	FORMAT_DEPTH16	 = 0x31,	// 16-bit fixed
	FORMAT_DEPTH24	 = 0x32,	// 24-bit fixed
	FORMAT_DEPTH16f	 = 0x33,	// 16-bit float S5.10
	FORMAT_DEPTH24f	 = 0x34,	// 24-bit float S8.23
	FORMAT_DEPTH24S8 = 0x35,	// 24-bit fixed + 8-bit stencil
	FORMAT_DEPTH8	 = 0x36,	// Wii only
};

#define FORMAT_R8    FORMAT_I8
#define FORMAT_RG8   FORMAT_IA8
#define FORMAT_R16   FORMAT_I16
#define FORMAT_RG16  FORMAT_IA16

// texture filter mode
enum TEXTURE_FILTER {
	// nearest image filters
	FILTER_POINT			= 0,

	// bilinear image filters
	FILTER_BILINEAR			= 1,
	FILTER_BILINEAR_ANISO	= 2, /* device may not support */
	
	// trilinear(mipmapped) image filters
	FILTER_TRILINEAR		= 3,
	FILTER_TRILINEAR_ANISO	= 4, /* device may not support */
};

// texture address
enum TEXTURE_ADDRESS {
	ADDRESS_REPEAT = 0,
	ADDRESS_CLAMP  = 1,
    ADDRESS_MIRROR = 2, // mirror repeat
    ADDRESS_BORDER = 3, // volume (3D) map
};

// texture flags helpers
#define RENDERABLE_BIT				(0x80000000L)
#define DEPTH_STENCIL_BIT			(0x40000000L)
#define CUBEMAP_BIT					(0x20000000L)
#define VOLUMEMAP_BIT				(0x10000000L)
#define SHADOWMAP_BIT				(0x08000000L)
#define GL_EXTERNAL_BIT				(0x01000000L)
#define FORMAT_MASK					(0x003F0000L)
#define FILTER_MASK					(0x00007000L)
#define ADDRESS_MASK				(0x0000003fL)
#define RENDER_TARGET(flags)		((RENDERABLE_BIT & flags)!=0)
#define DEPTH_STENCIL(flags)		((DEPTH_STENCIL_BIT & flags)!=0)
#define CUBEMAP(flags)				((CUBEMAP_BIT & flags)!=0)
#define VOLUMEMAP(flags)			((VOLUMEMAP_BIT & flags)!=0)
#define FORMAT(flags)				(TEXTURE_FORMAT) ((FORMAT_MASK & flags)>>16)
#define FILTER(flags)				(TEXTURE_FILTER) ((FILTER_MASK & flags)>>12)
#define ADDRESS_S(flags)			(TEXTURE_ADDRESS) ((0x00000030 & flags)>>4)
#define ADDRESS_T(flags)			(TEXTURE_ADDRESS) ((0x0000000C & flags)>>2)
#define ADDRESS_R(flags)			(TEXTURE_ADDRESS) ((0x00000003 & flags))

//-----------------------------------------------------------------------------
// texture base class - normally came from file
class ITexture : public IResource
{
	BL_DECLARE_RTTI;
	void*	context_;
	uint32	flags_;
	
	// check Wii version texture for this darn thing!
	// (if a color index map, load up color index object)
	virtual void OnRetrieveContext_() const {}

	// disable default/copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(ITexture);
	BL_NO_COPY_ALLOW(ITexture);

protected:
	ITexture(uint32 name, uint32 group):IResource(name, group),context_(NULL),flags_(0) {
//		BL_ASSERT(0!=name);
	}
	virtual ~ITexture() { 
		BL_ASSERT(context_==NULL);
		BL_ASSERT(0==RefCount());
	}

	// inherited class set the flag
	void ResetFlags_()							{ flags_ = 0; }
	void SetDepthStencilFlags_()				{ flags_ |= DEPTH_STENCIL_BIT; }
	void SetShadowMapFlags_()					{ flags_ |= SHADOWMAP_BIT; }
	void SetCubeFlags_()						{ flags_ |= CUBEMAP_BIT; }
	void SetVolumeFlags_()						{ flags_ |= VOLUMEMAP_BIT; }
	void SetGLExternalOESFlags_()               { flags_ |= GL_EXTERNAL_BIT; }
	void SetFormatFlags_(TEXTURE_FORMAT fmt)	{ flags_ = (flags_&~FORMAT_MASK)|((fmt<<16)&FORMAT_MASK); }
	void SetFilterFlags_(TEXTURE_FILTER filter) {
		flags_ = (flags_&~FILTER_MASK)|((filter<<12)&FILTER_MASK);
	}
	void SetAddressFlags_(TEXTURE_ADDRESS s, TEXTURE_ADDRESS t, TEXTURE_ADDRESS r) {
		flags_ = (flags_&~ADDRESS_MASK) | ((0x00000003&s)<<4) | ((0x00000003&t)<<2) | (0x00000003&r);
	}

	// inherited class implementation - (favor Wii/OpenGL platform)
	virtual bool SetFilterMode_(TEXTURE_FILTER&) { return true; }
	virtual bool SetAddressMode_(TEXTURE_ADDRESS&, TEXTURE_ADDRESS&, TEXTURE_ADDRESS&) { return true; } 

	// context management
	void ResetContext_()	{ context_ = NULL; }
	template<typename T> void SetContext_(T* t) { context_ = t; }

public:
	bool	IsNull() const	{ return (NULL==context_); }
	uint32	Flags() const	{ return flags_; }	
	template<typename T> void GetContext(T** ppContext) const {
		BL_ASSERT(NULL!=ppContext);
		if (NULL!=context_)
			OnRetrieveContext_();
		*ppContext = reinterpret_cast<T*>(context_);
	}

	// attributes
	TEXTURE_FORMAT Format() const	{ return FORMAT(flags_); }
	TEXTURE_FILTER Filter() const	{ return FILTER(flags_); }
	bool IsRenderable() const		{ return RENDER_TARGET(flags_); }
	bool IsDepthStencil() const		{ return DEPTH_STENCIL(flags_); }
	bool IsCube() const				{ return CUBEMAP(flags_); }
	bool IsVolume() const			{ return VOLUMEMAP(flags_); }
	bool IsGLExternalOES() const	{ return ((GL_EXTERNAL_BIT&flags_)!=0); }
	bool IsShadowMap() const		{ return ((SHADOWMAP_BIT&flags_)!=0); }
		// texture address can be retrieved via Flags() above and ADDRESS_S/ADDRESS_T/ADDRESS_R

	bool SetFilterMode(TEXTURE_FILTER filter) { 
		if (SetFilterMode_(filter)) {
			SetFilterFlags_(filter);
			return true;
		}
		return false;
	}
	bool SetAddressMode(TEXTURE_ADDRESS s, TEXTURE_ADDRESS t, TEXTURE_ADDRESS r=ADDRESS_CLAMP) {
		if (SetAddressMode_(s, t, r)) {
			SetAddressFlags_(s, t, r);
			return true;
		}
		return false;
	}

	// do we really need these?
	virtual uint16 Width() const = 0;
	virtual uint16 Height() const = 0;
	virtual uint16 Depth() const = 0;
	virtual uint8  LODs() const = 0;
	virtual uint8  AlphaBits() const = 0;

// UGLY Direct3D9 device lost handlers	
#ifdef BL_RENDERER_D3D
	virtual bool OnCreateDevice()	{ return true; }
	virtual bool OnResetDevice()	{ return true; }
	virtual bool OnLostDevice()		{ return true; }
	virtual bool OnDestroyDevice()  { return true; }
#endif
};

#undef RENDERABLE_BIT
#undef DEPTH_STENCIL_BIT
#undef CUBEMAP_BIT
#undef VOLUMEMAP_BIT
#undef SHADOWMAP_BIT
#undef FORMAT_MASK
#undef FILTER_MASK
#undef ADDRESS_MASK
#undef RENDER_TARGET
#undef DEPTH_STENCIL
#undef CUBEMAP
#undef VOLUMEMAP
//#undef FORMAT
//#undef FILTER
//#undef ADDRESS_S
//#undef ADDRESS_T
//#undef ADDRESS_R

//-----------------------------------------------------------------------------
// (create from scratch, good for debugging)
class Texture2D : public ITexture
{
    // Rtti
    BL_DECLARE_RTTI;

    // disable default/copy ctor and assignment operator
    BL_NO_DEFAULT_CTOR(Texture2D);
    BL_NO_COPY_ALLOW(Texture2D);

protected:
    uint16  width_;
    uint16  height_;
    explicit Texture2D(uint32 name):ITexture(name, 0),width_(0),height_(0) {}
    virtual ~Texture2D() { width_ = height_ = 0; } // use Release()

public:
    uint16 Width() const  { return width_; }
    uint16 Height() const { return height_; }
    uint16 Depth() const  { return 1; }
    uint8  LODs() const   { return 1; }
    uint8  AlphaBits() const {
        switch (Format())
        {
        case FORMAT_IA8: return 8;
        case FORMAT_RGBA8: return 8; 
        case FORMAT_RGB5A1: return 1;
        case FORMAT_RGBA4: return 4;
        default:
            break;
        }
        return 0;
    }

    // WARNING!!! this may be slow for static textures.
    //
    // supporting formats:
    //   unsigned byte : FORMAT_I8, FORMAT_IA8, FORMAT_RGB8, FORMAT_RGBA8.
    //   unsigned short : FORMAT_RGB565, FORMAT_RGB5A1, FORMAT_RGBA4.
    virtual bool UpdateImage(uint16 width, uint16 height, TEXTURE_FORMAT fmt, void const* image, bool complete=false)=0;

    // factory function
    static Texture2D* New(uint32 name, bool dynamic=false);
};

class Texture3D : public ITexture
{
    // Rtti
    BL_DECLARE_RTTI;

    // disable default/copy ctor and assignment operator
    BL_NO_DEFAULT_CTOR(Texture3D);
    BL_NO_COPY_ALLOW(Texture3D);

protected:
    uint16  width_;
    uint16  height_;
    uint16  depth_;
    uint16  pad_;
    explicit Texture3D(uint32 name):ITexture(name, 0),width_(0),height_(0),depth_(0) {}
    virtual ~Texture3D() { width_ = height_ = depth_ = 0; } // use Release()

public:
    uint16 Width() const  { return width_; }
    uint16 Height() const { return height_; }
    uint16 Depth() const  { return depth_; }
    uint8  LODs() const   { return 1; }
    uint8  AlphaBits() const {
        switch (Format())
        {
        case FORMAT_IA8:
        case FORMAT_RGBA8:
            return 8; 
        default:
            break;
        }
        return 0;
    }

    // WARNING!!! this may be slow for static textures.
    //
    // supporting formats:
    //   unsigned byte : FORMAT_I8, FORMAT_IA8, FORMAT_RGB8, FORMAT_RGBA8
    virtual bool UpdateImage(uint16 width, uint16 height, uint16 depth, TEXTURE_FORMAT fmt, void const* image)=0;

    // factory function
    static Texture3D* New(uint32 name);
};

//-----------------------------------------------------------------------------
// texture manager
class TextureManager : public ResourceManager<ITexture, 128>
{
    // private
    TextureManager() 
#ifdef BL_DEBUG_NAME
    :stringPool_(256, 2048)
#endif
    {}
    ~TextureManager() {}

public:
    static TextureManager& GetInstance() {
        static TextureManager _inst;
        return _inst;
    }

    // texture names - debug version only?
#ifdef BL_DEBUG_NAME
    StringPool stringPool_;
    void SaveName(char const* name) {
        if (name) stringPool_.AddString(name);
    }
    char const* FindName(Texture const* tex) {
        return (NULL==tex)? NULL:stringPool_.Find(tex->Name());
    }
#endif
    ITexture* FindTexture(char const* name) const {
        return (NULL!=name)? Find(HashName(name)):NULL;
    }

    static uint32 HashName(char const* name) { return StringPool::HashString(name); }
};

}}} // namespace mlabs::balai::graphics

#ifdef BL_DEBUG_NAME
#define SAVE_TEXTURE_NAME(name) mlabs::balai::graphics::TextureManager::GetInstance().SaveName(name)
#define GET_TEXTURE_NAME(tex)   mlabs::balai::graphics::TextureManager::GetInstance().FindName(tex)
#else
#define SAVE_TEXTURE_NAME(name)
#define GET_TEXTURE_NAME(tex)   (NULL)
#endif

#endif // ML_TEXTURE_H
