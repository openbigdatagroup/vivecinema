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
 * @file    AVDecoder.h
 * @author  andre chen
 * @history 2015/12/09 created
 *          2016/10/31 testing FFmpeg 3.1.5
 *          2017/07/07 migrate to FFmpeg 3.3.2 for better H.265 support
 *          2017/08/07 AVHWAccel, Hardware Accelerated decoder and AMD AMF SDK testing...
 *          2017/09/12 NVDEC + AMD AMF (AMD is still fixing HEVC issues 2017/09/22)
 *
 * @To upgrade, download dev and shared packages from https://ffmpeg.zeranoe.com/builds/
 *
 */
#ifndef AV_DECODER_H
#define AV_DECODER_H

#include "Subtitle.h"
#include "Audio.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"   // SwsContext
#include "libavutil/stereo3d.h"   // AVStereo3D(side data)
#include "libavutil/hwcontext.h"  // AVHWDeviceType, av_hwframe_transfer_data();
}

#include <condition_variable>
#include <thread>
#include <atomic>

using namespace mlabs::balai::audio;

// FFmpeg helpers
AVMediaType     FFmpeg_Codec_Type(AVStream const* s);
AVCodecContext* FFmpeg_AVCodecOpen(AVStream const* stream);
bool FFmpegSeekWrapper(AVFormatContext* formatCtx, int time, int streamId=-1); // time in milliseconds

// audio resample, interleaved to interleaved, return number of samples output per channel
int FFmpegAudioResample(uint8* dst, int max_size, int dstSampleRate, AUDIO_FORMAT dstFmt,
                        uint8 const* src, int srcSampleRate, AUDIO_FORMAT srcFmt, int channels, int samples);

namespace mlabs { namespace balai { namespace video {

// video type
enum VIDEO_3D {
    VIDEO_3D_MONO,      // MONO
    VIDEO_3D_TOPBOTTOM = 1, // top-bottom.
    VIDEO_3D_LEFTRIGHT = 2, // side by side
    VIDEO_3D_BOTTOMTOP = 3, // Matroska::StereoMode = 2
    VIDEO_3D_RIGHTLEFT = 4, // Matroska::StereoMode = 11
};

// video decoder context
// As you may experience, to open and close video context for every playback may
// lose couple of video frames. in serious case, > 10+ seconds.
//
// Note this helper class is made for VideoThread_() and SeekThread_(). callers must be cautious 
//
class VideoDecoderImp;

class VideoDecoder {
    AVFrame          frame_;
    AVStream const*  stream_;
    VideoDecoderImp* pImp_;
    AVCodecContext*  avctx_;

    /////////////////////////////////////////////////////////////////////////////////////
    // Be noted sws_scale() changes color, not just memmove. It can easily eat up 50ms.
    // So, do not use sws_scale() unless necessary!
    SwsContext*     swsctx_;
    /////////////////////////////////////////////////////////////////////////////////////

    AVHWDeviceType  avctx_hwaccel_;
    int             packet_cnt_;
    int             frame_cnt_;
    uint32          flags_; // available decoder mask

    // AVCodecContext.get_format callback
    static AVPixelFormat s_get_format_(AVCodecContext* avctx, AVPixelFormat const* pix_fmts);

public:
    VideoDecoder():stream_(NULL),pImp_(NULL),avctx_(NULL),swsctx_(NULL),
        avctx_hwaccel_(AV_HWDEVICE_TYPE_NONE),
        packet_cnt_(0),frame_cnt_(0),flags_(0) {
        memset(&frame_, 0, sizeof(frame_));
        av_frame_unref(&frame_); // a must, frame_.format = AV_PIX_FMT_NONE(-1)
    }
    ~VideoDecoder() { Clear(); }

    uint32 Flags() const { return flags_; };
    int IsHardwareVideoDecoder() const {
        return (NULL!=pImp_) ? 2:((AV_HWDEVICE_TYPE_NONE!=avctx_hwaccel_) ? 1:0);
    }
    char const* Name() const;

    bool Init(AVStream* stream, int w, int h, uint8& options);
    void Clear();

    bool Resume(); // get ready to accept packet
    bool Pause();  // decoding pause temporary(leaving video decoding thread)
    void Flush();  // only call this after ffmpeg seek(FFmpegSeekWrapper)

    // if a good time to send packet
    bool can_send_packet() const;

