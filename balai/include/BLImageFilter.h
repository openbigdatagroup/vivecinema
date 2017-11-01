/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2012 HTC Corporation
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
 * @file	BLImageFilter.h
 * @author	andre chen
 * @history	2012/06/27 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_POSTPROCESSING_IMAGE_FILTER_H
#define BL_POSTPROCESSING_IMAGE_FILTER_H

#include "BLPostProc.h"
#include "BLVector2.h"

namespace mlabs { namespace balai { namespace graphics {

class ImageFilter : public ImageEffect
{
	BL_DECLARE_RTTI;

protected:
	math::Vector2 taps_[4];
	float weights_[8];
	float constColor_[4];

	ImageFilter():ImageEffect(CalcCRC("ImageFilter")) {
		// default : average neighbor 8 taps
		taps_[0].x = 1.0f; taps_[0].y =  1.0f;
		taps_[1].x = 1.0f; taps_[1].y = -1.0f;
		taps_[2].x = 1.0f; taps_[2].y =  0.0f;
		taps_[3].x = 0.0f; taps_[3].y =  1.0f;

		// funcky
		weights_[0] = -1.0f;
		weights_[1] = -1.0f;
		weights_[2] = -1.0f;
		weights_[3] = -0.5f;
		weights_[4] = -0.5f;
		weights_[5] = 1.0f;
		weights_[6] = 1.0f;
		weights_[7] = 1.0f;

		constColor_[0] = 1.0f;
		constColor_[1] = 1.0f;
		constColor_[2] = 1.0f;
		constColor_[3] = 1.0f;
	}
	virtual ~ImageFilter() {}

public:
	void SetConstantColor(Color const& color) {
		constColor_[0] = color.r * math::constants::float_one_over_255;
		constColor_[1] = color.g * math::constants::float_one_over_255;
		constColor_[2] = color.b * math::constants::float_one_over_255;
		constColor_[3] = color.a * math::constants::float_one_over_255;
	}
	void SetKernelWeights(float const w[8]) {
		weights_[0] = w[0];
		weights_[1] = w[1];
		weights_[2] = w[2];
		weights_[3] = w[3];
		weights_[4] = w[4];
		weights_[5] = w[5];
		weights_[6] = w[6];
		weights_[7] = w[7];
	}

	//
	// to-do : a nice method to set 8 taps arrangment
	//

	static ImageFilter* NewEffect(); 
};

}}} // namespace mlabs::balai::graphics

#endif