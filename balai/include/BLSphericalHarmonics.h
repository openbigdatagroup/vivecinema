/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2017 - present HTC Corporation
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
 * @file    BLSphericalHarmonics.h
 * @author  andre chen
 * @history 2017/06/25 created
 *
 */
#ifndef SPHERICAL_HARMONICS_H
#define SPHERICAL_HARMONICS_H

#include "BLQuaternion.h"
#include "BLMatrix3.h"

namespace mlabs { namespace balai { namespace math {

struct Sphere2 {
    float Theta, Phi;
    Sphere2():Theta(0.0f),Phi(0.0f) {}
    Sphere2(float theta, float phi):Theta(theta),Phi(phi) {}
    Vector3 const Cartesian() const {
        float const sin_theta = std::sin(Theta);
        return Vector3(sin_theta*std::cos(Phi),
                       sin_theta*std::sin(Phi),
                       std::cos(Theta));
    }
};

// uniformly distributing samples over a sphere
bool UniformDistributeSamplesOverSphere(Sphere2* samples, int totalsamples);
//
//
// To adopt Condon-Shortley or not is a good question... 
// Here we consider Condon-Shortley be included in Associated Legendre Polynomial.
// and consider the Real Spherical Harmonics of form (with Condon-Shortley phase):
//
//  for m<0, yl,m = sqrt(2) * (-1)^m * Im(Yl,|m|)
//  for m=0, yl,m = Yl,0
//  for m>0, yl,m = sqrt(2) * (-1)^m * Re(Yl,m)
//
// To make it simple, if Condon-Shortley phase is NOT included, you will have:
//  y0,0  = +sqrt(1/4pi)
//  y1,-1 = +sqrt(3/4pi)*y
//  y1,0  = +sqrt(3/4pi)*z
//  y1,1  = +sqrt(3/4pi)*x
//
// if included, 
//  y0,0  = +sqrt(1/4pi)
//  y1,-1 = -sqrt(3/4pi)*y
//  y1,0  = +sqrt(3/4pi)*z
//  y1,1  = -sqrt(3/4pi)*x
//
// also note Condon-Shortley phase also affects SH Rotation.
// Since Ambisonics channels are lebeled, W, Y, Z, X. the Condon-Shortley phase
// must NOT be adapted.
//
namespace SphericalHarmonics {
    enum {
        // 32-bits integer is not enough to store factorial of 13 (13!)
        // makes a limitation to eval Spherical Harmonics of degree 6.
        // so the int32 limitation is 7 bands (degree 0 to degree 6, total 49 items)
        //
        // 64-bit double is not enough to store fatorial of 24 (23!=25852016738884978000000)
        // so the true limitation is 12 bands (degree 0 to degree 11, total 144 items)
        MAX_BANDS = 12,
    };

    //
    // evaluate Real Spherical Harmonic
    // l : degree
    // m : order
    // theta : polar angle from zenith, 0.0 toward Z axiz
    // phi : azimuth angle from x-axis, 0.0 toward X axis
    // with respect to -l <= m <= l, also 0 <= l <= MAX_BANDS
    // cs_phase : with or without Condon-Shortley phase.
    // Note : for ambisonics, set cs_phase = false
    double EvalSH(int l, int m, double theta, double phi, bool cs_phase=false);
    double EvalSH(int l, int m, double x, double y, double z, bool cs_phase=false);
    inline double EvalSH(int l, int m, Vector3 const& v, bool cs_phase=false) {
        return EvalSH(l, m, v.x, v.y, v.z, cs_phase);
    }

    class SHCoeffs
    {
        double coeffs_[MAX_BANDS*MAX_BANDS]; // this definitely wastes lots of memory!!!
        int    bands_;

        int index_check_(int l, int m) const {
            assert(0<=l && l<bands_);
            assert(-l<=m && m<=l);
            return l*(l+1) + m;
        }

    public:
        SHCoeffs():bands_(0) { memset(coeffs_, 0, sizeof(coeffs_)); }
        explicit SHCoeffs(int bands):bands_(bands) {
            assert(0<bands_ && bands_<=MAX_BANDS);
            memset(coeffs_, 0, sizeof(coeffs_));
        }
        //~SHCoeffs() {}

        int NumBands() const { return bands_; }
        int NumCoeffs() const { return bands_*bands_; }
        double operator() (int l, int m) const { return coeffs_[index_check_(l ,m)]; }
        double& operator() (int l, int m) { return coeffs_[index_check_(l ,m)]; }
        double operator[] (int i) const {
            assert(0<=i && i<(bands_*bands_));
            return coeffs_[i];
        }
        double& operator[] (int i) {
            assert(0<=i && i<(bands_*bands_));
            return coeffs_[i];
        }

