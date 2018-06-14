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
 * @file	BLGraphics.h
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 *          2012/01/06 take out fog setting
 */
#ifndef BL_GRAPHICS_H
#define BL_GRAPHICS_H

#include "BLShader.h"
#include "BLRenderSurface.h"
#include "BLColor.h"
#include "BLMatrix4.h"

namespace mlabs { namespace balai { namespace graphics {

// forward declarations
class Camera;

// gfxClearFrame / Clear / BeginScene / EndScene
enum GFX_CLEAR {
    GFX_CLEAR_COLOR         = 0x01,
    GFX_CLEAR_DEPTH         = 0x02,
    GFX_CLEAR_COLOR_DEPTH   = 0x03, // maybe slow on some platforms
    GFX_CLEAR_STENCIL       = 0x04,
    GFX_CLEAR_COLOR_STENCIL = 0x05, // maybe slow on some platforms
    GFX_CLEAR_DEPTH_STENCIL = 0x06,
    GFX_CLEAR_ALL           = 0x07
};

//
// [class Renderer] states, textures, shaders... management
enum GFXCULL {
    GFXCULL_BACK   = 0,
    GFXCULL_FRONT  = 1,
    GFXCULL_NONE   = 2,

    GFXCULL_TOTALS = 3
};

// (alpha) blending functions
enum GFXBLEND {
    GFXBLEND_ZERO = 0,
    GFXBLEND_ONE = 1,
    GFXBLEND_SRCCOLOR = 2,
    GFXBLEND_INVSRCCOLOR = 3,
    GFXBLEND_DSTCOLOR = 4,
    GFXBLEND_INVDSTCOLOR = 5,
    GFXBLEND_SRCALPHA = 6,
    GFXBLEND_INVSRCALPHA = 7,
    GFXBLEND_DSTALPHA = 8,
    GFXBLEND_INVDSTALPHA = 9,

    GFXBLEND_TOTALS = 10
};

// test(compare) functions
enum GFXCMPFUNC {
    GFXCMP_NEVER = 0,
    GFXCMP_LESS = 1,
    GFXCMP_EQUAL = 2,
    GFXCMP_LESSEQUAL = 3,
    GFXCMP_GREATER = 4,
    GFXCMP_NOTEQUAL = 5,
    GFXCMP_GREATEREQUAL = 6,
    GFXCMP_ALWAYS = 7,

    GFXCMP_TOTALS = 8
};

// stencil operation
enum GFXSTENCILOP {
    GFXSTENCILOP_KEEP = 0,
    GFXSTENCILOP_ZERO = 1,
    GFXSTENCILOP_REPLACE = 2,
    GFXSTENCILOP_INCRSAT = 3,
    GFXSTENCILOP_DECRSAT = 4,
    GFXSTENCILOP_INVERT = 5,
    GFXSTENCILOP_INCR = 6,
    GFXSTENCILOP_DECR = 7,

    GFXSTENCILOP_TOTALS = 8
};

// polygon mode(mainly use for debugging and developing)
enum GFXPOLYGON {
    GFXPOLYGON_POINT = 0,
    GFXPOLYGON_LINE  = 1,
    GFXPOLYGON_FILL  = 2,

    GFXPOLYGON_TOTAL = 3,
};

// Render falgs
enum RENDER_FLAGS { // temp
//  RENDER_CHECKMESHACTIVEFLAG      = 0x00000001,   // Hack for instances with inactive meshes
//  RENDER_NO_CULLING               = 0x00000002,   // Used for instances which have already been culled at a higher level
    RENDER_SHADOW_OCCLUDER          = 0x00000004,
//  RENDER_NOSETTRANSFORM           = 0x00000008,   // Used to indicate that only volumetric fog objects should be rendered
//  RENDER_WITHOUTAPPLYSHADERS      = 0x00000010,   // temporary hack, used to call sMesh->SubmitWithApplyShader instead of sMesh->Submit while rendering mesh.
    RENDER_ZPASS                    = 0x00000020,   // zbuffer pass
    RENDER_UI                       = 0x00000040,   // UI pass
//  RENDER_PERNODE_ALPHAWRITE       = 0x00000080,
//  RENDER_NO_LOD                   = 0x00000100,   // temporary additional viewcheck for volumetric effect
    RENDER_NORMALMAP_PASS           = 0x00000200,   // used to render objects collide with volumetric fog objects, can be used a Z-prepass as well.
    RENDER_NOLIGHTING               = 0x00000400,   // render objects without lighting
    RENDER_HEIGHTMAP                = 0x00000800,
    RENDER_SHADOW_MASK              = 0x00001000,
    RENDER_SOFTEDGE                 = 0x00002000,
//  RENDER_WATERMASK                = 0x00004000,   // for water simulation z-pass now.
//  RENDER_FILLBURST                = 0x00008000,
//  RENDER_WATERSPLASH              = 0x00010000,
//  RENDER_STENCILPASS              = 0x00020000,
//  RENDER_FORCE_LOD_SHADER         = 0x00040000,
//  RENDER_STATICS_ONLY             = 0x00080000,
//  RENDER_FILTERALWAYSPASS         = 0x00100000, // filter always pass
//  RENDER_SPLITPASS1               = 0x10000000,
//  RENDER_SPLITPASS2               = 0x20000000,
//  RENDER_ALPHADISABLED_FOR_OPAQUE = 0x40000000, // turns off alpha write for opaque objects

