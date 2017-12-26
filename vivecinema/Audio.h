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
 * @file    Audio.h
 * @author  andre chen
 * @history 2017/02/14 created
 *
 */
#ifndef BL_AUDIO_H
#define BL_AUDIO_H

#include "BLFFT.h"
#include "BLArray.h"
#include "BLSphericalHarmonics.h"
#include <functional>
#include <mutex>

namespace mlabs { namespace balai { namespace audio {

// audio sample format
enum AUDIO_FORMAT {
    AUDIO_FORMAT_UNKNOWN = 0,
    AUDIO_FORMAT_U8  = 1,
    AUDIO_FORMAT_S16 = 2,
    AUDIO_FORMAT_S32 = 3,
    AUDIO_FORMAT_F32 = 4,
};

//
// audio technique
// andre : pardon me, it should be more like AUDIO_XXX_FORMAT, but i just ran out of names....
//         so i borrow the "technique" concept from shader effect. like we do use
//         the same word "render" for both video(CG) and audio.
//         (nope, i have no relationship with audio-technica XD)
enum AUDIO_TECHNIQUE {
    AUDIO_TECHNIQUE_DEFAULT, // unknown, probably mono, stereo, 5.1 or 7.1 audio
    AUDIO_TECHNIQUE_AMBIX,   // ACN/SN3D, with or without 2 channels(stereo) headlock
    AUDIO_TECHNIQUE_FUMA,    // Furse-Malham/maxN, 1-3 order, with or without 2 channels headlock
    AUDIO_TECHNIQUE_TBE,     // 8 channels or 10 channels with 2 channels headlock
    // AUDIO_TECHNIQUE_AMBISONIC_ACN_N3D
    // AUDIO_TECHNIQUE_AMBISONIC_SID_N3D, // SID, Single Index Designation
};

//
// The 44.1 kHz sampling rate originated in the late 1970s with PCM adaptors,
// which recorded digital audio on video cassettes,[note 1] notably the
// Sony PCM-1600 (1979) and subsequent models in this series.
// 44.1 kHz audio is widely used, due to this being the sampling rate
// used in Compact Discs(CD).
//
// Human hearing range from 20Hz ~ 20000Hz. By Nyquist¡VShannon sampling theorem,
// the sampling rate for 20000Hz should be at least 40000Hz.
//

//- helpers -----------------------------------------------------------------------------
inline int BytesPerSample(AUDIO_FORMAT fmt) {
    int const values[] = { 0, 1, 2, 4, 4 };
    return (0<fmt && fmt<=AUDIO_FORMAT_F32) ? values[fmt]:0;
}

struct AudioConfig {
    AUDIO_FORMAT Format;
    uint32       SampleRate; // 44100, 48000, 96000, 192000
    uint32       Channels;   // 1(mono), 2(stereo), 4(quad), 6(5.1)

    AudioConfig():Format(AUDIO_FORMAT_UNKNOWN),SampleRate(0),Channels(0) {}
    void Reset() { Format=AUDIO_FORMAT_UNKNOWN; SampleRate=Channels=0; }
    int BytesPerFrame() const { return Channels*BytesPerSample(Format); }
    int BytesPerSecond() const { return SampleRate*BytesPerFrame(); }
    operator bool() const { return AUDIO_FORMAT_UNKNOWN!=Format&&SampleRate>0&&Channels>0; }
    bool operator==(AudioConfig const& b) const {
        return Format==b.Format && SampleRate==b.SampleRate && Channels==b.Channels;
    }
};

//
// wave file - Be cautious! it may easily take few mega bytes memory.
// Use with care!!!
//
// for .amb file, this loads 4, 9, 16 channels only.
// http://www.ambisonic.net/fileformats.html
//
//  3 channel = h = 1st order horizontal
//  4 channel = f = 1st order 3-D
//  5 channel = hh = 2nd order horizontal
//  6 channel = fh = 2nd order horizontal + 1st order height (formerly called 2.5 order)
//  7 channel = hhh = 3rd order horizontal
//  8 channel = fhh = 3rd order horizontal + 1st order height
//  9 channel = ff = 2nd order 3-D
//  11 channel = ffh = 3rd order horizontal + 2nd order height
//  16 channel = fff = 3rd order 3-D
//
// The W channel is attenuated by -3 dB (1/sqrt(2)) for all orders.
//
// Channel order is WXYZRSTUVKLMNOPQ with unused channels omitted.
//
class WavFile
{
    sint16* samples_;
    uint32 totalChannels_;
    uint32 totalFrames_;
    uint32 sampleRate_;
    uint32 playing_;

public:
    WavFile():samples_(NULL),
        totalChannels_(0),totalFrames_(0),sampleRate_(0),playing_(0) {};
    ~WavFile() { Unload(); }

