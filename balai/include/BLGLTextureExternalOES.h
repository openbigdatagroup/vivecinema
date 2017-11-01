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
 * @file    BLGLTextureExternalOES.h
 * @author  andre chen
 * @history 2012/08/13
 *
 */
#ifdef BL_RENDERER_OPENGL_ES
#ifndef BL_OPENGL_TEXTURE_EXTERNAL_OES_H
#define BL_OPENGL_TEXTURE_EXTERNAL_OES_H

#include "BLTexture.h"
#include "BLOpenGL.h"

/**
 * this is for GL_OES_EGL_image_external, requires EGL 1.2 also requires
 * either the EGL_KHR_image_base or the EGL_KHR_image extension.
 * typically, it's of yuv42 format directly from some devices like video camera.
 *
 * currently it's for android platform only
 * example of fragment shader :
char const* psh_externalOESmap_v0 =
    "#extension GL_OES_EGL_image_external : require \n"
    "varying lowp vec4 color_;                      \n"
    "varying mediump vec2 texcoord_;                \n"
    "uniform samplerExternalOES diffuseMap;         \n"
    "void main() {                                  \n"
    " gl_FragColor = color_ * texture2D(diffuseMap, texcoord_); \n"
    "}";
    
You can use CreatePrimitiveShaderTextureOES(declared below) to create a shader for primitive rendering.
e.g.

    // init a shader, a sampler and a textureOES
    externalOES_ = CreatePrimitiveShaderTextureOES(diffuseMap_);
    videoTexture_ = blNew GLTextureExternal2D(0);
    videoTexture_->Create(1280, 720); // 720p

    // draw a textured fullscreen quad...
    Renderer& renderer = Renderer::GetInstance();
    Primitives& prim = Primitives::GetInstance();
    prim.BeginDraw(GFXPT_SCREEN_QUADLIST);
        renderer.SetEffect(externalOES_);
        externalOES_->BindSampler(diffuseMap_, videoTexture_);
        renderer.CommitChanges();
        prim.AddVertex2D(0.0f, 0.0f, 0.0f, 0.0f);
        prim.AddVertex2D(0.0f, 1.0f, 0.0f, 1.0f);
        prim.AddVertex2D(1.0f, 1.0f, 1.0f, 1.0f);
        prim.AddVertex2D(1.0f, 0.0f, 1.0f, 0.0f);
    prim.EndDraw();
*/

namespace mlabs { namespace balai { namespace graphics {

class GLTextureExternal2D : public ITexture
{
    // disable default/copy ctor and assignment operator
    BL_NO_DEFAULT_CTOR(GLTextureExternal2D);
    BL_NO_COPY_ALLOW(GLTextureExternal2D);

    GLuint glTexture_;
    uint16 width_;
    uint16 height_;

    bool SetFilterMode_(TEXTURE_FILTER& filter) {
        if (glTexture_) {
            GLErrorCheck gl_error_check__("GLTextureExternal2D::SetFilterMode_()");
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, glTexture_);
            if (FILTER_POINT!=filter) {
                // set mag filter to linear
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                filter = FILTER_BILINEAR;
            }
            else {
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                filter = FILTER_POINT;
            }
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            return true;
        }
        return false;
    }
    bool SetAddressMode_(TEXTURE_ADDRESS& s, TEXTURE_ADDRESS& t, TEXTURE_ADDRESS& r) {
        if (glTexture_) {
            GLErrorCheck gl_error_check__("GLTextureExternal2D::SetAddressMode_()");
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, glTexture_);

            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            s = t = r = ADDRESS_CLAMP;
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
            return true;
        }
        return false;
    }

public:
    explicit GLTextureExternal2D(uint32 name):ITexture(name, 0),
        glTexture_(0),width_(0),height_(0) {}
    ~GLTextureExternal2D() {
        Destroy();
    }

    // do we really need these?
    uint16 Width() const { return width_; }
    uint16 Height() const { return height_; }
    uint16 Depth() const { return 1; }
    uint8  LODs() const { return 1; }
    uint8  AlphaBits() const { return 0; }

	GLuint GetGLTextureName() const { return glTexture_; }

    GLuint Create(uint16 width, uint16 height) {
        GLErrorCheck gl_error_check__("GLTextureExternal2D::Create()");

        // gen texture
        if (0==glTexture_) {
            glGenTextures(1, &glTexture_);
            if (0==glTexture_)
                return 0;
        }

        // succeed
        SetContext_(&glTexture_);
        
        ResetFlags_();
        SetGLExternalOESFlags_();
        SetFormatFlags_(FORMAT_UNKNOWN);

        width_ = width;
        height_ = height;

        // bind texture, set texture environment
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, glTexture_);

        // min/mag filter
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        SetFilterFlags_(FILTER_BILINEAR);

        // Initially, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_S is set to GL_REPEAT.
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        SetAddressFlags_(ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);

        // unbind
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

        return glTexture_;
    }

    void Destroy() {
        ResetContext_();
        ResetFlags_();
        if (glTexture_) {
            glDeleteTextures(1, &glTexture_);
            glTexture_ = 0;
        }
        width_ = height_ = 0;
    }
};

// create a shader for primitive rendering
class ShaderEffect;
namespace shader { struct Sampler; }
ShaderEffect* CreatePrimitiveShaderTextureOES(shader::Sampler const* &diffuseMap);

}}} // namespace mlabs::balai::graphics

#endif
#endif