    // check if it's a good time to receive a frame. if it is, receive_frame() follow up.
    // this function returns as quick as possible. if true is returned, with pts
    // is seriously late, it may call discard_frames(false) to discard this frame.
    bool can_receive_frame(int64_t& pts, int64_t& duration) const;

    // to send packet, return false if failed(packet need to resend).
    // As FFmpeg implementation, send_packet() may extract frame.
    bool send_packet(AVPacket const& pkt);

    // to receive frame in NV12 format, it may take some time
    bool receive_frame(uint8_t* nv12, int width, int height);

    // discard frames, all or one, must take less or equal time compare to receive_frame()
    bool discard_frame();
};

// audio info
struct AudioInfo {
    AUDIO_TECHNIQUE Technique;
    AUDIO_FORMAT    Format;
    uint32          SampleRate;
    uint8           StreamIndices[4];  // 0xff if invalid
    uint8           StreamChannels[4]; // 0 for unknown
    uint16          TotalStreams;
    uint16          TotalChannels;

    AudioInfo() { Reset(); }
    void Reset() {
        Technique = AUDIO_TECHNIQUE_DEFAULT;
        Format = AUDIO_FORMAT_UNKNOWN;
        SampleRate = TotalStreams = TotalChannels = 0;
        memset(StreamIndices, 0xff, sizeof(StreamIndices));
        memset(StreamChannels, 0, sizeof(StreamChannels));
    }
};

struct VideoOpenOption {
    // audio stream option
    AudioInfo AudioSetting;

    // video size limit
    int MaxVideoResolution;

    // index, 0xff if unknown, 0xfe if turn off
    uint8 VideoStreamId; // current not used, always pick the biggest one!
    uint8 AudioStreamId;    // audio main track
    uint8 SubtitleStreamId; // subtitle track
    uint8 IsLiveStream;     // if set to 1, it's a live stram... be catious!!!

    VideoOpenOption() { Reset(); }
    void Reset() {
        AudioSetting.Reset();
        MaxVideoResolution = -1;
        VideoStreamId = AudioStreamId =  SubtitleStreamId = 0xff;
        IsLiveStream = 0;
    }
};

// customized input stream (via AVIOContext)
class IInputStream
{
    mutable std::mutex mutex_;
    volatile int refCount_;

protected:
    IInputStream():mutex_(),refCount_(1) {}
    virtual ~IInputStream() { assert(refCount_<=0); }

public:
    int RefCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return refCount_;
    }
    int AddRef() {
        std::lock_guard<std::mutex> lock(mutex_);
        BL_ASSERT(refCount_>0); // you see dead people?
        return ++refCount_; 
    }
    int Release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            BL_ASSERT(refCount_>0); // you really did?
            if (--refCount_>0)
                return refCount_;
        } // must unlock before kill this->mutex_
        delete this; // kill self
        return 0;
    }

    // name
    virtual char const* Name() const { return "InputStream"; }

    // url
    virtual char const* URL() const = 0;

    // ready to read from start, will be called when stream is opened.
    virtual bool Rewind() = 0; 

    // read data and return number of bytes read.
    virtual int Read(uint8_t* buf, int buf_size) = 0;

    // like fseek()/_fseeki64(), whence = SEEK_SET(mostly), SEEK_CUR or SEEK_END
    // but unlike fseek()/_fseeki64(), it return stream position.
    virtual int64_t Seek(int64_t offset, int whence) = 0;

    // return stream length.
    virtual int64_t Size() = 0;
};

// host interface to be implemented
class IAVDecoderHost
{
protected:
    IAVDecoderHost() {}
    virtual ~IAVDecoderHost() {}

public:
    // [render thread/callback] video frame update
    virtual bool FrameUpdate(int decoder_id, void const* nv12, int frameId, int w, int h) = 0;

    // [render thread/callback] subtitle timing/texture update
    // channels = 1(U8, softsub) or 4(RGBA, hardsub)
    virtual bool SubtitleUpdate(int decoder_id,
                                void const* pixels, int width, int height, int channels,
                                int pts, int duration, int subtitleID,
                                SubtitleRect* rect, int num_rects) = 0;

    // [read thread/callback] when read end(no more packets)
    virtual void OnStreamEnded(int decoder_id, int error) = 0;

    // [read thread/callback] when video start to play(buffer loaded)
    // return zero for GO instantly, negative value to abort(playback will fail) or
    // positive value m to hold off m milliseconds(it will be called again after m milliseconds).
    virtual int OnStart(int decoder_id, int timestamp, int duration, bool video, bool audio) = 0;

