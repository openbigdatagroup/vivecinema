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
 * @file    BLRenderSurface.h
 * @author  andre chen
 * @history 2012/01/09 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_RENDERSURFACE_H
#define BL_RENDERSURFACE_H

// include BLRenderSurface.h will likely to include BLTexture.h as well.
// let's make life easier.
#include "BLTexture.h"

// max render targets support
#define BL_MAX_RENDER_TARGETS   4

namespace mlabs { namespace balai { namespace graphics {

// surface format are limited. let's define new enum instead of use TEXTURE_FORMAT
enum SURFACE_FORMAT {
    SURFACE_FORMAT_NONE,
    SURFACE_FORMAT_RGBA8,    // either R8G8B8A8 or A8R8G8B8(ps3) ?
    SURFACE_FORMAT_RGBA16F,  // W16_Z16_Y16_X16_FLOAT(ps3)
    SURFACE_FORMAT_RGBA32F,
    //SURFACE_FORMAT_X32F,   // R32F
    //SURFACE_FORMAT_X16F,   // R16F
    // more to come...

    // depth stencil format
    SURFACE_FORMAT_D16,       // 16-bit fixed
    SURFACE_FORMAT_D24,       // 24-bit fixed
    SURFACE_FORMAT_D16f,      // 16-bit float S5.10
    SURFACE_FORMAT_D24f,      // 24-bit float S8.23
    SURFACE_FORMAT_D24S8,     // 24-bit fixed + 8-bit stencil
    SURFACE_FORMAT_D32
};

//-----------------------------------------------------------------------------
// render surface : a composite, a render surface can combine max 4
// color textures/surfaces and probably a depth texture/surface all together.
//
class RenderSurface : public IResource
{
    BL_DECLARE_RTTI;

    // disable default/copy ctor and assignment operator
    BL_NO_DEFAULT_CTOR(RenderSurface);
    BL_NO_COPY_ALLOW(RenderSurface);

    // contains
    ITexture* renderTargets_[BL_MAX_RENDER_TARGETS];
    //ITexture* depthTexture_;
    void*  context_; // platform dependent, probabily not used.

protected:
    uint32 width_;
    uint32 height_;

    // created only by Generator or hiden concrete class
    explicit RenderSurface(uint32 name):IResource(name,0),/*depthTexture_(NULL),*/
        context_(NULL),width_(0),height_(0) {
        for (uint32 i=0; i<BL_MAX_RENDER_TARGETS; ++i)
            renderTargets_[i] = NULL;
    }
    virtual ~RenderSurface() {
        BL_ASSERT(context_==NULL);
        for (uint32 i=0; i<BL_MAX_RENDER_TARGETS; ++i) {
            if (renderTargets_[i]) {
                if (0!=renderTargets_[i]->Release()) {
                    BL_ERR("~RenderSurface() render targets not release!\n");
                }
                renderTargets_[i] = NULL;
            }
        }

        /*
        if (depthTexture_) {
            if (0!=depthTexture_->Release()) {
                BL_ERR("~RenderSurface() depthTexture_ not release!\n");
            }
            depthTexture_ = NULL;
        }*/
    }

    // context management
    void ResetContext_() { context_ = NULL; }
    template<typename T> void SetContext_(T* t) { context_ = t; }

    // assign textures(never fail, never leaks)
    void SetRenderTarget_(uint32 i, ITexture* t) {
        BL_ASSERT(i<BL_MAX_RENDER_TARGETS);
        if (i<BL_MAX_RENDER_TARGETS) {
            if (renderTargets_[i])
                renderTargets_[i]->Release();

            renderTargets_[i] = t;
            if (t)
                t->AddRef();
        }
    }/*
    void SetDepthTexture_(ITexture* t) {
        if (depthTexture_)
            depthTexture_->Release();

        depthTexture_ = t;
        if (depthTexture_)
            depthTexture_->AddRef();
    }
    */

public:
    template<typename T> void GetContext(T** ppContext) const {
        BL_ASSERT(NULL!=ppContext);
        *ppContext = reinterpret_cast<T*>(context_);
    }

