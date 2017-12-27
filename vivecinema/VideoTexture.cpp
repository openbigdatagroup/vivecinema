/*
 * Copyright (C) 2017 HTC Corporation
 *
 * Vive Cinema is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Vive Cinema is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Notice Regarding Standards. HTC does not provide a license or sublicense to
 * any Intellectual Property Rights relating to any standards, including but not
 * limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
 * AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
 * (collectively, the "Media Technologies"). For clarity, you will pay any
 * royalties due for such third party technologies, which may include the Media
 * Technologies that are owed as a result of HTC providing the Software to you.
 *
 * @file    VideoTexture.cpp
 * @author  andre chen
 * @history 2017/12/13
 *
 */
#include "VideoTexture.h"
#include "HWAccelDecoder.h"

#include "BLShader.h"

using namespace mlabs::balai::graphics;

namespace mlabs { namespace balai { namespace video {

class PBOTexture : public mlabs::balai::graphics::Texture2D
{
    GLuint glTexture_;

    // fixed, not changable
    bool SetFilterMode_(TEXTURE_FILTER&) { return false; }
    bool SetAddressMode_(TEXTURE_ADDRESS&, TEXTURE_ADDRESS&, TEXTURE_ADDRESS&) {
        return false;
    }

public:
    explicit PBOTexture(uint32 name):Texture2D(name),glTexture_(0) {}
    ~PBOTexture() {
        ResetContext_();
        ResetFlags_();
        if (glTexture_) {
            glDeleteTextures(1, &glTexture_);
            glTexture_ = 0;
        }
    }