    // retired flags
//  RENDER_OPAQUE                               = 0x00000001,
//  RENDER_SEMITRANSPARENT                      = 0x00000002,
//  RENDER_ALPHATEST                            = 0x00000080,   // alpha test objects
//  RENDER_NOALPHATEST                          = 0x00000100,   // non alpha test objects
//  RENDER_POINTLIST                            = 0x00000004,   // already submit vertexes as point list
//  RENDER_SORT_FRONT_TO_BACK                   = 0x00000010,   // Used to improve pixel rejection tests for opaque rendering only
//  RENDER_BILLBOARDS                           = 0x00000040,   // Used to indicate that billboards should be rendered
//  RENDER_INSTANCE_PRE_WORLD_SEMITRANSPARENT   = 0x00000080,   // Used to indicate that this instance rendering is happening prior to semitransparent world rendering
//  RENDER_INSTANCE_POST_WORLD_SEMITRANSPARENT  = 0x00000100,   // Used to indicate that this instance rendering is happening after semitransparent world rendering
//  RENDER_USE_VISIBLE_LIST                     = 0x00000200,
};

//
// render states
//
class RenderState {
    static uint32 const colorWriteMask  = 0x80000000; // target0, 1:enable
    static uint32 const zWriteMask      = 0x40000000; // depth write 1:enable
    static uint32 const cullModeMask    = 0x03000000; // [0..2]
    static uint32 const zFuncMask       = 0x00700000; // [0..7] if 7(GFXCMP_ALWAYS) means z test disable 
    static uint32 const alphaFuncMask   = 0x00070000; // [0..7] if 7(GFXCMP_ALWAYS) means alpha test disabled 
    static uint32 const srcBlendMask    = 0x0000f000; // [0..9] GFXBLEND
    static uint32 const dstBlendMask    = 0x00000f00; // [0..9] GFXBLEND, if src=1(GFXBLEND_ONE), dest=0(GFXBLEND_ZERO) alpha blend disable
    static uint32 const alphaRefMask    = 0x000000ff;

    // helpers
    #define PACK_CULL(cull)                         (((uint32)cull)<<24)
    #define PACK_ZFUN(zFunc)                        (((uint32)zFunc)<<20)
    #define PACK_ALPHA_TEST(alphaFunc, alphaRef)    ((((uint32)alphaFunc)<<16)|(alphaRef&alphaRefMask))
    #define PACK_ALPHA_BLEND(src, dest)             ((((uint32)src)<<12)|(((uint32)dest)<<8))
    #define UNPACK_CULL(flags)                      ((GFXCULL)((flags&cullModeMask)>>24))
    #define UNPACK_ZTEST(flags)                     ((GFXCMPFUNC)((flags&zFuncMask)>>20))
    #define UNPACK_ALPHA_TEST(flags)                ((GFXCMPFUNC)((flags&alphaFuncMask)>>16))
    #define UNPACK_SRCBLEND(flags)                  ((GFXBLEND)((flags&srcBlendMask)>>12))
    #define UNPACK_DESTBLEND(flags)                 ((GFXBLEND)((flags&dstBlendMask)>>8))

    // init states :
    // 1) color write enable
    // 2) zwrite enable
    // 3) polygon cull = back face
    // 4) depth test enable, test function = less
    // 5) alpha test disable with alpha reference = 0
    // 6) color blend disable(src=ONE, dest=ZERO) 
    static uint32 const init_states	= colorWriteMask | zWriteMask | PACK_CULL(GFXCULL_BACK) | PACK_ZFUN(GFXCMP_LESS) | alphaFuncMask | PACK_ALPHA_BLEND(GFXBLEND_ONE, GFXBLEND_ZERO);

    uint32 flags_;

public:
    RenderState():flags_(init_states) {}
    void Reset() { flags_ = init_states; }

    // retrieve states
    bool        IsColorWriteEnable() const  { return 0!=(flags_&colorWriteMask); }
    bool        IsZWriteEnable() const      { return 0!=(flags_&zWriteMask); }
    bool        IsZTestEnable() const       { return GFXCMP_ALWAYS!=UNPACK_ZTEST(flags_); }
    bool        IsAlphaTestEnable() const   { return GFXCMP_ALWAYS!=UNPACK_ALPHA_TEST(flags_); }
    bool        IsAlphaBlendEnable() const  { return (GFXBLEND_ONE!=UNPACK_SRCBLEND(flags_))||(GFXBLEND_ZERO!=UNPACK_DESTBLEND(flags_)); }
    GFXCMPFUNC  AlphaTest() const           { return UNPACK_ALPHA_TEST(flags_);}
    GFXCULL     CullMode() const            { return UNPACK_CULL(flags_); }
    GFXCMPFUNC  ZFunc() const               { return UNPACK_ZTEST(flags_); }
    GFXBLEND    SrcBlend() const            { return UNPACK_SRCBLEND(flags_); }
    GFXBLEND    DestBlend() const           { return UNPACK_DESTBLEND(flags_); }
    uint8       AlphaRef() const            { return (uint8) (flags_&alphaRefMask); }