    // [any thread/callback] when video is ended
    virtual void OnStopped(int decoder_id, int timestamp, int duration, bool theEnd) = 0;

    // [render thread/callback] when video is ended and then reset
    virtual void OnReset(int decoder_id) = 0;

    // [render thread/callback] when audio/video data lag
    virtual void OnDataLag(int decoder_id, int lagtime) = 0;

    // [audio device thread] this is the callback when AVDecoder::ConfirmAudioSetting()
    // implement should do :
    //  1) check if audioInfo struct make any sense.
    //  2) change audio device setting if needed.
    //  3) modify audioInfo struct if needed.
    // [hint] call AVDecoder::GetAudioStreamInfo() for complete audio track list.
    virtual bool AudioSettingCallback(int decoder_id, AudioInfo& audioInfo) = 0;
};

//
// main AV Decoder class. Not intent to be derived. use has-a (or has-many)
class AVDecoder
{
    enum STATUS {
        STATUS_EMPTY,   // video not loaded
        STATUS_READY,   // ready to play (from start), next status : PLAYING/SEEKING
        STATUS_PLAYING, // playing, next status : STOP or ENDED
        STATUS_SEEKING, // seeking frame, next status : STOP
        STATUS_ENDED,   // audio/video just funish rendering. next status : READY(instantly).
        STATUS_STOP,    // stop, next status : PLAYING, SEEKING or EMPTY
    };

    // constants - parameters to be tuned up
    enum {
        // limit of packets in 'free list'. 256(128+128) is cool.
        //
        // TO-DO : separate video and audio packets...
        //
        NUM_MAX_AUDIO_PACKETS = 256,
        NUM_MAX_VIDEO_PACKETS = 256,
        NUM_MAX_TOTAL_PACKETS = NUM_MAX_VIDEO_PACKETS + NUM_MAX_AUDIO_PACKETS,

        // max packets to be queued. packet decoding is very fast(if data compressed).
        // Likely the reading file time. For video, 1 packet <= 1 frame.
        NUM_VIDEO_BUFFER_PACKETS = 8,
        NUM_AUDIO_BUFFER_PACKETS = 12,

        //
        // video buffer - NV12 (12bpp)
        // 4K UHD (3840x2160*3/2=12150 KB; 3840x3840*3/2=21600 KB; 4096x4096*3/2=24 MB)
        // 8K UHD (7680x4320*3/2=48600 KB)
        //
        // since the bottleneck, avcodec_decode_video2(), sometimes may take 200ms,
        // set to 8 to avoid frame lag(30fps*200ms/1000ms = 6 frames).
        // but it also needs lots of memory - 192 MB for 4096x4096 or 380MB for 7680x4320
        NUM_VIDEO_BUFFER_FRAMES = 8, // 8/120=66.7ms(!), 8/60=133ms, 8/30=266ms, 8/24=333ms
    };

    class PacketQueue {
        std::mutex    mutex_;
        AVPacketList* first_;
        AVPacketList* last_;
        int64_t       duration_; // summation of all packets' duration
        int           size_;     // packet count
        int           sn_;       // serial number

    public:
        PacketQueue():mutex_(),first_(NULL),last_(NULL),duration_(0),size_(0),sn_(0) {}
        ~PacketQueue() { assert(NULL==first_&&NULL==last_&&0==size_&&0==duration_); }

        int64_t Duration() const { return duration_; }
        int Size() const { return size_; }
        int SN() const { return sn_; }
        void Put(AVPacketList* pk) {
            if (NULL!=pk) {
                assert(NULL==pk->next);
                std::lock_guard<std::mutex> lock(mutex_);
                if (NULL==last_) {
                    assert(NULL==first_ && 0==size_ && 0==duration_);
                    first_ = pk;
                }
                else {
                    last_->next = pk;
                }
                last_ = pk;
                duration_ += pk->pkt.duration;
                ++size_;
            }
        }
        AVPacketList* Get() {
            std::lock_guard<std::mutex> lock(mutex_);
            AVPacketList* pk = first_;
            if (pk) {
                first_ = first_->next;
                duration_ -= pk->pkt.duration;
                --size_; ++sn_;
                assert(size_>=0 && duration_>=0);
                if (NULL==first_) {
                    assert(0==size_ && 0==duration_);
                    last_ = NULL;
                }
                pk->next = NULL;
            }
            return pk;
        }
    };

