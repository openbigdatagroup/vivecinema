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
 * @file	BLFireEffect.h
 * @author	andre chen
 * @history	2012/05/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_POSTPROCESSING_FIRE_EFFECT_H
#define BL_POSTPROCESSING_FIRE_EFFECT_H

#include "BLPostProc.h"

namespace mlabs { namespace balai { namespace graphics {

class FireEffect : public ImageEffect
{
	BL_DECLARE_RTTI;

protected:
	float flowDirX_; // flame flow direction in screen space...
	float flowDirY_;

	FireEffect():ImageEffect(CalcCRC("FireFX")),flowDirX_(0.0f),flowDirY_(-1.0f) {}
	virtual ~FireEffect() {}

public:
	void SetFlameFlow(float dx=0.0f, float dy=-1.0f) {
		flowDirX_ = dx; flowDirY_ = dy;
	}
	static FireEffect* NewEffect(); 
};

}}} // namespace mlabs::balai::graphics

#endif