    // set states
    void SetColorWrite(bool enable) {
        if (enable)
            flags_ |= colorWriteMask;
        else
            flags_ &= ~colorWriteMask;
    }
    void SetZWrite(bool enable) {
        if (enable)
            flags_ |= zWriteMask;
        else
            flags_ &= ~zWriteMask;
    }
    void SetCull(GFXCULL cull) { flags_ = (flags_&~cullModeMask)|PACK_CULL(cull); }
    void SetZTest(GFXCMPFUNC zfun) { flags_ = (flags_&~zFuncMask)|PACK_ZFUN(zfun); }
    void SetAlphaTest(GFXCMPFUNC alphaFunc, uint8 alphaRef) {
        flags_ &= ~(alphaFuncMask|alphaRefMask);
        flags_ |= PACK_ALPHA_TEST(alphaFunc, alphaRef);
    }
    void SetAlphaBlend(GFXBLEND src, GFXBLEND dest) {
        flags_ &= ~(srcBlendMask|dstBlendMask);
        flags_ |= PACK_ALPHA_BLEND(src, dest);
    }

    #undef PACK_CULL
    #undef PACK_ZFUN
    #undef PACK_ALPHA_TEST
    #undef PACK_ALPHA_BLEND
    #undef UNPACK_CULL
    #undef UNPACK_ZTEST
    #undef UNPACK_ALPHA_TEST
    #undef UNPACK_SRCBLEND
    #undef UNPACK_DESTBLEND
};
BL_COMPILE_ASSERT(4==sizeof(RenderState), __unexpected_RenderState_size);

//
// Stencil state
//
class StencilState {
    static uint32 const stencilEnableMask = 0xC0000000; // 0:disable, 1:enable(front face), 2:(2-side)
    static uint32 const stencilFuncMask1  = 0x00e00000; // [0..7] stencil test function(front face), GFXCMPFUNC, default = GFXCMP_ALWAYS
    static uint32 const stencilFuncMask2  = 0x001C0000; // [0..7] stencil test function(back face), GFXCMPFUNC, default = GFXCMP_ALWAYS
    static uint32 const stencilFailMask1  = 0x00038000; // [0..7] stencil fail operation(front face), GFXSTENCILOP, default = GFXSTENCILOP_KEEP(0)
    static uint32 const stencilFailMask2  = 0x00007000; // [0..7] stencil fail operation(back face), GFXSTENCILOP, default = GFXSTENCILOP_KEEP(0)
    static uint32 const stencilZFailMask1 = 0x00000e00; // [0..7] stencil zfail operation(front face), GFXSTENCILOP, default = GFXSTENCILOP_KEEP(0)
    static uint32 const stencilZFailMask2 = 0x000001C0; // [0..7] stencil zfail operation(back face), GFXSTENCILOP, default = GFXSTENCILOP_KEEP(0)
    static uint32 const stencilPassMask1  = 0x00000038; // [0..7] stencil pass operation, GFXSTENCILOP, default = GFXSTENCILOP_KEEP(0)
    static uint32 const stencilPassMask2  = 0x00000007; // [0..7] stencil pass operation, GFXSTENCILOP, default = GFXSTENCILOP_KEEP(0)

    // helpers
    #define PACK_STENCIL_FUN(front, back)       ((((uint32)front)<<21)|(((uint32)back)<<18))
//  #define PACK_STENCILOP_FAIL(front, back)    (((uint32)zFunc)<<20)
//  #define PACK_STENCILOP_ZFAIL(front, back)   (((uint32)zFunc)<<20)
//  #define PACK_STENCILOP_PASS(front, back)    (((uint32)zFunc)<<20)

    // init states
    static uint32 const init_states	= PACK_STENCIL_FUN(GFXCMP_ALWAYS,GFXCMP_ALWAYS);

    //
    // stencil test : (StencilRef & StencilMask) CompFunc (StencilBufferValue & StencilMask)
    //
    uint32 flags_;
    uint8  value_[4]; // { StencilRef, StencilMask, stencilWriteMask, 0 }

public:
    StencilState():flags_(init_states) {
        value_[0] = 0;
        value_[1] = value_[2] = 0xff;
        value_[3] = 0;
    }
    void Reset() { 
        flags_ = init_states;
        value_[0] = 0;
        value_[1] = value_[2] = 0xff;
    }

    void SetEnable(bool enable);
    void SetTwoSideEnable(bool enable);
    void SetTestFunc(GFXCMPFUNC front, GFXCMPFUNC back=GFXCMP_ALWAYS);
    void SetFailOp(GFXSTENCILOP front, GFXSTENCILOP back=GFXSTENCILOP_KEEP);
    void SetZFailOp(GFXSTENCILOP front, GFXSTENCILOP back=GFXSTENCILOP_KEEP);
    void SetPassOp(GFXSTENCILOP front, GFXSTENCILOP back=GFXSTENCILOP_KEEP);
    void SetStencilRef(uint8 ref)        { value_[0] = ref; }
    void SetStencilMask(uint8 mask)      { value_[1] = mask; }
    void SetStencilWriteMask(uint8 mask) { value_[2] = mask; }

