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
 * @file    HWAccelDecoder.h
 * @author  andre chen
 * @history 2017/08/22 created
 *
 */
#include "BLCore.h"

extern "C" {
#include "libavformat/avformat.h"
}

namespace mlabs { namespace balai { namespace video {

// 3rd party/external video decoder
class VideoDecoderImp
{
    AVCodecID const codec_id_;

    BL_NO_DEFAULT_CTOR(VideoDecoderImp);
    BL_NO_COPY_ALLOW(VideoDecoderImp);

protected:
    VideoDecoderImp(AVCodecID cid):codec_id_(cid) {}
    virtual ~VideoDecoderImp() {}

public:
    AVCodecID CodecID() const { return codec_id_; }

    virtual char const* Name() const = 0;
    virtual bool Resume() = 0;
    virtual bool Pause() = 0;
    virtual void Flush() = 0;

    // decoding process
    virtual bool can_send_packet() = 0;
    virtual bool can_receive_frame(int64_t& pts, int64_t& duration) = 0;
    virtual bool send_packet(AVPacket const& pkt) = 0;
    virtual bool receive_frame(uint8_t* nv12, int width, int height) = 0;
    virtual bool discard_frame() = 0;

    // factory
    static VideoDecoderImp* New(AVStream const* stream, int width, int height);
    static void Delete(VideoDecoderImp* decoder);
};

// HW context
namespace hwaccel {
// [render thread]
// init context after graphics context is created.
// device should be object point to IDirect3DDevice9, ID3D11Device or NULL(OpenGL)
// Preprocessor either INIT_CUDA_GL, INIT_CUDA_D3D9 or INIT_CUDA_D3D11 is mandatory.
// (not actually test D3D/cuvid interop yet!)
int InitContext(void* device);

// [render thread]
void DeinitContext();

// how is GPU accelerated
int GPUAccelScore();
}

}}}
