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
 * @file    Ambisonics.cpp
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2017/03/22 created
 *
 */
#include "Audio.h"
#include "BLMatrix3.h"

using mlabs::balai::math::Matrix3;
using mlabs::balai::math::Vector3;

namespace mlabs { namespace balai { namespace audio {

/*
  links:
    https://github.com/google/spatial-media/blob/master/docs/spatial-audio-rfc.md
    https://support.google.com/youtube/answer/6395969?hl=en
    https://en.wikipedia.org/wiki/Ambisonic_data_exchange_formats
    https://facebook360.fb.com/spatial-workstation/
    https://code.facebook.com/posts/412047759146896/spatial-audio-bringing-realistic-sound-to-360-video/
    http://pcfarina.eng.unipr.it/TBE-conversion.htm
    https://www.york.ac.uk/sadie-project/binaural.html

  test videos here...
    http://www.angelofarina.it/Public/Jump-Videos/

  implementations:
    https://github.com/GoogleChrome/omnitone <<-- must read!
    https://github.com/kronihias/ambix <<-- recommended!!! (azimuth reversed)
    https://github.com/kronihias/libambix
    http://iem.at/~zmoelnig/libambix/index.html

  encoder:
    https://github.com/google/songbird

    #(channels) for spatial audio :
      4 : 1st order ambisonics(ambiX/FuMa)
      6 : 1st order ambisonics(ambiX/FuMa) + stereo headlock
      8 : TBE(2nd order ambisonics by merging Z+R channels)
      9 : 2nd order ambisonics(ambiX/FuMa)... really?
     10 : FB360(TBE+Stereo)
     11 : 2nd order ambisonics(ambiX/FuMa) + stereo headlock
     16 : 3rd order ambisonics(ambiX/FuMa)
     18 : 3rd order ambisonics(ambiX/FuMa) + stereo headlock

    special care must be taken when a channel data comes from LFE(sub-woofer). AAC tends
    to applie an aggressive low-pass filter and other techniques to compress the LFE channel.
    This is incompatible with faithful rendering of spatial audio. The LFE will likly be the
    4th channel for some .1 channel layout.
    eg. 5.1ch sequence = front-left, front-right, center, low-freq, rear-left, rear-right.

    Ambisonic Channel Number (ACN) index system :
      chmaps[0] = index to W channel
      chmaps[1] = index to Y channel
      chmaps[2] = index to Z channel
      chmaps[3] = index to X channel

    Azimuth = 0             => The source is in front of the listener
    Azimuth in (0, pi/2)    => The source is in the forward-left quadrant.
    Azimuth in (pi/2, pi)   => The source is in the back-left quadrant.
    Azimuth in (-pi/2, 0)   => The source is in the forward-right quadrant.
    Azimuth in (-pi, -pi/2) => The source is in the back-right quadrant.

    it means Z-axis up, X-axis faces front and Y-axis toward left.

    to transform to balai coordinate system:
     x = -Y
     y =  X
     z =  Z

    Spherical Harmonics (SN3D, no Condon-Shortley phase, no sqrt(1/4pi))

     W : 1

     Y : y
     Z : z
     X : x

     V : sqrt(3)*x*y
     T : sqrt(3)*y*z
     R : 1.5*z*z - 0.5
     S : sqrt(3)*x*z
     U : (sqrt(3)/2)*(x*x - y*y)

     Q : sqrt(5/8)*y*(3*x*x - y*y)
     O : sqrt(15)*x*y*z
     M : sqrt(3/8)*y*(5*z*z - 1.0)
     K : 0.5*z*(5*z*z - 3.0)
     L : sqrt(3/8)*x*(5*z*z - 1.0)
     N : sqrt(15/4)*(x*x - y*y)*z
     P : sqrt(5/8)*x*(x*x - 3*y*y)

    FB360_RenderingSDK\TBAudioEngine_0.9.95_C_All\TBAudioEngine_GettingStarted_C_Api.pdf
     FB360 coordinate system : 3D vectors and points are represented through the class
     TBVector3, where +x is right, -x is left, +y is up, -y is down,
     +z is forward(into the screen) and -z is backward. (Left-handed coordinate system!?)

               W
             Y Z X         First Order Ambisonic(FOA) = { W, Y, Z, X }
           V T R S U       2nd Order Ambisonic (9 channels)
         Q O M K L N P     3rd Order Ambisonic (16 channels)

     for B-Format ACN(X YZX VTRSU QOMKLNP) = {  // no remap
                   0,
               1,  2,  3,
           4,  5,  6,  7,  8,
       9, 10, 11, 12, 13, 14, 15
     };

     for B-Format FuMa(W XYZ UVSTR PQNOLMK) = { // convert to ACN ordering
                   0,
               2,  3,  1,
           5,  7,  8,  6,  4,
      10, 12, 14, 15, 13, 11, 9
    };

    also note the W channel of FuMa also has a -3 dB gain applied.
    -3 dB = 10^(-3/10) = 0.501187 power ratio = 0.707946 amplitude ratio
    (by amplitude ratio = sqrt(power ratio))
    i.e. W_FuMa = 0.708*W_Ambix

    for first order Ambisonics(FOA) - AmbiX the four channels use SN3D normalisation
    (which for first order Ambisonics simply means that the four channels have an
     uniform gain normalisation)

    Norm_N3D = Norm_SN3D * sqrt(2*l+1)
*/

//
// TBE 8 channel rotaion matrix
struct TBERotationMatrix {
    float m00; // * TBE[0]
    float m11, m12, m13; //   | TBE[1] |
    float m21, m22, m23; // * | TBE[2] |
    float m31, m32, m33; //   | TBE[3] |
    float s11, s12, s13, s14, s15; //   | TBE[3] |
    float s21, s22, s23, s24, s25; //   | TBE[4] |
    float s31, s32, s33, s34, s35; // * | TBE[5] |
    float s41, s42, s43, s44, s45; //   | TBE[6] |
    float s51, s52, s53, s54, s55; //   | TBE[7] |

#define PROF_ANGELO_REVISED_TBE_FORMULA
#ifndef PROF_ANGELO_REVISED_TBE_FORMULA
    TBERotationMatrix(math::SphericalHarmonics::SHRotateMatrix const& shRot) {
        //
        // Big thanks to Prof. Angelo Farina's share, and below is only my best guess.
        // Not to mean Two Big Ears decodes their TBE the same way.
        //
        //  TBE[0] =  0.486968 * Ambix[0] // W
        //  TBE[1] = -0.486968 * Ambix[1] // Y
        //  TBE[2] =  0.486968 * Ambix[3] // X
        //  TBE[3] =  0.344747 * Ambix[2] + 0.445656 * Ambix[6] // Z + R
        //  TBE[4] = -0.630957 * Ambix[8] // U
        //  TBE[5] = -0.630957 * Ambix[4] // V
        //  TBE[6] = -0.630957 * Ambix[5] // T
        //  TBE[7] =  0.630957 * Ambix[7] // S
        //
        // in matrix form...
        //
        //  | TBE[0] |   | a 0 0 0 0 0 0 0 0 |    | W |
        //  | TBE[1] |   | 0 b 0 0 0 0 0 0 0 |    | Y |
        //  | TBE[2] |   | 0 0 0 c 0 0 0 0 0 |    | Z |
        //  | TBE[3] | = | 0 0 d 0 0 0 e 0 0 |    | X |
        //  | TBE[4] |   | 0 0 0 0 0 0 0 0 f | *  | V |
        //  | TBE[5] |   | 0 0 0 0 g 0 0 0 0 |    | T |
        //  | TBE[6] |   | 0 0 0 0 0 h 0 0 0 |    | R |
        //  | TBE[7] |   | 0 0 0 0 0 0 0 i 0 |8x9 | S |
        //                                        | U |9x1
        //
        // take psedoinverse, the matrix transfroms TBE to fake 2nd order ambiX...
        //
        //  | W |   | a  0  0  0  0  0  0  0 |    | TBE[0] |
        //  | Y |   | 0 -a  0  0  0  0  0  0 |    | TBE[1] |
        //  | Z |   | 0  0  0  b  0  0  0  0 |    | TBE[2] |
        //  | X |   | 0  0  a  0  0  0  0  0 |    | TBE[3] |
        //  | V | = | 0  0  0  0  0 -d  0  0 | *  | TBE[4] |
        //  | T |   | 0  0  0  0  0  0 -d  0 |    | TBE[5] |
        //  | R |   | 0  0  0  c  0  0  0  0 |    | TBE[6] |
        //  | S |   | 0  0  0  0  0  0  0  d |    | TBE[7] |8x1
        //  | U |   | 0  0  0  0 -d  0  0  0 |9x8
        //
        // where a=2.053523, b=1.085955, c=1.403819 and d=1.584894
        //
        // error matrix :
        //    | 0  0         0  0  0  0         0,  0  0 |
        //    | 0  0         0  0  0  0         0,  0  0 |
        //    | 0  0 -0.625620  0  0  0  0.483962,  0  0 | --> Z
        //    | 0  0         0  0  0  0         0,  0  0 |
        //    | 0  0         0  0  0  0         0,  0  0 |
        //    | 0  0         0  0  0  0         0,  0  0 |
        //    | 0  0  0.483962  0  0  0 -0.374380,  0  0 | --> R, or 0.5*(3*z*z - 1)
        //    | 0  0         0  0  0  0,        0,  0  0 |
        //    | 0  0         0  0  0  0,        0,  0  0 |
        //
        float const a = 2.053523f;
        float const b = 1.085955f;
        float const c = 1.403819f;
        float const d = 1.584894f;

#ifdef THE_DESERTED_VENICE_2017
        float const w_normalize = 1.0f/a; // multiply 2.53 on w, too much!?
        float const o0_weight = w_normalize*1.0f;
        float const o1_weight = w_normalize*0.77f;
        float const o2_weight = w_normalize*0.4f;
        float const a0 = o0_weight*a;
        float const a1 = o1_weight*a;
        float const b1 = o1_weight*b;
        float const c2 = o2_weight*c;
        float const d2 = o2_weight*d;
#else
        float const normalize_scale = 1.0f/a; // multiply 2.53 on w, too much!?
        float const a0 = normalize_scale*a;
        float const a1 = normalize_scale*a;
        float const b1 = normalize_scale*b;
        float const c2 = normalize_scale*c;
        float const d2 = normalize_scale*d;
#endif
        // next, concatenate with Spherical Harmonics rotaion matrix
        float m[25];

        // W is easy
        shRot.GetSubMatrix(m, 0); // 1x1
        m00 = m[0]*a0;

        // Y, Z, X
        //
        // | m[0] m[1] m[2] |   | -a  0  0 |
        // | m[3] m[4] m[5] | * |  0  0  b |
        // | m[6] m[7] m[8] |   |  0  a  0 |
        //
        shRot.GetSubMatrix(m, 1); // 3x3
        m11 = -m[0]*a1; m12 = m[2]*a1; m13 = m[1]*b1;
        m21 = -m[3]*a1; m22 = m[5]*a1; m23 = m[4]*b1;
        m31 = -m[6]*a1; m32 = m[8]*a1; m33 = m[7]*b1;

        // V, T, R, S, U
        //
        // |  m[0]  m[1]  m[2]  m[3]  m[4] |   | 0  0 -d  0  0 |
        // |  m[5]  m[6]  m[7]  m[8]  m[9] |   | 0  0  0 -d  0 |
        // | m[10] m[11] m[12] m[13] m[14] | * | c  0  0  0  0 |
        // | m[15] m[16] m[17] m[18] m[19] |   | 0  0  0  0  d |
        // | m[20] m[21] m[22] m[23] m[24] |   | 0 -d  0  0  0 |
        //
        shRot.GetSubMatrix(m, 2); // 5x5
        s11 =  m[2]*c2; s12 =  -m[4]*d2; s13 =  -m[0]*d2; s14 =  -m[1]*d2; s15 =  m[3]*d2;
        s21 =  m[7]*c2; s22 =  -m[9]*d2; s23 =  -m[5]*d2; s24 =  -m[6]*d2; s25 =  m[8]*d2;
        s31 = m[12]*c2; s32 = -m[14]*d2; s33 = -m[10]*d2; s34 = -m[11]*d2; s35 = m[13]*d2;
        s41 = m[17]*c2; s42 = -m[19]*d2; s43 = -m[15]*d2; s44 = -m[16]*d2; s45 = m[18]*d2;
        s51 = m[22]*c2, s52 = -m[24]*d2; s53 = -m[20]*d2; s54 = -m[21]*d2; s55 = m[23]*d2;
    }
    // input TBE 8 channels, output 2nd order ambiX 9 channels
    bool Transform(float* dst, int dst_stride, float const* tbe) const {
        float const tbe1(tbe[1]);
        float const tbe2(tbe[2]);
        float const tbe3(tbe[3]);

        *dst = m00*tbe[0]; dst+=dst_stride;
        *dst = m11*tbe1 + m12*tbe2 + m13*tbe3; dst+=dst_stride;
        *dst = m21*tbe1 + m22*tbe2 + m23*tbe3; dst+=dst_stride;
        *dst = m31*tbe1 + m32*tbe2 + m33*tbe3; dst+=dst_stride;

        float const tbe4(tbe[4]);
        float const tbe5(tbe[5]);
        float const tbe6(tbe[6]);
        float const tbe7(tbe[7]);
        *dst = s11*tbe3 + s12*tbe4 + s13*tbe5 + s14*tbe6 + s15*tbe7; dst+=dst_stride;
        *dst = s21*tbe3 + s22*tbe4 + s23*tbe5 + s24*tbe6 + s25*tbe7; dst+=dst_stride;
        *dst = s31*tbe3 + s32*tbe4 + s33*tbe5 + s34*tbe6 + s35*tbe7; dst+=dst_stride;
        *dst = s41*tbe3 + s42*tbe4 + s43*tbe5 + s44*tbe6 + s45*tbe7; dst+=dst_stride;
        *dst = s51*tbe3 + s52*tbe4 + s53*tbe5 + s54*tbe6 + s55*tbe7; dst+=dst_stride;

        return true;
    }
#else
    // update version! Prof. Angelo TBE formula changed.
    TBERotationMatrix(math::SphericalHarmonics::SHRotateMatrix const& shRot) {
        //
        //  TBE[0] =  0.488603 * Ambix[0] // W
        //  TBE[1] = -0.488603 * Ambix[1] // Y
        //  TBE[2] =  0.488603 * Ambix[3] // X
        //  TBE[3] =  0.488603 * Ambix[2] // Z
        //  TBE[4] = -0.630783 * Ambix[8] // U
        //  TBE[5] = -0.630783 * Ambix[4] // V
        //  TBE[6] = -0.630783 * Ambix[5] // T
        //  TBE[7] =  0.630783 * Ambix[7] // S
        //
        // in matrix form(reversed)...
        //
        //  | W |   | a  0  0  0  0  0  0  0 |    | TBE[0] |
        //  | Y |   | 0 -a  0  0  0  0  0  0 |    | TBE[1] |
        //  | Z |   | 0  0  0  a  0  0  0  0 |    | TBE[2] |
        //  | X |   | 0  0  a  0  0  0  0  0 |    | TBE[3] |
        //  | V | = | 0  0  0  0  0 -b  0  0 | *  | TBE[4] |
        //  | T |   | 0  0  0  0  0  0 -b  0 |    | TBE[5] |
        //  | R |   | 0  0  0  0  0  0  0  0 |    | TBE[6] |
        //  | S |   | 0  0  0  0  0  0  0  b |    | TBE[7] |8x1
        //  | U |   | 0  0  0  0 -b  0  0  0 |9x8
        //
        float const a = 2.046651f; // 1.0f/0.488603f
        float const b = 1.585331f; // 1.0f/0.630783f

        // next, concatenate with Spherical Harmonics rotaion matrix
        float m[25];

        // W is easy
        shRot.GetSubMatrix(m, 0); // 1x1
        m00 = m[0]*a;

        // Y, Z, X
        //
        // | m[0] m[1] m[2] |   | -a  0  0 |
        // | m[3] m[4] m[5] | * |  0  0  a |
        // | m[6] m[7] m[8] |   |  0  a  0 |
        //
        shRot.GetSubMatrix(m, 1); // 3x3
        m11 = -m[0]*a; m12 = m[2]*a; m13 = m[1]*a;
        m21 = -m[3]*a; m22 = m[5]*a; m23 = m[4]*a;
        m31 = -m[6]*a; m32 = m[8]*a; m33 = m[7]*a;

        // V, T, R, S, U
        //
        // |  m[0]  m[1]  m[2]  m[3]  m[4] |   | 0  0 -b  0  0 |
        // |  m[5]  m[6]  m[7]  m[8]  m[9] |   | 0  0  0 -b  0 |
        // | m[10] m[11] m[12] m[13] m[14] | * | 0  0  0  0  0 |
        // | m[15] m[16] m[17] m[18] m[19] |   | 0  0  0  0  b |
        // | m[20] m[21] m[22] m[23] m[24] |   | 0 -b  0  0  0 |
        //
        shRot.GetSubMatrix(m, 2); // 5x5
        s11 = 0.0f; s12 =  -m[4]*b; s13 =  -m[0]*b; s14 =  -m[1]*b; s15 =  m[3]*b;
        s21 = 0.0f; s22 =  -m[9]*b; s23 =  -m[5]*b; s24 =  -m[6]*b; s25 =  m[8]*b;
        s31 = 0.0f; s32 = -m[14]*b; s33 = -m[10]*b; s34 = -m[11]*b; s35 = m[13]*b;
        s41 = 0.0f; s42 = -m[19]*b; s43 = -m[15]*b; s44 = -m[16]*b; s45 = m[18]*b;
        s51 = 0.0f, s52 = -m[24]*b; s53 = -m[20]*b; s54 = -m[21]*b; s55 = m[23]*b;
    }
    // input TBE 8 channels, output 2nd order ambiX 9 channels
    bool Transform(float* dst, int dst_stride, float const* tbe) const {
        *dst = m00*tbe[0]; dst+=dst_stride; // W

        float const tbe1(tbe[1]);
        float const tbe2(tbe[2]);
        float const tbe3(tbe[3]);
        *dst = m11*tbe1 + m12*tbe2 + m13*tbe3; dst+=dst_stride; // Y
        *dst = m21*tbe1 + m22*tbe2 + m23*tbe3; dst+=dst_stride; // Z
        *dst = m31*tbe1 + m32*tbe2 + m33*tbe3; dst+=dst_stride; // X

        float const tbe4(tbe[4]);
        float const tbe5(tbe[5]);
        float const tbe6(tbe[6]);
        float const tbe7(tbe[7]);
        *dst = s12*tbe4 + s13*tbe5 + s14*tbe6 + s15*tbe7; dst+=dst_stride; // V
        *dst = s22*tbe4 + s23*tbe5 + s24*tbe6 + s25*tbe7; dst+=dst_stride; // T
        *dst = s32*tbe4 + s33*tbe5 + s34*tbe6 + s35*tbe7; dst+=dst_stride; // R - amazingly R is generated by SH rotation
        *dst = s42*tbe4 + s43*tbe5 + s44*tbe6 + s45*tbe7; dst+=dst_stride; // S
        *dst = s52*tbe4 + s53*tbe5 + s54*tbe6 + s55*tbe7; dst+=dst_stride; // U

        return true;
    }
#endif
};

//---------------------------------------------------------------------------------------
bool AudioManager::DecodeAmbisonicHRTF_(void* output, AUDIO_FORMAT fmt,
                                        mlabs::balai::math::Matrix3 const& listener,
                                        void const* input, int frames, AudioDesc const& desc,
                                        float gain)
{
    int const total_samples = frames*desc.NumChannels;
    int buffer_size = total_samples*sizeof(float);
    if (AUDIO_FORMAT_F32!=desc.Format) {
        buffer_size *= 2;
    }
    else if (AUDIO_FORMAT_F32!=fmt) {
        buffer_size += frames*2*sizeof(float);
    }

    if (bufferSize_<buffer_size) {
        free(buffer_);
        buffer_ = (float*) malloc(buffer_size);
        if (buffer_) {
            bufferSize_ = buffer_size;
        }
        else {
            bufferSize_ = 0;
        }
    }

    if (NULL==buffer_)
        return false;

    // source data conversion if not f32
    float const* src = (float const*) input;
    if (AUDIO_FORMAT_F32!=desc.Format) {
        float* d = buffer_ + total_samples;
        src = d;

        if (AUDIO_FORMAT_S16==desc.Format) {
            sint16 const* s = (sint16 const*) input;
            float const scale = 1.0f/32767.0f;
            for (int i=0; i<total_samples; ++i) {
                *d++ = scale*(*s++);
            }
        }
        else if (AUDIO_FORMAT_U8==desc.Format) {
            uint8 const* s = (uint8 const*) input;
            float const scale = 2.0f/255.0f;
            for (int i=0; i<total_samples; ++i) {
                *d++ = scale*(*s++) - 1.0f;
            }
        }
        else if (AUDIO_FORMAT_S32==desc.Format) {
            int const* s = (int const*) input;
            float const scale = 1.0f/(0x7fffff);
            for (int i=0; i<total_samples; ++i) {
                *d++ = scale*((*s++)>>8);
            }
        }
        else {
            return false;
        }
    }

    // redirect destination buffer is not F32. it's safe for overlaping with
    // temportary input buffer, since final decoding can safely overwrite the
    // input buffer if there is.
    float* const dst = (AUDIO_FORMAT_F32==fmt) ? ((float*)output):(buffer_+total_samples);

    // rotation matrix for the soundfield
    mlabs::balai::math::Matrix3 const mtx = listener.GetInverse();

    // mid, side and headlock
    float const* mid = NULL;
    float const* side = NULL;
    float const* headlock = NULL;

    // show time!!! -- but just listen:-)
    if (AUDIO_TECHNIQUE_AMBIX==desc.Technique || AUDIO_TECHNIQUE_FUMA==desc.Technique) {
        // 2018.10.10 - rotation interpolation
        float sh1[16], sh2[16];
        float alpha(0.0f), inv_alpha;
        float const delta_alpha = 1.25f/frames;

        if (4==desc.NumChannels || 6==desc.NumChannels) { // 1st order
            if (AUDIO_TECHNIQUE_AMBIX==desc.Technique)
                shRotate_.BuildRotationMatrixAmbiX(2, mtx);
            else
                shRotate_.BuildRotationMatrixFuMa(2, mtx);

            // mid, side and headlock
            mid  = buffer_;
            side = buffer_ + frames;
            if (6==desc.NumChannels)
                headlock = src+4;

            // rotate and de-interleave
            float* w = buffer_;
            for (int i=0; i<frames; ++i,++w,src+=desc.NumChannels) {
                shRotate_.Transform(w, frames, src);
            }

            // HRTF processing
            for (int i=0; i<frames; i+=CONVOLUTION_BLOCK_SIZE) {
                w = buffer_ + i;
                float* y = w + frames;
                float const* z = y + frames;
                float const* x = z + frames;

                // w=mid, y=side
                foa_SH_HRTF_.Process(w, w, 0);
                foa_SH_HRTF_.Process(y, y, 1);
                foa_SH_HRTF_.ProcessAdd(w, z, 2);
                foa_SH_HRTF_.ProcessAdd(w, x, 3);
            }
        }
        else if (9==desc.NumChannels || 11==desc.NumChannels) { // 2nd order
            if (AUDIO_TECHNIQUE_AMBIX==desc.Technique)
                shRotate_.BuildRotationMatrixAmbiX(3, mtx);
            else
                shRotate_.BuildRotationMatrixFuMa(3, mtx);

            // mid, side and headlock
            mid  = buffer_;
            side = buffer_ + frames;
            if (11==desc.NumChannels)
                headlock = src+9;

            // rotate and de-interleave
            float* w = buffer_;

            if (!noSHLerp_) {
                assert(shRotateLast_.NumBands()==shRotate_.NumBands());
                for (int i=0; i<frames; ++i,++w,src+=desc.NumChannels) {
                    if (alpha<1.0f) {
                        inv_alpha = 1.0f - alpha;
                        shRotateLast_.Transform(sh1, 1, src);
                        shRotate_.Transform(sh2, 1, src);

                        for (int k=0; k<9; ++k) {
                            w[k*frames] = inv_alpha*sh1[k] + alpha*sh2[k];
                        }

                        alpha += delta_alpha;
                    }
                    else {
                        shRotate_.Transform(w, frames, src);
                    }
                }
            }
            else {
                noSHLerp_ = false;
                for (int i=0; i<frames; ++i,++w,src+=desc.NumChannels) {
                    shRotate_.Transform(w, frames, src);
                }
            }

            // HRTF processing
            for (int i=0; i<frames; i+=CONVOLUTION_BLOCK_SIZE) {
                w = buffer_ + i;
                float* y = w + frames;
                float const* z = y + frames;
                float const* x = z + frames;

                float const* v = x + frames;
                float const* t = v + frames;
                float const* r = t + frames;
                float const* s = r + frames;
                float const* u = s + frames;

                // w=mid, y=side
                soa_SH_HRTF_.Process(w, w, 0);
                soa_SH_HRTF_.Process(y, y, 1);
                soa_SH_HRTF_.ProcessAdd(w, z, 2);
                soa_SH_HRTF_.ProcessAdd(w, x, 3);

                soa_SH_HRTF_.ProcessAdd(y, v, 4);
                soa_SH_HRTF_.ProcessAdd(y, t, 5);
                soa_SH_HRTF_.ProcessAdd(w, r, 6);
                soa_SH_HRTF_.ProcessAdd(w, s, 7);
                soa_SH_HRTF_.ProcessAdd(w, u, 8);
            }
        }
        else if (16==desc.NumChannels || 18==desc.NumChannels) { // 3rd order
            if (AUDIO_TECHNIQUE_AMBIX==desc.Technique)
                shRotate_.BuildRotationMatrixAmbiX(4, mtx);
            else
                shRotate_.BuildRotationMatrixFuMa(4, mtx);

            // mid, side and headlock
            mid  = buffer_;
            side = buffer_ + frames;
            if (18==desc.NumChannels)
                headlock = src+16;

            // rotate and de-interleave
            float* w = buffer_;

            if (!noSHLerp_) {
                assert(shRotateLast_.NumBands()==shRotate_.NumBands());
                for (int i=0; i<frames; ++i,++w,src+=desc.NumChannels) {
                    if (alpha<1.0f) {
                        inv_alpha = 1.0f - alpha;
                        shRotateLast_.Transform(sh1, 1, src);
                        shRotate_.Transform(sh2, 1, src);

                        for (int k=0; k<16; ++k) {
                            w[k*frames] = inv_alpha*sh1[k] + alpha*sh2[k];
                        }

                        alpha += delta_alpha;
                    }
                    else {
                        shRotate_.Transform(w, frames, src);
                    }
                }
            }
            else {
                noSHLerp_ = false;
                for (int i=0; i<frames; ++i,++w,src+=desc.NumChannels) {
                    shRotate_.Transform(w, frames, src);
                }
            }

            // HRTF processing
            for (int i=0; i<frames; i+=CONVOLUTION_BLOCK_SIZE) {
                w = buffer_ + i;
                float* y = w + frames;
                float const* z = y + frames;
                float const* x = z + frames;

                float const* v = x + frames;
                float const* t = v + frames;
                float const* r = t + frames;
                float const* s = r + frames;
                float const* u = s + frames;

                float const* q = u + frames;
                float const* o = q + frames;
                float const* m = o + frames;
                float const* k = m + frames;
                float const* l = k + frames;
                float const* n = l + frames;
                float const* p = n + frames;

                // w=mid, y=side
                toa_SH_HRTF_.Process(w, w, 0);
                toa_SH_HRTF_.Process(y, y, 1);
                toa_SH_HRTF_.ProcessAdd(w, z, 2);
                toa_SH_HRTF_.ProcessAdd(w, x, 3);

                toa_SH_HRTF_.ProcessAdd(y, v, 4);
                toa_SH_HRTF_.ProcessAdd(y, t, 5);
                toa_SH_HRTF_.ProcessAdd(w, r, 6);
                toa_SH_HRTF_.ProcessAdd(w, s, 7);
                toa_SH_HRTF_.ProcessAdd(w, u, 8);

                toa_SH_HRTF_.ProcessAdd(y, q, 9);
                toa_SH_HRTF_.ProcessAdd(y, o, 10);
                toa_SH_HRTF_.ProcessAdd(y, m, 11);
                toa_SH_HRTF_.ProcessAdd(w, k, 12);
                toa_SH_HRTF_.ProcessAdd(w, l, 13);
                toa_SH_HRTF_.ProcessAdd(w, n, 14);
                toa_SH_HRTF_.ProcessAdd(w, p, 15);
            }
        }
        else {
            return false;
        }
    }
    else if (AUDIO_TECHNIQUE_TBE==desc.Technique &&
             (8==desc.NumChannels||10==desc.NumChannels)) {
        if (shRotate_.BuildRotationMatrixAmbiX(3, mtx)) {
            // TBE rotator
            TBERotationMatrix const tbeRotate(shRotate_);

            // mid, side and headlock
            mid  = buffer_;
            side = buffer_ + frames;
            if (10==desc.NumChannels)
                headlock = src+8;

            // rotate and de-interleave
            float* w = buffer_;
            for (int i=0; i<frames; ++i,++w,src+=desc.NumChannels) {
                tbeRotate.Transform(w, frames, src);

                //
                // TO-DO : FB360 Encoder 'Focus' settings...
                //         Focus size : 40 ~ 120 degrees (default=90.0)
                //         Off-focus level : -24.0 ~ 0 dB (amplitude attenuation scale 10^(-1.2) ~ 1.0)
                //
                // The final solution should embeded in tbeRotate.Transform(w, frames, src);
                //
            }

            //
            // note : tbeRotate.Transform() output 2nd order ambiX(9 channels)
            //
            for (int i=0; i<frames; i+=CONVOLUTION_BLOCK_SIZE) {
                w = buffer_ + i;
                float* y = w + frames;
                float const* z = y + frames;
                float const* x = z + frames;

                float const* v = x + frames;
                float const* t = v + frames;
                float const* r = t + frames;
                float const* s = r + frames;
                float const* u = s + frames;

                // w=mid, y=side
                soa_SH_HRTF_.Process(w, w, 0);
                soa_SH_HRTF_.Process(y, y, 1);
                soa_SH_HRTF_.ProcessAdd(w, z, 2);
                soa_SH_HRTF_.ProcessAdd(w, x, 3);

                soa_SH_HRTF_.ProcessAdd(y, v, 4);
                soa_SH_HRTF_.ProcessAdd(y, t, 5);
                soa_SH_HRTF_.ProcessAdd(w, r, 6);
                soa_SH_HRTF_.ProcessAdd(w, s, 7);
                soa_SH_HRTF_.ProcessAdd(w, u, 8);
            }
        }
    }
    else {
        assert(!"wrong format!!!");
        return false;
    }

    // update last
    shRotateLast_ = shRotate_;

    if (NULL!=mid && NULL!=side) {
        //
        // TO-DO : make it simd!
        //
        float* d = dst;
        if (NULL!=headlock) {
            if (gain==1.0f) {
                for (int i=0; i<frames; ++i,headlock+=desc.NumChannels) {
                    *d++ = *mid + *side + headlock[0];
                    *d++ = *mid - *side + headlock[1];
                    ++mid; ++side;
                }
            }
            else {
                for (int i=0; i<frames; ++i,headlock+=desc.NumChannels) {
                    *d++ = gain*(*mid + *side + headlock[0]);
                    *d++ = gain*(*mid - *side + headlock[1]);
                    ++mid; ++side;
                }
            }
        }
        else {
            if (gain==1.0f) {
                for (int i=0; i<frames; ++i) {
                    *d++ = *mid + *side;
                    *d++ = *mid - *side;
                    ++mid; ++side;
                }
            }
            else {
                for (int i=0; i<frames; ++i) {
                    *d++ = gain*(*mid + *side);
                    *d++ = gain*(*mid - *side);
                    ++mid; ++side;
                }
            }
        }
    }
    else {
        BL_ASSERT("DecodeAmbisonicHRTF_() failed!");
        return false;
    }

    // convert output if needed
    if (AUDIO_FORMAT_F32!=fmt) {
        float const* s = dst;
        int t;
        if (AUDIO_FORMAT_S16==fmt) {
            sint16* d = (sint16*) output;
            for (int i=0; i<total_samples; ++i) {
                t = (int) (32767.0f*(*s++));
                if (t<32767) {
                    *d++ = (-32767<t) ? sint16(t):(-32767);
                }
                else {
                    *d++ = 32767;
                }
            }
        }
        else if (AUDIO_FORMAT_U8==fmt) {
            uint8* d = (uint8*) output;
            for (int i=0; i<total_samples; ++i) {
                t = (int) (128.0f*(*s++) + 127);
                if (t<256) {
                    *d++ = (0<t) ? sint8(t):0; 
                }
                else {
                    *d++ = 255; 
                }
            }
        }
        else if (AUDIO_FORMAT_S32==fmt) {
            int* d = (int*) output;
            for (int i=0; i<total_samples; ++i,++s) {
                if (*s<1.0f) {
                    *d++ = (-1.0f<*s) ? int(s[0]*0x7fffffff):(-0x7fffffff);
                }
                else {
                    *d++ = 0x7fffffff; 
                }
            }
        }
        else {
            return false;
        }
    }

    return true;
}

//
// -- HRTF ambisonics binaural decoding end here ----------------------------------------
//

//
//
// below is the first version of ambisonics decoder with Vive Cinema 0.7.277(2016/12/16).
// it's simple, fast, no HRTFs, no other resources, but without HRTF the result cannot
// be too satisfied.
//
// Even though we should not use these functions any more, i'd like to keep these here
// in case we'll need it (for lower end machine?) someday.
//
// it decodes ambisonic by mean of direct projection.
//
//
namespace deprecated {

// 1st order
inline void spherical_harmonics_sn3d(float& sW, float& sY, float& sZ, float& sX,
                                     float x, float y, float z, float N0, float N1) {
    sW = N0;
    sY = N1*y;
    sZ = N1*z;
    sX = N1*x;
}

inline void spherical_harmonics_fuma(float& sW, float& sY, float& sZ, float& sX,
                                     float x, float y, float z, float N0, float N1) {
    sW = 1.414214f*N0;
    sY = N1*y;
    sZ = N1*z;
    sX = N1*x;
}

// 2nd order
inline void spherical_harmonics_sn3d(float& sW, float& sY, float& sZ, float& sX,
                                     float& sV, float& sT, float& sR, float& sS, float& sU,
                                     float x, float y, float z,
                                     float N0, float N1, float N2) {
    sW = N0;

    sY = N1*y;
    sZ = N1*z;
    sX = N1*x;

    sV = N2*1.732051f*x*y;
    sT = N2*1.732051f*y*z;
    sR = N2*(1.5f*z*z - 0.5f);
    sS = N2*1.732051f*x*z;
    sU = N2*0.866025f*(x*x-y*y);
}
inline void spherical_harmonics_n3d(float& sW, float& sY, float& sZ, float& sX,
                                    float& sV, float& sT, float& sR, float& sS, float& sU,
                                    float x, float y, float z,
                                    float N0, float N1, float N2) {
    spherical_harmonics_sn3d(sW, sY, sZ, sX, sV, sT, sR, sS, sU, x, y, z, N0, 1.732051f*N1, 2.236068f*N2);
}
inline void spherical_harmonics_fuma(float& sW, float& sY, float& sZ, float& sX,
                                     float& sV, float& sT, float& sR, float& sS, float& sU,
                                     float x, float y, float z,
                                     float N0, float N1, float N2) {
    sW = 1.414214f*N0;

    sY = N1*y;
    sZ = N1*z;
    sX = N1*x;

    sV = N2*1.5f*x*y;
    sT = N2*1.5f*y*z;
    sR = N2*(1.5f*z*z - 0.5f);
    sS = N2*1.5f*x*z;
    sU = N2*0.75f*(x*x-y*y);
}

// 3rd order
inline void spherical_harmonics_sn3d(float& sW, float& sY, float& sZ, float& sX,
                                     float& sV, float& sT, float& sR, float& sS, float& sU,
                                     float& sQ, float& sO, float& sM, float& sK, float& sL, float& sN, float& sP,
                                     float x, float y, float z,
                                     float N0, float N1, float N2, float N3) {
    sW = N0;

    sY = N1*y;
    sZ = N1*z;
    sX = N1*x;

    sV = N2*1.732051f*x*y;
    sT = N2*1.732051f*y*z;
    sR = N2*(1.5f*z*z - 0.5f);
    sS = N2*1.732051f*x*z;
    sU = N2*0.866025f*(x*x-y*y);

    sQ = N3*0.79057f*y*(3.0f*x*x-y*y);
    sO = N3*3.8729833f*x*y*z;
    sM = N3*0.6123724f*y*(5.0f*z*z - 1.0f);
    sK = N3*0.5f*z*(5.0f*z*z - 3.0f);
    sL = N3*0.6123724f*x*(5.0f*z*z - 1.0f);
    sN = N3*1.936492f*(x*x-y*y)*z;
    sP = N3*0.79057f*x*(x*x-3.0f*y*y);
}

inline void spherical_harmonics_n3d(float& sW, float& sY, float& sZ, float& sX,
                                    float& sV, float& sT, float& sR, float& sS, float& sU,
                                    float& sQ, float& sO, float& sM, float& sK, float& sL, float& sN, float& sP,
                                    float x, float y, float z,
                                    float N0, float N1, float N2, float N3) {
    spherical_harmonics_sn3d(sW, sY, sZ, sX, sV, sT, sR, sS, sU,
                             sQ, sO, sM, sK, sL, sN, sP,
                             x, y, z,
                             N0, 1.732051f*N1, 2.236068f*N2, 2.64575f*N3);
}
inline void spherical_harmonics_fuma(float& sW, float& sY, float& sZ, float& sX,
                                     float& sV, float& sT, float& sR, float& sS, float& sU,
                                     float& sQ, float& sO, float& sM, float& sK, float& sL, float& sN, float& sP,
                                     float x, float y, float z,
                                     float N0, float N1, float N2, float N3) {
    sW = 1.414214f*N0;

    sY = N1*y;
    sZ = N1*z;
    sX = N1*x;

    sV = N2*1.5f*x*y;
    sT = N2*1.5f*y*z;
    sR = N2*(1.5f*z*z - 0.5f);
    sS = N2*1.5f*x*z;
    sU = N2*0.75f*(x*x-y*y);

    sQ = N3*0.625f*y*(3.0f*x*x-y*y);
    sO = N3*2.886751f*x*y*z;
    sM = N3*0.516398f*y*(5.0f*z*z - 1.0f);
    sK = N3*0.5f*z*(5.0f*z*z - 3.0f);
    sL = N3*0.516398f*x*(5.0f*z*z - 1.0f);
    sN = N3*1.443376f*(x*x-y*y)*z;
    sP = N3*0.625f*x*(x*x-3.0f*y*y);
}

/*

 stereo audio settings:
        Blumlein       L:+45 / R:-45 degrees
        Cardioid       L:+60 / R:-60 degrees
        Hypercardioid  L:+60 / R:-60 degrees

        amblib stereo settings - L:+30 / R:-30 degrees

    what about earpad settings?

*/
#define STEREO_SEPARATE_ANGLE 120
#if (STEREO_SEPARATE_ANGLE<180)
Vector3 const right_ear_vector(sin(0.00873f*STEREO_SEPARATE_ANGLE),
                               cos(0.00873f*STEREO_SEPARATE_ANGLE), 0.0f);
Vector3 const left_ear_vector(-right_ear_vector.x, right_ear_vector.y, 0.0f);
#endif

//- ambiX -------------------------------------------------------------------------------