    //
    // The C++ Programming Language 4th, 42.3.4, Bjarne Strousstrup.
    // condition_variables can wake up "spuriously". That is, the system may decide
    // to resume wait()'s thread even though no other thread has notified it!
    // Appearingly, allowing spurious wake-up simplifies implementaion of
    // condition_variables on some system. Always use wait() in a loop.
    //
    // Always use wait() in a loop!
    //   Always use wait() in a loop!!
    //     Always use wait() in a loop!!!
    //

    //
    // 2 bug reports if compiles on Visual Studio 2012(fixed in Visual Studio 2013)
    //
    // a 44 bytes(win32)/72 bytes(win64) leak is reported for vs2012
    // https://connect.microsoft.com/VisualStudio/feedback/details/757212
    //
    // a condition_variable::wait_for() crash report persisted in vs2012
    // https://connect.microsoft.com/VisualStudio/feedback/details/762560
    //
    AVPacketList     avPackets_[NUM_MAX_TOTAL_PACKETS];
    SubtitleRect     subtitleRects_[Subtitle::MAX_NUM_SUBTITLE_RECTS];
    PacketQueue      videoQueue_;
    PacketQueue      subtitleQueue_;
    PacketQueue      audioQueue_;
    PacketQueue      audioQueueExt1_, audioQueueExt2_, audioQueueExt3_; // extended audio queues
    VideoDecoder     videoDecoder_;
    Subtitle         subtitleUtil_;
    AudioInfo        audioInfo_; // audio setting
    std::thread      decodeThread_;
    std::condition_variable videoCV_;
    std::mutex       videoMutex_;    // for videoCV_
    std::mutex       subtitleMutex_; // for switching subtitle
    std::mutex       audioMutex_;    // for switching audio
    std::mutex       pktListMutex_;
    std::mutex       asyncPlayMutex_;
    std::atomic<STATUS> status_;
    std::atomic<int> videoSeekTimestamp_;
    std::atomic<int> videoFramePut_;
    std::atomic<int> videoFrameGet_;
    std::atomic<int> subtitlePut_;
    std::atomic<int> subtitleGet_;
    std::atomic<int> audioBufferPut_;
    std::atomic<int> audioBufferGet_;
    int64_t          videoFramePTS_[NUM_VIDEO_BUFFER_FRAMES]; // must be microseconds
    IAVDecoderHost*  host_;     // decoder's owner/host
    IInputStream*    inputStream_;
    AVPacketList*    avPacketFreeList_;
    AVFormatContext* formatCtx_;
    AVIOContext*     ioCtx_;
    uint8_t*         videoBuffer_; // decoded AV_PIX_FMT_YUV420P pixels
    uint8_t*         subtitleBuffer_; // subtitle buffer, RGBA or U8 (signed distance) format
    uint8_t*         audioBuffer_; // audio ring buffer, decoded audio sample data
    int64_t          timeAudioCrossfade_; // the system time used for time cross fade
    int64_t          timeStartSysTime_; // the system time the video play at very start
    int64_t          timeInterruptSysTime_; // interrupt system time (when audio samples failed to decode)
    int64_t          timeLastUpdateFrameSysTime_; // last system time when UpdateFrame() is called
    int64_t          duration_;         // formatCtx_->duration if valid, 0 for not load, -1 if live streaming
    int64_t          startTime_;        // formatCtx_->start_time if valid
    int64_t          videoPutPTS_;      // video frame put pts
    int64_t          videoGetPTS_;      // video frame get pts
    int64_t          subtitlePTS_;      // subtitle put PTS
    int64_t          audioPutPTS_;      // audio data put PTS
    int64_t          audioGetPTS_;      // audio data get PTS
    VIDEO_3D         stereo3D_; // by FFmpeg side data, seems not always work?

    // unique decoder id specified by the host
    int const id_;

