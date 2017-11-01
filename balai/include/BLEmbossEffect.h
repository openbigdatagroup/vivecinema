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
 * @file	BLEmbossEffect.h
 * @author	andre chen
 * @history	2012/06/26 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_POSTPROCESSING_EMBOSS_EFFECT_H
#define BL_POSTPROCESSING_EMBOSS_EFFECT_H

#include "BLPostProc.h"

namespace mlabs { namespace balai { namespace graphics {

class EmbossEffect : public ImageEffect
{
	BL_DECLARE_RTTI;

protected:
	EmbossEffect():ImageEffect(CalcCRC("EmbossFX")) {}
	virtual ~EmbossEffect() {}

public:
	static EmbossEffect* NewEffect(); 
};

}}} // namespace mlabs::balai::graphics

#endif