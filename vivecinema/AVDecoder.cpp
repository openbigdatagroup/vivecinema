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
 * @file    AVDecoder.cpp
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2015/12/09 created
 *
 */
#include "AVDecoder.h"
#include "HWAccelDecoder.h"

//
// Important Note to utilize FFmpeg's hardware decoders/accelerators(AVHWDeviceType):
//  1. it depends on the FFmpeg build options. check avutil_configuration().
//  2. prefer NVDEC or AMF, since these implemantaions are fully on me/you!
//  3. prefer specialized HW AVCodec over general AVCodec + AVHWAccel.
//  4. check more HEVC videos(CUDA/D3D11VA) to see if it really works.
//  5. It works on GFX 1080 Ti, although with warning messages and crashes(when switching decoder) sometimes
//
// If you have better implementation on this, kindly let me know. andre.hl.chen@gmail.com.
// ~~~ much appreciated. ~~~
//
// disable temporarily
//#define ENABLE_FFMPEG_HARDWARE_ACCELERATED_DECODER
//

extern "C" {
// Whenever av_gettime() is used to measure relative period of time,
// av_gettime_relative() is prefered as it guarantee monotonic time
// on supported platforms(check av_gettime_relative_is_monotonic())
// On platforms that support it, the time comes from a monotonic clock.
// This property makes this time source ideal for measuring relative time.
// If a monotonic clock is not available on the targeted platform,
// the implementation fallsback on using av_gettime().
//
// Important Note : av_gettime_relative() don't work with network stream.
//                  still not knowing why...
//
// But to use av_gettime() seems good enough.
// 
#include "libavutil/time.h"     // av_usleep()
#include "libavutil/imgutils.h" // av_image_get_buffer_size()
#include "libswresample/swresample.h" // SwrContext()
#include "libavutil/opt.h"      // av_opt_set_int
}

//#include <future> // std::async, std::future
//#include <chrono> // condition_variable::wait_for()
/*
    // C++11
    using namespace std::chrono;
    std::chrono::time_point<std::chrono::system_clock> time_start_;
    std::chrono::time_point<std::chrono::system_clock> last_time_;
    time_start_ = std::chrono::high_resolution_clock::now();
    last_time_ = time_start_ - std::chrono::microseconds(long long(1000000/frame_rate));
    long long tt = duration_cast<std::chrono::milliseconds>(last_time_ - time_start_).count();
*/
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

#ifdef HTC_VIVEPORT_RELEASE
#ifdef BL_AVDECODER_VERBOSE
#undef BL_AVDECODER_VERBOSE
#endif

#ifdef DEBUG_PACKET_READING_BENCHMARK
#undef DEBUG_PACKET_READING_BENCHMARK
#endif
#ifdef DEBUG_SUBTITLE_DECODE_BENCHMARK
#undef DEBUG_SUBTITLE_DECODE_BENCHMARK
#endif
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
#undef DEBUG_AUDIO_DECODE_BENCHMARK
#endif

#else
//#define DEBUG_PACKET_READING_BENCHMARK
//#define DEBUG_SUBTITLE_DECODE_BENCHMARK
//#define DEBUG_AUDIO_DECODE_BENCHMARK
#endif

//
// audio buffer size
//
// for 48000 Hz, stereo, fltp :
//   1 second buffer size = stereo * sample_rate * sample data type
//                        = 2 * 48000 * 4 = 384000 = 375 KB
//
// for 48000 Hz, 5.1, fltp :
//   1 second buffer size = (5+1) * 48000 * 4 = 1152000 = 1125 KB
//
// for 48000 Hz, FOA, fltp :
//   1 second buffer size = 4 * 48000 * 4 = 768000 = 750 KB
//
// for 48000 Hz, TBE, fltp :
//   1 second buffer size = 8 * 48000 * 4 = 1536000 = 1500 KB
//
// for 48000 Hz, 2nd order ambisonics, fltp :
//   1 second buffer size = 9 * 48000 * 4 = 1728000 = 1687.5 KB
//
// for 48000 Hz, FB360(TBE+stereo headlock), fltp :
//   1 second buffer size = 10 * 48000 * 4 = 1920000 = 1875 KB
//
// for 48000 Hz, 3rd order Ambisonics, fltp :
//   1 second buffer size = 16 * 48000 * 4 = 3072000 = 3000 KB
//
// for 48000 Hz, 3rd order Ambisonics + stereo headlock, fltp :
//   1 second buffer size = 18 * 48000 * 4 = 3456000 = 3375 KB
//
// the normal audio buffering is about few hundreds milliseconds.
//
#define INIT_AUDIO_BUFFER_SIZE 1920000

// AVIO (customized stream source) buffer default size
#define AVIO_BUFFER_DEFAULT_SIZE 4096

// audio time fade in, nanoseconds
int const AUDIO_FADE_IN_TIME = (AV_TIME_BASE*1);

// #define AV_TIME_BASE_Q          (AVRational){1, AV_TIME_BASE}
static AVRational const timebase_q = { 1, AV_TIME_BASE };

using namespace std;

//---------------------------------------------------------------------------------------
AVMediaType FFmpeg_Codec_Type(AVStream const* s) {
    return (s && s->codecpar) ? (s->codecpar->codec_type):AVMEDIA_TYPE_UNKNOWN;
}
//---------------------------------------------------------------------------------------
//
// since FFmpeg version:20160412-git-196cfc2, AVCodecContext::active_thread_type
// is set as 0 by avcodec_open2(). (different from last build 20160409-git-0c90b2e).
// it needs set active_thread_type explicitly via an 'options'. Or extra 50ms will lost,
// and break our decoding process.
//
//
// Using threaded decoding by default breaks backward compatibility if
// AVHWAccel is used or if an appliction sets threadunsafe callbacks.
//
// https://lists.ffmpeg.org/pipermail/ffmpeg-cvslog/2012-January/046142.html
//
AVCodecContext* FFmpeg_AVCodecOpen(AVStream const* stream)
{
    if (NULL!=stream && NULL!=stream->codecpar) {
        AVCodecParameters* codecpar = stream->codecpar;
        AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (NULL!=codec) {
            AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
            if (NULL!=codecCtx) {
                if (0<=avcodec_parameters_to_context(codecCtx, codecpar)) {
                    AVDictionary* codec_opts = NULL;
                    av_dict_set(&codec_opts, "threads", "auto", 0);
                    //av_opt_set_int(codecCtx, "refcounted_frames", 1, 0);
                    if (0==avcodec_open2(codecCtx, codec, &codec_opts)) {
                        if (NULL!=codec_opts) {
                            av_dict_free(&codec_opts);
                            codec_opts = NULL;
                        }
                        if (AVMEDIA_TYPE_VIDEO==codecCtx->codec_type && 
                            0==(codecCtx->active_thread_type&FF_THREAD_FRAME)) {
                            // happens to be raw video!?
                            char codecInfo[256];
                            avcodec_string(codecInfo, 256, codecCtx, false);
                            BL_ERR("\n** video : codecCtx->active_thread_type=%d video decoding could be slow!!!\n(%s)\n\n",
                                   codecCtx->active_thread_type, codecInfo);
                        }

                        // [heck!] fix channel layout that may come out from some nasty encoders
                        // e.g. "audio : channel layout changed : 0x0->0x18003F737(ch:16)" (hexadecagonal)
                        if (AVMEDIA_TYPE_AUDIO==codecCtx->codec_type) {
                            assert(codecCtx->channels>0); // what the!?
                            if (codecCtx->channels!=av_get_channel_layout_nb_channels(codecCtx->channel_layout)) {
                                int64_t const channel_layout = av_get_default_channel_layout(codecCtx->channels);
                                BL_LOG("audio : channel layout changed : 0x%llX->0x%llX(ch:%d)\n",
                                       codecCtx->channel_layout, channel_layout, codecCtx->channels);
                                codecCtx->channel_layout = channel_layout;
                            }
                        }

                        av_codec_set_pkt_timebase(codecCtx, stream->time_base);
                        // av_codec_get_pkt_timebase(codecCtx) and codecCtx->time_base are not the same!!!
                        // assert(0==av_cmp_q(av_codec_get_pkt_timebase(codecCtx), codecCtx->time_base));
                        return codecCtx;
                    }

                    if (NULL!=codec_opts) {
                        av_dict_free(&codec_opts);
                        codec_opts = NULL;
                    }
                }
                avcodec_free_context(&codecCtx);
            }
        }
    }
    return NULL;
}
//---------------------------------------------------------------------------------------
bool FFmpegSeekWrapper(AVFormatContext* formatCtx, int timestamp, int streamId)
{
    //
    // 2 flavors of video seeking - avformat_seek_file() and av_seek_frame()
    //
    // #define AVSEEK_FLAG_BACKWARD 1 ///< seek backward
    // #define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
    // #define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non-keyframes
    // #define AVSEEK_FLAG_FRAME    8 ///< seeking based on frame number
    //
    // int av_seek_frame(AVFormatContext *s,
    //                   int stream_index,
    //                   int64_t timestamp,
    //                   int flags);
    //
    // int avformat_seek_file(AVFormatContext *s,
    //                        int stream_index,
    //                        int64_t min_ts, int64_t ts, int64_t max_ts,
    //                        int flags);
    //
    // Note :
    // 1) avformat_seek_file is part of the new seek API and is still under construction.
    //    Thus do not use this yet. It may change at any time, do not expect
    //    ABI compatibility yet!
    //
    // 2) to use AVSEEK_FLAG_ANY could be bad...
    //
    // 3) If flags contain AVSEEK_FLAG_BYTE, then all timestamps are in bytes and
    //    are the file position (this may not be supported by all demuxers).
    //
    // 4) If flags contain AVSEEK_FLAG_FRAME, then all timestamps are in frames
    //    in the stream with stream_index (this may not be supported by all demuxers).
    //
    // 5) Otherwise all timestamps are in units of the stream selected by stream_index
    //    or if stream_index is -1, in AV_TIME_BASE units.
    //
    // 6) If flags contain AVSEEK_FLAG_ANY, then non-keyframes are treated as keyframes
    //    (this may not be supported by all demuxers).
    //
    // 7) avformat_seek_file - Seeking will be done so that the point from which
    //    all active streams can be presented successfully will be closest to ts
    //    and within min/max_ts. Active streams are all streams that have
    //    AVStream.discard < AVDISCARD_ALL.
    //
    //
    // 2016.08.30 by tracing the code(FFmpeg 3.1), we have
    //  formatCtx_->iformat->read_seek2 = NULL;
    //  formatCtx_->iformat->read_seek != NULL;
    // means it likely use av_seek_frame();
    //
    // if (formatCtx_->iformat->read_seek2 && !formatCtx_->iformat->read_seek) {
    //    // av_seek_frame() call avformat_seek_file()
    // }
    //
    //  AVSEEK_FLAG_BACKWARD is ignored by avformat_seek_file()
    //  AVSEEK_FLAG_BYTE  may not be supported by all demuxers
    //  AVSEEK_FLAG_ANY   may lead to non-complete frame
    //  AVSEEK_FLAG_FRAME may not be supported by all demuxers
    //
    //  will only use AVSEEK_FLAG_BACKWARD if av_seek_frame()
    //
    assert(NULL!=formatCtx && 0<=timestamp);
    if (NULL==formatCtx || timestamp<0)
        return false;

#ifdef BL_AVDECODER_VERBOSE
    int64 const t0 = av_gettime_relative();
    int const ms = timestamp%1000;
    int ss = timestamp/1000;
    int mm = ss/60; ss %= 60;
    int hh = mm/60; mm %= 60;
#endif

    int64_t ts = int64_t(timestamp)*1000;
    if (-1!=streamId) {
        if (0<=streamId && streamId<(int)formatCtx->nb_streams) {
            ts = av_rescale_q(ts, timebase_q, formatCtx->streams[streamId]->time_base);
        }
        else {
            streamId = -1;
        }
    }

#define TRY_AVFORMAT_SEEK_FILE
#ifdef TRY_AVFORMAT_SEEK_FILE
    int64_t min_ts(INT64_MIN), max_ts(INT64_MAX);
    if (ts<=0) {
        max_ts = ts = 0;
    }
    else {
        min_ts = ts;
    }
    if (0<=avformat_seek_file(formatCtx, streamId, min_ts, ts, max_ts, 0)) {
#else
    int seek_flags = 0;
    if (ts<=0) {
        seek_flags = AVSEEK_FLAG_BACKWARD;
        ts = 0;
    }
    if (0<=av_seek_frame(formatCtx, streamId, ts, seek_flags)) {
#endif

#ifdef BL_AVDECODER_VERBOSE
        int64 const t1 = av_gettime_relative();
        if (t1>(t0+50000)) {
            BL_LOG("FFmpegSeek takes %dms at %02d:%02d:%02d:%03d\n",
                   (t1-t0)/1000, hh, mm, ss, ms);
        }
#endif
        return true;
    }

#ifdef BL_AVDECODER_VERBOSE
    BL_LOG("Failed!!! FFmpegSeek takes %dms at %02d:%02d:%02d:%03d\n",
           (av_gettime_relative()-t0)/1000, hh, mm, ss, ms);
#endif

    return false;
}

// quick helper
inline AVSampleFormat GetAVSampleFormat(AUDIO_FORMAT fmt) {
    if (AUDIO_FORMAT_F32==fmt) {
        return AV_SAMPLE_FMT_FLT;
    }
    else if (AUDIO_FORMAT_S32==fmt) {
        return AV_SAMPLE_FMT_S32;
    }
    else if (AUDIO_FORMAT_S16==fmt) {
        return AV_SAMPLE_FMT_S16;
    }
    else if (AUDIO_FORMAT_U8==fmt) {
        return AV_SAMPLE_FMT_U8;
    }
    return AV_SAMPLE_FMT_NONE;
}

int FFmpegAudioResample(uint8* dst, int max_size, int dstSampleRate, AUDIO_FORMAT dstFmt,
                        uint8 const* src, int srcSampleRate, AUDIO_FORMAT srcFmt, int channels, int src_samples)
{
    AVSampleFormat const sfmt = GetAVSampleFormat(srcFmt);
    AVSampleFormat const dfmt = GetAVSampleFormat(dstFmt);
    if (src && dst && channels>0 && src_samples>0 && AV_SAMPLE_FMT_NONE!=sfmt && AV_SAMPLE_FMT_NONE!=dfmt) {
        int64_t const channel_layout = av_get_default_channel_layout(channels);
        SwrContext* swrCtx = swr_alloc_set_opts(NULL,
                                     channel_layout, dfmt, dstSampleRate,
                                     channel_layout, sfmt, srcSampleRate, 0, NULL);
        if (NULL!=swrCtx) {
            int samples = 0;
            if (swr_init(swrCtx)>=0) {
                uint8_t const* in[] = { src, NULL, NULL, NULL };
                int const out_samples_max = max_size/(channels*av_get_bytes_per_sample(dfmt));
                samples = swr_convert(swrCtx, &dst, out_samples_max, in, src_samples);
            }

            swr_free(&swrCtx);
            return samples;
        }
    }
    return 0;
}

inline bool is_hwaccel_pix_fmt(AVPixelFormat pix_fmt) {
    AVPixFmtDescriptor const* desc = av_pix_fmt_desc_get(pix_fmt);
    return (NULL!=desc) && (0!=(desc->flags & AV_PIX_FMT_FLAG_HWACCEL));
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

// FFmpeg codec initializer & management
class FFmpegInitializer
{
    enum { MAX_FFMPEG_HWACCELS_SUPPORTS = 30 };

    // FFmpeg - to utilize AVHWDevice, you must confirm the relate ffmpeg hw build options are on.
    AVHWDeviceType hwAccels_[MAX_FFMPEG_HWACCELS_SUPPORTS];
    std::mutex mutex_;
    int   numAVHWDevices_;
    uint8 inited_;

public:
    FFmpegInitializer():mutex_(),numAVHWDevices_(0),inited_(0) {
        for (int i=0; i<MAX_FFMPEG_HWACCELS_SUPPORTS; ++i) {
            hwAccels_[i] = AV_HWDEVICE_TYPE_NONE;
        }

        // initialize FFmpeg - register all AVCodecs and HWAccels...
        av_register_all();

#ifdef ENABLE_FFMPEG_HARDWARE_ACCELERATED_DECODER
        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        AVBufferRef* hw_device_ctx = NULL;
        while (numAVHWDevices_<MAX_FFMPEG_HWACCELS_SUPPORTS &&
               AV_HWDEVICE_TYPE_NONE!=(type=av_hwdevice_iterate_types(type))) {
            if (av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0)>=0) {
                hwAccels_[numAVHWDevices_++] = type;
                av_buffer_unref(&hw_device_ctx);
            }
        }
#endif

#ifdef BL_AVDECODER_VERBOSE
        BL_LOG("**\n** FFmpeg version : %X(%s)\n", avcodec_version(), av_version_info());
        BL_LOG("** License : %s\n", avutil_license());
        BL_LOG("** Build config : %s\n", avutil_configuration());
        if (numAVHWDevices_>0) {
            BL_LOG("** HWAccel :");
            for (int i=0; i<numAVHWDevices_; ++i) {
                BL_LOG(" %s", av_hwdevice_get_type_name(hwAccels_[i]));
            }
            BL_LOG("\n**\n");
        }
#endif

#if 0
        AVCodec* codec = NULL;
        while (NULL!=(codec = av_codec_next(codec))) {
            if (AVMEDIA_TYPE_VIDEO==codec->type && av_codec_is_decoder(codec)) {
                BL_LOG("AVCodec:%s | %s", codec->name, codec->long_name);
                if (codec->pix_fmts) {
                    BL_LOG(" | ");
                    for (AVPixelFormat const* fmt=codec->pix_fmts; AV_PIX_FMT_NONE!=*fmt; ++fmt) {
                        BL_LOG(" %s", av_get_pix_fmt_name(*fmt));
                    }
                }
                BL_LOG("\n");
            }
        }

        AVHWAccel* hwaccel = NULL;
        while (NULL!=(hwaccel = av_hwaccel_next(hwaccel))) {
            if (AVMEDIA_TYPE_VIDEO==hwaccel->type) {
                BL_LOG("AVHWAccel:%s | %s(%d) | %s\n", hwaccel->name, av_get_pix_fmt_name(hwaccel->pix_fmt), hwaccel->pix_fmt, is_hwaccel_pix_fmt(hwaccel->pix_fmt)? "HW":"SW");
            }
        }
#endif
    }
    ~FFmpegInitializer() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (inited_) {
            avformat_network_deinit();
            inited_ = 0;
        }
    }

    void Init() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!inited_ && 0==avformat_network_init()) {
            inited_ = 1;
        }
    }

    AVHWDeviceType GetHWAccelType(AVCodec const*& hwcodec, uint32& flags, AVCodecID codec_id, int options) {
        std::lock_guard<std::mutex> lock(mutex_);

        hwcodec = NULL;
        flags = 0;
        AVHWDeviceType hwtype = AV_HWDEVICE_TYPE_NONE;
        for (int i=0; i<numAVHWDevices_; ++i) {
            AVHWDeviceType const type = hwAccels_[i];
            assert(AV_HWDEVICE_TYPE_NONE!=type);

            //
            // Look for AVCodec first!
            //
            // refer libavcodec/allcodecs.c, register_all() for all possible codecs.
            // eg, libavcodec/cuvid.c
            //
            AVCodec const* codec = NULL;
            while (NULL!=(codec=av_codec_next(codec))) {
                if (codec_id==codec->id && av_codec_is_decoder(codec) &&
                    codec->pix_fmts && is_hwaccel_fmt_compatible(type, codec->pix_fmts[0])) {
                    flags |= uint32(1<<i);
                    break;
                }
            }

            if (NULL==codec) {
                AVHWAccel* hwaccel = NULL;
                while (NULL!=(hwaccel=av_hwaccel_next(hwaccel))) {
                    if (codec_id==hwaccel->id && is_hwaccel_fmt_compatible(type, hwaccel->pix_fmt)) {
                        flags |= uint32(1<<i);
                        break;
                    }
                }
            }

            if (options==i && (flags&uint32(1<<i))) {
                hwcodec = codec;
                hwtype = type;
            }
        }

        return hwtype;
    }

} ffmpegInitializer;