    int videoBufferSize_;
    int videoFrameDataSize_; // a single video frame size
    int subtitleBufferSize_;
    int audioBufferCapacity_;
    int audioBufferWrapSize_; // align to audio sample size
    int videoStreamIndex_;
    int videoStreamCount_;
    int subtitleStreamIndex_; // external subtitles begin from formatCtx_->nb_streams
    int subtitleStreamIndexSwitch_;
    int subtitleStreamCount_;
    int audioStreamIndex_;
    int audioStreamIndexSwitch_;
    int audioStreamCount_;
    int audioExtStreamIndex1_; // extended audio stream#1
    int audioExtStreamIndex2_; // extended audio stream#2
    int audioExtStreamIndex3_; // extended audio stream#3
    int videoWidth_;
    int videoHeight_;
    int videoFrameRate100x_;  // e.g. 2997 means 29.97, 3000 stand for 30.0
    int subtitleDuration_; // current subtitle in milliseconds
    int subtitleWidth_, subtitleHeight_; // pixel size
    int subtitleRectCount_;
    int audioBytesPerFrame_; // bytes per frame(frame=channels*samples)
    int audioBytesPerSecond_; // bytes per second
    uint8 videoDecoderPreference_; // 0:FFmpeg SW, 1:GPU(AMF/NVDEC), 2+:FFmpeg + hw accel.
    uint8 subtitleGraphicsFmt_; // subtitle format(pixel bytes) 1:DistanceMap 4:RGBA8
    uint8 subtitleBufferFlushing_; // subtitle switched, must flush buffer and force sync
    uint8 audioBufferFlushing_; // audio switched, must flush buffer and force sync
    uint8 liveStream_;  // 2016.12.30 just a hint(for larger buffering)
    uint8 endOfStream_; // end of stream(internet traffic) or end of file
    volatile uint8 interruptRequest_;
    // pad 1 byte...

    // open/close url/file/custom stream
    bool DoOpen_(AVFormatContext* fmtCtx, char const* url, VideoOpenOption const* param);
    void DoClose_();

    // main playing (STATUS_PLAYING) thread. to read all packets and kick off and join
    // video and audio/subtitle threads
    void MainThread_();

    // video seeking (STATUS_SEEKING) thread
    void SeekThread_();

    // video decoding thread
    bool VideoThread_();

    // audio and subtitle decoding thread
    bool AudioSubtitleThread_();

    // flush all buffers and queues
    void FlushBuffers_() {
        // flash video decoder
        videoDecoder_.Flush();
        // flush queues and unfinished frames
        FlushQueue_(videoQueue_);
        FlushQueue_(subtitleQueue_);
        FlushQueue_(audioQueue_);
        FlushQueue_(audioQueueExt1_);
        FlushQueue_(audioQueueExt2_);
        FlushQueue_(audioQueueExt3_);
    }

    // packet management
    void FlushQueue_(PacketQueue& queue) {
        AVPacketList* pk = queue.Get();
        while (NULL!=pk) {
            ReleaseAVPacketList_(pk);
            pk = queue.Get();
        }
    }
    AVPacketList* GetPacketList_() {
        std::lock_guard<std::mutex> lock(pktListMutex_);
        AVPacketList* list = avPacketFreeList_;
        if (NULL!=list) {
            avPacketFreeList_ = list->next;
            list->next = NULL;
        }
        return list;
    }
    void ReleaseAVPacketList_(AVPacketList* pk) {
        if (NULL!=pk) {
            assert(avPackets_<=pk && pk<(avPackets_+NUM_MAX_TOTAL_PACKETS));
            av_packet_unref(&(pk->pkt));
            {
                std::lock_guard<std::mutex> lock(pktListMutex_);
                pk->next = avPacketFreeList_;
                avPacketFreeList_ = pk;
            }
        }
    }

    // interrupt request
    static int DecodeInterruptCB_(void* thiz) {
        if (NULL!=thiz && ((AVDecoder*)thiz)->interruptRequest_) {
            // abort blocking functions, e.g. av_read_frame() return AVERROR_EXIT(0xabb6a7bb)
#ifdef BL_AVDECODER_VERBOSE
            if (STATUS_PLAYING==((AVDecoder*)thiz)->status_) {
                BL_LOG("** immediate exit was requested!\n");
            }
#endif
            return 1;
        }
        return 0;
    }

    // input stream
    static int InputStreamRead_(void* is, uint8_t* buf, int buf_size) {
        return (NULL!=is) ? (((IInputStream*)is)->Read(buf, buf_size)):0;
    }
    static int64_t InputStreamSeek_(void* is, int64_t offset, int whence) {
        IInputStream* stream = (IInputStream*)is;
        if (stream) {
            whence &= ~AVSEEK_FORCE;
            return (0==(AVSEEK_SIZE&whence)) ? stream->Seek(offset, whence):stream->Size();
        }
        return 0;
    }

    // declare but not define
    AVDecoder();
    AVDecoder(AVDecoder const&);
    AVDecoder& operator=(AVDecoder const&);

public:
    explicit AVDecoder(IAVDecoderHost* listener, int decoder_id=0);
    ~AVDecoder(); // not intent to be derived!