    // 4-channels ambix - YouTube style
inline bool proj_1st_ambix(float* dst, int samples, float const* src, uint8 const* acn,
                           Matrix3 const& listener, float gain) {
    int const W = acn[0];
    int const Y = acn[1];
    int const Z = acn[2];
    int const X = acn[3];

    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sWR, sYR, sZR, sXR;
    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_sn3d(sWL, sYL, sZL, sXL,
                             v.y, -v.x, v.z, // x, y, z coordinate transform
                             n0, n1);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_sn3d(sWR, sYR, sZR, sXR,
                             v.y, -v.x, v.z, // x, y, z coordinate transform
                             n0, n1);

    for (int i=0; i<samples; ++i,src+=4) {
        *dst++ = sWL*src[W] + sYL*src[Y] + sZL*src[Z] + sXL*src[X];
        *dst++ = sWR*src[W] + sYR*src[Y] + sZR*src[Z] + sXR*src[X];
    }
#else
    Vector3 const v = listener.XAxis();
    float sW, sY, sZ, sX;
    spherical_harmonics_sn3d(sW, sY, sZ, sX,
                             v.y, -v.x, v.z, // x, y, z coordinate transform
                             n0, n1);
    float mid, side;
    for (int i=0; i<samples; ++i,src+=4) {
        mid = sW*src[W];
        side = sY*src[Y] + sZ*src[Z] + sX*src[X];
        *dst++ = mid - side; // left channel
        *dst++ = mid + side; // right channel
    }
#endif
    return true;
}

inline bool proj_2nd_ambix(float* dst, int samples, float const* src, uint8 const* acn,
                           Matrix3 const& listener, float gain) {
    int const W = acn[0];
    int const Y = acn[1];
    int const Z = acn[2];
    int const X = acn[3];
    int const V = acn[4];
    int const T = acn[5];
    int const R = acn[6];
    int const S = acn[7];
    int const U = acn[8];

    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;
    float const n2 = 1.0f*gain;

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sVL, sTL, sRL, sSL, sUL;
    float sWR, sYR, sZR, sXR, sVR, sTR, sRR, sSR, sUR;

    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_sn3d(sWL, sYL, sZL, sXL,
                             sVL, sTL, sRL, sSL, sUL,
                             v.y, -v.x, v.z, n0, n1, n2);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_sn3d(sWR, sYR, sZR, sXR,
                             sVR, sTR, sRR, sSR, sUR,
                             v.y, -v.x, v.z, n0, n1, n2);

    for (int i=0; i<samples; ++i,src+=9) {
        *dst++ = src[W]*sWL + src[Y]*sYL + src[Z]*sXL + src[X]*sZL +
                 src[V]*sVL + src[S]*sTL + src[T]*sRL + src[R]*sSL + src[U]*sUL;
        *dst++ = src[W]*sWR + src[Y]*sYR + src[Z]*sXR + src[X]*sZR +
                 src[V]*sVR + src[S]*sTR + src[T]*sRR + src[R]*sSR + src[U]*sUR;
    }
#else
    Vector3 const v = listener.XAxis();
    float sW, sY, sZ, sX, sV, sT, sR, sS, sU;
    spherical_harmonics_sn3d(sW, sY, sZ, sX,
                             sV, sT, sR, sS, sU,
                             v.y, -v.x, v.z, n0, n1, n2);

    float ll, rr, t;
    for (int i=0; i<samples; ++i,src+=9) {
        ll = rr = sW*src[W];

        // 1st order
        t = src[Y]*sY + src[Z]*sX + src[X]*sZ;
        ll -= t;
        rr += t;

        // 2nd order - all even functions... but does it make sense?
        t = src[V]*sV + src[S]*sT + src[T]*sR + src[R]*sS + src[U]*sU;
        ll += t;
        rr += t;

        *dst++ = ll;
        *dst++ = rr;
    }
#endif

    return true;
}

inline bool proj_3rd_ambix(float* dst, int samples, float const* src, uint8 const* acn,
                           Matrix3 const& listener, float gain) {
    int const W = acn[0];
    int const Y = acn[1];
    int const Z = acn[2];
    int const X = acn[3];
    int const V = acn[4];
    int const T = acn[5];
    int const R = acn[6];
    int const S = acn[7];
    int const U = acn[8];
    int const Q = acn[9];
    int const O = acn[10];
    int const M = acn[11];
    int const K = acn[12];
    int const L = acn[13];
    int const N = acn[14];
    int const P = acn[15];

    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;
    float const n2 = 0.8f*gain; // ???
    float const n3 = 0.5f*gain; // ???

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sVL, sTL, sRL, sSL, sUL, sQL, sOL, sML, sKL, sLL, sNL, sPL;
    float sWR, sYR, sZR, sXR, sVR, sTR, sRR, sSR, sUR, sQR, sOR, sMR, sKR, sLR, sNR, sPR;

    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_sn3d(sWL, sYL, sZL, sXL,
                             sVL, sTL, sRL, sSL, sUL,
                             sQL, sOL, sML, sKL, sLL, sNL, sPL,
                             v.y, -v.x, v.z, n0, n1, n2, n3);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_sn3d(sWR, sYR, sZR, sXR,
                             sVR, sTR, sRR, sSR, sUR,
                             sQR, sOR, sMR, sKR, sLR, sNR, sPR,
                             v.y, -v.x, v.z, n0, n1, n2, n3);
//#define TIME_METERING
#ifdef TIME_METERING
    static int counter = 0;
    static double time = 0.0f;
    static int total_samples = 0;
    double const t0 = mlabs::balai::system::GetTime();
#endif
    for (int i=0; i<samples; ++i,src+=16) {
        *dst++ = src[W]*sWL + src[Y]*sYL + src[Z]*sXL + src[X]*sZL +
                 src[V]*sVL + src[S]*sTL + src[T]*sRL + src[R]*sSL + src[U]*sUL + 
                 src[Q]*sQL + src[O]*sOL + src[M]*sML + src[K]*sKL + src[L]*sLL + src[N]*sNL + src[P]*sPL;
        *dst++ = src[W]*sWR + src[Y]*sYR + src[Z]*sXR + src[X]*sZR +
                 src[V]*sVR + src[S]*sTR + src[T]*sRR + src[R]*sSR + src[U]*sUR + 
                 src[Q]*sQR + src[O]*sOR + src[M]*sMR + src[K]*sKR + src[L]*sLR + src[N]*sNR + src[P]*sPR;
    }
#ifdef TIME_METERING
    double const t1 = mlabs::balai::system::GetTime();
    time += (t1-t0);
    total_samples += samples;
    ++counter;
    if (total_samples>44100) {
        BL_LOG("avg time:%.1fus  rate:%.1f samples/sec\n", 1000000.0f*time/counter, total_samples/time);
        time = 0.0;
        total_samples = counter = 0;
    }
#endif

#else
    Vector3 const axis = listener.XAxis();
    float sW, sY, sZ, sX, sV, sT, sR, sS, sU, sQ, sO, sM, sK, sL, sN, sP;
    spherical_harmonics_sn3d(sW, sY, sZ, sX,
                             sV, sT, sR, sS, sU,
                             sQ, sO, sM, sK, sL, sN, sP,
                             axis.y, -axis.x, axis.z, n0, n1, n2, n3);

    float ll, rr, t;
    for (int i=0; i<samples; ++i,src+=16) {
        ll = rr = sW*src[W];

        // 1st order
        t = src[Y]*sY + src[Z]*sX + src[X]*sZ;
        ll -= t;
        rr += t;

        // 2nd order - all even functions... but does it make sense?
        t = src[V]*sV + src[S]*sT + src[T]*sR + src[R]*sS + src[U]*sU;
        ll += t;
        rr += t;

        // 3rd order
        t = src[Q]*sQ + src[O]*sO + src[M]*sM + src[K]*sK + src[L]*sL + src[N]*sN + src[P]*sP;
        ll -= t;
        rr += t;

        *dst++ = ll;
        *dst++ = rr;
    }
#endif
    return true;
}

inline bool proj_tbe(float* dst, int samples, float const* src,
                     Matrix3 const& listener, float gain) {
    //
    // coordinate transform... balai to 3d audio space
    //
    //
    // Note we are not transforming to FB360 coordinate space!
    //
    // FB360_RenderingSDK\TBAudioEngine_0.9.95_C_All\TBAudioEngine_GettingStarted_C_Api.pdf
    //  FB360 coordinate system : 3D vectors and points are represented through the class
    //  TBVector3, where +x is right, -x is left, +y is up, -y is down,
    //  +z is forward(into the screen) and -z is backward. (Left-handed coordinate system!?)
    //
    // instead, having the formulas from Ambix to TBE will work on ambix space.
    // http://pcfarina.eng.unipr.it/TBE-conversion.htm
    //
    //  TBE[0] =  0.486968 * Ambix[0]
    //  TBE[1] = -0.486968 * Ambix[1]
    //  TBE[2] =  0.486968 * Ambix[3]
    //  TBE[3] =  0.344747 * Ambix[2] + 0.445656 * Ambix[6]
    //  TBE[4] = -0.630957 * Ambix[8]
    //  TBE[5] = -0.630957 * Ambix[4]
    //  TBE[6] = -0.630957 * Ambix[5]
    //  TBE[7] =  0.630957 * Ambix[7]
    //
    //  TBE[3] = 0.344747*(Ambix[2] + 1.292705*Ambix[6])
    //

    Vector3 const axis = listener.XAxis();
    float const x = axis.y;
    float const y = -axis.x;
    float const z = axis.z;
    float const gx = gain*x;
    float const gy = gain*y;
    float const gz = gain*z;

    float ll, rr, t;
    for (int i=0; i<samples; ++i,src+=8) {
        ll = rr = gain*src[0];

        //
        t = -src[1]*gy + src[3]*gz + src[2]*gx;
        ll -= t;
        rr += t;

        //

        *dst++ = ll;
        *dst++ = rr;
    }
    return true;
}

//- FB360(TBE+Stereo) -------------------------------------------------------------------
inline bool proj_fb360(float* dst, int samples, float const* src,
                       Matrix3 const& listener, float gain) {
    //
    // FB360_RenderingSDK\TBAudioEngine_0.9.95_C_All\TBAudioEngine_GettingStarted_C_Api.pdf
    //  FB360 coordinate system : 3D vectors and points are represented through the class
    //  TBVector3, where +x is right, -x is left, +y is up, -y is down, 
    //  +z is forward(into the screen) and -z is backward. (Left-handed coordinate system!?)
    //
    // instead, having the formulas from Ambix to TBE will work on ambix space.
    // http://pcfarina.eng.unipr.it/TBE-conversion.htm
    //
    //  TBE[0] =  0.486968 * Ambix[0] // W
    //  TBE[1] = -0.486968 * Ambix[1] // Y
    //  TBE[2] =  0.486968 * Ambix[3] // X
    //  TBE[3] =  0.344747 * Ambix[2] + 0.445656 * Ambix[6] // Z + R
    //  TBE[4] = -0.630957 * Ambix[8] // U
    //  TBE[5] = -0.630957 * Ambix[4] // V
    //  TBE[6] = -0.630957 * Ambix[5] // T
    //  TBE[7] =  0.630957 * Ambix[7] // S
    //
    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;
    float const n2 = 1.0f*gain;

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sVL, sTL, sRL, sSL, sUL;
    float sWR, sYR, sZR, sXR, sVR, sTR, sRR, sSR, sUR;

    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_sn3d(sWL, sYL, sZL, sXL,
                             sVL, sTL, sRL, sSL, sUL,
                             v.y, -v.x, v.z, n0, n1, n2);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_sn3d(sWR, sYR, sZR, sXR,
                             sVR, sTR, sRR, sSR, sUR,
                             v.y, -v.x, v.z, n0, n1, n2);

    for (int i=0; i<samples; ++i,src+=10) {
        *dst++ = sWL*(src[0]+src[8]) -
                 src[1]*sYL + src[2]*sXL + src[3]*sZL -
                 src[5]*sVL - src[7]*sTL + src[6]*sRL + src[4]*sUL;
        *dst++ = sWR*(src[0]+src[9]) -
                 src[1]*sYR + src[2]*sXR + src[3]*sZR -
                 src[5]*sVR - src[7]*sTR + src[6]*sRR - src[4]*sUR;
    }
#else
    Vector3 const v = listener.XAxis();
    float sW, sY, sZ, sX, sV, sT, sR, sS, sU;
    spherical_harmonics_sn3d(sW, sY, sZ, sX,
                             sV, sT, sR, sS, sU,
                             v.y, -v.x, v.z, n0, n1, n2);

    float ll, rr, t;
    for (int i=0; i<samples; ++i,src+=10) {
        ll = sW*(src[0]+src[8]);
        rr = sW*(src[0]+src[9]);

        // 1st order
        t = -src[1]*sY + src[2]*sX + src[3]*sZ;
        ll -= t;
        rr += t;

        // 2nd order - all even functions... but does it make sense?
        t = -src[5]*sV - src[7]*sT + src[6]*sR - src[4]*sU;
        ll += t;
        rr += t;

        *dst++ = ll;
        *dst++ = rr;
    }
#endif

    return true;
}

//- Furse-Malham(FuMa) ------------------------------------------------------------------
/*
  from libambix (fuma2ambix_weightorder(), adaptor_fuma.c) :
 
  static int order[]={
                   0,
               2,  3,  1,
           8,  6,  4,  5, 7,
      15, 13, 11,  9, 10, 12, 14,
  };
  float32_t weights[] = {
                                   sqrt2,
                             -1.0,  1.0,       -1.0,
              sqrt3_4,   -sqrt3_4,  1.0,   -sqrt3_4, sqrt3_4,
    -sqrt5_8, sqrt5_9, -sqrt32_45,  1.0, -sqrt32_45, sqrt5_9, -sqrt5_8,
  };

  where :
      const float32_t sqrt2    = (float32_t)(sqrt(2.));         // 1.414
      const float32_t sqrt3_4  = (float32_t)(sqrt(3.)/2.);      // 0.86603
      const float32_t sqrt5_8  = (float32_t)(sqrt(5./2.)/2.);   // 0.79057
      const float32_t sqrt32_45= (float32_t)(4.*sqrt(2./5.)/3.);// 0.84327
      const float32_t sqrt5_9  = (float32_t)(sqrt(5.)/3.);      // 0.74536
*/

inline bool proj_1st_fuma(float* dst, int samples, float const* src, uint8 const* acn,
                          Matrix3 const& listener, float gain) {
    int const W = acn[0];
    int const Y = acn[1];
    int const Z = acn[2];
    int const X = acn[3];

    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sWR, sYR, sZR, sXR;
    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_fuma(sWL, sYL, sZL, sXL,
                             v.y, -v.x, v.z, // x, y, z coordinate transform
                             n0, n1);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_fuma(sWR, sYR, sZR, sXR,
                             v.y, -v.x, v.z, // x, y, z coordinate transform
                             n0, n1);

    for (int i=0; i<samples; ++i,src+=4) {
        *dst++ = sWL*src[W] + sYL*src[Y] + sZL*src[Z] + sXL*src[X];
        *dst++ = sWR*src[W] + sYR*src[Y] + sZR*src[Z] + sXR*src[X];
    }
#else
    Vector3 const v = listener.XAxis();
    float sW, sY, sZ, sX;
    spherical_harmonics_fuma(sW, sY, sZ, sX,
                             v.y, -v.x, v.z, // x, y, z coordinate transform
                             n0, n1);
    float mid, side;
    for (int i=0; i<samples; ++i,src+=4) {
        mid = sW*src[W];
        side = sY*src[Y] + sZ*src[Z] + sX*src[X];
        *dst++ = mid - side; // left channel
        *dst++ = mid + side; // right channel
    }
#endif
    return true;
}

inline bool proj_2nd_fuma(float* dst, int samples, float const* src, uint8 const* acn,
                          Matrix3 const& listener, float gain) {
    int const W = acn[0];
    int const Y = acn[1];
    int const Z = acn[2];
    int const X = acn[3];
    int const V = acn[4];
    int const T = acn[5];
    int const R = acn[6];
    int const S = acn[7];
    int const U = acn[8];

    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;
    float const n2 = 1.0f*gain;

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sVL, sTL, sRL, sSL, sUL;
    float sWR, sYR, sZR, sXR, sVR, sTR, sRR, sSR, sUR;

    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_fuma(sWL, sYL, sZL, sXL,
                             sVL, sTL, sRL, sSL, sUL,
                             v.y, -v.x, v.z, n0, n1, n2);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_fuma(sWR, sYR, sZR, sXR,
                             sVR, sTR, sRR, sSR, sUR,
                             v.y, -v.x, v.z, n0, n1, n2);

    for (int i=0; i<samples; ++i,src+=9) {
        *dst++ = src[W]*sWL + src[Y]*sYL + src[Z]*sXL + src[X]*sZL +
                 src[V]*sVL + src[S]*sTL + src[T]*sRL + src[R]*sSL + src[U]*sUL;
        *dst++ = src[W]*sWR + src[Y]*sYR + src[Z]*sXR + src[X]*sZR +
                 src[V]*sVR + src[S]*sTR + src[T]*sRR + src[R]*sSR + src[U]*sUR;
    }
#else
    Vector3 const v = listener.XAxis();
    float sW, sY, sZ, sX, sV, sT, sR, sS, sU;
    spherical_harmonics_fuma(sW, sY, sZ, sX,
                             sV, sT, sR, sS, sU,
                             v.y, -v.x, v.z, n0, n1, n2);

    float ll, rr, t;
    for (int i=0; i<samples; ++i,src+=9) {
        ll = rr = sW*src[W];

        // 1st order
        t = src[Y]*sY + src[Z]*sX + src[X]*sZ;
        ll -= t;
        rr += t;

        // 2nd order - all even functions... but does it make sense?
        t = src[V]*sV + src[S]*sT + src[T]*sR + src[R]*sS + src[U]*sU;
        ll += t;
        rr += t;

        *dst++ = ll;
        *dst++ = rr;
    }
#endif

    return true;
}

inline bool proj_3rd_fuma(float* dst, int samples, float const* src, uint8 const* acn,
                          Matrix3 const& listener, float gain) {
    int const W = acn[0];
    int const Y = acn[1];
    int const Z = acn[2];
    int const X = acn[3];
    int const V = acn[4];
    int const T = acn[5];
    int const R = acn[6];
    int const S = acn[7];
    int const U = acn[8];
    int const Q = acn[9];
    int const O = acn[10];
    int const M = acn[11];
    int const K = acn[12];
    int const L = acn[13];
    int const N = acn[14];
    int const P = acn[15];

    float const n0 = 1.0f*gain;
    float const n1 = 1.0f*gain;
    float const n2 = 0.8f*gain; // ???
    float const n3 = 0.5f*gain; // ???

#if (STEREO_SEPARATE_ANGLE<180)
    float sWL, sYL, sZL, sXL, sVL, sTL, sRL, sSL, sUL, sQL, sOL, sML, sKL, sLL, sNL, sPL;
    float sWR, sYR, sZR, sXR, sVR, sTR, sRR, sSR, sUR, sQR, sOR, sMR, sKR, sLR, sNR, sPR;

    Vector3 v = listener.VectorTransform(left_ear_vector);
    spherical_harmonics_fuma(sWL, sYL, sZL, sXL,
                             sVL, sTL, sRL, sSL, sUL,
                             sQL, sOL, sML, sKL, sLL, sNL, sPL,
                             v.y, -v.x, v.z, n0, n1, n2, n3);

    v = listener.VectorTransform(right_ear_vector);
    spherical_harmonics_fuma(sWR, sYR, sZR, sXR,
                             sVR, sTR, sRR, sSR, sUR,
                             sQR, sOR, sMR, sKR, sLR, sNR, sPR,
                             v.y, -v.x, v.z, n0, n1, n2, n3);

    for (int i=0; i<samples; ++i,src+=16) {
        *dst++ = src[W]*sWL + src[Y]*sYL + src[Z]*sXL + src[X]*sZL +
                 src[V]*sVL + src[S]*sTL + src[T]*sRL + src[R]*sSL + src[U]*sUL + 
                 src[Q]*sQL + src[O]*sOL + src[M]*sML + src[K]*sKL + src[L]*sLL + src[N]*sNL + src[P]*sPL;
        *dst++ = src[W]*sWR + src[Y]*sYR + src[Z]*sXR + src[X]*sZR +
                 src[V]*sVR + src[S]*sTR + src[T]*sRR + src[R]*sSR + src[U]*sUR + 
                 src[Q]*sQR + src[O]*sOR + src[M]*sMR + src[K]*sKR + src[L]*sLR + src[N]*sNR + src[P]*sPR;
    }
#else
    Vector3 const axis = listener.XAxis();
    float sW, sY, sZ, sX, sV, sT, sR, sS, sU, sQ, sO, sM, sK, sL, sN, sP;
    spherical_harmonics_fuma(sW, sY, sZ, sX,
                             sV, sT, sR, sS, sU,
                             sQ, sO, sM, sK, sL, sN, sP,
                             axis.y, -axis.x, axis.z, n0, n1, n2, n3);

    float ll, rr, t;
    for (int i=0; i<samples; ++i,src+=16) {
        ll = rr = sW*src[W];

        // 1st order
        t = src[Y]*sY + src[Z]*sX + src[X]*sZ;
        ll -= t;
        rr += t;

        // 2nd order - all even functions... but does it make sense?
        t = src[V]*sV + src[S]*sT + src[T]*sR + src[R]*sS + src[U]*sU;
        ll += t;
        rr += t;

        // 3rd order
        t = src[Q]*sQ + src[O]*sO + src[M]*sM + src[K]*sK + src[L]*sL + src[N]*sN + src[P]*sP;
        ll -= t;
        rr += t;

        *dst++ = ll;
        *dst++ = rr;
    }
#endif
    return true;
}

} // deprecated

//---------------------------------------------------------------------------------------
bool AudioManager::DecodeAmbisonicByProjection_(void* out, AUDIO_FORMAT dstFmt,
                                                mlabs::balai::math::Matrix3 const& listener,
                                                float const* src, int frames,
                                                AudioDesc const& desc, float gain)
{
    float* dst = buffer_;
    if (AUDIO_FORMAT_F32==dstFmt) {
        dst = (float*) out;
    }
    else if (AUDIO_FORMAT_S16!=dstFmt ||
        bufferSize_<(int)(frames*2*sizeof(float))) {
        return false;
    }

    if (AUDIO_TECHNIQUE_AMBIX==desc.Technique) {
        uint8 const acn[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        if (4==desc.NumChannels) {
            deprecated::proj_1st_ambix(dst, frames, src, acn, listener, gain);
        }
        else if (16==desc.NumChannels) {
            deprecated::proj_3rd_ambix(dst, frames, src, acn, listener, gain);
        }
        else if (9==desc.NumChannels) {
            deprecated::proj_2nd_ambix(dst, frames, src, acn, listener, gain);
        }
        else {
            return false;
        }
    }
    else if (AUDIO_TECHNIQUE_FUMA==desc.Technique) {
        uint8 const fuma_acn[16] = { 0, 2,3,1, 8,6,4,5,7, 15,13,11,9,10,12,14 };
        if (4==desc.NumChannels) {
            deprecated::proj_1st_fuma(dst, frames, src, fuma_acn, listener, gain);
        }
        else if (16==desc.NumChannels) {
            deprecated::proj_3rd_fuma(dst, frames, src, fuma_acn, listener, gain);
        }
        else if (9==desc.NumChannels) {
            deprecated::proj_2nd_fuma(dst, frames, src, fuma_acn, listener, gain);
        }
        else {
            return false;
        }
    }
    else if (AUDIO_TECHNIQUE_TBE==desc.Technique) {
        if (8==desc.NumChannels) {
            deprecated::proj_tbe(dst, frames, src, listener, gain);
        }
        else if (10==desc.NumChannels) {
            deprecated::proj_fb360(dst, frames, src, listener, gain);
        }
        else {
            return false;
        }
    }

    if (AUDIO_FORMAT_S16==dstFmt) {
        src = buffer_;
        sint16* d = (sint16*) out;
        int s16;
        frames *= 2;
        for (int i=0; i<frames; ++i) {
            s16 = (int) ((*src++)*32767.0f);
            *d++ = s16<32768 ? (-32768<s16 ? (sint16)s16:-32768):32767;
        }
    }

    return true;
}

}}} // namespace mlabs::balai::audio