    // return 0:disable, 1:enable(single side), 2:2-sides enable
    uint32 IsEnable() const;
    GFXCMPFUNC   StencilFunc() const;
    GFXSTENCILOP FailOp() const;
    GFXSTENCILOP ZFailOp() const;
    GFXSTENCILOP PassOp() const;
    uint32       GetStencilRef() const       { return value_[0]; }
    uint32       GetStencilMask() const      { return value_[1]; }
    uint32       GetStencilWriteMask() const { return value_[2]; }

    // only if 2==IsEnable()...
    GFXCMPFUNC	 StencilFuncBackFace() const;
    GFXSTENCILOP FailOpBackFace() const;
    GFXSTENCILOP ZFailOpBackFace() const;
    GFXSTENCILOP PassOpBackFace() const;

    #undef PACK_STENCIL_FUN
};
BL_COMPILE_ASSERT(8==sizeof(StencilState), __stencil_state_size_not_ok_);

//- renderer ------------------------------------------------------------------
class Renderer
{
protected:
    // constants
    enum { 
        TRANSFORM_STACK_SIZE        = 16,
        STATES_STACK_SIZE           = 16,
        MAX_COMBINED_TEXTURE_UNITS  = 32, // total of vertex & pixel texture units
    };

private:
    // transforms
    math::Matrix3 worldMatrixStack_[TRANSFORM_STACK_SIZE];
    math::Matrix3 viewMatrixStack_[TRANSFORM_STACK_SIZE];
    math::Matrix4 projMatrixStack_[TRANSFORM_STACK_SIZE];

    // textures
    ITexture const* textures_[MAX_COMBINED_TEXTURE_UNITS][2]; // 0:current, 1:selected

    // render states
    GFXCULL     cullModeStack_[STATES_STACK_SIZE];
    GFXCMPFUNC  zFuncStack_[STATES_STACK_SIZE];
    GFXBLEND    blendModeStack_[STATES_STACK_SIZE][2]; // { srcBlend, destBlend }
    GFXCMPFUNC  alphaFuncStack_[STATES_STACK_SIZE];
    uint8       alphaRefStack_[STATES_STACK_SIZE];
    bool        colorMaskStack_[STATES_STACK_SIZE];
    bool        depthMaskStack_[STATES_STACK_SIZE];

    mutable math::Matrix3 invWorldMatrix_;     // inverse world matrix
    mutable math::Matrix3 invViewMatrix_;      // inverse view matrix
    mutable math::Matrix3 worldViewMatrix_;    // aka model-view matrix
    mutable math::Matrix3 invWorldViewMatrix_; // inverse model-view matrix
    mutable math::Matrix4 viewProjMatrix_;
    mutable math::Matrix4 worldViewProjMatrix_;

    RenderSurface* currentRenderSurface_;

    // current shaders
    ShaderEffect* currentFX_;

    // stack depth
    int worldStackTop_;
    int viewStackTop_;
    int projMatrixTop_;
    int stateStackTop_;

    // matrix dirty flag
    mutable uint32 WorldViewProjMatrixDirtyFlags_;

    // render target width/height
protected: // implement class see this...(ugly!)
    uint32 api_major_version_;
    uint32 api_minor_version_;
    uint32 screenWidth_;   // default screen width/height
    uint32 screenHeight_;  
    uint32 surfaceWidth_;  // current render surface width/height
    uint32 surfaceHeight_;

    uint32 maxTextureSize_;
    uint32 maxMultisampleSamples_;
    float  maxTextureAnisotropy_;

private:
    GFXBLEND    srcBlend_, dstBlend_;
    GFXCULL     cullMode_;
    GFXCMPFUNC  zFunc_;
    GFXCMPFUNC  alphaFunc_;
    uint8       alphaRef_;
    bool        colorMask_; // disable color write
    bool        depthMask_; // disable depth write

    //
    // stencil operation to be implemented,
    //     including stencil test : (StencilRef & StencilMask) CompFunc (StencilBufferValue & StencilMask)
    //
//  bool         stencilEnable_[2];  // { 0:current, 1:selected }
//  GFXCMPFUNC   stencilFunc_[2];    // { 0:front, 1:back }
//  GFXSTENCILOP stencilFailOp_[2];  // { 0:front, 1:back }
//  GFXSTENCILOP stencilZFailOp_[2]; // { 0:front, 1:back }
//  GFXSTENCILOP stencilPassOp_[2];  // { 0:front, 1:back }
//  uint32       stencilRef_;        // stencil reference value
//  uint32       stencilMask_;       // stencil mask value
//  uint32       stencilWriteMask_;  // stencil write mask
//

    // platform implementation...
    virtual bool SurfaceResize_(uint32 width, uint32 height)        = 0;
    virtual void SetCullMode_(GFXCULL cullMode)                     = 0;
    virtual void SetColorMask_(bool enable)                         = 0;
    virtual void SetDepthMask_(bool enable, GFXCMPFUNC ZFunc)       = 0; // pass 2nd param to favor Wii GX
    virtual void SetZFunc_(GFXCMPFUNC ZFunc, bool enable)           = 0; // pass 2nd param to favor Wii GX
    virtual void SetBlendMode_(GFXBLEND src, GFXBLEND dest)         = 0;
    virtual void SetAlphaTest_(GFXCMPFUNC alphaFun, uint8 alphaRef) = 0; // not support in OpenGL ES, extension required

