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
 * @file    SDLAudio.cpp
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2017/02/14 created
 *
 */
#include "Audio.h"
#include "SDL_audio.h"

// not to cause too frequent audio callbacks
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

//
// SDL input audio formats:(32-bit support from SDL 2.0)
//  AUDIO_S8 : signed 8-bit samples
//  AUDIO_U8 : unsigned 8-bit samples
//  AUDIO_S16LSB : signed 16-bit samples in little-endian byte order
//  AUDIO_S16MSB : signed 16-bit samples in big-endian byte order
//  AUDIO_S16SYS : signed 16-bit samples in native byte order
//     AUDIO_S16 = AUDIO_S16LSB, the SDL advised audio format
//  AUDIO_U16LSB : unsigned 16-bit samples in little-endian byte order
//  AUDIO_U16MSB : unsigned 16-bit samples in big-endian byte order
//  AUDIO_U16SYS : unsigned 16-bit samples in native byte order
//     AUDIO_U16 = AUDIO_U16LSB
//  AUDIO_S32LSB : 32-bit integer samples in little-endian byte order
//  AUDIO_S32MSB : 32-bit integer samples in big-endian byte order
//  AUDIO_S32SYS : 32-bit integer samples in native byte order
//     AUDIO_S32 = AUDIO_S32LSB
//  AUDIO_F32LSB : 32-bit floating point samples in little-endian byte order
//  AUDIO_F32MSB : 32-bit floating point samples in big-endian byte order
//  AUDIO_F32SYS : 32-bit floating point samples in native byte order
//     AUDIO_F32 = AUDIO_F32LSB
// (For Visual Studios IDE, AUDIO_FMT = AUDIO_FMT_SYS = AUDIO_FMTLSB)
//
// As of SDL 2.0, supported channel values are :
// 1 (mono), 2 (stereo), 4 (quad), and 6 (5.1).
//
// Channel data is interleaved -
//  Stereo(2) : stored in left/right ordering.
//  Quad(4)   : stored in front-left/front-right/rear-left/rear-right order.
//  5.1(6)    : stored in front-left/front-right/center/low-freq/rear-left/rear-right
//              ordering ("low-freq" is the ".1" speaker).
//
namespace mlabs { namespace balai { namespace audio {

class SDLAudioManager : public AudioManager
{
    // implementations
    void CloseAudioImp_() { SDL_CloseAudio(); }
    void PauseImp_(bool pause) { SDL_PauseAudio(pause); }
    bool OpenAudioImp_(AudioConfig& out, AudioConfig const& config) {
        if (!config) {
            out.Reset();
            return false;
        }

        SDL_AudioSpec desired, result;
        memset(&desired, 0, sizeof(desired));
        memset(&result, 0, sizeof(result));
        desired.callback = sSDLCallback_;
        desired.userdata = this;
        desired.format   = AUDIO_F32SYS;
        if (AUDIO_FORMAT_S16==config.Format) {
            desired.format = AUDIO_S16SYS;
        }
        else if (AUDIO_FORMAT_S32==config.Format) {
            desired.format = AUDIO_S32SYS;
        }
        else if (AUDIO_FORMAT_U8==config.Format) {
            desired.format = AUDIO_U8;
        }
        desired.freq     = config.SampleRate;
        desired.channels = (Uint8) config.Channels;

        // samples specifies a unit of audio data. When used with SDL_OpenAudioDevice()
        // this refers to the size of the audio buffer in sample frames.
        // A sample frame is a chunk of audio data of the size specified in format
        // multiplied by the number of channels. When the SDL_AudioSpec is used with
        // SDL_LoadWAV() samples is set to 4096. This field's value must be a power of two.
        desired.samples = 2048;
        while (desired.samples<8192 &&
              (desired.samples*SDL_AUDIO_MAX_CALLBACKS_PER_SEC)<desired.freq) {
            desired.samples *= 2;
        }

        if (SDL_OpenAudio(&desired, &result)>=0) {
            SDL_PauseAudio(1);
            out.SampleRate = result.freq;
            out.Channels = result.channels;
            if (AUDIO_F32SYS==result.format) {
                out.Format = AUDIO_FORMAT_F32;
            }
            else if (AUDIO_S16SYS==result.format) {
                out.Format = AUDIO_FORMAT_S16;
            }
            else if (AUDIO_S32SYS==result.format) {
                out.Format = AUDIO_FORMAT_S32;
            }
            else if (AUDIO_U8==result.format) {
                out.Format = AUDIO_FORMAT_U8;
            }
            else {
                SDL_CloseAudio();
                out.Reset();
                return false;
            }

            // init some buffers...
            SetRenderBatchSize_(out, result.samples);

            return true;
        }

        // return the suggest format that probably works...

        //
        // refer FFmpeg ffplay.c audio_open() for different enumeration strategy...
        //  int const next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
        //  int const next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
        //  channels fallback = 7 "6.1" -> 6 "5.1(back)" -> 4 "4.0" -> 2 "stereo" -> 1 "mono" -> 0 (fail)
        //                      5 "5.0(back)" -> 6 "5.1(back)"
        //                      3 "2.1" -> 6
        //
        out = config;
        if (out.Channels>6) {
            out.Channels = 6;
        }
        else {
            int const next_nb_channels[] = { 0, 0, 1, 2, 2, 4, 4 };
            out.Channels = next_nb_channels[config.Channels];
            if (0==out.Channels) {
                out.Reset();
            }
        }

        return false;
    }

    // a callback for audio system...
    static void sSDLCallback_(void* thiz, uint8* stream, int size) {
        assert(thiz && stream && size>0);
        if (stream && size>0) {
            if (nullptr!=thiz) {
                ((SDLAudioManager*)thiz)->RenderCallback_(stream, size);
            }
            else {
                memset(stream, 0, size);
            }
        }
    }

public:
    SDLAudioManager() {}
    ~SDLAudioManager() {}
};

//-----------------------------------------------------------------
AudioManager& AudioManager::GetInstance()
{
    static SDLAudioManager inst_;
    return inst_;
}

}}}