/////////////////////////////////////////////////////////////////////////////////////////
//#define TEST_AMF_FAIL_CASE
#ifdef TEST_AMF_FAIL_CASE
int64_t firstKeyframePTS(0), firstKeyframeDTS(0);
int packet_sn(0), keyframe_packets(0), corrupt_packets(0), discard_packets(0);
int debug_AMF_my_brothers_keeper_time_at = 448119; // My Brother's Keeper H265 Spatial Audio 17_0418.mp4
#endif
/////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------
AVPixelFormat VideoDecoder::s_get_format_(AVCodecContext* avctx, AVPixelFormat const* pix_fmts)
{
    VideoDecoder* inst = (NULL!=avctx) ? ((VideoDecoder*)(avctx->opaque)):NULL;
    if (inst /*&& avctx==inst->avctx_*/ && AV_HWDEVICE_TYPE_NONE!=inst->avctx_hwaccel_) {
        AVHWDeviceType& avctx_hwaccel_ = inst->avctx_hwaccel_;

       /**
        * AVCodecContext.get_format callback to negotiate the pixelFormat
        * @param fmt is the list of formats which are supported by the codec,
        * it is terminated by -1 as 0 is a valid format, the formats are ordered by quality.
        * The first is always the native one.
        * @note The callback may be called again immediately if initialization for
        * the selected (hardware-accelerated) pixel format failed.
        * @warning Behavior is undefined if the callback returns a value not
        * in the fmt list of formats.
        * @return the chosen format
        * - encoding: unused
        * - decoding: Set by user, if not set the native format will be chosen.
        */
        AVPixelFormat const* p = pix_fmts;
        for (; *p!=AV_PIX_FMT_NONE; ++p) {
            if (is_hwaccel_fmt_compatible(avctx_hwaccel_, *p)) {
                return *p;
            }
//            else if (!is_hwaccel_pix_fmt(*p)) {
//                avctx_hwaccel_ = AV_HWDEVICE_TYPE_NONE;
//                return *p;
//            }
        }

        // failed!?
        avctx_hwaccel_ = AV_HWDEVICE_TYPE_NONE;
        avctx->get_format = avcodec_default_get_format;
    }

    // not working!?
    return AV_PIX_FMT_NONE;
}
//---------------------------------------------------------------------------------------
char const* VideoDecoder::Name() const
{
    if (nullptr!=pImp_) {
        return pImp_->Name();
    }
    else if (AV_HWDEVICE_TYPE_NONE!=avctx_hwaccel_) {
        switch (avctx_hwaccel_)
        {
        case AV_HWDEVICE_TYPE_VDPAU: return "FFmpeg (VDPAU)";
        case AV_HWDEVICE_TYPE_CUDA: return "FFmpeg (CUDA)";
        case AV_HWDEVICE_TYPE_VAAPI: return "FFmpeg (VAAPI)";
        case AV_HWDEVICE_TYPE_DXVA2: return "FFmpeg (DXVA2)";
        case AV_HWDEVICE_TYPE_QSV: return "FFmpeg (QSV)";
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return "FFmpeg (VideoToolBox)";
        case AV_HWDEVICE_TYPE_D3D11VA: return "FFmpeg (D3D11VA)";
        default:
            break;
        }
    }
    return "FFmpeg (CPU)";
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::Init(AVStream* stream, int w, int h, uint8 options)
{
    assert(stream);
    assert(w>0 && h>0 && 0==(w&1) && 0==(1&h));
    packet_cnt_ = frame_cnt_ = 0;
    av_frame_unref(&frame_);

    AVCodecParameters* codecpar = stream->codecpar;
    if (codecpar && AVMEDIA_TYPE_VIDEO==codecpar->codec_type) {
        if (stream!=stream_) {
            Clear();
        }

        AVCodecID const codec_id = codecpar->codec_id;
        if (NULL!=pImp_ && codec_id!=pImp_->CodecID()) {
            VideoDecoderImp::Delete(pImp_);
            pImp_ = NULL;
        }

        if (NULL!=avctx_ && (NULL!=pImp_ || codec_id!=avctx_->codec_id)) { // how come!?
            avcodec_close(avctx_);
            avcodec_free_context(&avctx_);
            avctx_ = NULL; // unnecessary
        }

        if (NULL==pImp_ && NULL==avctx_) {
            // GPU accelerated video decoder
            pImp_ = VideoDecoderImp::New(stream, w, h);
            if (pImp_) {
                flags_ = 0x02;
                if (1!=options) {
                    VideoDecoderImp::Delete(pImp_);
                    pImp_ = NULL;
                }
            }
            else {
                flags_ = 0;
            }

            uint32 hw_flags = 0;
            AVCodec const* codec = NULL;
            avctx_hwaccel_ = ffmpegInitializer.GetHWAccelType(codec, hw_flags, codec_id, options>2 ? int(options-2):-1);
            if (NULL==codec) {
                codec = avcodec_find_decoder(codec_id);
                if (NULL==codec) {
                    return (NULL!=pImp_);
                }
            }

            avctx_ = avcodec_alloc_context3(codec);
            if (NULL==avctx_) {
                return (NULL!=pImp_);
            }

            if (0<=avcodec_parameters_to_context(avctx_, codecpar)) {
                avctx_->opaque = this;

                //
                // Using threaded decoding by default breaks backward compatibility if
                // AVHWAccel is used or if an appliction sets threadunsafe callbacks.
                //
                // https://lists.ffmpeg.org/pipermail/ffmpeg-cvslog/2012-January/046142.html
                //
                if (AV_HWDEVICE_TYPE_NONE!=avctx_hwaccel_) {
                    // set callback to negotiate the pixel format
                    avctx_->get_format = s_get_format_;

                    //
                    // it seems you don't need a hw_device_ctx, if AVCodec is of HW type.
                    //
                    if (NULL==codec->pix_fmts || !is_hwaccel_fmt_compatible(avctx_hwaccel_, codec->pix_fmts[0])) {
                        int const err = av_hwdevice_ctx_create(&(avctx_->hw_device_ctx), avctx_hwaccel_, NULL, NULL, 0);
                        if (err>=0) {
                            avctx_->get_format = s_get_format_;
                        }
                        else {
                            BL_LOG("Failed to create specified HW device(%d).\n", err);
                            avctx_hwaccel_ = AV_HWDEVICE_TYPE_NONE;
                        }
                    }
                }

                AVDictionary* codec_opts = NULL;
                av_opt_set_int(avctx_, "refcounted_frames", 1, 0);
                if (AV_HWDEVICE_TYPE_NONE!=avctx_hwaccel_) {
                    av_dict_set(&codec_opts, "threads", "auto", 0); //???
                }
                else {
                    av_dict_set(&codec_opts, "threads", "auto", 0);
                }

                // open
                if (0==avcodec_open2(avctx_, codec, &codec_opts)) {
                    if (AV_HWDEVICE_TYPE_NONE==avctx_hwaccel_ && 0==(avctx_->active_thread_type&FF_THREAD_FRAME)) {
                        char codecInfo[256];
                        avcodec_string(codecInfo, 256, avctx_, false);
                        BL_ERR("\n** video : codecCtx->active_thread_type=%d video decoding could be slow!!!\n(%s)\n\n",
                            avctx_->active_thread_type, codecInfo);
                    }

                    // set packet timebase
                    av_codec_set_pkt_timebase(avctx_, stream->time_base);
                    assert(0==av_cmp_q(av_codec_get_pkt_timebase(avctx_), stream->time_base));
                    // not necessary true!!!
                    // assert(0==av_cmp_q(av_codec_get_pkt_timebase(avctx_), avctx_->time_base)); 
                }
                else {
                    avcodec_free_context(&avctx_);
                    avctx_ = NULL;
                }

                if (NULL!=codec_opts) {
                    av_dict_free(&codec_opts);
                    codec_opts = NULL;
                }
            }
            else {
                avcodec_free_context(&avctx_);
                avctx_ = NULL;
                return false;
            }

            flags_ |= (uint32(hw_flags<<2) | 1);

#ifdef BL_AVDECODER_VERBOSE
            BL_LOG("** video decoder : %s\n", (NULL!=pImp_) ? pImp_->Name():"FFmpeg Software");
#endif
        }

        if (NULL!=avctx_ || NULL!=pImp_) {
            stream_ = stream;
            return true;
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------
void VideoDecoder::Clear()
{
    av_frame_unref(&frame_);
    stream_ = NULL;
    if (NULL!=pImp_) {
        VideoDecoderImp::Delete(pImp_);
        pImp_ = NULL;
    }
    if (NULL!=avctx_) {
        avcodec_close(avctx_);
        avcodec_free_context(&avctx_);
        avctx_ = NULL; // unnecessary
    }
    if (NULL!=swsctx_) {
        sws_freeContext(swsctx_);
        swsctx_ = NULL;
    }
    avctx_hwaccel_ = AV_HWDEVICE_TYPE_NONE;
    packet_cnt_ = frame_cnt_ = flags_ = 0;
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::Resume()
{
    if (NULL!=pImp_) {
        return pImp_->Resume();
    }
    return true;
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::Pause()
{
    if (NULL!=pImp_) {
        return pImp_->Pause();
    }
    return true;
}
//---------------------------------------------------------------------------------------
void VideoDecoder::Flush()
{
    if (NULL!=pImp_) {
        pImp_->Flush();
    }
    else if (NULL!=avctx_) {
        //
        // https://www.ffmpeg.org/doxygen/3.2/group__lavc__encdec.html
        //
        // send NULL to the avcodec_send_packet() functions. This will enter draining mode.
        avcodec_send_packet(avctx_, NULL);

        // call avcodec_receive_frame() in a loop until AVERROR_EOF is returned.
        // The functions will not return AVERROR(EAGAIN), unless you forgot to enter draining mode.
        while (AVERROR_EOF!=avcodec_receive_frame(avctx_, &frame_)) {}

        // decoding can be resumed again, the codec has to be reset with avcodec_flush_buffers().
        avcodec_flush_buffers(avctx_);
    }

    packet_cnt_ = frame_cnt_ = 0;
    av_frame_unref(&frame_);

#ifdef TEST_AMF_FAIL_CASE
    firstKeyframePTS = firstKeyframeDTS = 0;
    packet_sn = keyframe_packets = corrupt_packets = discard_packets = 0;
#endif
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::can_send_packet() const
{
    if (NULL!=pImp_) {
        return pImp_->can_send_packet();
    }
    else if (NULL!=avctx_) {
        return (AV_PIX_FMT_NONE==frame_.format);
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::can_receive_frame(int64_t& pts, int64_t& dur) const
{
    if (NULL!=pImp_) {
        return pImp_->can_receive_frame(pts, dur);
    }
    else if (NULL!=avctx_) {
        if (AV_PIX_FMT_NONE!=frame_.format) {
            pts = frame_.best_effort_timestamp;
            dur = frame_.pkt_duration;
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::send_packet(AVPacket const& pkt)
{/*
    static int64_t iframe_size = 0;
    static int64_t niframe_size = 0;
    static int iframes = 0;
    static int niframes = 0;
    if (pkt.flags&AV_PKT_FLAG_KEY) {
        iframe_size += pkt.size;
        ++iframes;
    }
    else {
        niframe_size += pkt.size;
        ++niframes;
    }

    if (0==((iframes+niframes)%100)) {
        int64_t ifr = iframe_size/iframes;
        int64_t nifr = niframe_size/niframes;
        BL_LOG("iframe:%.1f%%  %lld vs %lld ratio:%.1f\n",
               100.0f*iframes/(iframes+niframes), ifr, nifr, double(ifr)/nifr);
    }
*/
#ifdef TEST_AMF_FAIL_CASE
    ++packet_sn;
    if (pkt.flags&AV_PKT_FLAG_KEY) {
        if (1==++keyframe_packets) {
            firstKeyframePTS = pkt.pts;
            firstKeyframeDTS = pkt.dts;
        }
        BL_LOG("[send_packet] #%04d k pts:%lld(%+lld) dts:%lld(%+lld) size:%d bytes  ", packet_sn,
               pkt.pts, pkt.pts-firstKeyframePTS, pkt.dts, pkt.dts-firstKeyframeDTS, pkt.size);
    }
    else if (pkt.flags&AV_PKT_FLAG_CORRUPT) {
        ++corrupt_packets;
        BL_LOG("[send_packet] #%04d c pts:%lld(%+lld) dts:%lld(%+lld)  ", packet_sn,
               pkt.pts, pkt.pts-firstKeyframePTS, pkt.dts, pkt.dts-firstKeyframeDTS);
    }
    else if (pkt.flags&AV_PKT_FLAG_DISCARD) {
        ++discard_packets;
        BL_LOG("[send_packet] #%04d d pts:%lld(%+lld) dts:%lld(%+lld)  ", packet_sn,
               pkt.pts, pkt.pts-firstKeyframePTS, pkt.dts, pkt.dts-firstKeyframeDTS);
    }
    else {
        BL_LOG("[send_packet] #%04d - pts:%lld(%+lld) dts:%lld(%+lld)  ", packet_sn,
               pkt.pts, pkt.pts-firstKeyframePTS, pkt.dts, pkt.dts-firstKeyframeDTS);
/*
        if (pkt.pts<firstKeyframePTS) {
            return true;
        }
        else {

        }
*/
    }
#endif

    if (NULL!=pImp_) {
        if (pImp_->send_packet(pkt)) {
            ++packet_cnt_;
            return true;
        }
        return false;
    }
    else if (NULL!=avctx_) {
        int const ret = avcodec_send_packet(avctx_, &pkt);
        if (0==ret) {
            ++packet_cnt_;
            if (AV_PIX_FMT_NONE==frame_.format) {
                if (0==avcodec_receive_frame(avctx_, &frame_)) {
                    assert(AV_PIX_FMT_NONE!=frame_.format);
                }
            }
            return true;
        }
        else {
            // In particular, we don't expect AVERROR(EAGAIN), because we read all
            // decoded frames with avcodec_receive_frame() until done.
            // AVERROR(EAGAIN) : input is not accepted right now - the packet must be resent after trying to read output
            BL_ERR("** avcodec_send_packet failed(%d)\n", ret);
            if (AVERROR(EAGAIN)==ret) {
                return false;
            }
        }
    }

    return true; // bypass this packet
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::receive_frame(VideoFrame& output)
{
    // color space and range
    if (avctx_) {
        //
        // color space, check...
        // enum AVColorTransferCharacteristic avctx_->color_trc;
        // enum AVColorPrimaries              avctx_->color_primaries
        // enum AVColorSpace                  avctx_->colorspace;
        //
        // also check same members from AVFrame...
        //

        //
        // color range
        if (AVCOL_RANGE_JPEG==avctx_->color_range) {
            output.ColorRange = VideoFrame::RANGE_JPEG;
        }
        else {
            output.ColorRange = VideoFrame::RANGE_MPEG;
        }
    }

    if (NULL!=pImp_) {
        if (pImp_->receive_frame(output)) {
            ++frame_cnt_;
            return true;
        }
    }
    else if (NULL!=avctx_ && AV_PIX_FMT_NONE!=frame_.format) {
        uint8_t* nv12 = (uint8_t*) output.FramePtr;
        int const width  = output.Width;
        int const height = output.Height;

        AVFrame sw_frame;
        AVFrame* frame = NULL;
        if (frame_.hw_frames_ctx) {
            assert(AV_HWDEVICE_TYPE_NONE!=avctx_hwaccel_);
            assert(is_hwaccel_fmt_compatible(avctx_hwaccel_, (AVPixelFormat)frame_.format));
            memset(&sw_frame, 0, sizeof(sw_frame));
            av_frame_unref(&sw_frame);
            int const ret = av_hwframe_transfer_data(&sw_frame, &frame_, 0);
            if (ret>=0) {
                frame = &sw_frame;
            }
            else {
                BL_ERR("** Error transferring data to system memory(%d)\n", ret);
            }
        }
        else {
            assert(AV_HWDEVICE_TYPE_NONE==avctx_hwaccel_);
            frame = &frame_;
        }

        if (NULL!=frame) {
            // extract pixels in NV12 format
            uint8_t* y = nv12;
            uint8_t* uv = y + width*height;

            AVPixelFormat const format = (AVPixelFormat) frame->format;
            bool const fmt_chk = AV_PIX_FMT_NV12==format ||
                                 AV_PIX_FMT_NV21==format ||
                                 AV_PIX_FMT_YUV420P==format ||
                                 AV_PIX_FMT_YUVJ420P==format;
            if (fmt_chk && width==frame->width && height==frame->height) {
                // y plane
                uint8_t const* src1 = frame->data[0];
                int stride1 = frame->linesize[0];
                if (width==stride1) {
                    memcpy(y, src1, width*height);
                }
                else {
                    for (int i=0; i<height; ++i) {
                        memcpy(y, src1, width);
                        y += width;
                        src1 += stride1;
                    }
                }

                // uv plane
                int const uv_width = width/2;
                int const uv_height = height/2;
                src1 = frame->data[1];
                stride1 = frame->linesize[1];
                if (AV_PIX_FMT_NV12==format) {
                    if (width==stride1) {
                        memcpy(uv, src1, width*uv_height);
                    }
                    else {
                        for (int i=0; i<uv_height; ++i) {
                            memcpy(uv, src1, width);
                            uv += width;
                            src1 += stride1;
                        }
                    }
                }
                else if (AV_PIX_FMT_NV21==format) {
                    for (int i=0; i<uv_height; ++i) {
                        uint8_t const* vu = src1;
                        for (int j=0; j<uv_width; ++j) {
                            *uv++ = vu[1];
                            *uv++ = vu[0];
                            vu += 2;
                        }
                        src1 += stride1;
                    }
                }
                else {
                    uint8_t const* src2 = frame->data[2];
                    int const stride2 = frame->linesize[2];
                    for (int i=0; i<uv_height; ++i) {
                        uint8_t const* u = src1;
                        uint8_t const* v = src2;
                        for (int j=0; j<uv_width; ++j) {
                            *uv++ = *u++;
                            *uv++ = *v++;
                        }
                        src1 += stride1;
                        src2 += stride2;
                    }
                }
            }
            else {
                assert(0==frame_cnt_||NULL!=swsctx_);
                swsctx_ = sws_getCachedContext(swsctx_,
                                               frame->width, frame->height, format, 
                                               width, height, AV_PIX_FMT_NV12,
                                               SWS_BILINEAR, NULL, NULL, NULL);
                if (NULL!=swsctx_) {
                    uint8_t* dst2[4] = { y, uv, NULL, NULL };
                    int const stride[4] = { width, width, 0, 0 };
                    sws_scale(swsctx_, (uint8_t const* const*) frame->data,
                              frame->linesize, 0, frame->height, dst2, stride);
                }
                else {
                    BL_ERR("** VideoDecoder::receive_frame() : failed to create swsctx_\n");
                }
            }
            ++frame_cnt_;
        }

        if (frame_.hw_frames_ctx) {
            av_frame_unref(&sw_frame);
        }

        // receive new frame
        frame_.format = AV_PIX_FMT_NONE;
        if (0==avcodec_receive_frame(avctx_, &frame_)) {
            assert(AV_PIX_FMT_NONE!=frame_.format);
        }

        return (NULL!=frame);
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::discard_frame()
{
    if (pImp_) {
        if (pImp_->discard_frame()) {
            ++frame_cnt_;
            return true;
        }
        return false;
    }
    else if (AV_PIX_FMT_NONE!=frame_.format) {
        av_frame_unref(&frame_);
        ++frame_cnt_;
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoDecoder::finish_frame(VideoFrame& frame)
{
    if (pImp_) {
        pImp_->finish_frame(frame);
    }
    return true;
}
//---------------------------------------------------------------------------------------
AVDecoder::AVDecoder(IAVDecoderHost* listener, int id):
videoQueue_(),subtitleQueue_(),audioQueue_(),
audioQueueExt1_(),audioQueueExt2_(),audioQueueExt3_(),
videoDecoder_(),
subtitleUtil_(),
audioInfo_(),
decodeThread_(),
videoCV_(),
videoMutex_(),
subtitleMutex_(),
audioMutex_(),
pktListMutex_(),
asyncPlayMutex_(),
status_(STATUS_EMPTY),
videoSeekTimestamp_(0),
videoFramePut_(0),videoFrameGet_(0),
subtitlePut_(0),subtitleGet_(0),
audioBufferPut_(0),
audioBufferGet_(0),
host_(listener),
inputStream_(NULL),
avPacketFreeList_(NULL),
formatCtx_(NULL),
ioCtx_(NULL),
videoBuffer_(NULL),
subtitleBuffer_(NULL),
audioBuffer_(NULL),
timeAudioCrossfade_(0),
timeStartSysTime_(0),
timeInterruptSysTime_(0),
timeLastUpdateFrameSysTime_(0),
duration_(0),startTime_(0),
videoPutPTS_(0),videoGetPTS_(0),
subtitlePTS_(0),
audioPutPTS_(0),
audioGetPTS_(0),
stereo3D_(VIDEO_3D_MONO),
id_(id),
videoBufferSize_(0),
videoFrameDataSize_(0),
subtitleBufferSize_(0),
audioBufferCapacity_(0),
audioBufferWrapSize_(0),
videoStreamIndex_(-1),videoStreamCount_(0),
subtitleStreamIndex_(-1),subtitleStreamIndexSwitch_(-1),subtitleStreamCount_(0),
audioStreamIndex_(-1),audioStreamIndexSwitch_(-1),audioStreamCount_(0),
audioExtStreamIndex1_(-1),audioExtStreamIndex2_(-1),audioExtStreamIndex3_(-1),
videoWidth_(0),videoHeight_(0),videoFrameRate100x_(0),
subtitleDuration_(0),subtitleWidth_(0),subtitleHeight_(0),subtitleRectCount_(0),
audioBytesPerFrame_(0),audioBytesPerSecond_(0),
videoDecoderPreference_(0xff),
subtitleGraphicsFmt_(0),
subtitleBufferFlushing_(0),
audioBufferFlushing_(0),
liveStream_(0),
endOfStream_(0),
transcodeMode_(0),
interruptRequest_(0)
{
    // add decoder counter
    ffmpegInitializer.Init();

    // init packet free list
    memset(avPackets_, 0, sizeof(avPackets_));
    for (int i=0; i<NUM_MAX_TOTAL_PACKETS; ++i) {
        AVPacketList* packet = avPackets_ + i;
        packet->next = avPacketFreeList_;
        avPacketFreeList_ = packet;
    }

    // subtitle rects
    for (int i=0; i<Subtitle::MAX_NUM_SUBTITLE_RECTS; ++i) {
        subtitleRects_[i].Reset();
    }

    // default size = NUM_VIDEO_BUFFER_FRAMES*24MB = 48MB
    videoBufferSize_ = NUM_VIDEO_BUFFER_FRAMES*av_image_get_buffer_size(AV_PIX_FMT_NV12, 4096, 4096, 4);
    videoBuffer_ = (uint8_t*) av_malloc(videoBufferSize_);
    if (NULL==videoBuffer_) {
        videoBufferSize_ = 0; // fingers crossed...
    }

    audioBuffer_ = (uint8_t*) malloc(INIT_AUDIO_BUFFER_SIZE);
    if (NULL!=audioBuffer_) {
        audioBufferCapacity_ = INIT_AUDIO_BUFFER_SIZE;
    }

    if (!subtitleUtil_.Initialize()) {
        BL_ERR("AVDecoder failed to init subtitle module!!!\n");
    }
}
//---------------------------------------------------------------------------------------
AVDecoder::~AVDecoder()
{
    Close();

    host_ = NULL;

    subtitleUtil_.Finalize();

    av_free(videoBuffer_);
    videoBuffer_ = NULL;
    videoBufferSize_ = 0;

    if (subtitleBuffer_) {
        free(subtitleBuffer_);
        subtitleBuffer_ = NULL;
    }
    subtitleBufferSize_ = 0;

    free(audioBuffer_);
    audioBuffer_ = NULL;
    audioBufferCapacity_ = 0;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::DoOpen_(AVFormatContext* fmtCtx,  char const* url, VideoOpenOption const* param)
{
    assert(fmtCtx);
    assert(STATUS_EMPTY==status_);

    formatCtx_ = fmtCtx;
    char msg_buf[512];

    // retrieve stream information (this could be slow)
    int error = avformat_find_stream_info(formatCtx_, NULL);
    if (error<0) {
        av_strerror(error, msg_buf, sizeof(msg_buf));
        BL_ERR("avformat_find_stream_info() failed! error:%s\n", msg_buf);
        DoClose_();
        return false;
    }

    // is live stream
    bool const livestream = (NULL!=param) && (1==param->IsLiveStream);

    // start time
    startTime_ = (AV_NOPTS_VALUE!=formatCtx_->start_time) ? (formatCtx_->start_time):0;

    //
    // we had found some abnormal videos can have wrong duration. (probably because of
    // downloading failed). Also note each AVStream has duration member as well, but in
    // that case, int64_t AVStream::duration (in stream time base) does not help,
    // since it still can be value of AV_NOPTS_VALUE.
    //
    if (formatCtx_->duration>0/* && AV_NOPTS_VALUE!=formatCtx_->duration*/) {
        duration_ = formatCtx_->duration;
    }
    else {
        if (!livestream) {
            BL_ERR("** Invalid duration(formatCtx_->duration=%lli(0x%llX))!!!\n",
                   formatCtx_->duration, formatCtx_->duration);
        }
        duration_ = -1000000;
    }

#ifdef BL_AVDECODER_VERBOSE
    if (livestream) {
        BL_LOG("** live stream  \"%s\" format:%s\n", url, formatCtx_->iformat->name);
    }
    else {
        BL_LOG("** media \"%s\" format:%s\n", url, formatCtx_->iformat->name);
    }
#endif

    int const total_streams = (int) formatCtx_->nb_streams;
    int const ext_subtitles = subtitleUtil_.Create(url);

    // preference
    int sid(0), aid(0), maxSize(-1);
    if (NULL!=param) {
        int id = 0;
        if (param->AudioStreamId!=0xfe) {
            id = param->AudioStreamId;
            if (id<total_streams &&
                AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[id])) {
                audioStreamIndex_ = id;
                aid = 1; // fixed
            }
        }
        else {
            aid = -1; // disable audio... are you sure!?
        }

        if (param->SubtitleStreamId!=0xfe) {
            id = param->SubtitleStreamId;
            if (total_streams<=id) {
                if (0<ext_subtitles) {
                    subtitleStreamIndex_ = (id<(total_streams+ext_subtitles)) ? id:total_streams;
                    sid = 1;
                }
            }
            else {
                if (AVMEDIA_TYPE_SUBTITLE==FFmpeg_Codec_Type(formatCtx_->streams[id])) {
                    subtitleStreamIndex_ = id;
                    sid = 1; // fixed
                }
            }
        }
        else {
            sid = -1; // disable subtitle
        }

        maxSize = param->MaxVideoResolution;
    }

    //
    // for multiple video streams(Adaptive Bit-rate stream), pick the biggest one
    // (instead of av_find_best_stream). this is mainly because we found a live streaming
    // (the 12th KKBox Music Awards) has 4 video streams, but if we don't choose the
    // biggest one(#10), few non-interested streams will be consume internet traffic.
    //
    // what does 'adaptive' mean anyway!?
    //
    AVRational valid_avg_frame_rate = av_make_q(0, 0);
    int64_t very_end_pts = 0;
    int width(0), height(0);
    for (int i=0; i<total_streams; ++i) {
        AVStream const* stream = formatCtx_->streams[i];
        int64_t const endpts = av_stream_get_end_pts(stream);
        if (very_end_pts<endpts)
            very_end_pts = endpts;

        switch (FFmpeg_Codec_Type(stream))
        {
        case AVMEDIA_TYPE_VIDEO:
            if (stream->codecpar->width>0 && stream->codecpar->height>0) {
                ++videoStreamCount_;
                AVRational const& fps = stream->avg_frame_rate;
                if (fps.den>0 && fps.num>0) {
                    valid_avg_frame_rate = fps;
                }

                bool accept = (1==videoStreamCount_);
                if (!accept) {
                    if (maxSize>0) {
                        if (stream->codecpar->width<=maxSize && stream->codecpar->height<=maxSize) {
                            if (width<stream->codecpar->width) {
                                accept = true;
                            }
                        }
                        else if (stream->codecpar->width<width) {
                            accept = true;
                        }
                    }
                    else {
                        if (width<stream->codecpar->width) {
                            accept = true;
                        }
                    }
                }

                if (accept) {
                    videoStreamIndex_ = i;
                    width = stream->codecpar->width;
                    height = stream->codecpar->height;
                }
            }
            break;

        case AVMEDIA_TYPE_SUBTITLE:
            ++subtitleStreamCount_;
            if (0<=sid && subtitleStreamIndex_<0) {
                subtitleStreamIndex_ = i; // the 1st subtitle stream index
            }
            break;

        case AVMEDIA_TYPE_AUDIO:
            ++audioStreamCount_;
            if (0<=aid && audioStreamIndex_<0) {
                audioStreamIndex_ = i; // the 1st audio stream index
            }

            // [bug?] #(channels) and channel_layout not match!?
            if (stream->codecpar->channels!=av_get_channel_layout_nb_channels(stream->codecpar->channel_layout)) {
                uint64_t const new_channel_layout = av_get_default_channel_layout(stream->codecpar->channels);
                BL_LOG("** [FFmpeg bug?] audio track#%d(channels=%d) suspicious channel layout:0x%llX -> 0x%llX\n",
                       i, stream->codecpar->channels, stream->codecpar->channel_layout, new_channel_layout);
                stream->codecpar->channel_layout = new_channel_layout;
            }
            break;

        default:
#ifdef BL_AVDECODER_VERBOSE
            BL_LOG("** ignore stream (id:%d type:%s)\n", i, av_get_media_type_string(FFmpeg_Codec_Type(stream)));
#endif
            break;
        }
    }

    // not working...
    if (0<very_end_pts && 0<duration_) {
        assert(!livestream);
        if (duration_<very_end_pts+5000000) {
            //duration_ = formatCtx_->duration = very_end_pts;
        }
    }

    if (videoStreamIndex_<0 && audioStreamIndex_<0) {
        BL_LOG("** NO VIDEO! NO AUDIO! Loading failed\n");
        DoClose_();
        return false;
    }

    // find a better audio stream w.r.t. video stream
    if (1<audioStreamCount_ && aid<1 && 0<=audioStreamIndex_ && audioStreamIndex_<total_streams) {
        int const best_aid = av_find_best_stream(formatCtx_, AVMEDIA_TYPE_AUDIO, -1, videoStreamIndex_, NULL, 0);
        if (0<=best_aid && best_aid<total_streams)
            audioStreamIndex_ = best_aid;
    }

    // find a better subtitle stream w.r.t. audio stream(must be determined first)
    if (sid<1) { // not the best one!
        if (1<subtitleStreamCount_ && 0<=subtitleStreamIndex_ && subtitleStreamIndex_<total_streams) {
            int const best_sid = av_find_best_stream(formatCtx_, AVMEDIA_TYPE_SUBTITLE, -1, audioStreamIndex_, NULL, 0);
            if (0<=best_sid && best_sid<total_streams)
                subtitleStreamIndex_ = best_sid;
        }
        else if (0==sid && subtitleStreamIndex_<0 && 0<ext_subtitles) {
            subtitleStreamIndex_ = total_streams;
        }
    }

    // setup video codec context
    if (0<=videoStreamIndex_ && videoStreamIndex_<total_streams) {
        AVStream* stream = formatCtx_->streams[videoStreamIndex_];
        AVRational const& fps = stream->avg_frame_rate;
        if (fps.den>0 && fps.num>0) {
            videoFrameRate100x_ = (int) ((av_rescale(1000, fps.num, fps.den)+5)/10);
        }
        else {
            // we found invalid AVStream::avg_frame_rate while testing KKBox's 12th Music Award stream.
            // if stream's timebase is correct and all packets have valid pts/duration.
            // it should not cause any harm. so we just guess it here...
            // Gino from KKBox said he is working on it. 2017/04/06
            if (valid_avg_frame_rate.num<=0 || valid_avg_frame_rate.den<=0) {
                valid_avg_frame_rate = av_make_q(30000, 1001); // guess it's 29.97
            }
            
            videoFrameRate100x_ = (int) ((av_rescale(1000, valid_avg_frame_rate.num, valid_avg_frame_rate.den)+5)/10);
            stream->avg_frame_rate = valid_avg_frame_rate;
            BL_ERR("** video : invalid frame rate... guess it's %.2ffps!?\n", 0.01f*videoFrameRate100x_);
        }

        // video source width & height
        width = stream->codecpar->width;
        height = stream->codecpar->height;

        // video size(Texture width must be 4 bytes aligned)
        videoWidth_  = (width + 3)&~3;
        videoHeight_ = (height + 1)&~1;
        if (maxSize>240 && (videoWidth_>maxSize||videoHeight_>maxSize)) {
            if (videoWidth_>videoHeight_) {
                videoWidth_ = maxSize;
            }
            else {
                videoWidth_ = maxSize*width/height;
            }

            // round down, do not round up!
            videoWidth_ = videoWidth_ & ~3;
            videoHeight_ = (videoWidth_*height/width)&~1;
        }

        // movie resolution
        subtitleUtil_.SetMoiveResolution(videoWidth_, videoHeight_);

        // video decoder
        if (0xff==videoDecoderPreference_) {
            videoDecoderPreference_ = 0;
            if (1<hwaccel::GPUAccelScore()) {
                videoDecoderPreference_ = 1; // GPU
            }
        }
        videoDecoder_.Init(stream, videoWidth_, videoHeight_, videoDecoderPreference_);

        // allocate pixel buffer
        videoFrameDataSize_ = av_image_get_buffer_size(AV_PIX_FMT_NV12, videoWidth_, videoHeight_, 4);
        int const videoBufferSize = videoFrameDataSize_*NUM_VIDEO_BUFFER_FRAMES;
        if (videoBufferSize_<videoBufferSize) {
            av_free(videoBuffer_);
            videoBuffer_ = (uint8_t*) av_malloc(videoBufferSize);
            if (NULL!=videoBuffer_) {
                videoBufferSize_ = videoBufferSize;
            }
            else {
                BL_ERR("*** Out of Memory!!! failed to allocate %lld memory buffer\n", videoBufferSize);
                DoClose_();
                return false;
            }
        }

        for (int j=0; j<stream->nb_side_data; ++j) {
            AVPacketSideData const& sd = stream->side_data[j];
            if (AV_PKT_DATA_STEREO3D==sd.type) {
                if (sd.size>=sizeof(AVStereo3D)) {
                    AVStereo3D* s3D = (AVStereo3D*) sd.data;
                    switch (s3D->type)
                    {
                    case AV_STEREO3D_2D:
                        stereo3D_ = VIDEO_3D_MONO;
                        break;
                    case AV_STEREO3D_SIDEBYSIDE:
                        if (s3D->flags & AV_STEREO3D_FLAG_INVERT) {
                            stereo3D_ = VIDEO_3D_RIGHTLEFT;
                        }
                        else {
                            stereo3D_ = VIDEO_3D_LEFTRIGHT;
                        }
                        break;
                    case AV_STEREO3D_TOPBOTTOM:
                        if (s3D->flags & AV_STEREO3D_FLAG_INVERT) {
                            stereo3D_ = VIDEO_3D_BOTTOMTOP;
                        }
                        else {
                            stereo3D_ = VIDEO_3D_TOPBOTTOM;
                        }
                        break;

                    //
                    // TO-DO : support these...?!
                    //
                    case AV_STEREO3D_FRAMESEQUENCE:
                        BL_LOG("** Stereo3D = \"frame alternate\". To Be Implemented!?\n");
                        break;
                    case AV_STEREO3D_CHECKERBOARD:
                        BL_LOG("** Stereo3D = \"checkerboard\". To Be Implemented!?\n");
                        break;
                    case AV_STEREO3D_LINES:
                        BL_LOG("** Stereo3D = \"interleaved lines\". To Be Implemented!?\n");
                        break;
                    case AV_STEREO3D_COLUMNS:
                        BL_LOG("** Stereo3D = \"interleaved columns\". To Be Implemented!?\n");
                        break;
                    case AV_STEREO3D_SIDEBYSIDE_QUINCUNX:
                        BL_LOG("** Stereo3D = \"side by side (quincunx subsampling)\". To Be Implemented!?\n");
                        break;
                    default:
                        BL_LOG("** Stereo3D = \"unknown\"!?\n");
                        break;
                    }
                }
                break;
            }
        }
    }

    // subtitle codec context
    subtitleStreamIndexSwitch_ = subtitleStreamIndex_;
    if (0<=subtitleStreamIndex_) {
        if (subtitleStreamIndex_<total_streams) {
            //...
        }
        else {
            subtitleUtil_.OnChangeExternalSubtitle(subtitleStreamIndex_-total_streams);
        }
    }

    // audio codec context
    if (audioStreamIndex_>=0) {
        audioExtStreamIndex1_ = audioExtStreamIndex2_ = audioExtStreamIndex3_ = -1;

        if (NULL!=param) {
            audioInfo_ = param->AudioSetting;
        }

        // verify tracks and channels
        uint8 streamId[4];
        uint8 streamCh[4];
        uint16 verify_streams = 0;
        uint16 verify_channels = 0;
        for (int i=0; i<4&&i<audioInfo_.TotalStreams; ++i) {
            uint8 const id = audioInfo_.StreamIndices[i];
            if (0xff!=id && id<total_streams && AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[id])) {
                AVCodecParameters const* codecpar = formatCtx_->streams[id]->codecpar;
                int const ch = (NULL!=codecpar) ? (codecpar->channels):-1;
                if (0<ch && ch<0xff && (0==audioInfo_.StreamChannels[i]||ch==audioInfo_.StreamChannels[i])) {
                    streamId[i] = id;
                    streamCh[i] = (uint8) ch;
                    verify_channels += (uint8) ch;
                    ++verify_streams;
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }

        if (0<verify_streams && verify_streams==audioInfo_.TotalStreams &&
            (0==audioInfo_.TotalChannels||verify_channels==audioInfo_.TotalChannels)) {
            // audioInfo_.TotalStreams = verify_streams; // already is
            audioInfo_.TotalChannels = verify_channels;

            // fill streams & channels
            audioStreamIndex_ = streamId[0];
            audioInfo_.StreamIndices[0] = streamId[0];
            audioInfo_.StreamChannels[0] = streamCh[0];
            if (verify_streams>1) {
                audioExtStreamIndex1_ = streamId[1];
                audioInfo_.StreamIndices[1] = streamId[1];
                audioInfo_.StreamChannels[1] = streamCh[1];
                if (verify_streams>2) {
                    audioExtStreamIndex2_ = streamId[2];
                    audioInfo_.StreamIndices[2] = streamId[2];
                    audioInfo_.StreamChannels[2] = streamCh[2];
                    if (verify_streams>3) {
                        audioExtStreamIndex3_ = streamId[3];
                        audioInfo_.StreamIndices[3] = streamId[3];
                        audioInfo_.StreamChannels[3] = streamCh[3];
                    }
                }
            }
        }
        else {
            AVCodecParameters const* codecpar = formatCtx_->streams[audioStreamIndex_]->codecpar;
            if (NULL!=codecpar && 0<codecpar->channels && codecpar->channels<0xff) {
                audioInfo_.StreamIndices[0] = (uint8) audioStreamIndex_;
                audioInfo_.StreamChannels[0] = (uint8) codecpar->channels;
                audioInfo_.TotalStreams = 1;
                audioInfo_.TotalChannels = audioInfo_.StreamChannels[0];
            }
        }

        //
        // trust specified technique, of course this could be wrong,
        // host should call ConfirmAudioSetting() to fix mismatched
        // technique, streams and channels.
        //

        // format
        if (AUDIO_FORMAT_UNKNOWN==audioInfo_.Format) {
            AVCodecParameters const* codecpar = formatCtx_->streams[audioStreamIndex_]->codecpar;
            if (codecpar && (AV_SAMPLE_FMT_S16==codecpar->format || AV_SAMPLE_FMT_S16P==codecpar->format)) {
                audioInfo_.Format = AUDIO_FORMAT_S16;
            }
            else {
                audioInfo_.Format = AUDIO_FORMAT_F32;
            }
        }

        // sample rate
        if (audioInfo_.SampleRate<44100) {
            AVCodecParameters const* codecpar = formatCtx_->streams[audioStreamIndex_]->codecpar;
            if (codecpar && 44100<=codecpar->sample_rate) {
                audioInfo_.SampleRate = codecpar->sample_rate;
            }
            else {
                audioInfo_.SampleRate = 48000;
            }
        }

        // byte rate
        audioBytesPerFrame_  = audioInfo_.TotalChannels*BytesPerSample(audioInfo_.Format);
        audioBytesPerSecond_ = audioInfo_.SampleRate*audioBytesPerFrame_;

        audioStreamIndexSwitch_ = audioStreamIndex_;
    }

    // discard uninterested streams.
    for (int i=0; i<total_streams; ++i) {
        if (i!=videoStreamIndex_ && i!=subtitleStreamIndex_ && i!=audioStreamIndex_ &&
            i!=audioExtStreamIndex1_ && i!=audioExtStreamIndex2_ && i!=audioExtStreamIndex3_) {
            formatCtx_->streams[i]->discard = AVDISCARD_ALL;
        }
    }

    // media info
#ifdef BL_AVDECODER_VERBOSE
    if (0<=videoStreamIndex_) {
        sprintf(msg_buf, "%dx%d @ %.2ffps", width, height, 0.01f*videoFrameRate100x_);
        if (duration_>0) {
            int secs = (int) (duration_/AV_TIME_BASE);
            int const ds = (int) ((duration_%AV_TIME_BASE)/10000);
            int const hours = secs/3600; secs %= 3600;
            int const mins = secs/60; secs %= 60;
            BL_LOG("** duration: %02d:%02d:%02d.%02d(%lld ms) %s\n",
                   hours, mins, secs, ds, duration_/1000, msg_buf);
        }
        else {
            if (0<=videoStreamIndex_) {
                BL_LOG("** live stream: %s\n", msg_buf);
            }
        }
    }

    AVCodecContext* avctx = avcodec_alloc_context3(NULL);
    if (avctx) {
        char language[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        for (int i=0; i<total_streams; ++i) {
            AVStream* stream = formatCtx_->streams[i];

            language[0] = ' ';
            language[1] = '\0';
            if (stream->metadata) {
                AVDictionaryEntry* t = av_dict_get(stream->metadata, "language", NULL, 0);
                // ISO 639 Lanuage code
                if (NULL!=t) {
                    language[0] = ':';
                    language[1] = '(';
                    memcpy(language+2, t->value, 3);
                    language[5] = ')';
                    language[6] = ' ';
                    language[7] = '\0';
                }
                /*
                  to iterate all entries...
                  AVDictionaryEntry* t = NULL;
                  while (NULL!=(t = av_dict_get(dict, "", t, AV_DICT_IGNORE_SUFFIX))) {
                      BL_LOG("\t%s = %s\n", t->key, t->value);
                  }
                */
            }

            avcodec_parameters_to_context(avctx, stream->codecpar);
            avcodec_string(msg_buf, 256, avctx, false);
            BL_LOG("** stream#%d%s%s%s\n", i,
             (i==videoStreamIndex_||i==audioStreamIndex_||i==subtitleStreamIndex_||
              i==audioExtStreamIndex1_||i==audioExtStreamIndex2_||i==audioExtStreamIndex3_) ? "*":"",
             language, msg_buf);
        }

        if (0<ext_subtitles && total_streams<=subtitleStreamIndex_) {
            assert(subtitleStreamIndex_<(total_streams+ext_subtitles));
        }
        avcodec_free_context(&avctx);
    }
#endif

    // live streaming - just a hint for buffering
    liveStream_ = livestream;

    // ready to go... in case of audio is not set properly,
    // host should call ConfirmAudioSettings() later in audio setting thread.
    status_ = STATUS_READY;

    return true;
}
//---------------------------------------------------------------------------------------
void AVDecoder::DoClose_()
{
    // close audio if it's playing...
    if (STATUS_PLAYING==status_ && NULL!=host_) {
        host_->OnStopped(id_,
                (int)((((videoGetPTS_<audioGetPTS_) ? audioGetPTS_:videoGetPTS_)-startTime_)/1000),
                (int)(duration_/1000), false);
    }

    status_ = STATUS_EMPTY;
    if (decodeThread_.joinable()) {
        videoCV_.notify_all();
        interruptRequest_ = 1;
        decodeThread_.join();
    }

    // close video decoder
    videoDecoder_.Clear();

    // close file context
    if (NULL!=formatCtx_) {
        avformat_close_input(&formatCtx_);
        formatCtx_ = NULL; // unnecessary
    }

    //
    // refer ffmpeg/examples/avio_reading.c
    // note: the internal buffer could have changed, and be not that avio_ctx_buffer
    //       passed to avio_alloc_context(...)
    if (ioCtx_) {
        //
        // avio_close(ioCtx_)? NO!
        //
        av_freep(&ioCtx_->buffer);
        av_freep(&ioCtx_);
        ioCtx_ = NULL; // unnecessary
    }

    if (NULL!=inputStream_) {
        inputStream_->Release();
        inputStream_ = NULL;
    }

    // flush queues/unref uncomplete frames
    FlushQueue_(videoQueue_);
    FlushQueue_(subtitleQueue_);
    FlushQueue_(audioQueue_);
    FlushQueue_(audioQueueExt1_);
    FlushQueue_(audioQueueExt2_);
    FlushQueue_(audioQueueExt3_);

    // subtitle closed
    subtitleUtil_.Destroy();

    // reset
    audioInfo_.Reset();
    videoFrameGet_ = videoFramePut_ = 0;
    subtitlePut_ = subtitleGet_ = 0;
    audioBufferPut_ = audioBufferGet_ = 0;
    stereo3D_ = VIDEO_3D_MONO;
    timeAudioCrossfade_ = timeStartSysTime_ = timeInterruptSysTime_ = 0;
    duration_ = startTime_ = 0;
    videoPutPTS_ = videoGetPTS_ = subtitlePTS_ = audioPutPTS_ = audioGetPTS_ = 0;
    videoStreamIndex_ = subtitleStreamIndex_ = audioStreamIndex_ = -1;
    audioExtStreamIndex1_ = audioExtStreamIndex2_ = audioExtStreamIndex3_ = -1;
    subtitleStreamIndexSwitch_ = audioStreamIndexSwitch_ = -1;
    videoStreamCount_ = subtitleStreamCount_ = audioStreamCount_ = 0;
    subtitleWidth_ = subtitleHeight_ = subtitleRectCount_ = subtitleDuration_ = 0;
    videoFrameRate100x_ = videoWidth_ = videoHeight_ = videoFrameDataSize_ = 0;
    audioBytesPerFrame_ = audioBytesPerSecond_ = 0; 
    subtitleGraphicsFmt_ = subtitleBufferFlushing_ = audioBufferFlushing_ = 0;
    liveStream_ = endOfStream_ = interruptRequest_ = 0;

    /////////////////////////////////////////////////////////////////////////////////////
    // to be removed...
#ifdef BL_AVDECODER_VERBOSE
    AVPacketList* pk = avPacketFreeList_;
    int total_packets = 0;
    while (NULL!=pk) {
        ++total_packets;
        assert(avPackets_<=pk && pk<(avPackets_+NUM_MAX_TOTAL_PACKETS));
        pk = pk->next;
    }

    // if assertion failed, this is because of sdl audio function is not finish yet!?
    if (total_packets!=NUM_MAX_TOTAL_PACKETS) {
        BL_ERR("Reset() - fatal error: total free packets = %d(should be %d)\n", total_packets, NUM_MAX_TOTAL_PACKETS);
        assert(total_packets==NUM_MAX_TOTAL_PACKETS);
    }
#endif

    avPacketFreeList_ = NULL;
    for (int i=0; i<NUM_MAX_TOTAL_PACKETS; ++i) {
        AVPacketList* packet = avPackets_ + i;
        av_packet_unref(&(packet->pkt));
        packet->next = avPacketFreeList_;
        avPacketFreeList_ = packet;
    }
    /////////////////////////////////////////////////////////////////////////////////////
}
//---------------------------------------------------------------------------------------
bool AVDecoder::ToggleHardwareVideoDecoder()
{
    // stop() and start() lock(asyncPlayMutex_); so don't do this...
    //std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (NULL!=formatCtx_) {
        uint32 const flags = videoDecoder_.Flags();
        if (flags>1) {
            uint32 new_decoder = (videoDecoderPreference_+1)%32;
            while (new_decoder!=videoDecoderPreference_) {
                if (flags&(1<<new_decoder)) {
                    break;
                }
                new_decoder = (new_decoder+1)%32;
            }

            if (new_decoder!=videoDecoderPreference_ && Stop()) {
                videoDecoderPreference_ = (uint8) new_decoder;
                videoDecoder_.Clear();
                int const time = VideoTime() - 3000;
                return PlayAt(time>0 ? time:0); // not OK if just return Play()!!!
            }
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::Open(char const* url, VideoOpenOption const* param)
{
    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (NULL!=url) {
        DoClose_();
        AVFormatContext* fmtCtx = avformat_alloc_context();
        if (fmtCtx) {
            // setup callbacks
            fmtCtx->interrupt_callback.callback = DecodeInterruptCB_;
            fmtCtx->interrupt_callback.opaque = this;

            AVDictionary* format_opts = NULL;
            //av_dict_set(&format_opts,"rtsp_transport","tcp",0);
            //if (livestream) {
            //    av_dict_set(&format_opts, "timeout", "5000000", 0); // in microseconds
            //}

            int const error = avformat_open_input(&fmtCtx, url, NULL, &format_opts);
            if (NULL!=format_opts) {
                av_dict_free(&format_opts);
                format_opts = NULL;
            }

            if (0==error) {
                if (DoOpen_(fmtCtx, url, param))
                    return true;
                avformat_close_input(&fmtCtx);
            }
            else {
                BL_ERR("avformat_open_input(%s) failed(0%X)\n", url, error);
                avformat_free_context(fmtCtx);
            }
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::Open(IInputStream* is, VideoOpenOption const* param)
{
    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (NULL!=is && is->Rewind()) {
        DoClose_();

        //
        // note the AVIOContext's internal buffer could have changed at some points.
        // so it don't have to store this new allocated buffer to be deleted.
        // it always delete ioCtx_->buffer (and ioCtx_) when finished, see DoClose_()
        uint8_t* avio_ctx_buffer = (uint8_t*) av_malloc(AVIO_BUFFER_DEFAULT_SIZE);
        if (NULL==avio_ctx_buffer) {
            return false;
        }

        ioCtx_ = avio_alloc_context(avio_ctx_buffer,
                                    AVIO_BUFFER_DEFAULT_SIZE,
                                    0,
                                    is,
                                    InputStreamRead_,
                                    NULL,
                                    InputStreamSeek_);
        if (NULL==ioCtx_) {
            av_free(avio_ctx_buffer);
            return false;
        }

        AVFormatContext* fmtCtx = avformat_alloc_context();
        if (fmtCtx) {
            // setup callbacks
            fmtCtx->interrupt_callback.callback = DecodeInterruptCB_;
            fmtCtx->interrupt_callback.opaque = this;

            // set I/O context
            fmtCtx->pb = ioCtx_;

            AVDictionary* format_opts = NULL;
            //av_dict_set(&format_opts,"rtsp_transport","tcp",0);
            //if (livestream) {
            //    av_dict_set(&format_opts, "timeout", "5000000", 0); // in microseconds
            //}

            int const error = avformat_open_input(&fmtCtx, NULL, NULL, &format_opts);

            if (NULL!=format_opts) {
                av_dict_free(&format_opts);
                format_opts = NULL;
            }

            if (0==error) {
                if (DoOpen_(fmtCtx, is->URL(), param)) {
                    is->AddRef();
                    inputStream_ = is;
                    return true;
                }

                avformat_close_input(&fmtCtx);
            }
            else {
                BL_ERR("avformat_open_input(%s) failed(0%X)\n", is->Name(), error);
                avformat_free_context(fmtCtx);
            }

            if (ioCtx_) {
                av_freep(&ioCtx_->buffer);
                av_freep(&ioCtx_);
                ioCtx_ = NULL; // unnecessary
            }
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::ConfirmAudioSetting()
{
    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (STATUS_READY!=status_) {
        return false; // do nothing if not a good time to change!
    }

    assert(audioStreamIndexSwitch_==audioStreamIndex_);
    audioStreamIndexSwitch_ = audioStreamIndex_; // just in case
    int const total_streams = (NULL!=formatCtx_) ? ((int)formatCtx_->nb_streams):0;
    if (0<total_streams && 0<audioStreamCount_ && NULL!=host_) {
        assert(AUDIO_FORMAT_UNKNOWN!=audioInfo_.Format);
        assert(audioInfo_.SampleRate>=44100);
        assert(audioInfo_.TotalStreams>0);
        assert(audioInfo_.TotalChannels>=audioInfo_.StreamChannels[0]);
        assert(audioStreamIndex_==audioInfo_.StreamIndices[0]);
        assert(audioInfo_.StreamChannels[0]>0);
        assert(audioStreamIndex_<total_streams);

        AudioInfo ai = audioInfo_; // audio setting
        if (host_->AudioSettingCallback(id_, ai)) {
            // reset and rebuild
            audioInfo_.Reset();

            // rebuild stream/channel list
            AVCodecParameters const* codecpar = NULL;
            for (int i=0; i<sizeof(audioInfo_.StreamIndices)&&i<ai.TotalStreams; ++i) {
                uint8 const id = ai.StreamIndices[i];
                if (id<total_streams && AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[id])) {
                    audioInfo_.StreamIndices[i] = id;
                    audioInfo_.StreamChannels[i] = ai.StreamChannels[i];
                    audioInfo_.TotalChannels += audioInfo_.StreamChannels[i];
                    ++audioInfo_.TotalStreams;
                }
                else {
                    break;
                }
            }

            audioStreamIndex_ = -1;
            audioExtStreamIndex1_ = audioExtStreamIndex2_ = audioExtStreamIndex3_ = -1;
            if (audioInfo_.TotalStreams>0) {
                audioStreamIndex_ = audioInfo_.StreamIndices[0];
                if (audioInfo_.TotalStreams>1) {
                    audioExtStreamIndex1_ = audioInfo_.StreamIndices[1];
                    if (audioInfo_.TotalStreams>2) {
                        audioExtStreamIndex2_ = audioInfo_.StreamIndices[2];
                        if (audioInfo_.TotalStreams>3) {
                            audioExtStreamIndex3_ = audioInfo_.StreamIndices[3];
                        }
                    }
                }

                // technique - whatever you said, dear host.
                audioInfo_.Technique = ai.Technique;

                // format
                if (AUDIO_FORMAT_UNKNOWN!=ai.Format) {
                    audioInfo_.Format = ai.Format;
                }
                else {
                    audioInfo_.Format = AUDIO_FORMAT_F32; // favor f32 format
                }

                // sample rate
                if (0<ai.SampleRate) {
                    audioInfo_.SampleRate = ai.SampleRate;
                }
                else if (0<=audioStreamIndex_) {
                    codecpar = formatCtx_->streams[audioStreamIndex_]->codecpar;
                    if (codecpar && 44100<=codecpar->sample_rate) {
                        audioInfo_.SampleRate = codecpar->sample_rate;
                    }
                    else {
                        audioInfo_.SampleRate = 48000;
                    }
                }

                // byte rate
                audioBytesPerFrame_ = audioInfo_.TotalChannels*BytesPerSample(audioInfo_.Format);
                audioBytesPerSecond_ = audioInfo_.SampleRate*audioBytesPerFrame_;

                // enable/disable audio streams
                for (int i=0; i<total_streams; ++i) {
                    if (AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[i])) {
                        if (i==audioStreamIndex_ ||
                            i==audioExtStreamIndex1_ || i==audioExtStreamIndex2_ || i==audioExtStreamIndex3_) {
                            formatCtx_->streams[i]->discard = AVDISCARD_DEFAULT;
                        }
                        else {
                            formatCtx_->streams[i]->discard = AVDISCARD_ALL;
                        }
                    }
                }

#ifdef BL_AVDECODER_VERBOSE
                char log_buffer[128];
                AVSampleFormat const audioSampleFormat = GetAVSampleFormat(audioInfo_.Format);
                codecpar = formatCtx_->streams[audioStreamIndex_]->codecpar;
                av_get_channel_layout_string(log_buffer, 128, codecpar->channels, codecpar->channel_layout);
                BL_LOG("** audio info: %dHz/%s/%s", codecpar->sample_rate, av_get_sample_fmt_name((AVSampleFormat)codecpar->format), log_buffer);
                if (0<=audioExtStreamIndex1_) {
                    codecpar = formatCtx_->streams[audioExtStreamIndex1_]->codecpar;
                    av_get_channel_layout_string(log_buffer, 128, codecpar->channels, codecpar->channel_layout);
                    BL_LOG("+%dHz/%s/%s", codecpar->sample_rate, av_get_sample_fmt_name((AVSampleFormat)codecpar->format), log_buffer);
                }
                if (0<=audioExtStreamIndex2_) {
                    codecpar = formatCtx_->streams[audioExtStreamIndex2_]->codecpar;
                    av_get_channel_layout_string(log_buffer, 128, codecpar->channels, codecpar->channel_layout);
                    BL_LOG("+%dHz/%s/%s", codecpar->sample_rate, av_get_sample_fmt_name((AVSampleFormat)codecpar->format), log_buffer);
                }
                if (0<=audioExtStreamIndex3_) {
                    codecpar = formatCtx_->streams[audioExtStreamIndex3_]->codecpar;
                    av_get_channel_layout_string(log_buffer, 128, codecpar->channels, codecpar->channel_layout);
                    BL_LOG("+%dHz/%s/%s", codecpar->sample_rate, av_get_sample_fmt_name((AVSampleFormat)codecpar->format), log_buffer);
                }

                // output
                if (AUDIO_TECHNIQUE_AMBIX==audioInfo_.Technique) {
                    BL_LOG(" -> %dHz/%s/ambiX(%dch)\n", audioInfo_.SampleRate, av_get_sample_fmt_name(audioSampleFormat), audioInfo_.TotalChannels);
                }
                else if (AUDIO_TECHNIQUE_FUMA==audioInfo_.Technique) {
                    BL_LOG(" -> %dHz/%s/FuMa(%dch)\n", audioInfo_.SampleRate, av_get_sample_fmt_name(audioSampleFormat), audioInfo_.TotalChannels);
                }
                else if (AUDIO_TECHNIQUE_TBE==audioInfo_.Technique) {
                    BL_LOG(" -> %dHz/%s/TBE(%dch)\n", audioInfo_.SampleRate, av_get_sample_fmt_name(audioSampleFormat), audioInfo_.TotalChannels);
                }
                else {
                    av_get_channel_layout_string(log_buffer, 128, audioInfo_.TotalChannels, av_get_default_channel_layout(audioInfo_.TotalChannels));
                    BL_LOG(" -> %dHz/%s/%s\n", audioInfo_.SampleRate, av_get_sample_fmt_name(audioSampleFormat), log_buffer);
                }
#endif
                return true;
            }
        }
    }

    // disable audio streams
    for (int i=0; i<total_streams; ++i) {
        if (AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[i])) {
            formatCtx_->streams[i]->discard = AVDISCARD_ALL;
        }
    }

    audioInfo_.Reset();
    audioStreamIndex_ = audioStreamIndexSwitch_ = -1;
    audioExtStreamIndex1_ = audioExtStreamIndex2_ = audioExtStreamIndex3_ = -1;
    audioBytesPerFrame_ = audioBytesPerSecond_ = 0;
    BL_ERR("ConfirmAudioSetting() no compatible audio device! (play mute!)\n");

    return false;
}
//---------------------------------------------------------------------------------------
int AVDecoder::StreamingAudio(uint8* dst, int maxSize, int frames, float& gain, AudioInfo* info)
{
    // if video have not been tried to retrieve constantly, do not drain audio buffer.
    // (it may be because of the video playback is temporary paused)
    assert(dst && maxSize>0 && frames>0);
    gain = 1.0f;
    if (info) {
        *info = audioInfo_;
    }
    int64_t const curr_time = av_gettime();
    if (STATUS_PLAYING!=status_ || (timeLastUpdateFrameSysTime_+250000)<curr_time ||
        NULL==dst || frames<1 || maxSize<audioBytesPerFrame_) { // 250 ms
        return 0;
    }

    int64_t const crossfade = abs(timeAudioCrossfade_-curr_time);
    if (crossfade<AUDIO_FADE_IN_TIME) {
        gain = (0.001f*(crossfade*1000/AUDIO_FADE_IN_TIME));
    }

    // max frames
    int const required_frames = frames;
    int const limit_frames = maxSize/audioBytesPerFrame_;
    int const max_frames = (required_frames<limit_frames) ? required_frames:limit_frames;
    int64_t const data_duration = av_rescale(max_frames, AV_TIME_BASE, audioInfo_.SampleRate);
    int64_t const get_pts = audioGetPTS_;
    int64_t const get_pts_after = get_pts + data_duration;
    for (frames=0; frames<max_frames; ) {
        int const data_size = audioBufferPut_ - audioBufferGet_;
        if (data_size>0) {
            int const available_frames = data_size/audioBytesPerFrame_;
            assert(available_frames>0 && (available_frames*audioBytesPerFrame_)==data_size);
            int const need_frames = max_frames - frames;
            int const fill_frames = (need_frames<available_frames) ? need_frames:available_frames;
            int const fill_size = fill_frames*audioBytesPerFrame_;
            int const data_get_pos = ((int)audioBufferGet_)%audioBufferWrapSize_;
            if ((data_get_pos+fill_size)<=audioBufferWrapSize_) {
                memcpy(dst, audioBuffer_+data_get_pos, fill_size);
            }
            else {
                int const tail_size = audioBufferWrapSize_ - data_get_pos;
                assert(0<tail_size && tail_size<fill_size);
                memcpy(dst, audioBuffer_+data_get_pos, tail_size);
                memcpy(dst+tail_size, audioBuffer_, fill_size-tail_size);
            }

            frames += fill_frames;
            dst += fill_size;

            // move Get Pointer must ensure audioBufferGet_ < audioBufferWrapSize_
            {
                std::lock_guard<std::mutex> lock(audioMutex_);
                int next_get = audioBufferGet_ + fill_size;
                int const remain_size = audioBufferPut_ - next_get;
                if (remain_size>0) {
                    if (next_get<audioBufferWrapSize_) {
                        audioBufferGet_ = next_get;
                    }
                    else {
                        audioBufferGet_ = (next_get%=audioBufferWrapSize_);
                        audioBufferPut_ = next_get + remain_size;
                    }
                }
                else {
                    audioBufferPut_ = audioBufferGet_ = 0;
                }

                // while audioMutex_ is locking
                audioGetPTS_ = get_pts_after;
            }
        }

        // continue?
        if (frames<max_frames && av_gettime()<(curr_time+1000)) {
            av_usleep(100);
        }
        else {
            break;
        }
    }

    // synchronize audio & video timestamp
    if (frames==required_frames && NULL!=info && 0==timeInterruptSysTime_) {
        //
        // 1) to adjust video timestamp is the easiest (and the best way) to achieve.
        // 2) to resample audio(swr_convert) may produce bad sound to be heard.
        //
        // ***Only do this when audio data is normally decoded***
        //
        // data_duration ~= 1000/30 = 33ms
#if 1
        assert(0<videoFrameRate100x_);
        int64_t const half_audio_frame_time = data_duration/2;
        int64_t const half_video_frame_time = int64_t(50000000)/((videoFrameRate100x_>=2400) ? videoFrameRate100x_:2400);
        int64_t const error_tolerence = (half_video_frame_time>half_audio_frame_time) ? half_video_frame_time:half_audio_frame_time;
#else
        int64_t const error_tolerence = data_duration/2;
#endif
        int64_t const error_pts = timeStartSysTime_ + get_pts - curr_time;
        if (error_pts>error_tolerence) {
            timeStartSysTime_ -= error_tolerence;
        }
        else if (error_pts<-error_tolerence) {
            timeStartSysTime_ += error_tolerence;
        }
    }

    // if no more bytes available, it could be end already.
    if (frames<max_frames) {
        if (STATUS_PLAYING==status_) {
            if (endOfStream_) { // change status only if no video streams
                if (0==audioQueue_.Size() && videoStreamIndex_<0) {
                    if (STATUS_PLAYING==status_) { // check again!
                        status_ = STATUS_ENDED;
                    }
                }
            }
            else if (0==audioBufferFlushing_) {
                timeAudioCrossfade_ = curr_time + AUDIO_FADE_IN_TIME;

                int const at_time = (audioPutPTS_>startTime_) ? (int) ((audioPutPTS_-startTime_)/1000):0;
                int lag_time = 0;
                if (0==timeInterruptSysTime_) {
                    timeInterruptSysTime_ = curr_time;
                    lag_time = (int) av_rescale(max_frames-frames, 1000, audioInfo_.SampleRate);
                }
                else {
                    lag_time = (int) ((curr_time-timeInterruptSysTime_)/1000);
                }

                if (NULL!=host_) {
                    host_->OnAudioInterrupt(id_, lag_time, at_time);
                }
            }
        }
    }

    return frames;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::VideoThread_()
{
    // For video, AVPacket should typically contain one compressed frame.
    assert(0==videoFramePut_ && 0==videoFrameGet_);
    assert(NULL!=formatCtx_ && 0<=videoStreamIndex_);
    AVStream* stream = formatCtx_->streams[videoStreamIndex_];
    AVRational const& stream_time_base = stream->time_base;
    int64_t const avg_frame_time = av_rescale(100, AV_TIME_BASE, videoFrameRate100x_);
    int64_t pkt_pts(AV_NOPTS_VALUE), pkt_dts(AV_NOPTS_VALUE);
    int64_t frame_pts(AV_NOPTS_VALUE), frame_duration(AV_NOPTS_VALUE);

    videoDecoder_.Init(stream, videoWidth_, videoHeight_, videoDecoderPreference_);
    videoDecoder_.Resume();

    // drop frames
    AVPacketList* pk = NULL;
    int const max_consecutive_drop_frames = videoFrameRate100x_/200;
    int consecutive_drop_frames = 0;
    while (STATUS_PLAYING==status_ && 0==interruptRequest_) {
        int const frame_buffer_count = videoFramePut_ - videoFrameGet_;
        if (frame_buffer_count<NUM_VIDEO_BUFFER_FRAMES &&
            videoDecoder_.can_receive_frame(frame_pts, frame_duration)) {
            if (AV_NOPTS_VALUE!=frame_pts) {
                videoPutPTS_ = av_rescale_q(frame_pts, stream_time_base, timebase_q);
            }
// pkt pts may be out of order
//            else if (AV_NOPTS_VALUE!=pkt_pts) {
//                videoPutPTS_ = av_rescale_q(pkt_pts, stream_time_base, timebase_q);
//            }
            else if (AV_NOPTS_VALUE!=pkt_dts) {
                videoPutPTS_ = av_rescale_q(pkt_dts, stream_time_base, timebase_q);
            }
#ifdef BL_AVDECODER_VERBOSE
            else {
                BL_ERR("** FFmpeg video without pts, timing could be wrong!?\n");
            }
#endif
            int64_t const cur_time = av_gettime();
            bool frame_in_time = (0==timeStartSysTime_) || (timeStartSysTime_+videoPutPTS_+15000)>cur_time;

            // decode video frame
            if (videoFramePut_<=NUM_VIDEO_BUFFER_FRAMES || frame_in_time || transcodeMode_ ||
                consecutive_drop_frames>max_consecutive_drop_frames ||
                (consecutive_drop_frames>4 && (0==frame_buffer_count))) {
                int const slot = videoFramePut_%NUM_VIDEO_BUFFER_FRAMES;

                VideoFrame& frame = decodedFrames_[slot];
                frame.FramePtr   = (int64_t) (videoBuffer_+slot*videoFrameDataSize_);
                frame.PTS        = videoPutPTS_;
                frame.Type       = VideoFrame::NV12;
                frame.ColorSpace = VideoFrame::BT_709;
                frame.ColorRange = VideoFrame::RANGE_MPEG;
                frame.Width      = videoWidth_;
                frame.Height     = videoHeight_;
                frame.ID         = videoFramePut_;
                videoDecoder_.receive_frame(frame);

                ++videoFramePut_;
                consecutive_drop_frames = 0;
            }
            else { // discard this frame, since it's decoded so late:-(
                videoDecoder_.discard_frame();
                ++consecutive_drop_frames;
                if (host_) {
                    host_->OnVideoLateFrame(id_, consecutive_drop_frames, (int)((videoPutPTS_-startTime_)/1000));
                }
                if (0==frame_buffer_count) {
                    videoGetPTS_ = videoPutPTS_;
                }
            }

            // estimate next frame's pts
            if (AV_NOPTS_VALUE!=frame_duration) {
                videoPutPTS_ += av_rescale_q(frame_duration, stream_time_base, timebase_q);
            }
            else {
                videoPutPTS_ += avg_frame_time;
            }
        }

        if (videoDecoder_.can_send_packet()) {
            if (NULL==pk) {
                pk = videoQueue_.Get();
            }

            if (NULL!=pk) {
                if (videoDecoder_.send_packet(pk->pkt)) {
                    pkt_pts = pk->pkt.pts;
                    pkt_dts = pk->pkt.dts;
                    ReleaseAVPacketList_(pk);
                    pk = NULL;
                }
                else {
                    // In particular, we don't expect AVERROR(EAGAIN), because we read all
                    // decoded frames with avcodec_receive_frame() until done.
                    // AVERROR(EAGAIN) : input is not accepted right now -
                    // the packet must be resent after trying to read output
                }
            }
            else if (0==videoFramePut_) {
                av_usleep(1000); // have not started yet~ wait for more packets to come
            }
        }
        else if ((videoFramePut_-videoFrameGet_)>=NUM_VIDEO_BUFFER_FRAMES) {
            av_usleep(1000); // take a nap
        }
    }

    if (NULL!=pk) {
        if (!videoDecoder_.send_packet(pk->pkt)) {
            videoDecoder_.discard_frame();
            videoDecoder_.send_packet(pk->pkt);
        }
        ReleaseAVPacketList_(pk); pk = NULL;
    }

    // pause
    videoDecoder_.Pause();

    return true;
}
//---------------------------------------------------------------------------------------
class AudioDecoder {
    enum {
        MAXIMUM_BUFFER_SIZE = 16<<20, // 16M
        RESET_PTS_PERIOD_MINS = 12
    };
    AVRational      stream_time_base_;
    AVFormatContext const* const formatCtx_;
    AVCodecContext* codecCtx_;
    SwrContext*     swrCtx_; // audio resample and conversion
    AVFrame*        frame_;
    uint8_t*        buffer_;
    AVSampleFormat  sampleFormat_;
    int64_t         channelLayout_; // if 0 use codecCtx_->channel_layout
    int64_t         ptsStart_, ptsEnd_, ptsValid_;
    int             bufferSize_;
    int             dataPut_;
    int             sampleRate_;
    int             bytesPerSample_;
    int             bytesPerSecond_;
    int             streamId_;
    int             syncSampleCount_;

    bool InitSwrContext_(AVSampleFormat sample_fmt, int64 channel_layout, int sample_rate) {
        assert(NULL!=codecCtx_);
        sampleFormat_ = sample_fmt;
        channelLayout_ = channel_layout;
        sampleRate_ = sample_rate;
        if (0==channel_layout) {
            channel_layout = codecCtx_->channel_layout;
        }

        swrCtx_ = swr_alloc_set_opts(swrCtx_,
                                     channel_layout, sample_fmt, sample_rate,
                                     codecCtx_->channel_layout,
                                     codecCtx_->sample_fmt,
                                     codecCtx_->sample_rate,
                                     0, NULL);
        if (NULL!=swrCtx_) {
            if (swr_init(swrCtx_)>=0) {
                bytesPerSample_ = av_get_channel_layout_nb_channels(channel_layout) *
                                       av_get_bytes_per_sample(sample_fmt);
                bytesPerSecond_ = sampleRate_*bytesPerSample_;
                if (NULL==frame_) {
                    frame_ = av_frame_alloc();
                }
                return true;
            }

            swr_free(&swrCtx_); swrCtx_ = NULL;
            channelLayout_ = sampleRate_ = 0;
            BL_ERR("swr_init(swrCtx_) failed!!!\n");
        }
        BL_ERR("swrCtx_=NULL!!!\n");
        return false;
    }
    void DeInit_() {
        if (NULL!=codecCtx_) {
            avcodec_close(codecCtx_);
            avcodec_free_context(&codecCtx_);
            codecCtx_ = NULL;
        }
        if (NULL!=swrCtx_) {
            swr_free(&swrCtx_);
            swrCtx_ = NULL;
        }
        if (NULL!=frame_) {
            av_frame_free(&frame_);
            frame_ = NULL;
        }
        if (NULL!=buffer_) {
            free(buffer_);
            buffer_ = NULL;
        }
        bufferSize_ = dataPut_ = 0;
        sampleRate_ = bytesPerSample_ = bytesPerSecond_ = 0;
        streamId_ = syncSampleCount_ = -1;

        ptsValid_ = AV_NOPTS_VALUE;
    }

    // not defined
    AudioDecoder();
    AudioDecoder(AudioDecoder const&);
    AudioDecoder& operator=(AudioDecoder const&);

public:
    explicit AudioDecoder(AVFormatContext const* fmtctx):stream_time_base_(),
        formatCtx_(fmtctx),codecCtx_(NULL),swrCtx_(NULL),frame_(NULL),buffer_(NULL),
        sampleFormat_(AV_SAMPLE_FMT_NONE),channelLayout_(0),
        ptsStart_(0),ptsEnd_(0),ptsValid_(-1),bufferSize_(0),dataPut_(0),
        sampleRate_(0),bytesPerSample_(0),bytesPerSecond_(0),streamId_(-1),syncSampleCount_(-1) {
    }
    ~AudioDecoder() { DeInit_(); }

    int StreamId() const { return streamId_; }
    int DataSize() const { return dataPut_; }
    int NumBytesPerSample() const { return bytesPerSample_; }
    int NumSamples() const { return dataPut_/bytesPerSample_; }
    int64_t Timestamp() const { return dataPut_>0 ? ptsStart_:-1; }
    int64_t ValidPTS() const { return ptsValid_; }
    int DiscardSamples(int64_t pts) {
        assert(ptsStart_<=pts);
        int samples = dataPut_/bytesPerSample_;
        if (ptsStart_<pts) {
            int const drop_samples = (int) (av_rescale(pts-ptsStart_, bytesPerSecond_, AV_TIME_BASE)/bytesPerSample_);
            if (samples>drop_samples) {
                samples -= drop_samples;
                dataPut_ = samples*bytesPerSample_;
                memmove(buffer_, buffer_+(drop_samples*bytesPerSample_), dataPut_);
                ptsStart_ = pts;
            }
            else {
                dataPut_ = samples = 0;
                ptsStart_ = ptsEnd_;
            }
        }
        return samples;
    }

    bool Init(int streamId, AVSampleFormat sample_fmt, int64 channel_layout, int sample_rate, int64_t pts) {
        DeInit_();
        if (NULL!=formatCtx_ && 0<=streamId && streamId<((int)formatCtx_->nb_streams) &&
            AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[streamId])) {
            codecCtx_ = FFmpeg_AVCodecOpen(formatCtx_->streams[streamId]);
            if (NULL!=codecCtx_ && InitSwrContext_(sample_fmt, channel_layout, sample_rate)) {
                if (NULL==buffer_) {
                    bufferSize_ = 1<<20;
                    buffer_ = (uint8_t*) malloc(bufferSize_);
                    if (NULL==buffer_) {
                        bufferSize_ = 0;
                    }
                }
                stream_time_base_ = av_codec_get_pkt_timebase(codecCtx_);
                //formatCtx_->streams[streamId]->time_base
                streamId_ = streamId;
                ptsStart_ = ptsEnd_ = pts;
                syncSampleCount_ = 0;
                return true;
            }
        }
        return false;
    }

    // not thread-safe
    int64_t ReadPacket(AVPacket* pkt) {
        if (NULL==codecCtx_ || NULL==swrCtx_ || streamId_<0 || NULL==pkt ||
            streamId_!=pkt->stream_index)
            return -1;

        // this is to fix AV_PKT_DATA_PARAM_CHANGE, see below...
        AVSampleFormat const prev_sample_fmt = codecCtx_->sample_fmt;
        uint64_t const prev_channel_layout = codecCtx_->channel_layout;
        int const prev_channels = codecCtx_->channels;
        int const prev_sample_rate = codecCtx_->sample_rate;

        /////////////////////////////////////////////////////////////////////////////
        // What the...
        // avcodec_send_packet may changed codecCtx_!?
        // refer utils.c line 2777 avcodec_send_packet
        /**
        * An AV_PKT_DATA_PARAM_CHANGE side data packet is laid out as follows:
        * @code
        * u32le param_flags
        * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT)
        *     s32le channel_count
        * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT)
        *     u64le channel_layout
        * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE)
        *     s32le sample_rate
        * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS)
        *     s32le width
        *     s32le height
        * @endcode
        */
        int const ret = avcodec_send_packet(codecCtx_, pkt);
        if (0!=ret) {
#ifdef BL_AVDECODER_VERBOSE
            //  0xBEBBB1B7 : "Invalid data found when processing input"
            char errMsg[256];
            av_strerror(ret, errMsg, sizeof(errMsg));
            BL_LOG("** avcodec_send_packet(audio) failed(0x%X):%s\n", ret, errMsg);
#else
            BL_LOG("** avcodec_send_packet(audio) failed(0x%X)\n", ret);
#endif
            return -1;
        }
/*
        int size(0);
        int val = 0;
        uint64_t vCh = 0;
        uint8_t const* data = av_packet_get_side_data(&pk->pkt, AV_PKT_DATA_PARAM_CHANGE, &size);
        uint8_t const* data_end = data + size;
        if (NULL!=data && (data+4)<=data_end) {
            uint32_t flags = uint32_t(data[3])<<24 | uint32_t(data[2])<<16 | uint32_t(data[1])<<8 | uint32_t(data[0]);
            if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT && (data+4)<=data_end) {
                val = int(data[3])<<24 | int(data[2])<<16 | int(data[1])<<8 | int(data[0]);
                data += 4;
                //codecCtx_->channels = val;
            }
            if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT && (data+8)<=data_end) {
                vCh = uint64_t(data[7])<<56 | uint64_t(data[6])<<48 | uint64_t(data[5])<<40 | uint64_t(data[4])<<32 |
                                uint64_t(data[3])<<24 | uint64_t(data[2])<<16 | uint64_t(data[1])<<8 | uint64_t(data[0]);
                data += 8;
                //codecCtx_->channel_layout = vCh;
            }
            if (flags & AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE && (data+4)<=data_end) {
                val = int(data[3])<<24 | int(data[2])<<16 | int(data[1])<<8 | int(data[0]);
                data += 4;
                        
                //codecCtx_->sample_rate = val;
            }
        }
*/
        if (prev_sample_fmt!=codecCtx_->sample_fmt || prev_channels!=codecCtx_->channels ||
            prev_channel_layout!=codecCtx_->channel_layout || prev_sample_rate!=codecCtx_->sample_rate) {
#ifdef BL_AVDECODER_VERBOSE
            BL_LOG("** audioCodecCtx params changed : fmt:%d->%d ch:%d->%d layout:0x%llX->0x%llX\n",
                   prev_sample_fmt, codecCtx_->sample_fmt, prev_channels, codecCtx_->channels,
                   prev_channel_layout, codecCtx_->channel_layout);
#endif
            if (prev_channel_layout!=codecCtx_->channel_layout) { // should we fix it? or just trust it!?
                if (codecCtx_->channels!=av_get_channel_layout_nb_channels(codecCtx_->channel_layout)) {
                    BL_LOG("** audioCodecCtx error! : channel(%d) and layout(0x%llX) not match!!!\n",
                           codecCtx_->channels, codecCtx_->channel_layout);
                    codecCtx_->channel_layout = av_get_default_channel_layout(codecCtx_->channels);
                }
            }

            if (!InitSwrContext_(sampleFormat_, channelLayout_, sampleRate_)) {
                return -1;
            }
        }

        bool reset_pts_start = (dataPut_<=0);
        int64_t valid_pts = AV_NOPTS_VALUE;

        // decode audio frame. for audio, an AVPacket may contain several compressed frames...
        for (int got_frames=0; avcodec_receive_frame(codecCtx_, frame_)==0; ++got_frames) {
            int const out_samples_max = (frame_->nb_samples*sampleRate_)/(frame_->sample_rate) + 256;
            int const ext_buffer_size = out_samples_max*bytesPerSample_;

            // size check...
            int required_buffer_size = dataPut_ + ext_buffer_size;
            if (bufferSize_<required_buffer_size) {
                if (bufferSize_>MAXIMUM_BUFFER_SIZE) {
                    dataPut_ = 0; // audio buffer too large...  discard previous data.
                    required_buffer_size = ext_buffer_size;
                }

                if (bufferSize_<required_buffer_size) {
                    int const new_buffer_size = bufferSize_ + BL_ALIGN_UP(required_buffer_size, 16384);
                    uint8_t* new_buffer = (uint8_t*) malloc(new_buffer_size);
                    if (new_buffer) {
                        if (NULL!=buffer_) {
                            if (dataPut_>0) {
                                memcpy(new_buffer, buffer_, dataPut_);
                            }
                            free(buffer_);
                        }
                        buffer_ = new_buffer;
                        bufferSize_ = new_buffer_size;
                    }
                    else {
                        BL_ERR("** audio : failed to allocate %d bytes buffer\n", new_buffer_size);
                    }
                }
            }
            assert(NULL!=buffer_ && required_buffer_size<=bufferSize_);

            // resample audio data
            if (NULL!=buffer_ && required_buffer_size<=bufferSize_) {
                uint8_t const** in = (uint8_t const**) frame_->extended_data;
                uint8_t* out = buffer_ + dataPut_;
                int const out_samples = swr_convert(swrCtx_, &out, out_samples_max, in, frame_->nb_samples);
                if (0<out_samples) {
                    syncSampleCount_ += out_samples;
                    if (out_samples==out_samples_max) {
                        BL_LOG("** swr_convert() buffer too small!?\n");
                    }

                    //
                    // for PCM/ADPCM, 1 packet may be decoded to be multiple frames,
                    // only check best_effort_timestamp from the first decoded frame.
                    int64_t pts = ptsEnd_;
                    if (valid_pts<0) {
                        pts = av_frame_get_best_effort_timestamp(frame_);
                        if (AV_NOPTS_VALUE!=pts) {
                            pts = valid_pts = av_rescale_q(pts, stream_time_base_, timebase_q);
                        }
                        else if (AV_NOPTS_VALUE!=pkt->pts) {
                            pts = valid_pts = av_rescale_q(pkt->pts, stream_time_base_, timebase_q);
                        }
                        else if (AV_NOPTS_VALUE!=frame_->pkt_dts) {
                            pts = valid_pts = av_rescale_q(frame_->pkt_dts, stream_time_base_, timebase_q);
                        }
                        else {
                            pts = ptsEnd_;
                        }
                    }

                    if (pts<0) {
                        BL_LOG("** AudioDecoder::ReadPacket() - pts:%lld!?\n", pts);
                    }

                    int const data_size = out_samples*bytesPerSample_;
                    dataPut_ += data_size;
                    ptsEnd_ = pts + av_rescale(data_size, AV_TIME_BASE, bytesPerSecond_);
                }
            }

            // release frame
            av_frame_unref(frame_);
        } // packet to frames

        // will reset for every 12 secs
        if (AV_NOPTS_VALUE!=valid_pts) {
            if (AV_NOPTS_VALUE==ptsValid_ || syncSampleCount_>(RESET_PTS_PERIOD_MINS*sampleRate_)) {
                reset_pts_start = true;
            }
            ptsValid_ = valid_pts;
        }
        else if ((ptsValid_+RESET_PTS_PERIOD_MINS*AV_TIME_BASE)<ptsStart_) {
            ptsValid_ = AV_NOPTS_VALUE;
        }

        if (reset_pts_start) {
            syncSampleCount_ = 0;
            ptsStart_ = ptsEnd_ - av_rescale(dataPut_, AV_TIME_BASE, bytesPerSecond_);
            if (ptsStart_<0) // could be -1
                ptsStart_ = 0;
        }

        return ptsStart_;
    }

    int64_t StreamChanged(AVPacket* pkt) {
        if (NULL!=formatCtx_ && NULL!=pkt) {
            int const sId = pkt->stream_index;
            if (streamId_==sId)
                return ReadPacket(pkt); // no!!!

            if (0<=sId && sId<((int)formatCtx_->nb_streams) &&
                AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[sId])) {
                int64_t const bak_ptsStart_ = ptsStart_;
                int64_t const bak_ptsEnd_ = ptsEnd_;
                int const bak_data_size   = dataPut_;
                int const bak_buffer_size = bufferSize_;
                uint8*    bak_buffer      = buffer_;
                if (NULL!=codecCtx_) {
                    avcodec_close(codecCtx_);
                    avcodec_free_context(&codecCtx_);
                }
                if (NULL!=swrCtx_) {
                    swr_free(&swrCtx_);
                }
                streamId_ = -1;

                codecCtx_ = FFmpeg_AVCodecOpen(formatCtx_->streams[sId]);
                if (NULL!=codecCtx_ && InitSwrContext_(sampleFormat_, channelLayout_, sampleRate_)) {
                    dataPut_ = 0;
                    if (0==bufferSize_) {
                        bufferSize_ = 1<<20;
                    }
                    buffer_ = (uint8_t*) malloc(bufferSize_);
                    if (NULL==buffer_) { // if new allocation fails, take old buffer back
                        bufferSize_ = bak_buffer_size;
                        buffer_ = bak_buffer; bak_buffer = NULL;
                    }
                    stream_time_base_ = av_codec_get_pkt_timebase(codecCtx_);
                    streamId_ = sId;
                    ptsStart_ = ptsEnd_ = bak_ptsEnd_; // initial guess
                    ptsValid_ = AV_NOPTS_VALUE;
                    syncSampleCount_ = 0;
                    ReadPacket(pkt); // decode this packet
                    if (dataPut_<=0) { // restore if nothing found
                        free(buffer_);
                        ptsStart_ = bak_ptsStart_;
                        ptsEnd_ = bak_ptsEnd_;
                        dataPut_ = bak_data_size;
                        bufferSize_ = bak_buffer_size;
                        buffer_ = bak_buffer; bak_buffer = NULL;
                        return -1;
                    }

                    // likely, the late coming packet has later timestamp, i.e.
                    // bak_ptsEnd_<=ptsStart_. That means we can simply just drop
                    // all previous audio samples
                    if (NULL!=bak_buffer) {
                        free(bak_buffer);
                    }

                    return ptsStart_;
                }
            }
        }
        return -1;
    }

    // not thread-safe
    int FillData(uint8_t* dst, int samples, int stride, int64_t& ptsStart, int64_t& ptsEnd) {
        ptsEnd = ptsStart = ptsStart_;
        int const s = dataPut_/bytesPerSample_;
        if (samples>s)
            samples = s;

        if (samples>0) {
            int const total_bytes = samples*bytesPerSample_;
            if (bytesPerSample_<stride) {
                uint8_t const* src = buffer_;
                for (int i=0; i<samples; ++i) {
                    memcpy(dst, src, bytesPerSample_);
                    dst += stride;
                    src += bytesPerSample_;
                }
            }
            else {
                memcpy(dst, buffer_, total_bytes);
            }

            ptsEnd = ptsStart_ += av_rescale(total_bytes, AV_TIME_BASE, bytesPerSecond_);
            dataPut_ -= total_bytes;
            if (dataPut_>0) {
                memmove(buffer_, buffer_+total_bytes, dataPut_);
            }
        }

        return samples;
    }
    int FillData(uint8_t* dst, int samples, int stride) {
        int64_t ptsStart(0), ptsEnd(0);
        return FillData(dst, samples, stride, ptsStart, ptsEnd);
    }
};

// RAII class
class SubtitleDecodeRAII {
    SwsContext* subSwsCtx_;
    uint8_t*    buffer_;
    int         size_;  // buffer size

    // not defined
    SubtitleDecodeRAII(SubtitleDecodeRAII const&);
    SubtitleDecodeRAII& operator=(SubtitleDecodeRAII const&);

public:
    SubtitleDecodeRAII():subSwsCtx_(NULL),buffer_(NULL),size_(0) {}
    ~SubtitleDecodeRAII() {
        if (NULL!=subSwsCtx_) {
            sws_freeContext(subSwsCtx_);
            subSwsCtx_ = NULL;
        }
        if (NULL!=buffer_) {
            free(buffer_);
            buffer_ = NULL;
        }
        size_ = 0;
    }

    bool InitSwsContext(int dstW, int dstH, int srcW, int srcH) {
        subSwsCtx_ = sws_getCachedContext(subSwsCtx_,
                                       srcW, srcH, AV_PIX_FMT_PAL8,
                                       dstW, dstH, AV_PIX_FMT_RGBA,
                                       SWS_BILINEAR, NULL, NULL, NULL);
        return (NULL!=subSwsCtx_);
    }

    bool Convert(uint8_t* dst2, int dstW,
                 const uint8_t *const srcSlice[], const int srcStride[], int srcH) {
        if (NULL!=subSwsCtx_) {
            uint8_t* dst[1] = { dst2 };
            int const stride[1] = { dstW*4 };
            sws_scale(subSwsCtx_, srcSlice, srcStride, 0, srcH, dst, stride);
            return true;
        }
        return false;
    }

    uint8_t const* GenerateBlendLayer(int dstW, int dstH,
                                      const uint8_t *const srcSlice[], const int srcStride[], int srcH) {
        int required_size = dstW*dstH*4;
        if (required_size>size_ || NULL==buffer_) {
            if (NULL!=buffer_) {
                free(buffer_);
            }
            size_ = 4<<20; // 4MB
            while (size_<required_size) {
                size_ += (2<<20);
            }

            buffer_ = (uint8_t*) malloc(size_);
            if (NULL==buffer_) {
                size_ = 0;
                return NULL;
            }
        }

        if (NULL!=subSwsCtx_) {
            uint8_t* dst[1] = { buffer_ };
            int const stride[1] = { dstW*4 };
            sws_scale(subSwsCtx_, srcSlice, srcStride, 0, srcH, dst, stride);
            return buffer_;
        }

        return NULL;
    }
};

static bool blend_subrectRGBA8(uint8_t*& base, int& alloc_size, int& x, int&y, int& w, int& h,
                               uint8_t const* top, int x1, int y1, int w1, int h1)
{
    assert(base && alloc_size>=(w*h*4) && x>=0 && y>=0 && w>0 && h>0);
    assert(top && x1>=0 && y1>=0 && w1>0 && h1>0);

    int const x0 = x;
    int const y0 = y;
    int const w0 = w;
    int const h0 = h;

    if (x1<x) x = x1;
    if (y1<y) y = y1;

    if ((x0+w0)<(x1+w1)) {
        w = x1 + w1 - x;
    }
    else {
        w = x0 + w0 - x;
    }

    if ((y0+h0)<(y1+h1)) {
        h = y1 + h1 - y;
    }
    else {
        h = y0 + h0 - y;
    }

    int const dst_stride = w*4;
    if (w!=w0 || h!=h0) {
        int const required_size = h*dst_stride;
        int size = alloc_size;
        while (size<required_size) {
            size += (2<<20);
        }

        uint8* buf = (uint8_t*) malloc(size);
        if (NULL==buf) {
            x = x0;
            y = y0;
            w = w0;
            h = h0;
            return false;
        }

        memset(buf, 0, required_size);
        uint8* dst = buf + (y0-y)*dst_stride + (x0-x)*4;
        uint8* src = base;
        int const src_stride = w0*4;
        assert(src_stride<=dst_stride);
        for (int i=0; i<h0; ++i,src+=src_stride,dst+=dst_stride) {
            memcpy(dst, src, src_stride);
        }
        free(base);

        base = buf;
        alloc_size = size;
    }
    else {
        assert(x==x0 && y==y0);
    }

    // alpha blending
    uint8* dst = base + (y1-y)*dst_stride + (x1-x)*4;
    int const src_stride = w1*4;
    uint8 const* src = top;
    int a, ia;
    for (int i=0; i<h1; ++i,src+=src_stride,dst+=dst_stride) {
        uint8 const* s = src;
        uint8* d = dst;
        for (int j=0; j<w1; ++j,s+=4,d+=4) {
            if (s[3]>0) {
                a = 1024*s[3]/255; ia = 1024 - a;
                d[0] = (uint8) ((a*s[0] + ia*d[0])>>10);
                d[1] = (uint8) ((a*s[1] + ia*d[1])>>10);
                d[2] = (uint8) ((a*s[2] + ia*d[2])>>10);
                if (d[3]<s[3]) d[3] = s[3];
            }
        }
    }

    return true;
}

bool AVDecoder::AudioSubtitleThread_()
{
    assert(NULL!=formatCtx_);
    assert(0==subtitlePut_ && 0==subtitleGet_);
    assert(0==audioBufferPut_ && 0==audioBufferGet_);
    assert(0<audioStreamCount_ || 0<subtitleStreamCount_ || 0<subtitleUtil_.TotalStreams());

    bool const audio_on    = 0<audioStreamCount_;
    bool const subtitle_on = 0<subtitleStreamCount_ || 0<subtitleUtil_.TotalStreams();
    if (NULL==formatCtx_ || (!audio_on && !subtitle_on))
        return false;

    // subtitle
    SubtitleDecodeRAII subtitleRAII;
    AVCodecContext* subtitleCodecCtx = NULL;
    int const first_SRT_streams_index = (int) formatCtx_->nb_streams;
    subtitlePut_ = subtitleGet_ = 0;
    int src_video_width  = 1280;
    int dst_video_width  = 1280;
    int src_video_height = 720;
    int dst_video_height = 720;

    if (subtitle_on) {
        // lock
        std::lock_guard<std::mutex> lock(subtitleMutex_);

        // clear subtitle cache
        subtitleUtil_.ClearCache();

        // subtitle codec context
        if (0<=subtitleStreamIndex_ && subtitleStreamIndex_<first_SRT_streams_index) {
            AVStream* substream = formatCtx_->streams[subtitleStreamIndex_];
            assert(substream && substream->codecpar && AVMEDIA_TYPE_SUBTITLE==substream->codecpar->codec_type);
            if (substream && substream->codecpar && AVMEDIA_TYPE_SUBTITLE==substream->codecpar->codec_type) {
                subtitleCodecCtx = FFmpeg_AVCodecOpen(substream);
                if (NULL!=subtitleCodecCtx) {
                    ISO_639 lan = ISO_639_UNKNOWN;
                    AVDictionaryEntry const* t = av_dict_get(substream->metadata, "language", NULL, 0);
                    if (NULL!=t) {
                        lan = Translate_ISO_639(t->value);
                    }
                    subtitleUtil_.LoadStyles((char const*)subtitleCodecCtx->subtitle_header,
                                             subtitleCodecCtx->subtitle_header_size,
                                             lan);
                }
            }

            if (NULL==subtitleCodecCtx) {
                BL_LOG("** [error] fail to create subtitle(#%d) context\n", subtitleStreamIndex_);
                subtitleStreamIndex_ = subtitleStreamIndexSwitch_ = -1;
            }
        }

        if (0<=videoStreamIndex_ && videoStreamIndex_<first_SRT_streams_index) {
            AVStream const* stream = formatCtx_->streams[videoStreamIndex_];
            if (stream && stream->codecpar) {
                src_video_width  = stream->codecpar->width;
                src_video_height = stream->codecpar->height;
                dst_video_width  = videoWidth_;
                dst_video_height = videoHeight_;
            }
        }

        // pre-allocate subtitle memory
        int tmp_mem_size = 2<<20; // 2 MB >= 1920*1080
        if (subtitleStreamCount_>0) {
            int size = 1920*1080;
            if (size<src_video_width*src_video_height)
                size = src_video_width*src_video_height;

            size = (size*4+1023) & ~1023; // RGBA for graphics type subtitle
            if (tmp_mem_size<size)
                tmp_mem_size = size;
        }

        if (NULL==subtitleBuffer_ || subtitleBufferSize_<tmp_mem_size) {
            if (NULL!=subtitleBuffer_) {
                free(subtitleBuffer_);
            }
            subtitleBuffer_ = (uint8_t*) malloc(tmp_mem_size);
            if (NULL==subtitleBuffer_) {
                BL_ERR("failed to allocate subtitle memory...\n");
                subtitleStreamIndex_ = -1;
                subtitleBufferSize_ = 0;
                return false;
            }
            subtitleBufferSize_ = tmp_mem_size;
        }
    }

    // audio decoders
    // althought base audio stream may change, but the out resample format is fixed.
    audioBufferPut_ = audioBufferGet_ = 0;
    AudioDecoder audio_base(formatCtx_);
    AudioDecoder audio_aux1(formatCtx_), audio_aux2(formatCtx_), audio_aux3(formatCtx_);

    // audio pts tolerance is the half video frame time
    int64_t const audio_pts_tolerance = 50000000/((0<videoFrameRate100x_ && videoFrameRate100x_<3000) ? videoFrameRate100x_:3000);

    int64_t pts1, pts2;
    int bytes_offset_ext1(0), bytes_offset_ext2(0), bytes_offset_ext3(0);
    int bytes_per_sample = 0; // i.e. audioBytesPerSample_?
    bool do_multi_streams_pts_align = false;
    if (0<=audioStreamIndex_) {
        AVSampleFormat const sample_fmt = GetAVSampleFormat(audioInfo_.Format);
        int64 channel_layout = 0;
        if (AUDIO_TECHNIQUE_DEFAULT==audioInfo_.Technique) {
            channel_layout = av_get_default_channel_layout(audioInfo_.StreamChannels[0]);
        }

        // init timestamp - It's very important for some video formats with less precise
        // audio pts, like *.wmv.
        pts1 = videoPutPTS_; // video seeking timestamp, very rough.
        if (0<=videoStreamIndex_ && pts1!=0) {
            for (int i=0; i<10&&STATUS_PLAYING==status_&&0==interruptRequest_; ++i) {
                if (0==videoFramePut_) {
                    av_usleep(10000);
                }
                else {
                    // refined with actual video frame timestamp. Note video pts
                    // resolution is relative low to audio. Actual difference
                    // could still be as big as 1000000/29.97 ~= 33366.7 us
                    pts1 = decodedFrames_[0].PTS;
                    break;
                }
            }
        }
        audioPutPTS_ = audioGetPTS_ = pts1;

        // init all audio decoders with approximate initial timestamp
        audio_base.Init(audioStreamIndex_, sample_fmt, channel_layout, audioInfo_.SampleRate, pts1);
        bytes_per_sample = audio_base.NumBytesPerSample();
        if (0<=audioExtStreamIndex1_) {
            channel_layout = 0;
            do_multi_streams_pts_align = true;
            audio_aux1.Init(audioExtStreamIndex1_, sample_fmt, channel_layout, audioInfo_.SampleRate, pts1);
            bytes_offset_ext1 = bytes_per_sample;
            bytes_per_sample += audio_aux1.NumBytesPerSample();
            if (0<=audioExtStreamIndex2_) {
                audio_aux2.Init(audioExtStreamIndex2_, sample_fmt, channel_layout, audioInfo_.SampleRate, pts1);
                bytes_offset_ext2 = bytes_per_sample;
                bytes_per_sample += audio_aux2.NumBytesPerSample();
                if (0<=audioExtStreamIndex3_) {
                    audio_aux3.Init(audioExtStreamIndex3_, sample_fmt, channel_layout, audioInfo_.SampleRate, pts1);
                    bytes_offset_ext3 = bytes_per_sample;
                    bytes_per_sample += audio_aux3.NumBytesPerSample();
                }
            }
        }
        assert(0==(audioBufferWrapSize_%bytes_per_sample));
        assert(audioBytesPerFrame_==bytes_per_sample);
    }

    // benchmark
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
    double const print_period = 2.0;
    double next_print_time(mlabs::balai::system::GetTime()+print_period), t0, t1;
    double audio_packet_decode_time(0.0),audio_buffer_fill_time(0.0f);
    int64_t audio_buffer_time = -1;
    int decode_data_size(0), packet_data_size(0);
    int pkt_read(0), pkt1_read(0), pkt2_read(0), pkt3_read(0);
    int str_size(0), str1_size(0), str2_size(0), str3_size(0);
#endif

    // decoding loop
    AVPacketList* pk = NULL;
    while (STATUS_PLAYING==status_ && 0==interruptRequest_) {
        if (audio_on && audioBufferPut_<(audioBufferGet_+audioBufferWrapSize_)) {
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
            t1 = mlabs::balai::system::GetTime();
            pts1 = audioPutPTS_ - audioGetPTS_;
            if (pts1<audio_buffer_time)
                audio_buffer_time = pts1;
#endif
            int fill_samples = audio_base.NumSamples();
            if (fill_samples>0 && 0<=audioExtStreamIndex1_) {
                int s = audio_aux1.NumSamples();
                if (fill_samples>s)
                    fill_samples = s;

                if (fill_samples>0 && 0<=audioExtStreamIndex2_) {
                    s = audio_aux2.NumSamples();
                    if (fill_samples>s)
                        fill_samples = s;

                    if (fill_samples>0 && 0<=audioExtStreamIndex3_) {
                        s = audio_aux3.NumSamples();
                        if (fill_samples>s)
                            fill_samples = s;
                    }
                }

                // initial align pts for multiple audio stream
                if (do_multi_streams_pts_align && fill_samples>0) {
                    // find the late pts
                    pts1 = audio_base.Timestamp(); assert(pts1>=0);
                    pts2 = audio_aux1.Timestamp(); assert(pts2>=0);
                    if (pts1<pts2) pts1 = pts2;
                    if (0<=audioExtStreamIndex2_) {
                        pts2 = audio_aux2.Timestamp();
                        if (pts1<pts2) pts1 = pts2;
                        if (0<=audioExtStreamIndex3_) {
                            pts2 = audio_aux3.Timestamp();
                            if (pts1<pts2) pts1 = pts2;
                        }
                    }

                    // discard overdue samples
                    fill_samples = audio_base.DiscardSamples(pts1);
                    s = audio_aux1.DiscardSamples(pts1);
                    if (fill_samples>s)
                        fill_samples = s;

                    if (0<=audioExtStreamIndex2_) {
                        s = audio_aux2.DiscardSamples(pts1);
                        if (fill_samples>s)
                            fill_samples = s;
                        if (0<=audioExtStreamIndex3_) {
                            s = audio_aux3.DiscardSamples(pts1);
                            if (fill_samples>s)
                                fill_samples = s;
                        }
                    }

                    if (fill_samples>0) {
                        do_multi_streams_pts_align = false;
                    }
                }
            }

            // fill ring buffer
            if (fill_samples>0) {
                std::lock_guard<std::mutex> lock(audioMutex_); // a quick lock
                int const cur_data_size = audioBufferPut_ - audioBufferGet_;
                int const max_fill_bytes = audioBufferWrapSize_ - cur_data_size;
                int const max_fill_samples = max_fill_bytes/bytes_per_sample;
                int const put = ((int)audioBufferPut_)%audioBufferWrapSize_;
                assert(max_fill_bytes==(max_fill_samples*bytes_per_sample));
                assert(0==(put%bytes_per_sample));
                if (fill_samples>max_fill_samples) {
                    fill_samples = max_fill_samples;
                }

                int const fill_bytes = fill_samples*bytes_per_sample;
                uint8* dst = audioBuffer_ + put;
                if ((put+fill_bytes)<=audioBufferWrapSize_) {
                    audio_base.FillData(dst, fill_samples, bytes_per_sample, pts1, pts2);
                    if (0<=audioExtStreamIndex1_) {
                        audio_aux1.FillData(dst+bytes_offset_ext1, fill_samples, bytes_per_sample);
                        if (0<=audioExtStreamIndex2_) {
                            audio_aux2.FillData(dst+bytes_offset_ext2, fill_samples, bytes_per_sample);
                            if (0<=audioExtStreamIndex3_) {
                                audio_aux3.FillData(dst+bytes_offset_ext3, fill_samples, bytes_per_sample);
                            }
                        }
                    }
                }
                else {
                    int ss = (audioBufferWrapSize_ - put)/bytes_per_sample;
                    assert(ss>0 && (ss*bytes_per_sample)<fill_bytes);
                    audio_base.FillData(dst, ss, bytes_per_sample, pts1, pts2);
                    if (0<=audioExtStreamIndex1_) {
                        audio_aux1.FillData(dst+bytes_offset_ext1, ss, bytes_per_sample);
                        if (0<=audioExtStreamIndex2_) {
                            audio_aux2.FillData(dst+bytes_offset_ext2, ss, bytes_per_sample);
                            if (0<=audioExtStreamIndex3_) {
                                audio_aux3.FillData(dst+bytes_offset_ext3, ss, bytes_per_sample);
                            }
                        }
                    }

                    ss = fill_samples - ss; int64_t pts_dunny(0);
                    audio_base.FillData(audioBuffer_, ss, bytes_per_sample, pts_dunny, pts2);
                    if (0<=audioExtStreamIndex1_) {
                        audio_aux1.FillData(audioBuffer_+bytes_offset_ext1, ss, bytes_per_sample);
                        if (0<=audioExtStreamIndex2_) {
                            audio_aux2.FillData(audioBuffer_+bytes_offset_ext2, ss, bytes_per_sample);
                            if (0<=audioExtStreamIndex3_) {
                                audio_aux3.FillData(audioBuffer_+bytes_offset_ext3, ss, bytes_per_sample);
                            }
                        }
                    }
                }

                // advance pointer and pts
                audioBufferPut_ += fill_bytes;
                audioPutPTS_ = pts2;

                // audio late frame
                if (0<timeStartSysTime_ && NULL!=host_) {
                    int64_t const cur_time = av_gettime();
                    if ((timeStartSysTime_+pts1)<cur_time) {
                        host_->OnAudioLateFrame(id_, (int)((pts1-startTime_)/1000));
                    }
                }

                if (0==timeInterruptSysTime_) {
                    if (cur_data_size<=0) {
                        audioGetPTS_ = pts1; // initial value
                    }
                }
                else {
                    // recover form interrupt : fully trust pts2 - the new put PTS.
                    if (audioGetPTS_<audioPutPTS_) {
                        int const valid_size = ((int) (av_rescale(audioPutPTS_ - audioGetPTS_,
                                  audioBytesPerSecond_, AV_TIME_BASE)/audioBytesPerFrame_))*audioBytesPerFrame_;
                        if (valid_size<(audioBufferPut_-audioBufferGet_)) {
                            audioBufferGet_ = ((int) audioBufferPut_) - valid_size;
                            if ((audioBufferPut_-audioBufferGet_)>=(audioBytesPerSecond_/10)) { // 100ms
                                timeInterruptSysTime_ = 0;
#ifdef BL_AVDECODER_VERBOSE
                                BL_LOG("** Recovery from interrupt(time:%.1fms)\n", 0.001*(av_gettime()-timeInterruptSysTime_));
#endif
                            }
                        }
                        else { // if not resume successfully, sync again and fingers crossed
                            audioBufferPut_ = audioBufferGet_ = 0;
                        }
                    }
                    else {
                        audioBufferPut_ = audioBufferGet_ = 0;
                    }
                }
            }

#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
            t0 = mlabs::balai::system::GetTime();
            audio_buffer_fill_time += (t0 - t1);
            str_size  = audio_base.DataSize();
            str1_size = audio_aux1.DataSize();
            str2_size = audio_aux2.DataSize();
            str3_size = audio_aux3.DataSize();
#endif
            // decode more
            if (0!=audioBufferFlushing_ || audio_base.DataSize()<audioBufferWrapSize_) {
                pk = audioQueue_.Get();
                if (NULL!=pk) {
                    if (0==audioBufferFlushing_) {
                        assert(pk->pkt.stream_index==audioStreamIndex_);
                        assert(audioStreamIndex_==audio_base.StreamId());
                        pts1 = audio_base.ReadPacket(&(pk->pkt));
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
                        if (0<pts) {
                            packet_data_size += pk->pkt.size;
                            decode_data_size += (audio_base.DataSize() - str_size);
                            ++pkt_read;
                        }
#endif
                        ReleaseAVPacketList_(pk); pk = NULL;

                        // correct pts
                        if (0<pts1 && audio_base.ValidPTS()>0) {
                            pts2 = pts1 - audioPutPTS_; // difference
                            if (abs(pts2)>audio_pts_tolerance) {
                                audioPutPTS_ = pts1;
                                audioGetPTS_ = pts1 - av_rescale(audioBufferPut_-audioBufferGet_, AV_TIME_BASE, audioBytesPerSecond_);
                                if (audioGetPTS_<0) {
                                    audioGetPTS_ = 0;
                                }
#ifdef BL_AVDECODER_VERBOSE
                                if (0!=timeStartSysTime_) {
                                    BL_LOG("** audio : PTS error %+lld ns fixed\n", pts2);
                                }
#ifndef DEBUG_AUDIO_DECODE_BENCHMARK
//                            }
//                            else if ((abs(pts2)*audioInfo_.SampleRate)>1000000) { // 1 sample error
//                                BL_LOG("** audio : PTS error %+lld ns persists...\n", pts2);
#endif
#endif
                            }
                        }
                    }
                    else { // it only allows 1-audio stream changing
                        assert(AUDIO_TECHNIQUE_DEFAULT==audioInfo_.Technique);
                        assert(-1==audioExtStreamIndex1_);
                        assert(-1==audioExtStreamIndex2_);
                        assert(-1==audioExtStreamIndex3_);
                        if (pk->pkt.stream_index==audio_base.StreamId()) {
                            audio_base.ReadPacket(&(pk->pkt));
                            ReleaseAVPacketList_(pk);  pk = NULL;
                        }
                        else {
                            pts1 = audio_base.StreamChanged(&(pk->pkt));
                            ReleaseAVPacketList_(pk); pk = NULL;
                            if (pts1>=0) {
                                std::lock_guard<std::mutex> lock(audioMutex_);
                                timeAudioCrossfade_ = timeStartSysTime_ + pts1;

                                // overlap check
                                int size = audioBufferPut_ - audioBufferGet_;
                                if (pts1<audioPutPTS_) {
                                    int const bytes = (int) (av_rescale(audioPutPTS_-pts1, audioBytesPerSecond_, AV_TIME_BASE)/audioBytesPerFrame_)*audioBytesPerFrame_;
                                    if (2*bytes<size) {
                                        audioBufferPut_ -= bytes;
                                        size = audioBufferPut_ - audioBufferGet_;
                                    }
                                }

                                // sync
                                audioPutPTS_ = pts1;
                                audioGetPTS_ = pts1 - av_rescale(size, AV_TIME_BASE, audioBytesPerSecond_);
                                if (audioGetPTS_<0)
                                    audioGetPTS_ = 0;
                                audioBufferFlushing_ = 0;
#ifdef BL_AVDECODER_VERBOSE
                                BL_LOG("** audio : new stream #%d started!\n", audio_base.StreamId());
#endif
                            }
                        }
                    }
                }
            }

            pk = audioQueueExt1_.Get();
            if (NULL!=pk) {
                assert(pk->pkt.stream_index==audioExtStreamIndex1_);
                audio_aux1.ReadPacket(&(pk->pkt));
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
                packet_data_size += pk->pkt.size;
                decode_data_size += (audio_aux1.DataSize() - str1_size);
                ++pkt1_read;
#endif
                ReleaseAVPacketList_(pk); pk = NULL;
            }

            pk = audioQueueExt2_.Get();
            if (NULL!=pk) {
                assert(pk->pkt.stream_index==audioExtStreamIndex2_);
                audio_aux2.ReadPacket(&(pk->pkt));
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
                packet_data_size += pk->pkt.size;
                decode_data_size += (audio_aux2.DataSize() - str2_size);
                ++pkt2_read;
#endif
                ReleaseAVPacketList_(pk); pk = NULL;
            }

            pk = audioQueueExt3_.Get();
            if (NULL!=pk) {
                assert(pk->pkt.stream_index==audioExtStreamIndex3_);
                audio_aux3.ReadPacket(&(pk->pkt));
#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
                packet_data_size += pk->pkt.size;
                decode_data_size += (audio_aux3.DataSize() - str3_size);
                ++pkt3_read;
#endif
                ReleaseAVPacketList_(pk); pk = NULL;
            }

#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
            t1 = mlabs::balai::system::GetTime();
            audio_packet_decode_time += (t1 - t0);
#endif
        }
/*
 * [caution] [TO-DO] remove overdue audio data, consume some audio packets...
 *
 * if host doesn't consume audio data constantly, the packets will be taken by
 * audio queue eventually. hence break the decoding process. it must release some
 * audio packets in that case...
 *
        else if (audio_on && 0!=timeStartSysTime_) {
            // to reach here... audioBufferPut_ >= (audioBufferGet_+audioBufferWrapSize_)
            pts1 = av_gettime() - timeStartSysTime_;
            if ((audioGetPTS_+4*AV_TIME_BASE)<pts1) {
                // adjust audio buffer get
            }
        }
 */

        // subtitle changed
        if (subtitleBufferFlushing_) {
            std::lock_guard<std::mutex> lock(subtitleMutex_);

            if (NULL!=subtitleCodecCtx) {
                avcodec_close(subtitleCodecCtx);
                avcodec_free_context(&subtitleCodecCtx);
                subtitleCodecCtx = NULL;
            }

            if (0<=subtitleStreamIndex_ && subtitleStreamIndex_<first_SRT_streams_index) {
                AVStream* substream = formatCtx_->streams[subtitleStreamIndex_];
                assert(substream && substream->codecpar && AVMEDIA_TYPE_SUBTITLE==substream->codecpar->codec_type);
                if (substream && substream->codecpar && AVMEDIA_TYPE_SUBTITLE==substream->codecpar->codec_type) {
                    subtitleCodecCtx = FFmpeg_AVCodecOpen(substream);
                    if (NULL!=subtitleCodecCtx) {
                        ISO_639 lan = ISO_639_UNKNOWN;
                        AVDictionaryEntry const* t = av_dict_get(substream->metadata, "language", NULL, 0);
                        if (NULL!=t) {
                            lan = Translate_ISO_639(t->value);
                        }
                        subtitleUtil_.LoadStyles((char const*)subtitleCodecCtx->subtitle_header,
                                                 subtitleCodecCtx->subtitle_header_size,
                                                 lan);
                    }
                }

                if (NULL==subtitleCodecCtx) {
                    BL_LOG("** [error] fail to create subtitle(#%d) context\n", subtitleStreamIndex_);
                    subtitleStreamIndex_ = subtitleStreamIndexSwitch_ = -1;
                    FlushQueue_(subtitleQueue_);
                }
            }
            subtitleBufferFlushing_ = 0;
        }

        //
        // subtitle decoding...
        //
        // Note the subtitle decoding can be fully parallelized. I was using std::async() both for
        // softsub and hardsub decoding. (the hardsub takes < 5ms and softsub take 1ms to 20ms)
        //
        // According to C++ standard, If the std::future obtained from std::async is not moved
        // from or bound to a reference, the destructor of the std::future will BLOCK at the end
        // of the full expression until the asynchronous operation completes.
        // see http://en.cppreference.com/w/cpp/thread/async
        //
        // Although, std::async() does not block by the temp std::feature object in visual studio 2012.
        //
        if (0<=subtitleStreamIndex_ && subtitleGet_==subtitlePut_) {
            pk = subtitleQueue_.Get();
            if (NULL!=pk) {
                // hardsub
                assert(subtitleStreamIndex_<first_SRT_streams_index);
                assert(NULL!=subtitleCodecCtx && pk->pkt.stream_index==subtitleStreamIndex_);
                if (NULL!=subtitleCodecCtx && pk->pkt.stream_index==subtitleStreamIndex_) {
#ifdef DEBUG_SUBTITLE_DECODE_BENCHMARK
                    double const t0 = mlabs::balai::system::GetTime();
#endif
                    std::lock_guard<std::mutex> lock(subtitleMutex_);
                    AVPacket& packet = pk->pkt;
                    AVSubtitle sub; memset(&sub, 0, sizeof(sub));
                    int got_sub = 0;
                    got_sub = (0<avcodec_decode_subtitle2(subtitleCodecCtx, &sub, &got_sub, &packet)) && got_sub;

                    int64_t pkt_pts = AV_NOPTS_VALUE;
                    int pkt_dur = 0;
                    if (got_sub) {
                        if (AV_NOPTS_VALUE!=packet.pts) {
                            pkt_pts = av_rescale_q(packet.pts, av_codec_get_pkt_timebase(subtitleCodecCtx), timebase_q);
                        }
                        else if (AV_NOPTS_VALUE!=packet.dts) {
                            pkt_pts = av_rescale_q(packet.dts, av_codec_get_pkt_timebase(subtitleCodecCtx), timebase_q);
                        }

                        if (packet.duration>0) {
                            pkt_dur = (int) (1000.0*(av_q2d(av_codec_get_pkt_timebase(subtitleCodecCtx))*packet.duration));
                        }
                    }

                    ReleaseAVPacketList_(pk);

                    if (got_sub) {
                        subtitleGraphicsFmt_ = (0==sub.format) ? 4:1; // is graphics format
                        AVSubtitleType const type = (sub.num_rects>0) ? (sub.rects[0]->type):SUBTITLE_NONE;
                        subtitleWidth_ = subtitleHeight_ = subtitleRectCount_ = 0;
                        uint8_t* ptr = subtitleBuffer_;
                        int rects(0), xx(0), yy(0);
                        for (unsigned i=0; i<sub.num_rects; ++i) {
                            AVSubtitleRect* rt = sub.rects[i];
                            if (SUBTITLE_BITMAP==rt->type) {
                                assert(4==subtitleGraphicsFmt_);
                                int const rect_w = rt->w * dst_video_width / src_video_width;
                                int const rect_h = rt->h * dst_video_height / src_video_height;
                                if (SUBTITLE_BITMAP!=type ||
                                    !subtitleRAII.InitSwsContext(rect_w, rect_h, rt->w, rt->h)) {
                                    continue;
                                }

                                SubtitleRect& mainRect = subtitleRects_[0];
                                int const tmp_mem_size = rect_w*rect_h*4;
                                if (0==rects) {
                                    mainRect.Reset();
                                    if (subtitleBufferSize_<tmp_mem_size) {
                                        free(subtitleBuffer_);
                                        subtitleBuffer_ = (uint8_t*) malloc(tmp_mem_size);
                                        if (NULL==subtitleBuffer_) {
                                            BL_ERR("failed to allocate subtitle memory...\n");
                                            subtitleStreamIndex_ = -1;
                                            subtitleBufferSize_ = 0;
                                            break;
                                        }
                                        subtitleBufferSize_ = tmp_mem_size;
                                    }

                                    // convert
                                    if (!subtitleRAII.Convert(subtitleBuffer_, rect_w,
                                                              rt->data, rt->linesize, rt->h)) {
                                        continue;
                                    }

                                    xx = rt->x * dst_video_width/src_video_width;
                                    yy = rt->y * dst_video_height/src_video_height;
#ifdef BL_AVDECODER_VERBOSE
                                    // this happens... if hardsub didn't layout well...
                                    if (rect_w>videoWidth_) {
                                        BL_LOG("hardsub out of bound - subtitle:%dx%d video:%dx%d\n",
                                               rect_w, rect_h, videoWidth_, videoHeight_);
                                    }
#endif
                                    subtitleWidth_  = rect_w;
                                    subtitleHeight_ = rect_h;
                                    mainRect.PlayResX = (float) videoWidth_;
                                    mainRect.PlayResY = (float) videoHeight_;
                                    mainRect.X = (float) xx;
                                    mainRect.Y = (float) yy;
                                    mainRect.Width  = (float) subtitleWidth_;
                                    mainRect.Height = (float) subtitleHeight_;

                                    // timestamp are not valid.
                                    // it should use subtitlePTS_ and subtitleDuration_.
                                    mainRect.TimeStart = mainRect.TimeEnd = 0;

                                    subtitleRectCount_ = rects = 1;
                                }
                                else {
                                    uint8_t const* blendLayer = subtitleRAII.GenerateBlendLayer(
                                                                rect_w, rect_h, rt->data, rt->linesize, rt->h);
                                    if (NULL!=blendLayer &&
                                        blend_subrectRGBA8(subtitleBuffer_, subtitleBufferSize_,
                                                            xx, yy, subtitleWidth_, subtitleHeight_,
                                                            blendLayer,
                                                            rt->x * dst_video_width/src_video_width,
                                                            rt->y * dst_video_height/src_video_height,
                                                            rect_w, rect_h)) {
                                        mainRect.X = (float) xx;
                                        mainRect.Y = (float) yy;
                                        mainRect.Width  = (float) subtitleWidth_;
                                        mainRect.Height = (float) subtitleHeight_;
                                        ++rects;
                                    }
                                    else {
                                        continue;
                                    }
                                }
                            }
                            else if (SUBTITLE_ASS==rt->type) {
                                assert(1==subtitleGraphicsFmt_);
                                if (SUBTITLE_ASS!=type || NULL==rt->ass) {
                                    continue;
                                }

                                int w(0), h(0);
                                int ret = subtitleUtil_.Dialogue_ASS(
                                                    ptr, (int)(subtitleBuffer_+subtitleBufferSize_-ptr), w, h,
                                                    subtitleRects_+subtitleRectCount_, Subtitle::MAX_NUM_SUBTITLE_RECTS-subtitleRectCount_,
                                                    rt->ass);
                                if (ret<=0) {
                                    if (-ret==-(w*h)) {
                                        // buffer size too small
                                    }

                                    if (rects>0)
                                        break; // !!! fix me

                                    //
                                    // TO-DO : realloc buffer and try again!?
                                    //
                                }

                                if (ret>0) {
                                    SubtitleRect const& rect = subtitleRects_[subtitleRectCount_];
                                    subtitleRectCount_ += ret;
                                    if (1==++rects) {
                                        // we respect to ass timestamp. not sub.
                                        // but the question is, do it need to shift by startTime_?
                                        sub.pts = startTime_ + int64_t(rect.TimeStart)*1000;
                                        sub.start_display_time = rect.TimeStart;
                                        sub.end_display_time = rect.TimeEnd;

                                        subtitleWidth_ = w;
                                        subtitleHeight_ = h;

                                        ptr += w*h; // really?
                                        break;
                                    }
                                    else {
                                        //...
                                        if ((int)sub.start_display_time>rect.TimeStart) {
                                            sub.start_display_time = rect.TimeStart;
                                        }
                                        if ((int)sub.end_display_time<rect.TimeEnd) {
                                            sub.end_display_time = rect.TimeEnd;
                                        }
                                    }
                                }
                                break; // just one ass!?
                            }
                            else if (SUBTITLE_TEXT==rt->type) {
                                assert(1==subtitleGraphicsFmt_);
                                if (SUBTITLE_TEXT!=type || NULL==rt->text) {
                                    continue;
                                }

                                //
                                // TO-DO : finish my job...

                            }
                            else {
                                // unknown type!?
                            }
                        }

                        if (rects>0) {
                            if (AV_NOPTS_VALUE!=sub.pts) {
                                subtitlePTS_ = sub.pts;
                            }
                            else if (AV_NOPTS_VALUE!=pkt_pts) {
                                subtitlePTS_ = pkt_pts;
                            }
                            else {
                                BL_LOG("Subtitle thread : no reliable PTS\n");
                                subtitlePTS_ = AV_TIME_BASE/2; // 0.5 sec after current video time
                                if (0<=audioStreamIndex_)
                                    subtitlePTS_ += audioGetPTS_;
                                else
                                    subtitlePTS_ += decodedFrames_[videoFrameGet_%NUM_VIDEO_BUFFER_FRAMES].PTS;
                            }

                            int const sub_duration = (int) sub.end_display_time - (int) sub.start_display_time;
                            if (100<sub_duration) {
                                subtitleDuration_ = sub_duration;
                            }
                            else if (pkt_dur>100) {
                                subtitleDuration_ = pkt_dur;
                            }
                            else { // i guess
                                int const subtitle_duration_early = 3000; //  3 secs
                                int const subtitle_duration_late = 10000; // 10 secs
                                subtitleDuration_ = subtitle_duration_early + subtitle_duration_late*subtitleWidth_/src_video_width;
                                if (subtitleDuration_<subtitle_duration_early)
                                    subtitleDuration_ = subtitle_duration_early;
                                else if (subtitleDuration_>subtitle_duration_late)
                                    subtitleDuration_ = subtitle_duration_late;
                            }

                            ++subtitlePut_; // do this last
#ifdef DEBUG_SUBTITLE_DECODE_BENCHMARK
                            BL_LOG("** Embedded subtitle #%d (%ssub) finished in %.1fms\n",
                                   subtitleGet_, (SUBTITLE_BITMAP==type) ? "hard":"soft",
                                   1000.0*(mlabs::balai::system::GetTime()-t0));
                        }
                        else {
                            BL_LOG("** Embedded subtitle #%d failed(%.1fms)!?\n",
                                   subtitleGet_, 1000.0*(mlabs::balai::system::GetTime()-t0));
#endif
                        }

                        // free subtitle
                        avsubtitle_free(&sub);
                    }
                }
                else {
                    ReleaseAVPacketList_(pk);
                }
            }
            else if (first_SRT_streams_index<=subtitleStreamIndex_) {
                // softsub
                assert(subtitleStreamIndex_<(first_SRT_streams_index+subtitleUtil_.TotalStreams()));
                assert(NULL==subtitleCodecCtx);
                int const subId = subtitleStreamIndex_ - first_SRT_streams_index;
                int const timestamp = (0<=audioStreamIndex_) ? int((audioGetPTS_-startTime_)/1000):int((videoGetPTS_-startTime_)/1000);
                if (!subtitleUtil_.IsFinish(subId, timestamp)) {
#ifdef DEBUG_SUBTITLE_DECODE_BENCHMARK
                    double const t0 = mlabs::balai::system::GetTime();
#endif
                    std::lock_guard<std::mutex> lock(subtitleMutex_);
                    subtitleRectCount_ = subtitleWidth_ = subtitleHeight_ = 0;
                    int const ret = subtitleUtil_.Publish(subId, timestamp,
                                                        subtitleBuffer_, subtitleBufferSize_,
                                                        subtitleWidth_, subtitleHeight_,
                                                        subtitleRects_, Subtitle::MAX_NUM_SUBTITLE_RECTS);
                    if (ret>0) {
                        int startTime = subtitleRects_[0].TimeStart;
                        int endTime = startTime;
                        for (int i=0; i<ret; ++i) {
                            SubtitleRect& rt = subtitleRects_[i];
                            if (startTime>rt.TimeStart)
                                startTime = rt.TimeStart;
                            if (endTime<rt.TimeEnd)
                                endTime = rt.TimeEnd;
                        }

                        subtitleGraphicsFmt_ = 1;
                        subtitleRectCount_ = ret;
                        subtitlePTS_ = startTime_ + int64_t(startTime)*1000;
                        subtitleDuration_ = endTime - startTime;
                        ++subtitlePut_; // do this last

#ifdef DEBUG_SUBTITLE_DECODE_BENCHMARK
                        BL_LOG("** External subtitle #%d finished in %.1fms\n",
                               subtitleGet_, 1000.0*(mlabs::balai::system::GetTime()-t0));
#endif
                    }
                    else {
                        BL_LOG("external subtitle get nothing!\n"); // error
                    }
                }
            }
        }

        // take a short nap if we're good.
        if (0==timeInterruptSysTime_) {
            if (!audio_on || audioBufferPut_>(audioBufferGet_+(audioBufferWrapSize_*8/10))) {
                if (subtitleStreamIndex_<0 || subtitleGet_<subtitlePut_ ||
                    (subtitleStreamIndex_<first_SRT_streams_index && 0==subtitleQueue_.Size())) {
                    av_usleep(2000);
                }
            }
        }

#ifdef DEBUG_AUDIO_DECODE_BENCHMARK
        t1 = mlabs::balai::system::GetTime();
        if (next_print_time<t1) {
            t0 = (t1 - next_print_time) + print_period;
            BL_LOG(" [audio %.1fkbps/%.1fkbs]  decode:%.2f%% + fill:%.2f%%  buffer:%llums  %.1fpks/s(%dms) %.1fpks/s(%dms) %.1fpks/s(%dms) %.1fpks/s(%dms)\n",
                    (decode_data_size>>7)/t0, (packet_data_size>>7)/t0,
                    100.0*audio_packet_decode_time/t0, 100.0*audio_buffer_fill_time/t0,
                    audio_buffer_time/1000, // buffer

                    // 4 streams
                    pkt_read/t0, (0<=audioStreamIndex_) ? (1000*audio_base.NumSamples()/audioInfo_.SampleRate):0,
                    pkt1_read/t0, (0<=audioExtStreamIndex1_) ? (1000*audio_aux1.NumSamples()/audioInfo_.SampleRate):0,
                    pkt2_read/t0, (0<=audioExtStreamIndex2_) ? (1000*audio_aux2.NumSamples()/audioInfo_.SampleRate):0,
                    pkt3_read/t0, (0<=audioExtStreamIndex3_) ? (1000*audio_aux3.NumSamples()/audioInfo_.SampleRate):0);

            decode_data_size = packet_data_size = 0;
            pkt_read = pkt1_read = pkt2_read = pkt3_read = 0;
            audio_packet_decode_time = audio_buffer_fill_time = 0.0;
            next_print_time = t1 + print_period;
            audio_buffer_time = audioPutPTS_ - audioGetPTS_;
        }
#endif
    }

    // lock to ensure Subtitle job is done
    {
        std::lock_guard<std::mutex> lock(subtitleMutex_);
        if (NULL!=subtitleCodecCtx) {
            avcodec_close(subtitleCodecCtx);
            avcodec_free_context(&subtitleCodecCtx);
        }
    }

    return true;
}
//---------------------------------------------------------------------------------------
void AVDecoder::MainThread_()
{
    assert(0==timeStartSysTime_);
    assert(0==videoFramePut_ && 0==videoFrameGet_);
    assert(0==subtitlePut_ && 0==subtitleGet_);
    assert(0==audioBufferPut_ && 0==audioBufferGet_);

    //
    // Packets reading (av_read_frame) thread. This thread is mostly(95+%) idle.
    // The original design of this thread including decode video frames. But in some
    // cases, when video frame decoding take too long (200+ms), it interrupts audio
    // decoding. Video frame decoding must have its own thread.
    //
#ifdef DEBUG_PACKET_READING_BENCHMARK
    int stream_packets[128];
    int video_packets(0), audio_packets(0), other_packets(0), total_packets_hi(0);
    int64_t audio_data_size(0), video_data_size(0), other_data_size(0);
    double video_time(0.0), audio_time(0.0), other_time(0.0), sleep_time(0.0);
    double video_time_hi(0.0), audio_time_hi(0.0), last_print(mlabs::balai::system::GetTime()), t0;
    memset(stream_packets, 0, sizeof(stream_packets));
#endif

    // buffering control
    int const live_streaming_buffer_factor = (duration_<0||liveStream_) ? 32:1;
    int const buffering_duration_ms = 200; // 250

    int const total_streams = (int) formatCtx_->nb_streams;
    if (0<=audioStreamIndex_ && audioStreamIndex_==audioInfo_.StreamIndices[0] &&
        AUDIO_FORMAT_UNKNOWN!=audioInfo_.Format && audioInfo_.SampleRate>0 && 
        0<audioInfo_.TotalStreams && 0<audioInfo_.TotalChannels && 0<audioInfo_.StreamChannels[0]) {
        int64_t const frames_buffering = (av_rescale(buffering_duration_ms, audioBytesPerSecond_, 1000)*2)/audioBytesPerFrame_;

        // default capacity, INIT_AUDIO_BUFFER_SIZE = 1920,000 Bytes
        int frames_capacity = audioBufferCapacity_/audioBytesPerFrame_;
        if (frames_capacity<frames_buffering) {
            int const new_capacity = (frames_buffering*audioBytesPerFrame_ + 1023) & ~1023;
            assert(audioBufferCapacity_<new_capacity);
#ifdef BL_AVDECODER_VERBOSE
            BL_ERR("** audio : warning!!! expand audio buffer %d->%d Bytes\n", audioBufferCapacity_, new_capacity);
#endif
            uint8_t* new_audio_buffer = (uint8_t*) malloc(new_capacity);
            if (NULL!=new_audio_buffer) {
                free(audioBuffer_);
                audioBuffer_ = new_audio_buffer;
                audioBufferCapacity_ = new_capacity;
                frames_capacity = audioBufferCapacity_/audioBytesPerFrame_;
            }
        }

        if (frames_capacity<=frames_buffering) {
            audioBufferWrapSize_ = frames_capacity*audioBytesPerFrame_;
        }
        else {
            audioBufferWrapSize_ = (int) (frames_buffering*audioBytesPerFrame_);
        }
        audioBufferPut_ = audioBufferGet_ = 0;

        //
        // !!!
        if (audioBufferWrapSize_<=0 || NULL==audioBuffer_) {
            audioInfo_.Reset();
            audioStreamIndex_ = audioStreamIndexSwitch_ = -1;
            audioExtStreamIndex1_ = audioExtStreamIndex2_ = audioExtStreamIndex3_ = -1;
            audioBytesPerFrame_ = audioBytesPerSecond_ = 0;
        }
    }
    else {
        audioInfo_.Reset();
        audioStreamIndex_ = audioStreamIndexSwitch_ = -1;
        audioExtStreamIndex1_ = audioExtStreamIndex2_ = audioExtStreamIndex3_ = -1;
        audioBytesPerFrame_ = audioBytesPerSecond_ = 0;
        if (audioStreamCount_>0) {
            BL_ERR("Audio device not connected... play mute!\n");
        }
    }

    bool const video_stream = (0<=videoStreamIndex_);
    bool const audio_stream = (0<=audioStreamIndex_);
    bool const subtitle_stream = (0<subtitleStreamCount_ || 0<subtitleUtil_.TotalStreams());

    // video buffering - since to change video stream is not allowed, values could be const.
    int const video_packet_count_full = live_streaming_buffer_factor*NUM_VIDEO_BUFFER_PACKETS;
    int64_t const video_packet_duration_good = video_stream ?
         av_rescale_q(live_streaming_buffer_factor*buffering_duration_ms*1000, timebase_q, formatCtx_->streams[videoStreamIndex_]->time_base):0;

    // audio buffering - note main audio stream can be changed
    int const audio_buffer_size_good = audio_stream ?
                   (int) av_rescale(buffering_duration_ms*1000, audioBytesPerSecond_, AV_TIME_BASE):0;
    int64_t audio_packet_duration_good = audio_stream ?
                   av_rescale_q(buffering_duration_ms*1000, timebase_q, formatCtx_->streams[audioStreamIndex_]->time_base):0;

    // start video frame decoding thread if needed
    std::thread videoThread, audiosubtitleThread;
    if (video_stream) {
        videoThread = move(thread([this] { VideoThread_(); }));
    }
    else {
        assert(audio_stream);
        if (!audio_stream) {
            return;
        }
    }

    // subtitle and audio thread
    if (audio_stream || subtitle_stream) {
        audiosubtitleThread = move(thread([this] { AudioSubtitleThread_(); }));
    }

    timeAudioCrossfade_ = timeStartSysTime_ = timeInterruptSysTime_ = 0;

    AVPacketList* pk = NULL;
    while (STATUS_PLAYING==status_ && 0==interruptRequest_) {
        if (0==timeStartSysTime_ && (!video_stream || videoFramePut_>=NUM_VIDEO_BUFFER_FRAMES) &&
            (!audio_stream || audioGetPTS_<audioPutPTS_ || videoQueue_.Size()>=(NUM_VIDEO_BUFFER_PACKETS*2))) {
            int64_t const pts_frame = av_rescale(100, AV_TIME_BASE, videoFrameRate100x_);
            int64_t const pts_diff_threshold = pts_frame<100000 ? 100000:pts_frame;
            bool kickoff = true;
            if (video_stream && audio_stream && audioGetPTS_<audioPutPTS_) {    
                int64_t const pts_diff = audioGetPTS_ - decodedFrames_[0].PTS;
                if (pts_diff>pts_diff_threshold) {
                    // discard video frames
                    // frame_buffer_count>=NUM_VIDEO_BUFFER_FRAMES make video decoding thread be hold.
                    // so it's safe to remove some video frames. (if we set videoFramePTS_ last)
                    int overdues = 0;
                    while (overdues<NUM_VIDEO_BUFFER_FRAMES && decodedFrames_[overdues].PTS<audioGetPTS_) {
                        videoDecoder_.finish_frame(decodedFrames_[overdues]);
                        ++overdues;
                    }

                    int new_put = 0;
                    if (overdues<videoFramePut_) {
                        new_put = videoFramePut_ - overdues;
                        for (int i=0; i<new_put; ++i) {
                            VideoFrame& dst = decodedFrames_[i];
                            VideoFrame& src = decodedFrames_[i+overdues];

                            if (VideoFrame::NV12_HOST==src.Type) {
                                assert(VideoFrame::NV12_HOST==dst.Type);
                                memmove((void*)dst.FramePtr, (void*)src.FramePtr, videoFrameDataSize_);
                            }
                            else {
                                dst.FramePtr = src.FramePtr; // this could be wrong!?
                            }

                            dst.PTS        = src.PTS;
                            dst.Type       = src.Type;
                            dst.ColorSpace = src.ColorSpace;
                            dst.ColorRange = src.ColorRange;
                            dst.Width      = src.Width;
                            dst.Height     = src.Height;
                            dst.ID         = src.ID;
                        }
                    }
                    videoFramePut_ = new_put;

                    kickoff = false;
                }
                else if (pts_diff<-pts_diff_threshold) {
                    std::lock_guard<std::mutex> lock(audioMutex_); // discard audio data
                    assert(0==audioBufferGet_);
                    audioBufferGet_ = 0;
                    if (audioPutPTS_<=decodedFrames_[0].PTS) {
                        audioGetPTS_ = audioPutPTS_;
                        audioBufferPut_ = 0;
                    }
                    else {
                        int const discard_bytes = (int) ((av_rescale(-pts_diff, audioBytesPerSecond_, AV_TIME_BASE)/audioBytesPerFrame_)*audioBytesPerFrame_);
                        if (discard_bytes<audioBufferPut_) {
                            memmove(audioBuffer_, audioBuffer_+discard_bytes, (int)audioBufferPut_-discard_bytes);
                            audioGetPTS_ = decodedFrames_[0].PTS;
                        }
                        else {
                            audioGetPTS_ = audioPutPTS_;
                            audioBufferPut_ = 0;
                        }
                    }

                    kickoff = (audioGetPTS_<audioPutPTS_);
                }
            }

            if (kickoff) {
                int toStart = 0;
                if (NULL!=host_) {
                    int const ts = (int) (((video_stream ? decodedFrames_[0].PTS:audioGetPTS_) - startTime_)/1000);
                    int const dur = (int)(duration_/1000);

                    // prepare start, play music...
                    toStart = host_->OnStart(id_, ts, dur, video_stream, audio_stream);

#ifdef BL_AVDECODER_VERBOSE
                    int64_t const timestart = (toStart>0) ? av_gettime_relative():0;
#endif
                    int total_delay = 0;
                    while (toStart>0 && STATUS_PLAYING==status_ && 0==interruptRequest_) {
                        total_delay += toStart;
                        av_usleep(toStart*1000);
                        toStart = host_->OnStart(id_, ts, dur, video_stream, audio_stream);
                    }

#ifdef BL_AVDECODER_VERBOSE
                    if (0!=timestart) {
                        BL_LOG("** kicking off... (decoder#%d) host required %dms(%lldns) delay start\n",
                               id_, total_delay, av_gettime_relative()-timestart);
                    }
#endif
                }

#ifdef BL_AVDECODER_VERBOSE
                int vPTS = (int) ((decodedFrames_[0].PTS-startTime_)/1000);
                if (vPTS<0) vPTS = 0;
                int const ms = vPTS%1000;
                int ss = vPTS/1000;
                int mm = ss/60; ss %= 60;
                int hh = mm/60; mm %= 60;
                if (audio_stream) {
                    if (audioGetPTS_<audioPutPTS_) {
                        int ms1 = (int) ((audioGetPTS_-startTime_)/1000);
                        if (ms1<0) ms1 = 0;
                        int ss1 = ms1/1000; ms1 %= 1000;
                        int mm1 = ss1/60; ss1 %= 60;
                        int hh1 = mm1/60; mm1 %= 60;

                        int64_t const diff = audioGetPTS_-decodedFrames_[0].PTS;
                        BL_LOG("** start play audioPTS=%d:%02d:%02d:%03d(%dms %dbytes)  videoPTS=%d:%02d:%02d:%03d(%d frames)  dPTS=%+llims%s\n",
                                hh1, mm1, ss1, ms1, int(audioPutPTS_-audioGetPTS_)/1000, (int)audioBufferPut_-(int)audioBufferGet_,
                                hh, mm, ss, ms, (int)videoFramePut_-(int)videoFrameGet_, diff/1000, abs(diff>pts_diff_threshold) ? "!!!":"");
                    }
                    else {
                        BL_LOG("** start play videoPTS=%d:%02d:%02d:%03d(%d frames), no audio data loaded.\n",
                            hh, mm, ss, ms, (int)videoFramePut_-(int)videoFrameGet_);
                    }
                }
                else {
                    BL_LOG("** start play videoPTS=%d:%02d:%02d:%03d(%d frames), no audio.\n",
                            hh, mm, ss, ms, (int)videoFramePut_-(int)videoFrameGet_);
                }
#endif
                // timeLastUpdateFrameSysTime_ may be 0, if UpdateFrame() havn't been called yet.
                timeAudioCrossfade_ = timeLastUpdateFrameSysTime_ = av_gettime();
                timeStartSysTime_ = timeAudioCrossfade_ - decodedFrames_[0].PTS; // play!
                if (toStart<0) {
                    FlushQueue_(audioQueue_);
                    break;
                }
            }
        }

        // subtitle stream changed
        if (subtitle_stream && 0==subtitleBufferFlushing_ && subtitleStreamIndexSwitch_!=subtitleStreamIndex_) {
            std::lock_guard<std::mutex> lock(subtitleMutex_);
            FlushQueue_(subtitleQueue_); // clear all subtitles now...
            subtitleBufferFlushing_ = 1;
            if (0<=subtitleStreamIndex_ && subtitleStreamIndex_<total_streams) {
                assert(AVMEDIA_TYPE_SUBTITLE==FFmpeg_Codec_Type(formatCtx_->streams[subtitleStreamIndex_]));
                formatCtx_->streams[subtitleStreamIndex_]->discard = AVDISCARD_ALL;
            }

            if (subtitleStreamIndexSwitch_<0 || subtitleStreamIndexSwitch_>=total_streams) {
                if (subtitleStreamIndexSwitch_>=total_streams) {
                    int const SRT_stream_index = subtitleStreamIndexSwitch_ - total_streams;
                    if (subtitleUtil_.OnChangeExternalSubtitle(SRT_stream_index)) {
#ifdef BL_AVDECODER_VERBOSE
                        if (subtitleStreamIndex_<total_streams) {
                            if (0<=subtitleStreamIndex_) {
                                AVDictionaryEntry* t = av_dict_get(formatCtx_->streams[subtitleStreamIndex_]->metadata, "language", NULL, 0);
                                BL_LOG("** subtitle stream : embeded subtitle#%d(%s) to external subtitle#%d\n",
                                    subtitleStreamIndex_, (NULL!=t) ? t->value:"undefined", SRT_stream_index);
                            }
                            else {
                                BL_LOG("** subtitle stream : Play external subtitle#%d\n", SRT_stream_index);
                            }
                        }
                        else {
                            BL_LOG("** subtitle stream : external subtitle#%d to external subtitle#%d\n",
                                subtitleStreamIndex_-total_streams, SRT_stream_index);
                        }
#endif
                        subtitleStreamIndex_ = subtitleStreamIndexSwitch_;
                    }
                    else {
                        BL_ERR("** subtitle stream : failed to change to SRT stream %d\n", SRT_stream_index);
                        subtitleStreamIndexSwitch_ = subtitleStreamIndex_ = -1;
                    }
                }
                else {
                    subtitleStreamIndex_ = subtitleStreamIndexSwitch_ = -1;
                }
            }
            else if (AVMEDIA_TYPE_SUBTITLE==FFmpeg_Codec_Type(formatCtx_->streams[subtitleStreamIndexSwitch_])) {
                ISO_639 lan = ISO_639_UNKNOWN;
                AVDictionaryEntry* t = av_dict_get(formatCtx_->streams[subtitleStreamIndexSwitch_]->metadata, "language", NULL, 0);
                if (NULL!=t) {
                    lan = Translate_ISO_639(t->value);
                }
#ifdef BL_AVDECODER_VERBOSE
                if (subtitleStreamIndex_<total_streams) {
                    BL_LOG("** subtitle stream : embeded subtitle#%d to embeded subtitle#%d(%s)\n",
                        subtitleStreamIndex_, subtitleStreamIndexSwitch_,
                        (NULL!=t) ? t->value:"undefined");
                }
                else {
                    BL_LOG("** subtitle stream : external subtitle#%d to embeded subtitle#%d(%s)\n",
                        subtitleStreamIndex_-total_streams, subtitleStreamIndexSwitch_,
                        (NULL!=t) ? t->value:"undefined");
                }
#endif
                subtitleStreamIndex_ = subtitleStreamIndexSwitch_;
                formatCtx_->streams[subtitleStreamIndex_]->discard = AVDISCARD_DEFAULT; // or AVDISCARD_NONE ?
            }
            else { // are you joking?
                BL_ERR("** subtitle stream : %d to %d(invalid)\n", subtitleStreamIndex_, subtitleStreamIndexSwitch_);
                subtitleStreamIndexSwitch_ = subtitleStreamIndex_;
            }
        }

        // audio stream changed
        if (audio_stream && 0==audioBufferFlushing_ && audioStreamIndexSwitch_!=audioStreamIndex_ &&
            -1==audioExtStreamIndex1_ && 0<=audioStreamIndexSwitch_ && audioStreamIndexSwitch_<total_streams &&
            AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[audioStreamIndexSwitch_])) {
            assert(AUDIO_TECHNIQUE_DEFAULT==audioInfo_.Technique);
            if (0<=audioStreamIndex_ && audioStreamIndex_<total_streams) {
                assert(AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[audioStreamIndex_]));
                formatCtx_->streams[audioStreamIndex_]->discard = AVDISCARD_ALL;
            }

            AVStream* stream = formatCtx_->streams[audioStreamIndexSwitch_];
            stream->discard = AVDISCARD_DEFAULT;

#ifdef BL_AVDECODER_VERBOSE
            AVDictionaryEntry* t = av_dict_get(stream->metadata, "language", NULL, 0);
            BL_LOG("** audio : changing stream #%d to #%d(%s %dch fmt:%d freq:%d)\n",
                   audioStreamIndex_, audioStreamIndexSwitch_, (NULL!=t) ? t->value:"undefined",
                   stream->codecpar->channels, stream->codecpar->format, stream->codecpar->sample_rate);
#endif
            audio_packet_duration_good = av_rescale_q(buffering_duration_ms*1000, timebase_q, stream->time_base);
            audioBufferFlushing_ = 1; // audio must sync
            audioStreamIndex_ = audioStreamIndexSwitch_;

            // update current index - but keep audioInfo_.StreamChannels[0]
            assert(AUDIO_TECHNIQUE_DEFAULT==audioInfo_.Technique);
            assert(1==audioInfo_.TotalStreams);
            assert(audioInfo_.TotalChannels==audioInfo_.StreamChannels[0]);
            audioInfo_.StreamIndices[0] = (uint8) audioStreamIndex_;
        }

#ifdef DEBUG_PACKET_READING_BENCHMARK
        t0 = mlabs::balai::system::GetTime();
#endif
        if (NULL==pk) {
            pk = GetPacketList_();
            if (NULL==pk && 0==timeStartSysTime_ && 0==videoQueue_.Size()) {
                // if audio queue take all packets, it will never start!
                pk = audioQueue_.Get();
                if (NULL!=pk) {
                    av_packet_unref(&(pk->pkt));
                }
            }
        }

        // take a nap, if we've got enough.
        if (NULL==pk ||
            (
             (!video_stream || (videoQueue_.Duration()>video_packet_duration_good) ||
               videoQueue_.Size()>video_packet_count_full)
             &&
             (!audio_stream || (audioQueue_.Duration()>audio_packet_duration_good) ||
               (audioBufferPut_-audioBufferGet_)>audio_buffer_size_good)
            )
            ) {
            //
            // note : original implemented using condition_variable::wait_for(), but
            // a condition_variable::wait_for() crash report persisted in vs2012
            // https://connect.microsoft.com/VisualStudio/feedback/details/762560
            // (fixed in vs2013)
            //
            //  unique_lock<mutex> lock(videoMutex_);
            //  videoCV_.wait_for(lock, std::chrono::microseconds(short_nap));
            //
            av_usleep(1000);

#ifdef DEBUG_PACKET_READING_BENCHMARK
            sleep_time += (mlabs::balai::system::GetTime() - t0);
#endif
            continue;
        }

        //
        // read packet. For video, the packet contains exactly one frame. For audio,
        // it contains an integer number of frames if each frame has a known fixed size
        // (e.g. PCM or ADPCM data). If the audio frames have a variable size
        // (e.g. MPEG audio), then it contains one frame. packet.pts, packet.dts and
        // packet.duration are always set to correct values in AVStream.time_base units
        // (and guessed if the format cannot provide them).
        // packet.pts can be AV_NOPTS_VALUE if the video format has B-frames, so it is
        // better to rely on packet.dts if you do not decompress the payload.
        //
        AVPacket& packet = pk->pkt;
        int const error = av_read_frame(formatCtx_, &packet);
        if (0!=error) { // error occurs or finish packet reading
#ifdef BL_AVDECODER_VERBOSE
            char msg_buf[256];
            av_strerror(error, msg_buf, sizeof(msg_buf));
            BL_ERR("** av_read_frame() return 0x%x - %s\n", error, msg_buf);
#endif
            // late start?
            if (0==timeStartSysTime_) {
                // discard audio/subtitle.
                FlushQueue_(subtitleQueue_);
                FlushQueue_(audioQueue_);
                FlushQueue_(audioQueueExt1_);
                FlushQueue_(audioQueueExt2_);
                FlushQueue_(audioQueueExt3_);
                audioBufferPut_ = audioBufferGet_ = 0;

                // take a nap if it's not available soon.
                if (0==videoQueue_.Size()) {
                    av_usleep(liveStream_ ? 2000000:500000);
                    status_ = STATUS_ENDED;
                }
                else {
                    int64_t const time_out = av_gettime_relative() + 2000000;
                    while (0==videoFramePut_ && videoQueue_.Size() && av_gettime_relative()<time_out) {
                        av_usleep(1000);
                    }

                    if (videoFramePut_>0) {
                        if (NULL!=host_) {
                            host_->OnStart(id_, (int)((decodedFrames_[0].PTS-startTime_)/1000),
                                                (int)((decodedFrames_[(videoFramePut_-1)%NUM_VIDEO_BUFFER_FRAMES].PTS-startTime_)/1000), true, false);
                        }
                        timeAudioCrossfade_ = timeLastUpdateFrameSysTime_ = av_gettime();
                        timeStartSysTime_ = timeAudioCrossfade_ - decodedFrames_[0].PTS;
                    }
                    else {
                        FlushQueue_(videoQueue_);
                        status_ = STATUS_ENDED;
                    }
                }

#ifdef BL_AVDECODER_VERBOSE
                if (STATUS_ENDED==status_) {
                    BL_LOG("** failed to start!\n");
                }
                else {
                    BL_LOG("** \"late\" start!\n");
                }
#endif
            }

            endOfStream_ = (AVERROR_EOF==error) ? 1:0xff;
            if (host_) {
                host_->OnStreamEnded(id_, (AVERROR_EOF==error) ? 0:error);
            }
            break;
        }

#ifdef DEBUG_PACKET_READING_BENCHMARK
        t0 = mlabs::balai::system::GetTime() - t0;
        if (packet.stream_index<(sizeof(stream_packets)/sizeof(stream_packets[0])))
            stream_packets[packet.stream_index] += 1;
#endif
        if (packet.stream_index==videoStreamIndex_) {
            videoQueue_.Put(pk); pk = NULL;
#ifdef DEBUG_PACKET_READING_BENCHMARK
            ++video_packets;
            video_data_size += packet.size;
            video_time += t0;
            if (video_time_hi<t0)
                video_time_hi = t0;
#endif 
        }
        else if (packet.stream_index==subtitleStreamIndex_) {
            subtitleQueue_.Put(pk); pk = NULL;
        }
        else if (packet.stream_index==audioStreamIndex_) {
            audioQueue_.Put(pk); pk = NULL;
#ifdef DEBUG_PACKET_READING_BENCHMARK
            ++audio_packets;
            audio_data_size += packet.size;
            audio_time += t0;
            if (audio_time_hi<t0)
                audio_time_hi = t0;
#endif
        }
        else if (packet.stream_index==audioExtStreamIndex1_) {
            audioQueueExt1_.Put(pk); pk = NULL;
#ifdef DEBUG_PACKET_READING_BENCHMARK
            ++audio_packets;
            audio_data_size += packet.size;
            audio_time += t0;
            if (audio_time_hi<t0)
                audio_time_hi = t0;
#endif
        }
        else if (packet.stream_index==audioExtStreamIndex2_) {
            audioQueueExt2_.Put(pk); pk = NULL;
#ifdef DEBUG_PACKET_READING_BENCHMARK
            ++audio_packets;
            audio_data_size += packet.size;
            audio_time += t0;
            if (audio_time_hi<t0)
                audio_time_hi = t0;
#endif
        }
        else if (packet.stream_index==audioExtStreamIndex3_) {
            audioQueueExt3_.Put(pk); pk = NULL;
#ifdef DEBUG_PACKET_READING_BENCHMARK
            ++audio_packets;
            audio_data_size += packet.size;
            audio_time += t0;
            if (audio_time_hi<t0)
                audio_time_hi = t0;
#endif
        }
        else {
#ifdef DEBUG_PACKET_READING_BENCHMARK
            ++other_packets;
            other_data_size += packet.size;
            other_time += t0;
#endif
            av_packet_unref(&packet);
        }

#ifdef DEBUG_PACKET_READING_BENCHMARK
        int const packet_in_used = videoQueue_.Size() + subtitleQueue_.Size() +
                                   audioQueue_.Size() + audioQueueExt1_.Size() +
                                   audioQueueExt2_.Size() + audioQueueExt3_.Size();
        if (total_packets_hi<packet_in_used) {
            total_packets_hi = packet_in_used;
        }

        t0 = mlabs::balai::system::GetTime();
        if (t0>(last_print+1.0)) {
            t0 -= last_print;
            if (other_packets>0) {
                BL_LOG("[decode main] idle:%.1f%% + video:%.1f%% + audio:%.1f%% + other:%.1f%%  max_pkts:%d",
                    100.0f*sleep_time/t0, 100.0f*video_time/t0, 100.0f*audio_time/t0, 100.0f*other_time/t0,
                    total_packets_hi);
            }
            else {
                BL_LOG("[decode main] idle:%.1f%% + video:%.1f%% + audio:%.1f%%  max_pkts:%d",
                    100.0f*sleep_time/t0, 100.0f*video_time/t0, 100.0f*audio_time/t0, total_packets_hi);
            }

            BL_LOG("  video(%.1fMbps):%.1f/s %.1fms  audio(%.1fkbps):%.1f/s %.1fms",
                   double(video_data_size>>7)/(1024.0*t0), video_packets/t0, 1000.0f*video_time_hi,
                   double(audio_data_size>>7)/t0, audio_packets/t0, 1000.0f*audio_time_hi);

            if (other_packets>0) {
                BL_LOG("  other(%.1fkbps):%.1f/s\n               packet statistics :",
                       double(other_data_size>>7)/t0, other_packets/t0);
                for (int i=0; i<total_streams && i<sizeof(stream_packets)/sizeof(stream_packets[0]) ; ++i) {
                    BL_LOG(" %d", stream_packets[i]);
                }
            }
            BL_LOG("\n");

            video_packets = audio_packets = other_packets = total_packets_hi = 0;
            audio_data_size = video_data_size = other_data_size = 0;
            video_time = audio_time = other_time = sleep_time = 0.0;
            video_time_hi = audio_time_hi = 0.0;
            last_print = mlabs::balai::system::GetTime();
            memset(stream_packets, 0, sizeof(stream_packets));
        }
#endif
    }

    // join
    if (videoThread.joinable()) {
        videoThread.join();
    }
    if (audiosubtitleThread.joinable()) {
        audiosubtitleThread.join();
    }

    if (NULL!=pk) {
        ReleaseAVPacketList_(pk);
        pk = NULL;
    }
}
//---------------------------------------------------------------------------------------
void AVDecoder::SeekThread_()
{
    assert(duration_>0 && formatCtx_ && 0<=videoStreamIndex_ && videoStreamIndex_<(int)formatCtx_->nb_streams);
    AVPacket packet; av_init_packet(&packet);
    AVStream* stream = formatCtx_->streams[videoStreamIndex_];
    AVRational const& stream_time_base = stream->time_base;
    AVRational const& frame_rate = stream->avg_frame_rate;
    int64_t frame_pts(AV_NOPTS_VALUE), frame_duration(AV_NOPTS_VALUE);
    int seek_duration = (int)(duration_/1000);
    int seek_timestamp = -1000;
    bool reset_bad_duration = true;

    videoDecoder_.Init(stream, videoWidth_, videoHeight_, videoDecoderPreference_);
    videoDecoder_.Resume();

    while (STATUS_SEEKING==status_) {
        if (seek_timestamp==videoSeekTimestamp_) {
            unique_lock<mutex> lock(videoMutex_);
            while (STATUS_SEEKING==status_ && seek_timestamp==videoSeekTimestamp_) {
                videoCV_.wait(lock);
            }
        }

        // clamp range
        seek_timestamp = videoSeekTimestamp_;
        if (seek_timestamp>seek_duration) {
            seek_timestamp = seek_duration;
        }
        else if (seek_timestamp<0) {
            seek_timestamp = 0;
        }
        videoSeekTimestamp_ = seek_timestamp;

        // move video position
        int64_t seek_pts = int64_t(seek_timestamp)*1000;
        if (!FFmpegSeekWrapper(formatCtx_, seek_timestamp)) {
            int64_t const max_seek_pts = seek_pts;
            seek_pts = max_seek_pts*95/100;
            if (0>avformat_seek_file(formatCtx_, -1, 0, seek_pts, max_seek_pts, 0)) {
                videoSeekTimestamp_ = seek_timestamp*90/100;
#ifdef BL_AVDECODER_VERBOSE
                BL_LOG("VideoSeekThread_() seek failed, timestamp changed %d -> %dms\n",
                       seek_timestamp, (int)videoSeekTimestamp_);
#endif
                seek_timestamp = -1000;
                continue; // failed
            }
        }
        FlushBuffers_();

        // read just one frame from current seeking position
        int64_t last_packet_end_pts = -1000000;
        int got_frame = 0;
        while (STATUS_SEEKING==status_ && 0==got_frame) {
            int const error = av_read_frame(formatCtx_, &packet);
            if (0!=error) {
                if (AVERROR_EOF==error) { // hit the end.
                    if (last_packet_end_pts>startTime_ && reset_bad_duration) {
                        if (last_packet_end_pts<(duration_+startTime_)) {
#ifdef BL_AVDECODER_VERBOSE
                            BL_ERR("VideoSeeking_() hit the end, duration changed %lld -> %lldms\n",
                                   duration_/1000, (last_packet_end_pts-startTime_)/1000);
#endif
                            duration_ = last_packet_end_pts - startTime_;
                            seek_duration = (int)(duration_/1000);
                            seek_timestamp = seek_duration - 5000;
                        }
                        reset_bad_duration = false;
                    }
                    videoSeekTimestamp_ = seek_timestamp*95/100;
                    seek_timestamp = -1000;
                }
                break;
            }

            // if packet.buf is NULL, then the packet is valid until the next av_read_frame() or until avformat_close_input().
            if (packet.stream_index==videoStreamIndex_) {
                if (videoDecoder_.send_packet(packet) &&
                    videoDecoder_.can_receive_frame(frame_pts, frame_duration)) {
                    
                    

                    // timestamp
                    if (AV_NOPTS_VALUE!=frame_pts) {
                        videoPutPTS_ = av_rescale_q(frame_pts, stream_time_base, timebase_q);
                    }
                    else if (AV_NOPTS_VALUE!=packet.pts) {
                        videoPutPTS_ = av_rescale_q(packet.pts, stream_time_base, timebase_q);
                    }
                    else if (AV_NOPTS_VALUE!=packet.dts) {
                        videoPutPTS_ = av_rescale_q(packet.dts, stream_time_base, timebase_q);
                    }
                    else {
                        BL_ERR("VideoSeeking_() - no trustful frame pts\n");
                        videoPutPTS_ = seek_pts;
                    }

                    int const slot = videoFramePut_%NUM_VIDEO_BUFFER_FRAMES;
                    VideoFrame& frame = decodedFrames_[slot];
                    frame.FramePtr   = (int64_t) (videoBuffer_+slot*videoFrameDataSize_);
                    frame.PTS        = videoPutPTS_;
                    frame.Type       = VideoFrame::NV12;
                    frame.ColorSpace = VideoFrame::BT_709;
                    frame.ColorRange = VideoFrame::RANGE_MPEG;
                    frame.Width      = videoWidth_;
                    frame.Height     = videoHeight_;
                    frame.ID         = videoFramePut_;
                    videoDecoder_.receive_frame(frame);
                    ++videoFramePut_;

                    // estimate next frame's pts
                    if (AV_NOPTS_VALUE!=frame_duration) {
                        videoPutPTS_ += av_rescale_q(frame_duration, stream_time_base, timebase_q);
                    }
                    else {
                        videoPutPTS_ += av_rescale(AV_TIME_BASE, frame_rate.den, frame_rate.num);
                    }

                    // do not break here, still have packet to be freed.
                    got_frame = 1; // will break this while loop.
                }
            }
            else if (reset_bad_duration) {
                int64_t pts = (AV_NOPTS_VALUE!=packet.pts) ? packet.pts:packet.dts;
                if (AV_NOPTS_VALUE!=pts) {
                    pts = av_rescale_q(pts, formatCtx_->streams[packet.stream_index]->time_base, timebase_q);

#ifdef BL_AVDECODER_VERBOSE
                    // few hundred millisecond to few minutes
                    //if (last_packet_end_pts<0) {
                    //    BL_LOG("VideoSeeking_() seek pts different = %lldms\n", (seek_pts-pts)/1000);
                    //}
#endif
                    if (AV_NOPTS_VALUE!=packet.duration) {
                        pts += av_rescale_q(packet.duration, formatCtx_->streams[packet.stream_index]->time_base, timebase_q);
                    }

                    if (last_packet_end_pts<pts) {
                        last_packet_end_pts = pts;
                    }
                }
            }

            // free the packet that was allocated by av_read_frame
            av_packet_unref(&packet);
        }
    }

    videoDecoder_.Pause();
}
//---------------------------------------------------------------------------------------
int AVDecoder::Timestamp() const
{
    if (STATUS_READY<=status_) {
        assert(NULL!=formatCtx_);
        if (timeStartSysTime_>0 && STATUS_READY==STATUS_PLAYING) {
            int64_t const t = av_gettime() - timeStartSysTime_ - startTime_;
            if (0<t && t<=duration_)
                return (int)(t/1000);
        }
        return VideoTime();
    }
    return -1;
}
//---------------------------------------------------------------------------------------
float AVDecoder::PlaybackProgress() const
{
    if (STATUS_READY<=status_) {
        assert(NULL!=formatCtx_);
        int64_t ts = 0;
        if (timeStartSysTime_>0 && STATUS_READY==STATUS_PLAYING) {
            ts = av_gettime() - timeStartSysTime_ - startTime_;
        }
        else {
            ts = ((0<=videoStreamIndex_) ? videoGetPTS_:audioGetPTS_) - startTime_;
        }

        if (ts>0) {
            return (ts<duration_) ? (1.e-6f*av_rescale(ts, AV_TIME_BASE, duration_)):1.0f;
        }
    }
    return 0.0f;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::UpdateFrame()
{
    int64_t const cur_time = av_gettime();
    if (STATUS_PLAYING==status_) {
        if (timeStartSysTime_>0) {
            int64_t const elapsed_time = cur_time - timeLastUpdateFrameSysTime_;
            if (elapsed_time>AV_TIME_BASE && videoFrameGet_>0) {
                // realign timestamp if abnormal delay > 1 second(AV_TIME_BASE=1sec)
                int64_t const time_zero1 = timeStartSysTime_ + elapsed_time;

                // in case elapsed_time is a very huge number! e.g. timeLastFrameUpdated_=0
                int64_t const time_zero2 = cur_time - videoGetPTS_;
                timeStartSysTime_ = (time_zero1<time_zero2) ? time_zero1:time_zero2;
                timeAudioCrossfade_ = cur_time;

#ifdef BL_AVDECODER_VERBOSE
                BL_LOG("** UpdateFrame() called delay for %llums. reset zero time(diff=%llums)\n",
                       elapsed_time/1000, abs(time_zero1-time_zero2)/1000);
#endif
            }

            // update video frame
            bool video_update = false;
            if (videoFrameGet_<videoFramePut_) {
                //
                // it need to consume more video frames, if video fps > app fps.
                // e.g. Ang Lee's 120 fps 3D movie, "Billy Lynn's Long Halftime Walk".
                //

                //
                // FIX-ME!!! this value should be respect to application framerate...
                //  45 fps = 22222/2 = 11111
                //  60 fps = 16666/2 =  8333
                //  90 fps = 11111/2 =  5555
                // 120 fps =  8333/2 =  4166
                int64_t const app_half_frametime = 5000;
                int64_t const cur_pts = cur_time - timeStartSysTime_;
                int frameId = videoFrameGet_%NUM_VIDEO_BUFFER_FRAMES;
                if (decodedFrames_[frameId].PTS<(cur_pts+app_half_frametime)) {
                    video_update = true;
                    int next_get = videoFrameGet_ + 1;
                    while (next_get<videoFramePut_ && /*0==transcodeMode_ &&*/
                           cur_pts>=decodedFrames_[next_get%NUM_VIDEO_BUFFER_FRAMES].PTS) {
                        videoDecoder_.finish_frame(decodedFrames_[frameId]);
                        frameId = (next_get++)%NUM_VIDEO_BUFFER_FRAMES;
                    }

#ifdef BL_AVDECODER_VERBOSE
                    if ((next_get-(int)videoFrameGet_)>1) {
//                        BL_LOG("** UpdateFrame() discard %d overdue frame(s)\n", next_get-(int)videoFrameGet_-1);
                    }
#endif
                    if (NULL!=host_) {
                        host_->FrameUpdate(id_, decodedFrames_[frameId]);
                    }
                    videoGetPTS_ = decodedFrames_[frameId].PTS; // + video frame duration?

                    // finish this frame
                    videoDecoder_.finish_frame(decodedFrames_[frameId]);

                    // only now you can move on videoFrameGet_
                    videoFrameGet_ = next_get;
                    //videoCV_.notify_one(); // needed if MainThread_() uses videoCV_.wait_for()
                }
            }
            else if (endOfStream_ && 0==videoQueue_.Size() && 0==audioQueue_.Size() &&
                     audioBufferPut_<=audioBufferGet_) {
                status_ = STATUS_ENDED;
            }
#if 0
            // this waring is just a co-issue of dropping frames. ref VideoThread_()
            else if (0<videoFramePut_) {
                static int last_id = -1;
                if (last_id!=(int)videoFrameGet_) {
                    last_id = (int)videoFrameGet_;
                    BL_LOG("** UpdateFrame() - late frame #%d\n", last_id + 1);
                }
            }
#endif
            // Subtitle
            if (subtitleGet_<subtitlePut_) {
                int64_t const subtitle_time = timeStartSysTime_ + subtitlePTS_;
                if (subtitle_time<=cur_time || (!video_update && (subtitle_time<=cur_time+33000))) {
                    if (NULL!=host_ && cur_time<(subtitle_time+int64_t(1000)*subtitleDuration_) ) {
                        host_->SubtitleUpdate(id_,
                                              subtitleBuffer_, subtitleWidth_, subtitleHeight_,
                                              subtitleGraphicsFmt_,
                                              (int) ((subtitlePTS_-startTime_)/1000), subtitleDuration_, subtitlePut_,
                                              subtitleRects_, subtitleRectCount_);
                    }
                    ++subtitleGet_;
                }
            }
        }
    }
    else if (STATUS_SEEKING==status_) {
        if (videoFrameGet_<videoFramePut_) {
            VideoFrame& frame = decodedFrames_[videoFrameGet_%NUM_VIDEO_BUFFER_FRAMES];
            if (NULL!=host_) {
                host_->FrameUpdate(id_, frame);
            }
            videoGetPTS_ = frame.PTS;
            videoDecoder_.finish_frame(frame);
            ++videoFrameGet_;
        }
    }
    else if (STATUS_ENDED==status_) {
        if (timeStartSysTime_>0) { // 2017/07/28 check timeStartSysTime_, it may fail to start
            int64_t const end_time = cur_time - timeStartSysTime_ - startTime_;
            if (NULL!=host_) { // hit the end
                int const dur = (int)(duration_/1000);
                int const end = (end_time<duration_) ? (int)(end_time/1000):dur;
                host_->OnStopped(id_, end, dur, 1==endOfStream_);
            }

            if (1==endOfStream_ && 0==liveStream_ && 1000000<end_time && (end_time+5000000)<duration_) {
                ////////////////////////////////////////////////////
                // duration is wrong (error>5 secs)!? this may due
                // to partial download...
                // really fix this!?
                if (duration_>0) {
                    int const dur = (int)(duration_/1000);
                    int const ms = dur%1000;
                    int ss = dur/1000;
                    int mm = ss/60; ss%=60;
                    int hh = mm/60; mm%=60;

                    int const end = (int)(end_time/1000);
                    int const ms2 = end%1000;
                    int ss2 = end/1000;
                    int mm2 = ss2/60; ss2%=60;
                    int hh2 = mm2/60; mm2%=60;
                    BL_LOG("fix duration %02d:%02d:%02d:%03d -> %02d:%02d:%02d:%03d\n", hh, mm, ss, ms, hh2, mm2, ss2, ms2);
                    duration_ = end_time;
                }
            }
        }

        interruptRequest_ = 1;
        if (decodeThread_.joinable()) { // cannot be too cautious
            videoCV_.notify_all();
            decodeThread_.join();
        }

        status_ = STATUS_READY;
        videoFramePut_ = videoFrameGet_ = 0;
        subtitlePut_ = subtitleGet_ = 0;
        audioBufferPut_ = audioBufferGet_ = 0;

        // no reason to stay in the end, so rewind.
        if (duration_>0) {
            FFmpegSeekWrapper(formatCtx_, 0);
        }
        FlushBuffers_();

        subtitleDuration_ = 0;
        videoPutPTS_ = videoGetPTS_ = audioPutPTS_ = subtitlePTS_ = startTime_;
        timeInterruptSysTime_ = 0;
        endOfStream_ = interruptRequest_ = 0;

        // notify host the file is done
        if (NULL!=host_) {
            host_->OnReset(id_);
        }
    }

    timeLastUpdateFrameSysTime_ = cur_time;

    return true;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::StartVideoSeeking(int timestamp)
{
    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (videoStreamIndex_>=0 && duration_>0 &&
        (STATUS_READY==status_ || STATUS_PLAYING==status_ || STATUS_STOP==status_)) {
        if (STATUS_PLAYING==status_) {
            if (NULL!=host_) {
                host_->OnStopped(id_,
                    (int)((((videoGetPTS_<audioGetPTS_) ? audioGetPTS_:videoGetPTS_)-startTime_)/1000),
                    (int)(duration_/1000), false);
            }
        }

        if (decodeThread_.joinable()) {
            status_ = STATUS_STOP;
            interruptRequest_ = 1;
            videoCV_.notify_all();
            decodeThread_.join();
        }
        status_ = STATUS_SEEKING;

        videoFrameGet_ = videoFramePut_ = 0;
        subtitlePut_ = subtitleGet_ = 0;
        audioBufferPut_ = audioBufferGet_ = 0;
        interruptRequest_ = 0;

        // flush queues and incomplete frames
        FlushQueue_(videoQueue_);
        FlushQueue_(subtitleQueue_);
        FlushQueue_(audioQueue_);
        FlushQueue_(audioQueueExt1_);
        FlushQueue_(audioQueueExt2_);
        FlushQueue_(audioQueueExt3_);

        if (timestamp>0) {
            int const d = int(duration_/1000) - 5000;
            videoSeekTimestamp_ = (timestamp<d) ? timestamp:d;
        }
        else {
            videoSeekTimestamp_ = 0;
        }

        decodeThread_ = move(thread([this] { SeekThread_(); }));
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::Play()
{
    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (STATUS_READY==status_ || STATUS_STOP==status_) {
        // this will lost current loaded data.
        videoFramePut_ = videoFrameGet_ = 0;
        audioBufferPut_ = audioBufferGet_ = 0;
        audioPutPTS_ = subtitlePTS_ = audioGetPTS_ = 0;
        subtitlePut_ = subtitleGet_ = 0;
        subtitleWidth_ = subtitleHeight_ = subtitleDuration_ = subtitleRectCount_ = 0;
        subtitleBufferFlushing_ = audioBufferFlushing_ = endOfStream_ = 0;

        if (duration_>0) {
            if (STATUS_STOP==status_ && (videoGetPTS_+AV_TIME_BASE)>duration_) {
                FFmpegSeekWrapper(formatCtx_, 0);
                FlushBuffers_();
                videoPutPTS_ = videoGetPTS_ = 0;
            }
        }
        else {
            FlushBuffers_();
        }

        timeStartSysTime_ = timeAudioCrossfade_ = timeInterruptSysTime_ = 0;
        status_ = STATUS_PLAYING;

        //
        // run, thread, run! syntax:
        //std::thread localthread([this]() { MainThread_(); });
        //std::thread localthread([this] { MainThread_(); }); // argument list () can be omitted
        //std::thread localthread([this]() mutable { MainThread_();} ); // no need this mutable, the closure object is still const.
        //std::thread localthread([this] mutable { MainThread_(); }); // won't compile
        decodeThread_ = move(thread([this] { MainThread_(); })); 

        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::PlayAt(int time)
{
    /////////////////////////////////////////////////////////////////////////////////////
#ifdef TEST_AMF_FAIL_CASE
    time = debug_AMF_my_brothers_keeper_time_at;
#endif
    /////////////////////////////////////////////////////////////////////////////////////

    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (NULL!=formatCtx_ && 0<=time &&
        (STATUS_READY==status_ || STATUS_PLAYING==status_ || STATUS_STOP==status_ ||
         STATUS_SEEKING==status_)) {
        if (STATUS_PLAYING==status_ || STATUS_SEEKING==status_) {
            if (NULL!=host_ && STATUS_PLAYING==status_) {
                host_->OnStopped(id_,
                    (int)((((videoGetPTS_<audioGetPTS_) ? audioGetPTS_:videoGetPTS_)-startTime_)/1000),
                    (int)(duration_/1000), false);
            }

            status_ = STATUS_STOP; // need this to join
            if (decodeThread_.joinable()) {
                videoCV_.notify_all();
                interruptRequest_ = 1;
                decodeThread_.join();
                interruptRequest_ = 0;
            }
        }

        // duration_<0 is a live streaming video
        if (duration_>0) {
            videoPutPTS_ = 0;

            if (time<0)
                time = 0;
            if (FFmpegSeekWrapper(formatCtx_, time)) {
                videoPutPTS_ = int64_t(time)*1000;
            }
            else {
                if ((int64_t(time)+5000000)>=duration_) {
                    time = (int)((duration_-5000000)/1000);
                    if (time<0)
                        time = 0;
                }
                if (FFmpegSeekWrapper(formatCtx_, time))
                    videoPutPTS_ = int64_t(time)*1000;
            }
            videoGetPTS_ = videoPutPTS_;

            // flush buffers and queues, clear unfinished frames
            FlushBuffers_();
        }

        status_ = STATUS_PLAYING;
        videoFramePut_ = videoFrameGet_ = 0;
        audioBufferPut_ = audioBufferGet_ = 0;
        audioPutPTS_ = audioGetPTS_ = 0;
        subtitlePTS_ = 0;
        subtitlePut_ = subtitleGet_ = 0;
        subtitleWidth_ = subtitleHeight_ = subtitleRectCount_ = subtitleDuration_ = 0;
        subtitleBufferFlushing_ = audioBufferFlushing_ = endOfStream_ = interruptRequest_ = 0;
        timeAudioCrossfade_ = timeStartSysTime_ = timeInterruptSysTime_ = 0;
        decodeThread_ = std::move(std::thread([this] { MainThread_(); }));

        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool AVDecoder::Stop()
{
    std::lock_guard<std::mutex> lock(asyncPlayMutex_);
    if (STATUS_PLAYING==status_) {
        if (NULL!=host_) {
            host_->OnStopped(id_,
                (int)((((videoGetPTS_<audioGetPTS_) ? audioGetPTS_:videoGetPTS_)-startTime_)/1000),
                (int)(duration_/1000), false);
        }

        status_ = STATUS_STOP;
        if (decodeThread_.joinable()) {
            videoCV_.notify_all();
            interruptRequest_ = 1;
            decodeThread_.join();
            interruptRequest_ = 0;
        }

        //
        // do NOT flush queues/unref frames here... we need to continue!
        //

        return true;
    }
    else if (STATUS_SEEKING==status_) {
        timeInterruptSysTime_ = 0;
        status_ = STATUS_STOP;
        if (decodeThread_.joinable()) {
            videoCV_.notify_all();
            interruptRequest_ = 1;
            decodeThread_.join();
            interruptRequest_ = 0;
        }

        return true;
    }

    return false;
}

}}} // namespace mlabs::balai::video