    // set texture unit(vertex & fragment)
    virtual void SetTexture_(uint32 unit, ITexture const* tex) = 0;

    // set effect
    virtual void SetEffect_(ShaderEffect* fx) = 0;

    // set surface
    virtual bool SetSurface_(RenderSurface* surface, bool& colormask, bool& zmask) = 0;

    // set polygon mode - point, line or fill. if hardware/api support it,
    // you should implement it. return 
    //  1) 0 if both front and back face fill mode setting is shared
    //  2) 1 if both set front and back face can be set
    virtual int SetPolygonMode_(GFXPOLYGON /*front*/, GFXPOLYGON /*back*/) { return -1; }

protected:
    Renderer();
    virtual ~Renderer() {}

public:
    // single instance(implemented by sub-class)
    static Renderer& GetInstance();

    // set transforms
    void SetWorldMatrix(math::Matrix3 const& world) {
        // we set world transform pretty much...
//      if (world.Equals(worldMatrixStack_[worldStackTop_])) return;
        worldMatrixStack_[worldStackTop_] = world;
        WorldViewProjMatrixDirtyFlags_ |= 1;
    }
    void SetViewMatrix(math::Matrix3 const& view) {
        viewMatrixStack_[viewStackTop_] = view;
        WorldViewProjMatrixDirtyFlags_ |= 2;
    }
    void SetProjectionMatrix(math::Matrix4 const& proj) {
        projMatrixStack_[projMatrixTop_] = proj;
        WorldViewProjMatrixDirtyFlags_ |= 4;
    }
    math::Matrix3 const& GetWorldMatrix() const      { return worldMatrixStack_[worldStackTop_]; }
    math::Matrix3 const& GetViewMatrix() const       { return viewMatrixStack_[viewStackTop_];   }
    math::Matrix4 const& GetProjectionMatrix() const { return projMatrixStack_[projMatrixTop_];  }