    VIDEO_3D Stereoscopic3D() const { return stereo3D_; } // as metadata
    int VideoWidth() const { return videoWidth_; }
    int VideoHeight() const { return videoHeight_; }
    float VideoFrameRate() const { return ((float)videoFrameRate100x_)/100.0f; }
    float PlaybackProgress() const; // return playback progress [0.0, 1.0f]
    int VideoStreamID() const { return videoStreamIndex_; }
    int SubtitleStreamID() const { return subtitleStreamIndex_; }
    int AudioStreamID() const { return audioStreamIndex_; }
    int TotalStreamCount() const { return (NULL!=formatCtx_) ? int(formatCtx_->nb_streams):0; }
    int VideoStreamCount() const { return videoStreamCount_; }
    int SubtitleStreamCount() const {
        return subtitleStreamCount_ + subtitleUtil_.TotalStreams();
    }
    int ExternalSubtitleStreamCount() const { return subtitleUtil_.TotalStreams(); }
    int AudioStreamCount() const { return audioStreamCount_; }
    bool AudioStreamChangeable() const {
        if (0<audioStreamCount_ && 0<=audioStreamIndex_ && audioExtStreamIndex1_<0) {
            return (AUDIO_TECHNIQUE_DEFAULT==audioInfo_.Technique);
        }
        return false;
    }
    bool GetAudioSetting(AudioInfo& setting) const {
        if (0<audioStreamCount_ && 0<=audioStreamIndex_) {
            setting = audioInfo_;
            return true;
        }
        return false;
    }

    /////////////////////////////////////////////////////////////////////////////////////
    // call with care... FixErrorDuration() is to work around some videos with
    // wrong duration (probably because of imcomplete downloaded videos).
    // e.g. "The Jungle Book" from one of our mainland china user.
    //
    // doc says int64_t av_stream_get_end_pts(const AVStream *st) is to get the
    // pts of the last muxed packet + its duration. but it does not work for me...
    void FixErrorDuration(int d) {
        if (d>0) {
            duration_ = int64_t(d)*1000;
        }
    }
    /////////////////////////////////////////////////////////////////////////////////////

    // input stream
    IInputStream const* InputStream() const { return inputStream_; }

