/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2018
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
 * @file    Pan360.h
 * @author  andre chen
 * @history 2018/05/15 created
 *
 */
#ifndef PAN360_VIEWER_H
#define PAN360_VIEWER_H

#include "SystemFont.h"

#include "BLVR.h"
#include "BLGLShader.h"
#include "BLOpenGL.h"
#include "BLColor.h"

#include<thread>
#include<mutex>

using namespace mlabs::balai;

namespace htc {

class Pan360
{
    // target image path
    wchar_t path_[MAX_PATH];

    // current image filename
    wchar_t image_name_[MAX_PATH];
    wchar_t texture_name_[MAX_PATH];
    FILETIME filetime_;

    // image decoding thread
    std::thread decodeThread_;

    // graphics objects
    mlabs::balai::graphics::ShaderEffect* fontFx_; // simple font rendering shader, less aliasing
    mlabs::balai::graphics::ShaderEffect* subtitleFx_;
    mlabs::balai::graphics::shader::Constant const* subtitleFxCoeff_;
    mlabs::balai::graphics::shader::Constant const* subtitleFxShadowColor_;
    mlabs::balai::graphics::ShaderEffect* pan360_; // how could you not fall in love with pan360?
    mlabs::balai::graphics::shader::Constant const* pan360Crop_;
    mlabs::balai::graphics::shader::Sampler const* pan360Map_;
    mlabs::balai::graphics::Texture2D* glyph_;
    mlabs::balai::graphics::Texture2D* equiRect_;

    // VR manager
    mlabs::balai::VR::Manager& vrMgr_;

    // text blitter
    enum GLYPH {
        GLYPH_PATH = 0,
        GLYPH_LOADING,
        GLYPH_IMAGE_NAME,
        GLYPH_IMAGE_DESC,

        GLYPH_TOTALS,
    };
    mlabs::balai::system::TextBlitter textBlitter_;
    struct TexCoord { int s0, t0, s1, t1; } glyphRects_[GLYPH_TOTALS];
    uint8* glyphBuffer_;
    int    glyphBufferWidth_;
    int    glyphBufferHeight_;

    // it works perfectly!
    struct Tetrahedron {
        GLuint vao_, vbo_;
        Tetrahedron():vao_(0),vbo_(0) {}
        ~Tetrahedron() { Destroy(); }
        bool Create() {
            if (0!=vao_) {
                assert(0!=vbo_);
                return true;
            }

            assert(0==vbo_);
            glGenVertexArrays(1, &vao_);
            if (0==vao_) {
                return false;
            }

            glGenBuffers(1, &vbo_);
            if (0==vbo_) {
                glDeleteVertexArrays(1, &vao_); vao_ = 0;
                return false;
            }

            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            // vertex buffer
            math::Vector3 const v1( sqrt(8.0f/9.0f),             0.0f, -1.0f/3.0f);
            math::Vector3 const v2(-sqrt(2.0f/9.0f),  sqrt(2.0f/3.0f), -1.0f/3.0f);
            math::Vector3 const v3(-sqrt(2.0f/9.0f), -sqrt(2.0f/3.0f), -1.0f/3.0f);
            math::Vector3 const v4(0.0f, 0.0f, 1.0f); // apex
            math::Vector3 const vertices[12] = {
                v1, v2, v3, // base triangle
                v4, v1, v3,
                v4, v3, v2,
                v4, v2, v1,
            };

            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (GLvoid const*)0);

            // unbind all
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            return true;
        }
        void Destroy() {
            if (vao_) { glDeleteVertexArrays(1, &vao_); }
            if (vbo_) { glDeleteBuffers(1, &vbo_); }
            vao_ = vbo_ = 0;
        }
        bool Draw() const {
            if (vao_) {
                glBindVertexArray(vao_);
                glDrawArrays(GL_TRIANGLES, 0, 12);
                return true;
            }
            return false;
        }
    } envelope_;

    // HMD orientation
    mlabs::balai::math::Matrix3 hmd_xform_;

    // font rendering transform
    mlabs::balai::math::Matrix3 glyph_xform_;

    // to center 360 video plus user control(trigger+swipe)
    float azimuth_adjust_;

    // stereoscopic 3D
    int s3D_;

    // image index in the path
    std::mutex imageMutex_;
    void* image_pixels_;
    int image_width_;
    int image_height_;
    int image_components_;
    int session_id_;
    int image_index_;
    int image_totals_;
    int image_loading_;

    float image_loadtime_;

    // VR dashboard interrupt
    int dashboard_interrupt_;

    // quit app if quit_count_down_ to 0, e.g.
    // set to 90*10 if you want to quit app after 10 seconds.
    int quit_count_down_;

    // is lost tracking
    int lost_tracking_;

    // VR interrupt handler
    void VRInterrupt_(mlabs::balai::VR::VR_INTERRUPT e, void*) {
        switch(e)
        {
        case mlabs::balai::VR::VR_INTERRUPT_PAUSE:
            dashboard_interrupt_ = 1;
            break;

        case mlabs::balai::VR::VR_INTERRUPT_RESUME:
            dashboard_interrupt_ = 0;
            break;

        case mlabs::balai::VR::VR_QUIT:
            quit_count_down_ = 1; // quit at once
            break;
        }
    }

    void AlignGlyphTransform_();

    // png/jpg decoding loop
    void ImageDecodeLoop_();

    BL_NO_COPY_ALLOW(Pan360);

public:
    Pan360();
    ~Pan360() {}

    mlabs::balai::graphics::ITexture* GetEquirectangularMap() const {
        mlabs::balai::graphics::ITexture* tex = equiRect_;
        tex->AddRef();
        return tex;
    }

    bool ReadConfig(wchar_t const* xml,
                    char* win_title, int max_win_title_size,
                    int& width, int& height);

    bool IsLoading() const { return 0!=image_loading_; }
    int IsLostTracking() const { return lost_tracking_; }
    void ToggleStereoscopic3D() { if (0==image_loading_) s3D_ = (s3D_+1)%3;}
    char const* Stereoscopic3D() const {
        return (0!=s3D_) ? ((1==s3D_) ? "top-bottom":"left-right"):"mono";
    }

    // adjust azimuth angle
    void AdjustAzimuthAngle(float angle) { azimuth_adjust_ += angle; }

    // image change
    void ToggleImageChange(int add) {
        if (0==image_loading_) {
            std::unique_lock<std::mutex> lock(imageMutex_);
            if (0==add) {
                image_index_ = 0;
            }
            else if (0x12345678==add) {
                image_index_ = -1;
            }
            else {
                image_index_ += add;
            }
            ++session_id_;
            image_loading_ = 1;
            AlignGlyphTransform_();
        }
    }

    // Initialize()/Finalize()/FrameUpdate()/FrameMove()/Render() run on render thread
    bool Initialize();
    void Finalize();
    bool FrameMove();
    bool Render(mlabs::balai::VR::HMD_EYE eye) const;

    float DrawInfo(mlabs::balai::graphics::Color const& color, float x0, float y0, float size) const;
};

}
#endif