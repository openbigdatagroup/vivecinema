/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2017 HTC Corporation
 * All Rights Reserved.
 *
 * The information contained in this work is the exclusive property of HTC Corporation
 * ("HTC").  Only the user who is legally authorized by HTC ("Authorized User") has
 * right to employ this work within the scope of this statement.  Nevertheless, the
 * Authorized User shall not use this work for any purpose other than the purpose
 * agreed by HTC.  Any and all addition or modification to this work shall be
 * unconditionally granted back to HTC and such addition or modification shall be
 * solely owned by HTC.  No right is granted under this statement, including but not
 * limited to, distribution, reproduction, and transmission, except as otherwise
 * provided in this statement.  Any other usage of this work shall be subject to the
 * further written consent of HTC.
 *
 * @file    BLFFT.h
 * @author  andre chen
 * @history 2017/06/05 created
 *
 */
#ifndef BL_FFT_H
#define BL_FFT_H

#include "BLMath.h"

namespace mlabs { namespace balai { namespace math {
//
// real FFT, N (real) samples (in time domain) will produce (N/2)+1 complex samples
// in frequency domain.
//
// Important note : To fully recovery time domain signals from frequency signals,
// normalization is need either in FFT or inverse FFT. To be coincident with
// most cases(??? at least from my DSP guide book), normalization is taken
// on inverse FFT.
// 
//

// forward declaration. before we have implement this...
class FFTImp;

class RealFFT
{
    FFTImp* imp_;
    mutable void* padding_buffer_;
    int time_domain_samples_; // round up to power of 2

    void* alloc_padding_buffer_() const {
        BL_LOG("RealFFT : padding samples for FFT!?\n");
        if (NULL==padding_buffer_) {
            assert(time_domain_samples_>0);
            padding_buffer_ = malloc(time_domain_samples_*sizeof(float));
        }
        return padding_buffer_;
    }

    // not define
    RealFFT(RealFFT const&);
    RealFFT& operator=(RealFFT const&);

public:
    RealFFT();
    ~RealFFT() { Finalize(); }

    int TimeDomainSamples() const { return time_domain_samples_; }
    int FrequencySamples() const {
        return (time_domain_samples_>0) ? ((time_domain_samples_/2)+1):0;
    }

    bool Initialize(int NumSamples, bool analysis, bool synthesis);
    void Finalize();

    // FFT : result must have size >= FrequencySamples()
    bool Analysis(Complex* freqData, float const* signals, int samples=0) const;

    // inverse FFT
    bool Synthesis(float* signals, Complex const* freqData) const;
};

//
// FFT Convolver : convolution in time domain is multiplication in frequency domain
class FFTConvolver
{
    RealFFT  dft_;
    Complex* freqRes_; // array of frequency responses from array of impulse responses
    float*   scratchPad_;
    int      blockSize_;    // output size of each convolution 
    int      kernelSize_;
    int      overlapSize_;
    int      channels_;
    int      overlapStride_;
    int      freqResStride_;

    // not define
    FFTConvolver(FFTConvolver const&);
    FFTConvolver& operator=(FFTConvolver const&);

public:
    FFTConvolver():dft_(),freqRes_(NULL),scratchPad_(NULL),
        blockSize_(0),kernelSize_(0),overlapSize_(0),channels_(0),
        overlapStride_(0),freqResStride_(0) {}
    ~FFTConvolver() { Finalize(); } 

    int BlockSize() const { return blockSize_; }

    void Finalize() {
        dft_.Finalize();
        if (NULL!=freqRes_) {
            free(freqRes_);
            freqRes_ = NULL;
        }
        if (NULL!=scratchPad_) {
            free(scratchPad_);
            scratchPad_ = NULL;
        }
        blockSize_ = kernelSize_= overlapSize_ = channels_ = 0;
        overlapStride_ = freqResStride_ = 0;
    }

    void Reset(int ch=-1) {
        if (scratchPad_ && channels_>0 && overlapStride_>0) {
            int const nfft = dft_.TimeDomainSamples();
            if (nfft>0) {
                float* overlaps = scratchPad_ + nfft;
                if (0<=ch && ch<channels_) {
                    memset(overlaps + ch*overlapStride_, 0, overlapStride_*sizeof(float));
                }
                else {
                    memset(overlaps, 0, channels_*overlapStride_*sizeof(float));
                }
            }
        }
    }

    // initialize from impulse response
    // Note :
    //   1) if blockSize specified, blockSize >= IRsize or fail.
    //   2) multiple channels IR can be set. call Process() with correct channel id.
    bool Initialize(float const* IRs, int IRsize) {
        return Initialize(IRsize, IRs, 1, IRsize);
    }
    bool Initialize(int blockSize, float const* IRs, int IRsize) {
        return Initialize(blockSize, IRs, 1, IRsize);
    }
    bool Initialize(float const* IRs, int channels, int IRsize, int IRstride=0) {
        return Initialize(IRsize, IRs, channels, IRsize, IRstride);
    }
    bool Initialize(int blockSize, float const* IRs, int channels, int IRsize, int IRstride=0);

    // it's safe to set dst as src. each must have length bigger than blockSize_.
    bool Process(float* dst, float const* src, int ch=0) const; // dst[] = fft(src[])
    bool ProcessAdd(float* dst, float const* src, int ch=0) const; // dst[] += fft(src[])
};

}}}

#endif