    sint16* SampleData() const { return samples_; }
    uint32 TotalSampleSize() const { return totalFrames_*totalChannels_*2; }
    uint32 Channels() const { return totalChannels_; }
    uint32 TotalFrames() const { return totalFrames_; }
    uint32 SampleRate() const { return sampleRate_; }
    void Rewind() { playing_ = 0; }
    operator bool () const {
        return NULL!=samples_ && 0<totalChannels_ && 0<totalFrames_ && 0<sampleRate_;
    }
    int Duration() const { // in milliseconds
        if (0<totalFrames_ && 0<sampleRate_) {
            return (int) ((int64(totalFrames_)*1000)/sampleRate_);
        }
        return 0;
    }

    bool Load(void const* data, int frames, AudioConfig const& config) {
        return Load(data, frames, config.Format, config.Channels, config.SampleRate);
    }
    bool Load(void const* data, int frames,
              AUDIO_FORMAT fmt, uint32 Channels, uint32 SampleRate);
    bool Load(char const* filename, AUDIO_TECHNIQUE tech=AUDIO_TECHNIQUE_DEFAULT);
    void Unload();
    bool Save(char const* filename) const;
    int Streaming(void* samples, int total_frames, AUDIO_FORMAT fmt, float gain=1.0f);
};

// audio description
struct AudioDesc {
    AUDIO_TECHNIQUE Technique;
    AUDIO_FORMAT    Format;
    uint32          SampleRate;
    uint16          NumChannels;
    uint16          NumTracks;

    // channel or track indices. indices after -1 are all unknown(place -1 to end the list)
    // 16 doesn't mean max channels are supported... when it stores channel indices,
    // it probaly the fuma format it describe. since fuma format has max 16 channels,
    // we are fine. for HOA ambiX all channels are fixed. so 64 channels ambiX is possible.
    // and we will use Indices[16] to index tracks.
    uint8           Indices[16];

    AudioDesc() { Reset(); }
    void Reset() {
        Technique = AUDIO_TECHNIQUE_DEFAULT;
        Format = AUDIO_FORMAT_UNKNOWN;
        SampleRate = NumChannels = NumTracks = 0;
        memset(Indices, 0, sizeof(Indices));
        Indices[0] = 0xff;
    }
};

// audio decoder interface
class IAudioDecoder
{
    BL_NO_COPY_ALLOW(IAudioDecoder);

protected:
    IAudioDecoder() {}

public:
    virtual ~IAudioDecoder() { Finalize(); }

    // TBD
    virtual char const* Description() const = 0;
    virtual bool Initialize(AudioConfig const& config, char const* media) = 0;
    virtual void Finalize() = 0;
    virtual bool CanDo(AudioDesc const& out, AudioDesc const& in) = 0;
    virtual bool Do(AudioDesc const& out, mlabs::balai::math::Matrix3 const& listener,
                    AudioDesc const& in) = 0;
};

//
// [SDL2 Audio] Channel data is interleaved -
//  Stereo(2) : stored in left/right ordering.
//  Quad(4)   : stored in front-left/front-right/rear-left/rear-right order.
//  5.1(6)    : stored in front-left/front-right/center/low-freq/rear-left/rear-right
//              ordering ("low-freq" is the ".1" speaker). 
//
typedef std::function<void(uint8* stream, int size, AudioConfig const& config)> audio_render_fn;

//---------------------------------------------------------------------------------------
class AudioManager
{
    mutable std::mutex mutex_;