        int Set(int bands, double* coeffs=NULL) {
            if (0<bands && bands<=MAX_BANDS) {
                bands_ = bands;
                if (NULL!=coeffs) {
                    memcpy(coeffs_, coeffs, bands_*bands_*sizeof(double));
                }
                else {
                    memset(coeffs_, 0, sizeof(coeffs_));
                }
            }
            else {
                memset(coeffs_, 0, sizeof(coeffs_));
                bands_ = 0;
            }

            return bands_;
        }
/*
        // eval all SHs and save to respect co-efficients - doesn't make sense to me!?
        bool Eval(int bands, double theta, double phi) {
            if (0<bands && bands<=MAX_BANDS) {
                bands_ = bands;
                for (int l(0),i(0); l<bands; ++l) {
                    for (int m=-l; m<=l; ++m) {
                        coeffs_[i++] = SphericalHarmonics::EvalSH(l, m, theta, phi);
                    }
                }
                return true;
            }
            return false;
        }
*/
        // Synthesis - construct from SH co-efficients
        double EvalSH(double theta, double phi, bool cs_phase=false) const {
            double eval = 0.0;
            if (0<bands_ && bands_<=MAX_BANDS) {
                for (int l(0),i(0); l<bands_; ++l) {
                    for (int m=-l; m<=l; ++m) {
                        eval += coeffs_[i++]*SphericalHarmonics::EvalSH(l, m, theta, phi, cs_phase);
                    }
                }
            }
            return eval;
        }
    };

    // project - integrate over S2 to find SH coeffs.
    template<typename SphericalFunction>
    int Project(SHCoeffs& result,
                SphericalFunction const& eval_func,
                Sphere2 const* samples, SHCoeffs const* coeffs, int totals) {
        assert(samples && coeffs && totals>0);
        int project_samples = 0;
        if (samples && coeffs && totals>0) {
            int const bands = coeffs[0].NumBands();
            if (bands==result.Set(bands)) {
                int const total_coeffs = result.NumCoeffs();
                double eval;
                for (; project_samples<totals; ++project_samples) {
                    SHCoeffs const& sh = coeffs[project_samples];
                    if (bands==sh.NumBands()) {
                        eval = eval_func(samples[project_samples]);
                        for (int j=0; j<total_coeffs; ++j) {
                            result(j) += eval*sh(j);
                        }
                    }
                    else {
                        break;
                    }
                }

                // divide each coefficient by the number of samples and multiply by weights
                double const norm = 12.5663706143592/project_samples; // 4pi/samples
                for (int i=0; i<total_coeffs; ++i) {
                    result(i) *= norm;
                }
            }
        }

        return project_samples;
    }

    template<typename SphericalFunction>
    int Project(SHCoeffs& result, int bands,
                SphericalFunction const& eval_func,
                Sphere2 const* samples, int totals, bool cs_phase=false) {
        assert(samples && totals>0 && 0<bands && bands<=MAX_BANDS);
        int project_samples = 0;
        if (samples && totals>0 && 0<bands && bands<=MAX_BANDS && bands==result.Set(bands)) {
            int const total_coeffs = result.NumCoeffs();
            for (double eval; project_samples<totals; ++project_samples) {
                Sphere2 const& s = samples[project_samples];
                eval = eval_func(s);
                for (int l(0),i(0); l<bands; ++l) {
                    for (int m=-l; m<=l; ++m) {
                        result[i++] += eval*SphericalHarmonics::EvalSH(l, m, s.Theta, s.Phi, cs_phase);
                    }
                }
            }

            // divide each coefficient by the number of samples and multiply by weights
            double const norm = 12.5663706143592/project_samples; // 4pi/samples
            for (int i=0; i<total_coeffs; ++i) {
                result[i] *= norm;
            }
        }

        return project_samples;
    }

    // Rotation matrix for Spherical Harmonics
    class SHRotateMatrix
    {
        float* subMtx_; // high order(>1) sub-matrix
        float  m00_; // zero order, normally 1.0f
        float  m11_, m12_, m13_; // first order rotation matrix 3x3
        float  m21_, m22_, m23_;
        float  m31_, m32_, m33_;
        int    bands_;
        int    size_; // subMtx_ size

    public:
        SHRotateMatrix():subMtx_(NULL),
            m00_(0.0f),
            m11_(0.0f),m12_(0.0f),m13_(0.0f),
            m21_(0.0f),m22_(0.0f),m23_(0.0f),
            m31_(0.0f),m32_(0.0f),m33_(0.0f),
            bands_(0),size_(0) {}
        ~SHRotateMatrix() {
            if (subMtx_) {
                free(subMtx_);
                subMtx_ = NULL;
            }
            bands_ = size_ = 0;
        }

