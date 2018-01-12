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
 * @file    VideoTexture.h
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2017/12/13
 *
 */
#ifndef VIDEO_TEXTURE_H
#define VIDEO_TEXTURE_H

#include "AVDecoder.h"

#include "BLOpenGL.h"
#include "BLTexture.h"

namespace mlabs { namespace balai {

// forward declarations
namespace graphics {
    class ShaderEffect;
    namespace shader { struct Sampler; }
}

namespace video {

// [render thread]
class VideoTexture
{
    graphics::Texture2D* yPlane_;
    graphics::Texture2D* uvPlane_;

    //
    // Running on my GTX TITAN X (Maxwell, 900 series)
    // 1) register PBO size must be consistent.
    // 2) register PBO after first glBufferData() may fail to update texture objects.
    //
    GLuint glPBO_;
    int    cuRegSize_;

public:
    VideoTexture():yPlane_(NULL),uvPlane_(NULL),glPBO_(0),cuRegSize_(0) {}
    ~VideoTexture() {}

    bool ClearBlack();
    bool Resize(int w, int h);
    bool Initialize();
    void Finalize();

    bool Update(VideoFrame const& frame);

    bool Bind(graphics::ShaderEffect* fx,
              graphics::shader::Sampler const* mapY,
              graphics::shader::Sampler const* mapUV) const;
};

}}}
#endif