    Array<IAudioDecoder*> decoders_;
    IAudioDecoder*        activeDecoder_;

    // audio render function
    audio_render_fn renderAudioData_;

    // audio configs
    AudioConfig default_; // backoff config
    AudioConfig current_; // current config

    // convolvers
    enum { CONVOLUTION_BLOCK_SIZE = 256 };
    math::FFTConvolver foa_SH_HRTF_; // 1st order ambisonic SH HRTF, 4 channels.
    math::FFTConvolver soa_SH_HRTF_; // 2nd order ambisonics SH HRTF, 9 channels.
    math::FFTConvolver toa_SH_HRTF_; // 3rd order ambisonic SH HRTF, 16 channels.
    math::FFTConvolver itu_7_1_HRTF_; // ITU 5.1/7.1 HRTF (no subwoofer), 7*2 channels.

    // Ambisonics sound field rotator
    math::SphericalHarmonics::SHRotateMatrix shRotate_;

    // audio buffer for decoding
    float* buffer_;
    int    bufferSize_;

    bool pause_;

    // prepare...
    void Prepare_() {
        // convolver must reset!!!!
        foa_SH_HRTF_.Reset();
        soa_SH_HRTF_.Reset();
        toa_SH_HRTF_.Reset();
        itu_7_1_HRTF_.Reset();
    }

    bool InitHRTFs_();

    // rotate, and re-arrange audio data in ambiX/TBE way
    bool DecodeAmbisonicHRTF_(void* dst, AUDIO_FORMAT fmt,
                              mlabs::balai::math::Matrix3 const& listener,
                              void const* src, int frames, AudioDesc const& desc, float gain);

    // [deprecated] the naive way... Vive Cinema 0.7.281 submit 2016.12.20
    bool DecodeAmbisonicByProjection_(void* dst, AUDIO_FORMAT dstFmt,
                                      mlabs::balai::math::Matrix3 const& listener,
                                      float const* src, int frames,
                                      AudioDesc const& desc, float gain);
    // audio api
    virtual bool OpenAudioImp_(AudioConfig& result, AudioConfig const& config) = 0;
    virtual void CloseAudioImp_() = 0;
    virtual void PauseImp_(bool pause) = 0;

protected:
    void SetRenderBatchSize_(AudioConfig const& config, int batch_frames) {
        // prepare buffer for decoding...
        if (batch_frames>0) {
            int const size = batch_frames*(16+2)*sizeof(float);
            if (bufferSize_<size) {
                float* new_buffer = (float*) malloc(size);
                if (new_buffer) {
                    free(buffer_);
                    buffer_ = new_buffer;
                    bufferSize_ = size;
                }
            }
        }

        // convolver must respect to sampling rate!!!
        if (48000!=config.SampleRate) { // adjust convolver to match the sample rate
        }
    }
    void RenderCallback_(uint8* data, int size) {
        assert(current_ && /*!pause_ &&*/ renderAudioData_);
        assert(size==((size/current_.BytesPerFrame())*current_.BytesPerFrame()));
        if (nullptr!=renderAudioData_ && !pause_) {
            renderAudioData_(data, size, current_);
        }
        else {
            memset(data, 0, size);
        }
    }
    AudioManager():mutex_(),
        decoders_(32),activeDecoder_(NULL),
        renderAudioData_(nullptr),default_(),current_(),
        foa_SH_HRTF_(),soa_SH_HRTF_(),toa_SH_HRTF_(),itu_7_1_HRTF_(),
        shRotate_(),
        buffer_(NULL),bufferSize_(0),
        pause_(true) {
        bufferSize_ = 2048*(16+2)*sizeof(float);
        buffer_ = (float*) malloc(bufferSize_);
        if (NULL==buffer_) {
            bufferSize_ = 0;
        }
        shRotate_.Reserve(4);
    }
    virtual ~AudioManager() {
        DeInitAudio();
        free(buffer_); buffer_ = NULL;
        bufferSize_ = 0;
    }

public:
    AudioConfig const& GetConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_;
    }
  //  int  BatchSize() const { return batchSize_; } // 0 if unknown
    bool IsPause() const { return pause_; }