    int GetDuration() const { return (duration_>0) ? int(duration_/1000):-1000; }
    int Timestamp() const; // timestamp in milliseconds
    int VideoTime() const { return (int) ((videoGetPTS_-startTime_)/1000); }
    int AudioTime() const { return (int) ((audioGetPTS_-startTime_)/1000); }
    bool NearlyEnd(int time_remain=1000) const {
        return time_remain>0 && ((Timestamp()+time_remain)>(duration_/1000));
    }
    bool IsPlaying() const { return (STATUS_PLAYING==status_)||(STATUS_ENDED==status_); }
    bool IsSeeking() const { return (STATUS_SEEKING==status_); }
    bool IsLoaded() const { return (STATUS_READY<=status_); }
    bool SetSubtitleStream(int streamID, bool externalId=false) {
        std::lock_guard<std::mutex> lock(asyncPlayMutex_);
        if (NULL!=formatCtx_ && streamID!=subtitleStreamIndex_) {
            FlushQueue_(subtitleQueue_);
            subtitleGet_ = (int) subtitlePut_;
            if (0<=streamID) {
                if (externalId) {
                    if (streamID<subtitleUtil_.TotalStreams()) {
                        subtitleStreamIndexSwitch_ = (int)formatCtx_->nb_streams + streamID;
                        return true;
                    }
                }
                else {
                    if (streamID<(int)formatCtx_->nb_streams) {
                        if (AVMEDIA_TYPE_SUBTITLE==FFmpeg_Codec_Type(formatCtx_->streams[streamID])) {
                            subtitleStreamIndexSwitch_ = streamID;
                            return true;
                        }
                    }
                    else if (streamID<((int)formatCtx_->nb_streams + subtitleUtil_.TotalStreams())) {
                        subtitleStreamIndexSwitch_ = streamID;
                        return true;
                    }
                }
            }
            else {
                subtitleStreamIndexSwitch_ = -1;
                return true;
            }
        }
        return false;
    }
    bool SetAudioStream(int streamID) {
        std::lock_guard<std::mutex> lock(asyncPlayMutex_);
        if (NULL!=formatCtx_ && STATUS_PLAYING==status_ && streamID!=audioStreamIndex_ &&
            0<=audioStreamIndex_ && -1==audioExtStreamIndex1_ &&
            0<=streamID && streamID<(int)formatCtx_->nb_streams &&
            AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(formatCtx_->streams[streamID])) {
            audioStreamIndexSwitch_ = streamID;
            return true;
        }
        return false;
    }
    // not thread safe!
    int GetSubtitleStreamInfo(int streamIDs[], ISO_639 languages[], int max_count) const {
        int get_streams = 0;
        if (0<subtitleStreamCount_ && NULL!=formatCtx_) {
            int const total_streams = (int) formatCtx_->nb_streams;
            AVDictionaryEntry const* t = NULL;
            for (int i=0; i<total_streams; ++i) {
                AVStream* stream = formatCtx_->streams[i];
                if (NULL!=stream && AVMEDIA_TYPE_SUBTITLE==FFmpeg_Codec_Type(stream)) {
                    streamIDs[get_streams] = i;
                    t = av_dict_get(stream->metadata, "language", NULL, 0);
                    if (NULL!=t) {
                        languages[get_streams] = Translate_ISO_639(t->value);
                    }
                    else {
                        languages[get_streams] = ISO_639_UNKNOWN;
                    }

                    if (++get_streams>=max_count) {
                        break;
                    }
                }
            }
        }

        if (get_streams<max_count && NULL!=formatCtx_) {
            int const external_subtitle_streams =
                        subtitleUtil_.GetSubtitleStreamInfo(streamIDs+get_streams,
                                                            languages+get_streams,
                                                            max_count-get_streams);
            int const srt_stream_offsets = (int) formatCtx_->nb_streams;
            for (int i=0; i<external_subtitle_streams; ++i) {
                streamIDs[get_streams] += srt_stream_offsets;
                ++get_streams;
            }
        }

        return get_streams;
    }
    // not thread safe!
    int GetAudioStreamInfo(int streamIDs[], int channels[], ISO_639 languages[], int max_count) const {
        int get_streams = 0;
        if (0<audioStreamCount_ && NULL!=formatCtx_) {
            int const total_streams = (int) formatCtx_->nb_streams;
            AVDictionaryEntry const* t = NULL;
            for (int i=0; i<total_streams; ++i) {
                AVStream* stream = formatCtx_->streams[i];
                if (NULL!=stream && AVMEDIA_TYPE_AUDIO==FFmpeg_Codec_Type(stream)) {
                    streamIDs[get_streams] = i;
                    channels[get_streams] = (stream->codecpar) ? (stream->codecpar->channels):-1;
                    t = av_dict_get(stream->metadata, "language", NULL, 0);
                    if (NULL!=t) {
                        languages[get_streams] = Translate_ISO_639(t->value);
                    }
                    else {
                        languages[get_streams] = ISO_639_UNKNOWN;
                    }

                    if (++get_streams>=max_count) {
                        break;
                    }
                }
            }
        }

        return get_streams;
    }

    // audio setting
    AudioInfo const& GetAudioInfo() const { return audioInfo_; }

    // video buffer/decoding
    int DecodingVideoFrame() const { return videoFramePut_; }
    int VideoBuffering(int& packets, int& sn) const {
        sn = videoQueue_.SN();
        packets = videoQueue_.Size();
        return videoFramePut_ - videoFrameGet_;
    }

    // hardware(gpu) video decoder
    int IsHardwareVideoDecoder() const { return videoDecoder_.IsHardwareVideoDecoder(); }
    int NumAvailableVideoDecoders() const {
        uint32 flags = videoDecoder_.Flags();
        if (flags&0x01) {
            int decoders = 1;
            while (flags>>=1) {
                if (flags&0x01) {
                    ++decoders;
                }
            }
            return decoders;
        }
        return 0;
    }
    char const* VideoDecoderName() const { return videoDecoder_.Name(); }
    bool ToggleHardwareVideoDecoder();

    // return last subtitle pts and duration, in millisecond
    int SubtitleBuffering(int& extStreamId, bool& hardsub, int& start, int& duration) const {
        if (0<=subtitleStreamIndex_ && NULL!=formatCtx_) {
            int const total_streams = (int) formatCtx_->nb_streams;
            extStreamId = (subtitleStreamIndex_>=total_streams) ? (subtitleStreamIndex_-total_streams):-1;
            start = (subtitlePTS_>startTime_) ? (int) ((subtitlePTS_-startTime_)/1000):0;
            duration = subtitleDuration_;
            hardsub = (4==subtitleGraphicsFmt_);
            return subtitleStreamIndex_;
        }
        return -1;
    }

