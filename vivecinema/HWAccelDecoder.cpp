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
 * @file    HWAccelDecoder.cpp
 * @author  andre chen
 * @history 2017/08/22 created
 *
 */
#include "HWAccelDecoder.h"

//
// nVidia Video Codec SDK
#include "dynlink_nvcuvid.h"
#include "dynlink_cudaGL.h"  // cuda and GL interop

//
// Advanced Micro Devices(AMD) Advanced Media Framework(AMF) SDK
//
// Important Note:
//   For now, AMF may hang, result corrupted images after seek. I had fired
//   several bug reports to AMF team and Mikhail and the AMD codec team are now
//   working on it. The fix is expected to come with new released driver
//   (currently 17.9.2) released. Before that, fall back to Software video Decoder
//   for safety.
//
#include "./amf/public/common/AMFFactory.h"
#include "./amf/public/include/components/VideoDecoderUVD.h"
#include "./amf/public/common/Thread.h" // amf_sleep()

#include <mutex>

//
// not ready yet!
//#define CUVID_ENABLE_HDR_OUTPUT
//

namespace mlabs { namespace balai { namespace video {

inline wchar_t const* getAMFCodec(AVCodecID id) {
    if (AV_CODEC_ID_H264==id) {
        return AMFVideoDecoderUVD_H264_AVC;
    }
    else if (AV_CODEC_ID_HEVC==id) {
        return AMFVideoDecoderHW_H265_HEVC;
    }
    else if (AV_CODEC_ID_MPEG4==id) {
        return AMFVideoDecoderUVD_MPEG4;
    }
    else if (AV_CODEC_ID_MPEG2VIDEO==id) {
        return AMFVideoDecoderUVD_MPEG2;
    }
    else if (AV_CODEC_ID_WMV3==id) {
        return AMFVideoDecoderUVD_WMV3;
    }
    else if (AV_CODEC_ID_VC1==id) {
        return AMFVideoDecoderUVD_VC1;
    }
//    else if (AV_CODEC_ID_MJPEG==id) { // not interested
//        return AMFVideoDecoderUVD_MJPEG;
//    }
    else if ((AV_CODEC_ID_WRAPPED_AVFRAME+0x300000)==id) {
        return AMFVideoDecoderUVD_H264_MVC;
    }
/*
    case AV_CODEC_ID_???:
        codec = AMFVideoDecoderUVD_H264_SVC;
        break;

    case AV_CODEC_ID_???:
        codec = AMFVideoDecoderHW_H265_MAIN10;
        break;
*/
    return NULL;
}
inline cudaVideoCodec getCuvidCodec(AVCodecID id) {
    if (AV_CODEC_ID_H264==id) {
        return cudaVideoCodec_H264;
    }
    else if (AV_CODEC_ID_HEVC==id) {
        return cudaVideoCodec_HEVC;
    }
    else if (AV_CODEC_ID_VP8==id) {
        return cudaVideoCodec_VP8;
    }
    else if (AV_CODEC_ID_VP9==id) {
        return cudaVideoCodec_VP9;
    }
    else if (AV_CODEC_ID_MPEG4==id) {
        return cudaVideoCodec_MPEG4;
    }
//    else if (AV_CODEC_ID_MJPEG==id) {
//        return cudaVideoCodec_JPEG;
//    }
    else if (AV_CODEC_ID_MPEG1VIDEO==id) {
        return cudaVideoCodec_MPEG1;
    }
    else if (AV_CODEC_ID_MPEG2VIDEO==id) {
        return cudaVideoCodec_MPEG2;
    }
    else if (AV_CODEC_ID_VC1==id) {
        return cudaVideoCodec_VC1;
    }
    return cudaVideoCodec_NumCodecs;
}

// taken from cuvid samples VideoDecoder.cpp
inline int cuvidGetNumSurfaces(cudaVideoCodec nvCodec, int width, int height)
{
    if (cudaVideoCodec_H264==nvCodec || cudaVideoCodec_H264_SVC==nvCodec || cudaVideoCodec_H264_MVC==nvCodec) {
        return 20; // assume worst-case of 20 decode surfaces for H264
    }
    else if (cudaVideoCodec_VP9==nvCodec) {
        return 12;
    }
    else if (cudaVideoCodec_HEVC==nvCodec) { // ref HEVC spec: A.4.1 General tier and level limits
        int const MaxLumaPS = 35651584; // currently assuming level 6.2, 8Kx4K
        int const MaxDpbPicBuf = 6;
        int const PicSizeInSamplesY = width * height;
        int MaxDpbSize;
        if (PicSizeInSamplesY <= (MaxLumaPS>>2)) {
            MaxDpbSize = MaxDpbPicBuf*4;
        }
        else if (PicSizeInSamplesY <= (MaxLumaPS>>1)) {
            MaxDpbSize = MaxDpbPicBuf*2;
        }
        else if (PicSizeInSamplesY <= ((3*MaxLumaPS)>>2)) {
            MaxDpbSize = (MaxDpbPicBuf*4)/3;
        }
        else {
            MaxDpbSize = MaxDpbPicBuf;
        }

        return (MaxDpbSize<16) ? (MaxDpbSize+4):20;
    }

    return 8;
}

//
// GPU context for NVDEC and AMD AMF
struct GPUContext
{
    struct MaxSize {
        int Width, Height;
        MaxSize():Width(0),Height(0) {}
    } cuvidCaps8[cudaVideoCodec_NumCodecs], 
    cuvidCaps10[cudaVideoCodec_NumCodecs],
    cuvidCaps12[cudaVideoCodec_NumCodecs];
    std::mutex       mutex;
    CUcontext        cudaContext;
    amf::AMFContext* amfContext;
    int              activeDecoders;
    int              init;

    GPUContext():mutex(),cudaContext(NULL),amfContext(NULL),activeDecoders(0),init(0) {}
    ~GPUContext() {
        std::lock_guard<std::mutex> lock(mutex);
        assert(NULL==cudaContext);
        assert(NULL==amfContext);
        assert(0==init&&0==activeDecoders);
    }
} gpuCtx;

//
// AMD AMF decoder
class AMFDecoder : public VideoDecoderImp
{
    amf::AMFContext*     context_;
    amf::AMFComponentPtr decoder_;
    amf::AMFDataPtr      frame_;
    int const            width_;
    int const            height_;
    int                  framesLoaded_;

    BL_NO_DEFAULT_CTOR(AMFDecoder);
    BL_NO_COPY_ALLOW(AMFDecoder);

public:
    AMFDecoder(AVCodecID cid, int width, int height):
        VideoDecoderImp(cid),context_(nullptr),decoder_(nullptr),frame_(nullptr),
        width_(width),height_(height),framesLoaded_(0) {
    }

    ~AMFDecoder() {
        if (decoder_) {
            decoder_->Terminate();
            //decoder_->Release(); NO!
            decoder_ = NULL;
        }
        if (context_) {
            //context_->Terminate();
            context_ = NULL;
        }
    }

    // identity
    char const* Name() const { return "AMD AMF (GPU)"; }

