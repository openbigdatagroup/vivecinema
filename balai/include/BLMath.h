/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2011 HTC Corporation
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
 * @file	BLMath.h
 * @desc    math library
 * @author	andre chen
 * @history	2011/12/29 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_MATH_H
#define BL_MATH_H

#include "BLCore.h"
#include <cstddef>
#include <cstdlib>  // std::srand()
#include <cmath>

namespace mlabs { namespace balai { namespace math {

namespace constants {
	float const float_epsilon			=  1.192092896e-07f;
	float const float_minimum			=  1.175494351e-38f;
	float const float_maximum			=  3.402823466e+38f;
	float const float_zero_sup			= +1e-05f; // in meters, +1e-05f = 0.00001 m
	float const float_zero_inf			= -float_zero_sup;
	float const float_one_inf			=  (float)(1.0 + float_zero_inf);
	float const float_one_sup			=  (float)(1.0 + float_zero_sup);
	float const float_zero_angle_inf	= -(float)(std::acos(float_one_inf));
	float const float_zero_angle_sup	= +(float)(std::acos(float_one_inf));
	float const float_pi				=  (float)(4.0*std::atan(1.0));
	float const float_two_pi			=  (float)(8.0*std::atan(1.0));
	float const float_half_pi			=  (float)(2.0*std::atan(1.0));
	float const float_inv_pi			=  (float)(0.25/std::atan(1.0));
	float const float_inv_two_pi		=  (float)(0.125/std::atan(1.0));
	float const float_deg_to_rad		=  (float)(std::atan(1.0)/45.0);
	float const float_rad_to_deg		=  (float)(45.0/std::atan(1.0));
	float const float_half_sqrt3		=  (float)(0.5*std::sqrt(3.0));
	float const float_half_sqrt2		=  (float)(0.5*std::sqrt(2.0));
	float const float_one_third			=  (float)(1.0/3.0);
	float const float_inv_rand_max		=  (float)(1.0/RAND_MAX);
	float const float_one_over_255		=  (float)(1.0/255.0);

