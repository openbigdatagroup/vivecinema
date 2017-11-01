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
 * @file	BLPostProc.h
 * @author	andre chen
 * @history	2012/01/09 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_POSTPROCESSING_H
#define BL_POSTPROCESSING_H

#include "BLRenderSurface.h"
#include "BLPrimitives.h"

namespace mlabs { namespace balai { namespace graphics {

class ImageEffect : public IResource
{
	static void DrawNoEffect_(ITexture* src, RenderSurface* dest) {
		Renderer& renderer = Renderer::GetInstance();
		renderer.SetSurface(dest);
		renderer.PushState();
		renderer.SetBlendDisable();
		renderer.SetDepthWrite(false);
		renderer.SetZTestDisable();
		renderer.SetCullDisable();
		Primitives& prim = Primitives::GetInstance();
		prim.BeginDraw(src, GFXPT_SCREEN_QUADLIST);
		{
			prim.AddVertex2D(0.0f, 0.0f, 0.0f, 0.0f);
			prim.AddVertex2D(0.0f, 1.0f, 0.0f, 1.0f);
			prim.AddVertex2D(1.0f, 1.0f, 1.0f, 1.0f);
			prim.AddVertex2D(1.0f, 0.0f, 1.0f, 0.0f);
		}
		prim.EndDraw();
		renderer.PopState();
	}

	BL_DECLARE_RTTI;
	BL_NO_DEFAULT_CTOR(ImageEffect);
	BL_NO_COPY_ALLOW(ImageEffect);

protected:
	explicit ImageEffect(uint32 name):IResource(name, 0) {}
	virtual ~ImageEffect() {}

public:
	virtual bool Initialize() = 0;
	virtual bool FramebufferResize(uint32 width, uint32 height) = 0;
	virtual bool Simulate(float elapsedTime, ITexture* source, ITexture* image, RenderSurface* dest) = 0;
	virtual void Clear() = 0;
};


}}} // namespace mlabs::balai::graphics

#endif
