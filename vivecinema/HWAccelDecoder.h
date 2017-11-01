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

// mutex
#include <mutex>

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/hwcontext.h"  // AVHWDeviceType, av_hwframe_transfer_data();
}

inline bool is_hwaccel_fmt_compatible(AVHWDeviceType type, AVPixelFormat pix_fmt) {
    if (AV_HWDEVICE_TYPE_CUDA==type) {
        return (AV_PIX_FMT_CUDA==pix_fmt);
    }
    else if (AV_HWDEVICE_TYPE_D3D11VA==type) {
        //return (AV_PIX_FMT_D3D11==pix_fmt) || (AV_PIX_FMT_D3D11VA_VLD==pix_fmt) || (AV_PIX_FMT_DXVA2_VLD==pix_fmt);
        //return (AV_PIX_FMT_D3D11==pix_fmt) || (AV_PIX_FMT_D3D11VA_VLD==pix_fmt);
        return (AV_PIX_FMT_D3D11==pix_fmt);
    }
    else if (AV_HWDEVICE_TYPE_DXVA2==type) {
        //return (AV_PIX_FMT_DXVA2_VLD==pix_fmt) || (AV_PIX_FMT_D3D11==pix_fmt) || (AV_PIX_FMT_D3D11VA_VLD==pix_fmt);
        return (AV_PIX_FMT_DXVA2_VLD==pix_fmt);
    }
    else if (AV_HWDEVICE_TYPE_VAAPI==type) {
        return (AV_PIX_FMT_VAAPI==pix_fmt);
    }
    else if (AV_HWDEVICE_TYPE_VDPAU==type) {
        return (AV_PIX_FMT_VDPAU==pix_fmt);
    }
    else if (AV_HWDEVICE_TYPE_VIDEOTOOLBOX==type) { // apple
        return (AV_PIX_FMT_VIDEOTOOLBOX==pix_fmt);
    }
    else if (AV_HWDEVICE_TYPE_QSV==type) {
        return (AV_PIX_FMT_QSV==pix_fmt);
    }
    return false;
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

public:
    virtual ~VideoDecoderImp() {}
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
};

struct DecoderConfig {
    VideoDecoderImp* ExtDecoder;
    AVCodec const*   Codec;
    AVHWDeviceType   HWDeviceType;
    int              Options;
};

// manager
class FFmpegHWAccelInitializer
{
    enum {
        MAX_FFMPEG_HWACCELS_SUPPORTS = 16,
        MAX_HWACCEL_CODECS = 16 // must > cudaVideoCodec_NumCodecs(11)
    };

    // decode max size
    struct MinMaxSize {
        int MaxWidth, MaxHeight;
        int MinWidth, MinHeight;
    };

    // FFmpeg - to utilize AVHWDevice, you must confirm the relate ffmpeg hw build options are on.
    AVHWDeviceType hwAccels_[MAX_FFMPEG_HWACCELS_SUPPORTS];
    MinMaxSize     cuvidDecoderCap_[MAX_HWACCEL_CODECS];
    std::mutex mutex_;
    int availableFFmpegAVHWDevices_;
    int cuvidDeviceIndex_;
    int amfInit_;
    int activeDecoders_, totalDecoders_;
    int prefer_decoder_; // 0:internal(FFmpeg), 1:external(AMF/NEDEC), 2+:external + hw accel support.

public:
    FFmpegHWAccelInitializer();
    ~FFmpegHWAccelInitializer();

    int AddReferenceCount();
    void RemoveReferenceCount();

    // return false if not tweakable.
    bool TogglePreferVideoDecoder(int options, AVStream const* stream, int width, int height);

    // create decoder
    bool FindVideoDecoder(DecoderConfig& dconfig, AVStream const* stream, int width, int height);
};

}}}