    // return length of audio buffering data, in milliseconds
    int AudioBuffering(int& packets, int& sn) const {
        if (0<=audioStreamIndex_ && NULL!=formatCtx_) {
            sn = audioQueue_.SN();
            packets = audioQueue_.Size();
            return (int) av_rescale(audioBufferPut_ - audioBufferGet_, 1000, audioBytesPerSecond_);
            //return (int)((audioPutPTS_ - audioGetPTS_)/1000);
        }
        return 0;
    }

    // close video file
    void Close() {
        std::lock_guard<std::mutex> lock(asyncPlayMutex_);
        DoClose_();
    }

    // open with url (mpeg file or internet stream), must be utf-8 encoding.
    bool Open(char const* url, VideoOpenOption const* param=NULL);

    // open with input stream.
    bool Open(IInputStream* is, VideoOpenOption const* param=NULL);

    // config audio settings, should be called in audio setting thread.
    // it will callback IAVDecoderHost::AudioSettingCallback() to let host
    // change audio devices. It failed if not STATUS_READY, so call it in
    // audio thread as soon as video Open() successfully.
    bool ConfirmAudioSetting();

    // it must be called repeatly and it may trigger
    // host_->FrameUpdate() and/or host_->SubtitleUpdate() to update texture.
    // so we really need it to be called in render thread.
    bool UpdateFrame();

    // stream out audio data
    //  buffer[out] - pointer to buffer to retrieve audio data
    //  maxSize[in] - maximum size of buffer
    //  frames[in]  - total audio frames to get
    //  gain[out]   - crossover fading from 0.0(mute) to 1.0(fullscale)
    //  config[out] - (optional) audio data config
    int StreamingAudio(uint8* buffer, int maxSize, int frames, float& gain, AudioInfo* info);

    // play/stop
    bool Play();
    bool PlayAt(int time /* milliseconds */);
    bool Replay() { return PlayAt(0); }
    bool Stop();

    // seeking video frames async.
    bool StartVideoSeeking(int timestamp);
    bool SeekVideoFrame(int timestamp) {
        std::lock_guard<std::mutex> lock(asyncPlayMutex_);
        if (STATUS_SEEKING==status_ && 0<=timestamp) {
            videoSeekTimestamp_ = timestamp;
            videoCV_.notify_one();
            return true;
        }
        return false;
    }
    bool EndVideoSeeking(bool startPlay=false) {
        std::lock_guard<std::mutex> lock(asyncPlayMutex_);
        if (STATUS_SEEKING==status_) {
            status_ = STATUS_STOP; // need this to join
            if (decodeThread_.joinable()) {
                videoCV_.notify_all();
                interruptRequest_ = 1;
                decodeThread_.join();
            }

            audioBufferPut_ = audioBufferGet_ = 0;
            videoFramePut_ = videoFrameGet_ = 0;
            subtitlePut_ = subtitleGet_ = 0;
            subtitleWidth_ = subtitleHeight_ = subtitleRectCount_ = subtitleDuration_ = 0;
            subtitlePTS_ = audioPutPTS_ = audioGetPTS_ = 0;
            timeAudioCrossfade_ = timeStartSysTime_ = timeInterruptSysTime_ = 0;
            audioBufferFlushing_ = endOfStream_ = interruptRequest_ = 0;
            if (startPlay) {
                status_ = STATUS_PLAYING;
                decodeThread_ = std::move(std::thread([this] { MainThread_(); }));
            }
            return true;
        }
        return false;
    }

    // temp : not thread-safe
    bool ToggleAmbisonicFormat() {
        if (AUDIO_TECHNIQUE_AMBIX==audioInfo_.Technique) {
            audioInfo_.Technique = AUDIO_TECHNIQUE_FUMA;
            return true;
        }
        else if (AUDIO_TECHNIQUE_FUMA==audioInfo_.Technique) {
            audioInfo_.Technique = AUDIO_TECHNIQUE_AMBIX;
            return true;
        }
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // arrrh, this is very, very ugly!!! [and for Vive Cinema use only]
    // ---caller must free return pointer!---
    void* CookExtSubtitleNameTexture(int& width, int& height,
                                     int tc[][4], int* subStmIDs, int total_subtitles) const {
        int const id_offset = (NULL!=formatCtx_) ? (int) formatCtx_->nb_streams:0;
        return subtitleUtil_.CookExtSubtitleNameTexture(width, height,
                                                    tc, subStmIDs, id_offset, total_subtitles);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////
};

}}}
#endif