    math::Matrix3 const& GetInverseWorldMatrix() const {
        if (WorldViewProjMatrixDirtyFlags_&0x01) {
            invWorldMatrix_ = worldMatrixStack_[worldStackTop_].GetInverse();
            if (WorldViewProjMatrixDirtyFlags_&0x02)
                invViewMatrix_ = viewMatrixStack_[viewStackTop_].GetInverse();
            worldViewMatrix_ = viewMatrixStack_[viewStackTop_] * worldMatrixStack_[worldStackTop_];
            invWorldViewMatrix_ = worldViewMatrix_.GetInverse();
            viewProjMatrix_  = projMatrixStack_[projMatrixTop_] * viewMatrixStack_[viewStackTop_];
            worldViewProjMatrix_ = viewProjMatrix_ * worldMatrixStack_[worldStackTop_];
            WorldViewProjMatrixDirtyFlags_ = 0;
        }
        return invWorldMatrix_;
    }
    math::Matrix3 const& GetInverseViewMatrix() const {
        if (WorldViewProjMatrixDirtyFlags_&0x02) {
            if (WorldViewProjMatrixDirtyFlags_&0x01)
                invWorldMatrix_ = worldMatrixStack_[worldStackTop_].GetInverse();
            invViewMatrix_ = viewMatrixStack_[viewStackTop_].GetInverse();
            worldViewMatrix_ = viewMatrixStack_[viewStackTop_] * worldMatrixStack_[worldStackTop_];
            invWorldViewMatrix_ = worldViewMatrix_.GetInverse();
            viewProjMatrix_  = projMatrixStack_[projMatrixTop_] * viewMatrixStack_[viewStackTop_];
            worldViewProjMatrix_ = viewProjMatrix_ * worldMatrixStack_[worldStackTop_];
            WorldViewProjMatrixDirtyFlags_ = 0;
        }
        return invViewMatrix_;
    }
    math::Matrix3 const& GetWorldViewMatrix() const {
        if (WorldViewProjMatrixDirtyFlags_&0x03) {
            if (WorldViewProjMatrixDirtyFlags_&0x01)
                invWorldMatrix_ = worldMatrixStack_[worldStackTop_].GetInverse();
            if (WorldViewProjMatrixDirtyFlags_&0x02)
                invViewMatrix_ = viewMatrixStack_[viewStackTop_].GetInverse();

            worldViewMatrix_ = viewMatrixStack_[viewStackTop_] * worldMatrixStack_[worldStackTop_];
            invWorldViewMatrix_ = worldViewMatrix_.GetInverse();
            viewProjMatrix_  = projMatrixStack_[projMatrixTop_] * viewMatrixStack_[viewStackTop_];
            worldViewProjMatrix_ = viewProjMatrix_ * worldMatrixStack_[worldStackTop_];
            WorldViewProjMatrixDirtyFlags_ = 0;
        }
        return worldViewMatrix_;
    }
    math::Matrix4 const& GetViewProjMatrix() const {
        if (WorldViewProjMatrixDirtyFlags_&0x06) {
            if (WorldViewProjMatrixDirtyFlags_&0x01)
                invWorldMatrix_ = worldMatrixStack_[worldStackTop_].GetInverse();
            if (WorldViewProjMatrixDirtyFlags_&0x02)
                invViewMatrix_ = viewMatrixStack_[viewStackTop_].GetInverse();
            worldViewMatrix_ = viewMatrixStack_[viewStackTop_] * worldMatrixStack_[worldStackTop_];
            invWorldViewMatrix_ = worldViewMatrix_.GetInverse();
            viewProjMatrix_  = projMatrixStack_[projMatrixTop_] * viewMatrixStack_[viewStackTop_];
            worldViewProjMatrix_ = viewProjMatrix_ * worldMatrixStack_[worldStackTop_];
            WorldViewProjMatrixDirtyFlags_ = 0;
        }
        return viewProjMatrix_;
    }
    math::Matrix4 const& GetWorldViewProjMatrix() const {
        if (WorldViewProjMatrixDirtyFlags_&0x07) {
            if (WorldViewProjMatrixDirtyFlags_&0x01)
                invWorldMatrix_ = worldMatrixStack_[worldStackTop_].GetInverse();
            if (WorldViewProjMatrixDirtyFlags_&0x02)
                invViewMatrix_ = viewMatrixStack_[viewStackTop_].GetInverse();
            worldViewMatrix_ = viewMatrixStack_[viewStackTop_] * worldMatrixStack_[worldStackTop_];
            invWorldViewMatrix_ = worldViewMatrix_.GetInverse();
            viewProjMatrix_  = projMatrixStack_[projMatrixTop_] * viewMatrixStack_[viewStackTop_];
            worldViewProjMatrix_ = viewProjMatrix_ * worldMatrixStack_[worldStackTop_];
            WorldViewProjMatrixDirtyFlags_ = 0;
        }
        return worldViewProjMatrix_;
    }
    // push/pop matrix, return false if stack overflow(operation still succeed)
    bool PushWorldMatrix(math::Matrix3 const& world=math::Matrix3::Identity) {
        BL_ASSERT(worldStackTop_<TRANSFORM_STACK_SIZE);
        if (++worldStackTop_>=TRANSFORM_STACK_SIZE) {
            worldStackTop_ = 0;
        }
        worldMatrixStack_[worldStackTop_] = world;
        WorldViewProjMatrixDirtyFlags_ |= 1;
        return (0!=worldStackTop_);
    }
    bool PushViewMatrix(math::Matrix3 const& view) {
        BL_ASSERT(viewStackTop_<TRANSFORM_STACK_SIZE);
        if (++viewStackTop_>=TRANSFORM_STACK_SIZE) {
            viewStackTop_ = 0; // !!!
        }
        viewMatrixStack_[viewStackTop_] = view;
        WorldViewProjMatrixDirtyFlags_ |= 2;
        return (0!=viewStackTop_);
    }
    bool PushProjMatrix(math::Matrix4 const& proj=math::Matrix4::Identity) {
        BL_ASSERT(projMatrixTop_<TRANSFORM_STACK_SIZE);
        if (++projMatrixTop_>=TRANSFORM_STACK_SIZE) {
            projMatrixTop_ = 0; // !!!
        }
        projMatrixStack_[projMatrixTop_] = proj;
        WorldViewProjMatrixDirtyFlags_ |= 4;
        return (0==projMatrixTop_);
    }
    bool PopWorldMatrix(math::Matrix3* mat3=NULL) {
        BL_ASSERT(worldStackTop_>0);
        if (mat3) *mat3 = worldMatrixStack_[worldStackTop_];
        WorldViewProjMatrixDirtyFlags_ |= 1;
        if (--worldStackTop_<0) {
            worldStackTop_ = TRANSFORM_STACK_SIZE - 1; // !!!
            return false;
        }
        return true;
    }
    bool PopViewMatrix(math::Matrix3* mat3=NULL) {
        BL_ASSERT(viewStackTop_>0);
        if (mat3) *mat3 = viewMatrixStack_[viewStackTop_];
        WorldViewProjMatrixDirtyFlags_ |= 2;
        if (--viewStackTop_<0) {
            viewStackTop_ = TRANSFORM_STACK_SIZE - 1; // !!!	
            return false;
        }
        return true;
    }
    bool PopProjMatrix(math::Matrix4* mat4=NULL) {
        BL_ASSERT(projMatrixTop_>0);
        if (mat4) *mat4 = projMatrixStack_[projMatrixTop_];
        WorldViewProjMatrixDirtyFlags_ |= 4;
        if (--projMatrixTop_<0) {
            projMatrixTop_ = TRANSFORM_STACK_SIZE - 1; // !!!
            return false;
        }
        return true;
    }

    // screen/framebuffer size
    uint32 GetScreenWidth() const { return screenWidth_; }
    uint32 GetScreenHeight() const { return screenHeight_; }
    float  GetScreenAspectRatio() const { return screenWidth_/(float)screenHeight_; }
    uint32 GetFramebufferWidth() const { return surfaceWidth_; }
    uint32 GetFramebufferHeight() const { return surfaceHeight_; }
    float  GetFramebufferAspectRatio() const { return surfaceWidth_/(float)surfaceHeight_; }
    bool SetScreenSize(uint32 width, uint32 height) { // call when window surface resize(typically by windows graphics system)
        if (SurfaceResize_(width, height)) {
            surfaceWidth_ = screenWidth_ = width;
            surfaceHeight_ = screenHeight_ = height;
            return true;
        }
        return false;
    }