        // clear
        void Clear() {
            bands_ = 0; m00_ = 0.0f;
            m11_ = m12_ = m13_ = 0.0f;
            m21_ = m22_ = m23_ = 0.0f;
            m31_ = m32_ = m33_ = 0.0f;
        }

        // #(bands), 0 if invalid
        int NumBands() const { return bands_; }

        // reserve
        bool Reserve(int bands);

        // retrieve sub-matrix
        // note : size of mtx must >= (2*order+1)*(2*order+1)
        bool GetSubMatrix(float* mtx, int order) const;

        // no overlap check! no size check!
        bool Transform(float* dst, float const* src) const {
            *dst++ = m00_*src[0];
            *dst++ = m11_*src[1] + m12_*src[2] + m13_*src[3];
            *dst++ = m21_*src[1] + m22_*src[2] + m23_*src[3];
            *dst++ = m31_*src[1] + m32_*src[2] + m33_*src[3];

            if (subMtx_ && bands_>2) {
                src += 4;
                float const* subm = subMtx_;
                for (int l=2; l<bands_; ++l) {
                    int const dim = 2*l + 1;
                    for (int i=0; i<dim; ++i) {
                        float& d = dst[0] = subm[0]*src[0];
                        for (int j=1; j<dim; ++j) {
                            d += subm[j]*src[j];
                        }
                        subm += dim; // new row
                        ++dst; // next dst
                    }
                    src += dim;
                }
                return true;
            }
            return bands_>0;
        }
        // this transform from interleave src to planar dst with dst_stride set properly
        bool Transform(float* dst, int dst_stride, float const* src) const {
            *dst = m00_*src[0]; dst += dst_stride;
            *dst = m11_*src[1] + m12_*src[2] + m13_*src[3]; dst += dst_stride;
            *dst = m21_*src[1] + m22_*src[2] + m23_*src[3]; dst += dst_stride;
            *dst = m31_*src[1] + m32_*src[2] + m33_*src[3]; dst += dst_stride;

            if (subMtx_ && bands_>2) {
                src += 4;
                float const* subm = subMtx_;
                for (int l=2; l<bands_; ++l) {
                    int const dim = 2*l + 1;
                    for (int i=0; i<dim; ++i) {
                        float& d = dst[0] = subm[0]*src[0];
                        for (int j=1; j<dim; ++j) {
                            d += subm[j]*src[j];
                        }
                        subm += dim; // new row
                        dst += dst_stride; // next dst
                    }
                    src += dim;
                }
                return true;
            }
            return bands_>0;
        }
        bool TransformSafe(float* dst, float const* src, int eles) const {
            if (dst && src && 0<eles && eles==bands_*bands_) {
                float s[MAX_BANDS*MAX_BANDS];
                memcpy(s, src, eles*sizeof(float));
                return Transform(dst, s);
            }
            return true;
        }
        bool BuildRotationMatrix(int bands, Matrix3 const& rotationOnly, bool cs_phase=false);
        bool BuildRotationMatrix(int bands, Quaternion const& rot, bool cs_phase=false) {
            Matrix3 mtx;
            return BuildRotationMatrix(bands, rot.BuildRotationMatrix(mtx, true), cs_phase);
        }

        // ambisonics rotation matrix
        bool BuildRotationMatrixAmbiX(int bands, Matrix3 const& rotate);
        bool BuildRotationMatrixFuMa(int bands, Matrix3 const& rotate);
    };
    bool Rotate(SHCoeffs& dst, SHCoeffs const& src, Matrix3 const& rotationOnly, bool cs_phase);
    inline bool Rotate(SHCoeffs& dst, SHCoeffs const& src, Quaternion const& rot, bool cs_phase) {
        Matrix3 mtx;
        return Rotate(dst, src, rot.BuildRotationMatrix(mtx, true), cs_phase);
    }

#if 0
inline void Test_SHs() { // test SHs
    double y1[MAX_BANDS*MAX_BANDS], y2[MAX_BANDS*MAX_BANDS];
    double const theta = 0.01745329*180.0*rand()/RAND_MAX;
    double const phi = 0.01745329*360.0*rand()/RAND_MAX;
    double const x = sin(theta)*cos(phi);
    double const y = sin(theta)*sin(phi);
    double const z = cos(theta);
    for (int cs=0; cs<2; ++cs) {
        double error = 0.0;
        for (int l(0),i(0); l<MAX_BANDS; ++l) {
            for (int m=-l; m<=l; ++m,++i) {
                y1[i] = EvalSH(l, m, x, y, z, 0!=cs);
                y2[i] = EvalSH(l, m, theta, phi, 0!=cs);
                error += fabs(y1[i]-y2[i]);
            }
        }
        if (error>1.e-6) {
            BL_LOG("Failed! \n");
        }
    }
}
#endif

} // namespace SphericalHarmonics

}}} // namespace mlabs::balai::math

#endif