	double const double_epsilon			=  2.2204460492503131e-01;
	double const double_minimum			=  2.2250738585072014e-30;
	double const double_maximum			=  1.7976931348623157e+30;
	double const double_zero_sup		= +1e-10;
	double const double_zero_inf		= -double_zero_sup;
	double const double_one_inf			=  1.0f + double_zero_inf;
	double const double_one_sup			=  1.0f + double_zero_sup;
	double const double_zero_angle_inf	= -std::acos(double_one_inf);
	double const double_zero_angle_sup	= +std::acos(double_one_inf);
	double const double_pi				=  4.0*std::atan(1.0);
	double const double_two_pi			=  8.0*std::atan(1.0);
	double const double_half_pi			=  2.0*std::atan(1.0);
	double const double_inv_pi			=  0.25/std::atan(1.0);
	double const double_inv_two_pi		=  0.125/std::atan(1.0);
	double const double_deg_to_rad		=  std::atan(1.0)/45.0;
	double const double_rad_to_deg		=  45.0/std::atan(1.0);
	double const double_half_sqrt3		=  0.5*std::sqrt(3.0);
	double const double_half_sqrt2		=  0.5*std::sqrt(2.0);
	double const double_one_third		=  1.0/3.0;
	double const double_inv_rand_max	=  1.0/RAND_MAX;
	double const double_one_over_255	=  1.0/255.0;
}

//
// sin/cos table pending...
//
#if 0 
// sin cos table
//#define USE_SIN_COS_TABLE
//#ifdef USE_SIN_COS_TABLE
// # of entry from [0, 0.5*PI]
#define NUM_SIN_COS_ENTRIES 90
struct sin_cos_entry {
	static int __id;
	float _sin, _cos, _inv_sin, _inv_cos;
	sin_cos_entry() {
		_inv_cos = constants::float_deg_to_rad * __id * 90.0f/ NUM_SIN_COS_ENTRIES;
		_sin = std::sin(_inv_cos);
		_cos = std::cos(_inv_cos);
		_inv_sin = (0==__id) ? constants::float_maximum:1.0f/_sin;
		_inv_cos = 1.0f/_cos;
		++__id;
	}
};
int sin_cos_entry::__id = 0;
static sin_cos_entry const _sin_cos_entries[NUM_SIN_COS_ENTRIES];
float math::SinTableTest(float rad)
{
	//	size_t size = sizeof(_sin_cos_entries);
	//	size = 0;
	return rad;
}
#endif

//----------------------------------------------------------------------------
// Fast inverse square root : relative error ~= 0.0017758
// refer http://www.lomont.org/Math/Papers/2003/InvSqrt.pdf	
inline float FastInvSqrt(float s) {
	BL_ASSERT(s>0);
	union { int i; float f; } u;
	u.f = s;					  // bits for floating value
	u.i = 0x5f375a86L - (u.i>>1); // initial guess
//	u.i = 0x5f3759dfL - (u.i>>1); // original : John Carmack or Gary Tarolli?
	return u.f*(1.5f - 0.5f*s*u.f*u.f); // Newton step to increase accuracy
}

//----------------------------------------------------------------------------
// IEEE 754 floating-point trick...
template<int FractionBits> int Float2Fixed(float f) {
	// local class 
	// 1) can't define static member variables.
	// 2) can't access non-static local variables.
	union INTORFLOAT {
		int i_;
		float f_;
		INTORFLOAT(float fi):f_(fi) {}
		INTORFLOAT(int ii):i_(ii) {}
	};
	static INTORFLOAT const skPosBias_((150-FractionBits)<<23);
	static INTORFLOAT const skNegBias_(((150-FractionBits)<<23) + (1<<22));

	INTORFLOAT const& rkBias = f<0.0f ? skNegBias_:skPosBias_;
	return (INTORFLOAT(f + rkBias.f_).i_ - rkBias.i_);
}
//----------------------------------------------------------------------------
// Fast conversion from a IEEE 32-bit floating point number fFloat in [0,1] 
// to a 32-bit integer in [0,2^L-1].
inline int NRM_F32_TO_FIXED(float fFloat, int L=8) {
	union {
		float f;
		int	  i;
	} u;

	u.f = fFloat;
	int iShift = 150 - L - ((u.i>>23) & 0xFF);
	if (iShift>=24) return -1; // error

	int iInt = ((u.i & 0x007FFFFF) | 0x00800000) >> iShift;

	return (iInt == (1<<L)) ? --iInt:iInt;
}

//----------------------------------------------------------------------------
// Template functions 
// when it comes to template functions, overload it, But not specialize it.
// Specialization don't overload. ref "Exceptional C++ Style", Item#7
//----------------------------------------------------------------------------
inline float ToRadian(float deg) { return (deg*constants::float_deg_to_rad); }
inline float ToDegree(float rad) { return (rad*constants::float_rad_to_deg); }

//----------------------------------------------------------------------------
template <typename Real>
inline Real Clamp(Real f, Real Min=0, Real Max=(Real)1.0) {
	return f<Min ? Min:(f>Max? Max:f);
}

//----------------------------------------------------------------------------
inline bool ZeroAngle(float rad) {
	return (rad<constants::float_zero_angle_sup) && (rad>constants::float_zero_angle_inf);
}

//----------------------------------------------------------------------------
inline bool EqualsZero(float f) {
	return (f<constants::float_zero_sup) && (f>constants::float_zero_inf);
}

//----------------------------------------------------------------------------
inline bool EqualsOne(float f) {
	return (f<constants::float_one_sup) && (f>constants::float_one_inf);
}

//----------------------------------------------------------------------------
inline bool Equals(float a, float b, float range=constants::float_zero_sup) {
	return (a>b) ? ((b+range)>=a):((a+range)>=b);
}

//----------------------------------------------------------------------------
inline bool IsBounded(float f, float range, float tolorance=0.0f) {
	if (range<=0)
		return EqualsZero(f);

	if (tolorance>0) range += tolorance;
	return (f<range) && (f>-range);
}

//----------------------------------------------------------------------------
inline bool IsPositive(float s) {
	return s > constants::float_zero_sup;
}

//----------------------------------------------------------------------------
inline bool IsNegative(float s) {
	return s < constants::float_zero_inf;
}

//----------------------------------------------------------------------------
inline int Sign(float f) {
	return (constants::float_zero_inf < f && f<constants::float_zero_sup) ? 0 : (f>0 ? 1:-1);
}

//----------------------------------------------------------------------------
inline float UnitRandom(uint32 uiSeed = 0xFFFFFFFF) {
	if (uiSeed != 0xFFFFFFFF)
		std::srand(uiSeed);

	return constants::float_inv_rand_max*std::rand();
}

//----------------------------------------------------------------------------
inline float IntervalRandom(float fMin, float fMax, uint32 uiSeed = 0xFFFFFFFF) {
	if (uiSeed != 0xFFFFFFFF)
		std::srand(uiSeed);

	return fMin + (fMax-fMin)*(constants::float_inv_rand_max*std::rand());
}

//-----------------------------------------------------------------------------
inline int Random(int inf, int sup, uint32 uiSeed = 0xFFFFFFFF) {
	if (uiSeed != 0xFFFFFFFF)
		std::srand(uiSeed);

	if (inf>=sup)
		return inf;

	return inf + (std::rand()%(sup-inf));
}

//-----------------------------------------------------------------------------
inline void Shuffle(uint32 array[], uint32 size) {
	for (uint32 i=0; i<size; ++i)
		array[i] = i;

	for (uint32 i=0; i<size; ++i) {
		for (uint32 j=0; j<size; ++j) {
			uint32 const a = std::rand()%size;
			uint32 const b = std::rand()%size;
			if (a!=b) {
				uint32 const c = array[a];
				array[a] = array[b];
				array[b] = c;
			}
		}
	}
}

//-----------------------------------------------------------------------------
inline bool Stochastic(uint32 percent) {
	return (percent>=100) || (percent>(uint32)(std::rand()%100));
}

//-----------------------------------------------------------------------------
inline float ACos(float f) {
	if (f<-1.0f || f>1.0f) 
		return constants::float_maximum;
	
	return std::acos(f);
}

//----------------------------------------------------------------------------
inline float ASin(float f) {
	if (f<-1.0f || f>1.0f) 
		return constants::float_maximum;

	return std::asin(f);
}

//----------------------------------------------------------------------------
inline float ATan(float f) {
	return std::atan(f);
}

//----------------------------------------------------------------------------
inline float ATan2(float fY, float fX) {
	return std::atan2(fY, fX);
}

//----------------------------------------------------------------------------
inline float Floor(float f) {
	return std::floor(f);
}

//----------------------------------------------------------------------------
inline float Ceil(float f) {
	return std::ceil(f); 
}

//----------------------------------------------------------------------------
inline float Cos(float f) {
	return std::cos(f);
}

//----------------------------------------------------------------------------
inline float Sin(float f) {
	return std::sin(f);
}

//----------------------------------------------------------------------------
inline float Tan(float f) {
	return std::tan(f); 
}

//----------------------------------------------------------------------------
inline float Exp(float f) {
	return std::exp(f);
}

//----------------------------------------------------------------------------
inline float FMod(float fX, float fY) {
	return std::fmod(fX, fY); 
}

//----------------------------------------------------------------------------
inline float InvSqrt(float f) {
	return 1.0f/std::sqrt(f); 
}

//----------------------------------------------------------------------------
inline float Log(float f) { 
	return std::log(f); 
}

//----------------------------------------------------------------------------
inline float Sqrt(float f) { 
	return std::sqrt(f); 
}

//----------------------------------------------------------------------------
inline float Pow(float b, float e) { 
	return std::pow(b, e); 
}

//----------------------------------------------------------------------------
inline float FAbs(float f) {
	return (f>constants::float_zero_sup) ? 
               f:(f<constants::float_zero_sup ? -f:0);
}

//----------------------------------------------------------------------------
inline float CubicRoot(float s) {
	if (constants::float_zero_inf<s && s<constants::float_zero_sup) 
		return 0;

	// from C++ Reference http://www.cplusplus.com.
	// If base is negative and exponent is not an integral value, 
	// or if base is zero and exponent is negative, a domain error occurs, 
	// setting the global variable errno to the value EDOM.
	return s>0 ? std::pow(s,  constants::float_one_third):
				-std::pow(-s, constants::float_one_third);
}

//-----------------------------------------------------------------------------
inline float FastSin0(float rad) {
	float fASqr = rad*rad;
	float fResult = 7.61e-03f;
	fResult *= fASqr;
	fResult -= 1.6605e-01f;
	fResult *= fASqr;
	fResult += 1.0f;
	fResult *= rad;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastSin1(float rad) {
	float fASqr = rad*rad;
	float fResult = -2.39e-08f;
	fResult *= fASqr;
	fResult += 2.7526e-06f;
	fResult *= fASqr;
	fResult -= 1.98409e-04f;
	fResult *= fASqr;
	fResult += 8.3333315e-03f;
	fResult *= fASqr;
	fResult -= 1.666666664e-01f;
	fResult *= fASqr;
	fResult += 1.0f;
	fResult *= rad;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastCos0(float rad) {
	float fASqr = rad*rad;
	float fResult = 3.705e-02f;
	fResult *= fASqr;
	fResult -= 4.967e-01f;
	fResult *= fASqr;
	fResult += 1.0f;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastCos1(float rad) {
	float fASqr = rad*rad;
	float fResult = -2.605e-07f;
	fResult *= fASqr;
	fResult += 2.47609e-05f;
	fResult *= fASqr;
	fResult -= 1.3888397e-03f;
	fResult *= fASqr;
	fResult += 4.16666418e-02f;
	fResult *= fASqr;
	fResult -= 4.999999963e-01f;
	fResult *= fASqr;
	fResult += 1.0f;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastTan0(float rad) {
	float fASqr = rad*rad;
	float fResult = 2.033e-01f;
	fResult *= fASqr;
	fResult += 3.1755e-01f;
	fResult *= fASqr;
	fResult += 1.0f;
	fResult *= rad;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastTan1(float rad) {
	float fASqr   = rad*rad;
	float fResult = 9.5168091e-03f;
	fResult *= fASqr;
	fResult += 2.900525e-03f;
	fResult *= fASqr;
	fResult += 2.45650893e-02f;
	fResult *= fASqr;
	fResult += 5.33740603e-02f;
	fResult *= fASqr;
	fResult += 1.333923995e-01f;
	fResult *= fASqr;
	fResult += 3.333314036e-01f;
	fResult *= fASqr;
	fResult += 1.0;
	fResult *= rad;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastInvSin0(float rad) {
	float fRoot = Sqrt(1.0f-rad);
	float fResult = -0.0187293f;
	fResult *= rad;
	fResult += 0.0742610f;
	fResult *= rad;
	fResult -= 0.2121144f;
	fResult *= rad;
	fResult += 1.5707288f;
	fResult = constants::float_half_pi - fRoot*fResult;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastInvSin1(float rad) {
	float fRoot   = Sqrt(FAbs(1.0f-rad));
	float fResult = -0.0012624911f;
	fResult *= rad;
	fResult += 0.0066700901f;
	fResult *= rad;
	fResult -= 0.0170881256f;
	fResult *= rad;
	fResult += 0.0308918810f;
	fResult *= rad;
	fResult -= 0.0501743046f;
	fResult *= rad;
	fResult += 0.0889789874f;
	fResult *= rad;
	fResult -= 0.2145988016f;
	fResult *= rad;
	fResult += 1.5707963050f;
	fResult = constants::float_half_pi - fRoot*fResult;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastInvCos0(float fValue) {
	float fRoot   = Sqrt(1.0f-fValue);
	float fResult = -0.0187293f;
	fResult *= fValue;
	fResult += 0.0742610f;
	fResult *= fValue;
	fResult -= 0.2121144f;
	fResult *= fValue;
	fResult += 1.5707288f;
	fResult *= fRoot;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastInvCos1(float fValue) {
	float fRoot   = Sqrt(FAbs(1.0f-fValue));
	float fResult = -0.0012624911f;
	fResult *= fValue;
	fResult += 0.0066700901f;
	fResult *= fValue;
	fResult -= 0.0170881256f;
	fResult *= fValue;
	fResult += 0.0308918810f;
	fResult *= fValue;
	fResult -= 0.0501743046f;
	fResult *= fValue;
	fResult += 0.0889789874f;
	fResult *= fValue;
	fResult -= 0.2145988016f;
	fResult *= fValue;
	fResult += 1.5707963050f;
	fResult *= fRoot;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastInvTan0(float fValue) {
	float fVSqr   = fValue*fValue;
	float fResult = 0.0208351f;
	fResult *= fVSqr;
	fResult -= 0.085133f;
	fResult *= fVSqr;
	fResult += 0.180141f;
	fResult *= fVSqr;
	fResult -= 0.3302995f;
	fResult *= fVSqr;
	fResult += 0.999866f;
	fResult *= fValue;
	return fResult;
}

//----------------------------------------------------------------------------
inline float FastInvTan1(float fValue) {
	float fVSqr   = fValue*fValue;
	float fResult = 0.0028662257f;
	fResult *= fVSqr;
	fResult -= 0.0161657367f;
	fResult *= fVSqr;
	fResult += 0.0429096138f;
	fResult *= fVSqr;
	fResult -= 0.0752896400f;
	fResult *= fVSqr;
	fResult += 0.1065626393f;
	fResult *= fVSqr;
	fResult -= 0.1420889944f;
	fResult *= fVSqr;
	fResult += 0.1999355085f;
	fResult *= fVSqr;
	fResult -= 0.3333314528f;
	fResult *= fVSqr;
	fResult += 1.0f;
	fResult *= fValue;
	return fResult;
}

//----------------------------------------------------------------------------
// take a, b make equation as X^2 + a1X + a0
// return true if both roots(a1, a0) are real, 
//		  false and 2 conjugate roots are (a1 + i*a0) & (a1 - i*a0).
//----------------------------------------------------------------------------
template <typename Real>
inline bool SolveQaudraticEquation(Real& a1, Real& a0) {
	Real D = a1*a1 - 4.0*a0;
	if (constants::float_zero_inf < D && D < constants::float_zero_sup)  {
		a1 = a0 = -(Real)0.5*a1;
		return true;
	}
	else if (D>0) {
		D  = 0.5*std::sqrt(D);
		a1 = -0.5*a1; // temp
		a0 = a1 - D;
		a1 = a1 + D;
		return true;
	}
	else {	
		a1 = -0.5*a1;
		a0 =  0.5*std::sqrt(-D);
		return false;
	}
}

//----------------------------------------------------------------------------
// take a, b, c make equation as X^3 + a2X^2 + a1X + a0
// return true if all 3 roots are real.
// return false if complex get. Roots = { a, b+ci, b-ci }
// FYI - http://mathworld.wolfram.com/CubicFormula.html
//
// Beware of "float", due to precision problem, this method can go wrong.
// Use double in that case. 
// ex. (x-275)(x-275)(x-275) = x^3 - 825*x^2 + 226875*x - 20796875.
//     (-275)*(-275)*(-275) = 20796876(!=20796875)
//
// Also, We may apply newton method to raised the root precision.
// (not implement yet!)
//----------------------------------------------------------------------------
template <typename Real>
bool SolveCubicEquation(Real& a2, Real& a1, Real& a0)
{
	static double const kOneOver54(1.0/54.0);

	// To solve x^3 + a2 x^2 + a1 x + a0 = 0. First we shift z = x - a2/3, 
	// the original equation becomes z^3 + pz = q (the standard form)
	// where p = (3*a1 - a2*a2)/3,
	//		 q = (9*a1*a2 - 27*a0 - 2*a2*a2*a2)/27;
	double D(a2*a2);
	double const Shift(-constants::double_one_third*a2);
	double const Q((18.0*a1 - 6.0*D)*kOneOver54);
	double const R((9.0*a2*a1 - 27.0*a0 - 2.0*a2*D)*kOneOver54);
	D = (Q*Q*Q) + (R*R);

	// z^3 + pz = q, if letting Q = p/3, R = q/2, It is now to solve : 
	// z^3 + 3Qz = 2R
	// there is a quick root B = B1 + B2, 
	//		where	B1 = [R + sqrt(Q^3 + R^2) ]^(1/3),
	//				B1 = [R - sqrt(Q^3 + R^2) ]^(1/3)
	//
	// [Proof]	B^3 = B1^3 + 3*B1*B2(B1+B2) + B2^3
	//				= (B1^3 + B2^3) + 3*(B1*B2)*B
	//				= 2R + 3*(-Q)*B = 2R - 3QB
	//				=> B^3 + 3QB = 2R	 Q.E.D.
	//
	// the first root z1 = B1 + B2							-----(1)
	//
	// Continue with factoring out (z-B):
	// z^3 + 3Qz - 2R = (z-B)(z^2 + B*z + (B^2+3Q)) = 0,
	//
	// other 2 roots can be solved by quadratic equation:
	// (z^2 + B*z + (B^2+3Q)) = 0
	//  z2 = -B/2 + (sqrt(-3B^2 - 12Q))/2					-----(2)
	//  z3 = -B/2 - (sqrt(-3B^2 - 12Q))/2					-----(3)
	//
	// clear? let's continue:-)
	// letting A = B1 - B2 => 
	//		   A^2 = (B1-B2)^2 = (B1+B2)^2 - 4*B1*B2
	//			   =  B^2 + 4*Q
	// then (2) become z2 = -B/2 + (i * sqrt(3) * A)/2		-----(2')
	//		(3) become z3 = -B/2 - (i * sqrt(3) * A)/2		-----(3')
	//
	// Conclusion,
	//		x1 = -a2/3 + (S+T)								-----(4)
	//		x2 = -a2/3 - (S+T)/2 + (i * sqrt(3) * (S-T))/2	-----(5)
	//		x3 = -a2/3 - (S+T)/2 + (i * sqrt(3) * (S-T))/2	-----(6)
	// where S = [R + sqrt(Q^3 + R^2) ]^(1/3)
	//		 T = [R - sqrt(Q^3 + R^2) ]^(1/3)
	//		 Q = (3*a1 - a2*a2)/9,
	//		 R = (9*a1*a2 - 27*a0 - 2*a2*a2*a2)/54, and
	//		 i = sqrt(-1).	phew~
	//
	// wait! This is not the end of the whole story! 
	// what if (Q^3 + R^2)<0 ? In that case, we may apply
	//		z1 = c * cos(phi) - a2/3						-----(7)
	//		z2 = c * cos(phi + 2PI/3) - a2/3				-----(8)
	//		z3 = c * cos(phi + 4PI/3) - a2/3				-----(9)
	// where c = 2*sqrt(-Q),
	//		 phi = (arccos(R/sqrt(-Q^3)))/3.
	// (by using the identity : sin^3(phi) - 3sin(phi)/4 + sin(3phi)/4 = 0,
	//  but that another long story!!!)
	//
	// "I have discovered a truly remarkable proof which this margin is too 
	//  small to contain."	-Pierre Fermat
	//
	// More about discriminant D :
	// If D>0, One root is real and others 2 are complex conjugate,
	// If D=0, All 3 roots are real and at least 2 are identical,
	// If D<0, All 3 roots are real and distinct.
	//
	// Last words :
	// we may apply Newton method to better accuracy, but now the main
	// problem seems come from IEEE 754 float point precision issue.
	// So, i have not implemented it yet.
	//
	// Class dismissed!
	// -andre '07.08.18

	// quick observe
	if (-1.e-5<R && R<1.e-5) {
		// roots = 0, z^2 = -3Q;
		if (-1.e-5<Q && Q<1.e-5) {
			a2 = a1 = a0 = (Real) Shift;
			return true;
		}
		else if (Q>0) {
			a2 = a1 = (Real) Shift;
			a0 = (Real) std::sqrt(3.0*Q); 
			return false;
		}
		else {
			D  = std::sqrt(-3.0*Q);
			a2 = (Real)(Shift+D);
			a1 = (Real) Shift;
			a0 = (Real)(Shift-D);
			return true;
		}
	}

	if (-1.e-5<Q && Q<1.e-5) {
		// z^3 = 2R (R!=0)
		D  = CubicRoot(2.0*R);
		a2 = (Real)(Shift + D);
		a1 = (Real)(Shift - 0.5*D);
		a0 = (Real)(D>0 ? constants::double_half_sqrt3*D:
						 -constants::double_half_sqrt3*D);
		return false;
	}

	if (D>0) {
		// one root is real and the other 2 are complex conjugate
		// use eq.(4), (5), (6)
		D = std::sqrt(D);
		double const S = CubicRoot(R + D);
		double const T = CubicRoot(R - D);

		D = S + T;
		a2 = (Real)(Shift + D);
		a0 = a1 = (Real)(Shift - 0.5*D);
		D  = S - T;
		if (D<1.e-2) {
			// we prefer that's a precision issue. 
			// D is actually 0.
			if(a2<a1) {
				D = a2; a2 = a0; a0 = (Real)D;
			}
			return true;
		}
		else {
			a0 = (Real)(constants::double_half_sqrt3*D);
			return false;
		}
	}
	else {
		// D<0, all roots are real and unique
		// use eq.(7), (8), (9)
		// use formula : cos(a+b) = cos(a)cos(b)-sin(a)sin(b)
		double const phi = constants::double_one_third * std::acos(R/std::sqrt(-Q*Q*Q));
		double const c = std::cos(phi)*0.5;
		double const s = std::sin(phi)*constants::double_half_sqrt3;
		double const k = 2.0*std::sqrt(-Q);
		a2 = (Real)(Shift + 2.0*k*c);
		a1 = (Real)(Shift + k*( s - c));
		a0 = (Real)(Shift + k*(-s - c));

		// make first 2 sorted 
		if (a1>a2) {
			D = a2; a2 = a1; a1 = (Real)D;
		}

		// insert a0 to the right place and adjust the other 2
		if (a0>a2) { // 1 st place
			D = a2; a2 = a0; a0 = a1; a1 = (Real)D;
		}
		else if (a0>a1) { // 2nd running up
			D = a1; a1 = a0; a0 = (Real)D;
		}
//		else 
//			we have done!

		return true;
	}
}

// complex number 2017.06.05
struct Complex {
    float Real;
    float Imaginary;

    Complex():Real(0.0f),Imaginary(0.0f) {}
    Complex(float r, float i=0.0f):Real(r),Imaginary(i) {}
    Complex const Conjugate() const { return Complex(Real, -Imaginary); }
    float NormSq() const { return Real*Real + Imaginary*Imaginary; }
    float Norm() const { return sqrt(Real*Real + Imaginary*Imaginary); }
    float Phase() const { return atan2(Imaginary, Real); }
    
    // arithmetic updates
    Complex& operator+=(Complex const& b) {
        Real += b.Real; Imaginary += b.Imaginary;
        return *this;
    }
    Complex& operator-=(Complex const& b) {
        Real -= b.Real; Imaginary -= b.Imaginary;
        return *this;
    }
	Complex& operator*=(Complex const& b) {
        float const im = Real*b.Imaginary + Imaginary*b.Real;
        Real = Real*b.Real - Imaginary*b.Imaginary;
        Imaginary = im;
        return *this;
    }
    Complex& operator*=(float s) {
	    Real *= s; Imaginary *= s;
	    return *this;
    }
};
//--- operators ---------------------------------------------------------------
inline Complex const operator+(Complex const& a, Complex const& b) {
	return Complex(a.Real+b.Real, a.Imaginary+b.Imaginary); 
}
inline Complex const operator-(Complex const& a, Complex const& b) {
	return Complex(a.Real-b.Real, a.Imaginary-b.Imaginary); 
}
inline Complex const operator*(Complex const& a, Complex const& b) {
	return Complex(a.Real*b.Real - a.Imaginary*b.Imaginary, a.Real*b.Imaginary + b.Real*a.Imaginary); 
}
inline Complex const operator*(float s, Complex const& a) {
	return Complex(s*a.Real, s*a.Imaginary); 
}
inline Complex const operator*(Complex const& a, float s) {
    return Complex(s*a.Real, s*a.Imaginary); 
}
inline Complex const operator-(Complex const& a) { // negate
    return Complex(-a.Real, -a.Imaginary); 
}

}}} // namespace mlabs::balai::math

#endif // BL_MATH_H