    void GetRenderAPIVersion(uint32& major, uint32& minor) const {
        major = api_major_version_;
        minor = api_minor_version_;
    }

    // maximum texture size renderer support
    uint32 GetMaxTextureSize() const { return maxTextureSize_; }

    // maximun multisample samples renderer support
    uint32 GetMaxMultisampleSamples() const { return maxMultisampleSamples_; }

    // maximum anisotropy
    float GetMaxTextureAnisotropy() const { return maxTextureAnisotropy_; }

    // cull mode
	void SetCullMode(GFXCULL cull)  { cullModeStack_[stateStackTop_] = cull; }
	void SetCullDisable()           { cullModeStack_[stateStackTop_] = GFXCULL_NONE; }

	// color/depth write
	void SetColorWrite(bool enable) { colorMaskStack_[stateStackTop_] = !enable; }
	void SetDepthWrite(bool enable)	{ depthMaskStack_[stateStackTop_] = !enable; }
	void SetColorMask(bool enable)  { colorMaskStack_[stateStackTop_] = enable; }
	void SetDepthMask(bool enable)  { depthMaskStack_[stateStackTop_] = enable; }

	// z func
	void SetZTest(GFXCMPFUNC zFunc=GFXCMP_LESS) { zFuncStack_[stateStackTop_] = zFunc; }
	void SetZTestDisable()			 { zFuncStack_[stateStackTop_] = GFXCMP_ALWAYS; }

	// blending
	void SetBlendMode(GFXBLEND src, GFXBLEND dst) {
		blendModeStack_[stateStackTop_][0] = src;
		blendModeStack_[stateStackTop_][1] = dst;
	}
	void SetBlendDisable()  {
		blendModeStack_[stateStackTop_][0] = GFXBLEND_ONE;
		blendModeStack_[stateStackTop_][1] = GFXBLEND_ZERO;
	}
	
	// alpha test
	void SetAlphaTestFunc(GFXCMPFUNC alphaFunc, uint8 alphaRef) {
		alphaFuncStack_[stateStackTop_] = alphaFunc;
		alphaRefStack_[stateStackTop_]	= alphaRef;
	}
	void SetAlphaTestDisable() {
		alphaFuncStack_[stateStackTop_] = GFXCMP_ALWAYS;
		alphaRefStack_[stateStackTop_]	= 0xff;
	}

	// polygon mode - use for developing and debugging only.
	// depending on hardware/api, it doesn't guarantee to work. but it should not cause any trouble!
	int SetPolygonMode(GFXPOLYGON front, GFXPOLYGON back) {
		 return SetPolygonMode_(front, back);
	}

	// managed states
	void InitState() { SetState(RenderState()); }
	void SetState(RenderState const& states, bool force=false) { // this helps when some middlewares mangle renderstates
		cullModeStack_[stateStackTop_]		= states.CullMode();
		zFuncStack_[stateStackTop_]			= states.ZFunc();
		blendModeStack_[stateStackTop_][0]	= states.SrcBlend();
		blendModeStack_[stateStackTop_][1]	= states.DestBlend();
		alphaFuncStack_[stateStackTop_]		= states.AlphaTest();
		alphaRefStack_[stateStackTop_]		= states.AlphaRef();
		colorMaskStack_[stateStackTop_]		= !states.IsColorWriteEnable();
		depthMaskStack_[stateStackTop_]		= !states.IsZWriteEnable();
		if (force) {
			SetCullMode_(cullMode_=cullModeStack_[stateStackTop_]);
			SetColorMask_(colorMask_=colorMaskStack_[stateStackTop_]);
			SetDepthMask_(depthMask_ = depthMaskStack_[stateStackTop_], zFuncStack_[stateStackTop_]);
			SetZFunc_(zFunc_ = zFuncStack_[stateStackTop_], depthMaskStack_[stateStackTop_]);
			SetBlendMode_(srcBlend_=blendModeStack_[stateStackTop_][0], dstBlend_=blendModeStack_[stateStackTop_][1]);
			SetAlphaTest_(alphaFunc_=alphaFuncStack_[stateStackTop_], alphaRef_= alphaRefStack_[stateStackTop_]);
		}
	}
	bool PushState() {
		BL_ASSERT(stateStackTop_<STATES_STACK_SIZE);
		int const last = stateStackTop_;
		if (++stateStackTop_>=STATES_STACK_SIZE)
			stateStackTop_ = 0;
		cullModeStack_[stateStackTop_]		= cullModeStack_[last];
		zFuncStack_[stateStackTop_]			= zFuncStack_[last];
		blendModeStack_[stateStackTop_][0]	= blendModeStack_[last][0];
		blendModeStack_[stateStackTop_][1]	= blendModeStack_[last][1];
		alphaFuncStack_[stateStackTop_]		= alphaFuncStack_[last];
		alphaRefStack_[stateStackTop_]		= alphaRefStack_[last];
		colorMaskStack_[stateStackTop_]		= colorMaskStack_[last];
		depthMaskStack_[stateStackTop_]		= depthMaskStack_[last];
		return stateStackTop_>last;
	}
	bool PushState(RenderState const& states) {
		BL_ASSERT(stateStackTop_<STATES_STACK_SIZE);
		int const last = stateStackTop_;
		if (++stateStackTop_>=STATES_STACK_SIZE)
			stateStackTop_ = 0;
		
		// set states
		SetState(states);

		return (stateStackTop_>last);
	}
	bool PopState(RenderState* current = NULL) {
		BL_ASSERT(stateStackTop_>0);
		if (current) {
			current->SetColorWrite(!colorMaskStack_[stateStackTop_]);
			current->SetZWrite(!depthMaskStack_[stateStackTop_]);
			current->SetCull(cullModeStack_[stateStackTop_]);
			current->SetZTest(zFuncStack_[stateStackTop_]);
			current->SetAlphaTest(alphaFuncStack_[stateStackTop_], alphaRefStack_[stateStackTop_]);
			current->SetAlphaBlend(blendModeStack_[stateStackTop_][0], blendModeStack_[stateStackTop_][1]);
		}
		if (--stateStackTop_<0) {
			stateStackTop_ = STATES_STACK_SIZE - 1;
			return false;
		}
		return true;
	}

