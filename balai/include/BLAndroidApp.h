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
 * @file	BLAndroidApp.h
 * @desc    Android Application class
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_ALLOW_PLATFORM_APPLICATION_INCLUDE
#error "Do NOT include 'BLAndroidApp.h' directly, #include 'BLApplication.h' instead"
#endif

#ifndef BL_ANDROID_APPLIATION_H
#define BL_ANDROID_APPLIATION_H

namespace mlabs { namespace balai { namespace framework {

typedef class AndroidApp : protected Application
{
	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(AndroidApp);

    // methods you should overwrite
	virtual bool Initialize()				 = 0;
	virtual bool FrameMove(float updateTime) = 0;
	virtual bool Render()					 = 0;
	virtual void Cleanup()					 = 0;
    
    // touch...
	virtual void TouchBegan(uint32, uint32, float, float) {}
    virtual void TouchMoved(uint32, uint32, float, float, float, float) {}
    virtual void TouchEnded(uint32, uint32, float, float) {}
//  virtual void TouchCancelled(uint32, uint32, float, float) {}

protected:
	AndroidApp();
	virtual ~AndroidApp() {} // non-public not necessary virtual(but GCC give a warning about this!)
	
public:
	bool Create(void*);
	bool FrameUpdate();
	void Destroy();
} BaseApp;

}}}

#endif // BL_ANDROID_APPLIATION_H