    /////////////////////////////////////////////////////////////////////////////////////
    bool LoadDecoders(char const* decoderlib);
    bool SelectDecoder(AUDIO_TECHNIQUE tech, char const* name);
    /////////////////////////////////////////////////////////////////////////////////////

    // default decode method -
    // note : This is not for decoding multiple source data simutaneously!!!
    //        especially for those may involve convolution job.
    //
    // caveat : should use FORMAT_F32 for all non-default techniques.
    //
    int DecodeAudioData(void* dst, AudioConfig const& dst_config,
                        void const* src, int total_frames, AudioDesc const& src_desc,
                        mlabs::balai::math::Matrix3 const& listener, float gain=1.0f);

    // initialize audio, call once.
    bool InitAudio(audio_render_fn rendercallback, AudioConfig configs[]=nullptr, int totals=0) {
        CloseAudio();

        renderAudioData_ = rendercallback;
        if (nullptr!=renderAudioData_) {
            AudioConfig config;
            if (nullptr==configs || totals<1) {
                config.Format = AUDIO_FORMAT_F32;
                config.SampleRate = 48000;
                config.Channels = 2;
                configs = &config; totals = 1;
            }        

            for (int i=0; i<totals; ++i) {
                if (OpenAudioImp_(current_, configs[i])) {
                    if (!default_) {
                        default_ = current_;
                    }
                    assert(current_);
                    InitHRTFs_();
                    return true;
                }
            }
        }

        current_.Reset();
        return false;
    }

    // finish audio
    void DeInitAudio() {
        CloseAudio();
        renderAudioData_ = nullptr;

        //default_.Reset(); please don't!

        // finalize HRTFs
        foa_SH_HRTF_.Finalize();
        soa_SH_HRTF_.Finalize();
        toa_SH_HRTF_.Finalize();
        itu_7_1_HRTF_.Finalize();

        activeDecoder_ = nullptr;
        decoders_.for_each(safe_delete_functor());
        decoders_.cleanup();
    }

    // to change audio config
    bool OpenAudio(AudioConfig& result, AudioConfig const& desired, bool compatible=true) {
        assert(renderAudioData_ && current_ && default_);
        if (!desired || desired==current_) {
            result = current_;
            return desired && result;
        }

        CloseAudio();

        for (AudioConfig attemp=desired; attemp; ) {
            if (OpenAudioImp_(result, attemp)) {
                current_ = result;
                break;
            }
            else if (!compatible || result==attemp) {
                result.Reset();
                return false;
            }
            else {
                attemp = result; // continue...
            }
        }

        if (!current_) {
            if (OpenAudioImp_(result, default_)) {
                current_ = result;
            }
        }

        return (result==desired) || (compatible && result);
    }

    // close audio and reopen later
    void CloseAudio() {
        if (current_) {
            if (!pause_) {
                PauseImp_(true);
            }
            CloseAudioImp_();
            current_.Reset();
        }
        pause_ = true;
    }

    // pause, noneed to submit audio data
    bool Pause() {
        if (!pause_) {
            assert(current_);
            PauseImp_(pause_=true);
            return true;
        }
        return false;
    }

    // ready to send audio data and play the funky music...
    bool Resume() {
        assert(current_);
        if (pause_) {
            Prepare_();
            PauseImp_(pause_=false);
            return true;
        }
        return false;
    }

    //
    // to be implemented...
    static AudioManager& GetInstance();
};

}}}

#endif