    bool UpdateImage(uint16 width, uint16 height, TEXTURE_FORMAT fmt, void const* data_offset, bool) {
        GLErrorCheck gl_error_check__("PBOTexture::UpdateImage()");
        assert(0<width && 0<height);
        assert(FORMAT_I8==fmt || FORMAT_IA8==fmt);

        GLint internalformat = 0;
        GLenum format = 0;
        int stride = width;

        if (FORMAT_I8==fmt) {
            assert(0==data_offset);
            internalformat = GL_RED;
            format = GL_RED;
        }
        else {
            internalformat = GL_RG;
            format = GL_RG;
            stride = width*2;
        }

        //
        // this warning is respect to glPixelStorei(GL_UNPACK_ALIGNMENT, n); how pixel data is read from client memory
        // Specifies the alignment requirements for the start of each pixel row in memory, default value is 4.
        assert(0==(stride&3));

        // gen texture
        bool reset_flags = false;
        if (0==glTexture_) {
            glGenTextures(1, &glTexture_);
            if (0==glTexture_)
                return false;
            SetContext_(&glTexture_);
            reset_flags = true;
        }

        // bind texture, specify texture image
        glBindTexture(GL_TEXTURE_2D, glTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_UNSIGNED_BYTE, data_offset);
        if (gl_error_check__.Report()) {
            glBindTexture(GL_TEXTURE_2D, 0);
            return false;
        }
 
        width_ = width;
        height_ = height;

        if (reset_flags) {
            ResetFlags_();
            SetFormatFlags_(fmt);

            // bilinear
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            SetFilterFlags_(FILTER_BILINEAR);

            // clamp
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            SetAddressFlags_(ADDRESS_CLAMP, ADDRESS_CLAMP, ADDRESS_CLAMP);
        }

        // unbind
        glBindTexture(GL_TEXTURE_2D, 0);

        return true;
    }
};
//---------------------------------------------------------------------------------------
bool VideoTexture::Initialize()
{
    //
    // set register size -1 to mark the PBO is never cuda-registered.
    // as testing on GTX TITAN X (Maxwell) show, if glPBO is filled with glBufferDate()
    // but not yet CUDA-registered (before unbind, see ClearBlack() with cuRegSize_<=0).
    // texture object may fail to update after few frames.
    cuRegSize_ = -1;

    glGenBuffers(1, &glPBO_);
    if (glPBO_) {
        yPlane_ = new PBOTexture(0);
        uvPlane_ = new PBOTexture(1);
        // CUDA may not be ready yet...
        return Resize(1920, 1080);
        //return ClearBlack();
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoTexture::ClearBlack()
{
    if (glPBO_ && yPlane_ && uvPlane_) {
        int w = yPlane_->Width();
        int h = yPlane_->Height();

        if (cuRegSize_>0) {
            if (cuRegSize_!=(w*h*3/2)) {
                hwaccel::UnregisterGraphicsObject(&glPBO_);
                cuRegSize_ = 0;
            }
        }
        else {
            if (w<1920) w = 1920;
            if (h<1080) h = 1080;
        }

        int const pixel_size = w*h;
        int const frame_size = pixel_size*3/2;

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glPBO_);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, frame_size, NULL, GL_DYNAMIC_DRAW);
        uint8* dst = (uint8*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

        // fill black pixels : Y=16(studio swing), U=V=128(Y=U=V=0 is greenish color)
        if (dst) {
            memset(dst, 0x10, pixel_size);
            memset(dst+pixel_size, 0x80, pixel_size/2);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }
        else {
            dst = (uint8*) malloc(frame_size);
            if (dst) {
                memset(dst, 0x10, pixel_size);
                memset(dst+pixel_size, 0x80, pixel_size/2);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, frame_size, dst, GL_DYNAMIC_DRAW);
                free(dst); dst = NULL;
            }
        }

        yPlane_->UpdateImage((uint16)w, (uint16)h, FORMAT_I8, 0);
        uvPlane_->UpdateImage((uint16)w/2, (uint16)h/2, FORMAT_IA8, ((char const*)NULL)+pixel_size);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoTexture::Update(VideoFrame const& frame)
{
    //
    // for spherical videos, we can still update only subrect if viewing angle is known...
    //
    if (NULL!=frame.FramePtr && NULL!=yPlane_ && NULL!=uvPlane_ && frame.Width>1 && frame.Height>1) {
        int const pixel_size = frame.Width*frame.Height;
        int const frame_size = pixel_size*3/2;
        if (VideoFrame::NV12_CUDA==frame.Type) {
            assert(frame_size==cuRegSize_);
            hwaccel::CudaCopyVideoFrame(glPBO_, frame);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glPBO_);
        }
        else {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glPBO_);

            /*** faster ***/
            glBufferData(GL_PIXEL_UNPACK_BUFFER, frame_size, (const GLvoid*) frame.FramePtr, GL_DYNAMIC_DRAW);

            /*** slower ***
            void* dst = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (dst) {
                memcpy(dst, (void const*) frame.FramePtr, frame_size);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
            *** slower ***/
        }

        yPlane_->UpdateImage((uint16)frame.Width, (uint16)frame.Height, FORMAT_I8, 0);
        uvPlane_->UpdateImage(uint16(frame.Width/2), uint16(frame.Height/2), FORMAT_IA8, ((char const*)NULL)+pixel_size);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
void VideoTexture::Finalize()
{
    if (glPBO_) {
        if (0<cuRegSize_) {
            hwaccel::UnregisterGraphicsObject(&glPBO_);
            cuRegSize_ = 0;
        }
        glDeleteBuffers(1, &glPBO_);
        glPBO_ = 0;
    }
    BL_SAFE_RELEASE(yPlane_);
    BL_SAFE_RELEASE(uvPlane_);
}
//---------------------------------------------------------------------------------------
bool VideoTexture::Bind(mlabs::balai::graphics::ShaderEffect* fx,
                        mlabs::balai::graphics::shader::Sampler const* mapY,
                        mlabs::balai::graphics::shader::Sampler const* mapUV) const
{
    if (fx && mapY && mapUV && yPlane_ && uvPlane_) {
        fx->BindSampler(mapY, yPlane_);
        fx->BindSampler(mapUV, uvPlane_);
        return true;
    }
    return false;
}

}}} // namespace mlabs::balai::video