    bool Init(amf::AMFContext* ctx, AVCodecParameters* codecpar) {
        assert(nullptr==context_ && nullptr==decoder_);
        wchar_t const* codec = getAMFCodec(CodecID());
        if (codecpar && codec && ctx) {
            context_ = ctx;
            if (AMF_OK==g_AMFFactory.GetFactory()->CreateComponent(context_, codec, &decoder_)) {
#if 0
                // report error if hevc... [2017/09/14] AMD Mikhail Mironov : "It's a bug!!!"
                // [2017/09/22] Mikhail Mironov fixed in driver 17.9.2 released today.
                // 2017-09-08 15:42:16.092     3550 [AMFDecoderUVDImpl]   Error: ..\..\..\..\..\runtime\src\components\DecoderUVD\DecoderUVDImpl.cpp(1676):AMF_ERROR 1 : AMF_FAIL: pCaps->Init(m_pContext, m_codecId)
                amf::AMFCapsPtr decoderCaps;
                if (AMF_OK==decoder_->GetCaps(&decoderCaps)) {
                    //amf::AMF_ACCELERATION_TYPE accelType = decoderCaps->GetAccelerationType();
                    int minWidth(0), minHeight(0);
                    int maxWidth(0), maxHeight(0);
                    amf::AMFIOCapsPtr inputCaps;
                    if (AMF_OK==decoderCaps->GetInputCaps(&inputCaps)) {
                        //result = QueryIOCaps(inputCaps);
                        inputCaps->GetWidthRange(&minWidth, &maxWidth);
                        inputCaps->GetHeightRange(&minHeight, &maxHeight);
                    }

                    amf::AMFIOCapsPtr outputCaps;
                    if (AMF_OK==decoderCaps->GetOutputCaps(&outputCaps)) {
                        //result = QueryIOCaps(outputCaps);
                    }
                }
#endif
                //
                // [2017/08/16] message from AMD Mikhail Mironov....
                // "AMF internal timing is always in 100th nanoseconds and you
                //  may consider to convert. See FileDemuxerFFMPEGImpl.cpp,
                //  AMFFileDemuxerFFMPEGImpl::UpdateBufferProperties()."
                //
                // "The decoder really doesn't care about timescale. It just
                //  transfer the pts from input to output. It has three modes
                //  to deal with timestamps controlled by AMF_TIMESTAMP_MODE
                //  property which is one entry from AMF_TIMESTAMP_MODE_ENUM.
                //  You can choose any option but for ffmepg integration the best
                //  is to use AMF_TS_DECODE."
                //
                // andre : After checked out some amf samples, it should set
                //         timestamp mode as AMF_TS_SORT -- the only way to get
                //         monotonic increasing frame pts.
                //
                // PlaybackPipelineBase.cpp
                // "our sample H264 parser provides decode order timestamps - change depend on demuxer"
                // m_pVideoDecoder->SetProperty(AMF_TIMESTAMP_MODE,
                //            amf_int64(m_pVideoStreamParser!=NULL ? AMF_TS_DECODE:AMF_TS_SORT));
                //
                //AMF_TIMESTAMP_MODE_ENUM const ts_mode = AMF_TS_PRESENTATION;
                AMF_TIMESTAMP_MODE_ENUM const ts_mode = AMF_TS_SORT;
                //AMF_TIMESTAMP_MODE_ENUM const ts_mode = AMF_TS_DECODE;
                decoder_->SetProperty(AMF_TIMESTAMP_MODE, amf_int64(ts_mode));

                //
                // decoder reorder mode(defines number of surfaces in DPB list). options are
                //  AMF_VIDEO_DECODER_MODE_REGULAR(default), DPB delay is based on number of reference frames + 1 (from SPS)
                //  AMF_VIDEO_DECODER_MODE_COMPLIANT,        DPB delay is based on profile - up to 16
                //  AMF_VIDEO_DECODER_MODE_LOW_LATENCY,      DPB delay is 0. Expect stream with no reordering in P-Frames or B-Frames. B-frames can be present as long as they do not introduce any frame re-ordering 
                //
                // andre : i got some error message like "2108 [H264Parser] Warning: GetDPBSize() max_dec_frame_buffering=4 larger than MaxDpbSize=2"
                //         and rubbish frames are decoded. Can that problem be fixed via setting decoder mode correctly!?
                AMF_VIDEO_DECODER_MODE_ENUM const de_mode = AMF_VIDEO_DECODER_MODE_COMPLIANT;
                decoder_->SetProperty(AMF_VIDEO_DECODER_REORDER_MODE, amf_int64(de_mode));
                //
                //AMF_RESULT err = decoder_->SetProperty(AMF_VIDEO_DECODER_DPB_SIZE, amf_int64(4));
                // err = AMF_ACCESS_DENIED
                //

                // set SPS/PPS extracted from stream
                // since AVStream::codec is deprecated, use the codecpar struct instead
                //if (stream->codec->extradata_size>0 && stream->codec->extradata) {
                if (codecpar->extradata_size>0 && codecpar->extradata) {
                    amf::AMFBufferPtr buffer;
                    context_->AllocBuffer(amf::AMF_MEMORY_HOST, codecpar->extradata_size, &buffer);
                    if (buffer) {
                        memcpy(buffer->GetNative(), codecpar->extradata, codecpar->extradata_size);
                        decoder_->SetProperty(AMF_VIDEO_DECODER_EXTRADATA, amf::AMFVariant(buffer));
                    }
                }

                // init decoder with source width & height
                if (AMF_OK==decoder_->Init(amf::AMF_SURFACE_NV12, width_, height_)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool Resume() {
        //
        // when video decoding is about to start, what should we do now???
        //
        framesLoaded_ = 0;
        return true;
    }
    bool Pause() {
        //
        // when video decoding is about to stop(leaving decoding thread), what should we do now???
        //
        framesLoaded_ = 0;
        return true;
    }
    void Flush() {
        //
        // when video seek occurs
        //
        if (decoder_) {
            //
            // ????
            //decoder_->ReInit(width_ ,height_);
            //decoder_->Drain();
            //

            // just call Flush()?
            decoder_->Flush();
        }
        framesLoaded_ = 0;
    }

    bool can_send_packet() {
        return (framesLoaded_<1);
    }
    bool can_receive_frame(int64_t& pts, int64_t& dur) {
        if (0<framesLoaded_ && frame_) {
            pts = frame_->GetPts();
            dur = frame_->GetDuration();
            return true;
        }
        else if (AMF_OK==decoder_->QueryOutput(&frame_) && frame_) {
            framesLoaded_ = 1;
            pts = frame_->GetPts();
            dur = frame_->GetDuration();
            return true;
        }
        return false;
    }
    bool send_packet(AVPacket const& pkt) {
        if (pkt.size<=0 || NULL==pkt.data)
            return true;

        /*
            FF_INPUT_BUFFER_PADDING_SIZE(32) bytes zero padding is taken from AMFFileDemuxerFFMPEGImpl::BufferFromPacket(), FileDemuxerFFMPEGImpl.cpp
            Mikhail 2017/08/20 : Please do not add any padding to AMFBuffer. This will lead to decoding errors...
        */
        uint8* ptr = NULL;
        amf::AMFBufferPtr pkt_buffer;  // packet buffer
#ifdef AMF_NEED_PADDING_BYTES
        if (AMF_OK==context_->AllocBuffer(amf::AMF_MEMORY_HOST, pkt.size+FF_INPUT_BUFFER_PADDING_SIZE, &pkt_buffer)) {
#else
        if (AMF_OK==context_->AllocBuffer(amf::AMF_MEMORY_HOST, pkt.size, &pkt_buffer)) {
#endif
            if (AMF_OK==pkt_buffer->SetSize(pkt.size)) {
                ptr = (uint8*) pkt_buffer->GetNative();
            }
        }

        if (NULL==ptr) {
            BL_ERR("** [Error] AMFAccelContext::send_packet() failed to setup packet buffer(%d B)\n", pkt.size);
            return false;
        }

        // copy the packet memory
        memcpy(ptr, pkt.data, pkt.size);

#ifdef AMF_NEED_PADDING_BYTES
        // clear data padding like it is done by FFMPEG
        memset(ptr+pkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
#endif
        // set pts
        pkt_buffer->SetPts(pkt.pts); // must work with AMF_TIMESTAMP_MODE = AMF_TS_SORT;
        pkt_buffer->SetDuration(pkt.duration);

        //amf_pts ffmpeg_pts, ffmpeg_dts, ffmpeg_duration;
        //pkt_buffer->SetProperty(L"FFMPEG:pts", amf::AMFVariant(pkt.pts));
        //pkt_buffer->SetProperty(L"FFMPEG:dts", amf::AMFVariant(pkt.dts));
        //pkt_buffer->SetProperty(L"FFMPEG:duration", amf::AMFVariant(pkt.duration));
        //pkt_buffer->SetProperty(L"FFMPEG:stream_index", amf::AMFVariant(pkt.stream_index));
        //pkt_buffer->SetProperty(L"FFMPEG:flags", amf::AMFVariant(pkt.flags));
        //pkt_buffer->SetProperty(L"FFMPEG:pos", amf::AMFVariant(pkt.pos));
        //pkt_buffer->SetProperty(L"FFMPEG:convergence_duration", amf::AMFVariant(pkt.convergence_duration));

        AMF_RESULT err = decoder_->SubmitInput(pkt_buffer);
        //BL_LOG("  %d=SubmitInput(pkt_buffer)\n", err);
        if (AMF_OK==err) {
            if (0==framesLoaded_ && AMF_OK==decoder_->QueryOutput(&frame_) && frame_) {
                framesLoaded_ = 1;
            }
        }
        else if (AMF_NEED_MORE_INPUT==err) {
            // it's ok, just put more packets
        }
        else if (AMF_INPUT_FULL==err || AMF_DECODER_NO_FREE_SURFACES==err) {
/*
            TO-DO : double check!!!

            Mikhail 2017/08/20 :
            Handling of AMF_INPUT_FULL || AMF_DECODER_NO_FREE_SURFACES is incorrect. Since you do not
            call QueryOutput when you get one of these errors you will not clear hardware queue and
            next attempt resubmit buffer will cause the same error. if you are calling SubmitInput()
            and QueryOutput in the same thread when you get one of these errors you should sleep(),
            QueryOutput and resubmit the same buffer.
*/
            amf_sleep(1); // queue is full
#if 1
            if (0==framesLoaded_) {
                for (int i=0; i<10; ++i) {
                    if (AMF_OK==decoder_->QueryOutput(&frame_) && frame_) {
                        framesLoaded_ = 1;
                        err = decoder_->SubmitInput(pkt_buffer); // resend packet
                        return !(AMF_INPUT_FULL==err || AMF_DECODER_NO_FREE_SURFACES==err);
                    }
                    amf_sleep(1);
                }
            }
#endif
            return false; // make caller resend this packet
        }
        else {
            BL_ERR("** [what?] amf error(%d)!!!\n", err);
            // return false;
        }

        return true;
    }
    bool receive_frame(uint8_t* nv12, int ww, int hh) {
        assert(nv12 && ww==width_ && hh==height_);
        if (nv12 && 0<framesLoaded_ && frame_) {
            // convert to system memory. takes 10-15ms for a 4096x4096 frame on RX480
            int err = frame_->Convert(amf::AMF_MEMORY_HOST);

            //BL_LOG("[receive_frame] QueryOutput() pts:%lld duration:%lld\n", frame_->GetPts(), frame_->GetDuration());

            //
            if (AMF_OK!=err) {
                ///???
            }
            //

            //
            // to do : must scale to width_ x height_
            //

            //
            // [2017/08/16] message from AMD Mikhail Mironov....
            // "AMF operates on pointers, not on objects. Conversion from AMFData to
            //  AMFSurface is just a call to QueryInterface() function, not a copy.
            //  So use AMFSurfacePtr surface(data); it is in expensive."
            //
            // pack nv12 pixels take 3-5 ms
            amf::AMFSurfacePtr surface(frame_);
            amf::AMFPlane* plane = surface->GetPlane(amf::AMF_PLANE_Y);
            amf_int32 pixelSize = plane->GetPixelSizeInBytes(); // 1
            amf_int32 height    = plane->GetHeight();
            amf_int32 pitchH    = plane->GetHPitch();
            amf_int32 line_size = pixelSize*plane->GetWidth();
            uint8_t const* s = ((uint8_t const*)(plane->GetNative())) + pitchH*plane->GetOffsetY() + pixelSize*plane->GetOffsetX();
            uint8_t* d = nv12;
            if (pitchH==ww) {
                memcpy(d, s, line_size*height);
            }
            else {
                for (amf_int32 y=0; y<height; ++y) {
                    memcpy(d, s, line_size);
                    d += ww; s += pitchH;
                }
            }

            // UV plane
            plane     = surface->GetPlane(amf::AMF_PLANE_UV);
            pixelSize = plane->GetPixelSizeInBytes(); // 2
            height    = plane->GetHeight();
            pitchH    = plane->GetHPitch();
            line_size = pixelSize*plane->GetWidth();
            s = ((uint8_t const*)(plane->GetNative())) + pitchH*plane->GetOffsetY() + pixelSize*plane->GetOffsetX();
            d = nv12 + ww*hh;
            if (pitchH==ww) {
                memcpy(d, s, line_size*height);
            }
            else {
                for (amf_int32 y=0; y<height; ++y) {
                    memcpy(d, s, line_size);
                    d += ww; s += pitchH;
                }
            }

            --framesLoaded_;
            if (AMF_OK==decoder_->QueryOutput(&frame_) && frame_) {
                ++framesLoaded_;
            }
            return true;
        }
        return false;
    }
    bool discard_frame() {
        if (0<framesLoaded_ || 
            AMF_OK==decoder_->QueryOutput(&frame_) && frame_) {
            framesLoaded_ = 0;
            return true;
        }
        return false;
    }
};

//
// nVidia cuvid decoder. reference ffmpeg/libavcodec/cuvid.c
class CUVIDDecoder : public VideoDecoderImp
{
    enum { MAX_CUVID_PARSED_FRAMES = 32 };
    CUVIDPARSERDISPINFO cuvidParsedFrames_[MAX_CUVID_PARSED_FRAMES];
    mutable std::mutex  mutex_;
    CUVIDEOFORMATEX     cuvidFmt_;
    CUvideodecoder      cuvidDecoder_;
    CUvideoparser       cuvidParser_;
    CUcontext           cuCtx_;
    AVBSFContext*       bsfCtx_;
    void*               hostPtr_;
    int                 hostPtrSize_;
    int                 numSurfaces_;
    int                 width_, height_;
    int                 framePut_, frameGet_;

    // called before decoding frames and/or whenever there is a format change
    static int CUDAAPI sVideoSequenceCallback_(void* opaque, CUVIDEOFORMAT* fmt) {
        return opaque && fmt && ((CUVIDDecoder*)opaque)->CreateDecoder_(fmt);
    }
    // called when a picture is ready to be decoded (decode order)
    static int CUDAAPI sPictureDecodeCallback_(void* opaque, CUVIDPICPARAMS* pp) {
        return opaque && pp &&
            CUDA_SUCCESS==cuvidDecodePicture(((CUVIDDecoder*)opaque)->cuvidDecoder_, pp);
    }
    // called whenever a picture is ready to be displayed (display order)
    static int CUDAAPI sPictureDisplayCallback_(void* opaque, CUVIDPARSERDISPINFO* pdi) {
        return opaque && pdi && ((CUVIDDecoder*)opaque)->PictureDisplay_(pdi);
    }

    bool CreateDecoder_(CUVIDEOFORMAT* fmt) {
        assert(fmt && cudaVideoChromaFormat_420==fmt->chroma_format);
        CUVIDEOFORMAT& format = cuvidFmt_.format;
        format.frame_rate               = fmt->frame_rate;
        format.bitrate                  = fmt->bitrate;
        format.display_aspect_ratio     = fmt->display_aspect_ratio;
        format.video_signal_description = fmt->video_signal_description;
        if (cuvidDecoder_) {
            if (format.coded_width==fmt->coded_width && format.coded_height==fmt->coded_height &&
                format.codec==fmt->codec &&
                format.chroma_format==fmt->chroma_format &&
                format.progressive_sequence==fmt->progressive_sequence &&
                format.bit_depth_luma_minus8==fmt->bit_depth_luma_minus8 &&
                format.bit_depth_chroma_minus8==fmt->bit_depth_chroma_minus8 &&
                format.display_area.left==fmt->display_area.left &&
                format.display_area.top==fmt->display_area.top &&
                format.display_area.right==fmt->display_area.right &&
                format.display_area.bottom==fmt->display_area.bottom) {
                return true;
            }

            // reset
            cuvidDestroyDecoder(cuvidDecoder_);
            cuvidDecoder_ = NULL;
        }

        // reference cuvid_handle_video_sequence(...) ffmpeg/libavcodec/cuvid.c
        CUVIDDECODECREATEINFO dci;
        memset(&dci, 0, sizeof(CUVIDDECODECREATEINFO));
        dci.ulWidth             = fmt->coded_width;
        dci.ulHeight            = fmt->coded_height;
        dci.ulNumDecodeSurfaces = numSurfaces_;
        dci.CodecType           = fmt->codec;
        dci.ChromaFormat        = fmt->chroma_format;

        // taken from NV Codec SDK samples. FFmpeg always take cudaVideoCreate_PreferCUVID.
        // but it may be that ffmpeg don't take JPEG. By disabling JPEG HW support,
        // it's safe.
        dci.ulCreationFlags = cudaVideoCreate_PreferCUVID; // prefer cuvid
//        if (cudaVideoCodec_JPEG==dci.CodecType) {
//            dci.ulCreationFlags = cudaVideoCreate_PreferCUDA;
//        }

        dci.bitDepthMinus8      = fmt->bit_depth_luma_minus8; // The value "BitDepth minus 8"
        dci.ulIntraDecodeOnly   = 0; // Set 1 only if video has all intra frames (default value is 0).
                                     // This will optimize video memory for Intra frames only decoding

        dci.display_area.left   = (short) fmt->display_area.left;
        dci.display_area.top    = (short) fmt->display_area.top;
        dci.display_area.right  = (short) fmt->display_area.right;
        dci.display_area.bottom = (short) fmt->display_area.bottom;

        // output format
        dci.OutputFormat        = cudaVideoSurfaceFormat_NV12;
#ifdef CUVID_ENABLE_HDR_OUTPUT
        if (dci.bitDepthMinus8)
            dci.OutputFormat    = cudaVideoSurfaceFormat_P016;
#endif
        // deinterlacing
        dci.DeinterlaceMode     = cudaVideoDeinterlaceMode_Weave;
        if (0==fmt->progressive_sequence) {
            // cudaVideoDeinterlaceMode_Adaptive needs more video memory than other DImodes.
            dci.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;
        }

        // target width/height must be multiple of 2
        assert(0==(1&width_) && 0==(1&height_));
        dci.ulTargetWidth       = width_;
        dci.ulTargetHeight      = height_;
        dci.ulNumOutputSurfaces = 1; // ffmpeg/libavcodec/cuvid.c

        // context lock used for synchronizing ownership of the cuda context.
        // needed for cudaVideoCreate_PreferCUDA decode???
        dci.vidLock = NULL;
//        if (cudaVideoCreate_PreferCUDA==dci.ulCreationFlags) {
//            cuvidCtxLockCreate(&cuvidLock_, cuCtx_);
//            dci.vidLock = cuvidLock_;
//        }

        // 1:1 aspect ratio conversion, depends on scaled resolution, ffmpeg/libavcodec/cuvid.c
        dci.target_rect.left   = dci.target_rect.top = 0;
        dci.target_rect.right  = (short) dci.ulTargetWidth;
        dci.target_rect.bottom = (short) dci.ulTargetHeight;

        int err = cuvidCreateDecoder(&cuvidDecoder_, &dci);
        if (CUDA_SUCCESS==err && NULL!=cuvidDecoder_) {
            format.codec                   = fmt->codec;
            format.progressive_sequence    = fmt->progressive_sequence;
            format.bit_depth_luma_minus8   = fmt->bit_depth_luma_minus8;
            format.bit_depth_chroma_minus8 = fmt->bit_depth_chroma_minus8;
            format.coded_width             = fmt->coded_width;
            format.coded_height            = fmt->coded_height;
            format.display_area            = fmt->display_area;
            format.chroma_format           = fmt->chroma_format;
        }
        else {
            BL_LOG("** cuvidCreateDecoder() failed(%d)!\n", err);
        }

        return (NULL!=cuvidDecoder_);
    }
    bool PictureDisplay_(CUVIDPARSERDISPINFO* dinfo) {
        std::lock_guard<std::mutex> lock(mutex_);
        cuvidParsedFrames_[(framePut_++)%MAX_CUVID_PARSED_FRAMES] = *dinfo;
        if ((frameGet_+numSurfaces_)<framePut_) {
            int const next_get = framePut_ - numSurfaces_;
            BL_LOG("** CUVIDDecoder::PictureDisplay_() : drop frames Pictures ID = {");
            for (int i=frameGet_; i<next_get; ++i) {
                 CUVIDPARSERDISPINFO& drop = cuvidParsedFrames_[i%MAX_CUVID_PARSED_FRAMES];
                 BL_LOG(" #%d", drop.picture_index);
            }
            BL_LOG(" }, total=%d\n", next_get-frameGet_);
            frameGet_ = next_get;
        }
        return true;
    }

    bool MapFrame_(uint8* frame, CUVIDPARSERDISPINFO const& info) {
        CUVIDPROCPARAMS params;
        memset(&params, 0, sizeof(CUVIDPROCPARAMS));
        params.progressive_frame = info.progressive_frame;
        params.top_field_first = info.top_field_first;
        params.unpaired_field = (info.repeat_first_field<0);
        CUdeviceptr cuDevPtr = 0;
        unsigned int pitch = 0;

//#define TRACK_MEMCPY_D_TO_H_TIME
#ifdef TRACK_MEMCPY_D_TO_H_TIME
        static int scounter = 0;
        static int64_t stime = 0;
        int64_t t0 = av_gettime();
#endif
        // map video frame
        int err = cuvidMapVideoFrame(cuvidDecoder_, info.picture_index, &cuDevPtr, &pitch, &params);
        if (CUDA_SUCCESS==err) {
            int const scan_lines = height_ + height_/2;
            int const data_size = pitch*scan_lines;
            if (NULL==hostPtr_ || hostPtrSize_<data_size) {
                if (NULL!=hostPtr_) {
                    cuMemFreeHost(hostPtr_);
                    hostPtr_ = NULL;
                }
                hostPtrSize_ = 0;
                if (CUDA_SUCCESS==cuMemAllocHost(&hostPtr_, data_size)) {
                    hostPtrSize_ = data_size;
                }
            }

            bool const got_frame = (NULL!=hostPtr_) && (CUDA_SUCCESS==cuMemcpyDtoH(hostPtr_, cuDevPtr, data_size));

            // unmap video frame
            cuvidUnmapVideoFrame(cuvidDecoder_, cuDevPtr);

            if (got_frame) {
                if ((int)pitch>width_) {
                    uint8* d = (uint8*) frame;
                    uint8 const* s = (uint8 const*) hostPtr_;
                    for (int i=0; i<scan_lines; ++i) {
                        memcpy(d, s, width_);
                        d+=width_; s+=pitch;
                    }
                }
                else {
                    assert((int)pitch==width_);
                    memcpy(frame, hostPtr_, data_size);
                }
            }

#ifdef TRACK_MEMCPY_D_TO_H_TIME
            stime += av_gettime() - t0;
            if (++scounter>100) {
                BL_LOG("** cuMemcpyDtoH() : %lldus(avg)\n", stime/100);
                stime = scounter = 0;
            }
#endif
            return got_frame;
        }

        BL_LOG("** cuvidMapVideoFrame() failed(%d)\n", err);
        return false;
    }

    BL_NO_DEFAULT_CTOR(CUVIDDecoder);
    BL_NO_COPY_ALLOW(CUVIDDecoder);

public:
    CUVIDDecoder(AVCodecID codec):VideoDecoderImp(codec),
        mutex_(),cuvidDecoder_(NULL),cuvidParser_(NULL),cuCtx_(NULL),bsfCtx_(NULL),
        hostPtr_(NULL),hostPtrSize_(0),
        numSurfaces_(0),width_(0),height_(0),framePut_(0),frameGet_(0) {
        memset(&cuvidParsedFrames_, 0, sizeof(cuvidParsedFrames_));
        memset(&cuvidFmt_, 0, sizeof(cuvidFmt_));
    }

    ~CUVIDDecoder() { Destroy(); }

    void Destroy() {
        if (bsfCtx_) {
            av_bsf_free(&bsfCtx_);
            bsfCtx_ = NULL;
        }
        if (cuvidParser_) {
            cuvidDestroyVideoParser(cuvidParser_);
            cuvidParser_ = NULL;
        }
        if (cuvidDecoder_) {
            cuvidDestroyDecoder(cuvidDecoder_);
            cuvidDecoder_ = NULL;
        }

        cuCtx_ = NULL;

        if (hostPtr_) {
            cuMemFreeHost(hostPtr_);
            hostPtr_ = NULL;
        }
        memset(&cuvidParsedFrames_, 0, sizeof(cuvidParsedFrames_));
        memset(&cuvidFmt_, 0, sizeof(cuvidFmt_));
        hostPtrSize_ = numSurfaces_ = width_ = height_ = framePut_ = frameGet_ = 0;
    }

    // identity
    char const* Name() const { return "NVDEC (GPU)"; }

    bool Init(CUcontext cuCtx, AVStream const* stream, int width, int height) {
        assert(NULL==cuCtx_ && stream && stream->codecpar && stream->codecpar->codec_id==CodecID());
        assert(width && height && 0==(1&width) && 0==(1&height));// target width/height must be multiple of 2
        Destroy();

        if (NULL==cuCtx || NULL==stream || NULL==stream->codecpar || 0!=(1&width) || 0!=(1&height)) {
            return false;
        }

        AVCodecContext* avctx = stream->codec;
        AVCodecParameters* codecpar = stream->codecpar;
        cudaVideoCodec const codecType = getCuvidCodec(codecpar->codec_id);
        if (codecType>=cudaVideoCodec_NumCodecs) {
            return false;
        }

        // setup H264/HEVC bit stream filter and sequence header data
        char const* bsf_name = NULL;
        if (cudaVideoCodec_H264==codecType) {
            bsf_name = "h264_mp4toannexb";
        }
        else if (cudaVideoCodec_HEVC==codecType) {
            bsf_name = "hevc_mp4toannexb";
        }
        else {
            //
            // more...?
            //
            AVBitStreamFilter* bsf = NULL;
            while (NULL!=(bsf=av_bitstream_filter_next(bsf))) {
                for (AVCodecID const* codec_id=bsf->codec_ids; codec_id && AV_CODEC_ID_NONE!=*codec_id; ++codec_id) {
                    if (codecpar->codec_id==*codec_id && AVMEDIA_TYPE_VIDEO==avcodec_get_type(*codec_id)) {
                        BL_LOG("** bit stream filter - name:%s codec:%s???\n", bsf->name, avcodec_get_name(*codec_id));
                    }
                }
            }
        }

        if (NULL!=bsf_name) {
            AVBitStreamFilter const* bsf = av_bsf_get_by_name(bsf_name);
            if (NULL==bsf || 0!=av_bsf_alloc(bsf, &bsfCtx_) ||
                0!=avcodec_parameters_copy(bsfCtx_->par_in, codecpar) ||
                0!=av_bsf_init(bsfCtx_)) {
                if (bsfCtx_) {
                    av_bsf_free(&bsfCtx_);
                    bsfCtx_ = NULL;
                }
                BL_LOG("** CUVIDDecoder::Init() - Failed to init AVBSFContext\n");
                return false;
            }

            if (bsfCtx_->par_out->extradata_size<sizeof(cuvidFmt_.raw_seqhdr_data)) {
                cuvidFmt_.format.seqhdr_data_length = bsfCtx_->par_out->extradata_size;
            }
            else { // error!?
                cuvidFmt_.format.seqhdr_data_length = sizeof(cuvidFmt_.raw_seqhdr_data);
            }
            memcpy(cuvidFmt_.raw_seqhdr_data, bsfCtx_->par_out->extradata, cuvidFmt_.format.seqhdr_data_length);
        }
        else if (codecpar->extradata_size>0 && codecpar->extradata) {
            if (codecpar->extradata_size<sizeof(cuvidFmt_.raw_seqhdr_data)) {
                cuvidFmt_.format.seqhdr_data_length = codecpar->extradata_size;
            }
            else { // error!?
                cuvidFmt_.format.seqhdr_data_length = sizeof(cuvidFmt_.raw_seqhdr_data);
            }
            memcpy(cuvidFmt_.raw_seqhdr_data, codecpar->extradata, cuvidFmt_.format.seqhdr_data_length);
        }

        if (CUDA_SUCCESS==cuCtxPushCurrent(cuCtx)) {
            int const coded_width  = avctx->coded_width;
            int const coded_height = avctx->coded_height;

            // #(surfaces)
            numSurfaces_ = cuvidGetNumSurfaces(codecType, coded_width, coded_height);
            if (numSurfaces_>MAX_CUVID_PARSED_FRAMES)
                numSurfaces_ = MAX_CUVID_PARSED_FRAMES;

            // target size
            width_ = width;
            height_ = height;

            // build format
            CUVIDEOFORMAT fmt;
            memset(&fmt, 0, sizeof(fmt));
            fmt.codec                   = codecType;
            fmt.frame_rate.numerator    = stream->avg_frame_rate.num;
            fmt.frame_rate.denominator  = stream->avg_frame_rate.den;
            fmt.progressive_sequence    = (AV_FIELD_PROGRESSIVE==codecpar->field_order) || (AV_FIELD_UNKNOWN==codecpar->field_order);
            fmt.bit_depth_luma_minus8   = 0;
            switch (avctx->pix_fmt)
            {
            // planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples)
            case AV_PIX_FMT_YUV420P10BE: // big-endian
            case AV_PIX_FMT_YUV420P10LE: // little-endian
                fmt.bit_depth_luma_minus8 = 2; // 10 bits
                break;

            // planar YUV 4:2:0, 18bpp, (1 Cr & Cb sample per 2x2 Y samples)
            case AV_PIX_FMT_YUV420P12BE: // big-endian
            case AV_PIX_FMT_YUV420P12LE: // little-endian
                fmt.bit_depth_luma_minus8 = 4; // 12 bits
                break;

            default:
                break;
            }
            fmt.bit_depth_chroma_minus8 = fmt.bit_depth_luma_minus8;
            fmt.coded_width             = coded_width;
            fmt.coded_height            = coded_height;
            fmt.display_area.left       = 0;
            fmt.display_area.top        = 0;
            fmt.display_area.right      = codecpar->width;
            fmt.display_area.bottom     = codecpar->height;
            fmt.chroma_format           = cudaVideoChromaFormat_420;
            fmt.bitrate                 = 0; // video bitrate (bps, 0=unknown)
            fmt.display_aspect_ratio.x  = codecpar->width;
            fmt.display_aspect_ratio.y  = codecpar->height;
            // fmt.video_signal_description to be determined
            //fmt.video_signal_description.video_format           = 0; // bitfield[3]
            //fmt.video_signal_description.video_full_range_flag  = 0; // bitfield[1]
            //fmt.video_signal_description.reserved_zero_bits     = 0; // bitfield[4]
            //fmt.video_signal_description.color_primaries          = (unsigned char) avctx->color_primaries;
            //fmt.video_signal_description.transfer_characteristics = (unsigned char) avctx->color_trc;
            //fmt.video_signal_description.matrix_coefficients      = (unsigned char) avctx->colorspace;

            if (CreateDecoder_(&fmt)) {
                // cuvid parser params struct
                CUVIDPARSERPARAMS pps;
                memset(&pps, 0, sizeof(pps));
                pps.CodecType              = codecType;
                pps.ulMaxNumDecodeSurfaces = numSurfaces_;

                // Timestamp units in Hz (0=default=10000000Hz) 
                //pps.ulClockRate         = 0;

                // Error threshold (0-100) for calling pfnDecodePicture (100=always call
                // pfnDecodePicture even if picture bitstream is fully corrupted)
                //pps.ulErrorThreshold    = 0;

                // Max display queue delay (improves pipelining of decode with display)
                // 0=no delay (recommended values: 2..4)
                pps.ulMaxDisplayDelay   = 1;
                pps.pUserData           = this;
                pps.pfnSequenceCallback = sVideoSequenceCallback_;
                pps.pfnDecodePicture    = sPictureDecodeCallback_;
                pps.pfnDisplayPicture   = sPictureDisplayCallback_;
                pps.pExtVideoInfo       = &cuvidFmt_; // [Optional] sequence header data from system layer
                int err = cuvidCreateVideoParser(&cuvidParser_, &pps);
                if (CUDA_SUCCESS==err && NULL!=cuvidParser_) {
                    // first packet?
                    if (cuvidFmt_.format.seqhdr_data_length>0) {
                        CUVIDSOURCEDATAPACKET cupkt;
                        cupkt.flags = 0;
                        cupkt.payload_size = cuvidFmt_.format.seqhdr_data_length;
                        cupkt.payload = cuvidFmt_.raw_seqhdr_data;
                        cupkt.timestamp = 0;
                        err = cuvidParseVideoData(cuvidParser_, &cupkt);
                        if (CUDA_SUCCESS!=err) {
                            BL_LOG("** CUVIDDecoder::Init() - failed to parse header data(%d)\n", err);
                            cuvidDestroyVideoParser(cuvidParser_);
                            cuvidParser_ = NULL;
                        }
                    }

                    ////////////////////////////////////////////
                    // host memory... should be removed soon...
                    ////////////////////////////////////////////
                    if (NULL!=cuvidParser_) {
                        // alloc memory (1KB boundary)
                        // width=640  -> pitch=1024
                        // width=1280 -> pitch=1536
                        // width=2304 -> pitch=2560
                        // boundary could be 256 or 512
                        int const pitch = (width+1023)&~1023;
                        int const size  = pitch*(height + height/2);
                        if (NULL==hostPtr_ || hostPtrSize_<size) {
                            if (NULL!=hostPtr_) {
                                cuMemFreeHost(hostPtr_);
                            }
                            hostPtr_ = NULL;
                            hostPtrSize_ = 0;
                            if (CUDA_SUCCESS==cuMemAllocHost(&hostPtr_, size)) {
                                hostPtrSize_ = size;
                            }
                            else {
                                cuvidDestroyVideoParser(cuvidParser_);
                                cuvidParser_ = NULL;
                            }
                        }
                    }

                    if (NULL!=cuvidParser_) {
                        cuCtx_ = cuCtx; // final succeed
                    }
                    else {
                        cuvidDestroyDecoder(cuvidDecoder_);
                        cuvidDecoder_ = NULL;
                    }
                }
                else {
                    BL_LOG("** CUVIDDecoder::Init() - failed to create cuvid parser(%d)\n", err);
                }
            }

            cuCtxPopCurrent(NULL);

            if (NULL!=cuCtx_) {
                return true;
            }
        }

        Destroy();
        return false;
    }

    bool Resume() {
        std::lock_guard<std::mutex> lock(mutex_);
        framePut_ = frameGet_ = 0;
        return cuCtx_ && (CUDA_SUCCESS==cuCtxPushCurrent(cuCtx_));
    }
    bool Pause() {
        std::lock_guard<std::mutex> lock(mutex_);
        framePut_ = frameGet_ = 0;
        CUcontext dummy = NULL;
        return cuCtx_ && (CUDA_SUCCESS==cuCtxPopCurrent(&dummy)) && dummy==cuCtx_;
    }
    void Flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        framePut_ = frameGet_ = 0;
        CUcontext ctx_cur = NULL;
        if (cuCtx_ && CUDA_SUCCESS==cuCtxGetCurrent(&ctx_cur)) {
            assert(cuvidParser_);
            //assert(cuvidDecoder_);
#if 0
            BL_LOG("** CUVIDDecoder::Flush() - cuCtx_(0x%p) cuvidParser_(0x%p) cuvidDecoder_(0x%p)\n",
                   cuCtx_, cuvidParser_, cuvidDecoder_);
#endif
            bool pop_ctx = false;
            if (ctx_cur!=cuCtx_) {
                if (CUDA_SUCCESS==cuCtxPushCurrent(cuCtx_)) {
                    pop_ctx = true;
                }
                else {
                    BL_ERR("** [Fatal] CUVIDDecoder::Flush() - failed to push current CUDA context(%p)!!!\n", cuCtx_);
                    return; // failed!?
                }
            }
#if 0
            //
            // to send a discontinuity packet does not work!
            //
            CUVIDSOURCEDATAPACKET cupkt;
            cupkt.flags = CUVID_PKT_DISCONTINUITY;
            cupkt.payload_size = 0;
            cupkt.payload = NULL;
            cupkt.timestamp = 0;
            CUresult err = cuvidParseVideoData(cuvidParser_, &cupkt);
            if (CUDA_SUCCESS!=err) {
                BL_LOG("** cuvidParseVideoData(CUVID_PKT_DISCONTINUITY) error(%d)\n", err);
            }
#else
            //
            // re-build video parser...
            cuvidDestroyVideoParser(cuvidParser_);
            cuvidParser_ = NULL;

            CUVIDPARSERPARAMS pps;
            memset(&pps, 0, sizeof(pps));
            pps.CodecType              = cuvidFmt_.format.codec;
            pps.ulMaxNumDecodeSurfaces = numSurfaces_;

            // Timestamp units in Hz (0=default=10000000Hz) 
            //pps.ulClockRate         = 0;

            // Error threshold (0-100) for calling pfnDecodePicture (100=always call
            // pfnDecodePicture even if picture bitstream is fully corrupted)
            //pps.ulErrorThreshold    = 0;

            // Max display queue delay (improves pipelining of decode with display)
            // 0=no delay (recommended values: 2..4)
            pps.ulMaxDisplayDelay   = 1;
            pps.pUserData           = this;
            pps.pfnSequenceCallback = sVideoSequenceCallback_;
            pps.pfnDecodePicture    = sPictureDecodeCallback_;
            pps.pfnDisplayPicture   = sPictureDisplayCallback_;
            pps.pExtVideoInfo       = &cuvidFmt_; // [Optional] sequence header data from system layer
            if (CUDA_SUCCESS==cuvidCreateVideoParser(&cuvidParser_, &pps)) {
            }
            else {
                BL_ERR("** [Fatal] CUVIDDecoder::Flush() - failed to create video parser!!!\n");
            }
/*
            // decoder is fine...
            if (cuvidDecoder_) {
                cuvidDestroyDecoder(cuvidDecoder_);
                cuvidDecoder_ = NULL;
            }
*/
#endif
            if (pop_ctx) {
                cuCtxPopCurrent(&ctx_cur);
            }
        }
    }
    bool can_send_packet() {
        std::lock_guard<std::mutex> lock(mutex_);
        return framePut_<(frameGet_+4); // don't read too much
        //return framePut_<(frameGet_+numSurfaces_/2);
    }
    bool can_receive_frame(int64_t& pts, int64_t& dur) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frameGet_<framePut_) {
            pts = cuvidParsedFrames_[frameGet_%MAX_CUVID_PARSED_FRAMES].timestamp;
            dur = AV_NOPTS_VALUE; // unknown!? this does no harms...
            return true;
        }
        return false;
    }
    bool send_packet(AVPacket const& pkt) {
        if (cuvidParser_ && pkt.size>0 /*&& CUDA_SUCCESS==cuCtxPushCurrent(cuda_ctx_)*/) {
            CUVIDSOURCEDATAPACKET cupkt;
            //
            // CUVID_PKT_ENDOFSTREAM   = 0x01,   /**< Set when this is the last packet for this stream  */
            // CUVID_PKT_TIMESTAMP     = 0x02,   /**< Timestamp is valid                                */
            // CUVID_PKT_DISCONTINUITY = 0x04,   /**< Set when a discontinuity has to be signalled      */
            // CUVID_PKT_ENDOFPICTURE  = 0x08,   /**< Set when the packet contains exactly one frame    */
            //
            if (bsfCtx_) {
                AVPacket filtered_packet = { 0 };
#if 0
                AVPacket filter_packet = { 0 }; // really need this one?
                av_packet_ref(&filter_packet, &pkt);
                av_bsf_send_packet(bsfCtx_, &filter_packet);
#else
                // to const cast is cheating...
                av_bsf_send_packet(bsfCtx_, const_cast<AVPacket*>(&pkt));
#endif
                av_bsf_receive_packet(bsfCtx_, &filtered_packet);

                if (filtered_packet.size) {
                    cupkt.payload_size = filtered_packet.size;
                    cupkt.payload = filtered_packet.data;
                    if (filtered_packet.pts != AV_NOPTS_VALUE) {
                        cupkt.timestamp = filtered_packet.pts;
                        cupkt.flags = CUVID_PKT_TIMESTAMP;
                    }
                }
                else {
                    cupkt.payload_size = 0;
                    cupkt.payload = NULL;
                    cupkt.flags = CUVID_PKT_ENDOFSTREAM;
                }

                CUresult err = cuvidParseVideoData(cuvidParser_, &cupkt);
                if (CUDA_SUCCESS!=err) {
                    BL_LOG("** cuda error(%d)\n", err);
                }

                av_packet_unref(&filtered_packet);
            }
            else {
                cupkt.payload_size = pkt.size;
                cupkt.payload = pkt.data;
                if (AV_NOPTS_VALUE!=pkt.pts) {
                    cupkt.timestamp = pkt.pts;
                    cupkt.flags = CUVID_PKT_TIMESTAMP;
                }

                CUresult err = cuvidParseVideoData(cuvidParser_, &cupkt);
                if (CUDA_SUCCESS!=err) {
                    BL_LOG("** cuda error=%d\n", err);
                }
            }

//            CUcontext dummy(NULL);
//            cuCtxPopCurrent(&dummy);
        }
        return true;
    }
    bool receive_frame(uint8_t* nv12, int width, int height) {
        assert(nv12 && width==width_ && height==height_);
        std::lock_guard<std::mutex> lock(mutex_);
        if (frameGet_<framePut_) {
            int const id = (frameGet_++)%MAX_CUVID_PARSED_FRAMES;
            if (width==width_ && height==height_ && MapFrame_(nv12, cuvidParsedFrames_[id])) {
                //BL_LOG("[receive_frame] MapFrame_() pts:%lld\n", cuvidParsedFrames_[id].timestamp);
                return true;
            }
        }
        return false;
    }
    bool discard_frame() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (frameGet_<framePut_) {
            ++frameGet_;
            return true;
        }
        return false;
    }
};

//---------------------------------------------------------------------------------------
VideoDecoderImp* VideoDecoderImp::New(AVStream const* stream, int width, int height)
{
    if (NULL!=stream && NULL!=stream->codecpar && AVMEDIA_TYPE_VIDEO==stream->codecpar->codec_type) {
        AVCodecParameters* codecpar = stream->codecpar;
        AVCodecID const codec_id = codecpar->codec_id;

        std::lock_guard<std::mutex> lock(gpuCtx.mutex);

        //
        // (to supprt hevc, you'll need Maxwell(GM206) or Pascal GP10x)
        //   GTX 1080Ti/Titan X/Titan Xp = GP102
        //   GTX 1070/1080               = GP104
        //   GTX 1060                    = GP106
        //   GTX 1050                    = GP107
        //   GTX 1030                    = GP108
        //
        //   GTX 950/960                 = GM206 (max size=4096x2304)
        //

        // cuvid
        if (gpuCtx.cudaContext) {
            cudaVideoCodec const nvCodec = getCuvidCodec(codec_id);

            GPUContext::MaxSize const* max_sizes = NULL; gpuCtx.cuvidCaps8;
            switch (stream->codec->pix_fmt)
            {
            // planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples)
            case AV_PIX_FMT_YUV420P10BE: // big-endian
            case AV_PIX_FMT_YUV420P10LE: // little-endian
                max_sizes = gpuCtx.cuvidCaps10; // 10 bits
                break;

            // planar YUV 4:2:0, 18bpp, (1 Cr & Cb sample per 2x2 Y samples)
            case AV_PIX_FMT_YUV420P12BE: // big-endian
            case AV_PIX_FMT_YUV420P12LE: // little-endian
                max_sizes = gpuCtx.cuvidCaps12; // 12 bits
                break;

            case AV_PIX_FMT_YUV420P:
            case AV_PIX_FMT_YUVJ420P: // full scale (JPEG)
                max_sizes = gpuCtx.cuvidCaps8;
                break;

            default:
                BL_LOG("cuvid format :%d\n", (int) stream->codec->pix_fmt);
                break;
            }

            if (nvCodec<cudaVideoCodec_NumCodecs && NULL!=max_sizes &&
                width<=max_sizes[nvCodec].Width &&
                height<=max_sizes[nvCodec].Height) {
                try {
                    CUVIDDecoder* decoder = new CUVIDDecoder(codec_id);
                    if (decoder->Init(gpuCtx.cudaContext, stream, width, height)) {
                        ++gpuCtx.activeDecoders;
                        return decoder;
                    }
                    else {
                        delete decoder;
                    }
                }
                catch (...) {
                }
            }
        }

        // amd amf
        if (gpuCtx.amfContext) {
            // AMD AMF SDK - can AMF resize?
            if (width==codecpar->width && height==codecpar->height) {
                if (NULL!=getAMFCodec(codec_id)) {
                    try {
                        AMFDecoder* decoder = new AMFDecoder(codec_id, width, height);
                        if (decoder->Init(gpuCtx.amfContext, codecpar)) {
                            ++gpuCtx.activeDecoders;
                            return decoder;
                        }
                        else {
                            delete decoder;
                        }
                    }
                    catch(...) {
                    }
                }
            }
        }
    }

    return NULL;
}
//---------------------------------------------------------------------------------------
void VideoDecoderImp::Delete(VideoDecoderImp* decoder)
{
    if (NULL!=decoder) {
        std::lock_guard<std::mutex> lock(gpuCtx.mutex);
        delete decoder;
        --gpuCtx.activeDecoders;
    }
}

//---------------------------------------------------------------------------------------
namespace hwaccel {

int InitContext(void* /*device*/)
{
    std::lock_guard<std::mutex> lock(gpuCtx.mutex);
    if (0==gpuCtx.init && 0==gpuCtx.activeDecoders) {

        // CUDA and NVDECODE
        assert(NULL==gpuCtx.cudaContext);
        int cuda_device_count = 0;
        if (CUDA_SUCCESS==cuInit(0, __CUDA_API_VERSION, NULL) && CUDA_SUCCESS==cuvidInit(0) &&
            NULL!=cuDeviceGetCount && CUDA_SUCCESS==cuDeviceGetCount(&cuda_device_count) && cuda_device_count>0) {

            //
            // Note to make cuvidGetDecoderCaps() works, the cuda context must be created.
            // But cuvidGetDecoderCaps() may be wrong!?
            //

            //
            // Chroma format (cudaVideoChromaFormat)
            // JPEG supports Monochrome, YUV 4:2:0, YUV 4:2:2 and YUV 4:4:4 chroma formats.
            // H264, HEVC, VP9, VP8, VC1, MPEG1, MPEG2 and MPEG4 support YUV 4:2:0 chroma format only.
            //

            // best choice
            int best_device_index = -1;
            int best_major(0), best_minor(0);
            int best_h264_size(0), best_hevc_size(0);

            CUdevice cuda_device = 0;
            CUVIDDECODECAPS cap;
            memset(&cap, 0, sizeof(cap));
            cap.eChromaFormat = cudaVideoChromaFormat_420;
            cap.nBitDepthMinus8 = 0; // 2 for 10-bit, 4 for 12-bit
            for (int i=0; i<cuda_device_count; ++i) {
                int major(0), minor(0), bTCC(0);
                int h264_max_size(0), hevc_max_size(0);
                if (CUDA_SUCCESS==cuDeviceGet(&cuda_device, i) &&
                    CUDA_SUCCESS==cuDeviceComputeCapability(&major, &minor, cuda_device) &&
                    (major>1||(1==major && minor>=1)) &&
                    CUDA_SUCCESS==cuDeviceGetAttribute(&bTCC, CU_DEVICE_ATTRIBUTE_TCC_DRIVER, cuda_device) && 0==bTCC) {
                    CUcontext cudaCtx = NULL;
                    if (CUDA_SUCCESS==cuCtxCreate(&cudaCtx, CU_CTX_SCHED_BLOCKING_SYNC, cuda_device) && cudaCtx) {
                        cap.eCodecType = cudaVideoCodec_H264;
                        if (CUDA_SUCCESS==cuvidGetDecoderCaps(&cap) && cap.bIsSupported) {
                            h264_max_size = cap.nMaxWidth*cap.nMaxHeight; 
                            cap.eCodecType = cudaVideoCodec_HEVC;
                            if (CUDA_SUCCESS==cuvidGetDecoderCaps(&cap) && cap.bIsSupported) {
                                hevc_max_size = cap.nMaxWidth*cap.nMaxHeight;
                            }

                            // is this good?
                            if (best_device_index<0) {
                                best_device_index = i;
                                best_major = major;
                                best_minor = minor;
                                best_h264_size = h264_max_size;
                                best_hevc_size = hevc_max_size;
                            }
                            else if (best_hevc_size<hevc_max_size ||
                                     (best_hevc_size==hevc_max_size && best_h264_size<h264_max_size)) {
                                best_device_index = i;
                                best_major = major;
                                best_minor = minor;
                                best_h264_size = h264_max_size;
                                best_hevc_size = hevc_max_size;
                            }
                        }

                        cuCtxDestroy(cudaCtx);
                    }
                }
            }

            // create best cuda context...
            if (0<=best_device_index && CUDA_SUCCESS==cuDeviceGet(&cuda_device, best_device_index) &&
#if INIT_CUDA_GL
                CUDA_SUCCESS==cuGLCtxCreate(&gpuCtx.cudaContext, CU_CTX_SCHED_BLOCKING_SYNC, cuda_device) && gpuCtx.cudaContext) {
#else
#if INIT_CUDA_D3D9
                CUDA_SUCCESS==cuD3D9CtxCreate(&gpuCtx.cudaContext, &cuda_device, CU_CTX_SCHED_BLOCKING_SYNC, (IDirect3DDevice9*) device) && gpuCtx.cudaContext) {
#else
#if INIT_CUDA_D3D11
                CUDA_SUCCESS==cuD3D11CtxCreate(&gpuCtx.cudaContext, &cuda_device, CU_CTX_SCHED_BLOCKING_SYNC, (ID3D11Device*) device) && gpuCtx.cudaContext) {
#else
                CUDA_SUCCESS==cuCtxCreate(&gpuCtx.cudaContext, CU_CTX_SCHED_BLOCKING_SYNC, cuda_device) && gpuCtx.cudaContext) {
#endif
#endif
#endif

#ifdef BL_AVDECODER_VERBOSE
                char cuda_device_name[256];
                int driver_ver(0), major(0), minor(0);
                cuDeviceGetName(cuda_device_name, sizeof(cuda_device_name), cuda_device);
                cuDeviceComputeCapability(&major, &minor, cuda_device);
                cuDriverGetVersion(&driver_ver);
                BL_LOG("** Hardware info(#%d) : nVidia CUVID | %s | ver.%d.%d | driver:%d\n",
                       best_device_index, cuda_device_name, major, minor, driver_ver);
                char const* codec[cudaVideoCodec_NumCodecs] = {
                    "MPEG1   ", "MPEG2   ", "MPEG4   ", "VC1     ", "H264    ",
                    "JPEG    ", "H264-SVC", "H264-MVC", "HEVC    ", "VP8     ",
                    "VP9     ",
                };
#endif
                //
                // build a quick table
                CUVIDDECODECREATEINFO dci;
                memset(&dci, 0, sizeof(CUVIDDECODECREATEINFO));
                memset(&cap, 0, sizeof(cap));

                // 0 for 8-bit, 2 for 10-bit, 4 for 12-bit
                unsigned int const bit_depth_minus_8[3] = { 0, 2, 4 };
                for (int i=0; i<cudaVideoCodec_NumCodecs; ++i) {
                    GPUContext::MaxSize* max_sizes[3] = {
                        gpuCtx.cuvidCaps8 + i,
                        gpuCtx.cuvidCaps10 + i,
                        gpuCtx.cuvidCaps12 + i
                    };

                    cap.eCodecType = (cudaVideoCodec) i;
                    cap.eChromaFormat = cudaVideoChromaFormat_420;
                    for (int j=0; j<sizeof(bit_depth_minus_8)/sizeof(bit_depth_minus_8[0]); ++j) {
                        cap.nBitDepthMinus8 = bit_depth_minus_8[j];
                        GPUContext::MaxSize* max_size = max_sizes[j];
                        max_size->Width = max_size->Height = 0;
                        if (CUDA_SUCCESS==cuvidGetDecoderCaps(&cap) && cap.bIsSupported) {
                            // confirm again! note cuvidGetDecoderCaps() may lie.
                            // it makes us to create CUvideodecoder here!
                            dci.ulWidth             = cap.nMaxWidth;
                            dci.ulHeight            = cap.nMaxHeight;
                            dci.ulNumDecodeSurfaces = cuvidGetNumSurfaces(cap.eCodecType, cap.nMaxWidth, cap.nMaxHeight);
                            dci.CodecType           = cap.eCodecType;
                            dci.ChromaFormat        = cap.eChromaFormat; // cudaVideoChromaFormat_420
                            dci.ulCreationFlags     = cudaVideoCreate_PreferCUVID;
                            //if (cudaVideoCodec_JPEG==dci.CodecType)
                            //    dci.ulCreationFlags = cudaVideoCreate_PreferCUDA;
                            dci.bitDepthMinus8      = cap.nBitDepthMinus8;
                            dci.ulIntraDecodeOnly   = 0;
                            dci.display_area.left   = dci.display_area.top = 0;
                            dci.display_area.right  = (short) cap.nMaxWidth;
                            dci.display_area.bottom = (short) cap.nMaxHeight;
                            dci.OutputFormat        = cudaVideoSurfaceFormat_NV12;
#ifdef CUVID_ENABLE_HDR_OUTPUT
                            if (cap.nBitDepthMinus8)
                                dci.OutputFormat    = cudaVideoSurfaceFormat_P016;
#endif
                            dci.DeinterlaceMode     = cudaVideoDeinterlaceMode_Weave; // no deinterlacing
                            dci.ulTargetWidth       = cap.nMaxWidth;
                            dci.ulTargetHeight      = cap.nMaxHeight;
                            dci.ulNumOutputSurfaces = 1;
                            dci.vidLock             = NULL;
                            dci.target_rect.left    = dci.target_rect.top = 0;
                            dci.target_rect.right   = (short) cap.nMaxWidth;
                            dci.target_rect.bottom  = (short) cap.nMaxHeight;
                            CUvideodecoder cuvidDecoder = nullptr;
                            if (CUDA_SUCCESS==cuvidCreateDecoder(&cuvidDecoder, &dci) && cuvidDecoder) {
#ifdef BL_AVDECODER_VERBOSE
                                BL_LOG("    %s(%d-bit) %dx%d  %dx%d\n", codec[i], cap.nBitDepthMinus8+8,
                                       cap.nMinWidth, cap.nMinHeight, cap.nMaxWidth, cap.nMaxHeight);
#endif
                                max_size->Width = cap.nMaxWidth;
                                max_size->Height = cap.nMaxHeight;
                                cuvidDestroyDecoder(cuvidDecoder);
                            }
                        }
                    }
                }

                // pop
                cuCtxPopCurrent(NULL);
            }
        }

        //
        // IF CUVID NOT INITED, try AMD AMF...
        // must decoder->Init(amf::AMF_SURFACE_NV12, 3840, 2160) takes that long!?
        assert(NULL==gpuCtx.amfContext);
        if (NULL==gpuCtx.cudaContext && AMF_OK==g_AMFFactory.Init()) {
            bool amf_init = false;
            if (AMF_OK==g_AMFFactory.GetFactory()->CreateContext(&gpuCtx.amfContext) && gpuCtx.amfContext) {
               // try create amf decoder - must communicate with gpu to see if it's really there.
               // (the user may just get a new NVIDIA GPU)
               amf::AMFComponentPtr decoder;
               if (AMF_OK==gpuCtx.amfContext->InitDX9(NULL) &&
                   AMF_OK==g_AMFFactory.GetFactory()->CreateComponent(gpuCtx.amfContext, AMFVideoDecoderUVD_H264_AVC, &decoder)) {
                   if (AMF_OK==decoder->Init(amf::AMF_SURFACE_NV12, 3840, 2160)) {
                       amf_init = true;
                   }
                   decoder->Terminate();
                   decoder = NULL;
               }
            }

            if (!amf_init) {
#ifdef BL_AVDECODER_VERBOSE
                BL_LOG("AMF Failed to initialize\n");
#endif
                gpuCtx.amfContext->Terminate();
                gpuCtx.amfContext = NULL;
                g_AMFFactory.Terminate();
            }
        }

        gpuCtx.init = 1;
        return 0;
    }

    return -1;
}

void DeinitContext()
{
    std::lock_guard<std::mutex> lock(gpuCtx.mutex);
    assert(0!=gpuCtx.init && 0==gpuCtx.activeDecoders);
    if (0!=gpuCtx.init && 0==gpuCtx.activeDecoders) {
        if (gpuCtx.cudaContext) {
            cuCtxDestroy(gpuCtx.cudaContext);
            gpuCtx.cudaContext = NULL;
        }

        if (gpuCtx.amfContext) {
            gpuCtx.amfContext->Terminate();
            gpuCtx.amfContext = NULL;
            g_AMFFactory.Terminate();
        }
        gpuCtx.init = 0;
    }
}

int GPUAccelScore()
{
    std::lock_guard<std::mutex> lock(gpuCtx.mutex);
    if (gpuCtx.cudaContext) {
        return 3;
    }

    if (gpuCtx.amfContext) {
        return 1;
    }

    return 0;
}

} // namespace hwaccel

}}} // namespace mlabs::balai::video