	// set texture
	bool SetTexture(uint32 unit, ITexture const* tex) {
		if (unit<MAX_COMBINED_TEXTURE_UNITS) {
			textures_[unit][1] = tex;
			return true;
		}
		return false;
	}

    // 2016.03.18 sync up current dirty states
    void CommitRenderStates() {
        if (cullMode_!=cullModeStack_[stateStackTop_])
            SetCullMode_(cullMode_=cullModeStack_[stateStackTop_]);
        if (colorMask_!=colorMaskStack_[stateStackTop_])
            SetColorMask_(colorMask_=colorMaskStack_[stateStackTop_]);
        if (depthMask_!=depthMaskStack_[stateStackTop_])
            SetDepthMask_(depthMask_=depthMaskStack_[stateStackTop_], zFuncStack_[stateStackTop_]);
        if (zFunc_!=zFuncStack_[stateStackTop_])
            SetZFunc_(zFunc_=zFuncStack_[stateStackTop_], depthMaskStack_[stateStackTop_]);
        if (srcBlend_!=blendModeStack_[stateStackTop_][0] || dstBlend_!=blendModeStack_[stateStackTop_][1])
            SetBlendMode_(srcBlend_=blendModeStack_[stateStackTop_][0], dstBlend_=blendModeStack_[stateStackTop_][1]);
        if (alphaFunc_!=alphaFuncStack_[stateStackTop_] || alphaRef_!=alphaRefStack_[stateStackTop_])
            SetAlphaTest_(alphaFunc_=alphaFuncStack_[stateStackTop_], alphaRef_=alphaRefStack_[stateStackTop_]);
    }

    // 2016.03.18 invalid all texture units
    void InvalidTextureCache() {
        for (uint32 i=0; i<MAX_COMBINED_TEXTURE_UNITS; ++i) {
            SetTexture_(i, textures_[i][0]=textures_[i][1]=NULL);
        }
    }

	// set render surface, pass null to restore to default frame/depth buffer.
	// this may change zwrite & colorwrite state sliently according to surface config.
    // the caller aware it and reset both states after set back default surface if needed.
    bool SetSurface(RenderSurface* surface) {
        if (NULL!=currentRenderSurface_) {
            currentRenderSurface_->Unbind();
        }

        // sync up color/depth mask states
        if (colorMask_!=colorMaskStack_[stateStackTop_])
            SetColorMask_(colorMask_=colorMaskStack_[stateStackTop_]);
        if (depthMask_!=depthMaskStack_[stateStackTop_])
            SetDepthMask_(depthMask_=depthMaskStack_[stateStackTop_], zFuncStack_[stateStackTop_]);
        if (SetSurface_(surface, colorMask_, depthMask_)) {
            currentRenderSurface_ = surface;
            if (NULL!=currentRenderSurface_) {
                currentRenderSurface_->Bind();
            }
        }
        return true;
	}

	// set effect
	bool SetEffect(ShaderEffect* fx) {
		if (fx) {
			if (fx->OnSelected(currentFX_)) {
				currentFX_ = fx;
				return true;
			}
		}
		currentFX_ = NULL;
		return false;
	}

	// call at most once for each frame
	void Reset();

	// whenever you call functions above, you must call CommitChanges() before rendering
	void CommitChanges();

	// to be implemented
	virtual bool Initialize() = 0;
	virtual void Finalize() = 0;
	virtual void SetViewport(int x, int y, int width, int height) const = 0;
	virtual bool Clear(Color const& color=Color::Zero, float depth=1.0f, uint8 stencil=0, GFX_CLEAR option=GFX_CLEAR_ALL)=0;
    virtual bool BeginScene(Camera const* camera=NULL)=0;
    virtual void EndScene()=0;
//
// synchronization with GPU
//  virtual uint32 SetFence() = 0;
//  virtual void FinishFence(uint32 fence)=0;
};

}}} // namespace mlabs::balai::graphics

#endif // BL_GRAPHICS_H