    // fail to release after used will cause memory leaks
    ITexture* GetRenderTarget(uint32 i=0) const {
        if (i<BL_MAX_RENDER_TARGETS && renderTargets_[i]) {
            renderTargets_[i]->AddRef();
            return renderTargets_[i];
        }
        return NULL;
    }/*
    ITexture* GetDepthTexture() const {
        if (depthTexture_) {
            depthTexture_->AddRef();
            return depthTexture_;
        }
        return NULL;
    }*/

    uint32 GetNumRenderTargets() const {
        uint32 cnt = 0;
        while (NULL!=renderTargets_[cnt])
            ++cnt;
        return cnt;
    }
    //bool HasDepthStencil() const { return NULL!=depthTexture_; }

    // get the 'first' render target's dimension.(to be overwriten)
    bool GetSurfaceSize(uint32& w, uint32& h) const {
        w = width_;
        h = height_;
        return w>0 && h>0;
    }

    // called by platform renderer. Don't call me!
    virtual bool Bind() { return true; }
    virtual bool Unbind() { return true; }

    // generator - the factory
    friend struct SurfaceGenerator;
};

//-----------------------------------------------------------------------------
// the factory - note all RenderSurfaces must be generated in initial stage
struct SurfaceGenerator
{
    enum DEPTH_STENCIL_USAGE {
        DEPTH_STENCIL_USAGE_DEFAULT_READONLY = 1,
        DEPTH_STENCIL_USAGE_DEFAULT_READWRITE = 2,
        DEPTH_STENCIL_USAGE_SHADOWMAP = 3,
    };

    SURFACE_FORMAT  renderTargetFormats_[BL_MAX_RENDER_TARGETS];    // all RTs must be the same format(PS3)
    SURFACE_FORMAT  depthStencilFormat_;
    uint32          width_;          // if width_ and height_ both zero(default case)
    uint32          height_;         // dimension will be decided by framebuffer.
    uint8           numRenderTargets_;
    uint8           multisampleSamples_;   // multisampling
    uint8           depthStencilUsage_; // 1:shadowmap, 2:default depth, 3:write back to default depth
    uint8           extraInfo_;      // extra data, platform dependent thing...

public:
    SurfaceGenerator():
        depthStencilFormat_(SURFACE_FORMAT_NONE),
        numRenderTargets_(0),width_(0),height_(0),multisampleSamples_(0),depthStencilUsage_(0),extraInfo_(0) {
        for (int i=0; i<BL_MAX_RENDER_TARGETS; ++i)
            renderTargetFormats_[i] = SURFACE_FORMAT_NONE;
    }
    int SetRenderTargetFormats(SURFACE_FORMAT* fmts, int totals) {
        assert(fmts && 0<totals && totals<=BL_MAX_RENDER_TARGETS);
        numRenderTargets_ = 0;
        for (int i=0; i<totals; ++i) {
            if (SURFACE_FORMAT_NONE!=fmts[i]) {
                renderTargetFormats_[numRenderTargets_++] = fmts[i];
            }
        }
        return numRenderTargets_; 
    }
    int SetRenderTargetFormat(SURFACE_FORMAT fmt) {
        renderTargetFormats_[0] = fmt;
        numRenderTargets_ = (SURFACE_FORMAT_NONE!=fmt) ? 1:0;
        return numRenderTargets_; 
    }
	bool SetDepthFormat(SURFACE_FORMAT fmt) {
        if (fmt>=SURFACE_FORMAT_D16) {
            depthStencilFormat_ = fmt;
            return true;
        }
        depthStencilFormat_ = SURFACE_FORMAT_NONE;
        return false;
    }
    void SetSurfaceSize(uint32 w, uint32 h) { width_ = w; height_ = h; }
    void SetMultiSampleSamples(uint8 samples) { multisampleSamples_ = samples; };
    void SetDepthStencilUsage(uint8 i) { depthStencilUsage_ = i; }
    void SetExtraInfo(uint8 i) { extraInfo_ = i; }

    // factory - platform dependent
    RenderSurface* Generate(char const* name);
};

}}} // namespace mlabs::balai::graphics

#endif // BL_RENDERSURFACE_H