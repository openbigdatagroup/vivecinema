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
 * @file    Audio.cpp
 * @author  andre chen
 * @history 2017/02/21 created
 *
 */
#include "Audio.h"

namespace mlabs { namespace balai { namespace audio {

//---------------------------------------------------------------------------------------
bool AudioManager::LoadDecoders(char const* /*decoderlib*/)
{
    // TBD
    return true;
}
//---------------------------------------------------------------------------------------
int AudioManager::DecodeAudioData(void* dst, AudioConfig const& config,
                                  void const* src, int frames, AudioDesc const& desc,
                                  mlabs::balai::math::Matrix3 const& listener, float gain)
{
    assert(dst && src && frames>0);
    int const total_bytes = frames*config.BytesPerFrame();
    if (gain>0.0001f) {
        if (AUDIO_TECHNIQUE_DEFAULT!=desc.Technique) {
            if (2==config.Channels && config.SampleRate==desc.SampleRate) {
                if (DecodeAmbisonicHRTF_(dst, config.Format, listener, src, frames, desc, gain)) {
                    return frames;
                }
                else if (AUDIO_FORMAT_F32==desc.Format) {
                    // deprecated ambisonics decoder
                    if (DecodeAmbisonicByProjection_(dst, config.Format, listener,
                                                     (float const*)src, frames, desc, gain)) {
                        return frames;
                    }
                }
            }

            //
            // should never reach here...
            // see bool VRVideoPlayer::AudioSettingCallback(int decoder_id, AudioInfo& ai) for how.
            //
            BL_ERR("AudioManager::DecodeAudioData() - failed to decode audio(technique=%d)\n", desc.Technique);
        }
        else {
            assert(config.Format==desc.Format);
            if (config.Format==desc.Format && config.Channels==desc.NumChannels) {
                if (1.0f==gain) {
                    memcpy(dst, src, total_bytes);
                }
                else {
                    int const total_samples = frames*config.Channels;
                    if (AUDIO_FORMAT_U8==desc.Format) {
                        int const t = (int) (256.0f*gain + 0.5f);
                        uint8* d = (uint8*) dst;
                        uint8 const* s = (uint8 const*) src;
                        for (int i=0; i<total_samples; ++i) {
                            *d++ = (uint8) ((t*int(*s++))>>8);
                        }
                    }
                    else if (AUDIO_FORMAT_S16==desc.Format) {
                        int const t = (int) (256.0f*gain + 0.5f);
                        int16* d = (int16*) dst;
                        int16 const* s = (int16 const*) src;
                        for (int i=0; i<total_samples; ++i) {
                            *d++ = (int16) ((t*int(*s++))>>8);
                        }
                    }
                    else if (AUDIO_FORMAT_S32==desc.Format) {
                        int* d = (int*) dst;
                        int const* s = (int const*) src;
                        for (int i=0; i<total_samples; ++i) {
                            *d++ = (int)(gain*(*s++) + 0.5f);
                        }
                    }
                    else if (AUDIO_FORMAT_F32==desc.Format) {
                        float* d = (float*) dst;
                        float const* s = (float const*) src;
                        for (int i=0; i<total_samples; ++i) {
                            *d++ = (*s++)*gain;
                        }
                    }
                }

                return frames;
            }
            else if (config.Format==desc.Format && AUDIO_FORMAT_F32==desc.Format &&
                     (6==desc.NumChannels || 8==desc.NumChannels) ) { // 5.1 or 7.1?
                if (2==config.Channels && config.SampleRate==desc.SampleRate) {
                    float* dL = buffer_;     // dest L
                    float* dR = dL + frames; // dest R

                    float* FL = dR + frames; // input front-left
                    float* FR = FL + frames; // input front-right
                    float* C  = FR + frames; // input center
                    float* SL =  C + frames; // input side-left(left surround)
                    float* SR = SL + frames; // input side-right(right surround)
                    float* BL = SR + frames; // 7.1 input back-left
                    float* BR = BL + frames; // 7.1 input back-right

                    float const* s = (float const*) src;
                    if (6==desc.NumChannels) {
                        for (int i=0; i<frames; ++i,s+=6) {
                            FL[i] = s[0]; // left
                            FR[i] = s[1]; // right
                            C[i] = s[2];  // center
                            dL[i] = dR[i] = s[3]; // LFE
                            SL[i] = s[4]; // left surround
                            SR[i] = s[5]; // right surround
                        }
                    }
                    else {
                        for (int i=0; i<frames; ++i,s+=8) {
                            FL[i] = s[0]; // left
                            FR[i] = s[1]; // right
                            C[i] = s[2];  // center
                            dL[i] = dR[i] = s[3]; // LFE
                            SL[i] = s[4]; // left surround
                            SR[i] = s[5]; // right surround

                            BL[i] = s[6]; // back left
                            BR[i] = s[7]; // left surround
                        }
                    }

                    for (int i=0; i<frames; i+=CONVOLUTION_BLOCK_SIZE) {
                        itu_7_1_HRTF_.ProcessAdd(dL+i, FL+i, 0);
                        itu_7_1_HRTF_.ProcessAdd(dR+i, FL+i, 1);

                        itu_7_1_HRTF_.ProcessAdd(dL+i, FR+i, 2);
                        itu_7_1_HRTF_.ProcessAdd(dR+i, FR+i, 3);

                        itu_7_1_HRTF_.ProcessAdd(dL+i, C+i, 4);
                        itu_7_1_HRTF_.ProcessAdd(dR+i, C+i, 5);

                        itu_7_1_HRTF_.ProcessAdd(dL+i, SL+i, 6);
                        itu_7_1_HRTF_.ProcessAdd(dR+i, SL+i, 7);

                        itu_7_1_HRTF_.ProcessAdd(dL+i, SR+i, 8);
                        itu_7_1_HRTF_.ProcessAdd(dR+i, SR+i, 9);

                        if (8==desc.NumChannels) {
                            itu_7_1_HRTF_.ProcessAdd(dL+i, BL+i, 10);
                            itu_7_1_HRTF_.ProcessAdd(dR+i, BL+i, 11);

                            itu_7_1_HRTF_.ProcessAdd(dL+i, BR+i, 12);
                            itu_7_1_HRTF_.ProcessAdd(dR+i, BR+i, 13);
                        }
                    }

                    float* d = (float*) dst;
                    float const* sl = (float const*) dL;
                    float const* sr = (float const*) dR;
                    for (int i=0; i<frames; ++i) {
                        *d++ = (*sl++)*gain;
                        *d++ = (*sr++)*gain;
                    }

                    return frames;
                }
            }
        }
    }

    memset(dst, 0, total_bytes);
    return frames;
}

//---------------------------------------------------------------------------------------
inline uint32 Read_u32(uint8 const* d, bool big_endian) {
    if (!big_endian) {
        return BL_MAKE_4CC(d[3], d[2], d[1], d[0]);
    }
    else {
        return BL_MAKE_4CC(d[0], d[1], d[2], d[3]);
    }
}
inline uint32 Read_u16(uint8 const* data, bool big_endian) {
    if (!big_endian) {
        return (uint32(data[1])<<8)|uint32(data[0]);
    }
    else {
        return (uint32(data[0])<<8)|uint32(data[1]);
    }
}
inline float Read_f32(uint8 const* data, bool swap_byte) {
    if (!swap_byte) {
        return *(float*) data;
    }
    else {
        union {
            uint8 u8[4];
            float f32;
        } xx;
        xx.u8[0] = data[3];
        xx.u8[1] = data[2];
        xx.u8[2] = data[1];
        xx.u8[3] = data[0];
        return xx.f32;
    }
}
void WavFile::Unload()
{
    if (samples_) {
        free(samples_);
        samples_ = NULL;
    }
    totalChannels_ = totalFrames_ = sampleRate_ = playing_ = 0;
}
//---------------------------------------------------------------------------------------
bool WavFile::Load(void const* data, int frames, AUDIO_FORMAT fmt, uint32 ch, uint32 freq)
{
    Unload();
    if (data && frames>0 && AUDIO_FORMAT_UNKNOWN!=fmt && ch>0 && freq>0) {
        int const toal_samples = frames*ch;
        samples_ = (sint16*) malloc(toal_samples*2);
        if (NULL!=samples_) {
            if (AUDIO_FORMAT_F32==fmt) {
                sint16* dst = samples_;
                float const* src = (float const*) data;
                for (int i=0; i<toal_samples; ++i,++src,++dst) {
                    if (*src<1.0f) {
                        if (-1.0f<*src) {
                            *dst = (sint16) (*src * 32767.0f);
                        }
                        else {
                            *dst = -0x7fff;
                        }
                    }
                    else {
                        *dst = 0x7fff;
                    }
                }
            }
            else if (AUDIO_FORMAT_S32==fmt) {
                sint16* dst = samples_;
                int const* src = (int const*) data;
                for (int i=0; i<toal_samples; ++i,++src,++dst) {
                    *dst = (sint16) (*src/65536);
                }
            }
            else if (AUDIO_FORMAT_S16==fmt) {
                memcpy(samples_, data, toal_samples*2);
            }
            else if (AUDIO_FORMAT_U8==fmt) {
                sint16* dst = samples_;
                uint8 const* src = (uint8 const*) data;
                for (int i=0; i<toal_samples; ++i,++src,++dst) {
                    *dst = (sint16) ((2.0f*((*src)/255.0f) - 1.0f)*32767.0f);
                }
            }
            else {
                free(samples_);
                samples_ = NULL;
                return false;
            }

            totalChannels_ = ch;
            totalFrames_ = frames;
            sampleRate_ = freq;
            return true;
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------
bool WavFile::Load(char const* filename, AUDIO_TECHNIQUE tech)
{
    Unload();
    FILE* file = fopen(filename, "rb");
    if (NULL==file)
        return false;

    char const* ext = filename + strlen(filename) - 4;
    if (0==strcmp(ext, ".amb") || 0==strcmp(ext, ".AMB")) {
        tech = AUDIO_TECHNIQUE_FUMA;
    }

    //
    // Notes:
    // 1. The default byte ordering assumed for WAVE data files is little-endian.
    //    Files written using the big-endian byte ordering scheme have the identifier
    //    RIFX instead of RIFF.  --- TO-DO MAKE_U32('R', 'I', 'F', 'X')
    // 2. The sample data must end on an even byte boundary. Whatever that means.
    // 3. 8-bit samples are stored as unsigned bytes, ranging from 0 to 255.
    //    16-bit samples are stored as 2's-complement signed integers, ranging from -32768 to 32767.
    // 4. There may be additional subchunks in a Wave data stream. If so, each will have a char[4] SubChunkID,
    //    and unsigned long SubChunkSize, and SubChunkSize amount of data.
    // 5. RIFF stands for Resource Interchange File Format.
    //
    uint8 buf[64];
    bool wave_big_endian = false;
    int data_size(0), BitsPerSample(0);
    if (12==fread(buf, 1, 12, file) &&
        'R'==buf[0] && 'I'==buf[1] && 'F'==buf[2] && ('F'==buf[3] || 'X'==buf[3]) &&
        'W'==buf[8] && 'A'==buf[9] && 'V'==buf[10] && 'E'==buf[11]) {
        wave_big_endian = ('X'==buf[3]);
        //uint32 const chunk_size = Read_u32(buf+4, big_endian);

        // read the "fmt " subchunk
        uint32 audio_fmt(0), ByteRate(0), BlockAlign(0);
        while (8==fread(buf, 1, 8, file)) {
            uint32 const chunk_size = Read_u32(buf+4, wave_big_endian);
            if ('f'==buf[0] && 'm'==buf[1] && 't'==buf[2] && ' '==buf[3]) {
                if (16<=chunk_size && 16==fread(buf, 1, 16, file)) {
                    audio_fmt = Read_u16(buf, wave_big_endian); // 1:pcm
                    totalChannels_ = Read_u16(buf+2, wave_big_endian);
                    sampleRate_ = Read_u32(buf+4, wave_big_endian);
                    ByteRate = Read_u32(buf+8 , wave_big_endian);
                    BlockAlign = Read_u16(buf+12, wave_big_endian);
                    BitsPerSample = Read_u16(buf+14, wave_big_endian);
                    if (16<chunk_size) {
                        uint32 ext_size = chunk_size - 16;
                        if (ext_size<=sizeof(buf)) {
                            fread(buf, 1, ext_size, file);
                        }
                        else {
                            fread(buf, 1, sizeof(buf), file);
                            fseek(file, ext_size-sizeof(buf), SEEK_CUR);
                        }
                    }
                }
                break;
            }
            else {
                buf[4] = '\0';
                BL_LOG("WavFile::Load() - bypass %s chunk(%d bytes)\n", buf, chunk_size);
                fseek(file, chunk_size, SEEK_CUR);
            }
        }

        // support Full3D ambisonics only
        if (AUDIO_TECHNIQUE_AMBIX==tech ||
            AUDIO_TECHNIQUE_FUMA==tech) {
            if (4!=totalChannels_ && 9!=totalChannels_ && 16!=totalChannels_) {
                totalChannels_ = 0;
            }
        }

        // locate the "data" sub-chunk
        if (/*1==audio_fmt &&*/ totalChannels_>0 && sampleRate_>0 &&
            (8==BitsPerSample || 16==BitsPerSample || 24==BitsPerSample || 32==BitsPerSample)) {
            while (8==fread(buf, 1, 8, file)) {
                uint32 const chunk_size = Read_u32(buf+4, wave_big_endian);
                if ('d'==buf[0] && 'a'==buf[1] && 't'==buf[2] && 'a'==buf[3]) {
                    uint32 const BytesPerFrames = totalChannels_*BitsPerSample/8;
                    totalFrames_ = chunk_size/BytesPerFrames;
                    if (totalFrames_>0 &&
                        (totalFrames_*BytesPerFrames)==chunk_size &&
                        (sampleRate_*BytesPerFrames)==ByteRate) {
                        data_size = chunk_size;
                    }

                    break;
                }
                else {
                    buf[4] = '\0';
                    BL_LOG("WavFile::Load() - bypass %s chunk(%d bytes)\n", buf, chunk_size);
                    fseek(file, chunk_size, SEEK_CUR);
                }
            }
        }
    }

    if (data_size<=0) {
        Unload();
        fclose(file);
        return false;
    }

    // system endianness
    union Int16SB {
        int16 s16;
        uint8 u8[2];
    } t16;
    t16.u8[0] = 0x01;
    t16.u8[1] = 0x02;
    bool const system_big_endian = (0x0102==t16.s16);

    int const TotalSamples = totalFrames_*totalChannels_;
    sint16* dst = samples_ = (sint16*) malloc(TotalSamples*2);
    if (NULL==samples_) {
        fclose(file);
        Unload();
        return false;
    }

    if (AUDIO_TECHNIQUE_FUMA==tech) {
        // { W XYZ RSTUV KLMNOPQ } to { W YZX VTRSU QOMKLNP }
        int const acn[16] = { 0, 3,1,2, 6,7,5,8,4, 12,13,11,14,10,15,9 };

        // to sn3d
        float const s00 = sqrt(2.0f); // +3dB
        float const s20 = sqrt(3.0f)/2.0f;
        float const s30 = sqrt(32.0f/45.0f);
        float const s31 = sqrt(5.0f)/3.0f;
        float const s32 = sqrt(5.0f/8.0f);
        float const sn3d[16] = {
                               s00,                 // W
                        1.0f, 1.0f, 1.0f,           // X, Y, Z
                  1.0f,  s20,  s20,  s20, s20,      // R, S, T, U, V
            1.0f,  s30,  s30,  s31,  s31, s32, s32  // K, L, M, N, O, P, Q
        };

        int const BytesPerFrame = totalChannels_*BitsPerSample/8;
        if (8==BitsPerSample) {
            for (uint32 i=0; i<totalFrames_; ++i) {
                fread(buf, 1, BytesPerFrame, file);
                uint8 const* s = buf;
                for (uint32 j=0; j<totalChannels_; ++j,++s) {
                    dst[acn[j]] = (sint16) (sn3d[j]*((*s-128)/127.0f)*32767.0f);
                }
                dst += totalChannels_;
            }
        }
        else if (16==BitsPerSample) {
            if (wave_big_endian) {
                for (uint32 i=0; i<totalFrames_; ++i) {
                    fread(buf, 1, BytesPerFrame, file);
                    uint8 const* s = buf;
                    for (uint32 j=0; j<totalChannels_; ++j,s+=2) {
                        dst[acn[j]] = (uint16) (sn3d[j]*(((sint16)(s[0]<<8)|s[1])));
                    }
                    dst += totalChannels_;
                }
            }
            else {
                for (uint32 i=0; i<totalFrames_; ++i) {
                    fread(buf, 1, BytesPerFrame, file);
                    uint8 const* s = buf;
                    for (uint32 j=0; j<totalChannels_; ++j,s+=2) {
                        dst[acn[j]] = (uint16) (sn3d[j]*(((sint16)(s[1]<<8)|s[0])));
                    }
                    dst += totalChannels_;
                }
            }
        }
        else if (24==BitsPerSample) {
            if (wave_big_endian) {
               for (uint32 i=0; i<totalFrames_; ++i) {
                    fread(buf, 1, BytesPerFrame, file);
                    uint8 const* s = buf;
                    for (uint32 j=0; j<totalChannels_; ++j,s+=3) {
                        dst[acn[j]] = (uint16) (sn3d[j]*(((sint16)(s[0]<<8)|s[1])));
                    }
                    dst += totalChannels_;
                }
            }
            else {
                for (uint32 i=0; i<totalFrames_; ++i) {
                    fread(buf, 1, BytesPerFrame, file);
                    uint8 const* s = buf;
                    for (uint32 j=0; j<totalChannels_; ++j,s+=3) {
                        dst[acn[j]] = (uint16) (sn3d[j]*(((sint16)(s[2]<<8)|s[1])));
                    }
                    dst += totalChannels_;
                }
            }
        }
        else {
            assert(32==BitsPerSample);
            bool const swap_byte = (!system_big_endian) ^ wave_big_endian;
            for (uint32 i=0; i<totalFrames_; ++i) {
                fread(buf, 1, BytesPerFrame, file);
                uint8 const* s = buf;
                for (uint32 j=0; j<totalChannels_; ++j,s+=4) {
                    dst[acn[j]] = (uint16) (sn3d[j]*Read_f32(s, swap_byte)*32767.0f);
                }
                dst += totalChannels_;
            }
        }
    }
    else {
        if (16==BitsPerSample) {
            assert(TotalSamples*2==data_size);
            if (data_size!=(int)fread(samples_, 1, data_size, file)) {
                fclose(file);
                Unload();
                return false;
            }

            uint8 const* src = (uint8 const*) samples_;
            if (wave_big_endian) {
                if (!system_big_endian) {
                    for (int i=0; i<TotalSamples; ++i,src+=2) {
                        *dst++ = (sint16)(src[0]<<8)|src[1];
                    }
                }
            }
            else {
                if (system_big_endian) {
                    for (int i=0; i<TotalSamples; ++i,src+=2) {
                        *dst++ = (sint16)(src[1]<<8)|src[0];
                    }
                }
            }
        }
        else {
            uint8* data = (uint8*) malloc(data_size);
            if (NULL==data || data_size!=(int)fread(data, 1, data_size, file)) {
                if (NULL!=data) {
                    free(data);
                }
                fclose(file);
                Unload();
                return false;
            }

            uint8* src = data;
            if (8==BitsPerSample) {
                for (int i=0; i<TotalSamples; ++i,++src,++dst) {
                    *dst = (sint16) ((2.0f*((*src)/255.0f)-1.0f)*32767.0f);
                }
            }
            else if (24==BitsPerSample) {
                if (wave_big_endian) {
                    for (int i=0; i<TotalSamples; ++i,src+=3) {
                        *dst++ = (sint16)(src[0]<<8)|src[1];
                    }
                }
                else {
                    for (int i=0; i<TotalSamples; ++i,src+=3) {
                        *dst++ = (sint16)(src[2]<<8)|src[1];
                    }
                }
            }
            else if (32==BitsPerSample) { // assume it's float!?
                // swap byte... need to check...
                bool const swap_byte = system_big_endian ^ wave_big_endian;
                for (int i=0; i<TotalSamples; ++i,src+=4) {
                    *dst++ = (sint16)(32767.0f*Read_f32(src, swap_byte));
                }
            }
            free(data);
        }
    }

    fclose(file);
    return true;
}
//---------------------------------------------------------------------------------------
bool WavFile::Save(char const* filename) const
{
    int const data_size = totalFrames_*totalChannels_*2;
    if (NULL!=samples_ && 0<data_size && filename) {
        FILE* file = fopen(filename, "wb");
        if (NULL!=file) {
            uint32 u32 = 4 + 8 + 16 + 8 + data_size;
            fwrite("RIFF", 1, 4, file);
            fwrite(&u32, 1, 4, file);
            fwrite("WAVE", 1, 4, file);

            // fmt
            uint32 BytesPerFrames = data_size/totalFrames_;
            uint16 u16 = 0;
            fwrite("fmt ", 1, 4, file);
            u32 = 16; fwrite(&u32, 1, 4, file); // chunk size
            u16 = 1; fwrite(&u16, 1, 2, file); // audio_fmt
            u16 = (uint16) totalChannels_; fwrite(&u16, 1, 2, file); // channels
            u32 = sampleRate_; fwrite(&u32, 1, 4, file); // sample rate
            u32 = sampleRate_*BytesPerFrames; fwrite(&u32, 1, 4, file);// byte rate
            u16 = 4; fwrite(&u16, 1, 2, file); // block align
            u16 = (uint16) (8*(BytesPerFrames/totalChannels_)); fwrite(&u16, 1, 2, file); // bits per samples

            // data
            fwrite("data", 1, 4, file);
            fwrite(&data_size, 1, 4, file); // chunk size
            fwrite(samples_, 1, data_size, file);

            fclose(file);
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------------------
int WavFile::Streaming(void* samples, int total_frames, AUDIO_FORMAT fmt, float gain)
{
    if (samples && total_frames>0 && samples_ && AUDIO_FORMAT_UNKNOWN!=fmt) {
        int const max_frames = totalFrames_ - playing_;
        if (total_frames>max_frames) {
            total_frames = max_frames;
        }

        int const total_samples = totalChannels_*total_frames;
        sint16 const* src = samples_ + playing_*totalChannels_;

        if (AUDIO_FORMAT_F32==fmt) {
            gain /= 32767.0f;
            float* dst = (float*) samples;
            for (int i=0; i<total_samples; ++i) {
                *dst++ = gain*(*src++);
            }
        }
        else if (AUDIO_FORMAT_S16==fmt) {
            if (gain==1.0f) {
                memcpy(samples, src, total_samples*2);
            }
            else {
                sint16* dst = (sint16*) samples;
                for (int i=0; i<total_samples; ++i) {
                    *dst++ = (sint16) (gain*(*src++));
                }
            }
        }
        else if (AUDIO_FORMAT_U8==fmt) {
            gain *= 255.0f;
            uint8* dst = (uint8*) samples;
            for (int i=0; i<total_samples; ++i) {
                *dst++ = (uint8) (gain*(((*src++)/32767.0f)+0.5f));
            }
        }
        else if (AUDIO_FORMAT_S32==fmt) {
            return 0; // any chance!?
        }

        if ((playing_+=total_frames)>=totalFrames_) {
            //BL_LOG("ambient bgm rewind\n");
            playing_ = 0;
        }

        return total_frames;
    }
    return 0;
}

}}}